<?php


namespace ai_field_automator\includes\plugins\scalar;

use ai_field_automator\includes\AIFieldAutomatorBase;

/**
 * @file
 * Automator plugin for numeric fields.
 */
class AIFieldAutomatorNumeric extends AIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t(
      'Generates a numeric value and stores it in an integer/decimal/float field.'
    );
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return [
      'number_integer',
      'number_decimal',
      'number_float',
      'integer',
      'decimal',
      'float',
    ];
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
    $field_type = $field_info['type'] ?? '';
    $values = [];
    foreach ($prompts as $prompt) {
      $prompt .= "\n\nDo not include explanations. Return RFC8259 JSON only in this exact format: [{\"value\":\"requested numeric value\"}]";
      $text = $this->callAI($prompt, $config);
      if ($text === '') {
        continue;
      }

      $rows = $this->decodeJsonPayload($text);
      if (empty($rows)) {
        $rows = [['value' => $text]];
      }

      foreach ($rows as $row) {
        $candidate = (string) ($row['value'] ?? '');
        if (!preg_match('/[-+]?(?:\d+(?:[.,]\d+)?|[.,]\d+)/', $candidate, $m)) {
          continue;
        }
        $number = str_replace(',', '.', $m[0]);
        if (in_array($field_type, ['number_integer', 'integer'])) {
          $values[] = (int) round((float) $number);
        }
        else {
          $values[] = (float) $number;
        }
      }
    }
    return $values;
  }

}
