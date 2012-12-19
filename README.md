## ctags.rb

simple ruby wrapper for exuberant-ctags

### usage

``` ruby
>> pp Ctags.tags_for_file('/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr/lib/ruby/1.8/timeout.rb')
{"Timeout"=>
  {:kind=>"module",
   :pattern=>"module Timeout",
   :language=>"Ruby",
   :line=>30,
   :filename=>
    "/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr/lib/ruby/1.8/timeout.rb"},
 "ExitException"=>
  {:class=>"Timeout",
   :kind=>"class",
   :pattern=>"  class ExitException < ::Exception # :nodoc:",
   :language=>"Ruby",
   :line=>37,
   :filename=>
    "/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr/lib/ruby/1.8/timeout.rb"},
 "Error"=>
  {:class=>"Timeout",
   :kind=>"class",
   :pattern=>"  class Error < Interrupt",
   :language=>"Ruby",
   :line=>35,
   :filename=>
    "/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr/lib/ruby/1.8/timeout.rb"},
 "timeout"=>
  {:kind=>"method",
   :pattern=>"def timeout(n, e = nil, &block) # :nodoc:",
   :language=>"Ruby",
   :line=>100,
   :filename=>
    "/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr/lib/ruby/1.8/timeout.rb"}}
```
