class Camera < ActiveRecord::Base
  has_many :blocks

  def runner
    if !self.id
      logger.error "Trying to get a logger from a camera without id"
      exit 2
    end

    @@runners ||= {}
    @@runners[self.id] ||= CameraRunner.new(name)
  end

  def source; runner.source; end
  def source=(source); runner.source = source; end
  def run!; runner.run!; end
  def port; runner.port; end
end
