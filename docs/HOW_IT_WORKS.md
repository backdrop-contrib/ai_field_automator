# AI Field Automator: How It Works

## Overview

`ai_field_automator` is a Backdrop CMS port of the Drupal
`ai_automators` pattern. It lets you attach an AI automation rule directly to a
field instance and then run that rule during save, by batch, by queue/cron, or
through a widget-action button.

Unlike Drupal `ai_automators`, this Backdrop module stores configuration as
per-field config objects:

- `ai_field_automator.{entity_type}.{bundle}.{field_name}`

## Main pieces

### Field configuration UI

The module adds an **AI Automator** fieldset to the field settings form.

Main file:

- [includes/AIFieldAutomatorFieldForm.inc](/home/justink/Documents/GitHub/amafoundation-backdrop/modules/contrib/ai_field_automator/includes/AIFieldAutomatorFieldForm.inc)

That form stores:

- enabled state
- automator type
- prompt or token template
- base field/context source
- model and advanced AI settings
- worker type
- weight and edit mode
- type-specific options

### Type registry and plugin loading

Automator types are declared in:

- [ai_field_automator.module](/home/justink/Documents/GitHub/amafoundation-backdrop/modules/contrib/ai_field_automator/ai_field_automator.module)

`ai_field_automator_get_types()` collects definitions via
`hook_ai_field_automator_type_info()` and `hook_ai_field_automator_type_info_alter()`.

Each type definition provides:

- label
- supported field types
- plugin class
- include file
- optional flags such as `requires_model`, `requires_prompt`,
  `requires_base_field`, `field_names`, and `target_types`

### Plugin contract

All plugins implement:

- [includes/AIFieldAutomatorInterface.inc](/home/justink/Documents/GitHub/amafoundation-backdrop/modules/contrib/ai_field_automator/includes/AIFieldAutomatorInterface.inc)

The base helper class is:

- [includes/AIFieldAutomatorBase.inc](/home/justink/Documents/GitHub/amafoundation-backdrop/modules/contrib/ai_field_automator/includes/AIFieldAutomatorBase.inc)

Important methods:

- `allowedFieldTypes()`
- `allowedInputs()`
- `ruleIsAllowed()`
- `checkIfEmpty()`
- `postCheckIfEmpty()`
- `generate()`
- `verifyValue()`
- `storeValues()`

The `checkIfEmpty()` and `verifyValue()` hooks matter for partial-field cases,
for example:

- `text_with_summary` where body exists but summary is empty
- image fields where files exist but `alt` text is empty
- structured fields that contain partial JSON/config data

### Save-time runner

The main runtime is:

- [includes/AIFieldAutomatorRunner.inc](/home/justink/Documents/GitHub/amafoundation-backdrop/modules/contrib/ai_field_automator/includes/AIFieldAutomatorRunner.inc)

On save, the runner:

1. Loads enabled configs for the entity bundle
2. Sorts them by weight
3. Loads the plugin for each field
4. Checks whether the field should run
5. Calls `generate()`
6. Filters invalid output with `verifyValue()`
7. Stores values back on the entity

The runner also updates the synthetic status field through:

- [includes/AIFieldAutomatorStatusField.inc](/home/justink/Documents/GitHub/amafoundation-backdrop/modules/contrib/ai_field_automator/includes/AIFieldAutomatorStatusField.inc)

Status values:

- `pending`
- `processing`
- `failed`
- `finished`

### Worker modes

The module supports four worker modes:

- `direct`
- `batch`
- `queue_cron`
- `widget_action`

Relevant files:

- [ai_field_automator.module](/home/justink/Documents/GitHub/amafoundation-backdrop/modules/contrib/ai_field_automator/ai_field_automator.module)
- [includes/AIFieldAutomatorWidgetActions.inc](/home/justink/Documents/GitHub/amafoundation-backdrop/modules/contrib/ai_field_automator/includes/AIFieldAutomatorWidgetActions.inc)

If `ai_async` is available, widget actions for saved entities can dispatch
generation to a background async callback.

## Comparison to Drupal `ai_automators`

Compared against:

- `../modelaviation-d10/web/modules/contrib/ai/modules/ai_automators`

### Present here

- broad rule coverage across most practical field-generation cases
- field-level configuration UI
- ordering by weight
- worker-mode support
- widget-action support
- queue/cron processing

### Drupal-specific pieces not ported directly

These are upstream architecture differences, not missing Backdrop features:

- config entity architecture for automators/chains
- Symfony service container and event dispatcher integrations
- CKEditor plugin integration
- Drupal route/list-builder/admin entity layers
- attribute-based plugin discovery

Backdrop replaces those with procedural hooks, per-field config, form alters,
and direct runner classes.

### Parity gap found during review

The main functional gap I found was in save-time overwrite/skip decisions:

- the runner previously treated a field as "filled" if it had any stored data
- that caused false skips for summary-only generation, image ALT generation,
  and some partially structured field types

This was fixed by adding plugin-aware `checkIfEmpty()` and `verifyValue()`
support to the Backdrop plugin contract and runner.

Patched plugin types:

- `text_create_summary`
- `image_alt_text`
- `metatag`
- `chart_from_text`

## Extending the module

To add a new automator type:

1. Create a plugin file under `includes/plugins/...`
2. Add the class to `hook_autoload_info()`
3. Register the type in `hook_ai_field_automator_type_info()`
4. Implement generation, validation, and storage logic
5. Add any type-specific form controls in
   [includes/AIFieldAutomatorFieldForm.inc](/home/justink/Documents/GitHub/amafoundation-backdrop/modules/contrib/ai_field_automator/includes/AIFieldAutomatorFieldForm.inc)
