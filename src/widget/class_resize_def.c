// Auto-generated by idl-c.fluid

static const struct FieldDef clResizeButton[] = {
   { "DigitalX", 0x00000001 },
   { "DigitalY", 0x00000002 },
   { "Button1", 0x00000003 },
   { "Lmb", 0x00000003 },
   { "Button2", 0x00000004 },
   { "Rmb", 0x00000004 },
   { "Button3", 0x00000005 },
   { "Mmb", 0x00000005 },
   { "Button4", 0x00000006 },
   { "Button5", 0x00000007 },
   { "Button6", 0x00000008 },
   { "Button7", 0x00000009 },
   { "Button8", 0x0000000a },
   { "Button9", 0x0000000b },
   { "Button10", 0x0000000c },
   { "TriggerLeft", 0x0000000d },
   { "TriggerRight", 0x0000000e },
   { "ButtonStart", 0x0000000f },
   { "ButtonSelect", 0x00000010 },
   { "LeftBumper1", 0x00000011 },
   { "LeftBumper2", 0x00000012 },
   { "RightBumper1", 0x00000013 },
   { "RightBumper2", 0x00000014 },
   { "AnalogX", 0x00000015 },
   { "AnalogY", 0x00000016 },
   { "AnalogZ", 0x00000017 },
   { "Analog2X", 0x00000018 },
   { "Analog2Y", 0x00000019 },
   { "Analog2Z", 0x0000001a },
   { "Wheel", 0x0000001b },
   { "WheelTilt", 0x0000001c },
   { "PenTiltVertical", 0x0000001d },
   { "PenTiltHorizontal", 0x0000001e },
   { "AbsX", 0x0000001f },
   { "AbsY", 0x00000020 },
   { "EnteredSurface", 0x00000021 },
   { "LeftSurface", 0x00000022 },
   { "Pressure", 0x00000023 },
   { "DeviceTiltX", 0x00000024 },
   { "DeviceTiltY", 0x00000025 },
   { "DeviceTiltZ", 0x00000026 },
   { "DisplayEdge", 0x00000027 },
   { NULL, 0 }
};

static const struct FieldDef clResizeDirection[] = {
   { "Left", 0x00000004 },
   { "All", 0x0000000f },
   { "Down", 0x00000001 },
   { "Up", 0x00000002 },
   { "Right", 0x00000008 },
   { NULL, 0 }
};

static const struct FieldDef clResizeBorder[] = {
   { "Left", 0x00000002 },
   { "TopRight", 0x00000020 },
   { "BottomLeft", 0x00000040 },
   { "Right", 0x00000004 },
   { "BottomRight", 0x00000080 },
   { "Bottom", 0x00000008 },
   { "Top", 0x00000001 },
   { "TopLeft", 0x00000010 },
   { "All", 0x000000ff },
   { NULL, 0 }
};

static const struct ActionArray clResizeActions[] = {
   { AC_DataFeed, (APTR)RESIZE_DataFeed },
   { AC_Free, (APTR)RESIZE_Free },
   { AC_Init, (APTR)RESIZE_Init },
   { AC_NewObject, (APTR)RESIZE_NewObject },
   { 0, 0 }
};

