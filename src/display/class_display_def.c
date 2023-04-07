// Auto-generated by idl-c.fluid

static const struct FieldDef clDisplayFlags[] = {
   { "ReadOnly", (LONG)0xfe300019 },
   { "Visible", 0x00000001 },
   { "AutoSave", 0x00000002 },
   { "Buffer", 0x00000004 },
   { "NoAcceleration", 0x00000008 },
   { "Bit6", 0x00000010 },
   { "Borderless", 0x00000020 },
   { "AlphaBlend", 0x00000040 },
   { "Composite", 0x00000040 },
   { "Maxsize", 0x00100000 },
   { "Refresh", 0x00200000 },
   { "Hosted", 0x02000000 },
   { "Powersave", 0x04000000 },
   { "DPMSEnabled", 0x08000000 },
   { "GTFEnabled", 0x10000000 },
   { "Flippable", 0x20000000 },
   { "CustomWindow", 0x40000000 },
   { "Maximise", (LONG)0x80000000 },
   { NULL, 0 }
};

static const struct FieldDef clDisplayDisplayType[] = {
   { "Native", 0x00000001 },
   { "X11", 0x00000002 },
   { "Windows", 0x00000003 },
   { "Gles", 0x00000004 },
   { NULL, 0 }
};

static const struct FieldDef clDisplayDPMS[] = {
   { "Default", 0x00000000 },
   { "Off", 0x00000001 },
   { "Suspend", 0x00000002 },
   { "Standby", 0x00000003 },
   { NULL, 0 }
};

FDEF maUpdatePalette[] = { { "RGBPalette:NewPalette", FD_PTR|FD_STRUCT }, { 0, 0 } };
FDEF maSetDisplay[] = { { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { "InsideWidth", FD_LONG }, { "InsideHeight", FD_LONG }, { "BitsPerPixel", FD_LONG }, { "RefreshRate", FD_DOUBLE }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF maSizeHints[] = { { "MinWidth", FD_LONG }, { "MinHeight", FD_LONG }, { "MaxWidth", FD_LONG }, { "MaxHeight", FD_LONG }, { 0, 0 } };
FDEF maSetGamma[] = { { "Red", FD_DOUBLE }, { "Green", FD_DOUBLE }, { "Blue", FD_DOUBLE }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF maSetGammaLinear[] = { { "Red", FD_DOUBLE }, { "Green", FD_DOUBLE }, { "Blue", FD_DOUBLE }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF maSetMonitor[] = { { "Name", FD_STR }, { "MinH", FD_LONG }, { "MaxH", FD_LONG }, { "MinV", FD_LONG }, { "MaxV", FD_LONG }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF maUpdateDisplay[] = { { "Bitmap", FD_OBJECTPTR }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { "XDest", FD_LONG }, { "YDest", FD_LONG }, { 0, 0 } };

static const struct MethodEntry clDisplayMethods[] = {
   { -1, (APTR)DISPLAY_WaitVBL, "WaitVBL", 0, 0 },
   { -2, (APTR)DISPLAY_UpdatePalette, "UpdatePalette", maUpdatePalette, sizeof(struct gfxUpdatePalette) },
   { -3, (APTR)DISPLAY_SetDisplay, "SetDisplay", maSetDisplay, sizeof(struct gfxSetDisplay) },
   { -4, (APTR)DISPLAY_SizeHints, "SizeHints", maSizeHints, sizeof(struct gfxSizeHints) },
   { -5, (APTR)DISPLAY_SetGamma, "SetGamma", maSetGamma, sizeof(struct gfxSetGamma) },
   { -6, (APTR)DISPLAY_SetGammaLinear, "SetGammaLinear", maSetGammaLinear, sizeof(struct gfxSetGammaLinear) },
   { -7, (APTR)DISPLAY_SetMonitor, "SetMonitor", maSetMonitor, sizeof(struct gfxSetMonitor) },
   { -8, (APTR)DISPLAY_Minimise, "Minimise", 0, 0 },
   { -9, (APTR)DISPLAY_UpdateDisplay, "UpdateDisplay", maUpdateDisplay, sizeof(struct gfxUpdateDisplay) },
   { -10, (APTR)DISPLAY_CheckXWindow, "CheckXWindow", 0, 0 },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clDisplayActions[] = {
   { AC_Activate, DISPLAY_Activate },
   { AC_Clear, DISPLAY_Clear },
   { AC_DataFeed, DISPLAY_DataFeed },
   { AC_Disable, DISPLAY_Disable },
   { AC_Draw, DISPLAY_Draw },
   { AC_Enable, DISPLAY_Enable },
   { AC_Flush, DISPLAY_Flush },
   { AC_Focus, DISPLAY_Focus },
   { AC_Free, DISPLAY_Free },
   { AC_GetVar, DISPLAY_GetVar },
   { AC_Hide, DISPLAY_Hide },
   { AC_Init, DISPLAY_Init },
   { AC_Move, DISPLAY_Move },
   { AC_MoveToBack, DISPLAY_MoveToBack },
   { AC_MoveToFront, DISPLAY_MoveToFront },
   { AC_MoveToPoint, DISPLAY_MoveToPoint },
   { AC_NewObject, DISPLAY_NewObject },
   { AC_Redimension, DISPLAY_Redimension },
   { AC_Resize, DISPLAY_Resize },
   { AC_SaveImage, DISPLAY_SaveImage },
   { AC_SaveSettings, DISPLAY_SaveSettings },
   { AC_Show, DISPLAY_Show },
   { 0, NULL }
};

