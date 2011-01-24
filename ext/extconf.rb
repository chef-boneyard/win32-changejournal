require 'mkmf'
require 'Win32API'

# Ensure that win32-ipc is installed.
begin
  require 'win32/ipc'
rescue LoadError
  begin
    require 'rubygems'
    require 'win32/ipc'
  rescue LoadError
    msg = %Q{
      The win32-ipc library is a prerequisite for this library.
      Please install win32-ipc before continuing.
      Exiting.
    }
    STDERR.puts msg
    exit
  end
end

# Set $CPPFLAGS as needed since mkmf doesn't do this for us.
GetVersionEx = Win32API.new('kernel32','GetVersionEx','P','I')
swCSDVersion = "\0" * 128
OSVERSIONINFO = [148,0,0,0,0,swCSDVersion].pack("LLLLLa128")
GetVersionEx.call(OSVERSIONINFO)
info = OSVERSIONINFO.unpack("LLLLLa128")

major, minor = info[1,2]
platform = info[4]
macro = '_WIN32_WINNT='

case minor
  when 2   # 2003
    macro += '0x0502'
  when 1   # XP
    macro += '0x0501'
  else     # 2000
    macro += '0x0500'
end

$CPPFLAGS += " -D#{macro}"

create_makefile('win32/changejournal', 'win32')
