<?php


namespace ai_field_automator\includes\plugins\scalar;

use ai_field_automator\includes\AIFieldAutomatorBase;

/**
 * @file
 * Automator plugin for boolean fields.
 */
class AIFieldAutomatorBoolean extends AIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t(
      'Generates a boolean yes/no value and stores 1 (true) or 0 (false).'
    );
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
        $value = $row['value'] ?? '';
        if (is_bool($value)) {
          $values[] = $value ? 1 : 0;
          continue;
        }
        $raw = strtolower((string) $value);
        $normalized = trim($raw, " \t\n\r\0\x0B.,!?:;\"'()[]{}");
        if (preg_match('/^(true|yes|y|1|enabled|on)$/', $normalized)) {
          $values[] = 1;
        }
        elseif (preg_match('/^(false|no|n|0|disabled|off)$/', $normalized)) {
          $values[] = 0;
        }
      }
    }
    return $values;
  }

}
