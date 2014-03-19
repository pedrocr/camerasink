class CreateCameras < ActiveRecord::Migration
  def change
    create_table :cameras do |t|
      t.timestamps
    end
  end
end
