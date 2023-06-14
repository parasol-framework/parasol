// Auto-generated by idl-c.fluid

static const struct FieldDef clFontFlags[] = {
   { "PreferScaled", 0x00000001 },
   { "PreferFixed", 0x00000002 },
   { "RequireScaled", 0x00000004 },
   { "RequireFixed", 0x00000008 },
   { "Antialias", 0x00000010 },
   { "Smooth", 0x00000010 },
   { "HeavyLine", 0x00000020 },
   { "QuickAlias", 0x00000040 },
   { "CharClip", 0x00000080 },
   { "BaseLine", 0x00000100 },
   { "AllowScale", 0x00000200 },
   { "NoBlend", 0x00000400 },
   { "Scalable", 0x10000000 },
   { "Bold", 0x20000000 },
   { "Italic", 0x40000000 },
   { "Kerning", (LONG)0x80000000 },
   { NULL, 0 }
};

static const struct FieldDef clFontAlign[] = {
   { "Left", 0x00000001 },
   { "Right", 0x00000002 },
   { "Horizontal", 0x00000004 },
   { "Vertical", 0x00000008 },
   { "Center", 0x0000000c },
   { "Middle", 0x0000000c },
   { "Top", 0x00000010 },
   { "Bottom", 0x00000020 },
   { NULL, 0 }
};

static const struct ActionArray clFontActions[] = {
   { AC_Draw, FONT_Draw },
   { AC_Free, FONT_Free },
   { AC_Init, FONT_Init },
   { AC_NewObject, FONT_NewObject },
   { 0, NULL }
};

