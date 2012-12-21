require 'benchmark'
require 'rubygems'
$:.unshift File.expand_path('../lib', __FILE__)
require 'ctags'

Benchmark.bmbm(20) do |b|
  b.report('tags for file') do
    Ctags.tags_for_file(__FILE__)
  end

  b.report('tags for file x1000') do
    1000.times{ Ctags.tags_for_file(__FILE__) }
  end
end
