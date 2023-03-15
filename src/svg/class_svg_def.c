// Auto-generated by idl-c.fluid

static const struct FieldDef clSVGFlags[] = {
   { "Autoscale", 0x00000001 },
   { "Alpha", 0x00000002 },
   { NULL, 0 }
};

FDEF maRender[] = { { "Bitmap", FD_OBJECTPTR }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { 0, 0 } };

static const struct MethodArray clSVGMethods[] = {
   { -1, (APTR)SVG_Render, "Render", maRender, sizeof(struct svgRender) },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clSVGActions[] = {
   { AC_Activate, SVG_Activate },
   { AC_DataFeed, SVG_DataFeed },
   { AC_Deactivate, SVG_Deactivate },
   { AC_Free, SVG_Free },
   { AC_Init, SVG_Init },
   { AC_NewObject, SVG_NewObject },
   { AC_SaveImage, SVG_SaveImage },
   { AC_SaveToObject, SVG_SaveToObject },
   { 0, NULL }
};

