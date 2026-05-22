<?php


namespace ai_field_automator\includes\plugins\media;

use ai_field_automator\includes\AIFieldAutomatorBase;

/**
 * Generates speech audio from text and stores it into file fields.
 */
class AIFieldAutomatorTextToSpeech extends AIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Generate speech audio from text and save it to a file field.');
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return ['file'];
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
  public function generate($entity, $field_name, array $config) {
    $prompts = $this->buildPrompts($entity, $config);
    if (empty($prompts[0])) {
      return [];
    }
    $text = $prompts[0];

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
    $audio = $this->generateTextToSpeechBinary(
      $api,
      $stripped_model ?: $model,
      $model,
      $text,
      $voice,
      $format
    );
    if (!is_string($audio) || $audio === '') {
      return [];
    }

    return [['binary' => $audio, 'ext' => $format]];
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
    if (empty($values[0]['binary'])) {
      return;
    }

    $langcode = LANGUAGE_NONE;
    $ext = !empty($values[0]['ext']) ? $values[0]['ext'] : 'mp3';
    $filename = !empty($config['tts_filename_pattern'])
      ? $config['tts_filename_pattern']
      : ('ai-speech-' . REQUEST_TIME . '.' . $ext);
    $file = $this->createManagedFile($values[0]['binary'], $filename);
    if (!$file || empty($file->fid)) {
      return;
    }

    $entity->{$field_name}[$langcode] = [
      [
        'fid' => $file->fid,
        'display' => 1,
      ],
    ];
  }

}
