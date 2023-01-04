# frozen_string_literal: true

require "viewcat"
require "benchmark/ips"
require "active_support/core_ext/module/delegation"
require "action_view/buffers"

TIMES = 1000

GC.disable

Benchmark.ips do |x|
  x.report("actionview") do
    buffer = ActionView::OutputBuffer.new

    TIMES.times do
      buffer.append="<p>foo</p> baz "
      buffer.safe_append="bar"
    end

    buffer.to_str
  end

  x.report("viewcat") do
    buffer = Viewcat::OutputBuffer.new

    TIMES.times do
      buffer.append="<p>foo</p> baz "
      buffer.safe_append="bar"
    end

    buffer.to_str
  end

  x.compare!
end
