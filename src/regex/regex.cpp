/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-MODULE-
Regex: ..

-END-

*********************************************************************************************************************/

#define PRV_REGEX_MODULE

#include <parasol/main.h>
#include <parasol/modules/regex.h>
#include <parasol/strings.hpp>
#include <sstream>
#include <algorithm>

static ERR MODInit(OBJECTPTR, struct CoreBase *);
static ERR MODExpunge(void);
static ERR MODOpen(OBJECTPTR);

#include "module_def.c"

JUMPTABLE_CORE
static OBJECTPTR glRegexModule = nullptr;
static OBJECTPTR clRegex = 0;


#include "regex.h"

//********************************************************************************************************************

static ERR MODInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;
   return ERR::Okay;
}

static ERR MODOpen(OBJECTPTR Module)
{
   Module->set(FID_FunctionList, glFunctions);
   return ERR::Okay;
}

static ERR MODExpunge(void)
{
   for (auto& [id, handle] : glSoundChannels) {
      // NB: Most Regex objects will be disposed of prior to this module being expunged.
      if (handle) {
         pf::ScopedObjectLock<extRegex> regex(id, 3000);
         if (regex.granted()) regex->closeChannels(handle);
      }
   }
   glSoundChannels.clear();

   free_regex_class();
   free_sound_class();
   return ERR::Okay;
}

//********************************************************************************************************************

static STRUCTS glStructures = {
};

PARASOL_MOD(MODInit, nullptr, MODOpen, MODExpunge, MOD_IDL, &glStructures)
extern "C" struct ModHeader * register_regex_module() { return &ModHeader; }
