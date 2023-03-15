/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-MODULE-
Network: Provides miscellaneous network functions and hosts the NetSocket and ClientSocket classes.

The Network module exports a few miscellaneous networking functions.  For core network functionality surrounding
sockets and HTTP, please refer to the @NetSocket and @HTTP classes.
-END-

*********************************************************************************************************************/

//#define DEBUG

#define PRV_PROXY
#define PRV_NETLOOKUP
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

class extClientSocket : public objClientSocket {
   public:
   union {
      SOCKET_HANDLE SocketHandle;
      SOCKET_HANDLE Handle;
   };
   struct NetQueue WriteQueue; // Writes to the network socket are queued here in a buffer
   struct NetQueue ReadQueue;  // Read queue, often used for reading whole messages
   UBYTE OutgoingRecursion;
   UBYTE InUse;
};

class extNetSocket : public objNetSocket {
   public:
   SOCKET_HANDLE SocketHandle;   // Handle of the socket
   FUNCTION Outgoing;
   FUNCTION Incoming;
   FUNCTION Feedback;
   objNetLookup *NetLookup;
   struct NetClient *LastClient;
   struct NetQueue WriteQueue;
   struct NetQueue ReadQueue;
   UBYTE  ReadCalled:1;          // The Read() action sets this to TRUE whenever called.
   UBYTE  IPV6:1;
   UBYTE  Terminating:1;         // Set to TRUE when the NetSocket is marked for deletion.
   UBYTE  ExternalSocket:1;      // Set to TRUE if the SocketHandle field was set manually by the client.
   UBYTE  InUse;                 // Recursion counter to signal that the object is doing something.
   UBYTE  SSLBusy;               // Tracks the current actions of SSL handshaking.
   UBYTE  IncomingRecursion;     // Used by netsocket_client to prevent recursive handling of incoming data.
   UBYTE  OutgoingRecursion;
   #ifdef _WIN32
      #ifdef NO_NETRECURSION
         WORD WinRecursion; // For win32_netresponse()
      #endif
      union {
         void (*ReadSocket)(SOCKET_HANDLE, extNetSocket *);
         void (*ReadClientSocket)(SOCKET_HANDLE, extClientSocket *);
      };
      union {
         void (*WriteSocket)(SOCKET_HANDLE, extNetSocket *);
         void (*WriteClientSocket)(SOCKET_HANDLE, extClientSocket *);
      };
   #endif
   #ifdef ENABLE_SSL
      SSL *SSL;
      SSL_CTX *CTX;
      BIO *BIO;
   #endif
};

class extNetLookup : public objNetLookup {
   public:
   FUNCTION Callback;
   struct DNSEntry Info;
   std::unordered_set<OBJECTID> *Threads;
   std::mutex *ThreadLock;
};

//********************************************************************************************************************

enum {
   SSL_NOT_BUSY=0,
   SSL_HANDSHAKE_READ,
   SSL_HANDSHAKE_WRITE
};

#ifdef _WIN32
   #include "win32/winsockwrappers.h"
#endif

#include "module_def.c"

#ifdef REVERSE_BYTEORDER
inline ULONG cpu_be32(ULONG x) {
   return ((((UBYTE)x)<<24)|(((UBYTE)(x>>8))<<16)|((x>>8) & 0xff00)|(x>>24));
}
#define be32_cpu(x) ((x<<24)|((x<<8) & 0xff0000)|((x>>8) & 0xff00)|(x>>24))
#define cpu_be16(x) ((x<<8)|(x>>8))
#define be16_cpu(x) ((x<<8)|(x>>8))
#else
#define cpu_be32(x) (x)
#define be32_cpu(x) (x)
#define cpu_be16(x) (x)
#define be16_cpu(x) (x)
#endif

struct CoreBase *CoreBase;
static OBJECTPTR glModule = NULL;

#ifdef ENABLE_SSL
static BYTE ssl_init = FALSE;

static ERROR sslConnect(extNetSocket *);
static void sslDisconnect(extNetSocket *);
static ERROR sslInit(void);
static ERROR sslLinkSocket(extNetSocket *);
static ERROR sslSetup(extNetSocket *);
#endif

static OBJECTPTR clNetLookup = NULL;
static OBJECTPTR clProxy = NULL;
static OBJECTPTR clNetSocket = NULL;
static OBJECTPTR clClientSocket = NULL;
static KeyStore *glHosts = NULL;
static KeyStore *glAddresses = NULL;
static LONG glResolveNameMsgID = 0;
static LONG glResolveAddrMsgID = 0;

static void client_server_incoming(SOCKET_HANDLE, extNetSocket *);
static BYTE check_machine_name(CSTRING HostName) __attribute__((unused));
static ERROR resolve_name_receiver(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize);
static ERROR resolve_addr_receiver(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize);

static ERROR init_netsocket(void);
static ERROR init_clientsocket(void);
static ERROR init_proxy(void);
static ERROR init_netlookup(void);

static MsgHandler *glResolveNameHandler = NULL;
static MsgHandler *glResolveAddrHandler = NULL;

//********************************************************************************************************************

ERROR MODInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   pf::Log log;

   CoreBase = argCoreBase;

   argModule->getPtr(FID_Root, &glModule);

   glHosts = VarNew(64, KSF_THREAD_SAFE);
   glAddresses = VarNew(64, KSF_THREAD_SAFE);

   if (init_netsocket()) return ERR_AddClass;
   if (init_clientsocket()) return ERR_AddClass;
   if (init_proxy()) return ERR_AddClass;
   if (init_netlookup()) return ERR_AddClass;

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

//********************************************************************************************************************

ERROR MODOpen(OBJECTPTR Module)
{
   Module->set(FID_FunctionList, glFunctions);
   return ERR_Okay;
}

//********************************************************************************************************************
// Note: Take care and attention with the order of operations during the expunge process, particuarly due to the
// background processes that are managed by the module.

static ERROR MODExpunge(void)
{
   pf::Log log;

#ifdef _WIN32
   SetResourcePtr(RES_NET_PROCESSING, NULL);
#endif

   if (glResolveNameHandler) { FreeResource(glResolveNameHandler); glResolveNameHandler = NULL; }
   if (glResolveAddrHandler) { FreeResource(glResolveAddrHandler); glResolveAddrHandler = NULL; }
   if (glHosts)              { FreeResource(glHosts); glHosts = NULL; }
   if (glAddresses)          { FreeResource(glAddresses); glAddresses = NULL; }

#ifdef _WIN32
   log.msg("Closing winsock.");

   if (ShutdownWinsock() != 0) log.warning("Warning: Winsock DLL Cleanup failed.");
#endif

   if (clNetSocket)    { acFree(clNetSocket); clNetSocket = NULL; }
   if (clClientSocket) { acFree(clClientSocket); clClientSocket = NULL; }
   if (clProxy)        { acFree(clProxy); clProxy = NULL; }
   if (clNetLookup)    { acFree(clNetLookup); clNetLookup = NULL; }

#ifdef ENABLE_SSL
   if (ssl_init) {
      ERR_free_strings();
      EVP_cleanup();
      CRYPTO_cleanup_all_ex_data();
   }
#endif

   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
AddressToStr: Converts an IPAddress structure to an IPAddress in dotted string form.

Converts an IPAddress structure to a string containing the IPAddress in dotted format.  Please free the resulting
string with <function>FreeResource</> once it is no longer required.

-INPUT-
struct(IPAddress) IPAddress: A pointer to the IPAddress structure.

-RESULT-
!cstr: The IP address is returned as an allocated string.

*********************************************************************************************************************/

static CSTRING netAddressToStr(IPAddress *Address)
{
   pf::Log log(__FUNCTION__);

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

/*********************************************************************************************************************

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

*********************************************************************************************************************/

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

/*********************************************************************************************************************

-FUNCTION-
HostToShort: Converts a 16 bit (unsigned) word from host to network byte order.

Converts a 16 bit (unsigned) word from host to network byte order.

-INPUT-
uint Value: Data in host byte order to be converted to network byte order

-RESULT-
uint: The word in network byte order

*********************************************************************************************************************/

static ULONG netHostToShort(ULONG Value)
{
   return (ULONG)htons((UWORD)Value);
}

/*********************************************************************************************************************

-FUNCTION-
HostToLong: Converts a 32 bit (unsigned) long from host to network byte order.

Converts a 32 bit (unsigned) long from host to network byte order.

-INPUT-
uint Value: Data in host byte order to be converted to network byte order

-RESULT-
uint: The long in network byte order

*********************************************************************************************************************/

static ULONG netHostToLong(ULONG Value)
{
   return htonl(Value);
}

/*********************************************************************************************************************

-FUNCTION-
ShortToHost: Converts a 16 bit (unsigned) word from network to host byte order.

Converts a 16 bit (unsigned) word from network to host byte order.

-INPUT-
uint Value: Data in network byte order to be converted to host byte order

-RESULT-
uint: The Value in host byte order

*********************************************************************************************************************/

static ULONG netShortToHost(ULONG Value)
{
   return (ULONG)ntohs((UWORD)Value);
}

/*********************************************************************************************************************

-FUNCTION-
LongToHost: Converts a 32 bit (unsigned) long from network to host byte order.

Converts a 32 bit (unsigned) long from network to host byte order.

-INPUT-
uint Value: Data in network byte order to be converted to host byte order

-RESULT-
uint: The Value in host byte order.

*********************************************************************************************************************/

static ULONG netLongToHost(ULONG Value)
{
   return ntohl(Value);
}

/*********************************************************************************************************************

-FUNCTION-
SetSSL: Alters SSL settings on an initialised NetSocket object.

Use the SetSSL() function to send SSL commands to a NetSocket object.  The following table illustrates the commands
that are currently available:





If a failure occurs when executing a command, the execution of all further commands is aborted and the error code is
returned immediately.

-INPUT-
ext(NetSocket) NetSocket: The target NetSocket object.
tags Tags: Series of tags terminated by TAGEND.

-ERRORS-
Okay:
NullArgs: The NetSocket argument was not specified.
-END-

*********************************************************************************************************************/

static ERROR netSetSSL(extNetSocket *Socket, ...)
{
#ifdef ENABLE_SSL
   LONG value, tagid;
   ERROR error;
   va_list list;

   if (!Socket) return ERR_NullArgs;

   va_start(list, Socket);
   while ((tagid = va_arg(list, LONG))) {
      pf::Log log(__FUNCTION__);
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

//********************************************************************************************************************
// Used by RECEIVE() SSL support.

#ifdef _WIN32
static void client_server_pending(SOCKET_HANDLE FD, APTR Self) __attribute__((unused));
static void client_server_pending(SOCKET_HANDLE FD, APTR Self)
{
   #pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
   RegisterFD((HOSTHANDLE)((extNetSocket *)Self)->SocketHandle, RFD_REMOVE|RFD_READ|RFD_SOCKET, NULL, NULL);
   #pragma GCC diagnostic warning "-Wint-to-pointer-cast"
   client_server_incoming(FD, (extNetSocket *)Self);
}
#endif

//********************************************************************************************************************

static ERROR RECEIVE(extNetSocket *Self, SOCKET_HANDLE Socket, APTR Buffer, LONG BufferSize, LONG Flags, LONG *Result)
{
   pf::Log log(__FUNCTION__);

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

//********************************************************************************************************************

static ERROR SEND(extNetSocket *Self, SOCKET_HANDLE Socket, CPTR Buffer, LONG *Length, LONG Flags)
{
   pf::Log log(__FUNCTION__);

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

//********************************************************************************************************************

static BYTE check_machine_name(CSTRING HostName)
{
   for (LONG i=0; HostName[i]; i++) { // Check if it's a machine name
      if (HostName[i] IS '.') return FALSE;
   }
   return TRUE;
}

//********************************************************************************************************************

#include "netsocket/netsocket.cpp"
#include "clientsocket/clientsocket.cpp"
#include "class_proxy.cpp"
#include "class_netlookup.cpp"

//********************************************************************************************************************

static STRUCTS glStructures = {
   { "DNSEntry",  sizeof(DNSEntry) },
   { "IPAddress", sizeof(IPAddress) },
   { "NetClient", sizeof(NetClient) },
   { "NetMsg",    sizeof(NetMsg) },
   { "NetMsgEnd", sizeof(NetMsgEnd) },
   { "NetQueue",  sizeof(NetQueue) }
};

PARASOL_MOD(MODInit, NULL, MODOpen, MODExpunge, MODVERSION_NETWORK, MOD_IDL, &glStructures)

/*********************************************************************************************************************
                                 BACKTRACE IT
*********************************************************************************************************************/
