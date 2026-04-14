<?php

/**
 * Generates speech audio and stores it in entityreference fields targeting file.
 */
class OpenAIFieldAutomatorMediaAudioGeneration extends OpenAIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Generates speech audio and stores it as referenced files.');
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return array('entityreference');
  }

  /**
   * {@inheritdoc}
   */
  public function ruleIsAllowed($entity, $field_name, array $config) {
    $field = field_info_field($field_name);
    $target_type = $field['settings']['target_type'] ?? '';
    return $target_type === 'file';
  }

  /**
   * {@inheritdoc}
   */
  public function generate($entity, $field_name, array $config) {
    $prompts = $this->buildPrompts($entity, $config);
    if (empty($prompts)) {
      return array();
    }

    $model = !empty($config['model']) ? $config['model'] : '';
    if ($model === '') {
      return array();
    }

    $stripped_model = '';
    $api = openai_get_api_for_model($model, $stripped_model);
    if (!$api) {
      return array();
    }

    $voice = !empty($config['tts_voice']) ? $config['tts_voice'] : 'alloy';
    $format = !empty($config['tts_response_format']) ? $config['tts_response_format'] : 'mp3';
    $values = array();

    foreach ($prompts as $prompt) {
      $audio = $api->textToSpeech($stripped_model ?: $model, $prompt, $voice, $format);
      if (is_string($audio) && $audio !== '') {
        $values[] = array('binary' => $audio, 'ext' => $format);
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

    $items = array();
    foreach ($values as $value) {
      if (empty($value['binary'])) {
        continue;
      }
      $ext = !empty($value['ext']) ? $value['ext'] : 'mp3';
      $filename = !empty($config['tts_filename_pattern']) ? $config['tts_filename_pattern'] : ('ai-media-audio-' . REQUEST_TIME . '.' . $ext);
      $file = $this->createManagedFile($value['binary'], $filename);
      if (!$file || empty($file->fid)) {
        continue;
      }
      $items[] = array('target_id' => (int) $file->fid);
    }

    if (!empty($items)) {
      $entity->{$field_name}[LANGUAGE_NONE] = $items;
    }
  }

}

