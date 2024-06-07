// Auto-generated by idl-c.fluid

static const struct FieldDef clThreadFlags[] = {
   { "AutoFree", 0x00000001 },
   { NULL, 0 }
};

FDEF maSetData[] = { { "Data", FD_BUFFER|FD_PTR }, { "Size", FD_LONG|FD_BUFSIZE }, { 0, 0 } };

static const struct MethodEntry clThreadMethods[] = {
   { AC(-1), (APTR)THREAD_SetData, "SetData", maSetData, sizeof(struct th::SetData) },
   { AC::NIL, 0, 0, 0, 0 }
};

static const struct ActionArray clThreadActions[] = {
   { AC::Activate, THREAD_Activate },
   { AC::Deactivate, THREAD_Deactivate },
   { AC::Free, THREAD_Free },
   { AC::FreeWarning, THREAD_FreeWarning },
   { AC::Init, THREAD_Init },
   { AC::NewObject, THREAD_NewObject },
   { AC::NIL, NULL }
};

