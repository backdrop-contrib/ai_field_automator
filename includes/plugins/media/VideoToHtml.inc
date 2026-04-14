<?php

/**
 * Transcribes videos and generates HTML content.
 */
class OpenAIFieldAutomatorVideoToHtml extends OpenAIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Transcribes source videos and generates HTML output.');
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return array('text_long', 'text_with_summary', 'string_long');
  }

  /**
   * {@inheritdoc}
   */
  public function allowedInputs() {
    return array('file');
  }

  /**
   * {@inheritdoc}
   */
  public function generate($entity, $field_name, array $config) {
    $base_field = $config['base_field'] ?? '';
    if ($base_field === '' || empty($entity->{$base_field}[LANGUAGE_NONE])) {
      return array();
    }

    $speech_model = $config['speech_model'] ?? 'openai/whisper-1';
    $stripped_speech_model = '';
    $speech_api = openai_get_api_for_model($speech_model, $stripped_speech_model);
    if (!$speech_api) {
      return array();
    }

    $instruction = trim((string) ($config['prompt'] ?? 'Generate HTML content based on the transcript.'));
    if (($config['mode'] ?? 'base') === 'token' && !empty($config['token'])) {
      $instruction = trim((string) openai_field_automator_render_token_prompt($config['token'], $entity, $config['entity_type'] ?? ''));
    }
    if ($instruction === '') {
      $instruction = 'Generate HTML content based on the transcript.';
    }

    $values = array();
    foreach ($entity->{$base_field}[LANGUAGE_NONE] as $item) {
      if (empty($item['fid'])) {
        continue;
      }
      $file = file_load($item['fid']);
      if (!$file || empty($file->uri)) {
        continue;
      }
      if (!empty($file->filemime) && strpos($file->filemime, 'video/') !== 0) {
        continue;
      }
      $path = backdrop_realpath($file->uri);
      if (!$path || !is_file($path)) {
        continue;
      }

      $transcript = $speech_api->speechToText($stripped_speech_model ?: $speech_model, $path, 'transcribe');
      if (!is_string($transcript) || trim($transcript) === '') {
        continue;
      }

      $prompt = $instruction . "\n\nVideo transcript:\n" . trim($transcript);
      $prompt .= "\n\nReturn only HTML fragment output. Do not return markdown.";
      $html = $this->callAI($prompt, $config);
      if ($html !== '') {
        $values[] = trim($html);
      }
    }

    return $values;
  }

}

