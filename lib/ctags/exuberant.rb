require 'posix/spawn'
require 'yajl'
require 'tempfile'

module Ctags
  class Error < StandardError
  end

  module Exuberant
    include POSIX::Spawn

    BIN    = File.expand_path("../../../ext/dst/bin/ctags", __FILE__)
    CONFIG = File.expand_path("../../ctags.cnf", __FILE__)

    def start
      args = [
        '--options=NONE',
        "--options=#{CONFIG}",
        '--fields=+KlnzsStimfa',
        '--sort=no',
        '--json'
      ]

      @pid, @in, @out, @err = popen4(BIN, *args)
      at_exit{ stop }
      @info = Yajl.load(@out.gets, :symbolize_keys => true)
    end

    def stop
      if @pid
        begin
          Process.kill('KILL', @pid)
          Process.waitpid(@pid)
        rescue Errno::ESRCH, Errno::ECHILD
        end
      end
      @pid = nil
    end

    def alive?
      return true if @pid && Process.kill(0, @pid)
      false
    rescue Errno::ENOENT, Errno::ESRCH
      false
    rescue Errno::EPERM
      raise "EPERM checking if child process is alive."
    end

    def execute(command, data=nil)
      start unless alive?

      json = Yajl.dump(command)
      @in.puts json
      @in.write data if data

      warnings = []
      tags = []

      while line = @out.gets
        obj = Yajl.load(line, :symbolize_keys => true)
        if obj[:error] && obj[:fatal]
          raise obj[:error]
        elsif obj[:error]
          warnings << obj
        elsif obj[:_type] == 'tag'
          obj.delete :_type
          obj[:pattern].strip!
          tags << obj
        elsif obj[:completed]
          break
        else
          raise "unknown obj: #{obj.inspect}"
        end
      end

      if tags.empty? and warnings.any?
        raise Error, warnings[0][:error]
      end

      tags
    end

    def tags_for_code(filename, code)
      tags_for_file(filename, code)
    end

    def tags_for_file(filename, code=nil)
      cmd = {'command'=>'generate-tags','filename'=>filename}
      cmd['size'] = code.bytesize if code
      execute(cmd, code)
    end

    def tags_for_file_old(filename, code=nil)
      args = [
        '--options=NONE',
        '--fields=+KlnzsStimfa',
        '--sort=no',
        '--excmd=pattern',
        '-o', '-'
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

      tags = []

      child.out.each_line.map do |line|
        name, path, rest = line.strip.split("\t", 3)
        pattern, fields = rest.split("/;\"\t", 2)

        tag = {
          :name => name,
          :path => code ? filename : path,
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

        tags << tag
      end

      tags
    end
  end
end
