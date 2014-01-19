require 'socket'
require 'win32ole'
require 'timeout'

module Win32
  class ChangeJournal
    class Error < StandardError; end

    # The version of the win32-changejournal library
    VERSION = '0.4.0'

    # The struct yielded to the wait method
    ChangeJournalStruct = Struct.new(
      'ChangeJournalStruct',
      :file,
      :action,
      :changes
    )

    # The path to be monitored.
    attr_reader :path

    # The host on which to monitor the path.
    attr_reader :host

    # Creates a new ChangeJournal object which sets the path and hostname
    # on which the filesystem will be monitored for changes. If no hostname
    # is provided, then the current host is assumed.
    #
    def initialize(path, host = Socket.gethostname)
      @path = path.tr("/", "\\")
      @host = host
      @conn = "winmgmts:{impersonationlevel=impersonate}!//#{host}/root/cimv2"
    end

    # A event loop that will yield a ChangeJournalStruct object whenever there
    # is a change in a file on the path set in the constructor. The loop will
    # timeout after the number of seconds provided, or indefinitely if no
    # argument is provided.
    #
    # The ChangeJournalStruct contains the following members:
    #
    # * file    # The full path of the file that was changed.
    # * action  # Either create, delete or modify.
    # * changes # If modified, includes an array of changes.
    #
    # If a modification occurs, the 'changes' struct member contains an
    # array of arrays that describes the change. Each sub-array contains
    # three members - the item that was modified, the previous value, and
    # the current value.
    #
    # Example:
    #
    # cj = Win32::ChangeJournal.new("C:/Users/foo")
    #
    # # Monitor filesystem for one minute.
    # cj.wait(60){ |s| p s }
    #
    # # Sample struct returned after file was marked
    # # hidden and read-only:
    #
    # #<struct Struct::ChangeJournalStruct
    #    file="c:\\Users\\foo\\test.txt",
    #    action="modify",
    #    changes=[
    #      ["AccessMask", "2032127", "2032057"],
    #      ["Hidden", "false", "true"],
    #      ["Writeable", "true", "false"]
    #    ]
    #  >
    #
    def wait(seconds = nil)
      ole    = WIN32OLE.connect(@conn)
      drive  = @path.split(':').first + ":"
      folder = @path.split(':').last.gsub("\\", "\\\\\\\\")
      folder << "\\\\" unless folder[-1] == "\\"

      query = %Q{
        select * from __instanceOperationEvent
        within 2
        where targetInstance isa 'CIM_DataFile'
        and targetInstance.Drive='#{drive}'
        and targetInstance.Path='#{folder}'
      }

      # Asynchronous call. This will let the user break out of it manually.
      sink  = WIN32OLE.new('WbemScripting.SWbemSink')
      event = WIN32OLE_EVENT.new(sink)

      ole.execNotificationQueryAsync(sink, query)
      sleep 0.5

      event.on_event("OnObjectReady"){ |object, context|
        target = object.TargetInstance
        struct = ChangeJournalStruct.new
        struct[:file] = target.Name

        case object.Path_.Class.to_s
        when "__InstanceCreationEvent"
          struct[:action] = 'create'
        when "__InstanceDeletionEvent"
          struct[:action] = 'delete'
        when "__InstanceModificationEvent"
          previous = object.PreviousInstance
          struct[:action] = 'modify'
          struct[:changes] = []

          target.Properties_.each{ |prop|
            if prop.Value != previous.send(prop.Name)
              struct[:changes] << [
                prop.Name,
                previous.send(prop.Name).to_s,
                prop.Value.to_s
              ]
            end
          }
        end

        yield struct
      }

      # If an argument is provided, timeout after that many seconds.
      if seconds
        begin
          Timeout.timeout(seconds){
            loop do
              WIN32OLE_EVENT.message_loop
            end
          }
        rescue Timeout::Error
          # Do nothing, return control to user
        end
      else
        loop do
          WIN32OLE_EVENT.message_loop
        end
      end
    end
  end
end
