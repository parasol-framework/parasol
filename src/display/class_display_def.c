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
   { "GrabControllers", 0x00000080 },
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
   { "Wingdi", 0x00000003 },
   { "Gles", 0x00000004 },
   { NULL, 0 }
};

static const struct FieldDef clDisplayPowerMode[] = {
   { "Default", 0x00000000 },
   { "Off", 0x00000001 },
   { "Suspend", 0x00000002 },
   { "Standby", 0x00000003 },
   { NULL, 0 }
};

FDEF maUpdatePalette[] = { { "RGBPalette:NewPalette", FD_PTR|FD_STRUCT }, { 0, 0 } };
FDEF maSetDisplay[] = { { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { "InsideWidth", FD_LONG }, { "InsideHeight", FD_LONG }, { "BitsPerPixel", FD_LONG }, { "RefreshRate", FD_DOUBLE }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF maSizeHints[] = { { "MinWidth", FD_LONG }, { "MinHeight", FD_LONG }, { "MaxWidth", FD_LONG }, { "MaxHeight", FD_LONG }, { "EnforceAspect", FD_LONG }, { 0, 0 } };
FDEF maSetGamma[] = { { "Red", FD_DOUBLE }, { "Green", FD_DOUBLE }, { "Blue", FD_DOUBLE }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF maSetGammaLinear[] = { { "Red", FD_DOUBLE }, { "Green", FD_DOUBLE }, { "Blue", FD_DOUBLE }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF maSetMonitor[] = { { "Name", FD_STR }, { "MinH", FD_LONG }, { "MaxH", FD_LONG }, { "MinV", FD_LONG }, { "MaxV", FD_LONG }, { "Flags", FD_LONG }, { 0, 0 } };

static const struct MethodEntry clDisplayMethods[] = {
   { AC(-1), (APTR)DISPLAY_WaitVBL, "WaitVBL", 0, 0 },
   { AC(-2), (APTR)DISPLAY_UpdatePalette, "UpdatePalette", maUpdatePalette, sizeof(struct gfx::UpdatePalette) },
   { AC(-3), (APTR)DISPLAY_SetDisplay, "SetDisplay", maSetDisplay, sizeof(struct gfx::SetDisplay) },
   { AC(-4), (APTR)DISPLAY_SizeHints, "SizeHints", maSizeHints, sizeof(struct gfx::SizeHints) },
   { AC(-5), (APTR)DISPLAY_SetGamma, "SetGamma", maSetGamma, sizeof(struct gfx::SetGamma) },
   { AC(-6), (APTR)DISPLAY_SetGammaLinear, "SetGammaLinear", maSetGammaLinear, sizeof(struct gfx::SetGammaLinear) },
   { AC(-7), (APTR)DISPLAY_SetMonitor, "SetMonitor", maSetMonitor, sizeof(struct gfx::SetMonitor) },
   { AC(-8), (APTR)DISPLAY_Minimise, "Minimise", 0, 0 },
   { AC(-9), (APTR)DISPLAY_CheckXWindow, "CheckXWindow", 0, 0 },
   { AC::NIL, 0, 0, 0, 0 }
};

static const struct ActionArray clDisplayActions[] = {
   { AC::Activate, DISPLAY_Activate },
   { AC::Clear, DISPLAY_Clear },
   { AC::DataFeed, DISPLAY_DataFeed },
   { AC::Disable, DISPLAY_Disable },
   { AC::Draw, DISPLAY_Draw },
   { AC::Enable, DISPLAY_Enable },
   { AC::Flush, DISPLAY_Flush },
   { AC::Focus, DISPLAY_Focus },
   { AC::Free, DISPLAY_Free },
   { AC::GetKey, DISPLAY_GetKey },
   { AC::Hide, DISPLAY_Hide },
   { AC::Init, DISPLAY_Init },
   { AC::Move, DISPLAY_Move },
   { AC::MoveToBack, DISPLAY_MoveToBack },
   { AC::MoveToFront, DISPLAY_MoveToFront },
   { AC::MoveToPoint, DISPLAY_MoveToPoint },
   { AC::NewObject, DISPLAY_NewObject },
   { AC::NewPlacement, DISPLAY_NewPlacement },
   { AC::Redimension, DISPLAY_Redimension },
   { AC::Resize, DISPLAY_Resize },
   { AC::SaveImage, DISPLAY_SaveImage },
   { AC::SaveSettings, DISPLAY_SaveSettings },
   { AC::Show, DISPLAY_Show },
   { AC::NIL, NULL }
};

