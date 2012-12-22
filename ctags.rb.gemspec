require File.expand_path('../lib/ctags/version', __FILE__)

Gem::Specification.new do |s|
  s.name = 'ctags.rb'
  s.version = Ctags::VERSION

  s.summary = 'ctags wrapper for ruby'
  s.description = 'ctags.rb exposes exuberant-ctags to Ruby'

  s.homepage = 'http://github.com/tmm1/ctags.rb'
  s.has_rdoc = false

  s.authors = ['Aman Gupta']
  s.email = ['aman@tmm1.net']

  s.add_dependency 'posix-spawn', '~> 0.3.6'
  s.add_dependency 'yajl-ruby'

  s.extensions = ['ext/extconf.rb']
  s.require_paths = ['lib']

  s.files = `git ls-files`.split("\n")
end
