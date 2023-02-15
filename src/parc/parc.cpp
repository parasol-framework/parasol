/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Parc: This class manages the execution of PARC files.

The Parc class is used to execute Parasol Archives (`.parc` files) in the current process space.  In doing so, many
system privileges for the active process may be lost in favour of running the Parc file in a restricted sandbox.  For
this reason, it is recommended that Parc files are launched using either the `fluid` or `parasol` executables rather
than using the Parc class directly.  After executing a Parc file via the #Activate() action, it is strongly
recommended that the process is terminated because the loss of system privileges is irreversible.
-END-

*****************************************************************************/

#define PRV_PARC
#include <parasol/main.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/parc.h>

MODULE_COREBASE;
static OBJECTPTR clParc = NULL;

class extParc : public objParc {
   public:
   objCompression *Archive;
   objXML    *Info;           // The parc.xml file.
   OBJECTPTR Script;
   STRING    Args;            // The arguments to pass to the program
   STRING    Path;
   STRING    Allow;
   LONG      ProcessID;
   OBJECTID  WindowID;
};

static ERROR GET_Args(extParc *, CSTRING *);

static ERROR add_parc_class(void);

//********************************************************************************************************************

ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   return add_parc_class();
}

ERROR CMDExpunge(void)
{
   if (clParc) { acFree(clParc); clParc = 0; }
   return ERR_Okay;
}

/*********************************************************************************************************************

-ACTION-
Activate: Launches the program defined in the PARC info file.

Activating a PARC object will launch the default script or executable referenced in the `/info/run` tag of the
archive's `parc.xml` file.

A potential side-effect of calling this action is the permanent loss of system privileges.  This is due to the
sand-boxing of the application and protecting the host system.
-END-

*****************************************************************************/

static ERROR PARC_Activate(extParc *Self, APTR Void)
{
   parasol::Log log;

   if (Self->Script) { acFree(Self->Script); Self->Script = NULL; }

   if (!Self->Info) return log.warning(ERR_NotInitialised);

   log.branch("Launching PARC file.");

   char path[256] = "parc:";
   if (!acGetVar(Self->Info, "content:/info/run", path + 5, sizeof(path)-5)) {
      // Create a "parc:" volume that refers to the "parc" archive created during initialisation.  All file system
      // queries must be routed through parc: by default.  Accessing files outside of that volume must fail unless the
      // user has given permission for the program to do so.

      if (!SetVolume(AST_NAME, "parc", AST_PATH, "archive:parc/", AST_FLAGS, VOLUME_REPLACE|VOLUME_HIDDEN, TAGEND)) {
         CLASSID class_id, subclass_id;
         ERROR error;
         if (!(error = IdentifyFile(path, "Open", IDF_IGNORE_HOST, &class_id, &subclass_id, NULL))) {
            // Run the default script as specified in "parc.xml".

            if (class_id IS ID_SCRIPT) {
               if (!NewObject(subclass_id ? subclass_id : class_id, NF::INTEGRAL, &Self->Script)) {
                  SetField(Self->Script, FID_Path|TSTR, path);
                  if (!(error = acInit(Self->Script))) {
                     error = acActivate(Self->Script);
                  }
               }
               else error = ERR_NewObject;
            }
            else {
               log.warning("The file '%s' referenced by /info/run is not recognised as a script.", path);
               error = ERR_InvalidObject;
            }
         }

         DeleteVolume("parc");
         return error;
      }
      else return ERR_SetVolume;
   }
   else return ERR_NothingDone;
}

//********************************************************************************************************************

static const char glOutputScript[] = "\n\
   glSelf = obj.find('self')\n\
   local win = gui.window({ insidewidth=400, insideheight=300, quit=0, title=arg('title','Program Output'),'\n\
     icon='programs/shell', flags='!nomargins' })\n\
   local surface = win.new('surface', { x=win.leftMargin, y=win.topMargin, xOffset=win.rightMargin,\n\
      yOffset=win.bottomMargin, colour='230,230,230' })\n\
   surface.acShow()\n\
   local vsb = surface.new('scrollbar', { direction='vertical' })\n\
   local text = surface.new('text', { face='little', colour='0,0,0', vscroll=vsb, x=1, y=1, xoffset=20, yoffset=1 })\n\
   win:show()\n\
   glSelf._output = text.id\n\
   glSelf._window = win.id";

static ERROR PARC_DataFeed(extParc *Self, struct acDataFeed *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (Args->DataType IS DATA_TEXT) {
      if (!Args->Size) return ERR_Okay;

      // Create an output window if we don't have one yet

      if (!Self->WindowID) {
         objScript::create script = { fl::Owner(CurrentTaskID()), fl::Statement(glOutputScript) };
         if (script.ok()) {
            acSetVar(*script, "Title", "Program Output");
            script->activate();

            char str[90];
            acGetVar(*script, "window", str, sizeof(str));
            Self->WindowID = StrToInt(str);

            acGetVar(*script, "text", str, sizeof(str));
            Self->OutputID = StrToInt(str);
         }

         if (!Self->WindowID) {
            Self->WindowID = -1;
            return ERR_CreateObject;
         }
      }

      // Send the text through to the text object in the output script

      if (Self->OutputID) ActionMsg(AC_DataFeed, Self->OutputID, Args);
      return ERR_Okay;
   }
   else return ERR_NoSupport;
}

//********************************************************************************************************************

static ERROR PARC_Free(extParc *Self, APTR Void)
{
   if (Self->Script)  { acFree(Self->Script);   Self->Script = NULL; }
   if (Self->Archive) { acFree(Self->Archive);  Self->Archive = NULL; }
   if (Self->Info)    { acFree(Self->Info);     Self->Info = NULL; }
   if (Self->Args)    { FreeResource(Self->Args); Self->Args = NULL; }
   if (Self->Path)    { FreeResource(Self->Path); Self->Path = NULL; }
   return ERR_Okay;
}

/*********************************************************************************************************************

-ACTION-
Init: Initialises the object

Initialising a Parc object will open the file referenced at #Path, which will be verified for its validity
as a PARC file.  If the tests are passed, the parc.xml in the package will be readable from the #Info
field after this action returns.
-END-

*****************************************************************************/

static ERROR PARC_Init(extParc *Self, APTR Void)
{
   parasol::Log log;
   ERROR error;

   if ((Self->Archive = objCompression::create::integral(
         fl::Path(Self->Path),
         fl::ArchiveName("parc"),
         fl::Flags(CMF_READ_ONLY)))) {

      // Read the parc.xml file into the Info field.

      objFile::create info_file = { fl::Flags(FL_NEW|FL_BUFFER|FL_WRITE|FL_READ) };
      if (info_file.ok()) {
         if (!cmpDecompressObject(Self->Archive, "parc.xml", *info_file)) {
            info_file->seekStart(0);

            if (!(Self->Info = objXML::create::integral(fl::Flags(XMF_NEW),
                  fl::Statement(info_file->Buffer)))) {

               // Verify the parc.xml file.
               // TODO

               log.msg("Verifying the parc.xml file.");




               error = ERR_Okay;
            }
            else error = ERR_CreateObject;
         }
         else error = ERR_Decompression;
      }
      else error = ERR_CreateObject;
   }
   else error = ERR_CreateObject; // File is probably not a ZIP compressed source.

   return error;
}

//********************************************************************************************************************

static ERROR PARC_NewObject(extParc *Self, APTR Void)
{
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Allow: Private. Extends the access rights and allowable resource usage of the PARC program.

*****************************************************************************/

static ERROR GET_Allow(extParc *Self, CSTRING *Value)
{
   if (Self->Allow) {
      *Value = Self->Allow;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
}

static ERROR SET_Allow(extParc *Self, CSTRING Value)
{
   if (Self->Allow) { FreeResource(Self->Allow); Self->Allow = NULL; }

   if ((Value) and (*Value)) {
      if (!(Self->Allow = StrClone(Value))) return ERR_Okay;
      else return ERR_AllocMemory;
   }
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Args: Pass parameters to the PARC on execution.

Set the Args field to pass parameter values to the executable PARC program.  Multiple arguments must be separated with
whitespace.  If a parameter value needs to include whitespace, enclose the value in double quotes.

*****************************************************************************/

static ERROR GET_Args(extParc *Self, CSTRING *Value)
{
   if (Self->Args) {
      *Value = Self->Args;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
}

static ERROR SET_Args(extParc *Self, CSTRING Value)
{
   if (Self->Args) { FreeResource(Self->Args); Self->Args = NULL; }

   if ((Value) and (*Value)) {
      LONG i;
      if ((Value[0] IS '1') and (!Value[1])) return ERR_Okay; // Ignore arguments of "1"
      for (i=0; Value[i]; i++);
      if (!AllocMemory(i+1, MEM_STRING|MEM_NO_CLEAR, &Self->Args)) {
         for (i=0; Value[i]; i++) Self->Args[i] = Value[i];
         Self->Args[i] = 0;
      }
      else return ERR_AllocMemory;
   }
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Path: Defines the path to the source PARC file.

*****************************************************************************/

static ERROR GET_Path(extParc *Self, STRING *Value)
{
   if (Self->Path) { *Value = Self->Path; return ERR_Okay; }
   else return ERR_FieldNotSet;
}

static ERROR SET_Path(extParc *Self, CSTRING Value)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }

   if ((Value) and (*Value)) {
      if (!(Self->Path = StrClone(Value))) return ERR_AllocMemory;
   }
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Output: Nominate an object for receiving program output.

Some PARC programs may produce output that can be captured by the caller.  To capture this output within the calling
process, set the Output field to a target object that supports data channels.

If an Output object is not provided, all data from the program will be directed via stdout by default.

*****************************************************************************/

#include "parc_def.c"

static const FieldArray clFields[] = {
   { "Message",  FDF_STRING|FDF_R,     0, NULL, NULL },
   { "Output",   FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   // Virtual fields
   { "Allow",    FDF_STRING|FDF_W,    0, (APTR)GET_Allow, (APTR)SET_Allow },
   { "Args",     FDF_STRING|FDF_RW,   0, (APTR)GET_Args,  (APTR)SET_Args },
   { "Path",     FDF_STRING|FDF_RW,   0, (APTR)GET_Path,  (APTR)SET_Path },
   { "Src",      FDF_SYNONYM|FDF_STRING|FDF_SYNONYM|FDF_RW, 0, (APTR)GET_Path, (APTR)SET_Path },
   END_FIELD
};

//********************************************************************************************************************

static ERROR add_parc_class(void)
{
   clParc = objMetaClass::create::global(
      fl::ClassVersion(VER_PARC),
      fl::Name("Parc"),
      fl::FileExtension("*.parc"),
      fl::FileDescription("Parasol Archive"),
      fl::FileHeader("[0:$504b0304]"),
      fl::Category(CCF_SYSTEM),
      fl::Actions(clParcActions),
      fl::Fields(clFields),
      fl::Size(sizeof(extParc)),
      fl::Path(MOD_PATH));

   return clParc ? ERR_Okay : ERR_AddClass;
}

//********************************************************************************************************************

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, VER_PARC)

