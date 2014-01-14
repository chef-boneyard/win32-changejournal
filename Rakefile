require 'rake'
require 'rake/clean'
require 'rake/testtask'
require 'rbconfig'
include RbConfig

CLEAN.include(
  "**/*.gem",
  "**/*.so",
  "**/Makefile",
  "**/*.pdb",
  "**/*.obj",
  "**/*.def",
  "**/*.lib",
  "**/*.exp"
)

CLOBBER.include("lib")

desc "Build win32-changejournal (but don't install it)"
task :build => [:clean] do
  make = CONFIG['host_os'] =~ /mingw|cygwin/i ? "make" : "nmake"

  Dir.chdir('ext') do
    ruby 'extconf.rb'
    sh make
    mv 'changejournal.so', 'win32' # For the test suite
  end
end

namespace :gem do
  desc "Create the win32-changejournal gem"
  task :create do
    spec = eval(IO.read('win32-changejournal.gemspec'))
    if Gem::VERSION < "2.0"
      Gem::Builder.new(spec).build
    else
      require 'rubygems/package'
      Gem::Package.build(spec)
    end
  end
end

desc 'Run the example program'
task :example => [:build] do |t|
  ruby '-Ilib examples/example_changejournal.rb'
end

Rake::TestTask.new(:test) do |t|
  t.warning = true
  t.verbose = true
end

task :default => :test
