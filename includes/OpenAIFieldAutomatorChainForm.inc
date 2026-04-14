<?php

/**
 * @file
 * Chain (ordering) form for automators on a given entity bundle.
 *
 * Shows all active automators with drag-and-drop weight reordering,
 * similar to Drupal's AiChainForm.
 */

/**
 * Form builder: automator chain overview for one bundle.
 */
function openai_field_automator_chain_form($form, &$form_state, $entity_type, $bundle) {
  // When called from a local task on a content type page, the $entity_type
  // arg is the literal string 'node' and $bundle may be a loaded node type
  // object (from %node_type menu loader) or a plain string.
  if (is_object($bundle)) {
    // Node type objects have ->type property.
    $bundle = isset($bundle->type) ? $bundle->type : (string) $bundle;
  }

  $form['#entity_type'] = $entity_type;
  $form['#bundle'] = $bundle;

  $bundle_label = $bundle;
  if ($entity_type === 'node') {
    $type_info = node_type_get_type($bundle);
    if ($type_info) {
      $bundle_label = $type_info->name;
    }
  }

  $form['introduction'] = array(
    '#type' => 'markup',
    '#markup' => '<p>' . t('This page shows all AI Automators configured on <strong>@bundle</strong> (@entity_type) and allows you to reorder them. Automators execute in weight order (lowest first) during entity save.', array(
      '@bundle' => $bundle_label,
      '@entity_type' => $entity_type,
    )) . '</p>',
  );

  // Collect all enabled configs for this bundle.
  $instances = field_info_instances($entity_type, $bundle);
  $configs = array();
  if (!empty($instances)) {
    foreach (array_keys($instances) as $field_name) {
      if ($field_name === OpenAIFieldAutomatorStatusField::FIELD_NAME) {
        continue;
      }
      $config = openai_field_automator_load_field_config($entity_type, $bundle, $field_name);
      if (!empty($config['enabled'])) {
        $configs[$field_name] = $config;
      }
    }
  }

  if (empty($configs)) {
    $form['empty'] = array(
      '#type' => 'markup',
      '#markup' => '<p>' . t('No AI Automators are configured on this content type yet. Edit a field to enable one.') . '</p>',
    );
    return $form;
  }

  // Sort by weight.
  uasort($configs, function ($a, $b) {
    return ($a['weight'] ?? 0) - ($b['weight'] ?? 0);
  });

  // Get type definitions for labels.
  $types = openai_field_automator_get_types();

  // Build a tabledrag form element.
  $form['items'] = array(
    '#tree' => TRUE,
  );

  foreach ($configs as $field_name => $config) {
    $type_id = $config['automator_type'] ?? '';
    $type_label = isset($types[$type_id]['label']) ? $types[$type_id]['label'] : $type_id;
    $mode = $config['mode'] ?? 'base';
    $source = ($mode === 'token') ? t('Token') : ($config['base_field'] ?? '');
    $worker = $config['worker_type'] ?? 'direct';
    $automator_label = !empty($config['label']) ? $config['label'] : '';

    // Field label from instance info.
    $instance = field_info_instance($entity_type, $field_name, $bundle);
    $field_label = $instance ? $instance['label'] : $field_name;

    $form['items'][$field_name] = array(
      'label' => array(
        '#markup' => check_plain($automator_label ? $automator_label : $field_label),
      ),
      'field_label' => array(
        '#markup' => check_plain($field_label) . ' <small>(' . check_plain($field_name) . ')</small>',
      ),
      'automator_type' => array(
        '#markup' => check_plain($type_label),
      ),
      'source' => array(
        '#markup' => check_plain($source),
      ),
      'worker' => array(
        '#markup' => check_plain($worker),
      ),
      'weight' => array(
        '#type' => 'weight',
        '#title' => t('Weight for @field', array('@field' => $field_label)),
        '#title_display' => 'invisible',
        '#default_value' => $config['weight'] ?? 0,
        '#delta' => 100,
        '#attributes' => array('class' => array('automator-weight')),
      ),
      'operations' => array(
        '#markup' => _openai_field_automator_chain_operations_link($entity_type, $bundle, $field_name),
      ),
    );
  }

  $form['actions'] = array(
    '#type' => 'actions',
  );
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save order'),
  );

  return $form;
}

/**
 * Submit handler: saves the new weight order.
 */
function openai_field_automator_chain_form_submit($form, &$form_state) {
  $entity_type = $form['#entity_type'];
  $bundle = $form['#bundle'];
  $items = $form_state['values']['items'] ?? array();

  foreach ($items as $field_name => $vals) {
    $weight = (int) ($vals['weight'] ?? 0);
    $config_name = "openai_field_automator.{$entity_type}.{$bundle}.{$field_name}";
    $cfg = config($config_name);
    if ($cfg->get('enabled')) {
      $cfg->set('weight', $weight);
      $cfg->save();
    }
  }

  backdrop_set_message(t('Automator execution order has been saved.'));
}

/**
 * Renders the chain form items as a tabledrag table.
 */
function theme_openai_field_automator_chain_table($variables) {
  $element = $variables['element'];

  $header = array(
    t('Instruction Name'),
    t('Manipulated Field'),
    t('Automator Type'),
    t('Source'),
    t('Worker'),
    t('Weight'),
    t('Operations'),
  );

  $rows = array();
  foreach (element_children($element) as $field_name) {
    $row = &$element[$field_name];
    $rows[] = array(
      'data' => array(
        backdrop_render($row['label']),
        backdrop_render($row['field_label']),
        backdrop_render($row['automator_type']),
        backdrop_render($row['source']),
        backdrop_render($row['worker']),
        backdrop_render($row['weight']),
        backdrop_render($row['operations']),
      ),
      'class' => array('draggable'),
    );
  }

  backdrop_add_tabledrag('automator-chain-table', 'order', 'sibling', 'automator-weight');

  return theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'attributes' => array('id' => 'automator-chain-table'),
    'empty' => t('No automators configured.'),
  ));
}

/**
 * Returns an operations link for a field in the chain table.
 *
 * Uses entity_get_info() to resolve the admin path for any fieldable entity
 * type, so taxonomy terms, comments, users, etc. all get proper Edit links.
 */
function _openai_field_automator_chain_operations_link($entity_type, $bundle, $field_name) {
  $links = array();

  $entity_info = entity_get_info($entity_type);
  if (!empty($entity_info['bundles'][$bundle]['admin']['path'])) {
    $admin_path = $entity_info['bundles'][$bundle]['admin']['path'];
    // Replace any wildcard placeholder (e.g. %node_type) with the bundle name.
    $admin_path = preg_replace('/%[a-z_]+/', $bundle, $admin_path);
    $edit_path = $admin_path . '/fields/' . $field_name;
    $links[] = l(t('Edit'), $edit_path, array(
      'query' => backdrop_get_destination(),
    ));
  }

  return implode(' | ', $links);
}

