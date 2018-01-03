/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Task: System processes are managed by the Task class.

Tasks, also known as processes, form the basis of process execution in an operating system.  By creating a task object,
it is possible to execute a program from within the host system.  Programs that are compliant with Parasol may also
reveal additional meta information such as #Author and #Copyright strings in the task object.

To execute a compiled program, set the #Location field to point to the executable file before initialising the
task.  Arguments can be passed to the executable by setting the #Parameters field.  Once the task object is
successfully initialised, use the #Activate() action to run the executable.  If the file executes successfully,
a new task object is spawned separately to represent the executable (which means it is safe to destroy your task object
immediately afterwards).  If the #Activate() action returns with ERR_Okay then the executable program was run
successfully.

To find the task object that represents the active process, use the ~CurrentTask() function to quickly retrieve it.

To send messages to another task, you need to know its #MessageQueue ID so that ~SendMessage() can be used.  A simple
way to initiate interprocess communication is to pass your MessageQueue ID to the other task as a parameter.

-END-

*****************************************************************************/

#define PRV_TASK

#ifdef __CYGWIN__
#undef __unix__
#endif

#ifdef __unix__
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/wait.h>
#endif

#ifdef _WIN32
 #ifdef __CYGWIN__
  #include <unistd.h>
 #else
  #include <direct.h>
 #endif
#include <stdio.h>
#endif

#include "../defs.h"
#include <parasol/main.h>

extern void CloseCore(void);

static ERROR InterceptedAction(objTask *, APTR);

#ifdef __unix__

static void task_stdout(HOSTHANDLE FD, APTR);
static void task_stderr(HOSTHANDLE FD, APTR);

#elif _WIN32

#define DLLCALL // __declspec(dllimport)
#define WINAPI  __stdcall
#define HKEY_CLASSES_ROOT	   (0x80000000)
#define HKEY_CURRENT_USER	   (0x80000001)
#define HKEY_LOCAL_MACHINE	   (0x80000002)
#define HKEY_USERS	         (0x80000003)
#define HKEY_PERFORMANCE_DATA	(0x80000004)
#define HKEY_CURRENT_CONFIG	(0x80000005)
#define HKEY_DYN_DATA	      (0x80000006)

#define REG_DWORD 4
#define REG_DWORD_BIG_ENDIAN 5
#define REG_QWORD 11
#define REG_SZ 1

#define KEY_READ  0x20019
#define KEY_WRITE 0x20006

#define MAX_PATH 260
#define INVALID_HANDLE_VALUE (void *)(-1)

DLLCALL LONG WINAPI RegOpenKeyExA(LONG,CSTRING,LONG,LONG,APTR *);
DLLCALL LONG WINAPI RegQueryValueExA(APTR,CSTRING,LONG *,LONG *,BYTE *,LONG *);
DLLCALL LONG WINAPI RegSetValueExA(APTR hKey, CSTRING lpValueName, LONG Reserved, LONG dwType, const void *lpData, LONG cbData);

static LONG glProcessBreak = 0;
#endif

static ERROR GET_LaunchPath(objTask *, STRING *);

static ERROR TASK_Activate(objTask *, APTR);
static ERROR TASK_Free(objTask *, APTR);
static ERROR TASK_GetEnv(objTask *, struct taskGetEnv *);
static ERROR TASK_GetVar(objTask *, struct acGetVar *);
static ERROR TASK_Init(objTask *, APTR);
static ERROR TASK_NewObject(objTask *, APTR);
static ERROR TASK_ReleaseObject(objTask *, APTR);
static ERROR TASK_SetEnv(objTask *, struct taskSetEnv *);
static ERROR TASK_SetVar(objTask *, struct acSetVar *);
static ERROR TASK_Write(objTask *, struct acWrite *);

static ERROR TASK_AddArgument(objTask *, struct taskAddArgument *);
static ERROR TASK_CloseInstance(objTask *, APTR);
static ERROR TASK_Expunge(objTask *, APTR);
static ERROR TASK_Quit(objTask *, APTR);

static const struct FieldDef clFlags[] = {
   { "Foreign",    TSF_FOREIGN },
   { "Dummy",      TSF_DUMMY },
   { "Wait",       TSF_WAIT },
   { "Shell",      TSF_SHELL },
   { "ResetPath",  TSF_RESET_PATH },
   { "Privileged", TSF_PRIVILEGED },
   { "Debug",      TSF_DEBUG },
   { "Quiet",      TSF_QUIET },
   { "Attached",   TSF_ATTACHED },
   { "Detached",   TSF_DETACHED },
   { "Pipe",       TSF_PIPE },
   { NULL, 0 }
};

static const struct FieldArray clFields[];

static const struct ActionArray clActions[] = {
   { AC_Activate,      TASK_Activate },
   { AC_Free,          TASK_Free },
   { AC_GetVar,        TASK_GetVar },
   { AC_NewObject,     TASK_NewObject },
   { AC_ReleaseObject, TASK_ReleaseObject },
   { AC_SetVar,        TASK_SetVar },
   { AC_Init,          TASK_Init },
   { AC_Write,         TASK_Write },
   // The following actions are program dependent
   { AC_ActionNotify,  InterceptedAction },
   { AC_Clear,         InterceptedAction },
   { AC_Custom,        InterceptedAction },
   { AC_DataFeed,      InterceptedAction },
   { AC_Deactivate,    InterceptedAction },
   { AC_Disable,       InterceptedAction },
   { AC_Draw,          InterceptedAction },
   { AC_Enable,        InterceptedAction },
   { AC_Flush,         InterceptedAction },
   { AC_Focus,         InterceptedAction },
   { AC_Hide,          InterceptedAction },
   { AC_Lock,          InterceptedAction },
   { AC_LostFocus,     InterceptedAction },
   { AC_Move,          InterceptedAction },
   { AC_MoveToBack,    InterceptedAction },
   { AC_MoveToFront,   InterceptedAction },
   { AC_MoveToPoint,   InterceptedAction },
   { AC_NewChild,      InterceptedAction },
   { AC_NewOwner,      InterceptedAction },
   { AC_Query,         InterceptedAction },
   { AC_Read,          InterceptedAction },
   { AC_Redimension,   InterceptedAction },
   { AC_Rename,        InterceptedAction },
   { AC_Reset,         InterceptedAction },
   { AC_Resize,        InterceptedAction },
   { AC_SaveImage,     InterceptedAction },
   { AC_SaveToObject,  InterceptedAction },
   { AC_Scroll,        InterceptedAction },
   { AC_ScrollToPoint, InterceptedAction },
   { AC_Seek,          InterceptedAction },
   { AC_Show,          InterceptedAction },
   { AC_Unlock,        InterceptedAction },
   { AC_Clipboard,     InterceptedAction },
   { AC_Refresh,       InterceptedAction },
   { AC_Sort,          InterceptedAction },
   { AC_SaveSettings,  InterceptedAction },
   { AC_SelectArea,    InterceptedAction },
   { AC_Undo,          InterceptedAction },
   { AC_Redo,          InterceptedAction },
   { AC_DragDrop,      InterceptedAction },
   { NULL, NULL }
};

#include "class_task_def.c"

//****************************************************************************

ERROR add_task_class(void)
{
   LogF("~add_task_class()","");

   ERROR error;
   if (!NewPrivateObject(ID_METACLASS, 0, (OBJECTPTR *)&TaskClass)) {
      if (!SetFields((OBJECTPTR)TaskClass,
            FID_ClassVersion|TFLOAT,  VER_TASK,
            FID_Name|TSTRING,         "Task",
            FID_Category|TLONG,       CCF_SYSTEM,
            FID_FileExtension|TSTR,   "*.exe|*.bat|*.com",
            FID_FileDescription|TSTR, "Executable File",
            FID_FileHeader|TSTR,      "[0:$4d5a]|[0:$7f454c46]",
            FID_Actions|TPTR,         clActions,
            FID_Methods|TARRAY,       clTaskMethods,
            FID_Fields|TARRAY,        clFields,
            FID_Size|TLONG,           sizeof(objTask),
            FID_Path|TSTR,            "modules:core",
            TAGEND)) {
         error = acInit(&TaskClass->Head);
      }
      else error = ERR_SetField;
   }
   else error = ERR_NewObject;

   LogBack();
   return error;
}

#ifdef __unix__
static void check_incoming(objTask *Self)
{
   struct pollfd fd;

   if (Self->InFD != -1) {
      fd.fd = Self->InFD;
      fd.events = POLLIN;
      if ((poll(&fd, 1, 0) > 0) AND (fd.revents & POLLIN)) {
         task_stdout(Self->InFD, Self);
      }
   }

   if (Self->ErrFD != -1) {
      fd.fd = Self->ErrFD;
      fd.events = POLLIN;
      if ((poll(&fd, 1, 0) > 0) AND (fd.revents & POLLIN)) {
         task_stderr(Self->ErrFD, Self);
      }
   }
}
#endif

//****************************************************************************
// Data output from the executed process is passed via data channels to the object specified in Task->OutputID, and/or
// sent to a callback function.

#ifdef __unix__
static void task_stdout(HOSTHANDLE FD, APTR Task)
{
   static UBYTE recursive = 0;

   if (recursive) return;

   recursive++;

   LONG len;
   UBYTE buffer[2048];
   if ((len = read(FD, buffer, sizeof(buffer)-1)) > 0) {
      buffer[len] = 0;

      objTask *task = Task;
      if (task->OutputCallback.Type IS CALL_STDC) {
         void (*routine)(objTask *, APTR, LONG);
         routine = task->OutputCallback.StdC.Routine;
         routine(Task, buffer, len);
      }
      else if (task->OutputCallback.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = task->OutputCallback.Script.Script)) {
            const struct ScriptArg args[] = {
               { "Task",       FD_OBJECTPTR,       { .Address = Task } },
               { "Buffer",     FD_PTRBUFFER,       { .Address = buffer } },
               { "BufferSize", FD_LONG|FD_BUFSIZE, { .Long = len } }
            };
            scCallback(script, task->OutputCallback.Script.ProcedureID, args, ARRAYSIZE(args));
         }
      }
   }
   recursive--;
}

static void task_stderr(HOSTHANDLE FD, APTR Task)
{
   UBYTE buffer[2048];
   LONG len;
   static UBYTE recursive = 0;

   if (recursive) return;

   recursive++;
   if ((len = read(FD, buffer, sizeof(buffer)-1)) > 0) {
      buffer[len] = 0;

      objTask *task = Task;
      if (task->ErrorCallback.Type) {
         if (task->ErrorCallback.Type IS CALL_STDC) {
            void (*routine)(objTask *, APTR, LONG);
            routine = task->ErrorCallback.StdC.Routine;
            routine(Task, buffer, len);
         }
         else if (task->ErrorCallback.Type IS CALL_SCRIPT) {
            OBJECTPTR script;
            if ((script = task->ErrorCallback.Script.Script)) {
               const struct ScriptArg args[] = {
                  { "Task", FD_OBJECTPTR,       { .Address = Task } },
                  { "Data", FD_PTRBUFFER,       { .Address = buffer } },
                  { "Size", FD_LONG|FD_BUFSIZE, { .Long = len } }
               };
               scCallback(script, task->ErrorCallback.Script.ProcedureID, args, ARRAYSIZE(args));
            }
         }
      }
   }
   recursive--;
}
#endif

//****************************************************************************
// task_incoming_stdout() and task_incoming_stderr() are callbacks that are activated when data is incoming from a
// process that we've launched.

#ifdef _WIN32
static void output_callback(objTask *Task, FUNCTION *Callback, APTR Buffer, LONG Size)
{
   if (Callback->Type IS CALL_STDC) {
      void (*routine)(objTask *, APTR, LONG);
      routine = Callback->StdC.Routine;
      routine(Task, Buffer, Size);
   }
   else if (Callback->Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Callback->Script.Script)) {
         const struct ScriptArg args[] = {
            { "Task", FD_OBJECTPTR,       { .Address = Task } },
            { "Data", FD_PTRBUFFER,       { .Address = Buffer } },
            { "Size", FD_LONG|FD_BUFSIZE, { .Long = Size } }
         };
         scCallback(script, Callback->Script.ProcedureID, args, ARRAYSIZE(args));
      }
   }
}

static void task_incoming_stdout(WINHANDLE Handle, objTask *Task)
{
   static UBYTE recursive = 0;

   if (recursive) return;
   if (!Task->Platform) return;

   FMSG("task_stdout()","");

   UBYTE buffer[4096];
   LONG size = sizeof(buffer) - 1;
   winResetStdOut(Task->Platform, buffer, &size);

   if (size > 0) {
      recursive = 1;
      buffer[size] = 0;
      output_callback(Task, &Task->OutputCallback, buffer, size);
      recursive = 0;
   }
}

static void task_incoming_stderr(WINHANDLE Handle, objTask *Task)
{
   static UBYTE recursive = 0;

   if (recursive) return;
   if (!Task->Platform) return;

   FMSG("task_stderr()","");

   UBYTE buffer[4096];
   LONG size = sizeof(buffer) - 1;
   winResetStdErr(Task->Platform, buffer, &size);

   if (size > 0) {
      recursive = 1;
      buffer[size] = 0;
      output_callback(Task, &Task->ErrorCallback, buffer, size);
      recursive = 0;
   }
}

//****************************************************************************
// These functions arrange for callbacks to be made whenever one of our process-connected pipes receives data.

void task_register_stdout(objTask *Task, WINHANDLE Handle)
{
   FMSG("task_register_stdout()","Handle: %d", (LONG)Handle);
   RegisterFD(Handle, RFD_READ, (void (*)(void *, void *))&task_incoming_stdout, Task);
}

void task_register_stderr(objTask *Task, WINHANDLE Handle)
{
   FMSG("task_register_stderr()","Handle: %d", (LONG)Handle);
   RegisterFD(Handle, RFD_READ, (void (*)(void *, void *))&task_incoming_stderr, Task);
}

//****************************************************************************

void task_deregister_incoming(WINHANDLE Handle)
{
   RegisterFD(Handle, RFD_REMOVE|RFD_READ|RFD_WRITE|RFD_EXCEPT, NULL, NULL);
}
#endif

/*****************************************************************************
** This function is called when a WIN32 process that we launched has been terminated.
**
** For the linux equivalent, refer to internal.c validate_processID().
*/

#ifdef _WIN32
static void task_process_end(WINHANDLE FD, objTask *Task)
{
   winGetExitCodeProcess(Task->Platform, &Task->ReturnCode);
   if (Task->ReturnCode != 259) {
      Task->ReturnCodeSet = TRUE;
      LogF("~task_process_end","Process %d ended, return code: %d.", (LONG)FD, Task->ReturnCode);
   }
   else LogF("~@task_process_end","Process %d signalled exit too early.", (LONG)FD);

   // Read remaining data

   if (Task->Platform) {
      UBYTE buffer[4096];
      LONG size;

      do {
         size = sizeof(buffer);
         if ((!winReadStd(Task->Platform, TSTD_OUT, buffer, &size)) AND (size)) {
            LogF("task_process_end","Processing %d remaining bytes on stdout.", size);
            output_callback(Task, &Task->OutputCallback, buffer, size);
         }
         else break;
      } while (size IS sizeof(buffer));

      do {
         size = sizeof(buffer);
         if ((!winReadStd(Task->Platform, TSTD_ERR, buffer, &size)) AND (size)) {
            LogF("task_process_end","Processing %d remaining bytes on stderr.", size);
            output_callback(Task, &Task->ErrorCallback, buffer, size);
         }
         else break;
      } while (size IS sizeof(buffer));
   }

   winCloseHandle(FD);

   if (Task->Platform) {
      winFreeProcess(Task->Platform);
      Task->Platform = NULL;
   }

   // Call ExitCallback, if specified

   if (Task->ExitCallback.Type IS CALL_STDC) {
      void (*routine)(objTask *);
      routine = Task->ExitCallback.StdC.Routine;
      routine(Task);
   }
   else if (Task->ExitCallback.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Task->ExitCallback.Script.Script)) {
         const struct ScriptArg args[] = { { "Task", FD_OBJECTPTR, { .Address = Task } } };
         scCallback(script, Task->ExitCallback.Script.ProcedureID, args, ARRAYSIZE(args));
      }
   }

   // Post an event for the task's closure

   evTaskRemoved task_removed = { EVID_SYSTEM_TASK_REMOVED, Task->Head.UniqueID, Task->ProcessID };
   BroadcastEvent(&task_removed, sizeof(task_removed));

   // Send a break if we're waiting for this process to end

   if ((Task->Flags & TSF_WAIT) AND (Task->TimeOut > 0)) SendMessage(0, glProcessBreak, 0, NULL, 0);

   LogBack();
}
#endif

//****************************************************************************

#ifdef _WIN32
void register_process_pipes(objTask *Self, WINHANDLE ProcessHandle)
{
   FMSG("register_pipes()","Process: %d", (LONG)ProcessHandle);

   RegisterFD(ProcessHandle, RFD_READ, (void (*)(void *, void *))&task_process_end, Self);
}

void deregister_process_pipes(objTask *Self, WINHANDLE ProcessHandle)
{
   FMSG("deregister_pipes()","Process: %d", (LONG)ProcessHandle);

   if (ProcessHandle) RegisterFD(ProcessHandle, RFD_REMOVE|RFD_READ|RFD_WRITE|RFD_EXCEPT, NULL, NULL);
}
#endif

//****************************************************************************
// Action interception routine.

static ERROR InterceptedAction(objTask *Self, APTR Args)
{
   if (Self->Actions[tlContext->Action].PerformAction) {
      return Self->Actions[tlContext->Action].PerformAction((OBJECTPTR)Self, Args);
   }
   else return ERR_NoSupport;
}

/*****************************************************************************

-ACTION-
Activate: Activating a task object will execute it.

Activating a task results in the execution of the file referenced in the #Location field.

On successful execution, the ProcessID will refer to the ID of the executed process.  This ID is compatible with the
hosting platform's unique process numbers.

If the WAIT flag is specified, this action will not return until the executed process has returned or the
#TimeOut (if specified) has expired.  Messages are processed as normal during this time, ensuring that your
process remains responsive while waiting.

The process' return code can be read from the #ReturnCode field after the process has completed its execution.

In Microsoft Windows, output can be redirected to a file if the redirection symbol is used to direct output in one of
the task arguments.  For instance `&gt;C:\output.txt` will redirect both stderr and stdout to
`c:\output.txt`.  The use of `1&gt;` to redirect stdout and `2&gt;` to redirect stderr
independently of each other is also acceptable.

When running a DOS program in Microsoft Windows, the SHELL flag can be set in the #Flags field to prevent the
DOS window from appearing.  The DOS window will also be hidden if the stdout or stderr pipes are redirected.

-ERRORS-
Okay
FieldNotSet: The Location field has not been set.
Failed
TimeOut:     Can be returned if the WAIT flag is used.  Indicates that the process was launched, but the timeout expired before the process returned.
-END-

*****************************************************************************/

static ERROR TASK_Activate(objTask *Self, APTR Void)
{
   LONG i, j, k;
   BYTE buffer[1000];
   STRING path, *args;
   ERROR error;
   #ifdef _WIN32
      UBYTE launchdir[500];
      STRING redirect_stdout, redirect_stderr;
      BYTE hide_output;
      LONG winerror;
   #endif
   #ifdef __unix__
      LONG pid, argcount;
      BYTE privileged, shell;
   #endif

   Self->ReturnCodeSet = FALSE;

   // If this is a dummy task object then it is being used during the initialisation sequence, so do nothing.

   if (Self->Flags & TSF_DUMMY) return ERR_Okay;

   if (Self->Flags & TSF_FOREIGN) Self->Flags |= TSF_SHELL;

   if (!Self->Location) return PostError(ERR_FieldNotSet);


#ifdef _WIN32
   //struct taskAddArgument add;
   //StrFormat(buffer, sizeof(buffer), "--instance %d", glInstanceID);
   //add.Argument = buffer;
   //Action(MT_AddTaskArgument, Self, &add);

   // Determine the launch folder

   launchdir[0] = 0;
   if ((!GET_LaunchPath(Self, &path)) AND (path)) {
      if (!ResolvePath(path, RSF_APPROXIMATE|RSF_PATH, &path)) {
         for (i=0; (path[i]) AND (i < sizeof(launchdir)-1); i++) launchdir[i] = path[i];
         launchdir[i] = 0;
         FreeMemory(path);
      }
      else {
         for (i=0; (path[i]) AND (i < sizeof(launchdir)-1); i++) launchdir[i] = path[i];
         launchdir[i] = 0;
      }
   }
   else if (Self->Flags & TSF_RESET_PATH) {
      if (!ResolvePath(Self->Location, RSF_APPROXIMATE|RSF_PATH, &path)) {
         for (i=0; (path[i]) AND (i < sizeof(launchdir)-1); i++) launchdir[i] = path[i];
         FreeMemory(path);
      }
      else for (i=0; (Self->Location[i]) AND (i < sizeof(launchdir)-1); i++) launchdir[i] = Self->Location[i];

      while ((i > 0) AND (launchdir[i] != '\\')) i--;
      launchdir[i] = 0;
   }

   // Resolve the location of the executable (may contain an assignment) and copy it to the command line buffer.

   i = 0;
   buffer[i++] = '"';
   if (!ResolvePath(Self->Location, RSF_APPROXIMATE|RSF_PATH, &path)) {
      for (j=0; (path[j]) AND (i < sizeof(buffer)-1); i++,j++) {
         if (path[j] IS '/') buffer[i] = '\\';
         else buffer[i] = path[j];
      }
      FreeMemory(path);
   }
   else {
      for (j=0; (Self->Location[j]) AND (i < sizeof(buffer)-1); i++,j++) {
         if (Self->Location[j] IS '/') buffer[i] = '\\';
         else buffer[i] = Self->Location[j];
      }
   }
   buffer[i++] = '"';

   // Following the executable path are any arguments that have been used

   redirect_stdout = NULL;
   redirect_stderr = NULL;
   hide_output = FALSE;

   if (!GetField((OBJECTPTR)Self, FID_Parameters|TPTR, &args)) {
      for (j=0; args[j]; j++) {
         if (args[j][0] IS '>') {
            // Redirection argument detected

            if (!ResolvePath(args[j] + 1, RSF_NO_FILE_CHECK, &redirect_stdout)) {
               redirect_stderr = redirect_stdout;
            }

            LogMsg("StdOut/Err redirected to %s", redirect_stdout);

            hide_output = TRUE;
            continue;
         }
         else if ((args[j][0] IS '2') AND (args[j][1] IS '>')) {
            LogMsg("StdErr redirected to %s", args[j] + 2);
            ResolvePath(args[j] + 2, RSF_NO_FILE_CHECK, &redirect_stderr);
            hide_output = TRUE;
            continue;
         }
         else if ((args[j][0] IS '1') AND (args[j][1] IS '>')) {
            LogMsg("StdOut redirected to %s", args[j] + 2);
            ResolvePath(args[j] + 2, RSF_NO_FILE_CHECK, &redirect_stdout);
            hide_output = TRUE;
            continue;
         }

         buffer[i++] = ' ';

         // Check if the argument contains spaces - if so, we need to encapsulate it within quotes.  Otherwise, just copy it as normal.

         for (k=0; (args[j][k]) AND (args[j][k] != ' '); k++);

         if (args[j][k] IS ' ') {
            buffer[i++] = '"';
            for (k=0; args[j][k]; k++) buffer[i++] = args[j][k];
            buffer[i++] = '"';
         }
         else for (k=0; args[j][k]; k++) buffer[i++] = args[j][k];
      }
   }
   buffer[i] = 0;

   // Convert single quotes into double quotes

   BYTE whitespace;
   whitespace = TRUE;
   for (i=0; buffer[i]; i++) {
      if (whitespace) {
         if (buffer[i] IS '"') {
            // Skip everything inside double quotes
            i++;
            while ((buffer[i]) AND (buffer[i] != '"')) i++;
            if (!buffer[i]) break;
            whitespace = FALSE;
            continue;
         }
         else if (buffer[i] IS '\'') {
            for (j=i+1; buffer[j]; j++) {
               if (buffer[j] IS '\'') {
                  if (buffer[j+1] <= 0x20) {
                     buffer[i] = '"';
                     buffer[j] = '"';
                  }
                  i = j;
                  break;
               }
               else if (buffer[j] IS '"') break;
            }
         }
      }

      if (buffer[i] <= 0x20) whitespace = TRUE;
      else whitespace = FALSE;
   }

   MSG("Exec: %s", buffer);

   // Hide window if this is designated a shell program (i.e. hide the DOS window).
   // NB: If you hide a non-shell program, this usually results in the first GUI window that pops up being hidden.

   if (Self->Flags & TSF_SHELL) hide_output = TRUE;

   // Determine whether this new process will be a member of the parent process' group.  This can be forced with the TSF_DETACHED/ATTACHED flags,
   // otherwise it will be determined automatically according to the status of our current task.

   BYTE group;

   if (Self->Flags & TSF_ATTACHED) group = TRUE;
   else if (Self->Flags & TSF_DETACHED) group = FALSE;
   else {
      if (glMasterTask) group = TRUE;
      else group = FALSE;
   }

   LONG internal_redirect = 0;
   if (Self->OutputCallback.Type) internal_redirect |= TSTD_OUT;
   if (Self->ErrorCallback.Type) internal_redirect |= TSTD_ERR;
   if (Self->Flags & TSF_PIPE) internal_redirect |= TSTD_IN;

   if (!(winerror = winLaunchProcess(Self, buffer, (launchdir[0] != 0) ? launchdir : 0, group,
         internal_redirect, &Self->Platform, hide_output, redirect_stdout, redirect_stderr, &Self->ProcessID))) {

      error = ERR_Okay;
      if ((Self->Flags & TSF_WAIT) AND (Self->TimeOut > 0)) {
         LogMsg("Waiting for process to exit.  TimeOut: %.2f sec", Self->TimeOut);

         //if (!glProcessBreak) glProcessBreak = AllocateID(IDTYPE_MESSAGE);
         glProcessBreak = MSGID_BREAK;

         ProcessMessages(0, -1);

         winGetExitCodeProcess(Self->Platform, &Self->ReturnCode);
         if (Self->ReturnCode != 259) Self->ReturnCodeSet = TRUE;
      }
   }
   else {
      UBYTE msg[300];
      winFormatMessage(winerror, msg, sizeof(msg));
      LogErrorMsg("Launch Error: %s", msg);
      error = ERR_Failed;
   }

   if (redirect_stderr IS redirect_stdout) redirect_stderr = NULL;
   if (redirect_stdout) FreeMemory(redirect_stdout);
   if (redirect_stderr) FreeMemory(redirect_stderr);

   return error;

#elif __unix__

   // Add a 'cd' command so that the application starts in its own folder

   path = NULL;
   GET_LaunchPath(Self, &path);

   i = 0;
   if ((Self->Flags & TSF_RESET_PATH) OR (path)) {
      Self->Flags |= TSF_SHELL;

      buffer[i++] = 'c';
      buffer[i++] = 'd';
      buffer[i++] = ' ';

      if (!path) path = Self->Location;
      if (!ResolvePath(path, RSF_APPROXIMATE|RSF_PATH, &path)) {
         for (j=0; (path[j]) AND (i < sizeof(buffer)-1);) buffer[i++] = path[j++];
         FreeMemory(path);
      }
      else {
         for (j=0; (path[j]) AND (i < sizeof(buffer)-1);) buffer[i++] = path[j++];
      }

      while ((i > 0) AND (buffer[i-1] != '/')) i--;
      if (i > 0) {
         buffer[i++] = ';';
         buffer[i++] = ' ';
      }
   }

   // Resolve the location of the executable (may contain an assignment) and copy it to the command line buffer.

   if (!ResolvePath(Self->Location, RSF_APPROXIMATE|RSF_PATH, &path)) {
      for (j=0; (path[j]) AND (i < sizeof(buffer)-1);) buffer[i++] = path[j++];
      buffer[i] = 0;
      FreeMemory(path);
   }
   else {
      for (j=0; (Self->Location[j]) AND (i < sizeof(buffer)-1);) buffer[i++] = Self->Location[j++];
      buffer[i] = 0;
   }

   argcount = 0;
   if (!GetField((OBJECTPTR)Self, FID_Parameters|TPTR, &args)) {
      for (argcount=0; args[argcount]; argcount++);
   }

   STRING argslist[argcount+2];
   LONG bufend;

   bufend = i;

   // Following the executable path are any arguments that have been used. NOTE: This isn't needed if TSF_SHELL is used,
   // however it is extremely useful in the debug printout to see what is being executed.

   if (!GetField((OBJECTPTR)Self, FID_Parameters|TPTR, &args)) {
      for (j=0; args[j]; j++) {
         buffer[i++] = ' ';

         // Check if the argument contains spaces - if so, we need to encapsulate it within quotes.  Otherwise, just
         // copy it as normal.

         for (k=0; (args[j][k]) AND (args[j][k] != ' '); k++);

         if (args[j][k] IS ' ') {
            buffer[i++] = '"';
            for (k=0; args[j][k]; k++) buffer[i++] = args[j][k];
            buffer[i++] = '"';
         }
         else for (k=0; args[j][k]; k++) buffer[i++] = args[j][k];
      }
      buffer[i] = 0;
   }

   // Convert single quotes into double quotes

   for (i=0; buffer[i]; i++) if (buffer[i] IS '\'') buffer[i] = '"';

   LogErrorMsg(buffer);

   // If we're not going to run in shell mode, create an argument list for passing to the program.

   if (!(Self->Flags & TSF_SHELL)) {
      buffer[bufend] = 0;

      argslist[0] = buffer;
      for (i=0; i < argcount; i++) {
         argslist[i+1] = args[i];
      }
      argslist[i+1] = 0;

      if (Self->Flags & TSF_DEBUG) {
         for (i=1; argslist[i]; i++) {
            LogMsg("Arg %d: %s", i, argslist[i]);
         }
      }
   }

   LONG outpipe[2],errpipe[2];
   LONG out_errfd, in_errfd, out_fd, in_fd, input_fd;

   out_errfd = -1;
   out_fd    = -1;
   in_errfd  = -1;
   in_fd     = -1;

   input_fd = open("/dev/null", O_RDONLY); // Input is always NULL, we don't want the child process reading from our own stdin stream

   if (Self->OutputCallback.Type) {
      MSG("Output will be sent to callback.");
      if (!pipe(outpipe)) {
         out_fd = outpipe[1]; // for writing
         in_fd  = outpipe[0]; // for reading
      }
      else {
         LogErrorMsg("Failed to create pipe: %s", strerror(errno));
         if (input_fd != -1) close(input_fd);
         if (out_fd != -1)   close(out_fd);
         return ERR_Failed;
      }
   }

   if ((out_fd IS -1) AND (Self->Flags & TSF_QUIET)) {
      LogMsg("Output will go to NULL");
      out_fd = open("/dev/null", O_RDONLY);
   }

   if (Self->ErrorCallback.Type) {
      MSG("Error output will be sent to a callback.");
      if (!pipe(errpipe)) {
         out_errfd = errpipe[1];
         in_errfd  = errpipe[0];
      }
      else {
         LogErrorMsg("Failed to create pipe: %s", strerror(errno));
         if (input_fd != -1) close(input_fd);
         if (out_fd != -1)   close(out_fd);
         return ERR_Failed;
      }
   }

   if ((out_errfd IS -1) AND (TSF_QUIET)) {
      out_errfd = open("/dev/null", O_RDONLY);
   }

   // Fork a new task.  Remember that forking produces an exact duplicate of the process that made the fork.

   privileged = (Self->Flags & TSF_PRIVILEGED) ? 1 : 0;
   shell = (Self->Flags & TSF_SHELL) ? 1 : 0;

   if (LOCK_PROCESS_TABLE(4000) != ERR_Okay) {
      if (input_fd != -1)  close(input_fd);
      if (out_fd != -1)    close(out_fd);
      if (out_errfd != -1) close(out_errfd);
      if (in_fd != -1)     close(in_fd);
      if (in_errfd != -1)  close(in_errfd);
      return PostError(ERR_SystemLocked);
   }

   pid = fork();

   if (pid IS -1) {
      UNLOCK_PROCESS_TABLE();
      if (input_fd != -1)  close(input_fd);
      if (out_fd != -1)    close(out_fd);
      if (out_errfd != -1) close(out_errfd);
      if (in_fd != -1)     close(in_fd);
      if (in_errfd != -1)  close(in_errfd);
      LogErrorMsg("Failed in an attempt to fork().");
      return ERR_Failed;
   }

   if (pid) {
      // The following code is executed by the initiating process thread

      LogMsg("Created new process %d.  Shell: %d", pid, shell);

      Self->ProcessID = pid; // Record the native process ID

      // Preallocate a task slot for the newly running task.  This allows us to communicate a few things to the new
      // task, such as who the parent is and where data should be output to.

      for (i=0; (i < MAX_TASKS) AND (shTasks[i].ProcessID); i++);

      if (i < MAX_TASKS) {
         shTasks[i].ProcessID    = pid;
         shTasks[i].ParentID     = glCurrentTaskID;
         shTasks[i].CreationTime = PreciseTime() / 1000LL;
         shTasks[i].InstanceID   = glInstanceID;
      }

      UNLOCK_PROCESS_TABLE();

      if (in_fd != -1) {
         RegisterFD(in_fd, RFD_READ, &task_stdout, Self);
         Self->InFD = in_fd;
         close(out_fd);
      }

      if (in_errfd != -1) {
         RegisterFD(in_errfd, RFD_READ, &task_stderr, Self);
         Self->ErrFD = in_errfd;
         close(out_errfd);
      }

      // input_fd has no relevance to the parent process

      if (input_fd != -1) {
         close(input_fd);
         input_fd = -1;
      }

      error = ERR_Okay;
      if (Self->Flags & TSF_WAIT) {
         LogMsg("Waiting for process to turn into a zombie.");

         // Wait for the child process to turn into a zombie.  NB: A parent process or our own child handler may
         // potentially pick this up but that's fine as waitpid() will just fail with -1 in that case.

         LONG status = 0;
         LARGE ticks = PreciseTime() + F2I(Self->TimeOut * 1000000);
         while (!waitpid(pid, &status, WNOHANG)) {
            ProcessMessages(0, 20);

            if ((Self->TimeOut) AND (PreciseTime() >= ticks)) {
               error = PostError(ERR_TimeOut);
               break;
            }
         }

         // Find out what error code was returned

         if (WIFEXITED(status)) {
            Self->ReturnCode = (BYTE)WEXITSTATUS(status);
            Self->ReturnCodeSet = TRUE;
         }
      }

      check_incoming(Self);

      return error;
   }

   // The following code is executed by the newly forked process. Using execl() is the easiest way to clean up after a
   // fork because it will replace the process image, which means we don't have to worry about freeing memory and the
   // like.

   if (input_fd != -1) { // stdin
      close(0);
      dup2(input_fd, 0);
      close(input_fd);
   }

   // Duplicate our parent's output FD's for stdout and stderr

   if (out_fd != -1) { // stdout
      close(1);
      dup2(out_fd, 1);
      close(out_fd);
   }

   if (out_errfd != -1) { // stdin
      close(2);
      dup2(out_errfd, 2);
      close(out_errfd);
   }

   // Close the read-only end of the pipe as it's not relevant to the forked process.

   if (in_fd != -1)    close(in_fd);
   if (in_errfd != -1) close(in_errfd);

   if (!privileged) { // Drop privileges so that the program runs as normal
      seteuid(glUID);
      setegid(glGID);
      setuid(glUID);
      setgid(glGID);
   }

   if (shell) { // For some reason, bash terminates the argument list if it encounters a # symbol, so we'll strip those out.
      for (j=0,i=0; buffer[i]; i++) {
         if (buffer[i] != '#') buffer[j++] = buffer[i];
      }
      buffer[j] = 0;

      execl("/bin/sh", "sh", "-c", buffer, (char *)NULL);
   }
   else execv(buffer, (APTR)&argslist);

   exit(EXIT_FAILURE);
#endif
}

/*****************************************************************************

-METHOD-
AddArgument: Adds new arguments to the Parameters field.

This method provides a simple way of adding new arguments to the #Parameters field.  Provide the value of the
new argument to add it to the end of the list.  If the string is surrounded by quotes, they will be removed
automatically.

-INPUT-
cstr Argument: The argument string that you want to add.

-ERRORS-
Okay
Args
GetField:    The Parameters field could not be retrieved.
AllocMemory: Memory for the new Parameters could not be allocated.

*****************************************************************************/

static ERROR TASK_AddArgument(objTask *Self, struct taskAddArgument *Args)
{
   if ((!Args) OR (!Args->Argument) OR (!*Args->Argument)) return PostError(ERR_NullArgs);

   if (!Self->ParametersMID) {
      CSTRING array[2];
      array[0] = Args->Argument;
      return SetArray((OBJECTPTR)Self, FID_Parameters|TSTR, array, 1);
   }

   if (!Self->Parameters) {
      CSTRING *args;
      if (GetField((OBJECTPTR)Self, FID_Parameters|TPTR, &args) != ERR_Okay) {
         return PostError(ERR_GetField);
      }
      Self->Parameters = args;
   }

   // Calculate the new size of the argument block

   LONG total, len;
   for (len=0; Args->Argument[len]; len++);
   len++;

   MEMORYID argsmid;
   CSTRING *args;
   if (!AllocMemory(Self->ParametersSize + sizeof(STRING) + len, Self->Head.MemFlags|MEM_NO_CLEAR, (void **)&args, &argsmid)) {
      Self->ParametersSize += sizeof(STRING) + len;

      for (total=0; Self->Parameters[total]; total++);

      STRING str = (STRING)(args + total + 2);

      // Copy the old arguments across to the new array

      LONG i, j;
      for (i=0; Self->Parameters[i]; i++) {
         args[i] = str;
         for (j=0; Self->Parameters[i][j]; j++) str[j] = Self->Parameters[i][j];
         str[j++] = 0;
         str += j;
      }

      // Add the new argument.  Notice that we strip enclosing quotes if necessary.

      args[i++] = str;
      args[i]   = NULL;

      CSTRING src = Args->Argument;
      if (*src IS '"') {
         src++;
         while ((*src) AND (*src != '"')) *str++ = *src++;
      }
      else if (*src IS '\'') {
         src++;
         while ((*src) AND (*src != '\'')) *str++ = *src++;
      }
      else while (*src) *str++ = *src++;
      *str = 0;

      ReleaseMemoryID(Self->ParametersMID);
      FreeMemoryID(Self->ParametersMID);

      Self->Parameters    = args;
      Self->ParametersMID = argsmid;
      return ERR_Okay;
   }
   else return PostError(ERR_AllocMemory);
}

/*****************************************************************************

-METHOD-
CloseInstance: Sends a quit message to all tasks running in the current instance.

This method will close all tasks that are running in the current instance by sending them a quit message.  This
includes the process that is making the method call.

-ERRORS-
Okay

*****************************************************************************/

static ERROR TASK_CloseInstance(objTask *Self, APTR Void)
{
   LONG i;
   for (i=0; i < MAX_TASKS; i++) {
      if (shTasks[i].TaskID) SendMessage(shTasks[i].MessageID, MSGID_QUIT, NULL, NULL, NULL);
   }
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
Expunge: Forces a Task to expunge unused code.

The Expunge method releases all loaded libraries that are no longer in use by the active process.

If the Expunge method is called on the System Task, it will message all Tasks to perform the expunge sequence.
The System Task object can be found by searching for the "SystemTask" object.

-ERRORS-
Okay

*****************************************************************************/

static ERROR TASK_Expunge(objTask *Self, APTR Void)
{
   if (Self->Head.UniqueID IS SystemTaskID) {
      if (!LOCK_PROCESS_TABLE(4000)) {
         LONG i;
         for (i=0; i < MAX_TASKS; i++) {
            if ((shTasks[i].TaskID) AND (shTasks[i].TaskID != Self->Head.UniqueID)) {
               ActionMsg(MT_TaskExpunge, shTasks[i].TaskID, NULL, NULL, NULL);
            }
         }
         UNLOCK_PROCESS_TABLE();
      }
   }
   else Expunge(FALSE);

   return ERR_Okay;
}

//****************************************************************************

static ERROR TASK_Free(objTask *Self, APTR Void)
{
#ifdef __unix__
   check_incoming(Self);

   if (Self->InFD != -1) {
      RegisterFD(Self->InFD, RFD_REMOVE, NULL, NULL);
      close(Self->InFD);
      Self->InFD = -1;
   }

   if (Self->ErrFD != -1) {
      RegisterFD(Self->ErrFD, RFD_REMOVE, NULL, NULL);
      close(Self->ErrFD);
      Self->ErrFD = -1;
   }
#endif

#ifdef _WIN32
   if (Self->Env) { FreeMemory(Self->Env); Self->Env = NULL; }
   if (Self->Platform) { winFreeProcess(Self->Platform); Self->Platform = NULL; }
#endif

   // Free variable fields

   LONG i;
   for (i=0; Self->Fields[i]; i++) { FreeMemory(Self->Fields[i]); Self->Fields[i] = NULL; }

   // Free allocations

   if (Self->LaunchPath)  { ReleaseMemoryID(Self->LaunchPathMID);  Self->LaunchPath  = NULL; }
   if (Self->Location)    { ReleaseMemoryID(Self->LocationMID);    Self->Location    = NULL; }
   if (Self->Path)        { ReleaseMemoryID(Self->PathMID);        Self->Path        = NULL; }
   if (Self->ProcessPath) { ReleaseMemoryID(Self->ProcessPathMID); Self->ProcessPath = NULL; }
   if (Self->Parameters)  { ReleaseMemoryID(Self->ParametersMID);  Self->Parameters  = NULL; }
   if (Self->Copyright)   { ReleaseMemoryID(Self->CopyrightMID);   Self->Copyright   = NULL; }

   if (Self->LaunchPathMID)  { FreeMemoryID(Self->LaunchPathMID);  Self->LaunchPathMID  = 0; }
   if (Self->LocationMID)    { FreeMemoryID(Self->LocationMID);    Self->LocationMID    = 0; }
   if (Self->PathMID)        { FreeMemoryID(Self->PathMID);        Self->PathMID        = 0; }
   if (Self->ProcessPathMID) { FreeMemoryID(Self->ProcessPathMID); Self->ProcessPathMID = 0; }
   if (Self->ParametersMID)  { FreeMemoryID(Self->ParametersMID);  Self->ParametersMID  = 0; }
   if (Self->CopyrightMID)   { FreeMemoryID(Self->CopyrightMID);   Self->CopyrightMID   = 0; }
   if (Self->MessageMID)     { FreeMemoryID(Self->MessageMID);     Self->MessageMID     = 0; }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
GetEnv: Retrieves environment variables for the active process.

On platforms that support environment variables, GetEnv() returns the value of the environment variable matching the
Name string.  If there is no matching variable, ERR_DoesNotExist is returned.

In Windows, it is possible to look up registry keys if the string starts with one of the following (in all other
cases, the system's environment variables are queried):

<pre>
\HKEY_LOCAL_MACHINE\
\HKEY_CURRENT_USER\
\HKEY_CLASSES_ROOT\
\HKEY_USERS\
</pre>

Here is a valid example for reading the 'Parasol' key value `\HKEY_CURRENT_USER\Software\Parasol`

Caution: If your programming language uses backslash as an escape character (true for Fluid developers), remember to
use double-backslashes as the key value separator in your Name string.

-INPUT-
cstr Name:  The name of the environment variable to retrieve.
&cstr Value: The value of the environment variable is returned in this parameter.

-ERRORS-
Okay
Args
DoesNotExist: The environment variable is undefined.
NoSupport: The platform does not support environment variables.
-END-

*****************************************************************************/

static ERROR TASK_GetEnv(objTask *Self, struct taskGetEnv *Args)
{
   if ((!Args) OR (!Args->Name)) return PostError(ERR_NullArgs);

#ifdef _WIN32

   #define ENV_SIZE 4096
   LONG len;

   Args->Value = NULL;

   if (glCurrentTask != Self) return ERR_Failed;

   if (!Self->Env) {
      if (AllocMemory(ENV_SIZE, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (APTR *)&Self->Env, NULL) != ERR_Okay) {
         return ERR_AllocMemory;
      }
   }

   if (Args->Name[0] IS '\\') {
      struct {
         ULONG ID;
         CSTRING HKey;
      } keys[] = {
         { HKEY_LOCAL_MACHINE,  "\\HKEY_LOCAL_MACHINE\\" },
         { HKEY_CURRENT_USER,   "\\HKEY_CURRENT_USER\\" },
         { HKEY_CLASSES_ROOT,   "\\HKEY_CLASSES_ROOT\\" },
         { HKEY_USERS,          "\\HKEY_USERS\\" },
         { 0, 0 }
      };

      LONG i, ki;
      for (ki=0; ki < ARRAYSIZE(keys); ki++) {
         if (!StrCompare(keys[ki].HKey, Args->Name, 0, 0)) {
            for (len=0; keys[ki].HKey[len]; len++);

            CSTRING str = Args->Name + len; // str = Parasol\Something
            for (len=0; str[len]; len++); // End of string

            while (len > 0) {
               if (str[len] IS '\\') break;
               len--;
            }

            if (len > 0) {
               UBYTE path[len];
               for (i=0; i < len; i++) path[i] = str[i];
               path[i] = 0;

               APTR keyhandle;
               if (!RegOpenKeyExA(keys[ki].ID, path, 0, KEY_READ, &keyhandle)) {
                  LONG type;

                  len = ENV_SIZE;
                  if (!RegQueryValueExA(keyhandle, str+i+1, 0, &type, Self->Env, &len)) {
                     // Numerical registry types can be converted into strings

                     switch(type) {
                        case REG_DWORD:     IntToStr(((LONG *)Self->Env)[0], Self->Env, ENV_SIZE); break;
                        case REG_DWORD_BIG_ENDIAN: IntToStr(((LONG *)Self->Env)[0], Self->Env, ENV_SIZE); break; // Not quite right... we should convert the endianness first.
                        case REG_QWORD:     IntToStr(((LARGE *)Self->Env)[0], Self->Env, ENV_SIZE); break;
                     }

                     Args->Value = Self->Env;
                  }
                  winCloseHandle(keyhandle);
               }

               if (Args->Value) return ERR_Okay;
               else return ERR_DoesNotExist;
            }
            else return PostError(ERR_Syntax);
         }
      }
   }

   len = winGetEnv(Args->Name, Self->Env, ENV_SIZE);
   if (!len) return ERR_DoesNotExist;
   if (len >= ENV_SIZE) return PostError(ERR_BufferOverflow);

   Args->Value = Self->Env;
   return ERR_Okay;

#elif __unix__
   if ((Args->Value = getenv(Args->Name))) {
      return ERR_Okay;
   }
   else return ERR_DoesNotExist;
#else
   #warn Write support for GetEnv()
   return ERR_NoSupport;
#endif
}

/*****************************************************************************

-METHOD-
SetEnv: Sets environment variables for the active process.

On platforms that support environment variables, SetEnv() is used for defining values for named variables.  A Name and
accompanying Value string are required.  If the Value is NULL, the environment variable is removed if it already exists.

In Windows, it is possible to set registry keys if the string starts with one of the following (in all other cases, the
system's environment variables are queried):

<pre>
\HKEY_LOCAL_MACHINE\
\HKEY_CURRENT_USER\
\HKEY_CLASSES_ROOT\
\HKEY_USERS\
</pre>

When setting a registry key, the function will always set the Value as a string type unless the key already exists.  If
the existing key value is a number such as DWORD or QWORD, then the Value will be converted to an integer before the
key is set.

-INPUT-
cstr Name:  The name of the environment variable to set.
cstr Value: The value to assign to the environment variable.

-ERRORS-
Okay
Args
NoSupport: The platform does not support environment variables.
-END-

*****************************************************************************/

static ERROR TASK_SetEnv(objTask *Self, struct taskSetEnv *Args)
{
   if ((!Args) OR (!Args->Name)) return PostError(ERR_NullArgs);

#ifdef _WIN32

   if (Args->Name[0] IS '\\') {
      LONG i, ki, len;
      const struct {
         ULONG ID;
         CSTRING HKey;
      } keys[] = {
         { HKEY_LOCAL_MACHINE,  "\\HKEY_LOCAL_MACHINE\\" },
         { HKEY_CURRENT_USER,   "\\HKEY_CURRENT_USER\\" },
         { HKEY_CLASSES_ROOT,   "\\HKEY_CLASSES_ROOT\\" },
         { HKEY_USERS,          "\\HKEY_USERS\\" },
         { 0, 0 }
      };

      LogMsg("Registry: %s = %s", Args->Name, Args->Value);

      for (ki=0; ki < ARRAYSIZE(keys); ki++) {
         if (!StrCompare(keys[ki].HKey, Args->Name, 0, 0)) {
            for (len=0; keys[ki].HKey[len]; len++);

            CSTRING str = Args->Name + len;     // str = Parasol\Something
            for (len=0; str[len]; len++); // End of string

            while (len > 0) {
               if (str[len] IS '\\') break;
               len--;
            }

            if (len > 0) {
               UBYTE path[len];
               for (i=0; i < len; i++) path[i] = str[i];
               path[i] = 0;

               APTR keyhandle;
               if (!RegOpenKeyExA(keys[ki].ID, path, 0, KEY_READ|KEY_WRITE, &keyhandle)) {
                  LONG type;

                  if (!RegQueryValueExA(keyhandle, str+i+1, 0, &type, NULL, NULL)) {
                     // Numerical registry types can be converted into strings

                     switch(type) {
                        case REG_DWORD: {
                           LONG int32 = StrToInt(Args->Value);
                           RegSetValueExA(keyhandle, str+i+1, 0, REG_DWORD, &int32, sizeof(int32));
                           break;
                        }

                        case REG_QWORD: {
                           LARGE int64 = StrToInt(Args->Value);
                           RegSetValueExA(keyhandle, str+i+1, 0, REG_QWORD, &int64, sizeof(int64));
                           break;
                        }

                        default: {
                           RegSetValueExA(keyhandle, str+i+1, 0, REG_SZ, Args->Value, StrLength(Args->Value)+1);
                        }
                     }
                  }
                  else {
                     RegSetValueExA(keyhandle, str+i+1, 0, REG_SZ, Args->Value, StrLength(Args->Value)+1);
                  }

                  winCloseHandle(keyhandle);
               }

               return ERR_Okay;
            }
            else return PostError(ERR_Syntax);
         }
      }

      return PostError(ERR_Failed);
   }
   else {
      winSetEnv(Args->Name, Args->Value);
      return ERR_Okay;
   }

#elif __unix__

   if (Args->Value) setenv(Args->Name, Args->Value, 1);
   else unsetenv(Args->Name);
   return ERR_Okay;

#else

   #warn Write support for SetEnv()
   return ERR_NoSupport;

#endif
}

/*****************************************************************************
-ACTION-
GetVar: Retrieves variable field values.
-END-
*****************************************************************************/

static ERROR TASK_GetVar(objTask *Self, struct acGetVar *Args)
{
   LONG i, j;

   if ((!Args) OR (!Args->Buffer)) return PostError(ERR_NullArgs);

   for (i=0; Self->Fields[i]; i++) {
      if (!StrCompare(Args->Field, Self->Fields[i], NULL, STR_MATCH_LEN)) {
         STRING fieldvalue;
         for (fieldvalue=Self->Fields[i]; *fieldvalue; fieldvalue++);
         fieldvalue++;

         for (j=0; (fieldvalue[j]) AND (j < Args->Size-1); j++) Args->Buffer[j] = fieldvalue[j];
         Args->Buffer[j++] = 0;

         if (j >= Args->Size) return ERR_BufferOverflow;
         else return ERR_Okay;
      }
   }

   LogErrorMsg("The variable \"%s\" does not exist.", Args->Field);

   return ERR_Okay;
}

//****************************************************************************

static ERROR TASK_Init(objTask *Self, APTR Void)
{
   struct MessageHeader *msgblock;
   LONG i, len;

   if (Self->Head.UniqueID IS SystemTaskID) {
      // Perform the following if this is the System Task
      Self->ProcessID = 0;
   }
   else if ((!glCurrentTaskID) OR (glCurrentTaskID IS SystemTaskID)) {
      // Perform the following if this is a Task representing the current process

      Self->ProcessID = glProcessID;

      glCurrentTaskID = Self->Head.UniqueID;
      glCurrentTask   = Self;

      // Allocate the message block for this Task

      if (!AllocMemory(sizeof(struct MessageHeader), MEM_PUBLIC, (void **)&msgblock, &glTaskMessageMID)) {
         Self->MessageMID = glTaskMessageMID;
         msgblock->TaskIndex = glTaskEntry->Index;
         ReleaseMemoryID(glTaskMessageMID);
      }
      else return ERR_AllocMemory;

      // Refer to the task object ID in the system list

      if (!LOCK_PROCESS_TABLE(4000)) {
         glTaskEntry->TaskID = Self->Head.UniqueID;
         glTaskEntry->MessageID = glTaskMessageMID;
         UNLOCK_PROCESS_TABLE();
      }

#ifdef _WIN32
      UBYTE buffer[300];
      if (winGetExeDirectory(sizeof(buffer), buffer)) {
         for (len=0; buffer[len]; len++);
         while ((len > 1) AND (buffer[len-1] != '/') AND (buffer[len-1] != '\\') AND (buffer[len-1] != ':')) len--;
         if (!AllocMemory(len+1, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (void **)&Self->ProcessPath, &Self->ProcessPathMID)) {
            for (i=0; i < len; i++) Self->ProcessPath[i] = buffer[i];
            Self->ProcessPath[i] = 0;
         }
      }

      if ((len = winGetCurrentDirectory(sizeof(buffer), buffer))) {
         if (!AllocMemory(len+2, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (void **)&Self->Path, &Self->PathMID)) {
            for (i=0; i < len; i++) Self->Path[i] = buffer[i];
            if (Self->Path[i-1] != '\\') Self->Path[i++] = '\\';
            Self->Path[i] = 0;
         }
      }

#elif __unix__

         char buffer[256], procfile[50];

         // This method of path retrieval only works on Linux (most types of Unix don't provide any support for this).

         LONG pos = 0;
         for (i=0; "/proc/"[i]; i++) procfile[pos++] = "/proc/"[i];
         pos += IntToStr(glProcessID, procfile+pos, 20);
         for (i=0; "/exe"[i]; i++) procfile[pos++] = "/exe"[i];
         procfile[pos] = 0;

         buffer[0] = 0;
         if ((i = readlink(procfile, buffer, sizeof(buffer)-1)) > 0) {
            buffer[i] = 0;
            while (i > 0) { // Strip the process name
               if (buffer[i] IS '/') {
                  buffer[i+1] = 0;
                  break;
               }
               i--;
            }

            for (len=0; buffer[len]; len++);
            while ((len > 1) AND (buffer[len-1] != '/') AND (buffer[len-1] != '\\') AND (buffer[len-1] != ':')) len--;
            if (!AllocMemory(len+1, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (void **)&Self->ProcessPath, &Self->ProcessPathMID)) {
               for (i=0; i < len; i++) Self->ProcessPath[i] = buffer[i];
               Self->ProcessPath[i] = 0;
            }
         }

         if (!Self->PathMID) { // Set the working folder
            if (getcwd(buffer, sizeof(buffer))) {
               for (len=0; buffer[len]; len++);
               if (!AllocMemory(len+2, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (void **)&Self->Path, &Self->PathMID)) {
                  for (i=0; buffer[i]; i++) Self->Path[i] = buffer[i];
                  Self->Path[i++] = '/';
                  Self->Path[i] = 0;
               }
            }
         }
#endif

      LogMsg("Process Path: %s", Self->ProcessPath);
      LogMsg("Working Path: %s", Self->Path);
   }
   else if (Self->ProcessID) {
      // The process ID has been preset - this means that the task could represent a link to an existing Parasol
      // process, or to a foreign process.

      for (i=0; i < MAX_TASKS; i++) {
         if ((shTasks[i].TaskID) AND (shTasks[i].ProcessID IS Self->ProcessID)) {
            LogMsg("Connected process %d to task %d, message port %d.", Self->ProcessID, shTasks[i].TaskID, shTasks[i].MessageID);
            Self->MessageMID = shTasks[i].MessageID;
            break;
         }
      }

      if (i >= MAX_TASKS) Self->Flags |= TSF_FOREIGN;
   }

   return ERR_Okay;
}

/*****************************************************************************
** Task: NewObject
*/

static ERROR TASK_NewObject(objTask *Self, APTR Void)
{
#ifdef __unix__
   Self->InFD = -1;
   Self->ErrFD = -1;
#endif
   Self->TimeOut = 60 * 60 * 24;
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
Quit: Sends a quit message to a task.

The Quit method can be used as a convenient way of sending a task a quit message.  This will normally result in the
destruction of the task, so long as it is still functioning correctly and has been coded to respond to the
MSGID_QUIT message type.  It is legal for a task to ignore a quit request if it is programmed to stay alive.  A task
can be killed outright with the Free action.

-ERRORS-
Okay
-END-

*****************************************************************************/

static ERROR TASK_Quit(objTask *Self, APTR Void)
{
   if ((Self->ProcessID) AND (Self->ProcessID != glProcessID)) {
      LogMsg("Terminating foreign process %d", Self->ProcessID);

      #ifdef __unix__
         kill(Self->ProcessID, SIGHUP); // Safe kill signal - this actually results in that process generating an internal MSGID_QUIT message
      #elif _WIN32
         winTerminateApp(Self->ProcessID, 1000);
      #else
         #warning Add code to kill foreign processes.
      #endif
   }
   else if (Self->MessageMID) {
      LogMethod("Sending quit message to queue %d.", Self->MessageMID);
      if (!SendMessage(Self->MessageMID, MSGID_QUIT, NULL, NULL, NULL)) {
         return ERR_Okay;
      }
   }
   else LogErrorMsg("Task is not linked to a message queue or process.");

   return ERR_Okay;
}

//****************************************************************************

static ERROR TASK_ReleaseObject(objTask *Self, APTR Void)
{
   if (Self->LaunchPath)  { ReleaseMemoryID(Self->LaunchPathMID);  Self->LaunchPath = NULL; }
   if (Self->Location)    { ReleaseMemoryID(Self->LocationMID);    Self->Location  = NULL; }
   if (Self->Parameters)  { ReleaseMemoryID(Self->ParametersMID);  Self->Parameters  = NULL; }
   if (Self->Copyright)   { ReleaseMemoryID(Self->CopyrightMID);   Self->Copyright = NULL; }
   if (Self->Path)        { ReleaseMemoryID(Self->PathMID);        Self->Path      = NULL; }
   if (Self->ProcessPath) { ReleaseMemoryID(Self->ProcessPathMID); Self->ProcessPath = NULL; }
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
SetVar: Variable fields are supported for the general storage of program variables.
-END-
*****************************************************************************/

static ERROR TASK_SetVar(objTask *Self, struct acSetVar *Args)
{
   if ((!Args) OR (!Args->Field) OR (!Args->Value)) return ERR_NullArgs;

   // Find the insertion point

   LONG i;
   for (i=0; Self->Fields[i]; i++) {
      if (!StrMatch(Args->Field, Self->Fields[i])) {
         break;
      }
   }

   if (i < ARRAYSIZE(Self->Fields) - 1) {
      STRING field;
      if (!AllocMemory(StrLength(Args->Field) + StrLength(Args->Value) + 2,
            MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (void **)&field, NULL)) {

         LONG pos = StrCopy(Args->Field, field, COPY_ALL) + 1;
         StrCopy(Args->Value, field + pos, COPY_ALL);

         if (Self->Fields[i]) FreeMemory(Self->Fields[i]);
         Self->Fields[i] = field;

         return ERR_Okay;
      }
      else return PostError(ERR_AllocMemory);
   }
   else return PostError(ERR_ArrayFull);
}

/*****************************************************************************

-ACTION-
Write: Send raw data to a launched process' stdin descriptor.

After a process has been launched, data can be sent to its stdin pipe by calling the Write action.  Setting the Buffer
parameter to NULL will result in the pipe being closed (this will signal to the process that no more data is incoming).

*****************************************************************************/

static ERROR TASK_Write(objTask *Task, struct acWrite *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

#ifdef _WIN32
   LONG winerror;
   if (!(winerror = winWriteStd(Task->Platform, Args->Buffer, Args->Length))) {
      return ERR_Okay;
   }
   else return ERR_Failed;
#else
   return PostError(ERR_NoSupport);
#endif
}

/*****************************************************************************

-FIELD-
Actions: Used to gain direct access to a task's actions.

This field provides direct access to the actions of a task.  You can use it in the development of an executable program
to hook into the Core action system.  This allows you to create a program that blends in seamlessly with the
system's object oriented design.  In some cases this is a necessity, for example, use of some functions will require you
to hook into the ActionNotify action.

The Actions field itself points to a list of action routines that are arranged into a lookup table, sorted by action ID.
You can hook into an action simply by writing to its index in the table with a pointer to the routine that you want to
use for that action.  For example:

<pre>
if (!AccessObject(CurrentTask(), 5000, &task)) {
   GetPointer(task, FID_Actions, &amp;actions);
   actions[AC_ActionNotify] = PROGRAM_ActionNotify;
   ReleaseObject(task);
}
</pre>

The synopsis of the routines that you use for hooking into the action list must match
`ERROR PROGRAM_ActionNotify(*Task, APTR Args)`.

It is recommended that you refer to the Action Support Guide before hooking into any action that you have not written
code for before.

*****************************************************************************/

static ERROR GET_Actions(objTask *Self, struct ActionEntry **Value)
{
   *Value = Self->Actions;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Args: Command line arguments (string format).

If you would like to set the command line arguments for a new task using a single string, define the arguments through
this field.  The string that you pass will be disassembled and the arguments will be added to the #Parameters
field.

*****************************************************************************/

static ERROR SET_Args(objTask *Self, CSTRING Value)
{
   LONG i;

   if ((!Value) OR (!*Value)) return ERR_Okay;

   while (*Value) {
      while (*Value IS ' ') Value++;
      if (*Value) {
         // Extract the argument

         char buffer[400];
         for (i=0; (*Value) AND (*Value != ' ') AND (i < sizeof(buffer)-1);) {
            if (*Value IS '"') {
               Value++;
               while ((i < sizeof(buffer)-1) AND (*Value) AND (*Value != '"')) {
                  buffer[i++] = *Value++;
               }
               if (*Value IS '"') Value++;
            }
            else buffer[i++] = *Value++;
         }
         buffer[i] = 0;

         if (*Value) while (*Value != ' ') Value++;

         // Set the argument

         struct taskAddArgument add = { .Argument = buffer };
         Action(MT_TaskAddArgument, &Self->Head, &add);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Parameters: Command line arguments (list format).

If a program is written to accept user arguments, then this field is the place to obtain them.  The arguments are
listed in a string based array that is terminated with a NULL pointer. Thus if the user enters the following into
a command-line:

<pre>1&gt; YourProgram PREFS MyPrefs -file "documents:readme.txt"</pre>

Then you will get:

<pre>
CSTRING Args[] = {
   "PREFS",
   "MyPrefs",
   "-file",
   "documents:readme.txt",
   NULL
};
</pre>

If the user did not supply any command line arguments, this field will be set to NULL.

*****************************************************************************/

static ERROR GET_Parameters(objTask *Self, CSTRING **Value, LONG *Elements)
{
   if (Self->Parameters) {
      *Value = Self->Parameters;
      *Elements = 0;
      return ERR_Okay;
   }
   else if (!Self->ParametersMID) {
      LogMsg("No arguments to return.");
      *Value = NULL;
      *Elements = 0;
      return ERR_FieldNotSet;
   }
   else if (!AccessMemory(Self->ParametersMID, MEM_READ, 2000, (void **)&Self->Parameters)) {
      *Value = Self->Parameters;
      // Resolve string addresses

      LONG i;
      for (i=0; Self->Parameters[i]; i++);
      *Elements = i;

      CSTRING args = (CSTRING)(Self->Parameters + i + 1);

      for (i=0; Self->Parameters[i]; i++) {
         Self->Parameters[i] = args;
         while (*args) args++;
         args++;
      }

      return ERR_Okay;
   }
   else {
      *Value = NULL;
      *Elements = 0;
      return PostError(ERR_AccessMemory);
   }
}

static ERROR SET_Parameters(objTask *Self, CSTRING *Value, LONG Elements)
{
   if (Self->Parameters)    { ReleaseMemoryID(Self->ParametersMID); Self->Parameters = NULL; }
   if (Self->ParametersMID) { FreeMemoryID(Self->ParametersMID);    Self->ParametersMID = NULL; }

   if (Value) {
      // Calculate the size of the argument array and strings tacked onto the end

      LONG i, j;
      Self->ParametersSize = sizeof(STRING); // Null-terminated array entry
      for (j=0; j < Elements; j++) {
         if (!Value[j]) { Elements = j; break; }
         Self->ParametersSize += sizeof(STRING); // Array entry
         for (i=0; Value[j][i]; i++) Self->ParametersSize++; // String length
         Self->ParametersSize++; // String null terminator
      }

      if (!AllocMemory(Self->ParametersSize, MEM_NO_CLEAR|Self->Head.MemFlags, (void **)&Self->Parameters, &Self->ParametersMID)) {
         STRING args = (STRING)(Self->Parameters + j + 1);
         for (j=0; j < Elements; j++) {
            Self->Parameters[j] = args;
            for (i=0; Value[j][i]; i++) args[i] = Value[j][i];
            args[i++] = 0;
            args += i;
         }
         Self->Parameters[j] = 0;
      }
      else return PostError(ERR_AllocMemory);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Author: Describes the person that wrote the program.

This field gives information about the author of the program/task. If the author is not determinable from the
#Location, this field will usually be set to NULL.

*****************************************************************************/

static ERROR GET_Author(objTask *Self, STRING *Value)
{
   *Value = Self->Author;
   return ERR_Okay;
}

static ERROR SET_Author(objTask *Self, CSTRING Value)
{
   StrCopy(Value, Self->Author, sizeof(Self->Author));
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Copyright: Copyright/licensing details.

*****************************************************************************/

static ERROR GET_Copyright(objTask *Self, STRING *Value)
{
   if (Self->Copyright) {
      *Value = Self->Copyright;
      return ERR_Okay;
   }
   else if (!Self->CopyrightMID) {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
   else if (!AccessMemory(Self->CopyrightMID, MEM_READ, 2000, (void **)&Self->Copyright)) {
      *Value = Self->Copyright;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return PostError(ERR_AccessMemory);
   }
}

static ERROR SET_Copyright(objTask *Self, CSTRING Value)
{
   LONG i;

   if (Self->Copyright)    { ReleaseMemoryID(Self->CopyrightMID);   Self->Copyright = NULL; }
   if (Self->CopyrightMID) { FreeMemoryID(Self->CopyrightMID); Self->CopyrightMID = NULL; }

   if ((Value) AND (*Value)) {
      for (i=0; Value[i]; i++);
      if (!AllocMemory(i+1, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (void **)&Self->Copyright, &Self->CopyrightMID)) {
         for (i=0; Value[i]; i++) Self->Copyright[i] = Value[i];
         Self->Copyright[i] = 0;
      }
      else return PostError(ERR_AllocMemory);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Date: The date that the program was last updated or compiled.

The Date usually specifies the date on which the program was compiled for public release. It is up to the developer of
the program to set this string correctly and keep it current.

The correct specification for this string is `Day Month Year` or `Month Year` as in the following
examples:

<pre>
"14 February 1998"
"6 May 1997"
"January 2000"
</pre>

Please do not use shorthand dates such as `14/2/98`.  Do not include the time or any other information besides that
which is outlined here.

****************************************************************************/

static ERROR GET_Date(objTask *Self, STRING *Value)
{
   *Value = Self->Date;
   return ERR_Okay;
}

static ERROR SET_Date(objTask *Self, CSTRING Value)
{
   StrCopy(Value, Self->Date, sizeof(Self->Date));
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
ExitCallback: The callback is activated when the process is terminated.

The ExitCallback field can be set with a function reference that will be called when the executed process is
terminated.  The callback must follow the synopsis `Function(*Task)`.

Please keep in mind that if the Task is freed when the process is still executing, the ExitCallback routine will not be
called on termination because the Task object no longer exists for the control of the process.

*****************************************************************************/

static ERROR GET_ExitCallback(objTask *Self, FUNCTION **Value)
{
   if (Self->ExitCallback.Type != CALL_NONE) {
      *Value = &Self->ExitCallback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_ExitCallback(objTask *Self, FUNCTION *Value)
{
   if (Value) Self->ExitCallback = *Value;
   else Self->ExitCallback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
ErrorCallback: This callback returns incoming data from STDERR.

The ErrorCallback field can be set with a function reference that will be called when an active process sends data via
STDERR.  The callback must follow the synopsis `Function(*Task, APTR Data, LONG Size)`

The information read from STDERR will be returned in the Data pointer and the byte-length of the data will be
indicated by the Size.  The data pointer is temporary and will be invalid once the callback function has returned.

*****************************************************************************/

static ERROR GET_ErrorCallback(objTask *Self, FUNCTION **Value)
{
   if (Self->ErrorCallback.Type != CALL_NONE) {
      *Value = &Self->ErrorCallback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_ErrorCallback(objTask *Self, FUNCTION *Value)
{
   if (Value) Self->ErrorCallback = *Value;
   else Self->ErrorCallback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
OutputCallback: This callback returns incoming data from STDOUT.

The OutputCallback field can be set with a function reference that will be called when an active process sends data via
STDOUT.  The callback must follow the synopsis `Function(*Task, APTR Data, LONG Size)`

The information read from STDOUT will be returned in the Data pointer and the byte-length of the data will be indicated
by the Size.  The data pointer is temporary and will be invalid once the callback function has returned.

*****************************************************************************/

static ERROR GET_OutputCallback(objTask *Self, FUNCTION **Value)
{
   if (Self->OutputCallback.Type != CALL_NONE) {
      *Value = &Self->OutputCallback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_OutputCallback(objTask *Self, FUNCTION *Value)
{
   if (Value) Self->OutputCallback = *Value;
   else Self->OutputCallback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Flags: Optional flags.
Lookup: TSF

-FIELD-
LaunchPath: Launched executables will start in the path specified here.

Use the LaunchPath field to specify the folder that a launched executable will start in when the task object is
activated.  This will override all other path options, such as the RESET_PATH flag.

*****************************************************************************/

static ERROR GET_LaunchPath(objTask *Self, STRING *Value)
{
   if (Self->LaunchPath) {
      *Value = Self->LaunchPath;
      return ERR_Okay;
   }
   else if (!Self->LaunchPathMID) {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
   else if (!AccessMemory(Self->LaunchPathMID, MEM_READ, 2000, (void **)&Self->LaunchPath)) {
      *Value = Self->LaunchPath;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return PostError(ERR_AccessMemory);
   }
}

static ERROR SET_LaunchPath(objTask *Self, CSTRING Value)
{
   if (Self->LaunchPath)    { ReleaseMemoryID(Self->LaunchPathMID);   Self->LaunchPath = NULL; }
   if (Self->LaunchPathMID) { FreeMemoryID(Self->LaunchPathMID); Self->LaunchPathMID = NULL; }

   if ((Value) AND (*Value)) {
      LONG i;
      for (i=0; Value[i]; i++);
      if (!AllocMemory(i+1, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (void **)&Self->LaunchPath, &Self->LaunchPathMID)) {
         CopyMemory(Value, Self->LaunchPath, i+1);
      }
      else return PostError(ERR_AllocMemory);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Location: Location to load an executable file from.

When a task object is activated, the Location field will be checked for a valid filename.  If the path is valid, the
executable code will be loaded from this source.  The source must be in an executable format recognised by the
native platform.

Leading spaces will be ignored by the string parser.  The Location string can be enclosed with quotes, in which case
only the quoted portion of the string will be used as the source path.

*****************************************************************************/

static ERROR GET_Location(objTask *Self, STRING *Value)
{
   if (Self->Location) {
      *Value = Self->Location;
      return ERR_Okay;
   }
   else if (!Self->LocationMID) {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
   else if (!AccessMemory(Self->LocationMID, MEM_READ, 2000, (void **)&Self->Location)) {
      *Value = Self->Location;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return PostError(ERR_AccessMemory);
   }
}

static ERROR SET_Location(objTask *Self, CSTRING Value)
{
   if (Self->Location)    { ReleaseMemoryID(Self->LocationMID);   Self->Location = NULL; }
   if (Self->LocationMID) { FreeMemoryID(Self->LocationMID); Self->LocationMID = NULL; }

   if ((Value) AND (*Value)) {
      LONG i;
      for (i=0; Value[i]; i++);
      if (!AllocMemory(i+1, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (void **)&Self->Location, &Self->LocationMID)) {
         while ((*Value) AND (*Value <= 0x20)) Value++;
         if (*Value IS '"') {
            Value++;
            i = 0;
            while ((*Value) AND (*Value != '"')) Self->Location[i++] = *Value++;
         }
         else for (i=0; *Value; i++) Self->Location[i] = *Value++;
         Self->Location[i] = 0;
      }
      else return PostError(ERR_AllocMemory);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Instance: The instance ID that the process belongs to.

All tasks that use the Parasol API belong to a run-time instance that can host multiple processes.  Tasks that share
the same instance ID can easily communicate with each other, while those that do not will be in a separate namespace.

It is not possible to change the instance ID once the process has started.  New processes can be assigned an instance
ID on creation with the `--instance` commandline argument.  By default, any new process will share the same
instance ID as its creator.

*****************************************************************************/

static ERROR GET_Instance(objTask *Self, LONG *Value)
{
   *Value = glInstanceID;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
MessageQueue: Read this field to acquire a task's message queue ID.

If you need the ID of a task's message queue, read this field to obtain it.  Once you have a task's message queue ID,
you can use it to send messages to the task.  For information on messaging, refer to the ~GetMessage(),
~ProcessMessages() and ~SendMessage() functions.

*****************************************************************************/

static ERROR GET_MessageQueue(objTask *Task, MEMORYID *Value)
{
   if ((*Value = glTaskMessageMID)) return ERR_Okay;
   else return ERR_NoData;
}

/*****************************************************************************

-FIELD-
Name: Name of the task.

This field specifies the name of the Task or program that has been initialised. It is up to the developer of the
program to set the Name which will appear in this field.  If there is no name for the task then the system may
assign a randomly generated name.

*****************************************************************************/

static ERROR GET_Name(objTask *Self, STRING *Value)
{
   *Value = Self->Name;
   return ERR_Okay;
}

static ERROR SET_Name(objTask *Self, CSTRING Value)
{
   StrCopy(Value, Self->Name, sizeof(Self->Name));
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TimeOut: Limits the amount of time to wait for a launched process to return.

This field can be set in conjunction with the WAIT flag to define the time limit when waiting for a launched
process to return.  The time out is defined in seconds.

-FIELD-
ProcessID: Reflects the process ID when an executable is launched.

If a task object launches an executable file via Activate(), the ProcessID will be set to the 'pid' that was assigned
to the new process by the host system.  At all other times the ProcessID is set to zero.

-FIELD-
Path: The current working folder of the active process.

The Path specifies the 'working folder' that determines where files are loaded from when an absolute path is not
otherwise specified for file access.  Initially the working folder is usually set to the folder of the parent
process, such as that of a terminal shell.

The working folder can be changed at any time by updating the Path with a new folder location.

*****************************************************************************/

static ERROR GET_Path(objTask *Self, STRING *Value)
{
   if (Self->Path) {
      *Value = Self->Path;
      return ERR_Okay;
   }
   else if (!Self->PathMID) {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
   else if (!AccessMemory(Self->PathMID, MEM_READ, 2000, (void **)&Self->Path)) {
      *Value = Self->Path;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return PostError(ERR_AccessMemory);
   }
}

static ERROR SET_Path(objTask *Self, CSTRING Value)
{
   if (Self->Path)    { ReleaseMemoryID(Self->PathMID); Self->Path = NULL; }
   if (Self->PathMID) { FreeMemoryID(Self->PathMID); Self->PathMID = 0; }

   LogMsg("New Path: %s", Value);

   if ((Value) AND (*Value)) {
      LONG len, j;
      for (len=0; Value[len]; len++);
      while ((len > 1) AND (Value[len-1] != '/') AND (Value[len-1] != '\\') AND (Value[len-1] != ':')) len--;
      if (!AllocMemory(len+1, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (void **)&Self->Path, &Self->PathMID)) {
         for (j=0; j < len; j++) Self->Path[j] = Value[j];
         Self->Path[j] = 0;

#ifdef __unix__
         STRING path;
         if (!ResolvePath(Self->Path, RSF_NO_FILE_CHECK, &path)) {
            int unused;
            unused = chdir(path);
            FreeMemory(path);
         }
         else LogErrorMsg("Failed to resolve path \"%s\"", Self->Path);
#elif _WIN32
         STRING path;
         if (!ResolvePath(Self->Path, RSF_NO_FILE_CHECK|RSF_PATH, &path)) {
            LONG result = chdir(path);
            if (result) LogErrorMsg("Failed to switch current path to: %s", path);
            FreeMemory(path);

            if (result) return ERR_InvalidPath;
         }
         else return PostError(ERR_InvalidPath);
#else
#warn Support required for changing the current path.
#endif
      }
      else return PostError(ERR_AllocMemory);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
ProcessPath: The path of the executable that is associated with the task.

The ProcessPath is set to the path of the executable file that is associated with the task.  It is managed internally
and cannot be altered.

In Microsoft Windows it is not always possible to determine the origins of an executable, in which case the
ProcessPath is set to the working folder in use at the time the process was launched.

*****************************************************************************/

static ERROR GET_ProcessPath(objTask *Self, CSTRING *Value)
{
   if (Self->ProcessPath) {
      *Value = Self->ProcessPath;
      return ERR_Okay;
   }
   else if (!Self->ProcessPathMID) {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
   else if (!AccessMemory(Self->ProcessPathMID, MEM_READ, 2000, (void **)&Self->ProcessPath)) {
      *Value = Self->ProcessPath;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return PostError(ERR_AccessMemory);
   }
}

/*****************************************************************************

-FIELD-
Priority: The task priority in relation to other tasks is be defined here.

Set the Priority field to change the priority of the process associated with the task.  The default value for all
processes is zero.  High positive values will give the process more CPU time while negative values will yield
CPU time to other active processes.

Note that depending on the platform, there may be limits as to whether one process can change the priority level
of a foreign process.  Other factors such as the scheduler used by the host system should be considered in the
effect of prioritisation.

*****************************************************************************/

static ERROR SET_Priority(objTask *Self, LONG Value)
{
#ifdef __unix__
   LONG priority, unused;
   priority = -getpriority(PRIO_PROCESS, 0);
   unused = nice(-(Value - priority));
#endif
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
ReturnCode: The task's return code can be retrieved following execution.

Once a process has completed execution then its return code can be read from this field.  If process is still running,
the error code ERR_TaskStillExists will be returned.

-ERRORS-
Okay
TaskStillExists: The task is still running and has no return code at this stage.
DoesNotExist: The task is yet to be successfully launched with the Activate action.

*****************************************************************************/

static ERROR GET_ReturnCode(objTask *Self, LONG *Value)
{
   if (Self->ReturnCodeSet) {
      *Value = Self->ReturnCode;
      return ERR_Okay;
   }

   if (!Self->ProcessID) {
      LogMsg("Task hasn't been launched yet.");
      return ERR_DoesNotExist;
   }

#ifdef __unix__
   // Please note that ProcessMessages() will typically kill zombie processes.  This means waitpid() may not return the
   // status (although it will inform us that the task no longer exists).  This issue is resolved by getting
   // ProcessMessages() to set the ReturnCode field when it detects zombie tasks.

   LONG status = 0;
   LONG result = waitpid(Self->ProcessID, &status, WNOHANG);

   if ((result IS -1) OR (result IS Self->ProcessID)) {
      // The process has exited.  Find out what error code was returned and pass it as the result.

      if (WIFEXITED(status)) {
         Self->ReturnCode = (BYTE)WEXITSTATUS(status);
         Self->ReturnCodeSet = TRUE;
      }

      *Value = Self->ReturnCode;
      return ERR_Okay;
   }
   else return ERR_TaskStillExists;

#elif _WIN32

   winGetExitCodeProcess(Self->Platform, &Self->ReturnCode);
   if (Self->ReturnCode IS 259) return ERR_TaskStillExists;
   else {
      Self->ReturnCodeSet = TRUE;
      *Value = Self->ReturnCode;
      return ERR_Okay;
   }

#else

   return ERR_NoSupport;

#endif
}

static ERROR SET_ReturnCode(objTask *Self, LONG Value)
{
   Self->ReturnCodeSet = TRUE;
   Self->ReturnCode = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Short: A short description of the process' purpose.

This field allows for the specification of a short description for the process. The description should be under 80
characters (one sentence).  The description is typically useful for occasions where the user is debugging the
system or trying to get a quick overview of the processes that are currently running.
-END-

*****************************************************************************/

static ERROR GET_Short(objTask *Self, CSTRING *Value)
{
   *Value = Self->Short;
   return ERR_Okay;
}

static ERROR SET_Short(objTask *Self, CSTRING Value)
{
   StrCopy(Value, Self->Short, sizeof(Self->Short));
   return ERR_Okay;
}

//****************************************************************************

static const struct FieldArray clFields[] = {
   { "TimeOut",         FDF_DOUBLE|FDF_RW,     0, NULL, NULL },
   { "Flags",           FDF_LONGFLAGS|FDF_RI, (MAXINT)&clFlags, NULL, NULL },
   { "ReturnCode",      FDF_LONG|FDF_RW,      0, GET_ReturnCode, SET_ReturnCode },
   { "ProcessID",       FDF_LONG|FDF_RI,      0, NULL, NULL },
   // Virtual fields
   { "Actions",        FDF_POINTER|FDF_R,   0, GET_Actions,          NULL },
   { "Args",           FDF_STRING|FDF_W,    0, NULL,                 SET_Args },
   { "Parameters",     FDF_ARRAY|FDF_STRING|FDF_RW, 0, GET_Parameters, SET_Parameters },
   { "Author",         FDF_STRING|FDF_RW,   0, GET_Author,           SET_Author },
   { "Copyright",      FDF_STRING|FDF_RW,   0, GET_Copyright,        SET_Copyright },
   { "Date",           FDF_STRING|FDF_RW,   0, GET_Date,             SET_Date },
   { "ErrorCallback",  FDF_FUNCTIONPTR|FDF_RI, 0, GET_ErrorCallback, SET_ErrorCallback }, // STDERR
   { "ExitCallback",   FDF_FUNCTIONPTR|FDF_RW, 0, GET_ExitCallback,  SET_ExitCallback },
   { "Instance",       FDF_LONG|FDF_R,      0, GET_Instance,         NULL },
   { "LaunchPath",     FDF_STRING|FDF_RW,   0, GET_LaunchPath,       SET_LaunchPath },
   { "Location",       FDF_STRING|FDF_RW,   0, GET_Location,         SET_Location },
   { "MessageQueue",   FDF_LONG|FDF_R,      0, GET_MessageQueue,     NULL },
   { "Name",           FDF_STRING|FDF_RW,   0, GET_Name,             SET_Name },
   { "OutputCallback", FDF_FUNCTIONPTR|FDF_RI, 0, GET_OutputCallback, SET_OutputCallback }, // STDOUT
   { "Path",           FDF_STRING|FDF_RW,   0, GET_Path,              SET_Path },
   { "ProcessPath",    FDF_STRING|FDF_R,    0, GET_ProcessPath,       NULL },
   { "Priority",       FDF_LONG|FDF_W,      0, NULL,                  SET_Priority },
   { "Short",          FDF_STRING|FDF_RW,   0, GET_Short,             SET_Short },
   // Synonyms
   { "Src",            FDF_SYNONYM|FDF_STRING|FDF_RW,   0, GET_Location,     SET_Location },
   { "ArgsList",       FDF_ARRAY|FDF_STRING|FDF_SYSTEM|FDF_RW, 0, GET_Parameters, SET_Parameters }, // OBSOLETE
   END_FIELD
};
