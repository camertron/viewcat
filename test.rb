# frozen_string_literal: true

require "vitesse"
require "active_support/core_ext/string/output_safety"

buffer = Vitesse::OutputBuffer.new
10.times do
  buffer.safe_append=("this is a <p>longer</p> string that should need a pointer")
  buffer.append=("this is another quite <p>long</p> string that should need its own memory")
end
puts buffer.to_str
