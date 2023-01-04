# frozen_string_literal: true

require "rails"

module Viewcat
  class Engine < Rails::Engine
    config.viewcat = ActiveSupport::OrderedOptions.new(enabled: false)

    initializer "viewcat.patch" do |app|
      options = app.config.viewcat

      ActiveSupport.on_load(:action_view) do
        if options.enabled
          require "action_view/buffers"

          ActionView::OriginalOutputBuffer ||= ActionView::OutputBuffer
          silence_warnings { ActionView::OutputBuffer = Viewcat::OutputBuffer }

          if ActionView.const_defined?(:RawOutputBuffer)
            ActionView::OriginalRawOutputBuffer ||= ActionView::RawOutputBuffer
            silence_warnings { ActionView::RawOutputBuffer = Viewcat::RawOutputBuffer }
          end
        end
      end
    end
  end
end
