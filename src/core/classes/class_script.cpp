/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Script: The Script class defines a common interface for script execution.

The Script class defines a common interface for the purpose of executing scripts, such as Fluid.  The base class does
not include a default parser or execution process of any kind.

To execute a script file, choose a sub-class that matches the language and create the script object.  Set the #Path
field and then #Activate() the script.  Global input parameters for the script can be defined via the #SetVar()
action.

Note that client scripts may sometimes create objects that are unmanaged by the script object that created them.
Terminating the script will not remove objects that are outside its resource hierarchy.
-END-

*****************************************************************************/

#define PRV_SCRIPT 1
#include "../defs.h"
#include <parasol/main.h>

static ERROR GET_Results(objScript *, STRING **, LONG *);

static ERROR SET_Procedure(objScript *, CSTRING);
static ERROR SET_Results(objScript *, CSTRING *, LONG);
static ERROR SET_String(objScript *, CSTRING);

INLINE CSTRING check_bom(CSTRING Value)
{
   if ((Value[0] IS 0xef) and (Value[1] IS 0xbb) and (Value[2] IS 0xbf)) Value += 3; // UTF-8 BOM
   else if ((Value[0] IS 0xfe) and (Value[1] IS 0xff)) Value += 2; // UTF-16 BOM big endian
   else if ((Value[0] IS 0xff) and (Value[1] IS 0xfe)) Value += 2; // UTF-16 BOM little endian
   return Value;
}

/*****************************************************************************
-ACTION-
Activate: Executes the script.
-END-
*****************************************************************************/

static ERROR SCRIPT_Activate(objScript *Self, APTR Void)
{
   return ERR_NoSupport;
}

/*****************************************************************************
-ACTION-
DataFeed: Script source code can be passed to the object as XML or text via data feeds.
-END-
*****************************************************************************/

static ERROR SCRIPT_DataFeed(objScript *Self, struct acDataFeed *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->DataType IS DATA_XML) {
      Self->set(FID_String, (CSTRING)Args->Buffer);
   }
   else if (Args->DataType IS DATA_TEXT) {
      Self->set(FID_String, (CSTRING)Args->Buffer);
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
DerefProcedure: Dereferences a function.

This method is applicable to scripting languages that manage function references as a keyed resource.  Fluid is
one such language.

Any routine that accepts a script function as a parameter should call DerefProcedure at a later point in order to
ensure that the function reference is released.  Not doing so may leave the reference in memory until the Script that
owns the procedure is terminated.

-INPUT-
ptr(func) Procedure: The function to be dereferenced.

-ERRORS-
Okay:
NullArgs:

*****************************************************************************/

static ERROR SCRIPT_DerefProcedure(objScript *Self, struct scDerefProcedure *Args)
{
   // It is the responsibility of the sub-class to override this method with something appropriate.
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
Callback: An internal method for managing callbacks.

Private

-INPUT-
large ProcedureID: An identifier for the target procedure.
cstruct(*ScriptArg) Args: Optional CSV string containing arguments to pass to the procedure.
int TotalArgs: The total number of arguments in the Args parameter.
&int Error: The error code returned from the script, if any.

-ERRORS-
Okay:
Args:

-END-

*****************************************************************************/

static ERROR SCRIPT_Callback(objScript *Self, struct scCallback *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);
   if ((Args->TotalArgs < 0) or (Args->TotalArgs > 1024)) return log.warning(ERR_Args);

   LARGE save_id = Self->ProcedureID;
   CSTRING save_name = Self->Procedure;
   Self->ProcedureID = Args->ProcedureID;
   Self->Procedure = NULL;

   const ScriptArg *saveargs = Self->ProcArgs;
   Self->ProcArgs  = Args->Args;

   LONG savetotal = Self->TotalArgs;
   Self->TotalArgs = Args->TotalArgs;
   auto saved_error = Self->Error;
   auto saved_error_msg = Self->ErrorString;
   Self->ErrorString = NULL;
   Self->Error = ERR_Okay;

   ERROR error = acActivate(Self);

   Args->Error = Self->Error;
   Self->Error = saved_error;
   Self->ProcedureID = save_id;
   Self->Procedure = save_name;
   Self->ProcArgs  = saveargs;
   Self->TotalArgs = savetotal;
   if (Self->ErrorString) FreeResource(Self->ErrorString);
   Self->ErrorString = saved_error_msg;

   return error;
}

/*****************************************************************************

-METHOD-
Exec: Executes a procedure in the script.

Use the Exec method to execute a named procedure in a script, optionally passing that procedure a series of arguments.
This method has two different interfaces - one for scripting, which takes parameters as a CSV string, and another for
C/C++, which takes parameters in a serialised array.

The behaviour of the execution process matches that of the #Activate() action and will return the same error
codes in the event of failure.  If the procedure returns results, they will be available from the #Results
field after execution.

If parameters will be passed to the procedure in script (e.g. Fluid), they must be specified as a Comma Separated Value
list in the Args string. Exec will interpret all the values as a string type.  Double or single quotes should be used to
encapsulate string values (use two quotes in sequence as a means of an escape character).  Values should instead be
set as named variables in the script object.

If parameters will be passed to the procedure in C/C++ or similar compiled language, they must be specified as an array
of ScriptArg structures.  The following example illustrates such a list:

<pre>
struct ScriptArg args[] = {
   { "Object",       FD_OBJECTID, { .Long = Self->UID } },
   { "Output",       FD_PTR,      { .Address = output } },
   { "OutputLength", FD_LONG,     { .Long = len } }
};
</>

The ScriptArg structure follows this arrangement:

<pre>
struct ScriptArg {
   STRING Name;
   LONG Type;
   union {
      APTR   Address;
      LONG   Long;
      LARGE  Large;
      DOUBLE Double;
   };
};
</>

The Field Descriptor (FD) specified in the Type must be a match to whatever value is defined in the union.  For instance
if the Long field is defined then an FD_LONG Type must be used.  Supplementary field definition information, e.g.
FD_OBJECT, may be used to assist in clarifying the type of the value that is being passed.  Field Descriptors are
documented in detail in the Class Development Guide.

The C/C++ interface for Exec also requires a hidden third argument that is not specified in this documentation.  The
argument, TotalArgs, must reflect the total number of entries in the Args array.

-INPUT-
cstr Procedure: The name of the procedure to execute, or NULL for the default entry point.
cstruct(*ScriptArg) Args: Optional CSV string containing arguments to pass to the procedure (applies to script-based Exec only).
int TotalArgs: Total number of script arguments provided.

-ERRORS-
Okay: The procedure was executed.
NullArgs
Args: The TotalArgs value is invalid.
-END-

*****************************************************************************/

static ERROR SCRIPT_Exec(objScript *Self, struct scExec *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);
   if ((Args->TotalArgs < 0) or (Args->TotalArgs > 32)) return log.warning(ERR_Args);

   LARGE save_id = Self->ProcedureID;
   CSTRING save_name = Self->Procedure;
   Self->ProcedureID = 0;
   Self->Procedure = Args->Procedure;

   const ScriptArg *saveargs = Self->ProcArgs;
   Self->ProcArgs  = Args->Args;

   LONG savetotal = Self->TotalArgs;
   Self->TotalArgs = Args->TotalArgs;

   ERROR error = acActivate(Self);

   Self->ProcedureID = save_id;
   Self->Procedure = save_name;
   Self->ProcArgs  = saveargs;
   Self->TotalArgs = savetotal;

   return error;
}

//****************************************************************************

static ERROR SCRIPT_Free(objScript *Self, APTR Void)
{
   if (Self->Vars)        { FreeResource(Self->Vars);        Self->Vars = NULL; }
   if (Self->CacheFile)   { FreeResource(Self->CacheFile);   Self->CacheFile = NULL; }
   if (Self->Path)        { FreeResource(Self->Path);        Self->Path = NULL; }
   if (Self->String)      { FreeResource(Self->String);      Self->String = NULL; }
   if (Self->WorkingPath) { FreeResource(Self->WorkingPath); Self->WorkingPath = NULL; }
   if (Self->Procedure)   { FreeResource(Self->Procedure);   Self->Procedure = NULL; }
   if (Self->ErrorString) { FreeResource(Self->ErrorString); Self->ErrorString = NULL; }
   if (Self->Results)     { FreeResource(Self->Results);     Self->Results = NULL; }
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
GetProcedureID: Converts a procedure name to an ID.

This method will convert a procedure name to a unique reference that will be recognised by the script as a direct
reference to that procedure.  The ID can be used to create new FUNCTION definitions, for example:

<pre>
FUNCTION callback;
SET_FUNCTION_SCRIPT(callback, script, procedure_id);
</pre>

Resolving a procedure will often result in the Script maintaining an ongoing reference for it.  To discard the
reference, call <method>DerefProcedure</> once access to the procedure is no longer required.  Alternatively,
destroying the script will also dereference all procedures.

-INPUT-
cstr Procedure:   The name of the procedure.
&large ProcedureID: The computed ID will be returned in this parameter.

-ERRORS-
Okay
NullArgs
-END-

*****************************************************************************/

static ERROR SCRIPT_GetProcedureID(objScript *Self, struct scGetProcedureID *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Procedure) or (!Args->Procedure[0])) return log.warning(ERR_NullArgs);
   Args->ProcedureID = StrHash(Args->Procedure, 0);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
GetVar: Script parameters can be retrieved through this action.
-END-
*****************************************************************************/

static ERROR SCRIPT_GetVar(objScript *Self, struct acGetVar *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Buffer) or (!Args->Field)) return ERR_NullArgs;
   if (Args->Size < 2) return log.warning(ERR_Args);

   CSTRING arg = VarGetString(Self->Vars, Args->Field);

   if (arg) {
      StrCopy(arg, Args->Buffer, Args->Size);
      return ERR_Okay;
   }
   else {
      Args->Buffer[0] = 0;
      return ERR_UnsupportedField;
   }
}

//****************************************************************************

static ERROR SCRIPT_Init(objScript *Self, APTR Void)
{
   parasol::Log log;

   if (!Self->TargetID) { // Define the target if it has not been set already
      log.debug("Target not set, defaulting to owner #%d.", Self->ownerID());
      Self->TargetID = Self->ownerID();
   }

   if (Self->SubID) return ERR_Okay; // Break here to let the sub-class continue initialisation

   return ERR_NoSupport;
}

//****************************************************************************

static ERROR SCRIPT_NewObject(objScript *Self, APTR Void)
{
   Self->CurrentLine = -1;

   // Assume that the script is in English

   Self->Language[0] = 'e';
   Self->Language[1] = 'n';
   Self->Language[2] = 'g';
   Self->Language[3] = 0;

   StrCopy("lang", Self->LanguageDir, sizeof(Self->LanguageDir));

   return ERR_Okay;
}

// If reset, the script will be reloaded from the original file location the next time an activation occurs.  All
// arguments are also reset.

static ERROR SCRIPT_Reset(objScript *Self, APTR Void)
{
   if (Self->Vars) { FreeResource(Self->Vars); Self->Vars = NULL; }
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
SetVar: Script parameters can be set through this action.
-END-
*****************************************************************************/

static ERROR SCRIPT_SetVar(objScript *Self, struct acSetVar *Args)
{
   parasol::Log log;

   // It is acceptable to set zero-length string values (this has its uses in some scripts).

   if ((!Args) or (!Args->Field) or (!Args->Value)) return ERR_NullArgs;
   if (!Args->Field[0]) return ERR_NullArgs;

   log.trace("%s = %s", Args->Field, Args->Value);

   CSTRING field = Args->Field;
   if (*field IS '%') field++;
   if (*field IS '&') field++;
   if (*field IS '$') field++;

   if (!Self->Vars) {
      if (!(Self->Vars = VarNew(0, 0))) return ERR_AllocMemory;
   }

   return VarSetString(Self->Vars, field, Args->Value);
}
/*****************************************************************************

-FIELD-
CacheFile: Compilable script languages can be compiled to a cache file.

Scripts that support compilation of the source code can be compiled to a target file when the script is initialised.
This file is then used as a cache, so that if the cache file exists on the next initialisation then the cache
file is used instead of the original source code.

If the cache file exists, a determination on whether the source code has been edited is usually made by comparing
date stamps on the original and cache files.

*****************************************************************************/

static ERROR GET_CacheFile(objScript *Self, STRING *Value)
{
   *Value = Self->CacheFile;
   return ERR_Okay;
}

static ERROR SET_CacheFile(objScript *Self, CSTRING Value)
{
   if (Self->CacheFile) { FreeResource(Self->CacheFile); Self->CacheFile = NULL; }
   if (Value) Self->CacheFile = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
CurrentLine: Indicates the current line being executed when in debug mode.

In debug mode, the CurrentLine will indicate the current line of execution (according to the original source code for
the script).

It should be noted that not all script processors will support this feature, in which case the value for this field
will be set to -1.

-FIELD-
Error: If a script fails during execution, an error code may be readable here.

On execution of a script, the Error value is reset to ERR_Okay and will be updated if the script fails.  Be mindful
that if a script is likely to be executed recursively then the first thrown error will have priority and be
propagated through the call stack.

-FIELD-
ErrorString: A human readable error string may be declared here following a script execution failure.

*****************************************************************************/

static ERROR GET_ErrorString(objScript *Self, STRING *Value)
{
   *Value = Self->ErrorString;
   return ERR_Okay;
}

static ERROR SET_ErrorString(objScript *Self, CSTRING Value)
{
   if (Self->ErrorString) { FreeResource(Self->ErrorString); Self->ErrorString = NULL; }
   if (Value) Self->ErrorString = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Flags: Optional flags.
Lookup: SCF

-FIELD-
Language: Indicates the language (locale) that the source script is written in.

The Language value indicates the language in which the source script was written.  The default setting is ENG, the
code for international English.

*****************************************************************************/

static ERROR GET_Language(objScript *Self, STRING *Value)
{
   *Value = Self->Language;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
LineOffset: For debugging purposes, this value is added to any message referencing a line number.

The LineOffset is a value that is added to all line numbers that are referenced in script debugging output.  It is
primarily intended for internal usage only.

-FIELD-
Path: The location of the file that is to be processed as a script.

Script files can be loaded by a script object by setting the Path field to the path of the source file.  The
source must be provided prior to the initialisation process or the script object will fail (as an alternative, the
#Statement field can also be set).

Special parameters can also be passed to the script when setting the location.  The name of an executable procedure
may be passed by following the location with a semicolon, then the name of the procedure to execute.  Arguments
can also be passed to the script by following this with a second semicolon, then a sequence of arguments, each
separated with a comma.  The following string illustrates the format used:

<pre>dir:location;procedure;arg1=val1,arg2,arg3=val2</>

A target for the script may be specified by using the 'target' argument in the parameter list (value must refer to a
valid existing object).
-END-

*****************************************************************************/

static ERROR GET_Path(objScript *Self, STRING *Value)
{
   *Value = Self->Path;
   return ERR_Okay;
}

static ERROR SET_Path(objScript *Self, CSTRING Value)
{
   parasol::Log log;

   if (Self->Path) {
      // If the location has already been set, throw the value to SetVar instead.

      if ((Value) and (*Value)) {
         return acSetVar(Self, "Path", Value);
      }
   }
   else {
      if (Self->Path)        { FreeResource(Self->Path); Self->Path = NULL; }
      if (Self->String)      { FreeResource(Self->String); Self->String = NULL; }
      if (Self->WorkingPath) { FreeResource(Self->WorkingPath); Self->WorkingPath = NULL; }

      LONG i, j, len;
      if ((Value) and (*Value)) {
         if (!StrCompare("STRING:", Value, 7, 0)) {
            return SET_String(Self, Value + 7);
         }

         for (len=0; (Value[len]) and (Value[len] != ';'); len++);

         if (!AllocMemory(len+1, MEM_STRING|MEM_NO_CLEAR, (APTR *)&Self->Path, NULL)) {
            for (i=0; i < len; i++) Self->Path[i] = Value[i];
            Self->Path[i] = 0;

            // If a semi-colon has been used, this indicates that a procedure follows the filename.

            if (Value[i] IS ';') {
               char buffer[800], arg[100], argval[400];

               i++;
               while ((Value[i]) and (Value[i] <= 0x20)) i++;
               for (j=0; (Value[i]) and (Value[i] > 0x20) and (Value[i] != ';'); j++) buffer[j] = Value[i++];
               buffer[j] = 0;
               if (buffer[0]) SET_Procedure(Self, buffer);

               // The presence of an opening bracket precedes a series of arguments

               if (Value[i] IS ';') {
                  i++;

                  while (Value[i]) {
                     while ((Value[i]) and (Value[i] <= 0x20)) i++;
                     while (Value[i] IS ',') {
                        i++;
                        while ((Value[i]) and (Value[i] <= 0x20)) i++;
                     }

                     // Extract arg name

                     for (j=0; (Value[i] != ',') and (Value[i] != '=') and (Value[i] > 0x20); j++) arg[j] = Value[i++];
                     arg[j] = 0;

                     while ((Value[i]) and (Value[i] <= 0x20)) i++;

                     // Extract arg value

                     argval[0] = '1';
                     argval[1] = 0;
                     if (Value[i] IS '=') {
                        i++;
                        while ((Value[i]) and (Value[i] <= 0x20)) i++;
                        if (Value[i] IS '"') {
                           i++;
                           for (j=0; (Value[i]) and (Value[i] != '"'); j++) argval[j] = Value[i++];
                           argval[j] = 0;
                        }
                        else {
                           for (j=0; (Value[i]) and (Value[i] != ','); j++) argval[j] = Value[i++];
                           argval[j] = 0;
                        }
                     }

                     if (!StrMatch("target", arg)) Self->set(FID_Target, argval);
                     else acSetVar(Self, arg, argval);
                  }
               }
            }
         }
         else return ERR_AllocMemory;
      }
   }

   return ERR_Okay;
}

//****************************************************************************
// Internal: Name

static ERROR SET_Name(objScript *Self, CSTRING Name)
{
   if (Name) {
      SetName(Self, Name);
      struct acSetVar args = { .Field = "Name", .Value = Name };
      return SCRIPT_SetVar(Self, &args);
   }
   else return ERR_Okay;
}

/*****************************************************************************

PRIVATE: Owner

This field is implemented locally because the owner is temporarily modified during script activation (the owner is set
to the user's task).  Our implementation returns the true owner during this time, which affects Fluid code that
attempts to reference script.owner.  This does not affect the Core's view of the owner or C calls to GetOwner() because
they read the OwnerID field directly.

NB: It probably makes more sense to use a support routine for NewChild() to divert object resource tracking during
script activation - something to try when we have the time?

*****************************************************************************/

static ERROR GET_Owner(objScript *Self, OBJECTID *Value)
{
   if (Self->ScriptOwnerID) *Value = Self->ScriptOwnerID;
   else *Value = Self->ownerID();
   return ERR_Okay;
}

static ERROR SET_Owner(objScript *Self, OBJECTID Value)
{
   parasol::Log log;

   if (Value) {
      OBJECTPTR newowner;
      if (!AccessObject(Value, 2000, &newowner)) {
         SetOwner(Self, newowner);
         ReleaseObject(newowner);
         return ERR_Okay;
      }
      else return log.warning(ERR_ExclusiveDenied);
   }
   else return log.warning(ERR_Args);
}

/*****************************************************************************

-FIELD-
Procedure: Allows you to specify a procedure to be executed from within a script.

Sometimes scripts are split into several procedures or functions that can be executed independently from the 'main'
area of the script.  If a script that you have loaded contains procedures, you can set the Procedure field to execute a
specific routine whenever the script is activated with the Activate action.

If this field is not set, the first procedure in the script, or the 'main' procedure (as defined by the script type) is
executed by default.

*****************************************************************************/

static ERROR GET_Procedure(objScript *Self, CSTRING *Value)
{
   *Value = Self->Procedure;
   return ERR_Okay;
}

static ERROR SET_Procedure(objScript *Self, CSTRING Value)
{
   if (Self->Procedure) { FreeResource(Self->Procedure); Self->Procedure = NULL; }
   if (Value) Self->Procedure = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Results: Stores multiple string results for languages that support this feature.

If a scripting language supports the return of multiple results, this field may reflect those result values after the
execution of any procedure.

For maximum compatibility in type conversion, the results are stored as an array of strings.

*****************************************************************************/

static ERROR GET_Results(objScript *Self, STRING **Value, LONG *Elements)
{
   if (Self->Results) {
      *Value = Self->Results;
      *Elements = Self->ResultsTotal;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      *Elements = 0;
      return ERR_FieldNotSet;
   }
}

static ERROR SET_Results(objScript *Self, CSTRING *Value, LONG Elements)
{
   parasol::Log log;

   if (Self->Results) { FreeResource(Self->Results); Self->Results = 0; }

   Self->ResultsTotal = 0;

   if (Value) {
      LONG len = 0;
      for (LONG i=0; i < Elements; i++) {
         if (!Value[i]) return log.warning(ERR_InvalidData);
         len += StrLength(Value[i]) + 1;
      }
      Self->ResultsTotal = Elements;

      if (!AllocMemory((sizeof(CSTRING) * (Elements+1)) + len, MEM_STRING|MEM_NO_CLEAR, (APTR *)&Self->Results, NULL)) {
         STRING str = (STRING)(Self->Results + Elements + 1);
         LONG i;
         for (i=0; Value[i]; i++) {
            Self->Results[i] = str;
            str += StrCopy(Value[i], str, COPY_ALL) + 1;
         }
         Self->Results[i] = NULL;
         return ERR_Okay;
      }
      else return ERR_AllocMemory;
   }
   else return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Statement: Scripts can be executed from any string passed into this field.

Scripts may be compiled into a script object by setting the Statement field with a complete script string.  This is
often convenient for embedding a small script into another script file without having to make external file references.
It is also commonly used for executing scripts that have been embedded into program binaries.

*****************************************************************************/

static ERROR GET_String(objScript *Self, CSTRING *Value)
{
   *Value = Self->String;
   return ERR_Okay;
}

static ERROR SET_String(objScript *Self, CSTRING Value)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; } // Path removed when a statement string is being set
   if (Self->String) { FreeResource(Self->String); Self->String = NULL; }

   if (Value) Self->String = StrClone(check_bom(Value));
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Target: Reference to the default container that new script objects will be initialised to.

This field can refer to the target object that new objects at the root of the script will be initialised to.  If this
field is not set, the root-level objects in the script will be initialised to the script's owner.

-FIELD-
TotalArgs: Reflects the total number of arguments used in a script object.

The total number of arguments that have been set in a script object through the unlisted field mechanism are reflected
in the value of this field.  If you have not set any arguments then the field value will be zero.
-END-
*****************************************************************************/

static ERROR GET_TotalArgs(objScript *Self, LONG *Value)
{
   *Value = Self->Vars->Total;
   return ERR_Okay;
}

/*****************************************************************************
PRIVATE: Variables
*****************************************************************************/

static ERROR GET_Variables(objScript *Self, KeyStore **Value)
{
   if (!Self->Vars) {
      parasol::SwitchContext ctx(Self);
      Self->Vars = VarNew(0, 0);
      if (!Self->Vars) return ERR_AllocMemory;
   }

   *Value = Self->Vars;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
WorkingPath: Defines the script's working path (folder).

The working path for a script is defined here.  By default this is defined as the location from which the script was
loaded, without the file name.  If this cannot be determined then the working path for the parent process is used
(this is usually set to the location of the program).

The working path is always fully qualified with a slash or colon at the end of the string.

You can manually change the working path by setting this field with a custom string.
-END-

*****************************************************************************/

static ERROR GET_WorkingPath(objScript *Self, STRING *Value)
{
   parasol::Log log;

   // The working path is determined when the first attempt to read it is made.

   if (!Self->WorkingPath) {
      if (!Self->Path) {
         log.warning("Script has no defined Path.");
         return ERR_MissingPath;
      }

      // Determine if an absolute path has been indicated

      UBYTE path = FALSE;
      if (Self->Path[0] IS '/') path = TRUE;
      else {
        for (LONG j=0; (Self->Path[j]) and (Self->Path[j] != '/') and (Self->Path[j] != '\\'); j++) {
            if (Self->Path[j] IS ':') {
               path = TRUE;
               break;
            }
         }
      }

      LONG k;
      LONG j = 0;
      for (k=0; Self->Path[k]; k++) {
         if ((Self->Path[k] IS ':') or (Self->Path[k] IS '/') or (Self->Path[k] IS '\\')) j = k+1;
      }

      STRING workingpath;
      if (path) { // Extract absolute path
         parasol::SwitchContext ctx(Self);
         char save = Self->Path[j];
         Self->Path[j] = 0;
         Self->WorkingPath = StrClone(Self->Path);
         Self->Path[j] = save;
      }
      else if ((!CurrentTask()->get(FID_Path, &workingpath)) and (workingpath)) {
         char buf[1024];

         // Using ResolvePath() can help to determine relative paths such as "../path/file"

         if (j > 0) {
            char save = Self->Path[j];
            Self->Path[j] = 0;
            StrFormat(buf, sizeof(buf), "%s%s", workingpath, Self->Path);
            Self->Path[j] = save;
         }
         else StrFormat(buf, sizeof(buf), "%s", workingpath);

         parasol::SwitchContext ctx(Self);
         if (ResolvePath(buf, RSF_APPROXIMATE, &Self->WorkingPath) != ERR_Okay) {
            Self->WorkingPath = StrClone(workingpath);
         }
      }
      else log.warning("No working path.");
   }

   *Value = Self->WorkingPath;
   return ERR_Okay;
}

static ERROR SET_WorkingPath(objScript *Self, STRING Value)
{
   if (Self->WorkingPath) { FreeResource(Self->WorkingPath); Self->WorkingPath = NULL; }
   if (Value) Self->WorkingPath = StrClone(Value);
   return ERR_Okay;
}

//****************************************************************************

#include "class_script_def.c"

static const FieldArray clScriptFields[] = {
   { "Target",        FDF_OBJECTID|FDF_RW,  0, NULL, NULL },
   { "Flags",         FDF_LONGFLAGS|FDF_RI, (MAXINT)&clScriptFlags, NULL, NULL },
   { "Error",         FDF_LONG|FDF_R,       0, NULL, NULL },
   { "CurrentLine",   FDF_LONG|FDF_R,       0, NULL, NULL },
   { "LineOffset",    FDF_LONG|FDF_RW,      0, NULL, NULL },
   // Virtual Fields
   { "CacheFile",      FDF_STRING|FDF_RW,              0, (APTR)GET_CacheFile, (APTR)SET_CacheFile },
   { "ErrorString",    FDF_STRING|FDF_RW,              0, (APTR)GET_ErrorString, (APTR)SET_ErrorString },
   { "WorkingPath",    FDF_STRING|FDF_RW,              0, (APTR)GET_WorkingPath, (APTR)SET_WorkingPath },
   { "Language",       FDF_STRING|FDF_R,               0, (APTR)GET_Language, NULL },
   { "Location",       FDF_SYNONYM|FDF_STRING|FDF_RI,  0, (APTR)GET_Path, (APTR)SET_Path },
   { "Procedure",      FDF_STRING|FDF_RW,              0, (APTR)GET_Procedure, (APTR)SET_Procedure },
   { "Name",           FDF_STRING|FDF_SYSTEM|FDF_RW,   0, NULL, (APTR)SET_Name },
   { "Owner",          FDF_OBJECTID|FDF_SYSTEM|FDF_RW, 0, (APTR)GET_Owner, (APTR)SET_Owner },
   { "Path",           FDF_STRING|FDF_RI,              0, (APTR)GET_Path, (APTR)SET_Path },
   { "Results",        FDF_ARRAY|FDF_POINTER|FDF_STRING|FDF_RW, 0, (APTR)GET_Results, (APTR)SET_Results },
   { "Src",            FDF_SYNONYM|FDF_STRING|FDF_RI,  0, (APTR)GET_Path, (APTR)SET_Path },
   { "Statement",      FDF_STRING|FDF_RW,              0, (APTR)GET_String, (APTR)SET_String },
   { "String",         FDF_SYNONYM|FDF_STRING|FDF_RW,  0, (APTR)GET_String, (APTR)SET_String },
   { "TotalArgs",      FDF_LONG|FDF_R,                 0, (APTR)GET_TotalArgs, NULL },
   { "Variables",      FDF_POINTER|FDF_SYSTEM|FDF_R,   0, (APTR)GET_Variables, NULL },
   END_FIELD
};

//****************************************************************************

extern "C" ERROR add_script_class(void)
{
   glScriptClass = extMetaClass::create::global(
      fl::ClassVersion(VER_SCRIPT),
      fl::Name("Script"),
      fl::Category(CCF_DATA),
      fl::Flags(CLF_PRIVATE_ONLY),
      fl::Actions(clScriptActions),
      fl::Methods(clScriptMethods),
      fl::Fields(clScriptFields),
      fl::Size(sizeof(objScript)),
      fl::Path("modules:core"));

   return glScriptClass ? ERR_Okay : ERR_AddClass;
}
