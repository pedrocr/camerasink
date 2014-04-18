require 'fileutils'

BASESPEC = {adapter: "sqlite3",
            database: "db/production.sqlite3",
            pool: 5,
            timeout: 5000}

if defined? Camerasink::BASEDIR
  $stderr.puts "Starting the app on #{Camerasink::BASEDIR}"
  dbfile = File.expand_path(BASESPEC[:database], Camerasink::BASEDIR)
  ActiveRecord::Base.establish_connection BASESPEC.merge(database: dbfile)
  ActiveRecord::Migrator.migrate("db/migrate/")

  logdir = File.expand_path("log/", Camerasink::BASEDIR)
  FileUtils.mkdir_p logdir
  logfile = File.expand_path("camerasink.log", logdir)
  logger = Logger.new(logfile)
  Rails.logger = logger
  ActiveRecord::Base.logger = logger
  ActionController::Base.logger = logger
  ActionMailer::Base.logger = logger
else
  $stderr.puts "Trying to start the app without setting BASEDIR, no dice"
  exit 2
end
