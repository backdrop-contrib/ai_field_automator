<?php


namespace ai_field_automator\includes\plugins\scalar;

use BackdropDateTime;
use ai_field_automator\includes\AIFieldAutomatorBase;

/**
 * @file
 * Automator plugin for extracting dates from text into date fields.
 *
 * Handles all three Backdrop date field types: date (ISO varchar),
 * datetime (MySQL DATETIME), and datestamp (Unix timestamp).
 * Supports optional end-date extraction when the field has todate enabled.
 */
class AIFieldAutomatorDateExtract extends AIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t(
      'Extracts dates from text content (any format) and stores them in a date/datetime/datestamp field. Handles inconsistent date formats, relative dates, and optional end dates.'
    );
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return ['date', 'datetime', 'datestamp'];
  }

  /**
   * {@inheritdoc}
   */
  public function generate($entity, $field_name, array $config) {
    $prompts = $this->buildPrompts($entity, $config);
    if (empty($prompts)) {
      return [];
    }

    $field_info = field_info_field($field_name);
    $field_type = $field_info['type'] ?? 'datetime';
    $has_end_date = !empty($field_info['settings']['todate']);
    $granularity = date_granularity($field_info);
    $tz_handling = $field_info['settings']['tz_handling'] ?? 'site';

    $values = [];
    foreach ($prompts as $prompt) {
      $prompt .= $this->buildDateExtractionSuffix($has_end_date, $granularity);
      $text = $this->callAI($prompt, $config);
      if ($text === '') {
        continue;
      }

      $rows = $this->decodeJsonPayload($text);
      if (empty($rows)) {
        // Try to parse the raw text as a single date.
        $rows = [['value' => $text]];
      }

      foreach ($rows as $row) {
        $item = $this->buildDateItem(
          $row,
          $field_type,
          $has_end_date,
          $tz_handling
        );
        if ($item !== NULL) {
          $values[] = $item;
        }
      }
    }

    return $values;
  }

  /**
   * {@inheritdoc}
   *
   * Override storeValues because date fields need structured arrays, not
   * scalars.
   */
  public function storeValues(
    $entity,
    array $values,
    $field_name,
    array $config
  ) {
    if (empty($values)) {
      return;
    }
    $lang = LANGUAGE_NONE;
    $entity->{$field_name}[$lang] = array_values($values);
  }

  /**
   * Builds the AI instruction suffix for date extraction.
   */
  protected function buildDateExtractionSuffix(
    $has_end_date,
    array $granularity
  ) {
    $suffix
      = "\n\nExtract dates from the text above. Return RFC8259 JSON only.";

    $has_time = in_array('hour', $granularity)
      || in_array(
        'minute',
        $granularity
      );
    if ($has_time) {
      $format_desc = 'YYYY-MM-DDTHH:MM:SS (ISO 8601)';
      $example_val = '2025-03-15T14:30:00';
    }
    else {
      $format_desc = 'YYYY-MM-DD';
      $example_val = '2025-03-15';
    }

    if ($has_end_date) {
      $suffix .= " Each object must have \"value\" (start date) and \"value2\" (end date) as {$format_desc} strings.";
      $suffix .= " If only one date is found, use it for both value and value2.";
      $suffix .= " Example: [{\"value\":\"{$example_val}\",\"value2\":\"{$example_val}\"}]";
    }
    else {
      $suffix .= " Each object must have a \"value\" key as a {$format_desc} string.";
      $suffix .= " Example: [{\"value\":\"{$example_val}\"}]";
    }

    $suffix .= "\nParse any date format (American, European, relative, written-out, etc.) into the format shown.";
    $suffix .= " If the text contains no recognizable dates, return an empty array [].";
    $suffix .= " Do not include explanations. Return only the JSON array.";

    return $suffix;
  }

  /**
   * Converts a decoded JSON row into a date field item array.
   *
   * @param array $row
   *   Decoded JSON row with 'value' and optionally 'value2', 'timezone'.
   * @param string $field_type
   *   One of 'date', 'datetime', 'datestamp'.
   * @param bool $has_end_date
   *   Whether the field supports an end date (todate).
   * @param string $tz_handling
   *   Timezone handling mode from field settings.
   *
   * @return array|null
   *   A date field item array, or NULL if parsing fails.
   */
  protected function buildDateItem(
    array $row,
    $field_type,
    $has_end_date,
    $tz_handling
  ) {
    $raw_value = trim((string) ($row['value'] ?? ''));
    if ($raw_value === '') {
      return NULL;
    }

    $date = $this->parseDate($raw_value);
    if (!$date) {
      return NULL;
    }

    $item = [
      'value' => $this->formatForStorage($date, $field_type),
    ];

    if ($has_end_date) {
      $raw_end = trim((string) ($row['value2'] ?? ''));
      $end_date = $raw_end !== '' ? $this->parseDate($raw_end) : clone $date;
      $item['value2'] = $end_date ? $this->formatForStorage(
        $end_date,
        $field_type
      ) : $item['value'];
    }

    // Set timezone columns when tz_handling is 'date'.
    if ($tz_handling === 'date') {
      $tz_name = !empty($row['timezone']) ? $row['timezone']
        : date_default_timezone();
      $item['timezone'] = $tz_name;
      $item['offset'] = $date->getOffset();
      if ($has_end_date && !empty($end_date)) {
        $item['offset2'] = $end_date->getOffset();
      }
    }

    return $item;
  }

  /**
   * Parses a date string into a DateTime object.
   *
   * Tries BackdropDateTime first (handles many formats), then falls back to
   * PHP's strtotime() for relative/informal date strings.
   *
   * @param string $input
   *   Date string in any format.
   *
   * @return \DateTime|null
   *   Parsed DateTime object or NULL on failure.
   */
  protected function parseDate($input) {
    $input = trim($input);
    if ($input === '') {
      return NULL;
    }

    // Strip ordinal suffixes (1st, 2nd, 3rd, 4th, etc.) that may confuse parsers.
    $cleaned = preg_replace('/(\d+)(st|nd|rd|th)\b/i', '$1', $input);

    // Try BackdropDateTime if available (handles ISO, DATETIME, etc.).
    if (class_exists('BackdropDateTime')) {
      try {
        $date = new BackdropDateTime($cleaned, date_default_timezone());
        if ($date && !$date->hasErrors()) {
          return $date;
        }
      }
      catch (\Exception $e) {
        // Fall through to strtotime.
      }
    }

    // Fall back to PHP's strtotime for informal/relative dates.
    $timestamp = @strtotime($cleaned);
    if ($timestamp !== FALSE && $timestamp !== -1) {
      $date = new \DateTime('@' . $timestamp);
      $date->setTimezone(new \DateTimeZone(date_default_timezone()));
      return $date;
    }

    return NULL;
  }

  /**
   * Formats a DateTime for storage based on the field type.
   *
   * @param \DateTime $date
   *   The parsed date.
   * @param string    $field_type
   *   One of 'date', 'datetime', 'datestamp'.
   *
   * @return string|int
   *   Formatted date value for the database column.
   */
  protected function formatForStorage($date, $field_type) {
    switch ($field_type) {
      case 'datestamp':
        return (int) $date->format('U');

      case 'datetime':
        // MySQL DATETIME format.
        return $date->format('Y-m-d H:i:s');

      case 'date':
      default:
        // ISO format stored as varchar.
        return $date->format('Y-m-d\TH:i:s');
    }
  }

}
