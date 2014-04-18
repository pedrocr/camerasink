# Active Record can create more than one Camera object per camera id, CameraRunner
# is a singleton that actually handles the running of the camera capturing code
# and is the same actual object across all Camera objects with the same ID

class CameraRunner
  CAMERASAVE = File.expand_path('../../bin/camerasave', File.dirname(__FILE__))

  attr_accessor :source
  attr_reader :port

  def initialize(name)
    @camera = Camera.find_by_name(name)
    @name = name
    if !@camera
      Rails.logger.error "Couldn't find camera named #{name}"
      exit 2
    end
    @lock = Mutex.new
  end

  def port
    @lock.synchronize {return @port}
  end

  def run!
    if !@source
      Rails.logger.error "Trying to start camera without setting source"
      return
    end

    @basedir = File.expand_path("cameras/#{@name}/",Camerasink::BASEDIR)
    @tmpdir = File.expand_path('tmp', @basedir)
    @outdir = File.expand_path('stream', @basedir)
    
    #FIXME: Instead of deleting tmpdir we need to process partial files instead
    FileUtils.rm_rf @tmpdir    
    FileUtils.mkdir_p @tmpdir
    FileUtils.mkdir_p @outdir

    Rails.logger.info "Starting camera #{@name} on #{@basedir}"

    @runthread = Thread.new do
      IO.popen("#{CAMERASAVE} #{@source} #{@tmpdir}") do |cs|
        while IO.select([cs])
          if cs.eof?
            Rails.logger.warn "Got EOF on camerasave, seems to have stopped!"
            break
          else
            parseline cs.readline
          end
        end
      end
    end
  end

  def parseline(line)
    parts = line.split
    case parts[0]
    when 'STARTED'
      Rails.logger.info "Camera is starting"
    when 'CLOSEDFILE'
      newfile(parts[1],parts[2].to_i,parts[3].to_i)
    when 'LISTENING'
      @lock.synchronize {@port = parts[1].to_i}
      Rails.logger.info "Camera stream is on http://127.0.0.1:#{@port}/mjpeg"
    else
      Rails.logger.warn "CAMERASAVE: #{line}"
    end
  end

  def newfile(filename, starttime, endtime)
    Rails.logger.info "File ended #{filename}, from #{starttime} to #{endtime}"
    block = @camera.blocks.create
    block.filename = filename
    block.starttime = starttime
    block.endtime = endtime
    block.save!
    FileUtils.mv filename, @outdir
  end
end
