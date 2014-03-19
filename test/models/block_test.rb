require 'test_helper'

class BlockTest < ActiveSupport::TestCase
  test "block has camera" do 
    assert_equal cameras(:one), blocks(:one).camera
  end
end
