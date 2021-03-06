require 'mkmf'
require 'fileutils'

def sys(cmd)
  puts "  -- #{cmd}"
  unless ret = xsystem(cmd)
    puts " ==> tail -15 #{CWD}/mkmf.log"
    puts File.readlines("#{CWD}/mkmf.log").last(15).join("")
    raise "#{cmd} failed"
  end
  ret
end

CWD = File.expand_path('../', __FILE__)
FileUtils.mkdir_p "#{CWD}/dst"
xsystem('pwd') # to create mkmf.log before the chdir

Dir.chdir("#{CWD}/vendor/jansson-2.9") do
  sys "./configure --prefix=#{CWD}/dst --disable-shared" unless File.exists?('config.h')
  sys "touch configure aclocal.m4 Makefile.am Makefile.in"
  sys "make install"
end

ENV['JANSSON_CFLAGS']  = "-ggdb -I#{CWD}/dst/include"
ENV['JANSSON_LIBS'] = "-L#{CWD}/dst/lib -ljansson"

Dir.chdir("#{CWD}/vendor/ctags") do
  sys "./configure --prefix=#{CWD}/dst" unless File.exists?('config.h')
  sys "touch configure aclocal.m4 Makefile.am Makefile.in"
  sys "make install"
end

File.open('Makefile', 'w') do |f|
  f.puts "install:\n\t\n"
end
