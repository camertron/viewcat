# frozen_string_literal: true

require "rails"

module Vitesse
  class Engine < Rails::Engine
    config.vitesse = ActiveSupport::OrderedOptions.new(enabled: false)

    initializer "vitesse.patch" do |app|
      options = app.config.vitesse

      ActiveSupport.on_load(:action_view) do
        if options.enabled
          ActionView::OriginalOutputBuffer ||= ActionView::OutputBuffer
          ActionView::OriginalRawOutputBuffer ||= ActionView::RawOutputBuffer

          ActionView::OutputBuffer = Vitesse::OutputBuffer
          ActionView::RawOutputBuffer = Vitesse::RawOutputBuffer
        end
      end
    end
  end
end
