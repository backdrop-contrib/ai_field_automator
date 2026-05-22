# AI Field Automator — Automator Types Reference

The **AI Field Automator** module for Backdrop CMS automatically populates entity fields with AI-generated content when content is saved. Each automator type is a plugin that handles a specific kind of field and generation task.

This document covers all **35 automator types** included with the module, organized by category.

---

## Table of Contents

- [Overview](#overview)
- [Text Types](#text-types)
- [Scalar / Simple Value Types](#scalar--simple-value-types) _(includes Date Extract — Backdrop-only)_
- [Reference Types](#reference-types)
- [Media Types](#media-types)
- [Structured Data Types](#structured-data-types)
- [Domain-Specific Types](#domain-specific-types)
- [Search & Views Types](#search--views-types)
- [Configuration Reference](#configuration-reference)
- [Drupal Parity Notes](#drupal-parity-notes)

---

## Overview

Each automator type declares:

- **Field types** it supports (e.g. `text_long`, `image`, `entityreference`)
- Whether it **requires a model** (most do; search/views types do not)
- Whether it **requires a prompt** (most do; some generate from existing field data)
- Whether it **requires a base field** for context

When you edit a field under **Structure → Content Types → [type] → Manage fields**, the AI Automator section only shows automator types compatible with that field's type.

---

## Text Types

### Simple Text
| | |
|---|---|
| **Type ID** | `simple_text` |
| **Class** | `AIFieldAutomatorSimpleText` |
| **Field types** | `text`, `string`, `string_long` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Generates a short AI response and stores it in a plain text or string field. Ideal for titles, taglines, short descriptions, or any single-line/short-form text.

**Example use cases:**
- Generate a meta title from body content
- Create a short tagline from an article
- Extract a key phrase or headline

---

### Long Text / Body
| | |
|---|---|
| **Type ID** | `long_text` |
| **Class** | `AIFieldAutomatorLongText` |
| **Field types** | `text_long`, `text_with_summary` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Generates longer-form AI content for body fields and text areas. Preserves HTML if the field uses a text format. For `text_with_summary` fields, also auto-generates a summary (first 500 characters, stripped of tags).

**Example use cases:**
- Generate an article body from a topic prompt
- Rewrite or expand existing content
- Generate product descriptions from structured data

---

### Create Summary
| | |
|---|---|
| **Type ID** | `text_create_summary` |
| **Class** | `AIFieldAutomatorTextCreateSummary` |
| **Field types** | `text_with_summary` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Generates **only the summary** portion of a `text_with_summary` field. The main body text is left completely untouched. This is useful when the body is written by a human but you want AI to create the teaser/summary.

**Example use cases:**
- Auto-generate teaser text for existing articles
- Create search-optimized summaries from long-form content
- Generate excerpt text for content that already has a body

---

## Scalar / Simple Value Types

### Boolean
| | |
|---|---|
| **Type ID** | `boolean` |
| **Class** | `AIFieldAutomatorBoolean` |
| **Field types** | `list_boolean`, `boolean` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Uses AI to classify content as true or false. The AI is prompted to return `1` (true) or `0` (false) based on the context.

**Example use cases:**
- Flag content as "contains sensitive material"
- Determine if an article is "family friendly"
- Classify whether a post mentions a specific topic

---

### Numeric
| | |
|---|---|
| **Type ID** | `numeric` |
| **Class** | `AIFieldAutomatorNumeric` |
| **Field types** | `number_integer`, `number_decimal`, `number_float`, `integer`, `decimal`, `float` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Extracts numeric values from context text. The AI is prompted to return only a number.

**Example use cases:**
- Extract a price from product description text
- Determine a reading time estimate
- Extract a year or count from context

---

### Email
| | |
|---|---|
| **Type ID** | `email` |
| **Class** | `AIFieldAutomatorEmail` |
| **Field types** | `email` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Extracts or generates email addresses from context text.

**Example use cases:**
- Extract contact email from scraped text content
- Parse email addresses from unstructured data

---

### Telephone
| | |
|---|---|
| **Type ID** | `telephone` |
| **Class** | `AIFieldAutomatorTelephone` |
| **Field types** | `telephone`, `phone` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Extracts telephone numbers with country code from context text.

**Example use cases:**
- Extract phone numbers from contact information text
- Parse and standardize phone numbers from unstructured content

---

### List Options
| | |
|---|---|
| **Type ID** | `options` |
| **Class** | `AIFieldAutomatorOptions` |
| **Field types** | `list_text`, `list_integer`, `list_float` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Selects valid values from the field's allowed options list. The AI is given the list of allowed values and chooses the most appropriate one(s) based on the context.

**Example use cases:**
- Auto-categorize content from a predefined category list
- Set a priority level (High/Medium/Low) based on content analysis
- Select a content rating from predefined options

---

### Link / URL
| | |
|---|---|
| **Type ID** | `link` |
| **Class** | `AIFieldAutomatorLink` |
| **Field types** | `link_field`, `link`, `url` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Extracts a URL and optional link title from context text into link fields.

**Example use cases:**
- Extract source URLs from article text
- Generate "read more" links from content references
- Parse URLs from unstructured data

---

### Date Extract ⭐ _Backdrop-only_
| | |
|---|---|
| **Type ID** | `date_extract` |
| **Class** | `AIFieldAutomatorDateExtract` |
| **Field types** | `date`, `datetime`, `datestamp` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Extracts dates from free-form text (titles, body fields, rendered content) and stores them in a Backdrop date field. This automator does not exist in the Drupal AI Automators module — it was created specifically for this Backdrop port.

The AI is prompted to return dates in ISO 8601 format regardless of the input format (e.g. "March 3rd, 2025", "next Tuesday", "03/15/2025", "the fifteenth of March" are all handled). The plugin then stores the result in the correct format for the field's storage type:

| Field type | Storage format |
|-----------|----------------|
| `date` | ISO varchar: `Y-m-d\TH:i:s` |
| `datetime` | MySQL DATETIME: `Y-m-d H:i:s` |
| `datestamp` | Unix timestamp (integer) |

If the field has "to date" (end date) enabled, the prompt asks for both a start and end date.

**Example use cases:**
- Extract event dates from unstructured event description text
- Parse "published date" from imported content with inconsistent formats
- Extract deadline dates from contract or document text
- Pull birth/death dates from biographical text into a date range field

---

## Reference Types

### Taxonomy Terms
| | |
|---|---|
| **Type ID** | `taxonomy_terms` |
| **Class** | `AIFieldAutomatorTaxonomyTerms` |
| **Field types** | `taxonomy_term_reference` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Generates taxonomy terms/tags from context and maps them to term IDs in the allowed vocabulary. This is the most thoroughly tested automator type.

**Type-specific options:**
- **Tag text manipulation** — Cleanup options: none, lowercase, UPPERCASE, or First character uppercase
- **Auto-create missing terms** — Create new terms in the vocabulary when the AI suggests terms that don't exist yet
- **Find similar tags** — Uses an additional AI pass to map near-match generated tags to existing tags (best with larger models)

**Example use cases:**
- Auto-tag articles with relevant topic tags
- Generate category terms from article content
- Build a tag cloud automatically from body text

---

### Entity Reference
| | |
|---|---|
| **Type ID** | `entity_reference` |
| **Class** | `AIFieldAutomatorEntityReference` |
| **Field types** | `entityreference` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Maps AI-generated values to existing referenceable entities, or optionally generates new entities.

**Type-specific options:**
- **Entity reference mode** — Map to existing entities OR generate and create new ones
- **Auto-create missing referenced entities** — Create new nodes when a match isn't found
- **Bundle to create** — Which content type to create for auto-created entities
- **Generate target bundle / fields** — When generating new entities, specify which bundle and fields to auto-populate

**Example use cases:**
- Link related articles automatically
- Generate "see also" references based on content similarity
- Create and link new entities from extracted data

---

## Media Types

### Text to Image
| | |
|---|---|
| **Type ID** | `text_to_image` |
| **Class** | `AIFieldAutomatorTextToImage` |
| **Field types** | `image`, `file` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Generates images from prompt text using an AI image generation model (e.g. DALL-E) and stores the result in image or file fields.

**Type-specific options:**
- **Image size** — 256×256 to 1792×1024
- **Image quality** — Standard or HD
- **Image style** — Natural or Vivid
- **Response format** — b64_json or URL
- **Output format** — PNG, JPG, WebP, or provider default
- **Image filename** — Optional fixed filename pattern

---

### Image ALT Text
| | |
|---|---|
| **Type ID** | `image_alt_text` |
| **Class** | `AIFieldAutomatorImageAltText` |
| **Field types** | `image` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Generates descriptive alt text for images using a vision-capable model. The base field should be the image field itself.

**Example use cases:**
- Automatically generate accessible alt text for uploaded images
- Improve SEO by adding descriptive image alt attributes

---

### Audio to Text
| | |
|---|---|
| **Type ID** | `audio_to_text` |
| **Class** | `AIFieldAutomatorAudioToText` |
| **Field types** | `text`, `text_long`, `text_with_summary`, `string`, `string_long` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Transcribes audio files into text using a speech-to-text model (e.g. Whisper).

**Type-specific options:**
- **Speech task** — Transcribe (same language) or Translate to English

---

### Text to Speech
| | |
|---|---|
| **Type ID** | `text_to_speech` |
| **Class** | `AIFieldAutomatorTextToSpeech` |
| **Field types** | `file` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Generates speech audio files from text prompts using a TTS model.

**Type-specific options:**
- **TTS voice** — alloy, ash, ballad, coral, echo, fable, nova, onyx, sage, shimmer
- **Audio output format** — mp3, opus, aac, flac, wav, pcm
- **Audio filename** — Optional fixed filename pattern

---

### Video to Text
| | |
|---|---|
| **Type ID** | `video_to_text` |
| **Class** | `AIFieldAutomatorVideoToText` |
| **Field types** | `text`, `text_long`, `text_with_summary`, `string`, `string_long` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Transcribes speech from video files into text fields. Extracts the audio track and processes it through a speech-to-text model.

**Type-specific options:**
- **Speech task** — Transcribe or Translate to English

---

### Video to HTML
| | |
|---|---|
| **Type ID** | `video_to_html` |
| **Class** | `AIFieldAutomatorVideoToHtml` |
| **Field types** | `text_long`, `text_with_summary`, `string_long` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Transcribes video audio and then uses a language model to generate formatted HTML content from the transcript.

**Type-specific options:**
- **Speech-to-text model** — Fully-qualified model ID for transcription step (e.g. `provider/whisper-1`)

---

### Video to Image
| | |
|---|---|
| **Type ID** | `video_to_image` |
| **Class** | `AIFieldAutomatorVideoToImage` |
| **Field types** | `image`, `file` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Generates representative images from video transcript context. Transcribes the video first, then uses the transcript as context for image generation.

**Type-specific options:**
- **Speech-to-text model** — Model for the transcription step
- All image generation options (size, quality, style, etc.)

---

### Video to Video
| | |
|---|---|
| **Type ID** | `video_to_video` |
| **Class** | `AIFieldAutomatorVideoToVideo` |
| **Field types** | `file` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

The most complex automator type. Uses AI to select timestamps from a source video, then cuts and optionally combines video segments using FFmpeg.

**Requirements:**
- FFmpeg must be installed on the server

---

### Media Image Generation
| | |
|---|---|
| **Type ID** | `media_image_generation` |
| **Class** | `AIFieldAutomatorMediaImageGeneration` |
| **Field types** | `entityreference` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Like Text to Image, but stores the generated image as a referenced file/media entity rather than directly in an image field. Useful when your image field is an entity reference to a media type.

**Type-specific options:**
- All image generation options (size, quality, style, etc.)

---

### Media Audio Generation
| | |
|---|---|
| **Type ID** | `media_audio_generation` |
| **Class** | `AIFieldAutomatorMediaAudioGeneration` |
| **Field types** | `entityreference` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Like Text to Speech, but stores the generated audio as a referenced file/media entity.

**Type-specific options:**
- All TTS options (voice, format, filename)

---

### Rewrite Image Filename
| | |
|---|---|
| **Type ID** | `rewrite_image_filename` |
| **Class** | `AIFieldAutomatorRewriteImageFilename` |
| **Field types** | `image`, `file` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Renames existing image/file assets to AI-generated SEO-friendly filenames. The file itself is renamed on disk and in the database.

**Example use cases:**
- Rename `IMG_20240301_143022.jpg` to `ama-foundation-scholarship-ceremony-2024.jpg`
- Improve image SEO by giving files descriptive names

---

## Structured Data Types

### JSON Field
| | |
|---|---|
| **Type ID** | `json_field` |
| **Class** | `AIFieldAutomatorJsonStructured` |
| **Field types** | `json`, `text_long`, `string_long` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Generates structured JSON output. You can provide a schema or example JSON that the model should follow.

**Type-specific options:**
- **JSON schema / example** — Optional schema or JSON example for the model to follow

---

### JSON Native Field
| | |
|---|---|
| **Type ID** | `json_native_field` |
| **Class** | `AIFieldAutomatorJsonStructured` |
| **Field types** | `json_native` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Same as JSON Field but for `json_native` field types. Uses the same plugin class.

---

### JSON Native Binary Field
| | |
|---|---|
| **Type ID** | `json_native_binary_field` |
| **Class** | `AIFieldAutomatorJsonStructured` |
| **Field types** | `json_native_binary` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Same as JSON Field but for `json_native_binary` field types. Uses the same plugin class.

---

### Metatag
| | |
|---|---|
| **Type ID** | `metatag` |
| **Class** | `AIFieldAutomatorMetatag` |
| **Field types** | `metatag` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Generates SEO metadata values for metatag fields.

**Type-specific options:**
- **Metatag keys** — Comma-separated keys to generate (e.g. `title, description, og:title, og:description`)

---

### Moderation State
| | |
|---|---|
| **Type ID** | `moderation_state` |
| **Class** | `AIFieldAutomatorModerationState` |
| **Field types** | `string`, `list_text` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Classifies content into moderation/workflow states using AI. Supports both strict JSON parsing and free-text state matching.

**Type-specific options:**
- **Trigger states** — Comma-separated states that trigger this automator
- **Lookup states** — Valid output states the model is allowed to choose from
- **Use simple moderation parse mode** — Parses state from free text instead of requiring strict JSON
- **Store moderation explanation in** — Optionally stores the AI's reasoning in another text field

---

## Domain-Specific Types

### Address
| | |
|---|---|
| **Type ID** | `address` |
| **Class** | `AIFieldAutomatorAddress` |
| **Field types** | `address`, `addressfield` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Extracts structured address components (street, city, state, zip, country) from unstructured text.

**Example use cases:**
- Parse mailing addresses from pasted text
- Extract location data from event descriptions

---

### Office Hours
| | |
|---|---|
| **Type ID** | `office_hours` |
| **Class** | `AIFieldAutomatorOfficeHours` |
| **Field types** | `office_hours`, `officehours` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Extracts business/opening hours from text and structures them into office-hours field format.

**Example use cases:**
- Parse "Mon–Fri 9am–5pm, Sat 10am–2pm" into structured field data

---

### FAQ Field
| | |
|---|---|
| **Type ID** | `faq_field` |
| **Class** | `AIFieldAutomatorFaqField` |
| **Field types** | `faqfield` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Generates question/answer pairs from context text.

**Example use cases:**
- Auto-generate FAQ sections from product documentation
- Create Q&A content from article text

---

### Chart from Text
| | |
|---|---|
| **Type ID** | `chart_from_text` |
| **Class** | `AIFieldAutomatorChartFromText` |
| **Field types** | `chart_config` |
| **Requires model** | Yes |
| **Requires prompt** | Yes |

Generates chart-ready data (table/CSV) from context text for visualization fields.

**Example use cases:**
- Extract financial data from reports into chart format
- Generate data visualizations from narrative text

---

## Search & Views Types

These types do **not** require an AI model — they work by querying existing data sources.

### Vector Search: Text
| | |
|---|---|
| **Type ID** | `vector_search_text` |
| **Class** | `AIFieldAutomatorVectorSearchText` |
| **Field types** | `text`, `text_long`, `text_with_summary`, `string`, `string_long` |
| **Requires model** | No |
| **Requires prompt** | No |

Uses a Search API AI index with vector embeddings to retrieve semantically similar text snippets and store them in text fields.

**Type-specific options:**
- **Search index** — Which Search API index to query
- **Output field** — Which field from the index results to extract
- **Maximum results** — How many matches to use
- **Result offset** — Skip the first N matches
- **Minimum score** — Threshold for accepted matches (0–1)
- **Distinct results** — Avoid duplicates

---

### Vector Search: Entity Reference
| | |
|---|---|
| **Type ID** | `vector_search_entity_reference` |
| **Class** | `AIFieldAutomatorVectorSearchEntityReference` |
| **Field types** | `entityreference` |
| **Requires model** | No |
| **Requires prompt** | No |

Uses a Search API AI index to find semantically similar content and stores references to those entities.

**Type-specific options:**
- Same as Vector Search: Text (search index, max results, offset, score, distinct)

---

### Views: Text Extract
| | |
|---|---|
| **Type ID** | `views_extract_text` |
| **Class** | `AIFieldAutomatorViewsExtractor` |
| **Field types** | `text`, `text_long`, `text_with_summary`, `string`, `string_long` |
| **Requires model** | No |
| **Requires prompt** | No |
| **Requires base field** | No |

Executes a configured View display and stores its rendered output as text. No AI model is needed — this is purely a data extraction tool.

**Type-specific options:**
- **View display** — Which View and display ID to execute (format: `view_name:display_id`)
- **View arguments** — One argument value per line, supports token replacements
- **Exposed filters** — One per line in `identifier=value` format
- **Convert rendered HTML to plain text** — Strips HTML tags from the output

---

## Configuration Reference

### Common Settings (All Types)

| Setting | Description |
|---------|-------------|
| **Automator type** | Which plugin handles this field |
| **Input mode** | Base mode (uses a source field) or Token mode (uses token template) |
| **Base field** | Which field provides context — includes Title, any field, or "Rendered entity text" |
| **Prompt** | Instructions to the AI for generating content |

### Provider Configuration (Types Requiring a Model)

| Setting | Description | Default |
|---------|-------------|---------|
| **Model** | Fully-qualified model ID grouped by provider | — |
| **Temperature** | Sampling randomness (0–2.0) | 0.7 |
| **Frequency Penalty** | Penalize repeated tokens (-2.0 to 2.0) | 0 |
| **Presence Penalty** | Penalize tokens already in text (-2.0 to 2.0) | 0 |
| **Top P** | Nucleus sampling (0–1) | 1 |
| **Max Tokens** | Maximum output tokens | 1024 |
| **Reasoning Effort** | For reasoning models (o1, o3, o4-mini, etc.): Low/Medium/High. Ignored by standard models. | Not set |
| **Image Field** | Include images from this field in the prompt. Only used when a vision-capable model is selected. | No images |
| **Use text format** | Text format for filtered text fields | None/User Based |

### Advanced Settings

| Setting | Description | Default |
|---------|-------------|---------|
| **Edit mode** | Skip (never overwrite) / Edit when changed / Always overwrite | Skip |
| **Automator Worker** | Widget action (on-demand), Direct, Batch (JS queue), or Queue/Cron | Direct |
| **Weight** | Execution order when multiple automators exist (lower = first) | 0 |
| **Joiner** | Combine multiple AI values into one using a separator | Don't join |
| **Use text format** | Text format for filtered text fields | None/User Based |

### Config Storage

Each automator configuration is stored as a Backdrop config object:

```
ai_field_automator.{entity_type}.{bundle}.{field_name}
```

Example: `ai_field_automator.node.post.field_tags`

---

## Drupal Parity Notes

This Backdrop module is ported from the Drupal **AI Automators** module (`ai_automators`), part of the Drupal AI suite. Key differences:

| Aspect | Drupal | Backdrop |
|--------|--------|----------|
| **Number of automator types** | ~47 (separate plugin per field type) | 35 (consolidated — one plugin handles multiple field types; includes 1 Backdrop-only type) |
| **Plugin system** | Drupal Plugin Manager with annotations | Backdrop hooks + class autoload |
| **Configuration** | Config entities | Backdrop Config API (JSON files) |
| **Automator chains** | Separate entity type + ordering UI | Chain ordering page per bundle with tabledrag (`admin/config/ai/field-automator/chain/{entity_type}/{bundle}`) |
| **Status field** | Auto-created `ai_automator_status` `list_string` field | Auto-created `ai_automator_status` `list_text` field (pending/processing/failed/finished) |
| **Field widget actions** | 26 widget-level action plugins | Implemented as an in-module MVP: `worker_type = widget_action` adds a `Generate with AI` button for supported text-like widgets |
| **Entity reference filtering** | Plugin `target` annotation restricts to specific entity types | `target_types` array in plugin registry; checked against `$field['settings']['target_type']` |
| **Model capability UI** | Dynamic show/hide via JS for reasoning/vision fields | Fields always shown; descriptions indicate when each applies |
| **Form conditional fields** | Drupal `#states` | Backdrop `#states` (identical behavior) |

### Functional Equivalence

Despite fewer plugin files, the Backdrop module covers the same functional surface:
- Drupal has separate `llm_simple_text`, `llm_text`, `llm_string`, `llm_simple_string`, `llm_text_long`, `llm_string_long`, `llm_simple_text_long`, `llm_simple_string_long` — eight plugins that do the same thing for different field types. Backdrop has **two**: `simple_text` and `long_text`, each handling multiple field types.
- Same pattern applies across all categories.

### Automator Status Field

The module automatically creates an `ai_automator_status` field on any content type bundle that has at least one automator configured. The field is a `list_text` with four allowed values:

| Value | Label | When Set |
|-------|-------|----------|
| `pending` | Pending | Default value when entity is created |
| `processing` | Processing | Set when automators begin executing on save |
| `failed` | Failed | Set if any automator encounters an error |
| `finished` | Finished | Set after all automators complete successfully |

- **Auto-created** when the first automator is enabled on a bundle.
- **Auto-removed** when the last automator is disabled/deleted from a bundle.
- Useful for building Views that show entities awaiting or failing AI processing.
- Can be used to re-trigger processing on failed entities.

### Automator Chains (Ordering)

Visit **admin/config/ai/field-automator/chain/{entity_type}/{bundle}** to see all automators on a content type in a drag-and-drop table. Reorder them to control execution priority — lower weight runs first.

The overview page at **admin/config/ai/field-automator** shows all automators across all content types, with direct "Manage chain" links to each bundle's ordering page.

### Features Not Yet Ported

1. **Full Field Widget Action plugin framework** — Drupal has 26 discrete FieldWidgetAction plugins; Backdrop currently ships one built-in on-demand action flow (`worker_type = widget_action`) for supported text-like widgets.
2. **Alter hooks / events** — Drupal has 5 event types for other modules to intercept automator execution; Backdrop could add `hook_alter()` equivalents.
3. **Automator Tools** — AI agent function-call integration (advanced, low priority).

---

## Drupal → Backdrop File Mapping

The Drupal `ai_automators` module has **~120 PHP files** across its `src/` directory. The Backdrop module covers the same functionality in **~40 `.inc` files + 1 `.module` + 1 `.install`**. This is normal — Backdrop doesn't need DI containers, plugin managers, entity type annotations, route subscribers, service YAML, or PHP 8 attributes.

### Architecture / Orchestration

| Drupal `src/` File | Purpose | Backdrop Equivalent |
|---|---|---|
| `AiAutomatorEntityModifier.php` | Main presave orchestrator | `AIFieldAutomatorRunner.inc` |
| `AiAutomatorRuleRunner.php` | Runs one rule (generate → verify → store) | Logic in Runner + Base class |
| `AiAutomatorStatusField.php` | Status field lifecycle (create/remove) | `AIFieldAutomatorStatusField.inc` |
| `AiAutomatorCleanupService.php` | Cleanup on entity delete | `hook_field_delete_instance()` in `.module` |
| `AiFieldRules.php` | Rule discovery / registry | `ai_field_automator_get_types()` in `.module` |
| `AiPromptHelper.php` | Token/prompt builder | `buildPrompts()` in `AIFieldAutomatorBase.inc` |
| `AiAutomatorInterface.php` | Entity interface | `AIFieldAutomatorInterface.inc` |
| `Service/Automate.php` | Programmatic automator trigger | `AIFieldAutomatorRunner::processEntity()` |

### Entity Types (Drupal-specific, not needed in Backdrop)

| Drupal `src/` File | Purpose | Backdrop Equivalent |
|---|---|---|
| `Entity/AiAutomator.php` | Config entity for each automator | Config API objects (`ai_field_automator.{type}.{bundle}.{field}`) |
| `Entity/AutomatorChain.php` | Content entity (empty shell) | Chain ordering form page — no entity needed |
| `Entity/AutomatorChainType.php` | Config entity (bundle def for chains) | Not needed — chains are per-bundle config views |
| `Entity/AutomatorsTool.php` | Tools entity for AI agent functions | Not needed (future/advanced) |
| `AutomatorChainInterface.php` | Empty interface | Not needed |
| `AutomatorsToolInterface.php` | Tool entity interface | Not needed |

### Plugin Infrastructure (replaced by hooks in Backdrop)

| Drupal `src/` File | Purpose | Backdrop Equivalent |
|---|---|---|
| `Attribute/AiAutomatorType.php` | PHP 8 attribute for type plugins | `hook_ai_field_automator_type_info()` |
| `Attribute/AiAutomatorProcessRule.php` | PHP 8 attribute for process plugins | Not needed — worker types are strings in config |
| `PluginManager/AiAutomatorTypeManager.php` | Plugin discovery/factory | `ai_field_automator_get_types()` + `_load_plugin()` |
| `PluginManager/AiAutomatorFieldProcessManager.php` | Process plugin manager | Worker selection logic in Runner |
| `PluginInterfaces/AiAutomatorTypeInterface.php` | Type plugin interface | `AIFieldAutomatorInterface.inc` |
| `PluginInterfaces/AiAutomatorFieldProcessInterface.php` | Process plugin interface | Not needed — inline in Runner |
| `PluginInterfaces/AiAutomatorDirectProcessInterface.php` | Direct-save marker interface | Not needed |

### Forms & UI

| Drupal `src/` File | Purpose | Backdrop Equivalent |
|---|---|---|
| `FormAlter/AiAutomatorFieldConfig.php` | Service: alters field config form | `AIFieldAutomatorFieldForm.inc` |
| `Form/AiAutomatorForm.php` | Entity form for automator config | Field-level form in `FieldForm.inc` |
| `Form/AiChainForm.php` | Chain ordering page | `AIFieldAutomatorChainForm.inc` |
| `Form/AutomatorChainForm.php` | Chain entity CRUD form | Not needed — no entity type |
| `Form/AutomatorChainTypeForm.php` | Chain type CRUD form | Not needed — no entity type |
| `Form/AutomatorsToolForm.php` | Tool entity CRUD form | Not needed |
| `AiAutomatorListBuilder.php` | Admin list builder | `ai_field_automator_overview_page()` |
| `AutomatorChainListBuilder.php` | Chain list admin | Chain form is per-bundle |
| `AutomatorChainTypeListBuilder.php` | Chain type list admin | Not needed |
| `AutomatorsToolListBuilder.php` | Tool list admin | Not needed |

### Routing & Access

| Drupal `src/` File | Purpose | Backdrop Equivalent |
|---|---|---|
| `Routing/AutomatorRouteSubscriber.php` | Dynamic route generation | `hook_menu()` |
| `Routing/AutomatorChainHtmlRouteProvider.php` | Chain entity routes | `hook_menu()` chain route |
| `Routing/AutomatorChainTypeHtmlRouteProvider.php` | Chain type routes | Not needed |
| `Access/AutomatorAdvancedAccessChecker.php` | Route access check | `'access arguments'` in `hook_menu()` |
| `Plugin/Derivative/ChainLocalTasks.php` | Dynamic menu tabs on entity types | Menu items in `hook_menu()` |

### Worker / Process Plugins

| Drupal `src/` File | Purpose | Backdrop Equivalent |
|---|---|---|
| `Plugin/AiAutomatorProcess/DirectSaveProcessing.php` | Direct save worker | `worker_type = 'direct'` logic in Runner |
| `Plugin/AiAutomatorProcess/BatchProcessing.php` | Batch worker | `worker_type = 'batch'` + `_batch_process_field()` |
| `Plugin/AiAutomatorProcess/QueueWorkerProcessor.php` | Cron queue worker | `worker_type = 'queue_cron'` + `_queue_worker()` |
| `Plugin/AiAutomatorProcess/FieldWidgetProcessing.php` | Widget action processor | `worker_type = 'widget_action'` + `hook_field_widget_form_alter()` in `AIFieldAutomatorWidgetActions.inc` |
| `Plugin/QueueWorker/AutomatorFieldData.php` | Cron queue worker plugin | `hook_cron_queue_info()` + callback |
| `Batch/ProcessField.php` | Batch callback | `ai_field_automator_batch_process_field()` |

### Automator Type Plugins (~47 → 36 consolidated)

| Drupal `src/` File | Backdrop `.inc` |
|---|---|
| `LlmSimpleText.php`, `LlmText.php`, `LlmString.php`, `LlmSimpleString.php` | `plugins/text/SimpleText.inc` |
| `LlmTextLong.php`, `LlmStringLong.php`, `LlmSimpleTextLong.php`, `LlmSimpleStringLong.php` | `plugins/text/LongText.inc` |
| `LlmTextWithSummary.php`, `LlmSimpleTextWithSummary.php` | `plugins/text/LongText.inc` (handles summary) |
| `LlmTextCreateSummary.php` | `plugins/text/TextCreateSummary.inc` |
| `LlmBoolean.php` | `plugins/scalar/Boolean.inc` |
| `LlmInteger.php`, `LlmFloat.php`, `LlmDecimal.php` | `plugins/scalar/Numeric.inc` |
| `LlmEmail.php` | `plugins/scalar/Email.inc` |
| `LlmTelephone.php` | `plugins/scalar/Telephone.inc` |
| `LlmListString.php`, `LlmListInteger.php`, `LlmListFloat.php` | `plugins/scalar/Options.inc` |
| `LlmLink.php` | `plugins/scalar/Link.inc` |
| _(not in Drupal)_ | `plugins/scalar/DateExtract.inc` ⭐ Backdrop-only |
| `LlmTaxonomy.php` | `plugins/reference/TaxonomyTerms.inc` |
| `LlmEntityReference.php` | `plugins/reference/EntityReference.inc` |
| `LlmImageGeneration.php` | `plugins/media/TextToImage.inc` |
| `LlmImageAltText.php` | `plugins/media/ImageAltText.inc` |
| `LlmMediaImageGeneration.php` | `plugins/media/MediaImageGeneration.inc` |
| `LlmMediaAudioGeneration.php` | `plugins/media/MediaAudioGeneration.inc` |
| `LlmSpeechGeneration.php` | `plugins/media/TextToSpeech.inc` |
| `LlmAudioToTextLong.php`, `LlmAudioToStringLong.php`, `LlmAudioToTextWithSummary.php` | `plugins/media/AudioToText.inc` |
| `LlmVideoToTextLong.php`, `LlmVideoToStringLong.php` | `plugins/media/VideoToText.inc` |
| `LlmVideoToHtml.php` | `plugins/media/VideoToHtml.inc` |
| `LlmVideoToImage.php` | `plugins/media/VideoToImage.inc` |
| `LlmVideoToVideo.php` | `plugins/media/VideoToVideo.inc` |
| `LlmRewriteImageFilename.php` | `plugins/media/RewriteImageFilename.inc` |
| `LlmJsonField.php`, `LlmJsonNative.php`, `LlmJsonNativeBinary.php` | `plugins/structured/JsonStructured.inc` |
| `LlmMetatag.php` | `plugins/structured/Metatag.inc` |
| `LlmModerationState.php` | `plugins/structured/ModerationState.inc` |
| `LlmAddress.php` | `plugins/domain/Address.inc` |
| `LlmOfficeHours.php` | `plugins/domain/OfficeHours.inc` |
| `LlmFaqField.php` | `plugins/domain/FaqField.inc` |
| `LlmChartFromText.php` | `plugins/domain/ChartFromText.inc` |
| `VectorSearchText.php` | `plugins/search/VectorSearchText.inc` |
| `VectorSearchEntityReference.php` | `plugins/search/VectorSearchEntityReference.inc` |
| `ViewsExtractor.php` | `plugins/search/ViewsExtractor.inc` |

### Plugin Base Classes (~30 → 1 consolidated)

| Drupal `src/` File | Backdrop Equivalent |
|---|---|
| `PluginBaseClasses/RuleBase.php` | `AIFieldAutomatorBase.inc` |
| `PluginBaseClasses/SimpleTextChat.php` | Inline in `SimpleText.inc` |
| `PluginBaseClasses/ComplexTextChat.php` | Inline in `LongText.inc` |
| All other 27 base classes | Logic folded into each plugin's `.inc` file |

### Events (Drupal) → Alter Hooks (Backdrop, not yet added)

| Drupal Event | Purpose | Potential Backdrop Hook |
|---|---|---|
| `AutomatorConfigEvent` | Alter config before execution | `hook_ai_field_automator_config_alter()` |
| `ProcessFieldEvent` | Force skip/force process a field | `hook_ai_field_automator_process_alter()` |
| `RuleIsAllowedEvent` | Veto a rule from running | Already in `ruleIsAllowed()` per plugin |
| `ValuesChangeEvent` | Modify generated values post-AI | `hook_ai_field_automator_values_alter()` |
| `VerboseInformationEvent` | Debug/verbose logging | watchdog logging (already works) |

### Helpers, Traits, Misc

| Drupal `src/` File | Backdrop Equivalent |
|---|---|
| `Rulehelpers/GeneralHelper.php` | Joiner logic in Runner; prompt logic in Base |
| `Rulehelpers/FileHelper.php` | Inline in media plugins |
| `Traits/GeneralHelperTrait.php` | Methods on Base class |
| `Traits/FileHelperTrait.php` | Inline in media plugins |
| `Traits/AutomatorInstructionTrait.php` | Chain form logic in `ChainForm.inc` |
| `Exceptions/*.php` (5 files) | Generic `Exception` + watchdog |
| `Plugin/AiCKEditor/AiAutomatorsCKEditor.php` | Not yet implemented |
| `Plugin/AiFunctionCall/*.php` (2 files) | Not needed (AI agent tools) |
| `Plugin/FieldWidgetAction/*.php` (26 files) | Replaced by in-module MVP (`Generate with AI` button) in `AIFieldAutomatorWidgetActions.inc` |
| `Controller/WorkflowAutocomplete.php` | Not needed |

### Not Ported (and why)

| Feature | Drupal Files | Reason |
|---|---|---|
| **Field Widget Actions** | 26 `FieldWidgetAction/*.php` + `FieldWidgetProcessing.php` | Partially ported: in-module `widget_action` worker with AJAX `Generate with AI` button for supported text-like fields. Full plugin parity is future work. |
| **Alter hooks / events** | 5 `Event/*.php` | Low urgency — no third-party modules depend on them yet. Can add `hook_alter()` wrappers when needed. |
| **Automator Tools** | `AutomatorsTool.php`, `AutomatorsToolForm.php`, `AutomatorPluginBase.php`, `AutomatorPluginDeriver.php`, `AutomatorTools.php` | AI agent function-call integration. Advanced feature, low priority. |
| **CKEditor integration** | `AiAutomatorsCKEditor.php` | Would need CKEditor module-specific hooks. Low priority. |

---

*Last updated: 2026-03-03 — added widget action worker (`widget_action`) with on-demand AJAX field generation; updated parity notes and worker settings*
*Module path: `modules/contrib/ai_field_automator/`*
