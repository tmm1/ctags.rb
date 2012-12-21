require 'posix/spawn'
require 'tempfile'

module Ctags
  class Error < StandardError
  end

  module Exuberant
    include POSIX::Spawn

    BIN = File.expand_path("../../../ext/dst/bin/ctags", __FILE__)

    def tags_for_code(filename, code)
      tags_for_file(filename, code)
    end

    def tags_for_file(filename, code=nil)
      args = [
        '-o', '-',
        '--fields=+KlnzsStimfa',
        '--sort=no',
        '--excmd=pattern'
      ]

      child =
        if code
          # XXX stdin is not seekable
          # args << "--stdin-filename=#{filename}"
          # args << {:input => code}

          tempfile = Tempfile.new(['ctags-input', File.extname(filename)])
          tempfile.write(code)
          tempfile.close
          args << tempfile.path

          begin
            Child.new(BIN, *args)
          ensure
            tempfile.unlink
          end
        else
          args << filename
          Child.new(BIN, *args)
        end

      if !child.status.success?
        raise Error, child.err
      end

      tags = {}

      child.out.each_line.map do |line|
        tag, file, rest = line.strip.split("\t", 3)
        pattern, fields = rest.split("/;\"\t", 2)

        tag = tags[tag] = {
          :filename => code ? filename : file,
          :pattern => pattern.sub('/^','').chomp('$').gsub('\\\\','\\')
        }

        fields.split("\t").each do |field|
          if field == 'file:'
            key, value = :scope, 'file'
          else
            key, value = field.split(":", 2)
          end

          tag[key.to_sym] = value
        end

        tag[:line] = tag[:line].to_i
      end

      tags
    end
  end
end
