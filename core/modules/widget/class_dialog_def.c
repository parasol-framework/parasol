// Auto-generated by idl-c.fluid

static const struct FieldDef clDialogFlags[] = {
   { "Secret", 0x00000020 },
   { "Quit", 0x00000080 },
   { "Modal", 0x00000040 },
   { "Reverse", 0x00000008 },
   { "Wait", 0x00000001 },
   { "Input", 0x00000002 },
   { "OptionOn", 0x00000010 },
   { "InputRequired", 0x00000004 },
   { NULL, 0 }
};

static const struct FieldDef clDialogResponse[] = {
   { "No", 0x00000004 },
   { "Custom2", 0x00000800 },
   { "Yes", 0x00000002 },
   { "Custom4", 0x00002000 },
   { "NoAll", 0x00000020 },
   { "Okay", 0x00000008 },
   { "Cancel", 0x00000001 },
   { "YesAll", 0x00000040 },
   { "Closed", 0x00000200 },
   { "Ok", 0x00000008 },
   { "Custom3", 0x00001000 },
   { "Negative", 0x00000235 },
   { "Positive", 0x00003c4a },
   { "Quit", 0x00000010 },
   { "None", 0x00000080 },
   { "Option", 0x00000100 },
   { "Custom1", 0x00000400 },
   { "Retry", 0x00004000 },
   { NULL, 0 }
};

static const struct FieldDef clDialogType[] = {
   { "Message", 0x00000000 },
   { "Critical", 0x00000001 },
   { "Error", 0x00000002 },
   { "Warning", 0x00000003 },
   { "Attention", 0x00000004 },
   { "Alarm", 0x00000005 },
   { "Help", 0x00000006 },
   { "Info", 0x00000007 },
   { "Question", 0x00000008 },
   { "Request", 0x00000009 },
   { "Temporary", 0x0000000a },
   { NULL, 0 }
};

static const struct ActionArray clDialogActions[] = {
   { AC_ActionNotify, (APTR)DIALOG_ActionNotify },
   { AC_Activate, (APTR)DIALOG_Activate },
   { AC_DataFeed, (APTR)DIALOG_DataFeed },
   { AC_Free, (APTR)DIALOG_Free },
   { AC_GetVar, (APTR)DIALOG_GetVar },
   { AC_Init, (APTR)DIALOG_Init },
   { AC_NewObject, (APTR)DIALOG_NewObject },
   { AC_Refresh, (APTR)DIALOG_Refresh },
   { AC_SetVar, (APTR)DIALOG_SetVar },
   { AC_Show, (APTR)DIALOG_Show },
   { 0, 0 }
};

