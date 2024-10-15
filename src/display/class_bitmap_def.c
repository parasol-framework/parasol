// Auto-generated by idl-c.fluid

static const struct FieldDef clBitmapType[] = {
   { "Planar", 0x00000002 },
   { "Chunky", 0x00000003 },
   { NULL, 0 }
};

static const struct FieldDef clBitmapDataFlags[] = {
   { "Data", 0x00000000 },
   { "Managed", 0x00000001 },
   { "Video", 0x00000002 },
   { "Texture", 0x00000004 },
   { "Audio", 0x00000008 },
   { "Code", 0x00000010 },
   { "NoPool", 0x00000020 },
   { "TmpLock", 0x00000040 },
   { "Untracked", 0x00000080 },
   { "String", 0x00000100 },
   { "Object", 0x00000200 },
   { "NoLock", 0x00000400 },
   { "Exclusive", 0x00000800 },
   { "Collect", 0x00001000 },
   { "NoBlock", 0x00002000 },
   { "NoBlocking", 0x00002000 },
   { "Read", 0x00010000 },
   { "Write", 0x00020000 },
   { "ReadWrite", 0x00030000 },
   { "NoClear", 0x00040000 },
   { "Hidden", 0x00100000 },
   { "Caller", 0x00800000 },
   { NULL, 0 }
};

static const struct FieldDef clBitmapFlags[] = {
   { "BlankPalette", 0x00000001 },
   { "Compressed", 0x00000002 },
   { "NoData", 0x00000004 },
   { "Transparent", 0x00000008 },
   { "Mask", 0x00000010 },
   { "InverseAlpha", 0x00000020 },
   { "Queried", 0x00000040 },
   { "Clear", 0x00000080 },
   { "User", 0x00000100 },
   { "Accelerated2D", 0x00000200 },
   { "Accelerated3D", 0x00000400 },
   { "AlphaChannel", 0x00000800 },
   { "NeverShrink", 0x00001000 },
   { "X11DGA", 0x00002000 },
   { "FixedDepth", 0x00004000 },
   { "NoBlend", 0x00008000 },
   { "Premul", 0x00010000 },
   { NULL, 0 }
};

static const struct FieldDef clBitmapColourSpace[] = {
   { "SRGB", 0x00000001 },
   { "LinearRGB", 0x00000002 },
   { "CieLab", 0x00000003 },
   { "CieLch", 0x00000004 },
   { NULL, 0 }
};

FDEF maCopyArea[] = { { "DestBitmap", FD_OBJECTPTR }, { "Flags", FD_LONG }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { "XDest", FD_LONG }, { "YDest", FD_LONG }, { 0, 0 } };
FDEF maCompress[] = { { "Level", FD_LONG }, { 0, 0 } };
FDEF maDecompress[] = { { "RetainData", FD_LONG }, { 0, 0 } };
FDEF maFlip[] = { { "Orientation", FD_LONG }, { 0, 0 } };
FDEF maDrawRectangle[] = { { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { "Colour", FD_LONG|FD_UNSIGNED }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF maSetClipRegion[] = { { "Number", FD_LONG }, { "Left", FD_LONG }, { "Top", FD_LONG }, { "Right", FD_LONG }, { "Bottom", FD_LONG }, { "Terminate", FD_LONG }, { 0, 0 } };
FDEF maGetColour[] = { { "Red", FD_LONG }, { "Green", FD_LONG }, { "Blue", FD_LONG }, { "Alpha", FD_LONG }, { "Colour", FD_LONG|FD_UNSIGNED|FD_RESULT }, { 0, 0 } };

static const struct MethodEntry clBitmapMethods[] = {
   { AC(-1), (APTR)BITMAP_CopyArea, "CopyArea", maCopyArea, sizeof(struct bmp::CopyArea) },
   { AC(-2), (APTR)BITMAP_Compress, "Compress", maCompress, sizeof(struct bmp::Compress) },
   { AC(-3), (APTR)BITMAP_Decompress, "Decompress", maDecompress, sizeof(struct bmp::Decompress) },
   { AC(-4), (APTR)BITMAP_Flip, "Flip", maFlip, sizeof(struct bmp::Flip) },
   { AC(-6), (APTR)BITMAP_DrawRectangle, "DrawRectangle", maDrawRectangle, sizeof(struct bmp::DrawRectangle) },
   { AC(-7), (APTR)BITMAP_SetClipRegion, "SetClipRegion", maSetClipRegion, sizeof(struct bmp::SetClipRegion) },
   { AC(-8), (APTR)BITMAP_GetColour, "GetColour", maGetColour, sizeof(struct bmp::GetColour) },
   { AC(-10), (APTR)BITMAP_Premultiply, "Premultiply", 0, 0 },
   { AC(-11), (APTR)BITMAP_Demultiply, "Demultiply", 0, 0 },
   { AC(-12), (APTR)BITMAP_ConvertToLinear, "ConvertToLinear", 0, 0 },
   { AC(-13), (APTR)BITMAP_ConvertToRGB, "ConvertToRGB", 0, 0 },
   { AC::NIL, 0, 0, 0, 0 }
};

static const struct ActionArray clBitmapActions[] = {
   { AC::Clear, BITMAP_Clear },
   { AC::CopyData, BITMAP_CopyData },
   { AC::Draw, BITMAP_Draw },
   { AC::Flush, BITMAP_Flush },
   { AC::Free, BITMAP_Free },
   { AC::Init, BITMAP_Init },
   { AC::Lock, BITMAP_Lock },
   { AC::NewObject, BITMAP_NewObject },
   { AC::Query, BITMAP_Query },
   { AC::Read, BITMAP_Read },
   { AC::Resize, BITMAP_Resize },
   { AC::SaveImage, BITMAP_SaveImage },
   { AC::Seek, BITMAP_Seek },
   { AC::Unlock, BITMAP_Unlock },
   { AC::Write, BITMAP_Write },
   { AC::NIL, NULL }
};

