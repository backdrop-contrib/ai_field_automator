<?php


namespace ai_field_automator\includes\plugins\media;

use ai_field_automator\includes\AIFieldAutomatorBase;

/**
 * Generates image/file output from video transcript context.
 */
class AIFieldAutomatorVideoToImage extends AIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Transcribes videos and generates representative images.');
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return ['image', 'file'];
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
    $base_field = $config['base_field'] ?? '';
    if ($base_field === '' || empty($entity->{$base_field}[LANGUAGE_NONE])) {
      return [];
    }

    $image_model = !empty($config['model']) ? $config['model'] : '';
    if ($image_model === '') {
      return [];
    }
    $stripped_image_model = '';
    $image_api = ai_get_api_for_model($image_model, $stripped_image_model);
    if (!$image_api) {
      return [];
    }

    $speech_model = $config['speech_model'] ?? (config_get('ai.settings', 'default_stt_model') ?: 'whisper-1');
    $stripped_speech_model = '';
    $speech_api = ai_get_api_for_model(
      $speech_model,
      $stripped_speech_model
    );
    if (!$speech_api) {
      return [];
    }

    $instruction = trim(
      (string) ($config['prompt'] ??
        'Create a representative still image based on this video transcript.')
    );
    if (($config['mode'] ?? 'base') === 'token' && !empty($config['token'])) {
      $instruction = trim(
        (string) ai_field_automator_render_token_prompt(
          $config['token'],
          $entity,
          $config['entity_type'] ?? ''
        )
      );
    }
    if ($instruction === '') {
      $instruction
        = 'Create a representative still image based on this video transcript.';
    }

    $size = !empty($config['image_size']) ? $config['image_size'] : '1024x1024';
    $response_format = !empty($config['image_response_format'])
      ? $config['image_response_format'] : 'b64_json';
    $quality = !empty($config['image_quality']) ? $config['image_quality']
      : 'standard';
    $style = !empty($config['image_style']) ? $config['image_style']
      : 'natural';
    $output_format = !empty($config['image_output_format'])
      ? $config['image_output_format'] : NULL;

    $values = [];
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

      $transcript = $speech_api->speechToText(
        $stripped_speech_model ?: $speech_model,
        $path,
        'transcribe'
      );
      if (!is_string($transcript) || trim($transcript) === '') {
        continue;
      }

      $prompt = $instruction . "\n\nVideo transcript:\n" . trim($transcript);
      $result = $image_api->images(
        $stripped_image_model ?: $image_model,
        $prompt,
        $size,
        $response_format,
        $quality,
        $style,
        $output_format
      );
      $image = $this->extractImageBinary($result, $output_format);
      if (!empty($image['binary'])) {
        $values[] = $image;
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
    if (empty($values[0]['binary'])) {
      return;
    }

    $field = field_info_field($field_name);
    if (!$field) {
      return;
    }

    $langcode = LANGUAGE_NONE;
    $suffix = !empty($values[0]['ext']) ? $values[0]['ext'] : 'png';
    $filename = !empty($config['image_filename_pattern'])
      ? $config['image_filename_pattern']
      : ('ai-video-image-' . REQUEST_TIME . '.' . $suffix);
    $file = $this->createManagedFile($values[0]['binary'], $filename);
    if (!$file || empty($file->fid)) {
      return;
    }

    if ($field['type'] === 'image') {
      $entity->{$field_name}[$langcode] = [
        [
          'fid'   => $file->fid,
          'alt'   => '',
          'title' => '',
        ],
      ];
      return;
    }

    $entity->{$field_name}[$langcode] = [
      [
        'fid'     => $file->fid,
        'display' => 1,
      ],
    ];
  }

  /**
   * Extract image binary payload from image API response.
   */
  protected function extractImageBinary($result, $output_format = NULL) {
    if (empty($result['data'][0])) {
      return [];
    }
    $item = $result['data'][0];
    if (!empty($item['b64_json'])) {
      $binary = base64_decode($item['b64_json']);
      if ($binary !== FALSE && $binary !== '') {
        return [
          'binary' => $binary,
          'ext'    => !empty($output_format) ? $output_format : 'png',
        ];
      }
    }
    if (!empty($item['url'])) {
      $binary = @file_get_contents($item['url']);
      if ($binary) {
        $ext = 'png';
        if (class_exists('finfo')) {
          $finfo = new finfo(FILEINFO_MIME_TYPE);
          $mime = $finfo->buffer($binary);
          if ($mime === 'image/jpeg') {
            $ext = 'jpg';
          }
          elseif ($mime === 'image/webp') {
            $ext = 'webp';
          }
        }
        return ['binary' => $binary, 'ext' => $ext];
      }
    }
    return [];
  }

}
