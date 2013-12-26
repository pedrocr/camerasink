#!/usr/bin/env ruby

require 'gst'

Gst.init
$stderr.puts "INFO: Using #{Gst.version_string}"

unless ARGV.length == 1
  $stderr.puts "Usage: #{__FILE__} <filename>"
  exit 1
end

def make_element(factory,name)
  element = Gst::ElementFactory.make(factory, name)
  if !element
    $stderr.puts "FATAL: Couldn't make element #{name} from factory #{factory}"
    exit 2
  end
  element
end

# Setup the elements
pipeline = Gst::Pipeline.new("None")
videosrc = make_element("videotestsrc","videotestsrc")
encoder = make_element("x264enc","x264enc")
queue = make_element("queue","source")
avimux = make_element("avimux","avimux")
filesink = make_element("filesink","filesink")
filesink.location = ARGV[0]

# Setup the pipeline
pipeline.add(videosrc, encoder, queue, avimux, filesink)
videosrc >> encoder >> queue >> avimux >> filesink

pad = avimux.get_static_pad("src")
pad.add_probe(Gst::PadProbeType.BUFFER) do |info, data|

end
exit 2

# create the program's main loop
loop = GLib::MainLoop.new(nil, false)

# listen to playback events
bus = pipeline.bus
bus.add_watch do |bus, message|
  #case message.type
  #when Gst::Message::EOS
  #  loop.quit
  #when Gst::Message::ERROR
  #  p message.parse
  #  loop.quit
  #end
  true
end

# start playing
pipeline.play
begin
  loop.run
rescue Interrupt
ensure
  pipeline.stop
end
