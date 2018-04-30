// Auto-generated by idl-c.fluid

static const struct FieldDef clCheckBoxFlags[] = {
   { "Disabled", 0x00000001 },
   { "Hide", 0x00000002 },
   { "FadeBorder", 0x00000004 },
   { NULL, 0 }
};

static const struct FieldDef clCheckBoxAlign[] = {
   { "Bottom", 0x00000020 },
   { "Left", 0x00000001 },
   { "Horizontal", 0x00000004 },
   { "Top", 0x00000010 },
   { "Middle", 0x0000000c },
   { "Vertical", 0x00000008 },
   { "Center", 0x0000000c },
   { "Right", 0x00000002 },
   { NULL, 0 }
};

static const struct ActionArray clCheckBoxActions[] = {
   { AC_ActionNotify, (APTR)CHECKBOX_ActionNotify },
   { AC_Activate, (APTR)CHECKBOX_Activate },
   { AC_Disable, (APTR)CHECKBOX_Disable },
   { AC_Enable, (APTR)CHECKBOX_Enable },
   { AC_Focus, (APTR)CHECKBOX_Focus },
   { AC_Free, (APTR)CHECKBOX_Free },
   { AC_Hide, (APTR)CHECKBOX_Hide },
   { AC_Init, (APTR)CHECKBOX_Init },
   { AC_MoveToBack, (APTR)CHECKBOX_MoveToBack },
   { AC_MoveToFront, (APTR)CHECKBOX_MoveToFront },
   { AC_NewObject, (APTR)CHECKBOX_NewObject },
   { AC_Redimension, (APTR)CHECKBOX_Redimension },
   { AC_Resize, (APTR)CHECKBOX_Resize },
   { AC_Show, (APTR)CHECKBOX_Show },
   { 0, 0 }
};

