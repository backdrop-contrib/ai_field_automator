<?php


namespace ai_field_automator\includes\plugins\reference;

use ai_field_automator\includes\AIFieldAutomatorBase;
use stdClass;

/**
 * @file
 * Automator plugin for entityreference fields.
 */
class AIFieldAutomatorEntityReference extends AIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t(
      'Maps generated values to existing referenceable entities or generates new referenced entities with configured field prompts.'
    );
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return ['entityreference'];
  }

  /**
   * {@inheritdoc}
   */
  public function generate($entity, $field_name, array $config) {
    if (($config['entityreference_mode'] ?? 'map_existing')
      === 'generate_new'
    ) {
      return $this->generateNewEntities($entity, $field_name, $config);
    }
    return $this->mapExistingEntities($entity, $field_name, $config);
  }

  /**
   * Maps AI output to existing referenceable entities.
   */
  protected function mapExistingEntities($entity, $field_name, array $config) {
    $prompts = $this->buildPrompts($entity, $config);
    if (empty($prompts)) {
      return [];
    }

    $options = $this->getReferenceableOptions($field_name, $config);
    if (empty($options)) {
      return [];
    }

    $use_tools = $this->shouldUseToolLookup($field_name, $options, $config);
    $option_labels = array_values($options);
    $values = [];
    foreach ($prompts as $prompt) {
      if ($use_tools) {
        $messages = [
          [
            'role' => 'user',
            'content' => $prompt . "\n\nUse live node lookup tools when you need to resolve a candidate reference. Return RFC8259 JSON only in this exact format: [{\"value\":\"existing option label or entity id\"}]",
          ],
        ];
        $text = $this->callAIWithTools($messages, ['search_nodes', 'get_node_info', 'get_nodes_by_type'], $config);
      }
      else {
        $prompt .= "\n\nAvailable options:\n" . implode("\n", $option_labels);
        $prompt .= "\n\nDo not include explanations. Return RFC8259 JSON only in this exact format: [{\"value\":\"existing option label or entity id\"}]";
        $text = $this->callAI($prompt, $config);
      }
      if ($text === '') {
        continue;
      }

      $rows = $this->decodeJsonPayload($text);
      if (empty($rows)) {
        $rows = [['value' => $text]];
      }
      foreach ($rows as $row) {
        $candidate = trim((string) ($row['value'] ?? ''));
        if ($candidate === '') {
          continue;
        }
        $id = $this->resolveToId($candidate, $options);
        if (!$id && !empty($config['entityreference_auto_create'])) {
          $id = $this->autoCreateEntityByLabel(
            $candidate,
            $field_name,
            $config
          );
          if ($id) {
            $options[$id] = $candidate;
          }
        }
        if ($id) {
          $values[] = $id;
        }
      }
    }

    return array_values(array_unique($values));
  }

  /**
   * Uses tools when the candidate set is large enough.
   */
  protected function shouldUseToolLookup($field_name, array $options, array $config) {
    if (empty($options) || !module_exists('ai_tools')) {
      return FALSE;
    }

    $field = field_info_field($field_name);
    $target_type = $field['settings']['target_type'] ?? '';
    if ($target_type !== 'node') {
      return FALSE;
    }

    if (!empty($config['entityreference_use_tools'])) {
      return TRUE;
    }

    return count($options) > 100;
  }

  /**
   * Generates structured data for new referenced entities.
   */
  protected function generateNewEntities($entity, $field_name, array $config) {
    $prompts = $this->buildPrompts($entity, $config);
    $generate_fields = $config['entityreference_generate_fields'] ?? [];
    if (empty($prompts) || empty($generate_fields)) {
      return [];
    }

    $schema_parts = [];
    foreach ($generate_fields as $target_field => $field_prompt) {
      $schema_parts[] = '"' . $target_field . '": "' . $field_prompt . '"';
    }
    $schema = implode(', ', $schema_parts);

    $rows_out = [];
    foreach ($prompts as $prompt) {
      $prompt .= "\n\nDo not include explanations. Return RFC8259 JSON only in this exact format: [{\"value\":{"
        . $schema . "}}]";
      $text = $this->callAI($prompt, $config);
      if ($text === '') {
        continue;
      }
      $rows = $this->decodeJsonPayload($text);
      foreach ($rows as $row) {
        $value = $row['value'] ?? $row;
        if (is_array($value)) {
          $rows_out[] = $value;
        }
      }
    }

    return $rows_out;
  }

  /**
   * {@inheritdoc}
   */
  public function storeValues(
    $entity,
    array $values,
    $field_name,
    array $config
  ) {
    if (empty($values)) {
      return;
    }

    if (($config['entityreference_mode'] ?? 'map_existing')
      === 'generate_new'
    ) {
      $ids = $this->createGeneratedEntities($values, $field_name, $config);
      $items = [];
      foreach ($ids as $id) {
        $items[] = ['target_id' => (int) $id];
      }
      if (!empty($items)) {
        $entity->{$field_name}[LANGUAGE_NONE] = $items;
      }
      return;
    }

    $items = [];
    foreach ($values as $value) {
      $items[] = ['target_id' => (int) $value];
    }
    $entity->{$field_name}[LANGUAGE_NONE] = $items;
  }

  /**
   * Create generated target entities and return IDs.
   */
  protected function createGeneratedEntities(
    array $rows,
    $field_name,
    array $config
  ) {
    $field = field_info_field($field_name);
    $target_type = $field['settings']['target_type'] ?? '';
    if ($target_type !== 'node') {
      return [];
    }

    $bundle = $config['entityreference_generate_bundle'] ?? '';
    if (!$bundle) {
      $bundles = array_keys(
        $field['settings']['handler_settings']['target_bundles'] ?? []
      );
      $bundle = $bundles[0] ?? '';
    }
    if (!$bundle) {
      return [];
    }

    $target_instances = field_info_instances('node', $bundle);
    $ids = [];
    foreach ($rows as $row) {
      $node = new stdClass();
      $node->type = $bundle;
      node_object_prepare($node);
      $node->uid = $GLOBALS['user']->uid;
      $node->status = 1;

      $title = trim((string) ($row['title'] ?? ''));
      $node->title = $title !== ''
        ? truncate_utf8($title, 255, TRUE, TRUE)
        : t(
          'Generated reference'
        );

      foreach ($row as $target_field => $value) {
        if ($target_field === 'title'
          || !isset($target_instances[$target_field])
        ) {
          continue;
        }
        $instance = $target_instances[$target_field];
        $field_info = field_info_field($target_field);
        $column = array_key_first($field_info['columns'] ?? ['value' => []]);
        $item = [$column => $value];
        if (in_array(
          $field_info['type'],
          ['text', 'text_long', 'text_with_summary'],
          TRUE
        )
        ) {
          $item['format'] = $config['text_format'] ?? 'plain_text';
        }
        $node->{$target_field}[LANGUAGE_NONE][0] = $item;
      }

      node_save($node);
      if (!empty($node->nid)) {
        $ids[] = (int) $node->nid;
      }
    }

    return array_values(array_unique($ids));
  }

  /**
   * Returns all referenceable entity options keyed by target_id.
   */
  protected function getReferenceableOptions($field_name, array $config) {
    if (!function_exists('entityreference_get_selection_handler')) {
      return [];
    }

    $field = field_info_field($field_name);
    $instance = field_info_instance(
      $config['entity_type'],
      $field_name,
      $config['bundle']
    );
    if (empty($field) || empty($instance)) {
      return [];
    }

    $handler = entityreference_get_selection_handler(
      $field,
      $instance,
      $config['entity_type'],
      NULL
    );
    if (!$handler || !method_exists($handler, 'getReferencableEntities')) {
      return [];
    }

    $groups = $handler->getReferencableEntities(NULL, 'CONTAINS', 0);
    $options = [];
    foreach ($groups as $group) {
      foreach ($group as $id => $label) {
        $options[(int) $id] = (string) $label;
      }
    }
    return $options;
  }

  /**
   * Resolves candidate value to a reference ID.
   */
  protected function resolveToId($candidate, array $options) {
    if (ctype_digit($candidate) && isset($options[(int) $candidate])) {
      return (int) $candidate;
    }
    foreach ($options as $id => $label) {
      if (mb_strtolower($label) === mb_strtolower($candidate)) {
        return (int) $id;
      }
    }
    foreach ($options as $id => $label) {
      if (mb_stripos($label, $candidate) !== FALSE
        || mb_stripos(
          $candidate,
          $label
        ) !== FALSE
      ) {
        return (int) $id;
      }
    }
    return 0;
  }

  /**
   * Auto-creates a target entity for node references by label.
   */
  protected function autoCreateEntityByLabel(
    $label,
    $field_name,
    array $config
  ) {
    $field = field_info_field($field_name);
    $target_type = $field['settings']['target_type'] ?? '';
    if ($target_type !== 'node') {
      return 0;
    }

    $bundles = array_keys(
      $field['settings']['handler_settings']['target_bundles'] ?? []
    );
    $bundle = $config['entityreference_create_bundle'] ?? ($bundles[0] ?? '');
    if (empty($bundle)) {
      return 0;
    }

    $node = new stdClass();
    $node->type = $bundle;
    node_object_prepare($node);
    $node->uid = $GLOBALS['user']->uid;
    $node->status = 1;
    $node->title = truncate_utf8($label, 255, TRUE, TRUE);
    node_save($node);

    return !empty($node->nid) ? (int) $node->nid : 0;
  }

}
