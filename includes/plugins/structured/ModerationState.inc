<?php

/**
 * Automator for moderation_state-like fields.
 */
class OpenAIFieldAutomatorModerationState extends OpenAIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Classifies content into a moderation state from a configured list.');
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return array('string', 'list_text');
  }

  /**
   * {@inheritdoc}
   */
  public function ruleIsAllowed($entity, $field_name, array $config) {
    if ($field_name !== 'moderation_state') {
      return FALSE;
    }

    $triggers = $this->normalizeStateList($config['moderation_trigger_states'] ?? array());
    if (empty($triggers)) {
      return TRUE;
    }

    $current = trim((string) ($entity->{$field_name}[LANGUAGE_NONE][0]['value'] ?? ''));
    return in_array($current, $triggers, TRUE);
  }

  /**
   * {@inheritdoc}
   */
  public function generate($entity, $field_name, array $config) {
    $prompts = $this->buildPrompts($entity, $config);
    if (empty($prompts)) {
      return array();
    }

    $allowed = $this->normalizeStateList($config['moderation_lookup_states'] ?? array());
    if (empty($allowed)) {
      return array();
    }

    $simple_mode = !empty($config['moderation_use_simple_model']);
    $store_reasoning = !empty($config['moderation_store_explanation_field']);
    $values = array();

    foreach ($prompts as $prompt) {
      if ($simple_mode) {
        $prompt .= "\n\nAllowed moderation states: " . implode(', ', $allowed) . ".";
        $prompt .= "\nReturn the best state and a short reason.";
        $raw = $this->callAI($prompt, $config);
        if ($raw === '') {
          continue;
        }
        $state = $this->extractStateFromText($raw, $allowed);
        if ($state !== '') {
          $values[] = array(
            'state' => $state,
            'reasoning' => trim($raw),
          );
        }
        continue;
      }

      $prompt .= "\n\nAllowed moderation states: " . implode(', ', $allowed) . ".";
      if ($store_reasoning) {
        $prompt .= "\nReturn RFC8259 JSON only in this format: [{\"value\":{\"state\":\"allowed-state\",\"reasoning\":\"1-4 sentence reason\"}}]";
      }
      else {
        $prompt .= "\nReturn RFC8259 JSON only in this format: [{\"value\":{\"state\":\"allowed-state\"}}]";
      }

      $text = $this->callAI($prompt, $config);
      if ($text === '') {
        continue;
      }
      $rows = $this->decodeJsonPayload($text);
      if (empty($rows)) {
        $fallback_state = $this->extractStateFromText($text, $allowed);
        if ($fallback_state !== '') {
          $values[] = array('state' => $fallback_state, 'reasoning' => trim($text));
        }
        continue;
      }
      if ($this->isAssoc($rows)) {
        $rows = array($rows);
      }
      foreach ($rows as $row) {
        $candidate = isset($row['value']) ? $row['value'] : $row;
        if (!is_array($candidate)) {
          continue;
        }
        $state = trim((string) ($candidate['state'] ?? ''));
        if ($state === '') {
          continue;
        }
        if (!in_array($state, $allowed, TRUE)) {
          continue;
        }
        $values[] = array(
          'state' => $state,
          'reasoning' => trim((string) ($candidate['reasoning'] ?? '')),
        );
      }
    }

    return $values;
  }

  /**
   * {@inheritdoc}
   */
  public function storeValues($entity, array $values, $field_name, array $config) {
    if (empty($values)) {
      return;
    }

    $allowed = $this->normalizeStateList($config['moderation_lookup_states'] ?? array());
    foreach ($values as $value) {
      $state = trim((string) ($value['state'] ?? ''));
      if ($state === '') {
        continue;
      }
      if (!empty($allowed) && !in_array($state, $allowed, TRUE)) {
        continue;
      }
      $entity->{$field_name}[LANGUAGE_NONE][0]['value'] = $state;

      $store_field = trim((string) ($config['moderation_store_explanation_field'] ?? ''));
      $reasoning = trim((string) ($value['reasoning'] ?? ''));
      if ($store_field !== '' && $reasoning !== '') {
        $this->storeExplanation($entity, $store_field, $reasoning, $config);
      }
      break;
    }
  }

  /**
   * Store moderation explanation into configured field.
   */
  protected function storeExplanation($entity, $field_name, $text, array $config) {
    $field_info = field_info_field($field_name);
    if (empty($field_info)) {
      return;
    }

    $column = $this->getPrimaryColumn($field_info);
    $item = array($column => $text);
    if (in_array($field_info['type'], array('text', 'text_long', 'text_with_summary'), TRUE)) {
      $item['format'] = $config['text_format'] ?? 'plain_text';
    }
    $entity->{$field_name}[LANGUAGE_NONE][0] = $item;
  }

  /**
   * Normalize list of moderation states from config.
   */
  protected function normalizeStateList($raw) {
    if (!is_array($raw)) {
      $raw = preg_split('/\s*,\s*/', trim((string) $raw), -1, PREG_SPLIT_NO_EMPTY);
    }
    $states = array();
    foreach ($raw as $key => $value) {
      $candidate = is_int($key) ? $value : ($value ? $value : $key);
      $candidate = trim((string) $candidate);
      if ($candidate !== '') {
        $states[] = $candidate;
      }
    }
    return array_values(array_unique($states));
  }

  /**
   * Extract first allowed state mentioned in free text.
   */
  protected function extractStateFromText($text, array $allowed) {
    foreach ($allowed as $state) {
      if (preg_match('/\b' . preg_quote($state, '/') . '\b/i', $text)) {
        return $state;
      }
    }
    return '';
  }

  /**
   * Returns TRUE if array is associative.
   */
  protected function isAssoc(array $array) {
    return array_keys($array) !== range(0, count($array) - 1);
  }

}

