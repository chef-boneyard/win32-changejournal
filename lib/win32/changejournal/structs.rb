require 'ffi'

module Windows
  module ChangeJournal
    module Structs
      class CREATE_USN_JOURNAL_DATA < FFI::Struct
        layout(:MaximumSize, :uint64, :AllocationDelta, :uint64)
      end

      class DELETE_USN_JOURNAL_DATA < FFI::Struct
        layout(:UsnJournalID, :uint64, :DeleteFlags, :ulong)
      end

      class USN_JOURNAL_DATA < FFI::Struct
        layout(
          :UsnJournalID, :uint64,
          :FirstUsn, :double,
          :NextUsn, :double,
          :LowestValidUsn, :double,
          :MaxUsn, :double,
          :MaximumSize, :uint64,
          :AllocationDelta, :uint64
        )
      end

      class READ_USN_JOURNAL_DATA < FFI::Struct
        layout(
          :StartUsn, :double,
          :ReasonMask, :ulong,
          :ReturnOnlyOnClose, :bool,
          :Timeout, :uint64,
          :BytesToWaitFor, :uint64,
          :UsnJournalID, :uint64
        )
      end

      class USN_RECORD < FFI::Struct
        layout(
          :RecordLength,              :ulong,
          :MajorVersion,              :ushort,
          :MinorVersion,              :ushort,
          :FileReferenceNumber,       :ulong_long,
          :ParentFileReferenceNumber, :ulong_long,
          :Usn,                       :double,
          :TimeStamp,                 :uint64,
          :Reason,                    :ulong,
          :SourceInfo,                :ulong,
          :SecurityId,                :ulong,
          :FileAttributes,            :ulong,
          :FileNameLength,            :ushort,
          :FileNameOffset,            :ushort,
          :FileName,                  :pointer
        )
      end
    end
  end
end
