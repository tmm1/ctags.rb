require 'posix/spawn'
require 'yajl'
require 'tempfile'

module Ctags
  module Universal
    include POSIX::Spawn
    extend self

    BIN    = File.expand_path("../../../ext/dst/bin/ctags", __FILE__)
    CONFIG = File.expand_path("../../ctags.cnf", __FILE__)

    def start
      args = [
        '--options=NONE',
        "--options=#{CONFIG}",
        '--fields=*',
        '--interactive'
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

    def tags_for_file(filename, code=nil)
      cmd = {'command'=>'generate-tags','filename'=>filename}
      cmd['size'] = code.bytesize if code
      execute(cmd, code)
    end
  end
end
