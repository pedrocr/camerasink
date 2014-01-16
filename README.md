# camerasink

This is a proof-of-concept of the IPTV and surveillance camera software outlined in:

http://pedrocr.pt/text/a-better-base-for-opensource-iptv-tools/

The media handling stuff is written in C with gstreamer. Eventually the "porcelain" around this will be written in Ruby and Rails.

## Building

Running "rake" with no arguments should both build the C code and run the test suite. Because of a few bugs found while writing this only the current gstreamer master branch will run this properly. When it's released gstreamer 1.2.3 should also do.

## Usage

Right now there are three proof of concept programs. **testserver** is just a simplified version of "test-video" from gst-rtsp-server. Run it without arguments:

    $ ./bin/testserver 
    stream ready at rtsp://127.0.0.1:8554/test

**camerasave** reads from any URI (including file:// and rtsp://) and grabs any h264 stream it finds there and saves it to multiple individual Matroska files every few I-frames. Each file is a valid mkv file, playable with a regular media player. Run it as:

    $ ./bin/camerasave rtsp://127.0.0.1:8554/test someoutputdir

This will also create a http server to stream an mjpeg stream (http://localhost:4000/mjpeg)

**filejoin** reads from a set of .mkv files and constructs a single .mkv file with all the streams concatenated. It again generates a file that can be read in any media player. Run it as:

    $ ./bin/filejoin somefile.mkv someoutputdir

## Author

Pedro Côrte-Real <pedro@pedrocr.net>
    
