# frozen_string_literal: true

require "rails"

module Vitesse
  class Engine < Rails::Engine
    config.vitesse = ActiveSupport::OrderedOptions.new(enabled: false)

    initializer "vitesse.patch" do |app|
      options = app.config.vitesse

      ActiveSupport.on_load(:action_view) do
        if options.enabled
          require "action_view/buffers"

          ActionView::OriginalOutputBuffer ||= ActionView::OutputBuffer
          silence_warnings { ActionView::OutputBuffer = Vitesse::OutputBuffer }

          if ActionView.const_defined?(:RawOutputBuffer)
            ActionView::OriginalRawOutputBuffer ||= ActionView::RawOutputBuffer
            silence_warnings { ActionView::RawOutputBuffer = Vitesse::RawOutputBuffer }
          end
        end
      end
    end
  end
end
