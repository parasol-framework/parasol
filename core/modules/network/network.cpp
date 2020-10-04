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
// Ares Includes

extern const char * net_init_ares(void);
extern void net_free_ares(void);
extern int net_ares_error(int Code, const char **Message);

//****************************************************************************

enum {
   SSL_NOT_BUSY=0,
   SSL_HANDSHAKE_READ,
   SSL_HANDSHAKE_WRITE
};

static void client_server_incoming(SOCKET_HANDLE, struct rkNetSocket *);

#ifdef _WIN32
   #include "win32/winsockwrappers.h"
#endif

#include "module_def.c"

struct CoreBase *CoreBase;
static OBJECTPTR glModule = NULL;

struct dns_resolver {
   LARGE time;
   LARGE client_data;
   FUNCTION callback;
   struct dns_resolver *next;
   #ifdef __linux__
      int tcp, udp; // For Ares
   #endif
   #ifdef _WIN32
      union { // For win_async_resolvename()
         struct hostent Host;
         UBYTE MaxGetHostStruct[1024]; // MAXGETHOSTSTRUCT == 1024
      } WinHost;
   #endif
};

struct dns_cache {
   CSTRING HostName;
   CSTRING *Aliases;
   struct IPAddress *Addresses;
   LONG   AliasCount;
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
struct ares_channeldata *glAres = NULL; // All access must be bound to a VarLock on glDNS.
static struct KeyStore *glDNS = NULL;
static struct dns_resolver *glResolvers = NULL;

static void resolve_callback(LARGE, FUNCTION *, ERROR, CSTRING, CSTRING *, LONG, struct IPAddress *, LONG);
static BYTE check_machine_name(CSTRING HostName) __attribute__((unused));
static struct dns_cache * cache_host(struct hostent *);
static void free_resolver(struct dns_resolver *);
static struct dns_resolver * new_resolver(LARGE ClientData, FUNCTION *Callback);
static ERROR add_netsocket(void);
static ERROR add_clientsocket(void);
static ERROR init_proxy(void);

#if defined(USE_ARES) && defined(__linux__)
static int ares_socket_callback(int, int, struct dns_resolver *);
void ares_response(void *Arg, int Status, int Timeouts, struct hostent *);
extern void net_resolve_name(const char *HostName, struct dns_resolver *Resolver);
extern void net_ares_resolveaddr(int, void *Data, struct dns_resolver *);
#endif

//***************************************************************************

ERROR MODInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CSTRING msg;

   CoreBase = argCoreBase;

   GetPointer(argModule, FID_Master, &glModule);

   glDNS = VarNew(64, KSF_THREAD_SAFE);

   if (add_netsocket()) return ERR_AddClass;
   if (add_clientsocket()) return ERR_AddClass;
   if (init_proxy()) return ERR_AddClass;

#ifdef _WIN32
   // Configure Winsock

   if ((msg = StartupWinsock()) != 0) {
      LogErrorMsg("Winsock initialisation failed: %s", msg);
      return ERR_SystemCall;
   }

   SetResourcePtr(RES_NET_PROCESSING, reinterpret_cast<APTR>(win_net_processing)); // Hooks into ProcessMessages()
#endif

#ifdef USE_ARES
   if ((msg = net_init_ares())) {
      LogErrorMsg("Ares network library failed to initialise: %s", msg);
      return ERR_Failed;
   }
#endif

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
#ifdef _WIN32
   SetResourcePtr(RES_NET_PROCESSING, NULL);
#endif

   if (glDNS) { FreeResource(glDNS); glDNS = NULL; }

#ifdef _WIN32
   LogMsg("Closing winsock.");

   if (ShutdownWinsock() != 0) LogErrorMsg("Warning: Winsock DLL Cleanup failed.");
#endif

   if (clNetSocket) { acFree(clNetSocket); clNetSocket = NULL; }
   if (clClientSocket) { acFree(clClientSocket); clClientSocket = NULL; }
   if (clProxy) { acFree(clProxy); clProxy = NULL; }

#ifdef ENABLE_SSL
   if (ssl_init) {
      ERR_free_strings();
      EVP_cleanup();
      CRYPTO_cleanup_all_ex_data();
   }
#endif

#ifdef USE_ARES
   net_free_ares();
#endif

   while (glResolvers) free_resolver(glResolvers);

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

static CSTRING netAddressToStr(struct IPAddress *Address)
{
   if (!Address) return NULL;

   if (Address->Type != IPADDR_V4) {
      LogF("@netAddressToStr()","Only IPv4 Addresses are supported currently");
      return NULL;
   }

   struct in_addr addr;
   addr.s_addr = netHostToLong(Address->Data[0]); // Convert to network byte order

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
result = StrToAddress("127.0.0.1", &addr);
if (result IS ERR_Okay) {
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

static ERROR netStrToAddress(CSTRING Str, struct IPAddress *Address)
{
   if ((!Str) OR (!Address)) return ERR_NullArgs;

#ifdef __linux__
   ULONG result = inet_addr(Str);
#elif _WIN32
   ULONG result = win_inet_addr(Str);
#endif

   if (result IS INADDR_NONE) return ERR_Failed;

   // convert to host byte order
   result = netLongToHost(result);

   Address->Data[0] = result;
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

ResolveAddress() performs a IP address resolution, converting an address to an official host name, list of aliases and
IP addresses.  The resolution process involves contacting a DNS server.  As this can potentially cause a significant
delay, the ResolveAddress() function supports asynchronous communication by default in order to avoid lengthy waiting
periods.  If asynchronous operation is desired, the NSF_ASYNC_RESOLVE flag should be set prior to calling this method.

The function referenced in the Callback parameter will receive the results of the lookup.  Its C/C++ prototype is
`Function(LARGE ClientData, ERROR Error, CSTRING HostName, CSTRING *Aliases, LONG TotalAliases, IPAddress *Addresses, LONG TotalAddresses)`.

The Fluid prototype is as follows, with Aliases and Addresses being passed as external arrays accessible via the
array interface: `function Callback(ClientData, Error, HostName, Aliases, Addresses)`.

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
   if ((!Address) OR (!Callback)) return PostError(ERR_NullArgs);

   struct IPAddress ip;
   if (netStrToAddress(Address, &ip) != ERR_Okay) return ERR_Args;

   if (Flags & NSF_ASYNC_RESOLVE) goto async_resolve;

   // Attempt synchronous resolution.

   if (!VarLock(glDNS, 0x7fffffff)) {
      struct dns_resolver *resolve = new_resolver(ClientData, Callback);
      if (!resolve) {
         VarUnlock(glDNS);
         return ERR_AllocMemory;
      }

      #ifdef USE_ARES
         if (glAres) {
         #ifdef __linux__
            net_ares_resolveaddr(ip.Type == IPADDR_V4, ip.Data, resolve);
            VarUnlock(glDNS);
            return ERR_Okay;
         #elif _WIN32
            if (!win_ares_resolveaddr(&ip, glAres, resolve)) {
               VarUnlock(glDNS);
               return ERR_Okay;
            }
         #endif
         }
      #endif

      free_resolver(resolve); // Remove the resolver if background resolution failed.
      VarUnlock(glDNS);
   }

async_resolve:
   {
      struct hostent *host;
      #ifdef _WIN32
         host = win_gethostbyaddr(&ip);
      #else
         host = gethostbyaddr((const char *)&ip, sizeof(ip), (ip.Type IS IPADDR_V4) ? AF_INET : AF_INET6);
      #endif
      if (!host) return LogError(ERH_Function, ERR_Failed);

      struct dns_cache *dns = cache_host(host);
      if (dns) resolve_callback(ClientData, Callback, ERR_Okay, dns->HostName, dns->Aliases, dns->AliasCount, dns->Addresses, dns->AddressCount);
      return ERR_Okay;
   }
}

/*****************************************************************************

-FUNCTION-
ResolveName: Resolves a domain name to an official host name, a list of aliases, and a list of IP addresses.

ResolveName() performs a domain name resolution, converting a domain name to an official host name, list of aliases and
IP addresses.  The resolution process involves contacting a DNS server.  As this can potentially cause a significant
delay, the ResolveName() function attempts to use synchronous communication by default so that the function can return
immediately.  If asynchronous operation is desired, the NSF_ASYNC_RESOLVE flag should be set prior to calling this method
and the routine will not return until it has a response.

The function referenced in the Callback parameter will receive the results of the lookup.  Its C/C++ prototype is
`Function(LARGE ClientData, ERROR Error, CSTRING HostName, CSTRING *Aliases, LONG TotalAliases, IPAddress *Addresses, LONG TotalAddresses)`.

The Fluid prototype is as follows, with Aliases and Addresses being passed as external arrays accessible via the
array interface: `function Callback(ClientData, Error, HostName, Aliases, Addresses)`.

If the Error received by the callback is anything other than ERR_Okay, the HostName, Aliases and Addresses shall be set
to NULL.  It is recommended that ClientData is used as the unique identifier for the original request in this situation.

-INPUT-
cstr HostName: The host name to be resolved.
int(NSF) Flags: Optional flags.
ptr(func) Callback: This function will be called with the resolved IP address.
large ClientData: Client data value to be passed to the Callback.

-ERRORS-
Okay:
NullArgs:

*****************************************************************************/

static ERROR netResolveName(CSTRING HostName, LONG Flags, FUNCTION *Callback, LARGE ClientData)
{
   if ((!HostName) OR (!Callback)) return PostError(ERR_NullArgs);

   FMSG("ResolveName()","Host: %s", HostName);

   { // Use the cache if available.
      struct dns_cache *dns;
      if (!VarGet(glDNS, HostName, &dns, NULL)) {
         FMSG("ResolveName","Cache hit for host %s", dns->HostName);
         resolve_callback(ClientData, Callback, ERR_Okay, dns->HostName, dns->Aliases, dns->AliasCount, dns->Addresses, dns->AddressCount);
         return ERR_Okay;
      }
   }

   // Resolve 'localhost' immediately

   if (!StrMatch("localhost", HostName)) {
      struct IPAddress ip = { { 0x7f, 0x00, 0x00, 0x01 }, IPADDR_V4 };
      struct IPAddress list[] = { ip };
      resolve_callback(ClientData, Callback, ERR_Okay, "localhost", NULL, 0, list, 1);
   }

   if (!(Flags & NSF_ASYNC_RESOLVE)) {
      // Attempt synchronous resolution.

      VarLock(glDNS, 0x7fffffff);

      struct dns_resolver *resolver = new_resolver(ClientData, Callback);
      if (!resolver) {
         VarUnlock(glDNS);
         return ERR_AllocMemory;
      }

#ifdef USE_ARES
   #ifdef __linux__
      if (glAres) {
         net_resolve_name(HostName, resolver);
         VarUnlock(glDNS);
         return ERR_Okay;
      }
   #elif _WIN32
      if ((glAres) AND (!check_machine_name(HostName))) {
         FMSG("ResolveName","Resolving '%s' using Ares callbacks.", HostName);
         win_ares_resolvename(HostName, glAres, resolver);
         VarUnlock(glDNS);
         return ERR_Okay;
      }

      FMSG("ResolveName","Resolving machine name '%s' using WSAASync callbacks.", HostName);
      if (!win_async_resolvename(HostName, resolver, &resolver->WinHost.Host, sizeof(resolver->WinHost.MaxGetHostStruct))) {
         VarUnlock(glDNS);
         return ERR_Okay;
      }
   #endif
#endif

      free_resolver(resolver); // Remove the resolver if background resolution failed.
      VarUnlock(glDNS);
   }

   {
      struct hostent *host;
      #ifdef __linux__
         host = gethostbyname(HostName);
      #elif _WIN32
         host = win_gethostbyname(HostName);
      #endif
      if (!host) return LogError(ERH_Function, ERR_Failed);

      struct dns_cache *dns = cache_host(host);
      if (dns) resolve_callback(ClientData, Callback, ERR_Okay, dns->HostName, dns->Aliases, dns->AliasCount, dns->Addresses, dns->AddressCount);
      return ERR_Okay;
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

   if (!Socket) return PostError(ERR_NullArgs);

   va_start(list, Socket);
   while ((tagid = va_arg(list, LONG))) {
      FMSG("~SetSSL","Command: %d", tagid);

      switch(tagid) {
         case NSL_CONNECT:
            value = va_arg(list, LONG);
            if (value) {
               // Initiate an SSL connection on this socket

               if ((error = sslSetup(Socket)) IS ERR_Okay) {
                  sslLinkSocket(Socket);
                  error = sslConnect(Socket);
               }

               if (error) {
                  va_end(list);
                  STEP();
                  return error;
               }
            }
            else {
               // Disconnect SSL (i.e. go back to unencrypted mode)

               sslDisconnect(Socket);
            }
            break;
      }

      STEP();
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
// Used by RECEIVE()

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
   FMSG("~RECEIVE()","Socket: %d, BufSize: %d, Flags: $%.8x, SSLBusy: %d", Socket, BufferSize, Flags, Self->SSLBusy);

   *Result = 0;

#ifdef ENABLE_SSL
   if (Self->SSLBusy IS SSL_HANDSHAKE_WRITE) ssl_handshake_write(Socket, Self);
   else if (Self->SSLBusy IS SSL_HANDSHAKE_READ) ssl_handshake_read(Socket, Self);

   if (Self->SSLBusy != SSL_NOT_BUSY) {
      STEP();
      return ERR_Okay;
   }
#endif

   if (!BufferSize) { STEP(); return ERR_Okay; }

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
                  STEP();
                  return ERR_Disconnected;

               case SSL_ERROR_WANT_READ:
                  read_blocked = TRUE;
                  break;

                case SSL_ERROR_WANT_WRITE:
                  // WANT_WRITE is returned if we're trying to rehandshake and the write operation would block.  We
                  // need to wait on the socket to be writeable, then restart the read when it is.

                   LogF("RECEIVE()","SSL socket handshake requested by server.");
                   Self->SSLBusy = SSL_HANDSHAKE_WRITE;
                   #ifdef __linux__
                      RegisterFD((HOSTHANDLE)Socket, RFD_WRITE|RFD_SOCKET, &ssl_handshake_write, Self);
                   #else
                      win_socketstate(Socket, -1, TRUE);
                   #endif

                   STEP();
                   return ERR_Okay;

                case SSL_ERROR_SYSCALL:

                default:
                   LogErrorMsg("SSL read problem");
                   STEP();
                   return ERR_Okay; // Non-fatal
            }
         }
         else {
            *Result += result;
            Buffer = (APTR)((char *)Buffer + result);
            BufferSize -= result;
         }
      } while ((pending = SSL_pending(Self->SSL)) AND (!read_blocked) AND (BufferSize > 0));

      FMSG("RECEIVE","Pending: %d, BufSize: %d, Blocked: %d", pending, BufferSize, read_blocked);

      if (pending) {
         // With regards to non-blocking SSL sockets, be aware that a socket can be empty in terms of incoming data,
         // yet SSL can keep data that has already arrived in an internal buffer.  This means that we can get stuck
         // select()ing on the socket because you aren't told that there is internal data waiting to be processed by
         // SSL_read().
         //
         // For this reason we set the RECALL flag so that we can be called again manually when we know that there is
         // data pending.

         #ifdef __linux__
            RegisterFD((HOSTHANDLE)Socket, RFD_RECALL|RFD_READ|RFD_SOCKET, (APTR)&client_server_incoming, Self);
         #elif _WIN32
            // In Windows we don't want to listen to FD's on a permanent basis,
            // so this is a temporary setting that will be reset by client_server_pending()

            RegisterFD((HOSTHANDLE)(MAXINT)Socket, RFD_RECALL|RFD_READ|RFD_SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&client_server_pending), (APTR)Self);
         #endif
      }

      STEP();
      return ERR_Okay;
   }
#endif

#ifdef __linux__
   {
      LONG result = recv(Socket, Buffer, BufferSize, Flags);

      if (result > 0) {
         *Result = result;
         STEP();
         return ERR_Okay;
      }
      else if (result IS 0) { // man recv() says: The return value is 0 when the peer has performed an orderly shutdown.
         STEP();
         return ERR_Disconnected;
      }
      else if ((errno IS EAGAIN) OR (errno IS EINTR)) {
         STEP();
         return ERR_Okay;
      }
      else {
         LogErrorMsg("recv() failed: %s", strerror(errno));
         STEP();
         return ERR_Failed;
      }
   }
#elif _WIN32
   STEP();
   return WIN_RECEIVE(Socket, Buffer, BufferSize, Flags, Result);
#else
   #error No support for RECEIVE()
#endif
}

//****************************************************************************

static ERROR SEND(objNetSocket *Self, SOCKET_HANDLE Socket, CPTR Buffer, LONG *Length, LONG Flags)
{
   if (!*Length) return ERR_Okay;

#ifdef ENABLE_SSL

   if (Self->SSL) {
      FMSG("~SEND()","SSLBusy: %d, Length: %d", Self->SSLBusy, *Length);

      if (Self->SSLBusy IS SSL_HANDSHAKE_WRITE) ssl_handshake_write(Socket, Self);
      else if (Self->SSLBusy IS SSL_HANDSHAKE_READ) ssl_handshake_read(Socket, Self);

      if (Self->SSLBusy != SSL_NOT_BUSY) {
         STEP();
         return ERR_Okay;
      }

      LONG bytes_sent = SSL_write(Self->SSL, Buffer, *Length);

      if (bytes_sent < 0) {
         *Length = 0;
         LONG ssl_error = SSL_get_error(Self->SSL, bytes_sent);

         switch(ssl_error){
            case SSL_ERROR_WANT_WRITE:
               FMSG("@SEND()","Buffer overflow (SSL want write)");
               STEP();
               return ERR_BufferOverflow;

            // We get a WANT_READ if we're trying to rehandshake and we block on write during the current connection.
            //
            // We need to wait on the socket to be readable but reinitiate our write when it is

            case SSL_ERROR_WANT_READ:
               MSG("SEND() Handshake requested by server.");
               Self->SSLBusy = SSL_HANDSHAKE_READ;
               #ifdef __linux__
                  RegisterFD((HOSTHANDLE)Socket, RFD_READ|RFD_SOCKET, &ssl_handshake_read, Self);
               #elif _WIN32
                  win_socketstate(Socket, TRUE, -1);
               #endif
               STEP();
               return ERR_Okay;

            case SSL_ERROR_SYSCALL:
               LogErrorMsg("SSL_write() SysError %d: %s", errno, strerror(errno));
               STEP();
               return ERR_Failed;

            default:
               while (ssl_error) {
                  LogErrorMsg("SSL_write() error %d, %s", ssl_error, ERR_error_string(ssl_error, NULL));
                  ssl_error = ERR_get_error();
               }

               STEP();
               return ERR_Failed;
         }
      }
      else {
         if (*Length != bytes_sent) {
            FMSG("@SEND:","Sent %d of requested %d bytes.", bytes_sent, *Length);
         }
         *Length = bytes_sent;
      }

      STEP();
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
         LogErrorMsg("send() failed: %s", strerror(errno));
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

static struct dns_resolver * new_resolver(LARGE ClientData, FUNCTION *Callback)
{
   struct dns_resolver *resolve;

   OBJECTPTR context = SetContext(glModule);
   ERROR error = AllocMemory(sizeof(struct dns_resolver), MEM_DATA, &resolve, NULL);
   SetContext(context);
   if (error) return NULL;

   resolve->next = glResolvers;
   resolve->time = PreciseTime();
   resolve->client_data = ClientData;
   resolve->callback = *Callback;
   #if defined(USE_ARES) && defined(__linux__)
      resolve->tcp = 0;
      resolve->udp = 0;
   #endif
   glResolvers = resolve;

   return resolve;
}

//***************************************************************************
// Acquire a lock on glDNS before calling this function.

static void free_resolver(struct dns_resolver *Resolver)
{
   LogF("~free_resolver()","Removing resolver %p", Resolver);

#if defined(USE_ARES) && defined(__linux__)
   if (Resolver->tcp) DeregisterFD((HOSTHANDLE)Resolver->tcp);
   if (Resolver->udp) DeregisterFD((HOSTHANDLE)Resolver->udp);
#endif

   // Remove the structure from the list.

   if (glResolvers IS Resolver) {
      glResolvers = Resolver->next;
   }
   else {
      struct dns_resolver *scan;
      for (scan=glResolvers; scan; scan=scan->next) {
         if (scan->next IS Resolver) { scan->next = Resolver->next; break; }
      }
   }

   OBJECTPTR context = SetContext(glModule);
   FreeResource(Resolver);
   SetContext(context);

   LogBack();
}

//***************************************************************************

static void resolve_callback(LARGE ClientData, FUNCTION *Callback, ERROR Error, CSTRING HostName,
   CSTRING *Aliases, LONG TotalAliases,
   struct IPAddress *Addresses, LONG TotalAddresses)
{
   if (Callback->Type IS CALL_STDC) {
      ERROR (*routine)(LARGE, ERROR, CSTRING, CSTRING *, LONG, struct IPAddress *, LONG);
      OBJECTPTR context = SetContext(Callback->StdC.Context);
         routine = reinterpret_cast<ERROR (*)(LARGE, ERROR, CSTRING, CSTRING *, LONG, struct IPAddress *, LONG)>(Callback->StdC.Routine);
         routine(ClientData, Error, HostName, Aliases, TotalAliases, Addresses, TotalAddresses);
      SetContext(context);
   }
   else if (Callback->Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Callback->Script.Script)) {
         const struct ScriptArg args[] = {
            { "ClientData",   FD_LARGE, { .Large   = ClientData } },
            { "Error",        FD_LONG,  { .Long    = Error } },
            { "HostName",     FD_STR,   { .Address = (STRING)HostName } },
            { "Aliases",      FD_ARRAY|FD_STR, { .Address = Aliases } },
            { "TotalAliases", FD_ARRAYSIZE|FD_LONG, { .Long = TotalAliases } },
            { "IPAddress:Addresses", FD_ARRAY|FD_STRUCT,   { .Address = Addresses } },
            { "TotalAddresses",      FD_ARRAYSIZE|FD_LONG, { .Long    = TotalAddresses } }
         };
         scCallback(script, Callback->Script.ProcedureID, args, ARRAYSIZE(args));
      }
   }
}

//***************************************************************************

static struct dns_cache * cache_host(struct hostent *Host)
{
   if ((!Host) OR (!Host->h_name)) return NULL;

   LogF("7cache_host()","Host: %s, Aliases: %p, Addresses: %p (IPV6: %d)", Host->h_name, Host->h_aliases, Host->h_addr_list, (Host->h_addrtype == AF_INET6));

   if ((Host->h_addrtype != AF_INET) AND (Host->h_addrtype != AF_INET6)) {
      return NULL;
   }

   // Calculate the size of the data structure.

   LONG size = sizeof(struct dns_cache) + ALIGN64(StrLength(Host->h_name) + 1);
   LONG alias_count = 0;
   LONG alias_size = 0;
   if ((Host->h_aliases) AND (Host->h_aliases[0])) {
      for (alias_count=0; (alias_count < MAX_ALIASES) AND (Host->h_aliases[alias_count]); alias_count++) {
         alias_size += StrLength(Host->h_aliases[alias_count]) + 1;
      }
   }

   LONG address_count = 0;
   if (Host->h_addr_list) {
      for (address_count=0; (address_count < MAX_ADDRESSES) AND (Host->h_addr_list[address_count]); address_count++);
   }

   size += (sizeof(STRING) * alias_count) + ALIGN64(alias_size);
   size += address_count * sizeof(struct IPAddress);

   // Allocate an empty key-pair and fill it.

   struct dns_cache *cache;
   if (VarSetSized(glDNS, Host->h_name, size, &cache, NULL) != ERR_Okay) return NULL;

   char *buffer = (char *)cache;
   LONG offset = sizeof(struct dns_cache);

   if (address_count > 0) {
      cache->Addresses = (struct IPAddress *)(buffer + offset);
      offset += address_count * sizeof(struct IPAddress);

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

   if (alias_count > 0) {
      cache->Aliases = (CSTRING *)(buffer + offset);
      offset += sizeof(STRING) * alias_count;

      LONG i;
      for (i=0; i < alias_count; i++) {
         STRING str_alias = (STRING)(buffer + offset);
         cache->Aliases[i] = str_alias;
         offset += StrCopy(Host->h_aliases[i], str_alias, COPY_ALL) + 1;
         FMSG("Alias","%s", cache->Aliases[i]);
      }

      offset = ALIGN64(offset);
   }
   else cache->Aliases = NULL;

   cache->HostName = (STRING)(buffer + offset);
   offset += StrCopy(Host->h_name, (STRING)cache->HostName, COPY_ALL) + 1;

   cache->AliasCount   = alias_count;
   cache->AddressCount = address_count;

   return cache;
}

//***************************************************************************

static BYTE check_machine_name(CSTRING HostName)
{
   WORD i;
   for (i=0; HostName[i]; i++) { // Check if it's a machine name
      if (HostName[i] IS '.') return FALSE;
   }
   return TRUE;
}

//****************************************************************************
// Non-Ares DNS callback.

#if defined(_WIN32)

void win_dns_callback(struct dns_resolver *Resolver, ERROR Error, struct hostent *Host)
{
   if (Host) {
      LogF("~win_dns_callback()","Resolved: '%s', Time: %.4fs", Host->h_name, (DOUBLE)(PreciseTime() - Resolver->time) * 0.000001);
   }
   else LogF("~win_dns_callback()","Failed to resolve host.  Time: %.4fs", (DOUBLE)(PreciseTime() - Resolver->time) * 0.000001);

   if (!VarLock(glDNS, 0x7fffffff)) {
      if (Error != ERR_Okay) {
         LogF("@rk_callback:","Name resolution failure: %s", GetErrorMsg(Error));
         resolve_callback(Resolver->client_data, &Resolver->callback, Error, Host->h_name, NULL, 0, NULL, 0);
      }
      else if (Host) {
         struct dns_cache *dns = cache_host(Host);
         if (dns) resolve_callback(Resolver->client_data, &Resolver->callback, ERR_Okay, dns->HostName, dns->Aliases, dns->AliasCount, dns->Addresses, dns->AddressCount);
      }

      free_resolver(Resolver);
      VarUnlock(glDNS);
   }

   LogBack();
}

#endif

//****************************************************************************

#ifdef USE_ARES
void ares_response(void *Arg, int Status, int Timeouts, struct hostent *Host)
{
   struct dns_resolver *Resolver = reinterpret_cast<struct dns_resolver *>(Arg);

   if (Host) {
      LogF("~ares_response()","Resolved: '%s', Time: %.2fs", Host->h_name, (DOUBLE)(PreciseTime() - Resolver->time) * 0.000001);
   }
   else LogF("~ares_response()","Failed to resolve host.  Time: %.2fs", (DOUBLE)(PreciseTime() - Resolver->time) * 0.000001);

   if (!VarLock(glDNS, 0x7fffffff)) {
      if (Status) {
         const char *msg;
         ERROR error = net_ares_error(Status, &msg);
         LogF("@ares_response","Name resolution failure: %s", msg);
         resolve_callback(Resolver->client_data, &Resolver->callback, error, (Host) ? Host->h_name : NULL, NULL, 0, NULL, 0);
      }
      else if (Host) {
         struct dns_cache *dns = cache_host(Host);
         if (dns) resolve_callback(Resolver->client_data, &Resolver->callback, ERR_Okay, dns->HostName, dns->Aliases, dns->AliasCount, dns->Addresses, dns->AddressCount);
      }

      free_resolver(Resolver);
      VarUnlock(glDNS);
   }

   LogBack();
}

void register_read_socket(int Socket, void (*Callback)(int, void *), struct dns_resolver *Resolve)
{
   RegisterFD(Socket, RFD_READ|RFD_SOCKET, Callback, Resolve);
}

void register_write_socket(int Socket, void (*Callback)(int, void *), struct dns_resolver *Resolve)
{
   RegisterFD(Socket, RFD_WRITE|RFD_SOCKET, Callback, Resolve);
}

void deregister_fd(int FD)
{
   DeregisterFD(FD);
}

void set_resolver_socket(struct dns_resolver *Resolver, int UDP, int SocketHandle)
{
   if (UDP) Resolver->udp = SocketHandle;
   else Resolver->tcp = SocketHandle;
}

#endif // USE_ARES

//****************************************************************************

#include "netsocket/netsocket.cpp"
#include "clientsocket/clientsocket.cpp"
#include "class_proxy.cpp"

//****************************************************************************

PARASOL_MOD(MODInit, NULL, MODOpen, MODExpunge, MODVERSION_NETWORK)

/*****************************************************************************
                                 BACKTRACE IT
*****************************************************************************/
