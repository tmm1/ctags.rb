## ctags.rb

simple ruby wrapper for exuberant-ctags

### usage

``` ruby
>> pp Ctags.tags_for_file('/usr/lib/ruby/1.8/timeout.rb')
[
 {:path=>
   "/usr/lib/ruby/1.8/timeout.rb",
  :kind=>"module",
  :language=>"Ruby",
  :pattern=>"module Timeout",
  :name=>"Timeout",
  :line=>30},
 {:path=>
   "/usr/lib/ruby/1.8/timeout.rb",
  :kind=>"class",
  :class=>"Timeout",
  :language=>"Ruby",
  :pattern=>"class Error < Interrupt",
  :name=>"Error",
  :line=>35},
 {:path=>
   "/usr/lib/ruby/1.8/timeout.rb",
  :kind=>"class",
  :class=>"Timeout",
  :language=>"Ruby",
  :pattern=>"class ExitException < ::Exception # :nodoc:",
  :name=>"ExitException",
  :line=>37},
 {:path=>
   "/usr/lib/ruby/1.8/timeout.rb",
  :kind=>"method",
  :class=>"Timeout",
  :language=>"Ruby",
  :pattern=>"def timeout(sec, klass = nil)",
  :name=>"timeout",
  :line=>52},
 {:path=>
   "/usr/lib/ruby/1.8/timeout.rb",
  :kind=>"method",
  :language=>"Ruby",
  :pattern=>"def timeout(n, e = nil, &block) # :nodoc:",
  :name=>"timeout",
  :line=>100}
]
```
