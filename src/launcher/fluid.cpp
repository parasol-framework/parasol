
#include <parasol/main.h>
#include <startup.h>
#include <parasol/modules/core.h>
#include <string.h>

#include "common.h"

CSTRING ProgName      = "Fluid";
CSTRING ProgAuthor    = "Paul Manias";
CSTRING ProgDate      = "February 2022";
CSTRING ProgCopyright = "Copyright Paul Manias © 2000-2022";

extern struct CoreBase *CoreBase;

static CSTRING *glArgs = NULL;
static BYTE glTime = FALSE;
static STRING glProcedure = NULL;
static STRING glTargetFile = NULL;

static const char glHelp[] = {
"Usage: fluid [options...] script.fluid [--arg1=v1 --arg2=v2 ...]\n\
\n\
Special options are:\n\
\n\
 --procedure [n] The name of a procedure in the script to execute.\n\
 --time          Print the amount of time that it took to execute the program.\n\
 --log-info      Print log messages at INFO level.\n\
 --log-error     Print log messages at ERROR level.\n\
 --log-all       Print all log messages.\n\
 \n\
 If no script file is specified, the script will be parsed from std input after an EOF is received.\n\
 \n\
 All parameters following the script file are passed through as arguments to the program.\n\
 Arrays can be passed in the format key={ value1 value2 }\n"
};

//********************************************************************************************************************

static void set_script_args(objScript *Script, CSTRING *Args)
{
   char argbuffer[100];
   STRING argname = argbuffer;

   for (ULONG i=0; Args[i]; i++) {
      ULONG j = 0, k = 0;
      while (Args[i][k] IS '-') k++;
      while ((Args[i][k]) and (Args[i][k] != '=') and (j < sizeof(argbuffer)-10)) {
         argname[j++] = Args[i][k++];
      }
      argname[j] = 0;
      LONG al = j;

      CSTRING value = NULL;
      if (Args[i][k] IS '=') {
         value = Args[i] + k + 1;
      }
      else if (!Args[i+1]) {
         SetVar(Script, argname, "1");
         continue;
      }
      else if ((Args[i+1][0] IS '-') and (Args[i+1][1] IS '-')) {
         SetVar(Script, argname, "1");
         continue;
      }
      else value = Args[++i];

      if ((value[0] IS '{') and (value[1] <= 0x20)) {
         // Array definition, e.g. files={ file1.txt file2.txt }
         // This will be converted to files(0)=file.txt files(1)=file2.txt

         i++;
         LONG arg_index = 0;
         while ((Args[i]) and (Args[i][0] != '}')) {
            StrFormat(argname+al, sizeof(argbuffer)-al, "(%d)", arg_index);
            SetVar(Script, argname, Args[i]);
            arg_index++;
            i++;
         }
         if (!Args[i]) break;

         // Note that the last arg in the array will be the "}" that closes it

         char array_size[16];
         StrCopy(":size", argname+al, sizeof(argbuffer)-al);
         IntToStr(arg_index, array_size, sizeof(array_size));
         SetVar(Script, argname, array_size);
      }
      else SetVar(Script, argname, value);
   }
}

//********************************************************************************************************************

static LONG run_script(objScript *Script)
{
   parasol::Log log(__FUNCTION__);
   DOUBLE start_time = (DOUBLE)PreciseTime() / 1000000.0;
   ERROR error;

   if (!(error = acInit(Script))) {
      if (!(error = acActivate(Script))) {
         if (glTime) { // Print the execution time of the script
            DOUBLE end_time = (DOUBLE)PreciseTime() / 1000000.0;
            print("Script executed in %f seconds.\n\n", end_time - start_time);
         }

         if (Script->Error) {
            log.msg("Script returned an error code of %d: %s", Script->Error, GetErrorMsg(Script->Error));
            return -1;
         }

         STRING msg;
         if ((!Script->get(FID_ErrorString, &msg)) and (msg)) {
            log.msg("Script returned error message: %s", msg);
            return -1;
         }
         else return 0;
      }
      else print("Script failed during processing.  Use the --log-error option to examine the failure.");
   }
   else print("Failed to load / initialise the script: %s", GetErrorMsg(error));

   return -1;
}

//********************************************************************************************************************
// Executes the target.

static LONG exec_source(CSTRING TargetFile, CSTRING Procedure)
{
   CLASSID class_id, subclass;
   if (IdentifyFile(TargetFile, "Open", 0, &class_id, &subclass, NULL)) {
      subclass = ID_FLUID;
      class_id = ID_SCRIPT;
   }

   if (subclass != ID_FLUID) return -1;

   objScript *script;
   if (!NewObject(ID_FLUID, 0, &script)) {
      SetFields(script, FID_Path|TSTR,      TargetFile,
                        FID_Procedure|TSTR, Procedure,
                        TAGEND);

      if (glArgs) set_script_args(script, glArgs);

      return run_script(script);
   }
   else {
      print("Internal Failure: Failed to create a new Script object for file processing.");
      return -1;
   }
}

//********************************************************************************************************************
// Process arguments

static ERROR process_args(void)
{
   CSTRING *args;
   LONG i;

   if ((!CurrentTask()->getPtr(FID_Parameters, &args)) and (args)) {
      for (i=0; args[i]; i++) {
         if (!StrMatch(args[i], "--help")) {
            // Print help for the user
            print(glHelp);
            return ERR_Terminate;
         }
         else if (!StrMatch(args[i], "--verify")) {
            // Special internal function that checks that the installation is valid, returning 1 if checks pass.

            static CSTRING modules[] = { // These modules must be present for an installation to be valid.
               "display", "document", "fluid", "font", "http", "jpeg", "json", "network", "parc",
               "picture", "surface", "svg", "vector", "widget", "window", "xml"
            };

            struct DirInfo *dir;
            LONG total = 0;
            if (!OpenDir("modules:", RDF_QUALIFY, &dir)) {
               while (!ScanDir(dir)) {
                  struct FileInfo *folder = dir->Info;
                  if (folder->Flags & RDF_FILE) {
                     LONG m;
                     for (m=0; m < ARRAYSIZE(modules); m++) {
                        if (!StrCompare(modules[m], folder->Name, 0, 0)) total++;
                     }
                  }
               }
               FreeResource(dir);
            }

            if (total >= ARRAYSIZE(modules)) print("1");
            return ERR_Terminate;
         }
         else if (!StrMatch(args[i], "--time")) {
            glTime = TRUE;
         }
         else if (!StrMatch(args[i], "--procedure")) {
            if (glProcedure) { FreeResource(glProcedure); glProcedure = NULL; }

            if (args[i+1]) {
               glProcedure = StrClone(args[i+1]);
               i++;
            }
         }
         else {
            if ((args[i][0] IS '-') and (args[i][1] IS '-')) {
               glArgs = args + i;
            }
            else {
               // Assume this arg is the target file.

               if (ResolvePath(args[i], RSF_APPROXIMATE, &glTargetFile)) {
                  print("Unable to find file '%s'", args[i]);
                  return ERR_Terminate;
               }

               if (args[i+1]) glArgs = args + i + 1;
            }

            break;
         }
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// Support for stdin

static STRING glScriptBuffer     = NULL;
static LONG glScriptBufferSize   = 0;
static LONG glScriptBufferLength = 0;
static LONG glScriptReceivedMsg  = 0;

static void read_stdin(objTask *Task, APTR Buffer, LONG Size, ERROR Status)
{
   parasol::Log log(__FUNCTION__);

   if (Status IS ERR_Finished) {
      SendMessage(0, glScriptReceivedMsg, MSF_WAIT, NULL, 0);
      log.msg("Input pipe closed.");
      return;
   }

   if (!glScriptBuffer) {
      glScriptBufferSize = Size + 1;
      if (!AllocMemory(glScriptBufferSize, MEM_STRING, &glScriptBuffer, NULL)) {
         CopyMemory(Buffer, glScriptBuffer, glScriptBufferSize);
         glScriptBufferLength = Size;
      }
      else SendMessage(0, MSGID_QUIT, MSF_WAIT, NULL, 0);
   }
   else if (glScriptBufferLength + Size >= glScriptBufferSize-1) {
      LONG inc = (Size < 4096) ? 4096 : Size;
      if (!ReallocMemory(glScriptBuffer, glScriptBufferSize + inc, &glScriptBuffer, NULL)) {
         CopyMemory(Buffer, glScriptBuffer + glScriptBufferLength, Size);
         glScriptBufferSize += inc;
         glScriptBufferLength += Size;
         glScriptBuffer[glScriptBufferLength] = 0;
      }
      else SendMessage(0, MSGID_QUIT, MSF_WAIT, NULL, 0);
   }
   else {
      CopyMemory(Buffer, glScriptBuffer + glScriptBufferLength, Size);
      glScriptBufferLength += Size;
      glScriptBuffer[glScriptBufferLength] = 0;
   }

   if (glScriptBuffer[glScriptBufferLength-1] IS 0x1a) { // Ctrl-Z
      glScriptBuffer[glScriptBufferLength-1] = 0;
      SendMessage(0, glScriptReceivedMsg, MSF_WAIT, NULL, 0);
      log.msg("EOF received.");
      return;
   }
}

//********************************************************************************************************************

static ERROR msg_script_received(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   return ERR_Terminate;
}

//********************************************************************************************************************

int main(int argc, CSTRING *argv)
{
   const char *msg = init_parasol(argc, argv);
   if (msg) {
      for (int i=1; i < argc; i++) { // If in --verify mode, return with no error code and print nothing.
         if (!strcmp(argv[i], "--verify")) return 0;
      }
      print(msg);
      return -1;
   }

   int result = 0;
   if (!process_args()) {
      if (glTargetFile) {
         LONG type;
         if ((AnalysePath(glTargetFile, &type) != ERR_Okay) or (type != LOC_FILE)) {
            print("File '%s' does not exist.", glTargetFile);
            result = -1;
         }
         else result = exec_source(glTargetFile, glProcedure);
      }
      else {
         // Read script from std input

         glScriptReceivedMsg = AllocateID(IDTYPE_MESSAGE);

         auto call = make_function_stdc(msg_script_received);
         AddMsgHandler(NULL, glScriptReceivedMsg, &call, NULL);

         call = make_function_stdc(read_stdin);
         CurrentTask()->set(FID_InputCallback, &call);

         ProcessMessages(0, -1);

         if (glScriptBuffer) {
            objScript *script;
            if (!NewObject(ID_FLUID, 0, &script)) {
               script->set(FID_Statement, glScriptBuffer);
               if (glProcedure) script->set(FID_Procedure, glProcedure);
               if (glArgs) set_script_args(script, glArgs);
               result = run_script(script);
               acFree(script);
            }
            else {
               print("Internal Failure: Failed to create a new Script object for file processing.");
               result = -1;
            }
         }
      }
   }

   if (glScriptBuffer) { FreeResource(glScriptBuffer); glScriptBuffer = NULL; }
   if (glProcedure)    { FreeResource(glProcedure);    glProcedure = NULL; }
   if (glTargetFile)   { FreeResource(glTargetFile);   glTargetFile = NULL; }

   close_parasol();

   return result;
}
