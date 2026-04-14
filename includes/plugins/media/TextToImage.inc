<?php

/**
 * Generates images from prompt text and stores them into image/file fields.
 */
class OpenAIFieldAutomatorTextToImage extends OpenAIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Generate an image from text and save it to an image/file field.');
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return array('image', 'file');
  }

  /**
   * {@inheritdoc}
   */
  public function generate($entity, $field_name, array $config) {
    $prompts = $this->buildPrompts($entity, $config);
    if (empty($prompts[0])) {
      return array();
    }
    $prompt = $prompts[0];

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

    $result = $api->images($stripped_model ?: $model, $prompt, $size, $response_format, $quality, $style, $output_format);
    if (empty($result['data'][0])) {
      return array();
    }

    $item = $result['data'][0];
    if (!empty($item['b64_json'])) {
      $binary = base64_decode($item['b64_json']);
      if ($binary !== FALSE && $binary !== '') {
        return array(array('binary' => $binary, 'ext' => !empty($output_format) ? $output_format : 'png'));
      }
    }

    if (!empty($item['url'])) {
      $binary = @file_get_contents($item['url']);
      if ($binary) {
        $ext = 'png';
        $mime = '';
        if (class_exists('finfo')) {
          $finfo = new finfo(FILEINFO_MIME_TYPE);
          $mime = $finfo->buffer($binary);
        }
        if ($mime === 'image/jpeg') {
          $ext = 'jpg';
        }
        elseif ($mime === 'image/webp') {
          $ext = 'webp';
        }
        return array(array('binary' => $binary, 'ext' => $ext));
      }
    }

    return array();
  }

  /**
   * {@inheritdoc}
   */
  public function storeValues($entity, array $values, $field_name, array $config) {
    if (empty($values[0]['binary'])) {
      return;
    }

    $field = field_info_field($field_name);
    if (!$field) {
      return;
    }

    $langcode = LANGUAGE_NONE;
    $suffix = !empty($values[0]['ext']) ? $values[0]['ext'] : 'png';
    $filename = !empty($config['image_filename_pattern']) ? $config['image_filename_pattern'] : ('ai-image-' . REQUEST_TIME . '.' . $suffix);
    $file = $this->createManagedFile($values[0]['binary'], $filename);
    if (!$file || empty($file->fid)) {
      return;
    }

    if ($field['type'] === 'image') {
      $entity->{$field_name}[$langcode] = array(
        array(
          'fid' => $file->fid,
          'alt' => '',
          'title' => '',
        ),
      );
      return;
    }

    $entity->{$field_name}[$langcode] = array(
      array(
        'fid' => $file->fid,
        'display' => 1,
      ),
    );
  }

}
