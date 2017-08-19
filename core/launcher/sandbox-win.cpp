// Information on the Windows Integrity Mechanism:  https://msdn.microsoft.com/en-us/library/bb625957.aspx
//
// Low-integrity processes can write and create subfolders under %USER PROFILE%\AppData\LocalLow
// Reading files at any location will generally work, opening files with write access will not.
// Executing other programs is possible, but they will inherit the same low integrity permissions as the parent.
// If the low-integrity process needs to write to files outside of LocalLow, call set_low_file()

#define _WIN32_WINNT _WIN32_WINNT_WIN6

#include <windows.h>
#include <stdio.h>
#include <sddl.h>
#include <aclapi.h>

#include <parasol/system/types.h>
#include <parasol/system/errors.h>

#include "common.h"

IntegrityLevel get_integrity_level();
static const char * GetIntegrityLevelString(IntegrityLevel integrity_level);
static LONG str_copy(const char *String, char *Dest, int Length) __attribute__((unused));
static ERROR set_low_file(LPCWSTR pwszFileName) __attribute__((unused));

static LONG str_copy(const char *String, char *Dest, int Length)
{
   if (Length < 0) return(0);
   LONG i = 0;
   if ((String) AND (Dest)) {
      while ((i < Length) AND (*String)) {
         Dest[i++] = *String++;
      }
      if ((*String) AND (i >= Length)) Dest[i-1] = 0;
      else Dest[i] = 0;
   }
   return(i);
}

//****************************************************************************

int get_exe(char *Buffer, int Size)
{
   return(GetModuleFileName(NULL, Buffer, Size));
}

//****************************************************************************

static const char * GetIntegrityLevelString(IntegrityLevel integrity_level)
{
   switch (integrity_level) {
      case INTEGRITY_LEVEL_SYSTEM:     return "S-1-16-16384";
      case INTEGRITY_LEVEL_HIGH:       return "S-1-16-12288";
      case INTEGRITY_LEVEL_MEDIUM:     return "S-1-16-8192";
      case INTEGRITY_LEVEL_MEDIUM_LOW: return "S-1-16-6144";
      case INTEGRITY_LEVEL_LOW:        return "S-1-16-4096";
      case INTEGRITY_LEVEL_BELOW_LOW:  return "S-1-16-2048";
      case INTEGRITY_LEVEL_UNTRUSTED:  return "S-1-16-0";
      case INTEGRITY_LEVEL_UNKNOWN:
      case INTEGRITY_LEVEL_LAST:       return NULL;
   }

   return NULL;
}

//****************************************************************************
// Returns the integrity level of the running process, expressed as an INTEGRITY constant.

IntegrityLevel get_integrity_level(void)
{
   HANDLE hToken;
   HANDLE hProcess = GetCurrentProcess();
   IntegrityLevel result = INTEGRITY_LEVEL_UNKNOWN;

   if (OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_QUERY_SOURCE, &hToken)) {
      // Get the Integrity level.
      DWORD dwLengthNeeded;
      if (!GetTokenInformation(hToken, TokenIntegrityLevel, NULL, 0, &dwLengthNeeded)) {
         DWORD dwError = GetLastError();
         if (dwError == ERROR_INSUFFICIENT_BUFFER) {
            PTOKEN_MANDATORY_LABEL pTIL;
            pTIL = (PTOKEN_MANDATORY_LABEL)LocalAlloc(0, dwLengthNeeded);
            if (pTIL) {
               if (GetTokenInformation(hToken, TokenIntegrityLevel, pTIL, dwLengthNeeded, &dwLengthNeeded)) {
                  DWORD dwIntegrityLevel = *GetSidSubAuthority(pTIL->Label.Sid, (DWORD)(UCHAR)(*GetSidSubAuthorityCount(pTIL->Label.Sid)-1));
                  if (dwIntegrityLevel < SECURITY_MANDATORY_MEDIUM_RID) {
                     result = INTEGRITY_LEVEL_LOW;
                  }
                  else if (dwIntegrityLevel >= SECURITY_MANDATORY_MEDIUM_RID && dwIntegrityLevel < SECURITY_MANDATORY_HIGH_RID) {
                     result = INTEGRITY_LEVEL_MEDIUM;
                  }
                  else if (dwIntegrityLevel >= SECURITY_MANDATORY_HIGH_RID) {
                     result = INTEGRITY_LEVEL_HIGH;
                  }
               }
               LocalFree(pTIL);
            }
         }
      }
      CloseHandle(hToken);
   }

   return result;
}

//****************************************************************************
// Execute a process at low priority.

ERROR create_low_process(const char *ExePath, BYTE SharedOutput)
{
   ERROR result = ERR_Failed;
   HANDLE hToken = NULL;
   HANDLE hNewToken = NULL;
   PSID pIntegritySid = NULL;
   PROCESS_INFORMATION proc_info;
   STARTUPINFOEX startup_info;

   ZeroMemory(&startup_info, sizeof(startup_info));
   startup_info.StartupInfo.cb = sizeof(startup_info);
   ZeroMemory(&proc_info, sizeof(proc_info));

   if (SharedOutput) {
      startup_info.StartupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
      startup_info.StartupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
      startup_info.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
   }

   if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_ADJUST_DEFAULT | TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY, &hToken)) goto exit;
   if (!DuplicateTokenEx(hToken, 0, NULL, SecurityImpersonation, TokenPrimary, &hNewToken)) goto exit;
   if (!ConvertStringSidToSid(GetIntegrityLevelString(INTEGRITY_LEVEL_LOW), &pIntegritySid)) goto exit;

   {
      TOKEN_MANDATORY_LABEL TIL = {0};
      TIL.Label.Attributes = SE_GROUP_INTEGRITY; // Target group integrity.
      TIL.Label.Sid        = pIntegritySid; // Low priority SID

      // Set the integrity level for the new process.

      if (!SetTokenInformation(hNewToken, TokenIntegrityLevel, &TIL, sizeof(TOKEN_MANDATORY_LABEL) + GetLengthSid(pIntegritySid))) {
         goto exit;
      }
   }

   // Create the new process at Low integrity

   if (CreateProcessAsUser(hNewToken,
         NULL,
         (LPSTR)ExePath, // Command line
         NULL, // Process attributes
         NULL, // Thread attributes
         (SharedOutput) ? TRUE : FALSE, // Inherit handles
         EXTENDED_STARTUPINFO_PRESENT,
         NULL, // Environment
         NULL, // Directory
         (LPSTARTUPINFO)&startup_info,
         &proc_info)) {

      // Successfully created the process.  Wait for it to finish.
      WaitForSingleObject(proc_info.hProcess, INFINITE);

      // Get the exit code.
      DWORD rc, exitCode;
      rc = GetExitCodeProcess(proc_info.hProcess, &exitCode);

      result = ERR_Okay;
   }

exit:
   if (proc_info.hProcess != NULL) CloseHandle(proc_info.hProcess);
   if (proc_info.hThread != NULL) CloseHandle(proc_info.hThread);
   LocalFree(pIntegritySid);
   if (hNewToken != NULL) CloseHandle(hNewToken);
   if (hToken != NULL) CloseHandle(hToken);
   return result;
}

//****************************************************************************
// Changes the integrity of the target file so that low-integrity processes can write to it.
//
// This code could potentially target a file or directory, network share, registry key, semaphore, event, mutex, file mapping
// or timer (refer SetNamedSecurityInfo()).

static ERROR set_low_file(LPCWSTR pwszFileName)
{
   PSECURITY_DESCRIPTOR pSD = NULL;
   if (ConvertStringSecurityDescriptorToSecurityDescriptorW(L"S:(ML;;NW;;;LW)", SDDL_REVISION_1, &pSD, NULL)) {
      PACL pSacl = NULL; // not allocated
      BOOL fSaclPresent = FALSE;
      BOOL fSaclDefaulted = FALSE;
      if (GetSecurityDescriptorSacl(pSD, &fSaclPresent, &pSacl, &fSaclDefaulted)) {
         // Note that psidOwner, psidGroup, and pDacl are all NULL and set the new LABEL_SECURITY_INFORMATION
         SetNamedSecurityInfoW((LPWSTR) pwszFileName, SE_FILE_OBJECT, LABEL_SECURITY_INFORMATION, NULL, NULL, NULL, pSacl);
      }
      LocalFree(pSD);
      return(ERR_Okay);
   }
   else return(ERR_Failed);
}
