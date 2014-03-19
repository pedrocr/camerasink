class AddNameToCamera < ActiveRecord::Migration
  def change
    add_column :cameras, :name, :string
  end
end
