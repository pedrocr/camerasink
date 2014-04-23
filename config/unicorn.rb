nworkers = 16

$stderr.puts "Configuring unicorn for #{nworkers} workers"
worker_processes nworkers
