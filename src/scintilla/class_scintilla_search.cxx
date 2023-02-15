/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
ScintillaSearch: Provides search functionality for use on Scintilla objects.

-END-

*********************************************************************************************************************/

#define PRV_SCINTILLA

#include <string.h>
#include <string>
#include <vector>
#include <map>

#include "Platform.h"
#include "ILexer.h"
#include "Scintilla.h"
#include "PropSetSimple.h"
#include "SplitVector.h"
#include "Partitioning.h"
#include "RunStyles.h"
#include "ContractionState.h"
#include "CellBuffer.h"
#include "CallTip.h"
#include "KeyMap.h"
#include "Indicator.h"
#include "XPM.h"
#include "LineMarker.h"
#include "Style.h"
#include "ViewStyle.h"
#include "AutoComplete.h"
#include "CharClassify.h"
#include "Decoration.h"
#include "Document.h"
#include "Selection.h"
#include "PositionCache.h"
#include "Editor.h"
#include "ScintillaBase.h"

#include <parasol/main.h>
#include <parasol/modules/display.h>

#include "scintillaparasol.h"
#include <parasol/modules/scintilla.h>

#define SCICALL     ((extScintilla *)(Self->Scintilla))->API->SendScintilla

/*********************************************************************************************************************

-METHOD-
Find: Searches for a specific text string.

Call Find to initiate a string search within the targeted #Scintilla object.  The method will scan for the
first instance of the #Text string sequence and return its position in Pos.  The Flags parameter defines special
options that affect the search process.

To find subsequent string matches, call one of either the #Next() or #Prev() methods.

-INPUT-
&int Pos: The position to start searching from.  Set to -1 to start from the cursor position.  This parameter is updated with the byte position of the discovered string sequence.
int(STF) Flags: Optional flags.

-RESULT-
Okay: A successful search was made.
NullArgs:
Search: The string sequence was not found.
-END-

*********************************************************************************************************************/

static ERROR SEARCH_Find(objScintillaSearch *Self, struct ssFind *Args)
{
   parasol::Log log;
   LONG start, end, flags, pos, startLine, endLine, i, targstart, targend;

   if (!Self->Text) return log.warning(ERR_FieldNotSet);

   log.msg("Text: '%.10s'... From: %d, Flags: $%.8x", Self->Text, Args->Pos, Self->Flags);

   flags = ((Args->Flags & STF_CASE) ? SCFIND_MATCHCASE : 0) |
           ((Args->Flags & STF_EXPRESSION) ? SCFIND_REGEXP : 0);

   SCICALL(SCI_SETSEARCHFLAGS, flags);

   if (Self->Flags & STF_SCAN_SELECTION) {
      Self->Start = SCICALL(SCI_GETSELECTIONSTART);
      Self->End   = SCICALL(SCI_GETSELECTIONEND);
      if (Self->Flags & STF_BACKWARDS) {
         start = Self->End;
         end = Self->Start;
      }
      else {
         start = Self->Start;
         end = Self->End;
      }
   }
   else {
      if (Args->Pos < 0) start = SCICALL(SCI_GETCURRENTPOS);
      else start = Args->Pos;

      if (!(Self->Flags & STF_BACKWARDS)) end = SCICALL(SCI_GETLENGTH);
      else end = 0;

      if (start IS end) {
         if (Self->Flags & STF_WRAP) start = 0;
         else return ERR_Search;
      }
   }

   SCICALL(SCI_SETTARGETSTART, start);
   SCICALL(SCI_SETTARGETEND, end);
   pos = SCICALL(SCI_SEARCHINTARGET, StrLength(Self->Text), (char *)Self->Text);

   // If not found and wraparound is wanted, try again

   if ((pos IS -1) and (Self->Flags & STF_WRAP) and (!(Self->Flags & STF_SCAN_SELECTION))) {
      if (!(Self->Flags & STF_BACKWARDS)) {
         start = 0;
         end = SCICALL(SCI_GETLENGTH);
      }
      else {
         start = SCICALL(SCI_GETLENGTH);
         end = 0;
      }

      SCICALL(SCI_SETTARGETSTART, start);
      SCICALL(SCI_SETTARGETEND, end);
      pos = SCICALL(SCI_SEARCHINTARGET, StrLength((STRING)Self->Text), (char *)Self->Text);
   }

   if (pos IS -1) return ERR_Search;

   targstart = SCICALL(SCI_GETTARGETSTART);
   targend   = SCICALL(SCI_GETTARGETEND);
   startLine = SCICALL(SCI_LINEFROMPOSITION, targstart);
   endLine   = SCICALL(SCI_LINEFROMPOSITION, targend);

   for (i=startLine; i <= endLine; ++i) {
      SCICALL(SCI_ENSUREVISIBLEENFORCEPOLICY,i);
   }

   if (Self->Flags & STF_MOVE_CURSOR) {
      // Move cursor to the discovered text
   }
   else {
      // Set the selection
      if (Self->Flags & STF_BACKWARDS) SCICALL(SCI_SETSEL, targend, targstart);
      else SCICALL(SCI_SETSEL, targstart, targend);
   }

   Args->Pos = pos; // Return the position to the user
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR SEARCH_Free(objScintillaSearch *Self, APTR Void)
{
   if (Self->Text) { FreeResource(Self->Text); Self->Text = NULL; }
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR SEARCH_Init(objScintillaSearch *Self, APTR Void)
{
   parasol::Log log;

   if (!Self->Scintilla) { // Find our parent surface
      OBJECTID owner_id = Self->ownerID();
      while ((owner_id) and (GetClassID(owner_id) != ID_SCINTILLA)) {
         owner_id = GetOwnerID(owner_id);
      }
      if (owner_id) Self->Scintilla = (objScintilla *)GetObjectPtr(owner_id);
      else return log.warning(ERR_UnsupportedOwner);
   }

   if ((!Self->Text) or (!Self->Scintilla)) return log.warning(ERR_FieldNotSet);

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
Next: Continues a text search.

Use Next to continue a search after calling the #Find() method.  If a string sequence matching that of #Text is
discovered, its byte position will be returned in the Pos parameter.  If a new match is not discovered then ERR_Search
is returned to indicate an end to the search.

-INPUT-
&int Pos: The byte-position of the discovered string sequence is returned here.

-RESULT-
Okay: A successful search was made.
NullArgs
Search: The string could not be found.

-END-

*********************************************************************************************************************/

static ERROR SEARCH_Next(objScintillaSearch *Self, struct ssNext *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   log.branch("Text: '%.10s', Flags: $%.8x, Section %d to %d", Self->Text, Self->Flags, Self->Start, Self->End);

   LONG flags = ((Self->Flags & STF_CASE) ? SCFIND_MATCHCASE : 0) |
                ((Self->Flags & STF_EXPRESSION) ? SCFIND_REGEXP : 0);

   SCICALL(SCI_SETSEARCHFLAGS, flags);

   LONG start, end, i;
   if (Self->Flags & STF_SCAN_SELECTION) {
      if (Self->Flags & STF_BACKWARDS) {
         start = SCICALL(SCI_GETCURRENTPOS);
         end = Self->Start;
      }
      else {
         start = SCICALL(SCI_GETCURRENTPOS);
         end   = Self->End;
      }
   }
   else {
      start = SCICALL(SCI_GETCURRENTPOS);
      if (!(Self->Flags & STF_BACKWARDS)) end = SCICALL(SCI_GETLENGTH);
      else end = 0;

      if (start IS end) {
         if (Self->Flags & STF_WRAP) start = 0;
         else return ERR_Search;
      }
   }

   log.trace("Search from %d to %d", start, end);

   SCICALL(SCI_SETTARGETSTART, start);
   SCICALL(SCI_SETTARGETEND, end);
   LONG pos = SCICALL(SCI_SEARCHINTARGET, StrLength(Self->Text), (char *)Self->Text);

   // If not found and wraparound is wanted, try again

   if ((pos IS -1) and (Self->Flags & STF_WRAP)) {
      log.trace("Wrap-around");
      if (Self->Flags & STF_SCAN_SELECTION) {
         start = Self->Start;
         end   = Self->End;
      }
      else {
         start = 0;
         end = SCICALL(SCI_GETLENGTH);
      }

      if (Self->Flags & STF_BACKWARDS) {
         auto tmp = start;
         start = end;
         end   = tmp;
      }

      SCICALL(SCI_SETTARGETSTART, start);
      SCICALL(SCI_SETTARGETEND, end);
      pos = SCICALL(SCI_SEARCHINTARGET, StrLength((STRING)Self->Text), (char *)Self->Text);
   }

   if (pos IS -1) return ERR_Search;

   LONG targstart = SCICALL(SCI_GETTARGETSTART);
   LONG targend   = SCICALL(SCI_GETTARGETEND);
   LONG startLine = SCICALL(SCI_LINEFROMPOSITION,targstart);
   LONG endLine   = SCICALL(SCI_LINEFROMPOSITION,targend);

   for (i=startLine; i <= endLine; ++i) {
      SCICALL(SCI_ENSUREVISIBLEENFORCEPOLICY,i);
   }

   if (Self->Flags & STF_MOVE_CURSOR) { // Move cursor to the discovered text
   }
   else { // Set the selection
      if (Self->Flags & STF_BACKWARDS) SCICALL(SCI_SETSEL, targend, targstart);
      else SCICALL(SCI_SETSEL, targstart, targend);
   }

   Args->Pos = pos;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
Prev: Continues a text search in reverse.

The Prev method operates under the same circumstances as #Next(), except that the search will be in reverse.  Please
refer to #Next() for further information.

-INPUT-
&int Pos: The byte-position of the discovered string is returned here.

-RESULT-
Okay
NullArgs
Search: The string could not be found.
-END-

*********************************************************************************************************************/

static ERROR SEARCH_Prev(objScintillaSearch *Self, struct ssPrev *Args)
{
   if (!Args) return ERR_NullArgs;

   // Temporarily set the STF_BACKWARDS flag

   LONG flags = Self->Flags;
   if (Self->Flags & STF_BACKWARDS) Self->Flags &= ~STF_BACKWARDS;
   else Self->Flags |= STF_BACKWARDS;

   SEARCH_Next(Self, (struct ssNext *)Args);

   Self->Flags = flags; // Restore the original flags
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Flags: Optional flags.

Optional flags that affect the search process are specified here.

-FIELD-
Scintilla: Targets a Scintilla object for searching.

A Scintilla object must be targeted in this field in order to perform the search process.

-FIELD-
Text: The string sequence to search for.

This field defines the string sequence that will be searched for when calling either #Find(), #Next() or #Prev().
-END-

*********************************************************************************************************************/

static ERROR SET_Text(objScintillaSearch *Self, CSTRING Value)
{
   if (Self->Text) { FreeResource(Self->Text); Self->Text = NULL; }
   if (Value) {
      if (!(Self->Text = StrClone(Value))) return ERR_AllocMemory;
   }
   return ERR_Okay;
}

//********************************************************************************************************************

static const ActionArray clActions[] = {
   { AC_Free, (APTR)SEARCH_Free },
   { AC_Init, (APTR)SEARCH_Init },
   { 0, NULL }
};

//********************************************************************************************************************

static const FunctionField argsNext[] = { { "Pos", FD_LONG|FD_RESULT }, { NULL, 0 } };
static const FunctionField argsPrev[] = { { "Pos", FD_LONG|FD_RESULT }, { NULL, 0 } };
static const FunctionField argsFind[] = { { "Pos", FD_LONG|FD_RESULT }, { NULL, 0 } };

static const MethodArray clMethods[] = {
   { MT_SsNext, (APTR)SEARCH_Next, "Next", argsNext, sizeof(struct ssNext) },
   { MT_SsPrev, (APTR)SEARCH_Prev, "Prev", argsPrev, sizeof(struct ssPrev) },
   { MT_SsFind, (APTR)SEARCH_Find, "Find", argsFind, sizeof(struct ssFind) },
   { 0, NULL, NULL, NULL, 0 }
};

//********************************************************************************************************************

static const FieldDef clFlags[] = {
   { "Case",          STF_CASE },
   { "MoveCursor",    STF_MOVE_CURSOR },
   { "ScanSelection", STF_SCAN_SELECTION },
   { "Backwards",     STF_BACKWARDS },
   { "Expression",    STF_EXPRESSION },
   { "Wrap",          STF_WRAP },
   { NULL, 0 }
};

static const FieldArray clFields[] = {
   { "Scintilla", FDF_OBJECT|FDF_RI, ID_SCINTILLA, NULL, NULL },
   { "Text",      FDF_STRING|FDF_RW, 0, NULL, (APTR)SET_Text },
   { "Flags",     FDF_LONGFLAGS|FDF_RW, (MAXINT)&clFlags, NULL, NULL },
   END_FIELD
};

//********************************************************************************************************************

OBJECTPTR clScintillaSearch = NULL;

ERROR init_search(void)
{
   clScintillaSearch = objMetaClass::create::global(
      fl::ClassVersion(1.0),
      fl::Name("ScintillaSearch"),
      fl::Category(CCF_TOOL),
      fl::Actions(clActions),
      fl::Methods(clMethods),
      fl::Fields(clFields),
      fl::Size(sizeof(objScintillaSearch)),
      fl::Path("modules:scintilla"));

   return clScintillaSearch ? ERR_Okay : ERR_AddClass;
}
