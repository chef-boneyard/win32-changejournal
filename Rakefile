require 'rake'
require 'rake/clean'
require 'rake/testtask'
require 'rbconfig'
include Config

CLEAN.include(
  "**/*.gem",
  "**/*.so",
  "**/Makefile"
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
    Gem::Builder.new(spec).build
  end
end

=begin
desc 'Build a binary gem'
task :build_binary_gem => [:build] do
   mkdir_p 'lib/win32'
   mv 'ext/win32/changejournal.so', 'lib/win32'
   task :build_binary_gem => [:clean]

   spec = Gem::Specification.new do |gem|
      gem.name       = 'win32-changejournal'
      gem.version    = '0.3.3'
      gem.authors    = ['Daniel J. Berger', 'Park Heesob']
      gem.license    = 'Artistic 2.0'
      gem.email      = 'djberg96@gmail.com'
      gem.homepage   = 'http://www.rubyforge.org/projects/win32utils'
      gem.platform   = Gem::Platform::CURRENT
      gem.summary    = 'A library for monitoring files and directories on NTFS'
      gem.has_rdoc   = true
      gem.test_file  = 'test/test_win32_changejournal.rb'

      gem.files = [
         Dir['*'],
         Dir['test/*'],
         Dir['examples/*'],
         'ext/extconf.rb',
         'ext/win32/changejournal.c',
         'lib/win32/changejournal.so'
      ].flatten.reject{ |f| f.include?('CVS') }

      gem.required_ruby_version = '>= 1.8.2'
      gem.rubyforge_project = 'win32utils'

      gem.extra_rdoc_files = [
         'README',
         'CHANGES',
         'MANIFEST',
         'ext/win32/changejournal.c'
      ]

      gem.add_development_dependency('test-unit', '>= 2.0.3')

      gem.description = <<-EOF
         The win32-changejournal library provides an interface for MS Windows
         change journals. A change journal is a record of any changes on a given
         volume maintained by the operating system itself.
      EOF
   end

   Gem::Builder.new(spec).build
end
=end

desc 'Run the example program'
task :example => [:build] do |t|
  ruby '-Iext examples/example_changejournal.rb'
end

Rake::TestTask.new(:test) do |t|
  task :test => [:build]
  t.libs << 'ext'
  t.warning = true
  t.verbose = true
end

task :default => :test
