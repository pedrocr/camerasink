require 'net/http'

class Camera < ActiveRecord::Base
  has_many :blocks

  def runner
    if !self.id
      logger.error "Trying to get a logger from a camera without id"
      exit 2
    end

    @@runners ||= {}
    @@runners[self.id] ||= CameraRunner.new(name)
  end

  def source; runner.source; end
  def source=(source); runner.source = source; end
  def run!; runner.run!; end
  def port; runner.port; end

  MJPEG_MARKER = "SurelyJPEGDoesntIncludeThis"
  MJPEG_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=#{MJPEG_MARKER}"
  def mjpeg_stream
    #FIXME: check that streaming is actually possible
    Rails.logger.info "Connecting to camera on port #{self.port}"

    Net::HTTP.start("127.0.0.1", self.port) do |http|
      req = Net::HTTP::Get.new "/mjpeg"

      http.request req do |res|
        res.read_body do |chunk|
          yield chunk
        end
      end
    end
  end

  JPEG_CONTENT_TYPE = "image/jpeg"
  def jpeg_image
    synched = false
    mjpeg_stream do |chunk|
      return chunk if synched
      # Find a first line with the correct marker
      synched = true if chunk[2...MJPEG_MARKER.size+2] == MJPEG_MARKER
    end
  end
end
