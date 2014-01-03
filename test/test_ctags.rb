require 'test/unit'
$:.unshift File.expand_path('../../lib', __FILE__)
require 'ctags'

class CtagsTest < Test::Unit::TestCase
  def test_tags_for_file
    tags = Ctags.tags_for_file(__FILE__)
    tag = tags.find{ |t| t[:name] == __method__.to_s }

    assert_equal __FILE__,    tag[:path]
    assert_equal __LINE__-5,  tag[:line]
    assert_equal 'CtagsTest', tag[:class]
    assert_equal 'method',    tag[:kind]
  end

  def test_tags_for_file_with_code
    tags = Ctags.tags_for_file('file.rb', File.read(__FILE__))
    tag = tags.find{ |t| t[:name] == __method__.to_s }

    assert_equal 'file.rb',   tag[:path]
    assert_equal __LINE__-5,  tag[:line]
    assert_equal 'CtagsTest', tag[:class]
    assert_equal 'method',    tag[:kind]
  end

  def test_invalid_file
    assert_raises Ctags::Error do
      Ctags.tags_for_file('invalid.rb')
    end
  end
end
