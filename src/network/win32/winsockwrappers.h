#ifndef NETWORK_WINSOCKWRAPPERS_H
#define NETWORK_WINSOCKWRAPPERS_H TRUE

typedef unsigned int WSW_SOCKET; // type of socket handle for these wrapper procedures.  Same type as the windows SOCKET
struct sockaddr;
struct hostent;

void win32_netresponse(struct BaseClass *, WSW_SOCKET, int, int);
void win_net_processing(int, void *);
WSW_SOCKET win_accept(void *, WSW_SOCKET, struct sockaddr *, int *);
int win_bind(WSW_SOCKET, const struct sockaddr *, int);
int win_closesocket(WSW_SOCKET);
int win_connect(WSW_SOCKET, const struct sockaddr *, int);
struct hostent * win_gethostbyaddr(const struct IPAddress *);
struct hostent * win_gethostbyname(const char *);
int win_getpeername(WSW_SOCKET, struct sockaddr *, int *);
int win_getsockname(WSW_SOCKET, struct sockaddr *, int *);
unsigned long win_inet_addr(const char *);
char *win_inet_ntoa(unsigned long);//struct in_addr);
int win_listen(WSW_SOCKET, int);
void win_socketstate(WSW_SOCKET, char, char);
int WIN_SEND(WSW_SOCKET, const void *, int *, int);
int win_shutdown(WSW_SOCKET, int);
WSW_SOCKET win_socket(void *, char, char);/*uses PF_INET, SOCK_STREAM, 0 for params to socket() */
int WIN_RECEIVE(WSW_SOCKET, void *, int, int, int *);
void winCloseResolveHandle(void *);
void win_socket_reference(WSW_SOCKET, void *);

#ifdef USE_ARES
int win_ares_resolveaddr(struct IPAddress *, struct ares_channeldata *, void *);
int win_ares_resolvename(const char *, struct ares_channeldata *, void *);
void win_ares_deselect(int);
#endif

unsigned long win_htonl(unsigned long);
unsigned long win_ntohl(unsigned long);
unsigned short win_htons(unsigned short);
unsigned short win_ntohs(unsigned short);

// Some functions for checking error codes
int SocketWouldBlock();

int win_WSAGetLastError();

//ioctlsocket(Self->SocketHandle, FIONBIO, &non_blocking);
int SetNonBlocking(WSW_SOCKET);

//getsockopt(Self->SocketHandle, SOL_SOCKET, SO_ERROR, &result, &optlen)
int GetSockOptError(WSW_SOCKET, char *, int *);

const char * StartupWinsock();   // Returns zero if successful
int ShutdownWinsock();  // Returns zero if successful

enum {
   NTE_NONE=0,
   NTE_WRITE,
   NTE_READ,
   NTE_ACCEPT,
   NTE_CONNECT,
   NTE_CLOSE
};

#endif // NETWORK_WINSOCKWRAPPERS_H
