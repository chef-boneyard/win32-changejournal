require 'ffi'

module Windows
  module ChangeJournal
    module Functions

      module FFI::Library
        def attach_pfunc(*args)
          attach_function(*args)
          private args[0]
        end
      end

      extend FFI::Library

      typedef :uintptr_t, :handle
      typedef :ulong, :dword
      typedef :pointer, :ptr

      ffi_lib :kernel32

      attach_pfunc :CloseHandle, [:handle], :bool
      attach_pfunc :CreateFileW, [:buffer_in, :dword, :dword, :ptr, :dword, :dword, :handle], :handle
      attach_pfunc :DeviceIoControl, [:handle, :dword, :ptr, :dword, :ptr, :dword, :ptr, :ptr], :bool
      attach_pfunc :FormatMessage, :FormatMessageA, [:ulong, :ptr, :ulong, :ulong, :ptr, :ulong, :ptr], :ulong

      ffi_lib FFI::Library::LIBC

      attach_pfunc :memset, [:pointer, :int, :size_t], :pointer

      # Returns a nicer Windows error message
      def win_error(function, err=FFI.errno)
        flags = 0x00001000 | 0x00000200
        buf = FFI::MemoryPointer.new(:char, 1024)

        FormatMessage(flags, nil, err , 0x0409, buf, 1024, nil)

        function + ': ' + buf.read_string.strip
      end

      # Shortcut for win_error with raise builtin
      def raise_windows_error(function, err=FFI.errno)
        raise SystemCallError.new(win_error(function, err), err)
      end

      module_function :win_error
      module_function :raise_windows_error
    end
  end
end
