require 'socket'
require 'win32ole'

module Win32
  class ChangeJournal
    class Error < StandardError; end

    # The version of the win32-changejournal library
    VERSION = '0.4.0'

    Struct.new("ChangeJournalStruct", :action, :file_name, :path)

    attr_reader :path
    attr_reader :host

    def initialize(path, host = Socket.gethostname)
      @path = path.tr("/", "\\")
      @host = host
      @conn = "winmgmts://#{host}/root/cimv2"
    end

    def wait(seconds = nil)
      ole    = WIN32OLE.connect(@conn)
      drive  = @path.split(':').first + ":"
      folder = @path.split(':').last.gsub("\\", "\\\\\\\\")

      query = %Q{
        select * from __instanceOperationEvent
        within 2
        where targetInstance isa 'CIM_DataFile'
        and targetInstance.Drive='#{drive}'
        and targetInstance.Path='#{folder}'
      }

      events = ole.execNotificationQuery(query)

      while event = events.nextEvent
        p event.path_
        p event.path_.class
      end
    end
  end
end
