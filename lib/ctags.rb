require 'ctags/version'
require 'ctags/universal'
require 'ctags/ruby'

module Ctags
  extend self

  class Error < StandardError
  end

  def tags_for_file(file, code=nil)
    if file =~ /\.rb$/
      Ruby.tags_for_file(file, code)
    else
      Universal.tags_for_file(file, code)
    end
  end
end
