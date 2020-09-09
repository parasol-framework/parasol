
#ifndef WIN32
#define WIN32
#endif

#ifndef USE_ARES
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif

#ifdef USE_ARES
#include "config-win32.h"
#endif

struct IPAddress {
   union {
      char Data[16];  // Bytes 0-3 are IPv4 bytes.  In host byte order
      unsigned int Data32[4];
   };
   int Type;     // IPADDR_V4 or IPADDR_V6
};

#define IPADDR_V4 0
#define IPADDR_V6 1

#include "winsockwrappers.h"

#ifdef USE_ARES
#include "ares.h"
#include "ares_private.h"

extern ares_channel glAres;
extern void ares_response(void *, int Status, int Timeouts, struct hostent *);
#endif

#include <parasol/system/errors.h>

enum {
   NETMSG_START=0,
   NETMSG_END
};

enum {
   WM_NETWORK = WM_USER + 101, // WM_USER = 1024, 1125
   WM_RESOLVENAME, // 1126
   WM_NETWORK_ARES // 1127
};

#define IS ==
#define OR ||
#define AND &&

#define MAX_SOCKETS 40

struct socket_info {
   void *NetSocket;   // Reference to the NetSocket object.
   void *NetHost;     // For win_async_resolvename() and WM_RESOLVENAME
   WSW_SOCKET WinSocket; // Winsock socket FD
   HANDLE ResolveHandle; // For win_async_resolvename() and WM_RESOLVENAME
   int Flags;
   short Index;
};

static CRITICAL_SECTION csNetLookup;
static struct socket_info glNetLookup[MAX_SOCKETS];
static short glLastSocket = 0;

static char glSocketsDisabled = FALSE;

static HWND glNetWindow = 0;
static char glNetClassInit = FALSE;
static char glWinsockInitialised = FALSE;

// Lookup the entry for a NetSocket object (no creation if does not exist).

static struct socket_info * lookup_socket(void *NetSocket)
{
   EnterCriticalSection(&csNetLookup);

   LONG i;
   for (i=0; i < glLastSocket; i++) {
      if (NetSocket IS glNetLookup[i].NetSocket) {
         struct socket_info *info = &glNetLookup[i];
         LeaveCriticalSection(&csNetLookup);
         return info;
      }
   }

   LeaveCriticalSection(&csNetLookup);
   return NULL;
}

static struct socket_info * lookup_socket_handle(WSW_SOCKET SocketHandle)
{
   EnterCriticalSection(&csNetLookup);

   LONG i;
   for (i=0; i < glLastSocket; i++) {
      if (SocketHandle IS glNetLookup[i].WinSocket) {
         struct socket_info *info = &glNetLookup[i];
         LeaveCriticalSection(&csNetLookup);
         return info;
      }
   }

   LeaveCriticalSection(&csNetLookup);
   return NULL;
}

// Lookup for the entry for a NetSocket object, creating it if it does not exist.

static struct socket_info * get_socket(void *NetSocket)
{
   EnterCriticalSection(&csNetLookup);

   LONG i;
   for (i=0; i < glLastSocket; i++) { // Return socket entry if it already exists.
      if (NetSocket IS glNetLookup[i].NetSocket) {
         struct socket_info *info = &glNetLookup[i];
         LeaveCriticalSection(&csNetLookup);
         return info;
      }
   }

   for (i=0; i < glLastSocket; i++) { // Find an empty existing entry.
      if (!glNetLookup[i].NetSocket) {
         struct socket_info *info = &glNetLookup[i];
         info->NetSocket = NetSocket;
         info->Index = i;
         LeaveCriticalSection(&csNetLookup);
         return info;
      }
   }

   if (glLastSocket < MAX_SOCKETS) { // Create a new entry at the end of the array.
      i = glLastSocket++;
      struct socket_info *info = &glNetLookup[i];
      info->NetSocket = NetSocket;
      info->Index = i;
      LeaveCriticalSection(&csNetLookup);
      return info;
   }

   LeaveCriticalSection(&csNetLookup);
   return NULL;
}

//****************************************************************************

static const struct {
   int WinError;
   int PanError;
} glErrors[] = {
   { WSAEINTR,              ERR_Cancelled },
   { WSAEACCES,             ERR_PermissionDenied },
   { WSAEFAULT,             ERR_InvalidData },
   { WSAEINVAL,             ERR_Args },
   { WSAEMFILE,             ERR_OutOfSpace },
   { WSAEWOULDBLOCK,        ERR_BadState },
   { WSAEINPROGRESS,        ERR_Busy },
   { WSAEALREADY,           ERR_Busy },
   { WSAENOTSOCK,           ERR_Args },
   { WSAEDESTADDRREQ,       ERR_Args },
   { WSAEMSGSIZE,           ERR_DataSize },
   { WSAEPROTOTYPE,         ERR_Args },
   { WSAENOPROTOOPT,        ERR_Args },
   { WSAEPROTONOSUPPORT,    ERR_NoSupport },
   { WSAESOCKTNOSUPPORT,    ERR_NoSupport },
   { WSAEOPNOTSUPP,         ERR_NoSupport },
   { WSAEPFNOSUPPORT,       ERR_NoSupport },
   { WSAEAFNOSUPPORT,       ERR_NoSupport },
   { WSAEADDRINUSE,         ERR_InUse },
   { WSAENETDOWN,           ERR_NetworkUnreachable },
   { WSAENETUNREACH,        ERR_NetworkUnreachable },
   { WSAENETRESET,          ERR_Disconnected },
   { WSAECONNABORTED,       ERR_ConnectionAborted },
   { WSAECONNRESET,         ERR_Disconnected },
   { WSAENOBUFS,            ERR_BufferOverflow },
   { WSAEISCONN,            ERR_DoubleInit },
   { WSAENOTCONN,           ERR_Disconnected },
   { WSAESHUTDOWN,          ERR_Disconnected },
   { WSAETIMEDOUT,          ERR_TimeOut },
   { WSAECONNREFUSED,       ERR_ConnectionRefused },
   { WSAEHOSTDOWN,          ERR_HostUnreachable },
   { WSAEHOSTUNREACH,       ERR_HostUnreachable },
   { WSAHOST_NOT_FOUND,     ERR_HostNotFound },
   { WSASYSCALLFAILURE,     ERR_SystemCall },
   { 0, 0 }
};

extern void win_dns_callback(void *Resolver, int Error, struct hostent *);

//****************************************************************************

static int convert_error(int error)
{
   if (!error) error = WSAGetLastError();

   int i;
   for (i=0; glErrors[i].WinError; i++) {
      if (glErrors[i].WinError IS error) return glErrors[i].PanError;
   }
   return ERR_Failed;
}

//****************************************************************************

void winCloseResolveHandle(void *Handle)
{
   WSACancelAsyncRequest(Handle);
   CloseHandle(Handle);
}

//****************************************************************************

struct hostent * win_gethostbyaddr(struct IPAddress *Address)
{
   if (Address->Type IS IPADDR_V4) return gethostbyaddr((const char *)&Address->Data, 4, AF_INET);
   else return gethostbyaddr((const char *)&Address->Data, 16, AF_INET6);
}

//****************************************************************************

static LRESULT CALLBACK win_messages(HWND window, UINT msgcode, WPARAM wParam, LPARAM lParam)
{
   int error = WSAGETSELECTERROR(lParam);
   int event = WSAGETSELECTEVENT(lParam);

   if (msgcode IS WM_NETWORK) {
      int resub = FALSE;
      int state;
      switch (event) {
         case FD_READ:    state = NTE_READ; break;
         case FD_WRITE:   state = NTE_WRITE; resub = TRUE; break;
         case FD_ACCEPT:  state = NTE_ACCEPT; break;
         case FD_CLOSE:   state = NTE_CLOSE; break;
         case FD_CONNECT: state = NTE_CONNECT; break;
         default:         state = 0; break;
      }

      if (error IS WSAEWOULDBLOCK) error = ERR_Okay;
      else if (error) error = convert_error(error);

      struct socket_info * info = lookup_socket_handle((WSW_SOCKET)wParam);
      if (info) {
         if ((info->Flags & FD_READ) AND (!glSocketsDisabled)) {
            WSAAsyncSelect(info->WinSocket, glNetWindow, WM_NETWORK, info->Flags & (~FD_READ));
            //resub = TRUE;
         }

         win32_netresponse(info->NetSocket, info->WinSocket, state, error);

         if ((resub) AND (!glSocketsDisabled)) {
            WSAAsyncSelect(info->WinSocket, glNetWindow, WM_NETWORK, info->Flags);
         }
         return 0;
      }
   }
   else if (msgcode IS WM_RESOLVENAME) { // Managed by win_async_resolvename() for non-Ares DNS lookups
      int i;
      EnterCriticalSection(&csNetLookup);
      for (i=0; i < glLastSocket; i++) {
         if (glNetLookup[i].ResolveHandle IS (HANDLE)wParam) {
            // Note that there is no requirement to close the handle according to WSAAsyncGetHostByName() documentation.
            glNetLookup[i].ResolveHandle = INVALID_HANDLE_VALUE;

            win_dns_callback(glNetLookup[i].NetSocket, ERR_Okay, glNetLookup[i].NetHost);
            LeaveCriticalSection(&csNetLookup);
            return 0;
         }
      }
      LeaveCriticalSection(&csNetLookup);
   }
   #ifdef USE_ARES
   else if (msgcode IS WM_NETWORK_ARES) {
      // wParam will identify the Ares socket handle.

      fd_set readers, writers;
      FD_ZERO(&readers);
      FD_ZERO(&writers);

      if (event IS FD_READ) {
         FD_SET(wParam, &readers);
         ares_process(glAres, &readers, &writers);
      }
      else if (event IS FD_WRITE) {
         FD_SET(wParam, &writers);
         ares_process(glAres, &readers, &writers);
      }
   }
   #endif
   else return DefWindowProc(window, msgcode, wParam, lParam);

   return 0;
}

//****************************************************************************
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
         EnterCriticalSection(&csNetLookup);
            int i;
            for (i=0; i < glLastSocket; i++) {
               if (glNetLookup[i].WinSocket) {
                  WSAAsyncSelect(glNetLookup[i].WinSocket, glNetWindow, 0, 0); // Turn off network messages
               }
            }
         LeaveCriticalSection(&csNetLookup);
      }
   }
   else if (Status IS NETMSG_END) {
      glSocketsDisabled--;
      if (glSocketsDisabled == 0) {
         EnterCriticalSection(&csNetLookup);
            int i;
            for (i=0; i < glLastSocket; i++) {
               if (glNetLookup[i].WinSocket) {
                  WSAAsyncSelect(glNetLookup[i].WinSocket, glNetWindow, WM_NETWORK, glNetLookup[i].Flags); // Turn on network messages
               }
            }
         LeaveCriticalSection(&csNetLookup);
      }
   }
}

//****************************************************************************
// Sets the read/write state for a socket.

void win_socketstate(WSW_SOCKET Socket, char Read, char Write)
{
   struct socket_info *info = lookup_socket_handle(Socket);
   if (info) {
      if (Read IS 0) info->Flags &= ~FD_READ;
      else if (Read IS 1) info->Flags |= FD_READ;

      if (Write IS 0) info->Flags &= ~FD_WRITE;
      else if (Write IS 1) info->Flags |= FD_WRITE;
      //printf("Socket state: %d, Read: %d, Write: %d, State: $%.8x\n", Socket, Read, Write, info->Flags);

      if (!glSocketsDisabled) WSAAsyncSelect(info->WinSocket, glNetWindow, WM_NETWORK, info->Flags);
   }
   else printf("win_socketstate() failed to find socket %d\n", Socket);
}

//****************************************************************************

WSW_SOCKET win_accept(void *NetSocket, WSW_SOCKET S, struct sockaddr *Addr, int *AddrLen)
{
   struct socket_info *info = get_socket(NetSocket);
   if (!info) return (WSW_SOCKET)INVALID_SOCKET;

   WSW_SOCKET client = accept(S, Addr, AddrLen);

   ULONG non_blocking = 1;
   ioctlsocket(client, FIONBIO, &non_blocking);

   int flags = FD_CLOSE|FD_ACCEPT|FD_CONNECT|FD_READ;
   if (!glSocketsDisabled) WSAAsyncSelect(client, glNetWindow, WM_NETWORK, flags);

   info->WinSocket = client;
   info->Flags  = flags;
   return client;
}

//****************************************************************************

int win_bind(WSW_SOCKET SocketHandle, const struct sockaddr *Name, int NameLen)
{
   if (bind(SocketHandle, Name, NameLen) IS SOCKET_ERROR) return convert_error(0);
   else return ERR_Okay;
}

//****************************************************************************

int win_closesocket(WSW_SOCKET SocketHandle)
{
   EnterCriticalSection(&csNetLookup);

      int i;
      for (i=0; i < glLastSocket; i++) { // Remove this socket's registration if it exists.
         if (SocketHandle IS glNetLookup[i].WinSocket) {
            glNetLookup[i].WinSocket = 0;
            glNetLookup[i].NetSocket = 0;
            break;
         }
      }

      while ((glLastSocket > 0) AND (!glNetLookup[glLastSocket-1].NetSocket)) glLastSocket--;

   LeaveCriticalSection(&csNetLookup);

   return closesocket(SocketHandle);
}

//****************************************************************************

int win_connect(WSW_SOCKET SocketHandle, const struct sockaddr *Name, int NameLen)
{
   if (connect(SocketHandle, Name, NameLen) IS SOCKET_ERROR) {
      if (WSAGetLastError() IS WSAEWOULDBLOCK) return ERR_Okay; // connect() will always 'fail' for non-blocking sockets (however it will continue to connect/succeed...!)
      return convert_error(0);
   }
   else return ERR_Okay;
}

//****************************************************************************

struct hostent *win_gethostbyname(const char *Name)
{
   // Use WSAAsyncGetHostByName() if you want to do this asynchronously.

   return gethostbyname(Name);
}

//****************************************************************************

int win_getpeername(WSW_SOCKET S, struct sockaddr *Name, int *NameLen)
{
   return getpeername(S, Name, NameLen);
}

//****************************************************************************

int win_getsockname(WSW_SOCKET S, struct sockaddr *Name, int *NameLen)
{
   return getsockname(S, Name, NameLen);
}

//****************************************************************************

unsigned long win_inet_addr(const char *Str)
{
   return inet_addr(Str);
}

//****************************************************************************

char * win_inet_ntoa(unsigned long Addr) //struct in_addr);
{
   // TODO: Check this cast is alright
   struct in_addr addr;
   addr.s_addr = Addr;
   return inet_ntoa(addr);
}

//****************************************************************************

int win_listen(WSW_SOCKET SocketHandle, int BackLog)
{
   if (listen(SocketHandle, BackLog) IS SOCKET_ERROR) return convert_error(0);
   else return ERR_Okay;
}

//****************************************************************************

int WIN_RECEIVE(WSW_SOCKET SocketHandle, void *Buffer, int Len, int Flags, int *Result)
{
   *Result = 0;
   if (!Len) return ERR_Okay;
   LONG result = recv(SocketHandle, Buffer, Len, Flags);
   if (result > 0) {
      *Result = result;
      return ERR_Okay;
   }
   else if (result IS 0) {
      return ERR_Disconnected;
   }
   else if (WSAGetLastError() IS WSAEWOULDBLOCK) return ERR_Okay;
   else return convert_error(0);
}

//****************************************************************************

int WIN_SEND(WSW_SOCKET Socket, const void *Buffer, int *Length, int Flags)
{
   if (!*Length) return ERR_Okay;
   *Length = send(Socket, Buffer, *Length, Flags);
   if (*Length >= 0) return ERR_Okay;
   else {
      *Length = 0;

      switch (WSAGetLastError()) {
         case WSAEWOULDBLOCK:
         case WSAEALREADY:
            return ERR_BufferOverflow;
         case WSAEINPROGRESS:
            return ERR_Busy;
         default:
            return convert_error(0);
      }
   }
}

//****************************************************************************

int win_shutdown(WSW_SOCKET S, int How)
{
   return shutdown(S, How);
}

//****************************************************************************
// Creates and configures new winsock sockets.

WSW_SOCKET win_socket(void *NetSocket, char Read, char Write)
{
   struct socket_info *info = get_socket(NetSocket);
   if (!info) return (WSW_SOCKET)INVALID_SOCKET;

   // Create the socket, make it non-blocking and configure it to wake our task when activity occurs on the socket.

   int handle;
   if ((handle = socket(PF_INET, SOCK_STREAM, 0)) != INVALID_SOCKET) {
      __ms_u_long non_blocking = 1;
      ioctlsocket(handle, FIONBIO, &non_blocking);
      int flags = FD_CLOSE|FD_ACCEPT|FD_CONNECT;
      if (Read) flags |= FD_READ;
      if (Write) flags |= FD_WRITE;
      if (!glSocketsDisabled) WSAAsyncSelect(handle, glNetWindow, WM_NETWORK, flags);
      info->WinSocket = handle;
      info->Flags     = flags;
      return handle;
   }
   else return (WSW_SOCKET)INVALID_SOCKET;
}

//****************************************************************************

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

//****************************************************************************

int GetSockOptError(WSW_SOCKET S, char *Result, int *OptLen)
{
   return getsockopt(S, SOL_SOCKET, SO_ERROR, Result, OptLen);
}

//****************************************************************************

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

//****************************************************************************
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
      glNetClass.hInstance     = GetModuleHandle(NULL);
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
         (HWND)NULL,
         (HMENU)NULL,
         GetModuleHandle(NULL), NULL))) return "Failed to create Window for receiving network messages.";
   }

   if (!glWinsockInitialised) {
      WSADATA wsadata;
      unsigned short version_requested = MAKEWORD(1, 1);
      int code;
      if ((code = WSAStartup(version_requested, &wsadata)) != 0) {
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

   InitializeCriticalSection(&csNetLookup);

   return NULL;
}

//****************************************************************************

int ShutdownWinsock()
{
   if (glNetWindow) { DestroyWindow(glNetWindow); glNetWindow = FALSE; }
   if (glNetClassInit) { UnregisterClass("NetClass", GetModuleHandle(NULL)); glNetClassInit = FALSE; }
   if (glWinsockInitialised) { WSACleanup(); glWinsockInitialised = FALSE; }
   DeleteCriticalSection(&csNetLookup);
   return 0;
}

//****************************************************************************
// Use this function for resolving Windows machine names, e.g. \\MACHINE.  The name is something of a misnomer, by
// 'asynchronous' what Microsoft means is that multiple calls to WSAAsync() will execute in sequence as opposed to all
// running at the same time.  However if you only make the one name resolution, the effect is that of executing
// synchronously in the background.

int win_async_resolvename(const unsigned char *Name, void *Resolver, struct hostent *Host, int HostSize)
{
   struct socket_info *info = lookup_socket(Resolver);
   if (info) {
      // Initiate host search and save the handle against this NetSocket object.
      if ((info->ResolveHandle = WSAAsyncGetHostByName(glNetWindow, WM_RESOLVENAME, Name, (char *)Host, HostSize)) != INVALID_HANDLE_VALUE) {
         info->NetHost = Host;
         return ERR_Okay;
      }
      else {
         win_dns_callback(Resolver, ERR_Failed, NULL);
         return ERR_Failed;
      }
   }

   win_dns_callback(Resolver, ERR_SystemCorrupt, NULL);
   return ERR_SystemCorrupt;
}

//****************************************************************************

#ifdef ARES__H

// Refer to the code for ares_fds() to see where this loop came from.

int win_ares_handler(ares_channel Ares)
{
   int active_queries = !ares__is_list_empty(&(Ares->all_queries));

   int i;
   for (i=0; i < Ares->nservers; i++) {
      struct server_state *server = &Ares->servers[i];

      #ifdef DEBUG
         printf("ares_handler() Active: %d, UDP: %d, TCP: %d\n", active_queries, server->udp_socket, server->tcp_socket);
      #endif

      if ((active_queries) AND (server->udp_socket != ARES_SOCKET_BAD)) {
         if (WSAAsyncSelect(server->udp_socket, glNetWindow, WM_NETWORK_ARES, FD_CLOSE|FD_ACCEPT|FD_READ)) {
            return ERR_SystemCall;
         }
      }

      if (server->tcp_socket != ARES_SOCKET_BAD) {
         if (server->qhead) { // Write and read
            if (WSAAsyncSelect(server->tcp_socket, glNetWindow, WM_NETWORK_ARES, FD_CLOSE|FD_ACCEPT|FD_READ|FD_WRITE)) {
               return ERR_SystemCall;
            }
         }
         else { // Read only
            if (WSAAsyncSelect(server->tcp_socket, glNetWindow, WM_NETWORK_ARES, FD_CLOSE|FD_ACCEPT|FD_READ)) {
               return ERR_SystemCall;
            }
         }
      }
   }

   return ERR_Okay;
}

// Initiate a background host query by name.  Ares will call ares_response when it has finished.

int win_ares_resolvename(const unsigned char *Name, ares_channel Ares, void *Resolver)
{
   ares_gethostbyname(Ares, Name, AF_INET, &ares_response, Resolver);

   return win_ares_handler(Ares);
}

// Initiate a background host query by IP address.  Ares will call ares_response when it has finished.

int win_ares_resolveaddr(struct IPAddress *Address, ares_channel Ares, void *Resolver)
{
   if (Address->Type IS IPADDR_V4) {
      ares_gethostbyaddr(Ares, &Address->Data, 4, AF_INET, &ares_response, Resolver);
   }
   else ares_gethostbyaddr(Ares, &Address->Data, 16, AF_INET6, &ares_response, Resolver);

   return win_ares_handler(Ares);
}

void win_ares_deselect(int Handle)
{
   WSAAsyncSelect(Handle, glNetWindow, 0, 0); // Listen to nothing == cancellation.
}

#endif
