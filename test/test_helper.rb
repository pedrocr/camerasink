ENV["RAILS_ENV"] ||= "test"
require File.expand_path('../../config/environment', __FILE__)
require 'rails/test_help'
require 'fileutils'

class ActiveSupport::TestCase
  ActiveRecord::Migration.check_pending!

  # Setup all fixtures in test/fixtures/*.yml for all tests in alphabetical order.
  #
  # Note: You'll currently still have to declare fixtures explicitly in integration tests
  # -- they do not yet inherit this setting
  fixtures :all

  TMPDIR = File.expand_path('../tmp/', File.dirname(__FILE__))
  BINDIR = File.expand_path('../bin/', File.dirname(__FILE__))

  def sh(cmd,opts={}) 
    message = opts[:message] || "\"#{cmd}\" failed"
    cmd = cmd+" > /dev/null" if !opts[:noisy]
    assert(system(cmd), message)
  end
end
