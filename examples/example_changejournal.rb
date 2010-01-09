########################################################################
# example_changejournal.rb
#
# A test script for general futzing.  Modify as you see fit. You can
# run this test script via the 'rake example' task.
########################################################################
require 'win32/changejournal'

puts 'VERSION: ' + Win32::ChangeJournal::VERSION

cj = Win32::ChangeJournal.new('c:\\')

# Wait up to 5 minutes for a change journals
cj.wait(300){ |array|
   array.each{ |struct|
      puts 'Something changed'
      puts 'File: ' + struct.file_name
      puts 'Action: ' + struct.action
      puts 'Path: ' + struct.path
   }
}

cj.delete
