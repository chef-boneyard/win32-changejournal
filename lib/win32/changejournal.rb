require 'ffi'
require File.join(File.dirname(__FILE__), 'changejournal', 'constants')
require File.join(File.dirname(__FILE__), 'changejournal', 'structs')
require File.join(File.dirname(__FILE__), 'changejournal', 'functions')

module Win32
  class ChangeJournal
    include Windows::ChangeJournal::Constants
    include Windows::ChangeJournal::Structs
    include Windows::ChangeJournal::Functions

    def initialize(drive = 'C:')
      drive = "\\\\.\\" << drive << 0.chr
      drive.encode!('UTF-16LE')

      handle = CreateFileW(
        drive,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nil,
        OPEN_EXISTING,
        0,
        0
      )

      if handle == INVALID_HANDLE_VALUE
        raise SystemCallError.new("CreateFileW", FFI.errno)
      end

      begin
        bytes = FFI::MemoryPointer.new(:ulong)
        journal_data = USN_JOURNAL_DATA.new

        bool = DeviceIoControl(
          handle,
          FSCTL_QUERY_USN_JOURNAL(),
          nil,
          0,
          journal_data,
          journal_data.size,
          bytes,
          nil
        )

        unless bool
          raise SystemCallError.new("DeviceIoControl", FFI.errno)
        end

        read_data = READ_USN_JOURNAL_DATA.new
        read_data[:StartUsn] = 0
        read_data[:ReasonMask] = 0xFFFFFFFF
        read_data[:ReturnOnlyOnClose] = false
        read_data[:Timeout] = 0
        read_data[:BytesToWaitFor] = 0
        read_data[:UsnJournalID] = journal_data[:UsnJournalID]

        buffer = 0.chr * 4096
        bytes  = FFI::MemoryPointer.new(:ulong)

        bool = DeviceIoControl(
          handle,
          FSCTL_READ_USN_JOURNAL(),
          read_data,
          read_data.size,
          buffer,
          buffer.size,
          bytes,
          nil
        )

        unless bool
          raise SystemCallError.new("DeviceIoControl", FFI.errno)
        end

        usn   = FFI::MemoryPointer.new(:double)
        bytes = bytes.read_ulong
        return_bytes = bytes - usn.size

        p buffer[0,USN_RECORD.size]
      ensure
        CloseHandle(handle)
      end
    end
  end
end

Win32::ChangeJournal.new
