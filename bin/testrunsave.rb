#!/usr/bin/env ruby

rd, wr = IO.pipe

childpid = fork do
  rd.close
  exec "./camerasave rtsp://127.0.0.1:8554/test testoutput/ #{wr.fileno}"
end

wr.close

while IO.select([rd])
  puts rd.readline
end
