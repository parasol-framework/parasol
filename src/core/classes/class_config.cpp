/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
Config: Manages the reading and writing of configuration files.

The Config class is provided for reading text based key-values in a simple structured format.  Although basic and
lacking support for trees and types, they are reliable, easy to support and use minimal resources.

The following segment of a config file illustrates:

<pre>
[Action]
ClassID  = 5800
Path = modules:action

[Animation]
ClassID  = 1000
Path = modules:animation

[Arrow]
ClassID  = 3200
Path = modules:arrow
</pre>

Notice the text enclosed in square brackets, such as `[Action]`. These are referred to as 'groups', which are
responsible for holding groups of key values expressed as strings.  In the above example, keys are defined by the
`ClassID` and `Path` identifiers.

The following source code illustrates how to open the classes.cfg file and read a key from it:

<pre>
local cfg = obj.new('config', { path='config:classes.cfg' })
local err, str = cfg.mtReadValue('Action', 'Path')
print('The Action class is located at ' .. str)
</pre>

Please note that internal string comparisons of group and key names are case sensitive by default.  Use of camel-case
is recommended as the default naming format.

-END-

*********************************************************************************************************************/

#include "../defs.h"
#include <parasol/main.h>

class FilterConfig {
   public:
   bool reverse;
   std::string name;
   std::list<std::string> values;
};

static ERR GET_KeyFilter(extConfig *, CSTRING *);
static ERR GET_GroupFilter(extConfig *, CSTRING *);
static ERR GET_TotalGroups(extConfig *, LONG *);

static ERR CONFIG_SaveSettings(extConfig *, APTR);

static const FieldDef clFlags[] = {
   { "AutoSave",    CNF::AUTO_SAVE },
   { "StripQuotes", CNF::STRIP_QUOTES },
   { "New",         CNF::NEW },
   { NULL, 0 }
};

#define CF_MATCHED  1
#define CF_FAILED   0
#define CF_KEY_FAIL -1

//********************************************************************************************************************

static bool check_for_key(CSTRING);
static ERR parse_config(extConfig *, CSTRING);
static ConfigKeys * find_group_wild(extConfig *Self, CSTRING Group);
static void apply_key_filter(extConfig *, CSTRING);
static void apply_group_filter(extConfig *, CSTRING);
static class FilterConfig parse_filter(std::string, bool);

//********************************************************************************************************************

template <class T>
T next_line(T Data)
{
   while ((*Data != '\n') and (*Data)) Data++;
   while ((*Data) and (*Data <= 0x20)) Data++; // Skip empty lines and any leading whitespace
   return Data;
}

//********************************************************************************************************************
// Searches for the next group in a text buffer, returns its name and the start of the first key value.

template <class T>
T next_group(T Data, std::string &GroupName)
{
   while (*Data) {
      if (*Data IS '[') {
         LONG len;
         for (len=1; (Data[len] != '\n') and (Data[len]); len++) {
            if (Data[len] IS '[') break; // Invalid character check
            if (Data[len] IS ']') {
               GroupName.assign(Data, 1, len-1);
               return next_line(Data+len); // Skip all trailing characters to reach the next line
            }
         }
         Data += len;
      }
      Data = next_line(Data);
   }
   return Data;
}

//********************************************************************************************************************

static std::pair<std::string, std::map<std::string, std::string>> * find_group(extConfig *Self, std::string GroupName)
{
   for (auto scan = Self->Groups->begin(); scan != Self->Groups->end(); scan++) {
      if (!scan->first.compare(GroupName)) return &(*scan);
   }
   return NULL;
}

//********************************************************************************************************************

static ULONG calc_crc(extConfig *Self)
{
   ULONG crc = 0;
   for (auto& [group, keys] : Self->Groups[0]) {
      crc = GenCRC32(crc, (APTR)group.c_str(), group.size());
      for (auto& [k, v] : keys) {
         crc = GenCRC32(crc, (APTR)k.c_str(), k.size());
         crc = GenCRC32(crc, (APTR)v.c_str(), v.size());
      }
   }
   return crc;
}

//********************************************************************************************************************
// Open a file with read only and exclusive flags, then read all of the data into a buffer.  Terminate the buffer,
// then free the file.
//
// Note that multiple files can be specified by separating each file path with a pipe.  This allows you to merge
// many configuration files into one object.

static ERR parse_file(extConfig *Self, CSTRING Path)
{
   ERR error = ERR::Okay;
   while ((*Path) and (error IS ERR::Okay)) {
      objFile::create file = { fl::Path(Path), fl::Flags(FL::READ|FL::APPROXIMATE) };

      if (file.ok()) {
         auto filesize = file->get<LONG>(FID_Size);

         if (filesize > 0) {
            STRING data;
            if (AllocMemory(filesize + 3, MEM::DATA|MEM::NO_CLEAR, (APTR *)&data, NULL) IS ERR::Okay) {
               file->read(data, filesize); // Read the entire file
               data[filesize++] = '\n';
               data[filesize] = 0;
               error = parse_config(Self, (CSTRING)data);
               FreeResource(data);
            }
            else error = ERR::AllocMemory;
         }
      }
      else if ((Self->Flags & CNF::OPTIONAL_FILES) != CNF::NIL) error = ERR::Okay;

      while ((*Path) and (*Path != ';') and (*Path != '|')) Path++;
      if (*Path) Path++; // Skip separator
   }

   return error;
}

/*********************************************************************************************************************
-ACTION-
Clear: Clears all configuration data.
-END-
*********************************************************************************************************************/

static ERR CONFIG_Clear(extConfig *Self, APTR Void)
{
   if (Self->Groups) { Self->Groups->clear(); }
   if (Self->KeyFilter) { FreeResource(Self->KeyFilter); Self->KeyFilter = NULL; }
   if (Self->GroupFilter) { FreeResource(Self->GroupFilter); Self->GroupFilter = NULL; }
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
DataFeed: Data can be added to a Config object through this action.

This action will accept configuration data in `TEXT` format.  Any existing data that matches to the new group keys will
be overwritten with new values.
-END-
*********************************************************************************************************************/

static ERR CONFIG_DataFeed(extConfig *Self, struct acDataFeed *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   if (Args->Datatype IS DATA::TEXT) {
      ERR error = parse_config(Self, (CSTRING)Args->Buffer);
      if (error IS ERR::Okay) {
         if (Self->GroupFilter) apply_group_filter(Self, Self->GroupFilter);
         if (Self->KeyFilter) apply_key_filter(Self, Self->KeyFilter);
      }
      return error;
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
DeleteKey: Deletes single key entries.

This method deletes a single key from the Config object.

-INPUT-
cstr Group: The name of the targeted group.
cstr Key: The name of the targeted key.

-ERRORS-
Okay
NullArgs
Search
-END-

*********************************************************************************************************************/

static ERR CONFIG_DeleteKey(extConfig *Self, struct cfgDeleteKey *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Group) or (!Args->Key)) return ERR::NullArgs;

   log.msg("Group: %s, Key: %s", Args->Group, Args->Key);

   for (auto& [group, keys] : Self->Groups[0]) {
      if (!group.compare(Args->Group)) {
         keys.erase(Args->Key);
         return ERR::Okay;
      }
   }

   return ERR::Search;
}

/*********************************************************************************************************************

-METHOD-
DeleteGroup: Deletes entire groups of configuration data.

This method will delete an entire group of key-values from a config object if a matching group name is provided.

-INPUT-
cstr Group: The name of the group that will be deleted.

-ERRORS-
Okay: The group was deleted or does not exist.
NullArgs
-END-

*********************************************************************************************************************/

static ERR CONFIG_DeleteGroup(extConfig *Self, struct cfgDeleteGroup *Args)
{
   if ((!Args) or (!Args->Group)) return ERR::NullArgs;

   for (auto it = Self->Groups->begin(); it != Self->Groups->end(); it++) {
      if (!it->first.compare(Args->Group)) {
         Self->Groups->erase(it);
         return(ERR::Okay);
      }
   }

   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
Flush: Diverts to #SaveSettings().
-END-
*********************************************************************************************************************/

static ERR CONFIG_Flush(extConfig *Self, APTR Void)
{
   return CONFIG_SaveSettings(Self, NULL);
}

//********************************************************************************************************************

static ERR CONFIG_Free(extConfig *Self, APTR Void)
{
   pf::Log log;

   if ((Self->Flags & CNF::AUTO_SAVE) != CNF::NIL) {
      if (Self->Path) {
         auto crc = calc_crc(Self);

         if ((!crc) or (crc != Self->CRC)) {
            log.msg("Auto-saving changes to \"%s\" (CRC: %d : %d)", Self->Path, Self->CRC, crc);

            objFile::create file = { fl::Path(Self->Path), fl::Flags(FL::WRITE|FL::NEW), fl::Permissions(PERMIT::NIL) };
            Self->saveToObject(*file);
         }
         else log.msg("Not auto-saving data (CRC unchanged).");
      }
   }

   if (Self->Groups) { delete Self->Groups; Self->Groups = NULL; }
   if (Self->Path) { FreeResource(Self->Path); Self->Path = 0; }
   if (Self->KeyFilter) { FreeResource(Self->KeyFilter); Self->KeyFilter = 0; }
   if (Self->GroupFilter) { FreeResource(Self->GroupFilter); Self->GroupFilter = 0; }
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
GetGroupFromIndex: Converts an index number into its matching group string.

Use GetGroupFromIndex() to convert a group index number to its matching name.

-INPUT-
int Index: The group index that you want to identify.
&cstr Group: Points to the group string that matches the index number.

-ERRORS-
Okay
Args
OutOfRange: The index number is out of range of the available groups.
NoData: There is no data loaded into the config object.

*********************************************************************************************************************/

static ERR CONFIG_GetGroupFromIndex(extConfig *Self, struct cfgGetGroupFromIndex *Args)
{
   pf::Log log;

   if ((!Args) or (Args->Index < 0)) return log.warning(ERR::Args);

   if ((Args->Index >= 0) and (Args->Index < (LONG)Self->Groups->size())) {
      Args->Group = Self->Groups[0][Args->Index].first.c_str();
      return ERR::Okay;
   }
   else return log.warning(ERR::OutOfRange);
}

//********************************************************************************************************************

static ERR CONFIG_Init(extConfig *Self, APTR Void)
{
   pf::Log log;

   if ((Self->Flags & CNF::NEW) != CNF::NIL) return ERR::Okay; // Do not load any data even if the path is defined.

   ERR error = ERR::Okay;
   if (Self->Path) {
      error = parse_file(Self, Self->Path);
      if (error IS ERR::Okay) {
         if (Self->GroupFilter) apply_group_filter(Self, Self->GroupFilter);
         if (Self->KeyFilter) apply_key_filter(Self, Self->KeyFilter);
      }
   }

   if ((Self->Flags & CNF::AUTO_SAVE) != CNF::NIL) Self->CRC = calc_crc(Self); // Store the CRC in advance of any changes
   return error;
}

/*********************************************************************************************************************

-METHOD-
Merge: Merges two config objects together.

The Merge() method is used to merge configuration data from one config object provided as a source, into the target 
object.  Existing data in the target will be overwritten by the source in cases where there matching set of group 
keys.

-INPUT-
obj Source: The Config object to be merged.

-ERRORS-
Okay
NullArgs
AccessObject: The source configuration object could not be accessed.
-END-

*********************************************************************************************************************/

static ERR CONFIG_Merge(extConfig *Self, struct cfgMerge *Args)
{
   if ((!Args) or (!Args->Source)) return ERR::NullArgs;
   if (Args->Source->classID() != ID_CONFIG) return ERR::Args;

   auto src = (extConfig *)Args->Source;
   merge_groups(Self->Groups[0], src->Groups[0]);
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
MergeFile: Merges a configuration file into existing configuration data.

The MergeFile() method is used to pull configuration data from a file and merge it into the target config object.
The path to the configuration file is all that is required.  Existing data in the target will be overwritten by the
source in cases where there matching set of group keys.

-INPUT-
cstr Path: The location of the configuration file that you want to merge.

-ERRORS-
Okay
NullArgs
File: Failed to load the source file.
-END-

*********************************************************************************************************************/

static ERR CONFIG_MergeFile(extConfig *Self, struct cfgMergeFile *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Path)) return log.warning(ERR::NullArgs);

   log.branch("%s", Args->Path);

   extConfig::create src = { fl::Path(Args->Path) };

   if (src.ok()) {
      merge_groups(Self->Groups[0], src->Groups[0]);
      return ERR::Okay;
   }
   else return ERR::File;
}

//********************************************************************************************************************

static ERR CONFIG_NewObject(extConfig *Self, APTR Void)
{
   if (!(Self->Groups = new (std::nothrow) ConfigGroups)) return ERR::Memory;
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
ReadValue: Reads a key-value string.

This function retrieves key values in their original string format.  On success, the resulting string remains valid
only for as long as the client has exclusive access to the config object.  The pointer can also be invalidated
if more information is written to the config object.  For this reason, consider copying the result if it will be
used extensively.

If the `Group` parameter is set to `NULL`, the scan routine will treat all of the config data as a one dimensional array.
If the `Key` parameter is set to `NULL` then the first key in the requested group is returned.  If both parameters
are `NULL` then the first known key value will be returned.

-INPUT-
cstr Group: The name of a group to examine for a key.  If `NULL`, all groups are scanned.
cstr Key: The name of a key to retrieve (case sensitive).
&cstr Data: The key value will be stored in this parameter on returning.

-ERRORS-
Okay
NullArgs
Search: The requested configuration entry does not exist.
-END-

*********************************************************************************************************************/

static ERR CONFIG_ReadValue(extConfig *Self, struct cfgReadValue *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   for (auto & [group, keys] : Self->Groups[0]) {
      if ((Args->Group) and (group.compare(Args->Group))) continue;

      if (!Args->Key) {
         Args->Data = keys.cbegin()->second.c_str();
         return ERR::Okay;
      }
      else if (keys.contains(Args->Key)) {
         Args->Data = keys[Args->Key].c_str();
         return ERR::Okay;
      }
   }

   log.trace("Could not find key %s : %s.", Args->Group, Args->Key);
   Args->Data = NULL;
   return ERR::Search;
}

/*********************************************************************************************************************

-ACTION-
SaveSettings: Saves data to the file that the configuration data was loaded from.

This action will save the configuration data back to its original file source (assuming the #Path remains unchanged).

*********************************************************************************************************************/

static ERR CONFIG_SaveSettings(extConfig *Self, APTR Void)
{
   pf::Log log;
   log.branch();

   ULONG crc = calc_crc(Self);
   if (((Self->Flags & CNF::AUTO_SAVE) != CNF::NIL) and (crc IS Self->CRC)) return ERR::Okay;

   if (Self->Path) {
      objFile::create file = {
         fl::Path(Self->Path), fl::Flags(FL::WRITE|FL::NEW), fl::Permissions(PERMIT::NIL)
      };

      if (file.ok()) {
         if (Self->saveToObject(*file) IS ERR::Okay) Self->CRC = crc;
         return ERR::Okay;
      }
      else return ERR::File;
   }
   else return ERR::MissingPath;
}

/*********************************************************************************************************************
-ACTION-
SaveToObject: Saves configuration data to an object, using standard config text format.
-END-
*********************************************************************************************************************/

static ERR CONFIG_SaveToObject(extConfig *Self, struct acSaveToObject *Args)
{
   pf::Log log;

   log.msg("Saving %d groups to object #%d.", (LONG)Self->Groups->size(), Args->Dest->UID);

   ConfigGroups &groups = Self->Groups[0];
   for (auto& [group, keys] : groups) {
      std::string out_group("\n[" + group + "]\n");
      acWrite(Args->Dest, out_group.c_str(), out_group.size(), NULL);

      for (auto& [k, v] : keys) {
         std::string kv(k + " = " + v + "\n");
         acWrite(Args->Dest, kv.c_str(), kv.size(), NULL);
      }
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
Set: Sets keys in existing config groups (aborts if the group does not exist).

This method is identical to #WriteValue() except it will abort if the name of the referred group does not exist in the
config object.  The error code `ERR::Search` is returned if this is the case.  Please refer to #WriteValue() for further
information on the behaviour of this function.

-INPUT-
cstr Group: The name of the group.  Wildcards are supported.
cstr Key:  The name of the key.
cstr Data: The data that will be added to the given group/key.

-ERRORS-
Okay
NullArgs
Search: The referred group does not exist.
-END-

*********************************************************************************************************************/

static ERR CONFIG_Set(extConfig *Self, struct cfgSet *Args)
{
   if (!Args) return ERR::NullArgs;
   if ((!Args->Group) or (!Args->Group[0])) return ERR::NullArgs;
   if ((!Args->Key) or (!Args->Key[0])) return ERR::NullArgs;

   auto group = find_group_wild(Self, Args->Group);
   if (group) return Action(MT_CfgWriteValue, Self, Args);
   else return ERR::Search;
}

/*********************************************************************************************************************

-METHOD-
SortByKey: Sorts config data using a sequence of sort instructions.

The SortByKey() method sorts the groups of a config object by key values (the named key value should be present in 
every group).

-INPUT-
cstr Key: The name of the key to sort on.
int Descending: Set to true if a descending sort is required.

-ERRORS-
Okay
NoData
-END-

*********************************************************************************************************************/

static ERR CONFIG_SortByKey(extConfig *Self, struct cfgSortByKey *Args)
{
   if ((!Args) or (!Args->Key)) { // Sort by group name if no args provided.
      std::sort(Self->Groups->begin(), Self->Groups->end(),
         [](const ConfigGroup &a, const ConfigGroup &b ) {
         return a.first < b.first;
      });

      return ERR::Okay;
   }

   pf::Log log;

   log.branch("Key: %s, Descending: %d", Args->Key, Args->Descending);

   if (Args->Descending) {
      std::sort(Self->Groups->begin(), Self->Groups->end(),
         [Args](std::pair<std::string, std::map<std::string, std::string>> &a,
                std::pair<std::string, std::map<std::string, std::string>> &b ) {
         return a.second[Args->Key] > b.second[Args->Key];
      });
   }
   else {
      std::sort(Self->Groups->begin(), Self->Groups->end(),
         [Args](std::pair<std::string, std::map<std::string, std::string>> &a,
                std::pair<std::string, std::map<std::string, std::string>> &b ) {
         return a.second[Args->Key] < b.second[Args->Key];
      });
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
WriteValue: Adds new entries to config objects.

Use the WriteValue() method to add or update information in a config object.  A `Group` name, `Key` name, and `Data` 
value are required.  If the `Group` and `Key` arguments match an existing entry in the config object, the data of 
that entry will be replaced with the new Data value.

The `Group` string may refer to an index if the hash `#` character is used to precede a target index number.

-INPUT-
cstr Group: The name of the group.
cstr Key:   The name of the key.
cstr Data:  The data that will be added to the given group/key.

-ERRORS-
Okay
NullArgs
Args
AllocMemory: The additional memory required for the new entry could not be allocated.
-END-

*********************************************************************************************************************/

static ERR CONFIG_WriteValue(extConfig *Self, struct cfgWriteValue *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Group) or (!Args->Key)) return log.warning(ERR::NullArgs);
   if ((!Args->Group[0]) or (!Args->Key[0])) return log.warning(ERR::EmptyString);

   log.trace("%s.%s = %s", Args->Group, Args->Key, Args->Data);

   // Check if the named group already exists

   ConfigGroups &groups = *Self->Groups;
   for (auto& [group, keys] : groups) {
      if (!group.compare(Args->Group)) {
         keys[Args->Key] = Args->Data;
         return ERR::Okay;
      }
   }

   auto &new_group = Self->Groups->emplace_back();
   new_group.first.assign(Args->Group);
   new_group.second[Args->Key].assign(Args->Data);
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Data: Reference to the raw data values.

This field points to a C++ type that contains all key-values for the config object.  It is intended to be used only 
by system code that is included with the standard framework.

*********************************************************************************************************************/

static ERR GET_Data(extConfig *Self, ConfigGroups **Value)
{
   *Value = Self->Groups;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Flags: Optional flags may be set here.

-FIELD-
KeyFilter: Set this field to enable key filtering.

When dealing with large configuration files it may be useful to filter out groups of key-values that are not needed.
The KeyFilter field allows simple filters to be defined that will perform this task for you.  It is recommended that it
is set prior to parsing new data for best performance, but can be set or changed at any time to apply a new filter.

Key filters are created in the format `[Key] = [Data1], [Data2], ...`.  For example:

<pre>
Group = Sun, Light
Path = documents:
Name = Parasol
</pre>

Filters can be inversed by prefixing the key with the `!` character.

To create a filter based on group names, refer to the #GroupFilter field.

*********************************************************************************************************************/

static ERR GET_KeyFilter(extConfig *Self, CSTRING *Value)
{
   if (Self->KeyFilter) {
      *Value = Self->KeyFilter;
      return ERR::Okay;
   }
   else {
      *Value = NULL;
      return ERR::FieldNotSet;
   }
}

static ERR SET_KeyFilter(extConfig *Self, CSTRING Value)
{
   if (Self->KeyFilter) { FreeResource(Self->KeyFilter); Self->KeyFilter = NULL; }

   if ((Value) and (*Value)) {
      if (!(Self->KeyFilter = StrClone(Value))) return ERR::AllocMemory;
   }

   if (Self->initialised()) apply_key_filter(Self, Self->KeyFilter);

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
GroupFilter: Set this field to enable group filtering.

When dealing with large configuration files, filtering out unrelated data may be useful.  By setting the GroupFilter
field, it is possible to filter out entire groups that don't match the criteria.

Group filters are created in CSV format, i.e. `GroupA, GroupB, GroupC, ...`.

The filter can be reversed so that only the groups matching your criteria are filtered out.  To do this, prefix the
CSV list with the `!` character.

To create a filter based on key names, refer to the #KeyFilter field.

*********************************************************************************************************************/

static ERR GET_GroupFilter(extConfig *Self, CSTRING *Value)
{
   if (Self->GroupFilter) {
      *Value = Self->GroupFilter;
      return ERR::Okay;
   }
   else {
      *Value = NULL;
      return ERR::FieldNotSet;
   }
}

static ERR SET_GroupFilter(extConfig *Self, CSTRING Value)
{
   if (Self->GroupFilter) { FreeResource(Self->GroupFilter); Self->GroupFilter = NULL; }

   if ((Value) and (*Value)) {
      if (!(Self->GroupFilter = StrClone(Value))) return ERR::AllocMemory;
   }

   if (Self->initialised()) apply_group_filter(Self, Self->GroupFilter);

   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
Path: Set this field to the location of the source configuration file.

*********************************************************************************************************************/

static ERR SET_Path(extConfig *Self, CSTRING Value)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }

   if ((Value) and (*Value)) {
      if (!(Self->Path = StrClone(Value))) return ERR::AllocMemory;
   }

   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
TotalGroups: Returns the total number of groups in a config object.

*********************************************************************************************************************/

static ERR GET_TotalGroups(extConfig *Self, LONG *Value)
{
   *Value = Self->Groups->size();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
TotalKeys: The total number of key values loaded into the config object.
-END-

*********************************************************************************************************************/

static ERR GET_TotalKeys(extConfig *Self, LONG *Value)
{
   LONG total = 0;
   for (const auto& [group, keys] : Self->Groups[0]) {
      total += keys.size();
   }
   *Value = total;
   return ERR::Okay;
}

//********************************************************************************************************************
// Checks the next line in a buffer to see if it is a valid key.

static bool check_for_key(CSTRING Data)
{
   if ((*Data != '\n') and (*Data != '\r') and (*Data != '[') and (*Data != '#')) {
      while ((*Data) and (*Data != '\n') and (*Data != '\r') and (*Data != '=')) Data++; // Skip key name
      if (*Data != '=') return FALSE;
      Data++;
      while ((*Data) and (*Data != '\n') and (*Data <= 0x20)) Data++; // Skip whitespace
//      if ((*Data IS '\n') or (*Data IS 0)) return FALSE; // Check if data is present or the line terminates prematurely
      return true;
   }

   return false;
}

//********************************************************************************************************************

void merge_groups(ConfigGroups &Dest, ConfigGroups &Source)
{
   for (auto& [src_group, src_keys] : Source) {
      bool processed = false;

      // Check if the group already exists and merge the keys

      for (auto& [dest_group, dest_keys] : Dest) {
         if (!dest_group.compare(src_group)) {
            processed = true;
            for (auto& [k, v] : src_keys) {
               dest_keys[k] = v;
            }
         }
      }

      if (!processed) { // New group to be added
         auto &new_group = Dest.emplace_back();
         new_group.first  = src_group;
         new_group.second = src_keys;
      }
   }
}

//********************************************************************************************************************

static FilterConfig parse_filter(std::string Filter, bool KeyValue = false)
{
   FilterConfig f;

   LONG start = 0, end = 0;
   if (Filter[start] IS '!') {
      f.reverse = TRUE;
      start++;
   }
   else f.reverse = FALSE;

   for (; (Filter[start]) and (Filter[start] <= 0x20); start++);

   if (KeyValue) {
      for (end=start; Filter[end] != '='; end++);
      while ((end > start) and (Filter[end-1] <= 0x20)) end--;

      f.name = Filter.substr(start, end - start);
      start = end;

      while ((Filter[start]) and (Filter[start] <= 0x20)) start++;
      if (Filter[start] IS '=') start++;
      while ((Filter[start]) and (Filter[start] <= 0x20)) start++;
   }

   end = start;
   while (Filter[end]) {
      while ((Filter[end]) and (Filter[end] != ',')) end++;

      f.values.push_back(Filter.substr(start, end-start));

      if (Filter[end] IS ',') end++;
      while ((Filter[end]) and (Filter[end] <= 0x20)) end++;
      start = end;
   }

   return f;
}

//********************************************************************************************************************

static ERR parse_config(extConfig *Self, CSTRING Buffer)
{
   pf::Log log(__FUNCTION__);

   if (!Buffer) return ERR::NoData;

   log.traceBranch("%.20s", Buffer);

   std::string group_name;
   auto data = next_group(Buffer, group_name); // Find the first group

   while ((data) and (*data)) {
      while ((*data) and (*data <= 0x20)) data++;
      if (*data IS '#') { // Commented
         data = next_line(data);
         continue;
      }

      std::pair<std::string, std::map<std::string, std::string>> *current_group = NULL;
      while ((*data) and (*data != '[')) { // Keep processing keys until either a new group or EOF is reached
         if (check_for_key(data)) {
            std::string key, value;

            LONG len;
            for (len=0; (data[len]) and (data[len] != '='); len++);
            if (!data[len]) break;
            while (data[len-1] <= 0x20) len--;
            key.assign(data, 0, len);
            data += len;

            while ((*data) and (*data != '=')) data++;
            if (*data) data++;
            while ((*data) and (*data <= 0x20)) data++; // Skip any leading whitespace, including new lines

            if (((Self->Flags & CNF::STRIP_QUOTES) != CNF::NIL) and (*data IS '"')) {
               data++;
               for (len=0; (data[len]) and (data[len] != '"'); len++);
               value.assign(data, 0, len);
               data += len;
            }
            else {
               for (len=0; (data[len]) and (data[len] != '\n') and (data[len] != '\r'); len++);
               value.assign(data, 0, len);
               data += len;
            }
            data = next_line(data);

            if (!current_group) { // Check if a matching group already exists before creating a new one
               current_group = find_group(Self, group_name);
               if (!current_group) {
                  current_group = &Self->Groups->emplace_back();
                  current_group->first = group_name;
               }
            }
            current_group->second[key] = value;
         }
         else data = next_line(data);
      }
      data = next_group(data, group_name);
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static void apply_key_filter(extConfig *Self, CSTRING Filter)
{
   pf::Log log(__FUNCTION__);

   if ((!Filter) or (!Filter[0])) return;

   log.branch("Filter: %s", Filter);

   FilterConfig f = parse_filter(Filter, true);
   for (auto group = Self->Groups->begin(); group != Self->Groups->end(); ) {
      bool matched = (f.reverse) ? true : false;
      for (auto & [k, v] : group->second) {
         if (StrMatch(f.name.c_str(), k.c_str()) IS ERR::Okay) {
            for (auto const &cmp : f.values) {
               if (StrMatch(cmp.c_str(), v.c_str()) IS ERR::Okay) {
                  matched = f.reverse ? false : true;
                  break;
               }
            }
            break;
         }
      }

      if (!matched) group = Self->Groups->erase(group);
      else group++;
   }
}

//********************************************************************************************************************

static void apply_group_filter(extConfig *Self, CSTRING Filter)
{
   pf::Log log(__FUNCTION__);

   if ((!Filter) or (!Filter[0])) return;

   log.branch("Filter: %s", Filter);

   FilterConfig f = parse_filter(Filter, false);
   for (auto group = Self->Groups->begin(); group != Self->Groups->end(); ) {
      bool matched = (f.reverse) ? true : false;
      for (auto const &cmp : f.values) {
         if (!cmp.compare(group->first)) {
            matched = f.reverse ? false : true;
            break;
         }
      }

      if (!matched) group = Self->Groups->erase(group);
      else group++;
   }
}

//********************************************************************************************************************
// Returns the key-values for a group, given a group name.  Supports wild-cards.

static ConfigKeys * find_group_wild(extConfig *Self, CSTRING Group)
{
   if ((!Group) or (!*Group)) return NULL;

   for (auto & [group, keys] : Self->Groups[0]) {
      if (StrCompare(Group, group.c_str(), 0, STR::WILDCARD) IS ERR::Okay) return &keys;
   }

   return NULL;
}

//********************************************************************************************************************

#include "class_config_def.c"

static const FieldArray clFields[] = {
   { "Path",        FDF_STRING|FDF_RW, NULL, SET_Path },
   { "KeyFilter",   FDF_STRING|FDF_RW, GET_KeyFilter, SET_KeyFilter },
   { "GroupFilter", FDF_STRING|FDF_RW, GET_GroupFilter, SET_GroupFilter },
   { "Flags",       FDF_LONGFLAGS|FDF_RW, NULL, NULL, &clFlags },
   // Virtual fields
   { "Data",        FDF_POINTER|FDF_R, GET_Data },
   { "TotalGroups", FDF_LONG|FDF_R, GET_TotalGroups },
   { "TotalKeys",   FDF_LONG|FDF_R, GET_TotalKeys },
   END_FIELD
};

//********************************************************************************************************************

extern "C" ERR add_config_class(void)
{
   glConfigClass = extMetaClass::create::global(
      fl::BaseClassID(ID_CONFIG),
      fl::ClassVersion(VER_CONFIG),
      fl::Name("Config"),
      fl::Category(CCF::DATA),
      fl::FileExtension("*.cfg|*.cnf|*.config"),
      fl::FileDescription("Config File"),
      fl::Actions(clConfigActions),
      fl::Methods(clConfigMethods),
      fl::Fields(clFields),
      fl::Size(sizeof(extConfig)),
      fl::Path("modules:core"));

   return glConfigClass ? ERR::Okay : ERR::AddClass;
}

