<?php

/**
 * @file
 * Search API vector/semantic search to text output plugin.
 */

class OpenAIFieldAutomatorVectorSearchText extends OpenAIFieldAutomatorSearchBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Uses Search API AI semantic search results to populate text fields.');
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
  public function generate($entity, $field_name, array $config) {
    $prompts = $this->buildPrompts($entity, $config);
    if (empty($prompts)) {
      return [];
    }

    $output_field = trim((string) ($config['search_output_field'] ?? 'content'));
    if ($output_field === '') {
      $output_field = 'content';
    }

    $values = [];
    foreach ($prompts as $prompt) {
      $search = $this->runSearchQuery($prompt, $config);
      if (empty($search['results']) || empty($search['index'])) {
        continue;
      }

      $index = $search['index'];
      $results = $search['results'];
      $loaded_items = [];
      if (method_exists($index, 'loadItems')) {
        $loaded_items = $index->loadItems(array_keys($results));
      }

      foreach ($results as $item_id => $result) {
        if (!$this->passesMinimumScore($result, $config)) {
          continue;
        }
        $value = $this->extractOutputFieldText($item_id, $result, $output_field, $index, $loaded_items);
        if ($value !== '') {
          $values[] = $value;
        }
      }
    }

    $distinct = !array_key_exists('search_distinct', $config) || !empty($config['search_distinct']);
    if ($distinct) {
      $values = array_values(array_unique($values));
    }

    return $values;
  }

  /**
   * Extracts the configured output field value from one search result.
   */
  protected function extractOutputFieldText($item_id, array $result, $output_field, $index, array $loaded_items) {
    if ($output_field === 'content' && !empty($result['excerpt'])) {
      $excerpt = trim((string) $result['excerpt']);
      if ($excerpt !== '') {
        return $excerpt;
      }
    }

    if (!empty($result['fields'][$output_field])) {
      if (function_exists('search_api_get_sanitized_field_values')) {
        $sanitized = search_api_get_sanitized_field_values([$output_field => $result['fields'][$output_field]]);
        if (isset($sanitized[$output_field])) {
          $value = $this->flattenTextValue($sanitized[$output_field]);
          if ($value !== '') {
            return $value;
          }
        }
      }
      $value = $this->flattenTextValue($result['fields'][$output_field]);
      if ($value !== '') {
        return $value;
      }
    }

    $entity = $this->normalizeResultEntity($result['entity'] ?? NULL);
    if (!$entity && isset($loaded_items[$item_id])) {
      $entity = $this->normalizeResultEntity($loaded_items[$item_id]);
    }
    if (!$entity) {
      return '';
    }

    // Try extraction through Search API field mappings first.
    if ($index && function_exists('search_api_extract_fields') && method_exists($index, 'getFields') && method_exists($index, 'entityWrapper')) {
      try {
        $fields = $index->getFields(FALSE);
        if (!empty($fields[$output_field])) {
          $wrapper = $index->entityWrapper($entity, FALSE);
          $extracted = search_api_extract_fields($wrapper, [$output_field => $fields[$output_field]], ['sanitize' => TRUE]);
          if (isset($extracted[$output_field]['value'])) {
            $value = $this->flattenTextValue($extracted[$output_field]['value']);
            if ($value !== '') {
              return $value;
            }
          }
        }
      }
      catch (Exception $e) {
        watchdog('openai_field_automator', 'Search output field extraction failed: @msg', ['@msg' => $e->getMessage()], WATCHDOG_WARNING);
      }
    }

    // Fallback to direct field machine name.
    if (strpos($output_field, ':') !== FALSE) {
      $output_field = substr($output_field, strrpos($output_field, ':') + 1);
    }
    if (!empty($entity->{$output_field}[LANGUAGE_NONE][0]) && is_array($entity->{$output_field}[LANGUAGE_NONE][0])) {
      return $this->extractItemText($entity->{$output_field}[LANGUAGE_NONE][0]);
    }

    // "content" often maps to body-like fields.
    if ($output_field === 'content') {
      foreach (['body', 'field_body'] as $candidate_field) {
        if (!empty($entity->{$candidate_field}[LANGUAGE_NONE][0]) && is_array($entity->{$candidate_field}[LANGUAGE_NONE][0])) {
          return $this->extractItemText($entity->{$candidate_field}[LANGUAGE_NONE][0]);
        }
      }
    }

    return '';
  }

}
