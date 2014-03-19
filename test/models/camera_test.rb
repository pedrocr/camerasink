require 'test_helper'

class CameraTest < ActiveSupport::TestCase
  test "has blocks" do
     assert [blocks(:one),blocks(:two)].sort, cameras(:one).blocks.sort
  end

  test "runs camerasave and splits files" do
    testdir = File.expand_path(TMPDIR+"/splitsfiles/")
    inputfile = File.expand_path(testdir+"/input.mkv")

    # Clean up any previous tests
    FileUtils.rm_rf testdir

    # Create base folders
    FileUtils.mkdir_p testdir

    sh "gst-launch-1.0 videotestsrc num-buffers=500 ! x264enc ! matroskamux ! filesink location=#{inputfile}"

    camera = Camera.new("file://#{inputfile}", testdir)
    camera.run

    assert_equal 2, Dir["#{testdir}/stream/*.mkv"].size
  end
end
