// Auto-generated by idl-c.fluid

static const struct FieldDef clXMLFlags[] = {
   { "WellFormed", 0x00000001 },
   { "IncludeComments", 0x00000002 },
   { "StripContent", 0x00000004 },
   { "LowerCase", 0x00000008 },
   { "UpperCase", 0x00000010 },
   { "Indent", 0x00000020 },
   { "Readable", 0x00000020 },
   { "LockRemove", 0x00000040 },
   { "StripHeaders", 0x00000080 },
   { "New", 0x00000100 },
   { "NoEscape", 0x00000200 },
   { "AllContent", 0x00000400 },
   { "ParseHTML", 0x00000800 },
   { "StripCDATA", 0x00001000 },
   { "Debug", 0x00002000 },
   { "ParseEntity", 0x00004000 },
   { "IncludeSiblings", (LONG)0x80000000 },
   { NULL, 0 }
};

FDEF maSetAttrib[] = { { "Index", FD_LONG }, { "Attrib", FD_LONG }, { "Name", FD_STR }, { "Value", FD_STR }, { 0, 0 } };
FDEF maGetString[] = { { "Index", FD_LONG }, { "Flags", FD_LONG }, { "Result", FD_STR|FD_ALLOC|FD_RESULT }, { 0, 0 } };
FDEF maInsertXML[] = { { "Index", FD_LONG }, { "Where", FD_LONG }, { "XML", FD_STR }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF maGetContent[] = { { "Index", FD_LONG }, { "Buffer", FD_BUFFER|FD_STR }, { "Length", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF maSort[] = { { "XPath", FD_STR }, { "Sort", FD_STR }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF maRemoveTag[] = { { "Index", FD_LONG }, { "Total", FD_LONG }, { 0, 0 } };
FDEF maMoveTags[] = { { "Index", FD_LONG }, { "Total", FD_LONG }, { "DestIndex", FD_LONG }, { "Where", FD_LONG }, { 0, 0 } };
FDEF maGetAttrib[] = { { "Index", FD_LONG }, { "Attrib", FD_STR }, { "Value", FD_STR|FD_RESULT }, { 0, 0 } };
FDEF maInsertXPath[] = { { "XPath", FD_STR }, { "Where", FD_LONG }, { "XML", FD_STR }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF maFindTag[] = { { "XPath", FD_STR }, { "Callback", FD_FUNCTIONPTR }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF maFilter[] = { { "XPath", FD_STR }, { 0, 0 } };
FDEF maSetRoot[] = { { "XPath", FD_STR }, { 0, 0 } };
FDEF maCount[] = { { "XPath", FD_STR }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF maInsertContent[] = { { "Index", FD_LONG }, { "Where", FD_LONG }, { "Content", FD_STR }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF maRemoveXPath[] = { { "XPath", FD_STR }, { "Total", FD_LONG }, { 0, 0 } };
FDEF maGetXPath[] = { { "Index", FD_LONG }, { "Result", FD_STR|FD_ALLOC|FD_RESULT }, { 0, 0 } };
FDEF maFindTagFromIndex[] = { { "XPath", FD_STR }, { "Start", FD_LONG }, { "Callback", FD_FUNCTIONPTR }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF maGetTag[] = { { "Index", FD_LONG }, { "XMLTag:Result", FD_PTR|FD_STRUCT|FD_RESULT }, { 0, 0 } };

static const struct MethodArray clXMLMethods[] = {
   { -1, (APTR)XML_SetAttrib, "SetAttrib", maSetAttrib, sizeof(struct xmlSetAttrib) },
   { -2, (APTR)XML_GetString, "GetString", maGetString, sizeof(struct xmlGetString) },
   { -3, (APTR)XML_InsertXML, "InsertXML", maInsertXML, sizeof(struct xmlInsertXML) },
   { -4, (APTR)XML_GetContent, "GetContent", maGetContent, sizeof(struct xmlGetContent) },
   { -5, (APTR)XML_SortXML, "Sort", maSort, sizeof(struct xmlSort) },
   { -6, (APTR)XML_RemoveTag, "RemoveTag", maRemoveTag, sizeof(struct xmlRemoveTag) },
   { -7, (APTR)XML_MoveTags, "MoveTags", maMoveTags, sizeof(struct xmlMoveTags) },
   { -8, (APTR)XML_GetAttrib, "GetAttrib", maGetAttrib, sizeof(struct xmlGetAttrib) },
   { -9, (APTR)XML_InsertXPath, "InsertXPath", maInsertXPath, sizeof(struct xmlInsertXPath) },
   { -10, (APTR)XML_FindTag, "FindTag", maFindTag, sizeof(struct xmlFindTag) },
   { -11, (APTR)XML_Filter, "Filter", maFilter, sizeof(struct xmlFilter) },
   { -12, (APTR)XML_SetRoot, "SetRoot", maSetRoot, sizeof(struct xmlSetRoot) },
   { -13, (APTR)XML_Count, "Count", maCount, sizeof(struct xmlCount) },
   { -14, (APTR)XML_InsertContent, "InsertContent", maInsertContent, sizeof(struct xmlInsertContent) },
   { -15, (APTR)XML_RemoveXPath, "RemoveXPath", maRemoveXPath, sizeof(struct xmlRemoveXPath) },
   { -16, (APTR)XML_GetXPath, "GetXPath", maGetXPath, sizeof(struct xmlGetXPath) },
   { -17, (APTR)XML_FindTagFromIndex, "FindTagFromIndex", maFindTagFromIndex, sizeof(struct xmlFindTagFromIndex) },
   { -18, (APTR)XML_GetTag, "GetTag", maGetTag, sizeof(struct xmlGetTag) },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clXMLActions[] = {
   { AC_Clear, (APTR)XML_Clear },
   { AC_DataFeed, (APTR)XML_DataFeed },
   { AC_Free, (APTR)XML_Free },
   { AC_GetVar, (APTR)XML_GetVar },
   { AC_Init, (APTR)XML_Init },
   { AC_NewObject, (APTR)XML_NewObject },
   { AC_Reset, (APTR)XML_Reset },
   { AC_SaveToObject, (APTR)XML_SaveToObject },
   { AC_SetVar, (APTR)XML_SetVar },
   { 0, 0 }
};

#undef MOD_IDL
#define MOD_IDL "s.XMLAttrib:sName,sValue\ns.XMLTag:lIndex,lID,pChild:XMLTag,pPrev:XMLTag,pNext:XMLTag,pPrivate,pAttrib:XMLAttrib,wTotalAttrib,uwBranch,lLineNo\nc.XMF:ALL_CONTENT=0x400,DEBUG=0x2000,INCLUDE_COMMENTS=0x2,INCLUDE_SIBLINGS=0x80000000,INDENT=0x20,LOCK_REMOVE=0x40,LOWER_CASE=0x8,NEW=0x100,NO_ESCAPE=0x200,PARSE_ENTITY=0x4000,PARSE_HTML=0x800,READABLE=0x20,STRIP_CDATA=0x1000,STRIP_CONTENT=0x4,STRIP_HEADERS=0x80,UPPER_CASE=0x10,WELL_FORMED=0x1\nc.XMI:CHILD=0x1,CHILD_END=0x3,END=0x4,NEXT=0x2,PREV=0x0,PREVIOUS=0x0\nc.XMS:NEW=0xffffffff,UPDATE=0xfffffffd,UPDATE_ONLY=0xfffffffe\nc.XSF:CHECK_SORT=0x4,DESC=0x1,REPORT_SORTING=0x2\n"
