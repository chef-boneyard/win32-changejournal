require 'rubygems'

Gem::Specification.new do |spec|
  spec.name       = 'win32-changejournal'
  spec.version    = '0.3.4'
  spec.authors    = ['Daniel J. Berger', 'Park Heesob']
  spec.license    = 'Artistic 2.0'
  spec.email      = 'djberg96@gmail.com'
  spec.homepage   = 'http://www.rubyforge.org/projects/win32utils'
  spec.platform   = Gem::Platform::RUBY
  spec.summary    = 'A library for monitoring files and directories on NTFS'
  spec.has_rdoc   = true
  spec.test_file  = 'test/test_win32_changejournal.rb'
  spec.extensions = ['ext/extconf.rb']
  spec.files      = Dir['**/*'].reject{ |f| f.include?('git') }

  spec.rubyforge_project = 'win32utils'

  spec.extra_rdoc_files = [
    'README',
    'CHANGES',
    'MANIFEST',
    'ext/win32/changejournal.c'
  ]

  spec.add_development_dependency('test-unit', '>= 2.1.2')

  spec.description = <<-EOF
    The win32-changejournal library provides an interface for MS Windows
    change journals. A change journal is a record of any changes on a given
    volume maintained by the operating system itself.
  EOF
end
