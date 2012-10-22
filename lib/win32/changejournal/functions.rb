require 'ffi'

module Windows
  module ChangeJournal
    module Functions
      extend FFI::Library
      ffi_lib :kernel32

      attach_function :CloseHandle, [:ulong], :bool
      attach_function :CreateFileW, [:buffer_in, :ulong, :ulong, :pointer, :ulong, :ulong, :ulong], :ulong
      attach_function :DeviceIoControl, [:ulong, :ulong, :buffer_in, :ulong, :buffer_out, :ulong, :pointer, :pointer], :bool

      ffi_lib FFI::Library::LIBC

      attach_function :memset, [:pointer, :int, :size_t], :pointer
    end
  end
end
