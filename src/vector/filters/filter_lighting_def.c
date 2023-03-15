// Auto-generated by idl-c.fluid

FDEF maSetDistantLight[] = { { "Azimuth", FD_DOUBLE }, { "Elevation", FD_DOUBLE }, { 0, 0 } };
FDEF maSetPointLight[] = { { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { "Z", FD_DOUBLE }, { 0, 0 } };
FDEF maSetSpotLight[] = { { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { "Z", FD_DOUBLE }, { "PX", FD_DOUBLE }, { "PY", FD_DOUBLE }, { "PZ", FD_DOUBLE }, { "Exponent", FD_DOUBLE }, { "ConeAngle", FD_DOUBLE }, { 0, 0 } };

static const struct MethodArray clLightingFXMethods[] = {
   { -20, (APTR)LIGHTINGFX_SetDistantLight, "SetDistantLight", maSetDistantLight, sizeof(struct ltSetDistantLight) },
   { -22, (APTR)LIGHTINGFX_SetPointLight, "SetPointLight", maSetPointLight, sizeof(struct ltSetPointLight) },
   { -21, (APTR)LIGHTINGFX_SetSpotLight, "SetSpotLight", maSetSpotLight, sizeof(struct ltSetSpotLight) },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clLightingFXActions[] = {
   { AC_Draw, LIGHTINGFX_Draw },
   { AC_Free, LIGHTINGFX_Free },
   { AC_NewObject, LIGHTINGFX_NewObject },
   { 0, NULL }
};

