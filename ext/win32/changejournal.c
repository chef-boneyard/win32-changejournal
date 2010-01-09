/* changejournal.c */
#include "ruby.h"
#include <windows.h>
#include <winioctl.h>
#include "changejournal.h"

// Function Prototypes
void InitialzeForMonitoring(ChangeJournalStruct *ptr);

/* This is a helper function that opens a handle to the volume specified
 * by the cDriveLetter parameter.
 */
HANDLE Open(TCHAR cDriveLetter, DWORD dwAccess, BOOL fAsyncIO){
   TCHAR szVolumePath[_MAX_PATH];
   HANDLE hCJ;
   wsprintf(szVolumePath, TEXT("\\\\.\\%c:"), cDriveLetter);
   
   hCJ = CreateFile(
      szVolumePath,
      dwAccess,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL,
      OPEN_EXISTING,
      (fAsyncIO ? FILE_FLAG_OVERLAPPED : 0),
      NULL
   );
   
   return(hCJ);
}

/* This function creates a journal on the volume. If a journal already
 * exists this function will adjust the MaximumSize and AllocationDelta
 * parameters of the journal.
 */
BOOL Create(ChangeJournalStruct *ptr, DWORDLONG MaximumSize,
   DWORDLONG AllocationDelta)
{

   DWORD cb;
   BOOL fOk;
   CREATE_USN_JOURNAL_DATA cujd;

   cujd.MaximumSize = MaximumSize;
   cujd.AllocationDelta = AllocationDelta;
   
   fOk = DeviceIoControl(
      ptr->hCJ,
      FSCTL_CREATE_USN_JOURNAL,
      &cujd,
      sizeof(cujd),
      NULL,
      0,
      &cb,
      NULL
   );
   
   return(fOk);
}

/* If DeleteFlags specifies USN_DELETE_FLAG_DELETE, the specified journal
 * will be deleted. If USN_DELETE_FLAG_NOTIFY is specified, the function
 * waits until the system has finished the delete process.
 * USN_DELETE_FLAG_NOTIFY can be specified alone to wait for the system
 * to finish if another application has started deleting a journal.
 */
BOOL Delete(ChangeJournalStruct *ptr,DWORDLONG UsnJournalID,DWORD DeleteFlags){
   DWORD cb;
   DELETE_USN_JOURNAL_DATA dujd;
   BOOL fOk;

   dujd.UsnJournalID = UsnJournalID;
   dujd.DeleteFlags = DeleteFlags;
   
   fOk = DeviceIoControl(
         ptr->hCJ,
         FSCTL_DELETE_USN_JOURNAL,
         &dujd,
         sizeof(dujd),
         NULL,
         0,
         &cb,
         NULL
   );
   
   return(fOk);
}

/* Return statistics about the journal on the current volume
 */
BOOL Query(ChangeJournalStruct *ptr, PUSN_JOURNAL_DATA pUsnJournalData) {

   
   DWORD cb;

   BOOL fOk = DeviceIoControl(ptr->hCJ, FSCTL_QUERY_USN_JOURNAL, NULL, 0,
      pUsnJournalData, sizeof(*pUsnJournalData), &cb, NULL);

   return(fOk);
}

/* This function starts the process of reading data from the journal. The
 * parameters specify the initial location and filters to use when
 * reading the journal. The usn parameter may be zero to start reading
 * at the start of available data. The EnumNext function is called to
 * actualy get journal records.
 */
void SeekToUsn(ChangeJournalStruct *ptr,
   USN usn, DWORD ReasonMask,
   DWORD ReturnOnlyOnClose, DWORDLONG UsnJournalID) {

   // Store the parameters in rujd. This will determine how we load
   // buffers with the EnumNext function.
   ptr->rujd.StartUsn          = usn;
   ptr->rujd.ReasonMask        = ReasonMask;
   ptr->rujd.ReturnOnlyOnClose = ReturnOnlyOnClose;
   ptr->rujd.Timeout           = 0;
   ptr->rujd.BytesToWaitFor    = 0;
   ptr->rujd.UsnJournalID      = UsnJournalID;
   ptr->cbCJData = 0;
   ptr->pUsnRecord = NULL;
}

/* This will return the next record in the journal that meets the
 * requirements specified with the SeekToUsn function. If no more
 * records are available, the functions returns NULL immediately
 * (since BytesToWaitFor is zero). This function will also return
 * NULL if there is an error loading a buffer with the DeviceIoControl
 * function. Use the GetLastError function to determine the cause of the
 * error.
 *
 * If the EnumNext function returns NULL, GetLastError() may return one
 * of the following.
 * 
 * S_OK - There was no error, but there are no more available journal
 *    records. Use the NotifyMoreData function to wait for more data to
 *    become available.
 *
 * ERROR_JOURNAL_DELETE_IN_PROGRESS - The journal is being deleted. Use
 *    the Delete function with USN_DELETE_FLAG_NOTIFY to wait for this
 *    process to finish. Then, call the Create function and then SeekToUsn
 *    to start reading from the new journal.
 * 
 * ERROR_JOURNAL_NOT_ACTIVE - The journal has been deleted. Call the Create
 *    function then SeekToUsn to start reading from the new journal.
 * 
 * ERROR_INVALID_PARAMETER - Possibly caused if the journal's ID has changed.
 *    Flush all cached data. Call the Query function and then SeekToUsn to
 *    start reading from the new journal.
 *
 * ERROR_JOURNAL_ENTRY_DELETED - Journal records were purged from the journal
 *    before we got a chance to process them. Cached information should be
 *    flushed.
 */
PUSN_RECORD EnumNext(ChangeJournalStruct *ptr) {

   // Make sure we have a buffer to use
   if(ptr->pbCJData == NULL)
     rb_raise(cCJError, "make sure we have a buffer to use");

   // If we do not have a record loaded, or enumerating to the next record
   // will point us past the end of the output buffer returned
   // by DeviceIoControl, we need to load a new block of data into memory
   if( (NULL == ptr->pUsnRecord) ||
       ((PBYTE)ptr->pUsnRecord + ptr->pUsnRecord->RecordLength) >=
        (ptr->pbCJData + ptr->cbCJData)
     )
   {
      BOOL fOk;
      ptr->pUsnRecord = NULL;

      fOk = DeviceIoControl(ptr->hCJ, FSCTL_READ_USN_JOURNAL,
         &ptr->rujd, sizeof(ptr->rujd), ptr->pbCJData,
         HeapSize(GetProcessHeap(), 0, ptr->pbCJData), &ptr->cbCJData, NULL);
      if(fOk){
         // It is possible that DeviceIoControl succeeds, but has not
         // returned any records - this happens when we reach the end of
         // available journal records. We return NULL to the user if there's
         // a real error, or if no records are returned.

         // Set the last error to NO_ERROR so the caller can distinguish
         // between an error, and the case where no records were returned.
         SetLastError(NO_ERROR);

         // Store the 'next usn' into m_rujd.StartUsn for use the
         // next time we want to read from the journal
         ptr->rujd.StartUsn = * (USN *) ptr->pbCJData;

         // If we got more than sizeof(USN) bytes, we must have a record.
         // Point the current record to the first record in the buffer
         if (ptr->cbCJData > sizeof(USN))
            ptr->pUsnRecord = (PUSN_RECORD) &ptr->pbCJData[sizeof(USN)];
      }
   } else {
      // The next record is already available in our stored
      // buffer - Move pointer to next record
      ptr->pUsnRecord = (PUSN_RECORD)
         ((PBYTE) ptr->pUsnRecord + ptr->pUsnRecord->RecordLength);
   }
   return(ptr->pUsnRecord);
}


/* Cleanup the memory and handles we were using.
 */
void CleanUp(ChangeJournalStruct *ptr) {
   USN_JOURNAL_DATA ujd;

   Query(ptr,&ujd);
   Delete(ptr,ujd.UsnJournalID, USN_DELETE_FLAG_DELETE);

   if (ptr->hCJ != INVALID_HANDLE_VALUE)
      CloseHandle(ptr->hCJ);
   if (ptr->hCJAsync != INVALID_HANDLE_VALUE)
      CloseHandle(ptr->hCJAsync);
   if (ptr->pbCJData != NULL)
      HeapFree(GetProcessHeap(), 0, ptr->pbCJData);
   if (ptr->oCJAsync.hEvent != NULL) {

      // Make sure the helper thread knows that we are exiting. We set
      // m_hwndApp to NULL then signal the overlapped event to make sure the
      // helper thread wakes up.
      SetEvent(ptr->oCJAsync.hEvent);
      CloseHandle(ptr->oCJAsync.hEvent);
   }
}

/* Call this to initialize the structure. The cDriveLetter parameter
 * specifies the drive that this instance will access. The cbBuffer
 * parameter specifies the size of the interal buffer used to read records
 * from the journal. This should be large enough to hold several records
 * (for example, 10 kilobytes will allow this class to buffer several
 * dozen journal records at a time).
 */
BOOL Init(ChangeJournalStruct *ptr, TCHAR cDriveLetter, DWORD cbBuffer){
   if(ptr->pbCJData != NULL){
      rb_raise(cCJError,
         "you should not call this function twice for one instance."
      );
   }

   ptr->cDriveLetter = cDriveLetter;

   // Allocate internal buffer
   ptr->pbCJData = (PBYTE) HeapAlloc(GetProcessHeap(), 0, cbBuffer);
   if(NULL == ptr->pbCJData){
      CleanUp(ptr);
      return(FALSE);
   }

   // Open a handle to the volume
   ptr->hCJ = Open(cDriveLetter, GENERIC_WRITE | GENERIC_READ, FALSE);
   if(INVALID_HANDLE_VALUE == ptr->hCJ){
      CleanUp(ptr);
      return(FALSE);
   }

   // Open a handle to the volume for asynchronous I/O. This is used to wait
   // for new records after all current records have been processed
   ptr->hCJAsync = Open(cDriveLetter, GENERIC_WRITE | GENERIC_READ, TRUE);
   if(INVALID_HANDLE_VALUE == ptr->hCJAsync){
      CleanUp(ptr);
      return(FALSE);
   }

   // Create an event for asynchronous I/O.
   ptr->oCJAsync.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
   if(NULL == ptr->oCJAsync.hEvent){
      CleanUp(ptr);
      return(FALSE);
   }

   return(TRUE);
}

// This function ensures that the journal on the volume is active
void InitialzeForMonitoring(ChangeJournalStruct *ptr) {
   BOOL fOk = TRUE;
   USN_JOURNAL_DATA ujd;

   // Try to query for current journal information
   while(fOk && !Query(ptr,&ujd)){
      switch(GetLastError()){
      case ERROR_JOURNAL_DELETE_IN_PROGRESS:
         // The system is deleting a journal. We need to wait for it to finish
         // before trying to query it again.
         Delete(ptr, 0, USN_DELETE_FLAG_NOTIFY);
         break;

      case ERROR_JOURNAL_NOT_ACTIVE:
         // The journal is not active on the volume. We need to create it and
         // then query for its information again
         Create(ptr,0x800000, 0x100000);
         break;

      default:
         // Some other error happened while querying the journal information.
         // There is nothing we can do from here
         rb_raise(cCJError, "unable to query journal");
         fOk = FALSE;
         break;
      }
   }

   // We were not able to query the volume for journal information
   if(!fOk)
      return;

   // Start processing records at the start of the journal
   SeekToUsn(ptr,ujd.FirstUsn, 0xFFFFFFFF, FALSE, ujd.UsnJournalID);

   // Initialize Path DB
   PopulatePath(ptr);
}


/*
 * :no-doc:
 */
static VALUE changejournal_allocate(VALUE klass){
   ChangeJournalStruct* ptr = malloc(sizeof(ChangeJournalStruct));
   return Data_Wrap_Struct(klass, 0, changejournal_free, ptr);
}

/*
 * :call-seq:
 *
 *    Win32::ChangeJournal.new(volume)
 *
 * Returns a new ChangeJournal object and places a monitor on +volume+.
 * 
 * Example:
 *
 *    # Put a change journal monitor on the C: drive
 *    cj = Win32::Change::Journal.new("C:\\")
 */
static VALUE changejournal_init(VALUE self, VALUE v_drive)
{
   ChangeJournalStruct* ptr;
   LPCTSTR lpDriveLetter = StringValuePtr(v_drive);

   Data_Get_Struct(self, ChangeJournalStruct, ptr);

   // Do not allow a block for this class
   if(rb_block_given_p())
      rb_raise(cCJError, "block not permitted for this class");

   // Initialize member variables
   ptr->hCJ = INVALID_HANDLE_VALUE;
   ptr->hCJAsync = INVALID_HANDLE_VALUE;
   ZeroMemory(&(ptr->oCJAsync), sizeof(ptr->oCJAsync));
   ptr->pbCJData = NULL;
   ptr->pUsnRecord = NULL;

   /* Initialize the ChangeJournal object with the current drive letter and tell
    * it to allocate a buffer of 10000 bytes to read journal records.
    */
   if(!Init(ptr, lpDriveLetter[0], 10000))
      rb_raise(rb_eTypeError, "initialization error");

   InitialzeForMonitoring(ptr);

   return self;
}

/*
 * :call-seq:
 *
 *    Win32::ChangeJournal#wait(num_seconds=INFINITE)
 *    Win32::ChangeJournal#wait(num_seconds=INFINITE){ |struct| ... }
 *
 * Waits up to 'num_seconds' for a notification to occur, or infinitely if
 * no value is specified.
 *
 * If a block is provided, yields a frozen ChangeJournalStruct that contains
 * three members - file_name, action, and path.
 *
 * Example:
 *
 *    require 'win32/changejournal'
 *    cj = Win32::ChangeJournal.new("C:\\")
 *
 *    # Wait for something to happen for 5 seconds, but ignore the details
 *    cj.wait(5)
 *
 *    # Wait for 5 seconds, print the details if any, then bail out
 *    cj.wait(5){ |struct| p struct }
 *
 *    # Wait indefinitely, and print the details when something happens
 *    cj.wait{ |struct| p struct }
 */
static VALUE changejournal_wait(int argc, VALUE* argv, VALUE self){
	VALUE v_timeout, v_block;
   ChangeJournalStruct *ptr;
   DWORD dwTimeout, dwWait;
   READ_USN_JOURNAL_DATA rujd;
   BOOL fOk;

   Data_Get_Struct(self, ChangeJournalStruct, ptr);

   rb_scan_args(argc, argv, "01&", &v_timeout, &v_block);

   if(NIL_P(v_timeout)){
      dwTimeout = INFINITE;
   }
   else{
      dwTimeout = NUM2UINT(v_timeout);
      dwTimeout *= 1000; // Convert milliseconds to seconds
   }

   rujd = ptr->rujd;
   rujd.BytesToWaitFor = 1;

   // Try to read at least one byte from the journal at the specified USN.
   // When 1 byte is available, the event in m_oCJAsync will be signaled.

   fOk = DeviceIoControl(
      ptr->hCJAsync,
      FSCTL_READ_USN_JOURNAL,
      &rujd,
      sizeof(rujd),
      &ptr->UsnAsync,
      sizeof(ptr->UsnAsync),
      NULL,
      &ptr->oCJAsync
   );

   // Do nothing
   if(!fOk && (GetLastError() != ERROR_IO_PENDING)) { }

   dwWait = WaitForSingleObject(ptr->oCJAsync.hEvent, dwTimeout);

   switch(dwWait){
      case WAIT_FAILED:
         rb_raise(cCJError, ErrorDescription(GetLastError()));
         break;
      case WAIT_OBJECT_0:
         rb_iv_set(self, "@signaled", Qtrue);
         if(Qnil != v_block){
            rb_yield(get_file_action(ptr));
         }
         return INT2NUM(1);
         break;
      case WAIT_ABANDONED_0:
         return INT2NUM(-1);
         break;
      case WAIT_TIMEOUT:
         return INT2NUM(0);
         break;
      default:
         rb_raise(cCJError,
            "unknown return value from WaitForSingleObject()"
         );
   };

   return self;
}

/*
 * call-seq:
 * 
 *    Win32::ChangeJournal#delete
 * 
 * Deletes the change journal on a volume, or waits for notification of
 * change journal deletion.
 *
 * Example:
 *
 *    require 'win32/changejournal'
 *    cj = Win32::ChangeJournal.new("C:\\")
 *    cj.wait(5)
 *    cj.delete
 */
static VALUE changejournal_delete(VALUE self){
   ChangeJournalStruct *ptr;
   USN_JOURNAL_DATA ujd;

   Data_Get_Struct(self, ChangeJournalStruct, ptr);

   Query(ptr, &ujd);
   Delete(ptr, ujd.UsnJournalID, USN_DELETE_FLAG_DELETE);

   return self;
}

void Init_changejournal()
{
	VALUE mWin32, cChangeJournal;

   /* The Win32 module serves as a namespace only. */
   mWin32 = rb_define_module("Win32");

   /* The Win32::ChangeJournal class encapsulates functions related to a
    * Windows change journal.
    */
   cChangeJournal = rb_define_class_under(mWin32, "ChangeJournal", rb_cObject);

   /* The ChangeJournal::Error class is typically raised if any of the
    * Win32::ChangeJournal methods fail.
    */
   cCJError = rb_define_class_under(cChangeJournal, "Error", rb_eStandardError);

   /* ChangeJournal class and instance methods */
   rb_define_alloc_func(cChangeJournal, changejournal_allocate);

   rb_define_method(cChangeJournal, "initialize", changejournal_init, 1);
   rb_define_method(cChangeJournal, "wait", changejournal_wait, -1);
   rb_define_method(cChangeJournal, "delete", changejournal_delete, 0);

   /* Struct definitions */
   sChangeJournalStruct = rb_struct_define(
      "ChangeJournalStruct",
      "action",
      "file_name",
      "path",
      0
   );

   /* 0.3.3: The version of the win32-changejournal library */
   rb_define_const(cChangeJournal, "VERSION",
      rb_str_new2(WIN32_CHANGEJOURNAL_VERSION));
}
