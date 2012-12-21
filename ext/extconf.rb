require 'mkmf'
require 'fileutils'

HERE = File.expand_path('../', __FILE__)
FileUtils.mkdir_p "#{HERE}/dst"

Dir.chdir("#{HERE}/vendor/exuberant-ctags") do
  system "./configure --prefix=#{HERE}/dst" unless File.exists?('config.h')
  system "make install"
end

File.open('Makefile', 'w') do |f|
  f.puts "install:\n\t\n"
end
