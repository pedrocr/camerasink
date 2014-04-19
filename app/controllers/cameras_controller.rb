class CamerasController < ApplicationController
  include ActionController::Live

  before_action :set_camera, only: [:show, :edit, :update, :destroy, :stream]

  # GET /cameras
  # GET /cameras.json
  def index
    @cameras = Camera.all
  end

  # GET /cameras/1
  # GET /cameras/1.json
  def show
  end

  # GET /cameras/1/stream
  def stream
    if !@camera.port
      # FIXME: return a "waiting for camera image jpg"
    else
      begin
        response.headers['Content-Type'] = 'multipart/x-mixed-replace;boundary=SurelyJPEGDoesntIncludeThis'

        logger.info "Connecting to camera on port #{@camera.port}"

        Net::HTTP.start("127.0.0.1", @camera.port) do |http|
          req = Net::HTTP::Get.new "/mjpeg"

          http.request req do |res|
            res.read_body do |chunk|
              response.stream.write chunk
            end
          end
        end
      ensure
        response.stream.close
      end
    end
  end

  # GET /cameras/new
  def new
    @camera = Camera.new
  end

  # GET /cameras/1/edit
  def edit
  end

  # POST /cameras
  # POST /cameras.json
  def create
    @camera = Camera.new(camera_params)

    respond_to do |format|
      if @camera.save
        format.html { redirect_to @camera, notice: 'Camera was successfully created.' }
        format.json { render action: 'show', status: :created, location: @camera }
      else
        format.html { render action: 'new' }
        format.json { render json: @camera.errors, status: :unprocessable_entity }
      end
    end
  end

  # PATCH/PUT /cameras/1
  # PATCH/PUT /cameras/1.json
  def update
    respond_to do |format|
      if @camera.update(camera_params)
        format.html { redirect_to @camera, notice: 'Camera was successfully updated.' }
        format.json { head :no_content }
      else
        format.html { render action: 'edit' }
        format.json { render json: @camera.errors, status: :unprocessable_entity }
      end
    end
  end

  # DELETE /cameras/1
  # DELETE /cameras/1.json
  def destroy
    @camera.destroy
    respond_to do |format|
      format.html { redirect_to cameras_url }
      format.json { head :no_content }
    end
  end

  private
    # Use callbacks to share common setup or constraints between actions.
    def set_camera
      @camera = Camera.find(params[:id])
    end

    # Never trust parameters from the scary internet, only allow the white list through.
    def camera_params
      params[:camera]
    end
end
