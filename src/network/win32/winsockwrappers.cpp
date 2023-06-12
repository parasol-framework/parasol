
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

#include "winsockwrappers.h"

#include <unordered_map>
#include <cstdio>
#include <mutex>

using namespace std;

#include <parasol/system/errors.h>

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
   void *Reference = NULL;      // Reference to a NetSocket, ClientSocket
   void *NetHost = NULL;        // For win_async_resolvename() and WM_RESOLVENAME
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
   int PanError;
} glErrors[] = {
   { WSAEINTR,              ERR_Cancelled },
   { WSAEACCES,             ERR_PermissionDenied },
   { WSAEFAULT,             ERR_InvalidData },
   { WSAEINVAL,             ERR_Args },
   { WSAEMFILE,             ERR_OutOfSpace },
   { WSAEWOULDBLOCK,        ERR_InvalidState },
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

//********************************************************************************************************************

static int convert_error(int error)
{
   if (!error) error = WSAGetLastError();

   int i;
   for (i=0; glErrors[i].WinError; i++) {
      if (glErrors[i].WinError IS error) return glErrors[i].PanError;
   }
   return ERR_Failed;
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
   int error = WSAGETSELECTERROR(lParam);
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

         if (error IS WSAEWOULDBLOCK) error = ERR_Okay;
         else if (error) error = convert_error(error);

         socket_info *info = &glNetLookup[(WSW_SOCKET)wParam];
         if ((info->Flags & FD_READ) and (!glSocketsDisabled)) {
            WSAAsyncSelect(info->SocketHandle, glNetWindow, WM_NETWORK, info->Flags & (~FD_READ));
         }

         if (info->Reference) win32_netresponse((struct BaseClass *)info->Reference, info->SocketHandle, state, error);
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

   ULONG non_blocking = 1;
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

int win_bind(WSW_SOCKET SocketHandle, const struct sockaddr *Name, int NameLen)
{
   if (bind(SocketHandle, Name, NameLen) IS SOCKET_ERROR) return convert_error(0);
   else return ERR_Okay;
}

//********************************************************************************************************************

int win_closesocket(WSW_SOCKET SocketHandle)
{
   const lock_guard<recursive_mutex> lock(csNetLookup);
   glNetLookup.erase(SocketHandle);
   return closesocket(SocketHandle);
}

//********************************************************************************************************************

int win_connect(WSW_SOCKET SocketHandle, const struct sockaddr *Name, int NameLen)
{
   if (connect(SocketHandle, Name, NameLen) IS SOCKET_ERROR) {
      if (WSAGetLastError() IS WSAEWOULDBLOCK) return ERR_Okay; // connect() will always 'fail' for non-blocking sockets (however it will continue to connect/succeed...!)
      return convert_error(0);
   }
   else return ERR_Okay;
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

int win_listen(WSW_SOCKET SocketHandle, int BackLog)
{
   if (listen(SocketHandle, BackLog) IS SOCKET_ERROR) return convert_error(0);
   else return ERR_Okay;
}

//********************************************************************************************************************

int WIN_RECEIVE(WSW_SOCKET SocketHandle, void *Buffer, int Len, int Flags, int *Result)
{
   *Result = 0;
   if (!Len) return ERR_Okay;
   auto result = recv(SocketHandle, reinterpret_cast<char *>(Buffer), Len, Flags);
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

//********************************************************************************************************************

int WIN_SEND(WSW_SOCKET Socket, const void *Buffer, int *Length, int Flags)
{
   if (!*Length) return ERR_Okay;
   *Length = send(Socket, reinterpret_cast<const char *>(Buffer), *Length, Flags);
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

//********************************************************************************************************************

int win_shutdown(WSW_SOCKET S, int How)
{
   return shutdown(S, How);
}

//********************************************************************************************************************
// Create a socket, make it non-blocking and configure it to wake our task when activity occurs on the socket.

WSW_SOCKET win_socket(void *NetSocket, char Read, char Write)
{
   if (auto handle = socket(PF_INET, SOCK_STREAM, 0); handle != INVALID_SOCKET) {
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

   csNetLookup.unlock();
   return NULL;
}

//********************************************************************************************************************

int ShutdownWinsock()
{
   if (glNetWindow) { DestroyWindow(glNetWindow); glNetWindow = FALSE; }
   if (glNetClassInit) { UnregisterClass("NetClass", GetModuleHandle(NULL)); glNetClassInit = FALSE; }
   if (glWinsockInitialised) { WSACleanup(); glWinsockInitialised = FALSE; }
   return 0;
}
