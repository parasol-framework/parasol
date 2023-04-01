/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

This version of the Parasol launcher is intended for use from the command-line only.

*********************************************************************************************************************/

#include <parasol/main.h>
#include <parasol/modules/core.h>
#include <parasol/modules/display.h>
#include <startup.h>
#include <string.h>

#include "common.h"

extern struct CoreBase *CoreBase;

static std::string glProcedure;
static objSurface *glTarget = NULL;
pf::vector<std::string> *glArgs;
static LONG glArgsIndex = 0;
//static STRING glAllow = NULL;
static STRING glTargetFile = NULL;
static OBJECTPTR glTask = NULL;
static objScript *glScript = NULL;
static bool glSandbox = false;
static bool glRelaunched = false;
static bool glTime = false;

static ERROR exec_source(CSTRING, LONG, const std::string);

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

//********************************************************************************************************************

#include "exec.cpp"

//********************************************************************************************************************

static ERROR process_args(void)
{
   pf::Log log("Parasol");

   if ((!glTask->getPtr(FID_Parameters, &glArgs)) and (glArgs)) {
      pf::vector<std::string> &args = *glArgs;
      for (unsigned i=0; i < args.size(); i++) {
         if (!StrMatch(args[i], "--help")) { // Print help for the user
            print(glHelp);
            return ERR_Terminate;
         }
         else if (!StrMatch(args[i], "--verify")) {
            // Special internal function that checks that the installation is valid, returning 1 if checks pass.

            static CSTRING modules[] = { // These modules must be present for an installation to be valid.
               "audio", "display", "fluid", "font", "http", "json", "network", "picture", "svg", "vector", "xml"
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
            glSandbox = true;
         }
         else if (!StrMatch(args[i], "--time")) {
            glTime = true;
         }
         else if (!StrMatch(args[i], "--relaunch")) {
            glRelaunched = true;
         }
         else if (!StrMatch(args[i], "--procedure")) {
            if (i + 1 < args.size()) {
               glProcedure.assign(args[i+1]);
               i++;
            }
         }
         else { // If argument not recognised, assume this arg is the target file.
            if (ResolvePath(args[i].c_str(), RSF_APPROXIMATE, &glTargetFile)) {
               print("Unable to find file '%s'", args[i].c_str());
               return ERR_Terminate;
            }
            glArgsIndex = i + 1;
            break;
         }
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// Main entry point

int main(int argc, CSTRING *argv)
{
   pf::Log log("Parasol");

   if (auto msg = init_parasol(argc, argv)) {
      for (int i=1; i < argc; i++) { // If in --verify mode, return with no error code and print nothing.
         if (!strcmp(argv[i], "--verify")) return 0;
      }
      print(msg);
      return -1;
   }

   glTask = CurrentTask();

   int result = 0;
   if (!process_args()) {
      if (glTargetFile) {
         STRING path;
         if (!glTask->get(FID_Path, &path)) log.msg("Path: %s", path);
         else log.error("No working path.");

         LONG type;
         if ((AnalysePath(glTargetFile, &type) != ERR_Okay) or (type != LOC_FILE)) {
            print("File '%s' does not exist.", glTargetFile);
         }
         else result = exec_source(glTargetFile, glTime, glProcedure);
      }
      else print(glHelp);
   }

   if (glTargetFile) { FreeResource(glTargetFile); glTargetFile = NULL; }
   if (glScript)     { FreeResource(glScript); glScript = NULL; }

   close_parasol();

   return result;
}
