if defined? Camerasink::BASEDIR
  camera = Camera.find_by_name(:test)
  if !camera
    camera = Camera.new
    camera.name = "test"
    camera.save!
  end
  camera.source = "rtsp://127.0.0.1:8554/test"
  camera.run!
end
