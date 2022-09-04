// Auto-generated by idl-c.fluid

#ifdef  __cplusplus
extern "C" {
#endif

static ERROR fntGetList(struct FontList ** Result);
static LONG fntStringWidth(struct rkFont * Font, CSTRING String, LONG Chars);
static void fntStringSize(struct rkFont * Font, CSTRING String, LONG Chars, LONG Wrap, LONG * Width, LONG * Rows);
static ERROR fntConvertCoords(struct rkFont * Font, CSTRING String, LONG X, LONG Y, LONG * Column, LONG * Row, LONG * ByteColumn, LONG * BytePos, LONG * CharX);
static LONG fntCharWidth(struct rkFont * Font, ULONG Char, ULONG KChar, LONG * Kerning);
static DOUBLE fntSetDefaultSize(DOUBLE Size);
static APTR fntFreetypeHandle();
static ERROR fntInstallFont(CSTRING Files);
static ERROR fntRemoveFont(CSTRING Name);
static ERROR fntSelectFont(CSTRING Name, CSTRING Style, LONG Point, LONG Flags, CSTRING * Path);

#ifdef  __cplusplus
}
#endif
#ifndef FDEF
#define FDEF static const struct FunctionField
#endif

FDEF argsCharWidth[] = { { "Result", FD_LONG }, { "Font", FD_OBJECTPTR }, { "Char", FD_LONG|FD_UNSIGNED }, { "KChar", FD_LONG|FD_UNSIGNED }, { "Kerning", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsConvertCoords[] = { { "Error", FD_LONG|FD_ERROR }, { "Font", FD_OBJECTPTR }, { "String", FD_STR }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Column", FD_LONG|FD_RESULT }, { "Row", FD_LONG|FD_RESULT }, { "ByteColumn", FD_LONG|FD_RESULT }, { "BytePos", FD_LONG|FD_RESULT }, { "CharX", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsFreetypeHandle[] = { { "Result", FD_PTR }, { 0, 0 } };
FDEF argsGetList[] = { { "Error", FD_LONG|FD_ERROR }, { "FontList:Result", FD_PTR|FD_STRUCT|FD_ALLOC|FD_RESULT }, { 0, 0 } };
FDEF argsInstallFont[] = { { "Error", FD_LONG|FD_ERROR }, { "Files", FD_STR }, { 0, 0 } };
FDEF argsRemoveFont[] = { { "Error", FD_LONG|FD_ERROR }, { "Name", FD_STR }, { 0, 0 } };
FDEF argsSelectFont[] = { { "Error", FD_LONG|FD_ERROR }, { "Name", FD_STR }, { "Style", FD_STR }, { "Point", FD_LONG }, { "Flags", FD_LONG }, { "Path", FD_STR|FD_ALLOC|FD_RESULT }, { 0, 0 } };
FDEF argsSetDefaultSize[] = { { "Result", FD_DOUBLE }, { "Size", FD_DOUBLE }, { 0, 0 } };
FDEF argsStringSize[] = { { "Void", FD_VOID }, { "Font", FD_OBJECTPTR }, { "String", FD_STR }, { "Chars", FD_LONG }, { "Wrap", FD_LONG }, { "Width", FD_LONG|FD_RESULT }, { "Rows", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsStringWidth[] = { { "Result", FD_LONG }, { "Font", FD_OBJECTPTR }, { "String", FD_STR }, { "Chars", FD_LONG }, { 0, 0 } };

const struct Function glFunctions[] = {
   { (APTR)fntGetList, "GetList", argsGetList },
   { (APTR)fntStringWidth, "StringWidth", argsStringWidth },
   { (APTR)fntStringSize, "StringSize", argsStringSize },
   { (APTR)fntConvertCoords, "ConvertCoords", argsConvertCoords },
   { (APTR)fntCharWidth, "CharWidth", argsCharWidth },
   { (APTR)fntSetDefaultSize, "SetDefaultSize", argsSetDefaultSize },
   { (APTR)fntFreetypeHandle, "FreetypeHandle", argsFreetypeHandle },
   { (APTR)fntInstallFont, "InstallFont", argsInstallFont },
   { (APTR)fntRemoveFont, "RemoveFont", argsRemoveFont },
   { (APTR)fntSelectFont, "SelectFont", argsSelectFont },
   { NULL, NULL, NULL }
};

#undef MOD_IDL
#define MOD_IDL "s.FontList:pNext:FontList,sName,lPoints[0],sStyles,cScalable,cReserved1,wReserved2\nc.FSS:ALL=0xffffffff,LINE=0xfffffffe\nc.FTF:ALLOW_SCALE=0x200,ANTIALIAS=0x10,BASE_LINE=0x100,BOLD=0x20000000,CHAR_CLIP=0x80,HEAVY_LINE=0x20,ITALIC=0x40000000,KERNING=0x80000000,PREFER_FIXED=0x2,PREFER_SCALED=0x1,QUICK_ALIAS=0x40,REQUIRE_FIXED=0x8,REQUIRE_SCALED=0x4,SCALABLE=0x10000000,SMOOTH=0x10\n"
