// Auto-generated by idl-c.fluid

FDEF maFind[] = { { "Port", FD_INT }, { "Enabled", FD_INT }, { 0, 0 } };

static const struct MethodEntry clProxyMethods[] = {
   { AC(-1), (APTR)PROXY_DeleteRecord, "DeleteRecord", 0, 0 },
   { AC(-2), (APTR)PROXY_Find, "Find", maFind, sizeof(struct prx::Find) },
   { AC(-3), (APTR)PROXY_FindNext, "FindNext", 0, 0 },
   { AC::NIL, 0, 0, 0, 0 }
};

static const struct ActionArray clProxyActions[] = {
   { AC::Disable, PROXY_Disable },
   { AC::Enable, PROXY_Enable },
   { AC::Free, PROXY_Free },
   { AC::Init, PROXY_Init },
   { AC::NewPlacement, PROXY_NewPlacement },
   { AC::SaveSettings, PROXY_SaveSettings },
   { AC::NIL, nullptr }
};

