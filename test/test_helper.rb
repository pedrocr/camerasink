require 'test/unit'
require 'fileutils'

class Test::Unit::TestCase
  TMPDIR = File.expand_path('../tmp/', File.dirname(__FILE__))
  BINDIR = File.expand_path('../bin/', File.dirname(__FILE__))

  def sh(cmd,message=nil) 
    message ||= "\"#{cmd}\" failed"
    assert(system(cmd+" > /dev/null"), message)
  end
end
