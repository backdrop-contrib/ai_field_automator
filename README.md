# OpenAI Field Automator

The OpenAI Field Automator module lets you attach AI generation rules directly
to Backdrop field instances. When enabled on a field, the module can populate
that field from another field, from rendered entity content, from tokens, from
media inputs, or from search results depending on the selected automator type.

This module is intended for the Backdrop `openai` ecosystem and follows the
same provider/model configuration used by the rest of the OpenAI modules in
this codebase.

## Features

- Per-field AI automator configuration in Field UI
- Multiple worker modes: direct, batch, queue/cron, and widget action
- Rule ordering by weight
- Text, scalar, reference, media, structured, search, and domain-specific
  automator types
- Optional async widget-action generation when `openai_async` is enabled

## Requirements

- Backdrop CMS 1.x
- The `openai` module

Some automator types require additional modules or field types, for example:

- `token` support for token-mode prompts
- `search_api_ai` for vector-search automators
- media/file/image-related modules for media automators

## Installation

- Install this module using the official [Backdrop CMS instructions](https://backdropcms.org/user-guide/modules).

## Issues

Bugs and feature requests should be reported in the [Issue Queue](https://github.com/backdrop-contrib/openai_field_automator/issues).

## Configuration

1. Go to a field settings form, for example:
   `admin/structure/types/manage/[content-type]/fields/[field]/edit`
2. Open the **AI Automator** fieldset
3. Enable the automator for that field
4. Choose an automator type
5. Configure the prompt, base field, model, worker type, and any
   type-specific settings
6. Save the field

The module also provides an overview page at:

- `/admin/config/openai/field-automator`

## Worker Modes

- `Direct` runs during save
- `Batch` runs via Backdrop batch processing after form submit
- `Queue/Cron` queues work for cron
- `Widget action` adds a **Generate with AI** button to supported field widgets

## Automator Types

The module includes a broad set of automator types for:

- Text fields
- Numeric, boolean, email, telephone, link, and option fields
- Taxonomy and entity references
- Images, audio, speech, and video workflows
- JSON, metatag, moderation-state, address, office-hours, FAQ, and chart data
- Search API AI vector search and Views extraction

The full type reference is documented in:

- [AUTOMATOR_TYPES.md](/home/justink/Documents/GitHub/amafoundation-backdrop/modules/custom/openai_field_automator/AUTOMATOR_TYPES.md)

## Notes

- Configuration is stored per field, not as standalone config entities
- The module creates an `ai_automator_status` field on bundles that have active
  automators
- For queue/cron workers, choose an explicit text format when needed because
  cron may run as anonymous

## Current Maintainer

[Justin Keiser](https://github.com/keiserjb)

## Credits

- Created for Backdrop CMS by [Justin Keiser](https://github.com/keiserjb).  Inspired by the [Drupal implementation](https://www.drupal.org/project/ai).

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.