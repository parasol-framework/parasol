
#ifndef WIN32
#define WIN32
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

struct IPAddress {
   union {
      char Data[16];  // Bytes 0-3 are IPv4 bytes.  In host byte order
      unsigned int Data32[4];
   };
   int Type;     // IPADDR_V4 or IPADDR_V6
};

#define IPADDR_V4 0
#define IPADDR_V6 1

#include <parasol/system/errors.h>

#include "winsockwrappers.h"

#include <unordered_map>
#include <cstdio>
#include <mutex>

using namespace std;

enum {
   NETMSG_START=0,
   NETMSG_END
};

enum {
   WM_NETWORK = WM_USER + 101, // WM_USER = 1024, 1125
   WM_RESOLVENAME // 1126
};

#define IS ==

#define MAX_SOCKETS 40

class socket_info { // Only the SocketHandle FD is unique.  NetSocket may be referenced multiple times for as many clients exist.
public:
   void *Reference = nullptr;      // Reference to a NetSocket, ClientSocket
   void *NetHost = nullptr;        // For win_async_resolvename() and WM_RESOLVENAME
   HANDLE ResolveHandle = INVALID_HANDLE_VALUE; // For win_async_resolvename() and WM_RESOLVENAME
   WSW_SOCKET SocketHandle = 0; // Winsock socket FD (same as the key)
   int Flags = 0;
};

static std::recursive_mutex csNetLookup;
static std::unordered_map<WSW_SOCKET, socket_info> glNetLookup;
static char glSocketsDisabled = FALSE;
static HWND glNetWindow = 0;
static char glNetClassInit = FALSE;
static char glWinsockInitialised = FALSE;

//********************************************************************************************************************

static const struct {
   int WinError;
   ERR PanError;
} glErrors[] = {
   { WSAEINTR,              ERR::Cancelled },
   { WSAEACCES,             ERR::PermissionDenied },
   { WSAEFAULT,             ERR::InvalidData },
   { WSAEINVAL,             ERR::Args },
   { WSAEMFILE,             ERR::OutOfSpace },
   { WSAEWOULDBLOCK,        ERR::InvalidState },
   { WSAEINPROGRESS,        ERR::Busy },
   { WSAEALREADY,           ERR::Busy },
   { WSAENOTSOCK,           ERR::Args },
   { WSAEDESTADDRREQ,       ERR::Args },
   { WSAEMSGSIZE,           ERR::DataSize },
   { WSAEPROTOTYPE,         ERR::Args },
   { WSAENOPROTOOPT,        ERR::Args },
   { WSAEPROTONOSUPPORT,    ERR::NoSupport },
   { WSAESOCKTNOSUPPORT,    ERR::NoSupport },
   { WSAEOPNOTSUPP,         ERR::NoSupport },
   { WSAEPFNOSUPPORT,       ERR::NoSupport },
   { WSAEAFNOSUPPORT,       ERR::NoSupport },
   { WSAEADDRINUSE,         ERR::InUse },
   { WSAEADDRNOTAVAIL,      ERR::HostUnreachable },
   { WSAENETDOWN,           ERR::NetworkUnreachable },
   { WSAENETUNREACH,        ERR::NetworkUnreachable },
   { WSAENETRESET,          ERR::Disconnected },
   { WSAECONNABORTED,       ERR::ConnectionAborted },
   { WSAECONNRESET,         ERR::Disconnected },
   { WSAENOBUFS,            ERR::BufferOverflow },
   { WSAEISCONN,            ERR::DoubleInit },
   { WSAENOTCONN,           ERR::Disconnected },
   { WSAESHUTDOWN,          ERR::Disconnected },
   { WSAETIMEDOUT,          ERR::TimeOut },
   { WSAECONNREFUSED,       ERR::ConnectionRefused },
   { WSAEHOSTDOWN,          ERR::HostUnreachable },
   { WSAEHOSTUNREACH,       ERR::HostUnreachable },
   { WSAHOST_NOT_FOUND,     ERR::HostNotFound },
   { WSASYSCALLFAILURE,     ERR::SystemCall },
   { 0, ERR::NIL }
};

//********************************************************************************************************************

static ERR convert_error(int error)
{
   if (!error) error = WSAGetLastError();

   int i;
   for (i=0; glErrors[i].WinError; i++) {
      if (glErrors[i].WinError IS error) return glErrors[i].PanError;
   }
   return ERR::Failed;
}

//********************************************************************************************************************

void winCloseResolveHandle(void *Handle)
{
   WSACancelAsyncRequest(Handle);
   CloseHandle(Handle);
}

//********************************************************************************************************************

struct hostent * win_gethostbyaddr(const struct IPAddress *Address)
{
   if (Address->Type IS IPADDR_V4) return gethostbyaddr((const char *)&Address->Data, 4, AF_INET);
   else return gethostbyaddr((const char *)&Address->Data, 16, AF_INET6);
}

//********************************************************************************************************************

static LRESULT CALLBACK win_messages(HWND window, UINT msgcode, WPARAM wParam, LPARAM lParam)
{
   int winerror = WSAGETSELECTERROR(lParam);
   int event = WSAGETSELECTEVENT(lParam);

   if (msgcode IS WM_NETWORK) {
      const lock_guard<recursive_mutex> lock(csNetLookup);
      if (glNetLookup.contains((WSW_SOCKET)wParam)) {
         int state;
         int resub = FALSE;
         switch (event) {
            case FD_READ:    state = NTE_READ; break;
            case FD_WRITE:   state = NTE_WRITE; resub = TRUE; break; // Keep the socket subscribed while writing
            case FD_ACCEPT:  state = NTE_ACCEPT; break;
            case FD_CLOSE:   state = NTE_CLOSE; break;
            case FD_CONNECT: state = NTE_CONNECT; break;
            default:         state = 0; break;
         }

         ERR error;
         if (winerror IS WSAEWOULDBLOCK) error = ERR::Okay;
         else if (winerror) error = convert_error(winerror);
         else error = ERR::Okay;

         socket_info *info = &glNetLookup[(WSW_SOCKET)wParam];
         if ((info->Flags & FD_READ) and (!glSocketsDisabled)) {
            WSAAsyncSelect(info->SocketHandle, glNetWindow, WM_NETWORK, info->Flags & (~FD_READ));
         }

         if (info->Reference) win32_netresponse((struct Object *)info->Reference, info->SocketHandle, state, error);
         else printf("win_messages() Missing reference for FD %d, state %d\n", info->SocketHandle, state);

         if ((resub) and (!glSocketsDisabled)) {
            WSAAsyncSelect(info->SocketHandle, glNetWindow, WM_NETWORK, info->Flags);
         }
         return 0;
      }
   }
   else return DefWindowProc(window, msgcode, wParam, lParam);

   return 0;
}

//********************************************************************************************************************
// This function is called by ProcessMesages() before and after windows messages are processed.  We tell windows to
// not produce any new network events during the message processing by turning off the flags for each socket.  This
// stops Windows from flooding our application with messages when downloading over a fast connection for example.
//
// The state of each socket is restored when we are called with NETMSG_END.

void win_net_processing(int Status, void *Args)
{
   if (Status IS NETMSG_START) {
      glSocketsDisabled++;
      if (glSocketsDisabled == 1) {
         const lock_guard<recursive_mutex> lock(csNetLookup);
         for (auto it=glNetLookup.begin(); it != glNetLookup.end(); it++) {
            WSAAsyncSelect(it->first, glNetWindow, 0, 0); // Turn off network messages
         }
      }
   }
   else if (Status IS NETMSG_END) {
      glSocketsDisabled--;
      if (glSocketsDisabled == 0) {
         const lock_guard<recursive_mutex> lock(csNetLookup);
         for (auto it=glNetLookup.begin(); it != glNetLookup.end(); it++) {
            WSAAsyncSelect(it->first, glNetWindow, WM_NETWORK, it->second.Flags); // Turn on network messages
         }
      }
   }
}

//********************************************************************************************************************
// Sets the read/write state for a socket.

void win_socketstate(WSW_SOCKET Socket, char Read, char Write)
{
   const lock_guard<recursive_mutex> lock(csNetLookup);
   socket_info *sock = &glNetLookup[Socket];

   if (Read IS 0) sock->Flags &= ~FD_READ;
   else if (Read IS 1) sock->Flags |= FD_READ;

   if (Write IS 0) sock->Flags &= ~FD_WRITE;
   else if (Write IS 1) sock->Flags |= FD_WRITE;

   if (!glSocketsDisabled) WSAAsyncSelect(Socket, glNetWindow, WM_NETWORK, sock->Flags);
}

//********************************************************************************************************************
// Initially when accepting a connection from a client, the ClientSocket object won't exist yet.  This is rectified later
// with a call to win_socket_reference()

WSW_SOCKET win_accept(void *NetSocket, WSW_SOCKET SocketHandle, struct sockaddr *Addr, int *AddrLen)
{
   auto client_handle = WSW_SOCKET(accept(SocketHandle, Addr, AddrLen));
   //printf("win_accept() FD %d, NetSocket %p\n", client_handle, NetSocket);

   // Enable TCP_NODELAY by default for better responsiveness
   DWORD nodelay = 1;
   setsockopt(client_handle, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));

   u_long non_blocking = 1;
   ioctlsocket(client_handle, FIONBIO, &non_blocking);

   int flags = FD_CLOSE|FD_ACCEPT|FD_CONNECT|FD_READ;
   if (!glSocketsDisabled) WSAAsyncSelect(client_handle, glNetWindow, WM_NETWORK, flags);

   glNetLookup[client_handle].Reference = NetSocket;
   glNetLookup[client_handle].SocketHandle = client_handle;
   glNetLookup[client_handle].Flags = flags;
   return client_handle;
}

//********************************************************************************************************************
// Replace the reference on a known socket handle

void win_socket_reference(WSW_SOCKET SocketHandle, void *Reference)
{
   glNetLookup[SocketHandle].Reference = Reference;
}

//********************************************************************************************************************

ERR win_bind(WSW_SOCKET SocketHandle, const struct sockaddr *Name, int NameLen)
{
   if (bind(SocketHandle, Name, NameLen) IS SOCKET_ERROR) return convert_error(0);
   else return ERR::Okay;
}

//********************************************************************************************************************
// Wrapped by CLOSESOCKET()

void win_closesocket(WSW_SOCKET SocketHandle)
{
   if (SocketHandle IS INVALID_SOCKET) return;

   {
      const lock_guard<recursive_mutex> lock(csNetLookup);
      glNetLookup.erase(SocketHandle);
   }

   // Perform graceful disconnect before closing

   shutdown(SocketHandle, SD_BOTH);

   // Set a short timeout to allow pending data to be transmitted
   struct timeval timeout;
   timeout.tv_sec = 0;
   timeout.tv_usec = 100000; // 100ms timeout
   setsockopt(SocketHandle, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
   setsockopt(SocketHandle, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

   // Drain any remaining data in the receive buffer
   char buffer[1024];
   int bytes_received;
   do {
      bytes_received = recv(SocketHandle, buffer, sizeof(buffer), 0);
   } while (bytes_received > 0);

   closesocket(SocketHandle);
}

//********************************************************************************************************************

ERR win_connect(WSW_SOCKET SocketHandle, const struct sockaddr *Name, int NameLen)
{
   if (connect(SocketHandle, Name, NameLen) IS SOCKET_ERROR) {
      if (WSAGetLastError() IS WSAEWOULDBLOCK) return ERR::Okay; // connect() will always 'fail' for non-blocking sockets (however it will continue to connect/succeed...!)
      return convert_error(0);
   }
   else return ERR::Okay;
}

//********************************************************************************************************************

struct hostent *win_gethostbyname(const char *Name)
{
   // Use WSAAsyncGetHostByName() if you want to do this asynchronously.

   return gethostbyname(Name);
}

//********************************************************************************************************************

int win_getpeername(WSW_SOCKET S, struct sockaddr *Name, int *NameLen)
{
   return getpeername(S, Name, NameLen);
}

//********************************************************************************************************************

int win_getsockname(WSW_SOCKET S, struct sockaddr *Name, int *NameLen)
{
   return getsockname(S, Name, NameLen);
}

//********************************************************************************************************************

unsigned long win_inet_addr(const char *Str)
{
   return inet_addr(Str);
}

//********************************************************************************************************************

char * win_inet_ntoa(unsigned long Addr) //struct in_addr);
{
   // TODO: Check this cast is alright
   struct in_addr addr;
   addr.s_addr = Addr;
   return inet_ntoa(addr);
}

//********************************************************************************************************************

ERR win_listen(WSW_SOCKET SocketHandle, int BackLog)
{
   if (listen(SocketHandle, BackLog) IS SOCKET_ERROR) return convert_error(0);
   else return ERR::Okay;
}

//********************************************************************************************************************

ERR WIN_RECEIVE(WSW_SOCKET SocketHandle, void *Buffer, size_t Len, int Flags, int *Result)
{
   *Result = 0;
   if (!Len) return ERR::Okay;
   auto result = recv(SocketHandle, reinterpret_cast<char *>(Buffer), Len, Flags);
   if (result > 0) {
      *Result = result;
      return ERR::Okay;
   }
   else if (result IS 0) {
      return ERR::Disconnected;
   }
   else if (WSAGetLastError() IS WSAEWOULDBLOCK) return ERR::Okay;
   else return convert_error(0);
}

//********************************************************************************************************************

ERR WIN_SEND(WSW_SOCKET Socket, const void *Buffer, size_t *Length, int Flags)
{
   if (!*Length) return ERR::Okay;
   *Length = send(Socket, reinterpret_cast<const char *>(Buffer), *Length, Flags);
   if (*Length >= 0) return ERR::Okay;
   else {
      *Length = 0;

      switch (WSAGetLastError()) {
         case WSAEWOULDBLOCK:
         case WSAEALREADY:
            return ERR::BufferOverflow;
         case WSAEINPROGRESS:
            return ERR::Busy;
         default:
            return convert_error(0);
      }
   }
}

//********************************************************************************************************************

int win_shutdown(WSW_SOCKET S, int How)
{
   return shutdown(S, How);
}

//********************************************************************************************************************

int win_WSAGetLastError()
{
   return WSAGetLastError();
}

int win_WSAENETUNREACH()
{
   return WSAGetLastError() IS WSAENETUNREACH;
}

int win_WSAECONNREFUSED()
{
   return WSAGetLastError() IS WSAECONNREFUSED;
}

int SocketWouldBlock()
{
   return WSAGetLastError() IS WSAEWOULDBLOCK;
}

//********************************************************************************************************************

int GetSockOptError(WSW_SOCKET S, char *Result, int *OptLen)
{
   return getsockopt(S, SOL_SOCKET, SO_ERROR, Result, OptLen);
}

//********************************************************************************************************************

unsigned long win_htonl(unsigned long X)
{
   return htonl(X);
}

unsigned long win_ntohl(unsigned long X)
{
   return ntohl(X);
}

unsigned short win_htons(unsigned short X)
{
   return htons(X);
}

unsigned short win_ntohs(unsigned short X)
{
   return ntohs(X);
}

//********************************************************************************************************************
// Note: The startup and shutdown functionality have been tested as working with multiple initialisations and
// module expunges.  Avoid tampering as the Windows functions are a bit sensitive.

const char * StartupWinsock() // Return zero if succesful
{
   if (!glNetClassInit) {
      WNDCLASSEX glNetClass;
      ZeroMemory(&glNetClass, sizeof(glNetClass));
      glNetClass.cbSize        = sizeof(glNetClass);
      glNetClass.style         = CS_DBLCLKS;
      glNetClass.lpfnWndProc   = win_messages;
      glNetClass.hInstance     = GetModuleHandle(nullptr);
      glNetClass.lpszClassName = "NetClass";
      if (!RegisterClassEx(&glNetClass)) {
         return "Failed to register window class for network messages.";
      }
      glNetClassInit = TRUE;
   }

   if (!glNetWindow) {
      // Create an invisible window that we will use to wake us up when network events occur (WSAAsyncSelect() insists on
      // there being a window).

      if (!(glNetWindow = CreateWindowEx(0,
         "NetClass", "NetworkWindow",
         0,
         0, 0,
         CW_USEDEFAULT, CW_USEDEFAULT,
         (HWND)nullptr,
         (HMENU)nullptr,
         GetModuleHandle(nullptr), nullptr))) return "Failed to create Window for receiving network messages.";
   }

   if (!glWinsockInitialised) {
      WSADATA wsadata;
      unsigned short version_requested = MAKEWORD(1, 1);
      if (auto code = WSAStartup(version_requested, &wsadata)) {
         switch (code) {
            case WSASYSNOTREADY: return "WSASYSNOTREADY";
            case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
            case WSAEINPROGRESS: return "WSAEINPROGRESS";
            case WSAEPROCLIM: return "WSAEPROCLIM";
            case WSAEFAULT: return "WSAEFAULT";
            default: return "Reason not given.";
         }
      }
      glWinsockInitialised = TRUE;
   }

   return nullptr;
}

//********************************************************************************************************************

int ShutdownWinsock()
{
   if (glNetWindow) { DestroyWindow(glNetWindow); glNetWindow = FALSE; }
   if (glNetClassInit) { UnregisterClass("NetClass", GetModuleHandle(nullptr)); glNetClassInit = FALSE; }
   if (glWinsockInitialised) { WSACleanup(); glWinsockInitialised = FALSE; }
   return 0;
}

//********************************************************************************************************************
// IPv6 wrapper functions for Windows with fall-back to IPv4 if IPv6 is not supported or fails.

WSW_SOCKET win_socket_ipv6(void *NetSocket, char Read, char Write, bool &IPV6)
{
   IPV6 = false;
   // Try to create IPv6 dual-stack socket first
   auto handle = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
   if (handle != INVALID_SOCKET) {
      // Set dual-stack mode (disable IPv6-only mode to accept IPv4 connections)
      DWORD v6only = 0;
      if (setsockopt(handle, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&v6only, sizeof(v6only)) != 0) {
         // Log warning but continue - some systems may not support dual-stack
      }

      // Enable TCP_NODELAY by default for better responsiveness
      DWORD nodelay = 1;
      setsockopt(handle, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));

      u_long non_blocking = 1;
      ioctlsocket(handle, FIONBIO, &non_blocking);

      int flags = FD_CLOSE|FD_ACCEPT|FD_CONNECT;
      if (Read) flags |= FD_READ;
      if (Write) flags |= FD_WRITE;
      if (!glSocketsDisabled) WSAAsyncSelect(handle, glNetWindow, WM_NETWORK, flags);

      auto sock = WSW_SOCKET(handle);
      glNetLookup[sock].Reference = NetSocket;
      glNetLookup[sock].SocketHandle = sock;
      glNetLookup[sock].Flags = flags;
      IPV6 = true;
      return sock;
   }
   else { // Fall back to IPv4-only socket
      if (auto handle = socket(PF_INET, SOCK_STREAM, 0); handle != INVALID_SOCKET) {
         // Enable TCP_NODELAY by default for better responsiveness
         DWORD nodelay = 1;
         setsockopt(handle, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));

         u_long non_blocking = 1;
         ioctlsocket(handle, FIONBIO, &non_blocking);
         int flags = FD_CLOSE|FD_ACCEPT|FD_CONNECT;
         if (Read) flags |= FD_READ;
         if (Write) flags |= FD_WRITE;
         if (!glSocketsDisabled) WSAAsyncSelect(handle, glNetWindow, WM_NETWORK, flags);

         auto sock = WSW_SOCKET(handle);
         glNetLookup[sock].Reference = NetSocket;
         glNetLookup[sock].SocketHandle = sock;
         glNetLookup[sock].Flags = flags;
         return sock;
      }
      else return (WSW_SOCKET)INVALID_SOCKET;
   }
}

//********************************************************************************************************************
// Use Windows Winsock2 inet_pton if available (Windows Vista+)
// For older systems, provide basic IPv6 parsing

int win_inet_pton(int af, const char *src, void *dst)
{
   if (af IS AF_INET6) {
      // Basic IPv6 parsing for common formats
      if (strcmp(src, "::1") IS 0) { // IPv6 loopback
         memset(dst, 0, 16);
         ((unsigned char*)dst)[15] = 1;
         return 1;
      }
      else if (strcmp(src, "::") IS 0) { // IPv6 any address
         memset(dst, 0, 16);
         return 1;
      }
      else if (strncmp(src, "::ffff:", 7) IS 0) { // IPv4-mapped IPv6 address
         memset(dst, 0, 10);
         ((unsigned char*)dst)[10] = 0xff;
         ((unsigned char*)dst)[11] = 0xff;

         // Parse the IPv4 part
         unsigned long ipv4 = inet_addr(src + 7);
         if (ipv4 != INADDR_NONE) {
            memcpy(((unsigned char*)dst) + 12, &ipv4, 4);
            return 1;
         }
      }
      else { // Try to use Windows inet_pton if available
         static auto inet_pton_func = (int (WINAPI*)(int, const char*, void*))GetProcAddress(GetModuleHandle("ws2_32.dll"), "inet_pton");
         if (inet_pton_func) return inet_pton_func(af, src, dst);
         return 0;
      }
   }
   else if (af IS AF_INET) { // Use standard IPv4 parsing
      unsigned long result = inet_addr(src);
      if (result != INADDR_NONE) {
         memcpy(dst, &result, 4);
         return 1;
      }
   }

   return 0;
}

//********************************************************************************************************************

const char *win_inet_ntop(int af, const void *src, char *dst, size_t size)
{
   if (af IS AF_INET6) {
      if (size < 46) return nullptr; // Need at least 46 bytes for full IPv6 address

      const unsigned char *bytes = (const unsigned char*)src;

      // Check for special addresses
      static unsigned char loopback[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
      static unsigned char any[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

      if (memcmp(src, loopback, 16) IS 0) {
         strcpy(dst, "::1");
         return dst;
      }
      else if (memcmp(src, any, 16) IS 0) {
         strcpy(dst, "::");
         return dst;
      }
      else if (bytes[10] IS 0xff and bytes[11] IS 0xff) {
         // IPv4-mapped IPv6
         struct in_addr ipv4_addr;
         memcpy(&ipv4_addr, bytes + 12, 4);
         _snprintf(dst, size, "::ffff:%s", inet_ntoa(ipv4_addr));
         return dst;
      }
      else {
         // Try to use Windows inet_ntop if available
         static auto inet_ntop_func = (const char* (WINAPI*)(int, const void*, char*, size_t))
            GetProcAddress(GetModuleHandle("ws2_32.dll"), "inet_ntop");
         if (inet_ntop_func) {
            return inet_ntop_func(af, src, dst, size);
         }

         // Fallback: format as full hex representation
         _snprintf(dst, size,
            "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
            bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
            bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
         return dst;
      }
   }
   else if (af IS AF_INET) {
      if (size < 16) return nullptr; // Need at least 16 bytes for IPv4
      struct in_addr addr;
      memcpy(&addr, src, 4);
      const char *result = inet_ntoa(addr);
      if (result and strlen(result) < size) {
         strcpy(dst, result);
         return dst;
      }
   }

   return nullptr; // Error
}

//********************************************************************************************************************

WSW_SOCKET win_accept_ipv6(void *NetSocket, WSW_SOCKET ServerSocket, struct sockaddr *addr, int *addrlen, int *family)
{
   // Use sockaddr_storage for dual-stack accept
   struct sockaddr_storage storage;
   int storage_len = sizeof(storage);

   SOCKET client_fd = accept(ServerSocket, (struct sockaddr*)&storage, &storage_len);
   if (client_fd IS INVALID_SOCKET) return (WSW_SOCKET)INVALID_SOCKET;

   if (addr and addrlen) {
      int copy_len = (*addrlen < storage_len) ? *addrlen : storage_len;
      memcpy(addr, &storage, copy_len);
      *addrlen = copy_len;
   }

   if (family) *family = storage.ss_family;

   DWORD nodelay = 1;
   setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));

   u_long non_blocking = 1;
   ioctlsocket(client_fd, FIONBIO, &non_blocking);

   // Register the client socket for event handling
   int flags = FD_CLOSE | FD_READ | FD_WRITE;
   if (!glSocketsDisabled) WSAAsyncSelect(client_fd, glNetWindow, WM_NETWORK, flags);

   auto sock = WSW_SOCKET(client_fd);
   glNetLookup[sock].Reference = NetSocket;
   glNetLookup[sock].SocketHandle = sock;
   glNetLookup[sock].Flags = flags;

   return sock;
}
