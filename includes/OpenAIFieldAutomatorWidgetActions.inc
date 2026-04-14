<?php

/**
 * @file
 * On-demand field widget actions for OpenAI Field Automator.
 */

/**
 * Implements hook_field_widget_form_alter() logic.
 */
function _openai_field_automator_field_widget_form_alter(&$element, &$form_state, $context) {
  $field = $context['field'] ?? [];
  $instance = $context['instance'] ?? [];
  $delta = isset($context['delta']) ? (int) $context['delta'] : 0;
  if (empty($field) || empty($instance) || $delta !== 0) {
    return;
  }

  $entity_type = $instance['entity_type'] ?? '';
  $bundle = $instance['bundle'] ?? '';
  $field_name = $instance['field_name'] ?? '';
  if (empty($entity_type) || empty($bundle) || empty($field_name)) {
    return;
  }
  if ($field_name === OpenAIFieldAutomatorStatusField::FIELD_NAME) {
    return;
  }

  $config = openai_field_automator_load_field_config($entity_type, $bundle, $field_name);
  if (empty($config['enabled'])) {
    return;
  }
  if (($config['worker_type'] ?? 'direct') !== 'widget_action') {
    return;
  }

  // MVP support: text-like destination fields where widget value replacement is
  // predictable in Backdrop form arrays.
  $supported_field_types = ['text', 'text_long', 'text_with_summary', 'string', 'string_long', 'email', 'telephone', 'taxonomy_term_reference'];
  $field_type = $field['type'] ?? '';
  if (!in_array($field_type, $supported_field_types, TRUE)) {
    return;
  }

  // Re-apply generated values on rebuild so AJAX visibly updates widget fields.
  if (!empty($form_state['openai_field_automator_generated'][$field_name][LANGUAGE_NONE])) {
    $generated_items = $form_state['openai_field_automator_generated'][$field_name][LANGUAGE_NONE];
    if ($field_type === 'taxonomy_term_reference') {
      _openai_field_automator_widget_apply_generated_taxonomy($element, $generated_items);
    }
    elseif (!empty($generated_items[$delta]) && is_array($generated_items[$delta])) {
      _openai_field_automator_widget_apply_generated_item($element, $generated_items[$delta]);
    }
  }

  $wrapper_id = 'openai-field-automator-widget-' . $entity_type . '-' . $bundle . '-' . $field_name . '-' . $delta;
  $wrapper_id = drupal_html_id($wrapper_id);

  $existing_prefix = isset($element['#prefix']) ? $element['#prefix'] : '';
  $existing_suffix = isset($element['#suffix']) ? $element['#suffix'] : '';
  $element['#prefix'] = '<div id="' . check_plain($wrapper_id) . '">' . $existing_prefix;
  $element['#suffix'] = $existing_suffix . '</div>';

  $element['openai_field_automator_actions'] = [
    '#type' => 'container',
    '#weight' => 1000,
  ];

  // Check whether there is a pending async job for this field.
  // If the job finished since the last rebuild, apply its result and clear it.
  $pending_job_id = NULL;
  if (module_exists('openai_async')) {
    $stored_job_id = isset($form_state['storage']['openai_async_jobs'][$field_name])
      ? $form_state['storage']['openai_async_jobs'][$field_name]
      : NULL;

    if ($stored_job_id) {
      $job = openai_async_get_job($stored_job_id);
      if ($job && $job->status === 'complete') {
        // Job done — apply result to form_state and clear the stored ID.
        unset($form_state['storage']['openai_async_jobs'][$field_name]);
        $result = $job->getResult();
        if (!empty($result['values'])) {
          if ($result['field_type'] === 'taxonomy_term_reference') {
            $form_state['openai_field_automator_generated'][$field_name][LANGUAGE_NONE] = $result['values'];
          }
          else {
            $form_state['input'][$field_name][LANGUAGE_NONE]     = $result['values'];
            $form_state['values'][$field_name][LANGUAGE_NONE]    = $result['values'];
            $form_state['openai_field_automator_generated'][$field_name][LANGUAGE_NONE] = $result['values'];
          }
        }
      }
      elseif (!$job || $job->status === 'error') {
        // Job errored or disappeared — clear so we don't get stuck.
        unset($form_state['storage']['openai_async_jobs'][$field_name]);
        if ($job && $job->error) {
          backdrop_set_message(t('AI generation failed for @field: @msg', [
            '@field' => $field_name,
            '@msg'   => $job->error,
          ]), 'error');
        }
      }
      else {
        // Still pending/processing.
        $pending_job_id = $stored_job_id;
      }
    }
  }

  $button_disabled = ($pending_job_id !== NULL);
  $button_attrs = ['class' => ['button-small', 'openai-field-automator-widget-generate']];
  if ($button_disabled) {
    $button_attrs['disabled'] = 'disabled';
    $button_attrs['data-openai-async-job-id'] = $pending_job_id;
    $button_attrs['data-openai-async-wrapper-id'] = $wrapper_id;
    $button_attrs['data-openai-async-field-name'] = $field_name;
    $button_attrs['data-openai-async-field-type'] = $field_type;
    $button_attrs['data-openai-async-delta'] = (string) $delta;
    // Attach openai_async polling library, then our behavior.
    $element['#attached']['js'][] = backdrop_get_path('module', 'openai_async') . '/js/openai_async.js';
    $element['#attached']['js'][] = backdrop_get_path('module', 'openai_field_automator') . '/js/openai_field_automator_async.js';
    $element['#attached']['js'][] = [
      'type' => 'setting',
      'data' => ['openai_field_automator_async' => ['statusUrl' => url('openai/async/status/')]],
    ];
  }

  $element['openai_field_automator_actions']['openai_field_automator_generate'] = [
    '#type' => 'submit',
    '#value' => $button_disabled ? t('Generating…') : t('Generate with AI'),
    '#name' => 'openai_field_automator_generate_' . $field_name . '_' . $delta,
    '#submit' => ['openai_field_automator_widget_action_submit'],
    '#limit_validation_errors' => [],
    '#ajax' => [
      'callback' => 'openai_field_automator_widget_action_ajax',
      'wrapper' => $wrapper_id,
      'method' => 'replace',
      'effect' => 'fade',
    ],
    '#attributes' => $button_attrs,
    '#openai_field_automator' => [
      'entity_type' => $entity_type,
      'bundle' => $bundle,
      'field_name' => $field_name,
      'delta' => $delta,
    ],
  ];

  // Show spinner markup when job is in flight.
  if ($button_disabled) {
    $element['openai_field_automator_actions']['openai_field_automator_spinner'] = [
      '#markup' => '<span class="openai-field-automator-spinner ajax-progress ajax-progress-throbber"><span class="throbber">&nbsp;</span></span>',
      '#weight' => 1001,
    ];
  }
}

/**
 * Submit callback for on-demand widget action generation.
 */
function _openai_field_automator_widget_action_submit($form, &$form_state) {
  $trigger = $form_state['triggering_element'] ?? NULL;
  $meta = $trigger['#openai_field_automator'] ?? NULL;
  if (empty($meta['entity_type']) || empty($meta['bundle']) || empty($meta['field_name'])) {
    return;
  }

  $entity_type = $meta['entity_type'];
  $bundle = $meta['bundle'];
  $field_name = $meta['field_name'];
  $delta = isset($meta['delta']) ? (int) $meta['delta'] : 0;

  // Respect openai_privacy opt-out before doing any work.
  if (module_exists('openai_privacy') && !openai_privacy_entity_allowed($entity_type, $bundle)) {
    backdrop_set_message(t('AI processing is not permitted for this content type.'), 'warning');
    $form_state['rebuild'] = TRUE;
    return;
  }

  $config = openai_field_automator_load_field_config($entity_type, $bundle, $field_name);
  if (empty($config['enabled']) || empty($config['automator_type'])) {
    backdrop_set_message(t('No enabled automator configuration found for @field.', ['@field' => $field_name]), 'warning');
    $form_state['rebuild'] = TRUE;
    return;
  }

  $types = openai_field_automator_get_types();
  $type_id = $config['automator_type'];
  if (empty($types[$type_id])) {
    backdrop_set_message(t('Automator type %type is not available.', ['%type' => $type_id]), 'error');
    $form_state['rebuild'] = TRUE;
    return;
  }

  $plugin = openai_field_automator_load_plugin($type_id, $types[$type_id]);
  if (!$plugin) {
    backdrop_set_message(t('Could not load automator plugin for @field.', ['@field' => $field_name]), 'error');
    $form_state['rebuild'] = TRUE;
    return;
  }

  $entity = _openai_field_automator_widget_action_build_entity($form, $form_state, $entity_type, $bundle);
  if (!$entity) {
    backdrop_set_message(t('Could not build entity context for field generation.'), 'error');
    $form_state['rebuild'] = TRUE;
    return;
  }

  // Async path: for saved entities when openai_async is available, dispatch
  // to a background worker so the AI call doesn't block the page request.
  $entity_id = function_exists('entity_id') ? entity_id($entity_type, $entity) : NULL;
  if ($entity_id && module_exists('openai_async')) {
    $wrapper_id = 'openai-field-automator-widget-' . $entity_type . '-' . $bundle . '-' . $field_name . '-' . $delta;
    $wrapper_id = drupal_html_id($wrapper_id);

    $job_id = openai_async_submit('openai_field_automator_async_generate', array(
      'entity_type' => $entity_type,
      'entity_id'   => $entity_id,
      'bundle'      => $bundle,
      'field_name'  => $field_name,
      'config'      => $config,
      'wrapper_id'  => $wrapper_id,
      'delta'       => $delta,
      'account_uid' => $GLOBALS['user']->uid ?? 0,
    ));

    if ($job_id) {
      if (!isset($form_state['storage']) || !is_array($form_state['storage'])) {
        $form_state['storage'] = array();
      }
      if (!isset($form_state['storage']['openai_async_jobs']) || !is_array($form_state['storage']['openai_async_jobs'])) {
        $form_state['storage']['openai_async_jobs'] = array();
      }
      $form_state['storage']['openai_async_jobs'][$field_name] = $job_id;
      $form_state['rebuild'] = TRUE;
      return;
    }
    // If job submission failed, fall through to the synchronous path.
  }

  try {
    $values = $plugin->generate($entity, $field_name, $config);
    $values = openai_field_automator_filter_generated_values($plugin, $entity, $field_name, $config, $values);
    if (empty($values)) {
      backdrop_set_message(t('No generated values were returned for @field.', ['@field' => $field_name]), 'warning');
      $form_state['rebuild'] = TRUE;
      return;
    }

    // Keep parity with save-time runner joiner behavior.
    $joiner = $config['joiner'] ?? '';
    if ($joiner !== '' && count($values) > 1) {
      if ($joiner === 'other') {
        $joiner = $config['joiner_other'] ?? '';
      }
      $joiner = str_replace(['\n', '\t'], ["\n", "\t"], $joiner);
      $values = [implode($joiner, $values)];
    }

    $plugin->storeValues($entity, $values, $field_name, $config);

    $items = $entity->{$field_name}[LANGUAGE_NONE] ?? [];
    if (empty($items)) {
      backdrop_set_message(t('No storable values were generated for @field.', ['@field' => $field_name]), 'warning');
      $form_state['rebuild'] = TRUE;
      return;
    }

    // If a specific delta was requested, map first generated value to that delta.
    if ($delta > 0 && !isset($items[$delta]) && isset($items[0])) {
      $items = [$delta => $items[0]];
    }

    $field_info = field_info_field($field_name);
    $field_type = $field_info['type'] ?? '';
    if ($field_type === 'taxonomy_term_reference') {
      // Taxonomy autocomplete widget is a textfield; do not inject structured
      // field item arrays into form input/values or it will render as "Array".
      $text = _openai_field_automator_generated_taxonomy_text($items);
      if (!isset($form_state['input']) || !is_array($form_state['input'])) {
        $form_state['input'] = [];
      }
      if (!isset($form_state['values']) || !is_array($form_state['values'])) {
        $form_state['values'] = [];
      }
      $form_state['input'][$field_name][LANGUAGE_NONE] = $text;
      $form_state['values'][$field_name][LANGUAGE_NONE] = $text;
      $form_state['openai_field_automator_generated'][$field_name][LANGUAGE_NONE] = $items;
      $form_state['openai_field_automator_generated_taxonomy_text'][$field_name] = $text;
      backdrop_set_message(t('Generated new value for @field.', ['@field' => $field_name]));
      $form_state['rebuild'] = TRUE;
      return;
    }

    if (!isset($form_state['input']) || !is_array($form_state['input'])) {
      $form_state['input'] = [];
    }
    if (!isset($form_state['values']) || !is_array($form_state['values'])) {
      $form_state['values'] = [];
    }
    $form_state['input'][$field_name][LANGUAGE_NONE] = $items;
    $form_state['values'][$field_name][LANGUAGE_NONE] = $items;
    $form_state['openai_field_automator_generated'][$field_name][LANGUAGE_NONE] = $items;
    backdrop_set_message(t('Generated new value for @field.', ['@field' => $field_name]));
  }
  catch (Exception $e) {
    watchdog('openai_field_automator', 'Widget action failed for %field: @msg', [
      '%field' => $field_name,
      '@msg' => $e->getMessage(),
    ], WATCHDOG_ERROR);
    backdrop_set_message(t('AI generation failed for @field: @msg', [
      '@field' => $field_name,
      '@msg' => $e->getMessage(),
    ]), 'error');
  }

  $form_state['rebuild'] = TRUE;
}

/**
 * AJAX callback for on-demand widget actions.
 */
function _openai_field_automator_widget_action_ajax($form, &$form_state) {
  $trigger = $form_state['triggering_element'] ?? NULL;
  if (!$trigger || empty($trigger['#array_parents']) || !is_array($trigger['#array_parents'])) {
    return $form;
  }

  $parents = $trigger['#array_parents'];
  array_pop($parents);
  array_pop($parents);
  $element = backdrop_array_get_nested_value($form, $parents);
  return $element ? $element : $form;
}

/**
 * Builds a best-effort entity object from current form state values.
 */
function _openai_field_automator_widget_action_build_entity($form, &$form_state, $entity_type, $bundle) {
  $entity = NULL;
  if ($entity_type === 'node') {
    if (!empty($form_state['node']) && is_object($form_state['node'])) {
      $entity = clone $form_state['node'];
    }
    elseif (!empty($form['#node']) && is_object($form['#node'])) {
      $entity = clone $form['#node'];
    }
  }
  elseif ($entity_type === 'taxonomy_term') {
    if (!empty($form_state['term']) && is_object($form_state['term'])) {
      $entity = clone $form_state['term'];
    }
    elseif (!empty($form['#term']) && is_object($form['#term'])) {
      $entity = clone $form['#term'];
    }
  }

  if (!$entity || !is_object($entity)) {
    return NULL;
  }

  $values = $form_state['values'] ?? [];
  if (!is_array($values)) {
    $values = [];
  }

  // Apply title/label from unsaved form values when available.
  if ($entity_type === 'node' && isset($values['title']) && is_scalar($values['title'])) {
    $entity->title = (string) $values['title'];
  }
  if ($entity_type === 'taxonomy_term' && isset($values['name']) && is_scalar($values['name'])) {
    $entity->name = (string) $values['name'];
  }

  // Overlay current form field values onto the entity for accurate prompt
  // context in on-demand generation.
  $instances = field_info_instances($entity_type, $bundle);
  foreach ($instances as $field_name => $instance) {
    if (isset($values[$field_name]) && is_array($values[$field_name])) {
      $entity->{$field_name} = $values[$field_name];
    }
  }

  // Ensure rendered-entity context can use unsaved form text.
  if ($entity_type === 'node') {
    $preview = openai_field_automator_extract_node_preview_text_from_form_state($values, $bundle);
    if ($preview !== '') {
      $entity->openai_field_automator_preview_text = $preview;
    }
  }

  return $entity;
}

/**
 * Applies one generated field item onto a widget element on rebuild.
 */
function _openai_field_automator_widget_apply_generated_item(array &$element, array $item) {
  if (array_key_exists('value', $item)) {
    if (isset($element['value']['#default_value'])) {
      $element['value']['#default_value'] = $item['value'];
    }
    if (isset($element['#default_value'])) {
      $element['#default_value'] = $item['value'];
    }
  }

  if (array_key_exists('summary', $item) && isset($element['summary']['#default_value'])) {
    $element['summary']['#default_value'] = $item['summary'];
  }

  if (array_key_exists('format', $item) && isset($element['#format'])) {
    $element['#format'] = $item['format'];
  }
}

/**
 * Applies generated taxonomy term items onto taxonomy autocomplete widget.
 */
function _openai_field_automator_widget_apply_generated_taxonomy(array &$element, array $items) {
  $text = _openai_field_automator_generated_taxonomy_text($items);
  if (isset($element['#default_value'])) {
    $element['#default_value'] = $text;
  }
  if (isset($element['#value'])) {
    $element['#value'] = $text;
  }

  if ($text !== '') {
    return;
  }

  $tags = [];
  foreach ($items as $item) {
    if (empty($item['tid'])) {
      continue;
    }
    $term = taxonomy_term_load((int) $item['tid']);
    if ($term) {
      $tags[$term->tid] = $term;
    }
  }
  if (!empty($tags) && isset($element['#default_value'])) {
    $element['#default_value'] = taxonomy_implode_tags($tags);
  }
}

/**
 * Converts generated taxonomy term items into autocomplete text.
 */
function _openai_field_automator_generated_taxonomy_text(array $items) {
  $tags = [];
  foreach ($items as $item) {
    if (empty($item['tid'])) {
      continue;
    }
    $term = taxonomy_term_load((int) $item['tid']);
    if ($term) {
      $tags[$term->tid] = $term;
    }
  }
  return taxonomy_implode_tags($tags);
}

/**
 * Adds taxonomy widget-action buttons on node forms at field-container level.
 *
 * Taxonomy autocomplete widgets are single textfield elements; child action
 * elements are not reliably rendered inside them, so add button as sibling.
 */
function _openai_field_automator_node_form_add_taxonomy_widget_action_buttons(&$form, &$form_state) {
  if (empty($form['#node']) || !is_object($form['#node']) || empty($form['#node']->type)) {
    return;
  }
  $bundle = $form['#node']->type;
  $instances = field_info_instances('node', $bundle);
  if (empty($instances)) {
    return;
  }

  foreach ($instances as $field_name => $instance) {
    if ($field_name === OpenAIFieldAutomatorStatusField::FIELD_NAME || empty($form[$field_name])) {
      continue;
    }
    $field = field_info_field($field_name);
    if (empty($field) || ($field['type'] ?? '') !== 'taxonomy_term_reference') {
      continue;
    }
    $config = openai_field_automator_load_field_config('node', $bundle, $field_name);
    if (empty($config['enabled']) || (($config['worker_type'] ?? 'direct') !== 'widget_action')) {
      continue;
    }
    if (!empty($form[$field_name]['openai_field_automator_actions'])) {
      continue;
    }

    $wrapper_id = drupal_html_id('openai-field-automator-node-taxonomy-' . $bundle . '-' . $field_name);
    $existing_prefix = $form[$field_name]['#prefix'] ?? '';
    $existing_suffix = $form[$field_name]['#suffix'] ?? '';
    $form[$field_name]['#prefix'] = '<div id="' . check_plain($wrapper_id) . '">' . $existing_prefix;
    $form[$field_name]['#suffix'] = $existing_suffix . '</div>';

    $form[$field_name]['openai_field_automator_actions'] = [
      '#type' => 'container',
      '#weight' => 1000,
    ];

    // Check for a pending async job for this taxonomy field.
    $tax_pending_job_id = NULL;
    if (module_exists('openai_async')) {
      $tax_stored_job_id = isset($form_state['storage']['openai_async_jobs'][$field_name])
        ? $form_state['storage']['openai_async_jobs'][$field_name]
        : NULL;
      if ($tax_stored_job_id) {
        $tax_job = openai_async_get_job($tax_stored_job_id);
        if ($tax_job && $tax_job->status === 'complete') {
          unset($form_state['storage']['openai_async_jobs'][$field_name]);
          $tax_result = $tax_job->getResult();
          if (!empty($tax_result['values'])) {
            $form_state['openai_field_automator_generated'][$field_name][LANGUAGE_NONE] = $tax_result['values'];
          }
        }
        elseif (!$tax_job || $tax_job->status === 'error') {
          unset($form_state['storage']['openai_async_jobs'][$field_name]);
          if ($tax_job && $tax_job->error) {
            backdrop_set_message(t('AI generation failed for @field: @msg', [
              '@field' => $field_name,
              '@msg'   => $tax_job->error,
            ]), 'error');
          }
        }
        else {
          $tax_pending_job_id = $tax_stored_job_id;
        }
      }
    }

    $tax_button_disabled = ($tax_pending_job_id !== NULL);
    $tax_button_attrs = ['class' => ['button-small', 'openai-field-automator-widget-generate']];
    if ($tax_button_disabled) {
      $tax_button_attrs['disabled'] = 'disabled';
      $tax_button_attrs['data-openai-async-job-id'] = $tax_pending_job_id;
      $tax_button_attrs['data-openai-async-wrapper-id'] = $wrapper_id;
      $tax_button_attrs['data-openai-async-field-name'] = $field_name;
      $tax_button_attrs['data-openai-async-field-type'] = 'taxonomy_term_reference';
      $tax_button_attrs['data-openai-async-delta'] = '0';
      $form[$field_name]['#attached']['js'][] = backdrop_get_path('module', 'openai_async') . '/js/openai_async.js';
      $form[$field_name]['#attached']['js'][] = backdrop_get_path('module', 'openai_field_automator') . '/js/openai_field_automator_async.js';
      $form[$field_name]['#attached']['js'][] = [
        'type' => 'setting',
        'data' => ['openai_field_automator_async' => ['statusUrl' => url('openai/async/status/')]],
      ];
    }

    $form[$field_name]['openai_field_automator_actions']['openai_field_automator_generate'] = [
      '#type' => 'submit',
      '#value' => $tax_button_disabled ? t('Generating…') : t('Generate with AI'),
      '#name' => 'openai_field_automator_generate_' . $field_name . '_taxonomy',
      '#submit' => ['openai_field_automator_widget_action_submit'],
      '#limit_validation_errors' => [],
      '#ajax' => [
        'callback' => 'openai_field_automator_widget_action_ajax',
        'wrapper' => $wrapper_id,
        'method' => 'replace',
        'effect' => 'fade',
      ],
      '#attributes' => $tax_button_attrs,
      '#openai_field_automator' => [
        'entity_type' => 'node',
        'bundle' => $bundle,
        'field_name' => $field_name,
        'delta' => 0,
      ],
    ];

    if ($tax_button_disabled) {
      $form[$field_name]['openai_field_automator_actions']['openai_field_automator_spinner'] = [
        '#markup' => '<span class="openai-field-automator-spinner ajax-progress ajax-progress-throbber"><span class="throbber">&nbsp;</span></span>',
        '#weight' => 1001,
      ];
    }
  }
}
