/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
MetaClass: The MetaClass is used to manage all classes supported by the system core.

The MetaClass is at the root of the Core's object oriented design and is responsible for managing the construction of
new classes.  All classes that are created within the system at run-time are represented by a MetaClass object.  Each
MetaClass object can be inspected to discover detailed information about the class that has been declared.  Most
of the interesting structural data can be gleaned from the #Fields array.

A number of functions are available in the Core for the purpose of class management.  The Core maintains
its own list of MetaClass objects, which you can search by calling the ~FindClass() function.  The
~CheckAction() function provides a way of checking if a particular pre-defined action is supported
by a class.

Classes are almost always encapsulated by shared modules, although it is possible to create private classes inside
executable programs.  For information on the creation of classes, refer to the Class Development Guide for a
complete run-down on class development.

-END-

*****************************************************************************/

#include "../defs.h"

static ERROR OBJECT_GetClass(OBJECTPTR, struct rkMetaClass **);
static ERROR OBJECT_GetClassID(OBJECTPTR, CLASSID *);
static ERROR OBJECT_GetName(OBJECTPTR, STRING *);
static ERROR OBJECT_GetOwner(OBJECTPTR, OBJECTID *);
static ERROR OBJECT_SetOwner(OBJECTPTR, OBJECTID);
static ERROR OBJECT_SetName(OBJECTPTR, CSTRING);
static ERROR OBJECT_GetTask(OBJECTPTR, OBJECTID *);

static ERROR field_setup(struct rkMetaClass *);

static void copy_field(struct rkMetaClass *, const struct FieldArray *, struct Field *, LONG *);
static void register_fields(struct rkMetaClass *);
static struct Field * lookup_id_byclass(struct rkMetaClass *, ULONG, struct rkMetaClass **);

//****************************************************************************
// The MetaClass is the focal point of the OO design model.  Because classes are treated like objects, they must point
// back to a controlling class definition - this it.  See NewObject() for the management code for this data.

#define TOTAL_METAFIELDS  23
#define TOTAL_METAMETHODS 1

static ERROR GET_ActionTable(struct rkMetaClass *, struct ActionEntry **, LONG *);
static ERROR GET_Fields(struct rkMetaClass *, const struct FieldArray **, LONG *);
static ERROR GET_IDL(struct rkMetaClass *, CSTRING *);
static ERROR GET_Location(struct rkMetaClass *, CSTRING *);
static ERROR GET_Methods(struct rkMetaClass *Self, const struct MethodArray **, LONG *);
static ERROR GET_Module(struct rkMetaClass *, CSTRING *);
static ERROR GET_SubFields(struct rkMetaClass *, const struct FieldArray **, LONG *);
static ERROR GET_TotalMethods(struct rkMetaClass *, LONG *);

static ERROR SET_Actions(struct rkMetaClass *, const struct ActionArray *);
static ERROR SET_Fields(struct rkMetaClass *, const struct FieldArray *, LONG);
static ERROR SET_Methods(struct rkMetaClass *, const struct MethodArray *, LONG);

static ERROR GET_ClassName(struct rkMetaClass *Self, CSTRING *Value)
{
   *Value = Self->ClassName;
   return ERR_Okay;
}

static ERROR SET_ClassName(struct rkMetaClass *Self, CSTRING Value)
{
   Self->ClassName = Value;
   return ERR_Okay;
}

static const struct FieldDef CategoryTable[] = {
   { "Command",  CCF_COMMAND },  { "Drawable",   CCF_DRAWABLE },
   { "Effect",   CCF_EFFECT },   { "Filesystem", CCF_FILESYSTEM },
   { "Graphics", CCF_GRAPHICS }, { "GUI",        CCF_GUI },
   { "IO",       CCF_IO },       { "System",     CCF_SYSTEM },
   { "Tool",     CCF_TOOL },     { "Data",       CCF_DATA },
   { "Audio",    CCF_AUDIO },    { "Misc",       CCF_MISC },
   { "Network",  CCF_NETWORK },  { "Multimedia", CCF_MULTIMEDIA },
   { NULL, 0 }
};

static struct Field glMetaFieldsPreset[TOTAL_METAFIELDS+1] = {
   // If you adjust this table, remember to change TOTAL_METAFIELDS, adjust the index numbers and the byte offsets into the structure.
   { 0, 0, 0,                      writeval_default, "ClassVersion",                           FID_ClassVersion, sizeof(struct Head),                0, FDF_DOUBLE|FDF_RI },
   { (MAXINT)"MethodArray", (APTR)GET_Methods, (APTR)SET_Methods, writeval_default, "Methods", FID_Methods,      sizeof(struct Head)+8,              1, FDF_ARRAY|FD_STRUCT|FDF_RI },
   { (MAXINT)"FieldArray", (APTR)GET_Fields, (APTR)SET_Fields, writeval_default, "Fields",     FID_Fields,       sizeof(struct Head)+8+sizeof(APTR), 2, FDF_ARRAY|FD_STRUCT|FDF_RI },
   { 0, 0, 0,                      writeval_default, "ClassName",       FID_ClassName,       sizeof(struct Head)+8+(sizeof(APTR)*2),  3,  FDF_STRING|FDF_RI },
   { 0, 0, 0,                      writeval_default, "FileExtension",   FID_FileExtension,   sizeof(struct Head)+8+(sizeof(APTR)*3),  4,  FDF_STRING|FDF_RI },
   { 0, 0, 0,                      writeval_default, "FileDescription", FID_FileDescription, sizeof(struct Head)+8+(sizeof(APTR)*4),  5,  FDF_STRING|FDF_RI },
   { 0, 0, 0,                      writeval_default, "FileHeader",      FID_FileHeader,      sizeof(struct Head)+8+(sizeof(APTR)*5),  6,  FDF_STRING|FDF_RI },
   { 0, 0, 0,                      writeval_default, "Path",            FID_Path,            sizeof(struct Head)+8+(sizeof(APTR)*6),  7,  FDF_STRING|FDF_RI },
   { 0, 0, 0,                      writeval_default, "Size",            FID_Size,            sizeof(struct Head)+8+(sizeof(APTR)*7),  8,  FDF_LONG|FDF_RI },
   { 0, 0, 0,                      writeval_default, "Flags",           FID_Flags,           sizeof(struct Head)+12+(sizeof(APTR)*7), 9,  FDF_LONG|FDF_RI },
   { 0, 0, 0,                      writeval_default, "SubClassID",      FID_SubClassID,      sizeof(struct Head)+16+(sizeof(APTR)*7), 10, FDF_LONG|FDF_RI },
   { 0, 0, 0,                      writeval_default, "BaseClassID",     FID_BaseClassID,     sizeof(struct Head)+20+(sizeof(APTR)*7), 11, FDF_LONG|FDF_RI },
   { 0, 0, 0,                      writeval_default, "OpenCount",       FID_OpenCount,       sizeof(struct Head)+24+(sizeof(APTR)*7), 12, FDF_LONG|FDF_R },
   { 0, (APTR)GET_TotalMethods, 0, writeval_default, "TotalMethods",    FID_TotalMethods,    sizeof(struct Head)+28+(sizeof(APTR)*7), 13, FDF_LONG|FDF_R },
   { 0, 0, 0,                      writeval_default, "TotalFields",     FID_TotalFields,     sizeof(struct Head)+32+(sizeof(APTR)*7), 14, FDF_LONG|FDF_R },
   { (MAXINT)&CategoryTable, 0, 0, writeval_default, "Category",        FID_Category,        sizeof(struct Head)+36+(sizeof(APTR)*7), 15, FDF_LONG|FDF_LOOKUP|FDF_RI },
   // Virtual fields
   { 0, 0, (APTR)SET_Actions,      writeval_default, "Actions",         FID_Actions,         sizeof(struct Head), 16, FDF_POINTER|FDF_I },
   { 0, (APTR)GET_ActionTable, 0,  writeval_default, "ActionTable",     FID_ActionTable,     sizeof(struct Head), 17, FDF_ARRAY|FDF_POINTER|FDF_R },
   { 0, (APTR)GET_Location, 0,     writeval_default, "Location",        FID_Location,        sizeof(struct Head), 18, FDF_STRING|FDF_R },
   { 0, (APTR)GET_ClassName, (APTR)SET_ClassName, writeval_default, "Name", FID_Name,        sizeof(struct Head), 19, FDF_STRING|FDF_SYSTEM|FDF_RI },
   { 0, (APTR)GET_Module, 0,       writeval_default, "Module",          FID_Module,          sizeof(struct Head), 20, FDF_STRING|FDF_R },
   { 0, (APTR)GET_IDL, 0,          writeval_default, "IDL",             FID_IDL,             sizeof(struct Head), 21, FDF_STRING|FDF_R },
   { (MAXINT)"FieldArray", (APTR)GET_SubFields, 0, writeval_default, "SubFields", FID_SubFields, sizeof(struct Head), 22, FDF_ARRAY|FD_STRUCT|FDF_SYSTEM|FDF_R },
   { 0, 0, 0, NULL, "", 0, 0, 0,  0 }
};

static const struct FieldArray glMetaFields[] = {
   { "ClassVersion",    FDF_DOUBLE|FDF_RI,          0, NULL, NULL },
   { "Methods",         FDF_ARRAY|FD_STRUCT|FDF_RI, (MAXINT)"MethodArray", GET_Methods, SET_Methods },
   { "Fields",          FDF_ARRAY|FD_STRUCT|FDF_RI, (MAXINT)"FieldArray", GET_Fields, SET_Fields },
   { "ClassName",       FDF_STRING|FDF_RI,          0, NULL, NULL },
   { "FileExtension",   FDF_STRING|FDF_RI,          0, NULL, NULL },
   { "FileDescription", FDF_STRING|FDF_RI,          0, NULL, NULL },
   { "FileHeader",      FDF_STRING|FDF_RI,          0, NULL, NULL },
   { "Path",            FDF_STRING|FDF_RI,          0, NULL, NULL },
   { "Size",            FDF_LONG|FDF_RI,            0, NULL, NULL },
   { "Flags",           FDF_LONG|FDF_RI,            0, NULL, NULL },
   { "SubClassID",      FDF_LONG|FDF_RI,            0, NULL, NULL },
   { "BaseClassID",     FDF_LONG|FDF_RI,            0, NULL, NULL },
   { "OpenCount",       FDF_LONG|FDF_R,             0, NULL, NULL },
   { "TotalMethods",    FDF_LONG|FDF_R,             0, NULL, NULL },
   { "TotalFields",     FDF_LONG|FDF_R,             0, NULL, NULL },
   { "Category",        FDF_LONG|FDF_LOOKUP|FDF_RI, (MAXINT)&CategoryTable, NULL, NULL },
   // Virtual fields
   { "Actions",         FDF_POINTER|FDF_I,            0, NULL, NULL },
   { "ActionTable",     FDF_ARRAY|FDF_POINTER|FDF_R,  0, NULL, NULL },
   { "Location",        FDF_STRING|FDF_R,             0, NULL, NULL },
   { "Name",            FDF_STRING|FDF_SYSTEM|FDF_RI, 0, GET_ClassName, SET_ClassName },
   { "Module",          FDF_STRING|FDF_R,             0, GET_Module, NULL },
   { "IDL",             FDF_STRING|FDF_R,             0, GET_IDL, NULL },
   { "SubFields",       FDF_ARRAY|FD_STRUCT|FDF_SYSTEM|FDF_R, (MAXINT)"FieldArray", GET_SubFields, NULL },
   END_FIELD
};

ERROR CLASS_FindField(struct rkMetaClass *, struct mcFindField *);
ERROR CLASS_Free(struct rkMetaClass *, APTR);
ERROR CLASS_Init(struct rkMetaClass *, APTR);

FDEF argsFindField[] = { { "ID", FD_LONG }, { "Field:Field", FD_RESULT|FD_PTR|FD_STRUCT }, { "Source", FD_RESULT|FD_OBJECTPTR }, { 0, 0 } };

static struct MethodArray glMetaMethods[TOTAL_METAMETHODS+2] = {
   { 0, 0, 0, 0, 0 },
   { -1, (APTR)CLASS_FindField, "FindField", argsFindField, sizeof(struct mcFindField) },
   { 0, 0, 0, 0, 0 }
};

struct Stats glMetaClass_Stats = { .Name = "MetaClass" };

struct rkMetaClass glMetaClass = {
   .Head = {
     .Class         = &glMetaClass,
     .Stats         = &glMetaClass_Stats,
     .ChildPrivate  = NULL,
     .CreatorMeta   = NULL,
     .ClassID       = ID_METACLASS,
     .SubID         = ID_METACLASS,
     .UniqueID      = 123,
     .OwnerID       = 0,
     .Flags         = NF_INITIALISED,
     .MemFlags      = 0,
     .TaskID        = 0,
     .ThreadID      = 0,
     .ThreadMsg     = 0,
     .ThreadPending = 0,
     .Queue         = 0,
     .SleepQueue    = 0,
     .Locked        = 0,
     .ActionDepth   = 0
   },
   .ClassVersion    = 1,
   .Methods         = glMetaMethods,
   .Fields          = glMetaFields,
   .ClassName       = "MetaClass",
   .FileExtension   = 0,
   .FileDescription = 0,
   .FileHeader      = 0,
   .Path            = 0,
   .Size            = sizeof(struct rkMetaClass),
   .Flags           = 0,
   .SubClassID      = ID_METACLASS,
   .BaseClassID     = ID_METACLASS,
   .OpenCount       = 0,
   .TotalMethods    = TOTAL_METAMETHODS,
   .TotalFields     = TOTAL_METAFIELDS,
   .Category        = CCF_SYSTEM,
   .Base            = NULL,
   .prvFields       = glMetaFieldsPreset,
   .SubFields       = NULL,
   .Master          = NULL,
   .Children        = { 0, 0, 0, 0, 0, 0, 0, 0 },
   .Location        = NULL,
   .OriginalFieldTotal = ARRAYSIZE(glMetaFields)-1
};

//****************************************************************************
// Sort class lookup by class ID.

static void sort_class_db(void)
{
   LONG h = 1;
   while (h < glClassDB->Total / 9) h = 3 * h + 1;

   LONG *offsets = CL_OFFSETS(glClassDB);
   for (; h > 0; h /= 3) {
      LONG i;
      for (i=h; i < glClassDB->Total; i++) {
         LONG j;
         LONG temp = offsets[i];
         for (j=i; (j >= h) AND (((struct ClassItem *)((APTR)glClassDB + offsets[j-h]))->ClassID > ((struct ClassItem *)((APTR)glClassDB + temp))->ClassID); j -= h) {
            offsets[j] = offsets[j - h];
         }
         offsets[j] = temp;
      }
   }
}

/*****************************************************************************

-METHOD-
FindField: Search a class definition for a specific field.

This method checks if a class has defined a given field by scanning its blueprint for a matching ID.

If the field is present in an inherited class only, a reference to the inherited class will be returned in the Source
parameter.

-INPUT-
int ID: The field ID to search for.  Field names can be converted to ID's by using the ~StrHash() function.
&struct(*Field) Field: Pointer to the field if discovered, otherwise NULL.
&obj(MetaClass) Source: Pointer to the class that is associated with the field, or NULL if the field was not found.

-RESULT-
Okay
NullArgs
Search

-END-

*****************************************************************************/

ERROR CLASS_FindField(struct rkMetaClass *Class, struct mcFindField *Args)
{
   if (!Args) return ERR_NullArgs;

   struct rkMetaClass *src;
   Args->Field = lookup_id_byclass(Class, Args->ID, &src);
   Args->Source = src;
   if (Args->Field) return ERR_Okay;
   else return ERR_Search;
}

//****************************************************************************

ERROR CLASS_Free(struct rkMetaClass *Class, APTR Void)
{
   VarSet(glClassMap, Class->ClassName, NULL, 0); // Deregister the class.

   if (Class->prvFields) { FreeResource(Class->prvFields); Class->prvFields = NULL; }
   if (Class->Methods) { FreeResource(Class->Methods); Class->Methods = NULL; }
   if (Class->Location) { FreeResource(Class->Location); Class->Location = NULL; }

   return ERR_Okay;
}

//****************************************************************************

ERROR CLASS_Init(struct rkMetaClass *Self, APTR Void)
{
   struct rkMetaClass *base;
   LONG i;

   if (!Self->ClassName) return PostError(ERR_MissingClassName);

   // Base-class: SubClassID == BaseClassID
   // Sub-class:  SubClassID != BaseClassID
   //
   // If neither ID is specified, the hash is derived from the name and then applied to both SubClassID and BaseClassID.

   if ((Self->BaseClassID) AND (!Self->SubClassID)) {
      Self->SubClassID = StrHash(Self->ClassName, FALSE);
      //Self->SubClassID = Self->BaseClassID;
   }
   else if (!Self->BaseClassID) {
      if (!Self->SubClassID) {
         Self->SubClassID = StrHash(Self->ClassName, FALSE);
      }
      Self->BaseClassID = Self->SubClassID;
   }

   if (Self->BaseClassID IS Self->SubClassID) {
      if (!Self->Size) Self->Size = sizeof(struct Head);
      else if (Self->Size < sizeof(struct Head)) { // Object size not specified
         LogErrorMsg("Size of %d is not valid.", Self->Size);
         return ERR_FieldNotSet;
      }
   }

   // If this is a subclass, find the base class.  Note that FindClass() will automatically initialise the base if
   // there is a reference for it, so if it returns NULL then it is obvious that the base class is not installed on the
   // user's system.

   if ((Self->BaseClassID) AND (Self->SubClassID != Self->BaseClassID)) {
      if ((base = (struct rkMetaClass *)FindClass(Self->BaseClassID))) {
         MSG("Using baseclass $%.8x (%s) for %s", Self->BaseClassID, base->ClassName, Self->ClassName);
         if (!Self->FileDescription) Self->FileDescription = base->FileDescription;
         if (!Self->FileExtension)   Self->FileExtension   = base->FileExtension;
         if (!Self->ClassVersion)    Self->ClassVersion    = base->ClassVersion;

         // If over-riding field definitions have been specified by the sub-class, move them to the SubFields pointer.

         // NB: Sub-classes may not enlarge object structures, therefore they inherit directly from the base.

         if (Self->Fields) Self->SubFields = Self->Fields;
         Self->Fields = base->Fields;
         Self->OriginalFieldTotal = base->OriginalFieldTotal;

         Self->Flags |= base->Flags; // Allow flag inheritance, e.g. PROMOTE_CHILDREN

         // Sub-classes can override the base-class size on the basis that they may have a custom-sized private
         // variable area.

         if (!Self->Size) Self->Size = base->Size;
         Self->Base = base;

         // Note: Sub-classes can define their own custom methods independent of the base class, but care must be taken
         // to use a large enough cushion to prevent an overlap of method ID's.

         if ((Self->Methods) AND (base->Methods)) {
            if (Self->TotalMethods < base->TotalMethods) { // Expand the method table to match the base class.
               if (!ReallocMemory(Self->Methods, sizeof(struct MethodArray) * (base->TotalMethods+1), (APTR *)&Self->Methods, NULL)) {
                  Self->TotalMethods = base->TotalMethods;
               }
               else return PostError(ERR_ReallocMemory);
            }

            // Copy over method information from the base-class (the sub-class' function pointers will
            // not be modified).

            for (i=0; i < base->TotalMethods+1; i++) {
               Self->Methods[i].MethodID = base->Methods[i].MethodID;
               Self->Methods[i].Name = base->Methods[i].Name;
               Self->Methods[i].Args = base->Methods[i].Args;
               Self->Methods[i].Size = base->Methods[i].Size;
            }
         }
         else if ((!Self->Methods) AND (base->Methods)) { // Copy methods from the base-class
            if (!AllocMemory(sizeof(struct MethodArray) * (base->TotalMethods + 1), MEM_DATA, (APTR *)&Self->Methods, NULL)) {
               CopyMemory(base->Methods, Self->Methods, sizeof(struct MethodArray) * (base->TotalMethods + 1));
               Self->TotalMethods = base->TotalMethods;
            }
            else return ERR_AllocMemory;
         }
      }
      else {
         LogErrorMsg("A base for class $%.8x is not present!  Install it.", Self->BaseClassID);
         return ERR_Failed;
      }
   }
   else; // Base class

   if (field_setup(Self) != ERR_Okay) return ERR_Failed;

   // Note that classes are keyed by their unique name and not the base-class name.  This reduces the need for
   // iteration over the hash-map.

   VarSet(glClassMap, Self->ClassName, &Self, sizeof(APTR));

   Self->ActionTable[AC_OwnerDestroyed].PerformAction = MGR_OwnerDestroyed;

   // Record the name of the module that owns this class.

   struct ObjectContext *ctx = tlContext;
   while (ctx != &glTopContext) {
      if (ctx->Object->ClassID IS ID_MODULEMASTER) {
         Self->Master = (struct ModuleMaster *)ctx->Object;
         break;
      }
      ctx = ctx->Stack;
   }

   return register_class(Self->ClassName, (Self->BaseClassID IS Self->SubClassID) ? 0 : Self->BaseClassID, Self->Category, Self->Path, Self->FileExtension, Self->FileHeader);
}

/*****************************************************************************

-FIELD-
Actions: Set this field to define the actions supported by the class.

It is common practice when developing classes to support a number of actions that help to flesh-out the class
functionality.  To define the actions that your class will support, you need to create a pre-defined action list in
your code, and set this field with the action specifications before the class is initialised.

An action list is a simple array of action ID's and the routines associated with each ID.  When you set the Actions
field, the list will be processed into a jump table that is used internally.  After this process, your action list
will serve no further purpose.

The following example shows an action list array taken from the @Picture class:

<pre>
struct ActionArray clActions[] = {
   { AC_Free,          PIC_Free },
   { AC_NewObject,     PIC_NewObject },
   { AC_Init,          PIC_Init },
   { AC_Query,         PIC_Query },
   { AC_Read,          PIC_Read },
   { AC_SaveToObject,  PIC_SaveToObject },
   { AC_Seek,          PIC_Seek },
   { AC_Write,         PIC_Write },
   { NULL, NULL }
};
</pre>

The action ID's used in this particular list can be found in the system/actioncodes.h include file, along with many
others. Never define method ID's in an action list - please use the #Methods field to define your methods.

*****************************************************************************/

static ERROR SET_Actions(struct rkMetaClass *Self, const struct ActionArray *Actions)
{
   if (!Actions) return ERR_Failed;

   WORD i;
   for (i=0; Actions[i].ActionCode; i++) {
      WORD code = Actions[i].ActionCode;
      if ((code < AC_END) AND (code > 0) AND (code != AC_OwnerDestroyed)) {
         Self->ActionTable[code].PerformAction = Actions[i].Routine;
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
ActionTable: This field can be read to retrieve a MetaClass object's internal action table.

This field retrieves the internal action table of a class. The action table is arranged into a jump
table of action routines, with each routine pointing directly to the object support functions.  The size of the
jump table is defined by the global constant `AC_END`.  The table is sorted by action ID.

It is possible to check if an action is supported by a class by looking up its index within the ActionTable, for
example `Routine[AC_Read]`.  Calling an action routine directly is an illegal operation unless
A) The call is made from an action support function in a class module and B) Special circumstances allow for such
a call, as documented in the Action Support Guide.

*****************************************************************************/

static ERROR GET_ActionTable(struct rkMetaClass *Self, struct ActionEntry **Value, LONG *Elements)
{
   *Value = Self->ActionTable;
   *Elements = AC_END - 1;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
BaseClassID: Specifies the base class ID of a class object.

Prior to the initialisation of a MetaClass object, this field must be set to the base class ID that the class
will represent.  Class ID's are generated as a hash from the class #Name, so if you do not set this field
then the correct ID will be generated for the class automatically.

-FIELD-
Category: The system category that a class belongs to.
Lookup: CCF

When designing a new class it is recommended that a suitable category is chosen and declared in this field.
It is acceptable for a class to be a member of multiple categories by combining category flags together.

-FIELD-
ClassName: The name of the represented class.

This field specifies the name of the represented class.  Characters should be limited to those in the range of A-Z
and written in camel case, e.g. `KeyValue` and not `Keyvalue` or `KEYVALUE`.  Setting this field value is compulsory
prior to initialisation.

-FIELD-
ClassVersion: The version number of the class.

This field value must reflect the version of the class structure.  Legal version numbers start from
1.0 and ascend.  Revision numbers can be used to indicate bug-fixes or very minor changes.

If declaring a sub-class then this value can be 0, but base classes must set a value here.

-FIELD-
Fields: Points to a field array that describes the class' object structure.

This field points to an array that describes the structural arrangement of the objects that will be generated from the
class.  If creating a base class then it must be provided, while sub-classes will inherit this array from their base.

The Class Development Guide has a section devoted to the configuration of this array. Please read the guide for more
information.

*****************************************************************************/

static ERROR GET_Fields(struct rkMetaClass *Self, const struct FieldArray **Fields, LONG *Elements)
{
   *Fields = Self->Fields;
   *Elements = Self->OriginalFieldTotal;
   return ERR_Okay;
}

static ERROR SET_Fields(struct rkMetaClass *Self, const struct FieldArray *Fields, LONG Elements)
{
   if (!Fields) return ERR_Failed;

   Self->Fields = Fields;
   if (Elements > 0) {
      if (!Fields[Elements-1].Name) Elements--; // Make an adjustment in case the last entry is a null terminator.
      Self->OriginalFieldTotal = Elements;
   }
   else {
      LONG i;
      for (i=0; Fields[i].Name; i++);
      Self->OriginalFieldTotal = i;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
FileDescription: Describes the file type represented by the class.

This field allows you to specify a description of the class' file type, if the class is designed to be file related.
This setting can be important, as it helps to distinguish your class from the other file based classes.  Always
make sure that your file description is short, descriptive and unique.  A file description such as "JPEG" is not
acceptable, but "JPEG Picture" would be appropriate.

-FIELD-
FileExtension: Describes the file extension represented by the class.

This field describes the file extension/s that the class will recognise.  For example: `*.wav|*.wave|*.snd`.

Use of the vertical bar allows more than one file extension to be supported by the class. The file extension that
is preferred for saving data must come first.

-FIELD-
FileHeader: Defines a string expression that will allow relevant file data to be matched to the class.

If a class supports file data, then the FileHeader field is used to identify the types of data that the class supports.
This feature is used by routines that need to analyse files and determine which classes support them.

For example, the JPEG class supports files that start with a 32-bit token to identify them as JPEG.  Declaring a
FileHeader expression to match these tokens will allow the FileView feature to detect JPEG files and display an
appropriate icon for each JPEG entry in the file list.

The expression format is `[Offset:Value]...`

The offset is an integer that identifies the byte position at which the given Value will be present.  The Value is
expressed as a hexadecimal number if it is prefixed with a dollar sign $, otherwise the Value is treated as
case-sensitive text.  Multiple expressions can be specified in sequence if there is more than one comparison to be
made - so `[0:$ff][8:$fe]` would require $ff at offset 0 and $fe at offset 8 in order to generate a match.

In some cases, a series of unrelated token sequences may need to be used to match against files.  This is true for the
JPEG class, which supports three different tokens all at offset 0 for identification.  Each matching sequence must be
separated with an OR symbol | as demonstrated in this example for the JPEG header: `[0:$ffd8ffe0]|[0:$ffd8ffe1]|[0:$ffd8fffe]`.

-FIELD-
Flags: Optional flag settings.

-FIELD-
IDL: Returns a compressed IDL string from the module that manages the class.

If the module that created this MetaClass was compiled with an IDL string, it will be possible to read the IDL from
this field.  This feature is typically used by languages that are capable of resolving definitions at run-time,
e.g. Fluid.

A value of NULL is returned if the module does not provide an IDL string.

*****************************************************************************/

static ERROR GET_IDL(struct rkMetaClass *Self, CSTRING *Value)
{
   if (!(Self->Head.Flags & NF_INITIALISED)) return ERR_NotInitialised;

   if ((Self->Master) AND (Self->Master->Header)) {
      *Value = Self->Master->Header->Definitions;
      return ERR_Okay;
   }
   else { // If no Header defined, the class belongs to the Core.
      *Value = glIDL;
      return ERR_Okay;
   }
}

/*****************************************************************************

-FIELD-
Location: Returns the path from which the class binary is loaded.

The path from which the class binary was loaded is readable from this field.  The path may not necessarily include the
file extension of the source binary.

*****************************************************************************/

static STRING get_class_path(CLASSID ClassID)
{
   struct ClassItem *item;
   if ((item = find_class(ClassID))) {
      if (item->PathOffset) return (STRING)item + item->PathOffset;
   }
   return NULL;
}

static ERROR GET_Location(struct rkMetaClass *Self, CSTRING *Value)
{
   if (Self->Path) {
      *Value = Self->Path;
      return ERR_Okay;
   }

   if (Self->Location) {
      *Value = Self->Location;
      return ERR_Okay;
   }

   if (Self->SubClassID) Self->Location = get_class_path(Self->SubClassID);
   else Self->Location = get_class_path(Self->BaseClassID);

   if ((*Value = Self->Location)) return ERR_Okay;
   else return ERR_Failed;
}

/*****************************************************************************

-FIELD-
Methods: Set this field to define the methods supported by the class.

If a class design will include support for methods, define them by creating a method list and then
setting this field with the specifications before the class object is initialised.  A method list provides
information on each method's ID, name, arguments, and structure size. When you set the Methods field with your
method list, the information will be processed into a jump table that is used for making method calls.  After this
process, your method list will serve no further purpose.

The Class Development Guide has a section devoted to describing how the method list must be set up.  Please read
the guide for more information.

Never use action ID's in a Methods array - please use the #Actions field for this.

*****************************************************************************/

static ERROR GET_Methods(struct rkMetaClass *Self, const struct MethodArray **Methods, LONG *Elements)
{
   *Methods = Self->Methods;
   *Elements = Self->TotalMethods;
   return ERR_Okay;
}

static ERROR SET_Methods(struct rkMetaClass *Self, const struct MethodArray *Methods, LONG Elements)
{
   if (!Methods) return ERR_Failed;

   if (Self->Methods) { FreeResource(Self->Methods); Self->Methods = NULL; }

   // Search for the method with the lowest rating ID number

   LONG i;
   LONG j = 0;
   for (i=0; Methods[i].MethodID; i++) {
      if (Methods[i].MethodID < j) j = Methods[i].MethodID;
   }

   // Generate the method array.  Note that the first entry that we put in the array will
   // be NULL because methods start at -1, not zero.

   if (j < 0) {
      LogMsg("Detected %d methods in class %s.", -j, Self->ClassName ? Self->ClassName : (STRING)"Unnamed");
      j = (-j) + 2;
      if (!AllocMemory(sizeof(struct MethodArray) * j, MEM_DATA, (APTR *)&Self->Methods, NULL)) {
         for (i=0; Methods[i].MethodID; i++) {
            if (Methods[i].MethodID >= 0) {
               LogErrorMsg("Invalid method ID (%d) detected in the method array.", Methods[i].MethodID);
            }
            else {
               Self->Methods[-Methods[i].MethodID].MethodID = Methods[i].MethodID;
               Self->Methods[-Methods[i].MethodID].Routine  = Methods[i].Routine;
               Self->Methods[-Methods[i].MethodID].Size     = Methods[i].Size;
               Self->Methods[-Methods[i].MethodID].Name     = Methods[i].Name;
               Self->Methods[-Methods[i].MethodID].Args     = Methods[i].Args;
            }
         }

         // Store the total number of methods

         Self->TotalMethods = j-1;

         // NOTE: If this is a sub-class, the initialisation process will add the base-class methods to the list.
      }
      else return ERR_AllocMemory;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Module: The name of the module binary that initialised the class.

*****************************************************************************/

static ERROR GET_Module(struct rkMetaClass *Self, CSTRING *Value)
{
   if (!(Self->Head.Flags & NF_INITIALISED)) return ERR_NotInitialised;

   if (Self->Master) {
      *Value = Self->Master->LibraryName;
      return ERR_Okay;
   }
   else {
      *Value = "core";
      return ERR_Okay;
   }
}

/*****************************************************************************

-FIELD-
OpenCount: The total number of active objects that are linked back to the MetaClass.

Reading this field will reveal how many objects are currently using the class.  This figure will fluctuate over
time as new objects are created and old ones are destroyed.  When the OpenCount reaches zero, the system may flush the
@Module that the class is related to, so long as no more programs are using it or any other classes created by
the module.

-FIELD-
Path: The path to the module binary that represents the class.

The Path field must be set on initialisation and refers to the default location of the class' module binary, for
example `modules:surface`. For reasons of platform portability, the file extension must not be specified at the
end of the file name.

-FIELD-
Size: The total size of the object structure represented by the MetaClass.

This field value must indicate the byte size of the objects that will be created from the MetaClass.  For example, the
@Picture class defines this value as `sizeof(objPicture)`.

If the size is not explicitly defined, the initialisation process will determine the structure size by
evaluating the field definitions that have been provided.

-FIELD-
SubClassID: Specifies the sub-class ID of a class object.

The SubClassID field is used to indicate that a class is derived from a base class.  If you are creating a sub-class
then set the SubClassID field to the hash value of the class' name (use the ~StrHash() function).

This field must not be set when creating a base class.

To determine whether or not a class is a sub-class or a base class, compare the BaseClassID and SubClassID fields.  If
they are identical then it is a base class, otherwise it is a sub-class.

*****************************************************************************/

static ERROR GET_SubFields(struct rkMetaClass *Self, const struct FieldArray **Fields, LONG *Elements)
{
   LONG i;
   if (Self->SubFields) {
      for (i=0; Self->SubFields[i].Name; i++);
      *Fields = Self->SubFields;
      *Elements = i;
   }
   else {
      *Fields = NULL;
      *Elements = 0;
   }
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
TotalFields: The total number of fields defined by a class.

-FIELD-
TotalMethods: The total number of methods supported by a class.
-END-

*****************************************************************************/

static ERROR GET_TotalMethods(struct rkMetaClass *Class, LONG *Value)
{
   if (Class->TotalMethods > 0) {
      *Value = Class->TotalMethods - 1; // Minus 1 due to the dummy entry at the start
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return ERR_Okay;
   }
}

//****************************************************************************

static ERROR field_setup(struct rkMetaClass *Class)
{
   LONG i, j;

   if (Class->Base) {
      // This is a sub-class.  Clone the field array from the base class, then check for field over-riders specified in
      // the sub-class field list.  Sub-classes can also define additional fields if the fields are virtual.

      if (CloneMemory(Class->Base->prvFields, MEM_DATA, (APTR *)&Class->prvFields, NULL) != ERR_Okay) return ERR_Memory;
      Class->TotalFields = Class->Base->TotalFields;
      struct Field *fields = Class->prvFields;

      if (Class->SubFields) {
         LONG extended = 0;
         WORD ext[256];

         for (i=0; Class->SubFields[i].Name; i++) {
            ULONG hash = StrHash(Class->SubFields[i].Name, FALSE);
            for (j=0; j < Class->TotalFields; j++) {
               if (fields[j].FieldID IS hash) {
                  //if (Class->SubFields[i].Flags) fields[j].Flags = Class->SubFields[i].Flags; // Do we really want sub-classes to be able to change the type of an existing field?
                  //if (Class->SubFields[i].Arg)   fields[j].Arg   = Class->SubFields[i].Arg;   // Why would a sub-class need to redefine lookup or flag constants?

                  if (Class->SubFields[i].GetField) {
                     fields[j].GetValue = Class->SubFields[i].GetField;
                     fields[j].Flags |= FDF_R;
                  }

                  if (Class->SubFields[i].SetField) {
                     fields[j].SetValue = Class->SubFields[i].SetField;
                     if (fields[j].Flags & (FDF_W|FDF_I));
                     else fields[j].Flags |= FDF_W;
                  }

                  optimise_write_field(fields+j);

                  break;
               }
            }

            // If the field was not found in the base, it must be marked virtual or we cannot accept it.
            if (j >= Class->TotalFields) {
               if (Class->SubFields[i].Flags & FD_VIRTUAL) {
                  if (extended < ARRAYSIZE(ext)) ext[extended++] = i;
               }
               else LogErrorMsg("%s field %s has no match in the base class (change field to virtual).", Class->ClassName, Class->SubFields[i].Name);
            }
         }

         if (extended) {
            if (!ReallocMemory(fields, sizeof(fields[0]) * (Class->TotalFields + extended), (APTR *)&Class->prvFields, NULL)) {
               struct Field *fields = Class->prvFields;
               LONG j = Class->TotalFields;
               LONG offset = 0;
               for (i=0; i < extended; i++) {
                  copy_field(Class, Class->SubFields + ext[i], fields + j, &offset);
                  fields[j].Index = j;
                  j++;
               }
               Class->TotalFields += extended;
            }
         }
      }
   }
   else {
      // Generate hashes and count the total number of fields in the class

      WORD namefield  = 1;
      WORD ownerfield = 1;
      WORD taskfield  = 1;

      struct FieldArray *class_fields;
      if ((class_fields = (struct FieldArray *)Class->Fields)) {
         for (i=0; class_fields[i].Name; i++);
         Class->TotalFields = i;
      }
      else Class->TotalFields = 0;

      // Take a copy of the field array (NB: The array pointed to by the programmer is replaced with our own dynamic
      // array).  We also calculate the field offsets as part of this process.
      //
      // The +3 is for the Class & ClassID fields and an extra NULL entry at the end.

      struct Field *fields;
      if (AllocMemory(sizeof(struct Field) * (Class->TotalFields + namefield + ownerfield + taskfield + 3), 0, (APTR *)&fields, NULL) != ERR_Okay) {
         return ERR_AllocMemory;
      }

      LONG offset = sizeof(struct Head);
      for (i=0; i < Class->TotalFields; i++) {
         copy_field(Class, class_fields+i, fields+i, &offset);
         fields[i].Index = i;

         if (fields[i].FieldID IS FID_Name) namefield = 0;
         else if (fields[i].FieldID IS FID_Owner) ownerfield = 0;
         else if (fields[i].FieldID IS FID_Task) taskfield = 0;
      }

      Class->prvFields = fields;

      // Add mandatory system fields that haven't already been defined.

      if (namefield) {
         fields[Class->TotalFields].Name     = "Name";
         fields[Class->TotalFields].FieldID  = FID_Name;
         fields[Class->TotalFields].Flags    = FDF_STRING|FDF_RW|FDF_SYSTEM;
         fields[Class->TotalFields].Arg      = NULL;
         fields[Class->TotalFields].GetValue = (APTR)&OBJECT_GetName;
         fields[Class->TotalFields].SetValue = (APTR)&OBJECT_SetName;
         fields[Class->TotalFields].WriteValue = &writeval_default;
         Class->TotalFields++;
      }

      if (ownerfield) {
         fields[Class->TotalFields].Name     = "Owner";
         fields[Class->TotalFields].FieldID  = FID_Owner;
         fields[Class->TotalFields].Flags    = FDF_OBJECTID|FDF_RW|FDF_SYSTEM;
         fields[Class->TotalFields].Arg      = NULL;
         fields[Class->TotalFields].GetValue = (APTR)&OBJECT_GetOwner;
         fields[Class->TotalFields].SetValue = (APTR)&OBJECT_SetOwner;
         fields[Class->TotalFields].WriteValue = &writeval_default;
         Class->TotalFields++;
      }

      if (taskfield) {
         fields[Class->TotalFields].Name     = "Task";
         fields[Class->TotalFields].FieldID  = FID_Task;
         fields[Class->TotalFields].Flags    = FDF_OBJECTID|FDF_R|FDF_SYSTEM;
         fields[Class->TotalFields].Arg      = NULL;
         fields[Class->TotalFields].GetValue = (APTR)&OBJECT_GetTask;
         fields[Class->TotalFields].SetValue = NULL;
         fields[Class->TotalFields].WriteValue = &writeval_default;
         Class->TotalFields++;
      }

      // Add the Class field.  This is provided primarily to help scripting languages like Fluid.

      fields[Class->TotalFields].Name      = "Class";
      fields[Class->TotalFields].FieldID   = FID_Class;
      fields[Class->TotalFields].Flags     = FDF_OBJECT|FDF_POINTER|FDF_R|FDF_SYSTEM;
      fields[Class->TotalFields].Arg       = NULL;
      fields[Class->TotalFields].GetValue  = (APTR)&OBJECT_GetClass;
      fields[Class->TotalFields].SetValue  = NULL;
      fields[Class->TotalFields].WriteValue = &writeval_default;
      Class->TotalFields++;

      // Add the ClassID field

      fields[Class->TotalFields].Name      = "ClassID";
      fields[Class->TotalFields].FieldID   = FID_ClassID;
      fields[Class->TotalFields].Flags     = FDF_LONG|FDF_UNSIGNED|FDF_R|FDF_SYSTEM;
      fields[Class->TotalFields].Arg       = NULL;
      fields[Class->TotalFields].GetValue  = (APTR)&OBJECT_GetClassID;
      fields[Class->TotalFields].SetValue  = NULL;
      fields[Class->TotalFields].WriteValue = &writeval_default;
      Class->TotalFields++;
   }

   if (glLogLevel >= 2) register_fields(Class);

   // Check for field name hash collisions and other significant development errors

   struct Field *fields = Class->prvFields;

   if (glLogLevel >= 3) {
      for (i=0; i < Class->TotalFields; i++) {
         if (!(fields[i].Flags & FDF_FIELDTYPES)) {
            LogErrorMsg("Badly defined type in field \"%s\".", fields[i].Name);
         }

         for (j=0; j < Class->TotalFields; j++) {
            if (i IS j) continue;
            if (fields[i].FieldID IS fields[j].FieldID) {
               LogErrorMsg("%s: Hash collision - field '%s' collides with '%s'", Class->ClassName, fields[i].Name, fields[j].Name);
            }
         }
      }
   }

   return sort_class_fields(Class, fields);
}

//****************************************************************************
// Register a hashed field ID and its corresponding name.  Use GET_FIELD_NAME() to retrieve field names from the store.

static void register_fields(struct rkMetaClass *Class)
{
   if (!glFields) {
      glFields = VarNew(0, KSF_THREAD_SAFE|KSF_UNTRACKED);
      if (!glFields) return;
   }

   struct Field *fields = Class->prvFields;
   WORD i;
   if (!VarLock(glFields, 4000)) {
      for (i=0; i < Class->TotalFields; i++) {
         KeySet(glFields, fields[i].FieldID, fields[i].Name, StrLength(fields[i].Name)+1);
      }
      VarUnlock(glFields);
   }
}

//****************************************************************************

static void copy_field(struct rkMetaClass *Class, const struct FieldArray *Source, struct Field *Dest, LONG *Offset)
{
   Dest->Name       = Source->Name;
   Dest->FieldID    = StrHash(Source->Name, FALSE);
   Dest->Flags      = Source->Flags;
   Dest->Arg        = Source->Arg;
   Dest->GetValue   = Source->GetField;
   Dest->SetValue   = Source->SetField;
   Dest->WriteValue = writeval_default;
   Dest->Offset     = Offset[0];

   LONG fieldflags = Dest->Flags;

   if (fieldflags & FD_VIRTUAL); // No offset will be added for virtual fields
   else if (fieldflags & FD_RGB) Offset[0] += sizeof(BYTE) * 4;
   else if (fieldflags & (FD_POINTER|FD_ARRAY)) {
      #ifdef _LP64
         if (Offset[0] & 0x7) {
            Offset[0] = (Offset[0] + 7) & (~0x7);
            if (((fieldflags & FDF_R) AND (!Dest->GetValue)) OR
                ((fieldflags & FDF_W) AND (!Dest->SetValue))) {
               LogErrorMsg("Misaligned 64-bit pointer '%s' in class '%s'.", Dest->Name, Class->ClassName);
            }
         }
         Offset[0] += sizeof(APTR);
      #else
         Offset[0] += sizeof(APTR);
      #endif
   }
   else if (fieldflags & FD_LONG) Offset[0] += sizeof(LONG);
   else if (fieldflags & FD_BYTE) Offset[0] += sizeof(BYTE);
   else if (fieldflags & FD_FUNCTION) Offset[0] += sizeof(FUNCTION);
   else if (fieldflags & (FD_DOUBLE|FD_LARGE)) {
      if (Offset[0] & 0x7) {
         if (((fieldflags & FDF_R) AND (!Dest->GetValue)) OR
             ((fieldflags & FDF_W) AND (!Dest->SetValue))) {
            LogErrorMsg("Misaligned 64-bit field '%s' in class '%s'.", Dest->Name, Class->ClassName);
         }
      }
      Offset[0] += 8;
   }
   else LogErrorMsg("%s field \"%s\"/%d has an invalid flag setting.", Class->ClassName, Dest->Name, Dest->FieldID);

   optimise_write_field(Dest);
}

/*****************************************************************************
** Sort the field table by name.  We use a shell sort, similar to bubble sort but much faster because it can copy
** records over larger distances.
**
** NOTE: This is also used in NewObject() to sort the fields of the glMetaClass.
*/

ERROR sort_class_fields(struct rkMetaClass *Class, struct Field *fields)
{
   struct Field *temp;
   LONG i, j;
   ULONG children[ARRAYSIZE(Class->Children)];

   // Build a list of child objects before we do the sort

   UBYTE childcount = 0;
   if (Class->Flags & CLF_PROMOTE_INTEGRAL) {
      for (i=0; i < Class->TotalFields; i++) {
         if (fields[i].Flags & FD_INTEGRAL) {
            Class->Children[childcount] = i;
            children[childcount++] = fields[i].FieldID;
            if (childcount >= ARRAYSIZE(Class->Children)-1) break;
         }
      }
   }
   Class->Children[childcount] = 0xff;

   {
      struct Field * sort[Class->TotalFields];

      for (i=0; i < Class->TotalFields; i++) sort[i] = fields + i;

      LONG h = 1;
      while (h < Class->TotalFields / 9) h = 3 * h + 1;
      for (; h > 0; h /= 3) {
         for (i=h; i < Class->TotalFields; i++) {
            struct Field *temp = sort[i];
            for (j=i; (j >= h) AND (sort[j - h]->FieldID > temp->FieldID); j -= h) {
               sort[j] = sort[j - h];
            }
            sort[j] = temp;
         }
      }

      // Copy the sorted fields into a new field array.  There's a fast and slow version, chosen according to the
      // amount of stack space that could be taken up by the field buffer.

      LONG size = Class->TotalFields * sizeof(struct Field);
      if (size > 4096) {
         if (!AllocMemory(size, MEM_NO_CLEAR|MEM_UNTRACKED, (APTR *)&temp, NULL)) {
            for (i=0; i < Class->TotalFields; i++) {
               CopyMemory(sort[i], temp+i, sizeof(struct Field));
            }
            CopyMemory(temp, fields, (Class->TotalFields) * sizeof(struct Field));
            FreeResource(temp);
         }
         else return ERR_AllocMemory;
      }
      else {
         struct Field temp[Class->TotalFields];

         for (i=0; i < Class->TotalFields; i++) {
            CopyMemory(sort[i], temp+i, sizeof(struct Field));
         }
         CopyMemory(temp, fields, size);
      }
   }

   // Repair child indexes

   for (i=0; i < childcount; i++) {
      for (j=0; j < Class->TotalFields; j++) {
         if (children[i] IS fields[j].FieldID) {
            Class->Children[i] = j;
            break;
         }
      }
   }

   // Repair field indexes following the sort

   for (i=0; i < Class->TotalFields; i++) {
      fields[i].Index = i;
   }

   return ERR_Okay;
}

//****************************************************************************
// These are pre-defined fields that are applied to each class' object.

static ERROR OBJECT_GetClass(OBJECTPTR Self, struct rkMetaClass **Value)
{
   *Value = Self->Class;
   return ERR_Okay;
}

//****************************************************************************

static ERROR OBJECT_GetClassID(OBJECTPTR Self, CLASSID *Value)
{
   *Value = Self->ClassID;
   return ERR_Okay;
}

//****************************************************************************

static ERROR OBJECT_GetOwner(OBJECTPTR Self, OBJECTID *OwnerID)
{
   *OwnerID = GetOwner(Self);
   return ERR_Okay;
}

static ERROR OBJECT_SetOwner(OBJECTPTR Self, OBJECTID OwnerID)
{
   if (OwnerID) {
      OBJECTPTR newowner;
      if (!AccessObject(OwnerID, 2000, &newowner)) {
         SetOwner(Self, newowner);
         ReleaseObject(newowner);
         return ERR_Okay;
      }
      else return PostError(ERR_ExclusiveDenied);
   }
   else return PostError(ERR_NullArgs);
}

//****************************************************************************

static ERROR OBJECT_GetTask(OBJECTPTR Self, OBJECTID *TaskID)
{
   *TaskID = Self->TaskID;
   return ERR_Okay;
}

//****************************************************************************

static ERROR OBJECT_GetName(OBJECTPTR Self, STRING *Name)
{
   *Name = Self->Stats->Name;
   return ERR_Okay;
}

static ERROR OBJECT_SetName(OBJECTPTR Self, CSTRING Name)
{
   if (!Name) return SetName(Self, "");
   else return SetName(Self, Name);
}

//****************************************************************************
// This function expects you to have a lock on the class semaphore.

ERROR write_class_item(struct ClassItem *item)
{
   static BYTE write_attempted = FALSE;

   if (!fs_initialised) return ERR_Okay;

   FMSG("~write_class_item()","Record Index: %d", glClassDB->Total);

   OBJECTPTR file = NULL;
   if ((!glClassFileID) AND (!write_attempted)) {
      write_attempted = TRUE;
      LONG flags = FL_WRITE;
      if (AnalysePath(glClassBinPath, NULL) != ERR_Okay) flags |= FL_NEW;

      if (!NewLockedObject(ID_FILE, NF_INTEGRAL|NF_UNTRACKED, &file, &glClassFileID, NULL)) {
         SetFields(file,
            FID_Path|TSTR,         glClassBinPath,
            FID_Flags|TLONG,       flags,
            FID_Permissions|TLONG, PERMIT_USER_READ|PERMIT_USER_WRITE|PERMIT_GROUP_READ|PERMIT_GROUP_WRITE|PERMIT_OTHERS_READ,
            TAGEND);
         if (acInit(file) != ERR_Okay) {
            ReleaseObject(file);
            glClassFileID = 0;
            STEP();
            return ERR_File;
         }
      }
      else { STEP(); return ERR_NewObject; }
   }

   if (!file) {
      if (!glClassFileID) { STEP(); return ERR_Failed; }
      if (AccessObject(glClassFileID, 3000, &file)) { STEP(); return ERR_AccessObject; }
   }

   acSeekStart(file, 0); // Write the 32-bit header at the start (the total number of records)
   acWrite(file, &glClassDB->Total, sizeof(glClassDB->Total), NULL);
   acSeekEnd(file, 0); // Write the new item to the end of the file.
   acWrite(file, item, item->Size, NULL);

   ReleaseObject(file);

   STEP();
   return ERR_Okay;
}

//****************************************************************************
// Please note that this function will clear any registered classes, so the native classes are re-registered at the end
// of the routine.

ERROR load_classes(void)
{
   LogF("~load_classes()","");

   if (glClassDB) { ReleaseMemoryID(glSharedControl->ClassesMID); glClassDB = NULL; }
   if (glSharedControl->ClassesMID) { FreeResourceID(glSharedControl->ClassesMID); glSharedControl->ClassesMID = 0; }

   ERROR error;
   if (!(error = AccessSemaphore(glSharedControl->ClassSemaphore, 3000, 0))) {
      objFile *file;
      if (!CreateObject(ID_FILE, 0, (OBJECTPTR *)&file,
            FID_Path|TSTR,   glClassBinPath,
            FID_Flags|TLONG, FL_READ,
            TAGEND)) {

         LONG filesize;
         GetLong((OBJECTPTR)file, FID_Size, &filesize);

         LONG total;
         if (!(error = acRead(file, &total, sizeof(total), NULL))) {
            LogF("load_classes","There are %d class records to process.", total);
            LONG memsize = sizeof(struct ClassHeader) + (sizeof(LONG) * total) + filesize - sizeof(LONG);
            if (!(error = AllocMemory(memsize, MEM_NO_CLEAR|MEM_PUBLIC|MEM_UNTRACKED|MEM_NO_BLOCK, (APTR *)&glClassDB, &glSharedControl->ClassesMID))) {

               // Configure the header

               glClassDB->Total = total;
               glClassDB->Size  = memsize;

               if (!(error = acRead(file, CL_ITEMS(glClassDB), filesize - sizeof(LONG), NULL))) {
                  LogF("load_classes","Loaded %d classes.", glClassDB->Total);

                  // Build the class offset array

                  LONG i;
                  LONG *offsets = CL_OFFSETS(glClassDB);
                  struct ClassItem *item = CL_ITEMS(glClassDB);
                  for (i=0; i < total; i++) {
                     offsets[i] = ((MAXINT)item - (MAXINT)glClassDB);
                     item = ((APTR)item) + item->Size;
                  }

                  // Sort the offsets by class ID

                  sort_class_db();
               }
               else error = LogError(ERH_LoadClasses, ERR_Read);
            }
            else error = LogError(ERH_LoadClasses, ERR_AllocMemory);
         }
         else error = LogError(ERH_LoadClasses, ERR_Read);

         acFree(&file->Head);
      }
      else glScanClasses = TRUE;

      ReleaseSemaphore(glSharedControl->ClassSemaphore, 0);
   }
   else error = LogError(ERH_LoadClasses, ERR_AccessSemaphore);

   if (!error) {
      if ((error = register_class("Task", 0, CCF_SYSTEM, "modules:core", TaskClass->FileExtension, TaskClass->FileHeader)));
      else if ((error = register_class("Thread", 0, CCF_SYSTEM, "modules:core", NULL, NULL)));
      else if ((error = register_class("Time", 0, CCF_SYSTEM, "modules:core", NULL, NULL)));
      else if ((error = register_class("Config", 0, CCF_DATA, "modules:core", ConfigClass->FileExtension, NULL)));
      else if ((error = register_class("Module", 0, CCF_SYSTEM, "modules:core", NULL, NULL)));
      else if ((error = register_class("ModuleMaster", 0, CCF_SYSTEM, "modules:core", NULL, NULL)));
      else if ((error = register_class("File", 0, CCF_SYSTEM, "modules:core", NULL, NULL)));
      else if ((error = register_class("StorageDevice", 0, CCF_SYSTEM, "modules:core", NULL, NULL)));
      #ifdef __ANDROID__
      else if ((error = register_class("FileAssets", ID_FILE, CCF_SYSTEM, "modules:core", NULL, NULL)));
      #endif
      else error = register_class("MetaClass", 0, CCF_SYSTEM, "modules:core", NULL, NULL);
   }

   LogBack();
   return error;
}

//****************************************************************************
// [Refer to register_class() if you want to see how classes are recognised]
//
// If the classes.bin file is missing or incomplete, this code will scan for every module installed in the system and
// initialise it so that all classes can be registered in the class database.

void scan_classes(void)
{
   LogF("~Core","Scanning for available classes.");

   struct DirInfo *dir;
   if (!OpenDir("modules:", RDF_QUALIFY, &dir)) {
      LONG total = 0;
      while (!ScanDir(dir)) {
         struct FileInfo *list = dir->Info;

         if (list->Flags & RDF_FILE) {
            #ifdef __ANDROID__
               if (!StrCompare("libshim.", list->Name, 0, 0)) continue;
               if (!StrCompare("libcore.", list->Name, 0, 0)) continue;
            #else
               if (!StrCompare("core.", list->Name, 0, 0)) continue;
            #endif

            char modules[80] = "modules:";
            LONG i, k;
            for (i=0,k=sizeof("modules:")-1; list->Name[i]; i++) modules[k++] = list->Name[i];
            modules[k] = 0;

            LogF("Core","Loading module for class scan: %s", modules);

            OBJECTPTR module;
            if (!CreateObject(ID_MODULE, 0, &module,
                  FID_Name|TSTR, modules,
                  TAGEND)) {
               acFree(module);
            }
            total++;
         }

         // For every 16 modules loaded, run an expunge.  This keeps memory usage down, and on Android
         // is essential because there is a library limit.

         if ((total & 0x1f) IS 0x10) Expunge(FALSE);
      }
      FreeResource(dir);
   }

   LogF("Core","Class scan complete.");

   LogBack();
}

/*****************************************************************************
** Once a class is registered, there is no means to remove it.  You can however delete the classes.bin file to
** regenerate the database from scratch.
*/

ERROR register_class(CSTRING Name, CLASSID ParentID, LONG Category, CSTRING Path, CSTRING FileMatch, CSTRING FileHeader)
{
   LONG headerlen, i, pathlen, matchlen;

   if (!glSharedControl->ClassSemaphore) {
      FMSG("register_class()","No class semaphore available.");
      return ERR_Okay; // Semaphore doesn't exist in early start-up process.
   }

   if (!Name) return ERR_NullArgs;

   ULONG class_id = StrHash(Name, FALSE);
   if (ParentID IS class_id) ParentID = 0; // Parent ID should only be set if the class is a genuine child of another class

   if ((!glClassDB) AND (glSharedControl->ClassesMID)) {
      if (AccessMemory(glSharedControl->ClassesMID, MEM_READ|MEM_NO_BLOCK, 2000, (APTR *)&glClassDB) != ERR_Okay) {
         return PostError(ERR_AccessMemory);
      }
   }

   struct ClassItem *item;
   if (glClassDB) { // Return if the class is already registered
      if ((item = find_class(class_id))) {
         FMSG("register_class","Class already registered.");
         return ERR_Okay;
      }
   }

   LogF("~6register_class()","Name: %s, Path: %s", Name, Path);

   if (!Path) LogF("@register_class","No path given for class '%s'", Name);

   struct ClassHeader *classes;
   if (!AccessSemaphore(glSharedControl->ClassSemaphore, 3000, 0)) {
      UBYTE modpath[120];

      // Determine the size of the new class item structure and additional strings

      if (FileHeader) {
         for (headerlen=0; FileHeader[headerlen]; headerlen++);
         headerlen++;
      }
      else headerlen = 0;

      if (Path) {
         #ifdef __ANDROID__
            // On Android, all libraries are stored in the libs/ folder with no sub-folder hierarchy.  Because of this,
            // we rewrite the path to fit the Android system.

            for (i=0; Path[i]; i++);
            while ((i > 0) AND (Path[i] != '/') AND (Path[i] != '\\') AND (Path[i] != ':')) i--;
            if (i > 0) i++; // Skip folder separator.

            for (pathlen=0; (Path[i+pathlen]) AND (pathlen < sizeof(modpath)-1); pathlen++) modpath[pathlen] = Path[i+pathlen];
            modpath[pathlen++] = 0;
         #else
            for (pathlen=0; (Path[pathlen]) AND (pathlen < sizeof(modpath)-1); pathlen++) modpath[pathlen] = Path[pathlen];
            modpath[pathlen++] = 0;
         #endif
      }
      else { modpath[0] = 0; pathlen = 0; }

      if (FileMatch) {
         for (matchlen=0; FileMatch[matchlen]; matchlen++);
         matchlen++;
      }
      else matchlen = 0;

      LONG itemsize = sizeof(struct ClassItem) + pathlen + matchlen + headerlen;

      LONG totalsize;
      if (glClassDB) totalsize = glClassDB->Size + itemsize + sizeof(LONG);
      else totalsize = sizeof(struct ClassHeader) + itemsize + sizeof(LONG);

      totalsize = AlignLong(totalsize);

      MEMORYID classes_mid;
      if (AllocMemory(totalsize, MEM_NO_CLEAR|MEM_PUBLIC|MEM_NO_BLOCK|MEM_UNTRACKED, (APTR)&classes, &classes_mid)) {
         ReleaseSemaphore(glSharedControl->ClassSemaphore, 0);
         LogBack();
         return ERR_AllocMemory;
      }

      LONG *offsets = (LONG *)(classes + 1);

      if (glClassDB) {
         classes->Total = glClassDB->Total + 1;
         classes->Size  = totalsize;

         // Copy the offset array
         CopyMemory(CL_OFFSETS(glClassDB), offsets, CL_SIZE_OFFSETS(glClassDB));
         for (i=0; i < glClassDB->Total; i++) offsets[i] += sizeof(LONG); // All offsets increase due to table expansion

         // Copy the items

         CopyMemory(CL_ITEMS(glClassDB), offsets + classes->Total,
            glClassDB->Size - sizeof(struct ClassHeader) - CL_SIZE_OFFSETS(glClassDB));

         // Find an insertion point in the array

         LONG floor = 0;
         LONG ceiling = glClassDB->Total;
         while (floor < ceiling) {
            i = (floor + ceiling)>>1;
            if (((struct ClassItem *)((APTR)classes + offsets[i]))->ClassID < class_id) floor = i + 1;
            else ceiling = i;
         }

         if (glClassDB->Total - i > 0) { // Do the insert
            CopyMemory(offsets+i, offsets+i+1, sizeof(LONG) * (glClassDB->Total - i));
         }

         //i = classes->Total-1;
         offsets[i] = glClassDB->Size + sizeof(LONG);
         item = ((APTR)classes) + offsets[i];
      }
      else {
         classes->Total = 1;
         classes->Size = totalsize;
         item = (struct ClassItem *)(offsets + 1);
         offsets[0] = (LONG)((MAXINT)item - (MAXINT)classes);
      }

      // Configure the item structure

      ClearMemory(item, sizeof(struct ClassItem));

      item->ClassID  = class_id;
      item->ParentID = ParentID;
      item->Category = Category;
      for (i=0; (Name[i]) AND (i < sizeof(item->Name)-1); i++) item->Name[i] = Name[i];
      item->Name[i] = 0;
      item->Size = (sizeof(struct ClassItem) + pathlen + matchlen + headerlen + 3) & (~3);

      if (pathlen) {
         item->PathOffset = sizeof(struct ClassItem);
         CopyMemory(modpath, ((APTR)item) + item->PathOffset, pathlen);
      }
      else item->PathOffset = 0;

      if (matchlen) {
         item->MatchOffset = sizeof(struct ClassItem) + pathlen;
         STRING str = ((APTR)item) + item->MatchOffset;
         for (i=0; FileMatch[i]; i++) str[i] = FileMatch[i];
         str[i] = 0;
      }
      else item->MatchOffset = 0;

      if (headerlen) {
         item->HeaderOffset = sizeof(struct ClassItem) + pathlen + matchlen;
         STRING str = ((APTR)item) + item->HeaderOffset;
         for (i=0; FileHeader[i]; i++) str[i] = FileHeader[i];
         str[i] = 0;
      }
      else item->HeaderOffset = 0;

      // Replace the existing class array with the new one

      if (glClassDB) {
         FreeResourceID(glSharedControl->ClassesMID); // Mark for deletion
         ReleaseMemoryID(glSharedControl->ClassesMID);
      }
      glClassDB = classes;
      glSharedControl->ClassesMID = classes_mid; // Replace with the new memory block

      // Write the item to the class database if we have the permissions to do so.

      write_class_item(item);

      sort_class_db(); // The class lookup table must be sorted at all times.

      ReleaseSemaphore(glSharedControl->ClassSemaphore, 0);
      LogBack();
      return ERR_Okay;
   }
   else {
      LogF("@register_class","Time-out on semaphore %d.", glSharedControl->ClassSemaphore);
      LogBack();
      return ERR_TimeOut;
   }
}

//****************************************************************************
// Search the class database for a specific class ID.

struct ClassItem * find_class(ULONG Hash)
{
   if (glClassDB) {
      LONG *offsets = CL_OFFSETS(glClassDB);

      LONG floor = 0;
      LONG ceiling = glClassDB->Total;
      while (floor < ceiling) {
         LONG i = (floor + ceiling)>>1;
         struct ClassItem *item = (struct ClassItem *)((APTR)glClassDB + offsets[i]);

         if (item->ClassID < Hash) floor = i + 1;
         else if (item->ClassID > Hash) ceiling = i;
         else return item;
      }

      FMSG("5find_class","Failed to find class $%.8x from %d classes.", Hash, glClassDB->Total);
   }
   else FMSG("find_class","No classes registered.");

   return NULL;
}

//****************************************************************************
// Lookup the fields declared by a MetaClass, as opposed to the fields of the MetaClass itself.

static struct Field * lookup_id_byclass(struct rkMetaClass *Class, ULONG FieldID, struct rkMetaClass **Result)
{
   struct Field *field = Class->prvFields;

   LONG floor = 0;
   LONG ceiling = Class->TotalFields;
   while (floor < ceiling) {
      LONG i = (floor + ceiling)>>1;
      if (field[i].FieldID < FieldID) floor = i + 1;
      else if (field[i].FieldID > FieldID) ceiling = i;
      else {
         while ((i > 0) AND (field[i-1].FieldID IS FieldID)) i--;
         *Result = Class;
         return field+i;
      }
   }

   if (Class->Flags & CLF_PROMOTE_INTEGRAL) {
      LONG i;
      for (i=0; Class->Children[i] != 0xff; i++) {
         field = Class->prvFields + Class->Children[i];
         if (field->Arg) {
            struct rkMetaClass *childclass = FindClass(field->Arg);
            if (childclass) {
               *Result = childclass;
               field = lookup_id_byclass(childclass, FieldID, Result);
               if (field) return field;
               *Result = NULL;
            }
         }
      }
   }
   return 0;
}
