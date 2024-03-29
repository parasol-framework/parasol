// Auto-generated by idl-c.fluid

static const struct FieldDef clScriptFlags[] = {
   { "ExitOnError", 0x00000001 },
   { "LogAll", 0x00000002 },
   { NULL, 0 }
};

FDEF maExec[] = { { "Procedure", FD_STR }, { "ScriptArg:Args", FD_PTR|FD_STRUCT }, { "TotalArgs", FD_LONG }, { 0, 0 } };
FDEF maDerefProcedure[] = { { "Procedure", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF maCallback[] = { { "ProcedureID", FD_LARGE }, { "ScriptArg:Args", FD_PTR|FD_STRUCT }, { "TotalArgs", FD_LONG }, { "Error", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF maGetProcedureID[] = { { "Procedure", FD_STR }, { "ProcedureID", FD_LARGE|FD_RESULT }, { 0, 0 } };

static const struct MethodEntry clScriptMethods[] = {
   { -1, (APTR)SCRIPT_Exec, "Exec", maExec, sizeof(struct scExec) },
   { -2, (APTR)SCRIPT_DerefProcedure, "DerefProcedure", maDerefProcedure, sizeof(struct scDerefProcedure) },
   { -3, (APTR)SCRIPT_Callback, "Callback", maCallback, sizeof(struct scCallback) },
   { -4, (APTR)SCRIPT_GetProcedureID, "GetProcedureID", maGetProcedureID, sizeof(struct scGetProcedureID) },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clScriptActions[] = {
   { AC_Activate, SCRIPT_Activate },
   { AC_DataFeed, SCRIPT_DataFeed },
   { AC_Free, SCRIPT_Free },
   { AC_GetVar, SCRIPT_GetVar },
   { AC_Init, SCRIPT_Init },
   { AC_NewObject, SCRIPT_NewObject },
   { AC_Reset, SCRIPT_Reset },
   { AC_SetVar, SCRIPT_SetVar },
   { 0, NULL }
};

