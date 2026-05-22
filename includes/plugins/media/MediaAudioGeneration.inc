<?php


namespace ai_field_automator\includes\plugins\media;

use ai_field_automator\includes\AIFieldAutomatorBase;

/**
 * Generates speech audio and stores it in entityreference fields targeting
 * file.
 */
class AIFieldAutomatorMediaAudioGeneration extends
  AIFieldAutomatorBase {

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
    return ['entityreference'];
  }

  /**
   * {@inheritdoc}
   */
  public function allowedInputs() {
    return ['text', 'text_long', 'text_with_summary', 'string', 'string_long', 'paragraphs'];
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
      return [];
    }

    $model = !empty($config['model']) ? $config['model'] : '';
    if ($model === '') {
      return [];
    }

    $stripped_model = '';
    $api = ai_get_api_for_model($model, $stripped_model);
    if (!$api) {
      return [];
    }

    $voice = !empty($config['tts_voice']) ? $config['tts_voice'] : 'alloy';
    $format = !empty($config['tts_response_format'])
      ? $config['tts_response_format'] : 'mp3';
    $values = [];

    foreach ($prompts as $prompt) {
      $audio = $this->generateTextToSpeechBinary(
        $api,
        $stripped_model ?: $model,
        $model,
        $prompt,
        $voice,
        $format
      );
      if (is_string($audio) && $audio !== '') {
        $values[] = ['binary' => $audio, 'ext' => $format];
      }
    }

    return $values;
  }

  /**
   * {@inheritdoc}
   */
  public function storeValues(
    $entity,
    array $values,
    $field_name,
    array $config
  ) {
    if (empty($values)) {
      return;
    }

    $items = [];
    foreach ($values as $value) {
      if (empty($value['binary'])) {
        continue;
      }
      $ext = !empty($value['ext']) ? $value['ext'] : 'mp3';
      $filename = !empty($config['tts_filename_pattern'])
        ? $config['tts_filename_pattern']
        : ('ai-media-audio-' . REQUEST_TIME . '.' . $ext);
      $file = $this->createManagedFile($value['binary'], $filename);
      if (!$file || empty($file->fid)) {
        continue;
      }
      $items[] = ['target_id' => (int) $file->fid];
    }

    if (!empty($items)) {
      $entity->{$field_name}[LANGUAGE_NONE] = $items;
    }
  }

}
