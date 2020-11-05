/*****************************************************************************

The source code of the Parasol Framework is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
JSON: Extends the XML class with JSON support.

The JSON class is an extension for the @XML class.  It allows JSON data to be loaded into an XML tree, where
it can be manipulated and scanned using XML based functions.  This approach is advantageous in that the simplicity of
the JSON is maintained, yet advanced features such as XPath lookups can be used to inspect the data.

It is important to understand how JSON data is converted to the XML tree structure.  All JSON values will be represented
as 'item' tags that describe the name and type of value that is being represented.  Each value will be stored as content
in the corresponding item tag.  Arrays are stored as items that contain a series of value tags, in the case of strings
and numbers, or object tags.

-EXAMPLE-
The following example illustrates a JSON structure containing the common datatypes:

{ "string":"foo bar",
  "array":[ 0, 1, 2 ],
  "array2":[ { "ABC":"XYZ" },
             { "DEF":"XYZ" } ]
}

It will be translated to the following when loaded into an XML object:

&lt;item type="object"&gt;
  &lt;item name="string" type="string"&gt;foo bar&lt;/item&gt;

  &lt;item name="array" type="array" subtype="integer"&gt;
    &lt;value&gt;0&lt;/value&gt;
    &lt;value&gt;1&lt;/value&gt;
    &lt;value&gt;2&lt;/value&gt;
  &lt;/item&gt;

  &lt;item name="array2" type="array" subtype="object"&gt;
    &lt;item type="object"&gt;&lt;item name="ABC" type="string" value="XYZ"/&gt;&lt;/item&gt;
    &lt;item type="object"&gt;&lt;item name="DEF" type="string" value="XYZ"/&gt;&lt;/item&gt;
  &lt;/item&gt;
&lt;item&gt;

-END-

*****************************************************************************/

#undef DEBUG
//#define DEBUG

#define PRV_XML
#include <parasol/main.h>
#include <parasol/modules/xml.h>

MODULE_COREBASE;
static OBJECTPTR clJSON = NULL;
static APTR glTmp = NULL;
static LONG glTmpSize = 0;

static ERROR JSON_Init(objXML *, APTR);
static ERROR JSON_SaveToObject(objXML *, struct acSaveToObject *);

static UWORD glTagID = 1;

static struct ActionArray clActions[] = {
   { AC_Init,         JSON_Init },
   { AC_SaveToObject, JSON_SaveToObject },
   { 0, NULL }
};

struct exttag {
   CSTRING Start;
   LONG   TagIndex;
   LONG   Branch;
};

static ERROR load_file(objXML *, CSTRING);
static ERROR txt_to_json(objXML *, CSTRING Statement);
static ERROR extract_item(objXML *, struct exttag *, CSTRING *);
static ERROR create_tag(objXML *, LONG LineNo, struct exttag *, ...);
static ERROR next_item(objXML *, struct exttag *, CSTRING *);

//****************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   return(CreateObject(ID_METACLASS, 0, &clJSON,
      FID_BaseClassID|TLONG,    ID_XML,
      FID_SubClassID|TLONG,     ID_JSON,
      FID_Name|TSTRING,         "JSON",
      FID_Category|TLONG,       CCF_DATA,
      FID_FileExtension|TSTR,   "*.json",
      FID_FileDescription|TSTR, "JSON Data",
      FID_Actions|TPTR,         clActions,
      FID_Path|TSTR,            "classes/data/json",
      TAGEND));
}

ERROR CMDExpunge(void)
{
   if (clJSON) { acFree(clJSON); clJSON = NULL; }
   if (glTmp)  { FreeResource(glTmp); glTmp = NULL; }
   return ERR_Okay;
}

//****************************************************************************

static APTR get_tmp(LONG Size)
{
   if (Size < 1024) Size = 1024;

   if ((glTmp) AND (Size <= glTmpSize)) return glTmp;

   if (glTmp) { FreeResource(glTmp); glTmp = NULL; }

   if (!AllocMemory(Size, MEM_DATA|MEM_NO_CLEAR|MEM_UNTRACKED, &glTmp, NULL)) {
      glTmpSize = Size;
      return glTmp;
   }
   else return NULL;
}

//****************************************************************************

static void free_tags(objXML *Self)
{
   LONG i;
   for (i=0; (i < Self->TagCount) AND (Self->Tags[i]); i++) {
      FreeResource(Self->Tags[i]);
      Self->Tags[i] = NULL;
   }

   FreeResource(Self->Tags);
   Self->Tags = NULL;
   Self->TagCount = 0;
}

/*****************************************************************************
** Debug routines.
*/

#if defined(DEBUG)

static void debug_tree(objXML *Self)
{
   struct XMLTag *Tag;
   LONG i, j, index;
   UBYTE buffer[1000];

   for (index=0; index < Self->TagCount; index++) {
      Tag = Self->Tags[index];

      // Indenting

      for (i=0; i < Tag->Branch; i++) buffer[i] = ' ';
      buffer[i] = 0;

      if (Tag->Attrib) {
         if (Tag->Attrib->Name) {
            LogF("Tree","%.3d/%.3d: %p<-%p->%p Child %p %s%s", index, Tag->Index, Tag->Prev, Tag, Tag->Next, Tag->Child, buffer, Tag->Attrib->Name ? Tag->Attrib->Name : "Content");
         }
         else {
            // Extract a limited amount of content
            for (j=0; (Tag->Attrib->Value[j]) AND (j < 16) AND (i < sizeof(buffer)); j++) {
               if (Tag->Attrib->Value[j] IS '\n') buffer[i++] = '.';
               else buffer[i++] = Tag->Attrib->Value[j];
            }
            if (i) buffer[i] = 0;
            else StrCopy("<Empty Content>", buffer, sizeof(buffer));
            LogF("Tree","%.3d/%.3d: %p<-%p->%p Child %p %s", index, Tag->Index, Tag->Prev, Tag, Tag->Next, Tag->Child, buffer);
            //LogMsg("%.3d: %s", index, buffer);
         }
      }
   }
}

#endif

//****************************************************************************

static ERROR JSON_Init(objXML *Self, APTR Void)
{
   STRING location, statement;

   MSG("Attempting JSON interpretation of source data.");

   if ((!GetString(Self, FID_Statement, &statement)) AND (statement)) {
      if ((Self->ParseError = txt_to_json(Self, statement))) {
         LogErrorMsg("JSON Parsing Error: %s", GetErrorMsg(Self->ParseError));
         free_tags(Self);
      }

      #ifdef DEBUG
      debug_tree(Self);
      #endif

      FreeResource(statement);
      return Self->ParseError;
   }

   GetString(Self, FID_Path, &location);
   if ((!location) OR (Self->Flags & XMF_NEW)) {
      // If no location has been specified, assume that the JSON source is being
      // created from scratch (e.g. to save to disk).

      return ERR_Okay;
   }
   else {
      if ((Self->ParseError = load_file(Self, location))) {
         LogErrorMsg("Parsing Error: %s [File: %s]", GetErrorMsg(Self->ParseError), location);
         free_tags(Self);
         return Self->ParseError;
      }
      else return ERR_Okay;
   }

   return ERR_NoSupport;
}

//****************************************************************************

static ERROR JSON_SaveToObject(objXML *Self, struct acSaveToObject *Args)
{
   if (!Args) return PostError(ERR_NullArgs);



   return ERR_Okay;
}

//****************************************************************************

static ERROR txt_to_json(objXML *Self, CSTRING Text)
{
   struct XMLTag **tag, *prevtag;
   struct exttag ext;
   CSTRING str;
   LONG i, j;

   if ((!Self) OR (!Text)) return ERR_NullArgs;

   FMSG("~txt_to_json","");

   Self->LineNo  = 1;
   Self->TagCount = 500;
   for (str=Text; (*str) AND (*str != '{'); str++) if (*str IS '\n') Self->LineNo++;
   if (str[0] != '{') {
      LogErrorMsg("There is no JSON statement to process.");
      LOGRETURN();
      return ERR_NoData;
   }

   // Allocate an array to hold all of the tags

   if (!AllocMemory(sizeof(APTR) * (Self->TagCount + 1), MEM_DATA|MEM_UNTRACKED, &tag, NULL)) {
      FreeResource(Self->Tags);
      Self->Tags = tag;
   }
   else {
      LOGRETURN();
      return ERR_AllocMemory;
   }

   MSG("Extracting tag information with extract_tag()");

   ext.Start    = Text;
   ext.TagIndex = 0;
   ext.Branch   = 0;
   prevtag      = NULL;
   for (str=Text; (*str) AND (*str != '{'); str++) if (*str IS '\n') Self->LineNo++;
   if (*str IS '{') {
      create_tag(Self, Self->LineNo, &ext, "item", "type", "object", TAGEND);

      str++; // Skip '{'
      while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }

      ext.Branch++;

      do {
         i = ext.TagIndex; // Remember the current tag index before extract_item() changes it

         if (extract_item(Self, &ext, &str) != ERR_Okay) {
            LogErrorMsg("Aborting parsing of JSON statement.");
            LOGRETURN();
            return ERR_Syntax;
         }

         if (prevtag) prevtag->Next = Self->Tags[i];
         prevtag = Self->Tags[i];
      } while (!next_item(Self, &ext, &str));

      ext.Branch--;

      Self->Tags[0]->Child = Self->Tags[1];
   }

   if (*str != '}') {
      LogErrorMsg("Missing expected '}' terminator at line %d.", Self->LineNo);
      LOGRETURN();
      return ERR_Syntax;
   }

   FMSG("txt_to_json","%d values successfully extracted.", ext.TagIndex);

   // Reallocate the tag array if it has an excess of allocated memory

   if (Self->TagCount - ext.TagIndex > 50) {
      ReallocMemory(Self->Tags, sizeof(APTR) * (ext.TagIndex + 1), &Self->Tags, NULL);
   }
   Self->TagCount = ext.TagIndex;

   // Set the Prev and Index fields

   for (i=0; i < Self->TagCount; i++) {
      Self->Tags[i]->Index = i;
      if (Self->Tags[i]->Next) Self->Tags[i]->Next->Prev = Self->Tags[i];
   }

   // Upper/lowercase transformations

   if (Self->Flags & XMF_UPPER_CASE) {
      FMSG("txt_to_json","Performing uppercase translations.");
      STRING str;
      for (i=0; i < Self->TagCount; i++) {
         for (j=0; j < Self->Tags[i]->TotalAttrib; j++) {
            if ((str = Self->Tags[i]->Attrib[j].Name)) {
               while (*str) { if ((*str >= 'a') AND (*str <= 'z')) *str = *str - 'a' + 'A'; str++; }
            }
            if ((str = Self->Tags[i]->Attrib[j].Value)) {
               while (*str) { if ((*str >= 'a') AND (*str <= 'z')) *str = *str - 'a' + 'A'; str++; }
            }
         }
      }
   }
   else if (Self->Flags & XMF_LOWER_CASE) {
      FMSG("txt_to_json","Performing lowercase translations.");
      STRING str;
      for (i=0; i < Self->TagCount; i++) {
         for (j=0; j < Self->Tags[i]->TotalAttrib; j++) {
            if ((str = Self->Tags[i]->Attrib[j].Name)) {
               while (*str) { if ((*str >= 'A') AND (*str <= 'Z')) *str = *str - 'A' + 'a'; str++; }
            }
            if ((str = Self->Tags[i]->Attrib[j].Value)) {
               while (*str) { if ((*str >= 'A') AND (*str <= 'Z')) *str = *str - 'A' + 'a'; str++; }
            }
         }
      }
   }

   FMSG("txt_to_json","JSON parsing complete.");

   LOGRETURN();
   return ERR_Okay;
}

//****************************************************************************
// Called by txt_to_json() to extract the next item from a JSON string.  This function also recurses into itself.

static ERROR extract_item(objXML *Self, struct exttag *Status, CSTRING *Input)
{
   struct XMLTag *prevtag;
   LONG currentindex, i, line_no;
   CSTRING str;
   char item_name[80];

   FMSG("~extract_item()","Index: %d, Line: %d, %.20s", Status->TagIndex, Self->LineNo, *Input);

   // Array management

   if (Status->TagIndex >= Self->TagCount) {
      if (ReallocMemory(Self->Tags, sizeof(APTR) * (Self->TagCount + 250 + 1), &Self->Tags, NULL)) {
         LOGRETURN();
         return PostError(ERR_ReallocMemory);
      }
      Self->TagCount += 250;
   }

   str = Input[0];
   if (*str != '"') {
      LogErrorMsg("Malformed JSON statement detected at line %d, expected '\"', got '%c'.", Self->LineNo, str[0]);
      LOGRETURN();
      return ERR_Syntax;
   }

   currentindex = Status->TagIndex;
   prevtag = NULL;
   line_no = Self->LineNo;
   str++;
   i = 0;
   while ((*str != '"') AND (i < sizeof(item_name)-1)) {
      if (*str IS '\\') {
         str++;
         if (*str IS 'n') item_name[i++] = '\n';
         else if (*str IS 'r') item_name[i++] = '\r';
         else if (*str IS 't') item_name[i++] = '\t';
         else if (*str IS '"') item_name[i++] = '"';
         else {
            LogErrorMsg("Invalid use of back-slash in item name encountered at line %d", Self->LineNo);
            LOGRETURN();
            return ERR_Syntax;
         }
      }
      else if (*str < 0x20) {
         LogErrorMsg("Invalid item name encountered at line %d.", Self->LineNo);
         LOGRETURN();
         return ERR_Syntax;
      }
      else item_name[i++] = *str++;
   }
   item_name[i] = 0;
   if (i >= sizeof(item_name)) {
      LOGRETURN();
      return ERR_BufferOverflow;
   }

   if (*str IS '"') str++;
   else {
      LOGRETURN();
      return ERR_Syntax;
   }

   while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }

   if (*str != ':') {
      LogErrorMsg("Missing separator ':' after item name '%s' at line %d.", item_name, Self->LineNo);
      LOGRETURN();
      return ERR_Syntax;
   }

   str++; // Skip ':'
   while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }

   if (*str IS '[') {
      STRING subtype;
      LONG line_start = Self->LineNo;
      LONG array_index = Status->TagIndex;

      // Evaluates to:
      //
      //    <item name="array" type="array" subtype="type">
      //      <value>val</value>
      //      ...
      //    </item>
      //
      // Except for JSON arrays:
      //
      //    <item name="array" type="array" subtype="object">
      //      <object>...</object>
      //      ...
      //    </item>

      str++; // Skip '['
      while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }

      // Figure out what type of array this is

      if (*str IS '{') subtype = "object";
      else if (*str IS '"') subtype = "string";
      else if ((*str >= '0') AND (*str <= '9')) subtype = "integer";
      else if (*str IS ']') subtype = "null";
      else {
         LogErrorMsg("Invalid array defined at line %d.", line_start);
         LOGRETURN();
         return ERR_Syntax;
      }

      MSG("Processing %s array at line %d.", subtype, Self->LineNo);

      create_tag(Self, line_no, Status, "item", "name", item_name, "type", "array", "subtype", subtype, TAGEND);
      if (prevtag) prevtag->Next = Self->Tags[array_index];
      prevtag = Self->Tags[array_index];

      // Read the array values

      Status->Branch++;

      if (*str IS '{') {
         struct XMLTag *prevobject = NULL;
         while ((*str) AND (*str != ']')) {
            // Evaluates to: <object>...</object>
            // Which is handled entirely in the call to extract_item()

            ERROR error;

            LONG object_index = Status->TagIndex; // Remember the tag index of <object>

            create_tag(Self, Self->LineNo, Status, "item", "type", "object", TAGEND); // This is the object container tag.
            if (prevobject) prevobject->Next = Self->Tags[object_index];
            prevobject = Self->Tags[object_index];

            if (!Self->Tags[array_index]->Child) Self->Tags[array_index]->Child = Self->Tags[object_index];

            if (*str IS '{') {
               MSG("Processing new object in array.");

               str++; // Skip '{'
               while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }

               if (*str != '}') { // Don't process content if the object is empty.
                  Status->Branch++;

                  if ((error = extract_item(Self, Status, &str)) != ERR_Okay) {
                     LOGRETURN();
                     return error;
                  }

                  Status->Branch--;

                  Self->Tags[object_index]->Child = Self->Tags[object_index+1];

                  while ((*str) AND (*str != '}')) { if (*str IS '\n') Self->LineNo++; str++; } // Skip content/whitespace to get to the next tag.

                  if (*str != '}') {
                     LogErrorMsg("Missing '}' character to close an object by the end of line %d.", Self->LineNo);
                     LOGRETURN();
                     return ERR_Syntax;
                  }

                  // Go to next value, or end of array

                  MSG("End of object array reached.");

                  str++; // Skip '}'
                  while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
                  if (*str IS ',') {
                     str++;
                     while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
                  }
               }
               else {
                  LogErrorMsg("Invalid array entry encountered at line %d, expected object, encountered character '%c'.", Self->LineNo, *str);
                  LOGRETURN();
                  return ERR_Syntax;
               }
            }
         }
      }
      else if (*str IS '"') {
         LONG val_prev = 0;

         while ((*str) AND (*str != ']')) {
            if (*str != '"') {
               LogErrorMsg("Invalid array of strings at line %d.", line_start);
               LOGRETURN();
               return ERR_Syntax;
            }

            str++; // Skip '"'

            LONG len = 0;
            while ((str[len]) AND (str[len] != '"')) {
               if ((str[len] IS '\\') AND (str[len+1] IS '"')) len++;
               len++;
            }

            STRING buffer;
            if ((buffer = get_tmp(len+1))) {
               i = 0;
               while ((*str) AND (*str != '"')) {
                  if (*str IS '\\') {
                     str++;
                     if (*str IS 'n') buffer[i++] = '\n';
                     else if (*str IS 'r') buffer[i++] = '\r';
                     else if (*str IS 't') buffer[i++] = '\t';
                     else if (*str IS '"') buffer[i++] = '"';
                     else { buffer[i++] = '\\'; buffer[i++] = *str; }
                     str++;
                  }
                  else buffer[i++] = *str++;
               }
               buffer[i] = 0;

               // Insert two XML tags to create <value>string</value>

               LONG val_index = Status->TagIndex;

               create_tag(Self, Self->LineNo, Status, "value", TAGEND);
               if (!Self->Tags[array_index]->Child) Self->Tags[array_index]->Child = Self->Tags[val_index];

               Status->Branch++;
               create_tag(Self, Self->LineNo, Status, "", buffer, TAGEND);
               Status->Branch--;

               str++; // Skip terminating '"'

               // Link the <value> tag to the child content, and the previous <value> tag
               // (if any) to the newly inserted one.

               Self->Tags[val_index]->Child = Self->Tags[val_index + 1];
               if (val_prev) Self->Tags[val_prev]->Next = Self->Tags[val_index];
               val_prev = val_index;

               // Go to next value, or end of array

               while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
               if (*str IS ',') str++;
               while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
            }
            else {
               LOGRETURN();
               return ERR_AllocMemory;
            }
         }
      }
      else if ((str[0] IS '0') AND (str[1] IS 'x')) {
         // Hexadecimal number.

         LONG val_prev = 0;
         char numbuf[80];

         while ((*str) AND (*str != ']')) {
            if ((str[0] != '0') OR (str[1] != 'x')) {
               LogErrorMsg("Invalid array of hexadecimal numbers at line %d.", line_start);
               LOGRETURN();
               return ERR_Syntax;
            }

            i = 0;
            numbuf[i++] = *str++; // '0'
            numbuf[i++] = *str++; // 'x'
            while ((i < sizeof(numbuf)-1) AND (
               ((*str >= '0') AND (*str <= '9')) OR
               ((str[2] >= 'A') AND (str[2] <= 'F')) OR
               ((str[2] >= 'a') AND (str[2] <= 'f'))
            )) numbuf[i++] = *str++;
            if (i >= sizeof(numbuf)-1) { LOGRETURN(); return ERR_BufferOverflow; }
            numbuf[i] = 0;

            while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }

            if ((*str != ',') AND (*str != ']')) { // If the next character is something other than ',' or ']' then it indicates that the hex value has an invalid character in it, e.g. 0x939fW
               LogErrorMsg("Invalid array of hexadecimal numbers at line %d.", line_start);
               LOGRETURN();
               return ERR_Syntax;
            }

            // Insert two XML tags to create <value>number</value>

            LONG val_index = Status->TagIndex;

            create_tag(Self, Self->LineNo, Status, "value", TAGEND);
            if (!Self->Tags[array_index]->Child) Self->Tags[array_index]->Child = Self->Tags[val_index];

            Status->Branch++;
            create_tag(Self, Self->LineNo, Status, "", numbuf, TAGEND);
            Status->Branch--;

            // Link the <value> tag to the child content, and the previous <value> tag
            // (if any) to the newly inserted one.

            Self->Tags[val_index]->Child = Self->Tags[val_index + 1];
            if (val_prev) Self->Tags[val_prev]->Next = Self->Tags[val_index];
            val_prev = val_index;

            // Go to next value, or end of array

            if (*str IS ',') str++;
            while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
         }
      }
      else if (((*str >= '0') AND (*str <= '9')) OR (*str IS '-')) {
         LONG val_prev = 0;

         while ((*str) AND (*str != ']')) {
            if (((*str < '0') OR (*str > '9')) AND (*str != '-')) {
               LogErrorMsg("Invalid array of integers at line %d.", Self->LineNo);
               LOGRETURN();
               return ERR_Syntax;
            }

            char numbuf[80];
            for (i=0; i < sizeof(numbuf)-1; i++) {
               if ((*str IS '-') OR (*str IS '.') OR ((*str >= '0') AND (*str <= '9'))) {
                  numbuf[i] = *str++;
               }
               else break;
            }
            numbuf[i] = 0;

            // Insert two XML tags to create <value>number</value>

            LONG val_index = Status->TagIndex;

            create_tag(Self, Self->LineNo, Status, "value", TAGEND);
            if (!Self->Tags[array_index]->Child) Self->Tags[array_index]->Child = Self->Tags[val_index];

            Status->Branch++;
            create_tag(Self, Self->LineNo, Status, "", numbuf, TAGEND);
            Status->Branch--;

            // Link the <value> tag to the child content, and the previous <value> tag
            // (if any) to the newly inserted one.

            Self->Tags[val_index]->Child = Self->Tags[val_index + 1];
            if (val_prev) Self->Tags[val_prev]->Next = Self->Tags[val_index];
            val_prev = val_index;

            // Go to next value, or end of array

            while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
            if (*str IS ',') str++;
            while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
         }
      }
      else if (*str IS ']') {

      }
      else {
         LogErrorMsg("Invalid array defined at line %d.", line_start);
         LOGRETURN();
         return ERR_Syntax;
      }

      Status->Branch--;

      if (*str != ']') {
         LogErrorMsg("Array at line %d not terminated with expected ']' character.", line_start);
         LOGRETURN();
         return ERR_Syntax;
      }
      else str++; // Skip array terminator ']'
   }
   else if (*str IS '{') {
      // Evaluates to: <object>...</object>
      // Which is handled entirely in the call to extract_item()

      MSG("Item '%s' is an object.", item_name);

      LONG object_index = Status->TagIndex; // Remember the tag index of <object>

      create_tag(Self, Self->LineNo, Status, "item", "name", item_name, "type", "object", TAGEND); // This is the object container tag.
      if (prevtag) prevtag->Next = Self->Tags[object_index];
      prevtag = Self->Tags[object_index];

      str++; // Skip '{'
      while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; } // Skip content/whitespace to get to the next tag.

      if (*str != '}') {
         Status->Branch++;

         prevtag = NULL;
         do {
            i = Status->TagIndex; // Remember the current tag index before extract_item() changes it

            if (extract_item(Self, Status, &str) != ERR_Okay) {
               LogErrorMsg("Aborting parsing of JSON statement.");
               LOGRETURN();
               return ERR_Syntax;
            }

            if (prevtag) prevtag->Next = Self->Tags[i];
            prevtag = Self->Tags[i];
         } while (!next_item(Self, Status, &str));

         Status->Branch--;

         Self->Tags[object_index]->Child = Self->Tags[object_index+1];

         while ((*str) AND (*str != '}')) { if (*str IS '\n') Self->LineNo++; str++; } // Skip content/whitespace to get to the next tag.

         if (*str != '}') {
            LogErrorMsg("Missing '}' character to close one of the objects.");
            LOGRETURN();
            return ERR_Syntax;
         }
         else str++; // Skip '}'
      }
      else MSG("The object is empty.");
   }
   else if (*str IS '"') {
      // Evaluates to: <item name="item_name" type="string">string</item>

      MSG("Item '%s' is a string.", item_name);

      str++; // Skip '"'

      LONG item_index = Status->TagIndex; // Remember the tag index of <item>
      create_tag(Self, Self->LineNo, Status, "item", "name", item_name, "type", "string", TAGEND); // This is the object container tag.
      if (prevtag) prevtag->Next = Self->Tags[item_index];
      prevtag = Self->Tags[item_index];

      // Determine the maximum string length of the item value

      LONG len = 0;
      while ((str[len]) AND (str[len] != '"')) {
         if ((str[len] IS '\\') AND (str[len+1] IS '"')) len++;
         if (str[len] IS '\n') Self->LineNo++;
         len++;
      }

      if (!str[0]) {
         LogErrorMsg("Missing final '\"' terminator for string at line %d.", Self->LineNo);
         LOGRETURN();
         return ERR_Syntax;
      }

      if (len > 0) {
         // Extract the value and set the <item> tag's content

         STRING buffer;
         LONG pos = 0;
         if ((buffer = get_tmp(len+1))) {
            for (i=0; i < len; i++) {
               if (str[i] IS '\n') Self->LineNo++;
               if ((str[i] IS '\\') AND (i < len-1)) {
                  i++;
                  if (str[i] IS 'n') buffer[pos++] = '\n';
                  else if (str[i] IS 'r') buffer[pos++] = '\r';
                  else if (str[i] IS 't') buffer[pos++] = '\t';
                  else if (str[i] IS '"') buffer[pos++] = '"';
                  else { buffer[pos++] = '\\'; buffer[pos++] = str[i]; }
               }
               else buffer[pos++] = str[i];
            }
            buffer[pos] = 0;

            Status->Branch++;
            create_tag(Self, Self->LineNo, Status, "", buffer, TAGEND);
            Status->Branch--;
         }
         else {
            LOGRETURN();
            return ERR_AllocMemory;
         }

         Self->Tags[item_index]->Child = Self->Tags[item_index+1];
      }

      str += len + 1; // Skip value and '"'
   }
  else if ((str[0] IS '0') AND (str[1] IS 'x')) {
      // Evaluates to: <item name="item_name" type="integer">number</item>

      UBYTE numbuf[64];
      i = 0;
      numbuf[i++] = *str++; // '0'
      numbuf[i++] = *str++; // 'x'
      while ((i < sizeof(numbuf)-1) AND (
         ((*str >= '0') AND (*str <= '9')) OR
         ((*str >= 'A') AND (*str <= 'F')) OR
         ((*str >= 'a') AND (*str <= 'f'))
      )) numbuf[i++] = *str++;
      if (i >= sizeof(numbuf)-1) { LOGRETURN(); return ERR_BufferOverflow; }
      numbuf[i] = 0;

      // Skip whitespace and check that the number was valid.
      while (*str) {
         if (*str IS '\n') Self->LineNo++;
         else if (*str <= 0x20);
         else if (*str IS ',') break;
         else if (*str IS '}') break;
         else {
            LogErrorMsg("Invalid hexadecimal number '%s' at line %d", numbuf, Self->LineNo);
            LOGRETURN();
            return ERR_Syntax;
         }
         str++;
      }

      LONG item_index = Status->TagIndex; // Remember the tag index of <item>
      create_tag(Self, Self->LineNo, Status, "item", "name", item_name, "type", "number", TAGEND); // This is the object container tag.
      if (prevtag) prevtag->Next = Self->Tags[item_index];
      prevtag = Self->Tags[item_index];

      Status->Branch++;
      create_tag(Self, Self->LineNo, Status, "", numbuf, TAGEND);
      Status->Branch--;

      Self->Tags[item_index]->Child = Self->Tags[item_index+1];
   }
   else if (((*str >= '0') AND (*str <= '9')) OR
            ((*str IS '-') AND (str[1] >= '0') AND (str[1] <= '9'))) {
      // Evaluates to: <item name="item_name" type="integer">number</item>

      UBYTE numbuf[64];
      for (i=0; i < sizeof(numbuf)-1; i++) {
         if ((*str IS '-') OR (*str IS '.') OR ((*str >= '0') AND (*str <= '9'))) numbuf[i] = *str++;
         else break;
      }
      numbuf[i] = 0;

      // Skip whitespace and check that the number was valid.
      while (*str) {
         if (*str IS '\n') Self->LineNo++;
         else if (*str <= 0x20);
         else if (*str IS ',') break;
         else if (*str IS '}') break;
         else {
            LogErrorMsg("Invalid number at line %d", Self->LineNo);
            LOGRETURN();
            return ERR_Syntax;
         }
         str++;
      }

      LONG item_index = Status->TagIndex; // Remember the tag index of <item>
      create_tag(Self, Self->LineNo, Status, "item", "name", item_name, "type", "number", TAGEND); // This is the object container tag.
      if (prevtag) prevtag->Next = Self->Tags[item_index];
      prevtag = Self->Tags[item_index];

      Status->Branch++;
      create_tag(Self, Self->LineNo, Status, "", numbuf, TAGEND);
      Status->Branch--;

      Self->Tags[item_index]->Child = Self->Tags[item_index+1];
   }
   else if (!StrCompare("null", str, 4, 0)) {
      // Evaluates to <item name="item_name" type="null"/>

      str += 4;

      LONG item_index = Status->TagIndex;
      create_tag(Self, Self->LineNo, Status, "item", "name", item_name, "type", "null", TAGEND);
      if (prevtag) prevtag->Next = Self->Tags[item_index];
      prevtag = Self->Tags[item_index];
   }
   else {
      LogErrorMsg("Invalid value character '%c' encountered for item '%s' at line %d.", *str, item_name, Self->LineNo);
      LOGRETURN();
      return ERR_Syntax;
   }

   *Input = str;
   LOGRETURN();
   return ERR_Okay;
}

//****************************************************************************

static ERROR create_tag(objXML *Self, LONG LineNo, struct exttag *Status, ...)
{
   struct XMLTag *tag;
   STRING *tags;
   LONG totalattrib, attribsize, i, a;

   tags = (STRING *)&Status;
   tags++;

   attribsize = 0;
   for (totalattrib=0; tags[totalattrib] != (STRING)TAGEND; totalattrib++) {
      attribsize += StrLength(tags[totalattrib]) + 1;
   }
   totalattrib = (totalattrib + 1)>>1;

   FMSG("create_tag()","Attribs: %d, Line: %d", totalattrib, LineNo);

   if ((totalattrib < 1) OR (!attribsize)) return ERR_Args;

#ifdef DEBUG
   if (AllocMemory(sizeof(struct XMLTag) + Self->PrivateDataSize + (sizeof(struct XMLAttrib) * totalattrib) + attribsize, 0, &tag, NULL) != ERR_Okay) {  // Memory will be tracked in debug mode.
#else
   if (AllocMemory(sizeof(struct XMLTag) + Self->PrivateDataSize + (sizeof(struct XMLAttrib) * totalattrib) + attribsize, MEM_UNTRACKED, &tag, NULL) != ERR_Okay) {
#endif
      return PostError(ERR_AllocMemory);
   }

   tag->Private     = ((APTR)tag) + sizeof(struct XMLTag);
   tag->Attrib      = ((APTR)tag) + sizeof(struct XMLTag);
   tag->TotalAttrib = totalattrib;
   tag->AttribSize  = attribsize;
   tag->ID          = glTagID++;
   tag->Branch       = Status->Branch;
   tag->LineNo      = LineNo;

   Self->Tags[Status->TagIndex++] = tag;

   // Set the XML tag attributes

   STRING buffer = ((APTR)tag->Attrib) + (sizeof(struct XMLAttrib) * tag->TotalAttrib);
   if (tags[0][0]) {
      tag->Attrib[0].Name = buffer; // The first tag is the XML tag name, or NULL if it is content (identified by a NULL string)
      buffer += StrCopy(tags[0], buffer, COPY_ALL) + 1;
      a = 1;
      i = 1;
      while (tags[i] != (STRING)TAGEND) {
         tag->Attrib[a].Name  = buffer;
         buffer += StrCopy(tags[i++], buffer, COPY_ALL) + 1;
         tag->Attrib[a].Value = buffer;
         buffer += StrCopy(tags[i++], buffer, COPY_ALL) + 1;
         a++;
      }
   }
   else {
      tag->Attrib[0].Name = NULL;
      tag->Attrib[0].Value = buffer;
      buffer += StrCopy(tags[1], buffer, COPY_ALL) + 1;
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR load_file(objXML *Self, CSTRING Path)
{
   struct CacheFile *filecache;

   if (!(Self->ParseError = LoadFile(Self->Path, 0, &filecache))) {
      Self->ParseError = txt_to_json(Self, filecache->Data);
      UnloadFile(filecache);
      return Self->ParseError;
   }
   else return Self->ParseError;
}

//****************************************************************************

static ERROR next_item(objXML *Self, struct exttag *Status, CSTRING *Input)
{
   CSTRING str = *Input;
   while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
   if (*str IS ',') {
      str++;
      while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
      *Input = str;
      return ERR_Okay;
   }
   else {
      *Input = str;
      return ERR_Failed;
   }
}

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, 1.0)
