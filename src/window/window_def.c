// Auto-generated by idl-c.fluid

static const struct FieldDef clWindowFlags[] = {
   { "Disabled", 0x00000001 },
   { "SmartLimits", 0x00000002 },
   { "Background", 0x00000004 },
   { "Video", 0x00000008 },
   { "NoMargins", 0x00000010 },
   { "Borderless", 0x00000020 },
   { "ForcePos", 0x00000040 },
   { NULL, 0 }
};

static const struct FieldDef clWindowResizeFlags[] = {
   { "Top", 0x00000001 },
   { "Left", 0x00000002 },
   { "Right", 0x00000004 },
   { "Bottom", 0x00000008 },
   { "TopLeft", 0x00000010 },
   { "TopRight", 0x00000020 },
   { "BottomLeft", 0x00000040 },
   { "BottomRight", 0x00000080 },
   { "All", 0x000000ff },
   { NULL, 0 }
};

static const struct FieldDef clWindowOrientation[] = {
   { "Any", 0x00000000 },
   { "Portrait", 0x00000001 },
   { "Landscape", 0x00000002 },
   { NULL, 0 }
};

FDEF maMaximise[] = { { "Toggle", FD_LONG }, { 0, 0 } };

static const struct MethodArray clWindowMethods[] = {
   { -1, (APTR)WINDOW_Maximise, "Maximise", maMaximise, sizeof(struct winMaximise) },
   { -2, (APTR)WINDOW_Minimise, "Minimise", 0, 0 },
   { -3, (APTR)WINDOW_Close, "Close", 0, 0 },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clWindowActions[] = {
   { AC_AccessObject, (APTR)WINDOW_AccessObject },
   { AC_ActionNotify, (APTR)WINDOW_ActionNotify },
   { AC_Activate, (APTR)WINDOW_Activate },
   { AC_Disable, (APTR)WINDOW_Disable },
   { AC_Draw, (APTR)WINDOW_Draw },
   { AC_Enable, (APTR)WINDOW_Enable },
   { AC_Focus, (APTR)WINDOW_Focus },
   { AC_Free, (APTR)WINDOW_Free },
   { AC_Hide, (APTR)WINDOW_Hide },
   { AC_Init, (APTR)WINDOW_Init },
   { AC_Move, (APTR)WINDOW_Move },
   { AC_MoveToBack, (APTR)WINDOW_MoveToBack },
   { AC_MoveToFront, (APTR)WINDOW_MoveToFront },
   { AC_MoveToPoint, (APTR)WINDOW_MoveToPoint },
   { AC_NewChild, (APTR)WINDOW_NewChild },
   { AC_NewObject, (APTR)WINDOW_NewObject },
   { AC_NewOwner, (APTR)WINDOW_NewOwner },
   { AC_Redimension, (APTR)WINDOW_Redimension },
   { AC_ReleaseObject, (APTR)WINDOW_ReleaseObject },
   { AC_Resize, (APTR)WINDOW_Resize },
   { AC_Show, (APTR)WINDOW_Show },
   { 0, 0 }
};

#undef MOD_IDL
#define MOD_IDL "c.WOR:LANDSCAPE=0x2,ANY=0x0,PORTRAIT=0x1\nc.WNF:BACKGROUND=0x4,VIDEO=0x8,DISABLED=0x1,NO_MARGINS=0x10,FORCE_POS=0x40,SMART_LIMITS=0x2,BORDERLESS=0x20\n"
