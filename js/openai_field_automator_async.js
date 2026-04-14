/**
 * @file
 * Polls openai_async job status for widget-action buttons and injects
 * generated values into the field widget when the job completes.
 *
 * Requires openai_async.js (OpenAIAsync.poll) to be loaded first.
 */

(function ($) {
  'use strict';

  Backdrop.behaviors.openaiFieldAutomatorAsync = {
    attach: function (context, settings) {
      // Find every Generate button that has an in-flight async job attached.
      $('[data-openai-async-job-id]', context).once('openai-fa-async', function () {
        var $button = $(this);
        var jobId     = $button.attr('data-openai-async-job-id');
        var wrapperId = $button.attr('data-openai-async-wrapper-id');
        var fieldName = $button.attr('data-openai-async-field-name');
        var fieldType = $button.attr('data-openai-async-field-type');
        var delta     = parseInt($button.attr('data-openai-async-delta') || '0', 10);

        if (!jobId) {
          return;
        }

        var statusUrl = (settings.openai_field_automator_async && settings.openai_field_automator_async.statusUrl)
          ? settings.openai_field_automator_async.statusUrl
          : '/openai/async/status/';

        OpenAIAsync.poll(jobId, {
          interval: 2000,
          maxWait:  180000,

          onComplete: function (result) {
            _openaiFieldAutomatorApplyResult($button, wrapperId, fieldName, fieldType, delta, result);
          },

          onError: function (message) {
            _openaiFieldAutomatorRestoreButton($button);
            // Surface the error as a Backdrop message so the user sees it.
            var $msg = $('<div class="messages error">' + Backdrop.checkPlain(message) + '</div>');
            $('#' + wrapperId).before($msg);
          },

          onTimeout: function () {
            _openaiFieldAutomatorRestoreButton($button);
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
  function _openaiFieldAutomatorApplyResult($button, wrapperId, fieldName, fieldType, delta, result) {
    var $wrapper = $('#' + wrapperId);

    if (!result || !result.values) {
      _openaiFieldAutomatorRestoreButton($button);
      return;
    }

    var values = result.values;

    if (fieldType === 'taxonomy_term_reference') {
      // Taxonomy autocomplete: result.values is a comma-separated term string
      // (the module returns the raw text stored by storeValues for autocomplete).
      var text = '';
      if (typeof values === 'string') {
        text = values;
      }
      else if ($.isArray(values) && values.length && typeof values[0] === 'object') {
        // Array of {name: '...'} objects — join as "Tag1, Tag2"
        var names = [];
        $.each(values, function (i, item) {
          if (item.name) {
            names.push(item.name);
          }
        });
        text = names.join(', ');
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

    _openaiFieldAutomatorRestoreButton($button);
  }

  /**
   * Removes the spinner, re-enables the button, and resets its label.
   */
  function _openaiFieldAutomatorRestoreButton($button) {
    $button
      .removeAttr('disabled')
      .val(Backdrop.t('Generate with AI'));
    $button.siblings('.openai-field-automator-spinner').remove();
    // Clear the job-id attribute so the behavior won't re-attach on next
    // AJAX rebuild.
    $button.removeAttr('data-openai-async-job-id');
  }

}(jQuery));
