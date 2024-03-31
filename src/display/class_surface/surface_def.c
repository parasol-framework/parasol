// Auto-generated by idl-c.fluid

static const struct FieldDef clSurfaceFlags[] = {
   { "Transparent", 0x00000001 },
   { "StickToBack", 0x00000002 },
   { "StickToFront", 0x00000004 },
   { "Visible", 0x00000008 },
   { "Sticky", 0x00000010 },
   { "GrabFocus", 0x00000020 },
   { "HasFocus", 0x00000040 },
   { "Disabled", 0x00000080 },
   { "AutoQuit", 0x00000100 },
   { "Host", 0x00000200 },
   { "Precopy", 0x00000400 },
   { "Video", 0x00000800 },
   { "WriteOnly", 0x00000800 },
   { "NoHorizontal", 0x00001000 },
   { "NoVertical", 0x00002000 },
   { "Cursor", 0x00004000 },
   { "Pointer", 0x00004000 },
   { "ScrollContent", 0x00008000 },
   { "AfterCopy", 0x00010000 },
   { "ReadOnly", 0x00014040 },
   { "Volatile", 0x00014400 },
   { "FixedBuffer", 0x00020000 },
   { "PervasiveCopy", 0x00040000 },
   { "NoFocus", 0x00080000 },
   { "FixedDepth", 0x00100000 },
   { "TotalRedraw", 0x00200000 },
   { "Composite", 0x00400000 },
   { "NoPrecomposite", 0x00400000 },
   { "PostComposite", 0x00400000 },
   { "FullScreen", 0x00800000 },
   { "IgnoreFocus", 0x01000000 },
   { "InitOnly", 0x01960e81 },
   { "AspectRatio", 0x02000000 },
   { NULL, 0 }
};

static const struct FieldDef clSurfaceAlign[] = {
   { "Left", 0x00000001 },
   { "Right", 0x00000002 },
   { "Horizontal", 0x00000004 },
   { "Vertical", 0x00000008 },
   { "Center", 0x0000000c },
   { "Middle", 0x0000000c },
   { "Top", 0x00000010 },
   { "Bottom", 0x00000020 },
   { NULL, 0 }
};

static const struct FieldDef clSurfaceDimensions[] = {
   { "ScaledX", 0x00000001 },
   { "ScaledY", 0x00000002 },
   { "FixedX", 0x00000004 },
   { "X", 0x00000005 },
   { "FixedY", 0x00000008 },
   { "Y", 0x0000000a },
   { "ScaledXOffset", 0x00000010 },
   { "ScaledYOffset", 0x00000020 },
   { "FixedXOffset", 0x00000040 },
   { "XOffset", 0x00000050 },
   { "FixedYOffset", 0x00000080 },
   { "YOffset", 0x000000a0 },
   { "FixedHeight", 0x00000100 },
   { "FixedWidth", 0x00000200 },
   { "ScaledHeight", 0x00000400 },
   { "Height", 0x00000500 },
   { "HeightFlags", 0x000005a0 },
   { "VerticalFlags", 0x000005aa },
   { "ScaledWidth", 0x00000800 },
   { "Width", 0x00000a00 },
   { "WidthFlags", 0x00000a50 },
   { "HorizontalFlags", 0x00000a55 },
   { "FixedDepth", 0x00001000 },
   { "ScaledDepth", 0x00002000 },
   { "FixedZ", 0x00004000 },
   { "ScaledZ", 0x00008000 },
   { "ScaledRadiusX", 0x00010000 },
   { "FixedRadiusX", 0x00020000 },
   { "ScaledCenterX", 0x00040000 },
   { "ScaledCenterY", 0x00080000 },
   { "FixedCenterX", 0x00100000 },
   { "FixedCenterY", 0x00200000 },
   { "StatusChangeH", 0x00400000 },
   { "StatusChangeV", 0x00800000 },
   { "StatusChange", 0x00c00000 },
   { "ScaledRadiusY", 0x01000000 },
   { "ScaledRadius", 0x01010000 },
   { "FixedRadiusY", 0x02000000 },
   { "FixedRadius", 0x02020000 },
   { NULL, 0 }
};

static const struct FieldDef clSurfaceDragStatus[] = {
   { "None", 0x00000000 },
   { "Anchor", 0x00000001 },
   { "Normal", 0x00000002 },
   { NULL, 0 }
};

static const struct FieldDef clSurfaceCursor[] = {
   { "NoChange", 0x00000000 },
   { "Default", 0x00000001 },
   { "SizeBottomLeft", 0x00000002 },
   { "SizeBottomRight", 0x00000003 },
   { "SizeTopLeft", 0x00000004 },
   { "SizeTopRight", 0x00000005 },
   { "SizeLeft", 0x00000006 },
   { "SizeRight", 0x00000007 },
   { "SizeTop", 0x00000008 },
   { "SizeBottom", 0x00000009 },
   { "Crosshair", 0x0000000a },
   { "Sleep", 0x0000000b },
   { "Sizing", 0x0000000c },
   { "SplitVertical", 0x0000000d },
   { "SplitHorizontal", 0x0000000e },
   { "Magnifier", 0x0000000f },
   { "Hand", 0x00000010 },
   { "HandLeft", 0x00000011 },
   { "HandRight", 0x00000012 },
   { "Text", 0x00000013 },
   { "Paintbrush", 0x00000014 },
   { "Stop", 0x00000015 },
   { "Invisible", 0x00000016 },
   { "Custom", 0x00000017 },
   { "Draggable", 0x00000018 },
   { NULL, 0 }
};

static const struct FieldDef clSurfaceType[] = {
   { "Root", 0x00000001 },
   { NULL, 0 }
};

FDEF maInheritedFocus[] = { { "FocusID", FD_OBJECTID }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF maExpose[] = { { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF maInvalidateRegion[] = { { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { 0, 0 } };
FDEF maSetDisplay[] = { { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { "InsideWidth", FD_LONG }, { "InsideHeight", FD_LONG }, { "BitsPerPixel", FD_LONG }, { "RefreshRate", FD_DOUBLE }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF maSetOpacity[] = { { "Value", FD_DOUBLE }, { "Adjustment", FD_DOUBLE }, { 0, 0 } };
FDEF maAddCallback[] = { { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF maResetDimensions[] = { { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { "XOffset", FD_DOUBLE }, { "YOffset", FD_DOUBLE }, { "Width", FD_DOUBLE }, { "Height", FD_DOUBLE }, { "Dimensions", FD_LONG }, { 0, 0 } };
FDEF maRemoveCallback[] = { { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };

static const struct MethodEntry clSurfaceMethods[] = {
   { -1, (APTR)SURFACE_InheritedFocus, "InheritedFocus", maInheritedFocus, sizeof(struct drwInheritedFocus) },
   { -2, (APTR)SURFACE_Expose, "Expose", maExpose, sizeof(struct drwExpose) },
   { -3, (APTR)SURFACE_InvalidateRegion, "InvalidateRegion", maInvalidateRegion, sizeof(struct drwInvalidateRegion) },
   { -4, (APTR)SURFACE_SetDisplay, "SetDisplay", maSetDisplay, sizeof(struct drwSetDisplay) },
   { -5, (APTR)SURFACE_SetOpacity, "SetOpacity", maSetOpacity, sizeof(struct drwSetOpacity) },
   { -6, (APTR)SURFACE_AddCallback, "AddCallback", maAddCallback, sizeof(struct drwAddCallback) },
   { -7, (APTR)SURFACE_Minimise, "Minimise", 0, 0 },
   { -8, (APTR)SURFACE_ResetDimensions, "ResetDimensions", maResetDimensions, sizeof(struct drwResetDimensions) },
   { -9, (APTR)SURFACE_RemoveCallback, "RemoveCallback", maRemoveCallback, sizeof(struct drwRemoveCallback) },
   { -10, (APTR)SURFACE_ScheduleRedraw, "ScheduleRedraw", 0, 0 },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clSurfaceActions[] = {
   { AC_Activate, SURFACE_Activate },
   { AC_Disable, SURFACE_Disable },
   { AC_Draw, SURFACE_Draw },
   { AC_Enable, SURFACE_Enable },
   { AC_Focus, SURFACE_Focus },
   { AC_Free, SURFACE_Free },
   { AC_Hide, SURFACE_Hide },
   { AC_Init, SURFACE_Init },
   { AC_LostFocus, SURFACE_LostFocus },
   { AC_Move, SURFACE_Move },
   { AC_MoveToBack, SURFACE_MoveToBack },
   { AC_MoveToFront, SURFACE_MoveToFront },
   { AC_MoveToPoint, SURFACE_MoveToPoint },
   { AC_NewObject, SURFACE_NewObject },
   { AC_NewOwner, SURFACE_NewOwner },
   { AC_Redimension, SURFACE_Redimension },
   { AC_Resize, SURFACE_Resize },
   { AC_SaveImage, SURFACE_SaveImage },
   { AC_Scroll, SURFACE_Scroll },
   { AC_ScrollToPoint, SURFACE_ScrollToPoint },
   { AC_Show, SURFACE_Show },
   { 0, NULL }
};

