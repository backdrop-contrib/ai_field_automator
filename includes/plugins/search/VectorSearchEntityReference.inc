<?php


namespace ai_field_automator\includes\plugins\search;

/**
 * @file
 * Search API vector/semantic search to entity reference plugin.
 */
class AIFieldAutomatorVectorSearchEntityReference extends
  AIFieldAutomatorSearchBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t(
      'Uses Search API AI semantic search results to populate entity references.'
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
    $prompts = $this->buildPrompts($entity, $config);
    if (empty($prompts)) {
      return [];
    }

    $field = field_info_field($field_name);
    $target_type = $field['settings']['target_type'] ?? '';
    $target_ids = [];

    foreach ($prompts as $prompt) {
      $search = $this->runSearchQuery($prompt, $config);
      if (empty($search['results']) || empty($search['index'])) {
        continue;
      }

      $index = $search['index'];
      $results = $search['results'];
      $loaded_items = [];
      if (method_exists($index, 'loadItems')) {
        $loaded_items = $index->loadItems(array_keys($results));
      }

      foreach ($results as $item_id => $result) {
        if (!$this->passesMinimumScore($result, $config)) {
          continue;
        }
        $target_id = $this->extractTargetId(
          $item_id,
          $result,
          $target_type,
          $loaded_items
        );
        if ($target_id) {
          $target_ids[] = $target_id;
        }
      }
    }

    $distinct = !array_key_exists('search_distinct', $config)
      || !empty($config['search_distinct']);
    if ($distinct) {
      $target_ids = array_values(array_unique($target_ids));
    }

    return $target_ids;
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

    $items = [];
    foreach ($values as $id) {
      if (!is_numeric($id)) {
        continue;
      }
      $items[] = ['target_id' => (int) $id];
    }
    if (!empty($items)) {
      $entity->{$field_name}[LANGUAGE_NONE] = $items;
    }
  }

  /**
   * Extracts target entity ID from one search result.
   */
  protected function extractTargetId(
    $item_id,
    array $result,
    $target_type,
    array $loaded_items
  ) {
    foreach ([$result['id'] ?? '', $item_id] as $candidate) {
      $parsed = $this->parseResultIdentifier($candidate);
      if (!$parsed['id']) {
        continue;
      }
      if ($parsed['entity_type'] !== '' && $target_type
        && $parsed['entity_type'] !== $target_type
      ) {
        continue;
      }
      return (int) $parsed['id'];
    }

    $entity = $this->normalizeResultEntity($result['entity'] ?? NULL);
    if (!$entity && isset($loaded_items[$item_id])) {
      $entity = $this->normalizeResultEntity($loaded_items[$item_id]);
    }
    if (!$entity) {
      return 0;
    }

    return $this->parseEntityIdFromObject($entity, $target_type);
  }

}

