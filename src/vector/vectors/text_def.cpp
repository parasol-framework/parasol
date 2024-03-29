// Auto-generated by idl-c.fluid

static const struct FieldDef clVectorTextVTXF[] = {
   { "Underline", 0x00000001 },
   { "Overline", 0x00000002 },
   { "LineThrough", 0x00000004 },
   { "Blink", 0x00000008 },
   { "Edit", 0x00000010 },
   { "Editable", 0x00000010 },
   { "AreaSelected", 0x00000020 },
   { "NoSysKeys", 0x00000040 },
   { "Overwrite", 0x00000080 },
   { "Raster", 0x00000100 },
   { NULL, 0 }
};

static const struct FieldDef clVectorTextVTS[] = {
   { "Inherit", 0x00000000 },
   { "Normal", 0x00000001 },
   { "Wider", 0x00000002 },
   { "Narrower", 0x00000003 },
   { "UltraCondensed", 0x00000004 },
   { "ExtraCondensed", 0x00000005 },
   { "Condensed", 0x00000006 },
   { "SemiCondensed", 0x00000007 },
   { "Expanded", 0x00000008 },
   { "SemiExpanded", 0x00000009 },
   { "UltraExpanded", 0x0000000a },
   { "ExtraExpanded", 0x0000000b },
   { NULL, 0 }
};

FDEF maDeleteLine[] = { { "Line", FD_LONG }, { 0, 0 } };

static const struct MethodEntry clVectorTextMethods[] = {
   { -30, (APTR)VECTORTEXT_DeleteLine, "DeleteLine", maDeleteLine, sizeof(struct vtDeleteLine) },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clVectorTextActions[] = {
   { AC_Free, VECTORTEXT_Free },
   { AC_Init, VECTORTEXT_Init },
   { AC_NewObject, VECTORTEXT_NewObject },
   { 0, NULL }
};

