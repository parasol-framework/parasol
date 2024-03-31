// Auto-generated by idl-c.fluid

static const struct FieldDef clVectorGradientSpreadMethod[] = {
   { "Undefined", 0x00000000 },
   { "Pad", 0x00000001 },
   { "Reflect", 0x00000002 },
   { "Repeat", 0x00000003 },
   { "ReflectX", 0x00000004 },
   { "ReflectY", 0x00000005 },
   { "Clip", 0x00000006 },
   { NULL, 0 }
};

static const struct FieldDef clVectorGradientUnits[] = {
   { "Undefined", 0x00000000 },
   { "BoundingBox", 0x00000001 },
   { "Userspace", 0x00000002 },
   { NULL, 0 }
};

static const struct FieldDef clVectorGradientType[] = {
   { "Linear", 0x00000000 },
   { "Radial", 0x00000001 },
   { "Conic", 0x00000002 },
   { "Diamond", 0x00000003 },
   { "Contour", 0x00000004 },
   { NULL, 0 }
};

static const struct FieldDef clVectorGradientFlags[] = {
   { "ScaledX1", 0x00000001 },
   { "ScaledY1", 0x00000002 },
   { "ScaledX2", 0x00000004 },
   { "ScaledY2", 0x00000008 },
   { "ScaledCX", 0x00000010 },
   { "ScaledCY", 0x00000020 },
   { "ScaledFX", 0x00000040 },
   { "ScaledFY", 0x00000080 },
   { "ScaledRadius", 0x00000100 },
   { "FixedX1", 0x00000200 },
   { "FixedY1", 0x00000400 },
   { "FixedX2", 0x00000800 },
   { "FixedY2", 0x00001000 },
   { "FixedCX", 0x00002000 },
   { "FixedCY", 0x00004000 },
   { "FixedFX", 0x00008000 },
   { "FixedFY", 0x00010000 },
   { "FixedRadius", 0x00020000 },
   { NULL, 0 }
};

static const struct FieldDef clVectorGradientColourSpace[] = {
   { "Inherit", 0x00000000 },
   { "SRGB", 0x00000001 },
   { "LinearRGB", 0x00000002 },
   { NULL, 0 }
};

static const struct ActionArray clVectorGradientActions[] = {
   { AC_Free, VECTORGRADIENT_Free },
   { AC_Init, VECTORGRADIENT_Init },
   { AC_NewObject, VECTORGRADIENT_NewObject },
   { 0, NULL }
};

