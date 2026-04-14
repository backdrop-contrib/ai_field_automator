<?php

/**
 * Generates images and stores them in entityreference fields targeting files.
 */
class OpenAIFieldAutomatorMediaImageGeneration extends OpenAIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Generates images and stores them as referenced files.');
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

    $size = !empty($config['image_size']) ? $config['image_size'] : '1024x1024';
    $response_format = !empty($config['image_response_format']) ? $config['image_response_format'] : 'b64_json';
    $quality = !empty($config['image_quality']) ? $config['image_quality'] : 'standard';
    $style = !empty($config['image_style']) ? $config['image_style'] : 'natural';
    $output_format = !empty($config['image_output_format']) ? $config['image_output_format'] : NULL;

    $values = array();
    foreach ($prompts as $prompt) {
      $result = $api->images($stripped_model ?: $model, $prompt, $size, $response_format, $quality, $style, $output_format);
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
  public function storeValues($entity, array $values, $field_name, array $config) {
    if (empty($values)) {
      return;
    }

    $items = array();
    foreach ($values as $value) {
      if (empty($value['binary'])) {
        continue;
      }
      $suffix = !empty($value['ext']) ? $value['ext'] : 'png';
      $filename = !empty($config['image_filename_pattern']) ? $config['image_filename_pattern'] : ('ai-media-image-' . REQUEST_TIME . '.' . $suffix);
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

  /**
   * Extract image binary payload from image API response.
   */
  protected function extractImageBinary($result, $output_format = NULL) {
    if (empty($result['data'][0])) {
      return array();
    }
    $item = $result['data'][0];
    if (!empty($item['b64_json'])) {
      $binary = base64_decode($item['b64_json']);
      if ($binary !== FALSE && $binary !== '') {
        return array('binary' => $binary, 'ext' => !empty($output_format) ? $output_format : 'png');
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
        return array('binary' => $binary, 'ext' => $ext);
      }
    }
    return array();
  }

}

