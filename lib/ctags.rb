require 'ctags/version'
require 'ctags/exuberant'
require 'ctags/ruby'

module Ctags
  extend self

  class Error < StandardError
  end

  def tags_for_file(file, code=nil)
    if file =~ /\.rb$/
      Ruby.tags_for_file(file, code)
    else
      Exuberant.tags_for_file(file, code)
    end
  end
end
