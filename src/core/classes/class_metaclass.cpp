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

static ERROR field_setup(extMetaClass *);
static void sort_class_fields(extMetaClass *, std::vector<Field> &);

static void add_field(extMetaClass *, const FieldArray *, UWORD &);
static void register_fields(extMetaClass *);
static Field * lookup_id_byclass(extMetaClass *, ULONG, extMetaClass **);

//********************************************************************************************************************
// The MetaClass is the focal point of the OO design model.  Because classes are treated like objects, they must point
// back to a controlling class definition - this it.  See NewObject() for the management code for this data.

#define TOTAL_METAFIELDS  25
#define TOTAL_METAMETHODS 1

static ERROR GET_ActionTable(extMetaClass *, ActionEntry **, LONG *);
static ERROR GET_Fields(extMetaClass *, const FieldArray **, LONG *);
static ERROR GET_Location(extMetaClass *, CSTRING *);
static ERROR GET_Methods(extMetaClass *Self, const MethodArray **, LONG *);
static ERROR GET_Module(extMetaClass *, CSTRING *);
static ERROR GET_PrivateObjects(extMetaClass *, OBJECTID **, LONG *);
static ERROR GET_RootModule(extMetaClass *, struct RootModule **);
static ERROR GET_Dictionary(extMetaClass *, struct Field **, LONG *);
static ERROR GET_SubFields(extMetaClass *, const FieldArray **, LONG *);
static ERROR GET_TotalMethods(extMetaClass *, LONG *);

static ERROR SET_Actions(extMetaClass *, const ActionArray *);
static ERROR SET_Fields(extMetaClass *, const FieldArray *, LONG);
static ERROR SET_Methods(extMetaClass *, const MethodArray *, LONG);

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
   { "Command",  CCF_COMMAND },  { "Drawable",   CCF_DRAWABLE },
   { "Effect",   CCF_EFFECT },   { "Filesystem", CCF_FILESYSTEM },
   { "Graphics", CCF_GRAPHICS }, { "GUI",        CCF_GUI },
   { "IO",       CCF_IO },       { "System",     CCF_SYSTEM },
   { "Tool",     CCF_TOOL },     { "Data",       CCF_DATA },
   { "Audio",    CCF_AUDIO },    { "Misc",       CCF_MISC },
   { "Network",  CCF_NETWORK },  { "Multimedia", CCF_MULTIMEDIA },
   { NULL, 0 }
};

static const std::vector<Field> glMetaFieldsPreset = {
   // If you adjust this table, remember to change TOTAL_METAFIELDS, adjust the index numbers and the byte offsets into the structure.
   { 0, NULL, NULL,                writeval_default, "ClassVersion",    FID_ClassVersion, sizeof(BaseClass), 0, FDF_DOUBLE|FDF_RI },
   { (MAXINT)"MethodArray", (ERROR (*)(APTR, APTR))GET_Methods, (APTR)SET_Methods, writeval_default, "Methods", FID_Methods,      sizeof(BaseClass)+8,              1, FDF_ARRAY|FD_STRUCT|FDF_RI },
   { (MAXINT)"FieldArray", (ERROR (*)(APTR, APTR))GET_Fields, (APTR)SET_Fields, writeval_default, "Fields",     FID_Fields,       sizeof(BaseClass)+8+sizeof(APTR), 2, FDF_ARRAY|FD_STRUCT|FDF_RI },
   { (MAXINT)"Field",      (ERROR (*)(APTR, APTR))GET_Dictionary, NULL, writeval_default, "Dictionary",         FID_Dictionary,   sizeof(BaseClass)+8+(sizeof(APTR)*2), 3, FDF_ARRAY|FD_STRUCT|FDF_R },
   { 0, NULL, NULL,                writeval_default, "ClassName",       FID_ClassName,       sizeof(BaseClass)+8+(sizeof(APTR)*3),  4,  FDF_STRING|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "FileExtension",   FID_FileExtension,   sizeof(BaseClass)+8+(sizeof(APTR)*4),  5,  FDF_STRING|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "FileDescription", FID_FileDescription, sizeof(BaseClass)+8+(sizeof(APTR)*5),  6,  FDF_STRING|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "FileHeader",      FID_FileHeader,      sizeof(BaseClass)+8+(sizeof(APTR)*6),  7,  FDF_STRING|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "Path",            FID_Path,            sizeof(BaseClass)+8+(sizeof(APTR)*7),  8,  FDF_STRING|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "Size",            FID_Size,            sizeof(BaseClass)+8+(sizeof(APTR)*8),  9,  FDF_LONG|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "Flags",           FID_Flags,           sizeof(BaseClass)+12+(sizeof(APTR)*8), 10, FDF_LONG|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "SubClassID",      FID_SubClassID,      sizeof(BaseClass)+16+(sizeof(APTR)*8), 11, FDF_LONG|FDF_UNSIGNED|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "BaseClassID",     FID_BaseClassID,     sizeof(BaseClass)+20+(sizeof(APTR)*8), 12, FDF_LONG|FDF_UNSIGNED|FDF_RI },
   { 0, NULL, NULL,                writeval_default, "OpenCount",       FID_OpenCount,       sizeof(BaseClass)+24+(sizeof(APTR)*8), 13, FDF_LONG|FDF_R },
   { 0, (ERROR (*)(APTR, APTR))GET_TotalMethods, 0, writeval_default,   "TotalMethods", FID_TotalMethods, sizeof(BaseClass)+28+(sizeof(APTR)*8), 14, FDF_LONG|FDF_R },
   { 0, NULL, NULL,                writeval_default, "TotalFields",     FID_TotalFields,     sizeof(BaseClass)+32+(sizeof(APTR)*8), 15, FDF_LONG|FDF_R },
   { (MAXINT)&CategoryTable, NULL, NULL, writeval_default, "Category",  FID_Category,        sizeof(BaseClass)+36+(sizeof(APTR)*8), 16, FDF_LONG|FDF_LOOKUP|FDF_RI },
   // Virtual fields
   { 0, NULL, (APTR)SET_Actions,   writeval_default, "Actions",         FID_Actions,         sizeof(BaseClass), 17, FDF_POINTER|FDF_I },
   { 0, (ERROR (*)(APTR, APTR))GET_ActionTable, 0,  writeval_default,   "ActionTable",       FID_ActionTable,     sizeof(BaseClass), 18, FDF_ARRAY|FDF_POINTER|FDF_R },
   { 0, (ERROR (*)(APTR, APTR))GET_Location, 0,     writeval_default,   "Location",          FID_Location,        sizeof(BaseClass), 19, FDF_STRING|FDF_R },
   { 0, (ERROR (*)(APTR, APTR))GET_ClassName, (APTR)SET_ClassName, writeval_default, "Name", FID_Name,            sizeof(BaseClass), 20, FDF_STRING|FDF_SYSTEM|FDF_RI },
   { 0, (ERROR (*)(APTR, APTR))GET_Module, 0,       writeval_default,   "Module",            FID_Module,          sizeof(BaseClass), 21, FDF_STRING|FDF_R },
   { 0, (ERROR (*)(APTR, APTR))GET_PrivateObjects, 0, writeval_default, "PrivateObjects",    FID_PrivateObjects,  sizeof(BaseClass), 22, FDF_ARRAY|FDF_LONG|FDF_ALLOC|FDF_R },
   { (MAXINT)"FieldArray", (ERROR (*)(APTR, APTR))GET_SubFields, 0, writeval_default, "SubFields", FID_SubFields, sizeof(BaseClass), 23, FDF_ARRAY|FD_STRUCT|FDF_SYSTEM|FDF_R },
   { ID_ROOTMODULE, (ERROR (*)(APTR, APTR))GET_RootModule, 0, writeval_default, "RootModule", FID_RootModule,     sizeof(BaseClass), 24, FDF_OBJECT|FDF_R },
   { 0, 0, 0, NULL, "", 0, 0, 0,  0 }
};

static const FieldArray glMetaFields[] = {
   { "ClassVersion",    FDF_DOUBLE|FDF_RI },
   { "Methods",         FDF_ARRAY|FD_STRUCT|FDF_RI, GET_Methods, SET_Methods, "MethodArray" },
   { "Fields",          FDF_ARRAY|FD_STRUCT|FDF_RI, GET_Fields, SET_Fields, "FieldArray" },
   { "Dictionary",      FDF_ARRAY|FD_STRUCT|FDF_R, GET_Dictionary, NULL, "Field" },
   { "ClassName",       FDF_STRING|FDF_RI },
   { "FileExtension",   FDF_STRING|FDF_RI },
   { "FileDescription", FDF_STRING|FDF_RI },
   { "FileHeader",      FDF_STRING|FDF_RI },
   { "Path",            FDF_STRING|FDF_RI },
   { "Size",            FDF_LONG|FDF_RI },
   { "Flags",           FDF_LONG|FDF_RI },
   { "SubClassID",      FDF_LONG|FDF_UNSIGNED|FDF_RI },
   { "BaseClassID",     FDF_LONG|FDF_UNSIGNED|FDF_RI },
   { "OpenCount",       FDF_LONG|FDF_R },
   { "TotalMethods",    FDF_LONG|FDF_R },
   { "TotalFields",     FDF_LONG|FDF_R },
   { "Category",        FDF_LONG|FDF_LOOKUP|FDF_RI, NULL, NULL, &CategoryTable },
   // Virtual fields
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

static MethodArray glMetaMethods[TOTAL_METAMETHODS+2] = {
   { 0, 0, 0, 0, 0 },
   { -1, (APTR)CLASS_FindField, "FindField", argsFindField, sizeof(struct mcFindField) },
   { 0, 0, 0, 0, 0 }
};

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
   glMetaClass.BaseClass::ClassID = ID_METACLASS;
   glMetaClass.BaseClass::SubID   = ID_METACLASS;
   glMetaClass.BaseClass::UID     = 123;
   glMetaClass.BaseClass::Flags   = NF::INITIALISED;

   glMetaClass.ClassVersion       = 1;
   glMetaClass.Methods            = glMetaMethods;
   glMetaClass.Fields             = glMetaFields;
   glMetaClass.ClassName          = "MetaClass";
   glMetaClass.Size               = sizeof(extMetaClass);
   glMetaClass.SubClassID         = ID_METACLASS;
   glMetaClass.BaseClassID        = ID_METACLASS;
   glMetaClass.TotalMethods       = TOTAL_METAMETHODS;
   glMetaClass.TotalFields        = TOTAL_METAFIELDS;
   glMetaClass.Category           = CCF_SYSTEM;
   glMetaClass.prvDictionary      = glMetaFieldsPreset;
   glMetaClass.OriginalFieldTotal = ARRAYSIZE(glMetaFields)-1;

   glMetaClass.ActionTable[AC_Free].PerformAction = (ERROR (*)(OBJECTPTR, APTR))CLASS_Free;
   glMetaClass.ActionTable[AC_Init].PerformAction = (ERROR (*)(OBJECTPTR, APTR))CLASS_Init;
   glMetaClass.ActionTable[AC_NewObject].PerformAction = (ERROR (*)(OBJECTPTR, APTR))CLASS_NewObject;
   glMetaClass.ActionTable[AC_Signal].PerformAction = &DEFAULT_Signal;

   sort_class_fields(&glMetaClass, glMetaClass.prvDictionary);

   glClassMap[ID_METACLASS] = &glMetaClass;
}

/*********************************************************************************************************************

-METHOD-
FindField: Search a class definition for a specific field.

This method checks if a class has defined a given field by scanning its blueprint for a matching ID.

If the field is present in an inherited class only, a reference to the inherited class will be returned in the Source
parameter.

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

ERROR CLASS_FindField(extMetaClass *Class, struct mcFindField *Args)
{
   if ((!Args) or (!Args->ID)) return ERR_NullArgs;

   extMetaClass *src = NULL;
   Args->Field = lookup_id_byclass(Class, Args->ID, &src);
   Args->Source = src;
   if (Args->Field) return ERR_Okay;
   else return ERR_Search;
}

//********************************************************************************************************************

ERROR CLASS_Free(extMetaClass *Self, APTR Void)
{
   if (Self->SubClassID) glClassMap.erase(Self->SubClassID);
   if (Self->Methods)  { FreeResource(Self->Methods);  Self->Methods  = NULL; }
   if (Self->Location) { FreeResource(Self->Location); Self->Location = NULL; }
   Self->~extMetaClass();
   return ERR_Okay;
}

//********************************************************************************************************************

ERROR CLASS_Init(extMetaClass *Self, APTR Void)
{
   pf::Log log;
   extMetaClass *base;

   if (!Self->ClassName) return log.warning(ERR_MissingClassName);

   // Base-class: SubClassID == BaseClassID
   // Sub-class:  SubClassID != BaseClassID
   //
   // If neither ID is specified, the hash is derived from the name and then applied to both SubClassID and BaseClassID.

   if ((Self->BaseClassID) and (!Self->SubClassID)) {
      Self->SubClassID = StrHash(Self->ClassName, FALSE);
   }
   else if (!Self->BaseClassID) {
      if (!Self->SubClassID) Self->SubClassID = StrHash(Self->ClassName, FALSE);
      Self->BaseClassID = Self->SubClassID;
   }

   if (Self->BaseClassID IS Self->SubClassID) {
      if (!Self->Size) Self->Size = sizeof(BaseClass);
      else if (Self->Size < (LONG)sizeof(BaseClass)) { // Object size not specified
         log.warning("Size of %d is not valid.", Self->Size);
         return ERR_FieldNotSet;
      }
   }

   // If this is a subclass, find the base class.  Note that FindClass() will automatically initialise the base if
   // there is a reference for it, so if it returns NULL then it is obvious that the base class is not installed on the
   // user's system.

   if ((Self->BaseClassID) and (Self->SubClassID != Self->BaseClassID)) {
      if ((base = (extMetaClass *)FindClass(Self->BaseClassID))) {
         log.trace("Using baseclass $%.8x (%s) for %s", Self->BaseClassID, base->ClassName, Self->ClassName);
         if (!Self->FileDescription) Self->FileDescription = base->FileDescription;
         if (!Self->FileExtension)   Self->FileExtension   = base->FileExtension;
         if (!Self->ClassVersion)    Self->ClassVersion    = base->ClassVersion;

         // If over-riding field definitions have been specified by the sub-class, move them to the SubFields pointer.

         // NB: Sub-classes may not enlarge object structures, therefore they inherit directly from the base.

         if (Self->Fields) Self->SubFields = Self->Fields;
         Self->Fields = base->Fields;
         Self->OriginalFieldTotal = base->OriginalFieldTotal;

         Self->Flags |= base->Flags; // Allow flag inheritance, e.g. PROMOTE_CHILDREN

         // In tightly controlled configurations, a sub-class can define a structure that is larger than the base
         // class.  Vector filter effects are one example.

         if (!Self->Size) Self->Size = base->Size;
         Self->Base = base;

         // Note: Sub-classes can define their own custom methods independent of the base class, but care must be taken
         // to use a large enough cushion to prevent an overlap of method ID's.

         if ((Self->Methods) and (base->Methods)) {
            if (Self->TotalMethods < base->TotalMethods) { // Expand the method table to match the base class.
               if (!ReallocMemory(Self->Methods, sizeof(MethodArray) * (base->TotalMethods+1), (APTR *)&Self->Methods, NULL)) {
                  Self->TotalMethods = base->TotalMethods;
               }
               else return log.warning(ERR_ReallocMemory);
            }

            // Copy over method information from the base-class (the sub-class' function pointers will
            // not be modified).

            for (LONG i=0; i < base->TotalMethods+1; i++) {
               Self->Methods[i].MethodID = base->Methods[i].MethodID;
               Self->Methods[i].Name = base->Methods[i].Name;
               Self->Methods[i].Args = base->Methods[i].Args;
               Self->Methods[i].Size = base->Methods[i].Size;
            }
         }
         else if ((!Self->Methods) and (base->Methods)) { // Copy methods from the base-class
            if (!AllocMemory(sizeof(MethodArray) * (base->TotalMethods + 1), MEM_DATA, (APTR *)&Self->Methods, NULL)) {
               CopyMemory(base->Methods, Self->Methods, sizeof(MethodArray) * (base->TotalMethods + 1));
               Self->TotalMethods = base->TotalMethods;
            }
            else return ERR_AllocMemory;
         }
      }
      else {
         log.warning("A base for class $%.8x is not present!  Install it.", Self->BaseClassID);
         return ERR_Failed;
      }
   }
   else; // Base class

   if (field_setup(Self) != ERR_Okay) return ERR_Failed;

   glClassMap[Self->SubClassID] = Self;

   // Record the name of the module that owns this class.

   auto ctx = tlContext;
   while (ctx != &glTopContext) {
      if (ctx->object()->ClassID IS ID_ROOTMODULE) {
         Self->Root = (RootModule *)ctx->object();
         break;
      }
      ctx = ctx->Stack;
   }

   bool save = false;
   if (!glClassDB.contains(Self->SubClassID)) {
      save = fs_initialised ? true : false;

      glClassDB[Self->SubClassID] = [Self]() {
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

   if (save) {
      // Saving is only necessary if this is a new dictionary entry.
      ThreadLock lock(TL_CLASSDB, 3000);
      if (lock.granted()) {
         static bool write_attempted = false;
         if ((!glClassFile) and (!write_attempted)) {
            write_attempted = true;
            LONG flags = FL_WRITE;
            if (AnalysePath(glClassBinPath, NULL) != ERR_Okay) flags |= FL_NEW;

            auto file = objFile::create::untracked(fl::Name("class_dict_output"), fl::Path(glClassBinPath), fl::Flags(flags),
               fl::Permissions(PERMIT_USER_READ|PERMIT_USER_WRITE|PERMIT_GROUP_READ|PERMIT_GROUP_WRITE|PERMIT_OTHERS_READ));

            if (!file) return ERR_File;
            glClassFile = file;

            LONG hdr = CLASSDB_HEADER;
            glClassFile->write(&hdr, sizeof(hdr));
         }

         if (glClassFile) {
            glClassFile->seekEnd(0);
            glClassDB[Self->SubClassID].write(glClassFile);
         }
      }
      else return log.warning(ERR_TimeOut);
   }

   return ERR_Okay;
}

//********************************************************************************************************************

ERROR CLASS_NewObject(extMetaClass *Self, APTR Void)
{
   new (Self) extMetaClass;
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
   *Elements = AC_END - 1;
   return ERR_Okay;
}

/*********************************************************************************************************************

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
Dictionary: Returns a field lookup table sorted by field IDs.

Following initialisation of the MetaClass, the Dictionary can be read to retrieve a fast field lookup table that is
sorted by FieldID.  The client should typically use the binary search technique to find fields by their ID.

*********************************************************************************************************************/

static ERROR GET_Dictionary(extMetaClass *Self, struct Field **Value, LONG *Elements)
{
   if (Self->initialised()) {
      *Value    = Self->prvDictionary.data();
      *Elements = Self->prvDictionary.size();
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

   if (Self->SubClassID) {
      if (glClassDB.contains(Self->SubClassID)) {
         Self->Location = StrClone(glClassDB[Self->SubClassID].Path.c_str());
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

If a class design will include support for methods, define them by creating a method list and then
setting this field with the specifications before the class object is initialised.  A method list provides
information on each method's ID, name, arguments, and structure size. When you set the Methods field with your
method list, the information will be processed into a jump table that is used for making method calls.  After this
process, your method list will serve no further purpose.

The Class Development Guide has a section devoted to describing how the method list must be set up.  Please read
the guide for more information.

Never use action ID's in a Methods array - please use the #Actions field for this.

*********************************************************************************************************************/

static ERROR GET_Methods(extMetaClass *Self, const MethodArray **Methods, LONG *Elements)
{
   *Methods = Self->Methods;
   *Elements = Self->TotalMethods;
   return ERR_Okay;
}

static ERROR SET_Methods(extMetaClass *Self, const MethodArray *Methods, LONG Elements)
{
   pf::Log log;

   if (!Methods) return ERR_Failed;

   if (Self->Methods) { FreeResource(Self->Methods); Self->Methods = NULL; }

   // Search for the method with the lowest rating ID number

   LONG j = 0;
   for (LONG i=0; Methods[i].MethodID; i++) {
      if (Methods[i].MethodID < j) j = Methods[i].MethodID;
   }

   // Generate the method array.  Note that the first entry that we put in the array will
   // be NULL because methods start at -1, not zero.

   if (j < 0) {
      log.msg("Detected %d methods in class %s.", -j, Self->ClassName ? Self->ClassName : (STRING)"Unnamed");
      j = (-j) + 2;
      if (!AllocMemory(sizeof(MethodArray) * j, MEM_DATA, (APTR *)&Self->Methods, NULL)) {
         for (LONG i=0; Methods[i].MethodID; i++) {
            if (Methods[i].MethodID >= 0) {
               log.warning("Invalid method ID (%d) detected in the method array.", Methods[i].MethodID);
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

   ThreadLock lock(TL_PRIVATE_MEM, 4000);
   if (lock.granted()) {
      for (const auto & [ id, mem ] : glPrivateMemory) {
         OBJECTPTR object;
         if ((mem.Flags & MEM_OBJECT) and (object = (OBJECTPTR)mem.Address)) {
            if (Self->SubClassID IS object->ClassID) {
               objlist.push_back(object->UID);
            }
         }
      }
   }
   else return log.warning(ERR_LockFailed);

   if (!objlist.size()) {
      *Array = NULL;
      *Elements = 0;
      return ERR_Okay;
   }

   objlist.sort([](const OBJECTID &a, const OBJECTID &b) { return (a < b); });

   OBJECTID *result;
   if (!AllocMemory(sizeof(OBJECTID) * objlist.size(), MEM_NO_CLEAR, (APTR *)&result, NULL)) {
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

static ERROR GET_RootModule(extMetaClass *Self, struct RootModule **Value)
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

-FIELD-
SubClassID: Specifies the sub-class ID of a class object.

The SubClassID field is used to indicate that a class is derived from a base class.  If you are creating a sub-class
then set the SubClassID field to the hash value of the class' name (use the ~StrHash() function).

This field must not be set when creating a base class.

To determine whether or not a class is a sub-class or a base class, compare the BaseClassID and SubClassID fields.  If
they are identical then it is a base class, otherwise it is a sub-class.

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

/*********************************************************************************************************************
-FIELD-
TotalFields: The total number of fields defined by a class.

-FIELD-
TotalMethods: The total number of methods supported by a class.
-END-

*********************************************************************************************************************/

static ERROR GET_TotalMethods(extMetaClass *Class, LONG *Value)
{
   if (Class->TotalMethods > 0) {
      *Value = Class->TotalMethods - 1; // Minus 1 due to the dummy entry at the start
      return ERR_Okay;
   }
   else {
      *Value = 0;
      return ERR_Okay;
   }
}

//********************************************************************************************************************

static ERROR field_setup(extMetaClass *Class)
{
   pf::Log log(__FUNCTION__);

   if (Class->Base) {
      // This is a sub-class.  Clone the field array from the base class, then check for field over-riders specified in
      // the sub-class field list.  Sub-classes can also define additional fields if the fields are virtual.

      Class->prvDictionary = Class->Base->prvDictionary;
      auto &fields = Class->prvDictionary;

      if (Class->SubFields) {
         std::vector<WORD> ext;

         for (LONG i=0; Class->SubFields[i].Name; i++) {
            auto hash = StrHash(Class->SubFields[i].Name, false);
            unsigned j;
            for (j=0; j < Class->prvDictionary.size(); j++) {
               if (fields[j].FieldID IS hash) {
                  if (Class->SubFields[i].GetField) {
                     fields[j].GetValue = (ERROR (*)(APTR, APTR))Class->SubFields[i].GetField;
                     fields[j].Flags |= FDF_R;
                  }

                  if (Class->SubFields[i].SetField) {
                     fields[j].SetValue = Class->SubFields[i].SetField;
                     if (fields[j].Flags & (FDF_W|FDF_I));
                     else fields[j].Flags |= FDF_W;
                  }

                  optimise_write_field(fields[j]);
                  break;
               }
            }

            // If the field was not found in the base, it must be marked virtual or we cannot accept it.

            if (j >= Class->prvDictionary.size()) {
               if (Class->SubFields[i].Flags & FD_VIRTUAL) ext.emplace_back(i);
               else log.warning("%s field %s has no match in the base class (change field to virtual).", Class->ClassName, Class->SubFields[i].Name);
            }
         }

         if (!ext.empty()) {
            unsigned j = Class->prvDictionary.size();
            UWORD offset = 0;
            for (unsigned i=0; i < ext.size(); i++) {
               add_field(Class, Class->SubFields + ext[i], offset);
               Class->prvDictionary[j].Index = j;
               j++;
            }
         }
      }
   }
   else {
      bool name_field  = true;
      bool owner_field = true;
      auto class_fields = (FieldArray *)Class->Fields;
      auto &fields = Class->prvDictionary;
      UWORD offset = sizeof(BaseClass);
      for (unsigned i=0; class_fields[i].Name; i++) {
         add_field(Class, &class_fields[i], offset);
         fields[i].Index = i;

         if (fields[i].FieldID IS FID_Name) name_field = false;
         else if (fields[i].FieldID IS FID_Owner) owner_field = false;
      }

      Class->prvDictionary = fields;

      // Add mandatory system fields that haven't already been defined.

      if (name_field) {
         fields.push_back({
            .Arg      = 0,
            .GetValue = (ERROR (*)(APTR, APTR))&OBJECT_GetName,
            .SetValue = (APTR)&OBJECT_SetName,
            .WriteValue = &writeval_default,
            .Name     = "Name",
            .FieldID  = FID_Name,
            .Offset   = 0,
            .Index    = 0,
            .Flags    = FDF_STRING|FDF_RW|FDF_SYSTEM
         });
      }

      if (owner_field) {
         fields.push_back({
            .Arg      = 0,
            .GetValue = (ERROR (*)(APTR, APTR))&OBJECT_GetOwner,
            .SetValue = (APTR)&OBJECT_SetOwner,
            .WriteValue = &writeval_default,
            .Name     = "Owner",
            .FieldID  = FID_Owner,
            .Offset   = 0,
            .Index    = 0,
            .Flags    = FDF_OBJECTID|FDF_RW|FDF_SYSTEM
         });
      }

      // Add the Class field.  This is provided primarily to help scripting languages like Fluid.

      fields.push_back({
         .Arg       = 0,
         .GetValue  = (ERROR (*)(APTR, APTR))&OBJECT_GetClass,
         .SetValue  = NULL,
         .WriteValue = &writeval_default,
         .Name      = "Class",
         .FieldID   = FID_Class,
         .Offset   = 0,
         .Index    = 0,
         .Flags     = FDF_OBJECT|FDF_POINTER|FDF_R|FDF_SYSTEM
      });

      // Add the ClassID field

      fields.push_back({
         .Arg       = 0,
         .GetValue  = (ERROR (*)(APTR, APTR))&OBJECT_GetClassID,
         .SetValue  = NULL,
         .WriteValue = &writeval_default,
         .Name      = "ClassID",
         .FieldID   = FID_ClassID,
         .Offset   = 0,
         .Index    = 0,
         .Flags     = FDF_LONG|FDF_UNSIGNED|FDF_R|FDF_SYSTEM
      });
   }

   Class->TotalFields = Class->prvDictionary.size();

   if (glLogLevel >= 2) register_fields(Class);

   // Check for field name hash collisions and other significant development errors if logging is enabled.

   auto &fields = Class->prvDictionary;

   if (glLogLevel >= 3) {
      for (unsigned i=0; i < Class->prvDictionary.size(); i++) {
         if (!(fields[i].Flags & FDF_FIELDTYPES)) {
            log.warning("Badly defined type in field \"%s\".", fields[i].Name);
         }

         for (unsigned j=0; j < Class->prvDictionary.size(); j++) {
            if (i IS j) continue;
            if (fields[i].FieldID IS fields[j].FieldID) {
               log.warning("%s: Hash collision - field '%s' collides with '%s'", Class->ClassName, fields[i].Name, fields[j].Name);
            }
         }
      }
   }

   sort_class_fields(Class, fields);
   Class->Dictionary = Class->prvDictionary.data();
   return ERR_Okay;
}

//********************************************************************************************************************
// Register a hashed field ID and its corresponding name.  Use FieldName() to retrieve field names from the store.

static void register_fields(extMetaClass *Class)
{
   ThreadLock lock(TL_FIELDKEYS, 1000);
   if (lock.granted()) {
      for (unsigned i=0; i < Class->prvDictionary.size(); i++) {
         if (!glFields.contains(Class->prvDictionary[i].FieldID)) {
            glFields[Class->prvDictionary[i].FieldID] = Class->prvDictionary[i].Name;
         }
      }
   }
}

//********************************************************************************************************************

static void add_field(extMetaClass *Class, const FieldArray *Source, UWORD &Offset)
{
   pf::Log log(__FUNCTION__);

   auto &field = Class->prvDictionary.emplace_back(
      Source->Arg,
      (ERROR (*)(APTR, APTR))Source->GetField,
      Source->SetField,
      writeval_default,
      Source->Name,
      StrHash(Source->Name, FALSE),
      Offset,
      0,
      Source->Flags
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

/*********************************************************************************************************************
** Sort the field table by name.  We use a shell sort, similar to bubble sort but much faster because it can copy
** records over larger distances.
**
** NOTE: This is also used in NewObject() to sort the fields of the glMetaClass.
*/

static void sort_class_fields(extMetaClass *Class, std::vector<Field> &fields)
{
   ULONG integral[ARRAYSIZE(Class->Integral)];

   // Build a list of integral objects before we do the sort

   UBYTE childcount = 0;
   if (Class->Flags & CLF_PROMOTE_INTEGRAL) {
      for (unsigned i=0; i < Class->prvDictionary.size(); i++) {
         if (fields[i].Flags & FD_INTEGRAL) {
            Class->Integral[childcount] = i;
            integral[childcount++] = fields[i].FieldID;
            if (childcount >= ARRAYSIZE(Class->Integral)-1) break;
         }
      }
   }
   Class->Integral[childcount] = 0xff;

   std::sort(Class->prvDictionary.begin(), Class->prvDictionary.end(),
      [](const Field &a, const Field &b ) {
         return a.FieldID < b.FieldID;
      }
   );

   // Repair integral indexes

   for (unsigned i=0; i < childcount; i++) {
      for (unsigned j=0; j < Class->prvDictionary.size(); j++) {
         if (integral[i] IS fields[j].FieldID) {
            Class->Integral[i] = j;
            break;
         }
      }
   }

   // Repair field indexes

   for (unsigned i=0; i < Class->prvDictionary.size(); i++) fields[i].Index = i;
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
   *Value = Self->ClassID;
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

void scan_classes(void)
{
   pf::Log log("Core");

   log.branch("Scanning for available classes.");

   glClassDB.clear();
   DeleteFile(glClassBinPath, NULL);

   DirInfo *dir;
   if (!OpenDir("modules:", RDF_QUALIFY, &dir)) {
      LONG total = 0;
      while (!ScanDir(dir)) {
         FileInfo *list = dir->Info;

         if (list->Flags & RDF_FILE) {
            #ifdef __ANDROID__
               if (!StrCompare("libshim.", list->Name, 0, 0)) continue;
               if (!StrCompare("libcore.", list->Name, 0, 0)) continue;
            #else
               if (!StrCompare("core.", list->Name, 0, 0)) continue;
            #endif

            auto modules = std::string("modules:") + list->Name;

            log.msg("Loading module for class scan: %s", modules.c_str());

            objModule::create mod = { fl::Name(modules), fl::Flags(MOF_SYSTEM_PROBE) };

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

//********************************************************************************************************************
// Lookup the fields declared by a MetaClass, as opposed to the fields of the MetaClass itself.

static Field * lookup_id_byclass(extMetaClass *Class, ULONG FieldID, extMetaClass **Result)
{
   auto &field = Class->prvDictionary;

   LONG floor = 0;
   LONG ceiling = Class->prvDictionary.size();
   while (floor < ceiling) {
      LONG i = (floor + ceiling)>>1;
      if (field[i].FieldID < FieldID) floor = i + 1;
      else if (field[i].FieldID > FieldID) ceiling = i;
      else {
         while ((i > 0) and (field[i-1].FieldID IS FieldID)) i--;
         *Result = Class;
         return &field[i];
      }
   }

   if (Class->Flags & CLF_PROMOTE_INTEGRAL) {
      for (LONG i=0; Class->Integral[i] != 0xff; i++) {
         auto &field = Class->prvDictionary[Class->Integral[i]];
         if (field.Arg) {
            if (auto child_class = (extMetaClass *)FindClass(field.Arg)) {
               *Result = child_class;
               if (auto child_field = lookup_id_byclass(child_class, FieldID, Result)) return child_field;
               *Result = NULL;
            }
         }
      }
   }
   return 0;
}
