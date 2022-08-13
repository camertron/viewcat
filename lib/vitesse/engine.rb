# frozen_string_literal: true

require "rails"

module Vitesse
  class Engine < Rails::Engine
    config.vitesse = ActiveSupport::OrderedOptions.new

    initializer "vitesse.load" do |app|
      options = app.config.view_component

      ActiveSupport.on_load(:action_view) do
        require File.expand_path(File.join("..", "..", "ext", "vitesse", "vitesse"), __dir__)
        ActionView::OutputBuffer.prepend(Vitesse::OutputBufferPatch)
        ActionView::RawOutputBuffer.prepend(Vitesse::RawOutputBufferPatch)
      end
    end
  end
end
