class CreateBlocks < ActiveRecord::Migration
  def change
    create_table :blocks do |t|
      t.references :camera, :index => true
      t.integer :starttime, :limit => 8, :index => true
      t.integer :endtime, :limit => 8, :index => true
      t.timestamps
    end
  end
end
