require 'rake/testtask'

PROGS = ["camerasave","filejoin","testserver","testcase_libsoup"].map{|p| "bin/"+p}
BINDIR = File.expand_path('bin/', File.dirname(__FILE__))

PROGS.each do |prog|
  file prog => ["#{prog}.c","Rakefile","#{BINDIR}/common.h"] do
    sh "libtool --quiet --mode=link gcc -g -Wall -Werror -Wfatal-errors #{prog}.c -o #{prog} -I#{BINDIR} $(pkg-config --cflags --libs libsoup-2.4 gio-2.0 gstreamer-1.0 gstreamer-app-1.0 gstreamer-rtsp-server-1.0)"
  end
end

task :build => PROGS

task :default => [:build, :test]

Rake::TestTask.new(:test) do |test|
  test.pattern = 'test/*_test.rb'
  test.verbose = true
end
