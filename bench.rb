# frozen_string_literal: true

require "vitesse"
require "benchmark/ips"
require "active_support/core_ext/module/delegation"
require "action_view/buffers"

TIMES = 1000

GC.disable

Benchmark.ips do |x|
  x.report("actionview") do
    buffer = ActionView::OutputBuffer.new

    TIMES.times do
      buffer.safe_append=("this is a longer string that should need a pointer")
      buffer.append=("this is another <em>quite</em> long string that should need its own memory")
    end

    buffer.to_str
  end

  x.report("vitesse") do
    buffer = Vitesse::OutputBuffer.new

    TIMES.times do
      buffer.safe_append=("this is a longer string that should need a pointer")
      buffer.append=("this is another <em>quite</em> long string that should need its own memory")
    end

    buffer.to_str
  end

  x.compare!
end
