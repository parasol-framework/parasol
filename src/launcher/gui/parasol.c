/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

*****************************************************************************/

#include <parasol/main.h>
#include <startup.h>
#include <parasol/system/task.h>
#include <parasol/data/script.h>
#include <parasol/data/compression.h>
#include <parasol/files/file.h>
#include <parasol/functions/strtoint.c>

#include <parasol/modules/filesystem.h>

CSTRING ProgName = "Parasol";

extern struct CoreBase *CoreBase;
struct FileSystemBase *FileSystemBase;

#define STR_UNPACK "temp:scripts/"
#define STR_MAIN   "main.fluid"

static OBJECTID glTargetID = 0;
static STRING glDirectory = NULL, *glArgs = NULL;
static char glBind[40] = { 0 };

static OBJECTPTR glTask = NULL;
static OBJECTPTR glScript = NULL;

static ERROR decompress_archive(STRING);
static ERROR prep_environment(LONG, LONG, LONG);
static ERROR exec_script(STRING, OBJECTID *, LONG, STRING);
static ERROR PROGRAM_ActionNotify(OBJECTPTR, struct acActionNotify *);

static const char Help[] = {
"This command-line program will execute scripts written for the Parasol framework.  The core distribution\n\
supports Fluid (.fluid) scripts.  Quick start:\n\
\n\
   parasol [args] [script.ext] arg1 arg2 ...\n\
\n\
Available commands:\n\
\n\
 --help      : Prints this help page.\n\
 --procedure : The name of a procedure to execute.\n\
 --program   : Treat the script like a program.  The process will enter message handling mode if execution succeeds.\n\
 --persistent: Similar to -program but ensures that there is no exit until a quit message has been posted to the\n\
               message queue.\n\
 --bind      : Follow this argument with an object name to bind the process to a specific object after script\n\
               execution.  If the object is not found, the program aborts.\n\
 --progonly  : If the script does not explicitly declare itself as a program, abort the execution process.\n\
 --target    : The name of an object that the script will target for the first object's initialisation.  Used in\n\
               multi-tasking environments.\n\
 --time      : Print the amount of time that it took to execute the script.\n\
\n\
 --debug    : Activates run-time debug output.\n\
 --errors   : Activates the output of run-time errors.\n"
};

/*********************************************************************************************************************
** Function: main()
*/

extern "C" void program(void)
{
   parasol::Log log;

   LONG i, j;

   glTask = CurrentTask();
   BYTE time       = FALSE;
   LONG winhandle  = 0;
   STRING procedure  = NULL;
   STRING scriptfile = NULL;
   LONG width      = 0;
   LONG height     = 0;

   FileSystemBase = (struct FileSystemBase *)GetResourcePtr(RES_FILESYSTEM);

   // Process arguments

   STRING *Args;
   if ((!glTask->getPtr(FID_ArgsList, &Args)) and (Args)) {
      for (i=0; Args[i]; i++) {
         if (!StrMatch(Args[i], "--help")) {
            // Print help for the user
            print(Help);
            goto exit;
         }
         else if (!StrMatch(Args[i], "--time")) {
            time = TRUE;
         }
         else if (!StrMatch(Args[i], "--info")) {
            print("Instance: %d", GetResource(RES_INSTANCE));
         }
         else if (!StrMatch(Args[i], "--instance")) {
            glTask->get(FID_Instance, &j);
            print("Instance: %d", j);
         }
         else if (!StrMatch(Args[i], "--winhandle")) { // Target a desktop window in the host environment
            if (Args[i+1]) {
               if ((winhandle = StrToInt(Args[i+1]))) i++;
            }
         }
         else if (!StrMatch(Args[i], "--width")) {
            if (Args[i+1]) {
               if ((width = StrToInt(Args[i+1]))) i++;
            }
         }
         else if (!StrMatch(Args[i], "--height")) {
            if (Args[i+1]) {
               if ((height = StrToInt(Args[i+1]))) i++;
            }
         }
         else if (!StrMatch(Args[i], "--procedure")) {
            if (procedure) { FreeResource(procedure); procedure = NULL; }

            if (Args[i+1]) {
               for (j=0; Args[i+1][j]; j++);
               if (!AllocMemory(j+1, MEM_STRING|MEM_NO_CLEAR, &procedure)) {
                  for (j=0; Args[i+1][j]; j++) procedure[j] = Args[i+1][j];
                  procedure[j] = 0;
               }
               i++;
            }
         }
         else if (!StrMatch(Args[i], "--target")) {
            if (Args[i+1]) {
               if (FindObject(Args[i+1], 0, FOF_SMART_NAMES, &TargetID) != ERR_Okay) {
                  print("Warning - could not find target object \"%s\".", Args[i+1]);
               }
               else log.msg("Using target %d", TargetID);
            }
         }
         else if (!StrMatch(Args[i], "--hash")) {
            if (Args[i+1]) {
               ULONG hash = LCASEHASH(Args[i+1]);
               print("Hash for %s = 0x%.8x\n", Args[i+1], hash);
               i++;
            }
         }
         else {
            // If argument not recognised, assume this arg is the script file.
            if (ResolvePath(Args[i], RSF_APPROXIMATE, &scriptfile)) {
               print("Unable to find file '%s'", Args[i]);
               goto exit;
            }
            if (Args[i+1]) glArgs = Args + i + 1;
            break;
         }
      }
   }

   if (!scriptfile) { // Abort if no script file to load.
      print(Help);
      goto exit;
   }

   STRING path;
   if (!glTask->get(FID_Path, &path)) log.msg("Path: %s", path);
   else log.error("No working path.");

   if (winhandle) {
      if (prep_environment(winhandle, width, height) != ERR_Okay) {
         print("Failed to prepare an environment for running this script.");
         goto exit;
      }
   }

   if ((AnalysePath(scriptfile, &i) != ERR_Okay) or (i != LOC_FILE)) {
      print("File '%s' does not exist.", scriptfile);
      goto exit;
   }

   if (exec_script(scriptfile, time, procedure)) goto exit;

   if (winhandle) acShow(TargetID);

   ProcessMessages(0, 0);

exit:
   log.msg("parasol now exiting...");

   if (CoreObjectID) {
      OBJECTPTR object;
      if (!AccessObjectID(CoreObjectID, 5000, &object)) {
         UnsubscribeAction(object, 0, glTask->UniqueID);
         ReleaseObject(object);
      }
   }

   if (procedure) { FreeResource(procedure); procedure = NULL; }
   if (scriptfile) { FreeResource(scriptfile); scriptfile = NULL; }

   if (glDirectory) {
      for (i=0; glDirectory[i]; i++);
      while ((i > 0) and (glDirectory[i-1] != '/')) i--;
      glDirectory[i] = 0;

      OBJECTPTR file;
      if (!PrivateObject(ID_FILE, 0, &file, FID_Path|TSTR, glDirectory, TAGEND)) {
         flDelete(file, 0);
         acFree(file);
      }

      FreeResource(glDirectory);
   }

   if (glScript) acFree(glScript);
}

/*********************************************************************************************************************
** Prepares a special environment for running scripts.
*/

ERROR prep_environment(LONG WindowHandle, LONG Width, LONG Height)
{
   parasol::Log log(__FUNCTION__);

   log.branch("Win: %d, Size: %dx%d", WindowHandle, Width, Height);

   objSurface::create target = { fl::Name("SystemSurface"), fl::WindowHandle(WindowHandle),
      fl::XCoord(0), fl::YCoord(0), fl::Width(Width), fl::Height(Height) };

   if (target.ok()) {
      objPointer::create pointer = { fl::Owner(target->UID), fl::Name("SystemPointer") }
      if (pointer.ok()) {
         objScript::create script = { fl::Path("templates:defaultvariables.fluid"), fl::Target(target->UID) };
         if (script.ok()) return acActivate(script);
         else return ERR_CreateObject;
      }
      else return ERR_CreateObject;
   }
   else return ERR_CreateObject;
}

/*********************************************************************************************************************
** Runs scripts.
*/

ERROR exec_script(STRING ScriptFile, OBJECTID *CoreObjectID, LONG ShowTime, STRING Procedure)
{
   LONG i, j, k;
   ERROR error;
   BYTE argbuffer[100], *argname;

   *CoreObjectID = 0;

   CLASSID class_id, subclass;
   if (!(error = IdentifyFile(ScriptFile, "Open", 0, &class_id, &subclass, NULL))) {
      if (class_id IS ID_COMPRESSION) {
         // The Fluid source may be a compressed file that contains multiple script files.  This part of the routine will decompress the contents to "temp:scripts/".

         if (decompress_archive(ScriptFile) != ERR_Okay) {
            print("Failed to decompress the script archive.");
            return(ERR_Failed);
         }
         ScriptFile = glDirectory;
         class_id = ID_SCRIPT;
      }
      else if (class_id != ID_SCRIPT) {
         OBJECTPTR run;

         // The script is actually a reference to a data file, in which case we may be able to run it, if it has a file association.

         if (!PrivateObject(ID_RUN, 0, &run, FID_Location|TSTR, ScriptFile, TAGEND)) {
            if (glArgs) {
               argname = argbuffer+1; // Skip the first byte... reserved for '+'
               argbuffer[0] = '+'; // Append arg indicator
               for (i=0; glArgs[i]; i++) {
                  for (j=0; (glArgs[i][j]) and (glArgs[i][j] != '=') and (j < (LONG)sizeof(argbuffer)-10); j++) argname[j] = glArgs[i][j];
                  argname[j] = 0;

                  if (glArgs[i][j] IS '=') {
                     j++;
                     if (glArgs[i][j] IS '{') {
                        // Array definition, e.g. files={ file1.txt file2.txt }

                        // Check in case of no gap, e.g. files={arg1 ... }

                        j++;
                        if (glArgs[i][j] > 0x20) {
                           SetVar(run, argbuffer, glArgs[i] + j);
                        }

                        i++;
                        while ((glArgs[i]) and (glArgs[i][0] != '}')) {
                           SetVar(run, argbuffer, glArgs[i]);
                           i++;
                        }
                     }
                     else if (glArgs[i][j] IS '"') {
                        j++;
                        for (k=j; (glArgs[i][k]) and (glArgs[i][k] != '"'); k++);
                        if (glArgs[i][k] IS '"') glArgs[i][k] = 0;
                        SetVar(run, argname, glArgs[i]+j);
                     }
                     else SetVar(run, argname, glArgs[i]+j);
                  }
                  else SetVar(run, argname, "1");
               }
            }

            acActivate(run);
            acFree(run);
         }

         return(ERR_LimitedSuccess);
      }
   }
   else {
      print("Failed to identify the type of file for path '%s', error: %s.  Assuming ID_SCRIPT.", ScriptFile, GetErrorMsg(error));
      subclass = ID_SCRIPT;
      class_id = ID_SCRIPT;
   }

   if (!NewObject(subclass ? subclass : class_id, 0, &glScript)) {
      if (!TargetID) TargetID = CurrentTaskID();

      SetFields(glScript, FID_Path|TSTR,      ScriptFile,
                          FID_Target|TLONG,   TargetID,
                          FID_Procedure|TSTR, Procedure,
                          TAGEND);

         if (glArgs) {
            argname = argbuffer+1; // Skip the first byte... reserved for '+'
            argbuffer[0] = '+'; // Append arg indicator
            for (i=0; glArgs[i]; i++) {
               for (j=0; (glArgs[i][j]) and (glArgs[i][j] != '=') and (j < (LONG)sizeof(argbuffer)-10); j++) argname[j] = glArgs[i][j];
               argname[j] = 0;

               if (glArgs[i][j] IS '=') {
                  j++;
                  if (glArgs[i][j] IS '{') {
                     // Array definition, e.g. files={ file1.txt file2.txt }

                     j++;
                     if (glArgs[i][j] > 0x20) {
                        SetVar(glScript, argbuffer, glArgs[i] + j);
                     }

                     i++;
                     while ((glArgs[i]) and (glArgs[i][0] != '}')) {
                        SetVar(glScript, argbuffer, glArgs[i]);
                        i++;
                     }
                     if (!glArgs[i]) break;
                     // The last arg in the array will be the } that closes it
                  }
                  else if (glArgs[i][j] IS '"') {
                     j++;
                     for (k=j; (glArgs[i][k]) and (glArgs[i][k] != '"'); k++);
                     if (glArgs[i][k] IS '"') glArgs[i][k] = 0;
                     SetVar(glScript, argname, glArgs[i]+j);
                  }
                  else SetVar(glScript, argname, glArgs[i]+j);
               }
               else SetVar(glScript, argname, "1");
            }
         }

      // Start the timer if requested

      LARGE start_time = 0;
      if (ShowTime) start_time = PreciseTime();

      if (!(error = acInit(glScript))) {
         if (!(error = acActivate(glScript))) {
            if (ShowTime) {
               DOUBLE startseconds = (DOUBLE)start_time / 1000000.0;
               DOUBLE endseconds   = (DOUBLE)PreciseTime() / 1000000.0;
               print("Script executed in %f seconds.\n\n", endseconds - startseconds);
            }
         }
         else {
            print("Script failed during processing.  Use the --debug option to examine the failure.");
            return(ERR_Failed);
         }
      }
      else {
         print("Failed to load / initialise the script.");
         return(ERR_Failed);
      }

      log.msg("Script initialised.");
   }
   else {
      print("Internal Failure: Failed to create a new Script object for file processing.");
      return(ERR_Failed);
   }

   return(ERR_Okay);
}

/*********************************************************************************************************************
** Function: decompress_archive()
*/

static ERROR decompress_archive(STRING Location)
{
   if (!Location) return(ERR_NullArgs);

   LONG len, i, j;
   for (len=0; Location[len]; len++);

   objCompression::create compress = { fl::Path(Location) };
   if (compress.ok()) {
      if (!(error = AllocMemory(sizeof(STR_UNPACK) + len + sizeof(STR_MAIN) + 2, MEM_STRING, &glDirectory))) {
         for (i=0; STR_UNPACK[i]; i++) glDirectory[i] = STR_UNPACK[i];
         for (j=len; (j > 1) and (Location[j-1] != '/') and (Location[j-1] != '\\') and (Location[j-1] != ':'); j--);
         while (Location[j]) {
            glDirectory[i++] = Location[j];
            j++;
         }
         glDirectory[i++] = '/';
         glDirectory[i] = 0;

         if (!(error = cmpDecompressFile(compress, "*", glDirectory, 0))) {
            for (j=0; STR_MAIN[j]; j++) glDirectory[i++] = STR_MAIN[j];
            glDirectory[i] = 0;

            return ERR_Okay;
         }
         else print("Failed to decompress the file contents.");
      }
      else print("Failed to allocate a memory block.");
   }
   else print("Failed to open the compressed file.");

   return ERR_Failed;
}
