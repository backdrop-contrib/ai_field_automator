<?php

/**
 * @file
 * Automator plugin for boolean fields.
 */

class OpenAIFieldAutomatorBoolean extends OpenAIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Generates a boolean yes/no value and stores 1 (true) or 0 (false).');
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return ['list_boolean', 'boolean'];
  }

  /**
   * {@inheritdoc}
   */
  public function generate($entity, $field_name, array $config) {
    $prompts = $this->buildPrompts($entity, $config);
    if (empty($prompts)) {
      return [];
    }

    $values = [];
    foreach ($prompts as $prompt) {
      $prompt .= "\n\nDo not include explanations. Return RFC8259 JSON only in this exact format: [{\"value\":\"TRUE or FALSE\"}]";
      $text = $this->callAI($prompt, $config);
      if ($text === '') {
        continue;
      }

      $rows = $this->decodeJsonPayload($text);
      if (empty($rows)) {
        $rows = [['value' => $text]];
      }

      foreach ($rows as $row) {
        $raw = strtolower((string) ($row['value'] ?? ''));
        if (preg_match('/\b(true|yes|y|1|enabled|on)\b/', $raw)) {
          $values[] = 1;
        }
        elseif (preg_match('/\b(false|no|n|0|disabled|off)\b/', $raw)) {
          $values[] = 0;
        }
      }
    }
    return $values;
  }

}
