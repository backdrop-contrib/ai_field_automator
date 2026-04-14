<?php

/**
 * Automator for FAQ field types.
 */
class OpenAIFieldAutomatorFaqField extends OpenAIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Generates FAQ question/answer pairs from context.');
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return array('faqfield');
  }

  /**
   * {@inheritdoc}
   */
  public function generate($entity, $field_name, array $config) {
    $prompts = $this->buildPrompts($entity, $config);
    if (empty($prompts)) {
      return array();
    }

    $values = array();
    foreach ($prompts as $prompt) {
      $prompt .= "\n\nReturn RFC8259 JSON only in this format: [{\"value\":{\"question\":\"FAQ question\",\"answer\":\"FAQ answer\"}}]";
      $text = $this->callAI($prompt, $config);
      if ($text === '') {
        continue;
      }
      $rows = $this->decodeJsonPayload($text);
      if (empty($rows)) {
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
        $question = trim((string) ($candidate['question'] ?? ''));
        $answer = trim((string) ($candidate['answer'] ?? ''));
        if ($question === '' || $answer === '') {
          continue;
        }
        $values[] = array(
          'question' => $question,
          'answer' => $answer,
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

    $field_info = field_info_field($field_name);
    if (empty($field_info['columns'])) {
      return;
    }
    $columns = array_keys($field_info['columns']);
    $primary = $this->getPrimaryColumn($field_info);

    $items = array();
    foreach ($values as $value) {
      $question = trim((string) ($value['question'] ?? ''));
      $answer = trim((string) ($value['answer'] ?? ''));
      if ($question === '' || $answer === '') {
        continue;
      }

      $item = array();
      if (in_array('question', $columns, TRUE) && in_array('answer', $columns, TRUE)) {
        $item['question'] = $question;
        $item['answer'] = $answer;
      }
      else {
        $item[$primary] = "Q: " . $question . "\nA: " . $answer;
        if (!empty($field_info['settings']['allowed_formats'])) {
          $item['format'] = $config['text_format'] ?? 'plain_text';
        }
      }
      $items[] = $item;
    }

    if (!empty($items)) {
      $entity->{$field_name}[LANGUAGE_NONE] = $items;
    }
  }

  /**
   * Returns TRUE if array is associative.
   */
  protected function isAssoc(array $array) {
    return array_keys($array) !== range(0, count($array) - 1);
  }

}

