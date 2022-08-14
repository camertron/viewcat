# frozen_string_literal: true

module Vitesse
  def self.load!
    require "action_view/buffers"
    require File.expand_path(File.join("..", "ext", "vitesse", "vitesse"), __dir__)
    ActionView::OutputBuffer.prepend(Vitesse::OutputBufferPatch)
    ActionView::RawOutputBuffer.prepend(Vitesse::RawOutputBufferPatch)
  end
end

if defined?(Rails::Engine)
  require "vitesse/engine"
end
