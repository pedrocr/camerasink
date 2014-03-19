class Camera < ActiveRecord::Base
  has_many :blocks

  CAMERASAVE = File.expand_path('../../bin/camerasave', File.dirname(__FILE__))

  attr_accessor :source, :basedir
  def basedir=(basedir)
    @basedir = basedir
    @tmpdir = File.expand_path('tmp', basedir)
    @outdir = File.expand_path('stream', basedir)
  end

  def run
    FileUtils.mkdir_p @tmpdir
    FileUtils.mkdir_p @outdir

    rd, wr = IO.pipe
    childpid = fork do
      rd.close
      exec "#{CAMERASAVE} #{@source} #{@tmpdir} #{wr.fileno} > /dev/null"
    end
    wr.close

    while IO.select([rd])
      if rd.eof?
        logger.warn "Got EOF on camerasave, seems to have stopped!"
        break
      else
        parseline rd.readline
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
