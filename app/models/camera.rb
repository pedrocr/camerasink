class Camera < ActiveRecord::Base
  has_many :blocks

  CAMERASAVE = File.expand_path('../../bin/camerasave', File.dirname(__FILE__))

  def initialize(source, basedir)
    @source = source
    @basedir = basedir
    @tmpdir = File.expand_path('tmp', basedir)
    FileUtils.mkdir_p @tmpdir
    @outdir = File.expand_path('stream', basedir)
    FileUtils.mkdir_p @outdir
  end

  def run
    rd, wr = IO.pipe
    childpid = fork do
      rd.close
      exec "#{CAMERASAVE} #{@source} #{@tmpdir} #{wr.fileno} > /dev/null"
    end
    wr.close

    while IO.select([rd])
      if rd.eof?
        $stderr.puts "Got EOF on camerasave, seems to have stopped!"
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
      $stderr.puts "Camera is starting"
    when 'CLOSEDFILE'
      newfile(parts[1],parts[2].to_i,parts[3].to_i)
    end
  end

  def newfile(filename, starttime, endtime)
    $stderr.puts "File ended #{filename}, from #{starttime} to #{endtime}"
    FileUtils.mv filename, @outdir
  end
end
