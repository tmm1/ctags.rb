require 'benchmark'
require 'rubygems'
$:.unshift File.expand_path('../lib', __FILE__)
require 'ctags'

Benchmark.bmbm(30) do |b|
  [
    ['small', __FILE__],
    ['medium', Gem.find_files('mkmf.rb')[0]],
    ['big', Gem.find_files('tk.rb')[0]]
  ].each do |type, file|
    b.report("tags for #{type} file") do
      Ctags.tags_for_file(file)
    end

    b.report("tags for #{type} file x500") do
      500.times{ Ctags.tags_for_file(file) }
    end

    b.report("tags for #{type} blob") do
      Ctags.tags_for_file(file, File.read(file))
    end

    b.report("tags for #{type} blob x500") do
      500.times{ Ctags.tags_for_file(file, File.read(file)) }
    end
  end
end
