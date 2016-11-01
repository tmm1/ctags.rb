require 'mkmf'
require 'fileutils'

def sys(cmd)
  puts "  -- #{cmd}"
  unless ret = xsystem(cmd)
    puts " ==> tail -10 #{CWD}/mkmf.log"
    puts File.readlines("#{CWD}/mkmf.log").last(10).join("")
    raise "#{cmd} failed"
  end
  ret
end

CWD = File.expand_path('../', __FILE__)
FileUtils.mkdir_p "#{CWD}/dst"
xsystem('pwd') # to create mkmf.log before the chdir

Dir.chdir("#{CWD}/vendor/jansson-2.9") do
  sys "./configure --prefix=#{CWD}/dst --disable-shared" unless File.exists?('config.h')
  sys "touch *"
  sys "make install"
end

ENV['CFLAGS']  = "-ggdb -I#{CWD}/dst/include"
ENV['LDFLAGS'] = "-L#{CWD}/dst/lib"
ENV['LIBS']    = "-ljansson"

if !have_func('fmemopen','stdio.h') && have_func('funopen','stdio.h')
  ENV['CFLAGS'] += " -DHAVE_FMEMOPEN_C"
  FileUtils.cp "#{CWD}/vendor/fmemopen/fmemopen.c", "#{CWD}/dst/include"
end

Dir.chdir("#{CWD}/vendor/exuberant-ctags") do
  sys "./configure --prefix=#{CWD}/dst" unless File.exists?('config.h')
  sys "make install"
end

File.open('Makefile', 'w') do |f|
  f.puts "install:\n\t\n"
end
