/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
Proxy: Manages user settings for proxy servers.

The proxy server class provides a global management service for a user's proxy servers.  You can alter proxy settings
manually or present the user with a dialog box to edit and create new proxies.  Scanning functions are also provided
with filtering, allowing you to scan for proxies that should be used with the user's network connection.

Proxy objects are designed to work similarly to database recordsets. Creating a new proxy object will allow you to
create a new proxy record if all required fields are set and the object is saved.

Searching through the records with the #Find() and #FindNext() methods will move the recordset through
each entry the proxy database.  You may change existing values of any proxy and then save the changes by calling the
#SaveSettings() action.
-END-

*********************************************************************************************************************/

#define PRV_PROXY

#ifdef _WIN32
#define HKEY_PROXY "\\HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings\\"
#endif

class extProxy : public objProxy {
   public:
   std::string GroupName;
   char  FindPort[16];
   BYTE  FindEnabled;
   UBYTE Find:1;
};

static objConfig *glConfig = NULL; // NOT THREAD SAFE

static ERR find_proxy(extProxy *);
static void clear_values(extProxy *);
static ERR get_record(extProxy *);


/*********************************************************************************************************************

-METHOD-
DeleteRecord: Removes a proxy from the database.

Call the DeleteRecord() method to remove a proxy from the system.  The proxy will be permanently removed from the proxy
database on the success of this function.

-ERRORS-
Okay: Proxy deleted.
-END-

*********************************************************************************************************************/

static ERR PROXY_DeleteRecord(extProxy *Self)
{
   pf::Log log;

   if ((Self->GroupName.empty()) or (!Self->Record)) return log.error(ERR::Failed);

   log.branch();

   if (Self->Host) {
      #ifdef _WIN32

      #endif
   }

   if (glConfig) { FreeResource(glConfig); glConfig = NULL; }

   if ((glConfig = objConfig::create::untracked(fl::Path("user:config/network/proxies.cfg")))) {
      glConfig->deleteGroup(Self->GroupName.c_str());
      acSaveSettings(glConfig);
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-ACTION-
Disable: Marks a proxy as disabled.

Calling the Disable() action will mark the proxy as disabled.  Disabled proxies remain in the system but are ignored by
programs that scan the database for active proxies.

The change will not come into effect until the proxy record is saved.

*********************************************************************************************************************/

static ERR PROXY_Disable(extProxy *Self)
{
   Self->Enabled = FALSE;
   return ERR::Okay;
}

/*********************************************************************************************************************

-ACTION-
Enable: Enables a proxy.

Calling the Enable() action will mark the proxy as enabled.  The change will not come into effect until the proxy record
is saved.

*********************************************************************************************************************/

static ERR PROXY_Enable(extProxy *Self)
{
   Self->Enabled = TRUE;
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
Find: Search for a proxy that matches a set of filters.

The following example searches for all proxies available for use on port 80 (HTTP).

<pre>
objProxy::create proxy;
if (proxy.ok()) {
   if (prxFind(*proxy, 80) IS ERR::Okay) {
      do {
         ...
      } while (prxFindNext(*proxy) IS ERR::Okay);
   }
}
</pre>

-INPUT-
int Port: The port number  to access.  If zero, all proxies will be returned if you perform a looped search.
int Enabled: Set to `true` to return only enabled proxies, `false` for disabled proxies or `-1` for all proxies.

-ERRORS-
Okay: A proxy was discovered.
NoSearchResult: No matching proxy was discovered.
-END-

*********************************************************************************************************************/

static ERR PROXY_Find(extProxy *Self, struct prx::Find *Args)
{
   pf::Log log;

   log.traceBranch("Port: %d, Enabled: %d", (Args) ? Args->Port : 0, (Args) ? Args->Enabled : -1);

   // Remove the previous cache of the proxy database

   if (glConfig) { FreeResource(glConfig); glConfig = NULL; }

   // Load the current proxy database into the cache

   if ((glConfig = objConfig::create::untracked(fl::Path("user:config/network/proxies.cfg")))) {
      #ifdef _WIN32
         // Remove any existing host proxy settings

         ConfigGroups *groups;
         if (glConfig->get(FID_Data, groups) IS ERR::Okay) {
            std::stack <std::string> group_list;
            for (auto& [group, keys] : groups[0]) {
               if (keys.contains("Host")) group_list.push(group);
            }

            while (group_list.size() > 0) {
               glConfig->deleteGroup(group_list.top().c_str());
               group_list.pop();
            }
         }

         // Add the host system's current proxy settings

         CSTRING value;
         bool bypass = false;
         auto task = CurrentTask();
         if (task->getEnv(HKEY_PROXY "ProxyEnable", &value) IS ERR::Okay) {
            LONG enabled = strtol(value, NULL, 0);

            // If ProxyOverride is set and == <local> then you should bypass the proxy for local addresses.

            CSTRING override;
            if (task->getEnv(HKEY_PROXY "ProxyOverride", &override) IS ERR::Okay) {
               if (pf::iequals("<local>", override)) bypass = true;
            }

            CSTRING servers;
            if ((task->getEnv(HKEY_PROXY "ProxyServer", &servers) IS ERR::Okay) and (servers[0])) {
               log.msg("Host has defined default proxies: %s", servers);

               CSTRING name = NULL;
               LONG port  = 0;
               LONG i     = 0;
               LONG index = 0;
               while (true) {
                  if (pf::startswith("ftp=", servers+i)) {
                     name = "Windows FTP";
                     port = 21;
                     index = i + 4;
                  }
                  else if (pf::startswith("http=", servers+i)) {
                     name = "Windows HTTP";
                     port = 80;
                     index = i + 5;
                  }
                  else if (pf::startswith("https=", servers+i)) {
                     name = "Windows HTTPS";
                     port = 443;
                     index = i + 6;
                  }
                  else {
                     // If a string is set with no equals, it will be a global proxy server.  Looks like "something.com:80"

                     LONG j;
                     for (j=i; servers[j] and (servers[j] != ':'); j++) {
                        if (servers[j] IS '=') break;
                     }

                     if (servers[j] IS ':') {
                        index = i;
                        name = "Windows";
                        port = 0; // Proxy applies to all ports
                     }
                     else {
                        name = NULL;
                        port = -1;
                     }
                  }

                  if ((name) and (port != -1)) {
                     LONG id, serverport;
                     std::string group;
                     char server[80];

                     id = 0;
                     glConfig->read("ID", "Value", id);
                     id = id + 1;
                     glConfig->write("ID", "Value", std::to_string(id));

                     group = std::to_string(id);

                     size_t s;
                     for (s=0; servers[index+s] and (servers[index+s] != ':') and (s < sizeof(server)-1); s++) server[s] = servers[index+s];
                     server[s] = 0;

                     if (servers[index+s] IS ':') {
                        serverport = strtol(servers + index + s + 1, NULL, 0);

                        log.trace("Discovered proxy server %s, port %d", server, serverport);

                        glConfig->write(group, "Name", name);
                        glConfig->write(group, "Server", server);

                        if (enabled > 0) glConfig->write(group, "Enabled", std::to_string(enabled));
                        else glConfig->write(group, "Enabled", std::to_string(enabled));

                        glConfig->write(group, "Port", std::to_string(port));
                        glConfig->write(group, "ServerPort", std::to_string(serverport));
                        glConfig->write(group, "Host", "1"); // Indicate that this proxy originates from host OS settings

                        i = index + s;
                     }
                  }

                  while ((servers[i]) and (servers[i] != ';')) i++;

                  if (!servers[i]) break;
                  i++;
               }
            }
         }
         else log.msg("Host does not have proxies enabled (registry setting: %s)", HKEY_PROXY);

      #endif

      if (Args) {
         if (Args->Port > 0) pf::strcopy(std::to_string(Args->Port), Self->FindPort, sizeof(Self->FindPort));
         else Self->FindPort[0] = 0;
         Self->FindEnabled = Args->Enabled;
      }
      else {
         Self->FindPort[0] = 0;
         Self->FindEnabled = -1;
      }

      Self->GroupName.clear();

      return find_proxy(Self);
   }
   else return ERR::AccessObject;
}

/*********************************************************************************************************************

-METHOD-
FindNext: Continues an initiated search.

This method continues searches that have been initiated by the #Find() method. If a proxy is found that matches
the filter, `ERR::Okay` is returned and the details of the proxy object will reflect the data of the discovered record.
`ERR::NoSearchResult` is returned if there are no more matching proxies.

-ERRORS-
Okay: A proxy was discovered.
NoSearchResult: No matching proxy was discovered.
-END-

*********************************************************************************************************************/

static ERR PROXY_FindNext(extProxy *Self)
{
   if (!Self->Find) return ERR::NoSearchResult; // Ensure that Find() was used to initiate a search

   return find_proxy(Self);
}

//********************************************************************************************************************

static ERR find_proxy(extProxy *Self)
{
   pf::Log log(__FUNCTION__);

   clear_values(Self);

   if (!glConfig) {
      log.trace("Global config not loaded.");
      return ERR::NoSearchResult;
   }

   if (!Self->Find) Self->Find = TRUE; // This is the start of the search

   ConfigGroups *groups;
   if (glConfig->get(FID_Data, groups) != ERR::Okay) return ERR::NoData;

   auto group = groups->begin();

   if (!Self->GroupName.empty()) {
      // Go to the next record
      while (group != groups->end()) {
         if (!group->first.compare(Self->GroupName)) { group++; break; }
         else group++;
      }
   }


   log.trace("Finding next proxy.  Port: '%s', Enabled: %d", Self->FindPort, Self->FindEnabled);

   while (group != groups->end()) {
      bool match = true;

      log.trace("Group: %s", group->first.c_str());

      auto keys = group->second;

      if (Self->FindPort[0]) { // Does the port match for this proxy?
         if (keys.contains("Port")) {
            auto port = keys["Port"];

            if (!port.compare("0")) { } // Port is set to 'All' (0) so the match is automatic.
            else {
               if (!pf::wildcmp(port, Self->FindPort)) {
                  log.trace("Port '%s' doesn't match requested port '%s'", port.c_str(), Self->FindPort);
                  match = false;
               }
            }
         }
      }

      if ((match) and (Self->FindEnabled != -1)) { // Does the enabled status match for this proxy?
         if (keys.contains("Enabled")) {
            auto num = std::stoi(keys["Enabled"]);
            if (Self->FindEnabled != num) {
               log.trace("Enabled state of %d does not match requested state %d.", num, Self->FindEnabled);
               match = false;
            }
         }
      }

      if ((match) and (keys.contains("NetworkFilter"))) {
         // Do any of the currently connected networks match with the filter?

         log.error("Network filters not supported yet.");
      }

      if ((match) and (keys.contains("GatewayFilter"))) {
         // Do any connected gateways match with the filter?

         log.error("Gateway filters not supported yet.");
      }

      if (match) {
         log.trace("Found a matching proxy.");
         Self->GroupName.assign(group->first);
         return get_record(Self);
      }
   }

   log.trace("No proxy matched.");

   Self->Find = FALSE;
   return ERR::NoSearchResult;
}

//********************************************************************************************************************

static ERR PROXY_Free(extProxy *Self)
{
   clear_values(Self);
   Self->~extProxy();
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR PROXY_Init(extProxy *Self)
{
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR PROXY_NewPlacement(extProxy *Self)
{
   new (Self) extProxy;
   Self->Enabled = true;
   Self->Port = 80;
   return ERR::Okay;
}

/*********************************************************************************************************************

-ACTION-
SaveSettings: Permanently saves user configurable settings for a proxy.

This action saves a user's settings for a proxy. Saving the proxy settings will make them available to the user on
subsequent logins.

Settings are saved to the user's local account under `user:config/network/proxies.cfg`.  It is possible for the
administrator to define proxy settings as the default for all users by copying the `proxies.cfg` file to the
`system:users/default/config/network/` folder.
-END-

*********************************************************************************************************************/

static ERR PROXY_SaveSettings(extProxy *Self)
{
   pf::Log log;

   if ((!Self->Server) or (!Self->ServerPort)) return log.error(ERR::FieldNotSet);

   log.branch("Host: %d", Self->Host);

   if (Self->Host) {
      #ifdef _WIN32
         objTask *task = CurrentTask();

         if (Self->Enabled) task->setEnv(HKEY_PROXY "ProxyEnable", "1");
         else task->setEnv(HKEY_PROXY "ProxyEnable", "0");

         if ((!Self->Server) or (!Self->Server[0])) {
            log.trace("Clearing proxy server value.");
            task->setEnv(HKEY_PROXY "ProxyServer", "");
         }
         else if (Self->Port IS 0) {
            // Proxy is for all ports

            char buffer[120];
            snprintf(buffer, sizeof(buffer), "%s:%d", Self->Server, Self->ServerPort);

            log.trace("Changing all-port proxy to: %s", buffer);

            task->setEnv(HKEY_PROXY "ProxyServer", buffer);
         }
         else {
            std::string portname;
            switch(Self->Port) {
               case 21: portname = "ftp"; break;
               case 80: portname = "http"; break;
               case 443: portname = "https"; break;
            }

            if (!portname.empty()) {
               CSTRING servers;
               char server_buffer[200];
               task->getEnv(HKEY_PROXY "ProxyServer", &servers);
               if (!servers) servers = "";
               pf::strcopy(servers, server_buffer, sizeof(server_buffer));

               if (auto index = pf::strisearch(portname + "=", server_buffer); index != -1) { // Entry already exists - remove it first
                  LONG end;
                  for (end=index; server_buffer[end]; end++) {
                     if (server_buffer[end] IS ';') {
                        end++;
                        break;
                     }
                  }

                  // Remove the substring by shifting remaining characters left
                  memmove(server_buffer + index, server_buffer + end, strlen(server_buffer + end) + 1);
               }

               // Add the entry to the end of the string list

               std::string server = portname + "=" + Self->Server + ":" + std::to_string(Self->ServerPort);

               STRING newlist;
               LONG end = strlen(server_buffer);
               if (AllocMemory(end + server.size() + 2, MEM::STRING|MEM::NO_CLEAR, &newlist) IS ERR::Okay) {
                  if (end > 0) {
                     pf::copymem(server_buffer, newlist, end);
                     newlist[end++] = ';';
                  }

                  pf::copymem(server.c_str(), newlist+end, server.size()+1);

                  // Save the new proxy list

                  task->setEnv(HKEY_PROXY "ProxyServer", newlist);
                  FreeResource(newlist);
               }
            }
            else log.error("Windows' host proxy settings do not support port %d", Self->Port);
         }

      #endif

      return ERR::Okay;
   }

   objConfig::create config = { fl::Path("user:config/network/proxies.cfg") };

   if (config.ok()) {
      if (!Self->GroupName.empty()) config->deleteGroup(Self->GroupName.c_str());
      else { // This is a new proxy
         LONG id = 0;
         config->read("ID", "Value", id);
         id = id + 1;
         config->write("ID", "Value", std::to_string(id));

         Self->GroupName = std::to_string(id);
         Self->Record = id;
      }

      config->write(Self->GroupName, "Port",          std::to_string(Self->Port));
      config->write(Self->GroupName, "NetworkFilter", Self->NetworkFilter);
      config->write(Self->GroupName, "GatewayFilter", Self->GatewayFilter);
      config->write(Self->GroupName, "Username",      Self->Username);
      config->write(Self->GroupName, "Password",      Self->Password);
      config->write(Self->GroupName, "Name",          Self->ProxyName);
      config->write(Self->GroupName, "Server",        Self->Server);
      config->write(Self->GroupName, "ServerPort",    std::to_string(Self->ServerPort));
      config->write(Self->GroupName, "Enabled",       std::to_string(Self->Enabled));

      objFile::create file = {
         fl::Path("user:config/network/proxies.cfg"),
         fl::Permissions(PERMIT::USER_READ|PERMIT::USER_WRITE),
         fl::Flags(FL::NEW|FL::WRITE)
      };

      if (file.ok()) return config->saveToObject(*file);
      else return ERR::CreateObject;
   }
   else return ERR::CreateObject;
}

/*********************************************************************************************************************

-FIELD-
GatewayFilter: The IP address of the gateway that the proxy is limited to.

The GatewayFilter defines the IP address of the gateway that this proxy is limited to. It is intended to limit the
results of searches performed by the #Find() method.

*********************************************************************************************************************/

static ERR SET_GatewayFilter(extProxy *Self, CSTRING Value)
{
   if (Self->GatewayFilter) { FreeResource(Self->GatewayFilter); Self->GatewayFilter = NULL; }

   if ((Value) and (Value[0])) {
      if (!(Self->GatewayFilter = pf::strclone(Value))) return ERR::AllocMemory;
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Host: If `true`, the proxy settings are derived from the host operating system's default settings.

If Host is set to `true`, the proxy settings are derived from the host operating system's default settings.  Hosted
proxies are treated differently to user proxies - they have priority, and any changes are applied directly to the host
system rather than the user's configuration.

-FIELD-
Port: Defines the ports supported by this proxy.

The Port defines the port that the proxy server is supporting, e.g. port 80 for HTTP.

*********************************************************************************************************************/

static ERR SET_Port(extProxy *Self, LONG Value)
{
   if (Value >= 0) {
      Self->Port = Value;
      return ERR::Okay;
   }
   else return ERR::OutOfRange;
}

/*********************************************************************************************************************

-FIELD-
NetworkFilter: The name of the network that the proxy is limited to.

The NetworkFilter defines the name of the network that this proxy is limited to. It is intended to limit the results of
searches performed by the #Find() method.

This filter must not be set if the proxy needs to work on an unnamed network.

*********************************************************************************************************************/

static ERR SET_NetworkFilter(extProxy *Self, CSTRING Value)
{
   if (Self->NetworkFilter) { FreeResource(Self->NetworkFilter); Self->NetworkFilter = NULL; }

   if ((Value) and (Value[0])) {
      if (!(Self->NetworkFilter = pf::strclone(Value))) return ERR::AllocMemory;
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Username: The username to use when authenticating against the proxy server.

If the proxy requires authentication, the user name may be set here to enable an automated authentication process. If
the username is not set, a dialog will be required to prompt the user for the user name before communicating with the
proxy server.

*********************************************************************************************************************/

static ERR SET_Username(extProxy *Self, CSTRING Value)
{
   if (Self->Username) { FreeResource(Self->Username); Self->Username = NULL; }

   if ((Value) and (Value[0])) {
      if (!(Self->Username = pf::strclone(Value))) return ERR::AllocMemory;
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Password: The password to use when authenticating against the proxy server.

If the proxy requires authentication, the user password may be set here to enable an automated authentication process.
If the password is not set, a dialog will need to be used to prompt the user for the password before communicating with
the proxy.

*********************************************************************************************************************/

static ERR SET_Password(extProxy *Self, CSTRING Value)
{
   if (Self->Password) { FreeResource(Self->Password); Self->Password = NULL; }

   if ((Value) and (Value[0])) {
      if (!(Self->Password = pf::strclone(Value))) return ERR::AllocMemory;
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
ProxyName: A human readable name for the proxy server entry.

A proxy can be given a human readable name by setting this field.

*********************************************************************************************************************/

static ERR SET_ProxyName(extProxy *Self, CSTRING Value)
{
   if (Self->ProxyName) { FreeResource(Self->ProxyName); Self->ProxyName = NULL; }

   if ((Value) and (Value[0])) {
      if (!(Self->ProxyName = pf::strclone(Value))) return ERR::AllocMemory;
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Server: The destination address of the proxy server - may be an IP address or resolvable domain name.

The domain name or IP address of the proxy server must be defined here.

*********************************************************************************************************************/

static ERR SET_Server(extProxy *Self, CSTRING Value)
{
   if (Self->Server) { FreeResource(Self->Server); Self->Server = NULL; }

   if ((Value) and (Value[0])) {
      if (!(Self->Server = pf::strclone(Value))) return ERR::AllocMemory;
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
ServerPort: The port that is used for proxy server communication.

The port used to communicate with the proxy server must be defined here.

*********************************************************************************************************************/

static ERR SET_ServerPort(extProxy *Self, LONG Value)
{
   pf::Log log;
   if ((Value > 0) and (Value <= 65536)) {
      Self->ServerPort = Value;
      return ERR::Okay;
   }
   else return log.error(ERR::OutOfRange);
}

/*********************************************************************************************************************

-FIELD-
Enabled: All proxies are enabled by default until this field is set to `false`.

To disable a proxy, set this field to `false` or call the #Disable() action.  This prevents the proxy from being
discovered in searches.

*********************************************************************************************************************/

static ERR SET_Enabled(extProxy *Self, LONG Value)
{
   if (Value) Self->Enabled = TRUE;
   else Self->Enabled = FALSE;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Record: The unique ID of the current proxy record.

The Record is set to the unique ID of the current proxy record.  If no record is indexed then the Record is set to
zero.

If Record is set manually, the proxy object will attempt to lookup that record.  `ERR::Okay` will be returned if the
record is found and all record fields will be updated to reflect the data of that proxy.
-END-

*********************************************************************************************************************/

static ERR SET_Record(extProxy *Self, LONG Value)
{
   clear_values(Self);
   Self->GroupName = std::to_string(Value);
   return get_record(Self);
}

//********************************************************************************************************************
// The group field must be set to the record that you want before you call this function.
//
// Also not that you must have called clear_values() at some point before this function.

static ERR get_record(extProxy *Self)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Group: %s", Self->GroupName.c_str());

   Self->Record = std::stoi(Self->GroupName);

   std::string str;
   if (glConfig->read(Self->GroupName, "Server", str) IS ERR::Okay)   {
      Self->Server = pf::strclone(str);
      if (glConfig->read(Self->GroupName, "NetworkFilter", str) IS ERR::Okay) Self->NetworkFilter = pf::strclone(str);
      if (glConfig->read(Self->GroupName, "GatewayFilter", str) IS ERR::Okay) Self->GatewayFilter = pf::strclone(str);
      if (glConfig->read(Self->GroupName, "Username", str) IS ERR::Okay)      Self->Username = pf::strclone(str);
      if (glConfig->read(Self->GroupName, "Password", str) IS ERR::Okay)      Self->Password = pf::strclone(str);
      if (glConfig->read(Self->GroupName, "Name", str) IS ERR::Okay)          Self->ProxyName = pf::strclone(str);
      glConfig->read(Self->GroupName, "Port", Self->Port);
      glConfig->read(Self->GroupName, "ServerPort", Self->ServerPort);
      glConfig->read(Self->GroupName, "Enabled", Self->Enabled);
      glConfig->read(Self->GroupName, "Host", Self->Host);
      return ERR::Okay;
   }
   else return log.error(ERR::NotFound);
}

//********************************************************************************************************************

static void clear_values(extProxy *Self)
{
   pf::Log log(__FUNCTION__);

   log.trace("");

   Self->Record     = 0;
   Self->Port       = 0;
   Self->Enabled    = 0;
   Self->ServerPort = 0;
   Self->Host       = 0;
   if (Self->NetworkFilter) { FreeResource(Self->NetworkFilter); Self->NetworkFilter = NULL; }
   if (Self->GatewayFilter) { FreeResource(Self->GatewayFilter); Self->GatewayFilter = NULL; }
   if (Self->Username)      { FreeResource(Self->Username);      Self->Username      = NULL; }
   if (Self->Password)      { FreeResource(Self->Password);      Self->Password      = NULL; }
   if (Self->ProxyName)     { FreeResource(Self->ProxyName);     Self->ProxyName     = NULL; }
   if (Self->Server)        { FreeResource(Self->Server);        Self->Server        = NULL; }
}

//********************************************************************************************************************

static const FieldDef clPorts[] = {
   { "FTP-Data",  20 },
   { "FTP",       21 },
   { "SSH",       22 },
   { "Telnet",    23 },
   { "SMTP",      25 },
   { "RSFTP",     26 },
   { "HTTP",      80 },
   { "SFTP",      115 },
   { "SQL",       118 },
   { "IRC",       194 },
   { "LDAP",      389 },
   { "HTTPS",     443 },
   { "FTPS",      990 },
   { "TelnetSSL", 992 },
   { "All",       0 },   // All ports
   { NULL, 0 }
};

static const FieldArray clProxyFields[] = {
   { "NetworkFilter", FDF_STRING|FDF_RW, NULL, SET_NetworkFilter },
   { "GatewayFilter", FDF_STRING|FDF_RW, NULL, SET_GatewayFilter },
   { "Username",      FDF_STRING|FDF_RW, NULL, SET_Username },
   { "Password",      FDF_STRING|FDF_RW, NULL, SET_Password },
   { "ProxyName",     FDF_STRING|FDF_RW, NULL, SET_ProxyName },
   { "Server",        FDF_STRING|FDF_RW, NULL, SET_Server },
   { "Port",          FDF_INT|FDF_LOOKUP|FDF_RW, NULL, SET_Port, &clPorts },
   { "ServerPort",    FDF_INT|FDF_RW, NULL, SET_ServerPort },
   { "Enabled",       FDF_INT|FDF_RW, NULL, SET_Enabled },
   { "Record",        FDF_INT|FDF_RW, NULL, SET_Record },
   END_FIELD
};

#include "class_proxy_def.c"

//********************************************************************************************************************

ERR init_proxy(void)
{
   clProxy = objMetaClass::create::global(
      fl::ClassVersion(VER_PROXY),
      fl::Name("Proxy"),
      fl::Category(CCF::NETWORK),
      fl::Actions(clProxyActions),
      fl::Methods(clProxyMethods),
      fl::Fields(clProxyFields),
      fl::Size(sizeof(extProxy)),
      fl::Path(MOD_PATH));

   return clProxy ? ERR::Okay : ERR::AddClass;
}
