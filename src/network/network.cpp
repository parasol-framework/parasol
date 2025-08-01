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
#include <parasol/strings.hpp>
#include <thread>

#ifdef ENABLE_SSL
  #ifdef _WIN32
    #include "win32/ssl_wrapper.h"
  #else
    #include <openssl/ssl.h>
    #include <openssl/err.h>
    #include <openssl/bio.h>
    #include <openssl/bn.h>
    #include <openssl/rsa.h>
    #include <openssl/evp.h>
    #include <openssl/x509.h>
    #include <openssl/pem.h>
  #endif
#endif

#include <unordered_set>
#include <stack>
#include <mutex>
#include <span>
#include <cstring>

struct DNSEntry {
   std::string HostName;
   std::vector<IPAddress> Addresses;    // IP address list

   DNSEntry & operator=(DNSEntry other) {
      std::swap(HostName, other.HostName);
      std::swap(Addresses, other.Addresses);
      return *this;
   }
};

#ifdef __linux__
typedef int SOCKET_HANDLE;
#elif _WIN32
typedef uint32_t SOCKET_HANDLE; // NOTE: declared as uint32_t instead of SOCKET for now to avoid including winsock.h
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
         struct { uint8_t s_b1,s_b2,s_b3,s_b4; } S_un_b;
         struct { UWORD s_w1,s_w2; } S_un_w;
         uint32_t S_addr;
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

   constexpr uint32_t NOHANDLE = (uint32_t)(~0);
   constexpr int SOCKET_ERROR = -1;
   constexpr int AF_INET      = 2;
   constexpr int AF_INET6     = 23;
   constexpr int INADDR_ANY   = 0;
   constexpr int MSG_PEEK     = 2;

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
   uint8_t OutgoingRecursion;
   uint8_t InUse;
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
   uint8_t ReadCalled:1;          // The Read() action sets this to TRUE whenever called.
   uint8_t IPV6:1;
   uint8_t Terminating:1;         // Set to TRUE when the NetSocket is marked for deletion.
   uint8_t ExternalSocket:1;      // Set to TRUE if the SocketHandle field was set manually by the client.
   uint8_t InUse;                 // Recursion counter to signal that the object is doing something.
   #ifndef _WIN32
      uint8_t  SSLBusy;            // Tracks the current actions of SSL handshaking.
   #endif
   uint8_t IncomingRecursion;     // Used by netsocket_client to prevent recursive handling of incoming data.
   uint8_t OutgoingRecursion;
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
      #ifdef _WIN32
         union {
            SSL_HANDLE SSL;
            SSL_HANDLE WinSSL;
         };
      #else
        SSL *SSL;
        SSL_CTX *CTX;
        BIO *BIO;
      #endif
   #endif
 
   ERR write_queue(NetQueue &Queue, CPTR Message, size_t Length);
};

class extNetLookup : public objNetLookup {
   public:
   FUNCTION Callback;
   struct DNSEntry Info;
   std::vector<std::unique_ptr<std::jthread>> Threads; // Simple mechanism for auto-joining all the threads on object destruction
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

JUMPTABLE_CORE

#ifdef ENABLE_SSL
  #ifdef _WIN32
    // Windows SSL wrapper forward declarations
    static ERR sslConnect(extNetSocket *);
    static void sslDisconnect(extNetSocket *);
    static ERR sslSetup(extNetSocket *);
  #else
    // OpenSSL forward declarations (non-Windows)
    static bool ssl_init = false;
    static ERR sslConnect(extNetSocket *);
    static void sslDisconnect(extNetSocket *);
    static ERR sslLinkSocket(extNetSocket *);
    static ERR sslSetup(extNetSocket *);
  #endif
#endif

//********************************************************************************************************************

struct CaseInsensitiveMap {
   bool operator() (const std::string &lhs, const std::string &rhs) const {
      return ::strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
   }
};

typedef std::map<std::string, DNSEntry, CaseInsensitiveMap> HOSTMAP;

//********************************************************************************************************************

static OBJECTPTR clNetLookup = nullptr;
static OBJECTPTR clProxy = nullptr;
static OBJECTPTR clNetSocket = nullptr;
static OBJECTPTR clClientSocket = nullptr;
static HOSTMAP glHosts;
static HOSTMAP glAddresses;
static MSGID glResolveNameMsgID = MSGID::NIL;
static MSGID glResolveAddrMsgID = MSGID::NIL;

static void client_server_incoming(SOCKET_HANDLE, extNetSocket *);
static int8_t check_machine_name(CSTRING HostName) __attribute__((unused));
static ERR resolve_name_receiver(APTR Custom, MSGID MsgID, int MsgType, APTR Message, int MsgSize);
static ERR resolve_addr_receiver(APTR Custom, MSGID MsgID, int MsgType, APTR Message, int MsgSize);

static ERR init_netsocket(void);
static ERR init_clientsocket(void);
static ERR init_proxy(void);
static ERR init_netlookup(void);

static MsgHandler *glResolveNameHandler = nullptr;
static MsgHandler *glResolveAddrHandler = nullptr;

//********************************************************************************************************************

static ERR MODInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   pf::Log log;

   CoreBase = argCoreBase;

   if (init_netsocket() != ERR::Okay) return ERR::AddClass;
   if (init_clientsocket() != ERR::Okay) return ERR::AddClass;
   if (init_proxy() != ERR::Okay) return ERR::AddClass;
   if (init_netlookup() != ERR::Okay) return ERR::AddClass;

   glResolveNameMsgID = (MSGID)AllocateID(IDTYPE::MESSAGE);
   glResolveAddrMsgID = (MSGID)AllocateID(IDTYPE::MESSAGE);

#ifdef _WIN32
   // Configure Winsock
   {
      CSTRING msg;
      if ((msg = StartupWinsock()) != 0) {
         log.warning("Winsock initialisation failed: %s", msg);
         return ERR::SystemCall;
      }
      SetResourcePtr(RES::NET_PROCESSING, reinterpret_cast<APTR>(win_net_processing)); // Hooks into ProcessMessages()
   }
#endif

   auto recv_function = C_FUNCTION(resolve_name_receiver);
   recv_function.Context = CurrentTask();
   if (AddMsgHandler(nullptr, glResolveNameMsgID, &recv_function, &glResolveNameHandler) != ERR::Okay) {
      return ERR::Failed;
   }

   recv_function.Routine = (APTR)resolve_addr_receiver;
   if (AddMsgHandler(nullptr, glResolveAddrMsgID, &recv_function, &glResolveAddrHandler) != ERR::Okay) {
      return ERR::Failed;
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR MODOpen(OBJECTPTR Module)
{
   Module->set(FID_FunctionList, glFunctions);
   return ERR::Okay;
}

//********************************************************************************************************************
// Note: Take care and attention with the order of operations during the expunge process, particuarly due to the
// background processes that are managed by the module.

static ERR MODExpunge(void)
{
   pf::Log log;

#ifdef _WIN32
   SetResourcePtr(RES::NET_PROCESSING, nullptr);
#endif

   if (glResolveNameHandler) { FreeResource(glResolveNameHandler); glResolveNameHandler = nullptr; }
   if (glResolveAddrHandler) { FreeResource(glResolveAddrHandler); glResolveAddrHandler = nullptr; }

#ifdef _WIN32
   log.msg("Closing winsock.");

   if (ShutdownWinsock() != 0) log.warning("Warning: Winsock DLL Cleanup failed.");
#endif

   if (clNetSocket)    { FreeResource(clNetSocket); clNetSocket = nullptr; }
   if (clClientSocket) { FreeResource(clClientSocket); clClientSocket = nullptr; }
   if (clProxy)        { FreeResource(clProxy); clProxy = nullptr; }
   if (clNetLookup)    { FreeResource(clNetLookup); clNetLookup = nullptr; }

#ifdef ENABLE_SSL
  #ifdef _WIN32
    ssl_wrapper_cleanup();
  #else
    if (ssl_init) {
       ERR_free_strings();
       EVP_cleanup();
       CRYPTO_cleanup_all_ex_data();
    }
  #endif
#endif

   return ERR::Okay;
}

namespace net {

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

CSTRING AddressToStr(IPAddress *Address)
{
   pf::Log log(__FUNCTION__);

   if (!Address) return nullptr;

   if (Address->Type != IPADDR::V4) {
      log.warning("Only IPv4 Addresses are supported currently");
      return nullptr;
   }

   struct in_addr addr;
   addr.s_addr = net::HostToLong(Address->Data[0]);

   STRING result;
#ifdef __linux__
   result = inet_ntoa(addr);
#elif _WIN32
   result = win_inet_ntoa(addr.s_addr);
#endif

   if (!result) return nullptr;
   return pf::strclone(result);
}

/*********************************************************************************************************************

-FUNCTION-
StrToAddress: Converts an IP Address in string form to an !IPAddress structure.

Converts an IPv4 or an IPv6 address in dotted string format to an !IPAddress structure.  The `String` must be of form
`1.2.3.4` (IPv4).

<pre>
struct IPAddress addr;
if (!StrToAddress("127.0.0.1", &addr)) {
   ...
}
</pre>

-INPUT-
cstr String:  A null-terminated string containing the IP Address in dotted format.
struct(IPAddress) Address: Must point to an !IPAddress structure that will be filled in.

-ERRORS-
Okay:    The `Address` was converted successfully.
NullArgs
Failed:  The `String` was not a valid IP Address.

*********************************************************************************************************************/

ERR StrToAddress(CSTRING Str, IPAddress *Address)
{
   if ((!Str) or (!Address)) return ERR::NullArgs;

#ifdef __linux__
   uint32_t result = inet_addr(Str);
#elif _WIN32
   uint32_t result = win_inet_addr(Str);
#endif

   if (result IS INADDR_NONE) return ERR::Failed;

   Address->Data[0] = net::LongToHost(result);
   Address->Data[1] = 0;
   Address->Data[2] = 0;
   Address->Data[3] = 0;
   Address->Type = IPADDR::V4;
   Address->Pad = 0;

   return ERR::Okay;
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

uint32_t HostToShort(uint32_t Value)
{
   return (uint32_t)htons((UWORD)Value);
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

uint32_t HostToLong(uint32_t Value)
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

uint32_t ShortToHost(uint32_t Value)
{
   return (uint32_t)ntohs((UWORD)Value);
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

uint32_t LongToHost(uint32_t Value)
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
obj(NetSocket) NetSocket: The target NetSocket object.
tags Tags: Series of tags terminated by TAGEND.

-ERRORS-
Okay:
NullArgs: The NetSocket argument was not specified.
-END-

*********************************************************************************************************************/

ERR SetSSL(objNetSocket *Socket, ...)
{
#ifdef ENABLE_SSL
   int value, tagid;
   ERR error;
   va_list list;

   if (!Socket) return ERR::NullArgs;

   va_start(list, Socket);
   while ((tagid = va_arg(list, LONG))) {
      pf::Log log(__FUNCTION__);
      log.traceBranch("Command: %d", tagid);

      switch(NSL(tagid)) {
         case NSL::CONNECT:
            value = va_arg(list, LONG);
            if (value) { // Initiate an SSL connection on this socket
               if ((error = sslSetup((extNetSocket *)Socket)) IS ERR::Okay) {
                  error = sslConnect((extNetSocket *)Socket);
               }

               if (error != ERR::Okay) {
                  va_end(list);
                  return error;
               }
            }
            else { // Disconnect SSL (i.e. go back to unencrypted mode)
               sslDisconnect((extNetSocket *)Socket);
            }
            break;
         default:
            break;
      }
   }

   va_end(list);
   return ERR::Okay;
#else
   return ERR::NoSupport;
#endif
}

} // namespace

#ifdef ENABLE_SSL
  #ifdef _WIN32
    #include "win32_ssl.cpp"
  #else
    #include "ssl.cpp"
  #endif
#endif

//********************************************************************************************************************
// Used by RECEIVE() SSL support.

#ifdef _WIN32
static void client_server_pending(SOCKET_HANDLE FD, APTR Self) __attribute__((unused));
static void client_server_pending(SOCKET_HANDLE FD, APTR Self)
{
   #pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
   RegisterFD((HOSTHANDLE)((extNetSocket *)Self)->SocketHandle, RFD::REMOVE|RFD::READ|RFD::SOCKET, nullptr, nullptr);
   #pragma GCC diagnostic warning "-Wint-to-pointer-cast"
   client_server_incoming(FD, (extNetSocket *)Self);
}
#endif

//********************************************************************************************************************

static ERR RECEIVE(extNetSocket *Self, SOCKET_HANDLE Socket, APTR Buffer, int BufferSize, int Flags, int *Result)
{
   pf::Log log(__FUNCTION__);

#ifdef _WIN32
   log.traceBranch("Socket: %d, BufSize: %d, Flags: $%.8x", Socket, BufferSize, Flags);
#else
   log.traceBranch("Socket: %d, BufSize: %d, Flags: $%.8x, SSLBusy: %d", Socket, BufferSize, Flags, Self->SSLBusy);
#endif

   *Result = 0;

#ifdef ENABLE_SSL
  #ifdef _WIN32
    // Windows SSL wrapper doesn't use handshake busy state
  #else
    if (Self->SSLBusy IS SSL_HANDSHAKE_WRITE) ssl_handshake_write(Socket, Self);
    else if (Self->SSLBusy IS SSL_HANDSHAKE_READ) ssl_handshake_read(Socket, Self);

    if (Self->SSLBusy != SSL_NOT_BUSY) return ERR::Okay;
  #endif
#endif

   if (!BufferSize) return ERR::Okay;

#ifdef ENABLE_SSL
  #ifdef _WIN32
    if (Self->WinSSL) {
       // If we're in the middle of SSL handshake, read raw data for handshake processing
       if (Self->State IS NTC::CONNECTING_SSL) {
          log.trace("Windows SSL handshake in progress, reading raw data.");
          ERR error = WIN_RECEIVE(Socket, Buffer, BufferSize, Flags, Result);
          if ((error IS ERR::Okay) and (*Result > 0)) {
             sslHandshakeReceived(Self, Buffer, *Result);
          }
          return error;
       }
       else { // Normal SSL data read for established connections
          if (int result = ssl_wrapper_read(Self->WinSSL, Buffer, BufferSize); result > 0) {
             *Result = result;
             return ERR::Okay;
          }
          else if (!result) {
             return ERR::Disconnected;
          }
          else {
             SSL_ERROR_CODE error = ssl_wrapper_get_error(Self->WinSSL);
             if (error IS SSL_ERROR_WOULD_BLOCK) {
                log.traceWarning("No more data to read from the SSL socket.");
                return ERR::Okay;
             }
             else {
                log.warning("Windows SSL read error: %d", error);
                return ERR::Failed;
             }
          }
       }
    }
  #else
    int8_t read_blocked;
    int pending;

    if (Self->SSL) {
      do {
         read_blocked = 0;

         int result = SSL_read(Self->SSL, Buffer, BufferSize);

         if (result <= 0) {
            switch (SSL_get_error(Self->SSL, result)) {
               case SSL_ERROR_ZERO_RETURN:
                  return ERR::Disconnected;

               case SSL_ERROR_WANT_READ:
                  read_blocked = TRUE;
                  break;

                case SSL_ERROR_WANT_WRITE:
                  // WANT_WRITE is returned if we're trying to rehandshake and the write operation would block.  We
                  // need to wait on the socket to be writeable, then restart the read when it is.

                   log.msg("SSL socket handshake requested by server.");
                   Self->SSLBusy = SSL_HANDSHAKE_WRITE;
                   #ifdef __linux__
                      RegisterFD((HOSTHANDLE)Socket, RFD::WRITE|RFD::SOCKET, &ssl_handshake_write, Self);
                   #else
                      win_socketstate(Socket, -1, TRUE);
                   #endif

                   return ERR::Okay;

                case SSL_ERROR_SYSCALL:

                default:
                   log.warning("SSL read problem");
                   return ERR::Okay; // Non-fatal
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
            RegisterFD((HOSTHANDLE)Socket, RFD::RECALL|RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&client_server_incoming), Self);
         #elif _WIN32
            // In Windows we don't want to listen to FD's on a permanent basis,
            // so this is a temporary setting that will be reset by client_server_pending()

            RegisterFD((HOSTHANDLE)(MAXINT)Socket, RFD::RECALL|RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&client_server_pending), (APTR)Self);
         #endif
      }

      return ERR::Okay;
    }
  #endif
#endif

#ifdef __linux__
   {
      int result = recv(Socket, Buffer, BufferSize, Flags);

      if (result > 0) {
         *Result = result;
         return ERR::Okay;
      }
      else if (result IS 0) { // man recv() says: The return value is 0 when the peer has performed an orderly shutdown.
         return ERR::Disconnected;
      }
      else if ((errno IS EAGAIN) or (errno IS EINTR)) {
         return ERR::Okay;
      }
      else {
         log.warning("recv() failed: %s", strerror(errno));
         return ERR::Failed;
      }
   }
#elif _WIN32
   return WIN_RECEIVE(Socket, Buffer, BufferSize, Flags, Result);
#else
   #error No support for RECEIVE()
#endif
}

//********************************************************************************************************************

static ERR SEND(extNetSocket *Self, SOCKET_HANDLE Socket, CPTR Buffer, size_t *Length, int Flags)
{
   pf::Log log(__FUNCTION__);

   if (!*Length) return ERR::Okay;

#ifdef ENABLE_SSL
  #ifdef _WIN32
    if (Self->WinSSL) {
       log.traceBranch("Windows SSL Length: %d", int(*Length));

       size_t bytes_sent = ssl_wrapper_write(Self->WinSSL, Buffer, *Length);

       if (bytes_sent > 0) {
          if (*Length != bytes_sent) {
             log.traceWarning("Sent %d of requested %d bytes.", int(bytes_sent), int(*Length));
          }
          *Length = bytes_sent;
          return ERR::Okay;
       }
       else {
          SSL_ERROR_CODE error = ssl_wrapper_get_error(Self->WinSSL);
          *Length = 0;

          if (error IS SSL_ERROR_WOULD_BLOCK) {
             log.traceWarning("Buffer overflow (SSL want write)");
             return ERR::BufferOverflow;
          }
          else {
             log.warning("Windows SSL write error: %d", error);
             return ERR::Failed;
          }
       }
    }
  #else
    if (Self->SSL) {
       log.traceBranch("SSLBusy: %d, Length: %d", Self->SSLBusy, int(*Length));

      if (Self->SSLBusy IS SSL_HANDSHAKE_WRITE) ssl_handshake_write(Socket, Self);
      else if (Self->SSLBusy IS SSL_HANDSHAKE_READ) ssl_handshake_read(Socket, Self);

      if (Self->SSLBusy != SSL_NOT_BUSY) return ERR::Okay;

      int bytes_sent = SSL_write(Self->SSL, Buffer, *Length);

      if (bytes_sent < 0) {
         *Length = 0;
         int ssl_error = SSL_get_error(Self->SSL, bytes_sent);

         switch(ssl_error){
            case SSL_ERROR_WANT_WRITE:
               log.traceWarning("Buffer overflow (SSL want write)");
               return ERR::BufferOverflow;

            // We get a WANT_READ if we're trying to rehandshake and we block on write during the current connection.
            //
            // We need to wait on the socket to be readable but reinitiate our write when it is

            case SSL_ERROR_WANT_READ:
               log.trace("Handshake requested by server.");
               Self->SSLBusy = SSL_HANDSHAKE_READ;
               #ifdef __linux__
                  RegisterFD((HOSTHANDLE)Socket, RFD::READ|RFD::SOCKET, &ssl_handshake_read, Self);
               #elif _WIN32
                  win_socketstate(Socket, TRUE, -1);
               #endif
               return ERR::Okay;

            case SSL_ERROR_SYSCALL:
               log.warning("SSL_write() SysError %d: %s", errno, strerror(errno));
               return ERR::Failed;

            default:
               while (ssl_error) {
                  log.warning("SSL_write() error %d, %s", ssl_error, ERR_error_string(ssl_error, nullptr));
                  ssl_error = ERR_get_error();
               }

               return ERR::Failed;
         }
      }
      else {
         if (*Length != bytes_sent) {
            log.traceWarning("Sent %d of requested %d bytes.", bytes_sent, *Length);
         }
         *Length = bytes_sent;
      }

      return ERR::Okay;
    }
  #endif
#endif

#ifdef __linux__
   *Length = send(Socket, Buffer, *Length, Flags);

   if (*Length >= 0) return ERR::Okay;
   else {
      *Length = 0;
      if (errno IS EAGAIN) return ERR::BufferOverflow;
      else if (errno IS EMSGSIZE) return ERR::DataSize;
      else {
         log.warning("send() failed: %s", strerror(errno));
         return ERR::Failed;
      }
   }
#elif _WIN32
   return WIN_SEND(Socket, Buffer, Length, Flags);
#else
   #error No support for SEND()
#endif
}

//********************************************************************************************************************

static int8_t check_machine_name(CSTRING HostName)
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
   { "NetQueue",  sizeof(NetQueue) }
};

PARASOL_MOD(MODInit, nullptr, MODOpen, MODExpunge, MOD_IDL, &glStructures)
extern "C" struct ModHeader * register_network_module() { return &ModHeader; }

/*********************************************************************************************************************
                                                     BACKTRACE IT
*********************************************************************************************************************/
