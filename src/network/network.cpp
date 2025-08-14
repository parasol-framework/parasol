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
#define PRV_NETCLIENT

#include <stdio.h>
#include <sys/types.h>
#include <unordered_set>
#include <ctime>
#include <type_traits>

#include <parasol/main.h>
#include <parasol/modules/network.h>
#include <parasol/strings.hpp>

#ifndef DISABLE_SSL
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
#include <thread>

std::mutex glmThreads;
std::unordered_set<std::shared_ptr<std::jthread>> glThreads;

static int glSocketLimit = 0x7fffffff; // System imposed socket limit

struct NetQueue {
   uint32_t Index;    // The current read/write position within the buffer
   std::vector<uint8_t> Buffer; // The buffer hosting the data
   ERR write(CPTR Message, size_t Length);
};

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

   struct sockaddr_in6 {
      short sin6_family;
      UWORD sin6_port;
      uint32_t sin6_flowinfo;
      struct in6_addr sin6_addr;
      uint32_t sin6_scope_id;
   };

   struct sockaddr_storage {
      short ss_family;
      char __ss_pad1[6];
      int64_t __ss_align;
      char __ss_pad2[112];
   };

   constexpr uint32_t NOHANDLE = (uint32_t)(~0);
   constexpr int SOCKET_ERROR = -1;
   constexpr int AF_INET      = 2;
   constexpr int AF_INET6     = 23;
   constexpr int INADDR_ANY   = 0;
   constexpr int MSG_PEEK     = 2;
   constexpr int IPPROTO_IPV6 = 41;
   constexpr int IPV6_V6ONLY  = 27;

   // getaddrinfo constants
   constexpr int AF_UNSPEC    = 0;
   constexpr int AI_CANONNAME = 2;
   constexpr int EAI_AGAIN    = 2;
   constexpr int EAI_FAIL     = 3;
   constexpr int EAI_MEMORY   = 4;
   constexpr int EAI_SYSTEM   = 5;

   // IPv6 constants
   static const struct in6_addr in6addr_any = {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};

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
   #include <netinet/tcp.h>
   #include <sys/socket.h>

   #define NOHANDLE -1

   static void CLOSESOCKET(SOCKET_HANDLE Handle) {
      pf::Log log(__FUNCTION__);
      log.traceBranch("Handle: %d", Handle);
      if (Handle IS NOHANDLE) return;

      // Perform graceful disconnect before closing

      shutdown(Handle, SHUT_RDWR);

      // Set a short timeout to allow pending data to be transmitted
      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = 100000; // 100ms timeout
      setsockopt(Handle, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
      setsockopt(Handle, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

      // Drain any remaining data in the receive buffer
      char buffer[1024];
      int bytes_received;
      do {
         bytes_received = recv(Handle, buffer, sizeof(buffer), 0);
      } while (bytes_received > 0);

      close(Handle);
   }

#elif _WIN32

   #include <string.h>

   #define htons win_htons
   #define htonl win_htonl
   #define ntohs win_ntohs
   #define ntohl win_ntohl

   // Forward declarations for getaddrinfo functions (available in ws2_32.lib)
   extern "C" {
      int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
      void freeaddrinfo(struct addrinfo *res);
   }

#else
   #error "No support for this platform"
#endif

class extClientSocket : public objClientSocket {
   public:
   SOCKET_HANDLE Handle;
   struct NetQueue WriteQueue; // Writes to the network socket are queued here in a buffer
   struct NetQueue ReadQueue;  // Read queue, often used for reading whole messages
   uint8_t OutgoingRecursion;  // Recursion manager
   uint8_t InUse;       // Recursion manager
   bool ReadCalled;     // TRUE if the Read action has been called

   #ifndef DISABLE_SSL
      #ifdef _WIN32
         SSL_HANDLE SSLHandle;
      #else
         SSL *SSLHandle;     // SSL connection handle for this client
         BIO *BIOHandle;     // SSL BIO handle for this client
         uint8_t  SSLBusy; // Tracks the current actions of SSL handshaking.
      #endif
   #endif
};

class extNetSocket : public objNetSocket {
   public:
   SOCKET_HANDLE Handle;   // Handle of the socket
   FUNCTION Outgoing;
   FUNCTION Incoming;
   FUNCTION Feedback;
   objNetLookup *NetLookup;
   objNetClient *LastClient;      // For linked-list management for server sockets.  Points to the last client IP on the chain
   struct NetQueue WriteQueue;
   struct NetQueue ReadQueue;
   uint8_t ReadCalled:1;          // The Read() action sets this to TRUE whenever called.
   uint8_t IPV6:1;
   uint8_t Terminating:1;         // Set to TRUE when the NetSocket is marked for deletion.
   uint8_t ExternalSocket:1;      // Set to TRUE if the SocketHandle field was set manually by the client.
   uint8_t InUse;                 // Recursion counter to signal that the object is doing something.
   uint8_t IncomingRecursion;     // Used by netsocket_client to prevent recursive handling of incoming data.
   uint8_t OutgoingRecursion;
   #ifdef _WIN32
      int16_t WinRecursion; // For win32_netresponse()
   #endif
   #ifndef DISABLE_SSL
      // These handles are only used when the NetSocket is a client of a server.
      #ifdef _WIN32
         SSL_HANDLE SSLHandle;
      #else
        SSL *SSLHandle;
        uint8_t  SSLBusy; // Tracks the current actions of SSL handshaking.
        BIO *BIOHandle;
      #endif
   #endif
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

#ifndef DISABLE_SSL
  #ifdef _WIN32
    // Windows SSL wrapper forward declarations
    template <class T> ERR sslConnect(T *);
    template <class T> void sslDisconnect(T *);
    static ERR sslSetup(extNetSocket *);
  #else
    // OpenSSL forward declarations
    static bool ssl_init = false;
    static ERR sslConnect(extNetSocket *);
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

struct CaseInsensitiveHash {
   std::size_t operator()(const std::string& s) const noexcept {
      std::string lower = s;
      std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
      return std::hash<std::string>{}(lower);
   }
};

struct CaseInsensitiveEqual {
   bool operator()(const std::string& lhs, const std::string& rhs) const noexcept {
      return ::strcasecmp(lhs.c_str(), rhs.c_str()) == 0;
   }
};

typedef ankerl::unordered_dense::map<std::string, DNSEntry, CaseInsensitiveHash, CaseInsensitiveEqual> HOSTMAP;

//********************************************************************************************************************
// Performing the socket close in a separate thread means there'll be plenty of time for a
// graceful socket closure without affecting the current thread.

static void CLOSESOCKET_THREADED(SOCKET_HANDLE Handle)
{
#ifdef _WIN32
   win_deregister_socket(Handle);
#endif

   std::lock_guard<std::mutex> lock(glmThreads);
      auto thread_ptr = std::make_shared<std::jthread>();
      *thread_ptr = std::jthread([] (int Handle) {
         CLOSESOCKET(Handle);
      }, Handle);
      glThreads.insert(thread_ptr);
      thread_ptr->detach();
}

#ifndef DISABLE_SSL
  #ifdef _WIN32
    #include "win32/win32_ssl.cpp"
  #else
    #include "openssl.cpp"
  #endif
#endif

//********************************************************************************************************************

static OBJECTPTR clNetLookup = nullptr;
static OBJECTPTR clProxy = nullptr;
static OBJECTPTR clNetSocket = nullptr;
static OBJECTPTR clClientSocket = nullptr;
static OBJECTPTR clNetClient = nullptr;
static HOSTMAP glHosts;
static HOSTMAP glAddresses;
static MSGID glResolveNameMsgID = MSGID::NIL;
static MSGID glResolveAddrMsgID = MSGID::NIL;

static void netsocket_incoming(SOCKET_HANDLE, extNetSocket *);
static int8_t check_machine_name(CSTRING HostName) __attribute__((unused));
static ERR resolve_name_receiver(APTR Custom, MSGID MsgID, int MsgType, APTR Message, int MsgSize);
static ERR resolve_addr_receiver(APTR Custom, MSGID MsgID, int MsgType, APTR Message, int MsgSize);

static ERR init_netclient(void);
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

   if (init_netclient() != ERR::Okay) return ERR::AddClass;
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

#ifdef __linux__   
   struct rlimit fd_limit;
   if (getrlimit(RLIMIT_NOFILE, &fd_limit) == 0) {
      glSocketLimit = fd_limit.rlim_cur * 0.8; // Set a threshold at 80% of the system limit
   }
#endif

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

   if (clNetClient)    { FreeResource(clNetClient); clNetClient = nullptr; }
   if (clNetSocket)    { FreeResource(clNetSocket); clNetSocket = nullptr; }
   if (clClientSocket) { FreeResource(clClientSocket); clClientSocket = nullptr; }
   if (clProxy)        { FreeResource(clProxy); clProxy = nullptr; }
   if (clNetLookup)    { FreeResource(clNetLookup); clNetLookup = nullptr; }

#ifndef DISABLE_SSL
  #ifdef _WIN32
    ssl_wrapper_cleanup();
  #else
    if (ssl_init) {
       if (glClientSSL) { SSL_CTX_free(glClientSSL); glClientSSL = nullptr; }
       if (glServerSSL) { SSL_CTX_free(glServerSSL); glServerSSL = nullptr; }
       ERR_free_strings();
       EVP_cleanup();
       CRYPTO_cleanup_all_ex_data();
    }
  #endif
#endif

   {
      std::lock_guard<std::mutex> lock(glmThreads);

      for (auto &thread_ptr : glThreads) {
         if (thread_ptr and thread_ptr->joinable()) {
            thread_ptr->request_stop();
         }
      }

      // Give threads time to respond to stop request
      constexpr auto STOP_TIMEOUT = std::chrono::milliseconds(2000);
      auto start_time = std::chrono::steady_clock::now();

      while (!glThreads.empty() and (std::chrono::steady_clock::now() - start_time) < STOP_TIMEOUT) {
         // Remove completed threads
         std::erase_if(glThreads, [](const auto& thread_ptr) {
            return !thread_ptr || !thread_ptr->joinable();
         });

         if (!glThreads.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
         }
      }
      glThreads.clear();
   }

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

   if (Address->Type IS IPADDR::V6) {
      #ifdef __linux__
         char ipv6_str[INET6_ADDRSTRLEN];
         struct in6_addr addr;
         pf::copymem(Address->Data, &addr.s6_addr, 16);

         if (inet_ntop(AF_INET6, &addr, ipv6_str, INET6_ADDRSTRLEN)) {
            return pf::strclone(ipv6_str);
         }
      #elif _WIN32
         // Windows IPv6 string conversion using wrapper function
         char ipv6_str[46];
         const char *result = win_inet_ntop(AF_INET6, Address->Data, ipv6_str, sizeof(ipv6_str));
         if (result) return pf::strclone(result);
      #endif
      return nullptr;
   }
   else if (Address->Type IS IPADDR::V4) {
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
   else {
      log.warning("Unsupported address type: %d", int(Address->Type));
      return nullptr;
   }
}

/*********************************************************************************************************************

-FUNCTION-
StrToAddress: Converts an IP Address in string form to an !IPAddress structure.

Converts an IPv4 or an IPv6 address in string format to an !IPAddress structure.  The `String` must be of form
`1.2.3.4` (IPv4) or `2001:db8::1` (IPv6).  IPv6 addresses are automatically detected by the presence of colons.

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

   pf::clearmem(Address, sizeof(IPAddress));

   // Try IPv6 first (contains colons)
   if (strchr(Str, ':')) {
      #ifdef __linux__
         struct in6_addr ipv6_addr;
         if (inet_pton(AF_INET6, Str, &ipv6_addr) IS 1) {
            pf::copymem(&ipv6_addr.s6_addr, Address->Data, 16);
            Address->Type = IPADDR::V6;
            return ERR::Okay;
         }
      #elif _WIN32
         // Windows IPv6 parsing using wrapper function
         struct in6_addr ipv6_addr;
         if (win_inet_pton(AF_INET6, Str, &ipv6_addr) IS 1) {
            pf::copymem(&ipv6_addr.s6_addr, Address->Data, 16);
            Address->Type = IPADDR::V6;
            return ERR::Okay;
         }
         return ERR::Failed;
      #endif
   }

   // IPv4
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
#ifndef DISABLE_SSL
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

//********************************************************************************************************************

static ERR receive_from_server(extNetSocket *Self, SOCKET_HANDLE Socket, APTR Buffer, int BufferSize, int Flags, size_t *Result)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Socket: %d, BufSize: %d, Flags: $%.8x", Socket, BufferSize, Flags);
   
   *Result = 0;
   
   if (!BufferSize) return ERR::Okay;

#ifndef DISABLE_SSL
  #ifdef _WIN32
    if (Self->SSLHandle) {
       // If we're in the middle of SSL handshake, read raw data for handshake processing
       if (Self->State IS NTC::HANDSHAKING) {
          log.trace("Windows SSL handshake in progress, reading raw data.");
          ERR error = WIN_RECEIVE(Socket, Buffer, BufferSize, Flags, Result);
          if ((error IS ERR::Okay) and (*Result > 0)) {
             sslHandshakeReceived(Self, Buffer, *Result);
          }
          return error;
       }
       else { // Normal SSL data read for established connections
          if (auto result = ssl_wrapper_read(Self->SSLHandle, Buffer, BufferSize); result > 0) {
             *Result = result;
          }
          else if (!result) {
             return ERR::Disconnected;
          }
          else {
             CSTRING msg;
             auto error = ssl_wrapper_get_error(Self->SSLHandle, &msg);
             if (error IS SSL_ERROR_WOULD_BLOCK) {
                log.traceWarning("No more data to read from the SSL socket.");
             }
             else {
                log.warning("Windows SSL read error: %s", msg);
                return ERR::Failed;
             }
          }
       }
       return ERR::Okay;
    }
  #else // OpenSSL
    bool read_blocked;
    int pending;
    
    if (Self->SSLBusy IS SSL_HANDSHAKE_WRITE) ssl_handshake_write(Socket, Self);
    else if (Self->SSLBusy IS SSL_HANDSHAKE_READ) ssl_handshake_read(Socket, Self);

    if (Self->SSLBusy != SSL_NOT_BUSY) return ERR::Okay;

    if (Self->SSLHandle) {
      do {
         read_blocked = false;

         auto result = SSL_read(Self->SSLHandle, Buffer, BufferSize);

         if (result <= 0) {
            auto ssl_error = SSL_get_error(Self->SSLHandle, result);
            switch (ssl_error) {
               case SSL_ERROR_ZERO_RETURN:
                  return log.traceWarning(ERR::Disconnected);

               case SSL_ERROR_WANT_READ:
                  read_blocked = true;
                  break;

                case SSL_ERROR_WANT_WRITE:
                  // WANT_WRITE is returned if we're trying to rehandshake and the write operation would block.  We
                  // need to wait on the socket to be writeable, then restart the read when it is.

                   log.msg("SSL socket handshake requested by server.");
                   Self->SSLBusy = SSL_HANDSHAKE_WRITE;
                   RegisterFD((HOSTHANDLE)Socket, RFD::WRITE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(ssl_handshake_write<extNetSocket>), Self);
                   return ERR::Okay;

                case SSL_ERROR_SYSCALL:
                default:
                   log.warning("SSL read failed with error %d: %s", ssl_error, ERR_error_string(ssl_error, nullptr));
                   return ERR::Read;
            }
         }
         else {
            *Result += result;
            Buffer = (APTR)((char *)Buffer + result);
            BufferSize -= result;
         }
      } while ((pending = SSL_pending(Self->SSLHandle)) and (!read_blocked) and (BufferSize > 0));

      log.trace("Pending: %d, BufSize: %d, Blocked: %d", pending, BufferSize, read_blocked);

      if (pending) {
         // With regards to non-blocking SSL sockets, be aware that a socket can be empty in terms of incoming data,
         // yet SSL can keep data that has already arrived in an internal buffer.  This means that we can get stuck
         // select()ing on the socket because you aren't told that there is internal data waiting to be processed by
         // SSL_read().
         //
         // For this reason we set the RECALL flag so that we can be called again manually when we know that there is
         // data pending.

         RegisterFD((HOSTHANDLE)Socket, RFD::RECALL|RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&netsocket_incoming), Self);
      }

      return ERR::Okay;
    }
  #endif
#endif

#ifdef __linux__
   auto result = recv(Socket, Buffer, BufferSize, Flags);

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
      return ERR::SystemCall;
   }
#elif _WIN32
   return WIN_RECEIVE(Socket, Buffer, BufferSize, Flags, Result);
#else
   #error No support for RECEIVE()
#endif
}

//********************************************************************************************************************
// Template function to handle SSL and socket sending for both NetSocket and ClientSocket

template<typename T>
static ERR send_data(T *Self, CPTR Buffer, size_t *Length)
{
   pf::Log log(__FUNCTION__);

   if (!*Length) return ERR::Okay;
   
#ifndef DISABLE_SSL
   if (Self->SSLHandle) {
      #ifdef _WIN32
         log.traceBranch("SSL Length: %d", int(*Length));

         size_t bytes_sent;
         auto error = ssl_wrapper_write(Self->SSLHandle, Buffer, *Length, &bytes_sent);

         if (error IS SSL_OK) {
            if (*Length != bytes_sent) {
               log.traceWarning("Sent %d of %d bytes.", int(bytes_sent), int(*Length));
            }
            *Length = bytes_sent;
            return ERR::Okay;
         }
         else {
            CSTRING msg;
            ssl_wrapper_get_error(Self->SSLHandle, &msg);
            *Length = 0;
         
            if (error IS SSL_ERROR_WOULD_BLOCK) {
               return log.traceWarning(ERR::BufferOverflow);
            }
            else {
               log.warning("Windows SSL write error: %s", msg);
               return ERR::Write;
            }
         }
      #else
         log.traceBranch("SSL Length: %d", int(*Length));
      
         if (Self->SSLBusy IS SSL_HANDSHAKE_WRITE) ssl_handshake_write(Self->Handle, Self);
         else if (Self->SSLBusy IS SSL_HANDSHAKE_READ) ssl_handshake_read(Self->Handle, Self);
     
         if (Self->SSLBusy != SSL_NOT_BUSY) return ERR::Okay;
     
         auto bytes_sent = SSL_write(Self->SSLHandle, Buffer, *Length);

         if (bytes_sent < 0) {
            *Length = 0;
            auto ssl_error = SSL_get_error(Self->SSLHandle, bytes_sent);

            switch(ssl_error){
               case SSL_ERROR_WANT_WRITE:
                  log.traceWarning("Buffer overflow (SSL want write)");
                  return ERR::BufferOverflow;

               case SSL_ERROR_WANT_READ:
                  log.trace("Handshake requested by server.");
                  Self->SSLBusy = SSL_HANDSHAKE_READ;
                  RegisterFD((HOSTHANDLE)Self->Handle, RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(ssl_handshake_read<extNetSocket>), Self);
                  return ERR::Okay;

               case SSL_ERROR_SYSCALL:
                  log.warning("SSL_write() SysError %d: %s", errno, strerror(errno));
                  return ERR::Write;

               default:
                  while (ssl_error) {
                     log.warning("SSL_write() error %d, %s", ssl_error, ERR_error_string(ssl_error, nullptr));
                     ssl_error = ERR_get_error();
                  }
                  return ERR::Write;
            }
         }
         else {
            if (*Length != size_t(bytes_sent)) {
               log.trace("Sent %d of %d bytes.", int(bytes_sent), int(*Length));
            }
            *Length = bytes_sent;
         }
      #endif
      return ERR::Okay;
   }
#endif

   // Fallback to regular socket send
#ifdef __linux__
   *Length = send(Self->Handle, Buffer, *Length, 0);

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
   return WIN_SEND(Self->Handle, Buffer, Length, 0);
#else
   #error No support for send_data()
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
#include "netclient/netclient.cpp"

//********************************************************************************************************************

static STRUCTS glStructures = {
   { "DNSEntry",  sizeof(DNSEntry) },
   { "IPAddress", sizeof(IPAddress) },
   { "NetQueue",  sizeof(NetQueue) }
};

PARASOL_MOD(MODInit, nullptr, MODOpen, MODExpunge, MOD_IDL, &glStructures)
extern "C" struct ModHeader * register_network_module() { return &ModHeader; }

/*********************************************************************************************************************
                                                     BACKTRACE IT
*********************************************************************************************************************/
