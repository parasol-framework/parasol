// Auto-generated by idl-c.fluid

static const struct FieldDef clFileFlags[] = {
   { "Write", 0x00000001 },
   { "New", 0x00000002 },
   { "Read", 0x00000004 },
   { "Directory", 0x00000008 },
   { "Folder", 0x00000008 },
   { "Approximate", 0x00000010 },
   { "Link", 0x00000020 },
   { "Buffer", 0x00000040 },
   { "Loop", 0x00000080 },
   { "File", 0x00000100 },
   { "ResetDate", 0x00000200 },
   { "Device", 0x00000400 },
   { "Stream", 0x00000800 },
   { "ExcludeFiles", 0x00001000 },
   { "ExcludeFolders", 0x00002000 },
   { NULL, 0 }
};

FDEF maStartStream[] = { { "Subscriber", FD_OBJECTID }, { "Flags", FD_LONG }, { "Length", FD_LONG }, { 0, 0 } };
FDEF maDelete[] = { { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF maMove[] = { { "Dest", FD_STR }, { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF maCopy[] = { { "Dest", FD_STR }, { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF maSetDate[] = { { "Year", FD_LONG }, { "Month", FD_LONG }, { "Day", FD_LONG }, { "Hour", FD_LONG }, { "Minute", FD_LONG }, { "Second", FD_LONG }, { "Type", FD_LONG }, { 0, 0 } };
FDEF maReadLine[] = { { "Result", FD_STR|FD_RESULT }, { 0, 0 } };
FDEF maNext[] = { { "File", FD_OBJECTPTR|FD_ALLOC|FD_RESULT }, { 0, 0 } };
FDEF maWatch[] = { { "Callback", FD_FUNCTIONPTR }, { "Custom", FD_LARGE }, { "Flags", FD_LONG }, { 0, 0 } };

static const struct MethodArray clFileMethods[] = {
   { -1, (APTR)FILE_StartStream, "StartStream", maStartStream, sizeof(struct flStartStream) },
   { -2, (APTR)FILE_StopStream, "StopStream", 0, 0 },
   { -3, (APTR)FILE_Delete, "Delete", maDelete, sizeof(struct flDelete) },
   { -4, (APTR)FILE_MoveFile, "Move", maMove, sizeof(struct flMove) },
   { -5, (APTR)FILE_Copy, "Copy", maCopy, sizeof(struct flCopy) },
   { -6, (APTR)FILE_SetDate, "SetDate", maSetDate, sizeof(struct flSetDate) },
   { -7, (APTR)FILE_ReadLine, "ReadLine", maReadLine, sizeof(struct flReadLine) },
   { -8, (APTR)FILE_BufferContent, "BufferContent", 0, 0 },
   { -9, (APTR)FILE_NextFile, "Next", maNext, sizeof(struct flNext) },
   { -10, (APTR)FILE_Watch, "Watch", maWatch, sizeof(struct flWatch) },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clFileActions[] = {
   { AC_Activate, (APTR)FILE_Activate },
   { AC_DataFeed, (APTR)FILE_DataFeed },
   { AC_Free, (APTR)FILE_Free },
   { AC_Init, (APTR)FILE_Init },
   { AC_NewObject, (APTR)FILE_NewObject },
   { AC_Query, (APTR)FILE_Query },
   { AC_Read, (APTR)FILE_Read },
   { AC_Rename, (APTR)FILE_Rename },
   { AC_Reset, (APTR)FILE_Reset },
   { AC_Seek, (APTR)FILE_Seek },
   { AC_Write, (APTR)FILE_Write },
   { 0, 0 }
};

