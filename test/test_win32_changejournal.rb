#############################################################################
# test_win32_changejournal.rb
#
# Test suite for the win32-changejournal library. You should run this
# via the 'rake test' task.
#############################################################################
require 'test-unit'
require 'win32/changejournal'
include Win32

class TC_Win32_ChangeJournal < Test::Unit::TestCase
  def self.startup
    Dir.chdir(File.expand_path(File.dirname(__FILE__)))
    @@file = 'win32_changejournal_test.txt'
  end

  def setup
    # The thread is used to force an event to happen for the tests
    @journal = ChangeJournal.new("c:\\")
    @thread  = Thread.new{
       sleep 2
       File.open(@@file, 'w'){ |fh| fh.puts 'Delete me!' }
    }
  end

  test "version constant is set to expected value" do
    assert_equal('0.4.0', ChangeJournal::VERSION)
  end

=begin
   def test_changejournal_action
      @thread.join
      @journal.wait(2){ |c|
         assert_kind_of(Array, c)
         assert_kind_of(Struct::ChangeJournalStruct, c.first)
         assert_equal(['action', 'file_name', 'path'], c.first.members)
         assert_true(c.first.frozen?)
      }
   end

   def test_delete
      assert_respond_to(@journal, :delete)
      assert_nothing_raised{ @journal.delete }
   end

   # We provide some very short timeouts here - shouldn't slow the tests down
   def test_wait_basic
      assert_respond_to(@journal, :wait)
      assert_nothing_raised{ @journal.wait(0.01) }
      assert_nothing_raised{ @journal.wait(0.01){ |s| } }
   end

   def test_wait_expected_errors
      assert_raise(ArgumentError){ @journal.wait(1,1) }
      assert_raise(TypeError){ @journal.wait('a') }
   end

   def test_error_class_defined
      assert_kind_of(Object, Win32::ChangeJournal::Error)
   end
=end

  def teardown
    @thread.kill if @thread.alive?

    @journal = nil
    @flags   = nil
    @thread  = nil
  end

  def self.shutdown
    File.delete(@@file) if File.exists?(@@file)
  end
end
