<?php


namespace ai_field_automator\includes\plugins\media;

use ai_field_automator\includes\AIFieldAutomatorBase;

/**
 * Cuts and combines video clips based on AI-selected timestamps.
 */
class AIFieldAutomatorVideoToVideo extends AIFieldAutomatorBase {

  /**
   * {@inheritdoc}
   */
  public function helpText() {
    return t('Extracts and combines video segments using ffmpeg.');
  }

  /**
   * {@inheritdoc}
   */
  public function allowedFieldTypes() {
    return ['file'];
  }

  /**
   * {@inheritdoc}
   */
  public function allowedInputs() {
    return ['file'];
  }

  /**
   * {@inheritdoc}
   */
  public function ruleIsAllowed($entity, $field_name, array $config) {
    return $this->isFfmpegAvailable();
  }

  /**
   * {@inheritdoc}
   */
  public function generate($entity, $field_name, array $config) {
    $base_field = $config['base_field'] ?? '';
    if ($base_field === '' || empty($entity->{$base_field}[LANGUAGE_NONE])) {
      return [];
    }

    if (!$this->isFfmpegAvailable()) {
      return [];
    }

    $speech_model = $config['speech_model'] ?? (config_get('ai.settings', 'default_stt_model') ?: 'whisper-1');
    $stripped_speech_model = '';
    $speech_api = ai_get_api_for_model(
      $speech_model,
      $stripped_speech_model
    );
    if (!$speech_api) {
      return [];
    }

    $instruction = trim(
      (string) ($config['prompt'] ?? 'Select the best video segments to keep.')
    );
    if (($config['mode'] ?? 'base') === 'token' && !empty($config['token'])) {
      $instruction = trim(
        (string) ai_field_automator_render_token_prompt(
          $config['token'],
          $entity,
          $config['entity_type'] ?? ''
        )
      );
    }
    if ($instruction === '') {
      $instruction = 'Select the best video segments to keep.';
    }

    $values = [];
    foreach ($entity->{$base_field}[LANGUAGE_NONE] as $item) {
      if (empty($item['fid'])) {
        continue;
      }
      $file = file_load($item['fid']);
      if (!$file || empty($file->uri)) {
        continue;
      }
      if (!empty($file->filemime) && strpos($file->filemime, 'video/') !== 0) {
        continue;
      }
      $path = backdrop_realpath($file->uri);
      if (!$path || !is_file($path)) {
        continue;
      }

      $transcript = $speech_api->speechToText(
        $stripped_speech_model ?: $speech_model,
        $path,
        'transcribe'
      );
      if (!is_string($transcript) || trim($transcript) === '') {
        continue;
      }

      $prompt = $instruction . "\n\nVideo transcript:\n" . trim($transcript);
      $prompt .= "\n\nReturn RFC8259 JSON only in this format: [{\"value\":{\"start_time\":\"HH:MM:SS(.ms)\",\"end_time\":\"HH:MM:SS(.ms)\"}}]";
      $text = $this->callAI($prompt, $config);
      if ($text === '') {
        continue;
      }
      $rows = $this->decodeJsonPayload($text);
      if (empty($rows)) {
        continue;
      }
      if ($this->isAssoc($rows)) {
        $rows = [$rows];
      }
      foreach ($rows as $row) {
        $candidate = isset($row['value']) ? $row['value'] : $row;
        if (!is_array($candidate)) {
          continue;
        }
        $start = $this->normalizeTimestamp(
          $candidate['start_time'] ?? $candidate['start'] ?? ''
        );
        $end = $this->normalizeTimestamp(
          $candidate['end_time'] ?? $candidate['end'] ?? ''
        );
        if ($start === '' || $end === '') {
          continue;
        }
        if ($this->timeToSeconds($end) <= $this->timeToSeconds($start)) {
          continue;
        }
        $values[] = [
          'fid'        => (int) $item['fid'],
          'start_time' => $start,
          'end_time'   => $end,
        ];
      }
    }

    return $values;
  }

  /**
   * {@inheritdoc}
   */
  public function storeValues(
    $entity,
    array $values,
    $field_name,
    array $config
  ) {
    if (empty($values) || !$this->isFfmpegAvailable()) {
      return;
    }

    $grouped = [];
    foreach ($values as $value) {
      if (empty($value['fid']) || empty($value['start_time'])
        || empty($value['end_time'])
      ) {
        continue;
      }
      $grouped[(int) $value['fid']][] = $value;
    }
    if (empty($grouped)) {
      return;
    }

    $items = [];
    foreach ($grouped as $fid => $segments) {
      $source_file = file_load($fid);
      if (!$source_file || empty($source_file->uri)) {
        continue;
      }
      $source_path = backdrop_realpath($source_file->uri);
      if (!$source_path || !is_file($source_path)) {
        continue;
      }

      $tmp_dir = file_directory_temp() . '/ai_field_automator_' . uniqid();
      if (!file_prepare_directory(
        $tmp_dir,
        FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS
      )
      ) {
        continue;
      }

      $segment_paths = [];
      foreach ($segments as $delta => $segment) {
        $segment_path = $tmp_dir . '/segment_' . $delta . '.mp4';
        $command = 'ffmpeg -y -nostdin -i ' . escapeshellarg($source_path)
          . ' -ss ' . escapeshellarg($segment['start_time'])
          . ' -to ' . escapeshellarg($segment['end_time'])
          . ' -c:v libx264 -c:a aac ' . escapeshellarg($segment_path)
          . ' 2>/dev/null';
        @shell_exec($command);
        if (is_file($segment_path) && filesize($segment_path) > 0) {
          $segment_paths[] = $segment_path;
        }
      }

      if (empty($segment_paths)) {
        $this->deleteTmpDir($tmp_dir);
        continue;
      }

      $final_path = $segment_paths[0];
      if (count($segment_paths) > 1) {
        $list_path = $tmp_dir . '/concat.txt';
        $lines = [];
        foreach ($segment_paths as $segment_path) {
          $lines[] = "file '" . str_replace("'", "'\\''", $segment_path) . "'";
        }
        file_put_contents($list_path, implode("\n", $lines));
        $final_path = $tmp_dir . '/combined.mp4';
        $concat_command = 'ffmpeg -y -nostdin -f concat -safe 0 -i '
          . escapeshellarg($list_path)
          . ' -c:v libx264 -c:a aac ' . escapeshellarg($final_path)
          . ' 2>/dev/null';
        @shell_exec($concat_command);
        if (!is_file($final_path) || filesize($final_path) === 0) {
          $final_path = $segment_paths[0];
        }
      }

      $binary = @file_get_contents($final_path);
      if ($binary !== FALSE && $binary !== '') {
        $managed = $this->createManagedFile(
          $binary,
          'ai-video-cut-' . REQUEST_TIME . '-' . $fid . '.mp4'
        );
        if ($managed && !empty($managed->fid)) {
          $items[] = [
            'fid'     => (int) $managed->fid,
            'display' => 1,
          ];
        }
      }

      $this->deleteTmpDir($tmp_dir);
    }

    if (!empty($items)) {
      $entity->{$field_name}[LANGUAGE_NONE] = $items;
    }
  }

  /**
   * Checks if ffmpeg is available.
   */
  protected function isFfmpegAvailable() {
    $which = @shell_exec('which ffmpeg 2>/dev/null');
    return !empty(trim((string) $which));
  }

  /**
   * Normalize timestamp into ffmpeg-compatible format.
   */
  protected function normalizeTimestamp($value) {
    $value = trim((string) $value);
    if (preg_match('/^\d{1,2}:\d{2}:\d{2}(?:\.\d+)?$/', $value)) {
      return $value;
    }
    if (preg_match('/^\d+$/', $value)) {
      $seconds = (int) $value;
      $h = floor($seconds / 3600);
      $m = floor(($seconds % 3600) / 60);
      $s = $seconds % 60;
      return sprintf('%02d:%02d:%02d', $h, $m, $s);
    }
    return '';
  }

  /**
   * Convert HH:MM:SS(.ms) time to seconds.
   */
  protected function timeToSeconds($value) {
    $parts = explode(':', $value);
    if (count($parts) !== 3) {
      return 0;
    }
    $h = (int) $parts[0];
    $m = (int) $parts[1];
    $s = (float) $parts[2];
    return ($h * 3600) + ($m * 60) + $s;
  }

  /**
   * Recursively removes temporary directory.
   */
  protected function deleteTmpDir($dir) {
    if (!is_dir($dir)) {
      return;
    }
    foreach (scandir($dir) as $entry) {
      if ($entry === '.' || $entry === '..') {
        continue;
      }
      $path = $dir . '/' . $entry;
      if (is_dir($path)) {
        $this->deleteTmpDir($path);
      }
      else {
        @unlink($path);
      }
    }
    @rmdir($dir);
  }

  /**
   * Returns TRUE if array is associative.
   */
  protected function isAssoc(array $array) {
    return array_keys($array) !== range(0, count($array) - 1);
  }

}
