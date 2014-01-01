require File.expand_path('test_helper.rb', File.dirname(__FILE__))

class TestRoundtrip < Test::Unit::TestCase
  NUMBUFFERS = 1000

  def test_roundtrip_video
    testdir = File.expand_path(TMPDIR+"/roundtrip_video/")
    inputfile = File.expand_path(testdir+"/input.mkv")
    outputdir = File.expand_path(testdir+"/splits/")
    outputfile = File.expand_path(testdir+"/output.mkv")

    # Clean up any previous tests
    FileUtils.rm_rf testdir

    # Create base folders
    FileUtils.mkdir_p testdir
    FileUtils.mkdir_p outputdir

    sh "gst-launch-1.0 videotestsrc num-buffers=#{NUMBUFFERS} ! x264enc ! matroskamux ! filesink location=#{inputfile} > /dev/null"

    sh "#{BINDIR}/testsave file://#{inputfile} #{outputdir} > /dev/null"
    sh "#{BINDIR}/testread #{outputfile} #{outputdir}/*.mkv > /dev/null"
    
    [inputfile,outputfile].each do |file|
      sh "gst-launch-1.0 filesrc location=#{file} ! matroskademux ! h264parse ! filesink location=#{file}.raw > /dev/null"
    end

    sh "diff #{inputfile}.raw #{outputfile}.raw > /dev/null", "Binary files differ"

    # Clean up at the end
    FileUtils.rm_rf testdir
  end
end
