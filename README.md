# camerasink

This is a proof-of-concept of the IPTV and surveillance camera software outlined in:

http://pedrocr.pt/text/a-better-base-for-opensource-iptv-tools/

The media handling stuff is written in C with gstreamer. Eventually the "porcelain" around this will be written in Ruby and Rails.

## Building

Running "rake" with no arguments should both build the C code and run the test suite.

## Usage

Right now there are three proof of concept programs. "testserver" is just a simplified version of "test-video" from gst-rtsp-server you can run it without arguments

    $ ./bin/testserver 
    stream ready at rtsp://127.0.0.1:8554/test

"testsave" reads from any URI (including file:// and rtsp://) and grabs any h264 stream it finds there and saves it to multiple individual Matroska files every few I-frames. Each file is a valid mkv file, playable with a regular media player. Run it as:

    $ ./bin/testsave rtsp://127.0.0.1:8554/test someoutputdir

"testread" reads from a set of .mkv files and constructs a single .mp4 file with all the streams concatenated. It again generates a file that can be read in any media player and/or the HTML5 <video> tag. Run it as:

    $ ./bin/testsave somefile.mp4 someoutputdir
    
