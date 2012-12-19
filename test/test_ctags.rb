#coding: utf-8

require 'test/unit'
require File.expand_path('../../lib/ctags.rb', __FILE__)

class CtagsTest < Test::Unit::TestCase
  def test_tags_for_file
    tags = Ctags.tags_for_file(__FILE__)
    tag = tags[__method__.to_s]

    assert_equal __FILE__,    tag[:file]
    assert_equal __LINE__-5,  tag[:line]
    assert_equal 'CtagsTest', tag[:class]
  end

  def test_tags_for_code
    tags = Ctags.tags_for_code('file.rb', File.read(__FILE__))
    tag = tags[__method__.to_s]

    assert_equal 'file.rb',   tag[:file]
    assert_equal __LINE__-5,  tag[:line]
    assert_equal 'CtagsTest', tag[:class]
  end
end
