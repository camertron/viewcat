require_relative "ext/vitesse/vitesse"
require "benchmark/ips"

Benchmark.ips do |x|
  x.report do
    buffer = OutputBuffer.new
    buffer.safe_append("foo")
    buffer.safe_append("bar")

    buffer.to_str
  end
end
