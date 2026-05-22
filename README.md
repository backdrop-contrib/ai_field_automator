# AI Field Automator

The AI Field Automator module lets you attach AI generation rules directly
to Backdrop field instances. When enabled on a field, the module can populate
that field from another field, from rendered entity content, from tokens, from
media inputs, or from search results depending on the selected automator type.

This module is intended for the Backdrop `ai` ecosystem and follows the
same provider/model configuration used by the rest of the AI modules in
this codebase.

## Features

- Per-field AI automator configuration in Field UI
- Multiple worker modes: direct, batch, queue/cron, and widget action
- Rule ordering by weight
- Text, scalar, reference, media, structured, search, and domain-specific
  automator types
- Optional async widget-action generation when `ai_async` is enabled

## Requirements

- Backdrop CMS 1.x
- The `ai` module

Some automator types require additional modules or field types, for example:

- `token` support for token-mode prompts
- `ai_search` for vector-search automators
- media/file/image-related modules for media automators

## Installation

1. Place the module in `modules/custom/`
2. Enable it with `bee pm-enable ai_field_automator` or from
   `admin/modules`
3. Make sure at least one AI provider/model is configured

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

- `/admin/config/ai/field-automator`

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

- [AUTOMATOR_TYPES.md](/home/justink/Documents/GitHub/amafoundation-backdrop/modules/contrib/ai_field_automator/AUTOMATOR_TYPES.md)

## More Documentation

Implementation details and module architecture are documented separately in:

- [HOW_IT_WORKS.md](/home/justink/Documents/GitHub/amafoundation-backdrop/modules/contrib/ai_field_automator/HOW_IT_WORKS.md)

Focused Xdebug notes for the `post.field_summary` widget-action automator are in:

- [DEBUGGING_FIELD_SUMMARY.md](/home/justink/Documents/GitHub/amafoundation-backdrop/modules/contrib/ai_field_automator/DEBUGGING_FIELD_SUMMARY.md)

## Notes

- Configuration is stored per field, not as standalone config entities
- The module creates an `ai_automator_status` field on bundles that have active
  automators
- For queue/cron workers, choose an explicit text format when needed because
  cron may run as anonymous
## Credits

- Created for Backdrop CMS by [Justin Keiser](https://github.com/keiserjb).
- Inspired by the Drupal AI Automators module.
- Developed with AI assistance.

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.
