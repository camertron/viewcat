# frozen_string_literal: true

require "bundler"
require "rake/extensiontask"

spec = Bundler.load_gemspec("viewcat.gemspec")

Rake::ExtensionTask.new("viewcat", spec) do |ext|
  ext.ext_dir = "ext/viewcat"
  ext.lib_dir = "lib/viewcat"
end

Bundler::GemHelper.install_tasks

task :bench do
  load File.join(*%w(script bench.rb))
end

task :prof do
  load File.join(*%w(script prof.rb))
end
