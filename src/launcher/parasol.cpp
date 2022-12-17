/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

This version of the Parasol launcher is intended for use from the command-line
only.

*****************************************************************************/

#include <parasol/main.h>
#include <startup.h>
#include <parasol/modules/core.h>
#include <string.h>

#include "common.h"

CSTRING ProgName      = "Parasol";
CSTRING ProgAuthor    = "Paul Manias";
CSTRING ProgDate      = "February 2022";
CSTRING ProgCopyright = "Copyright Paul Manias © 2000-2022";

extern struct CoreBase *CoreBase;

static OBJECTID glTargetID = 0;
static OBJECTID glSystemPointerID;
static CSTRING *glArgs = NULL;
static STRING glAllow = NULL;
static BYTE glSandbox = FALSE;
static BYTE glRelaunched = FALSE;
static OBJECTPTR glTask = NULL;
static OBJECTPTR glScript = NULL;
static BYTE glTime = FALSE;
static LONG glWinHandle = 0;
static STRING glProcedure = NULL;
static STRING glTargetFile = NULL;
static LONG glWidth = 0;
static LONG glHeight = 0;

static ERROR prep_environment(LONG, LONG, LONG);
static ERROR exec_source(CSTRING, LONG, CSTRING);

static const char glHelp[] = {
"This command-line program can execute scripts and PARC files developed for the Parasol framework.  The\n\
Fluid scripting language (.fluid files) are supported in the default distribution.  Quick start:\n\
\n\
   parasol [args] [script.ext] arg1 arg2 ...\n\
\n\
The following parameters can be used when executing script files:\n\
\n\
 --procedure [n] The name of a procedure to execute.\n\
 --target    [n] The name of an existing object that the script will target for hosting.  Used in multi-tasking\n\
                 environments.\n\
 --time          Print the amount of time that it took to execute the program.\n\
 --sandbox       Applies the PARC sand-boxing feature when executing scripts.\n\
 --allow         For sand-boxed execution, specifies additional access rights and limitations.\n\
\n\
 --log-info      Activates run-time log messages at INFO level.\n\
 --log-error     Activates run-time log messages at ERROR level.\n"
};

//****************************************************************************

#include "exec.cpp"

//****************************************************************************
// Process arguments

static ERROR process_args(void)
{
   parasol::Log log("Parasol");
   CSTRING *args;
   LONG i, j;

   if ((!GetPointer(glTask, FID_Parameters, &args)) AND (args)) {
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
         else if (!StrMatch(args[i], "--sandbox")) {
            glSandbox = TRUE;
         }
         else if (!StrMatch(args[i], "--time")) {
            glTime = TRUE;
         }
         else if (!StrMatch(args[i], "--instance")) {
            GetLong(glTask, FID_Instance, &j);
            print("Instance: %d", j);
         }
         else if (!StrMatch(args[i], "--winhandle")) { // Target a desktop window in the host environment
            if (args[i+1]) {
               if ((glWinHandle = StrToInt(args[i+1]))) i++;
            }
         }
         else if (!StrMatch(args[i], "--width")) {
            if (args[i+1]) {
               if ((glWidth = StrToInt(args[i+1]))) i++;
            }
         }
         else if (!StrMatch(args[i], "--height")) {
            if (args[i+1]) {
               if ((glHeight = StrToInt(args[i+1]))) i++;
            }
         }
         else if (!StrMatch(args[i], "--procedure")) {
            if (glProcedure) { FreeResource(glProcedure); glProcedure = NULL; }

            if (args[i+1]) {
               glProcedure = StrClone(args[i+1]);
               i++;
            }
         }
         else if (!StrMatch(args[i], "--target")) {
            if (args[i+1]) {
               LONG count = 1;
               if (FindObject(args[i+1], 0, FOF_INCLUDE_SHARED|FOF_SMART_NAMES, &glTargetID, &count) != ERR_Okay) {
                  print("Warning - could not find target object \"%s\".", args[i+1]);
               }
               else log.msg("Using target %d", glTargetID);
            }
         }
         else if (!StrMatch(args[i], "--relaunch")) {
            glRelaunched = TRUE;
         }
         else {
            // If argument not recognised, assume this arg is the target file.
            if (ResolvePath(args[i], RSF_APPROXIMATE, &glTargetFile)) {
               print("Unable to find file '%s'", args[i]);
               return ERR_Terminate;
            }
            if (args[i+1]) glArgs = args + i + 1;
            break;
         }
      }
   }

   return ERR_Okay;
}

//****************************************************************************
// Main entry point

int main(int argc, CSTRING *argv)
{
   parasol::Log log("Parasol");

   const char *msg = init_parasol(argc, argv);
   if (msg) {
      int i;
      for (i=1; i < argc; i++) { // If in --verify mode, return with no error code and print nothing.
         if (!strcmp(argv[i], "--verify")) return 0;
      }
      print(msg);
      return -1;
   }

   glTask = CurrentTask();

   if (!process_args()) {
      if (glTargetFile) {
         STRING path;
         if (!GetString(glTask, FID_Path, &path)) log.msg("Path: %s", path);
         else log.error("No working path.");

         if (glWinHandle) {
            if (prep_environment(glWinHandle, glWidth, glHeight) != ERR_Okay) {
               print("Failed to prepare an environment for running this script.");
               goto exit;
            }
         }

         if (!AnalysePath("parasol:pictures/", NULL)) {
            SetVolume(AST_NAME, "pictures", AST_PATH, "parasol:pictures/", AST_FLAGS, VOLUME_REPLACE, AST_ICON, "misc/picture",  TAGEND);
         }

         if (!AnalysePath("parasol:programs/", NULL)) {
            SetVolume(AST_NAME, "programs", AST_PATH, "parasol:programs/", AST_FLAGS, VOLUME_REPLACE, AST_ICON, "items/launch",  TAGEND);
         }

         LONG type;
         if ((AnalysePath(glTargetFile, &type) != ERR_Okay) or (type != LOC_FILE)) {
            print("File '%s' does not exist.", glTargetFile);
         }
         else exec_source(glTargetFile, glTime, glProcedure);
      }
      else print(glHelp);
   }

exit:
   log.msg("parasol now exiting...");

   if (glProcedure) { FreeResource(glProcedure); glProcedure = NULL; }
   if (glTargetFile) { FreeResource(glTargetFile); glTargetFile = NULL; }
   if (glScript) acFree(glScript);

   close_parasol();

   return 0;
}

//****************************************************************************
// This function targets an existing OS window handle and prepares it for running Parasol.

ERROR prep_environment(LONG WindowHandle, LONG Width, LONG Height)
{
   parasol::Log log("Parasol");
   log.branch("Win: %d, Size: %dx%d", WindowHandle, Width, Height);

   OBJECTPTR target;
   ERROR error;
   if (!NewLockedObject(ID_SURFACE, 0, &target, &glTargetID)) {
      SetFields(target,
         FID_Name|TSTR,    "SystemSurface",
         FID_WindowHandle|TLONG, WindowHandle,
         FID_X|TLONG,      0,
         FID_Y|TLONG,      0,
         FID_Width|TLONG,  Width,
         FID_Height|TLONG, Height,
         TAGEND);
      if (!acInit(target)) {
         OBJECTPTR pointer;
         if (!NewLockedObject(ID_POINTER, NF_PUBLIC, &pointer, &glSystemPointerID)) {
            SetFields(pointer,
               FID_Owner|TLONG,  glTargetID,
               FID_Name|TSTRING, "SystemPointer",
               TAGEND);
            if (!acInit(pointer)) {
               OBJECTPTR script;
               if (!CreateObject(ID_SCRIPT, 0, &script,
                     FID_Path|TSTR,    "~templates:defaultvariables",
                     FID_Target|TLONG, glTargetID,
                     TAGEND)) {
                  error = acActivate(script);
                  acFree(script);
               }
               else error = ERR_CreateObject;
            }
            else error = ERR_Init;

            if (error) { acFree(pointer); glSystemPointerID = 0; }

            ReleaseObject(pointer);
         }
         else error = ERR_NewObject;
      }
      else error = ERR_Init;

      if (error) { acFree(target); glTargetID = 0; }

      ReleaseObject(target);
   }
   else error = ERR_NewObject;

   return error;
}
