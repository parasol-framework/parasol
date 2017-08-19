
static const struct FieldArray clClientSocketFields[] = {
   { "ConnectTime", FDF_LARGE|FDF_R,    0, NULL, NULL },
   { "Prev",        FDF_POINTER|FDF_STRUCT|FDF_R, (MAXINT)"ClientSocket", NULL, NULL },
   { "Next",        FDF_POINTER|FDF_STRUCT|FDF_R, (MAXINT)"ClientSocket", NULL, NULL },
   { "Client",      FDF_POINTER|FDF_STRUCT|FDF_R, (MAXINT)"ClientSocket", NULL, NULL },
   { "UserData",    FDF_POINTER|FDF_R,  0, NULL, NULL },
   { "Outgoing",    FDF_FUNCTION|FDF_R, 0, NULL, NULL },
   { "Incoming",    FDF_FUNCTION|FDF_R, 0, NULL, NULL },
   { "Handle",      FDF_LONG|FDF_R,     0, NULL, NULL },
   { "MsgLen",      FDF_LONG|FDF_R,     0, NULL, NULL },
   END_FIELD
};

//****************************************************************************

static ERROR add_clientsocket(void)
{
   if (CreateObject(ID_METACLASS, 0, &clClientSocket,
      FID_BaseClassID|TLONG,    ID_CLIENTSOCKET,
      FID_ClassVersion|TDOUBLE, 1.0,
      FID_Name|TSTRING,         "ClientSocket",
      FID_Category|TLONG,       CCF_NETWORK,
      FID_Fields|TARRAY,        clClientSocketFields,
      FID_Size|TLONG,           sizeof(objClientSocket),
      FID_Path|TSTR,            MOD_PATH,
      TAGEND) != ERR_Okay) return ERR_CreateObject;

   return ERR_Okay;
}
