require 'rack/streaming_proxy'

if defined? Camerasink::BASEDIR
  camera = Camera.find_by_name(:test)
  if !camera
    camera = Camera.new
    camera.name = "test"
    camera.save!
  end
  camera.source = "rtsp://127.0.0.1:8554/test"
  camera.run!

  Camerasink::Application.configure do
    config.streaming_proxy.logger             = Rails.logger
    config.streaming_proxy.log_verbosity      = Rails.env.production? ? :low : :high
    config.streaming_proxy.num_retries_on_5xx = 0    # 0 by default
    config.streaming_proxy.raise_on_5xx       = true # false by default

    # Will be inserted at the end of the middleware stack by default.
    config.middleware.use Rack::StreamingProxy::Proxy do |request|

      # Inside the request block, return the full URI to redirect the request to,
      # or nil/false if the request should continue on down the middleware stack.
      if request.path.start_with?('/camera/stream/')
        Rails.logger.info "Got request for #{request.path}"
        parts = request.path.split('/')
        camera = Camera.find_by_name(parts[-1])
        if camera && camera.port
          url = "http://127.0.0.1:#{camera.port}/mjpeg"
          Rails.logger.info "Passing stream for camera #{parts[-1]} to #{url}"
          url
        else
          Rails.logger.error "Couldn't get stream url for camera #{parts[-1]}"
          $stderr.puts "Camera #{camera.inspect} and port #{camera.port.inspect}"
          false
        end
      end
    end
  end
end
