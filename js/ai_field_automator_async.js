/**
 * @file
 * Polls ai_async job status for widget-action buttons and injects
 * generated values into the field widget when the job completes.
 *
 * Requires ai_async.js (AiAsync.poll) to be loaded first.
 */

(function ($) {
  'use strict';

  Backdrop.behaviors.aiFieldAutomatorAsync = {
    attach: function (context, settings) {
      // Find every Generate button that has an in-flight async job attached.
      $('[data-ai-async-job-id]', context).once('ai-fa-async', function () {
        var $button = $(this);
        var jobId     = $button.attr('data-ai-async-job-id');
        var wrapperId = $button.attr('data-ai-async-wrapper-id');
        var fieldName = $button.attr('data-ai-async-field-name');
        var fieldType = $button.attr('data-ai-async-field-type');
        var delta     = parseInt($button.attr('data-ai-async-delta') || '0', 10);

        if (!jobId) {
          return;
        }

        var statusUrl = (settings.ai_field_automator_async && settings.ai_field_automator_async.statusUrl)
          ? settings.ai_field_automator_async.statusUrl
          : '/ai/async/status/';

        AiAsync.poll(jobId, {
          interval: 2000,
          maxWait:  180000,

          onComplete: function (result) {
            _aiFieldAutomatorApplyResult($button, wrapperId, fieldName, fieldType, delta, result);
          },

          onError: function (message) {
            _aiFieldAutomatorRestoreButton($button);
            // Surface the error as a Backdrop message so the user sees it.
            var $msg = $('<div class="messages error">' + Backdrop.checkPlain(message) + '</div>');
            $('#' + wrapperId).before($msg);
          },

          onTimeout: function () {
            _aiFieldAutomatorRestoreButton($button);
            var $msg = $('<div class="messages warning">' + Backdrop.t('AI generation timed out. Please try again.') + '</div>');
            $('#' + wrapperId).before($msg);
          }
        });
      });
    }
  };

  /**
   * Injects the async result into the field widget and re-enables the button.
   */
  function _aiFieldAutomatorApplyResult($button, wrapperId, fieldName, fieldType, delta, result) {
    var $wrapper = $('#' + wrapperId);
    var payload = result || {};
    if (payload.result && typeof payload.result === 'object') {
      payload = payload.result;
    }

    var values = payload.values;
    var generatedItems = $.isArray(payload.generated_items) ? payload.generated_items : [];

    if (fieldType === 'taxonomy_term_reference') {
      // Taxonomy autocomplete expects a comma-separated term string.
      var text = '';
      if (typeof values === 'string') {
        text = values;
      }
      else if ($.isArray(values) && values.length && typeof values[0] === 'object') {
        var names = [];
        $.each(values, function (i, item) {
          if (item.name) {
            names.push(item.name);
          }
        });
        text = names.join(', ');
      }
      else if (generatedItems.length) {
        var itemNames = [];
        $.each(generatedItems, function (i, item) {
          if (item.name) {
            itemNames.push(item.name);
          }
        });
        text = itemNames.join(', ');
      }
      $wrapper.find('input[type="text"]').val(text).trigger('change');
    }
    else {
      // Standard text fields: inject the first matching delta value.
      var item = null;
      if ($.isArray(values)) {
        item = values[delta] || values[0] || null;
      }
      else if (typeof values === 'object') {
        item = values[delta] || values[0] || null;
      }

      if (item) {
        var val = (typeof item === 'object') ? (item.value || item.safe_value || '') : item;
        // Target the main textarea or text input inside the wrapper.
        var $textarea = $wrapper.find('textarea').first();
        var $input    = $wrapper.find('input[type="text"]').first();
        if ($textarea.length) {
          $textarea.val(val).trigger('change');
        }
        else if ($input.length) {
          $input.val(val).trigger('change');
        }
      }
    }

    _aiFieldAutomatorRestoreButton($button);
  }

  /**
   * Removes the spinner, re-enables the button, and resets its label.
   */
  function _aiFieldAutomatorRestoreButton($button) {
    $button
      .removeAttr('disabled')
      .val(Backdrop.t('Generate with AI'));
    $button.siblings('.ai-field-automator-spinner').remove();
    // Clear the job-id attribute so the behavior won't re-attach on next
    // AJAX rebuild.
    $button.removeAttr('data-ai-async-job-id');
  }

}(jQuery));
