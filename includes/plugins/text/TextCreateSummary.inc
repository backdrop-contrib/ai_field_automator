<?php

/**
 * @file
 * Automator plugin that generates ONLY the summary for text_with_summary fields.
 *
 * Unlike the LongText plugin which generates the full body text, this plugin
 * assumes the main text already exists and uses AI to create a concise summary.
 */

class OpenAIFieldAutomatorTextCreateSummary extends OpenAIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Generates an AI summary for text_with_summary fields. The main text body is left untouched — only the summary column is written.');
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return ['text_with_summary'];
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

  /**
   * {@inheritdoc}
   */
  public function checkIfEmpty(array $value, array $config = array()) {
    foreach ($value as $item) {
      if (empty($item['summary'])) {
        return array();
      }
    }
    return $value;
  }

  /**
   * {@inheritdoc}
   */
  public function verifyValue($entity, $value, $field_name, array $config) {
    return is_scalar($value) && trim((string) $value) !== '';
  }

  /**
   * {@inheritdoc}
   *
   * Only updates the 'summary' column of existing field items. The main 'value'
   * and 'format' columns are preserved.
   */
  public function storeValues($entity, array $values, $field_name, array $config) {
    if (empty($values)) {
      return;
    }
    $lang = LANGUAGE_NONE;
    $existing = $entity->{$field_name}[$lang] ?? [];

    foreach ($values as $delta => $summary_text) {
      if (isset($existing[$delta]['value'])) {
        $existing[$delta]['summary'] = $summary_text;
      }
      else {
        // No existing body text at this delta — store summary as new item.
        $existing[$delta] = [
          'value' => '',
          'summary' => $summary_text,
          'format' => $config['text_format'] ?? 'plain_text',
        ];
      }
    }

    $entity->{$field_name}[$lang] = $existing;
  }

}
