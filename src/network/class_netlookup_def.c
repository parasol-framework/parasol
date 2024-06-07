// Auto-generated by idl-c.fluid

static const struct FieldDef clNetLookupFlags[] = {
   { "NoCache", 0x00000001 },
   { NULL, 0 }
};

FDEF maResolveName[] = { { "HostName", FD_STR }, { 0, 0 } };
FDEF maResolveAddress[] = { { "Address", FD_STR }, { 0, 0 } };
FDEF maBlockingResolveName[] = { { "HostName", FD_STR }, { 0, 0 } };
FDEF maBlockingResolveAddress[] = { { "Address", FD_STR }, { 0, 0 } };

static const struct MethodEntry clNetLookupMethods[] = {
   { AC(-1), (APTR)NETLOOKUP_ResolveName, "ResolveName", maResolveName, sizeof(struct nl::ResolveName) },
   { AC(-2), (APTR)NETLOOKUP_ResolveAddress, "ResolveAddress", maResolveAddress, sizeof(struct nl::ResolveAddress) },
   { AC(-3), (APTR)NETLOOKUP_BlockingResolveName, "BlockingResolveName", maBlockingResolveName, sizeof(struct nl::BlockingResolveName) },
   { AC(-4), (APTR)NETLOOKUP_BlockingResolveAddress, "BlockingResolveAddress", maBlockingResolveAddress, sizeof(struct nl::BlockingResolveAddress) },
   { AC::NIL, 0, 0, 0, 0 }
};

static const struct ActionArray clNetLookupActions[] = {
   { AC::Free, NETLOOKUP_Free },
   { AC::FreeWarning, NETLOOKUP_FreeWarning },
   { AC::NewObject, NETLOOKUP_NewObject },
   { AC::NIL, NULL }
};

