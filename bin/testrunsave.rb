#!/usr/bin/env ruby

require File.expand_path('../lib/camerasink', File.dirname(__FILE__))

camera = CameraSink::Camera.new("file:///home/pedrocr/Projects/camerasink/bin/test.mkv", "testoutput/")
camera.run
$stderr.puts "Leaving so soon?"
