# frozen_string_literal: true

lib = File.expand_path("../lib", __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require "viewcat/version"

Gem::Specification.new do |spec|
  spec.name = "viewcat"
  spec.version = Viewcat::VERSION
  spec.authors = ["Cameron C. Dutro"]

  spec.summary = "A faster ActionView::OutputBuffer written in C."
  spec.homepage = "https://github.com/camertron/viewcat"
  spec.license = "MIT"

  spec.files = Dir["LICENSE.txt", "README.md", "{ext,lib,spec}/**/*"]
  spec.require_paths = ["lib"]

  spec.required_ruby_version = ">= 2.7.0"
  spec.extensions = %w[ext/viewcat/extconf.rb]

  spec.add_runtime_dependency "actionview" # , "~> 7.0.0"
  spec.add_runtime_dependency "railties" # , "~> 7.0"
end
