/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
MetaClass: The MetaClass is used to manage all classes supported by the system core.

The MetaClass is at the root of the Core's object oriented design and is responsible for managing the construction of
new classes.  All classes that are created within the system at run-time are represented by a MetaClass object.  Each
MetaClass object can be inspected to discover detailed information about the class that has been declared.  Most
of the interesting structural data can be gleaned from the #Fields array.

A number of functions are available in the Core for the purpose of class management.  The Core maintains its own list
of MetaClass objects, which you can search by calling the ~FindClass() function.  The ~CheckAction() function
provides a way of checking if a particular pre-defined action is supported by a class.

Classes are almost always encapsulated by shared modules, although it is possible to create private classes inside
executable programs.  For information on the creation of classes, refer to the Class Development Guide for a
complete run-down on class development.

-END-

*********************************************************************************************************************/

#include "../defs.h"

static ERROR OBJECT_GetClass(OBJECTPTR, extMetaClass **);
static ERROR OBJECT_GetClassID(OBJECTPTR, CLASSID *);
static ERROR OBJECT_GetName(OBJECTPTR, STRING *);
static ERROR OBJECT_GetOwner(OBJECTPTR, OBJECTID *);
static ERROR OBJECT_SetOwner(OBJECTPTR, OBJECTID);
static ERROR OBJECT_SetName(OBJECTPTR, CSTRING);

static void field_setup(extMetaClass *);
static void sort_class_fields(extMetaClass *, std::vector<Field> &);

static void add_field(extMetaClass *, std::vector<Field> &, const FieldArray &, UWORD &);
static void register_fields(std::vector<Field> &);
static Field * lookup_id_byclass(extMetaClass *, ULONG, extMetaClass **);

//********************************************************************************************************************
// The MetaClass is the focal point of the OO design model.  Because classes are treated like objects, they must point
// back to a controlling class definition - this it.  See NewObject() for the management code for this data.

static ERROR GET_ActionTable(extMetaClass *, ActionEntry **, LONG *);
static ERROR GET_Fields(extMetaClass *, const FieldArray **, LONG *);
static ERROR GET_Location(extMetaClass *, CSTRING *);
static ERROR GET_Methods(extMetaClass *, const MethodEntry **, LONG *);
static ERROR GET_Module(extMetaClass *, CSTRING *);
static ERROR GET_PrivateObjects(extMetaClass *, OBJECTID **, LONG *);
static ERROR GET_RootModule(extMetaClass *, class RootModule **);
static ERROR GET_Dictionary(extMetaClass *, struct Field **, LONG *);
static ERROR GET_SubFields(extMetaClass *, const FieldArray **, LONG *);

static ERROR SET_Actions(extMetaClass *, const ActionArray *);
static ERROR SET_Fields(extMetaClass *, const FieldArray *, LONG);
static ERROR SET_Methods(extMetaClass *, const MethodEntry *, LONG);

static ERROR GET_ClassName(extMetaClass *Self, CSTRING *Value)
{
   *Value = Self->ClassName;
   return ERR_Okay;
}

static ERROR SET_ClassName(extMetaClass *Self, CSTRING Value)
{
   Self->ClassName = Value;
   return ERR_Okay;
}

static const FieldDef CategoryTable[] = {
   { "Command",  CCF::COMMAND },  { "Drawable",   CCF::DRAWABLE },
   { "Effect",   CCF::EFFECT },   { "Filesystem", CCF::FILESYSTEM },
   { "Graphics", CCF::GRAPHICS }, { "GUI",        CCF::GUI },
   { "IO",       CCF::IO },       { "System",     CCF::SYSTEM },
   { "Tool",     CCF::TOOL },     { "Data",       CCF::DATA },
   { "Audio",    CCF::AUDIO },    { "Misc",       CCF::MISC },
   { "Network",  CCF::NETWORK },  { "Multimedia", CCF::MULTIMEDIA },
   { NULL, 0 }
};

static const std::vector<Field> glMetaFieldsPreset = {
   // If you adjust this table, remember to adjust the index numbers and the byte offsets into the structure.
   { 0, NULL, NULL,                writeval_default, "ClassVersion",    FID_ClassVersion, sizeof(BaseClass), 0, FDF_DOUBLE|FDF_RI },
   { (MAXINT)"FieldArray", (ERROR (*)(APTR, APTR))GET_Fields, (APTR)SET_Fields, writeval_default, "Fields", FID_Fields, sizeof(BaseClass)+8, 1, FDF_ARRAY|FD_STRUCT|FDF_RI },
   { (MAXINT)"Field",      (ERROR (*)(APTR, APTR))GET_Dictionary, NULL, writeval_default, "Dictionary", FID_Dictionary, sizeof(BaseClass)+8+(sizeof(APTR)*1), 2, FDF_ARRAY|FD_STRUCT|FDF_R },
   { 0, NULL, NULL,                writeval_default, "ClassName",       FID_ClassName,       sizeof(BaseClass)+8+(sizeof(APTR)*2),  3,  FDF_STRING|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "FileExtension",   FID_FileExtension,   sizeof(BaseClass)+8+(sizeof(APTR)*3),  4,  FDF_STRING|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "FileDescription", FID_FileDescription, sizeof(BaseClass)+8+(sizeof(APTR)*4),  5,  FDF_STRING|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "FileHeader",      FID_FileHeader,      sizeof(BaseClass)+8+(sizeof(APTR)*5),  6,  FDF_STRING|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "Path",            FID_Path,            sizeof(BaseClass)+8+(sizeof(APTR)*6),  7,  FDF_STRING|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "Size",            FID_Size,            sizeof(BaseClass)+8+(sizeof(APTR)*7),  8,  FDF_LONG|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "Flags",           FID_Flags,           sizeof(BaseClass)+12+(sizeof(APTR)*7), 9, FDF_LONG|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "ClassID",         FID_ClassID,         sizeof(BaseClass)+16+(sizeof(APTR)*7), 10, FDF_LONG|FDF_UNSIGNED|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "BaseClassID",     FID_BaseClassID,     sizeof(BaseClass)+20+(sizeof(APTR)*7), 11, FDF_LONG|FDF_UNSIGNED|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "OpenCount",       FID_OpenCount,       sizeof(BaseClass)+24+(sizeof(APTR)*7), 12, FDF_LONG|FDF_R },
   { (MAXINT)&CategoryTable, NULL, NULL, writeval_default, "Category",  FID_Category,        sizeof(BaseClass)+28+(sizeof(APTR)*7), 13, FDF_LONG|FDF_LOOKUP|FDF_RI },
   // Virtual fields
   { (MAXINT)"MethodEntry", (ERROR (*)(APTR, APTR))GET_Methods, (APTR)SET_Methods, writeval_default, "Methods", FID_Methods, sizeof(BaseClass), 14, FDF_ARRAY|FD_STRUCT|FDF_RI },
   { 0, NULL, (APTR)SET_Actions,                    writeval_default,   "Actions",           FID_Actions,         sizeof(BaseClass), 15, FDF_POINTER|FDF_I },
   { 0, (ERROR (*)(APTR, APTR))GET_ActionTable, 0,  writeval_default,   "ActionTable",       FID_ActionTable,     sizeof(BaseClass), 16, FDF_ARRAY|FDF_POINTER|FDF_R },
   { 0, (ERROR (*)(APTR, APTR))GET_Location, 0,     writeval_default,   "Location",          FID_Location,        sizeof(BaseClass), 17, FDF_STRING|FDF_R },
   { 0, (ERROR (*)(APTR, APTR))GET_ClassName, (APTR)SET_ClassName, writeval_default, "Name", FID_Name,            sizeof(BaseClass), 18, FDF_STRING|FDF_SYSTEM|FDF_RI },
   { 0, (ERROR (*)(APTR, APTR))GET_Module, 0,       writeval_default,   "Module",            FID_Module,          sizeof(BaseClass), 19, FDF_STRING|FDF_R },
   { 0, (ERROR (*)(APTR, APTR))GET_PrivateObjects, 0, writeval_default, "PrivateObjects",    FID_PrivateObjects,  sizeof(BaseClass), 20, FDF_ARRAY|FDF_LONG|FDF_ALLOC|FDF_R },
   { (MAXINT)"FieldArray", (ERROR (*)(APTR, APTR))GET_SubFields, 0, writeval_default, "SubFields", FID_SubFields, sizeof(BaseClass), 21, FDF_ARRAY|FD_STRUCT|FDF_SYSTEM|FDF_R },
   { ID_ROOTMODULE, (ERROR (*)(APTR, APTR))GET_RootModule, 0, writeval_default, "RootModule", FID_RootModule,     sizeof(BaseClass), 22, FDF_OBJECT|FDF_R },
   { 0, 0, 0, NULL, "", 0, 0, 0,  0 }
};

static const FieldArray glMetaFields[] = {
   { "ClassVersion",    FDF_DOUBLE|FDF_RI },
   { "Fields",          FDF_ARRAY|FD_STRUCT|FDF_RI, GET_Fields, SET_Fields, "FieldArray" },
   { "Dictionary",      FDF_ARRAY|FD_STRUCT|FDF_R, GET_Dictionary, NULL, "Field" },
   { "ClassName",       FDF_STRING|FDF_RI },
   { "FileExtension",   FDF_STRING|FDF_RI },
   { "FileDescription", FDF_STRING|FDF_RI },
   { "FileHeader",      FDF_STRING|FDF_RI },
   { "Path",            FDF_STRING|FDF_RI },
   { "Size",            FDF_LONG|FDF_RI },
   { "Flags",           FDF_LONG|FDF_RI },
   { "ClassID",         FDF_LONG|FDF_UNSIGNED|FDF_RI },
   { "BaseClassID",     FDF_LONG|FDF_UNSIGNED|FDF_RI },
   { "OpenCount",       FDF_LONG|FDF_R },
   { "Category",        FDF_LONG|FDF_LOOKUP|FDF_RI, NULL, NULL, &CategoryTable },
   // Virtual fields
   { "Methods",         FDF_ARRAY|FD_STRUCT|FDF_RI, GET_Methods, SET_Methods, "MethodEntry" },
   { "Actions",         FDF_POINTER|FDF_I },
   { "ActionTable",     FDF_ARRAY|FDF_POINTER|FDF_R, GET_ActionTable },
   { "Location",        FDF_STRING|FDF_R },
   { "Name",            FDF_STRING|FDF_SYSTEM|FDF_RI, GET_ClassName, SET_ClassName },
   { "Module",          FDF_STRING|FDF_R, GET_Module },
   { "PrivateObjects",  FDF_ARRAY|FDF_LONG|FDF_ALLOC|FDF_R, GET_PrivateObjects },
   { "SubFields",       FDF_ARRAY|FD_STRUCT|FDF_SYSTEM|FDF_R, GET_SubFields, NULL, "FieldArray" },
   { "RootModule",      FDF_OBJECT|FDF_R, GET_RootModule, NULL, ID_ROOTMODULE },
   END_FIELD
};

extern "C" ERROR CLASS_FindField(extMetaClass *, struct mcFindField *);
extern "C" ERROR CLASS_Free(extMetaClass *, APTR);
extern "C" ERROR CLASS_Init(extMetaClass *, APTR);
extern "C" ERROR CLASS_NewObject(extMetaClass *, APTR);

FDEF argsFindField[] = { { "ID", FD_LONG }, { "Field:Field", FD_RESULT|FD_PTR|FD_STRUCT }, { "Source", FD_RESULT|FD_OBJECTPTR }, { 0, 0 } };

extMetaClass glMetaClass;

//********************************************************************************************************************
// Standard signal action, applicable to all classes

static ERROR DEFAULT_Signal(OBJECTPTR Object, APTR Void)
{
   Object->Flags |= NF::SIGNALLED;
   return ERR_Okay;
}

//********************************************************************************************************************

void init_metaclass(void)
{
   glMetaClass.BaseClass::Class   = &glMetaClass;
   glMetaClass.BaseClass::UID     = 123;
   glMetaClass.BaseClass::Flags   = NF::INITIALISED;

   glMetaClass.ClassVersion       = 1;
   glMetaClass.Fields             = glMetaFields;
   glMetaClass.ClassName          = "MetaClass";
   glMetaClass.Size               = sizeof(extMetaClass);
   glMetaClass.ClassID            = ID_METACLASS;
   glMetaClass.BaseClassID        = ID_METACLASS;
   glMetaClass.Category           = CCF::SYSTEM;
   glMetaClass.FieldLookup        = glMetaFieldsPreset;
   glMetaClass.OriginalFieldTotal = ARRAYSIZE(glMetaFields)-1;
   glMetaClass.Integral[0]        = 0xff;

   glMetaClass.Methods.resize(2);
   glMetaClass.Methods[1] = { -1, (APTR)CLASS_FindField, "FindField", argsFindField, sizeof(struct mcFindField) };

   glMetaClass.ActionTable[AC_Free].PerformAction = (ERROR (*)(OBJECTPTR, APTR))CLASS_Free;
   glMetaClass.ActionTable[AC_Init].PerformAction = (ERROR (*)(OBJECTPTR, APTR))CLASS_Init;
   glMetaClass.ActionTable[AC_NewObject].PerformAction = (ERROR (*)(OBJECTPTR, APTR))CLASS_NewObject;
   glMetaClass.ActionTable[AC_Signal].PerformAction = &DEFAULT_Signal;

   sort_class_fields(&glMetaClass, glMetaClass.FieldLookup);

   glMetaClass.BaseCeiling = glMetaClass.FieldLookup.size();

   glClassMap[ID_METACLASS] = &glMetaClass;
}

/*********************************************************************************************************************

-METHOD-
FindField: Search a class definition for a specific field.

This method checks if a class has defined a given field by scanning its blueprint for a matching ID.

If the field is present in an integral class, a reference to that class will be returned in the Source parameter.

-INPUT-
int ID: The field ID to search for.  Field names can be converted to ID's by using the ~StrHash() function.
&struct(*Field) Field: Pointer to the field if discovered, otherwise NULL.
&obj(MetaClass) Source: Pointer to the class that is associated with the field (which can match the caller), or NULL if the field was not found.

-RESULT-
Okay
NullArgs
Search

-END-

*********************************************************************************************************************/

ERROR CLASS_FindField(extMetaClass *Self, struct mcFindField *Args)
{
   if ((!Args) or (!Args->ID)) return ERR_NullArgs;

   extMetaClass *src = NULL;
   Args->Field = lookup_id_byclass(Self, Args->ID, &src);
   Args->Source = src;
   if (Args->Field) return ERR_Okay;
   else return ERR_Search;
}

//********************************************************************************************************************

ERROR CLASS_Free(extMetaClass *Self, APTR Void)
{
   if (Self->ClassID) glClassMap.erase(Self->ClassID);
   if (Self->Location) { FreeResource(Self->Location); Self->Location = NULL; }
   Self->~extMetaClass();
   return ERR_Okay;
}

//********************************************************************************************************************

ERROR CLASS_Init(extMetaClass *Self, APTR Void)
{
   pf::Log log;

   if (!Self->ClassName) return log.warning(ERR_MissingClassName);

   auto class_hash = StrHash(Self->ClassName, false);

   if (Self->BaseClassID) {
      if (class_hash != Self->BaseClassID) Self->ClassID = class_hash;
      else if (!Self->ClassID) Self->ClassID = Self->BaseClassID;
   }
   else if (!Self->BaseClassID) {
      Self->ClassID = Self->BaseClassID = class_hash;
   }

   if (Self->BaseClassID IS Self->ClassID) { // Base class
      if (!Self->Size) Self->Size = sizeof(BaseClass);
      else if (Self->Size < (LONG)sizeof(BaseClass)) { // Object size not specified
         log.warning("Size of %d is not valid.", Self->Size);
         return ERR_FieldNotSet;
      }
   }
   else {
      // If this is a sub-class, find the base class.  Note that FindClass() will automatically initialise the base if
      // there is a reference for it, so if it returns NULL then it is obvious that the base class is not installed on the
      // user's system.

      if (auto base = (extMetaClass *)FindClass(Self->BaseClassID)) {
         log.trace("Using baseclass $%.8x (%s) for %s", Self->BaseClassID, base->ClassName, Self->ClassName);
         if (!Self->FileDescription) Self->FileDescription = base->FileDescription;
         if (!Self->FileExtension)   Self->FileExtension   = base->FileExtension;
         if (!Self->ClassVersion)    Self->ClassVersion    = base->ClassVersion;

         // If over-riding field definitions have been specified by the sub-class, move them to the SubFields pointer.

         if (Self->Fields) Self->SubFields = Self->Fields;
         Self->Fields = base->Fields;
         Self->OriginalFieldTotal = base->OriginalFieldTotal;

         Self->Flags |= base->Flags;

         // In tightly controlled configurations, a sub-class can define a structure that is larger than the base
         // class.  Vector filter effects are one example.

         if (!Self->Size) Self->Size = base->Size;
         Self->Base = base;

         // Note: Sub-classes can override methods and also define custom methods independent of the base class.

         if (Self->Methods.size() < base->Methods.size()) Self->Methods.resize(base->Methods.size());

         // Copy method info from the base-class, but leave the sub-class' function pointers if defined.

         for (unsigned i=0; i < base->Methods.size(); i++) {
            Self->Methods[i].MethodID = base->Methods[i].MethodID;
            Self->Methods[i].Name = base->Methods[i].Name;
            Self->Methods[i].Args = base->Methods[i].Args;
            Self->Methods[i].Size = base->Methods[i].Size;
            if (!Self->Methods[i].Routine) Self->Methods[i].Routine = base->Methods[i].Routine;
         }
      }
      else {
         log.warning("A base for class $%.8x is not present!", Self->BaseClassID);
         return ERR_Failed;
      }
   }

   field_setup(Self);

   glClassMap[Self->ClassID] = Self;

   // Record the name of the module that owns this class.

   auto ctx = tlContext;
   while (ctx != &glTopContext) {
      if (ctx->object()->Class->ClassID IS ID_ROOTMODULE) {
         Self->Root = (RootModule *)ctx->object();
         break;
      }
      ctx = ctx->Stack;
   }

   bool save = false;
   if (!glClassDB.contains(Self->ClassID)) {
      save = fs_initialised ? true : false;

      glClassDB[Self->ClassID] = [Self]() {
         #ifdef __ANDROID__
            // On Android, all libraries are stored in the libs/ folder with no sub-folder hierarchy.  Because of this,
            // we rewrite the path to fit the Android system.

            if (Self->Path) {
               LONG i = StrLength(Self->Path);
               while ((i > 0) and (Self->Path[i] != '/') and (Self->Path[i] != '\\') and (Self->Path[i] != ':')) i--;
               if (i > 0) i++; // Skip folder separator.

               return ClassRecord(Self, make_optional(Self->Path.substr(i)));
            }
            else return ClassRecord(Self);
         #else
            return ClassRecord(Self);
         #endif
      }();
   }

#ifndef PARASOL_STATIC
   if (save) {
      // Saving is only necessary if this is a new dictionary entry.
      if (auto lock = std::unique_lock{glmClassDB, 3s}) {
         static bool write_attempted = false;
         if ((!glClassFile) and (!write_attempted)) {
            write_attempted = true;
            auto flags = FL::WRITE;
            if (AnalysePath(glClassBinPath, NULL) != ERR_Okay) flags |= FL::NEW;

            auto file = objFile::create::untracked(fl::Name("class_dict_output"), fl::Path(glClassBinPath), fl::Flags(flags),
               fl::Permissions(PERMIT::USER_READ|PERMIT::USER_WRITE|PERMIT::GROUP_READ|PERMIT::GROUP_WRITE|PERMIT::OTHERS_READ));

            if (!file) return ERR_File;
            glClassFile = file;

            LONG hdr = CLASSDB_HEADER;
            glClassFile->write(&hdr, sizeof(hdr));
         }

         if (glClassFile) {
            glClassFile->seekEnd(0);
            glClassDB[Self->ClassID].write(glClassFile);
         }
      }
      else return log.warning(ERR_SystemLocked);
   }
#endif

   return ERR_Okay;
}

//********************************************************************************************************************

ERROR CLASS_NewObject(extMetaClass *Self, APTR Void)
{
   new (Self) extMetaClass;
   Self->Integral[0] = 0xff;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Actions: Defines the actions supported by the class.

It is common practice when developing classes to write code for actions that will enhance class functionality.
Action support is defined by listing a series of action ID's paired with customised routines.  The list will be
copied to a jump table that is used internally.  After this operation, the original action list will serve no further
purpose.

The following example shows an action list array borrowed from the @Picture class:

<pre>
ActionArray clActions[] = {
   { AC_Free,          PIC_Free },
   { AC_NewObject,     PIC_NewObject },
   { AC_Init,          PIC_Init },
   { AC_Query,         PIC_Query },
   { AC_Read,          PIC_Read },
   { AC_SaveToObject,  PIC_SaveToObject },
   { AC_Seek,          PIC_Seek },
   { AC_Write,         PIC_Write },
   { 0, NULL }
};
</pre>

Never define method ID's in an action list - the #Methods field is provided for this purpose.

*********************************************************************************************************************/

static ERROR SET_Actions(extMetaClass *Self, const ActionArray *Actions)
{
   if (!Actions) return ERR_Failed;

   Self->ActionTable[AC_Signal].PerformAction = &DEFAULT_Signal;

   for (auto i=0; Actions[i].ActionCode; i++) {
      auto code = Actions[i].ActionCode;
      if ((code < AC_END) and (code > 0)) {
         Self->ActionTable[code].PerformAction = (ERROR (*)(OBJECTPTR, APTR))Actions[i].Routine;
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
ActionTable: This field can be read to retrieve a MetaClass object's internal action table.

This field retrieves the internal action table of a class. The action table is arranged into a jump
table of action routines, with each routine pointing directly to the object support functions.  The size of the
jump table is defined by the global constant `AC_END`.  The table is sorted by action ID.

It is possible to check if an action is supported by a class by looking up its index within the ActionTable, for
example `Routine[AC_Read]`.  Calling an action routine directly is an illegal operation unless
A) The call is made from an action support function in a class module and B) Special circumstances allow for such
a call, as documented in the Action Support Guide.

*********************************************************************************************************************/

static ERROR GET_ActionTable(extMetaClass *Self, ActionEntry **Value, LONG *Elements)
{
   *Value = Self->ActionTable;
   *Elements = AC_END;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
BaseClassID: Specifies the base class ID of a class object.

Prior to the initialisation of a MetaClass object, this field must be set to the base class ID that the class
will represent.  Class ID's are generated as a hash from the class #Name, so if this field is undefined then it
is generated from the Name automatically.

-FIELD-
ClassID: Specifies the ID of a class object.

The ClassID uniquely identifies a class.  If this value differs from the BaseClassID, then the class is determined
to be a sub-class.  The ClassID must be a value that is hashed from the class #Name using ~StrHash().

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
Dictionary: Returns a field lookup table sorted by field IDs.

Following initialisation of the MetaClass, the Dictionary can be read to retrieve the internal field lookup table.
For base-classes, the client can use the binary search technique to find fields by their ID.  For sub-classes, use
linear scanning.

*********************************************************************************************************************/

static ERROR GET_Dictionary(extMetaClass *Self, struct Field **Value, LONG *Elements)
{
   if (Self->initialised()) {
      *Value    = Self->FieldLookup.data();
      *Elements = Self->FieldLookup.size();
      return ERR_Okay;
   }
   else return ERR_NotInitialised;
}

/*********************************************************************************************************************

-FIELD-
Fields: Points to a field array that describes the class' object structure.

This field points to an array that describes the structural arrangement of the objects that will be generated from the
class.  If creating a base class then it must be provided, while sub-classes will inherit this array from their base.

The Class Development Guide has a section devoted to the configuration of this array. Please read the guide for more
information.

*********************************************************************************************************************/

static ERROR GET_Fields(extMetaClass *Self, const FieldArray **Fields, LONG *Elements)
{
   *Fields = Self->Fields;
   *Elements = Self->OriginalFieldTotal;
   return ERR_Okay;
}

static ERROR SET_Fields(extMetaClass *Self, const FieldArray *Fields, LONG Elements)
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

/*********************************************************************************************************************

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
Location: Returns the path from which the class binary is loaded.

The path from which the class binary was loaded is readable from this field.  The path may not necessarily include the
file extension of the source binary.

*********************************************************************************************************************/

static ERROR GET_Location(extMetaClass *Self, CSTRING *Value)
{
   if (Self->Path) { *Value = Self->Path; return ERR_Okay; }
   if (Self->Location) { *Value = Self->Location; return ERR_Okay; }

   if (Self->ClassID) {
      if (glClassDB.contains(Self->ClassID)) {
         Self->Location = StrClone(glClassDB[Self->ClassID].Path.c_str());
      }
   }
   else if (glClassDB.contains(Self->BaseClassID)) {
      Self->Location = StrClone(glClassDB[Self->BaseClassID].Path.c_str());
   }

   if ((*Value = Self->Location)) return ERR_Okay;
   else return ERR_Failed;
}

/*********************************************************************************************************************

-FIELD-
Methods: Set this field to define the methods supported by the class.

If a class design will include support for methods, create a MethodEntry and set this field prior to initialisation.
A method array provides information on each method's ID, name, arguments, and structure size.

The Class Development Guide has a section devoted to method configuration.  Please read the guide for further
information.

*********************************************************************************************************************/

static ERROR GET_Methods(extMetaClass *Self, const MethodEntry **Methods, LONG *Elements)
{
   *Methods = Self->Methods.data();
   *Elements = Self->Methods.size();
   return ERR_Okay;
}

static ERROR SET_Methods(extMetaClass *Self, const MethodEntry *Methods, LONG Elements)
{
   pf::Log log;

   Self->Methods.clear();
   if (!Methods) return ERR_Okay;

   // Search for the method with the lowest ID number

   LONG lowest = 0;
   for (LONG i=0; Methods[i].MethodID; i++) {
      if (Methods[i].MethodID < lowest) lowest = Methods[i].MethodID;
   }

   // Generate the method array.  Note that the first entry that we put in the array will
   // be NULL because methods start at -1, not zero.

   if (lowest < 0) {
      log.msg("Detected %d methods in class %s.", -lowest, Self->ClassName ? Self->ClassName : (STRING)"Unnamed");
      Self->Methods.resize((-lowest) + 1);
      for (unsigned i=0; Methods[i].MethodID; i++) {
         Self->Methods[-Methods[i].MethodID].MethodID = Methods[i].MethodID;
         Self->Methods[-Methods[i].MethodID].Routine  = Methods[i].Routine;
         Self->Methods[-Methods[i].MethodID].Size     = Methods[i].Size;
         Self->Methods[-Methods[i].MethodID].Name     = Methods[i].Name;
         Self->Methods[-Methods[i].MethodID].Args     = Methods[i].Args;
      }

      // NOTE: If this is a sub-class, the initialisation process will add the base-class methods to the list.
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Module: The name of the module binary that initialised the class.

*********************************************************************************************************************/

static ERROR GET_Module(extMetaClass *Self, CSTRING *Value)
{
   if (!Self->initialised()) return ERR_NotInitialised;

   if (Self->Root) {
      *Value = Self->Root->LibraryName;
      return ERR_Okay;
   }
   else {
      *Value = "core";
      return ERR_Okay;
   }
}

/*********************************************************************************************************************

-FIELD-
PrivateObjects: Returns an allocated list of all objects that belong to this class.

This field will compile a list of all objects that belong to the class.  The list is sorted with the oldest
object appearing first.

The resulting array must be terminated with ~FreeResource() after use.

*********************************************************************************************************************/

static ERROR GET_PrivateObjects(extMetaClass *Self, OBJECTID **Array, LONG *Elements)
{
   pf::Log log;
   std::list<OBJECTID> objlist;

   if (auto lock = std::unique_lock{glmMemory}) {
      for (const auto & [ id, mem ] : glPrivateMemory) {
         OBJECTPTR object;
         if (((mem.Flags & MEM::OBJECT) != MEM::NIL) and (object = (OBJECTPTR)mem.Address)) {
            if (Self->Class->ClassID IS object->Class->ClassID) {
               objlist.push_back(object->UID);
            }
         }
      }
   }
   else return log.warning(ERR_SystemLocked);

   if (!objlist.size()) {
      *Array = NULL;
      *Elements = 0;
      return ERR_Okay;
   }

   objlist.sort([](const OBJECTID &a, const OBJECTID &b) { return (a < b); });

   OBJECTID *result;
   if (!AllocMemory(sizeof(OBJECTID) * objlist.size(), MEM::NO_CLEAR, (APTR *)&result, NULL)) {
      LONG i = 0;
      for (const auto & id : objlist) result[i++] = id;
      *Array = result;
      *Elements = objlist.size();
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*********************************************************************************************************************

-FIELD-
OpenCount: The total number of active objects that are linked back to the MetaClass.

Reading this field will reveal how many objects are currently using the class.  This figure will fluctuate over
time as new objects are created and old ones are destroyed.  When the OpenCount reaches zero, the system may flush the
@Module that the class is related to, so long as no more programs are using it or any other classes created by
the module.

-FIELD-
Path: The path to the module binary that represents the class.

The Path field must be set on initialisation and refers to the default location of the class' module binary, for
example `modules:display`. For reasons of platform portability, the file extension must not be specified at the
end of the file name.

-FIELD-
RootModule: Returns a direct reference to the RootModule object that hosts the class.

*********************************************************************************************************************/

static ERROR GET_RootModule(extMetaClass *Self, class RootModule **Value)
{
   *Value = Self->Root;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Size: The total size of the object structure represented by the MetaClass.

This field value must indicate the byte size of the objects that will be created from the MetaClass.  For example, the
@Picture class defines this value as `sizeof(objPicture)`.

If the size is not explicitly defined, the initialisation process will determine the structure size by
evaluating the field definitions that have been provided.
-END-

*********************************************************************************************************************/

static ERROR GET_SubFields(extMetaClass *Self, const FieldArray **Fields, LONG *Elements)
{
   if (Self->SubFields) {
      LONG i;
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

//********************************************************************************************************************
// Build the FieldLookup table, which is sorted by FieldID.  For base-classes this is straight-forward.
//
// For sub-classes the FieldLookup table is inherited from the base, then it may be modified by the sub-class
// blueprint.  Furthermore if the sub-class defines additional fields, these are appended to the FieldLookup table so
// that there is no interference with the order of fields at the base-class level.  This arrangement is important
// because some optimisations rely on the field order being consistent between base-class and sub-class
// implementations.

static void field_setup(extMetaClass *Class)
{
   if (Class->Base) { // This is a sub-class.
      Class->FieldLookup = Class->Base->FieldLookup;
      Class->BaseCeiling = Class->FieldLookup.size();

      if (Class->SubFields) {
         std::vector<Field> subFields;
         UWORD offset = 0;
         for (unsigned i=0; Class->SubFields[i].Name; i++) {
            bool found = false;
            auto hash = StrHash(Class->SubFields[i].Name, false);
            for (unsigned j=0; j < Class->FieldLookup.size(); j++) {
               if (Class->FieldLookup[j].FieldID IS hash) {
                  if (Class->SubFields[i].GetField) {
                     Class->FieldLookup[j].GetValue = (ERROR (*)(APTR, APTR))Class->SubFields[i].GetField;
                     Class->FieldLookup[j].Flags |= FDF_R;
                  }

                  if (Class->SubFields[i].SetField) {
                     Class->FieldLookup[j].SetValue = Class->SubFields[i].SetField;
                     if (Class->FieldLookup[j].Flags & (FDF_W|FDF_I));
                     else Class->FieldLookup[j].Flags |= FDF_W;
                  }

                  optimise_write_field(Class->FieldLookup[j]);

                  found = true;
                  break;
               }
            }

            if (!found) add_field(Class, subFields, Class->SubFields[i], offset);
         }

         if (glLogLevel >= 2) register_fields(subFields);

         sort_class_fields(Class, subFields);

         Class->FieldLookup.insert(Class->FieldLookup.end(), subFields.begin(), subFields.end());
      }
   }
   else {
      bool name_field   = true;
      bool owner_field  = true;
      auto class_fields = Class->Fields;
      UWORD offset      = sizeof(BaseClass);
      for (unsigned i=0; class_fields[i].Name; i++) {
         add_field(Class, Class->FieldLookup, class_fields[i], offset);

         if (Class->FieldLookup[i].FieldID IS FID_Name) name_field = false;
         else if (Class->FieldLookup[i].FieldID IS FID_Owner) owner_field = false;
      }

      // Add mandatory system fields that haven't already been defined.

      if (name_field) {
         Class->FieldLookup.push_back({
            .Arg        = 0,
            .GetValue   = (ERROR (*)(APTR, APTR))&OBJECT_GetName,
            .SetValue   = (APTR)&OBJECT_SetName,
            .WriteValue = &writeval_default,
            .Name       = "Name",
            .FieldID    = FID_Name,
            .Offset     = 0,
            .Index      = 0,
            .Flags      = FDF_STRING|FDF_RW|FDF_SYSTEM
         });
      }

      if (owner_field) {
         Class->FieldLookup.push_back({
            .Arg        = 0,
            .GetValue   = (ERROR (*)(APTR, APTR))&OBJECT_GetOwner,
            .SetValue   = (APTR)&OBJECT_SetOwner,
            .WriteValue = &writeval_default,
            .Name       = "Owner",
            .FieldID    = FID_Owner,
            .Offset     = 0,
            .Index      = 0,
            .Flags      = FDF_OBJECTID|FDF_RW|FDF_SYSTEM
         });
      }

      // Add the Class field.  This is provided primarily to help scripting languages like Fluid.

      Class->FieldLookup.push_back({
         .Arg        = 0,
         .GetValue   = (ERROR (*)(APTR, APTR))&OBJECT_GetClass,
         .SetValue   = NULL,
         .WriteValue = &writeval_default,
         .Name       = "Class",
         .FieldID    = FID_Class,
         .Offset     = 0,
         .Index      = 0,
         .Flags      = FDF_OBJECT|FDF_POINTER|FDF_R|FDF_SYSTEM
      });

      // Add the ClassID field

      Class->FieldLookup.push_back({
         .Arg        = 0,
         .GetValue   = (ERROR (*)(APTR, APTR))&OBJECT_GetClassID,
         .SetValue   = NULL,
         .WriteValue = &writeval_default,
         .Name       = "ClassID",
         .FieldID    = FID_ClassID,
         .Offset     = 0,
         .Index      = 0,
         .Flags      = FDF_LONG|FDF_UNSIGNED|FDF_R|FDF_SYSTEM
      });

      if (glLogLevel >= 2) register_fields(Class->FieldLookup);

      // Build a list of integral objects before we do the sort

      ULONG integral[ARRAYSIZE(Class->Integral)];

      UBYTE int_count = 0;
      if ((Class->Flags & CLF::PROMOTE_INTEGRAL) != CLF::NIL) {
         for (unsigned i=0; i < Class->FieldLookup.size(); i++) {
            if (Class->FieldLookup[i].Flags & FD_INTEGRAL) {
               Class->Integral[int_count] = i;
               integral[int_count++] = Class->FieldLookup[i].FieldID;
               if (int_count >= ARRAYSIZE(Class->Integral)-1) break;
            }
         }
      }
      Class->Integral[int_count] = 0xff;

      sort_class_fields(Class, Class->FieldLookup);

      // Repair integral indexes

      for (unsigned i=0; i < int_count; i++) {
         for (unsigned j=0; j < Class->FieldLookup.size(); j++) {
            if (integral[i] IS Class->FieldLookup[j].FieldID) {
               Class->Integral[i] = j;
               break;
            }
         }
      }

      Class->BaseCeiling = Class->FieldLookup.size();
   }

   Class->Dictionary = Class->FieldLookup.data(); // This client-accessible lookup reference aids optimisation.
}

//********************************************************************************************************************
// Register a hashed field ID and its corresponding name.  Use FieldName() to retrieve field names from the store.

static void register_fields(std::vector<Field> &Fields)
{
   if (auto lock = std::unique_lock{glmFieldKeys, 1s}) {
      for (auto &field : Fields) {
         if (!glFields.contains(field.FieldID)) glFields[field.FieldID] = field.Name;
      }
   }
}

//********************************************************************************************************************

static void add_field(extMetaClass *Class, std::vector<Field> &Fields, const FieldArray &Source, UWORD &Offset)
{
   pf::Log log(__FUNCTION__);

   auto &field = Fields.emplace_back(
      Source.Arg,
      (ERROR (*)(APTR, APTR))Source.GetField,
      Source.SetField,
      writeval_default,
      Source.Name,
      StrHash(Source.Name, false),
      Offset,
      0,
      Source.Flags
   );

   if (field.Flags & FD_VIRTUAL); // No offset will be added for virtual fields
   else if (field.Flags & FD_RGB) Offset += sizeof(BYTE) * 4;
   else if (field.Flags & (FD_POINTER|FD_ARRAY)) {
      #ifdef _LP64
         if (Offset & 0x7) { // Check for mis-alignment
            Offset = (Offset + 7) & (~0x7);
            if (((field.Flags & FDF_R) and (!field.GetValue)) or
                ((field.Flags & FDF_W) and (!field.SetValue))) {
               log.warning("Misaligned 64-bit pointer '%s' in class '%s'.", field.Name, Class->ClassName);
            }
         }
      #endif
      Offset += sizeof(APTR);
   }
   else if (field.Flags & FD_LONG) Offset += sizeof(LONG);
   else if (field.Flags & FD_BYTE) Offset += sizeof(BYTE);
   else if (field.Flags & FD_FUNCTION) Offset += sizeof(FUNCTION);
   else if (field.Flags & (FD_DOUBLE|FD_LARGE)) {
      if (Offset & 0x7) {
         if (((field.Flags & FDF_R) and (!field.GetValue)) or
             ((field.Flags & FDF_W) and (!field.SetValue))) {
            log.warning("Misaligned 64-bit field '%s' in class '%s'.", field.Name, Class->ClassName);
         }
      }
      Offset += 8;
   }
   else log.warning("%s field \"%s\"/%d has an invalid flag setting.", Class->ClassName, field.Name, field.FieldID);

   optimise_write_field(field);
}

//********************************************************************************************************************
// Sort the field table by FID and compute the field indexes.

static void sort_class_fields(extMetaClass *Class, std::vector<Field> &Fields)
{
   std::sort(Fields.begin(), Fields.end(),
      [](const Field &a, const Field &b ) { return a.FieldID < b.FieldID; }
   );

   for (unsigned i=0; i < Fields.size(); i++) Fields[i].Index = i;
}

//********************************************************************************************************************
// These are pre-defined fields that are applied to each class' object.

static ERROR OBJECT_GetClass(OBJECTPTR Self, extMetaClass **Value)
{
   *Value = Self->ExtClass;
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR OBJECT_GetClassID(OBJECTPTR Self, CLASSID *Value)
{
   *Value = Self->Class->ClassID;
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR OBJECT_GetOwner(OBJECTPTR Self, OBJECTID *OwnerID)
{
   *OwnerID = Self->ownerID();
   return ERR_Okay;
}

static ERROR OBJECT_SetOwner(OBJECTPTR Self, OBJECTID OwnerID)
{
   pf::Log log;

   if (OwnerID) {
      OBJECTPTR newowner;
      if (!AccessObject(OwnerID, 2000, &newowner)) {
         SetOwner(Self, newowner);
         ReleaseObject(newowner);
         return ERR_Okay;
      }
      else return log.warning(ERR_ExclusiveDenied);
   }
   else return log.warning(ERR_NullArgs);
}

//********************************************************************************************************************

static ERROR OBJECT_GetName(OBJECTPTR Self, STRING *Name)
{
   *Name = Self->Name;
   return ERR_Okay;
}

static ERROR OBJECT_SetName(OBJECTPTR Self, CSTRING Name)
{
   if (!Name) return SetName(Self, "");
   else return SetName(Self, Name);
}

//********************************************************************************************************************
// [Refer to register_class() to see how classes are recognised]
//
// If the classes.bin file is missing or incomplete, this code will scan for every module installed in the system and
// initialise it so that all classes can be registered in the class database.

#ifndef PARASOL_STATIC
void scan_classes(void)
{
   pf::Log log("Core");

   log.branch("Scanning for available classes.");

   glClassDB.clear();
   DeleteFile(glClassBinPath, NULL);

   DirInfo *dir;
   if (!OpenDir("modules:", RDF::QUALIFY, &dir)) {
      LONG total = 0;
      while (!ScanDir(dir)) {
         FileInfo *list = dir->Info;

         if ((list->Flags & RDF::FILE) != RDF::NIL) {
            #ifdef __ANDROID__
               if (!StrCompare("libshim.", list->Name)) continue;
               if (!StrCompare("libcore.", list->Name)) continue;
            #else
               if (!StrCompare("core.", list->Name)) continue;
            #endif

            auto modules = std::string("modules:") + list->Name;

            log.msg("Loading module for class scan: %s", modules.c_str());

            objModule::create mod = { fl::Name(modules), fl::Flags(MOF::SYSTEM_PROBE) };

            total++;
         }

         // For every 16 modules loaded, run an expunge.  This keeps memory usage down, and on Android
         // is essential because there is a library limit.

         if ((total & 0x1f) IS 0x10) Expunge(FALSE);
      }
      FreeResource(dir);
   }

   log.msg("Class scan complete.");
}
#endif

//********************************************************************************************************************
// Lookup the fields declared by a MetaClass, as opposed to the fields of the MetaClass itself.

static Field * lookup_id_byclass(extMetaClass *Class, ULONG FieldID, extMetaClass **Result)
{
   auto &field = Class->FieldLookup;

   LONG floor = 0;
   LONG ceiling = Class->BaseCeiling;
   while (floor < ceiling) {
      LONG i = (floor + ceiling)>>1;
      if (field[i].FieldID < FieldID) floor = i + 1;
      else if (field[i].FieldID > FieldID) ceiling = i;
      else {
         *Result = Class;
         return &field[i];
      }
   }

   // Sub-class fields (located in the upper register of FieldLookup)

   if (Class->BaseCeiling < Class->FieldLookup.size()) {
      unsigned floor = Class->BaseCeiling;
      unsigned ceiling = Class->FieldLookup.size();
      while (floor < ceiling) {
         LONG i = (floor + ceiling)>>1;
         if (field[i].FieldID < FieldID) floor = i + 1;
         else if (field[i].FieldID > FieldID) ceiling = i;
         else {
            *Result = Class;
            return &field[i];
         }
      }
   }

   for (unsigned i=0; Class->Integral[i] != 0xff; i++) {
      auto &field = Class->FieldLookup[Class->Integral[i]];
      if (field.Arg) {
         if (auto child_class = (extMetaClass *)FindClass(field.Arg)) {
            *Result = child_class;
            if (auto child_field = lookup_id_byclass(child_class, FieldID, Result)) return child_field;
            *Result = NULL;
         }
      }
   }

   return NULL;
}
