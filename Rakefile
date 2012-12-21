require "bundler/gem_tasks"

task :default => :test

# ==========================================================
# Packaging
# ==========================================================

GEMSPEC = Gem::Specification.load('ctags.rb.gemspec')

require 'rubygems/package_task'
Gem::PackageTask.new(GEMSPEC) do |pkg|
end

# ==========================================================
# Compiling
# ==========================================================

task :build do
  Dir.chdir('ext') do
    ruby 'extconf.rb'
  end
end

# ==========================================================
# Testing
# ==========================================================

require 'rake/testtask'
Rake::TestTask.new 'test' do |t|
  t.test_files = FileList['test/test_*.rb']
  t.ruby_opts = ['-rubygems']
end
task :test => :build
