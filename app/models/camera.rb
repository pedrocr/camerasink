class Camera < ActiveRecord::Base
  has_many :blocks

  CAMERASAVE = File.expand_path('../../bin/camerasave', File.dirname(__FILE__))

  attr_accessor :source

  def run!
    if !name or !@source
      logger.error "Trying to start camera without setting name or source"
      return
    end

    @basedir = File.expand_path("cameras/#{name}/",Camerasink::BASEDIR)
    @tmpdir = File.expand_path('tmp', @basedir)
    @outdir = File.expand_path('stream', @basedir)
    
    #FIXME: Instead of deleting tmpdir we need to process partial files instead
    FileUtils.rm_rf @tmpdir    
    FileUtils.mkdir_p @tmpdir
    FileUtils.mkdir_p @outdir

    logger.info "Starting camera #{name} on #{@basedir}"

    @runthread = Thread.new do
      IO.popen("#{CAMERASAVE} #{@source} #{@tmpdir}") do |cs|
        while IO.select([cs])
          if cs.eof?
            logger.warn "Got EOF on camerasave, seems to have stopped!"
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
      logger.info "Camera is starting"
    when 'CLOSEDFILE'
      newfile(parts[1],parts[2].to_i,parts[3].to_i)
    else
      logger.warn "CAMERASAVE: #{line}"
    end
  end

  def newfile(filename, starttime, endtime)
    logger.info "File ended #{filename}, from #{starttime} to #{endtime}"
    block = self.blocks.create
    block.filename = filename
    block.starttime = starttime
    block.endtime = endtime
    block.save!
    FileUtils.mv filename, @outdir
  end
end
