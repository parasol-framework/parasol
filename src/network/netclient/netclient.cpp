/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
NetClient: Represents a single client IP address.

When a connection is opened between a client IP and a @NetSocket object, a new NetClient object will be created for 
the client's IP address if one does not already exist.  All @ClientSocket connections to that IP address are then 
tracked under the single NetClient object.

-END-

*********************************************************************************************************************/

static ERR NETCLIENT_Free(objNetClient *Self)
{
   Self->~objNetClient();
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR NETCLIENT_Init(objNetClient *Self)
{
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR NETCLIENT_NewPlacement(objNetClient *Self)
{
   new (Self) objNetClient;
   return ERR::Okay;
}

//********************************************************************************************************************

#include "netclient_def.c"

static const FieldArray clNetClientFields[] = {
   { "IP",          FDF_ARRAY|FDF_BYTE|FDF_R, nullptr, nullptr, 8 },
   { "Next",        FDF_OBJECT|FDF_R, nullptr, nullptr, CLASSID::NETCLIENT },
   { "Prev",        FDF_OBJECT|FDF_R, nullptr, nullptr, CLASSID::NETCLIENT },
   { "NetSocket",   FDF_OBJECT|FDF_R, nullptr, nullptr, CLASSID::NETSOCKET },
   { "Connections", FDF_OBJECT|FDF_R, nullptr, nullptr, CLASSID::CLIENTSOCKET },
   { "ClientData",  FDF_POINTER|FDF_RW },
   { "TotalConnections", FDF_INT|FDF_R },
   END_FIELD
};

//********************************************************************************************************************

static ERR init_netclient(void)
{
   clNetClient = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::NETCLIENT),
      fl::ClassVersion(1.0),
      fl::Name("NetClient"),
      fl::Category(CCF::NETWORK),
      fl::Actions(clNetClientActions),
      fl::Fields(clNetClientFields),
      fl::Size(sizeof(objNetClient)),
      fl::Path(MOD_PATH));

   return clNetClient ? ERR::Okay : ERR::AddClass;
}
