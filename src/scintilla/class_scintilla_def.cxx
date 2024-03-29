// Auto-generated by idl-c.fluid

static const struct FieldDef clScintillaEventFlags[] = {
   { "Modified", 0x00000001 },
   { "CursorPos", 0x00000002 },
   { "FailRo", 0x00000004 },
   { "NewChar", 0x00000008 },
   { NULL, 0 }
};

static const struct FieldDef clScintillaFlags[] = {
   { "Disabled", 0x00000001 },
   { "DetectLexer", 0x00000002 },
   { "Edit", 0x00000004 },
   { "ExtPage", 0x00000008 },
   { NULL, 0 }
};

static const struct FieldDef clScintillaLexer[] = {
   { "Errorlist", 0x0000000a },
   { "Makefile", 0x0000000b },
   { "Batch", 0x0000000c },
   { "Fluid", 0x0000000f },
   { "Diff", 0x00000010 },
   { "Pascal", 0x00000012 },
   { "Python", 0x00000002 },
   { "Ruby", 0x00000016 },
   { "Vbscript", 0x0000001c },
   { "Asp", 0x0000001d },
   { "Cpp", 0x00000003 },
   { "Assembler", 0x00000022 },
   { "Css", 0x00000026 },
   { "HTML", 0x00000004 },
   { "Xml", 0x00000005 },
   { "Perl", 0x00000006 },
   { "Bash", 0x0000003e },
   { "Phpscript", 0x00000045 },
   { "Sql", 0x00000007 },
   { "Rebol", 0x00000047 },
   { "Vb", 0x00000008 },
   { "Properties", 0x00000009 },
   { NULL, 0 }
};

FDEF maSetFont[] = { { "Face", FD_STR }, { 0, 0 } };
FDEF maReplaceText[] = { { "Find", FD_STR }, { "Replace", FD_STR }, { "Flags", FD_LONG }, { "Start", FD_LONG }, { "End", FD_LONG }, { 0, 0 } };
FDEF maDeleteLine[] = { { "Line", FD_LONG }, { 0, 0 } };
FDEF maSelectRange[] = { { "Start", FD_LONG }, { "End", FD_LONG }, { 0, 0 } };
FDEF maInsertText[] = { { "String", FD_STR }, { "Pos", FD_LONG }, { 0, 0 } };
FDEF maGetLine[] = { { "Line", FD_LONG }, { "Buffer", FD_BUFFER|FD_STR }, { "Length", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF maReplaceLine[] = { { "Line", FD_LONG }, { "String", FD_STR }, { "Length", FD_LONG }, { 0, 0 } };
FDEF maGotoLine[] = { { "Line", FD_LONG }, { 0, 0 } };
FDEF maGetPos[] = { { "Line", FD_LONG }, { "Column", FD_LONG }, { "Pos", FD_LONG|FD_RESULT }, { 0, 0 } };

static const struct MethodEntry clScintillaMethods[] = {
   { -1, (APTR)SCINTILLA_SetFont, "SetFont", maSetFont, sizeof(struct sciSetFont) },
   { -2, (APTR)SCINTILLA_ReplaceText, "ReplaceText", maReplaceText, sizeof(struct sciReplaceText) },
   { -3, (APTR)SCINTILLA_DeleteLine, "DeleteLine", maDeleteLine, sizeof(struct sciDeleteLine) },
   { -4, (APTR)SCINTILLA_SelectRange, "SelectRange", maSelectRange, sizeof(struct sciSelectRange) },
   { -5, (APTR)SCINTILLA_InsertText, "InsertText", maInsertText, sizeof(struct sciInsertText) },
   { -6, (APTR)SCINTILLA_GetLine, "GetLine", maGetLine, sizeof(struct sciGetLine) },
   { -7, (APTR)SCINTILLA_ReplaceLine, "ReplaceLine", maReplaceLine, sizeof(struct sciReplaceLine) },
   { -8, (APTR)SCINTILLA_GotoLine, "GotoLine", maGotoLine, sizeof(struct sciGotoLine) },
   { -9, (APTR)SCINTILLA_TrimWhitespace, "TrimWhitespace", 0, 0 },
   { -10, (APTR)SCINTILLA_GetPos, "GetPos", maGetPos, sizeof(struct sciGetPos) },
   { -11, (APTR)SCINTILLA_ReportEvent, "ReportEvent", 0, 0 },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clScintillaActions[] = {
   { AC_Clear, SCINTILLA_Clear },
   { AC_Clipboard, SCINTILLA_Clipboard },
   { AC_DataFeed, SCINTILLA_DataFeed },
   { AC_Disable, SCINTILLA_Disable },
   { AC_Draw, SCINTILLA_Draw },
   { AC_Enable, SCINTILLA_Enable },
   { AC_Focus, SCINTILLA_Focus },
   { AC_Free, SCINTILLA_Free },
   { AC_Hide, SCINTILLA_Hide },
   { AC_Init, SCINTILLA_Init },
   { AC_NewObject, SCINTILLA_NewObject },
   { AC_NewOwner, SCINTILLA_NewOwner },
   { AC_Redo, SCINTILLA_Redo },
   { AC_SaveToObject, SCINTILLA_SaveToObject },
   { AC_ScrollToPoint, SCINTILLA_ScrollToPoint },
   { AC_Show, SCINTILLA_Show },
   { AC_Undo, SCINTILLA_Undo },
   { 0, NULL }
};

