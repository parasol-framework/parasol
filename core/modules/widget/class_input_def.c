// Auto-generated by idl-c.fluid

static const struct FieldDef clInputFlags[] = {
   { "Raised", 0x00000020 },
   { "NoGfx", 0x00000004 },
   { "SelectText", 0x00000100 },
   { "Sunken", 0x00000010 },
   { "Hide", 0x00000001 },
   { "EnterTab", 0x00000400 },
   { "Secret", 0x00000200 },
   { "Disabled", 0x00000002 },
   { "NoBkgd", 0x00000008 },
   { "ActiveDraw", 0x00000040 },
   { "Commandline", 0x00000080 },
   { NULL, 0 }
};

static const struct ActionArray clInputActions[] = {
   { AC_ActionNotify, (APTR)INPUT_ActionNotify },
   { AC_DataFeed, (APTR)INPUT_DataFeed },
   { AC_Disable, (APTR)INPUT_Disable },
   { AC_Enable, (APTR)INPUT_Enable },
   { AC_Focus, (APTR)INPUT_Focus },
   { AC_Free, (APTR)INPUT_Free },
   { AC_Hide, (APTR)INPUT_Hide },
   { AC_Init, (APTR)INPUT_Init },
   { AC_MoveToBack, (APTR)INPUT_MoveToBack },
   { AC_MoveToFront, (APTR)INPUT_MoveToFront },
   { AC_NewObject, (APTR)INPUT_NewObject },
   { AC_Redimension, (APTR)INPUT_Redimension },
   { AC_Resize, (APTR)INPUT_Resize },
   { AC_Show, (APTR)INPUT_Show },
   { 0, 0 }
};

