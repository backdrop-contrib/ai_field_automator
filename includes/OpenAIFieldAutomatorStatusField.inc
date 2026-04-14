<?php

/**
 * @file
 * Manages the ai_automator_status field lifecycle on entity bundles.
 *
 * Automatically creates/removes a list_text field named
 * 'ai_automator_status' on any entity bundle that has (or no longer has)
 * active automator configurations. Status values:
 *   - pending    : Entity is awaiting automator processing.
 *   - processing : Automators are currently running.
 *   - failed     : One or more automators encountered an error.
 *   - finished   : All automators completed successfully.
 */

class OpenAIFieldAutomatorStatusField {

  /**
   * The field machine name.
   */
  const FIELD_NAME = 'ai_automator_status';

  /**
   * Status constants.
   */
  const STATUS_PENDING = 'pending';
  const STATUS_PROCESSING = 'processing';
  const STATUS_FAILED = 'failed';
  const STATUS_FINISHED = 'finished';

  /**
   * Checks if the status field is needed and creates or removes it.
   *
   * Call this after saving/deleting an automator config so the field is
   * kept in sync with whether any automators exist on the bundle.
   *
   * @param string $entity_type
   *   The entity type (e.g. 'node').
   * @param string $bundle
   *   The bundle machine name.
   *
   * @return bool
   *   TRUE on success.
   */
  public static function modifyStatusField($entity_type, $bundle) {
    $needed = self::automatorFieldNeeded($entity_type, $bundle);
    $exists = self::automatorFieldExists($entity_type, $bundle);

    if ($needed && !$exists) {
      self::addStatusField($entity_type, $bundle);
    }
    elseif (!$needed && $exists) {
      self::removeStatusField($entity_type, $bundle);
    }

    return TRUE;
  }

  /**
   * Returns TRUE if any enabled automator configs exist for this bundle.
   */
  public static function automatorFieldNeeded($entity_type, $bundle) {
    $instances = field_info_instances($entity_type, $bundle);
    if (empty($instances)) {
      return FALSE;
    }
    foreach (array_keys($instances) as $field_name) {
      $config = openai_field_automator_load_field_config($entity_type, $bundle, $field_name);
      if (!empty($config['enabled'])) {
        return TRUE;
      }
    }
    return FALSE;
  }

  /**
   * Returns TRUE if the status field already exists on this bundle.
   */
  public static function automatorFieldExists($entity_type, $bundle) {
    $instance = field_read_instance($entity_type, self::FIELD_NAME, $bundle);
    return !empty($instance);
  }

  /**
   * Creates the ai_automator_status field and instance.
   */
  protected static function addStatusField($entity_type, $bundle) {
    // Create the base field if it doesn't exist yet for this entity type.
    $field = field_read_field(self::FIELD_NAME);
    if (empty($field)) {
      $field = array(
        'field_name' => self::FIELD_NAME,
        'type' => 'list_text',
        'module' => 'list',
        'settings' => array(
          'allowed_values' => array(
            self::STATUS_PENDING => 'Pending',
            self::STATUS_PROCESSING => 'Processing',
            self::STATUS_FAILED => 'Failed',
            self::STATUS_FINISHED => 'Finished',
          ),
        ),
        'locked' => TRUE,
        'cardinality' => 1,
        'translatable' => FALSE,
      );
      try {
        field_create_field($field);
      }
      catch (FieldException $e) {
        watchdog('openai_field_automator', 'Could not create status field storage: @msg', array(
          '@msg' => $e->getMessage(),
        ), WATCHDOG_ERROR);
        return;
      }
    }

    // Create the instance on this bundle.
    $instance = array(
      'field_name' => self::FIELD_NAME,
      'entity_type' => $entity_type,
      'bundle' => $bundle,
      'label' => t('AI Automator Status'),
      'description' => t('Tracks the current status of AI automator processing on this entity.'),
      'required' => FALSE,
      'default_value' => array(
        array('value' => self::STATUS_PENDING),
      ),
      'widget' => array(
        'type' => 'options_select',
        'weight' => 100,
      ),
      'display' => array(
        'default' => array(
          'label' => 'above',
          'type' => 'list_default',
          'weight' => 100,
        ),
      ),
    );
    try {
      field_create_instance($instance);
      watchdog('openai_field_automator', 'Created AI Automator Status field on %type/%bundle.', array(
        '%type' => $entity_type,
        '%bundle' => $bundle,
      ), WATCHDOG_INFO);
    }
    catch (FieldException $e) {
      watchdog('openai_field_automator', 'Could not create status field instance on %type/%bundle: @msg', array(
        '%type' => $entity_type,
        '%bundle' => $bundle,
        '@msg' => $e->getMessage(),
      ), WATCHDOG_ERROR);
    }
  }

  /**
   * Removes the status field instance from a bundle.
   *
   * If no other bundles use the field, the base field is also removed.
   */
  protected static function removeStatusField($entity_type, $bundle) {
    $instance = field_read_instance($entity_type, self::FIELD_NAME, $bundle);
    if (empty($instance)) {
      return;
    }

    try {
      field_delete_instance($instance, TRUE);
      watchdog('openai_field_automator', 'Removed AI Automator Status field from %type/%bundle.', array(
        '%type' => $entity_type,
        '%bundle' => $bundle,
      ), WATCHDOG_INFO);
    }
    catch (Exception $e) {
      watchdog('openai_field_automator', 'Could not remove status field from %type/%bundle: @msg', array(
        '%type' => $entity_type,
        '%bundle' => $bundle,
        '@msg' => $e->getMessage(),
      ), WATCHDOG_ERROR);
    }
  }

  /**
   * Sets the automator status on an entity.
   *
   * @param string $entity_type
   *   The entity type.
   * @param object $entity
   *   The entity object (modified in-place).
   * @param string $status
   *   One of the STATUS_* constants.
   */
  public static function setStatus($entity_type, $entity, $status) {
    list(, , $bundle) = entity_extract_ids($entity_type, $entity);
    if (!self::automatorFieldExists($entity_type, $bundle)) {
      return;
    }
    $entity->{self::FIELD_NAME}[LANGUAGE_NONE][0]['value'] = $status;
  }

  /**
   * Returns the current status value from an entity, or NULL if not set.
   */
  public static function getStatus($entity_type, $entity) {
    if (isset($entity->{self::FIELD_NAME}[LANGUAGE_NONE][0]['value'])) {
      return $entity->{self::FIELD_NAME}[LANGUAGE_NONE][0]['value'];
    }
    return NULL;
  }

}
