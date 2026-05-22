<?php


namespace ai_field_automator\includes\plugins\text;

use ai_field_automator\includes\AIFieldAutomatorBase;

/**
 * @file
 * Automator plugin for simple text and string fields.
 */
class AIFieldAutomatorSimpleText extends AIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t(
      'Generates a short AI response and stores it in a plain text or string field.'
    );
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return ['text', 'string', 'string_long'];
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
      $text = $this->callAI($prompt, $config);
      if ($text !== '') {
        $values[] = $text;
      }
    }
    return $values;
  }

}
