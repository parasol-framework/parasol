// Auto-generated by idl-c.fluid

FDEF maFind[] = { { "Port", FD_LONG }, { "Enabled", FD_LONG }, { 0, 0 } };

static const struct MethodEntry clProxyMethods[] = {
   { -1, (APTR)PROXY_Delete, "Delete", 0, 0 },
   { -2, (APTR)PROXY_Find, "Find", maFind, sizeof(struct prx::Find) },
   { -3, (APTR)PROXY_FindNext, "FindNext", 0, 0 },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clProxyActions[] = {
   { AC_Disable, PROXY_Disable },
   { AC_Enable, PROXY_Enable },
   { AC_Free, PROXY_Free },
   { AC_Init, PROXY_Init },
   { AC_NewObject, PROXY_NewObject },
   { AC_SaveSettings, PROXY_SaveSettings },
   { 0, NULL }
};

