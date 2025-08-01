// Auto-generated by idl-c.fluid

static const struct FieldDef clPictureFlags[] = {
   { "NoPalette", 0x00000001 },
   { "Scalable", 0x00000002 },
   { "New", 0x00000004 },
   { "Mask", 0x00000008 },
   { "Alpha", 0x00000010 },
   { "Lazy", 0x00000020 },
   { "ForceAlpha32", 0x00000040 },
   { nullptr, 0 }
};

static const struct ActionArray clPictureActions[] = {
   { AC::Activate, PICTURE_Activate },
   { AC::Free, PICTURE_Free },
   { AC::Init, PICTURE_Init },
   { AC::NewObject, PICTURE_NewObject },
   { AC::NewPlacement, PICTURE_NewPlacement },
   { AC::Query, PICTURE_Query },
   { AC::Read, PICTURE_Read },
   { AC::Refresh, PICTURE_Refresh },
   { AC::SaveImage, PICTURE_SaveImage },
   { AC::SaveToObject, PICTURE_SaveToObject },
   { AC::Seek, PICTURE_Seek },
   { AC::Write, PICTURE_Write },
   { AC::NIL, nullptr }
};

#undef MOD_IDL
#define MOD_IDL "c.PCF:ALPHA=0x10,FORCE_ALPHA_32=0x40,LAZY=0x20,MASK=0x8,NEW=0x4,NO_PALETTE=0x1,SCALABLE=0x2\n"
