/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
Task: System processes are managed by the Task class.

Tasks, also known as processes, form the basis of process execution in an operating system.  By creating a task object,
it is possible to execute a program from within the host system.

To execute a compiled program, set the #Location field to point to the executable file before initialising the
task.  Arguments can be passed to the executable by setting the #Parameters field.  Once the task object is
successfully initialised, use the #Activate() action to run the executable.  If the file executes successfully,
a new task object is spawned separately to represent the executable (which means it is safe to destroy your task object
immediately afterwards).  If the #Activate() action returns with ERR::Okay then the executable program was run
successfully.

To find the task object that represents the active process, use the ~CurrentTask() function to quickly retrieve it.

-END-

*********************************************************************************************************************/

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

extern "C" void CloseCore(void);

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

extern "C" DLLCALL LONG WINAPI RegOpenKeyExA(LONG,CSTRING,LONG,LONG,APTR *);
extern "C" DLLCALL LONG WINAPI RegQueryValueExA(APTR,CSTRING,LONG *,LONG *,BYTE *,LONG *);
extern "C" DLLCALL LONG WINAPI RegSetValueExA(APTR hKey, CSTRING lpValueName, LONG Reserved, LONG dwType, const void *lpData, LONG cbData);

static LONG glProcessBreak = 0;
#endif

static ERR GET_LaunchPath(extTask *, STRING *);

static ERR TASK_Activate(extTask *, APTR);
static ERR TASK_Free(extTask *, APTR);
static ERR TASK_GetEnv(extTask *, struct taskGetEnv *);
static ERR TASK_GetVar(extTask *, struct acGetVar *);
static ERR TASK_Init(extTask *, APTR);
static ERR TASK_NewObject(extTask *, APTR);
static ERR TASK_SetEnv(extTask *, struct taskSetEnv *);
static ERR TASK_SetVar(extTask *, struct acSetVar *);
static ERR TASK_Write(extTask *, struct acWrite *);

static ERR TASK_AddArgument(extTask *, struct taskAddArgument *);
static ERR TASK_Expunge(extTask *, APTR);
static ERR TASK_Quit(extTask *, APTR);

static const FieldDef clFlags[] = {
   { "Foreign",    TSF::FOREIGN },
   { "Wait",       TSF::WAIT },
   { "Shell",      TSF::SHELL },
   { "ResetPath",  TSF::RESET_PATH },
   { "Privileged", TSF::PRIVILEGED },
   { "LogAll",     TSF::LOG_ALL },
   { "Quiet",      TSF::QUIET },
   { "Attached",   TSF::ATTACHED },
   { "Detached",   TSF::DETACHED },
   { "Pipe",       TSF::PIPE },
   { NULL, 0 }
};

static const ActionArray clActions[] = {
   { AC_Activate,      TASK_Activate },
   { AC_Free,          TASK_Free },
   { AC_GetVar,        TASK_GetVar },
   { AC_NewObject,     TASK_NewObject },
   { AC_SetVar,        TASK_SetVar },
   { AC_Init,          TASK_Init },
   { AC_Write,         TASK_Write },
   { 0, NULL }
};

#include "class_task_def.c"

//********************************************************************************************************************

static void task_stdinput_callback(HOSTHANDLE FD, void *Task)
{
   auto Self = (extTask *)Task;
   char buffer[4096];
   ERR error;

#ifdef _WIN32
   LONG bytes_read;
   auto result = winReadStdInput(FD, buffer, sizeof(buffer)-1, &bytes_read);
   if (!result) error = ERR::Okay;
   else if (result IS 1) return;
   else if (result IS -2) {
      error = ERR::Finished;
      RegisterFD(winGetStdInput(), RFD::READ|RFD::REMOVE, &task_stdinput_callback, Self);
   }
   else return;
#else
   LONG bytes_read = read(fileno(stdin), buffer, sizeof(buffer)-1);
   if (bytes_read >= 0) error = ERR::Okay;
   else error = ERR::Finished;
#endif

   buffer[bytes_read] = 0;

   if (Self->InputCallback.isC()) {
      auto routine = (void (*)(extTask *, APTR, LONG, ERR, APTR))Self->InputCallback.StdC.Routine;
      routine(Self, buffer, bytes_read, error, Self->InputCallback.StdC.Meta);
   }
   else if (Self->InputCallback.isScript()) {
      const ScriptArg args[] = {
         { "Task",       Self },
         { "Buffer",     buffer, FD_PTRBUFFER },
         { "BufferSize", bytes_read, FD_LONG|FD_BUFSIZE },
         { "Status",     LONG(error), FD_ERROR }
      };
      scCallback(Self->InputCallback.Script.Script, Self->InputCallback.Script.ProcedureID, args, std::ssize(args), NULL);
   }
}

#ifdef __unix__
static void check_incoming(extTask *Self)
{
   struct pollfd fd;

   if (Self->InFD != -1) {
      fd.fd = Self->InFD;
      fd.events = POLLIN;
      if ((poll(&fd, 1, 0) > 0) and (fd.revents & POLLIN)) {
         task_stdout(Self->InFD, Self);
      }
   }

   if (Self->ErrFD != -1) {
      fd.fd = Self->ErrFD;
      fd.events = POLLIN;
      if ((poll(&fd, 1, 0) > 0) and (fd.revents & POLLIN)) {
         task_stderr(Self->ErrFD, Self);
      }
   }
}
#endif

//********************************************************************************************************************
// Data output from the executed process is passed via data channels to the object specified in Task->OutputID, and/or
// sent to a callback function.

#ifdef __unix__
static void task_stdout(HOSTHANDLE FD, APTR Task)
{
   static UBYTE recursive = 0;

   if (recursive) return;

   recursive++;

   LONG len;
   char buffer[2048];
   if ((len = read(FD, buffer, sizeof(buffer)-1)) > 0) {
      buffer[len] = 0;

      auto task = (extTask *)Task;
      if (task->OutputCallback.isC()) {
         auto routine = (void (*)(extTask *, APTR, LONG, APTR))task->OutputCallback.StdC.Routine;
         routine(task, buffer, len, task->OutputCallback.StdC.Meta);
      }
      else if (task->OutputCallback.isScript()) {
         const ScriptArg args[] = {
            { "Task",       Task,   FD_OBJECTPTR },
            { "Buffer",     buffer, FD_PTRBUFFER },
            { "BufferSize", len,    FD_LONG|FD_BUFSIZE }
         };
         scCallback(task->OutputCallback.Script.Script, task->OutputCallback.Script.ProcedureID, args, std::ssize(args), NULL);
      }
   }
   recursive--;
}

static void task_stderr(HOSTHANDLE FD, APTR Task)
{
   char buffer[2048];
   LONG len;
   static UBYTE recursive = 0;

   if (recursive) return;

   recursive++;
   if ((len = read(FD, buffer, sizeof(buffer)-1)) > 0) {
      buffer[len] = 0;

      auto task = (extTask *)Task;
      if (task->ErrorCallback.Type) {
         if (task->ErrorCallback.isC()) {
            auto routine = (void (*)(extTask *, APTR, LONG, APTR))task->ErrorCallback.StdC.Routine;
            routine(task, buffer, len, task->ErrorCallback.StdC.Meta);
         }
         else if (task->ErrorCallback.isScript()) {
            if (auto script = task->ErrorCallback.Script.Script) {
               const ScriptArg args[] = {
                  { "Task", Task, FD_OBJECTPTR },
                  { "Data", buffer, FD_PTRBUFFER },
                  { "Size", len, FD_LONG|FD_BUFSIZE }
               };
               scCallback(script, task->ErrorCallback.Script.ProcedureID, args, std::ssize(args), NULL);
            }
         }
      }
   }
   recursive--;
}
#endif

//********************************************************************************************************************
// task_incoming_stdout() and task_incoming_stderr() are callbacks that are activated when data is incoming from a
// process that we've launched.

#ifdef _WIN32
static void output_callback(extTask *Task, FUNCTION *Callback, APTR Buffer, LONG Size)
{
   if (Callback->isC()) {
      auto routine = (void (*)(extTask *, APTR, LONG, APTR))Callback->StdC.Routine;
      routine(Task, Buffer, Size, Callback->StdC.Meta);
   }
   else if (Callback->isScript()) {
      auto script = Callback->Script.Script;
      const ScriptArg args[] = {
         { "Task", Task, FD_OBJECTPTR },
         { "Data", Buffer, FD_PTRBUFFER },
         { "Size", Size, FD_LONG|FD_BUFSIZE }
      };
      scCallback(script, Callback->Script.ProcedureID, args, std::ssize(args), NULL);
   }
}

static void task_incoming_stdout(WINHANDLE Handle, extTask *Task)
{
   pf::Log log(__FUNCTION__);
   static UBYTE recursive = 0;

   if (recursive) return;
   if (!Task->Platform) return;

   log.traceBranch("");

   char buffer[4096];
   LONG size = sizeof(buffer) - 1;
   winResetStdOut(Task->Platform, buffer, &size);

   if (size > 0) {
      recursive = 1;
      buffer[size] = 0;
      output_callback(Task, &Task->OutputCallback, buffer, size);
      recursive = 0;
   }
}

static void task_incoming_stderr(WINHANDLE Handle, extTask *Task)
{
   pf::Log log(__FUNCTION__);
   static UBYTE recursive = 0;

   if (recursive) return;
   if (!Task->Platform) return;

   log.traceBranch("");

   char buffer[4096];
   LONG size = sizeof(buffer) - 1;
   winResetStdErr(Task->Platform, buffer, &size);

   if (size > 0) {
      recursive = 1;
      buffer[size] = 0;
      output_callback(Task, &Task->ErrorCallback, buffer, size);
      recursive = 0;
   }
}

//********************************************************************************************************************
// These functions arrange for callbacks to be made whenever one of our process-connected pipes receives data.

extern "C" void task_register_stdout(extTask *Task, WINHANDLE Handle)
{
   pf::Log log(__FUNCTION__);
   log.traceBranch("Handle: %d", (LONG)(MAXINT)Handle);
   RegisterFD(Handle, RFD::READ, (void (*)(void *, void *))&task_incoming_stdout, Task);
}

extern "C" void task_register_stderr(extTask *Task, WINHANDLE Handle)
{
   pf::Log log(__FUNCTION__);
   log.traceBranch("Handle: %d", (LONG)(MAXINT)Handle);
   RegisterFD(Handle, RFD::READ, (void (*)(void *, void *))&task_incoming_stderr, Task);
}

//********************************************************************************************************************

extern "C" void task_deregister_incoming(WINHANDLE Handle)
{
   RegisterFD(Handle, RFD::REMOVE|RFD::READ|RFD::WRITE|RFD::EXCEPT, NULL, NULL);
}
#endif

//********************************************************************************************************************

static ERR msg_waitforobjects(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   return ERR::Terminate;
}

//********************************************************************************************************************

static CSTRING action_id_name(LONG ActionID)
{
   static char idname[20];
   if ((ActionID > 0) and (ActionID < AC_END)) {
      return ActionTable[ActionID].Name;
   }
   else {
      snprintf(idname, sizeof(idname), "%d", ActionID);
      return idname;
   }
}

//********************************************************************************************************************

static ERR msg_action(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   pf::Log log("ProcessMessages");
   ActionMessage *action;

   if (!(action = (ActionMessage *)Message)) {
      log.warning("No data attached to MSGID_ACTION message.");
      return ERR::Okay;
   }

   #ifdef DBG_INCOMING
      log.function("Executing action %s on object #%d, Data: %p, Size: %d", action_id_name(action->ActionID), action->ObjectID, Message, MsgSize);
   #endif

   if ((action->ObjectID) and (action->ActionID)) {
      OBJECTPTR obj;
      ERR error;
      if ((error = AccessObject(action->ObjectID, 5000, &obj)) IS ERR::Okay) {
         if (action->SendArgs IS false) {
            obj->Flags |= NF::MESSAGE;
            Action(action->ActionID, obj, NULL);
            obj->Flags = obj->Flags & (~NF::MESSAGE);
            ReleaseObject(obj);
         }
         else {
            const FunctionField *fields;
            if (action->ActionID > 0) fields = ActionTable[action->ActionID].Args;
            else {
               auto cl = obj->ExtClass;
               if (cl->Base) cl = cl->Base;
               fields = cl->Methods[-action->ActionID].Args;
            }

            // Use resolve_args() to process the args structure back into something readable

            if (fields) {
               if (resolve_args(action+1, fields) IS ERR::Okay) {
                  obj->Flags |= NF::MESSAGE;
                  Action(action->ActionID, obj, action+1);
                  obj->Flags = obj->Flags & (~NF::MESSAGE);
                  ReleaseObject(obj);

                  local_free_args(action+1, fields);
               }
               else {
                  log.warning("Failed to resolve arguments for action %s.", action_id_name(action->ActionID));
                  ReleaseObject(obj);
               }
            }
         }
      }
      else {
         if ((error != ERR::NoMatchingObject) and (error != ERR::MarkedForDeletion)) {
            if (action->ActionID > 0) log.warning("Could not gain access to object %d to execute action %s.", action->ObjectID, action_id_name(action->ActionID));
            else log.warning("Could not gain access to object %d to execute method %d.", action->ObjectID, action->ActionID);
         }
      }
   }
   else log.warning("Action message %s specifies an object ID of #%d.", action_id_name(action->ActionID), action->ObjectID);

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR msg_quit(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   pf::Log log(__FUNCTION__);
   log.function("Processing quit message");
   glTaskState = TSTATE::STOPPING;
   return ERR::Okay;
}

//********************************************************************************************************************
// Determine whether or not a process is alive

extern "C" ERR validate_process(LONG ProcessID)
{
   pf::Log log(__FUNCTION__);
   static LONG glValidating = 0;

   log.function("PID: %d", ProcessID);

   if (glValidating) return ERR::Okay;
   if (glValidateProcessID IS ProcessID) glValidateProcessID = 0;
   if ((ProcessID IS glProcessID) or (!ProcessID)) return ERR::Okay;

   #ifdef _WIN32
      // On Windows we don't check if the process is alive because validation can often occur during the final shutdown
      // phase of the other process.
   #elif __unix__
      if ((kill(ProcessID, 0) IS -1) and (errno IS ESRCH));
      else return ERR::Okay;
   #else
      log.error("This platform does not support validate_process()");
      return ERR::Okay;
   #endif

   OBJECTID task_id = 0;
   for (auto it = glTasks.begin(); it != glTasks.end(); it++) {
      if (it->ProcessID IS ProcessID) {
         task_id = it->TaskID;
         glTasks.erase(it);
         break;
      }
   }

   if (!task_id) return ERR::False;

   evTaskRemoved task_removed = { GetEventID(EVG::SYSTEM, "task", "removed"), task_id, ProcessID };
   BroadcastEvent(&task_removed, sizeof(task_removed));

   glValidating = 0;
   return ERR::False; // Return ERR::False to indicate that the task was not healthy
}

//********************************************************************************************************************
// This function is called when a WIN32 process that we launched has been terminated.
//
// For the linux equivalent, refer to internal.c validate_processID().

#ifdef _WIN32
static void task_process_end(WINHANDLE FD, extTask *Task)
{
   pf::Log log(__FUNCTION__);

   winGetExitCodeProcess(Task->Platform, &Task->ReturnCode);
   if (Task->ReturnCode != 259) {
      Task->ReturnCodeSet = true;
      log.branch("Process %" PF64 " ended, return code: %d.", (LARGE)FD, Task->ReturnCode);
   }
   else log.branch("Process %" PF64 " signalled exit too early.", (LARGE)FD);

   if (Task->Platform) {
      char buffer[4096];
      LONG size;

      // Process remaining data

      do {
         size = sizeof(buffer);
         if ((!winReadStd(Task->Platform, TSTD_OUT, buffer, &size)) and (size)) {
            log.msg("Processing %d remaining bytes on stdout.", size);
            output_callback(Task, &Task->OutputCallback, buffer, size);
         }
         else break;
      } while (size IS sizeof(buffer));

      do {
         size = sizeof(buffer);
         if ((!winReadStd(Task->Platform, TSTD_ERR, buffer, &size)) and (size)) {
            log.msg("Processing %d remaining bytes on stderr.", size);
            output_callback(Task, &Task->ErrorCallback, buffer, size);
         }
         else break;
      } while (size IS sizeof(buffer));

      winFreeProcess(Task->Platform);
      Task->Platform = NULL;
   }
   else winCloseHandle(FD); // winFreeProcess() normally does this with Process->Handle

   // Call ExitCallback, if specified

   if (Task->ExitCallback.isC()) {
      auto routine = (void (*)(extTask *, APTR))Task->ExitCallback.StdC.Routine;
      routine(Task, Task->ExitCallback.StdC.Meta);
   }
   else if (Task->ExitCallback.isScript()) {
      auto script = Task->ExitCallback.Script.Script;
      const ScriptArg args[] = { { "Task", Task, FD_OBJECTPTR } };
      scCallback(script, Task->ExitCallback.Script.ProcedureID, args, std::ssize(args), NULL);
   }

   // Post an event for the task's closure

   evTaskRemoved task_removed = { EVID_SYSTEM_TASK_REMOVED, Task->UID, Task->ProcessID };
   BroadcastEvent(&task_removed, sizeof(task_removed));

   // Send a break if we're waiting for this process to end

   if (((Task->Flags & TSF::WAIT) != TSF::NIL) and (Task->TimeOut > 0)) SendMessage(glProcessBreak, MSF::NIL, NULL, 0);
}
#endif

//********************************************************************************************************************

#ifdef _WIN32
extern "C" void register_process_pipes(extTask *Self, WINHANDLE ProcessHandle)
{
   pf::Log log;
   log.traceBranch("Process: %d", (LONG)(MAXINT)ProcessHandle);
   RegisterFD(ProcessHandle, RFD::READ, (void (*)(void *, void *))&task_process_end, Self);
}

extern "C" void deregister_process_pipes(extTask *Self, WINHANDLE ProcessHandle)
{
   pf::Log log;
   log.traceBranch("Process: %d", (LONG)(MAXINT)ProcessHandle);
   if (ProcessHandle) RegisterFD(ProcessHandle, RFD::REMOVE|RFD::READ|RFD::WRITE|RFD::EXCEPT, NULL, NULL);
}
#endif

/*********************************************************************************************************************

-ACTION-
Activate: Activating a task object will execute it.

Activating a task results in the execution of the file referenced in the #Location field.

On successful execution, the ProcessID will refer to the ID of the executed process.  This ID is compatible with the
hosting platform's unique process numbers.

If the `WAIT` flag is specified, this action will not return until the executed process has returned or the
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

*********************************************************************************************************************/

static ERR TASK_Activate(extTask *Self, APTR Void)
{
   pf::Log log;
   LONG i, j, k;
   char buffer[1000];
   STRING path;
   ERR error;
   #ifdef _WIN32
      char launchdir[500];
      STRING redirect_stdout, redirect_stderr;
      BYTE hide_output;
      LONG winerror;
   #endif
   #ifdef __unix__
      LONG pid;
      BYTE privileged, shell;
   #endif

   Self->ReturnCodeSet = false;

   if ((Self->Flags & TSF::FOREIGN) != TSF::NIL) Self->Flags |= TSF::SHELL;

   if (!Self->Location) return log.warning(ERR::MissingPath);

   if (!glJanitorActive) {
      pf::SwitchContext ctx(glCurrentTask);
      auto call = FUNCTION(process_janitor);
      SubscribeTimer(60, &call, &glProcessJanitor);
      glJanitorActive = true;
   }

#ifdef _WIN32
   // Determine the launch folder

   launchdir[0] = 0;
   if ((GET_LaunchPath(Self, &path) IS ERR::Okay) and (path)) {
      if (ResolvePath(path, RSF::APPROXIMATE|RSF::PATH, &path) IS ERR::Okay) {
         for (i=0; (path[i]) and ((size_t)i < sizeof(launchdir)-1); i++) launchdir[i] = path[i];
         launchdir[i] = 0;
         FreeResource(path);
      }
      else {
         for (i=0; (path[i]) and ((size_t)i < sizeof(launchdir)-1); i++) launchdir[i] = path[i];
         launchdir[i] = 0;
      }
   }
   else if ((Self->Flags & TSF::RESET_PATH) != TSF::NIL) {
      if (ResolvePath(Self->Location, RSF::APPROXIMATE|RSF::PATH, &path) IS ERR::Okay) {
         for (i=0; (path[i]) and ((size_t)i < sizeof(launchdir)-1); i++) launchdir[i] = path[i];
         FreeResource(path);
      }
      else for (i=0; (Self->Location[i]) and ((size_t)i < sizeof(launchdir)-1); i++) launchdir[i] = Self->Location[i];

      while ((i > 0) and (launchdir[i] != '\\')) i--;
      launchdir[i] = 0;
   }

   // Resolve the location of the executable (may contain a volume) and copy it to the command line buffer.

   i = 0;
   buffer[i++] = '"';
   if (ResolvePath(Self->Location, RSF::APPROXIMATE|RSF::PATH, &path) IS ERR::Okay) {
      for (j=0; (path[j]) and ((size_t)i < sizeof(buffer)-1); i++,j++) {
         if (path[j] IS '/') buffer[i] = '\\';
         else buffer[i] = path[j];
      }
      FreeResource(path);
   }
   else {
      for (j=0; (Self->Location[j]) and ((size_t)i < sizeof(buffer)-1); i++,j++) {
         if (Self->Location[j] IS '/') buffer[i] = '\\';
         else buffer[i] = Self->Location[j];
      }
   }
   buffer[i++] = '"';

   // Following the executable path are any arguments that have been used

   redirect_stdout = NULL;
   redirect_stderr = NULL;
   hide_output = false;

   for (auto &param : Self->Parameters) {
      if (param[0] IS '>') {
         // Redirection argument detected

         if (ResolvePath(param.c_str() + 1, RSF::NO_FILE_CHECK, &redirect_stdout) IS ERR::Okay) {
            redirect_stderr = redirect_stdout;
         }

         log.msg("StdOut/Err redirected to %s", redirect_stdout);

         hide_output = true;
         continue;
      }
      else if ((param[0] IS '2') and (param[1] IS '>')) {
         log.msg("StdErr redirected to %s", param.c_str() + 2);
         ResolvePath(param.c_str() + 2, RSF::NO_FILE_CHECK, &redirect_stderr);
         hide_output = true;
         continue;
      }
      else if ((param[0] IS '1') and (param[1] IS '>')) {
         log.msg("StdOut redirected to %s", param.c_str() + 2);
         ResolvePath(param.c_str() + 2, RSF::NO_FILE_CHECK, &redirect_stdout);
         hide_output = true;
         continue;
      }

      buffer[i++] = ' ';

      // Check if the argument contains spaces - if so, we need to encapsulate it within quotes.  Otherwise, just
      // copy it as normal.

      for (k=0; (param[k]) and (param[k] != ' '); k++);

      if (param[k] IS ' ') {
         buffer[i++] = '"';
         for (k=0; param[k]; k++) buffer[i++] = param[k];
         buffer[i++] = '"';
      }
      else for (k=0; param[k]; k++) buffer[i++] = param[k];
   }

   buffer[i] = 0;

   // Convert single quotes into double quotes

   bool whitespace = true;
   for (i=0; buffer[i]; i++) {
      if (whitespace) {
         if (buffer[i] IS '"') {
            // Skip everything inside double quotes
            i++;
            while ((buffer[i]) and (buffer[i] != '"')) i++;
            if (!buffer[i]) break;
            whitespace = false;
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

      if (buffer[i] <= 0x20) whitespace = true;
      else whitespace = false;
   }

   log.trace("Exec: %s", buffer);

   // Hide window if this is designated a shell program (i.e. hide the DOS window).
   // NB: If you hide a non-shell program, this usually results in the first GUI window that pops up being hidden.

   if ((Self->Flags & TSF::SHELL) != TSF::NIL) hide_output = true;

   // Determine whether this new process will be a member of the parent process' group.  This can be forced with the TSF::DETACHED/ATTACHED flags,
   // otherwise it will be determined automatically according to the status of our current task.

   bool group;

   if ((Self->Flags & TSF::ATTACHED) != TSF::NIL) group = true;
   else if ((Self->Flags & TSF::DETACHED) != TSF::NIL) group = false;
   else group = true;

   LONG internal_redirect = 0;
   if (Self->OutputCallback.Type) internal_redirect |= TSTD_OUT;
   if (Self->ErrorCallback.Type) internal_redirect |= TSTD_ERR;
   if ((Self->Flags & TSF::PIPE) != TSF::NIL) internal_redirect |= TSTD_IN;

   if (!(winerror = winLaunchProcess(Self, buffer, (launchdir[0] != 0) ? launchdir : 0, group,
         internal_redirect, &Self->Platform, hide_output, redirect_stdout, redirect_stderr, &Self->ProcessID))) {

      error = ERR::Okay;
      if (((Self->Flags & TSF::WAIT) != TSF::NIL) and (Self->TimeOut > 0)) {
         log.msg("Waiting for process to exit.  TimeOut: %.2f sec", Self->TimeOut);

         //if (!glProcessBreak) glProcessBreak = AllocateID(IDTYPE_MESSAGE);
         glProcessBreak = MSGID_BREAK;

         ProcessMessages(PMF::NIL, Self->TimeOut * 1000.0);

         winGetExitCodeProcess(Self->Platform, &Self->ReturnCode);
         if (Self->ReturnCode != 259) Self->ReturnCodeSet = true;
      }
   }
   else {
      char msg[300];
      winFormatMessage(winerror, msg, sizeof(msg));
      log.warning("Launch Error: %s", msg);
      error = ERR::Failed;
   }

   if (redirect_stderr IS redirect_stdout) redirect_stderr = NULL;
   if (redirect_stdout) FreeResource(redirect_stdout);
   if (redirect_stderr) FreeResource(redirect_stderr);

   return error;

#elif __unix__

   // Add a 'cd' command so that the application starts in its own folder

   path = NULL;
   GET_LaunchPath(Self, &path);

   i = 0;
   if (((Self->Flags & TSF::RESET_PATH) != TSF::NIL) or (path)) {
      Self->Flags |= TSF::SHELL;

      buffer[i++] = 'c';
      buffer[i++] = 'd';
      buffer[i++] = ' ';

      if (!path) path = Self->Location;
      if (!ResolvePath(path, RSF::APPROXIMATE|RSF::PATH, &path)) {
         for (j=0; (path[j]) and ((size_t)i < sizeof(buffer)-1);) buffer[i++] = path[j++];
         FreeResource(path);
      }
      else {
         for (j=0; (path[j]) and ((size_t)i < sizeof(buffer)-1);) buffer[i++] = path[j++];
      }

      while ((i > 0) and (buffer[i-1] != '/')) i--;
      if (i > 0) {
         buffer[i++] = ';';
         buffer[i++] = ' ';
      }
   }

   // Resolve the location of the executable (may contain an volume) and copy it to the command line buffer.

   if (!ResolvePath(Self->Location, RSF::APPROXIMATE|RSF::PATH, &path)) {
      for (j=0; (path[j]) and ((size_t)i < sizeof(buffer)-1);) buffer[i++] = path[j++];
      buffer[i] = 0;
      FreeResource(path);
   }
   else {
      for (j=0; (Self->Location[j]) and ((size_t)i < sizeof(buffer)-1);) buffer[i++] = Self->Location[j++];
      buffer[i] = 0;
   }

   CSTRING argslist[Self->Parameters.size()+2];
   LONG bufend = i;

   // Following the executable path are any arguments that have been used. NOTE: This isn't needed if TSF::SHELL is used,
   // however it is extremely useful in the debug printout to see what is being executed.

   for (auto &param : Self->Parameters) {
      buffer[i++] = ' ';

      // Check if the argument contains spaces - if so, we need to encapsulate it within quotes.  Otherwise, just
      // copy it as normal.

      for (k=0; (param[k]) and (param[k] != ' '); k++);

      if (param[k] IS ' ') {
         buffer[i++] = '"';
         for (k=0; param[k]; k++) buffer[i++] = param[k];
         buffer[i++] = '"';
      }
      else for (k=0; param[k]; k++) buffer[i++] = param[k];
   }
   buffer[i] = 0;

   // Convert single quotes into double quotes

   for (i=0; buffer[i]; i++) if (buffer[i] IS '\'') buffer[i] = '"';

   log.warning("%s", buffer);

   // If we're not going to run in shell mode, create an argument list for passing to the program.

   if ((Self->Flags & TSF::SHELL) IS TSF::NIL) {
      buffer[bufend] = 0;

      argslist[0] = buffer;
      unsigned i;
      for (i=0; i < Self->Parameters.size(); i++) {
         argslist[i+1] = Self->Parameters[i].c_str();
      }
      argslist[i+1] = NULL;

      if ((Self->Flags & TSF::LOG_ALL) != TSF::NIL) {
         for (i=1; argslist[i]; i++) {
            log.msg("Arg %d: %s", i, argslist[i]);
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
      log.trace("Output will be sent to callback.");
      if (!pipe(outpipe)) {
         out_fd = outpipe[1]; // for writing
         in_fd  = outpipe[0]; // for reading
      }
      else {
         log.warning("Failed to create pipe: %s", strerror(errno));
         if (input_fd != -1) close(input_fd);
         if (out_fd != -1)   close(out_fd);
         return ERR::Failed;
      }
   }

   if ((out_fd IS -1) and ((Self->Flags & TSF::QUIET) != TSF::NIL)) {
      log.msg("Output will go to NULL");
      out_fd = open("/dev/null", O_RDONLY);
   }

   if (Self->ErrorCallback.Type) {
      log.trace("Error output will be sent to a callback.");
      if (!pipe(errpipe)) {
         out_errfd = errpipe[1];
         in_errfd  = errpipe[0];
      }
      else {
         log.warning("Failed to create pipe: %s", strerror(errno));
         if (input_fd != -1) close(input_fd);
         if (out_fd != -1)   close(out_fd);
         return ERR::Failed;
      }
   }

   if ((out_errfd IS -1) and ((Self->Flags & TSF::QUIET) != TSF::NIL)) {
      out_errfd = open("/dev/null", O_RDONLY);
   }

   // Fork a new task.  Remember that forking produces an exact duplicate of the process that made the fork.

   privileged = ((Self->Flags & TSF::PRIVILEGED) != TSF::NIL) ? 1 : 0;
   shell = ((Self->Flags & TSF::SHELL) != TSF::NIL) ? 1 : 0;

   pid = fork();

   if (pid IS -1) {
      if (input_fd != -1)  close(input_fd);
      if (out_fd != -1)    close(out_fd);
      if (out_errfd != -1) close(out_errfd);
      if (in_fd != -1)     close(in_fd);
      if (in_errfd != -1)  close(in_errfd);
      log.warning("Failed in an attempt to fork().");
      return ERR::Failed;
   }

   if (pid) {
      // The following code is executed by the initiating process thread

      log.msg("Created new process %d.  Shell: %d", pid, shell);

      Self->ProcessID = pid; // Record the native process ID

      glTasks.emplace_back(Self);

      if (in_fd != -1) {
         RegisterFD(in_fd, RFD::READ, &task_stdout, Self);
         Self->InFD = in_fd;
         close(out_fd);
      }

      if (in_errfd != -1) {
         RegisterFD(in_errfd, RFD::READ, &task_stderr, Self);
         Self->ErrFD = in_errfd;
         close(out_errfd);
      }

      // input_fd has no relevance to the parent process

      if (input_fd != -1) {
         close(input_fd);
         input_fd = -1;
      }

      error = ERR::Okay;
      if ((Self->Flags & TSF::WAIT) != TSF::NIL) {
         log.branch("Waiting for process to turn into a zombie in %.2fs.", Self->TimeOut);

         // Wait for the child process to turn into a zombie.  NB: A parent process or our own child handler may
         // potentially pick this up but that's fine as waitpid() will just fail with -1 in that case.

         LONG status = 0;
         LARGE ticks = PreciseTime() + LARGE(Self->TimeOut * 1000000.0);
         while (!waitpid(pid, &status, WNOHANG)) {
            ProcessMessages(PMF::NIL, 100);

            auto remaining = ticks - PreciseTime();
            if (remaining <= 0) {
               error = log.warning(ERR::TimeOut);
               break;
            }
         }

         // Find out what error code was returned

         if (WIFEXITED(status)) {
            Self->ReturnCode = (BYTE)WEXITSTATUS(status);
            Self->ReturnCodeSet = true;
         }

         if (kill(pid, 0)) {
            for (auto it = glTasks.begin(); it != glTasks.end(); it++) {
               if (it->ProcessID IS pid) {
                  glTasks.erase(it);
                  break;
               }
            }
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
   else execv(buffer, (char * const *)&argslist);

   exit(EXIT_FAILURE);
#endif
}

/*********************************************************************************************************************

-METHOD-
AddArgument: Adds a new argument to the Parameters field.

This method will add a new argument to the end of the #Parameters field array.  If the string is surrounded by quotes,
they will be removed automatically.

-INPUT-
cstr Argument: The new argument string.

-ERRORS-
Okay
NullArgs

*********************************************************************************************************************/

static ERR TASK_AddArgument(extTask *Self, struct taskAddArgument *Args)
{
   if ((!Args) or (!Args->Argument) or (!*Args->Argument)) return ERR::NullArgs;

   auto src = Args->Argument;
   if ((*src IS '"') or (*src IS '\'')) {
      auto end = *src++;
      LONG len = 0;
      while ((src[len]) and (src[len] != end)) len++;
      Self->Parameters.emplace_back(std::string(src, len));
   }
   else Self->Parameters.emplace_back(Args->Argument);

   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
Expunge: Forces a Task to expunge unused code.

The Expunge method releases all loaded libraries that are no longer in use by the active process.

-ERRORS-
Okay

*********************************************************************************************************************/

static ERR TASK_Expunge(extTask *Self, APTR Void)
{
   Expunge(false);
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR TASK_Free(extTask *Self, APTR Void)
{
   pf::Log log;

#ifdef __unix__
   check_incoming(Self);

   if (Self->InFD != -1) {
      RegisterFD(Self->InFD, RFD::REMOVE, NULL, NULL);
      close(Self->InFD);
      Self->InFD = -1;
   }

   if (Self->ErrFD != -1) {
      RegisterFD(Self->ErrFD, RFD::REMOVE, NULL, NULL);
      close(Self->ErrFD);
      Self->ErrFD = -1;
   }

   if (Self->InputCallback.Type) RegisterFD(fileno(stdin), RFD::READ|RFD::REMOVE, &task_stdinput_callback, Self);
#endif

#ifdef _WIN32
   if (Self->Env) { FreeResource(Self->Env); Self->Env = NULL; }
   if (Self->Platform) { winFreeProcess(Self->Platform); Self->Platform = NULL; }
   if (Self->InputCallback.Type) RegisterFD(winGetStdInput(), RFD::READ|RFD::REMOVE, &task_stdinput_callback, Self);
#endif

   // Free allocations

   if (Self->LaunchPath)  { FreeResource(Self->LaunchPath);  Self->LaunchPath  = NULL; }
   if (Self->Location)    { FreeResource(Self->Location);    Self->Location    = NULL; }
   if (Self->Path)        { FreeResource(Self->Path);        Self->Path        = NULL; }
   if (Self->ProcessPath) { FreeResource(Self->ProcessPath); Self->ProcessPath = NULL; }
   if (Self->MessageMID)  { FreeResource(Self->MessageMID);  Self->MessageMID  = 0; }

   if (Self->MsgAction)         { FreeResource(Self->MsgAction);         Self->MsgAction          = NULL; }
   if (Self->MsgDebug)          { FreeResource(Self->MsgDebug);          Self->MsgDebug           = NULL; }
   if (Self->MsgWaitForObjects) { FreeResource(Self->MsgWaitForObjects); Self->MsgWaitForObjects  = NULL; }
   if (Self->MsgQuit)           { FreeResource(Self->MsgQuit);           Self->MsgQuit            = NULL; }
   if (Self->MsgFree)           { FreeResource(Self->MsgFree);           Self->MsgFree            = NULL; }
   if (Self->MsgEvent)          { FreeResource(Self->MsgEvent);          Self->MsgEvent           = NULL; }
   if (Self->MsgThreadCallback) { FreeResource(Self->MsgThreadCallback); Self->MsgThreadCallback  = NULL; }
   if (Self->MsgThreadAction)   { FreeResource(Self->MsgThreadAction);   Self->MsgThreadAction    = NULL; }

   Self->~extTask();
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
GetEnv: Retrieves environment variables for the active process.

On platforms that support environment variables, GetEnv() returns the value of the environment variable matching the
Name string.  If there is no matching variable, `ERR::DoesNotExist` is returned.

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

*********************************************************************************************************************/

static ERR TASK_GetEnv(extTask *Self, struct taskGetEnv *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Name)) return log.warning(ERR::NullArgs);

#ifdef _WIN32

   #define ENV_SIZE 4096
   LONG len;

   Args->Value = NULL;

   if (glCurrentTask != Self) return ERR::Failed;

   if (!Self->Env) {
      if (AllocMemory(ENV_SIZE, MEM::STRING|MEM::NO_CLEAR, (APTR *)&Self->Env, NULL) != ERR::Okay) {
         return ERR::AllocMemory;
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

      for (LONG ki=0; ki < std::ssize(keys); ki++) {
         if (StrCompare(keys[ki].HKey, Args->Name) IS ERR::Okay) {
            CSTRING str = Args->Name + StrLength(keys[ki].HKey); // str = Parasol\Something
            len = StrLength(str); // End of string

            while (len > 0) {
               if (str[len] IS '\\') break;
               len--;
            }

            if (len > 0) {
               std::string path(str, len);

               APTR keyhandle;
               if (!RegOpenKeyExA(keys[ki].ID, path.c_str(), 0, KEY_READ, &keyhandle)) {
                  LONG type;
                  LONG envlen = ENV_SIZE;
                  if (!RegQueryValueExA(keyhandle, str+len+1, 0, &type, Self->Env, &envlen)) {
                     // Numerical registry types can be converted into strings

                     switch(type) {
                        case REG_DWORD:
                           snprintf(Self->Env, ENV_SIZE, "%d", ((LONG *)Self->Env)[0]);
                           break;
                        case REG_DWORD_BIG_ENDIAN: {
                           if constexpr (std::endian::native == std::endian::little) {
                              StrCopy(std::to_string(reverse_long(((LONG *)Self->Env)[0])).c_str(), Self->Env, ENV_SIZE);
                           }
                           else {
                              StrCopy(std::to_string(((LONG *)Self->Env)[0]).c_str(), Self->Env, ENV_SIZE);
                           }
                           break;
                        }
                        case REG_QWORD:
                           IntToStr(((LARGE *)Self->Env)[0], Self->Env, ENV_SIZE);
                           break;
                     }

                     Args->Value = Self->Env;
                  }
                  winCloseHandle(keyhandle);
               }

               if (Args->Value) return ERR::Okay;
               else return ERR::DoesNotExist;
            }
            else return log.warning(ERR::Syntax);
         }
      }
   }

   len = winGetEnv(Args->Name, Self->Env, ENV_SIZE);
   if (!len) return ERR::DoesNotExist;
   if (len >= ENV_SIZE) return log.warning(ERR::BufferOverflow);

   Args->Value = Self->Env;
   return ERR::Okay;

#elif __unix__
   if ((Args->Value = getenv(Args->Name))) {
      return ERR::Okay;
   }
   else return ERR::DoesNotExist;
#else
   #warn Write support for GetEnv()
   return ERR::NoSupport;
#endif
}

/*********************************************************************************************************************
-ACTION-
GetVar: Retrieves variable field values.
-END-
*********************************************************************************************************************/

static ERR TASK_GetVar(extTask *Self, struct acGetVar *Args)
{
   pf::Log log;
   LONG j;

   if ((!Args) or (!Args->Buffer) or (Args->Size <= 0)) return log.warning(ERR::NullArgs);

   auto it = Self->Fields.find(Args->Field);
   if (it != Self->Fields.end()) {
      for (j=0; (it->second[j]) and (j < Args->Size-1); j++) Args->Buffer[j] = it->second[j];
      Args->Buffer[j++] = 0;

      if (j >= Args->Size) return ERR::BufferOverflow;
      else return ERR::Okay;
   }

   log.warning("The variable \"%s\" does not exist.", Args->Field);

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR TASK_Init(extTask *Self, APTR Void)
{
   pf::Log log;
   LONG len;

   if (!fs_initialised) { // Perform the following if this is a Task representing the current process
      Self->ProcessID = glProcessID;

#ifdef _WIN32
      glTaskLock = get_threadlock(); // This lock can be used by other threads to wake the main task.

      LONG i;
      char buffer[300];
      if (winGetExeDirectory(sizeof(buffer), buffer)) {
         LONG len = StrLength(buffer);
         while ((len > 1) and (buffer[len-1] != '/') and (buffer[len-1] != '\\') and (buffer[len-1] != ':')) len--;
         if (AllocMemory(len+1, MEM::STRING|MEM::NO_CLEAR, (void **)&Self->ProcessPath, NULL) IS ERR::Okay) {
            for (i=0; i < len; i++) Self->ProcessPath[i] = buffer[i];
            Self->ProcessPath[i] = 0;
         }
      }

      if ((len = winGetCurrentDirectory(sizeof(buffer), buffer))) {
         if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
         if (AllocMemory(len+2, MEM::STRING|MEM::NO_CLEAR, (void **)&Self->Path, NULL) IS ERR::Okay) {
            for (i=0; i < len; i++) Self->Path[i] = buffer[i];
            if (Self->Path[i-1] != '\\') Self->Path[i++] = '\\';
            Self->Path[i] = 0;
         }
      }

#elif __unix__

         char buffer[256], procfile[50];
         LONG i;

         // This method of path retrieval only works on Linux (most types of Unix don't provide any support for this).

         snprintf(procfile, sizeof(procfile), "/proc/%d/exe", glProcessID);

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
            while ((len > 1) and (buffer[len-1] != '/') and (buffer[len-1] != '\\') and (buffer[len-1] != ':')) len--;
            if (!AllocMemory(len+1, MEM::STRING|MEM::NO_CLEAR, (void **)&Self->ProcessPath, NULL)) {
               for (i=0; i < len; i++) Self->ProcessPath[i] = buffer[i];
               Self->ProcessPath[i] = 0;
            }
         }

         if (!Self->Path) { // Set the working folder
            if (getcwd(buffer, sizeof(buffer))) {
               if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
               for (len=0; buffer[len]; len++);
               if (!AllocMemory(len+2, MEM::STRING|MEM::NO_CLEAR, (void **)&Self->Path, NULL)) {
                  for (i=0; buffer[i]; i++) Self->Path[i] = buffer[i];
                  Self->Path[i++] = '/';
                  Self->Path[i] = 0;
               }
            }
         }
#endif

      // Initialise message handlers so that the task can process messages.

      FUNCTION call;
      call.Type = CALL_STDC;
      call.StdC.Routine = (APTR)msg_action;
      AddMsgHandler(NULL, MSGID_ACTION, &call, &Self->MsgAction);

      call.StdC.Routine = (APTR)msg_free;
      AddMsgHandler(NULL, MSGID_FREE, &call, &Self->MsgFree);

      call.StdC.Routine = (APTR)msg_quit;
      AddMsgHandler(NULL, MSGID_QUIT, &call, &Self->MsgQuit);

      call.StdC.Routine = (APTR)msg_waitforobjects;
      AddMsgHandler(NULL, MSGID_WAIT_FOR_OBJECTS, &call, &Self->MsgWaitForObjects);

      call.StdC.Routine = (APTR)msg_event; // lib_events.c
      AddMsgHandler(NULL, MSGID_EVENT, &call, &Self->MsgEvent);

      call.StdC.Routine = (APTR)msg_threadcallback; // class_thread.c
      AddMsgHandler(NULL, MSGID_THREAD_CALLBACK, &call, &Self->MsgThreadCallback);

      call.StdC.Routine = (APTR)msg_threadaction; // class_thread.c
      AddMsgHandler(NULL, MSGID_THREAD_ACTION, &call, &Self->MsgThreadAction);

      log.msg("Process Path: %s", Self->ProcessPath);
      log.msg("Working Path: %s", Self->Path);
   }
   else if (Self->ProcessID) Self->Flags |= TSF::FOREIGN;

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR TASK_NewObject(extTask *Self, APTR Void)
{
   new (Self) extTask;
#ifdef __unix__
   Self->InFD = -1;
   Self->ErrFD = -1;
#endif
   Self->TimeOut = 60 * 60 * 24;
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
Quit: Sends a quit message to a task.

The Quit method can be used as a convenient way of sending a task a quit message.  This will normally result in the
destruction of the task, so long as it is still functioning correctly and has been coded to respond to the
`MSGID_QUIT` message type.  It is legal for a task to ignore a quit request if it is programmed to stay alive.  A task
can be killed outright with the Free action.

-ERRORS-
Okay
-END-

*********************************************************************************************************************/

static ERR TASK_Quit(extTask *Self, APTR Void)
{
   pf::Log log;

   if ((Self->ProcessID) and (Self->ProcessID != glProcessID)) {
      log.msg("Terminating foreign process %d", Self->ProcessID);

      #ifdef __unix__
         kill(Self->ProcessID, SIGHUP); // Safe kill signal - this actually results in that process generating an internal MSGID_QUIT message
      #elif _WIN32
         winTerminateApp(Self->ProcessID, 1000);
      #else
         #warning Add code to kill foreign processes.
      #endif
   }
   else {
      log.branch("Sending QUIT message to self.");
      SendMessage(MSGID_QUIT, MSF::NIL, NULL, 0);
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

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
the existing key value is a number such as `DWORD` or `QWORD`, then the Value will be converted to an integer before the
key is set.

-INPUT-
cstr Name:  The name of the environment variable to set.
cstr Value: The value to assign to the environment variable.

-ERRORS-
Okay
Args
NoSupport: The platform does not support environment variables.
-END-

*********************************************************************************************************************/

static ERR TASK_SetEnv(extTask *Self, struct taskSetEnv *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Name)) return log.warning(ERR::NullArgs);

#ifdef _WIN32

   if (Args->Name[0] IS '\\') {
      LONG ki, len;
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

      log.msg("Registry: %s = %s", Args->Name, Args->Value);

      for (ki=0; ki < ARRAYSIZE(keys); ki++) {
         if (StrCompare(keys[ki].HKey, Args->Name) IS ERR::Okay) {
            CSTRING str = Args->Name + StrLength(keys[ki].HKey); // str = Parasol\Something

            for (len=StrLength(str); (len > 0) and (str[len] != '\\'); len--);

            if (len > 0) {
               std::string path(str, len);
               APTR keyhandle;
               if (!RegOpenKeyExA(keys[ki].ID, path.c_str(), 0, KEY_READ|KEY_WRITE, &keyhandle)) {
                  LONG type;
                  if (!RegQueryValueExA(keyhandle, str+len+1, 0, &type, NULL, NULL)) {
                     // Numerical registry types can be converted into strings

                     switch(type) {
                        case REG_DWORD: {
                           LONG int32 = StrToInt(Args->Value);
                           RegSetValueExA(keyhandle, str+len+1, 0, REG_DWORD, &int32, sizeof(int32));
                           break;
                        }

                        case REG_QWORD: {
                           LARGE int64 = StrToInt(Args->Value);
                           RegSetValueExA(keyhandle, str+len+1, 0, REG_QWORD, &int64, sizeof(int64));
                           break;
                        }

                        default: {
                           RegSetValueExA(keyhandle, str+len+1, 0, REG_SZ, Args->Value, StrLength(Args->Value)+1);
                        }
                     }
                  }
                  else RegSetValueExA(keyhandle, str+len+1, 0, REG_SZ, Args->Value, StrLength(Args->Value)+1);

                  winCloseHandle(keyhandle);
               }

               return ERR::Okay;
            }
            else return log.warning(ERR::Syntax);
         }
      }

      return log.warning(ERR::Failed);
   }
   else {
      winSetEnv(Args->Name, Args->Value);
      return ERR::Okay;
   }

#elif __unix__

   if (Args->Value) setenv(Args->Name, Args->Value, 1);
   else unsetenv(Args->Name);
   return ERR::Okay;

#else

   #warn Write support for SetEnv()
   return ERR::NoSupport;

#endif
}

/*********************************************************************************************************************
-ACTION-
SetVar: Variable fields are supported for the general storage of program variables.
-END-
*********************************************************************************************************************/

static ERR TASK_SetVar(extTask *Self, struct acSetVar *Args)
{
   if ((!Args) or (!Args->Field) or (!Args->Value)) return ERR::NullArgs;

   Self->Fields[Args->Field] = Args->Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-ACTION-
Write: Send raw data to a launched process' stdin descriptor.

If a process is successfully launched with the `PIPE` set in #Flags, data can be sent to its stdin pipe by calling the
Write action.  Setting the Buffer parameter to NULL will result in the pipe being closed (this will signal to the
process that no more data is incoming).

*********************************************************************************************************************/

static ERR TASK_Write(extTask *Task, struct acWrite *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

#ifdef _WIN32
   if (Task->Platform) {
      if (auto winerror = winWriteStd(Task->Platform, Args->Buffer, Args->Length); !winerror) {
         return ERR::Okay;
      }
      else return log.warning(ERR::Write);
   }
   else return log.warning(ERR::Failed);
#else
   return log.warning(ERR::NoSupport);
#endif
}

/*********************************************************************************************************************

-FIELD-
Actions: Used to gain direct access to a task's actions.

This field provides direct access to the actions of a task.  You can use it in the development of an executable program
to hook into the Core action system.  This allows you to create a program that blends in seamlessly with the
system's object oriented design.

The Actions field itself points to a list of action routines that are arranged into a lookup table, sorted by action ID.
You can hook into an action simply by writing to its index in the table with a pointer to the routine that you want to
use for that action.  For example:

<pre>
if (!AccessObject(CurrentTask(), 5000, &task)) {
   task->getPtr(FID_Actions, &amp;actions);
   actions[AC_Seek] = PROGRAM_Seek;
   ReleaseObject(task);
}
</pre>

*********************************************************************************************************************/

static ERR GET_Actions(extTask *Self, struct ActionEntry **Value)
{
   *Value = Self->Actions;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Args: Command line arguments (string format).

This field allows command line arguments to be set using a single string, whereby each value is separated by whitespace.
The string will be disassembled and the arguments will be available to read from the #Parameters field.

If an argument needs to include whitespace, use double-quotes to encapsulate the value.

*********************************************************************************************************************/

static ERR SET_Args(extTask *Self, CSTRING Value)
{
   if ((!Value) or (!*Value)) return ERR::Okay;

   while (*Value) {
      while (*Value <= 0x20) Value++; // Skip whitespace

      if (*Value) { // Extract the argument
         char buffer[400];
         LONG i;
         for (i=0; (*Value) and (*Value > 0x20) and ((size_t)i < sizeof(buffer)-1);) {
            if (*Value IS '"') {
               Value++;
               while (((size_t)i < sizeof(buffer)-1) and (*Value) and (*Value != '"')) {
                  buffer[i++] = *Value++;
               }
               if (*Value IS '"') Value++;
            }
            else buffer[i++] = *Value++;
         }
         buffer[i] = 0;

         if (*Value) while (*Value > 0x20) Value++;

         struct taskAddArgument add = { .Argument = buffer };
         Action(MT_TaskAddArgument, Self, &add);
      }
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
ExitCallback: The callback is activated when the process is terminated.

The ExitCallback field can be set with a function reference that will be called when the executed process is
terminated.  The callback must follow the synopsis `Function(*Task)`.

Please keep in mind that if the Task is freed when the process is still executing, the ExitCallback routine will not be
called on termination because the Task object no longer exists for the control of the process.

*********************************************************************************************************************/

static ERR GET_ExitCallback(extTask *Self, FUNCTION **Value)
{
   if (Self->ExitCallback.defined()) {
      *Value = &Self->ExitCallback;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_ExitCallback(extTask *Self, FUNCTION *Value)
{
   if (Value) Self->ExitCallback = *Value;
   else Self->ExitCallback.clear();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
ErrorCallback: This callback returns incoming data from STDERR.

The ErrorCallback field can be set with a function reference that will be called when an active process sends data via
STDERR.  The callback must follow the synopsis `Function(*Task, APTR Data, LONG Size)`

The information read from STDERR will be returned in the Data pointer and the byte-length of the data will be
indicated by the Size.  The data pointer is temporary and will be invalid once the callback function has returned.

*********************************************************************************************************************/

static ERR GET_ErrorCallback(extTask *Self, FUNCTION **Value)
{
   if (Self->ErrorCallback.defined()) {
      *Value = &Self->ErrorCallback;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_ErrorCallback(extTask *Self, FUNCTION *Value)
{
   if (Value) Self->ErrorCallback = *Value;
   else Self->ErrorCallback.clear();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
InputCallback: This callback returns incoming data from STDIN.

The InputCallback field is available for use only when the Task object represents the current process.
The referenced function will be called when process receives data from STDIN.  The callback must follow the
synopsis `Function(*Task, APTR Data, LONG Size, ERR Status)`

The information read from STDOUT will be returned in the Data pointer and the byte-length of the data will be indicated
by the Size.  The data buffer is temporary and will be invalid once the callback function has returned.

A status of ERR::Finished is sent if the stdinput handle has been closed.

*********************************************************************************************************************/

static ERR GET_InputCallback(extTask *Self, FUNCTION **Value)
{
   if (Self->InputCallback.defined()) {
      *Value = &Self->InputCallback;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_InputCallback(extTask *Self, FUNCTION *Value)
{
   if (Self != glCurrentTask) return ERR::Failed;

   if (Value) {
      #ifdef __unix__
      fcntl(fileno(stdin), F_SETFL, fcntl(fileno(stdin), F_GETFL) | O_NONBLOCK);
      if (auto error = RegisterFD(fileno(stdin), RFD::READ, &task_stdinput_callback, Self); error IS ERR::Okay) {
      #elif _WIN32
      if (auto error = RegisterFD(winGetStdInput(), RFD::READ, &task_stdinput_callback, Self); error IS ERR::Okay) {
      #endif
         Self->InputCallback = *Value;
      }
      else return error;
   }
   else {
      #ifdef _WIN32
      if (Self->InputCallback.Type) RegisterFD(winGetStdInput(), RFD::READ|RFD::REMOVE, &task_stdinput_callback, Self);
      #else
      if (Self->InputCallback.Type) RegisterFD(fileno(stdin), RFD::READ|RFD::REMOVE, &task_stdinput_callback, Self);
      #endif
      Self->InputCallback.clear();
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
OutputCallback: This callback returns incoming data from STDOUT.

The OutputCallback field can be set with a function reference that will be called when an active process sends data via
STDOUT.  The callback must follow the synopsis `Function(*Task, APTR Data, LONG Size)`

The information read from STDOUT will be returned in the Data pointer and the byte-length of the data will be indicated
by the Size.  The data pointer is temporary and will be invalid once the callback function has returned.

*********************************************************************************************************************/

static ERR GET_OutputCallback(extTask *Self, FUNCTION **Value)
{
   if (Self->OutputCallback.defined()) {
      *Value = &Self->OutputCallback;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_OutputCallback(extTask *Self, FUNCTION *Value)
{
   if (Value) Self->OutputCallback = *Value;
   else Self->OutputCallback.clear();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Flags: Optional flags.
Lookup: TSF

-FIELD-
LaunchPath: Launched executables will start in the path specified here.

Use the LaunchPath field to specify the folder that a launched executable will start in when the task object is
activated.  This will override all other path options, such as the RESET_PATH flag.

*********************************************************************************************************************/

static ERR GET_LaunchPath(extTask *Self, STRING *Value)
{
   *Value = Self->LaunchPath;
   return ERR::Okay;
}

static ERR SET_LaunchPath(extTask *Self, CSTRING Value)
{
   pf::Log log;

   if (Self->LaunchPath) { FreeResource(Self->LaunchPath); Self->LaunchPath = NULL; }

   if ((Value) and (*Value)) {
      LONG i;
      for (i=0; Value[i]; i++);
      if (AllocMemory(i+1, MEM::STRING|MEM::NO_CLEAR, (void **)&Self->LaunchPath, NULL) IS ERR::Okay) {
         CopyMemory(Value, Self->LaunchPath, i+1);
      }
      else return log.warning(ERR::AllocMemory);
   }
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Location: Location of an executable file to launch.

When a task object is activated, the Location field will be checked for a valid filename.  If the path is valid, the
executable code will be loaded from this source.  The source must be in an executable format recognised by the
native platform.

Leading spaces will be ignored by the string parser.  The Location string can be enclosed with quotes, in which case
only the quoted portion of the string will be used as the source path.

*********************************************************************************************************************/

static ERR GET_Location(extTask *Self, STRING *Value)
{
   *Value = Self->Location;
   return ERR::Okay;
}

static ERR SET_Location(extTask *Self, CSTRING Value)
{
   pf::Log log;

   if (Self->Location) { FreeResource(Self->Location); Self->Location = NULL; }

   if ((Value) and (*Value)) {
      LONG i;
      for (i=0; Value[i]; i++);
      if (AllocMemory(i+1, MEM::STRING|MEM::NO_CLEAR, (void **)&Self->Location, NULL) IS ERR::Okay) {
         while ((*Value) and (*Value <= 0x20)) Value++;
         if (*Value IS '"') {
            Value++;
            i = 0;
            while ((*Value) and (*Value != '"')) Self->Location[i++] = *Value++;
         }
         else for (i=0; *Value; i++) Self->Location[i] = *Value++;
         Self->Location[i] = 0;
      }
      else return log.warning(ERR::AllocMemory);
   }
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Name: Name of the task.

This field specifies the name of the Task or program that has been initialised. It is up to the developer of the
program to set the Name which will appear in this field.  If there is no name for the task then the system may
assign a randomly generated name.

*********************************************************************************************************************/

static ERR GET_Name(extTask *Self, STRING *Value)
{
   *Value = Self->Name;
   return ERR::Okay;
}

static ERR SET_Name(extTask *Self, CSTRING Value)
{
   StrCopy(Value, Self->Name, sizeof(Self->Name));
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Parameters: Command line arguments (list format).

Command line arguments for a program can be defined here as a vector list, whereby each argument is an independent
string.  To illustrate, the following command-line string:

<pre>1&gt; YourProgram PREFS MyPrefs -file "documents:readme.txt"</pre>

Would be represented as follows:

<pre>
pf::vector&lt;std::string&gt; Args = {
   "PREFS",
   "MyPrefs",
   "-file",
   "documents:readme.txt"
};
</pre>

*********************************************************************************************************************/

static ERR GET_Parameters(extTask *Self, pf::vector<std::string> **Value, LONG *Elements)
{
   *Value = &Self->Parameters;
   *Elements = Self->Parameters.size();
   return ERR::Okay;
}

static ERR SET_Parameters(extTask *Self, const pf::vector<std::string> *Value, LONG Elements)
{
   if (Value) Self->Parameters = Value[0];
   else Self->Parameters.clear();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
ProcessID: Reflects the process ID when an executable is launched.

If a task object launches an executable file via Activate(), the ProcessID will be set to the 'pid' that was assigned
to the new process by the host system.  At all other times the ProcessID is set to zero.

-FIELD-
Path: The current working folder of the active process.

The Path specifies the 'working folder' that determines where files are loaded from when an absolute path is not
otherwise specified for file access.  Initially the working folder is usually set to the folder of the parent
process, such as that of a terminal shell.

The working folder can be changed at any time by updating the Path with a new folder location.  If changing to the
new folder fails for any reason, the working folder will remain unchanged and the path value will not be updated.

*********************************************************************************************************************/

static ERR GET_Path(extTask *Self, STRING *Value)
{
   *Value = Self->Path;
   return ERR::Okay;
}

static ERR SET_Path(extTask *Self, CSTRING Value)
{
   STRING new_path = NULL;

   pf::Log log;

   log.trace("ChDir: %s", Value);

   ERR error = ERR::Okay;
   if ((Value) and (*Value)) {
      LONG len = StrLength(Value);
      while ((len > 1) and (Value[len-1] != '/') and (Value[len-1] != '\\') and (Value[len-1] != ':')) len--;
      if (AllocMemory(len+1, MEM::STRING|MEM::NO_CLEAR, (void **)&new_path, NULL) IS ERR::Okay) {
         CopyMemory(Value, new_path, len);
         new_path[len] = 0;

#ifdef __unix__
         STRING path;
         if (!ResolvePath(new_path, RSF::NO_FILE_CHECK, &path)) {
            if (chdir(path)) {
               error = ERR::InvalidPath;
               log.msg("Failed to switch current path to: %s", path);
            }
            FreeResource(path);
         }
         else error = log.warning(ERR::ResolvePath);
#elif _WIN32
         STRING path;
         if (ResolvePath(new_path, RSF::NO_FILE_CHECK|RSF::PATH, &path) IS ERR::Okay) {
            if (chdir(path)) {
               error = ERR::InvalidPath;
               log.msg("Failed to switch current path to: %s", path);
            }
            FreeResource(path);
         }
         else error = log.warning(ERR::ResolvePath);
#else
#warn Support required for changing the current path.
#endif
      }
      else error = log.warning(ERR::AllocMemory);
   }
   else error = ERR::EmptyString;

   if (error IS ERR::Okay) {
      if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
      Self->Path = new_path;
   }
   else if (new_path) FreeResource(new_path);

   return error;
}

/*********************************************************************************************************************

-FIELD-
ProcessPath: The path of the executable that is associated with the task.

The ProcessPath is set to the path of the executable file that is associated with the task.  It is managed internally
and cannot be altered.

In Microsoft Windows it is not always possible to determine the origins of an executable, in which case the
ProcessPath is set to the working folder in use at the time the process was launched.

*********************************************************************************************************************/

static ERR GET_ProcessPath(extTask *Self, CSTRING *Value)
{
   *Value = Self->ProcessPath;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Priority: The task priority in relation to other tasks is be defined here.

Set the Priority field to change the priority of the process associated with the task.  The default value for all
processes is zero.  High positive values will give the process more CPU time while negative values will yield
CPU time to other active processes.

Note that depending on the platform, there may be limits as to whether one process can change the priority level
of a foreign process.  Other factors such as the scheduler used by the host system should be considered in the
effect of prioritisation.

*********************************************************************************************************************/

static ERR SET_Priority(extTask *Self, LONG Value)
{
#ifdef __unix__
   LONG priority, unused;
   priority = -getpriority(PRIO_PROCESS, 0);
   unused = nice(-(Value - priority));
#endif
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
ReturnCode: The task's return code can be retrieved following execution.

Once a process has completed execution then its return code can be read from this field.  If process is still running,
the error code ERR::TaskStillExists will be returned.

-ERRORS-
Okay
TaskStillExists: The task is still running and has no return code at this stage.
DoesNotExist: The task is yet to be successfully launched with the Activate action.

*********************************************************************************************************************/

static ERR GET_ReturnCode(extTask *Self, LONG *Value)
{
   pf::Log log;

   if (Self->ReturnCodeSet) {
      *Value = Self->ReturnCode;
      return ERR::Okay;
   }

   if (!Self->ProcessID) {
      log.msg("Task hasn't been launched yet.");
      return ERR::DoesNotExist;
   }

#ifdef __unix__
   // Please note that ProcessMessages() will typically kill zombie processes.  This means waitpid() may not return the
   // status (although it will inform us that the task no longer exists).  This issue is resolved by getting
   // ProcessMessages() to set the ReturnCode field when it detects zombie tasks.

   LONG status = 0;
   LONG result = waitpid(Self->ProcessID, &status, WNOHANG);

   if ((result IS -1) or (result IS Self->ProcessID)) {
      // The process has exited.  Find out what error code was returned and pass it as the result.

      if (WIFEXITED(status)) {
         Self->ReturnCode = (BYTE)WEXITSTATUS(status);
         Self->ReturnCodeSet = true;
      }

      *Value = Self->ReturnCode;
      return ERR::Okay;
   }
   else return ERR::TaskStillExists;

#elif _WIN32

   winGetExitCodeProcess(Self->Platform, &Self->ReturnCode);
   if (Self->ReturnCode IS 259) return ERR::TaskStillExists;
   else {
      Self->ReturnCodeSet = true;
      *Value = Self->ReturnCode;
      return ERR::Okay;
   }

#else

   return ERR::NoSupport;

#endif
}

static ERR SET_ReturnCode(extTask *Self, LONG Value)
{
   Self->ReturnCodeSet = true;
   Self->ReturnCode = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
TimeOut: Limits the amount of time to wait for a launched process to return.

This field can be set in conjunction with the `WAIT` flag to define the time limit when waiting for a launched
process to return.  The time out is defined in seconds.

*********************************************************************************************************************/

static const FieldArray clFields[] = {
   { "TimeOut",         FDF_DOUBLE|FDF_RW },
   { "Flags",           FDF_LONGFLAGS|FDF_RI, NULL, NULL, &clFlags },
   { "ReturnCode",      FDF_LONG|FDF_RW, GET_ReturnCode, SET_ReturnCode },
   { "ProcessID",       FDF_LONG|FDF_RI },
   // Virtual fields
   { "Actions",        FDF_POINTER|FDF_R,  GET_Actions },
   { "Args",           FDF_STRING|FDF_W,   NULL, SET_Args },
   { "Parameters",     FDF_CPP|FDF_ARRAY|FDF_STRING|FDF_RW, GET_Parameters, SET_Parameters },
   { "ErrorCallback",  FDF_FUNCTIONPTR|FDF_RI, GET_ErrorCallback,   SET_ErrorCallback }, // STDERR
   { "ExitCallback",   FDF_FUNCTIONPTR|FDF_RW, GET_ExitCallback,    SET_ExitCallback },
   { "InputCallback",  FDF_FUNCTIONPTR|FDF_RW, GET_InputCallback,   SET_InputCallback }, // STDIN
   { "LaunchPath",     FDF_STRING|FDF_RW,      GET_LaunchPath,      SET_LaunchPath },
   { "Location",       FDF_STRING|FDF_RW,      GET_Location,        SET_Location },
   { "Name",           FDF_STRING|FDF_RW,      GET_Name,            SET_Name },
   { "OutputCallback", FDF_FUNCTIONPTR|FDF_RI, GET_OutputCallback,  SET_OutputCallback }, // STDOUT
   { "Path",           FDF_STRING|FDF_RW,      GET_Path,            SET_Path },
   { "ProcessPath",    FDF_STRING|FDF_R,       GET_ProcessPath },
   { "Priority",       FDF_LONG|FDF_W,         NULL, SET_Priority },
   // Synonyms
   { "Src",            FDF_SYNONYM|FDF_STRING|FDF_RW, GET_Location, SET_Location },
   END_FIELD
};

//********************************************************************************************************************

extern "C" ERR add_task_class(void)
{
   glTaskClass = objMetaClass::create::global(
      fl::ClassVersion(VER_TASK),
      fl::Name("Task"),
      fl::Category(CCF::SYSTEM),
      fl::FileExtension("*.exe|*.bat|*.com"),
      fl::FileDescription("Executable File"),
      fl::FileHeader("[0:$4d5a]|[0:$7f454c46]"),
      fl::Actions(clActions),
      fl::Methods(clTaskMethods),
      fl::Fields(clFields),
      fl::Size(sizeof(extTask)),
      fl::Path("modules:core"));

   return glTaskClass ? ERR::Okay : ERR::AddClass;
}
