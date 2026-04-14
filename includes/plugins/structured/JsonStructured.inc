<?php

/**
 * Generates structured JSON output.
 */
class OpenAIFieldAutomatorJsonStructured extends OpenAIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Generates strict JSON output for JSON-capable fields.');
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return array('json', 'json_native', 'json_native_binary', 'text_long', 'string_long');
  }

  /**
   * {@inheritdoc}
   */
  public function generate($entity, $field_name, array $config) {
    $prompts = $this->buildPrompts($entity, $config);
    if (empty($prompts)) {
      return array();
    }

    $schema = trim((string) ($config['json_schema'] ?? ''));
    $values = array();
    foreach ($prompts as $prompt) {
      $prompt .= "\n\nReturn RFC8259 JSON only. Do not include prose, markdown, or code fences.";
      if ($schema !== '') {
        $prompt .= "\nExpected schema/example:\n" . $schema;
      }
      $response = $this->callAI($prompt, $config);
      $normalized = $this->normalizeJsonString($response);
      if ($normalized !== '') {
        $values[] = $normalized;
      }
    }

    return $values;
  }

  /**
   * Normalize AI text response into compact JSON.
   */
  protected function normalizeJsonString($response) {
    if (!is_string($response) || trim($response) === '') {
      return '';
    }

    $clean = trim(str_replace(array('```json', '```'), '', $response));
    $decoded = json_decode($clean, TRUE);
    if (!is_array($decoded)) {
      return '';
    }

    return json_encode($decoded, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
  }

}

