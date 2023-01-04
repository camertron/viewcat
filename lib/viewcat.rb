# frozen_string_literal: true

# load native extension
require "viewcat/viewcat"

if defined?(Rails::Engine)
  require "viewcat/engine"
end
