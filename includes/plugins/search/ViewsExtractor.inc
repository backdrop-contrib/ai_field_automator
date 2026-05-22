<?php


namespace ai_field_automator\includes\plugins\search;

use AIContentExtractor;
use ai_field_automator\includes\AIFieldAutomatorBase;

/**
 * @file
 * Views output extraction plugin.
 */
class AIFieldAutomatorViewsExtractor extends AIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t(
      'Executes a configured View display and stores the rendered output.'
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
  public function ruleIsAllowed($entity, $field_name, array $config) {
    return module_exists('views') && function_exists('views_get_view');
  }

  /**
   * {@inheritdoc}
   */
  public function generate($entity, $field_name, array $config) {
    if (!module_exists('views') || !function_exists('views_get_view')) {
      return [];
    }

    [$view_name, $display_id] = $this->parseViewReference(
      $config['views_view'] ?? ''
    );
    if ($view_name === '') {
      return [];
    }

    $view = views_get_view($view_name);
    if (!$view || !$view->set_display($display_id)
      || !$view->access(
        [$display_id]
      )
    ) {
      return [];
    }

    $args = $this->buildViewArguments($entity, $config);
    if (!empty($args)) {
      $view->set_arguments($args);
    }

    $filters = $this->buildViewExposedFilters($entity, $config);
    if (!empty($filters)) {
      $view->set_exposed_input($filters);
    }

    $output = $view->preview($display_id, $args);
    if (is_array($output)) {
      $output = backdrop_render($output);
    }
    $output = trim((string) $output);
    if ($output === '') {
      return [];
    }

    if (!empty($config['views_html_to_markdown'])) {
      $output = AIContentExtractor::htmlToMarkdown($output);
    }

    return $output !== '' ? [$output] : [];
  }

  /**
   * Parses "view_name:display_id" into parts.
   */
  protected function parseViewReference($value) {
    $value = trim((string) $value);
    if ($value === '') {
      return ['', 'default'];
    }

    $parts = explode(':', $value, 2);
    $view_name = trim($parts[0]);
    $display_id = trim($parts[1] ?? 'default');
    return [$view_name, $display_id !== '' ? $display_id : 'default'];
  }

  /**
   * Builds ordered argument values for a View display.
   */
  protected function buildViewArguments($entity, array $config) {
    $raw = $config['views_arguments'] ?? [];
    if (!is_array($raw)) {
      $raw = preg_split('/\R+/', trim((string) $raw), -1, PREG_SPLIT_NO_EMPTY);
    }

    $args = [];
    foreach ($raw as $arg) {
      $arg = trim((string) $arg);
      if ($arg === '') {
        continue;
      }
      $arg = ai_field_automator_render_token_prompt(
        $arg,
        $entity,
        $config['entity_type'] ?? ''
      );
      $args[] = $arg;
    }
    return $args;
  }

  /**
   * Builds exposed filter key/value array for a View display.
   */
  protected function buildViewExposedFilters($entity, array $config) {
    $raw = $config['views_exposed_filters'] ?? [];
    if (!is_array($raw)) {
      $lines = preg_split(
        '/\R+/',
        trim((string) $raw),
        -1,
        PREG_SPLIT_NO_EMPTY
      );
      $raw = [];
      foreach ($lines as $line) {
        $parts = explode('=', $line, 2);
        $key = trim((string) ($parts[0] ?? ''));
        $value = trim((string) ($parts[1] ?? ''));
        if ($key === '' || $value === '') {
          continue;
        }
        $raw[$key] = $value;
      }
    }

    $filters = [];
    foreach ($raw as $key => $value) {
      $key = trim((string) $key);
      $value = trim((string) $value);
      if ($key === '' || $value === '') {
        continue;
      }
      $filters[$key] = ai_field_automator_render_token_prompt(
        $value,
        $entity,
        $config['entity_type'] ?? ''
      );
    }

    return $filters;
  }

}
