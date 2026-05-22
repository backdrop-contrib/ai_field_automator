<?php


namespace ai_field_automator\includes;

/**
 * @file
 * Interface for AI Field Automator plugins.
 */
interface AIFieldAutomatorInterface {

  /**
   * Returns help text shown in the admin UI.
   *
   * @return string
   */
  public function helpText();

  /**
   * Returns an array of Backdrop field types this plugin handles.
   *
   * @return array
   *   e.g. ['text', 'text_long', 'string'].
   */
  public function allowedFieldTypes();

  /**
   * Runtime gate — return FALSE to skip this rule on a given entity/field.
   *
   * @param object $entity
   *   The entity being saved.
   * @param string $field_name
   *   The field machine name.
   * @param array  $config
   *   The rule configuration array.
   *
   * @return bool
   */
  public function ruleIsAllowed($entity, $field_name, array $config);

  /**
   * Calls the AI provider and returns an array of generated values.
   *
   * @param object $entity
   *   The entity being saved.
   * @param string $field_name
   *   The destination field machine name.
   * @param array  $config
   *   The rule configuration array.
   *
   * @return array
   *   Flat array of generated value strings.
   */
  public function generate($entity, $field_name, array $config);

  /**
   * Returns field types that are valid as the "base field" (context) input.
   * Return an empty array to allow any field type.
   *
   * @return array
   */
  public function allowedInputs();

  /**
   * Normalizes the current destination field value for emptiness checks.
   *
   * Plugins can override this when a field is only partially populated but
   * should still be considered empty for automator purposes.
   *
   * @param array $value
   *   Current field items.
   * @param array $config
   *   The rule configuration array.
   *
   * @return mixed
   *   Normalized value used to decide whether generation should run.
   */
  public function checkIfEmpty(array $value, array $config = []);

  /**
   * Optional second-pass normalization after checkIfEmpty().
   *
   * @param object $entity
   *   The entity being saved.
   * @param mixed  $value
   *   Value returned from checkIfEmpty().
   * @param array  $config
   *   The rule configuration array.
   *
   * @return mixed
   *   Post-processed value.
   */
  public function postCheckIfEmpty($entity, $value, array $config = []);

  /**
   * Writes the generated values back onto the entity.
   *
   * @param object $entity
   *   The entity being saved (modified in-place).
   * @param array  $values
   *   Array of generated value strings.
   * @param string $field_name
   *   The destination field machine name.
   * @param array  $config
   *   The rule configuration array.
   */
  public function storeValues(
    $entity,
    array $values,
    $field_name,
    array $config
  );

  /**
   * Verifies a generated value before it is written back to the entity.
   *
   * @param object $entity
   *   The entity being saved.
   * @param mixed  $value
   *   Candidate generated value.
   * @param string $field_name
   *   Destination field machine name.
   * @param array  $config
   *   The rule configuration array.
   *
   * @return bool
   *   TRUE when the value is valid for storage.
   */
  public function verifyValue($entity, $value, $field_name, array $config);

}
