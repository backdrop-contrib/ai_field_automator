<?php


/**
 * @file
 * Form alter and submit logic for the field instance edit form.
 *
 * When editing any field under Structure > Content Types > [type] > Manage
 * fields, an "AI Automator" section is added at the bottom.  On save it
 * writes (or deletes) a per-field config object:
 *   ai_field_automator.{entity_type}.{bundle}.{field_name}
 */

/**
 * Alter hook: adds the AI Automator fieldset to field_ui_field_edit_form.
 *
 * @see ai_field_automator_form_field_ui_field_edit_form_alter()
 */
function _ai_field_automator_field_edit_form_alter(&$form, &$form_state) {
  $field    = $form['#field'];
  $instance = $form['#instance'];

  $entity_type = $instance['entity_type'];
  $bundle      = $instance['bundle'];
  $field_name  = $instance['field_name'];
  $field_type  = $field['type'];

  // Find automator plugins that support this field type.
  $matching_types = [];
  $field_target_type = $field['settings']['target_type'] ?? NULL;
  foreach (ai_field_automator_get_types() as $tid => $tdef) {
    if (in_array($field_type, $tdef['field_types'])) {
      if (!empty($tdef['field_names']) && !in_array($field_name, $tdef['field_names'], TRUE)) {
        continue;
      }
      // If the plugin declares target_types, the field's target_type must match.
      if (!empty($tdef['target_types']) && !in_array($field_target_type, $tdef['target_types'], TRUE)) {
        continue;
      }
      $matching_types[$tid] = $tdef['label'];
    }
  }

  // Don't add the section if no plugins support this field type.
  if (empty($matching_types)) {
    return;
  }

  // Load existing config for this field (if any).
  $config = ai_field_automator_load_field_config($entity_type, $bundle, $field_name);
  $config = is_array($config) ? $config : [];
  $enabled = !empty($config['enabled']);
  $current_enabled = isset($form_state['values']['automator_enabled'])
    ? !empty($form_state['values']['automator_enabled'])
    : $enabled;

  // Determine the currently selected automator type (for AJAX rebuilds).
  $chosen_type = $form_state['values']['automator_type']
    ?? ($config['automator_type'] ?? key($matching_types));
  if (!isset($matching_types[$chosen_type])) {
    $chosen_type = key($matching_types);
  }

  $form['ai_automator'] = [
    '#type'        => 'fieldset',
    '#title'       => t('AI Automator'),
    '#weight'      => 50,
    '#description' => t('Automatically fill this field with AI-generated content when the content is saved.'),
    '#attributes'  => ['id' => 'ai-automator-fieldset'],
  ];

  $form['ai_automator']['automator_enabled'] = [
    '#type'          => 'checkbox',
    '#title'         => t('Enable AI Automator for this field'),
    '#default_value' => $current_enabled,
    '#ajax'          => [
      'callback' => 'ai_field_automator_enabled_ajax',
      'wrapper'  => 'ai-automator-fieldset',
    ],
  ];

  $form['ai_automator']['automator_label'] = [
    '#type'          => 'textfield',
    '#title'         => t('Automator label'),
    '#description'   => t('A human-readable name for this automator (e.g. "Auto-generate tags"). Shown in the Run Order tab.'),
    '#default_value' => $config['label'] ?? '',
    '#maxlength'     => 255,
    '#access'        => $current_enabled,
  ];

  // Wrapper rebuilt by AJAX when the type changes.
  $form['ai_automator']['settings_wrapper'] = [
    '#type'       => 'container',
    '#attributes' => ['id' => 'automator-settings-wrapper'],
    '#access'     => $current_enabled,
  ];

  $form['ai_automator']['settings_wrapper']['automator_type'] = [
    '#type'          => 'select',
    '#title'         => t('Automator type'),
    '#options'       => $matching_types,
    '#default_value' => $chosen_type,
    '#ajax'          => [
      'callback' => 'ai_field_automator_type_ajax',
      'wrapper'  => 'automator-settings-wrapper',
    ],
  ];

  // Show help text for the chosen type.
  $types = ai_field_automator_get_types();
  $type_definition = $types[$chosen_type] ?? [];
  $requires_model = !array_key_exists('requires_model', $type_definition) || !empty($type_definition['requires_model']);
  $requires_prompt = !array_key_exists('requires_prompt', $type_definition) || !empty($type_definition['requires_prompt']);
  $requires_base_field = !array_key_exists('requires_base_field', $type_definition) || !empty($type_definition['requires_base_field']);
  $chosen_model = $form_state['values']['automator_model']
    ?? ($config['model'] ?? '');

  $plugin = ai_field_automator_load_plugin($chosen_type, $types[$chosen_type]);
  if ($plugin && $plugin->helpText()) {
    $form['ai_automator']['settings_wrapper']['automator_help'] = [
      '#markup' => '<p class="description">' . check_plain($plugin->helpText()) . '</p>',
    ];
  }

  // Base field: which field provides context for the prompt.
  $instances      = field_info_instances($entity_type, $bundle);
  $base_options   = ['' => t('-- None --')];
  $allowed_inputs = $plugin ? $plugin->allowedInputs() : [];
  foreach ($instances as $fname => $finstance) {
    if ($fname === $field_name && !in_array($chosen_type, ['image_alt_text', 'rewrite_image_filename'], TRUE)) {
      continue;
    }
    $fi = field_info_field($fname);
    if (empty($allowed_inputs) || in_array($fi['type'], $allowed_inputs)) {
      $base_options[$fname] = $finstance['label'] . ' (' . $fname . ')';
    }
  }
  // Always include the node title as a context source.
  $base_options['_title'] = t('Title / Label');
  $base_options['_rendered_entity'] = ($entity_type === 'node')
    ? t('Rendered node text (all fields + paragraphs)')
    : t('Rendered entity text (all fields)');

  $mode_options = [
    'base' => t('Base mode'),
  ];
  if ($requires_prompt) {
    $mode_options['token'] = t('Token mode');
  }
  $mode_default = $config['mode'] ?? 'base';
  if (!isset($mode_options[$mode_default])) {
    $mode_default = 'base';
  }

  $form['ai_automator']['settings_wrapper']['automator_mode'] = [
    '#type' => 'select',
    '#title' => t('Automator input mode'),
    '#options' => $mode_options,
    '#default_value' => $mode_default,
    '#description' => t('Base mode uses a selected base field as context. Token mode uses Token module replacements in the prompt template.'),
  ];

  $base_field_states = [
    'visible' => [
      'select[name="automator_mode"]' => ['value' => 'base'],
    ],
  ];
  if ($requires_base_field) {
    $base_field_states['required'] = [
      'input[name="automator_enabled"]' => ['checked' => TRUE],
      'select[name="automator_mode"]' => ['value' => 'base'],
    ];
  }

  $form['ai_automator']['settings_wrapper']['automator_base_field'] = [
    '#type'          => 'select',
    '#title'         => t('Base field (context)'),
    '#description'   => t('This field\'s value will be passed to the AI as context.'),
    '#options'       => $base_options,
    '#default_value' => $config['base_field'] ?? '',
    '#states'        => $base_field_states,
    '#access'        => $requires_base_field,
  ];

  $prompt_states = [
    'visible' => [
      'select[name="automator_mode"]' => ['value' => 'base'],
    ],
  ];
  if ($requires_prompt) {
    $prompt_states['required'] = [
      'input[name="automator_enabled"]' => ['checked' => TRUE],
      'select[name="automator_mode"]' => ['value' => 'base'],
    ];
  }

  $form['ai_automator']['settings_wrapper']['automator_prompt'] = [
    '#type'          => 'textarea',
    '#title'         => t('Prompt'),
    '#description'   => t('Tokens: <code>{title}</code> = entity title, <code>{context}</code> = value of the base field, <code>{field:FIELD_NAME}</code> = value of any field.'),
    '#default_value' => $config['prompt'] ?? '',
    '#rows'          => 4,
    '#states'        => $prompt_states,
    '#access'        => $requires_prompt,
  ];

  $token_states = [
    'visible' => [
      'select[name="automator_mode"]' => ['value' => 'token'],
    ],
  ];
  if ($requires_prompt) {
    $token_states['required'] = [
      'input[name="automator_enabled"]' => ['checked' => TRUE],
      'select[name="automator_mode"]' => ['value' => 'token'],
    ];
  }

  $form['ai_automator']['settings_wrapper']['automator_token'] = [
    '#type'          => 'textarea',
    '#title'         => t('Token prompt template'),
    '#description'   => t('Used only in Token mode. Example: Summarize [node:title] for social media. Use the token browser below to see available tokens; click a token to insert it into the focused field.'),
    '#default_value' => $config['token'] ?? '',
    '#rows'          => 4,
    '#states'        => $token_states,
    '#access'        => $requires_prompt,
  ];

  if (ai_field_automator_tokens_available()) {
    $token_type = ($entity_type === 'taxonomy_term') ? 'term' : $entity_type;
    $form['ai_automator']['settings_wrapper']['automator_token_help'] = [
      '#type' => 'fieldset',
      '#title' => t('Token browser'),
      '#description' => t('Browse the tokens available for this entity type and global site data. Click a token to insert it into the token prompt template field.'),
      '#collapsible' => FALSE,
      '#collapsed' => FALSE,
      '#states' => [
        'visible' => [
          'select[name="automator_mode"]' => ['value' => 'token'],
        ],
      ],
      '#access' => $requires_prompt,
    ];
    $form['ai_automator']['settings_wrapper']['automator_token_help']['tokens'] = [
      '#markup' => theme('token_tree', [
        'token_types' => [$token_type],
        'global_types' => TRUE,
        'show_restricted' => FALSE,
        'click_insert' => TRUE,
      ]),
    ];
  }

  // Taxonomy-term specific options.
  $form['ai_automator']['settings_wrapper']['automator_taxonomy_cleanup'] = [
    '#type' => 'select',
    '#title' => t('Tag text manipulation'),
    '#options' => [
      '' => t('None'),
      'lowercase' => t('lowercase'),
      'uppercase' => t('UPPERCASE'),
      'first_char' => t('First character uppercase'),
    ],
    '#default_value' => $config['taxonomy_cleanup'] ?? '',
    '#states' => [
      'visible' => [
        'select[name="automator_type"]' => ['value' => 'taxonomy_terms'],
      ],
    ],
  ];

  $form['ai_automator']['settings_wrapper']['automator_taxonomy_auto_create'] = [
    '#type' => 'checkbox',
    '#title' => t('Auto-create missing terms'),
    '#description' => t('If enabled, generated terms not found in the allowed vocabulary will be created automatically.'),
    '#default_value' => !empty($config['taxonomy_auto_create']),
    '#states' => [
      'visible' => [
        'select[name="automator_type"]' => ['value' => 'taxonomy_terms'],
      ],
    ],
  ];
  $form['ai_automator']['settings_wrapper']['automator_taxonomy_search_similar_tags'] = [
    '#type' => 'checkbox',
    '#title' => t('Find similar tags'),
    '#description' => t('Uses an additional AI pass to map near-match generated tags to existing tags.'),
    '#default_value' => !empty($config['taxonomy_search_similar_tags']),
    '#states' => [
      'visible' => [
        'select[name="automator_type"]' => ['value' => 'taxonomy_terms'],
      ],
    ],
  ];

  // Entity-reference specific options.
  $entityreference_bundle_options = ['' => t('-- Default allowed bundle --')];
  if ($field_type === 'entityreference') {
    $handler_bundles = $field['settings']['handler_settings']['target_bundles'] ?? [];
    foreach ($handler_bundles as $bundle_key => $bundle_value) {
      $entityreference_bundle_options[$bundle_key] = $bundle_value;
    }
  }
  $form['ai_automator']['settings_wrapper']['automator_entityreference_auto_create'] = [
    '#type' => 'checkbox',
    '#title' => t('Auto-create missing referenced entities (nodes only)'),
    '#default_value' => !empty($config['entityreference_auto_create']),
    '#states' => [
      'visible' => [
        'select[name="automator_type"]' => ['value' => 'entity_reference'],
      ],
    ],
  ];
  $form['ai_automator']['settings_wrapper']['automator_entityreference_create_bundle'] = [
    '#type' => 'select',
    '#title' => t('Bundle to create'),
    '#options' => $entityreference_bundle_options,
    '#default_value' => $config['entityreference_create_bundle'] ?? '',
    '#states' => [
      'visible' => [
        'select[name="automator_type"]' => ['value' => 'entity_reference'],
        'input[name="automator_entityreference_auto_create"]' => ['checked' => TRUE],
      ],
    ],
  ];
  $form['ai_automator']['settings_wrapper']['automator_entityreference_mode'] = [
    '#type' => 'select',
    '#title' => t('Entity reference mode'),
    '#options' => [
      'map_existing' => t('Map to existing entities'),
      'generate_new' => t('Generate and create new referenced entities'),
    ],
    '#default_value' => $config['entityreference_mode'] ?? 'map_existing',
    '#states' => [
      'visible' => [
        'select[name="automator_type"]' => ['value' => 'entity_reference'],
      ],
    ],
  ];
  $form['ai_automator']['settings_wrapper']['automator_entityreference_generate_bundle'] = [
    '#type' => 'select',
    '#title' => t('Generate target bundle'),
    '#options' => $entityreference_bundle_options,
    '#default_value' => $config['entityreference_generate_bundle'] ?? '',
    '#states' => [
      'visible' => [
        'select[name="automator_type"]' => ['value' => 'entity_reference'],
        'select[name="automator_entityreference_mode"]' => ['value' => 'generate_new'],
      ],
    ],
  ];
  $target_type = $field['settings']['target_type'] ?? '';
  $generate_bundle = $form_state['values']['automator_entityreference_generate_bundle'] ?? ($config['entityreference_generate_bundle'] ?? '');
  $form['ai_automator']['settings_wrapper']['automator_entityreference_generate_fields'] = [
    '#type'        => 'fieldset',
    '#title'       => t('Fields to generate'),
    '#collapsible' => FALSE,
    '#access'      => !empty($target_type) && !empty($generate_bundle),
    '#states'      => [
      'visible' => [
        'select[name="automator_type"]' => ['value' => 'entity_reference'],
        'select[name="automator_entityreference_mode"]' => ['value' => 'generate_new'],
      ],
    ],
  ];
  if ($target_type && $generate_bundle) {
    $target_instances = field_info_instances($target_type, $generate_bundle);
    if ($target_type === 'node') {
      $form['ai_automator']['settings_wrapper']['automator_entityreference_generate_fields']['automator_entity_field_enable_title'] = [
        '#type' => 'checkbox',
        '#title' => t('Title'),
        '#default_value' => !empty($config['entityreference_generate_fields']['title']),
      ];
      $form['ai_automator']['settings_wrapper']['automator_entityreference_generate_fields']['automator_entity_field_generate_title'] = [
        '#type' => 'textarea',
        '#title' => t('Title prompt'),
        '#default_value' => $config['entityreference_generate_fields']['title'] ?? '',
        '#rows' => 2,
        '#states' => [
          'visible' => [
            ':input[name="automator_entity_field_enable_title"]' => ['checked' => TRUE],
          ],
        ],
      ];
    }
    foreach ($target_instances as $target_field_name => $target_instance) {
      $target_field_info = field_info_field($target_field_name);
      if (empty($target_field_info) || !in_array($target_field_info['type'], ['text', 'text_long', 'text_with_summary', 'string', 'string_long', 'list_text', 'list_integer', 'list_float', 'email', 'telephone', 'integer', 'decimal', 'float', 'number_integer', 'number_decimal', 'number_float'], TRUE)) {
        continue;
      }
      $form['ai_automator']['settings_wrapper']['automator_entityreference_generate_fields']['automator_entity_field_enable_' . $target_field_name] = [
        '#type' => 'checkbox',
        '#title' => $target_instance['label'] . ' (' . $target_field_name . ')',
        '#default_value' => !empty($config['entityreference_generate_fields'][$target_field_name]),
      ];
      $form['ai_automator']['settings_wrapper']['automator_entityreference_generate_fields']['automator_entity_field_generate_' . $target_field_name] = [
        '#type' => 'textarea',
        '#title' => t('@field prompt', ['@field' => $target_instance['label']]),
        '#default_value' => $config['entityreference_generate_fields'][$target_field_name] ?? '',
        '#rows' => 2,
        '#states' => [
          'visible' => [
            ':input[name="automator_entity_field_enable_' . $target_field_name . '"]' => ['checked' => TRUE],
          ],
        ],
      ];
    }
  }

  $form['ai_automator']['settings_wrapper']['automator_image_size'] = [
    '#type' => 'select',
    '#title' => t('Image size'),
    '#options' => [
      '256x256' => '256x256',
      '512x512' => '512x512',
      '1024x1024' => '1024x1024',
      '1024x1792' => '1024x1792',
      '1792x1024' => '1792x1024',
    ],
    '#default_value' => $config['image_size'] ?? '1024x1024',
    '#states' => _ai_field_automator_type_states(['text_to_image', 'media_image_generation', 'video_to_image']),
  ];
  $form['ai_automator']['settings_wrapper']['automator_image_quality'] = [
    '#type' => 'select',
    '#title' => t('Image quality'),
    '#options' => [
      'standard' => t('Standard'),
      'hd' => t('HD'),
    ],
    '#default_value' => $config['image_quality'] ?? 'standard',
    '#states' => _ai_field_automator_type_states(['text_to_image', 'media_image_generation', 'video_to_image']),
  ];
  $form['ai_automator']['settings_wrapper']['automator_image_style'] = [
    '#type' => 'select',
    '#title' => t('Image style'),
    '#options' => [
      'natural' => t('Natural'),
      'vivid' => t('Vivid'),
    ],
    '#default_value' => $config['image_style'] ?? 'natural',
    '#states' => _ai_field_automator_type_states(['text_to_image', 'media_image_generation', 'video_to_image']),
  ];
  $form['ai_automator']['settings_wrapper']['automator_image_response_format'] = [
    '#type' => 'select',
    '#title' => t('Image response format'),
    '#options' => [
      'b64_json' => 'b64_json',
      'url' => 'url',
    ],
    '#default_value' => $config['image_response_format'] ?? 'b64_json',
    '#states' => _ai_field_automator_type_states(['text_to_image', 'media_image_generation', 'video_to_image']),
  ];
  $form['ai_automator']['settings_wrapper']['automator_image_output_format'] = [
    '#type' => 'select',
    '#title' => t('Image output format'),
    '#options' => [
      '' => t('Provider default'),
      'png' => 'png',
      'jpg' => 'jpg',
      'webp' => 'webp',
    ],
    '#default_value' => $config['image_output_format'] ?? '',
    '#states' => _ai_field_automator_type_states(['text_to_image', 'media_image_generation', 'video_to_image']),
  ];
  $form['ai_automator']['settings_wrapper']['automator_image_filename_pattern'] = [
    '#type' => 'textfield',
    '#title' => t('Image filename'),
    '#default_value' => $config['image_filename_pattern'] ?? '',
    '#description' => t('Optional fixed filename, e.g. <code>hero-image.png</code>. Leave blank for generated names.'),
    '#states' => _ai_field_automator_type_states(['text_to_image', 'media_image_generation', 'video_to_image']),
  ];

  $form['ai_automator']['settings_wrapper']['automator_speech_task'] = [
    '#type' => 'select',
    '#title' => t('Speech task'),
    '#options' => [
      'transcribe' => t('Transcribe'),
      'translate' => t('Translate to English'),
    ],
    '#default_value' => $config['speech_task'] ?? 'transcribe',
    '#states' => _ai_field_automator_type_states(['audio_to_text', 'video_to_text']),
  ];

  $form['ai_automator']['settings_wrapper']['automator_speech_model'] = [
    '#type' => 'textfield',
    '#title' => t('Speech-to-text model'),
    '#description' => t('Fully-qualified speech model ID used for transcription in video-derived automators.'),
    '#default_value' => $config['speech_model'] ?? (config_get('ai.settings', 'default_stt_model') ?: 'whisper-1'),
    '#states' => _ai_field_automator_type_states(['video_to_html', 'video_to_image']),
  ];

  $tts_voice_options = ai_field_automator_tts_voice_options($chosen_model);
  $tts_voice_default = $form_state['values']['automator_tts_voice'] ?? ($config['tts_voice'] ?? 'alloy');
  if (!isset($tts_voice_options[$tts_voice_default])) {
    reset($tts_voice_options);
    $tts_voice_default = key($tts_voice_options);
  }
  $tts_voice_description = t('Voice used for audio generation.');
  if (!empty($chosen_model) && strpos($chosen_model, 'elevenlabs/') === 0) {
    $tts_voice_description = t('Available ElevenLabs voices for the selected model. The saved value is the actual ElevenLabs voice ID.');
  }

  $form['ai_automator']['settings_wrapper']['automator_tts_voice'] = [
    '#type' => 'select',
    '#title' => t('TTS voice'),
    '#options' => $tts_voice_options,
    '#default_value' => $tts_voice_default,
    '#description' => $tts_voice_description,
    '#states' => _ai_field_automator_type_states(['text_to_speech', 'media_audio_generation']),
  ];
  $form['ai_automator']['settings_wrapper']['automator_tts_response_format'] = [
    '#type' => 'select',
    '#title' => t('Audio output format'),
    '#options' => [
      'mp3' => 'mp3',
      'opus' => 'opus',
      'aac' => 'aac',
      'flac' => 'flac',
      'wav' => 'wav',
      'pcm' => 'pcm',
    ],
    '#default_value' => $config['tts_response_format'] ?? 'mp3',
    '#states' => _ai_field_automator_type_states(['text_to_speech', 'media_audio_generation']),
  ];
  $form['ai_automator']['settings_wrapper']['automator_tts_filename_pattern'] = [
    '#type' => 'textfield',
    '#title' => t('Audio filename'),
    '#default_value' => $config['tts_filename_pattern'] ?? '',
    '#description' => t('Optional fixed filename, e.g. <code>voiceover.mp3</code>. Leave blank for generated names.'),
    '#states' => _ai_field_automator_type_states(['text_to_speech', 'media_audio_generation']),
  ];

  $moderation_trigger_default = !empty($config['moderation_trigger_states']) ? implode(', ', (array) $config['moderation_trigger_states']) : '';
  $moderation_lookup_default = !empty($config['moderation_lookup_states']) ? implode(', ', (array) $config['moderation_lookup_states']) : '';
  $views_arguments_default = !empty($config['views_arguments']) ? implode("\n", (array) $config['views_arguments']) : '';
  $views_exposed_default = '';
  if (!empty($config['views_exposed_filters']) && is_array($config['views_exposed_filters'])) {
    $rows = [];
    foreach ($config['views_exposed_filters'] as $filter_key => $filter_value) {
      $rows[] = $filter_key . '=' . $filter_value;
    }
    $views_exposed_default = implode("\n", $rows);
  }

  $explanation_field_options = ['' => t('-- Do not store --')];
  foreach ($instances as $fname => $finstance) {
    $fi = field_info_field($fname);
    if (empty($fi)) {
      continue;
    }
    if (in_array($fi['type'], ['text', 'text_long', 'text_with_summary', 'string_long'], TRUE)) {
      $explanation_field_options[$fname] = $finstance['label'] . ' (' . $fname . ')';
    }
  }

  $form['ai_automator']['settings_wrapper']['automator_json_schema'] = [
    '#type' => 'textarea',
    '#title' => t('JSON schema / example'),
    '#description' => t('Optional schema or JSON example that the model should follow.'),
    '#default_value' => $config['json_schema'] ?? '',
    '#rows' => 4,
    '#states' => _ai_field_automator_type_states(['json_field', 'json_native_field', 'json_native_binary_field']),
  ];

  $form['ai_automator']['settings_wrapper']['automator_metatag_keys'] = [
    '#type' => 'textfield',
    '#title' => t('Metatag keys'),
    '#description' => t('Comma-separated keys to generate (e.g. <code>title, description, og:title, og:description</code>).'),
    '#default_value' => !empty($config['metatag_keys']) ? implode(', ', (array) $config['metatag_keys']) : '',
    '#states' => _ai_field_automator_type_states(['metatag']),
  ];

  $form['ai_automator']['settings_wrapper']['automator_moderation_trigger_states'] = [
    '#type' => 'textfield',
    '#title' => t('Trigger states'),
    '#description' => t('Comma-separated moderation states that are allowed to trigger this automator.'),
    '#default_value' => $moderation_trigger_default,
    '#states' => _ai_field_automator_type_states(['moderation_state']),
  ];

  $form['ai_automator']['settings_wrapper']['automator_moderation_lookup_states'] = [
    '#type' => 'textfield',
    '#title' => t('Lookup states'),
    '#description' => t('Comma-separated list of valid output states the model is allowed to choose from.'),
    '#default_value' => $moderation_lookup_default,
    '#states' => _ai_field_automator_type_states(['moderation_state']),
  ];

  $form['ai_automator']['settings_wrapper']['automator_moderation_use_simple_model'] = [
    '#type' => 'checkbox',
    '#title' => t('Use simple moderation parse mode'),
    '#description' => t('Parses state from free text output instead of requiring strict JSON.'),
    '#default_value' => !empty($config['moderation_use_simple_model']),
    '#states' => _ai_field_automator_type_states(['moderation_state']),
  ];

  $form['ai_automator']['settings_wrapper']['automator_moderation_store_explanation_field'] = [
    '#type' => 'select',
    '#title' => t('Store moderation explanation in'),
    '#options' => $explanation_field_options,
    '#default_value' => $config['moderation_store_explanation_field'] ?? '',
    '#states' => _ai_field_automator_type_states(['moderation_state']),
  ];

  $search_index_default = $form_state['values']['automator_search_index'] ?? ($config['search_index'] ?? '');
  $form['ai_automator']['settings_wrapper']['automator_search_index'] = [
    '#type' => 'select',
    '#title' => t('Search index'),
    '#description' => t('Search API index used for semantic/vector lookup.'),
    '#options' => ['' => t('-- Select index --')] + ai_field_automator_get_search_index_options(),
    '#default_value' => $search_index_default,
    '#states' => _ai_field_automator_type_states(['vector_search_text', 'vector_search_entity_reference']),
    '#ajax' => [
      'callback' => 'ai_field_automator_type_ajax',
      'wrapper' => 'automator-settings-wrapper',
    ],
  ];
  $form['ai_automator']['settings_wrapper']['automator_search_output_field'] = [
    '#type' => 'select',
    '#title' => t('Output field'),
    '#description' => t('Field from the selected search index to store in this destination field.'),
    '#options' => ai_field_automator_get_search_index_field_options($search_index_default),
    '#default_value' => $config['search_output_field'] ?? 'content',
    '#states' => _ai_field_automator_type_states(['vector_search_text']),
  ];
  $form['ai_automator']['settings_wrapper']['automator_search_max_results'] = [
    '#type' => 'number',
    '#title' => t('Maximum results'),
    '#description' => t('Maximum number of search matches to use.'),
    '#min' => 1,
    '#max' => 100,
    '#default_value' => (int) ($config['search_max_results'] ?? 10),
    '#states' => _ai_field_automator_type_states(['vector_search_text', 'vector_search_entity_reference']),
  ];
  $form['ai_automator']['settings_wrapper']['automator_search_offset'] = [
    '#type' => 'number',
    '#title' => t('Result offset'),
    '#description' => t('Number of top matches to skip before collecting values.'),
    '#min' => 0,
    '#default_value' => (int) ($config['search_offset'] ?? 0),
    '#states' => _ai_field_automator_type_states(['vector_search_text', 'vector_search_entity_reference']),
  ];
  $form['ai_automator']['settings_wrapper']['automator_search_minimum_score'] = [
    '#type' => 'textfield',
    '#title' => t('Minimum score'),
    '#description' => t('Optional threshold for accepted matches (0-1). Leave 0 for no threshold.'),
    '#default_value' => (string) ($config['search_minimum_score'] ?? '0'),
    '#states' => _ai_field_automator_type_states(['vector_search_text', 'vector_search_entity_reference']),
  ];
  $form['ai_automator']['settings_wrapper']['automator_search_distinct'] = [
    '#type' => 'checkbox',
    '#title' => t('Distinct results'),
    '#description' => t('Avoid duplicate outputs/targets from repeated search matches.'),
    '#default_value' => !array_key_exists('search_distinct', $config) || !empty($config['search_distinct']),
    '#states' => _ai_field_automator_type_states(['vector_search_text', 'vector_search_entity_reference']),
  ];

  $form['ai_automator']['settings_wrapper']['automator_views_view'] = [
    '#type' => 'select',
    '#title' => t('View display'),
    '#description' => t('Choose a View and display to execute (format: view_name:display_id).'),
    '#options' => ['' => t('-- Select view display --')] + ai_field_automator_get_view_display_options(),
    '#default_value' => $config['views_view'] ?? '',
    '#states' => _ai_field_automator_type_states(['views_extract_text']),
  ];
  $form['ai_automator']['settings_wrapper']['automator_views_arguments'] = [
    '#type' => 'textarea',
    '#title' => t('View arguments'),
    '#description' => t('One argument value per line, in display argument order. Token replacements are supported.'),
    '#rows' => 4,
    '#default_value' => $views_arguments_default,
    '#states' => _ai_field_automator_type_states(['views_extract_text']),
  ];
  $form['ai_automator']['settings_wrapper']['automator_views_exposed_filters'] = [
    '#type' => 'textarea',
    '#title' => t('Exposed filters'),
    '#description' => t('One filter per line in the format <code>identifier=value</code>. Token replacements are supported.'),
    '#rows' => 4,
    '#default_value' => $views_exposed_default,
    '#states' => _ai_field_automator_type_states(['views_extract_text']),
  ];
  $form['ai_automator']['settings_wrapper']['automator_views_html_to_markdown'] = [
    '#type' => 'checkbox',
    '#title' => t('Convert rendered HTML to plain text'),
    '#default_value' => !empty($config['views_html_to_markdown']),
    '#states' => _ai_field_automator_type_states(['views_extract_text']),
  ];

  // Agent plugin fields.
  $agent_options = ['' => t('-- Select agent --')];
  if (module_exists('ai_agents') && function_exists('ai_agents_list_agent_options')) {
    $agent_options += ai_agents_list_agent_options();
  }
  $form['ai_automator']['settings_wrapper']['automator_agent_id'] = [
    '#type'          => 'select',
    '#title'         => t('Agent'),
    '#description'   => t('Select the configured AI agent to run for this field.'),
    '#options'       => $agent_options,
    '#default_value' => $config['agent_id'] ?? '',
    '#states'        => _ai_field_automator_type_states(['agent_field']),
  ];
  $form['ai_automator']['settings_wrapper']['automator_agent_task'] = [
    '#type'          => 'textarea',
    '#title'         => t('Task instruction'),
    '#description'   => t('Optional task instruction sent to the agent. Leave blank to use the agent\'s system prompt alone.'),
    '#rows'          => 3,
    '#default_value' => $config['agent_task'] ?? '',
    '#states'        => _ai_field_automator_type_states(['agent_field']),
  ];

  // Provider Configuration — model selector and all generation parameters.
  if ($requires_model) {
    $form['ai_automator']['settings_wrapper']['provider_config'] = [
      '#type'        => 'fieldset',
      '#title'       => t('Provider Configuration'),
      '#collapsible' => TRUE,
      '#collapsed'   => FALSE,
    ];

    $model_options = ai_field_automator_model_options($chosen_type);
    if (!empty($chosen_model) && !ai_field_automator_model_option_exists($model_options, $chosen_model)) {
      $model_options = [
        t('Current model') => [
          $chosen_model => $chosen_model,
        ],
      ] + $model_options;
    }
    if ($model_options) {
      $form['ai_automator']['settings_wrapper']['provider_config']['automator_model'] = [
        '#type'          => 'select',
        '#title'         => t('Model'),
        '#options'       => $model_options,
        '#empty_option'  => t('-- Select a model --'),
        '#default_value' => $chosen_model,
        '#description'   => t('Models are grouped by provider.'),
        '#ajax'          => [
          'callback' => 'ai_field_automator_type_ajax',
          'wrapper'  => 'automator-settings-wrapper',
        ],
      ];
    }
    else {
      $form['ai_automator']['settings_wrapper']['provider_config']['automator_model'] = [
        '#type'          => 'textfield',
        '#title'         => t('Model'),
        '#default_value' => $chosen_model,
        '#description'   => t('Enter the fully-qualified model ID (e.g. <code>provider/model</code>, <code>ollama/llama3.2</code>, <code>anthropic/claude-3-5-sonnet</code>).'),
      ];
    }

    $form['ai_automator']['settings_wrapper']['provider_config']['automator_temperature'] = [
      '#type'          => 'textfield',
      '#title'         => t('Temperature'),
      '#default_value' => $config['temperature'] ?? 0.7,
      '#size'          => 6,
      '#description'   => t('Sampling temperature 0–2.0. Higher values mean more random output. Not used by reasoning models (forced to 1).'),
    ];

    $form['ai_automator']['settings_wrapper']['provider_config']['automator_frequency_penalty'] = [
      '#type'          => 'textfield',
      '#title'         => t('Frequency Penalty'),
      '#default_value' => $config['frequency_penalty'] ?? 0,
      '#size'          => 6,
      '#description'   => t('Number between -2.0 and 2.0. Penalizes tokens based on existing frequency.'),
    ];

    $form['ai_automator']['settings_wrapper']['provider_config']['automator_presence_penalty'] = [
      '#type'          => 'textfield',
      '#title'         => t('Presence Penalty'),
      '#default_value' => $config['presence_penalty'] ?? 0,
      '#size'          => 6,
      '#description'   => t('Number between -2.0 and 2.0. Penalizes tokens based on whether they appear in the text so far.'),
    ];

    $form['ai_automator']['settings_wrapper']['provider_config']['automator_top_p'] = [
      '#type'          => 'textfield',
      '#title'         => t('Top P'),
      '#default_value' => $config['top_p'] ?? 1,
      '#size'          => 6,
      '#description'   => t('Nucleus sampling (0–1). Alternative to temperature.'),
    ];

    $form['ai_automator']['settings_wrapper']['provider_config']['automator_max_tokens'] = [
      '#type'          => 'number',
      '#title'         => t('Max Tokens'),
      '#default_value' => $config['max_tokens'] ?? 1024,
      '#min'           => 1,
      '#max'           => 32000,
      '#description'   => t('Maximum tokens in the completion.'),
    ];

    $form['ai_automator']['settings_wrapper']['provider_config']['automator_reasoning_effort'] = [
      '#type'          => 'select',
      '#title'         => t('Reasoning Effort'),
      '#options'       => [
        ''       => t('-- Default (medium) --'),
        'low'    => t('Low — faster, less thorough'),
        'medium' => t('Medium'),
        'high'   => t('High — slower, more thorough'),
      ],
      '#default_value' => $config['reasoning_effort'] ?? '',
      '#description'   => t('Only used by reasoning models (o1, o3, o4-mini, etc.). Ignored for standard models.'),
    ];

    // Image field selector — for vision-capable models.
    $image_field_options = ['' => t('-- No images --')];
    foreach ($instances as $fname => $finstance) {
      $fi = field_info_field($fname);
      if (empty($fi)) {
        continue;
      }
      if (in_array($fi['type'], ['image', 'file', 'media'], TRUE)) {
        $image_field_options[$fname] = $finstance['label'] . ' (' . $fname . ')';
      }
    }
    if (count($image_field_options) > 1) {
      $form['ai_automator']['settings_wrapper']['provider_config']['automator_image_field'] = [
        '#type'          => 'select',
        '#title'         => t('Image Field'),
        '#options'       => $image_field_options,
        '#default_value' => $config['image_field'] ?? '',
        '#description'   => t('For vision-capable models: include images from this field in the prompt.'),
      ];
    }
  }

  $form['ai_automator']['settings_wrapper']['automator_advanced'] = [
    '#type'        => 'fieldset',
    '#title'       => t('Advanced settings'),
    '#collapsible' => TRUE,
    '#collapsed'   => TRUE,
  ];

  $form['ai_automator']['settings_wrapper']['automator_advanced']['automator_edit_mode'] = [
    '#type'          => 'select',
    '#title'         => t('Edit mode'),
    '#options'       => [
      'skip'    => t('Skip — never overwrite an existing value (default)'),
      'changed' => t('Edit when changed — regenerate only when the source field changes'),
      'always'  => t('Always overwrite — regenerate on every save'),
    ],
    '#default_value' => $config['edit_mode'] ?? 'skip',
    '#description'   => t('"Edit when changed" compares the current source field value to the value at last save and skips regeneration when nothing changed.'),
  ];

  $form['ai_automator']['settings_wrapper']['automator_advanced']['automator_worker_type'] = [
    '#type' => 'radios',
    '#title' => t('Automator Worker'),
    '#options' => [
      'widget_action' => t('Widget action - Adds a "Generate with AI" button on the field widget and runs on demand without saving.'),
      'batch' => t('Batch - Uses JavaScript batch queue (not recommended), will not work on programmatical saving.'),
      'queue_cron' => t('Queue/Cron - Saves as a queue worker and runs on cron.'),
      'direct' => t('Direct - Processes and saves the value directly.'),
    ],
    '#default_value' => $config['worker_type'] ?? 'direct',
  ];

  $form['ai_automator']['settings_wrapper']['automator_advanced']['automator_weight'] = [
    '#type'          => 'number',
    '#title'         => t('Automator Weight'),
    '#description'   => t('When multiple fields on the same bundle have automators, lower weight runs first.'),
    '#default_value' => $config['weight'] ?? 0,
    '#min'           => 0,
    '#max'           => 1000,
  ];

  $form['ai_automator']['settings_wrapper']['automator_advanced']['automator_joiner'] = [
    '#type'          => 'select',
    '#title'         => t('Joiner'),
    '#description'   => t('If the AI returns multiple values, join them into a single value using this separator. Leave empty to keep multiple values.'),
    '#options'       => [
      ''          => t("-- Don't join --"),
      ', '        => t('Comma + space (, )'),
      ' '         => t('Space'),
      '. '        => t('Period + space (. )'),
      '\n'        => t('New line'),
      '\t'        => t('Tab'),
      '<br />'    => t('HTML line break (<br />)'),
      '<br /><br />' => t('HTML double break'),
      '<hr />'    => t('HTML horizontal rule (<hr />)'),
      ','         => t('Comma (,)'),
      ';'         => t('Semicolon (;)'),
      '.'         => t('Period (.)'),
      'other'     => t('Custom…'),
    ],
    '#default_value' => $config['joiner'] ?? '',
  ];

  $form['ai_automator']['settings_wrapper']['automator_advanced']['automator_joiner_other'] = [
    '#type'          => 'textfield',
    '#title'         => t('Custom joiner'),
    '#default_value' => $config['joiner_other'] ?? '',
    '#size'          => 20,
    '#description'   => t('Enter the exact separator string. Use <code>\n</code> for newline, <code>\t</code> for tab.'),
    '#states'        => [
      'visible' => [
        'select[name="automator_joiner"]' => ['value' => 'other'],
      ],
    ],
  ];

  $text_format_options = ['' => t('-- None/User Based --')];
  foreach (filter_formats() as $format_id => $format) {
    $text_format_options[$format_id] = $format->name;
  }
  $form['ai_automator']['settings_wrapper']['automator_advanced']['automator_text_format'] = [
    '#type'          => 'select',
    '#title'         => t('Use text format'),
    '#options'       => $text_format_options,
    '#default_value' => $config['text_format'] ?? '',
    '#description'   => t('If you want to use a specific text format, select it here. Always pick one for cron jobs since cron runs as anonymous.'),
  ];

  // Attach our save handler (runs after field_ui saves the instance).
  $form['#validate'][] = 'ai_field_automator_field_edit_form_validate';
  $form['#submit'][] = 'ai_field_automator_field_edit_form_submit';
}

/**
 * AJAX callback: rebuild the settings wrapper when automator type changes.
 */
function ai_field_automator_type_ajax($form, &$form_state) {
  return $form['ai_automator']['settings_wrapper'];
}

/**
 * AJAX callback: rebuilds the AI Automator section when enable is toggled.
 */
function ai_field_automator_enabled_ajax($form, &$form_state) {
  return $form['ai_automator'];
}

/**
 * Validate handler for field edit form.
 */
function _ai_field_automator_field_edit_form_validate($form, &$form_state) {
  $values = $form_state['values'];

  if (empty($values['automator_enabled'])) {
    return;
  }

  if (empty($values['automator_type'])) {
    form_set_error('automator_type', t('Choose an automator type.'));
  }

  $type = $values['automator_type'] ?? '';
  $types = ai_field_automator_get_types();
  $type_definition = $types[$type] ?? [];
  $requires_model = !array_key_exists('requires_model', $type_definition) || !empty($type_definition['requires_model']);
  $requires_prompt = !array_key_exists('requires_prompt', $type_definition) || !empty($type_definition['requires_prompt']);
  $requires_base_field = !array_key_exists('requires_base_field', $type_definition) || !empty($type_definition['requires_base_field']);

  if ($requires_model && empty($values['automator_model'])) {
    form_set_error('automator_model', t('Choose a model.'));
  }

  $mode = $values['automator_mode'] ?? 'base';
  if ($mode === 'base') {
    $base_is_optional = in_array($type, ['image_alt_text', 'rewrite_image_filename'], TRUE);
    if ($requires_base_field && empty($values['automator_base_field'])) {
      if (!$base_is_optional) {
        form_set_error('automator_base_field', t('Choose a base field in Base mode.'));
      }
    }
    if ($requires_prompt && trim((string) ($values['automator_prompt'] ?? '')) === '') {
      form_set_error('automator_prompt', t('Enter a prompt for Base mode.'));
    }
  }
  elseif ($mode === 'token' && $requires_prompt) {
    if (!ai_field_automator_tokens_available()) {
      form_set_error('automator_mode', t('Token mode requires Backdrop token support to be available.'));
    }
    if (trim((string) ($values['automator_token'] ?? '')) === '') {
      form_set_error('automator_token', t('Enter a token prompt template for Token mode.'));
    }
  }

  if (($values['automator_type'] ?? '') === 'entity_reference' && ($values['automator_entityreference_mode'] ?? 'map_existing') === 'generate_new') {
    if (empty($values['automator_entityreference_generate_bundle'])) {
      form_set_error('automator_entityreference_generate_bundle', t('Choose a target bundle for generated references.'));
    }
    $has_generated_field = FALSE;
    foreach ($values as $key => $value) {
      if (strpos($key, 'automator_entity_field_enable_') === 0 && !empty($value)) {
        $field_name = str_replace('automator_entity_field_enable_', '', $key);
        if (trim((string) ($values['automator_entity_field_generate_' . $field_name] ?? '')) !== '') {
          $has_generated_field = TRUE;
          break;
        }
      }
    }
    if (!$has_generated_field) {
      form_set_error('automator_entityreference_generate_fields', t('Enable at least one target field and provide a generation prompt.'));
    }
  }

  if (($values['automator_type'] ?? '') === 'moderation_state') {
    if (trim((string) ($values['automator_moderation_trigger_states'] ?? '')) === '') {
      form_set_error('automator_moderation_trigger_states', t('Enter at least one trigger state.'));
    }
    if (trim((string) ($values['automator_moderation_lookup_states'] ?? '')) === '') {
      form_set_error('automator_moderation_lookup_states', t('Enter at least one lookup state.'));
    }
  }

  if (in_array($type, ['vector_search_text', 'vector_search_entity_reference'], TRUE)) {
    if (empty($values['automator_search_index'])) {
      form_set_error('automator_search_index', t('Choose a search index.'));
    }
    if (!is_numeric($values['automator_search_minimum_score'] ?? 0)) {
      form_set_error('automator_search_minimum_score', t('Minimum score must be numeric.'));
    }
    else {
      $minimum = (float) ($values['automator_search_minimum_score'] ?? 0);
      if ($minimum < 0 || $minimum > 1) {
        form_set_error('automator_search_minimum_score', t('Minimum score must be between 0 and 1.'));
      }
    }
    if ($type === 'vector_search_text' && empty($values['automator_search_output_field'])) {
      form_set_error('automator_search_output_field', t('Choose an output field.'));
    }
  }

  if ($type === 'views_extract_text' && empty($values['automator_views_view'])) {
    form_set_error('automator_views_view', t('Choose a View display.'));
  }
}

/**
 * Submit handler: saves or deletes the per-field automator config.
 */
function _ai_field_automator_field_edit_form_submit($form, &$form_state) {
  $instance    = $form['#instance'];
  $entity_type = $instance['entity_type'];
  $bundle      = $instance['bundle'];
  $field_name  = $instance['field_name'];
  $values      = $form_state['values'];

  $config_key = 'ai_field_automator.' . $entity_type . '.' . $bundle . '.' . $field_name;

  if (!empty($values['automator_enabled'])) {
    $data = [
      'enabled'        => TRUE,
      'label'          => trim((string) ($values['automator_label'] ?? '')),
      'entity_type'    => $entity_type,
      'bundle'         => $bundle,
      'field_name'     => $field_name,
      'automator_type' => $values['automator_type'] ?? '',
      'mode'           => $values['automator_mode'] ?? 'base',
      'base_field'     => $values['automator_base_field'] ?? '',
      'prompt'         => $values['automator_prompt'] ?? '',
      'token'          => $values['automator_token'] ?? '',
      'model'          => $values['automator_model'] ?? '',
      'edit_mode'      => $values['automator_edit_mode'] ?? 'skip',
      'worker_type'    => $values['automator_worker_type'] ?? 'direct',
      'weight'         => (int) ($values['automator_weight'] ?? 0),
      'temperature'    => (float) ($values['automator_temperature'] ?? 0.7),
      'frequency_penalty' => (float) ($values['automator_frequency_penalty'] ?? 0),
      'presence_penalty'  => (float) ($values['automator_presence_penalty'] ?? 0),
      'top_p'          => (float) ($values['automator_top_p'] ?? 1),
      'max_tokens'     => (int) ($values['automator_max_tokens'] ?? 1024),
      'reasoning_effort' => $values['automator_reasoning_effort'] ?? '',
      'image_field'    => $values['automator_image_field'] ?? '',
      'text_format'    => $values['automator_text_format'] ?? '',
      'joiner'         => $values['automator_joiner'] ?? '',
      'joiner_other'   => $values['automator_joiner_other'] ?? '',
      'taxonomy_cleanup' => $values['automator_taxonomy_cleanup'] ?? '',
      'taxonomy_auto_create' => !empty($values['automator_taxonomy_auto_create']),
      'taxonomy_search_similar_tags' => !empty($values['automator_taxonomy_search_similar_tags']),
      'entityreference_auto_create' => !empty($values['automator_entityreference_auto_create']),
      'entityreference_create_bundle' => $values['automator_entityreference_create_bundle'] ?? '',
      'entityreference_mode' => $values['automator_entityreference_mode'] ?? 'map_existing',
      'entityreference_generate_bundle' => $values['automator_entityreference_generate_bundle'] ?? '',
      'entityreference_generate_fields' => [],
      'image_size' => $values['automator_image_size'] ?? '1024x1024',
      'image_quality' => $values['automator_image_quality'] ?? 'standard',
      'image_style' => $values['automator_image_style'] ?? 'natural',
      'image_response_format' => $values['automator_image_response_format'] ?? 'b64_json',
      'image_output_format' => $values['automator_image_output_format'] ?? '',
      'image_filename_pattern' => trim((string) ($values['automator_image_filename_pattern'] ?? '')),
      'speech_task' => $values['automator_speech_task'] ?? 'transcribe',
      'speech_model' => trim((string) ($values['automator_speech_model'] ?? (config_get('ai.settings', 'default_stt_model') ?: 'whisper-1'))),
      'tts_voice' => $values['automator_tts_voice'] ?? 'alloy',
      'tts_response_format' => $values['automator_tts_response_format'] ?? 'mp3',
      'tts_filename_pattern' => trim((string) ($values['automator_tts_filename_pattern'] ?? '')),
      'json_schema' => trim((string) ($values['automator_json_schema'] ?? '')),
      'metatag_keys' => ai_field_automator_parse_csv_list($values['automator_metatag_keys'] ?? ''),
      'moderation_trigger_states' => ai_field_automator_parse_csv_list($values['automator_moderation_trigger_states'] ?? ''),
      'moderation_lookup_states' => ai_field_automator_parse_csv_list($values['automator_moderation_lookup_states'] ?? ''),
      'moderation_use_simple_model' => !empty($values['automator_moderation_use_simple_model']),
      'moderation_store_explanation_field' => trim((string) ($values['automator_moderation_store_explanation_field'] ?? '')),
      'search_index' => trim((string) ($values['automator_search_index'] ?? '')),
      'search_output_field' => trim((string) ($values['automator_search_output_field'] ?? 'content')),
      'search_max_results' => max(1, (int) ($values['automator_search_max_results'] ?? 10)),
      'search_offset' => max(0, (int) ($values['automator_search_offset'] ?? 0)),
      'search_minimum_score' => (float) ($values['automator_search_minimum_score'] ?? 0),
      'search_distinct' => !empty($values['automator_search_distinct']),
      'views_view' => trim((string) ($values['automator_views_view'] ?? '')),
      'views_arguments' => ai_field_automator_parse_line_list($values['automator_views_arguments'] ?? ''),
      'views_exposed_filters' => ai_field_automator_parse_key_value_lines($values['automator_views_exposed_filters'] ?? ''),
      'views_html_to_markdown' => !empty($values['automator_views_html_to_markdown']),
      'agent_id'   => trim((string) ($values['automator_agent_id'] ?? '')),
      'agent_task' => trim((string) ($values['automator_agent_task'] ?? '')),
    ];
    foreach ($values as $key => $value) {
      if (strpos($key, 'automator_entity_field_enable_') === 0 && !empty($value)) {
        $target_field = str_replace('automator_entity_field_enable_', '', $key);
        $prompt_key = 'automator_entity_field_generate_' . $target_field;
        $prompt_value = trim((string) ($values[$prompt_key] ?? ''));
        if ($prompt_value !== '') {
          $data['entityreference_generate_fields'][$target_field] = $prompt_value;
        }
      }
    }
    config($config_key)->setData($data)->save();
    backdrop_set_message(t('AI Automator configuration saved for %field.', ['%field' => $field_name]));
  }
  else {
    // Disable — delete the config object if it exists.
    $existing = config($config_key);
    if (!$existing->isNew()) {
      $existing->delete();
    }
  }

  // Clear the per-bundle static cache.
  backdrop_static_reset('ai_field_automator_load_field_config');
}

/**
 * Normalizes comma-separated input into unique string list.
 *
 * @param string|array $value
 *   CSV string or array.
 *
 * @return array
 *   Unique, trimmed list.
 */
function ai_field_automator_parse_csv_list($value) {
  if (is_array($value)) {
    $parts = $value;
  }
  else {
    $parts = preg_split('/\s*,\s*/', trim((string) $value), -1, PREG_SPLIT_NO_EMPTY);
  }
  $parts = array_map('trim', $parts);
  $parts = array_filter($parts, 'strlen');
  return array_values(array_unique($parts));
}

/**
 * Returns enabled Search API index options keyed by machine name.
 */
function ai_field_automator_get_search_index_options() {
  if (!module_exists('search_api') || !function_exists('search_api_index_load_multiple')) {
    return [];
  }

  $options = [];
  $indexes = search_api_index_load_multiple(FALSE, ['enabled' => 1]);
  foreach ($indexes as $index) {
    $label = !empty($index->name) ? $index->name : $index->machine_name;
    $options[$index->machine_name] = $label . ' (' . $index->machine_name . ')';
  }

  return $options;
}

/**
 * Returns index field options for a selected Search API index.
 */
function ai_field_automator_get_search_index_field_options($index_id) {
  $options = ['content' => t('Content')];

  if (!module_exists('search_api') || !function_exists('search_api_index_load') || empty($index_id)) {
    return $options;
  }

  $index = search_api_index_load($index_id);
  if (!$index || empty($index->enabled) || !method_exists($index, 'getFields')) {
    return $options;
  }

  foreach ($index->getFields(FALSE) as $field_id => $field_info) {
    if ($field_id === 'content') {
      continue;
    }
    $options[$field_id] = !empty($field_info['name']) ? $field_info['name'] : $field_id;
  }

  return $options;
}

/**
 * Returns enabled view display options keyed by "view:display".
 */
function ai_field_automator_get_view_display_options() {
  if (!module_exists('views') || !function_exists('views_get_views_as_options')) {
    return [];
  }
  return views_get_views_as_options(FALSE, 'enabled', NULL, TRUE, TRUE);
}

/**
 * Parses a textarea into an ordered list (one non-empty line per item).
 */
function ai_field_automator_parse_line_list($value) {
  if (is_array($value)) {
    $parts = $value;
  }
  else {
    $parts = preg_split('/\R+/', trim((string) $value), -1, PREG_SPLIT_NO_EMPTY);
  }
  $parts = array_map('trim', $parts);
  $parts = array_filter($parts, 'strlen');
  return array_values($parts);
}

/**
 * Parses "key=value" lines into an associative array.
 */
function ai_field_automator_parse_key_value_lines($value) {
  $lines = ai_field_automator_parse_line_list($value);
  $out = [];
  foreach ($lines as $line) {
    $parts = explode('=', $line, 2);
    $key = trim((string) ($parts[0] ?? ''));
    $item_value = trim((string) ($parts[1] ?? ''));
    if ($key === '' || $item_value === '') {
      continue;
    }
    $out[$key] = $item_value;
  }
  return $out;
}

/**
 * Builds a #states 'visible' condition for one or more automator types.
 *
 * Returns an OR condition: the element is visible when the automator_type
 * select matches ANY of the given $types values.
 *
 * @param string[] $types
 *   One or more automator type machine names.
 *
 * @return array
 *   A #states array ready to drop onto a form element.
 */
function _ai_field_automator_type_states(array $types) {
  $conditions = [];
  foreach ($types as $type) {
    $conditions[] = ['select[name="automator_type"]' => ['value' => $type]];
  }
  return ['visible' => $conditions];
}

/**
 * Return provider-aware TTS voice options for the selected model.
 *
 * @param string $model
 *   Fully-qualified model ID.
 *
 * @return array
 *   Voice value => label options.
 */
function ai_field_automator_tts_voice_options($model = '') {
  $default_options = [
    'alloy' => 'alloy',
    'ash' => 'ash',
    'ballad' => 'ballad',
    'coral' => 'coral',
    'echo' => 'echo',
    'fable' => 'fable',
    'nova' => 'nova',
    'onyx' => 'onyx',
    'sage' => 'sage',
    'shimmer' => 'shimmer',
  ];

  if (empty($model) || strpos($model, 'elevenlabs/') !== 0) {
    return $default_options;
  }

  $stripped_model = '';
  $api = ai_get_api_for_model($model, $stripped_model);
  if (!$api || !method_exists($api, 'getVoices')) {
    return $default_options;
  }

  try {
    $voices = $api->getVoices($stripped_model);
    if (!empty($voices) && is_array($voices)) {
      return $voices;
    }
  }
  catch (\Exception $e) {
    watchdog('ai_field_automator', 'Could not load TTS voices for @model: @msg', [
      '@model' => $model,
      '@msg' => $e->getMessage(),
    ], WATCHDOG_WARNING);
  }

  return $default_options;
}

/**
 * Checks whether a grouped model options array contains a given model ID.
 */
function ai_field_automator_model_option_exists(array $model_options, $model_id) {
  foreach ($model_options as $group) {
    if (is_array($group) && array_key_exists($model_id, $group)) {
      return TRUE;
    }
  }
  return FALSE;
}
