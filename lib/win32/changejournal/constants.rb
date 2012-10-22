require 'ffi'

module Windows
  module ChangeJournal
    module Constants
      INVALID_HANDLE_VALUE    = 0xFFFFFFFF
      GENERIC_READ            = 0x80000000
      GENERIC_WRITE           = 0x40000000
      FILE_FLAG_OVERLAPPED    = 0x40000000
      FILE_SHARE_READ         = 1
      FILE_SHARE_WRITE        = 2
      OPEN_EXISTING           = 3
      FILE_DEVICE_FILE_SYSTEM = 0x00000009
      METHOD_BUFFERED         = 0

      def CTL_CODE(device, function, method, access)
        ((device) << 16) | ((access) << 14) | ((function) << 2) | (method)
      end

      def FSCTL_QUERY_USN_JOURNAL
        CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 61, METHOD_BUFFERED, 0)
      end
    end
  end
end
