// Auto-generated by idl-c.fluid

static const struct FieldDef clCompressionFlags[] = {
   { "Password", 0x00000001 },
   { "New", 0x00000002 },
   { "CreateFile", 0x00000004 },
   { "ReadOnly", 0x00000008 },
   { "NoLinks", 0x00000010 },
   { "ApplySecurity", 0x00000020 },
   { nullptr, 0 }
};

static const struct FieldDef clCompressionPermissions[] = {
   { "Read", 0x00000001 },
   { "UserRead", 0x00000001 },
   { "UserWrite", 0x00000002 },
   { "Write", 0x00000002 },
   { "Exec", 0x00000004 },
   { "UserExec", 0x00000004 },
   { "Delete", 0x00000008 },
   { "User", 0x0000000f },
   { "GroupRead", 0x00000010 },
   { "GroupWrite", 0x00000020 },
   { "GroupExec", 0x00000040 },
   { "GroupDelete", 0x00000080 },
   { "Group", 0x000000f0 },
   { "OthersRead", 0x00000100 },
   { "AllRead", 0x00000111 },
   { "EveryoneRead", 0x00000111 },
   { "OthersWrite", 0x00000200 },
   { "AllWrite", 0x00000222 },
   { "EveryoneWrite", 0x00000222 },
   { "EveryoneReadwrite", 0x00000333 },
   { "OthersExec", 0x00000400 },
   { "AllExec", 0x00000444 },
   { "EveryoneExec", 0x00000444 },
   { "OthersDelete", 0x00000800 },
   { "AllDelete", 0x00000888 },
   { "EveryoneDelete", 0x00000888 },
   { "Others", 0x00000f00 },
   { "EveryoneAccess", 0x00000fff },
   { "Hidden", 0x00001000 },
   { "Archive", 0x00002000 },
   { "Password", 0x00004000 },
   { "Userid", 0x00008000 },
   { "Groupid", 0x00010000 },
   { "Inherit", 0x00020000 },
   { "Offline", 0x00040000 },
   { "Network", 0x00080000 },
   { nullptr, 0 }
};

FDEF maCompressBuffer[] = { { "Input", FD_BUFFER|FD_PTR }, { "InputSize", FD_INT|FD_BUFSIZE }, { "Output", FD_BUFFER|FD_PTR }, { "OutputSize", FD_INT|FD_BUFSIZE }, { "Result", FD_INT|FD_RESULT }, { 0, 0 } };
FDEF maCompressFile[] = { { "Location", FD_STR }, { "Path", FD_STR }, { 0, 0 } };
FDEF maDecompressBuffer[] = { { "Input", FD_BUFFER|FD_PTR }, { "Output", FD_BUFFER|FD_PTR }, { "OutputSize", FD_INT|FD_BUFSIZE }, { "Result", FD_INT|FD_RESULT }, { 0, 0 } };
FDEF maDecompressFile[] = { { "Path", FD_STR }, { "Dest", FD_STR }, { "Flags", FD_INT }, { 0, 0 } };
FDEF maRemoveFile[] = { { "Path", FD_STR }, { 0, 0 } };
FDEF maCompressStream[] = { { "Input", FD_BUFFER|FD_PTR }, { "Length", FD_INT|FD_BUFSIZE }, { "Callback", FD_FUNCTIONPTR }, { "Output", FD_BUFFER|FD_PTR }, { "OutputSize", FD_INT|FD_BUFSIZE }, { 0, 0 } };
FDEF maDecompressStream[] = { { "Input", FD_BUFFER|FD_PTR }, { "Length", FD_INT|FD_BUFSIZE }, { "Callback", FD_FUNCTIONPTR }, { "Output", FD_BUFFER|FD_PTR }, { "OutputSize", FD_INT|FD_BUFSIZE }, { 0, 0 } };
FDEF maCompressStreamEnd[] = { { "Callback", FD_FUNCTIONPTR }, { "Output", FD_BUFFER|FD_PTR }, { "OutputSize", FD_INT|FD_BUFSIZE }, { 0, 0 } };
FDEF maDecompressStreamEnd[] = { { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF maDecompressObject[] = { { "Path", FD_STR }, { "Object", FD_OBJECTPTR }, { 0, 0 } };
FDEF maScan[] = { { "Folder", FD_STR }, { "Filter", FD_STR }, { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF maFind[] = { { "Path", FD_STR }, { "CaseSensitive", FD_INT }, { "Wildcard", FD_INT }, { "CompressedItem:Item", FD_PTR|FD_STRUCT|FD_RESULT }, { 0, 0 } };

static const struct MethodEntry clCompressionMethods[] = {
   { AC(-1), (APTR)COMPRESSION_CompressBuffer, "CompressBuffer", maCompressBuffer, sizeof(struct cmp::CompressBuffer) },
   { AC(-2), (APTR)COMPRESSION_CompressFile, "CompressFile", maCompressFile, sizeof(struct cmp::CompressFile) },
   { AC(-3), (APTR)COMPRESSION_DecompressBuffer, "DecompressBuffer", maDecompressBuffer, sizeof(struct cmp::DecompressBuffer) },
   { AC(-4), (APTR)COMPRESSION_DecompressFile, "DecompressFile", maDecompressFile, sizeof(struct cmp::DecompressFile) },
   { AC(-5), (APTR)COMPRESSION_RemoveFile, "RemoveFile", maRemoveFile, sizeof(struct cmp::RemoveFile) },
   { AC(-6), (APTR)COMPRESSION_CompressStream, "CompressStream", maCompressStream, sizeof(struct cmp::CompressStream) },
   { AC(-7), (APTR)COMPRESSION_DecompressStream, "DecompressStream", maDecompressStream, sizeof(struct cmp::DecompressStream) },
   { AC(-8), (APTR)COMPRESSION_CompressStreamStart, "CompressStreamStart", 0, 0 },
   { AC(-9), (APTR)COMPRESSION_CompressStreamEnd, "CompressStreamEnd", maCompressStreamEnd, sizeof(struct cmp::CompressStreamEnd) },
   { AC(-10), (APTR)COMPRESSION_DecompressStreamEnd, "DecompressStreamEnd", maDecompressStreamEnd, sizeof(struct cmp::DecompressStreamEnd) },
   { AC(-11), (APTR)COMPRESSION_DecompressStreamStart, "DecompressStreamStart", 0, 0 },
   { AC(-12), (APTR)COMPRESSION_DecompressObject, "DecompressObject", maDecompressObject, sizeof(struct cmp::DecompressObject) },
   { AC(-13), (APTR)COMPRESSION_Scan, "Scan", maScan, sizeof(struct cmp::Scan) },
   { AC(-14), (APTR)COMPRESSION_Find, "Find", maFind, sizeof(struct cmp::Find) },
   { AC::NIL, 0, 0, 0, 0 }
};

static const struct ActionArray clCompressionActions[] = {
   { AC::Flush, COMPRESSION_Flush },
   { AC::Free, COMPRESSION_Free },
   { AC::Init, COMPRESSION_Init },
   { AC::NewObject, COMPRESSION_NewObject },
   { AC::NewPlacement, COMPRESSION_NewPlacement },
   { AC::NIL, nullptr }
};

