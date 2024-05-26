/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

NOTE: The NetLookup class was created in order to support asynchronous name resolution in a way that would
be thread safe.  In essence the class is acting as a thread pool that is safely deallocated on termination.

-CLASS-
NetLookup: Resolve network IP addresses and names using Domain Name Servers.

Use the NetLookup class for resolving network names to IP addresses and vice versa.

-END-

*********************************************************************************************************************/

#define PRV_NETLOOKUP

struct resolve_buffer {
   OBJECTID NetLookupID;
   OBJECTID ThreadID;
   ERR Error;
}; // Host name or IP address is appended

static ERR resolve_address(CSTRING, const IPAddress *, DNSEntry **);
static ERR resolve_name(CSTRING, DNSEntry **);
#ifdef _WIN32
static ERR cache_host(HOSTMAP &, CSTRING, struct hostent *, DNSEntry **);
#else
static ERR cache_host(HOSTMAP &, CSTRING, struct hostent *, DNSEntry **);
static ERR cache_host(HOSTMAP &, CSTRING, struct addrinfo *, DNSEntry **);
#endif

static std::vector<IPAddress> glNoAddresses;

static void resolve_callback(extNetLookup *, ERR, const std::string & = "", std::vector<IPAddress> & = glNoAddresses);

//********************************************************************************************************************
// Used for receiving asynchronous execution results (sent as a message).
// These routines execute in the main process.

static ERR resolve_name_receiver(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   pf::Log log(__FUNCTION__);

   auto r = (resolve_buffer *)Message;

   log.traceBranch("MsgID: %d, MsgType: %d, Host: %s, Thread: %d", MsgID, MsgType, (CSTRING)(r + 1), r->ThreadID);

   extNetLookup *nl;
   if (AccessObject(r->NetLookupID, 2000, &nl) IS ERR::Okay) {
      {
         std::lock_guard<std::mutex> lock(*nl->ThreadLock);
         nl->Threads->erase(r->ThreadID);
      }

      auto it = glHosts.find((CSTRING)(r + 1));
      if (it != glHosts.end()) {
         nl->Info = it->second;
         resolve_callback(nl, ERR::Okay, nl->Info.HostName, nl->Info.Addresses);
      }
      else resolve_callback(nl, ERR::Failed);

      ReleaseObject(nl);
   }
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR resolve_addr_receiver(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   pf::Log log(__FUNCTION__);

   auto r = (resolve_buffer *)Message;

   log.traceBranch("MsgID: %d, MsgType: %d, Address: %s, Thread: %d", MsgID, MsgType, (CSTRING)(r + 1), r->ThreadID);

   extNetLookup *nl;
   if (AccessObject(r->NetLookupID, 2000, &nl) IS ERR::Okay) {
      {
         std::lock_guard<std::mutex> lock(*nl->ThreadLock);
         nl->Threads->erase(r->ThreadID);
      }

      auto it = glAddresses.find((CSTRING)(r + 1));
      if (it != glAddresses.end()) {
         nl->Info = it->second;
         resolve_callback(nl, ERR::Okay, nl->Info.HostName, nl->Info.Addresses);
      }
      else resolve_callback(nl, ERR::Failed);

      ReleaseObject(nl);
   }
   return ERR::Okay;
}

//********************************************************************************************************************
// Thread routines for asynchronous name & address resolution

static ERR thread_resolve_name(objThread *Thread)
{
   pf::Log log(__FUNCTION__);

   auto rb = (resolve_buffer *)Thread->Data;

   log.traceBranch("Thread %d resolving name %s", Thread->UID, (CSTRING)(rb + 1));

   DNSEntry *dummy;
   rb->Error = resolve_name((CSTRING)(rb + 1), &dummy);

   SendMessage(glResolveNameMsgID, MSF::WAIT, rb, Thread->DataSize); // See resolve_name_receiver()
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR thread_resolve_addr(objThread *Thread)
{
   pf::Log log(__FUNCTION__);

   auto rb = (resolve_buffer *)Thread->Data;

   log.traceBranch("Thread %d resolving address", Thread->UID);

   DNSEntry *dummy;
   auto ip_address = (const IPAddress *)(rb + 1);
   rb->Error = resolve_address((CSTRING)(ip_address + 1), ip_address, &dummy);

   SendMessage(glResolveAddrMsgID, MSF::WAIT, rb, Thread->DataSize); // See resolve_addr_receiver()
   return ERR::Okay;
}

//********************************************************************************************************************

static void notify_free_callback(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   ((extNetLookup *)CurrentContext())->Callback.clear();
}

/*********************************************************************************************************************

-METHOD-
BlockingResolveAddress: Resolves an IP address to a host name.

BlockingResolveAddress() performs an IP address resolution, converting an address to an official host name and list of
IP addresses.  The resolution process requires contact with a DNS server and this will cause the routine to block until
a response is received.

The results can be read from the #HostName field or received via the #Callback function.

-INPUT-
cstr Address: IP address to be resolved, e.g. 123.111.94.82.

-ERRORS-
Okay: The IP address was resolved successfully.
Args
NullArgs
Failed: The address could not be resolved

*********************************************************************************************************************/

static ERR NETLOOKUP_BlockingResolveAddress(extNetLookup *Self, struct nlBlockingResolveAddress *Args)
{
   pf::Log log;
   ERR error;

   if (!Args->Address) return log.warning(ERR::NullArgs);

   log.branch("Address: %s", Args->Address);

   IPAddress ip;
   if (netStrToAddress(Args->Address, &ip) IS ERR::Okay) {
      DNSEntry *info;
      if ((error = resolve_address(Args->Address, &ip, &info)) IS ERR::Okay) {
         Self->Info = *info;
         resolve_callback(Self, ERR::Okay, Self->Info.HostName, Self->Info.Addresses);
         return ERR::Okay;
      }
      else {
         resolve_callback(Self, error);
         return error;
      }
   }
   else return log.warning(ERR::Args);
}

/*********************************************************************************************************************

-METHOD-
BlockingResolveName: Resolves a domain name to an official host name and a list of IP addresses.

BlockingResolveName() performs a domain name resolution, converting a domain name to its official host name and
IP addresses.  The resolution process requires contact with a DNS server and the function will block until a
response is received or a timeout occurs.

The results can be read from the #Addresses field or received via the #Callback function.

-INPUT-
cstr HostName: The host name to be resolved.

-ERRORS-
Okay:
NullArgs:
AllocMemory:
Failed:

*********************************************************************************************************************/

static ERR NETLOOKUP_BlockingResolveName(extNetLookup *Self, struct nlResolveName *Args)
{
   pf::Log log;
   ERR error;

   if (!Args->HostName) return log.error(ERR::NullArgs);

   log.branch("Host: %s", Args->HostName);

   DNSEntry *info;
   if ((error = resolve_name(Args->HostName, &info)) IS ERR::Okay) {
      Self->Info = *info;
      resolve_callback(Self, ERR::Okay, Self->Info.HostName, Self->Info.Addresses);
      return ERR::Okay;
   }
   else {
      resolve_callback(Self, error, Args->HostName);
      return error;
   }
}

/*********************************************************************************************************************

-ACTION-
Free: Terminate the object.

This routine may block temporarily if there are unresolved requests awaiting completion in separate threads.

*********************************************************************************************************************/

static ERR NETLOOKUP_Free(extNetLookup *Self)
{
   if (Self->Threads) { delete Self->Threads; Self->Threads = NULL; }
   if (Self->ThreadLock) { delete Self->ThreadLock; Self->ThreadLock = NULL; }

   if (Self->Callback.isScript()) {
      UnsubscribeAction(Self->Callback.Context, AC_Free);
      Self->Callback.Type = CALL::NIL;
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR NETLOOKUP_FreeWarning(extNetLookup *Self)
{
   if (not Self->Threads->empty()) {
      pf::Log log;
      log.msg("Waiting on %d threads that remain active...", (LONG)Self->Threads->size());
      while (not Self->Threads->empty()) { // Threads will automatically remove themselves
restart:
         for (auto &id : *Self->Threads) {
            if (CheckObjectExists(id) != ERR::Okay) {
               Self->Threads->erase(id);
               goto restart;
            }
         }

         WaitTime(1, 0);
      }
   }
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR NETLOOKUP_NewObject(extNetLookup *Self)
{
   if (!(Self->Threads = new (std::nothrow) std::unordered_set<OBJECTID>)) return ERR::Memory;
   if (!(Self->ThreadLock = new (std::nothrow) std::mutex)) return ERR::Memory;
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
ResolveAddress: Resolves an IP address to a host name.

ResolveAddress() performs a IP address resolution, converting an address to an official host name and list of
IP addresses.  The resolution process involves contacting a DNS server.  To prevent delays, asynchronous communication
is used so that the function can return immediately.  The #Callback function will be called on completion of the process.

If synchronous (blocking) operation is desired then use the #BlockingResolveAddress() method.

-INPUT-
cstr Address: IP address to be resolved, e.g. "123.111.94.82".

-ERRORS-
Okay: The IP address was resolved successfully.
Args
NullArgs
Failed: The address could not be resolved

*********************************************************************************************************************/

static ERR NETLOOKUP_ResolveAddress(extNetLookup *Self, struct nlResolveAddress *Args)
{
   pf::Log log;

   if (!Args->Address) return log.warning(ERR::NullArgs);
   if (Self->Callback.Type IS CALL::NIL) return log.warning(ERR::FieldNotSet);

   log.branch("Address: %s", Args->Address);

   if ((Self->Flags & NLF::NO_CACHE) IS NLF::NIL) { // Use the cache if available.
      auto it = glAddresses.find(Args->Address);
      if (it != glAddresses.end()) {
         Self->Info = it->second;
         log.trace("Cache hit for address %s", Args->Address);
         resolve_callback(Self, ERR::Okay, Self->Info.HostName, Self->Info.Addresses);
         return ERR::Okay;
      }
   }

   IPAddress ip;
   if (netStrToAddress(Args->Address, &ip) IS ERR::Okay) {
      auto addr_len = StrLength(Args->Address) + 1;
      LONG pkg_size = sizeof(resolve_buffer) + sizeof(IPAddress) + addr_len;
      if (auto th = objThread::create::local(fl::Routine((CPTR)thread_resolve_addr), fl::Flags(THF::AUTO_FREE))) {
         auto buffer = std::make_unique<UBYTE[]>(pkg_size);
         auto rb = (resolve_buffer *)buffer.get();
         rb->NetLookupID = Self->UID;
         rb->ThreadID = th->UID;
         CopyMemory(&ip, (rb + 1), sizeof(ip));
         CopyMemory(Args->Address, ((STRING)(rb + 1)) + sizeof(IPAddress), addr_len);
         if ((thSetData(th, rb, pkg_size) IS ERR::Okay) and (th->activate() IS ERR::Okay)) {
            std::lock_guard<std::mutex> lock(*Self->ThreadLock);
            Self->Threads->insert(th->UID);
            return ERR::Okay;
         }
         else {
            FreeResource(th);
            return log.warning(ERR::Failed);
         }
      }
      else return log.warning(ERR::CreateObject);
   }
   else return log.warning(ERR::Failed);
}

/*********************************************************************************************************************

-METHOD-
ResolveName: Resolves a domain name to an official host name and a list of IP addresses.

ResolveName() performs a domain name resolution, converting a domain name to an official host name and IP addresses.
The resolution process involves contacting a DNS server.  To prevent delays, asynchronous communication is used so
that the function can return immediately.  The #Callback function will be called on completion of the process.

If synchronous (blocking) operation is desired then use the #BlockingResolveName() method.

-INPUT-
cstr HostName: The host name to be resolved.

-ERRORS-
Okay:
NullArgs:
AllocMemory:
Failed:

*********************************************************************************************************************/

static ERR NETLOOKUP_ResolveName(extNetLookup *Self, struct nlResolveName *Args)
{
   pf::Log log;

   if (!Args->HostName) return log.error(ERR::NullArgs);

   log.branch("Host: %s", Args->HostName);

   if ((Self->Flags & NLF::NO_CACHE) IS NLF::NIL) { // Use the cache if available.
      auto it = glHosts.find(Args->HostName);
      if (it != glHosts.end()) {
         Self->Info = it->second;
         log.trace("Cache hit for host %s", Self->Info.HostName);
         resolve_callback(Self, ERR::Okay, Self->Info.HostName, Self->Info.Addresses);
         return ERR::Okay;
      }
   }

   ERR error;
   LONG pkg_size = sizeof(resolve_buffer) + StrLength(Args->HostName) + 1;
   if (auto th = objThread::create::local(fl::Routine((CPTR)thread_resolve_name),
         fl::Flags(THF::AUTO_FREE))) {
      auto buffer = std::make_unique<UBYTE[]>(pkg_size);
      auto rb = (resolve_buffer *)buffer.get();
      rb->NetLookupID = Self->UID;
      rb->ThreadID    = th->UID;
      StrCopy(Args->HostName, (STRING)(rb + 1), pkg_size - sizeof(resolve_buffer));
      if ((thSetData(th, buffer.get(), pkg_size) IS ERR::Okay) and (th->activate() IS ERR::Okay)) {
         std::lock_guard<std::mutex> lock(*Self->ThreadLock);
         Self->Threads->insert(th->UID);
         return ERR::Okay;
      }
      else {
         FreeResource(th);
         error = ERR::Failed;
      }
   }
   else error = ERR::CreateObject;

   return log.warning(error);
}

/*********************************************************************************************************************

-FIELD-
Addresses: List of resolved IP addresses.

A list of the most recently resolved IP addresses can be read from this field.

*********************************************************************************************************************/

static ERR GET_Addresses(extNetLookup *Self, BYTE **Value, LONG *Elements)
{
   if (!Self->Info.Addresses.empty()) {
      *Value = (BYTE *)Self->Info.Addresses.data();
      *Elements = Self->Info.Addresses.size();
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

/*********************************************************************************************************************

-FIELD-
Callback: This function will be called on the completion of any name or address resolution.

The function referenced here will receive the results of the most recently resolved name or address.  The C/C++ prototype
is `Function(*NetLookup, ERR Error, const std::string &amp;HostName, const std::vector&lt;IPAddress&gt; &amp;Addresses)`.

The Fluid prototype is as follows, with results readable from the #HostName and #Addresses fields:
`function(NetLookup, Error)`.

*********************************************************************************************************************/

static ERR GET_Callback(extNetLookup *Self, FUNCTION **Value)
{
   if (Self->Callback.defined()) {
      *Value = &Self->Callback;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_Callback(extNetLookup *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->Callback.isScript()) UnsubscribeAction(Self->Callback.Context, AC_Free);
      Self->Callback = *Value;
      if (Self->Callback.isScript()) {
         SubscribeAction(Self->Callback.Context, AC_Free, C_FUNCTION(notify_free_callback));
      }
   }
   else Self->Callback.clear();

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
HostName: Name of the most recently resolved host.

The name of the most recently resolved host is readable from this field.

*********************************************************************************************************************/

static ERR GET_HostName(extNetLookup *Self, CSTRING *Value)
{
   if (!Self->Info.HostName.empty()) {
      *Value = Self->Info.HostName.c_str();
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

//********************************************************************************************************************

static ERR cache_host(HOSTMAP &Store, CSTRING Key, struct hostent *Host, DNSEntry **Cache)
{
   if ((!Host) or (!Cache)) return ERR::NullArgs;

   if (!Key) {
      if (!(Key = Host->h_name)) return ERR::Args;
   }

   pf::Log log(__FUNCTION__);

   log.detail("Key: %s, Addresses: %p (IPV6: %d)", Key, Host->h_addr_list, (Host->h_addrtype == AF_INET6));

   *Cache = NULL;
   if ((Host->h_addrtype != AF_INET) and (Host->h_addrtype != AF_INET6)) return ERR::Args;

   DNSEntry cache;
   if (!Host->h_name) cache.HostName = Key;
   else cache.HostName = Host->h_name;

   if (Host->h_addr_list) {
      if (Host->h_addrtype IS AF_INET) {
         for (unsigned i=0; Host->h_addr_list[i]; i++) {
            auto addr = *((ULONG *)Host->h_addr_list[i]);
            cache.Addresses.push_back({ ntohl(addr), 0, 0, 0, IPADDR::V4 });
         }
      }
      else if (Host->h_addrtype IS AF_INET6) {
         for (unsigned i=0; Host->h_addr_list[i]; i++) {
            auto addr = ((struct in6_addr **)Host->h_addr_list)[i];
            cache.Addresses.push_back({
               ((ULONG *)addr)[0], ((ULONG *)addr)[1], ((ULONG *)addr)[2], ((ULONG *)addr)[3], IPADDR::V6
            });
         }
      }
   }

   Store[Key] = std::move(cache);
   *Cache = &Store[Key];
   return ERR::Okay;
}

#ifdef __linux__

static ERR cache_host(HOSTMAP &Store, CSTRING Key, struct addrinfo *Host, DNSEntry **Cache)
{
   if ((!Host) or (!Cache)) return ERR::NullArgs;

   *Cache = NULL;

   if (!Key) {
      if (!(Key = Host->ai_canonname)) return ERR::Args;
   }

   pf::Log log(__FUNCTION__);
   log.detail("Key: %s, Addresses: %p (IPV6: %d)", Key, Host->ai_addr, (Host->ai_family == AF_INET6));

   DNSEntry cache;
   if (!Host->ai_canonname) cache.HostName = Key;
   else cache.HostName = Host->ai_canonname;

   if ((Host->ai_family != AF_INET) and (Host->ai_family != AF_INET6)) return ERR::Args;

   for (auto scan=Host; scan; scan=scan->ai_next) {
      if (!scan->ai_addr) continue;

      if (scan->ai_family IS AF_INET) {
         auto addr = ((struct sockaddr_in *)scan->ai_addr)->sin_addr.s_addr;
         cache.Addresses.push_back({ ntohl(addr), 0, 0, 0, IPADDR::V4 });
      }
      else if (scan->ai_family IS AF_INET6) {
         auto addr = (struct sockaddr_in6 *)scan->ai_addr;
         cache.Addresses.push_back({
            ((ULONG *)addr)[0], ((ULONG *)addr)[1], ((ULONG *)addr)[2], ((ULONG *)addr)[3], IPADDR::V6
         });
      }
   }

   Store[Key] = std::move(cache);
   *Cache = &Store[Key];
   return ERR::Okay;
}

#endif

//********************************************************************************************************************

static ERR resolve_address(CSTRING Address, const IPAddress *IP, DNSEntry **Info)
{
#ifdef _WIN32
   struct hostent *host = win_gethostbyaddr(IP);
   if (!host) return ERR::Failed;
   return cache_host(glAddresses, Address, host, Info);
#else
   char host_name[256], service[128];
   int result;

   if (IP->Type IS IPADDR::V4) {
      const struct sockaddr_in sa = {
         .sin_family = AF_INET,
         .sin_port = 0,
         .sin_addr = { .s_addr = IP->Data[0] }
      };
      result = getnameinfo((struct sockaddr *)&sa, sizeof(sa), host_name, sizeof(host_name), service, sizeof(service), NI_NAMEREQD);
   }
   else {
      const struct sockaddr_in6 sa = {
         .sin6_family   = AF_INET6,
         .sin6_port     = 0,
         .sin6_flowinfo = 0,
         .sin6_scope_id = 0
      };
      CopyMemory(IP->Data, (APTR)sa.sin6_addr.s6_addr, 16);
      result = getnameinfo((struct sockaddr *)&sa, sizeof(sa), host_name, sizeof(host_name), service, sizeof(service), NI_NAMEREQD);
   }

   switch(result) {
      case 0: {
         struct hostent host = {
            .h_name      = host_name,
            .h_addrtype  = (IP->Type IS IPADDR::V4) ? AF_INET : AF_INET6,
            .h_length    = 0,
            .h_addr_list = NULL
         };
         return cache_host(glAddresses, Address, &host, Info);
      }
      case EAI_AGAIN:    return ERR::Retry;
      case EAI_MEMORY:   return ERR::Memory;
      case EAI_OVERFLOW: return ERR::BufferOverflow;
      case EAI_SYSTEM:   return ERR::SystemCall;
      default:           return ERR::Failed;
   }

   return ERR::Failed;
#endif
}

//********************************************************************************************************************

static ERR resolve_name(CSTRING HostName, DNSEntry **Info)
{
   // Use the cache if available.
   auto it = glHosts.find(HostName);
   if (it != glHosts.end()) {
      *Info = &it->second;
      return ERR::Okay;
   }

#ifdef __linux__
   struct addrinfo hints, *servinfo;

   ClearMemory(&hints, sizeof hints);
   hints.ai_family   = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags    = AI_CANONNAME;
   int result = getaddrinfo(HostName, NULL, &hints, &servinfo);

   switch (result) {
      case 0: {
         ERR error = cache_host(glHosts, HostName, servinfo, Info);
         freeaddrinfo(servinfo);
         return error;
      }
      case EAI_AGAIN:  return ERR::Retry;
      case EAI_FAIL:   return ERR::Failed;
      case EAI_MEMORY: return ERR::Memory;
      case EAI_SYSTEM: return ERR::SystemCall;
      default:         return ERR::Failed;
   }

   return ERR::Failed;
#elif _WIN32
   struct hostent *host = win_gethostbyname(HostName);
   if (!host) return ERR::Failed;
   return cache_host(glHosts, HostName, host, Info);
#endif
}

//********************************************************************************************************************

static void resolve_callback(extNetLookup *Self, ERR Error, const std::string &HostName, std::vector<IPAddress> &Addresses)
{
   pf::Log log(__FUNCTION__);
   log.traceBranch("Host: %s", HostName.c_str());

   if (Self->Callback.isC()) {
      pf::SwitchContext context(Self->Callback.Context);
      auto routine = (ERR (*)(extNetLookup *, ERR, const std::string &, const std::vector<IPAddress> &, APTR))(Self->Callback.Routine);
      routine(Self, Error, HostName, Addresses, Self->Callback.Meta);
   }
   else if (Self->Callback.isScript()) {
      scCall(Self->Callback, std::to_array<ScriptArg>({ { "NetLookup", Self, FDF_OBJECT }, { "Error", LONG(Error) } }));
   }
}

//********************************************************************************************************************

static const FieldArray clNetLookupFields[] = {
   { "ClientData", FDF_LARGE|FDF_RW },
   { "Flags",      FDF_LONG|FDF_FLAGS|FDF_RW },
   // Virtual fields
   { "Callback",  FDF_FUNCTIONPTR|FDF_RW, GET_Callback, SET_Callback },
   { "HostName",  FDF_STRING|FDF_R, GET_HostName },
   { "Addresses", FDF_STRUCT|FDF_ARRAY|FDF_R, GET_Addresses, NULL, "IPAddress" },
   END_FIELD
};

#include "class_netlookup_def.c"

//********************************************************************************************************************

ERR init_netlookup(void)
{
   clNetLookup = objMetaClass::create::global(
      fl::ClassVersion(VER_NETLOOKUP),
      fl::Name("NetLookup"),
      fl::Category(CCF::NETWORK),
      fl::Actions(clNetLookupActions),
      fl::Methods(clNetLookupMethods),
      fl::Fields(clNetLookupFields),
      fl::Size(sizeof(extNetLookup)),
      fl::Path(MOD_PATH));

   return clNetLookup ? ERR::Okay : ERR::AddClass;
}
