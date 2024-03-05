// Auto-generated by idl-c.fluid

extern "C" {
ERROR fntGetList(struct FontList ** Result);
LONG fntStringWidth(extFont * Font, CSTRING String, LONG Chars);
LONG fntCharWidth(extFont * Font, ULONG Char, ULONG KChar, LONG * Kerning);
ERROR fntSelectFont(CSTRING Name, CSTRING Style, LONG Point, FTF Flags, CSTRING * Path, FMETA * Meta);

} // extern c
#ifndef FDEF
#define FDEF static const struct FunctionField
#endif

FDEF argsCharWidth[] = { { "Result", FD_LONG }, { "Font", FD_OBJECTPTR }, { "Char", FD_LONG|FD_UNSIGNED }, { "KChar", FD_LONG|FD_UNSIGNED }, { "Kerning", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsGetList[] = { { "Error", FD_LONG|FD_ERROR }, { "FontList:Result", FD_PTR|FD_STRUCT|FD_ALLOC|FD_RESULT }, { 0, 0 } };
FDEF argsSelectFont[] = { { "Error", FD_LONG|FD_ERROR }, { "Name", FD_STR }, { "Style", FD_STR }, { "Point", FD_LONG }, { "Flags", FD_LONG }, { "Path", FD_STR|FD_ALLOC|FD_RESULT }, { "Meta", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsStringWidth[] = { { "Result", FD_LONG }, { "Font", FD_OBJECTPTR }, { "String", FD_STR }, { "Chars", FD_LONG }, { 0, 0 } };

const struct Function glFunctions[] = {
   { (APTR)fntGetList, "GetList", argsGetList },
   { (APTR)fntStringWidth, "StringWidth", argsStringWidth },
   { (APTR)fntCharWidth, "CharWidth", argsCharWidth },
   { (APTR)fntSelectFont, "SelectFont", argsSelectFont },
   { NULL, NULL, NULL }
};

#undef MOD_IDL
#define MOD_IDL "s.FontList:pNext:FontList,sName,lPoints[0],sStyles,cScalable,cVariable,cHinting[HINT],cReserved2\nc.FMETA:HINT_INTERNAL=0x10,HINT_LIGHT=0x8,HINT_NORMAL=0x4,SCALED=0x1,VARIABLE=0x2\nc.FSS:ALL=0xffffffff,LINE=0xfffffffe\nc.FTF:ALLOW_SCALE=0x40,BASE_LINE=0x20,BOLD=0x20000000,HEAVY_LINE=0x10,ITALIC=0x40000000,KERNING=0x80000000,PREFER_FIXED=0x2,PREFER_SCALED=0x1,REQUIRE_FIXED=0x8,REQUIRE_SCALED=0x4,SCALABLE=0x10000000,VARIABLE=0x8000000\nc.HINT:INTERNAL=0x2,LIGHT=0x3,NORMAL=0x1\n"
