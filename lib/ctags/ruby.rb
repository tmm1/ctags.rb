require 'ripper-tags/parser'

module Ctags
  module Ruby
    extend self

    def tags_for_file(file, data=nil)
      data ||= File.read(file)
      RipperTags::Parser.extract(data, file)
    rescue Errno::ENOENT
      raise Error, $!.message
    rescue Interrupt
      raise
    rescue Object => e
      STDERR.puts [e, file].inspect
      []
    end
  end
end
