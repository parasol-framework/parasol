// Auto-generated by idl-c.fluid

static const struct FieldDef clNetLookupFlags[] = {
   { "NoCache", 0x00000001 },
   { NULL, 0 }
};

FDEF maResolveName[] = { { "HostName", FD_STR }, { 0, 0 } };
FDEF maResolveAddress[] = { { "Address", FD_STR }, { 0, 0 } };
FDEF maBlockingResolveName[] = { { "HostName", FD_STR }, { 0, 0 } };
FDEF maBlockingResolveAddress[] = { { "Address", FD_STR }, { 0, 0 } };

static const struct MethodArray clNetLookupMethods[] = {
   { -1, (APTR)NETLOOKUP_ResolveName, "ResolveName", maResolveName, sizeof(struct nlResolveName) },
   { -2, (APTR)NETLOOKUP_ResolveAddress, "ResolveAddress", maResolveAddress, sizeof(struct nlResolveAddress) },
   { -3, (APTR)NETLOOKUP_BlockingResolveName, "BlockingResolveName", maBlockingResolveName, sizeof(struct nlBlockingResolveName) },
   { -4, (APTR)NETLOOKUP_BlockingResolveAddress, "BlockingResolveAddress", maBlockingResolveAddress, sizeof(struct nlBlockingResolveAddress) },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clNetLookupActions[] = {
   { AC_Free, (APTR)NETLOOKUP_Free },
   { AC_FreeWarning, (APTR)NETLOOKUP_FreeWarning },
   { AC_NewObject, (APTR)NETLOOKUP_NewObject },
   { 0, NULL }
};

