// For a discussion on clipboard formatting, see http://netez.com/2xExplorer/shellFAQ/adv_clip.html

#undef DEBUG

#ifdef WINDOWS_WINDOWS_H
#ifdef __cplusplus
extern "C" {
#endif
extern int winAddClip(int Datatype, void * Data, int Size, int Cut);
extern void winClearClipboard(void);
extern void winCopyClipboard(void);
extern int winExtractFile(void *pida, int Index, char *Result, int Size);
extern void winGetClip(int Datatype);
extern int winInit(void);
extern void winTerminate(void);
#ifdef __cplusplus
}
#endif
#else

#include <windows.h>
#include <winuser.h>
#include <shlobj.h>
#include <objidl.h>
#include <stdio.h>

#include <parasol/system/errors.h>

#define IS ==
#define OR ||
#define AND &&

enum {
   CT_DATA=0,
   CT_AUDIO,
   CT_IMAGE,
   CT_FILE,
   CT_OBJECT,
   CT_TEXT,
   CT_END      // End
};

#define CLIP_DATA    (1<<CT_DATA)   // 1
#define CLIP_AUDIO   (1<<CT_AUDIO)  // 2
#define CLIP_IMAGE   (1<<CT_IMAGE)  // 4
#define CLIP_FILE    (1<<CT_FILE)   // 8
#define CLIP_OBJECT  (1<<CT_OBJECT) // 16
#define CLIP_TEXT    (1<<CT_TEXT)   // 32

static HWND glClipWindow = 0, glCBChain = 0;
static BYTE glClipClassInit = 0, glOleInit = 0;
static DWORD glIgnoreClip = 0;
static UINT fmtShellIDList = 0;
static UINT fmtPasteSucceeded = 0;
static UINT fmtPerformedDropEffect = 0;
static UINT fmtPreferredDropEffect = 0;

#define HIDA_GetPIDLFolder(pida) (LPCITEMIDLIST)(((LPBYTE)pida)+(pida)->aoffset[0])
#define HIDA_GetPIDLItem(pida, i) (LPCITEMIDLIST)(((LPBYTE)pida)+(pida)->aoffset[i+1])

#ifdef DEBUG
#define MSG(...) fprintf(stderr, __VA_ARGS__)
#else
#define MSG(...) //fprintf(stderr, __VA_ARGS__)
#endif

void report_windows_clip_text(void *);
void report_windows_clip_utf16(void *);
void report_windows_files(LPIDA, int);
void report_windows_hdrop(LPIDA, int);

void winCopyClipboard(void);

//*****************************************************************************

void winClearClipboard(void)
{
   if (OpenClipboard(NULL)) {
      EmptyClipboard();
      CloseClipboard();
   }
}

//*****************************************************************************
// Called from clipAddFile(), clipAddText() etc

int winAddClip(int Datatype, void *Data, int Size, int Cut)
{
   UINT format;
   HGLOBAL hdata;
   char * pdata;

   MSG("winAddClip()\n");

   switch(Datatype) {
      case CLIP_DATA:   return ERR_NoSupport; break;
      case CLIP_AUDIO:  format = CF_WAVE; break;
      case CLIP_IMAGE:  format = CF_BITMAP; break;
      case CLIP_FILE:   format = CF_HDROP; Size += sizeof(DROPFILES); break;
      case CLIP_OBJECT: return ERR_NoSupport; break;
      case CLIP_TEXT:   format = CF_UNICODETEXT; break;
      default:
         return ERR_NoSupport;
   }

   if (OpenClipboard(NULL)) {
      int error;

      EmptyClipboard();

      if ((hdata = GlobalAlloc(GMEM_DDESHARE, Size))) {
         if ((pdata = (char *)GlobalLock(hdata))) {
            memcpy(pdata, Data, Size);
            GlobalUnlock(hdata);

            glIgnoreClip = GetTickCount();

            SetClipboardData(format, hdata);
            error = ERR_Okay;
         }
         else error = ERR_Lock;
      }
      else error = ERR_AllocMemory;

      CloseClipboard();
      return error;
   }
   else return ERR_Failed;

   return ERR_NoSupport;
}

//*****************************************************************************

void winGetClip(int Datatype)
{
   UINT format;

   switch (Datatype) {
      case CLIP_DATA:   return; break;
      case CLIP_AUDIO:  format = CF_WAVE; break;
      case CLIP_IMAGE:  format = CF_BITMAP; break;
      case CLIP_FILE:   format = CF_HDROP; break;
      case CLIP_OBJECT: return; break;
      case CLIP_TEXT:   format = CF_UNICODETEXT; break;
      default:
         return;
   }

   GetClipboardData(CF_UNICODETEXT);
}

//*****************************************************************************

void winCopyClipboard(void)
{
   void *pdata;
   IDataObject *pDataObj;
   IEnumFORMATETC *pEnumFmt;
   FORMATETC fmt;

   if (!glOleInit) {
      MSG("OLE not initialised.\n");
      return;
   }

   MSG("winCopyClipboard()\n");

   glIgnoreClip = GetTickCount(); // Needed to avoid automated successive calls to this function.

   if (OleGetClipboard(&pDataObj) IS S_OK) {
      // Enumerate the formats supported by this clip.  It is assumed that the formats
      // that are encountered first have priority.

      if (pDataObj->lpVtbl->EnumFormatEtc(pDataObj, DATADIR_GET, &pEnumFmt) IS S_OK) {
         while (pEnumFmt->lpVtbl->Next(pEnumFmt, 1, &fmt, NULL) IS S_OK) {
            if (fmt.cfFormat IS CF_UNICODETEXT) {
               FORMATETC fmt = { CF_UNICODETEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
               STGMEDIUM stgm;
               if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &stgm) IS S_OK) {
                  if ((pdata = GlobalLock(stgm.hGlobal))) {
                     report_windows_clip_utf16(pdata);
                     GlobalUnlock(stgm.hGlobal);
                  }
                  ReleaseStgMedium(&stgm);
               }
               break;
            }
            else if ((fmt.cfFormat IS CF_TEXT) OR (fmt.cfFormat IS CF_OEMTEXT) OR (fmt.cfFormat IS CF_DSPTEXT)) {
               FORMATETC fmt = { CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
               STGMEDIUM stgm;
               if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &stgm) IS S_OK) {
                  if ((pdata = GlobalLock(stgm.hGlobal))) {
                     report_windows_clip_utf16(pdata);
                     GlobalUnlock(stgm.hGlobal);
                  }
                  ReleaseStgMedium(&stgm);
               }
               break;
            }
            else if (fmt.cfFormat IS CF_HDROP) {
               FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
               STGMEDIUM stgm, effect;
               LPIDA pida;
               DWORD *effect_data;
               char cut_operation = 0;

               if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &stgm) IS S_OK) {
                  FORMATETC fmt = { fmtPreferredDropEffect, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
                  if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &effect) IS S_OK) {
                     if ((effect_data = (DWORD *)GlobalLock(effect.hGlobal))) {
                        if (*effect_data IS DROPEFFECT_MOVE) cut_operation = 1;
                        GlobalUnlock(effect.hGlobal);
                     }
                     ReleaseStgMedium(&effect);
                  }

                  if ((pida = (LPIDA)GlobalLock(stgm.hGlobal))) {
                     report_windows_hdrop(pida, cut_operation);
                     GlobalUnlock(stgm.hGlobal);
                  }
               }
            }
            else if (fmt.cfFormat IS fmtShellIDList) {
               // List of files found
               FORMATETC fmt = { fmtShellIDList, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
               STGMEDIUM stgm, effect;
               LPIDA pida;
               DWORD *effect_data;
               char cut_operation = 0;

               if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &stgm) IS S_OK) {
                  FORMATETC fmt = { fmtPreferredDropEffect, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
                  if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &effect) IS S_OK) {
                     if ((effect_data = (DWORD *)GlobalLock(effect.hGlobal))) {
                        if (*effect_data IS DROPEFFECT_MOVE) cut_operation = 1;
                        GlobalUnlock(effect.hGlobal);
                     }
                     ReleaseStgMedium(&effect);
                  }

                  if ((pida = (LPIDA)GlobalLock(stgm.hGlobal))) {
                     report_windows_files(pida, cut_operation);
                     GlobalUnlock(stgm.hGlobal);
                  }
                  ReleaseStgMedium(&stgm);
                }
                break;
            }
         }
         pEnumFmt->lpVtbl->Release(pEnumFmt);
      }

      pDataObj->lpVtbl->Release(pDataObj);
   }
}

//*****************************************************************************

int winExtractFile(LPIDA pida, int Index, char *Result, int Size)
{
   TCHAR path[MAX_PATH];

   if (Index >= (int)pida->cidl) return 0;

   LPCITEMIDLIST list;
   list = HIDA_GetPIDLFolder(pida);
   if (SHGetPathFromIDList(list, path)) {
      int pos, j;
      for (pos=0; (path[pos]) AND (pos < Size-1); pos++) Result[pos] = path[pos];

      list = HIDA_GetPIDLItem(pida, Index);
      if (SHGetPathFromIDList(list, path)) {
         for (j=0; path[j]; j++);
         while ((j > 0) AND (path[j-1] != '/') AND (path[j-1] != '\\')) j--;

         while ((path[j]) AND (pos < Size-1)) Result[pos++] = path[j++];
         Result[pos] = 0;

         return 1;
      }
      else return 0;
   }
   else return 0;
}

//*****************************************************************************

static LRESULT CALLBACK win_messages(HWND window, UINT msgcode, WPARAM wParam, LPARAM lParam)
{
   if (msgcode IS WM_DRAWCLIPBOARD) {
      // Clipboard content has changed
      if (GetTickCount() - glIgnoreClip < 500) { // The ignore flag is only valid if it has been set within the last 1/2 second (sometimes the ignore flag is set inappropriately)
         // Ignore anything that we've put on the Windows clipboard ourselves
         MSG("win_messages() ignoring clip.\n");
         glIgnoreClip = 0;
      }
      else {
         MSG("win_messages() calling copy clipboard.\n");
         winCopyClipboard();
      }

      // Send message to next listener on the chain

      SendMessage(glCBChain, msgcode, wParam, lParam);
   }
   else if (msgcode IS WM_CHANGECBCHAIN) {
      if ((HWND)wParam IS glCBChain) glCBChain = (HWND)lParam;
      else if (glCBChain != NULL) SendMessage(glCBChain, msgcode, wParam, lParam);
   }
   else {
      MSG("Clipboard message detected %d.\n", msgcode);
      return DefWindowProc(window, msgcode, wParam, lParam);
   }

   return 0;
}

/*****************************************************************************
** Initialisation sequence for Windows.
*/

int winInit(void)
{
   // Create an invisible window that we will use to wake us up when clipboard events occur.

   if (!fmtShellIDList) fmtShellIDList = RegisterClipboardFormat(CFSTR_SHELLIDLIST);
   if (!fmtPasteSucceeded) fmtPasteSucceeded = RegisterClipboardFormat(CFSTR_PASTESUCCEEDED);
   if (!fmtPerformedDropEffect) fmtPerformedDropEffect = RegisterClipboardFormat(CFSTR_PERFORMEDDROPEFFECT);
   if (!fmtPreferredDropEffect) fmtPreferredDropEffect = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);

   if (!glClipClassInit) {
      WNDCLASSEX glClipClass;
      glClipClass.cbSize        = sizeof(glClipClass);
      glClipClass.style         = CS_DBLCLKS;
      glClipClass.lpfnWndProc   = win_messages;
      glClipClass.cbClsExtra    = 0;
      glClipClass.cbWndExtra    = 0;
      glClipClass.hInstance     = GetModuleHandle(NULL);
      glClipClass.hIcon         = NULL;
      glClipClass.hCursor       = NULL;
      glClipClass.hbrBackground = NULL;
      glClipClass.lpszMenuName  = NULL;
      glClipClass.lpszClassName = "ClipClass";
      glClipClass.hIconSm       = NULL;
      if (!RegisterClassEx(&glClipClass)) return ERR_Failed;

      glClipClassInit = 1;
   }

   if (!glClipWindow) {
      if (!(glClipWindow = CreateWindowEx(0,
         "ClipClass", "ClipWindow",
         0,
         0, 0,
         CW_USEDEFAULT, CW_USEDEFAULT,
         (HWND)NULL,
         (HMENU)NULL,
         GetModuleHandle(NULL), NULL))) return ERR_Failed;
   }

   if (!glOleInit) {
      HRESULT result = OleInitialize(NULL);

      if (result IS S_OK) glOleInit = 1; // 1 = Successful initialisation
      else if (result IS S_FALSE) glOleInit = 2; // 2 = Attempted initialisation failed.
   }

   // Calling SetClipboardViewer() on the window will result in clipboard message WM_DRAWCLIPBOARD being sent whenever
   // the content of the clipboard changes.

   if (!glCBChain) glCBChain = SetClipboardViewer(glClipWindow);

   return ERR_Okay;
}

//*****************************************************************************

void winTerminate(void)
{
   if (glClipWindow) {
      if (glCBChain) {
         ChangeClipboardChain(glClipWindow, glCBChain);
         glCBChain = 0;
      }

      DestroyWindow(glClipWindow);
      glClipWindow = 0;
   }

   if (glClipClassInit) {
      UnregisterClass("ClipClass", GetModuleHandle(NULL));
      glClipClassInit = 0;
   }

   if (glOleInit IS 1) {
      OleUninitialize();
      glOleInit = 0;
   }

}

#endif // WINDOWS_WINDOWS_H
