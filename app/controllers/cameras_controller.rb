class CamerasController < ApplicationController
  include ActionController::Live

  before_action :set_camera, only: [:show, :edit, :update, :destroy, :stream, :snap]

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
    response.headers['Content-Type'] = Camera::MJPEG_CONTENT_TYPE
    @camera.mjpeg_stream do |chunk|
      response.stream.write chunk
    end
  ensure
    response.stream.close
  end

  # GET /cameras/1/snap
  def snap
    response.headers['Content-Type'] = Camera::JPEG_CONTENT_TYPE
    response.stream.write @camera.jpeg_image
  ensure
    response.stream.close
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
