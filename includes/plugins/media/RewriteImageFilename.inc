<?php


namespace ai_field_automator\includes\plugins\media;

use ai_field_automator\includes\AIFieldAutomatorBase;

/**
 * Rewrites existing image/file filenames using AI.
 */
class AIFieldAutomatorRewriteImageFilename extends
  AIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t(
      'Rewrites existing file names to descriptive, SEO-friendly values.'
    );
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
    return ['image', 'file'];
  }

  /**
   * {@inheritdoc}
   */
  public function generate($entity, $field_name, array $config) {
    $source_field = !empty($config['base_field']) ? $config['base_field']
      : $field_name;
    if (empty($entity->{$source_field}[LANGUAGE_NONE])) {
      return [];
    }

    $instruction = trim(
      (string) ($config['prompt'] ??
        'Rewrite this file name to be descriptive, lowercase, hyphen-separated, and without extension.')
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
        = 'Rewrite this file name to be descriptive, lowercase, hyphen-separated, and without extension.';
    }

    $values = [];
    foreach ($entity->{$source_field}[LANGUAGE_NONE] as $item) {
      if (empty($item['fid'])) {
        continue;
      }
      $file = file_load($item['fid']);
      if (!$file || empty($file->filename)) {
        continue;
      }
      $filename_no_ext = pathinfo($file->filename, PATHINFO_FILENAME);
      $prompt = $instruction . "\nCurrent filename: " . $file->filename;
      $prompt .= "\nReturn RFC8259 JSON only in this exact format: [{\"value\":{\"filename\":\"new-filename-without-extension\"}}]";
      $text = $this->callAI($prompt, $config);

      $new_name = '';
      if ($text !== '') {
        $rows = $this->decodeJsonPayload($text);
        if (!empty($rows)) {
          $row = $rows[0];
          $candidate = isset($row['value']) ? $row['value'] : $row;
          if (is_array($candidate)) {
            $new_name = trim((string) ($candidate['filename'] ?? ''));
          }
        }
      }
      if ($new_name === '') {
        $new_name = $filename_no_ext;
      }
      $values[] = [
        'fid' => (int) $file->fid,
        'filename' => $this->sanitizeFilenameStem($new_name),
      ];
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
    if (empty($entity->{$field_name}[LANGUAGE_NONE]) || empty($values)) {
      return;
    }

    foreach ($entity->{$field_name}[LANGUAGE_NONE] as $delta => &$item) {
      if (empty($item['fid'])) {
        continue;
      }

      $candidate = $values[$delta] ?? NULL;
      if (!$candidate) {
        foreach ($values as $value) {
          if (!empty($value['fid'])
            && (int) $value['fid'] === (int) $item['fid']
          ) {
            $candidate = $value;
            break;
          }
        }
      }
      if (empty($candidate['filename'])) {
        continue;
      }

      $file = file_load($item['fid']);
      if (!$file || empty($file->uri) || empty($file->filename)) {
        continue;
      }
      $extension = pathinfo($file->filename, PATHINFO_EXTENSION);
      $target_name = $candidate['filename'] . ($extension ? '.' . $extension
          : '');

      $parts = explode('://', $file->uri, 2);
      if (count($parts) !== 2) {
        continue;
      }
      [$scheme, $target] = $parts;
      $dir = dirname($target);
      if ($dir === '.' || $dir === DIRECTORY_SEPARATOR) {
        $new_uri = $scheme . '://' . $target_name;
      }
      else {
        $new_uri = $scheme . '://' . $dir . '/' . $target_name;
      }

      $moved = file_move($file, $new_uri, FILE_EXISTS_RENAME);
      if ($moved && !empty($moved->fid)) {
        $item['fid'] = (int) $moved->fid;
      }
    }
  }

  /**
   * Sanitizes filename stem.
   */
  protected function sanitizeFilenameStem($filename) {
    $filename = strtolower(trim((string) $filename));
    $filename = preg_replace('/[^a-z0-9]+/', '-', $filename);
    $filename = trim($filename, '-');
    if ($filename === '') {
      $filename = 'ai-file-' . REQUEST_TIME;
    }
    return $filename;
  }

}

