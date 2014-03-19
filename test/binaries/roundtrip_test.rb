require File.expand_path('../test_helper', File.dirname(__FILE__))

class TestRoundtrip < ActiveSupport::TestCase
  def roundtrip_video(numbuffers, encoder)
    testdir = File.expand_path(TMPDIR+"/roundtrip_video_#{encoder}/")
    inputfile = File.expand_path(testdir+"/input.mkv")
    outputdir = File.expand_path(testdir+"/splits/")
    outputfile = File.expand_path(testdir+"/output.mkv")

    # Clean up any previous tests
    FileUtils.rm_rf testdir

    # Create base folders
    FileUtils.mkdir_p testdir
    FileUtils.mkdir_p outputdir

    sh "gst-launch-1.0 videotestsrc num-buffers=#{numbuffers} ! #{encoder} ! matroskamux ! filesink location=#{inputfile}"

    sh "#{BINDIR}/camerasave file://#{inputfile} #{outputdir}"
    sh "#{BINDIR}/filejoin #{outputfile} #{outputdir}/*.mkv"
    
    # Get raw video from input and output files to be able to do binary comparison
    [inputfile,outputfile].each do |file|
      sh "gst-launch-1.0 filesrc location=#{file} ! decodebin ! filesink location=#{file}.raw"
    end

    sh "diff #{inputfile}.raw #{outputfile}.raw", :message => "Binary files differ"

    # Clean up at the end
    FileUtils.rm_rf testdir
  end

  def test_roundtrip_h264
    roundtrip_video(1000, "x264enc")
  end

  def test_roundtrip_mjpeg
    roundtrip_video(1000, "jpegenc")
  end
end
