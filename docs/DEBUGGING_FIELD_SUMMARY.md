# Debugging the Post Summary Automator

This note traces the active `post.field_summary` automator from field config to
prompt construction, provider routing, and useful Xdebug breakpoints.

## Active Config

The relevant config object is:

- `ai_field_automator.node.post.field_summary`
- `config_3925413841b7cdf12a96360339e59b6d/active/ai_field_automator.node.post.field_summary.json`

Important values:

```json
{
  "entity_type": "node",
  "bundle": "post",
  "field_name": "field_summary",
  "automator_type": "long_text",
  "mode": "base",
  "base_field": "_rendered_entity",
  "prompt": "Create a plain text summary of 300 characters or less based on the {context}. Do not include hashtags.",
  "model": "browser/chrome",
  "edit_mode": "skip",
  "worker_type": "widget_action"
}
```

This means:

- The automator targets the `field_summary` field on `post` nodes.
- It uses the `long_text` automator plugin.
- It builds context from the rendered node, not from one specific source field.
- It sends the final prompt to the `browser/chrome` model.
- It runs from the `Generate with AI` widget button.

## Widget Action Flow

Because `worker_type` is `widget_action`, this automator does not run in the
normal save-time runner path. Instead, the module adds a button to supported
field widgets.

Button injection starts at:

```php
ai_field_automator_field_widget_form_alter()
```

That wrapper loads:

```php
includes/AIFieldAutomatorWidgetActions.inc
```

and calls:

```php
_ai_field_automator_field_widget_form_alter()
```

The button submit callback is:

```php
ai_field_automator_widget_action_submit()
_ai_field_automator_widget_action_submit()
```

The submit handler:

1. Reads the triggering field metadata from `#ai_field_automator`.
2. Loads the field config with `ai_field_automator_load_field_config()`.
3. Resolves `long_text` with `ai_field_automator_get_types()`.
4. Instantiates the plugin with `ai_field_automator_load_plugin()`.
5. Builds an entity clone from the current form values.
6. Calls the plugin's `generate()` method.
7. Stores generated values back into form state for the AJAX rebuild.

For this automator, the plugin class is:

```php
ai_field_automator\includes\plugins\text\AIFieldAutomatorLongText
```

registered in `ai_field_automator_ai_field_automator_type_info()`.

## Async Caveat

If the entity has an ID and `ai_async` is enabled, the widget action does not
run generation directly in the AJAX request. It submits a background job instead:

```php
ai_async_submit('ai_field_automator_async_generate', ...)
```

That callback is registered by:

```php
ai_field_automator_ai_async_callbacks()
```

and implemented as:

```php
ai_field_automator_async_generate(array $data)
```

The async callback loads the saved entity, loads the same plugin, and then calls:

```php
$plugin->generate($entity, $field_name, $config);
```

For Xdebug, this means breakpoints in `AIFieldAutomatorLongText::generate()`
may not fire in the original browser/AJAX request while `ai_async` is on.
Disable `ai_async` or attach Xdebug to the async PHP process to step through
generation directly.

## Prompt Construction

`AIFieldAutomatorBase::callAI()` does not build the prompt. It receives an
already-built `$prompt`.

The prompt is built by this call chain:

```php
AIFieldAutomatorLongText::generate()
  $prompts = $this->buildPrompts($entity, $config);

AIFieldAutomatorBase::buildPrompts()
  $context = $this->extractEntityContextText($entity, $entity_type);
  $prompt = $this->buildPrompt($entity, $config, $context);

AIFieldAutomatorBase::buildPrompt()
  $prompt = $config['prompt'];
  str_replace('{context}', $context_value, $prompt);
```

For this config, `base_field` is `_rendered_entity`, so `buildPrompts()` uses:

```php
AIFieldAutomatorBase::extractEntityContextText()
```

That method now delegates the actual extraction to the shared AI helper:

```php
AIContentExtractor::extractEntityText()
```

For saved entities, that method prefers rendered entity text:

```php
AIContentExtractor::renderEntityToMarkdown()
  entity_view($entity_type, [$entity_id => $entity], 'full');
  backdrop_render($build);
  AIContentExtractor::htmlToMarkdown($html);
```

For unsaved widget-action form changes, the entity clone may have:

```php
$entity->ai_field_automator_preview_text
```

That preview text is populated by:

```php
_ai_field_automator_widget_action_build_entity()
ai_field_automator_extract_node_preview_text_from_form_state()
AIContentExtractor::extractNodeFormValuesText()
```

The final prompt sent to the model is the config prompt with `{context}` replaced
by rendered or preview text. It will look roughly like:

```text
Create a plain text summary of 300 characters or less based on the [rendered node text]. Do not include hashtags.
```

## AI Call and Provider Routing

After the prompt is built, `AIFieldAutomatorLongText::generate()` calls:

```php
$text = $this->callAI($prompt, $config);
```

`AIFieldAutomatorBase::callAI()` builds AI-standard chat messages:

```php
$messages = [
  ['role' => 'user', 'content' => $prompt],
];
```

Then it calls:

```php
ai_chat(
  $model,
  $messages,
  $temperature,
  $max_tokens,
  FALSE,
  ['operation' => 'field_generation']
);
```

For `browser/chrome`, `ai_chat()` calls:

```php
ai_get_api_for_model('browser/chrome', $stripped_model);
```

That splits the model ID into:

```php
$provider = 'browser';
$stripped_model = 'chrome';
```

The `browser` provider is registered by:

```php
ai_provider_browser_ai_provider_info()
```

which points to:

```php
BrowserAdapter
```

The provider call path is:

```php
ai_chat()
AIApi::chat('chrome', ...)
BrowserAdapter::chat('chrome', ...)
BrowserAdapter::waitForResult($uuid, ...)
```

`BrowserAdapter::chat()` stores a browser request in the PHP session:

```php
$_SESSION['ai_provider_browser_requests'][$uuid] = $request;
```

Then it releases the session lock and waits for browser-side JavaScript to pick
up the request and submit the result. The result is read from cache/session by:

```php
BrowserAdapter::waitForResult()
```

The Drupal-style file below is present in the repo but is not the active Backdrop
provider path for this automator:

```text
modules/custom/ai_provider_browser/src/Plugin/AiProvider/BrowserProvider.php
```

This Backdrop flow uses:

```text
modules/contrib/ai_provider_browser/includes/BrowserAdapter.php
```

## Useful Breakpoints

Start here to confirm the widget button path:

```php
_ai_field_automator_field_widget_form_alter()
_ai_field_automator_widget_action_submit()
```

If async is enabled, also break here:

```php
ai_field_automator_async_generate()
```

To inspect prompt construction:

```php
AIFieldAutomatorLongText::generate()
AIFieldAutomatorBase::buildPrompts()
AIFieldAutomatorBase::buildPrompt()
AIFieldAutomatorBase::extractEntityContextText()
AIContentExtractor::extractEntityText()
AIContentExtractor::renderEntityToMarkdown()
```

The best single place to inspect the final prompt before the provider call is in
`AIFieldAutomatorLongText::generate()` immediately after:

```php
$prompts = $this->buildPrompts($entity, $config);
```

or in `AIFieldAutomatorBase::buildPrompt()` immediately before:

```php
return $prompt;
```

To inspect the chat request and provider routing:

```php
AIFieldAutomatorBase::callAI()
ai_chat()
ai_get_api_for_model()
AIApi::chat()
BrowserAdapter::chat()
BrowserAdapter::waitForResult()
```

## File Map

- `config_3925413841b7cdf12a96360339e59b6d/active/ai_field_automator.node.post.field_summary.json`
- `modules/contrib/ai_field_automator/ai_field_automator.module`
- `modules/contrib/ai_field_automator/includes/AIFieldAutomatorWidgetActions.inc`
- `modules/contrib/ai_field_automator/includes/plugins/text/LongText.inc`
- `modules/contrib/ai_field_automator/includes/AIFieldAutomatorBase.inc`
- `modules/contrib/ai/ai.module`
- `modules/contrib/ai/includes/AIContentExtractor.php`
- `modules/contrib/ai/includes/AIApi.php`
- `modules/contrib/ai_provider_browser/ai_provider_browser.module`
- `modules/contrib/ai_provider_browser/includes/BrowserAdapter.php`
