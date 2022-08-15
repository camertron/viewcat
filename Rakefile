# frozen_string_literal: true

require "rake/extensiontask"

Rake::ExtensionTask.new "vitesse" do |ext|
  ext.lib_dir = "lib/vitesse"
end

task :bench do
  load "bench.rb"
end

task :prof do
  load "prof.rb"
end
