require 'test/unit'
require 'fileutils'

class Test::Unit::TestCase
  TMPDIR = File.expand_path('../tmp/', File.dirname(__FILE__))
  BINDIR = File.expand_path('../bin/', File.dirname(__FILE__))

  def sh(cmd,opts={}) 
    message = opts[:message] || "\"#{cmd}\" failed"
    cmd = cmd+" > /dev/null" if !opts[:noisy]
    assert(system(cmd), message)
  end
end
