<?php

/**
 * Transcribes video file fields into text-based target fields.
 */
class OpenAIFieldAutomatorVideoToText extends OpenAIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Transcribe speech from video files into text.');
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return array('text', 'text_long', 'text_with_summary', 'string', 'string_long');
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
    $base = !empty($config['base_field']) ? $config['base_field'] : '';
    if ($base === '' || empty($entity->{$base})) {
      return array();
    }

    $model = !empty($config['model']) ? $config['model'] : 'openai/whisper-1';
    $stripped_model = '';
    $api = openai_get_api_for_model($model, $stripped_model);
    if (!$api) {
      return array();
    }

    $task = !empty($config['speech_task']) ? $config['speech_task'] : 'transcribe';
    $values = array();
    foreach ($entity->{$base} as $lang_items) {
      if (!is_array($lang_items)) {
        continue;
      }
      foreach ($lang_items as $item) {
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
        $filepath = backdrop_realpath($file->uri);
        if (!$filepath || !is_file($filepath)) {
          continue;
        }
        $text = $api->speechToText($stripped_model ?: $model, $filepath, $task);
        if (is_string($text) && trim($text) !== '') {
          $values[] = trim($text);
        }
      }
    }

    return $values;
  }

}
