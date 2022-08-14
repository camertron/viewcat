# frozen_string_literal: true

require "rails"

module Vitesse
  class Engine < Rails::Engine
    config.vitesse = ActiveSupport::OrderedOptions.new(enabled: true)

    initializer "vitesse.load" do |app|
      options = app.config.vitesse

      ActiveSupport.on_load(:action_view) do
        Vitesse.load! if options.enabled
      end
    end
  end
end
