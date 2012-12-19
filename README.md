## ctags.rb

simple ruby wrapper for exuberant-ctags

### usage

``` ruby
>> pp Ctags.tags_for_file(`gem which timeout`.strip)
{"Timeout"=>
  {:type=>"m",
   :file=>"/Users/test/.rbenv/versions/1.8.7-p358/lib/ruby/1.8/timeout.rb",
   :line=>30},
 "ExitException"=>
  {:type=>"c",
   :class=>"Timeout",
   :file=>"/Users/test/.rbenv/versions/1.8.7-p358/lib/ruby/1.8/timeout.rb",
   :line=>37},
 "timeout"=>
  {:type=>"f",
   :class=>"Timeout",
   :file=>"/Users/test/.rbenv/versions/1.8.7-p358/lib/ruby/1.8/timeout.rb",
   :line=>52},
 "timeout.rb"=>
  {:type=>"F",
   :file=>"/Users/test/.rbenv/versions/1.8.7-p358/lib/ruby/1.8/timeout.rb",
   :line=>1},
 "Error"=>
  {:type=>"c",
   :class=>"Timeout",
   :file=>"/Users/test/.rbenv/versions/1.8.7-p358/lib/ruby/1.8/timeout.rb",
   :line=>35}}
```
