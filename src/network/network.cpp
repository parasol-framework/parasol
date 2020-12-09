/***************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-MODULE-
Network: Provides miscellaneous network functions and hosts the NetSocket and ClientSocket classes.

The Network module exports a few miscellaneous networking functions.  For core network functionality surrounding
sockets and HTTP, please refer to the @NetSocket and @HTTP classes.
-END-

*****************************************************************************/

//#define DEBUG

#define PRV_PROXY
#define PRV_NETWORK
#define PRV_NETWORK_MODULE
#define PRV_NETSOCKET
#define PRV_CLIENTSOCKET
#define NO_NETRECURSION

#include <stdio.h>
#include <sys/types.h>

#include <parasol/main.h>
#include <parasol/modules/network.h>

#ifdef ENABLE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#endif

#include <unordered_set>
#include <stack>
#include <mutex>

#ifdef __linux__
typedef LONG SOCKET_HANDLE;
#elif _WIN32
typedef ULONG SOCKET_HANDLE; // NOTE: declared as ULONG instead of SOCKET for now to avoid including winsock.h
#else
#error "No support for this platform"
#endif

#ifdef _WIN32
   #define INADDR_NONE 0xffffffff

   #define SOCK_STREAM 1
   #define SOCK_DGRAM 2

   struct  hostent {
      char	 *h_name;
      char	 **h_aliases;
      short h_addrtype;
      short h_length;
      char  **h_addr_list;
   #define h_addr h_addr_list[0]
   };

   struct in_addr {
      union {
         struct { UBYTE s_b1,s_b2,s_b3,s_b4; } S_un_b;
         struct { UWORD s_w1,s_w2; } S_un_w;
         ULONG S_addr;
      } S_un;
   #define s_addr  S_un.S_addr
   #define s_host  S_un.S_un_b.s_b2
   #define s_net   S_un.S_un_b.s_b1
   #define s_imp   S_un.S_un_w.s_w2
   #define s_impno S_un.S_un_b.s_b4
   #define s_lh    S_un.S_un_b.s_b3
   };

   struct sockaddr_in {
      short  sin_family;
      UWORD  sin_port;
      struct in_addr sin_addr;
      char   sin_zero[8];
   };

   struct addrinfo {
     int             ai_flags;
     int             ai_family;
     int             ai_socktype;
     int             ai_protocol;
     size_t          ai_addrlen;
     char            *ai_canonname;
     struct sockaddr *ai_addr;
     struct addrinfo *ai_next;
   };

   struct in6_addr {
      unsigned char   s6_addr[16];   // IPv6 address
   };

   #define NOHANDLE     (ULONG)(~0)
   #define SOCKET_ERROR (-1)
   #define AF_INET      2
   #define AF_INET6     23
   #define INADDR_ANY   0
   #define MSG_PEEK     2

   #define CLOSESOCKET(a) win_closesocket(a);
#endif

#ifdef __linux__
   #include <arpa/inet.h>
   #include <netdb.h>
   #include <unistd.h>
   #include <fcntl.h>
   #include <sys/ioctl.h>
   #include <errno.h>
   #include <string.h>

   #define NOHANDLE -1
   #define CLOSESOCKET(a) close(a)

#elif _WIN32

   #include <string.h>

   #define htons win_htons
   #define htonl win_htonl
   #define ntohs win_ntohs
   #define ntohl win_ntohl

#else
   #error "No support for this platform"
#endif

//****************************************************************************

enum {
   SSL_NOT_BUSY=0,
   SSL_HANDSHAKE_READ,
   SSL_HANDSHAKE_WRITE
};

#ifdef _WIN32
   #include "win32/winsockwrappers.h"
#endif

#include "module_def.c"

struct CoreBase *CoreBase;
static OBJECTPTR glModule = NULL;

struct dns_cache {
   CSTRING HostName;
   IPAddress *Addresses;
   LONG   AddressCount;
};

#ifdef ENABLE_SSL
static BYTE ssl_init = FALSE;

static ERROR sslConnect(objNetSocket *);
static void sslDisconnect(objNetSocket *);
static ERROR sslInit(void);
static ERROR sslLinkSocket(objNetSocket *);
static ERROR sslSetup(objNetSocket *);
#endif

static OBJECTPTR clProxy = NULL;
static OBJECTPTR clNetSocket = NULL;
static OBJECTPTR clClientSocket = NULL;
static KeyStore *glDNS = NULL;
static LONG glResolveNameMsgID = 0;
static LONG glResolveAddrMsgID = 0;

static void client_server_incoming(SOCKET_HANDLE, rkNetSocket *);
static void resolve_callback(LARGE, FUNCTION *, ERROR, CSTRING, IPAddress *, LONG);
static BYTE check_machine_name(CSTRING HostName) __attribute__((unused));
static ERROR cache_host(CSTRING, struct hostent *, struct dns_cache **);
#ifdef __linux__
static ERROR cache_host(CSTRING, struct addrinfo *, struct dns_cache **);
#endif
static ERROR add_netsocket(void);
static ERROR add_clientsocket(void);
static ERROR init_proxy(void);

struct resolve_name_buffer {
   LARGE client_data;
   FUNCTION callback;
}; // Host name is appended

struct resolve_addr_buffer {
   LARGE client_data;
   FUNCTION callback;
}; // Address is appended

static MsgHandler *glResolveNameHandler = NULL;
static MsgHandler *glResolveAddrHandler = NULL;
static std::unordered_set<OBJECTPTR> glThreads;
static std::mutex glThreadLock;

//***************************************************************************
// Used for receiving asynchronous execution results (sent as a message) from netResolveName() and netResolveAddress().
// These routines execute in the main process.

static ERROR resolve_name_receiver(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   auto rnb = (resolve_name_buffer *)Message;
   parasol::Log log(__FUNCTION__);
   log.traceBranch("MsgID: %d, MsgType: %d, Message: %p, Host: %s", MsgID, MsgType, Message, (CSTRING)(rnb + 1));
   // This call will cause the cached DNS result to be pulled and forwarded to the user's callback.
   netResolveName((CSTRING)(rnb + 1), NSF_SYNCHRONOUS, &rnb->callback, rnb->client_data);
   return ERR_Okay;
}

static ERROR resolve_addr_receiver(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   auto rab = (resolve_addr_buffer *)Message;
   netResolveAddress((CSTRING)(rab + 1), NSF_SYNCHRONOUS, &rab->callback, rab->client_data);
   return ERR_Okay;
}

//***************************************************************************
// Thread routine for asynchronous calls to netResolveName() and netResolveAddress()

static ERROR thread_resolve_name(objThread *Thread)
{
   auto rnb = (resolve_name_buffer *)Thread->Data;
   netResolveName((CSTRING)(rnb + 1), NSF_SYNCHRONOUS, NULL, 0); // Blocking

   // Signal back to the main thread so that the callback is executed in the correct context.
   // Part of the trick to this process is that netResolveName() will have cached the IP results, so they
   // will be retrievable when the main thread gets the message.

   SendMessage(0, glResolveNameMsgID, MSF_WAIT, rnb, Thread->DataSize);

   std::lock_guard<std::mutex> lock(glThreadLock);
   glThreads.erase(&Thread->Head);
   return ERR_Okay;
}

static ERROR thread_resolve_addr(objThread *Thread)
{
   auto rab = (resolve_addr_buffer *)Thread->Data;
   netResolveAddress((CSTRING)(rab + 1), NSF_SYNCHRONOUS, NULL, 0); // Blocking

   SendMessage(0, glResolveAddrMsgID, MSF_WAIT, rab, Thread->DataSize);

   std::lock_guard<std::mutex> lock(glThreadLock);
   glThreads.erase(&Thread->Head);
   return ERR_Okay;
}

//***************************************************************************

ERROR MODInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   parasol::Log log;

   CoreBase = argCoreBase;

   GetPointer(argModule, FID_Master, &glModule);

   glDNS = VarNew(64, KSF_THREAD_SAFE);

   if (add_netsocket()) return ERR_AddClass;
   if (add_clientsocket()) return ERR_AddClass;
   if (init_proxy()) return ERR_AddClass;

   glResolveNameMsgID = AllocateID(IDTYPE_MESSAGE);
   glResolveAddrMsgID = AllocateID(IDTYPE_MESSAGE);

#ifdef _WIN32
   // Configure Winsock
   {
      CSTRING msg;
      if ((msg = StartupWinsock()) != 0) {
         log.warning("Winsock initialisation failed: %s", msg);
         return ERR_SystemCall;
      }
      SetResourcePtr(RES_NET_PROCESSING, reinterpret_cast<APTR>(win_net_processing)); // Hooks into ProcessMessages()
   }
#endif

   auto recv_function = make_function_stdc(resolve_name_receiver, CurrentTask());
   if (AddMsgHandler(NULL, glResolveNameMsgID, &recv_function, &glResolveNameHandler)) {
      return ERR_Failed;
   }

   recv_function.StdC.Routine = (APTR)resolve_addr_receiver;
   if (AddMsgHandler(NULL, glResolveAddrMsgID, &recv_function, &glResolveAddrHandler)) {
      return ERR_Failed;
   }

   return ERR_Okay;
}

//***************************************************************************

ERROR MODOpen(OBJECTPTR Module)
{
   SetPointer(Module, FID_FunctionList, glFunctions);
   return ERR_Okay;
}

//****************************************************************************
// Note: Take care and attention with the order of operations during the expunge process, particuarly due to the
// background processes that are managed by the module.

static ERROR MODExpunge(void)
{
   parasol::Log log;

   if (not glThreads.empty()) {
      log.msg("Waiting on any Network threads (%d) that remain active...", (LONG)glThreads.size());
      while (not glThreads.empty()) { // Threads will automatically remove themselves
         WaitTime(1, 0);
      }
   }

#ifdef _WIN32
   SetResourcePtr(RES_NET_PROCESSING, NULL);
#endif

   if (glResolveNameHandler) { FreeResource(glResolveNameHandler); glResolveNameHandler = NULL; }
   if (glResolveAddrHandler) { FreeResource(glResolveAddrHandler); glResolveAddrHandler = NULL; }
   if (glDNS) { FreeResource(glDNS); glDNS = NULL; }

#ifdef _WIN32
   log.msg("Closing winsock.");

   if (ShutdownWinsock() != 0) log.warning("Warning: Winsock DLL Cleanup failed.");
#endif

   if (clNetSocket)    { acFree(clNetSocket); clNetSocket = NULL; }
   if (clClientSocket) { acFree(clClientSocket); clClientSocket = NULL; }
   if (clProxy)        { acFree(clProxy); clProxy = NULL; }

#ifdef ENABLE_SSL
   if (ssl_init) {
      ERR_free_strings();
      EVP_cleanup();
      CRYPTO_cleanup_all_ex_data();
   }
#endif

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
AddressToStr: Converts an IPAddress structure to an IPAddress in dotted string form.

Converts an IPAddress structure to a string containing the IPAddress in dotted format.  Please free the resulting
string with <function>FreeResource</> once it is no longer required.

-INPUT-
struct(IPAddress) IPAddress: A pointer to the IPAddress structure.

-RESULT-
!cstr: The IP address is returned as an allocated string.

*****************************************************************************/

static CSTRING netAddressToStr(IPAddress *Address)
{
   parasol::Log log(__FUNCTION__);

   if (!Address) return NULL;

   if (Address->Type != IPADDR_V4) {
      log.warning("Only IPv4 Addresses are supported currently");
      return NULL;
   }

   struct in_addr addr;
   addr.s_addr = netHostToLong(Address->Data[0]);

   STRING result;
#ifdef __linux__
   result = inet_ntoa(addr);
#elif _WIN32
   result = win_inet_ntoa(addr.s_addr);
#endif

   if (!result) return NULL;
   return StrClone(result);
}

/*****************************************************************************

-FUNCTION-
StrToAddress: Converts an IP Address in string form to an IPAddress structure.

Converts an IPv4 or an IPv6 address in dotted string format to an IPAddress structure.  The String must be of form
`1.2.3.4` (IPv4).

<pre>
struct IPAddress addr;
if (!StrToAddress("127.0.0.1", &addr)) {
   ...
}
</pre>

-INPUT-
cstr String:  A null-terminated string containing the IP Address in dotted format.
struct(IPAddress) Address: Must point to an IPAddress structure that will be filled in.

-ERRORS-
Okay:    The Address was converted successfully.
Args:    Either the string or the IPAddress pointer were NULL.
Failed:  The String was not a valid IP Address.

*****************************************************************************/

static ERROR netStrToAddress(CSTRING Str, IPAddress *Address)
{
   if ((!Str) or (!Address)) return ERR_NullArgs;

#ifdef __linux__
   ULONG result = inet_addr(Str);
#elif _WIN32
   ULONG result = win_inet_addr(Str);
#endif

   if (result IS INADDR_NONE) return ERR_Failed;

   Address->Data[0] = netLongToHost(result);
   Address->Data[1] = 0;
   Address->Data[2] = 0;
   Address->Data[3] = 0;
   Address->Type = IPADDR_V4;

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
HostToShort: Converts a 16 bit (unsigned) word from host to network byte order.

Converts a 16 bit (unsigned) word from host to network byte order.

-INPUT-
uint Value: Data in host byte order to be converted to network byte order

-RESULT-
uint: The word in network byte order

*****************************************************************************/

static ULONG netHostToShort(ULONG Value)
{
   return (ULONG)htons((UWORD)Value);
}

/*****************************************************************************

-FUNCTION-
HostToLong: Converts a 32 bit (unsigned) long from host to network byte order.

Converts a 32 bit (unsigned) long from host to network byte order.

-INPUT-
uint Value: Data in host byte order to be converted to network byte order

-RESULT-
uint: The long in network byte order

*****************************************************************************/

static ULONG netHostToLong(ULONG Value)
{
   return htonl(Value);
}

/*****************************************************************************

-FUNCTION-
ShortToHost: Converts a 16 bit (unsigned) word from network to host byte order.

Converts a 16 bit (unsigned) word from network to host byte order.

-INPUT-
uint Value: Data in network byte order to be converted to host byte order

-RESULT-
uint: The Value in host byte order

*****************************************************************************/

static ULONG netShortToHost(ULONG Value)
{
   return (ULONG)ntohs((UWORD)Value);
}

/*****************************************************************************

-FUNCTION-
LongToHost: Converts a 32 bit (unsigned) long from network to host byte order.

Converts a 32 bit (unsigned) long from network to host byte order.

-INPUT-
uint Value: Data in network byte order to be converted to host byte order

-RESULT-
uint: The Value in host byte order.

*****************************************************************************/

static ULONG netLongToHost(ULONG Value)
{
   return ntohl(Value);
}

/*****************************************************************************

-FUNCTION-
ResolveAddress: Resolves an IP address to a host name.

ResolveAddress() performs a IP address resolution, converting an address to an official host name and list of
IP addresses.  The resolution process involves contacting a DNS server.  As this can potentially cause a significant
delay, the ResolveAddress() function supports asynchronous communication by default in order to avoid lengthy waiting
periods.  If synchronous (blocking) operation is desired then the NSF_SYNCHRONOUS flag must be set prior to calling
this method.

The function referenced in the Callback parameter will receive the results of the lookup.  Its C/C++ prototype is
`Function(LARGE ClientData, ERROR Error, CSTRING HostName, IPAddress *Addresses, LONG TotalAddresses)`.

The Fluid prototype is as follows, with Addresses being passed as external arrays accessible via the
array interface: `function Callback(ClientData, Error, HostName, Addresses)`.

-INPUT-
cstr Address: IP address to be resolved, e.g. 123.111.94.82.
int(NSF) Flags: Optional flags.
ptr(func) Callback: This function will be called with the resolved IP address.
large ClientData: Client data value to be passed to the Callback.

-ERRORS-
Okay: The IP address was resolved successfully.
Args
NullArgs
Failed: The address could not be resolved

*****************************************************************************/

static ERROR netResolveAddress(CSTRING Address, LONG Flags, FUNCTION *Callback, LARGE ClientData)
{
   parasol::Log log(__FUNCTION__);

   if (!Address) return log.error(ERR_NullArgs);

   IPAddress ip;
   if (netStrToAddress(Address, &ip) != ERR_Okay) return ERR_Args;

   if (!(Flags & NSF_SYNCHRONOUS)) { // Attempt asynchronous resolution in the background, return immediately.
      OBJECTPTR thread;
      LONG pkg_size = sizeof(resolve_addr_buffer) + StrLength(Address) + 1;
      if (!CreateObject(ID_THREAD, NF_UNTRACKED, &thread,
            FID_Routine|TPTR, &thread_resolve_addr,
            FID_Flags|TLONG,  THF_AUTO_FREE,
            TAGEND)) {
         char buffer[pkg_size];
         resolve_addr_buffer *rab = (resolve_addr_buffer *)&buffer;
         rab->callback = *Callback;
         rab->client_data = ClientData;
         StrCopy(Address, (STRING)(rab + 1), COPY_ALL);
         if ((!thSetData(thread, rab, pkg_size)) and (!acActivate(thread))) {
            std::lock_guard<std::mutex> lock(glThreadLock);
            glThreads.insert(thread);
            return ERR_Okay;
         }
         else acFree(thread);
      }
   }

   {
      #ifdef _WIN32
         struct dns_cache *dns;
         struct hostent *host = win_gethostbyaddr(&ip);
         if (!host) return log.error(ERR_Failed);

         ERROR error = cache_host(NULL, host, &dns);
         if (!error) {
            resolve_callback(ClientData, Callback, ERR_Okay, dns->HostName, dns->Addresses, dns->AddressCount);
            return ERR_Okay;
         }
         else return error;
      #else
         char host_name[256], service[128];
         int result;
         ERROR error;

         if (ip.Type IS IPADDR_V4) {
            const struct sockaddr_in sa = {
               .sin_family = AF_INET,
               .sin_port = 0,
               .sin_addr = { .s_addr = ip.Data[0] }
            };
            result = getnameinfo((struct sockaddr *)&sa, sizeof(ip), host_name, sizeof(host_name), service, sizeof(service), NI_NAMEREQD);
         }
         else {
            const struct sockaddr_in6 sa = {
               .sin6_family   = AF_INET6,
               .sin6_port     = 0,
               .sin6_flowinfo = 0,
               .sin6_scope_id = 0
            };
            CopyMemory(ip.Data, (APTR)sa.sin6_addr.s6_addr, 16);
            result = getnameinfo((struct sockaddr *)&sa, sizeof(ip), host_name, sizeof(host_name), service, sizeof(service), NI_NAMEREQD);
         }

         switch(result) {
            case 0: {
               struct hostent host = {
                  .h_name      = host_name,
                  .h_addrtype  = (ip.Type IS IPADDR_V4) ? AF_INET : AF_INET6,
                  .h_length    = 0,
                  .h_addr_list = NULL
               };
               struct dns_cache *dns;
               ERROR error = cache_host(host_name, &host, &dns);
               if (!error) {
                  resolve_callback(ClientData, Callback, ERR_Okay, dns->HostName, dns->Addresses, dns->AddressCount);
                  return ERR_Okay;
               }

            }
            case EAI_AGAIN:    error = ERR_Retry; break;
            case EAI_MEMORY:   error = ERR_Memory; break;
            case EAI_OVERFLOW: error = ERR_BufferOverflow; break;
            case EAI_SYSTEM:   error = ERR_SystemCall; break;
            default:           error = ERR_Failed; break;
         }

         resolve_callback(ClientData, Callback, error, NULL, &ip, 1);
         return error;
      #endif
   }
}

/*****************************************************************************

-FUNCTION-
ResolveName: Resolves a domain name to an official host name and a list of IP addresses.

ResolveName() performs a domain name resolution, converting a domain name to an official host name and
IP addresses.  The resolution process involves contacting a DNS server.  As this can potentially cause a significant
delay, the ResolveName() function attempts to use asynchronous communication by default so that the function can return
immediately.  If a synchronous (blocking) operation is desired then the NSF_SYNCHRONOUS flag must be set prior to
calling this method and the routine will not return until it has a response.

The function referenced in the Callback parameter will receive the results of the lookup.  Its C/C++ prototype is
`Function(LARGE ClientData, ERROR Error, CSTRING HostName, IPAddress *Addresses, LONG TotalAddresses)`.

The Fluid prototype is as follows, with Addresses being passed as external arrays accessible via the
array interface: `function Callback(ClientData, Error, HostName, Addresses)`.

If the Error received by the callback is anything other than ERR_Okay, the HostName and Addresses shall be set
to NULL.  It is recommended that ClientData is used as the unique identifier for the original request in this situation.

-INPUT-
cstr HostName: The host name to be resolved.
int(NSF) Flags: Optional flags.
ptr(func) Callback: This function will be called with the resolved IP address.
large ClientData: Client data value to be passed to the Callback.

-ERRORS-
Okay:
NullArgs:
AllocMemory:
Failed:

*****************************************************************************/

static ERROR netResolveName(CSTRING HostName, LONG Flags, FUNCTION *Callback, LARGE ClientData)
{
   parasol::Log log(__FUNCTION__);

   if (!HostName) return log.error(ERR_NullArgs);

   log.branch("Host: %s, Synchronous: %c, Callback: %p, ClientData: " PF64() "/%p", HostName, (Flags & NSF_SYNCHRONOUS) ? 'Y' : 'N', Callback, ClientData, (APTR)(MAXINT)ClientData);

   { // Use the cache if available.
      struct dns_cache *dns;
      if (!VarGet(glDNS, HostName, &dns, NULL)) {
         log.trace("Cache hit for host %s", dns->HostName);
         resolve_callback(ClientData, Callback, ERR_Okay, dns->HostName, dns->Addresses, dns->AddressCount);
         return ERR_Okay;
      }
   }

   // Resolve 'localhost' immediately

   if (!StrMatch("localhost", HostName)) {
      IPAddress ip = { { 0x7f, 0x00, 0x00, 0x01 }, IPADDR_V4 };
      IPAddress list[] = { ip };
      resolve_callback(ClientData, Callback, ERR_Okay, "localhost", list, 1);
   }

   if (!(Flags & NSF_SYNCHRONOUS)) { // Attempt asynchronous resolution in the background, return immediately.
      OBJECTPTR thread;
      LONG pkg_size = sizeof(resolve_name_buffer) + StrLength(HostName) + 1;
      if (!CreateObject(ID_THREAD, NF_UNTRACKED, &thread,
            FID_Routine|TPTR, &thread_resolve_name,
            FID_Flags|TLONG,  THF_AUTO_FREE,
            TAGEND)) {
         char buffer[pkg_size];
         auto rnb = (resolve_name_buffer *)&buffer;
         rnb->callback = *Callback;
         rnb->client_data = ClientData;
         StrCopy(HostName, (STRING)(rnb + 1), COPY_ALL);
         if ((!thSetData(thread, buffer, pkg_size)) and (!acActivate(thread))) {
            std::lock_guard<std::mutex> lock(glThreadLock);
            glThreads.insert(thread);
            return ERR_Okay;
         }
         else acFree(thread);
      }
      log.warning("Failed to resolve the name asynchronously.");
   }

   {
      #ifdef __linux__
         struct addrinfo hints, *servinfo;

         ClearMemory(&hints, sizeof hints);
         hints.ai_family   = AF_UNSPEC;
         hints.ai_socktype = SOCK_STREAM;
         hints.ai_flags    = AI_CANONNAME;
         int result = getaddrinfo(HostName, NULL, &hints, &servinfo);

         ERROR error = ERR_Failed;
         switch (result) {
            case 0: {
               struct dns_cache *dns;
               error = cache_host(HostName, servinfo, &dns);
               freeaddrinfo(servinfo);
               if (!error) {
                  resolve_callback(ClientData, Callback, ERR_Okay, dns->HostName, dns->Addresses, dns->AddressCount);
                  return ERR_Okay;
               }
               break;
            }
            case EAI_AGAIN:  error = ERR_Retry; break;
            case EAI_FAIL:   error = ERR_Failed; break;
            case EAI_MEMORY: error = ERR_Memory; break;
            case EAI_SYSTEM: error = ERR_SystemCall; break;
            default:
               error = ERR_Failed;
         }

         resolve_callback(ClientData, Callback, error, HostName, NULL, 0);
         return error;
      #elif _WIN32
         ERROR error;
         struct hostent *host = win_gethostbyname(HostName);
         if (!host) return log.error(ERR_Failed);

         struct dns_cache *dns;
         if (!(error = cache_host(HostName, host, &dns))) {
            resolve_callback(ClientData, Callback, ERR_Okay, dns->HostName, dns->Addresses, dns->AddressCount);
            return ERR_Okay;
         }
         else {
            resolve_callback(ClientData, Callback, error, HostName, NULL, 0);
            return error;
         }
      #endif
   }
}

/*****************************************************************************

-FUNCTION-
SetSSL: Alters SSL settings on an initialised NetSocket object.

Use the SetSSL() function to send SSL commands to a NetSocket object.  The following table illustrates the commands
that are currently available:





If a failure occurs when executing a command, the execution of all further commands is aborted and the error code is
returned immediately.

-INPUT-
obj(NetSocket) NetSocket: The target NetSocket object.
tags Tags: Series of tags terminated by TAGEND.

-ERRORS-
Okay:
NullArgs: The NetSocket argument was not specified.
-END-

*****************************************************************************/

static ERROR netSetSSL(objNetSocket *Socket, ...)
{
#ifdef ENABLE_SSL
   LONG value, tagid;
   ERROR error;
   va_list list;

   if (!Socket) return ERR_NullArgs;

   va_start(list, Socket);
   while ((tagid = va_arg(list, LONG))) {
      parasol::Log log(__FUNCTION__);
      log.traceBranch("Command: %d", tagid);

      switch(tagid) {
         case NSL_CONNECT:
            value = va_arg(list, LONG);
            if (value) { // Initiate an SSL connection on this socket
               if ((error = sslSetup(Socket)) IS ERR_Okay) {
                  sslLinkSocket(Socket);
                  error = sslConnect(Socket);
               }

               if (error) {
                  va_end(list);
                  return error;
               }
            }
            else { // Disconnect SSL (i.e. go back to unencrypted mode)
               sslDisconnect(Socket);
            }
            break;
      }
   }

   va_end(list);
   return ERR_Okay;
#else
   return ERR_NoSupport;
#endif
}

#ifdef ENABLE_SSL
#include "ssl.cpp"
#endif

//****************************************************************************
// Used by RECEIVE() SSL support.

#ifdef _WIN32
static void client_server_pending(SOCKET_HANDLE FD, APTR Self) __attribute__((unused));
static void client_server_pending(SOCKET_HANDLE FD, APTR Self)
{
   #pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
   RegisterFD((HOSTHANDLE)((objNetSocket *)Self)->SocketHandle, RFD_REMOVE|RFD_READ|RFD_SOCKET, NULL, NULL);
   #pragma GCC diagnostic warning "-Wint-to-pointer-cast"
   client_server_incoming(FD, (objNetSocket *)Self);
}
#endif

//****************************************************************************

static ERROR RECEIVE(objNetSocket *Self, SOCKET_HANDLE Socket, APTR Buffer, LONG BufferSize, LONG Flags, LONG *Result)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("Socket: %d, BufSize: %d, Flags: $%.8x, SSLBusy: %d", Socket, BufferSize, Flags, Self->SSLBusy);

   *Result = 0;

#ifdef ENABLE_SSL
   if (Self->SSLBusy IS SSL_HANDSHAKE_WRITE) ssl_handshake_write(Socket, Self);
   else if (Self->SSLBusy IS SSL_HANDSHAKE_READ) ssl_handshake_read(Socket, Self);

   if (Self->SSLBusy != SSL_NOT_BUSY) return ERR_Okay;
#endif

   if (!BufferSize) return ERR_Okay;

#ifdef ENABLE_SSL
   BYTE read_blocked;
   LONG pending;

   if (Self->SSL) {
      do {
         read_blocked = 0;

         LONG result = SSL_read(Self->SSL, Buffer, BufferSize);

         if (result <= 0) {
            switch (SSL_get_error(Self->SSL, result)) {
               case SSL_ERROR_ZERO_RETURN:
                  return ERR_Disconnected;

               case SSL_ERROR_WANT_READ:
                  read_blocked = TRUE;
                  break;

                case SSL_ERROR_WANT_WRITE:
                  // WANT_WRITE is returned if we're trying to rehandshake and the write operation would block.  We
                  // need to wait on the socket to be writeable, then restart the read when it is.

                   log.msg("SSL socket handshake requested by server.");
                   Self->SSLBusy = SSL_HANDSHAKE_WRITE;
                   #ifdef __linux__
                      RegisterFD((HOSTHANDLE)Socket, RFD_WRITE|RFD_SOCKET, &ssl_handshake_write, Self);
                   #else
                      win_socketstate(Socket, -1, TRUE);
                   #endif

                   return ERR_Okay;

                case SSL_ERROR_SYSCALL:

                default:
                   log.warning("SSL read problem");
                   return ERR_Okay; // Non-fatal
            }
         }
         else {
            *Result += result;
            Buffer = (APTR)((char *)Buffer + result);
            BufferSize -= result;
         }
      } while ((pending = SSL_pending(Self->SSL)) and (!read_blocked) and (BufferSize > 0));

      log.trace("Pending: %d, BufSize: %d, Blocked: %d", pending, BufferSize, read_blocked);

      if (pending) {
         // With regards to non-blocking SSL sockets, be aware that a socket can be empty in terms of incoming data,
         // yet SSL can keep data that has already arrived in an internal buffer.  This means that we can get stuck
         // select()ing on the socket because you aren't told that there is internal data waiting to be processed by
         // SSL_read().
         //
         // For this reason we set the RECALL flag so that we can be called again manually when we know that there is
         // data pending.

         #ifdef __linux__
            RegisterFD((HOSTHANDLE)Socket, RFD_RECALL|RFD_READ|RFD_SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&client_server_incoming), Self);
         #elif _WIN32
            // In Windows we don't want to listen to FD's on a permanent basis,
            // so this is a temporary setting that will be reset by client_server_pending()

            RegisterFD((HOSTHANDLE)(MAXINT)Socket, RFD_RECALL|RFD_READ|RFD_SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&client_server_pending), (APTR)Self);
         #endif
      }

      return ERR_Okay;
   }
#endif

#ifdef __linux__
   {
      LONG result = recv(Socket, Buffer, BufferSize, Flags);

      if (result > 0) {
         *Result = result;
         return ERR_Okay;
      }
      else if (result IS 0) { // man recv() says: The return value is 0 when the peer has performed an orderly shutdown.
         return ERR_Disconnected;
      }
      else if ((errno IS EAGAIN) or (errno IS EINTR)) {
         return ERR_Okay;
      }
      else {
         log.warning("recv() failed: %s", strerror(errno));
         return ERR_Failed;
      }
   }
#elif _WIN32
   return WIN_RECEIVE(Socket, Buffer, BufferSize, Flags, Result);
#else
   #error No support for RECEIVE()
#endif
}

//****************************************************************************

static ERROR SEND(objNetSocket *Self, SOCKET_HANDLE Socket, CPTR Buffer, LONG *Length, LONG Flags)
{
   parasol::Log log(__FUNCTION__);

   if (!*Length) return ERR_Okay;

#ifdef ENABLE_SSL

   if (Self->SSL) {
      log.traceBranch("SSLBusy: %d, Length: %d", Self->SSLBusy, *Length);

      if (Self->SSLBusy IS SSL_HANDSHAKE_WRITE) ssl_handshake_write(Socket, Self);
      else if (Self->SSLBusy IS SSL_HANDSHAKE_READ) ssl_handshake_read(Socket, Self);

      if (Self->SSLBusy != SSL_NOT_BUSY) {
         return ERR_Okay;
      }

      LONG bytes_sent = SSL_write(Self->SSL, Buffer, *Length);

      if (bytes_sent < 0) {
         *Length = 0;
         LONG ssl_error = SSL_get_error(Self->SSL, bytes_sent);

         switch(ssl_error){
            case SSL_ERROR_WANT_WRITE:
               log.traceWarning("Buffer overflow (SSL want write)");
               return ERR_BufferOverflow;

            // We get a WANT_READ if we're trying to rehandshake and we block on write during the current connection.
            //
            // We need to wait on the socket to be readable but reinitiate our write when it is

            case SSL_ERROR_WANT_READ:
               log.trace("Handshake requested by server.");
               Self->SSLBusy = SSL_HANDSHAKE_READ;
               #ifdef __linux__
                  RegisterFD((HOSTHANDLE)Socket, RFD_READ|RFD_SOCKET, &ssl_handshake_read, Self);
               #elif _WIN32
                  win_socketstate(Socket, TRUE, -1);
               #endif
               return ERR_Okay;

            case SSL_ERROR_SYSCALL:
               log.warning("SSL_write() SysError %d: %s", errno, strerror(errno));
               return ERR_Failed;

            default:
               while (ssl_error) {
                  log.warning("SSL_write() error %d, %s", ssl_error, ERR_error_string(ssl_error, NULL));
                  ssl_error = ERR_get_error();
               }

               return ERR_Failed;
         }
      }
      else {
         if (*Length != bytes_sent) {
            log.traceWarning("Sent %d of requested %d bytes.", bytes_sent, *Length);
         }
         *Length = bytes_sent;
      }

      return ERR_Okay;
   }
#endif

#ifdef __linux__
   *Length = send(Socket, Buffer, *Length, Flags);

   if (*Length >= 0) return ERR_Okay;
   else {
      *Length = 0;
      if (errno IS EAGAIN) return ERR_BufferOverflow;
      else if (errno IS EMSGSIZE) return ERR_DataSize;
      else {
         log.warning("send() failed: %s", strerror(errno));
         return ERR_Failed;
      }
   }
#elif _WIN32
   return WIN_SEND(Socket, Buffer, Length, Flags);
#else
   #error No support for SEND()
#endif
}

//***************************************************************************

static void resolve_callback(LARGE ClientData, FUNCTION *Callback, ERROR Error, CSTRING HostName, IPAddress *Addresses, LONG TotalAddresses)
{
   if (!Callback) return;
   if (Callback->Type IS CALL_STDC) {
      parasol::SwitchContext context(Callback->StdC.Context);
      auto routine = (ERROR (*)(LARGE, ERROR, CSTRING, IPAddress *, LONG))(Callback->StdC.Routine);
      routine(ClientData, Error, HostName, Addresses, TotalAddresses);
   }
   else if (Callback->Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Callback->Script.Script)) {
         const ScriptArg args[] = {
            { "ClientData",   FD_LARGE, { .Large   = ClientData } },
            { "Error",        FD_LONG,  { .Long    = Error } },
            { "HostName",     FD_STR,   { .Address = (STRING)HostName } },
            { "IPAddress:Addresses", FD_ARRAY|FD_STRUCT,   { .Address = Addresses } },
            { "TotalAddresses",      FD_ARRAYSIZE|FD_LONG, { .Long    = TotalAddresses } }
         };
         scCallback(script, Callback->Script.ProcedureID, args, ARRAYSIZE(args), NULL);
      }
   }
}

//***************************************************************************

static ERROR cache_host(CSTRING HostName, struct hostent *Host, struct dns_cache **Cache)
{
   if ((!Host) or (!Cache)) return ERR_NullArgs;

   if (!HostName) {
      if (!(HostName = Host->h_name)) return ERR_Args;
   }

   parasol::Log log(__FUNCTION__);

   log.debug("Host: %s, Addresses: %p (IPV6: %d)", HostName, Host->h_addr_list, (Host->h_addrtype == AF_INET6));

   CSTRING real_name = Host->h_name;
   if (!real_name) real_name = HostName;

   *Cache = NULL;
   if ((Host->h_addrtype != AF_INET) and (Host->h_addrtype != AF_INET6)) {
      return ERR_Args;
   }

   // Calculate the size of the data structure.

   LONG size = sizeof(struct dns_cache) + ALIGN64(StrLength(real_name) + 1);
   LONG address_count = 0;
   if (Host->h_addr_list) {
      for (address_count=0; (address_count < MAX_ADDRESSES) and (Host->h_addr_list[address_count]); address_count++);
   }

   size += address_count * sizeof(IPAddress);

   // Allocate an empty key-pair and fill it.

   struct dns_cache *cache;
   if (VarSetSized(glDNS, HostName, size, &cache, NULL) != ERR_Okay) return ERR_Failed;

   char *buffer = (char *)cache;
   LONG offset = sizeof(struct dns_cache);

   if (address_count > 0) {
      cache->Addresses = (IPAddress *)(buffer + offset);
      offset += address_count * sizeof(IPAddress);

      if (Host->h_addrtype IS AF_INET) {
         LONG i;
         for (i=0; i < address_count; i++) {
            ULONG addr = *((ULONG *)Host->h_addr_list[i]);
            cache->Addresses[i].Data[0] = ntohl(addr);
            cache->Addresses[i].Data[1] = 0;
            cache->Addresses[i].Data[2] = 0;
            cache->Addresses[i].Data[3] = 0;
            cache->Addresses[i].Type = IPADDR_V4;
         }
      }
      else if  (Host->h_addrtype IS AF_INET6) {
         LONG i;
         for (i=0; i < address_count; i++) {
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
   StrCopy(real_name, (STRING)cache->HostName, COPY_ALL);
   cache->AddressCount = address_count;

   *Cache = cache;
   return ERR_Okay;
}

#ifdef __linux__
static ERROR cache_host(CSTRING HostName, struct addrinfo *Host, struct dns_cache **Cache)
{
   if ((!Host) or (!Cache)) return ERR_NullArgs;

   if (!HostName) {
      if (!(HostName = Host->ai_canonname)) return ERR_Args;
   }

   parasol::Log log(__FUNCTION__);

   log.debug("Host: %s, Addresses: %p (IPV6: %d)", HostName, Host->ai_addr, (Host->ai_family == AF_INET6));

   CSTRING real_name = Host->ai_canonname;
   if (!real_name) real_name = HostName;

   *Cache = NULL;
   if ((Host->ai_family != AF_INET) and (Host->ai_family != AF_INET6)) return ERR_Args;

   // Calculate the size of the data structure.

   LONG size = sizeof(struct dns_cache) + ALIGN64(StrLength(real_name) + 1);

   LONG address_count = 0;
   for (struct addrinfo *scan=Host; scan; scan=scan->ai_next) {
      if (scan->ai_addr) address_count++;
   }

   size += address_count * sizeof(IPAddress);

   // Allocate an empty key-pair and fill it.

   struct dns_cache *cache;
   if (VarSetSized(glDNS, HostName, size, &cache, NULL) != ERR_Okay) return ERR_Failed;

   char *buffer = (char *)cache;
   LONG offset = sizeof(struct dns_cache);

   if (address_count > 0) {
      cache->Addresses = (IPAddress *)(buffer + offset);
      offset += address_count * sizeof(IPAddress);

      LONG i = 0;
      for (struct addrinfo *scan=Host; scan; scan=scan->ai_next) {
         if (!scan->ai_addr) continue;

         if (scan->ai_family IS AF_INET) {
            ULONG addr = ((struct sockaddr_in *)scan->ai_addr)->sin_addr.s_addr;
            cache->Addresses[i].Data[0] = ntohl(addr);
            cache->Addresses[i].Data[1] = 0;
            cache->Addresses[i].Data[2] = 0;
            cache->Addresses[i].Data[3] = 0;
            cache->Addresses[i].Type = IPADDR_V4;
            i++;
         }
         else if  (scan->ai_family IS AF_INET6) {
            struct sockaddr_in6 *addr = (struct sockaddr_in6 *)scan->ai_addr;
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
   StrCopy(real_name, (STRING)cache->HostName, COPY_ALL);
   cache->AddressCount = address_count;
   *Cache = cache;
   return ERR_Okay;
}
#endif

//***************************************************************************

static BYTE check_machine_name(CSTRING HostName)
{
   for (LONG i=0; HostName[i]; i++) { // Check if it's a machine name
      if (HostName[i] IS '.') return FALSE;
   }
   return TRUE;
}

//****************************************************************************

#include "netsocket/netsocket.cpp"
#include "clientsocket/clientsocket.cpp"
#include "class_proxy.cpp"

//****************************************************************************

PARASOL_MOD(MODInit, NULL, MODOpen, MODExpunge, MODVERSION_NETWORK)

/*****************************************************************************
                                 BACKTRACE IT
*****************************************************************************/
