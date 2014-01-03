require 'rake/testtask'

PROGS = ["testsave","testserver","testread","testcase_bug721289"].map{|p| "bin/"+p}

PROGS.each do |prog|
  file prog => ["#{prog}.c","Rakefile"] do
    sh "libtool --quiet --mode=link gcc -g -Wall -Werror -Wfatal-errors #{prog}.c -o #{prog} $(pkg-config --cflags --libs gstreamer-1.0 gstreamer-rtsp-server-1.0)"
  end
end

task :default => PROGS+[:test]

Rake::TestTask.new(:test) do |test|
  test.pattern = 'test/*_test.rb'
  test.verbose = true
end
