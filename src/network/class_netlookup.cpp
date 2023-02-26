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
   ERROR Error;
}; // Host name or IP address is appended

static ERROR resolve_address(CSTRING, const IPAddress *, DNSEntry **);
static ERROR resolve_name(CSTRING, DNSEntry **);
static void resolve_callback(extNetLookup *, ERROR, CSTRING, IPAddress *, LONG);
#ifdef _WIN32
static ERROR cache_host(KeyStore *, CSTRING, struct hostent *, DNSEntry **);
#else
static ERROR cache_host(KeyStore *, CSTRING, struct hostent *, DNSEntry **);
static ERROR cache_host(KeyStore *, CSTRING, struct addrinfo *, DNSEntry **);
#endif

//********************************************************************************************************************
// Used for receiving asynchronous execution results (sent as a message).
// These routines execute in the main process.

static ERROR resolve_name_receiver(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   pf::Log log(__FUNCTION__);

   auto r = (resolve_buffer *)Message;

   log.traceBranch("MsgID: %d, MsgType: %d, Host: %s, Thread: %d", MsgID, MsgType, (CSTRING)(r + 1), r->ThreadID);

   extNetLookup *nl;
   if (!AccessObjectID(r->NetLookupID, 2000, &nl)) {
      {
         std::lock_guard<std::mutex> lock(*nl->ThreadLock);
         nl->Threads->erase(r->ThreadID);
      }

      DNSEntry *info;
      if (!VarGet(glHosts, (CSTRING)(r + 1), &info, NULL)) {
         nl->Info = *info;
         resolve_callback(nl, ERR_Okay, nl->Info.HostName, nl->Info.Addresses, nl->Info.TotalAddresses);
      }
      else resolve_callback(nl, ERR_Failed, NULL, NULL, 0);

      ReleaseObject(nl);
   }
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR resolve_addr_receiver(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   pf::Log log(__FUNCTION__);

   auto r = (resolve_buffer *)Message;

   log.traceBranch("MsgID: %d, MsgType: %d, Address: %s, Thread: %d", MsgID, MsgType, (CSTRING)(r + 1), r->ThreadID);

   extNetLookup *nl;
   if (!AccessObjectID(r->NetLookupID, 2000, &nl)) {
      {
         std::lock_guard<std::mutex> lock(*nl->ThreadLock);
         nl->Threads->erase(r->ThreadID);
      }

      DNSEntry *info;
      if (!VarGet(glAddresses, (CSTRING)(r + 1), &info, NULL)) {
         nl->Info = *info;
         resolve_callback(nl, ERR_Okay, nl->Info.HostName, nl->Info.Addresses, nl->Info.TotalAddresses);
      }
      else resolve_callback(nl, ERR_Failed, NULL, NULL, 0);

      ReleaseObject(nl);
   }
   return ERR_Okay;
}

//********************************************************************************************************************
// Thread routines for asynchronous name & address resolution

static ERROR thread_resolve_name(objThread *Thread)
{
   pf::Log log(__FUNCTION__);

   auto rb = (resolve_buffer *)Thread->Data;

   log.traceBranch("Thread %d resolving name %s", Thread->UID, (CSTRING)(rb + 1));

   DNSEntry *dummy;
   rb->Error = resolve_name((CSTRING)(rb + 1), &dummy);

   SendMessage(0, glResolveNameMsgID, MSF_WAIT, rb, Thread->DataSize); // See resolve_name_receiver()
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR thread_resolve_addr(objThread *Thread)
{
   pf::Log log(__FUNCTION__);

   auto rb = (resolve_buffer *)Thread->Data;

   log.traceBranch("Thread %d resolving address", Thread->UID);

   DNSEntry *dummy;
   auto ip_address = (const IPAddress *)(rb + 1);
   rb->Error = resolve_address((CSTRING)(ip_address + 1), ip_address, &dummy);

   SendMessage(0, glResolveAddrMsgID, MSF_WAIT, rb, Thread->DataSize); // See resolve_addr_receiver()
   return ERR_Okay;
}

//********************************************************************************************************************

static void notify_free_callback(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args)
{
   ((extNetLookup *)CurrentContext())->Callback.Type = CALL_NONE;
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

static ERROR NETLOOKUP_BlockingResolveAddress(extNetLookup *Self, struct nlBlockingResolveAddress *Args)
{
   pf::Log log;
   ERROR error;

   if (!Args->Address) return log.warning(ERR_NullArgs);

   log.branch("Address: %s", Args->Address);

   IPAddress ip;
   if (!netStrToAddress(Args->Address, &ip)) {
      DNSEntry *info;
      if (!(error = resolve_address(Args->Address, &ip, &info))) {
         Self->Info = *info;
         resolve_callback(Self, ERR_Okay, Self->Info.HostName, Self->Info.Addresses, Self->Info.TotalAddresses);
         return ERR_Okay;
      }
      else {
         resolve_callback(Self, error, NULL, &ip, 1);
         return error;
      }
   }
   else return log.warning(ERR_Args);
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

static ERROR NETLOOKUP_BlockingResolveName(extNetLookup *Self, struct nlResolveName *Args)
{
   pf::Log log;
   ERROR error;

   if (!Args->HostName) return log.error(ERR_NullArgs);

   log.branch("Host: %s", Args->HostName);

   DNSEntry *info;
   if (!(error = resolve_name(Args->HostName, &info))) {
      Self->Info = *info;
      resolve_callback(Self, ERR_Okay, Self->Info.HostName, Self->Info.Addresses, Self->Info.TotalAddresses);
      return ERR_Okay;
   }
   else {
      resolve_callback(Self, error, Args->HostName, NULL, 0);
      return error;
   }
}

/*********************************************************************************************************************

-ACTION-
Free: Terminate the object.

This routine may block temporarily if there are unresolved requests awaiting completion in separate threads.

*********************************************************************************************************************/

static ERROR NETLOOKUP_Free(extNetLookup *Self, APTR Void)
{
   if (Self->Threads) { delete Self->Threads; Self->Threads = NULL; }
   if (Self->ThreadLock) { delete Self->ThreadLock; Self->ThreadLock = NULL; }

   if (Self->Callback.Type IS CALL_SCRIPT) {
      UnsubscribeAction(Self->Callback.Script.Script, AC_Free);
      Self->Callback.Type = 0;
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR NETLOOKUP_FreeWarning(extNetLookup *Self, APTR Void)
{
   if (not Self->Threads->empty()) {
      pf::Log log;
      log.msg("Waiting on %d threads that remain active...", (LONG)Self->Threads->size());
      while (not Self->Threads->empty()) { // Threads will automatically remove themselves
restart:
         for (auto &id : *Self->Threads) {
            if (CheckObjectExists(id) != ERR_Okay) {
               Self->Threads->erase(id);
               goto restart;
            }
         }

         WaitTime(1, 0);
      }
   }
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR NETLOOKUP_NewObject(extNetLookup *Self, APTR Void)
{
   Self->Threads = new std::unordered_set<OBJECTID>;
   Self->ThreadLock = new std::mutex;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
ResolveAddress: Resolves an IP address to a host name.

ResolveAddress() performs a IP address resolution, converting an address to an official host name and list of
IP addresses.  The resolution process involves contacting a DNS server.  To prevent delays, asynchronous communication
is used so that the function can return immediately.  The #Callback function will be called on completion of the process.

If synchronous (blocking) operation is desired then use the @BlockingResolveAddress() method.

-INPUT-
cstr Address: IP address to be resolved, e.g. "123.111.94.82".

-ERRORS-
Okay: The IP address was resolved successfully.
Args
NullArgs
Failed: The address could not be resolved

*********************************************************************************************************************/

static ERROR NETLOOKUP_ResolveAddress(extNetLookup *Self, struct nlResolveAddress *Args)
{
   pf::Log log;

   if (!Args->Address) return log.warning(ERR_NullArgs);
   if (!Self->Callback.Type) return log.warning(ERR_FieldNotSet);

   log.branch("Address: %s", Args->Address);

   if (!(Self->Flags & NLF_NO_CACHE)) { // Use the cache if available.
      DNSEntry *info;
      if (!VarGet(glAddresses, Args->Address, &info, NULL)) {
         Self->Info = *info;
         log.trace("Cache hit for address %s", Args->Address);
         resolve_callback(Self, ERR_Okay, Self->Info.HostName, Self->Info.Addresses, Self->Info.TotalAddresses);
         return ERR_Okay;
      }
   }

   IPAddress ip;
   if (!netStrToAddress(Args->Address, &ip)) {
      LONG pkg_size = sizeof(resolve_buffer) + sizeof(IPAddress) + StrLength(Args->Address) + 1;
      if (auto th = objThread::create::integral(fl::Routine((CPTR)thread_resolve_addr),
            fl::Flags(THF_AUTO_FREE))) {
         char buffer[pkg_size];
         auto rb = (resolve_buffer *)&buffer;
         rb->NetLookupID = Self->UID;
         rb->ThreadID = th->UID;
         CopyMemory(&ip, (rb + 1), sizeof(ip));
         StrCopy(Args->Address, ((STRING)(rb + 1)) + sizeof(IPAddress));
         if ((!thSetData(th, rb, pkg_size)) and (!th->activate())) {
            std::lock_guard<std::mutex> lock(*Self->ThreadLock);
            Self->Threads->insert(th->UID);
            return ERR_Okay;
         }
         else {
            acFree(th);
            return log.warning(ERR_Failed);
         }
      }
      else return log.warning(ERR_CreateObject);
   }
   else return log.warning(ERR_Failed);
}

/*********************************************************************************************************************

-METHOD-
ResolveName: Resolves a domain name to an official host name and a list of IP addresses.

ResolveName() performs a domain name resolution, converting a domain name to an official host name and IP addresses.
The resolution process involves contacting a DNS server.  To prevent delays, asynchronous communication is used so
that the function can return immediately.  The #Callback function will be called on completion of the process.

If synchronous (blocking) operation is desired then use the @BlockingResolveName() method.

-INPUT-
cstr HostName: The host name to be resolved.

-ERRORS-
Okay:
NullArgs:
AllocMemory:
Failed:

*********************************************************************************************************************/

static ERROR NETLOOKUP_ResolveName(extNetLookup *Self, struct nlResolveName *Args)
{
   pf::Log log;

   if (!Args->HostName) return log.error(ERR_NullArgs);

   log.branch("Host: %s", Args->HostName);

   if (!(Self->Flags & NLF_NO_CACHE)) { // Use the cache if available.
      DNSEntry *info;
      if (!VarGet(glHosts, Args->HostName, &info, NULL)) {
         Self->Info = *info;
         log.trace("Cache hit for host %s", Self->Info.HostName);
         resolve_callback(Self, ERR_Okay, Self->Info.HostName, Self->Info.Addresses, Self->Info.TotalAddresses);
         return ERR_Okay;
      }
   }

   ERROR error;
   LONG pkg_size = sizeof(resolve_buffer) + StrLength(Args->HostName) + 1;
   if (auto th = objThread::create::integral(fl::Routine((CPTR)thread_resolve_name),
         fl::Flags(THF_AUTO_FREE))) {
      char buffer[pkg_size];
      auto rb = (resolve_buffer *)&buffer;
      rb->NetLookupID = Self->UID;
      rb->ThreadID = th->UID;
      StrCopy(Args->HostName, (STRING)(rb + 1));
      if ((!thSetData(th, buffer, pkg_size)) and (!th->activate())) {
         std::lock_guard<std::mutex> lock(*Self->ThreadLock);
         Self->Threads->insert(th->UID);
         return ERR_Okay;
      }
      else {
         acFree(th);
         error = ERR_Failed;
      }
   }
   else error = ERR_CreateObject;

   return log.warning(error);
}

/*********************************************************************************************************************

-FIELD-
Addresses: List of resolved IP addresses.

A list of the most recently resolved IP addresses can be read from this field.

*********************************************************************************************************************/

static ERROR GET_Addresses(extNetLookup *Self, BYTE **Value, LONG *Elements)
{
   if (Self->Info.Addresses) {
      *Value = (BYTE *)Self->Info.Addresses;
      *Elements = Self->Info.TotalAddresses;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

/*********************************************************************************************************************

-FIELD-
Callback: This function will be called on the completion of any name or address resolution.

The function referenced here will receive the results of the most recently resolved name or address.  The C/C++ prototype
is `Function(*NetLookup, ERROR Error, CSTRING HostName, IPAddress *Addresses, LONG TotalAddresses)`.

The Fluid prototype is as follows, with results readable from the #HostName and #Addresses fields:
`function(NetLookup, Error)`.

*********************************************************************************************************************/

static ERROR GET_Callback(extNetLookup *Self, FUNCTION **Value)
{
   if (Self->Callback.Type != CALL_NONE) {
      *Value = &Self->Callback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Callback(extNetLookup *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->Callback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->Callback.Script.Script, AC_Free);
      Self->Callback = *Value;
      if (Self->Callback.Type IS CALL_SCRIPT) {
         auto callback = make_function_stdc(notify_free_callback);
         SubscribeAction(Self->Callback.Script.Script, AC_Free, &callback);
      }
   }
   else Self->Callback.Type = CALL_NONE;

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
HostName: Name of the most recently resolved host.

The name of the most recently resolved host is readable from this field.

*********************************************************************************************************************/

static ERROR GET_HostName(extNetLookup *Self, CSTRING *Value)
{
   if (Self->Info.HostName) {
      *Value = Self->Info.HostName;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

//********************************************************************************************************************

static ERROR cache_host(KeyStore *Store, CSTRING Key, struct hostent *Host, DNSEntry **Cache)
{
   if ((!Host) or (!Cache)) return ERR_NullArgs;

   if (!Key) {
      if (!(Key = Host->h_name)) return ERR_Args;
   }

   pf::Log log(__FUNCTION__);

   log.debug("Key: %s, Addresses: %p (IPV6: %d)", Key, Host->h_addr_list, (Host->h_addrtype == AF_INET6));

   CSTRING real_name = Host->h_name;
   if (!real_name) real_name = Key;

   *Cache = NULL;
   if ((Host->h_addrtype != AF_INET) and (Host->h_addrtype != AF_INET6)) {
      return ERR_Args;
   }

   // Calculate the size of the data structure.

   LONG size = sizeof(DNSEntry) + ALIGN64(StrLength(real_name) + 1);
   LONG address_count = 0;
   if (Host->h_addr_list) {
      for (address_count=0; (address_count < MAX_ADDRESSES) and (Host->h_addr_list[address_count]); address_count++);
   }

   size += address_count * sizeof(IPAddress);

   // Allocate an empty key-pair and fill it.

   DNSEntry *cache;
   if (VarSetSized(Store, Key, size, &cache, NULL) != ERR_Okay) return ERR_Failed;

   char *buffer = (char *)cache;
   LONG offset = sizeof(DNSEntry);

   if (address_count > 0) {
      cache->Addresses = (IPAddress *)(buffer + offset);
      offset += address_count * sizeof(IPAddress);

      if (Host->h_addrtype IS AF_INET) {
         for (LONG i=0; i < address_count; i++) {
            ULONG addr = *((ULONG *)Host->h_addr_list[i]);
            cache->Addresses[i].Data[0] = ntohl(addr);
            cache->Addresses[i].Data[1] = 0;
            cache->Addresses[i].Data[2] = 0;
            cache->Addresses[i].Data[3] = 0;
            cache->Addresses[i].Type = IPADDR_V4;
         }
      }
      else if (Host->h_addrtype IS AF_INET6) {
         for (LONG i=0; i < address_count; i++) {
            struct in6_addr * addr = ((struct in6_addr **)Host->h_addr_list)[i];
            cache->Addresses[i].Data[0] = ((ULONG *)addr)[0];
            cache->Addresses[i].Data[1] = ((ULONG *)addr)[1];
            cache->Addresses[i].Data[2] = ((ULONG *)addr)[2];
            cache->Addresses[i].Data[3] = ((ULONG *)addr)[3];
            cache->Addresses[i].Type = IPADDR_V6;
         }
      }
   }
   else cache->Addresses = NULL;

   cache->HostName = (STRING)(buffer + offset);
   StrCopy(real_name, (STRING)cache->HostName);
   cache->TotalAddresses = address_count;

   *Cache = cache;
   return ERR_Okay;
}

#ifdef __linux__

static ERROR cache_host(KeyStore *Store, CSTRING Key, struct addrinfo *Host, DNSEntry **Cache)
{
   if ((!Host) or (!Cache)) return ERR_NullArgs;

   if (!Key) {
      if (!(Key = Host->ai_canonname)) return ERR_Args;
   }

   pf::Log log(__FUNCTION__);

   log.debug("Key: %s, Addresses: %p (IPV6: %d)", Key, Host->ai_addr, (Host->ai_family == AF_INET6));

   CSTRING real_name = Host->ai_canonname;
   if (!real_name) real_name = Key;

   *Cache = NULL;
   if ((Host->ai_family != AF_INET) and (Host->ai_family != AF_INET6)) return ERR_Args;

   // Calculate the size of the data structure.

   LONG size = sizeof(DNSEntry) + ALIGN64(StrLength(real_name) + 1);

   LONG address_count = 0;
   for (auto scan=Host; scan; scan=scan->ai_next) {
      if (scan->ai_addr) address_count++;
   }

   size += address_count * sizeof(IPAddress);

   // Allocate an empty key-pair and fill it.

   DNSEntry *cache;
   if (VarSetSized(Store, Key, size, &cache, NULL) != ERR_Okay) return ERR_Failed;

   char *buffer = (char *)cache;
   LONG offset = sizeof(DNSEntry);

   if (address_count > 0) {
      cache->Addresses = (IPAddress *)(buffer + offset);
      offset += address_count * sizeof(IPAddress);

      LONG i = 0;
      for (auto scan=Host; scan; scan=scan->ai_next) {
         if (!scan->ai_addr) continue;

         if (scan->ai_family IS AF_INET) {
            auto addr = ((struct sockaddr_in *)scan->ai_addr)->sin_addr.s_addr;
            cache->Addresses[i].Data[0] = ntohl(addr);
            cache->Addresses[i].Data[1] = 0;
            cache->Addresses[i].Data[2] = 0;
            cache->Addresses[i].Data[3] = 0;
            cache->Addresses[i].Type = IPADDR_V4;
            i++;
         }
         else if  (scan->ai_family IS AF_INET6) {
            auto addr = (struct sockaddr_in6 *)scan->ai_addr;
            cache->Addresses[i].Data[0] = ((ULONG *)addr)[0];
            cache->Addresses[i].Data[1] = ((ULONG *)addr)[1];
            cache->Addresses[i].Data[2] = ((ULONG *)addr)[2];
            cache->Addresses[i].Data[3] = ((ULONG *)addr)[3];
            cache->Addresses[i].Type = IPADDR_V6;
            i++;
         }
      }
   }
   else cache->Addresses = NULL;

   cache->HostName = (STRING)(buffer + offset);
   StrCopy(real_name, (STRING)cache->HostName);
   cache->TotalAddresses = address_count;
   *Cache = cache;
   return ERR_Okay;
}

#endif

//********************************************************************************************************************

static ERROR resolve_address(CSTRING Address, const IPAddress *IP, DNSEntry **Info)
{
#ifdef _WIN32
   struct hostent *host = win_gethostbyaddr(IP);
   if (!host) return ERR_Failed;
   return cache_host(glAddresses, Address, host, Info);
#else
   char host_name[256], service[128];
   int result;

   if (IP->Type IS IPADDR_V4) {
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
            .h_addrtype  = (IP->Type IS IPADDR_V4) ? AF_INET : AF_INET6,
            .h_length    = 0,
            .h_addr_list = NULL
         };
         return cache_host(glAddresses, Address, &host, Info);
      }
      case EAI_AGAIN:    return ERR_Retry;
      case EAI_MEMORY:   return ERR_Memory;
      case EAI_OVERFLOW: return ERR_BufferOverflow;
      case EAI_SYSTEM:   return ERR_SystemCall;
      default:           return ERR_Failed;
   }

   return ERR_Failed;
#endif
}

//********************************************************************************************************************

static ERROR resolve_name(CSTRING HostName, DNSEntry **Info)
{
   // Use the cache if available.
   if (!VarGet(glHosts, HostName, Info, NULL)) return ERR_Okay;

#ifdef __linux__
   struct addrinfo hints, *servinfo;

   ClearMemory(&hints, sizeof hints);
   hints.ai_family   = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags    = AI_CANONNAME;
   int result = getaddrinfo(HostName, NULL, &hints, &servinfo);

   switch (result) {
      case 0: {
         ERROR error = cache_host(glHosts, HostName, servinfo, Info);
         freeaddrinfo(servinfo);
         return error;
      }
      case EAI_AGAIN:  return ERR_Retry;
      case EAI_FAIL:   return ERR_Failed;
      case EAI_MEMORY: return ERR_Memory;
      case EAI_SYSTEM: return ERR_SystemCall;
      default:         return ERR_Failed;
   }

   return ERR_Failed;
#elif _WIN32
   struct hostent *host = win_gethostbyname(HostName);
   if (!host) return ERR_Failed;
   return cache_host(glHosts, HostName, host, Info);
#endif
}

//********************************************************************************************************************

static void resolve_callback(extNetLookup *Self, ERROR Error, CSTRING HostName, IPAddress *Addresses, LONG TotalAddresses)
{
   pf::Log log(__FUNCTION__);
   log.traceBranch("Host: %s", HostName);

   if (Self->Callback.Type IS CALL_STDC) {
      pf::SwitchContext context(Self->Callback.StdC.Context);
      auto routine = (ERROR (*)(extNetLookup *, ERROR, CSTRING, IPAddress *, LONG))(Self->Callback.StdC.Routine);
      routine(Self, Error, HostName, Addresses, TotalAddresses);
   }
   else if (Self->Callback.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Self->Callback.Script.Script)) {
         const ScriptArg args[] = {
            { "NetLookup", FDF_OBJECT, { .Address = Self } },
            { "Error",     FD_LONG,    { .Long    = Error } }
         };
         scCallback(script, Self->Callback.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
      }
   }
}

//********************************************************************************************************************

static const FieldArray clNetLookupFields[] = {
   { "UserData", FDF_LARGE|FDF_RW, 0, NULL, NULL },
   { "Flags",    FDF_LONG|FDF_FLAGS|FDF_RW, 0, NULL, NULL },
   // Virtual fields
   { "Callback",  FDF_FUNCTIONPTR|FDF_RW,  0, (APTR)GET_Callback, (APTR)SET_Callback },
   { "HostName",  FDF_STRING|FDF_R,        0, (APTR)GET_HostName, NULL },
   { "Addresses", FDF_STRUCT|FDF_ARRAY|FDF_R, (MAXINT)"IPAddress", (APTR)GET_Addresses, NULL },
   END_FIELD
};

#include "class_netlookup_def.c"

//********************************************************************************************************************

ERROR init_netlookup(void)
{
   clNetLookup = objMetaClass::create::global(
      fl::ClassVersion(VER_NETLOOKUP),
      fl::Name("NetLookup"),
      fl::Category(CCF_NETWORK),
      fl::Actions(clNetLookupActions),
      fl::Methods(clNetLookupMethods),
      fl::Fields(clNetLookupFields),
      fl::Size(sizeof(extNetLookup)),
      fl::Path(MOD_PATH));

   return clNetLookup ? ERR_Okay : ERR_AddClass;
}
