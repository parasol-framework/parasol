// Auto-generated by idl-c.fluid

static const struct FieldDef clFilterEffectDimensions[] = {
   { "ScaledX", 0x00000001 },
   { "ScaledY", 0x00000002 },
   { "FixedX", 0x00000004 },
   { "FixedY", 0x00000008 },
   { "ScaledXOffset", 0x00000010 },
   { "ScaledYOffset", 0x00000020 },
   { "FixedXOffset", 0x00000040 },
   { "FixedYOffset", 0x00000080 },
   { "FixedHeight", 0x00000100 },
   { "FixedWidth", 0x00000200 },
   { "ScaledHeight", 0x00000400 },
   { "ScaledWidth", 0x00000800 },
   { "FixedDepth", 0x00001000 },
   { "ScaledDepth", 0x00002000 },
   { "FixedZ", 0x00004000 },
   { "ScaledZ", 0x00008000 },
   { "ScaledRadiusX", 0x00010000 },
   { "FixedRadiusX", 0x00020000 },
   { "ScaledCenterX", 0x00040000 },
   { "ScaledCenterY", 0x00080000 },
   { "FixedCenterX", 0x00100000 },
   { "FixedCenterY", 0x00200000 },
   { "StatusChangeH", 0x00400000 },
   { "StatusChangeV", 0x00800000 },
   { "ScaledRadiusY", 0x01000000 },
   { "FixedRadiusY", 0x02000000 },
   { nullptr, 0 }
};

static const struct FieldDef clFilterEffectSourceType[] = {
   { "Ignore", 0x00000000 },
   { "None", 0x00000000 },
   { "Graphic", 0x00000001 },
   { "Alpha", 0x00000002 },
   { "Bkgd", 0x00000003 },
   { "BkgdAlpha", 0x00000004 },
   { "Fill", 0x00000005 },
   { "Stroke", 0x00000006 },
   { "Reference", 0x00000007 },
   { "Previous", 0x00000008 },
   { nullptr, 0 }
};

static const struct FieldDef clFilterEffectMixType[] = {
   { "Ignore", 0x00000000 },
   { "None", 0x00000000 },
   { "Graphic", 0x00000001 },
   { "Alpha", 0x00000002 },
   { "Bkgd", 0x00000003 },
   { "BkgdAlpha", 0x00000004 },
   { "Fill", 0x00000005 },
   { "Stroke", 0x00000006 },
   { "Reference", 0x00000007 },
   { "Previous", 0x00000008 },
   { nullptr, 0 }
};

static const struct FieldDef clFilterEffectVSF[] = {
   { "Ignore", 0x00000000 },
   { "None", 0x00000000 },
   { "Graphic", 0x00000001 },
   { "Alpha", 0x00000002 },
   { "Bkgd", 0x00000003 },
   { "BkgdAlpha", 0x00000004 },
   { "Fill", 0x00000005 },
   { "Stroke", 0x00000006 },
   { "Reference", 0x00000007 },
   { "Previous", 0x00000008 },
   { nullptr, 0 }
};

static const struct ActionArray clFilterEffectActions[] = {
   { AC::Free, FILTEREFFECT_Free },
   { AC::Init, FILTEREFFECT_Init },
   { AC::MoveToBack, FILTEREFFECT_MoveToBack },
   { AC::MoveToFront, FILTEREFFECT_MoveToFront },
   { AC::NewObject, FILTEREFFECT_NewObject },
   { AC::NewOwner, FILTEREFFECT_NewOwner },
   { AC::NIL, nullptr }
};

