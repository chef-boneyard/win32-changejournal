#define WIN32_CHANGEJOURNAL_VERSION "0.3.3"
#define MAX_STRING 1024

static VALUE cCJError;
static VALUE sChangeJournalStruct;
static char error[MAX_STRING];
static VALUE hPathDB;

// Prototype
BOOL Query(ChangeJournalStruct, PUSN_JOURNAL_DATA);
void add_reason(char*, const char*);

struct changejournalstruct {
   TCHAR  cDriveLetter; // drive letter of volume
   HANDLE hCJ;          // handle to volume

   // Members used to enumerate journal records
   READ_USN_JOURNAL_DATA rujd;       // parameters for reading records
   PBYTE                 pbCJData;   // buffer of records
   DWORD                 cbCJData;   // valid bytes in buffer
   PUSN_RECORD           pUsnRecord; // pointer to current record

   // Members used to notify application of new data
   HANDLE     hCJAsync;      // Async handle to volume
   OVERLAPPED oCJAsync;      // overlapped structure
   USN        UsnAsync;      // output buffer for overlapped I/O
   DWORD      dwDelay;       // delay before sending notification
};

typedef struct changejournalstruct ChangeJournalStruct;

void CleanUp(ChangeJournalStruct *ptr);

static void changejournal_free(ChangeJournalStruct *p)
{
   CleanUp(p);
   free(p);
}

// Helper function for the get_file_action function.
void add_reason(char* str, const char* reason){
   if(strlen(str) > 0)
      strcat(str, ", ");

   strcat(str, reason);
}

PUSN_RECORD EnumNext(ChangeJournalStruct *ptr);

// Enumerate the MFT for all entries. Store the file reference numbers of
// any directories in the database.
void PopulatePath(ChangeJournalStruct *ptr) {
   USN_JOURNAL_DATA ujd;
   BY_HANDLE_FILE_INFORMATION fi;
   TCHAR szRoot[_MAX_PATH];
   HANDLE hDir;
   MFT_ENUM_DATA med;
   BYTE pData[sizeof(DWORDLONG) + 0x10000]; // Process MFT in 64k chunks
   DWORDLONG fnLast = 0;
   DWORDLONG IndexRoot, FRN, PFRN;
   DWORD cb;
   LPWSTR pszFileName;
   WCHAR szFile[MAX_PATH];
   int cFileName;
   VALUE v_path_db;

   v_path_db = rb_hash_new();
   Query(ptr, &ujd);

   // Get the FRN of the root directory
   wsprintf(szRoot, TEXT("%c:\\"), ptr->cDriveLetter);

   hDir = CreateFile(
      szRoot,
      0,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL,
      OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS,
      NULL
   );

   GetFileInformationByHandle(hDir, &fi);
   CloseHandle(hDir);

   IndexRoot = (((DWORDLONG) fi.nFileIndexHigh) << 32) | fi.nFileIndexLow;

   wsprintf(szRoot, TEXT("%c:"), ptr->cDriveLetter);

   FRN = IndexRoot;
   PFRN =  0;

   rb_hash_aset(v_path_db,ULL2NUM(FRN),
      rb_ary_new3(2, rb_str_new(szRoot,2), ULL2NUM(PFRN)));

   med.StartFileReferenceNumber = 0;
   med.LowUsn = 0;
   med.HighUsn = ujd.NextUsn;

   while (DeviceIoControl(ptr->hCJ, FSCTL_ENUM_USN_DATA, &med, sizeof(med),
      pData, sizeof(pData), &cb, NULL) != FALSE) {
      PUSN_RECORD pRecord = (PUSN_RECORD) &pData[sizeof(USN)];

      while((PBYTE) pRecord < (pData + cb)){
         if(0 != (pRecord->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
            pszFileName = (LPWSTR)((PBYTE) pRecord + pRecord->FileNameOffset);
            cFileName = pRecord->FileNameLength / sizeof(WCHAR);

            wcsncpy(szFile, pszFileName, cFileName);
            szFile[cFileName] = 0;

            FRN = pRecord->FileReferenceNumber;
            PFRN = pRecord->ParentFileReferenceNumber;

            WideCharToMultiByte(
               CP_ACP,
               0,
               szFile,
               -1,
               szRoot,
               MAX_PATH,
               NULL,
               NULL
            );

         	rb_hash_aset(
               v_path_db,
               ULL2NUM(FRN),
               rb_ary_new3(2, rb_str_new2((char*)szRoot), ULL2NUM(PFRN))
            );

         }
         pRecord = (PUSN_RECORD) ((PBYTE) pRecord + pRecord->RecordLength);
      }

      med.StartFileReferenceNumber = * (DWORDLONG *) pData;
   }

   hPathDB = v_path_db;
}

// Returns both the file action and name in a Ruby struct
static VALUE get_file_action(ChangeJournalStruct *ptr){
   VALUE v_struct, v_action, v_array, v_arr, v_path, v_path_array;
   WCHAR szFile[MAX_PATH];
   char file_name[MAX_PATH];
   char path[MAX_PATH];
   char szReason[MAX_STRING];
   LPWSTR pszFileName;
   int cFileName;
   DWORDLONG FRN;
   DWORDLONG PFRN;
   VALUE v_path_db = hPathDB;

   v_array = rb_ary_new();

   while(EnumNext(ptr)) {
      pszFileName =
          (LPWSTR)((PBYTE) ptr->pUsnRecord + ptr->pUsnRecord->FileNameOffset);

      cFileName = ptr->pUsnRecord->FileNameLength / sizeof(WCHAR);
      wcsncpy(szFile, pszFileName, cFileName);
      szFile[cFileName] = 0;

      // If this is a close record for a directory, we may need to adjust
      // our directory database

      if(0 != (ptr->pUsnRecord->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
         && 0 != (ptr->pUsnRecord->Reason & USN_REASON_CLOSE))
      {

         FRN = ptr->pUsnRecord->FileReferenceNumber;
         PFRN = ptr->pUsnRecord->ParentFileReferenceNumber;

         WideCharToMultiByte(
            CP_ACP,
            0,
            szFile,
            -1,
            path,
            MAX_PATH,
            NULL,
            NULL
         );

         // Process newly created directories
         if(0 != (ptr->pUsnRecord->Reason & USN_REASON_FILE_CREATE)){
            rb_hash_aset(v_path_db,ULL2NUM(FRN),
            rb_ary_new3(2, rb_str_new2(path), ULL2NUM(PFRN)));
         }

         // Process renamed directories
         if(0 != (ptr->pUsnRecord->Reason & USN_REASON_RENAME_NEW_NAME)) {
            rb_hash_aset(v_path_db,ULL2NUM(FRN),
            rb_ary_new3(2, rb_str_new2(path), ULL2NUM(PFRN)));
         }

          // Process deleted directories
         if(0 != (ptr->pUsnRecord->Reason & USN_REASON_FILE_DELETE)){
            rb_hash_delete(v_path_db,ULL2NUM(FRN));
          }
      }

      szReason[0] = 0;

      if(ptr->pUsnRecord->Reason & USN_REASON_DATA_OVERWRITE)
         add_reason(szReason, "data_overwrite");

      if(ptr->pUsnRecord->Reason & USN_REASON_DATA_EXTEND)
         add_reason(szReason, "data_extend");

      if(ptr->pUsnRecord->Reason & USN_REASON_DATA_TRUNCATION)
         add_reason(szReason, "data_truncation");

      if(ptr->pUsnRecord->Reason & USN_REASON_NAMED_DATA_OVERWRITE)
         add_reason(szReason, "named_data_overwrite");

      if(ptr->pUsnRecord->Reason & USN_REASON_NAMED_DATA_EXTEND)
         add_reason(szReason, "named_data_extend");

      if(ptr->pUsnRecord->Reason & USN_REASON_NAMED_DATA_TRUNCATION)
         add_reason(szReason, "named_data_truncation");

      if(ptr->pUsnRecord->Reason & USN_REASON_FILE_CREATE)
         add_reason(szReason, "file_create");

      if(ptr->pUsnRecord->Reason & USN_REASON_FILE_DELETE)
         add_reason(szReason, "file_delete");

#ifdef USN_REASON_PROPERTY_CHANGE
      if(ptr->pUsnRecord->Reason & USN_REASON_PROPERTY_CHANGE)
         add_reason(szReason, "property_change");
#endif

#ifdef USN_REASON_EA_CHANGE
      if(ptr->pUsnRecord->Reason & USN_REASON_EA_CHANGE)
         add_reason(szReason, "property_change");
#endif

      if(ptr->pUsnRecord->Reason & USN_REASON_SECURITY_CHANGE)
         add_reason(szReason, "security_change");

      if(ptr->pUsnRecord->Reason & USN_REASON_RENAME_OLD_NAME)
         add_reason(szReason, "rename_old_name");

      if(ptr->pUsnRecord->Reason & USN_REASON_RENAME_NEW_NAME)
         add_reason(szReason, "rename_new_name");

      if(ptr->pUsnRecord->Reason & USN_REASON_INDEXABLE_CHANGE)
         add_reason(szReason, "indexable_change");

      if(ptr->pUsnRecord->Reason & USN_REASON_BASIC_INFO_CHANGE)
         add_reason(szReason, "basic_info_change");

      if(ptr->pUsnRecord->Reason & USN_REASON_HARD_LINK_CHANGE)
         add_reason(szReason, "hard_link_change");

      if(ptr->pUsnRecord->Reason & USN_REASON_COMPRESSION_CHANGE)
         add_reason(szReason, "compression_change");

      if(ptr->pUsnRecord->Reason & USN_REASON_ENCRYPTION_CHANGE)
         add_reason(szReason, "encryption_change");

      if(ptr->pUsnRecord->Reason & USN_REASON_OBJECT_ID_CHANGE)
         add_reason(szReason, "object_id_change");

      if(ptr->pUsnRecord->Reason & USN_REASON_REPARSE_POINT_CHANGE)
         add_reason(szReason, "reparse_point_change");

#ifdef USN_REASON_MOUNT_TABLE_CHANGE
      if(ptr->pUsnRecord->Reason & USN_REASON_MOUNT_TABLE_CHANGE)
         add_reason(szReason, "stream_change");

#endif
#ifdef USN_REASON_STREAM_CHANGE
      if(ptr->pUsnRecord->Reason & USN_REASON_STREAM_CHANGE)
         add_reason(szReason, "stream_change");
#endif

      if(ptr->pUsnRecord->Reason & USN_REASON_CLOSE)
         add_reason(szReason, "close");

      v_action = rb_str_new2(szReason);

	   *path = '\0';

	   FRN = ptr->pUsnRecord->ParentFileReferenceNumber;
	   v_path_array = rb_ary_new();

	   while(FRN != 0) {
         v_arr = rb_hash_aref(v_path_db,ULL2NUM(FRN));

         if(NIL_P(v_arr))
            break;

		   v_path = RARRAY(v_arr)->ptr[0];
		   rb_ary_unshift(v_path_array, v_path);
         FRN = NUM2ULL(RARRAY(rb_hash_aref(v_path_db, ULL2NUM(FRN)))->ptr[1]);
	   }

	   v_path = rb_ary_join(v_path_array, rb_str_new("\\", 1));
	   strcpy(path, StringValuePtr(v_path));

      WideCharToMultiByte(
         CP_ACP,
         0,
         szFile,
         -1,
         file_name,
         MAX_PATH,
         NULL,
         NULL
      );

      v_struct = rb_struct_new(
         sChangeJournalStruct,
         v_action,
         rb_str_new2(file_name),
         rb_str_new2(path)
      );

      // This is read-only information
      rb_obj_freeze(v_struct);

      rb_ary_push(v_array, v_struct);
   }

   return v_array;
}

// Return an error code as a string
LPTSTR ErrorDescription(DWORD p_dwError)
{
   HLOCAL hLocal = NULL;
   static char ErrStr[MAX_STRING];
   int len;

   if (!(len=FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER |
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      p_dwError,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
      (LPTSTR)&hLocal,
      0,
      NULL)))
   {
      rb_raise(rb_eStandardError, "Unable to format error message");
   }

   memset(ErrStr, 0, MAX_STRING);
   strncpy(ErrStr, (LPTSTR)hLocal, len-2); // remove \r\n
   LocalFree(hLocal);
   return ErrStr;
}
