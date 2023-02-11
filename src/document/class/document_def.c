// Auto-generated by idl-c.fluid

static const struct FieldDef clDocumentEventMask[] = {
   { "Path", 0x00000001 },
   { "LinkActivated", 0x00000002 },
   { NULL, 0 }
};

static const struct FieldDef clDocumentFlags[] = {
   { "Edit", 0x00000001 },
   { "Overwrite", 0x00000002 },
   { "NoSysKeys", 0x00000004 },
   { "Disabled", 0x00000008 },
   { "NoScrollbars", 0x00000010 },
   { "NoLayoutMsg", 0x00000020 },
   { "Unrestricted", 0x00000040 },
   { NULL, 0 }
};

static const struct FieldDef clDocumentBorderEdge[] = {
   { "Top", 0x00000001 },
   { "Left", 0x00000002 },
   { "Right", 0x00000004 },
   { "Bottom", 0x00000008 },
   { NULL, 0 }
};

FDEF maFeedParser[] = { { "String", FD_STR }, { 0, 0 } };
FDEF maSelectLink[] = { { "Index", FD_LONG }, { "Name", FD_STR }, { 0, 0 } };
FDEF maApplyFontStyle[] = { { "DocStyle:Style", FD_PTR|FD_STRUCT }, { "Font", FD_OBJECTPTR }, { 0, 0 } };
FDEF maFindIndex[] = { { "Name", FD_STR }, { "Start", FD_LONG|FD_RESULT }, { "End", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF maInsertXML[] = { { "XML", FD_STR }, { "Index", FD_LONG }, { 0, 0 } };
FDEF maRemoveContent[] = { { "Start", FD_LONG }, { "End", FD_LONG }, { 0, 0 } };
FDEF maInsertText[] = { { "Text", FD_STR }, { "Index", FD_LONG }, { "Preformat", FD_LONG }, { 0, 0 } };
FDEF maCallFunction[] = { { "Function", FD_STR }, { "ScriptArg:Args", FD_PTR|FD_STRUCT }, { "TotalArgs", FD_LONG }, { 0, 0 } };
FDEF maAddListener[] = { { "Trigger", FD_LONG }, { "Function", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF maRemoveListener[] = { { "Trigger", FD_LONG }, { "Function", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF maShowIndex[] = { { "Name", FD_STR }, { 0, 0 } };
FDEF maHideIndex[] = { { "Name", FD_STR }, { 0, 0 } };
FDEF maEdit[] = { { "Name", FD_STR }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF maReadContent[] = { { "Format", FD_LONG }, { "Start", FD_LONG }, { "End", FD_LONG }, { "Result", FD_STR|FD_ALLOC|FD_RESULT }, { 0, 0 } };

static const struct MethodArray clDocumentMethods[] = {
   { -1, (APTR)DOCUMENT_FeedParser, "FeedParser", maFeedParser, sizeof(struct docFeedParser) },
   { -2, (APTR)DOCUMENT_SelectLink, "SelectLink", maSelectLink, sizeof(struct docSelectLink) },
   { -3, (APTR)DOCUMENT_ApplyFontStyle, "ApplyFontStyle", maApplyFontStyle, sizeof(struct docApplyFontStyle) },
   { -4, (APTR)DOCUMENT_FindIndex, "FindIndex", maFindIndex, sizeof(struct docFindIndex) },
   { -5, (APTR)DOCUMENT_InsertXML, "InsertXML", maInsertXML, sizeof(struct docInsertXML) },
   { -6, (APTR)DOCUMENT_RemoveContent, "RemoveContent", maRemoveContent, sizeof(struct docRemoveContent) },
   { -7, (APTR)DOCUMENT_InsertText, "InsertText", maInsertText, sizeof(struct docInsertText) },
   { -8, (APTR)DOCUMENT_CallFunction, "CallFunction", maCallFunction, sizeof(struct docCallFunction) },
   { -9, (APTR)DOCUMENT_AddListener, "AddListener", maAddListener, sizeof(struct docAddListener) },
   { -10, (APTR)DOCUMENT_RemoveListener, "RemoveListener", maRemoveListener, sizeof(struct docRemoveListener) },
   { -11, (APTR)DOCUMENT_ShowIndex, "ShowIndex", maShowIndex, sizeof(struct docShowIndex) },
   { -12, (APTR)DOCUMENT_HideIndex, "HideIndex", maHideIndex, sizeof(struct docHideIndex) },
   { -13, (APTR)DOCUMENT_Edit, "Edit", maEdit, sizeof(struct docEdit) },
   { -14, (APTR)DOCUMENT_ReadContent, "ReadContent", maReadContent, sizeof(struct docReadContent) },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clDocumentActions[] = {
   { AC_Activate, (APTR)DOCUMENT_Activate },
   { AC_Clear, (APTR)DOCUMENT_Clear },
   { AC_Clipboard, (APTR)DOCUMENT_Clipboard },
   { AC_DataFeed, (APTR)DOCUMENT_DataFeed },
   { AC_Disable, (APTR)DOCUMENT_Disable },
   { AC_Draw, (APTR)DOCUMENT_Draw },
   { AC_Enable, (APTR)DOCUMENT_Enable },
   { AC_Focus, (APTR)DOCUMENT_Focus },
   { AC_Free, (APTR)DOCUMENT_Free },
   { AC_GetVar, (APTR)DOCUMENT_GetVar },
   { AC_Init, (APTR)DOCUMENT_Init },
   { AC_NewObject, (APTR)DOCUMENT_NewObject },
   { AC_NewOwner, (APTR)DOCUMENT_NewOwner },
   { AC_Refresh, (APTR)DOCUMENT_Refresh },
   { AC_SaveToObject, (APTR)DOCUMENT_SaveToObject },
   { AC_ScrollToPoint, (APTR)DOCUMENT_ScrollToPoint },
   { AC_SetVar, (APTR)DOCUMENT_SetVar },
   { 0, 0 }
};

