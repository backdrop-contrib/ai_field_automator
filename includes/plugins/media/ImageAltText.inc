<?php


namespace ai_field_automator\includes\plugins\media;

use ai_field_automator\includes\AIFieldAutomatorBase;

/**
 * Generates image alt text for image field items.
 */
class AIFieldAutomatorImageAltText extends AIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Generate ALT text for image field items.');
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return ['image'];
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
    $base_field = !empty($config['base_field']) ? $config['base_field']
      : $field_name;
    if (empty($entity->{$base_field})) {
      return [];
    }

    $prompt_template = trim(
      (string) ($config['prompt'] ?? 'Write concise alt text for this image.')
    );
    $model = !empty($config['model']) ? $config['model'] : '';
    if ($model === '') {
      return [];
    }

    $values = [];
    foreach ($entity->{$base_field} as $lang_items) {
      if (!is_array($lang_items)) {
        continue;
      }
      foreach ($lang_items as $item) {
        if (empty($item['fid'])) {
          continue;
        }
        $file = file_load($item['fid']);
        if (!$file) {
          continue;
        }
        $context = strtr($prompt_template, [
          '{filename}' => !empty($file->filename) ? $file->filename : '',
        ]);
        $response = $this->callAI($context, $config, [$file]);
        if ($response !== '') {
          $values[] = trim($response);
        }
      }
    }

    return $values;
  }

  /**
   * {@inheritdoc}
   */
  public function checkIfEmpty(array $value, array $config = []) {
    foreach ($value as $item) {
      if (empty($item['alt'])) {
        return [];
      }
    }
    return $value;
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
    if (empty($values) || empty($entity->{$field_name})) {
      return;
    }

    $langcode = LANGUAGE_NONE;
    if (empty($entity->{$field_name}[$langcode])
      || !is_array(
        $entity->{$field_name}[$langcode]
      )
    ) {
      return;
    }

    foreach ($entity->{$field_name}[$langcode] as $delta => &$item) {
      if (!isset($values[$delta]) || trim((string) $values[$delta]) === '') {
        continue;
      }
      $item['alt'] = trim((string) $values[$delta]);
    }
  }

  /**
   * {@inheritdoc}
   */
  public function verifyValue($entity, $value, $field_name, array $config) {
    return is_scalar($value) && trim((string) $value) !== '';
  }

}
