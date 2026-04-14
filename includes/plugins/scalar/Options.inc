<?php

/**
 * @file
 * Automator plugin for list/option fields.
 */

class OpenAIFieldAutomatorOptions extends OpenAIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Generates an option and maps it to an allowed value key for list fields.');
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return ['list_text', 'list_integer', 'list_float'];
  }

  /**
   * {@inheritdoc}
   */
  public function generate($entity, $field_name, array $config) {
    $prompts = $this->buildPrompts($entity, $config);
    if (empty($prompts)) {
      return [];
    }

    $field_info = field_info_field($field_name);
    $allowed = $field_info['settings']['allowed_values'] ?? [];
    if (empty($allowed)) {
      return [];
    }

    $values = [];
    foreach ($prompts as $prompt) {
      $prompt .= "\n\nDo not include explanations. Return RFC8259 JSON only in this exact format: [{\"value\":\"requested value\"}]";
      $text = trim($this->callAI($prompt, $config));
      if ($text === '') {
        continue;
      }
      $rows = $this->decodeJsonPayload($text);
      if (empty($rows)) {
        $rows = [['value' => $text]];
      }
      foreach ($rows as $row) {
        $text = trim((string) ($row['value'] ?? ''));
        if ($text === '') {
          continue;
        }

        // First try exact key.
        if (array_key_exists($text, $allowed)) {
          $values[] = $text;
          continue;
        }

        // Then try case-insensitive label match.
        $picked = NULL;
        foreach ($allowed as $key => $label) {
          if (mb_strtolower((string) $label) === mb_strtolower($text)) {
            $picked = $key;
            break;
          }
        }

        if ($picked !== NULL) {
          $values[] = $picked;
        }
      }
    }

    return $values;
  }

}
