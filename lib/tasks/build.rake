require 'rake/testtask'

PROGS = ["camerasave","filejoin","testserver"].map{|p| "bin/"+p}
BINDIR = File.expand_path('../../bin/', File.dirname(__FILE__))

PROGS.each do |prog|
  file prog => ["#{prog}.c",__FILE__,"#{BINDIR}/common.h"] do
    sh "libtool --quiet --mode=link gcc -g -Wall -Werror -Wfatal-errors #{prog}.c -o #{prog} -I#{BINDIR} $(pkg-config --cflags --libs libsoup-2.4 gio-2.0 gio-unix-2.0 gstreamer-1.0 gstreamer-app-1.0 gstreamer-rtsp-server-1.0)"
  end
end

task :build => PROGS

namespace :test do
  desc "Test binaries"
  Rake::TestTask.new(:binaries => :build) do |t|    
    t.libs << "test"
    t.pattern = 'test/binaries/**/*_test.rb'
    t.verbose = true    
  end
end
Rake::Task[:test].enhance { Rake::Task["test:binaries"].invoke }
