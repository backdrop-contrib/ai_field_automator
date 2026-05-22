<?php


namespace ai_field_automator\includes\plugins\media;

use ai_field_automator\includes\AIFieldAutomatorBase;

/**
 * @file
 * Automator plugin for extracting text from images (OCR).
 */
class AIFieldAutomatorImageToText extends AIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t(
      'Extracts text from images (OCR] and stores it in a text or HTML field. Requires a vision-capable model.'
    );
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
    return ['image', 'file'];
  }

  /**
   * {@inheritdoc}
   */
  public function generate($entity, $field_name, array $config) {
    // If an explicit image field is set in config, use it.
    // Otherwise fall back to the base field if it is an image/file field.
    $image_field = !empty($config['image_field']) ? $config['image_field'] : (!empty($config['base_field']) ? $config['base_field'] : '');
    $files = $this->getImagesFromFieldByDelta($entity, $image_field);

    if (empty($files)) {
      return [];
    }

    $prompts = $this->buildPrompts($entity, $config);
    if (empty($prompts)) {
      // If no prompt template, use a default OCR prompt.
      $prompts = [t('Extract all text from the attached image(s]. Format as clean text or HTML if requested.')];
    }

    $values = [];
    $single_prompt = count($prompts) === 1 ? reset($prompts) : NULL;
    foreach ($files as $delta => $file) {
      $prompt = isset($prompts[$delta]) ? $prompts[$delta] : $single_prompt;
      if (empty($prompt)) {
        continue;
      }
      $text = $this->callAI($prompt, $config, [$file]);
      if ($text !== '') {
        $values[] = $text;
      }
    }
    return $values;
  }

}
