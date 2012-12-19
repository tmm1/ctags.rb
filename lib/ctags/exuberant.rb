require 'posix/spawn'

module Ctags
  class Error < StandardError
  end

  module Exuberant
    include POSIX::Spawn

    BIN = File.expand_path("../../../ext/dst/bin/ctags", __FILE__)

    def tags_for_code(file, code)
      tags_for_file(file, code)
    end

    def tags_for_file(file, code=nil)
      if code
        child = Child.new(BIN, '-f', '-', '--extra=+f', '--excmd=number', "--stdin-filename=#{file}", :input => code)
      else
        child = Child.new(BIN, '-f', '-', '--extra=+f', '--excmd=number', file)
      end

      if !child.status.success?
        raise Error, child.err
      end

      tags = {}

      child.out.each_line.map do |line|
        name, file, line, type, *rest = line.split("\t")
        tags[name] = {
          :file => file,
          :line => line.sub(';"','').to_i,
          :type => type.strip
        }
        rest.each do |part|
          key, value = part.strip.split(':', 2)
          tags[name][key.to_sym] = value
        end
      end

      tags
    end
  end
end
