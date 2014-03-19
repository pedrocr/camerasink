BASESPEC = {adapter: "sqlite3",
            database: "db/production.sqlite3",
            pool: 5,
            timeout: 5000}

if defined? Camerasink::BASEDIR
  $stderr.puts "Starting the app on #{Camerasink::BASEDIR}"
  dbfile = File.expand_path(BASESPEC[:database], Camerasink::BASEDIR)
  ActiveRecord::Base.establish_connection BASESPEC.merge(database: dbfile)
  ActiveRecord::Migrator.migrate("db/migrate/")
end
