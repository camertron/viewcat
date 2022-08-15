# frozen_string_literal: true

require "bundler"
require "rake/extensiontask"

spec = Bundler.load_gemspec("vitesse.gemspec")

Rake::ExtensionTask.new("vitesse", spec) do |ext|
  ext.ext_dir = "ext/vitesse"
  ext.lib_dir = "lib/vitesse"
end

Bundler::GemHelper.install_tasks

task :bench do
  load "bench.rb"
end

task :prof do
  load "prof.rb"
end
