## ctags.rb

simple ruby wrapper for [universal-ctags](https://github.com/universal-ctags/ctags) and [ripper-tags](https://github.com/tmm1/ripper-tags)

### usage

``` ruby
>> pp Ctags.tags_for_file('/usr/lib/ruby/1.8/timeout.rb')
[
 {:path=> "/usr/lib/ruby/1.8/timeout.rb", :kind=>"module", :language=>"Ruby", :pattern=>"module Timeout", :name=>"Timeout", :line=>30},
 {:path=> "/usr/lib/ruby/1.8/timeout.rb", :kind=>"class", :class=>"Timeout", :language=>"Ruby", :pattern=>"class Error < Interrupt", :name=>"Error", :line=>35},
 {:path=> "/usr/lib/ruby/1.8/timeout.rb", :kind=>"method", :class=>"Timeout", :language=>"Ruby", :pattern=>"def timeout(sec, klass = nil)", :name=>"timeout", :line=>52},
]
```

### architecture

This ctags.rb gem includes the sources of my fork of [universal-ctags](https://github.com/tmm1/ctags). The source is compiled into a `ctags` binary when the gem is installed, and the binary is invoked as an external program by this gem to generate code tags.

The changes in my fork include https://github.com/universal-ctags/ctags/pull/1071, which adds an interactive mode. In this mode, a single long-running instance of `ctags` can be used to generate tags for various inputs by sending it json commands over a pipe. Code can also be streamed into `ctags` over stdin, for cases where it is fetched over a network and not readily available on disk. This parent-child setup was chosen for several reasons:

- the ctags codebase was written as a unix command line utility, which makes it very hard to link directly and use as a library (error handling generally causes an `exit(1)` and leaves global variables in a dirty state)
- the language parsers in ctags could contain security vulnerabilities, so running them in a separate process provides for better sandboxing opportunities
- on some platforms (like win32), process bootup cost is very expensive. using a single long-running ctags invocation eliminates this overhead.

### license

ctags.rb is licensed under the MIT license.

universal-ctags is licensed under the GPLv2 license.
