// Auto-generated by idl-c.fluid

static const struct FieldDef clVectorVisibility[] = {
   { "Hidden", 0x00000000 },
   { "Visible", 0x00000001 },
   { "Collapse", 0x00000002 },
   { "Inherit", 0x00000003 },
   { NULL, 0 }
};

static const struct FieldDef clVectorFlags[] = {
   { "Disabled", 0x00000001 },
   { "HasFocus", 0x00000002 },
   { "JoinPaths", 0x00000004 },
   { "Isolated", 0x00000008 },
   { NULL, 0 }
};

static const struct FieldDef clVectorCursor[] = {
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

static const struct FieldDef clVectorPathQuality[] = {
   { "Auto", 0x00000000 },
   { "Fast", 0x00000001 },
   { "Crisp", 0x00000002 },
   { "Precise", 0x00000003 },
   { "Best", 0x00000004 },
   { NULL, 0 }
};

static const struct FieldDef clVectorColourSpace[] = {
   { "Inherit", 0x00000000 },
   { "SRGB", 0x00000001 },
   { "LinearRGB", 0x00000002 },
   { NULL, 0 }
};

FDEF maPush[] = { { "Position", FD_LONG }, { 0, 0 } };
FDEF maTrace[] = { { "Callback", FD_FUNCTIONPTR }, { "Scale", FD_DOUBLE }, { "Transform", FD_LONG }, { 0, 0 } };
FDEF maGetBoundary[] = { { "Flags", FD_LONG }, { "X", FD_DOUBLE|FD_RESULT }, { "Y", FD_DOUBLE|FD_RESULT }, { "Width", FD_DOUBLE|FD_RESULT }, { "Height", FD_DOUBLE|FD_RESULT }, { 0, 0 } };
FDEF maPointInPath[] = { { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { 0, 0 } };
FDEF maSubscribeInput[] = { { "Mask", FD_LONG }, { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF maSubscribeKeyboard[] = { { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF maSubscribeFeedback[] = { { "Mask", FD_LONG }, { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF maNewMatrix[] = { { "VectorMatrix:Transform", FD_PTR|FD_STRUCT|FD_RESOURCE|FD_RESULT }, { "End", FD_LONG }, { 0, 0 } };
FDEF maFreeMatrix[] = { { "VectorMatrix:Matrix", FD_PTR|FD_STRUCT }, { 0, 0 } };

static const struct MethodEntry clVectorMethods[] = {
   { AC(-1), (APTR)VECTOR_Push, "Push", maPush, sizeof(struct vec::Push) },
   { AC(-2), (APTR)VECTOR_Trace, "Trace", maTrace, sizeof(struct vec::Trace) },
   { AC(-3), (APTR)VECTOR_GetBoundary, "GetBoundary", maGetBoundary, sizeof(struct vec::GetBoundary) },
   { AC(-4), (APTR)VECTOR_PointInPath, "PointInPath", maPointInPath, sizeof(struct vec::PointInPath) },
   { AC(-5), (APTR)VECTOR_SubscribeInput, "SubscribeInput", maSubscribeInput, sizeof(struct vec::SubscribeInput) },
   { AC(-6), (APTR)VECTOR_SubscribeKeyboard, "SubscribeKeyboard", maSubscribeKeyboard, sizeof(struct vec::SubscribeKeyboard) },
   { AC(-7), (APTR)VECTOR_SubscribeFeedback, "SubscribeFeedback", maSubscribeFeedback, sizeof(struct vec::SubscribeFeedback) },
   { AC(-8), (APTR)VECTOR_Debug, "Debug", 0, 0 },
   { AC(-9), (APTR)VECTOR_NewMatrix, "NewMatrix", maNewMatrix, sizeof(struct vec::NewMatrix) },
   { AC(-10), (APTR)VECTOR_FreeMatrix, "FreeMatrix", maFreeMatrix, sizeof(struct vec::FreeMatrix) },
   { AC::NIL, 0, 0, 0, 0 }
};

static const struct ActionArray clVectorActions[] = {
   { AC::Disable, VECTOR_Disable },
   { AC::Draw, VECTOR_Draw },
   { AC::Enable, VECTOR_Enable },
   { AC::Free, VECTOR_Free },
   { AC::Hide, VECTOR_Hide },
   { AC::Init, VECTOR_Init },
   { AC::MoveToBack, VECTOR_MoveToBack },
   { AC::MoveToFront, VECTOR_MoveToFront },
   { AC::NewOwner, VECTOR_NewOwner },
   { AC::NewPlacement, VECTOR_NewPlacement },
   { AC::Show, VECTOR_Show },
   { AC::NIL, NULL }
};

