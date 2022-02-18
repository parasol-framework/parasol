// Auto-generated by idl-c.fluid

static const struct FieldDef clCompressionFlags[] = {
   { "Password", 0x00000001 },
   { "CreateFile", 0x00000004 },
   { "ApplySecurity", 0x00000020 },
   { "ReadOnly", 0x00000008 },
   { "New", 0x00000002 },
   { "NoLinks", 0x00000010 },
   { NULL, 0 }
};

static const struct FieldDef clCompressionPermissions[] = {
   { "Inherit", 0x00020000 },
   { "Offline", 0x00040000 },
   { "Read", 0x00000001 },
   { "Delete", 0x00000008 },
   { "Network", 0x00080000 },
   { "EveryoneExec", 0x00000444 },
   { "EveryoneDelete", 0x00000888 },
   { "GroupExec", 0x00000040 },
   { "AllRead", 0x00000111 },
   { "AllWrite", 0x00000222 },
   { "GroupDelete", 0x00000080 },
   { "AllDelete", 0x00000888 },
   { "EveryoneAccess", 0x00000fff },
   { "OthersRead", 0x00000100 },
   { "EveryoneReadwrite", 0x00000333 },
   { "OthersWrite", 0x00000200 },
   { "Hidden", 0x00001000 },
   { "OthersExec", 0x00000400 },
   { "Others", 0x00000f00 },
   { "Group", 0x000000f0 },
   { "OthersDelete", 0x00000800 },
   { "User", 0x0000000f },
   { "AllExec", 0x00000444 },
   { "GroupWrite", 0x00000020 },
   { "GroupRead", 0x00000010 },
   { "Archive", 0x00002000 },
   { "Write", 0x00000002 },
   { "Userid", 0x00008000 },
   { "Password", 0x00004000 },
   { "UserRead", 0x00000001 },
   { "UserWrite", 0x00000002 },
   { "UserExec", 0x00000004 },
   { "Exec", 0x00000004 },
   { "EveryoneRead", 0x00000111 },
   { "Groupid", 0x00010000 },
   { "EveryoneWrite", 0x00000222 },
   { NULL, 0 }
};

FDEF maCompressBuffer[] = { { "Input", FD_BUFFER|FD_PTR }, { "InputSize", FD_LONG|FD_BUFSIZE }, { "Output", FD_BUFFER|FD_PTR }, { "OutputSize", FD_LONG|FD_BUFSIZE }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF maCompressFile[] = { { "Location", FD_STR }, { "Path", FD_STR }, { 0, 0 } };
FDEF maDecompressBuffer[] = { { "Input", FD_BUFFER|FD_PTR }, { "Output", FD_BUFFER|FD_PTR }, { "OutputSize", FD_LONG|FD_BUFSIZE }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF maDecompressFile[] = { { "Path", FD_STR }, { "Dest", FD_STR }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF maRemoveFile[] = { { "Path", FD_STR }, { 0, 0 } };
FDEF maCompressStream[] = { { "Input", FD_BUFFER|FD_PTR }, { "Length", FD_LONG|FD_BUFSIZE }, { "Callback", FD_FUNCTIONPTR }, { "Output", FD_BUFFER|FD_PTR }, { "OutputSize", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF maDecompressStream[] = { { "Input", FD_BUFFER|FD_PTR }, { "Length", FD_LONG|FD_BUFSIZE }, { "Callback", FD_FUNCTIONPTR }, { "Output", FD_BUFFER|FD_PTR }, { "OutputSize", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF maCompressStreamEnd[] = { { "Callback", FD_FUNCTIONPTR }, { "Output", FD_BUFFER|FD_PTR }, { "OutputSize", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF maDecompressStreamEnd[] = { { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF maDecompressObject[] = { { "Path", FD_STR }, { "Object", FD_OBJECTPTR }, { 0, 0 } };
FDEF maScan[] = { { "Folder", FD_STR }, { "Filter", FD_STR }, { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF maFind[] = { { "Path", FD_STR }, { "Flags", FD_LONG }, { "CompressedItem:Item", FD_PTR|FD_STRUCT|FD_RESULT }, { 0, 0 } };

static const struct MethodArray clCompressionMethods[] = {
   { -1, (APTR)COMPRESSION_CompressBuffer, "CompressBuffer", maCompressBuffer, sizeof(struct cmpCompressBuffer) },
   { -2, (APTR)COMPRESSION_CompressFile, "CompressFile", maCompressFile, sizeof(struct cmpCompressFile) },
   { -3, (APTR)COMPRESSION_DecompressBuffer, "DecompressBuffer", maDecompressBuffer, sizeof(struct cmpDecompressBuffer) },
   { -4, (APTR)COMPRESSION_DecompressFile, "DecompressFile", maDecompressFile, sizeof(struct cmpDecompressFile) },
   { -5, (APTR)COMPRESSION_RemoveFile, "RemoveFile", maRemoveFile, sizeof(struct cmpRemoveFile) },
   { -6, (APTR)COMPRESSION_CompressStream, "CompressStream", maCompressStream, sizeof(struct cmpCompressStream) },
   { -7, (APTR)COMPRESSION_DecompressStream, "DecompressStream", maDecompressStream, sizeof(struct cmpDecompressStream) },
   { -8, (APTR)COMPRESSION_CompressStreamStart, "CompressStreamStart", 0, 0 },
   { -9, (APTR)COMPRESSION_CompressStreamEnd, "CompressStreamEnd", maCompressStreamEnd, sizeof(struct cmpCompressStreamEnd) },
   { -10, (APTR)COMPRESSION_DecompressStreamEnd, "DecompressStreamEnd", maDecompressStreamEnd, sizeof(struct cmpDecompressStreamEnd) },
   { -11, (APTR)COMPRESSION_DecompressStreamStart, "DecompressStreamStart", 0, 0 },
   { -12, (APTR)COMPRESSION_DecompressObject, "DecompressObject", maDecompressObject, sizeof(struct cmpDecompressObject) },
   { -13, (APTR)COMPRESSION_Scan, "Scan", maScan, sizeof(struct cmpScan) },
   { -14, (APTR)COMPRESSION_Find, "Find", maFind, sizeof(struct cmpFind) },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clCompressionActions[] = {
   { AC_ActionNotify, (APTR)COMPRESSION_ActionNotify },
   { AC_Flush, (APTR)COMPRESSION_Flush },
   { AC_Free, (APTR)COMPRESSION_Free },
   { AC_Init, (APTR)COMPRESSION_Init },
   { AC_NewObject, (APTR)COMPRESSION_NewObject },
   { 0, 0 }
};

