// Auto-generated by idl-c.fluid

static const struct FieldDef clNetSocketState[] = {
   { "Disconnected", 0x00000000 },
   { "Connecting", 0x00000001 },
   { "ConnectingSSL", 0x00000002 },
   { "Connected", 0x00000003 },
   { NULL, 0 }
};

static const struct FieldDef clNetSocketFlags[] = {
   { "Server", 0x00000001 },
   { "SSL", 0x00000002 },
   { "MultiConnect", 0x00000004 },
   { "Debug", 0x00000010 },
   { "AsyncResolve", 0x00000008 },
   { NULL, 0 }
};

FDEF maConnect[] = { { "Address", FD_STR }, { "Port", FD_LONG }, { 0, 0 } };
FDEF maGetLocalIPAddress[] = { { "IPAddress:Address", FD_PTR|FD_STRUCT }, { 0, 0 } };
FDEF maDisconnectClient[] = { { "Client", FD_OBJECTPTR }, { 0, 0 } };
FDEF maDisconnectSocket[] = { { "Socket", FD_OBJECTPTR }, { 0, 0 } };
FDEF maReadMsg[] = { { "Message", FD_PTR|FD_RESULT }, { "Length", FD_LONG|FD_RESULT }, { "Progress", FD_LONG|FD_RESULT }, { "CRC", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF maWriteMsg[] = { { "Message", FD_BUFFER|FD_PTR }, { "Length", FD_LONG|FD_BUFSIZE }, { 0, 0 } };

static const struct MethodArray clNetSocketMethods[] = {
   { -1, (APTR)NETSOCKET_Connect, "Connect", maConnect, sizeof(struct nsConnect) },
   { -2, (APTR)NETSOCKET_GetLocalIPAddress, "GetLocalIPAddress", maGetLocalIPAddress, sizeof(struct nsGetLocalIPAddress) },
   { -3, (APTR)NETSOCKET_DisconnectClient, "DisconnectClient", maDisconnectClient, sizeof(struct nsDisconnectClient) },
   { -4, (APTR)NETSOCKET_DisconnectSocket, "DisconnectSocket", maDisconnectSocket, sizeof(struct nsDisconnectSocket) },
   { -5, (APTR)NETSOCKET_ReadMsg, "ReadMsg", maReadMsg, sizeof(struct nsReadMsg) },
   { -6, (APTR)NETSOCKET_WriteMsg, "WriteMsg", maWriteMsg, sizeof(struct nsWriteMsg) },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clNetSocketActions[] = {
   { AC_ActionNotify, (APTR)NETSOCKET_ActionNotify },
   { AC_DataFeed, (APTR)NETSOCKET_DataFeed },
   { AC_Disable, (APTR)NETSOCKET_Disable },
   { AC_Free, (APTR)NETSOCKET_Free },
   { AC_FreeWarning, (APTR)NETSOCKET_FreeWarning },
   { AC_Init, (APTR)NETSOCKET_Init },
   { AC_NewObject, (APTR)NETSOCKET_NewObject },
   { AC_Read, (APTR)NETSOCKET_Read },
   { AC_Write, (APTR)NETSOCKET_Write },
   { 0, 0 }
};

