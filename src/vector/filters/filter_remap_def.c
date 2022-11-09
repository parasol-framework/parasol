// Auto-generated by idl-c.fluid

FDEF maSelectGamma[] = { { "Component", FD_LONG }, { "Amplitude", FD_DOUBLE }, { "Offset", FD_DOUBLE }, { "Exponent", FD_DOUBLE }, { 0, 0 } };
FDEF maSelectTable[] = { { "Component", FD_LONG }, { "Values", FD_ARRAY|FD_DOUBLE }, { "Size", FD_LONG|FD_ARRAYSIZE }, { 0, 0 } };
FDEF maSelectLinear[] = { { "Component", FD_LONG }, { "Slope", FD_DOUBLE }, { "Intercept", FD_DOUBLE }, { 0, 0 } };
FDEF maSelectIdentity[] = { { "Component", FD_LONG }, { 0, 0 } };
FDEF maSelectDiscrete[] = { { "Component", FD_LONG }, { "Values", FD_ARRAY|FD_DOUBLE }, { "Size", FD_LONG|FD_ARRAYSIZE }, { 0, 0 } };

static const struct MethodArray clRemapFXMethods[] = {
   { -20, (APTR)REMAPFX_SelectGamma, "SelectGamma", maSelectGamma, sizeof(struct rfSelectGamma) },
   { -21, (APTR)REMAPFX_SelectTable, "SelectTable", maSelectTable, sizeof(struct rfSelectTable) },
   { -22, (APTR)REMAPFX_SelectLinear, "SelectLinear", maSelectLinear, sizeof(struct rfSelectLinear) },
   { -23, (APTR)REMAPFX_SelectIdentity, "SelectIdentity", maSelectIdentity, sizeof(struct rfSelectIdentity) },
   { -24, (APTR)REMAPFX_SelectDiscrete, "SelectDiscrete", maSelectDiscrete, sizeof(struct rfSelectDiscrete) },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clRemapFXActions[] = {
   { AC_Draw, (APTR)REMAPFX_Draw },
   { AC_NewObject, (APTR)REMAPFX_NewObject },
   { 0, 0 }
};

