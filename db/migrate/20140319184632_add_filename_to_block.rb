class AddFilenameToBlock < ActiveRecord::Migration
  def change
    add_column :blocks, :filename, :string
  end
end
