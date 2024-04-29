/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

This version of the Parasol launcher is intended for use from the command-line only.

*********************************************************************************************************************/

#include <parasol/main.h>
#include <parasol/modules/core.h>
#include <parasol/modules/display.h>
#include <parasol/startup.h>
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

static ERR exec_source(CSTRING, LONG, const std::string);

static const char glHelp[] = {
"This command-line program can execute Fluid scripts and PARC files developed for the Parasol framework.\n\
\n\
   parasol [args] [script.ext] arg1 arg2=value ...\n\
\n\
The following parameters can be used when executing script files:\n\
\n\
 --procedure [n] The name of a procedure to execute.\n\
 --time          Print the amount of time that it took to execute the program.\n\
\n\
 --log-api       Activates run-time log messages at API level.\n\
 --log-info      Activates run-time log messages at INFO level.\n\
 --log-error     Activates run-time log messages at ERROR level.\n"
};

//********************************************************************************************************************

#include "exec.cpp"

//********************************************************************************************************************

static ERR process_args(void)
{
   pf::Log log("Parasol");

   if ((glTask->getPtr(FID_Parameters, &glArgs) IS ERR::Okay) and (glArgs)) {
      pf::vector<std::string> &args = *glArgs;
      for (unsigned i=0; i < args.size(); i++) {
         if (StrMatch(args[i], "--help") IS ERR::Okay) { // Print help for the user
            printf(glHelp);
            return ERR::Terminate;
         }
         else if (StrMatch(args[i], "--verify") IS ERR::Okay) { // Dummy option for verifying installs
            return ERR::Terminate;
         }
         else if (StrMatch(args[i], "--sandbox") IS ERR::Okay) {
            glSandbox = true;
         }
         else if (StrMatch(args[i], "--time") IS ERR::Okay) {
            glTime = true;
         }
         else if (StrMatch(args[i], "--relaunch") IS ERR::Okay) {
            // Internal argument to detect relaunching at an altered security level
            glRelaunched = true;
         }
         else if (StrMatch(args[i], "--procedure") IS ERR::Okay) {
            if (i + 1 < args.size()) {
               glProcedure.assign(args[i+1]);
               i++;
            }
         }
         else { // If argument not recognised, assume this arg is the target file.
            if (ResolvePath(args[i].c_str(), RSF::APPROXIMATE, &glTargetFile) != ERR::Okay) {
               printf("Unable to find file '%s'\n", args[i].c_str());
               return ERR::Terminate;
            }
            glArgsIndex = i + 1;
            break;
         }
      }
   }

   return ERR::Okay;
}

//********************************************************************************************************************

extern "C" int main(int argc, char **argv)
{
   pf::Log log("Parasol");

   if (auto msg = init_parasol(argc, (CSTRING *)argv)) {
      for (int i=1; i < argc; i++) { // If in --verify mode, return with no error code and print nothing.
         if (!strcmp(argv[i], "--verify")) return 0;
      }
      printf("%s\n", msg);
      return -1;
   }

   glTask = CurrentTask();

   int result = 0;
   if (process_args() IS ERR::Okay) {
      if (glTargetFile) {
         STRING path;
         if (glTask->get(FID_Path, &path) IS ERR::Okay) log.msg("Path: %s", path);
         else log.error("No working path.");

         LOC type;
         if ((AnalysePath(glTargetFile, &type) != ERR::Okay) or (type != LOC::FILE)) {
            printf("File '%s' does not exist.\n", glTargetFile);
         }
         else result = LONG(exec_source(glTargetFile, glTime, glProcedure));
      }
      else printf(glHelp);
   }

   if (glTargetFile) { FreeResource(glTargetFile); glTargetFile = NULL; }
   if (glScript)     { FreeResource(glScript); glScript = NULL; }

   close_parasol();

   return result;
}
