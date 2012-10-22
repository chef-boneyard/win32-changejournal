require 'ffi'

module Windows
  module ChangeJournal
    module Constants
      INVALID_HANDLE_VALUE = 0xFFFFFFFF
      GENERIC_READ         = 0x80000000
      GENERIC_WRITE        = 0x40000000
      FILE_SHARE_READ      = 1
      FILE_SHARE_WRITE     = 2
      FILE_FLAG_OVERLAPPED = 0x40000000
    end
  end
end
