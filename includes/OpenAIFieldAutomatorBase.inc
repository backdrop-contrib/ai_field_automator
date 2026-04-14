<?php

/**
 * @file
 * Abstract base class for OpenAI Field Automator plugins.
 */

abstract class OpenAIFieldAutomatorBase implements OpenAIFieldAutomatorInterface {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return '';
  }

  /**
   * {@inheritdoc}
   */
  public function allowedInputs() {
    // Default: accept text-like fields as context.
    return ['text', 'text_long', 'text_with_summary', 'string', 'string_long'];
  }

  /**
   * {@inheritdoc}
   */
  public function ruleIsAllowed($entity, $field_name, array $config) {
    return TRUE;
  }

  /**
   * {@inheritdoc}
   */
  public function checkIfEmpty(array $value, array $config = array()) {
    return $value;
  }

  /**
   * {@inheritdoc}
   */
  public function postCheckIfEmpty($entity, $value, array $config = array()) {
    return $value;
  }

  /**
   * {@inheritdoc}
   */
  public function storeValues($entity, array $values, $field_name, array $config) {
    if (empty($values)) {
      return;
    }
    $field_info = field_info_field($field_name);
    $primary_column = $this->getPrimaryColumn($field_info);
    $lang = LANGUAGE_NONE;

    // Build field items array.
    $items = [];
    foreach ($values as $delta => $value) {
      // Support both plain strings and arrays (e.g. text_with_summary).
      if (is_array($value)) {
        $items[$delta] = $value;
      }
      else {
        $items[$delta][$primary_column] = $value;
        // Carry over existing format for filtered text fields.
        if (!empty($field_info['settings']['allowed_formats'])) {
          $format = !empty($config['text_format']) ? $config['text_format'] : filter_default_format();
          $items[$delta]['format'] = $format;
        }
      }
    }

    $entity->{$field_name}[$lang] = $items;
  }

  /**
   * {@inheritdoc}
   */
  public function verifyValue($entity, $value, $field_name, array $config) {
    return TRUE;
  }

  /**
   * Builds the prompt string from config, substituting simple {tokens}.
   *
   * Supported tokens:
   *   {title}              — entity label / title
   *   {field:FIELD_NAME}   — plain-text value of another field
   *
   * @param object $entity
   *   The entity being saved.
   * @param array $config
   *   The rule config array.
   *
   * @return string
   *   The resolved prompt string.
   */
  protected function buildPrompt($entity, array $config, $context_value = NULL) {
    $prompt = $config['prompt'] ?? '';
    if (empty($prompt)) {
      return '';
    }

    // Replace {title} (tolerates optional spaces inside braces).
    // Use preg_replace only to normalize the brace variant, then str_replace for
    // the actual substitution — preg_replace treats $ in the replacement string
    // as a backreference, which corrupts values like "$100,000".
    $label = entity_label($config['entity_type'], $entity);
    $prompt = preg_replace('/\{\s*title\s*\}/', '{title}', $prompt);
    $prompt = str_replace('{title}', check_plain($label), $prompt);

    // Replace {context} (or { context } with optional spaces) with the explicit
    // context value, or resolve it from the configured base field. Same two-step
    // approach to avoid preg_replace backreference corruption.
    if ($context_value !== NULL) {
      $prompt = preg_replace('/\{\s*context\s*\}/', '{context}', $prompt);
      $prompt = str_replace('{context}', $context_value, $prompt);
    }
    elseif (!empty($config['base_field'])) {
      $base = $config['base_field'];
      $lang = LANGUAGE_NONE;
      $context_value = '';
      if ($base === '_title') {
        $context_value = check_plain($label);
      }
      elseif ($base === '_rendered_entity') {
        $context_value = $this->extractEntityContextText($entity, $config['entity_type'] ?? '');
      }
      elseif (isset($entity->{$base}[$lang][0])) {
        $item = $entity->{$base}[$lang][0];
        $context_value = strip_tags($item['value'] ?? $item['safe_value'] ?? '');
      }
      $prompt = preg_replace('/\{\s*context\s*\}/', '{context}', $prompt);
      $prompt = str_replace('{context}', $context_value, $prompt);
    }

    // Replace {field:FIELD_NAME} tokens.
    preg_match_all('/\{field:([a-z0-9_]+)\}/', $prompt, $matches);
    foreach ($matches[1] as $token_field) {
      $lang = LANGUAGE_NONE;
      $field_value = '';
      if (isset($entity->{$token_field}[$lang][0])) {
        $item = $entity->{$token_field}[$lang][0];
        $field_value = strip_tags($item['value'] ?? $item['safe_value'] ?? '');
      }
      $prompt = str_replace('{field:' . $token_field . '}', $field_value, $prompt);
    }

    return $prompt;
  }

  /**
   * Builds one or more prompts based on mode and field cardinality.
   *
   * Base mode:
   * - If the base field has multiple values, one prompt is built per delta.
   * Token mode:
   * - One token-resolved prompt is built.
   *
   * @param object $entity
   *   The entity being saved.
   * @param array $config
   *   Automator config.
   *
   * @return array
   *   Array of prompt strings.
   */
  protected function buildPrompts($entity, array $config) {
    $mode = $config['mode'] ?? 'base';
    $entity_type = $config['entity_type'] ?? '';

    if ($mode === 'token') {
      $template = $config['token'] ?? '';
      if ($template === '') {
        return [];
      }
      $prompt = openai_field_automator_render_token_prompt($template, $entity, $config['entity_type'] ?? '');
      return $prompt !== '' ? [$prompt] : [];
    }

    $base_field = $config['base_field'] ?? '';
    if ($base_field === '_title') {
      $label = entity_label($config['entity_type'], $entity);
      $prompt = $this->buildPrompt($entity, $config, check_plain($label));
      return $prompt !== '' ? [$prompt] : [];
    }
    if ($base_field === '_rendered_entity') {
      $context = $this->extractEntityContextText($entity, $entity_type);
      $prompt = $this->buildPrompt($entity, $config, $context !== '' ? $context : NULL);
      return $prompt !== '' ? [$prompt] : [];
    }

    if (empty($base_field) || !isset($entity->{$base_field}[LANGUAGE_NONE])) {
      $fallback_context = $this->extractEntityContextText($entity, $entity_type);
      $prompt = $this->buildPrompt($entity, $config, $fallback_context !== '' ? $fallback_context : NULL);
      return $prompt !== '' ? [$prompt] : [];
    }

    $prompts = [];
    foreach ($entity->{$base_field}[LANGUAGE_NONE] as $item) {
      $context = $this->extractItemText($item);
      if ($context === '') {
        continue;
      }
      $prompt = $this->buildPrompt($entity, $config, $context);
      if ($prompt !== '') {
        $prompts[] = $prompt;
      }
    }

    // If the selected base field doesn't yield text (common with paragraph-only
    // editing patterns), fall back to all entity text including paragraph items.
    if (empty($prompts)) {
      $fallback_context = $this->extractEntityContextText($entity, $entity_type);
      if ($fallback_context !== '') {
        $prompt = $this->buildPrompt($entity, $config, $fallback_context);
        if ($prompt !== '') {
          $prompts[] = $prompt;
        }
      }
    }

    return $prompts;
  }

  /**
   * Extracts best-effort plain text from a field item array.
   *
   * @param array $item
   *   A field item.
   *
   * @return string
   *   Plain text value.
   */
  protected function extractItemText(array $item) {
    $value = $item['value'] ?? $item['safe_value'] ?? $item['summary'] ?? '';
    if ($value === '') {
      $skip_keys = [
        'format' => TRUE,
        'summary_format' => TRUE,
        'weight' => TRUE,
        'delta' => TRUE,
        'item_id' => TRUE,
        'revision_id' => TRUE,
        'target_id' => TRUE,
        'fid' => TRUE,
        'uid' => TRUE,
        'nid' => TRUE,
        'tid' => TRUE,
      ];
      foreach ($item as $key => $candidate) {
        if (is_string($key) && isset($skip_keys[$key])) {
          continue;
        }
        if (is_scalar($candidate) && trim((string) $candidate) !== '') {
          $candidate = trim((string) $candidate);
          if (preg_match('/^\d+$/', $candidate)) {
            continue;
          }
          $value = $candidate;
          break;
        }
      }
    }
    if (!is_scalar($value)) {
      return '';
    }
    $value = trim(strip_tags((string) $value));
    if ($value !== '' && preg_match('/^\d+$/', $value)) {
      return '';
    }
    return $value;
  }

  /**
   * Extracts aggregate plain text context from an entity.
   *
   * For saved entities the preferred path is to render the entity to HTML and
   * strip the markup — this naturally handles paragraphs, avoids duplicated
   * content from body+paragraphs, and excludes image noise without needing
   * field-type-specific heuristics. The field-crawl fallback is kept for
   * unsaved/preview entities that cannot be rendered.
   */
  protected function extractEntityContextText($entity, $entity_type = '') {
    if (!is_object($entity) || empty($entity_type) || !function_exists('field_info_instances')) {
      return '';
    }

    // For saved entities the rendered entity is the cleanest source: it is
    // deduplicated, excludes non-content fields (moderation state, etc.), and
    // handles paragraphs correctly. Use it first when an entity ID exists.
    $entity_id = function_exists('entity_id') ? entity_id($entity_type, $entity) : NULL;
    if ($entity_id) {
      $text = $this->renderEntityToText($entity, $entity_type);
      if ($text !== '') {
        return $text;
      }
    }

    // For new/unsaved entities (no entity ID yet) fall back to form-value
    // preview text set by the widget_action AJAX handler.
    if (!empty($entity->openai_field_automator_preview_text) && is_string($entity->openai_field_automator_preview_text)) {
      $preview = trim($this->htmlToMarkdown($entity->openai_field_automator_preview_text));
      if ($preview !== '') {
        return $preview;
      }
    }

    // Fallback: field-by-field crawl for unsaved/preview entities.
    $chunks = [];
    $label = entity_label($entity_type, $entity);
    if (is_string($label)) {
      $label = trim(strip_tags($label));
      if ($label !== '') {
        $chunks[] = $label;
      }
    }

    $visited_paragraph_ids = [];
    $this->collectEntityTextFromFields($entity, $entity_type, $chunks, $visited_paragraph_ids);

    $chunks = array_values(array_unique(array_filter(array_map('trim', $chunks))));
    return implode("\n\n", $chunks);
  }

  /**
   * Renders an entity to plain text, stripping images and other non-text markup.
   *
   * @param object $entity
   *   A fully-loaded entity object.
   * @param string $entity_type
   *   The entity type (e.g. 'node').
   *
   * @return string
   *   Plain text, or empty string on failure.
   */
  protected function renderEntityToText($entity, $entity_type) {
    if (!function_exists('entity_view') || !function_exists('backdrop_render')) {
      return '';
    }

    try {
      $entity_id = entity_id($entity_type, $entity);
      $build = entity_view($entity_type, [$entity_id => $entity], 'full');
      $html = backdrop_render($build);
    }
    catch (Exception $e) {
      return '';
    }

    if (empty($html)) {
      return '';
    }

    return $this->htmlToMarkdown($html);
  }

  /**
   * Converts an HTML string to Markdown suitable for AI context.
   *
   * Delegates to StringHelper::htmlToMarkdown() — the canonical implementation
   * shared across all openai_* submodules.
   *
   * @param string $html
   *   Raw HTML string.
   *
   * @return string
   *   Markdown representation.
   */
  protected function htmlToMarkdown($html) {
    return StringHelper::htmlToMarkdown($html);
  }

  /**
   * Collects text fragments from all field values on an entity.
   *
   * Used as fallback for unsaved entities that cannot be rendered via
   * entity_view(). For saved entities, renderEntityToText() is preferred.
   */
  protected function collectEntityTextFromFields($entity, $entity_type, array &$chunks, array &$visited_paragraph_ids) {
    if (!function_exists('entity_extract_ids') || !function_exists('field_info_instances') || !function_exists('field_info_field')) {
      return;
    }

    list(, , $bundle) = entity_extract_ids($entity_type, $entity);
    if (empty($bundle)) {
      return;
    }

    $instances = field_info_instances($entity_type, $bundle);
    if (empty($instances)) {
      return;
    }

    foreach ($instances as $field_name => $instance) {
      if (in_array($field_name, ['revision_log', 'revision_log_message'], TRUE)) {
        continue;
      }
      if (empty($entity->{$field_name}) || !is_array($entity->{$field_name})) {
        continue;
      }

      $field_info = field_info_field($field_name);
      $field_type = $field_info['type'] ?? '';
      if ($field_type === 'paragraphs') {
        $this->collectParagraphItemsText($entity->{$field_name}, $chunks, $visited_paragraph_ids);
        continue;
      }
      // Skip image and file fields — metadata (URIs, MIME types, dimensions)
      // contaminates the context. Alt text alone isn't worth the noise.
      if (in_array($field_type, ['image', 'file'], TRUE)) {
        continue;
      }
      $this->collectNestedText($entity->{$field_name}, $chunks);
    }
  }

  /**
   * Collects text from paragraph item references recursively.
   */
  protected function collectParagraphItemsText(array $field_items, array &$chunks, array &$visited_paragraph_ids) {
    // Keep inline values if present, then expand saved paragraph entities.
    $this->collectNestedText($field_items, $chunks);
    if (!function_exists('paragraphs_item_load')) {
      return;
    }

    foreach ($field_items as $lang_items) {
      if (!is_array($lang_items)) {
        continue;
      }
      foreach ($lang_items as $item) {
        if (!is_array($item)) {
          continue;
        }
        $paragraph_id = 0;
        foreach (['item_id', 'value', 'target_id'] as $id_key) {
          if (!empty($item[$id_key]) && is_scalar($item[$id_key]) && ctype_digit((string) $item[$id_key])) {
            $paragraph_id = (int) $item[$id_key];
            break;
          }
        }
        if (!$paragraph_id || isset($visited_paragraph_ids[$paragraph_id])) {
          continue;
        }

        $visited_paragraph_ids[$paragraph_id] = TRUE;
        $paragraph = paragraphs_item_load($paragraph_id);
        if ($paragraph && is_object($paragraph)) {
          $this->collectEntityTextFromFields($paragraph, 'paragraphs_item', $chunks, $visited_paragraph_ids);
        }
      }
    }
  }



  /**
   * Recursively collects plain text fragments from nested array values.
   */
  protected function collectNestedText($value, array &$chunks) {
    if (is_string($value)) {
      $candidate = trim(strip_tags($value));
      if ($candidate !== '') {
        $chunks[] = $candidate;
      }
      return;
    }
    if (!is_array($value)) {
      return;
    }

    $skip_keys = [
      'format' => TRUE,
      'summary_format' => TRUE,
      'weight' => TRUE,
      'delta' => TRUE,
      'item_id' => TRUE,
      'revision_id' => TRUE,
      'bundle' => TRUE,
      'actions' => TRUE,
      'add_more' => TRUE,
      'remove' => TRUE,
      'open' => TRUE,
      '_weight' => TRUE,
      'fid' => TRUE,
      'uid' => TRUE,
      'nid' => TRUE,
      'tid' => TRUE,
      'target_id' => TRUE,
      'openai_field_automator_actions' => TRUE,
    ];
    foreach ($value as $key => $item) {
      if (is_string($key) && isset($skip_keys[$key])) {
        continue;
      }
      $this->collectNestedText($item, $chunks);
    }
  }

  /**
   * Returns the primary storage column for a field type.
   *
   * @param array $field_info
   *   Field info from field_info_field().
   *
   * @return string
   *   Primary column name, defaulting to "value".
   */
  protected function getPrimaryColumn(array $field_info) {
    if (!empty($field_info['columns']) && is_array($field_info['columns'])) {
      $keys = array_keys($field_info['columns']);
      if (!empty($keys[0])) {
        return $keys[0];
      }
    }
    return 'value';
  }

  /**
   * Calls openai_chat() with the configured provider/model and returns text.
   *
   * @param string $prompt
   *   The user prompt string.
   * @param array $config
   *   Rule config (keys: model, temperature, frequency_penalty, presence_penalty,
   *   top_p, max_tokens, reasoning_effort, image_field).
   *
   * @return string
   *   The AI-generated text, or empty string on error.
   */
  protected function callAI($prompt, array $config) {
    $model = $config['model'] ?? '';
    $temperature = (float) ($config['temperature'] ?? 0.7);
    $max_tokens = (int) ($config['max_tokens'] ?? 1024);

    if (empty($model) || empty($prompt)) {
      return '';
    }

    $messages = [
      ['role' => 'user', 'content' => $prompt],
    ];

    try {
      // For reasoning models, force temperature=1 (the only allowed value)
      // and pass reasoning_effort if set.
      $is_reasoning = function_exists('openai_field_automator_is_reasoning_model')
        && openai_field_automator_is_reasoning_model($model);

      if ($is_reasoning) {
        $temperature = 1.0;
        $reasoning_effort = $config['reasoning_effort'] ?? '';
        // Use the underlying API object directly to pass reasoning_effort.
        if (!empty($reasoning_effort) && function_exists('openai_get_api_for_model')) {
          $stripped_model = '';
          $api = openai_get_api_for_model($model, $stripped_model);
          if ($api && method_exists($api, 'chatWithOptions')) {
            $response = $api->chatWithOptions($stripped_model, $messages, [
              'temperature'      => $temperature,
              'max_tokens'       => $max_tokens,
              'reasoning_effort' => $reasoning_effort,
              'operation'        => 'field_generation',
            ]);
            if (is_string($response) && trim($response) !== '') {
              return trim($response);
            }
          }
        }
      }

      $response = openai_chat($model, $messages, $temperature, $max_tokens, FALSE, ['operation' => 'field_generation']);
      // All provider adapters return the content string directly.
      if (is_string($response) && trim($response) !== '') {
        return trim($response);
      }
      // Fallback: handle raw array format from any adapter that may return it.
      if (is_array($response) && !empty($response['choices'][0]['message']['content'])) {
        return trim($response['choices'][0]['message']['content']);
      }
    }
    catch (Exception $e) {
      watchdog('openai_field_automator', 'AI call failed: @msg', ['@msg' => $e->getMessage()], WATCHDOG_ERROR);
    }

    return '';
  }

  /**
   * Decodes AI text response into JSON array payload.
   *
   * Supports fenced code blocks and plain JSON arrays/objects.
   *
   * @param string $text
   *   Raw AI response text.
   *
   * @return array
   *   Normalized list of items.
   */
  protected function decodeJsonPayload($text) {
    if (!is_string($text) || trim($text) === '') {
      return [];
    }

    $clean = trim(str_replace(['```json', '```'], '', $text));
    $decoded = json_decode($clean, TRUE);
    if (!is_array($decoded)) {
      return [];
    }

    // Normalize object to a single-item list.
    if (isset($decoded['value']) || isset($decoded['uri']) || isset($decoded['title'])) {
      return [$decoded];
    }

    return $decoded;
  }

  /**
   * Creates and saves a managed file from binary data.
   *
   * @param string $binary
   *   File binary data.
   * @param string $filename
   *   Target filename.
   *
   * @return object|null
   *   Saved file entity-like object with fid, or NULL on failure.
   */
  protected function createManagedFile($binary, $filename) {
    if (!is_string($binary) || $binary === '') {
      return NULL;
    }

    $uri = file_unmanaged_save_data($binary, file_default_scheme() . '://' . $filename, FILE_EXISTS_RENAME);
    if (!$uri) {
      return NULL;
    }

    $file = (object) [
      'uid' => $GLOBALS['user']->uid ?? 0,
      'status' => 1,
      'filename' => basename($uri),
      'uri' => $uri,
      'filemime' => file_get_mimetype($uri),
      'timestamp' => REQUEST_TIME,
    ];
    file_save($file);
    return $file;
  }

}
