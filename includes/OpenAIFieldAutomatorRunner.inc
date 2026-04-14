<?php

/**
 * @file
 * Loads and runs automator configurations against an entity on save.
 *
 * Configuration is stored per-field as a Backdrop config object:
 *   openai_field_automator.{entity_type}.{bundle}.{field_name}
 */

class OpenAIFieldAutomatorRunner {

  /**
   * Process all configured automators for the fields of the given entity.
   *
   * @param string $entity_type
   *   The entity type (e.g. 'node').
   * @param object $entity
   *   The entity being saved (modified in-place).
   */
  public function processEntity($entity_type, $entity, array $options = []) {
    list(, , $bundle) = entity_extract_ids($entity_type, $entity);
    if (empty($bundle)) {
      return;
    }

    // Respect openai_privacy opt-out settings before running any automators.
    if (module_exists('openai_privacy') && !openai_privacy_entity_allowed($entity_type, $bundle)) {
      return;
    }

    $allowed_workers = $options['workers'] ?? ['direct', 'batch', 'queue_cron'];
    $specific_field = $options['specific_field'] ?? NULL;

    // Load all field instances for this bundle, then check which have a
    // saved automator config.
    $instances = field_info_instances($entity_type, $bundle);
    if (empty($instances)) {
      return;
    }

    // Collect enabled configs, keyed by weight so we can sort.
    $configs = [];
    foreach (array_keys($instances) as $field_name) {
      if ($field_name === OpenAIFieldAutomatorStatusField::FIELD_NAME) {
        continue;
      }
      $config = openai_field_automator_load_field_config($entity_type, $bundle, $field_name);
      if (!empty($config['enabled'])) {
        $configs[$field_name] = $config;
      }
    }

    if (empty($configs)) {
      return;
    }

    // Sort by weight ascending so dependent fields run in the right order.
    uasort($configs, function ($a, $b) {
      return ($a['weight'] ?? 0) - ($b['weight'] ?? 0);
    });

    $types = openai_field_automator_get_types();
    $had_error = FALSE;
    $status_started = FALSE;

    foreach ($configs as $field_name => $config) {
      if ($specific_field && $field_name !== $specific_field) {
        continue;
      }

      $worker_type = $config['worker_type'] ?? 'direct';
      if (!in_array($worker_type, $allowed_workers, TRUE)) {
        continue;
      }

      // Batch mode in Drupal relies on form batch context; for now we process
      // it as direct in backend workers and cron-safe paths.
      if ($worker_type === 'batch') {
        $worker_type = 'direct';
      }

      $type_id = $config['automator_type'] ?? '';
      $mode = $config['mode'] ?? 'base';

      if (!isset($types[$type_id])) {
        watchdog('openai_field_automator', 'Unknown automator type %type on field %field.', [
          '%type'  => $type_id,
          '%field' => $field_name,
        ], WATCHDOG_WARNING);
        continue;
      }

      $definition = $types[$type_id];
      $requires_base_field = !array_key_exists('requires_base_field', $definition) || !empty($definition['requires_base_field']);

      // In base mode we normally need context from the base field (or title pseudo-field).
      if ($mode === 'base' && $requires_base_field) {
        $base_field = $config['base_field'] ?? '';
        if (empty($base_field)) {
          continue;
        }
        if (!in_array($base_field, ['_title', '_rendered_entity'], TRUE) && !$this->fieldHasValue($entity, $base_field)) {
          continue;
        }
      }

      $plugin = openai_field_automator_load_plugin($type_id, $types[$type_id]);
      if (!$plugin) {
        continue;
      }

      $normalized_value = $this->normalizeFieldValueForProcessing($plugin, $entity, $field_name, $config);
      $has_value = !$this->isValueEmptyAfterCheck($normalized_value);

      // Migrate legacy boolean edit_mode values from old configs.
      $raw_edit_mode = $config['edit_mode'] ?? 'skip';
      if ($raw_edit_mode === TRUE || $raw_edit_mode === 1) {
        $edit_mode = 'changed';
      }
      elseif ($raw_edit_mode === FALSE || $raw_edit_mode === 0) {
        $edit_mode = 'skip';
      }
      else {
        $edit_mode = (string) $raw_edit_mode;
      }

      // Token mode never overwrites.
      if ($mode === 'token' && $has_value) {
        continue;
      }
      // 'skip' — never overwrite an existing value.
      if ($edit_mode === 'skip' && $has_value) {
        continue;
      }
      // 'changed' — only regenerate when the source field actually changed.
      if ($edit_mode === 'changed' && $has_value) {
        $base_field = $config['base_field'] ?? '';
        if (!$this->baseFieldChanged($entity, $base_field)) {
          continue;
        }
      }
      // 'always' — fall through; always regenerate regardless of existing value.

      if (!$plugin->ruleIsAllowed($entity, $field_name, $config)) {
        continue;
      }

      if (!$status_started) {
        OpenAIFieldAutomatorStatusField::setStatus($entity_type, $entity, OpenAIFieldAutomatorStatusField::STATUS_PROCESSING);
        $status_started = TRUE;
      }

      try {
        $values = $plugin->generate($entity, $field_name, $config);
        if (!empty($values)) {
          $values = $this->filterGeneratedValues($plugin, $entity, $field_name, $config, $values);
        }
        if (!empty($values)) {
          // Apply joiner: collapse multiple values into one if configured.
          $joiner = $config['joiner'] ?? '';
          if ($joiner !== '' && count($values) > 1) {
            if ($joiner === 'other') {
              $joiner = $config['joiner_other'] ?? '';
            }
            // Convert escape sequences to real characters.
            $joiner = str_replace(['\n', '\t'], ["\n", "\t"], $joiner);
            $values = [implode($joiner, $values)];
          }
          $plugin->storeValues($entity, $values, $field_name, $config);
        }
      }
      catch (Exception $e) {
        $had_error = TRUE;
        watchdog('openai_field_automator', 'Error running automator on %field: @msg', [
          '%field' => $field_name,
          '@msg'   => $e->getMessage(),
        ], WATCHDOG_ERROR);
      }
    }

    if (!$status_started) {
      return;
    }

    // Set final status.
    $final_status = $had_error
      ? OpenAIFieldAutomatorStatusField::STATUS_FAILED
      : OpenAIFieldAutomatorStatusField::STATUS_FINISHED;
    OpenAIFieldAutomatorStatusField::setStatus($entity_type, $entity, $final_status);
  }

  /**
   * Returns TRUE when a field has at least one non-empty item.
   */
  protected function fieldHasValue($entity, $field_name) {
    if (!isset($entity->{$field_name}[LANGUAGE_NONE])) {
      return FALSE;
    }
    foreach ($entity->{$field_name}[LANGUAGE_NONE] as $item) {
      $value = $item['value'] ?? $item['safe_value'] ?? $item['summary'] ?? '';
      if ($value === '') {
        foreach ($item as $candidate) {
          if (is_scalar($candidate) && trim((string) $candidate) !== '') {
            $value = $candidate;
            break;
          }
        }
      }
      if (is_scalar($value) && trim((string) $value) !== '') {
        return TRUE;
      }
    }
    return FALSE;
  }

  /**
   * Normalizes the current field value using plugin-aware empty checks.
   */
  protected function normalizeFieldValueForProcessing($plugin, $entity, $field_name, array $config) {
    $value = isset($entity->{$field_name}[LANGUAGE_NONE]) ? $entity->{$field_name}[LANGUAGE_NONE] : array();
    if ($plugin && method_exists($plugin, 'checkIfEmpty')) {
      $value = $plugin->checkIfEmpty($value, $config);
    }
    if ($plugin && method_exists($plugin, 'postCheckIfEmpty')) {
      $value = $plugin->postCheckIfEmpty($entity, $value, $config);
    }
    return $value;
  }

  /**
   * Returns TRUE when a normalized field value should count as empty.
   */
  protected function isValueEmptyAfterCheck($value) {
    if (is_array($value)) {
      if (empty($value)) {
        return TRUE;
      }
      foreach ($value as $item) {
        if (is_array($item)) {
          foreach ($item as $candidate) {
            if (is_scalar($candidate) && trim((string) $candidate) !== '') {
              return FALSE;
            }
          }
          continue;
        }
        if (is_scalar($item) && trim((string) $item) !== '') {
          return FALSE;
        }
      }
      return TRUE;
    }

    return empty($value);
  }

  /**
   * Removes generated values rejected by the plugin verifier.
   */
  protected function filterGeneratedValues($plugin, $entity, $field_name, array $config, array $values) {
    if (!$plugin || !method_exists($plugin, 'verifyValue')) {
      return $values;
    }

    foreach ($values as $key => $value) {
      if (!$plugin->verifyValue($entity, $value, $field_name, $config)) {
        unset($values[$key]);
      }
    }

    return array_values($values);
  }

  /**
   * Returns TRUE when the selected base field changed from original entity.
   */
  protected function baseFieldChanged($entity, $base_field) {
    if (empty($base_field) || in_array($base_field, ['_title', '_rendered_entity'], TRUE)) {
      return TRUE;
    }
    if (!isset($entity->original)) {
      return TRUE;
    }
    $current = $entity->{$base_field}[LANGUAGE_NONE] ?? [];
    $original = $entity->original->{$base_field}[LANGUAGE_NONE] ?? [];
    return json_encode($current) !== json_encode($original);
  }

  /**
   * Enqueues queue/cron automator work for an entity.
   */
  public function enqueueEntity($entity_type, $entity) {
    list($id, , $bundle) = entity_extract_ids($entity_type, $entity);
    if (empty($id) || empty($bundle)) {
      return;
    }

    $instances = field_info_instances($entity_type, $bundle);
    if (empty($instances)) {
      return;
    }

    $has_queue_work = FALSE;
    foreach (array_keys($instances) as $field_name) {
      $config = openai_field_automator_load_field_config($entity_type, $bundle, $field_name);
      if (empty($config['enabled']) || (($config['worker_type'] ?? 'direct') !== 'queue_cron')) {
        continue;
      }
      $has_queue_work = TRUE;
      break;
    }

    if (!$has_queue_work) {
      return;
    }

    $queue = BackdropQueue::get('openai_field_automator_queue');
    $queue->createItem([
      'entity_type' => $entity_type,
      'entity_id' => $id,
      'bundle' => $bundle,
    ]);
  }

}
