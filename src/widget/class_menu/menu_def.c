// Auto-generated by idl-c.fluid

static const struct FieldDef clMenuFlags[] = {
   { "ExtColumn", 0x00000004 },
   { "ShowIcons", 0x00000001 },
   { "ReverseX", 0x00000010 },
   { "NoTranslation", 0x00000400 },
   { "PointerPlacement", 0x00001000 },
   { "ReverseY", 0x00000020 },
   { "PointerXY", 0x00001000 },
   { "Cache", 0x00000800 },
   { "NoHide", 0x00000040 },
   { "PreserveBkgd", 0x00000200 },
   { "Popup", 0x00000008 },
   { "Sort", 0x00000080 },
   { "ShowKeys", 0x00000002 },
   { "ShowImages", 0x00000001 },
   { "IgnoreFocus", 0x00000100 },
   { NULL, 0 }
};

FDEF maSwitch[] = { { "TimeLapse", FD_LONG }, { 0, 0 } };
FDEF maSelectItem[] = { { "ID", FD_LONG }, { "State", FD_LONG }, { 0, 0 } };
FDEF maGetItem[] = { { "ID", FD_LONG }, { "Item", FD_OBJECTPTR|FD_RESULT }, { 0, 0 } };

static const struct MethodArray clMenuMethods[] = {
   { -1, (APTR)MENU_Switch, "Switch", maSwitch, sizeof(struct mnSwitch) },
   { -2, (APTR)MENU_SelectItem, "SelectItem", maSelectItem, sizeof(struct mnSelectItem) },
   { -3, (APTR)MENU_GetItem, "GetItem", maGetItem, sizeof(struct mnGetItem) },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clMenuActions[] = {
   { AC_ActionNotify, (APTR)MENU_ActionNotify },
   { AC_Activate, (APTR)MENU_Activate },
   { AC_Clear, (APTR)MENU_Clear },
   { AC_DataFeed, (APTR)MENU_DataFeed },
   { AC_Free, (APTR)MENU_Free },
   { AC_GetVar, (APTR)MENU_GetVar },
   { AC_Hide, (APTR)MENU_Hide },
   { AC_Init, (APTR)MENU_Init },
   { AC_MoveToPoint, (APTR)MENU_MoveToPoint },
   { AC_NewObject, (APTR)MENU_NewObject },
   { AC_Refresh, (APTR)MENU_Refresh },
   { AC_ScrollToPoint, (APTR)MENU_ScrollToPoint },
   { AC_SetVar, (APTR)MENU_SetVar },
   { AC_Show, (APTR)MENU_Show },
   { 0, 0 }
};

