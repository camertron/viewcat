require "vitesse"
require "ruby-prof"
require "active_support/core_ext/module/delegation"
require "action_view/buffers"

# profile the code
result = RubyProf.profile do
  10_000.times do
    buffer = Vitesse::OutputBuffer.new
    buffer.safe_append=("this is a longer string that should need a pointer")
    buffer.append=("this is another quite long string that should need its own memory")
    buffer.to_str
  end
end

# print a graph profile to text
printer = RubyProf::GraphPrinter.new(result)
printer.print(STDOUT, {})
