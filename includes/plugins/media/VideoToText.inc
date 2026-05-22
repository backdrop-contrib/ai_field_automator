<?php


namespace ai_field_automator\includes\plugins\media;

use ai_field_automator\includes\AIFieldAutomatorBase;

/**
 * Transcribes video file fields into text-based target fields.
 */
class AIFieldAutomatorVideoToText extends AIFieldAutomatorBase {

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
    return ['text', 'text_long', 'text_with_summary', 'string', 'string_long'];
  }

  /**
   * {@inheritdoc}
   */
  public function allowedInputs() {
    return ['file'];
  }

  /**
   * {@inheritdoc}
   */
  public function generate($entity, $field_name, array $config) {
    $base = !empty($config['base_field']) ? $config['base_field'] : '';
    if ($base === '' || empty($entity->{$base})) {
      return [];
    }

    $model = !empty($config['model']) ? $config['model'] : (config_get('ai.settings', 'default_stt_model') ?: 'whisper-1');
    $stripped_model = '';
    $api = ai_get_api_for_model($model, $stripped_model);
    if (!$api) {
      return [];
    }

    $task = !empty($config['speech_task']) ? $config['speech_task']
      : 'transcribe';
    $values = [];
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
        if (!empty($file->filemime)
          && strpos($file->filemime, 'video/') !== 0
        ) {
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
