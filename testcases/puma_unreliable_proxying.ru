require 'puma'
require 'rack'
require 'rack/streaming_proxy'
require 'webrick'

class BasicServing < WEBrick::HTTPServlet::AbstractServlet
  def do_GET(req, res)
    res.body = "Hello!\n"
    res.status = 200
    res['Content-Type'] = "text/html"
  end
end

s = WEBrick::HTTPServer.new(:Port => 9000)
s.mount('/', BasicServing)

thread = Thread.new do
  s.start
end
while s.status != :Running; sleep 0.1; end # Make sure the server is up

CHUNKED_SOURCE = "http://127.0.0.1:9000/"

use Rack::StreamingProxy::Proxy do |request|
  CHUNKED_SOURCE
end

app = lambda {|env| [200, {"Content-Type" => "text/plain"}, ["Hello"]] }
run app
