require 'minitest/autorun'
$:.unshift File.expand_path('../../lib', __FILE__)
require 'ctags'

class CtagsTest < MiniTest::Test
  def test_tags_for_ruby_file
    tags = Ctags.tags_for_file(__FILE__)
    tag = tags.find{ |t| t[:name] == __method__.to_s }

    assert_equal __FILE__,    tag[:path]
    assert_equal __LINE__-5,  tag[:line]
    assert_equal 'CtagsTest', tag[:class]
    assert_equal 'method',    tag[:kind]
  end

  def test_tags_for_ruby_file_with_code
    tags = Ctags.tags_for_file('file.rb', File.read(__FILE__))
    tag = tags.find{ |t| t[:name] == __method__.to_s }

    assert_equal 'file.rb',   tag[:path]
    assert_equal __LINE__-5,  tag[:line]
    assert_equal 'CtagsTest', tag[:class]
    assert_equal 'method',    tag[:kind]
  end

  def test_invalid_ruby_file
    assert_raises Ctags::Error do
      Ctags.tags_for_file('invalid.rb')
    end
  end

  def test_tags_for_file
    tags = Ctags.tags_for_file(File.expand_path("../test.c", __FILE__))
    assert_equal 2, tags.size
    assert_equal %w[ hello_world main ], tags.map{ |t| t[:name] }
  end

  def test_tags_for_file_with_code
    tags = Ctags.tags_for_file("test.c", IO.binread(File.expand_path("../test.c", __FILE__)))
    assert_equal 2, tags.size
    assert_equal %w[ hello_world main ], tags.map{ |t| t[:name] }
  end

  def test_invalid_file
    assert_raises Ctags::Error do
      Ctags.tags_for_file('invalid.c')
    end
  end
end
