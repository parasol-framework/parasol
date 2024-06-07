// Auto-generated by idl-c.fluid

FDEF maSelectGamma[] = { { "Component", FD_LONG }, { "Amplitude", FD_DOUBLE }, { "Offset", FD_DOUBLE }, { "Exponent", FD_DOUBLE }, { 0, 0 } };
FDEF maSelectTable[] = { { "Component", FD_LONG }, { "Values", FD_ARRAY|FD_DOUBLE }, { "Size", FD_LONG|FD_ARRAYSIZE }, { 0, 0 } };
FDEF maSelectLinear[] = { { "Component", FD_LONG }, { "Slope", FD_DOUBLE }, { "Intercept", FD_DOUBLE }, { 0, 0 } };
FDEF maSelectIdentity[] = { { "Component", FD_LONG }, { 0, 0 } };
FDEF maSelectDiscrete[] = { { "Component", FD_LONG }, { "Values", FD_ARRAY|FD_DOUBLE }, { "Size", FD_LONG|FD_ARRAYSIZE }, { 0, 0 } };
FDEF maSelectInvert[] = { { "Component", FD_LONG }, { 0, 0 } };
FDEF maSelectMask[] = { { "Component", FD_LONG }, { "Mask", FD_LONG }, { 0, 0 } };

static const struct MethodEntry clRemapFXMethods[] = {
   { AC(-20), (APTR)REMAPFX_SelectGamma, "SelectGamma", maSelectGamma, sizeof(struct rf::SelectGamma) },
   { AC(-21), (APTR)REMAPFX_SelectTable, "SelectTable", maSelectTable, sizeof(struct rf::SelectTable) },
   { AC(-22), (APTR)REMAPFX_SelectLinear, "SelectLinear", maSelectLinear, sizeof(struct rf::SelectLinear) },
   { AC(-23), (APTR)REMAPFX_SelectIdentity, "SelectIdentity", maSelectIdentity, sizeof(struct rf::SelectIdentity) },
   { AC(-24), (APTR)REMAPFX_SelectDiscrete, "SelectDiscrete", maSelectDiscrete, sizeof(struct rf::SelectDiscrete) },
   { AC(-25), (APTR)REMAPFX_SelectInvert, "SelectInvert", maSelectInvert, sizeof(struct rf::SelectInvert) },
   { AC(-26), (APTR)REMAPFX_SelectMask, "SelectMask", maSelectMask, sizeof(struct rf::SelectMask) },
   { AC::NIL, 0, 0, 0, 0 }
};

static const struct ActionArray clRemapFXActions[] = {
   { AC::Draw, REMAPFX_Draw },
   { AC::Free, REMAPFX_Free },
   { AC::NewObject, REMAPFX_NewObject },
   { AC::NIL, NULL }
};

