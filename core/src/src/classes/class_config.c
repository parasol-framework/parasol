/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Config: Manages the reading and writing of configuration files.

The Config class is provided for reading text based key-values in a simple structured format.  Although basic and
lacking support for trees and types, they are reliable, easy to support and use minimal resources.

The following segment of a config file illustrates:

<pre>
[Action]
ClassID  = 5800
Location = modules:action

[Animation]
ClassID  = 1000
Location = modules:animation

[Arrow]
ClassID  = 3200
Location = modules:arrow
</pre>

Notice the text enclosed in square brackets, such as `[Action]`. These are referred to as 'sections', which are
responsible for holding groups of keys expressed as string values.  In the above example, keys are defined by the
ClassID and Path identifiers.

The following source code illustrates how to open the classes.cfg file and read a key from it:

<pre>
local cfg = obj.new('config', { path='config:classes.cfg' })
local err, str = cfg.mtReadValue('Action', 'Location')
print('The Action class is located at ' .. str)
</pre>

You can also search through config data using your own array iterator.  The following example illustrates:

<pre>
LONG i;
for (i=0; i < cfg->Entries; i++) {
   LogMsg("Section: %s, Key: %s, Data: %s", cfg->Entries[i].Section,
      cfg->Entries[i].Key, cfg->Entries[i].Data);
}
</pre>

-END-

*****************************************************************************/

#define STRBLOCKSIZE 2048
#define ENTBLOCKSIZE 100

#define PRV_CONFIG
#include "../defs.h"
#include <parasol/main.h>

static ERROR GET_Entries(objConfig *, struct ConfigEntry **);
static ERROR GET_KeyFilter(objConfig *, STRING *);
static ERROR GET_Path(objConfig *, STRING *);
static ERROR GET_SectionFilter(objConfig *, STRING *);
static ERROR GET_TotalSections(objConfig *, LONG *);

static ERROR CONFIG_SaveSettings(objConfig *, APTR);

static const struct FieldDef clFlags[] = {
   { "AutoSave",    CNF_AUTO_SAVE },
   { "StripQuotes", CNF_STRIP_QUOTES },
   { "LockRecords", CNF_LOCK_RECORDS },
   { "FileExists",  CNF_FILE_EXISTS },
   { "New",         CNF_NEW },
   { NULL, 0 }
};

//****************************************************************************

static WORD check_for_key(CSTRING);
static CSTRING next_section(CSTRING);
static CSTRING next_line(CSTRING);
static LONG check_filter(objConfig *, STRING, STRING, STRING);
static ERROR defragment(objConfig *);
static ERROR process_config_data(objConfig *, UBYTE *);
static LONG find_section(objConfig *, LONG);
static LONG find_section_name(objConfig *, CSTRING);
static LONG find_section_wild(objConfig *, CSTRING);
static STRING read_config(objConfig *, CSTRING, CSTRING);

static const struct FieldArray clFields[];
static const struct MethodArray clConfigMethods[];
static const struct ActionArray clConfigActions[];

static void resolve_addresses(objConfig *Self)
{
   LONG i;
   for (i=0; i < Self->AmtEntries; i++) {
      Self->Entries[i].Section = Self->Strings + Self->Entries[i].SectionOffset;
      Self->Entries[i].Key    = Self->Strings + Self->Entries[i].KeyOffset;
      Self->Entries[i].Data    = Self->Strings + Self->Entries[i].DataOffset;
   }
}

INLINE LONG scopy(CSTRING Src, STRING Dest)
{
   LONG i;
   for (i=0; Src[i]; i++) Dest[i] = Src[i];
   Dest[i++] = 0;
   return i;
}

//****************************************************************************

ERROR add_config_class(void)
{
   if (!NewPrivateObject(ID_METACLASS, 0, (OBJECTPTR *)&ConfigClass)) {
      if (!SetFields((OBJECTPTR)ConfigClass,
            FID_BaseClassID|TLONG,    ID_CONFIG,
            FID_ClassVersion|TFLOAT,  VER_CONFIG,
            FID_Name|TSTR,            "Config",
            FID_Category|TLONG,       CCF_DATA,
            FID_FileExtension|TSTR,   "*.cfg|*.cnf|*.config",
            FID_FileDescription|TSTR, "Config File",
            FID_Actions|TPTR,         clConfigActions,
            FID_Methods|TARRAY,       clConfigMethods,
            FID_Fields|TARRAY,        clFields,
            FID_Size|TLONG,           sizeof(objConfig),
            FID_Path|TSTR,            "modules:core",
            TAGEND)) {
         return acInit(&ConfigClass->Head);
      }
      else return ERR_SetField;
   }
   else return ERR_NewObject;
}

//****************************************************************************

static ERROR CONFIG_AccessObject(objConfig *Self, APTR Void)
{
   if (Self->EntriesMID) {
      if (AccessMemory(Self->EntriesMID, MEM_READ_WRITE, 2000, (void **)&Self->Entries) != ERR_Okay) {
         return PostError(ERR_AccessMemory);
      }
   }

   if (Self->StringsMID) {
      if (AccessMemory(Self->StringsMID, MEM_READ_WRITE, 2000, (void **)&Self->Strings) != ERR_Okay) {
         return PostError(ERR_AccessMemory);
      }
   }

   if (Self->KeyFilterMID) {
      if (AccessMemory(Self->KeyFilterMID, MEM_READ_WRITE, 2000, (void **)&Self->KeyFilter) != ERR_Okay) {
         return PostError(ERR_AccessMemory);
      }
   }

   if (Self->SectionFilterMID) {
      if (AccessMemory(Self->SectionFilterMID, MEM_READ_WRITE, 2000, (void **)&Self->SectionFilter) != ERR_Okay) {
         return PostError(ERR_AccessMemory);
      }
   }

   resolve_addresses(Self);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Clear: Clears all configuration data.
-END-
*****************************************************************************/

static ERROR CONFIG_Clear(objConfig *Self, APTR Void)
{
   if (Self->Entries)    { ReleaseMemoryID(Self->EntriesMID); Self->Entries = NULL; }
   if (Self->EntriesMID) { FreeMemoryID(Self->EntriesMID); Self->EntriesMID = NULL; }
   if (Self->Strings)    { ReleaseMemoryID(Self->StringsMID); Self->Strings = NULL; }
   if (Self->StringsMID) { FreeMemoryID(Self->StringsMID); Self->StringsMID = NULL; }
   if (Self->KeyFilter)    { ReleaseMemoryID(Self->KeyFilterMID); Self->KeyFilter = NULL; }
   if (Self->KeyFilterMID) { FreeMemoryID(Self->KeyFilterMID); Self->KeyFilterMID = NULL; }
   if (Self->SectionFilter)    { ReleaseMemoryID(Self->SectionFilterMID); Self->SectionFilter = NULL; }
   if (Self->SectionFilterMID) { FreeMemoryID(Self->SectionFilterMID); Self->SectionFilterMID = NULL; }

   Self->AmtEntries    = 0;
   Self->StringsSize   = 0;
   Self->StringsPos    = 0;
   Self->MaxEntries    = 0;
   Self->TotalSections = 0;

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
DeleteIndex: Deletes single configuration entries.

This method deletes a single key from the config object.  The index number of the key that you want to delete is
required.  If you don't know the index number, scan the #Entries array.

-INPUT-
int Index: The number of the entry that you want to delete.

-ERRORS-
Okay
Args
NullArgs
GetField: The Entries field could not be retrieved.
-END-

*****************************************************************************/

static ERROR CONFIG_DeleteIndex(objConfig *Self, struct cfgDeleteIndex *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   if ((Args->Index < 0) OR (Args->Index >= Self->AmtEntries)) {
      LogErrorMsg("Index %d is out of bounds.", Args->Index);
      return ERR_Args;
   }

   LogMsg("Index: %d", Args->Index);

   BYTE lastsection = TRUE;
   if ((Args->Index > 0) AND (Self->Entries[Args->Index].Section IS Self->Entries[Args->Index-1].Section)) lastsection = FALSE;
   if ((Args->Index < Self->AmtEntries) AND (Self->Entries[Args->Index].Section IS Self->Entries[Args->Index+1].Section)) lastsection = FALSE;

   LONG i;
   for (i=Args->Index; i < Self->AmtEntries-1; i++) {
      CopyMemory(Self->Entries+i+1, Self->Entries+i, sizeof(struct ConfigEntry));
   }

   Self->AmtEntries--;
   if (lastsection IS TRUE) Self->TotalSections--;
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
DeleteSection: Deletes entire sections of configuration data.

This method is used to delete entire sections of information from config objects.  Simply specify the name of the
section that you want to delete and it will be removed.

-INPUT-
cstr Section: The name of the section that you want to delete.

-ERRORS-
Okay: The section was deleted.
Args
GetField: The Entries field could not be retrieved.
-END-

*****************************************************************************/

static ERROR CONFIG_DeleteSection(objConfig *Self, struct cfgDeleteSection *Args)
{
   if ((!Args) OR (!Args->Section)) return PostError(ERR_NullArgs);

   UBYTE found = FALSE;
   LONG i;
   for (i=Self->AmtEntries-1; i >= 0; i--) {
      if (!StrMatch(Args->Section, Self->Entries[i].Section)) {
         found = TRUE;

         // Entries are deleted by manipulating the entries array

         if (i < Self->AmtEntries-1) {
            CopyMemory(Self->Entries+i+1, Self->Entries+i, sizeof(struct ConfigEntry) * (Self->AmtEntries - i - 1));
         }

         Self->AmtEntries--;
      }
   }

   if (found) {
      Self->TotalSections--;

      return defragment(Self);
   }
   else return ERR_Okay;
}

//****************************************************************************
// To defragment a config object, we simply recreate the entries array and the strings buffer from scratch and copy
// across the old entries.

static ERROR defragment(objConfig *Self)
{
   struct ConfigEntry *newentries;
   MEMORYID newid, newstrid;
   STRING newstr, current_section;
   LONG i, current_sectionpos;

   if (!Self->AmtEntries) {
      FMSG("defragment()","Emptying config object.");

      if (Self->Entries) {
         ReleaseMemoryID(Self->EntriesMID);
         FreeMemoryID(Self->EntriesMID);
         Self->Entries = NULL;
         Self->EntriesMID = 0;
      }

      if (Self->Strings) {
         ReleaseMemoryID(Self->StringsMID);
         FreeMemoryID(Self->StringsMID);
         Self->Strings = NULL;
         Self->StringsMID = 0;
      }

      Self->StringsPos = 0;
      Self->MaxEntries = 0;
      Self->TotalSections = 0;
      return ERR_Okay;
   }
   else FMSG("defragment()","Reducing size from %d entries, %d sections, %d string bytes.", Self->AmtEntries, Self->TotalSections, Self->StringsSize);

   if (!AllocMemory(Self->AmtEntries * sizeof(struct ConfigEntry), Self->Head.MemFlags|MEM_NO_CLEAR, (void **)&newentries, &newid)) {
      // Calculate the size of the string buffer

      LONG strsize = 0;
      STRING lastsection = NULL;
      for (i=0; i < Self->AmtEntries; i++) {
         if (lastsection != Self->Entries[i].Section) {
            lastsection = Self->Entries[i].Section;
            strsize += StrLength(Self->Entries[i].Section) + 1;
         }
         strsize += StrLength(Self->Entries[i].Key) + 1;
         strsize += StrLength(Self->Entries[i].Data) + 1;
      }

      if (strsize < STRBLOCKSIZE) strsize = STRBLOCKSIZE;

      if (!AllocMemory(strsize, Self->Head.MemFlags|MEM_NO_CLEAR, (void **)&newstr, &newstrid)) {
         // Copy the entries array

         CopyMemory(Self->Entries, newentries, sizeof(struct ConfigEntry) * Self->AmtEntries);

         // Copy the strings

         LONG pos = 0;
         lastsection = NULL;
         current_section = NULL;
         current_sectionpos = 0;
         for (i=0; i < Self->AmtEntries; i++) {
            if (lastsection != Self->Entries[i].Section) {
               lastsection = Self->Entries[i].Section;
               current_section = newstr + pos;
               current_sectionpos = pos;
               pos += StrCopy(Self->Entries[i].Section, newstr + pos, COPY_ALL) + 1;
            }
            newentries[i].Section = current_section;
            newentries[i].SectionOffset = current_sectionpos;

            newentries[i].Key = newstr + pos;
            newentries[i].KeyOffset = pos;
            pos += StrCopy(Self->Entries[i].Key, newstr + pos, COPY_ALL) + 1;

            newentries[i].Data = newstr + pos;
            newentries[i].DataOffset = pos;
            pos += StrCopy(Self->Entries[i].Data, newstr + pos, COPY_ALL) + 1;
         }

         // Replace old allocations with the new ones

         if (Self->Entries) { ReleaseMemoryID(Self->EntriesMID); FreeMemoryID(Self->EntriesMID); }
         if (Self->Strings) { ReleaseMemoryID(Self->StringsMID); FreeMemoryID(Self->StringsMID); }

         Self->Entries    = newentries;
         Self->EntriesMID = newid;
         Self->Strings    = newstr;
         Self->StringsMID = newstrid;

         Self->StringsPos  = pos;
         Self->StringsSize = strsize;
         Self->MaxEntries  = Self->AmtEntries;

         // String addresses in the new entries array have to match the offsets in the new strings buffer

         resolve_addresses(Self);

         FMSG("defragment","There are now %d sections and %d entries.  Strings Buffer: %d bytes", Self->TotalSections, Self->AmtEntries, Self->StringsSize);
         return ERR_Okay;
      }
      else return ERR_AllocMemory;
   }
   else return ERR_AllocMemory;
}

/*****************************************************************************
-ACTION-
Flush: Diverts to #SaveSettings().
-END-
*****************************************************************************/

static ERROR CONFIG_Flush(objConfig *Self, APTR Void)
{
   return CONFIG_SaveSettings(Self, NULL);
}

//****************************************************************************

static ERROR CONFIG_Free(objConfig *Self, APTR Void)
{
   if (Self->Flags & CNF_AUTO_SAVE) {
      if (!GET_Path(Self, &Self->Path)) {
         ULONG crc = GenCRC32(0, (BYTE *)Self->Entries, Self->AmtEntries * sizeof(struct ConfigEntry));
         crc = GenCRC32(crc, Self->Strings, Self->StringsPos);

         if ((!crc) OR (crc != (ULONG)Self->CRC)) {
            LogMsg("Auto-saving changes to \"%s\" (CRC: %d : %d)", Self->Path, Self->CRC, crc);

            OBJECTPTR file;
            if (!CreateObject(ID_FILE, 0, &file,
                  FID_Path|TSTR,         Self->Path,
                  FID_Flags|TLONG,       FL_WRITE|FL_NEW,
                  FID_Permissions|TLONG, NULL,
                  TAGEND)) {
               ActionTags(AC_SaveToObject, (OBJECTPTR)Self, file->UniqueID, 0);
               acFree(file);
            }
         }
         else LogMsg("Not auto-saving data (CRC unchanged).");
      }
   }

   if (Self->Entries)    { ReleaseMemoryID(Self->EntriesMID); Self->Entries = NULL; }
   if (Self->EntriesMID) { FreeMemoryID(Self->EntriesMID); Self->EntriesMID = 0; }
   if (Self->Strings)    { ReleaseMemoryID(Self->StringsMID); Self->Strings = NULL; }
   if (Self->StringsMID) { FreeMemoryID(Self->StringsMID); Self->StringsMID = 0; }
   if (Self->Path)    { ReleaseMemoryID(Self->PathMID); Self->Path = NULL; }
   if (Self->PathMID) { FreeMemoryID(Self->PathMID); Self->PathMID = 0; }
   if (Self->KeyFilter)    { ReleaseMemoryID(Self->KeyFilterMID); Self->KeyFilter = NULL; }
   if (Self->KeyFilterMID) { FreeMemoryID(Self->KeyFilterMID); Self->KeyFilterMID = 0; }
   if (Self->SectionFilter)    { ReleaseMemoryID(Self->SectionFilterMID); Self->SectionFilter = NULL; }
   if (Self->SectionFilterMID) { FreeMemoryID(Self->SectionFilterMID); Self->SectionFilterMID = 0; }
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
GetSectionFromIndex: Converts an index number into its matching section string.

Use GetSectionFromIndex to convert a section index number to its matching name.

-INPUT-
int Index: The section index that you want to identify.
&cstr Section: Points to the section string that matches the index number.

-ERRORS-
Okay
Args
OutOfRange: The index number is out of range of the available sections.
NoData: There is no data loaded into the config object.

*****************************************************************************/

static ERROR CONFIG_GetSectionFromIndex(objConfig *Self, struct cfgGetSectionFromIndex *Args)
{
   if ((!Args) OR (Args->Index < 0)) return PostError(ERR_Args);

   LONG index = Args->Index;
   struct ConfigEntry *entries;
   if (!GET_Entries(Self, &entries)) {
      LONG pos = 0; // Position starts from the requested index at the very least
      while ((index > 0) AND (pos < Self->AmtEntries-1)) {
         if (entries[pos].Section != entries[pos+1].Section) index--;
         pos++;
      }

      if (!index) {
         Args->Section = entries[pos].Section;
         return ERR_Okay;
      }
      else return PostError(ERR_OutOfRange);
   }
   else return PostError(ERR_NoData);
}

/*****************************************************************************

-ACTION-
GetVar: Retrieves data from a config object.

Variable fields are used to retrieve data from specific keys in config objects.  Retrieval can be achieved in a
variety of ways. The simplest method is to refer to the key name when retrieving data, for instance `mykey`.
To pull a key from a specific section, use the format `mykey(section)`, where the section is a number
referring to a section index that you want to retrieve.

You can reference any key in the config object using indexes, if you wish to treat the object as a one-dimensional
array.  To do this, use the `index(section,key)` string format, where key refers to the index of the key that
you wish to lookup.  To refer to a key by name, use quotes to indicate that the section and/or key is a named
reference.

To retrieve the name of a key rather than its data, use the string format `key(value)`.
-END-

*****************************************************************************/

static ERROR CONFIG_GetVar(objConfig *Self, struct acGetVar *Args)
{
   if (!Args) return ERR_NullArgs;
   if ((!Args->Field) OR (!Args->Buffer) OR (Args->Size < 1)) return ERR_Args;

   STRING buffer = Args->Buffer;
   buffer[0] = 0;

   CSTRING fieldname = Args->Field;
   BYTE getkey = FALSE;

   LONG section_index, i, j, pos, k, index;
   UBYTE section[160], key[160];

   struct ConfigEntry *entries;
   struct cfgGetSectionFromIndex getsection;
   if (GET_Entries(Self, &entries) != ERR_Okay) return PostError(ERR_NoData);

   if (!StrCompare("section(", fieldname, 8, NULL)) {
      // Field is in the format: Section(SectionIndex) OR Section(#AbsIndex)
      // The value that is returned is the name of the section at the specified index.
      // The index is not absolute.

      if (fieldname[8] IS '#') {
         // Absolute index
         section_index = StrToInt(fieldname+9);
         if ((section_index >= 0) AND (section_index < Self->AmtEntries)) {
            StrCopy(Self->Entries[section_index].Section, buffer, Args->Size);
            return ERR_Okay;
         }
         else return ERR_OutOfRange;
      }
      else {
         getsection.Index = StrToInt(fieldname+8);
         if (!CONFIG_GetSectionFromIndex(Self, &getsection)) {
            StrCopy(getsection.Section, buffer, Args->Size);
            return ERR_Okay;
         }
         else return ERR_OutOfRange;
      }
   }
   else if ((!StrCompare("index(", fieldname, 6, NULL)) OR
            (!StrCompare("key(", fieldname, 5, NULL))) {
      // Field is one of these formats:
      //
      //    Index(["SectionName"|'SectionName'|SectionIndex],["Key"|KeyIndex])
      //    Index(AbsoluteIndex)
      //
      // You can use any combination of string or numeric references.  Quotes should be used to
      // explicitly indicate strings instead of indexes.

      if (!StrCompare("key(", fieldname, 5, NULL)) {
         i = 5;
         getkey = TRUE;
      }
      else i = 6;

      // Extract the section index

      if (fieldname[i] IS '"') {
         i++;
         for (j=0; (fieldname[i]) AND (fieldname[i] != '"') AND (j < sizeof(section)-1); j++) section[j] = fieldname[i++];
         section[j] = 0;
         if (fieldname[i] IS '"') i++;
         index = find_section_wild(Self, section); // Convert the section string to an absolute index
      }
      else if (fieldname[i] IS '\'') {
         i++;
         for (j=0; (fieldname[i]) AND (fieldname[i] != '\'') AND (j < sizeof(section)-1); j++) section[j] = fieldname[i++];
         section[j] = 0;
         if (fieldname[i] IS '\'') i++;
         index = find_section_wild(Self, section); // Convert the section string to an absolute index
      }
      else {
         for (j=0; (fieldname[i]) AND (fieldname[i] != ')') AND (fieldname[i] != ',') AND (j < sizeof(section)-1); j++) section[j] = fieldname[i++];
         section[j] = 0;

         if (StrDatatype(section) IS STT_NUMBER) {
            // This is a section index (if a key is specified following) or an absolute index (no key specified following).

            section_index = StrToInt(section);

            while ((fieldname[i]) AND (fieldname[i] <= 0x20)) i++;
            if (fieldname[i] IS ',') {
               if ((index = find_section(Self, section_index)) IS -1) {
                  LogErrorMsg("Invalid section index %d (from \"%s\")", section_index, section);
                  return ERR_OutOfRange;
               }
            }
            else index = section_index; // Index is absolute if no key reference follows the section number
         }
         else index = find_section_name(Self, section);
      }

      if (index IS -1) {
         LogMsg("Failed to find section '%s' (ref: %s)", section, Args->Field);
         return ERR_Search;
      }

      while ((fieldname[i]) AND (fieldname[i] <= 0x20)) i++;
      if (fieldname[i] IS ',') {
         i++;
         while ((fieldname[i]) AND (fieldname[i] <= 0x20)) i++;

         // Extract the key index (if there is one) and add it to the absolute index

         if ((fieldname[i] IS '"') OR (fieldname[i] IS '\'')) {
            if (fieldname[i] IS '"') {
               i++;
               for (j=0; (fieldname[i]) AND (fieldname[i] != '"') AND (j < sizeof(key)-1); j++) key[j] = fieldname[i++];
            }
            else {
               i++;
               for (j=0; (fieldname[i]) AND (fieldname[i] != '\'') AND (j < sizeof(key)-1); j++) key[j] = fieldname[i++];
            }
            key[j] = 0;

            while (index < Self->AmtEntries) {
               if (!StrMatch(key, entries[index].Key)) break;

               if ((index < Self->AmtEntries-1) AND (entries[index+1].Section != entries[index].Section)) {
                  // We have reached the end of the section without finding the key
                  return ERR_Search;
               }
               index++;
            }
         }
         else {
            for (j=0; (fieldname[i]) AND (fieldname[i] != ')') AND (j < sizeof(key)-1); j++) key[j] = fieldname[i++];
            key[j] = 0;

            if (StrDatatype(key) IS STT_NUMBER) index += StrToInt(key);
            else while (index < Self->AmtEntries) {
               if (!StrMatch(key, entries[index].Key)) break;
               if ((index < Self->AmtEntries-1) AND (entries[index+1].Section != entries[index].Section)) {
                  // We have reached the end of the section without finding the key
                  return ERR_Search;
               }
               index++;
            }
         }
      }

      // We now have an overall index that we can use

      if ((index >= Self->AmtEntries) OR (index < 0)) return PostError(ERR_OutOfRange);

      if (getkey IS TRUE) StrCopy(entries[index].Key, buffer, Args->Size);
      else StrCopy(entries[index].Data, buffer, Args->Size);
      return ERR_Okay;
   }

   // Extract the key and the section number from the field name

   for (i=0; (fieldname[i]) AND (fieldname[i] != '('); i++) key[i] = fieldname[i];
   key[i] = 0;

   if (fieldname[i] IS '(') {
      i++;

      if ((fieldname[i] >= '0') AND (fieldname[i] <= '9')) {
         section_index = 0;
         for (k=i; fieldname[k]; k++) {
            if ((fieldname[k] >= '0') AND (fieldname[k] <= '9')) {
               section_index *= 10;
               section_index += (fieldname[k] - '0');
            }
            else break;
         }

         // Convert the section number into an absolute index

         pos = 0;
         while ((section_index > 0) AND (pos < Self->AmtEntries-1)) {
            if (entries[pos].Section != entries[pos+1].Section) section_index--;
            pos++;
         }
      }
      else {
         if (fieldname[i] IS '"') {
            i++;
            for (j=0; (fieldname[i]) AND (fieldname[i] != '"') AND (j < sizeof(section)-1); j++) section[j] = fieldname[i++];
            section[j] = 0;
         }
         else if (fieldname[i] IS '\'') {
            i++;
            for (j=0; (fieldname[i]) AND (fieldname[i] != '\'') AND (j < sizeof(section)-1); j++) section[j] = fieldname[i++];
            section[j] = 0;
         }
         else {
            for (j=0; (fieldname[i]) AND (fieldname[i] != ')') AND (j < sizeof(section)-1); j++) section[j] = fieldname[i++];
            section[j] = 0;
         }

         for (pos=0; pos < Self->AmtEntries; pos++) {
            if (!StrMatch(section, entries[pos].Section)) break;
         }
      }
   }
   else pos = 0; // Assume the key is in section 0

   // Search the entries for the data that we are looking for

   section_index = pos;
   while ((pos < Self->AmtEntries) AND (entries[pos].Section IS entries[section_index].Section)) {
      if (!StrMatch(entries[pos].Key, key)) {
         for (i=0; (entries[pos].Data[i]) AND (i < Args->Size - 1); i++) {
            buffer[i] = entries[pos].Data[i];
         }
         buffer[i] = 0;
         return ERR_Okay;
      }
      pos++;
   }

   return ERR_Search;
}

//****************************************************************************

static ERROR CONFIG_Init(objConfig *Self, APTR Void)
{
   if (Self->Flags & CNF_NEW) return ERR_Okay; // Do not load any data - location refers to a new file yet to be created

   struct rkFile *file = NULL;
   STRING data = NULL;
   ERROR error = ERR_Failed;

   // Open a file with read only and exclusive flags, then read all of the data into a buffer.  Terminate the buffer,
   // then free the file.
   //
   // Note that multiple files can be specified by separating each file path with a semi-colon.  This allows you to merge
   // many configuration files into one object.

   STRING location;
   if (!GET_Path(Self, &location)) {
      LONG datasize = 0;
      while (*location) {
         if (!(error = CreateObject(ID_FILE, 0, (OBJECTPTR *)&file,
               FID_Path|TSTR,   location,
               FID_Flags|TLONG, FL_READ|FL_APPROXIMATE,
               TAGEND))) {

            LONG filesize;
            GetLong((OBJECTPTR)file, FID_Size, &filesize);

            if (filesize > 0) {
               if (data) {
                  if (ReallocMemory(data, datasize + filesize + 3, (APTR *)&data, NULL) != ERR_Okay) goto exit;
               }
               else if (AllocMemory(filesize + 3, MEM_DATA|MEM_NO_CLEAR, (APTR *)&data, NULL) != ERR_Okay) goto exit;

               acRead(file, data + datasize, filesize, NULL); // Read the entire file

               datasize += filesize;
               data[datasize++] = '\n';
            }

            acFree(&file->Head);
            file = NULL;
         }
         else if (Self->Flags & CNF_FILE_EXISTS) return ERR_FileNotFound;

         while ((*location) AND (*location != ';') AND (*location != '|')) location++;
         if (*location) location++; // Skip separator
      }

      // Process the configuration data.  Note that if no files were loaded, we do not fail.  We just make it look like
      // the files existed as empty files (some DML programs like System Options rely on this feature).

      if (data) {
         data[datasize] = 0; // Terminate the buffer
         if ((error = process_config_data(Self, data)) != ERR_Okay) goto exit;
      }
   }
   else if (Self->Flags & CNF_FILE_EXISTS) return ERR_FileNotFound;
   else return ERR_Okay; // Return OK if there is no source location

   // Key filtering

   BYTE section[40];
   LONG j;

   if ((Self->KeyFilterMID) AND (Self->Entries)) {
      if (!Self->KeyFilter) {
         AccessMemory(Self->KeyFilterMID, MEM_READ, 2000, (void *)&Self->KeyFilter);
      }

      if (Self->KeyFilter) {
         BYTE current_section[sizeof(section)];

         for (j=0; (Self->Entries[0].Section[j]) AND (j < sizeof(current_section)-1); j++) current_section[j] = Self->Entries[0].Section[j];
         current_section[j] = 0;

         LONG i;
         WORD last_index = 0;
         for (i=0; i < Self->AmtEntries; i++) {
            // If we run through a section without encountering the given key, we must delete the entire section.

            if (StrMatch(Self->Entries[i].Section, current_section) != ERR_Okay) {
               struct cfgDeleteSection delete = { current_section };
               CONFIG_DeleteSection(Self, &delete);
               i = last_index-1;

               if (last_index < Self->AmtEntries) {
                  for (j=0; (Self->Entries[last_index].Section[j]) AND (j < sizeof(current_section)-1); j++) {
                     current_section[j] = Self->Entries[last_index].Section[j];
                  }
                  current_section[j] = 0;
               }
            }
            else {
               WORD status = check_filter(Self, Self->KeyFilter, Self->Entries[i].Key, Self->Entries[i].Data);
               if (status IS 1) {
                  while ((i+1 < Self->AmtEntries) AND (!StrMatch(Self->Entries[i+1].Section, current_section))) i++;
                  last_index = i+1;

                  if (i+1 < Self->AmtEntries) {
                     for (j=0; (Self->Entries[i+1].Section[j]) AND (j < sizeof(current_section)-1); j++) {
                        current_section[j] = Self->Entries[i+1].Section[j];
                     }
                     current_section[j] = 0;
                  }
               }
               else if ((status IS 0) OR (i IS Self->AmtEntries-1)) {
                  for (j=0; (Self->Entries[i].Section[j]) AND (j < sizeof(section)-1); j++) section[j] = Self->Entries[i].Section[j];
                  section[j] = 0;
                  struct cfgDeleteSection delete = { section };
                  CONFIG_DeleteSection(Self, &delete);
                  i = last_index-1;

                  // Update the current section
                  if (last_index < Self->AmtEntries) {
                     for (j=0; (Self->Entries[last_index].Section[j]) AND (j < sizeof(current_section)-1); j++) {
                        current_section[j] = Self->Entries[last_index].Section[j];
                     }
                     current_section[j] = 0;
                  }
               }
            }
         }
         LogMsg("Filtered keys with \"%s\", reduced entries to %d.", Self->KeyFilter, Self->AmtEntries);
      }
   }

   // Section filtering

   if (Self->SectionFilterMID) {
      if (!Self->SectionFilter) {
         AccessMemory(Self->SectionFilterMID, MEM_READ, 2000, (void *)&Self->SectionFilter);
      }

      if (Self->SectionFilter) {
         LONG i;
         for (i=Self->AmtEntries-1; i >= 0; i--) {
            if (check_filter(Self, Self->SectionFilter, Self->Entries[i].Section, NULL)) {
               for (j=0; (Self->Entries[i].Section[j]) AND (j < sizeof(section)-1); j++) {
                  section[j] = Self->Entries[i].Section[j];
               }
               section[j] = 0;
               struct cfgDeleteSection delete = { section };
               CONFIG_DeleteSection(Self, &delete);
               if (i > Self->AmtEntries) i = Self->AmtEntries;
            }
         }

         LogMsg("Filtered sections with \"%s\", reduced entries to %d.", Self->SectionFilter, Self->AmtEntries);
      }
   }

   error = ERR_Okay;

   if (Self->Flags & CNF_AUTO_SAVE) {
      // Calculate a checksum for all the data
      Self->CRC = GenCRC32(0, (BYTE *)Self->Entries, Self->AmtEntries * sizeof(struct ConfigEntry));
      Self->CRC = GenCRC32(Self->CRC, Self->Strings, Self->StringsPos);
   }

exit:
   if (file) acFree(&file->Head);
   if (data) FreeMemory(data);
   return error;
}

/*****************************************************************************

-METHOD-
Merge: Merges two config objects together.

The Merge method is used to merge configuration data from one config object into the object that this method is called
on.  You need to provide the unique object id of the config object providing the source data.  When performing the
merge, existing data will be overwritten by the source in cases where there is a duplication of section and key lines.

-INPUT-
oid ConfigID: The ID of the config object to be merged.

-ERRORS-
Okay
NullArgs
AccessObject: The source configuration object could not be accessed.
-END-

*****************************************************************************/

static ERROR CONFIG_Merge(objConfig *Self, struct cfgMerge *Args)
{
   if ((!Args) OR (!Args->ConfigID)) return ERR_NullArgs;

   objConfig *src;
   if (!AccessObject(Args->ConfigID, 5000, (OBJECTPTR *)&src)) {
      LONG i;
      for (i=0; i < src->AmtEntries; i++) {
         cfgWriteValue(Self, src->Entries[i].Section, src->Entries[i].Key, src->Entries[i].Data);
      }

      acFree(&src->Head);

      ReleaseObject((OBJECTPTR)src);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-METHOD-
MergeFile: Merges a foreign configuration file into existing configuration data.

The MergeFile method is used to pull configuration data from a file and merge it into the existing config object.
You need to provide the location of the configuration file only.  When performing the merge, existing data will be
overwritten by the source file in cases where there is a duplication of section and key lines.

-INPUT-
cstr Path: The location of the configuration file that you want to merge.

-ERRORS-
Okay
NullArgs
File: Failed to load the source file.
-END-

*****************************************************************************/

static ERROR CONFIG_MergeFile(objConfig *Self, struct cfgMergeFile *Args)
{
   if ((!Args) OR (!Args->Path)) return PostError(ERR_NullArgs);

   LogBranch(Args->Path);

   objConfig *src;
   if (!CreateObject(ID_CONFIG, 0, (OBJECTPTR *)&src,
         FID_Path|TSTR, Args->Path,
         TAGEND)) {
      LONG i;
      for (i=0; i < src->AmtEntries; i++) {
         cfgWriteValue(Self, src->Entries[i].Section, src->Entries[i].Key, src->Entries[i].Data);
      }
      acFree(&src->Head);
      LogBack();
      return ERR_Okay;
   }
   else {
      LogBack();
      return ERR_File;
   }
}

/*****************************************************************************

-METHOD-
ReadValue: Reads one selected string from a configuration file.

This function retrieves key values in their original string format.  On success, the resulting string remains valid
only for as long as the client has exclusive access to the config object.  The pointer can also be invalidated
if more information is written to the config object.  For this reason, consider copying the Data string if it will be
used extensively.

If the Section parameter is set to NULL, the scan routine will treat all of the config data as a one dimensional array.
If the Key parameter is set to NULL then the first key in the requested section is returned.  If both parameters
are NULL then the first known key value will be returned.

-INPUT-
cstr Section: The name of a section to examine for a key.  If NULL, all sections are scanned.
cstr Key: The name of a key to retrieve.
&cstr Data: The key value will be stored in this parameter on returning.

-ERRORS-
Okay
NullArgs
Search: The requested configuration entry does not exist.
-END-

*****************************************************************************/

static ERROR CONFIG_ReadValue(objConfig *Self, struct cfgReadValue *Args)
{
   if (!Args) return PostError(ERR_NullArgs);
   if (!Self->Entries) return ERR_Search;

   Args->Data = NULL;

   if (!Args->Section) {
      LONG i;
      for (i=0; i < Self->AmtEntries; i++) {
         if ((!Args->Key) OR (!StrMatch(Args->Key, Self->Entries[i].Key))) {
            Args->Data = Self->Entries[i].Data;
            return ERR_Okay;
         }
      }
   }
   else {
      LONG i;
      for (i=0; i < Self->AmtEntries; i++) {
         if (!StrMatch(Args->Section, Self->Entries[i].Section)) {
            if ((!Args->Key) OR (!StrMatch(Args->Key, Self->Entries[i].Key))) {
               Args->Data = Self->Entries[i].Data;
               return ERR_Okay;
            }
         }
         else {
            // Skip this entire section
            while ((i < Self->AmtEntries-1) AND (Self->Entries[i+1].Section IS Self->Entries[i].Section)) i++;
         }
      }
   }

   MSG("Could not find key %s : %s.", Args->Section, Args->Key);
   return ERR_Search;
}

/*****************************************************************************

-METHOD-
ReadFloat: Reads keys in floating-point format.

This method is identical to #ReadValue() but for converting the value to an integer before returning.  If the requested
key cannot be found, a fail code will be returned and the Float result set to zero.  If the key is not convertible to
a float then a success code will still be returned and the result will be zero.

-INPUT-
cstr Section: The name of the section that the integer will be retrieved from.
cstr Key: The specific key that contains the integer you want to retrieve.
&double Float: This result parameter will be set to the floating point value read from the configuration data.

-ERRORS-
Okay
NullArgs
Failed: The requested configuration entry could not be read.
-END-

*****************************************************************************/

static ERROR CONFIG_ReadFloat(objConfig *Self, struct cfgReadFloat *Args)
{
   if (!Args) {
      Args->Float = 0;
      return ERR_NullArgs;
   }
   else {
      struct cfgReadValue read = { .Section = Args->Section, .Key = Args->Key };
      if (!Action(MT_CfgReadValue, &Self->Head, &read)) {
         Args->Float = StrToFloat(read.Data);
         return ERR_Okay;
      }
      else { Args->Float = 0; return ERR_Failed; }
   }
}

/*****************************************************************************

-METHOD-
ReadInt: Reads keys in integer format.

This method is identical to #ReadValue() but for converting the value to an integer before returning.  If the requested
key cannot be found, a fail code will be returned and the Integer result set to zero.  If the key is not convertible to
an integer then a success code will still be returned and the result will be zero.

-INPUT-
cstr Section: The name of the section that the integer will be retrieved from.
cstr Key: The specific key that contains the integer you want to retrieve.
&int Integer: This result argument will be set to the integer value read from the configuration data.

-ERRORS-
Okay
NullArgs
Failed: The requested configuration entry could not be read.
-END-

*****************************************************************************/

static ERROR CONFIG_ReadInt(objConfig *Self, struct cfgReadInt *Args)
{
   if (!Args) {
      Args->Integer = 0;
      return ERR_NullArgs;
   }
   else {
      struct cfgReadValue read = { Args->Section, Args->Key };
      if (!Action(MT_CfgReadValue, &Self->Head, &read)) {
         Args->Integer = StrToInt(read.Data);
         return ERR_Okay;
      }
      else { Args->Integer = 0; return ERR_Failed; }
   }
}

//****************************************************************************

static ERROR CONFIG_ReleaseObject(objConfig *Self, APTR Void)
{
   if (Self->Entries)       { ReleaseMemoryID(Self->EntriesMID);       Self->Entries = NULL; }
   if (Self->Strings)       { ReleaseMemoryID(Self->StringsMID);       Self->Strings = NULL; }
   if (Self->Path)          { ReleaseMemoryID(Self->PathMID);          Self->Path = NULL; }
   if (Self->KeyFilter)     { ReleaseMemoryID(Self->KeyFilterMID);     Self->KeyFilter = NULL; }
   if (Self->SectionFilter) { ReleaseMemoryID(Self->SectionFilterMID); Self->SectionFilter = NULL; }
   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
SaveSettings: Saves data to the file that the configuration data was loaded from.

This action will save the configuration data back to its original file source (assuming the #Path remains unchanged).

*****************************************************************************/

static ERROR CONFIG_SaveSettings(objConfig *Self, APTR Void)
{
   ULONG crc = 0;
   if (Self->Flags & CNF_AUTO_SAVE) {
      // Perform a CRC check
      crc = GenCRC32(0, (BYTE *)Self->Entries, Self->AmtEntries * sizeof(struct ConfigEntry));
      crc = GenCRC32(crc, Self->Strings, Self->StringsPos);
      if (crc IS Self->CRC) return ERR_Okay;
   }

   if (!GET_Path(Self, &Self->Path)) {
      OBJECTPTR file;
      if (!CreateObject(ID_FILE, 0, &file,
            FID_Path|TSTR,         Self->Path,
            FID_Flags|TLONG,       FL_WRITE|FL_NEW,
            FID_Permissions|TLONG, 0,
            TAGEND)) {
         if (!ActionTags(AC_SaveToObject, (OBJECTPTR)Self, file->UniqueID, 0)) {
            Self->CRC = crc;
         }
         acFree(file);
         return ERR_Okay;
      }
      else return ERR_File;
   }
   else return ERR_MissingPath;
}

/*****************************************************************************
-ACTION-
SaveToObject: Saves configuration data to an object, using standard config text format.
-END-
*****************************************************************************/

static ERROR CONFIG_SaveToObject(objConfig *Self, struct acSaveToObject *Args)
{
   LogMsg("Saving %d keys to object #%d.", Self->AmtEntries, Args->DestID);

   OBJECTPTR object;
   if (!AccessObject(Args->DestID, 5000, &object)) {
      struct ConfigEntry *entries;
      if (!GET_Entries(Self, &entries)) {
         STRING section = NULL;
         LONG i, j, k;
         for (i=0; i < Self->AmtEntries; i++) {
            if ((!section) OR (StrMatch(section, entries[i].Section) != ERR_Okay)) {
               section = entries[i].Section;

               BYTE buffer[60];
               j = 0;
               buffer[j++] = '\n';
               buffer[j++] = '[';
               for (k=0; (section[k]) AND (k < sizeof(buffer)-j-2); k++) {
                  buffer[j++] = section[k];
               }
               buffer[j++] = ']';
               buffer[j++] = '\n';

               acWrite(object, buffer, j, NULL);
            }

            if ((entries[i].Key) AND (entries[i].Data)) {
               LONG keylen, datalen;
               for (keylen=0; entries[i].Key[keylen]; keylen++);
               for (datalen=0; entries[i].Data[datalen]; datalen++);

               {
                  UBYTE buffer[keylen+datalen+4];

                  k = 0;
                  for (j=0; j < keylen; j++) buffer[k++] = entries[i].Key[j];
                  buffer[k++] = ' ';
                  buffer[k++] = '=';
                  buffer[k++] = ' ';
                  for (j=0; j < datalen; j++) buffer[k++] = entries[i].Data[j];
                  buffer[k++] = '\n';

                  acWrite(object, buffer, k, NULL);
               }
            }
         }
      }

      ReleaseObject(object);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-METHOD-
Set: Sets keys in existing config sections (aborts if the section does not exist).

This method is identical to #WriteValue() except it will abort if the name of the referred section does not exist in the
config object.  The error code ERR_Search is returned if this is the case.  Please refer to #WriteValue() for further
information on the behaviour of this function.

-INPUT-
cstr Section: The name of the section.
cstr Key:  The name of the key.
cstr Data: The data that will be added to the given section/key.

-ERRORS-
Okay
NullArgs
Search: The referred section does not exist.
AllocMemory
GetField: The Entries field could not be retrieved.
-END-

*****************************************************************************/

static ERROR CONFIG_Set(objConfig *Self, struct cfgSet *Args)
{
   if (!Args) return ERR_NullArgs;
   if ((!Args->Section) OR (!Args->Section[0])) return ERR_NullArgs;
   if ((!Args->Key) OR (!Args->Key[0])) return ERR_NullArgs;

   if (find_section_wild(Self, Args->Section) != -1) {
      return Action(MT_CfgWriteValue, &Self->Head, Args);
   }
   else return ERR_Search;
}

/*****************************************************************************

-ACTION-
SetVar: Allows new entries to be added to a config object with variable field names.

The variable field mechanism can be used to add new entries to a config object.  The field data will be added under the
`Variables` section with a key name identified by the Field parameter and a data value determined by the Value
parameter.  To determine the section under which the key was added (something other than `Variables`),
combine the section and key names into the Field parameter by using a semicolon.  For example,
`Internet;IPAddress`.

Update existing keys using indexing, which follows the format `Index(SectionIndex;KeyIndex)` or
`Index(ArrayIndex)`.  Named lookups can be made by enclosing a section or key name in quotes.

It is recommended that where possible, the #Write() method is used for updating or writing new keys to a config object.
-END-

*****************************************************************************/

static ERROR CONFIG_SetVar(objConfig *Self, struct acSetVar *Args)
{
   LONG i, len, index, j;
   UBYTE section[160], key[160];

   if ((!Args) OR (!Args->Field) OR (!Args->Field[0])) return ERR_NullArgs;

   if (!StrCompare("Index(", Args->Field, 6, NULL)) {
      // Field is in the format "Index(SectionIndex;KeyIndex)" or "Index(OverallIndex)".
      // Quotes can be used to indicate strings instead of indexes.

      i = 6;
      struct ConfigEntry *entries = Self->Entries;

      // Extract the section index

      if (Args->Field[i] IS '"') {
         i++;
         for (j=0; (Args->Field[i]) AND (Args->Field[i] != '"') AND (j < sizeof(section)-1); j++) section[j] = Args->Field[i++];
         section[j] = 0;

         if (Args->Field[i] IS '"') i++;
         if (Args->Field[i] IS ',') i++;

         // Convert the section string to an absolute index

         index = find_section_name(Self, section);
      }
      else {
         index = StrToInt(Args->Field);
         while ((Args->Field[i] >= '0') AND (Args->Field[i] <= '9')) i++;
         if (Args->Field[i] IS ',') {
            i++;
            // Convert the section index so that it is absolute
            if ((index = find_section(Self, index)) IS -1) return PostError(ERR_OutOfRange);
         }
      }

      // Extract the key index (if there is one) and add it to the absolute index

      if (Args->Field[i] IS '"') {
         i++;
         for (j=0; (Args->Field[i]) AND (Args->Field[i] != '"') AND (j < sizeof(key)-2); j++) key[j] = Args->Field[i++];
         key[j] = 0;

         while (index < Self->AmtEntries) {
            if (!StrMatch(key, entries[index].Key)) break;

            if ((index < Self->AmtEntries-1) AND (entries[index+1].Section != entries[index].Section)) {
               // We have reached the end of the section without finding the key
               return ERR_Search;
            }
            index++;
         }
      }
      else if ((Args->Field[i] >= '0') AND (Args->Field[i] <= '9')) {
         index += StrToInt(Args->Field + i);
      }

      // We now have an overall index that we can use

      if ((index >= Self->AmtEntries) OR (index < 0)) return PostError(ERR_OutOfRange);

      StrCopy(entries[index].Section, section, sizeof(section));
      StrCopy(entries[index].Key, key, sizeof(key));
      return cfgWriteValue(Self, section, key, Args->Value);
   }

   for (len=0; Args->Field[len]; len++) {
      if ((Args->Field[len] IS ';') OR (Args->Field[len] IS '.')) {
         // Field is in the format: "Section;Key" or "Section.Key"
         UBYTE buffer[len+1];
         for (i=0; i < len; i++) buffer[i] = Args->Field[i];
         buffer[i] = 0;
         return cfgWriteValue(Self, buffer, Args->Field + len + 1, Args->Value);
      }
      else if (Args->Field[len] IS '(') {
         // Field is in the format "Section(Key)"
         UBYTE section[len+1], field[40];
         for (i=0; i < len; i++) section[i] = Args->Field[i];
         section[i] = 0;

         len++;
         for (i=0; (Args->Field[len]) AND (Args->Field[len] != ')') AND (i < sizeof(field)-1); i++) field[i] = Args->Field[len++];
         field[i] = 0;
         return cfgWriteValue(Self, section, field, Args->Value);
      }
   }

   // Field is in the format: "Key".  Section defaults to "Variables"

   cfgWriteValue(Self, "Variables", Args->Field, Args->Value);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Sort: Sorts config sections into alphabetical order.
-END-
*****************************************************************************/

static ERROR CONFIG_Sort(objConfig *Self, APTR Void)
{
   struct ConfigEntry *entries;
   LONG i, pos, j;
   STRING array[Self->TotalSections+1];
   struct ConfigEntry entrybuffer[Self->AmtEntries];

   LogBranch("Sorting by section name.");

   // Copy all of the section strings into an array and sort them

   if ((entries = Self->Entries)) {
      pos = 0;
      array[pos++] = entries[0].Section;
      for (i=0; i < Self->AmtEntries-1; i++) {
         if (entries[i].Section != entries[i+1].Section) {
            if (pos < Self->TotalSections) array[pos] = entries[i+1].Section;
            pos++;
         }
      }

      if (pos > Self->TotalSections) {
         LogErrorMsg("Buffer overflow - expected %d sections, encountered %d.", Self->TotalSections, pos);
         LogBack();
         return ERR_BufferOverflow;
      }
   }
   else {
      LogBack();
      return ERR_NoData;
   }

   array[pos] = NULL;
   StrSort(array, NULL);

   // Re-sort the config data based on the sorted section strings

   pos = 0;
   for (i=0; array[i]; i++) {
      for (j=0; j < Self->AmtEntries; j++) {
         if (!StrCompare(array[i], Self->Entries[j].Section, 0, STR_CASE|STR_MATCH_LEN)) {
            CopyMemory(Self->Entries + j, entrybuffer + pos, sizeof(struct ConfigEntry));
            pos++;
         }
      }
   }

   // Copy our sorted buffer back into the config entry array

   CopyMemory(entrybuffer, Self->Entries, sizeof(struct ConfigEntry) * Self->AmtEntries);

   LogBack();
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SortByKey: Sorts config data using a sequence of sort instructions.

The SortByKey method sorts a config object by section, using a common key for each section as the sort key.

-INPUT-
cstr Key: The name of the key to sort on.
int Descending: Set to TRUE if a descending sort is required.

-ERRORS-
Okay
NoData
-END-

*****************************************************************************/

struct sortlist {
   STRING section; // The name of the section
   STRING sort;    // The text to sort the section on
};

static void sift_down(struct sortlist *, LONG, LONG);
static void sift_up(struct sortlist *, LONG, LONG);

static ERROR CONFIG_SortByKey(objConfig *Self, struct cfgSortByKey *Args)
{
   struct ConfigEntry *entries;
   LONG i, pos, j, heapsize;
   struct sortlist array[Self->TotalSections+1], temp;
   struct ConfigEntry entrybuffer[Self->AmtEntries];

   // If no args are provided, use the default Sort action instead

   if ((!Args) OR (!Args->Key)) return CONFIG_Sort(Self, NULL);

   if (!(entries = Self->Entries)) return PostError(ERR_NoData);

   LogBranch("Key: %s", Args->Key);

   // Generate a sorting table consisting of unique section names and the key values that we will be sorting on.

   array[0].section = entries[0].Section;
   array[0].sort    = read_config(Self, entries[0].Section, Args->Key);
   pos = 1;
   for (i=0; i < Self->AmtEntries-1; i++) {
      if (entries[i].Section != entries[i+1].Section) {
         if (pos < Self->TotalSections) {
            array[pos].section = entries[i+1].Section;
            array[pos].sort    = read_config(Self, entries[i+1].Section, Args->Key);
         }
         pos++;
      }
   }

   if (pos != Self->TotalSections) {
      LogErrorMsg("Buffer overflow/underflow - expected %d sections, encountered %d.", Self->TotalSections, pos);
      LogBack();
      return ERR_BufferOverflow;
   }

   array[pos].section = NULL;
   array[pos].sort    = NULL;

   // Do the sort

   if (Args->Descending) {
      for (i=Self->TotalSections>>1; i >= 0; i--) sift_down(array, i, Self->TotalSections);

      heapsize = Self->TotalSections;
      for (i=heapsize; i > 0; i--) {
         temp = array[0];
         array[0] = array[i-1];
         array[i-1] = temp;
         sift_down(array, 0, --heapsize);
      }
   }
   else {
      for (i=Self->TotalSections>>1; i >= 0; i--) sift_up(array, i, Self->TotalSections);

      heapsize = Self->TotalSections;
      for (i=heapsize; i > 0; i--) {
         temp = array[0];
         array[0] = array[i-1];
         array[i-1] = temp;
         sift_up(array, 0, --heapsize);
      }
   }

   // Re-sort the config data according to the sort results

   pos = 0;
   for (i=0; i < Self->TotalSections; i++) {
      for (j=0; j < Self->AmtEntries; j++) {
         if (!StrCompare(array[i].section, Self->Entries[j].Section, 0, STR_CASE|STR_MATCH_LEN)) {
            CopyMemory(Self->Entries + j, entrybuffer + pos, sizeof(struct ConfigEntry));
            pos++;
         }
      }
   }

   // Copy our sorted buffer back into the config entry array

   CopyMemory(entrybuffer, Self->Entries, sizeof(struct ConfigEntry) * Self->AmtEntries);

   LogBack();
   return ERR_Okay;
}

inline static BYTE sort(STRING Name1, STRING Name2)
{
   UBYTE char1, char2;

   while ((*Name1) AND (*Name2)) {
      char1 = *Name1;
      char2 = *Name2;
      if ((char1 >= 'A') AND (char1 <= 'Z')) char1 = char1 - 'A' + 'a';
      if ((char2 >= 'A') AND (char2 <= 'Z')) char2 = char2 - 'A' + 'a';

      if ((char1 > char2) OR ((Name1[1]) AND (!Name2[1]))) return 1; // Name1 is greater
      else if (char1 < char2) return -1; // Name1 is lesser

      Name1++;
      Name2++;
   }

   if ((!*Name1) AND (!*Name2)) return 0;
   else if (!*Name1) return -1;
   else return 1;
}

static void sift_down(struct sortlist *lookup, LONG i, LONG heapsize)
{
   struct sortlist temp;
   LONG left, right;

   LONG largest = i;
   do {
      i = largest;
      left	= (i << 1) + 1;
      right	= left + 1;

      if (left < heapsize){
         if (sort(lookup[largest].sort, lookup[left].sort) > 0) largest = left;

         if (right < heapsize) {
            if (sort(lookup[largest].sort, lookup[right].sort) > 0) largest = right;
         }
      }

      if (largest != i) {
         temp = lookup[i];
         lookup[i] = lookup[largest];
         lookup[largest] = temp;
      }
   } while (largest != i);
}

static void sift_up(struct sortlist *lookup, LONG i, LONG heapsize)
{
   struct sortlist temp;
   LONG left, right;

   LONG largest = i;
   do {
      i = largest;
      left	= (i << 1) + 1;
      right	= left + 1;

      if (left < heapsize){
         if (sort(lookup[largest].sort, lookup[left].sort) < 0) largest = left;

         if (right < heapsize) {
            if (sort(lookup[largest].sort, lookup[right].sort) < 0) largest = right;
         }
      }

      if (largest != i) {
         temp = lookup[i];
         lookup[i] = lookup[largest];
         lookup[largest] = temp;
      }
   } while (largest != i);
}

/*****************************************************************************

-METHOD-
WriteValue: Adds new entries to config objects.

Use the WriteValue method to add or update information in a config object.  A Section name, Key name, and Data value
are required.  If the Section and Key arguments match an existing entry in the config object, the data of that entry
will be replaced with the new Data value.

The Section string may refer to an index if the hash `#` character is used to precede a target index number.

-INPUT-
cstr Section: The name of the section.
cstr Key:    The name of the key.
cstr Data:   The data that will be added to the given section/key.

-ERRORS-
Okay
NullArgs
Args
AllocMemory: The additional memory required for the new entry could not be allocated.
GetField: The Entries field could not be retrieved.
-END-

*****************************************************************************/

static ERROR CONFIG_WriteValue(objConfig *Self, struct cfgWriteValue *Args)
{
   struct ConfigEntry *entries, *newentries;
   MEMORYID newEntriesMID, newStrMID;
   STRING newstr, str;
   LONG i, j, pos, replaceindex, len, sectionindex, strlen, strsize, maxentries;
   UBYTE section[160];

   if (!Args) return PostError(ERR_NullArgs);

   //MSG("%s.%s = %s", Args->Section, Args->Key, Args->Data);

   if ((!Args->Section) OR (!Args->Section[0])) {
      LogErrorMsg("The Section argument is missing.");
      return ERR_Args;
   }

   if ((!Args->Key) OR (!Args->Key[0])) {
      LogErrorMsg("The Key argument is missing.");
      return ERR_Args;
   }

   // Take a copy of the section.  This is important because sometimes our method will be passed a section name that
   // refers to our own address space (e.g. entries[i].Section), and since this function invalidates that address
   // space, it's safer to make a copy.

   for (i=0; (Args->Section[i]) AND (i < sizeof(section)-1); i++) section[i] = Args->Section[i];
   section[i] = 0;

   if (section[0] IS '#') {
      // Section name is actually referring to an index - we need to extract the section name from that.

      LONG index = StrToInt(section+1);
      if ((i = find_section(Self, index)) != -1) {
         if ((entries = Self->Entries)) {
            for (j=0; (entries[i].Section[j]) AND (j < sizeof(section)-1); j++) section[j] = entries[i].Section[j];
            section[j] = 0;
         }
         else return PostError(ERR_NoData);
      }
      else return PostError(ERR_Search);
   }

   if (Self->AmtEntries < 1) {
      // Create the first entry and return

      for (len=0; section[len]; len++);
      strlen = len + 1;

      for (len=0; Args->Key[len]; len++);
      strlen += len + 1;

      if (Args->Data) for (len=0; Args->Data[len]; len++);
      else len = 0;
      strlen += len + 1;

      Self->MaxEntries = ENTBLOCKSIZE;

      if (strlen > STRBLOCKSIZE) Self->StringsSize = strlen;
      else Self->StringsSize = STRBLOCKSIZE;

      if (!AllocMemory(Self->MaxEntries * sizeof(struct ConfigEntry), Self->Head.MemFlags|MEM_NO_CLEAR, (void **)&Self->Entries, &Self->EntriesMID)) {
         if (!AllocMemory(Self->StringsSize, Self->Head.MemFlags|MEM_NO_CLEAR, (void **)&Self->Strings, &Self->StringsMID)) {
            pos = 0;
            str = Self->Strings;

            Self->Entries[0].Section = str + pos;
            Self->Entries[0].SectionOffset = pos;
            for (j=0; section[j]; j++) str[pos++] = section[j];
            str[pos++] = 0;

            Self->Entries[0].Key = str + pos;
            Self->Entries[0].KeyOffset = pos;
            for (j=0; Args->Key[j]; j++) str[pos++] = Args->Key[j];
            str[pos++] = 0;

            Self->Entries[0].Data = str + pos;
            Self->Entries[0].DataOffset = pos;
            if (Args->Data) for (j=0; Args->Data[j]; j++) str[pos++] = Args->Data[j];
            str[pos++] = 0;

            Self->TotalSections = 1;
            Self->AmtEntries = 1;
            Self->StringsPos = pos;
            return ERR_Okay;
         }
         else return ERR_AllocMemory;
      }
      else return ERR_AllocMemory;
   }

   if (!(entries = Self->Entries)) return ERR_GetField;

   // Check if the section and key names match an existing record.  If so, we'll 'delete' that entry from the buffer
   // as our new entry information will be replacing it.

   replaceindex = -1;
   sectionindex = -1;
   for (i=0; i < Self->AmtEntries; i++) {
      if (!StrMatch(entries[i].Section, section)) {
         sectionindex = i;
         if (!StrMatch(entries[i].Key, Args->Key)) {
            if (!StrMatch(entries[i].Data, Args->Data)) return ERR_Okay;

            if (Self->Flags & CNF_LOCK_RECORDS) return ERR_Exists;

            replaceindex = i;
            break;
         }
      }
   }

   // Calculate the amount of bytes required for the Section, Key & Data

   for (len=0; section[len]; len++);
   strsize = len + 1;

   for (len=0; Args->Key[len]; len++);
   strsize += len + 1;

   if (Args->Data) for (len=0; Args->Data[len]; len++);
   else len = 0;
   strsize += len + 1;

   if (replaceindex != -1) {
      // Replace an existing key.  Expand the string buffer if necessary, replace the entry with new key and data
      // string pointers, then return.  The section remains unchanged in the event of a replace operation.

      MSG("Replace %d/%d %s / %s = %s TO %d/%d", replaceindex, Self->AmtEntries, Args->Section, Args->Key, Args->Data, Self->StringsPos, Self->StringsSize);

      if (Self->StringsPos + strsize >= Self->StringsSize) {
         if (strsize > STRBLOCKSIZE) strsize = Self->StringsPos + strsize;
         else strsize = Self->StringsPos + STRBLOCKSIZE;

         if (AllocMemory(strsize, Self->Head.MemFlags|MEM_NO_CLEAR, (void **)&newstr, &newStrMID) != ERR_Okay) return ERR_AllocMemory;
         CopyMemory(Self->Strings, newstr, Self->StringsPos);
         ReleaseMemoryID(Self->StringsMID);
         FreeMemoryID(Self->StringsMID);

         Self->Strings = newstr;
         Self->StringsMID = newStrMID;
         Self->StringsSize = strsize;

         resolve_addresses(Self);
      }

      pos = Self->StringsPos;
      str = Self->Strings;

      if (StrMatch(Self->Entries[replaceindex].Key, Args->Key) != ERR_Okay) {
         MSG("Replace @ Key offset %d", pos);
         Self->Entries[replaceindex].Key       = str + pos;
         Self->Entries[replaceindex].KeyOffset = pos;
         for (j=0; Args->Key[j]; j++) str[pos++] = Args->Key[j];
         str[pos++] = 0;
      }

      if (StrMatch(Self->Entries[replaceindex].Data, Args->Data) != ERR_Okay) {
         MSG("Replace @ Data offset %d", pos);
         Self->Entries[replaceindex].Data       = str + pos;
         Self->Entries[replaceindex].DataOffset = pos;
         if (Args->Data) for (j=0; Args->Data[j]; j++) str[pos++] = Args->Data[j];
         str[pos++] = 0;
      }

      Self->StringsPos = pos;

      return ERR_Okay;
   }

   //MSG("Total Entries: %d++, [%s] Key: '%.10s' = '%.10s'", Self->AmtEntries, Args->Section, Args->Key, Args->Data);

   if (Self->AmtEntries >= Self->MaxEntries-1) {
      // Expand the entries array
      MSG("Expanding the entries array.");
      maxentries = Self->MaxEntries + ENTBLOCKSIZE;
      if (!AllocMemory(maxentries * sizeof(struct ConfigEntry), Self->Head.MemFlags|MEM_NO_CLEAR, (void **)&newentries, &newEntriesMID)) {
         CopyMemory(Self->Entries, newentries, Self->AmtEntries * sizeof(struct ConfigEntry));
         ReleaseMemoryID(Self->EntriesMID);
         FreeMemoryID(Self->EntriesMID);

         Self->MaxEntries = maxentries;
         Self->Entries    = newentries;
         Self->EntriesMID = newEntriesMID;
      }
      else return ERR_AllocMemory;
   }

   if (Self->StringsPos + strsize >= Self->StringsSize) {
      MSG("Expanding the strings array.");
      if (strsize > STRBLOCKSIZE) strsize = Self->StringsPos + strsize;
      else strsize = Self->StringsPos + STRBLOCKSIZE;

      if (!AllocMemory(strsize, Self->Head.MemFlags|MEM_NO_CLEAR, (void **)&newstr, &newStrMID)) {
         CopyMemory(Self->Strings, newstr, Self->StringsPos);
         ReleaseMemoryID(Self->StringsMID);
         FreeMemoryID(Self->StringsMID);

         Self->StringsSize = strsize;
         Self->Strings     = newstr;
         Self->StringsMID  = newStrMID;

         resolve_addresses(Self);
      }
      else return ERR_AllocMemory;
   }

   if (sectionindex != -1) {
      // Entry belongs in an existing section
      sectionindex++;
      if (sectionindex < Self->AmtEntries) {
         // Create a space in the entries array
         CopyMemory(Self->Entries + sectionindex, Self->Entries + sectionindex + 1, sizeof(struct ConfigEntry) * (Self->AmtEntries - sectionindex));
      }

      Self->Entries[sectionindex].SectionOffset = Self->Entries[sectionindex-1].SectionOffset;
      Self->Entries[sectionindex].Section = Self->Entries[sectionindex-1].Section;
   }
   else {
      // Entry starts a new section
      sectionindex = Self->AmtEntries;
      Self->Entries[sectionindex].SectionOffset = Self->StringsPos;
      Self->Entries[sectionindex].Section = Self->Strings + Self->StringsPos;
      Self->StringsPos += scopy(Args->Section, Self->Strings + Self->StringsPos);
      Self->TotalSections++;
   }

   Self->Entries[sectionindex].KeyOffset = Self->StringsPos;
   Self->Entries[sectionindex].Key       = Self->Strings + Self->StringsPos;
   Self->StringsPos += scopy(Args->Key, Self->Strings + Self->StringsPos);

   Self->Entries[sectionindex].DataOffset = Self->StringsPos;
   Self->Entries[sectionindex].Data       = Self->Strings + Self->StringsPos;
   Self->StringsPos += scopy(Args->Data ? Args->Data : (STRING)"", Self->Strings + Self->StringsPos);

   Self->AmtEntries++;
   return ERR_Okay;
}

//****************************************************************************

static ERROR process_config_data(objConfig *Self, UBYTE *Src)
{
   struct cfgWriteValue write;
   STRING data;
   CSTRING start;
   LONG pos, sectionpos;

   if (!Src) return ERR_NoData;

   FMSG("~process_config()","%.20s", Src);

   // Process the file and get rid of PC carriage returns (by replacing them with standard line feeds).

   data = Src;
   while (*data != 0) {
      if (*data IS '\r') *data = '\n';
      data++;
   }

   write.Section = NULL;
   pos = 0;
   sectionpos = 0;

   if (!(data = (STRING)next_section(Src))) { // Find the first section
      data = Src;
   }

   while (*data) {
      while ((*data) AND (*data <= 0x20)) data++;
      if (*data IS '#') {
         data = (STRING)next_line(data);
         continue;
      }

      while ((*data) AND (*data != '[')) {
         if (check_for_key(data)) {
            write.Key = data;
            while ((*data) AND (*data != '=')) data++;
            if (!*data) break;
            while (data[-1] <= 0x20) data--;
            if (*data) *data++ = 0;

            while ((*data) AND (*data <= 0x20)) data++; // Skip whitespace
            if (*data IS '=') data++;
            while ((*data) AND (*data != '\n') AND (*data <= 0x20)) data++; // Skip any leading whitespace

            if ((Self->Flags & CNF_STRIP_QUOTES) AND (*data IS '"')) {
               data++;
               write.Data = data;
               while ((*data) AND (*data != '"')) data++;
               if (*data) *data++ = 0;
               data = (STRING)next_line(data);
            }
            else {
               write.Data = data;
               while ((*data) AND (*data != '\n')) data++;
               if (*data) *data++ = 0;
            }

            CONFIG_WriteValue(Self, &write);
         }
         else data = (STRING)next_line(data);
      }

      // Whenever we get to this point, there is either a new section or we must have come to the end of the buffer.

      if (*data IS '[') {
         data++; // Skip '[' character
         start = data;
         while ((*data != ']') AND (*data != '\n') AND (*data)) data++;
         if (*data IS ']') {
            write.Section = start;
            *data++ = 0;
         }
      }

      // Get the next line and repeat our loop

      data = (STRING)next_line(data);
   }

   STEP();
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
AmtEntries: The total number of values loaded into the config object.

-FIELD-
Entries: References the raw data values.

This field points to an array of @ConfigEntry keys. Each key represents a string of data parsed from the
source configuration file.  Direct scanning of the list of keys is a viable alternative to using the #ReadValue()
method.

This field is not writeable.  New values should be set with the #WriteValue() method.

*****************************************************************************/

static ERROR GET_Entries(objConfig *Self, struct ConfigEntry **Value)
{
   if (Self->Entries) {
      *Value = Self->Entries;
      return ERR_Okay;
   }
   else if (!Self->EntriesMID) {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
   else if (!AccessMemory(Self->EntriesMID, MEM_READ_WRITE, 2000, (APTR *)&Self->Entries)) {
      if (!AccessMemory(Self->StringsMID, MEM_READ_WRITE, 2000, (APTR *)&Self->Strings)) {
         resolve_addresses(Self);
         *Value = Self->Entries;
         return ERR_Okay;
      }
   }

   *Value = NULL;
   return ERR_AccessMemory;
}

/*****************************************************************************

-FIELD-
Flags: Special flags may be set here.

-FIELD-
KeyFilter: Set this field to enable key filtering.

When dealing with large configuration files, filtering out unrelated data may be useful for speeding up operations on
the config object.  By setting the KeyFilter field, you can filter out entire sections by setting criteria for each
configuration entry.

Key filters are created in the format `[Key] = [Data1], [Data2], ...`

Here are some examples:

<list type="unsorted">
<li>Group = Sun, Light</li>
<li>Path = documents:</li>
<li>Name = Parasol</li>
</>

You can also 'reverse' the filter so that only the keys matching your specifications are filtered out.  To do this,
use the exclamation character, as in the following examples:

<list type="unsorted">
<li>!Group = Sun, Light</li>
<li>!Path = documents:</li>
<li>!Name = Parasol</li>
</>

To create a filter based on section names, refer to the #SectionFilter field.

*****************************************************************************/

static ERROR GET_KeyFilter(objConfig *Self, STRING *Value)
{
   if (Self->KeyFilter) {
      *Value = Self->KeyFilter;
      return ERR_Okay;
   }
   else if (!Self->KeyFilterMID) {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
   else if (!AccessMemory(Self->KeyFilterMID, MEM_READ, 2000, (void **)&Self->KeyFilter)) {
      *Value = Self->KeyFilter;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return ERR_AccessMemory;
   }
}

static ERROR SET_KeyFilter(objConfig *Self, CSTRING Value)
{
   if (Self->KeyFilter)    { ReleaseMemoryID(Self->KeyFilterMID);   Self->KeyFilter = NULL; }
   if (Self->KeyFilterMID) { FreeMemoryID(Self->KeyFilterMID); Self->KeyFilterMID = NULL; }

   if ((Value) AND (*Value)) {
      LONG i;
      for (i=0; Value[i]; i++);
      if (!AllocMemory(i+1, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (void **)&Self->KeyFilter, &Self->KeyFilterMID)) {
         for (i=0; Value[i]; i++) Self->KeyFilter[i] = Value[i];
         Self->KeyFilter[i] = 0;
      }
      else return ERR_AllocMemory;
   }
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Path: Set this field to the location of the source configuration file.

*****************************************************************************/

static ERROR GET_Path(objConfig *Self, STRING *Value)
{
   if (Self->Path) {
      *Value = Self->Path;
      return ERR_Okay;
   }
   else if (!Self->PathMID) {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
   else if (!AccessMemory(Self->PathMID, MEM_READ, 2000, (void **)&Self->Path)) {
      *Value = Self->Path;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return ERR_AccessMemory;
   }
}

static ERROR SET_Path(objConfig *Self, CSTRING Value)
{
   if (Self->Path)    { ReleaseMemoryID(Self->PathMID);   Self->Path = NULL; }
   if (Self->PathMID) { FreeMemoryID(Self->PathMID); Self->PathMID = NULL; }

   if ((Value) AND (*Value)) {
      LONG i;
      for (i=0; Value[i]; i++);
      if (!AllocMemory(i+1, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (void **)&Self->Path, &Self->PathMID)) {
         for (i=0; Value[i]; i++) Self->Path[i] = Value[i];
         Self->Path[i] = 0;
      }
      else return ERR_AllocMemory;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
SectionFilter: Set this field to enable section filtering.

When dealing with large configuration files, filtering out unrelated data may be useful.  By setting the SectionFilter
field, it is possible to filter out entire sections that don't match the criteria.

Section filters are created using the format `[Section1], [Section2], [Section3], ...`.

Here are some examples:

<list type="unsorted">
<li>Program, Application, Game</li>
<li>Apple, Banana</li>
</>

You can also reverse the filter so that only the sections matching your criteria are filtered out.  To do this, use the
exclamation character, as in the following examples:

<list type="unsorted">
<li>!Program, Application, Game</li>
<li>!Apple, Banana</li>
</>

If you need to create a filter based on key names, refer to the #KeyFilter field.

*****************************************************************************/

static ERROR GET_SectionFilter(objConfig *Self, STRING *Value)
{
   if (Self->SectionFilter) {
      *Value = Self->SectionFilter;
      return ERR_Okay;
   }
   else if (!Self->SectionFilterMID) {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
   else if (!AccessMemory(Self->SectionFilterMID, MEM_READ, 2000, (void **)&Self->SectionFilter)) {
      *Value = Self->SectionFilter;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return ERR_AccessMemory;
   }
}

static ERROR SET_SectionFilter(objConfig *Self, STRING Value)
{
   if (Self->SectionFilter)    { ReleaseMemoryID(Self->SectionFilterMID);   Self->SectionFilter = NULL; }
   if (Self->SectionFilterMID) { FreeMemoryID(Self->SectionFilterMID); Self->SectionFilterMID = NULL; }

   if ((Value) AND (*Value)) {
      LONG i;
      for (i=0; Value[i]; i++);
      if (!AllocMemory(i+1, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (void **)&Self->SectionFilter, &Self->SectionFilterMID)) {
         for (i=0; Value[i]; i++) Self->SectionFilter[i] = Value[i];
         Self->SectionFilter[i] = 0;
      }
      else return ERR_AllocMemory;
   }
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
TotalSections: Returns the total number of sections in a config object.
-END-
*****************************************************************************/

static ERROR GET_TotalSections(objConfig *Self, LONG *Result)
{
   struct ConfigEntry *entries;

   if (!GET_Entries(Self, &entries)) {
      LONG count = 1;
      LONG i;
      for (i=0; i < Self->AmtEntries-1; i++) {
         if (entries[i].Section != entries[i+1].Section) count++;
      }
      *Result = count;
      return ERR_Okay;
   }
   else {
      *Result = NULL;
      return ERR_FieldNotSet;
   }
}

//****************************************************************************
// Checks the next line in a buffer to see if it is a valid key.

static WORD check_for_key(CSTRING Data)
{
   if ((*Data != '\n') AND (*Data != '[') AND (*Data != '#')) {
      while ((*Data) AND (*Data != '\n') AND (*Data != '=')) Data++; // Skip key name
      if (*Data != '=') return FALSE;
      Data++;
      while ((*Data) AND (*Data != '\n') AND (*Data <= 0x20)) Data++; // Skip whitespace
//      if ((*Data IS '\n') OR (*Data IS 0)) return FALSE; // Check if data is present or the line terminates prematurely
      return TRUE;
   }

   return FALSE;
}

/*****************************************************************************
** Return codes
** ------------
** 1:  Indicates a match (do not delete the section).
** 0:  Indicates a failed match (delete the section).
** -1: Indicates that the key does not match the key specified in the filter.
*/

static LONG check_filter(objConfig *Self, STRING Filter, STRING Key, STRING Data)
{
   while ((*Filter) AND (*Filter <= 0x20)) Filter++;

   BYTE reverse = FALSE;
   if (*Filter IS '!') {
      reverse = TRUE;
      Filter++;
   }

   while ((*Filter) AND (*Filter <= 0x20)) Filter++;

   // Pull out the key

   #define DATA_SIZE 100
   BYTE data[DATA_SIZE];
   LONG i;
   for (i=0; (*Filter != '=') AND (i < DATA_SIZE-1); i++) data[i] = *Filter++;
   while ((i > 0) AND (data[i-1] <= 0x20)) i--;
   data[i] = 0;

   if (StrMatch(data, Key) IS ERR_Okay) {
      if (!Data) return 1;

      // Skip " = "

      while ((*Filter) AND (*Filter <= 0x20)) Filter++;
      if (*Filter IS '=') Filter++;
      while ((*Filter) AND (*Filter <= 0x20)) Filter++;

      while (*Filter) {
         for (i=0; ((*Filter) AND (*Filter != ',')); i++) data[i] = *Filter++;
         data[i] = 0;

         if (!data[0]) return 0;

         if (!StrMatch(data, Data)) return 1;

         if (*Filter IS ',') Filter++;
         while ((*Filter) AND (*Filter <= 0x20)) Filter++;
      }
      return 0;
   }
   else return -1; // -1 indicates that the filter's key does not match the entry
}

//****************************************************************************
// This function searches for the next line of information.  Note that if it reaches a NULL byte, it will
// stop and return the Data pointer immediately rather than running into other mem space.

static CSTRING next_line(CSTRING Data)
{
   while ((*Data != '\n') AND (*Data)) Data++;
   while (*Data IS '\n') Data++; // If there are extra line breaks, skip them
   return Data;
}

//****************************************************************************
// Checks for the next section in a text buffer and validates it.

static CSTRING next_section(CSTRING Data)
{
   CSTRING CData;
   while (*Data != 0) {
      if (*Data IS '[') {
         CData = Data + 1;
         while ((*CData != '\n') AND (*CData != 0)) {
            if (*CData IS ']') return Data;
            CData++;
         }
      }
      Data = next_line(Data);
   }
   return Data;
}

//****************************************************************************
// Converts a standard section index into an absolute index.

static LONG find_section(objConfig *Self, LONG Number)
{
   if (Number < 0) return -1;

   LONG pos = 0;
   while ((Number > 0) AND (pos < Self->AmtEntries-1)) {
      if (Self->Entries[pos].Section != Self->Entries[pos+1].Section) Number--;
      pos++;
   }

   if (!Number) return pos;

   return -1;
}

//****************************************************************************
// Returns the index of a section, given a section name.

static LONG find_section_name(objConfig *Self, CSTRING Section)
{
   if ((!Section) OR (!*Section)) return ERR_NullArgs;

   LONG index;
   for (index=0; index < Self->AmtEntries; index++) {
      if ((index > 0) AND (Self->Entries[index-1].Section IS Self->Entries[index].Section)) continue; // Avoid string comparisons where we can help it

      if (!StrMatch(Section, Self->Entries[index].Section)) {
         return index;
      }
   }
   return -1;
}

//****************************************************************************
// Returns the index of a section, given a section name.

static LONG find_section_wild(objConfig *Self, CSTRING Section)
{
   if ((!Section) OR (!*Section)) return ERR_NullArgs;

   LONG index;
   for (index=0; index < Self->AmtEntries; index++) {
      if ((index > 0) AND (Self->Entries[index-1].Section IS Self->Entries[index].Section)) continue; // Avoid string comparisons where we can help it

      if (!StrCompare(Section, Self->Entries[index].Section, NULL, STR_WILDCARD)) {
         return index;
      }
   }

   return -1;
}

//****************************************************************************

static STRING read_config(objConfig *Self, CSTRING Section, CSTRING Key)
{
   LONG i;
   for (i=0; i < Self->AmtEntries; i++) {
      if (Section IS Self->Entries[i].Section) {
         if (!StrMatch(Key, Self->Entries[i].Key)) return Self->Entries[i].Data;
      }
   }
   return NULL;
}

//****************************************************************************

#include "class_config_def.c"

static const struct FieldArray clFields[] = {
   { "Entries",       FDF_POINTER|FDF_R, 0, GET_Entries, NULL },   // This is not an FDF_ARRAY because each entry is sizeof(struct ConfigEntry)
   { "Path",          FDF_STRING|FDF_RI, 0, GET_Path, SET_Path },
   { "KeyFilter",     FDF_STRING|FDF_RI, 0, GET_KeyFilter, SET_KeyFilter },
   { "SectionFilter", FDF_STRING|FDF_RW, 0, GET_SectionFilter, SET_SectionFilter },
   { "AmtEntries",    FDF_LONG|FDF_R, 0, NULL, NULL },
   { "Flags",         FDF_LONGFLAGS|FDF_RW, (MAXINT)&clFlags, NULL, NULL },
   { "TotalSections", FDF_LONG|FDF_R, 0, GET_TotalSections, NULL },
   // Virtual fields
   { "Location",      FDF_SYNONYM|FDF_STRING|FDF_RI, 0, GET_Path, SET_Path },
   { "Src",           FDF_SYNONYM|FDF_STRING|FDF_RI, 0, GET_Path, SET_Path },
   END_FIELD
};

