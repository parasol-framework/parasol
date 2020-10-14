
#include "ares_setup.h"
#include "ares.h"
#include "ares_dns.h"
#include "ares_inet_net_pton.h"
#include "ares_private.h"

#include <parasol/system/errors.h>

extern struct ares_channeldata *glAres;

//****************************************************************************

#ifdef __linux__
extern void deregister_fd(int);
extern void register_read_socket(int, void (*Callback)(int, void *), struct dns_resolver *);
extern void register_write_socket(int, void (*Callback)(int, void *), struct dns_resolver *);
extern void ares_response(void *Arg, int Status, int Timeouts, struct hostent *Host);
extern void set_resolver_socket(struct dns_resolver *Resolver, int UDP, int SocketHandle);

// Associate handles created by Ares with their respective DNS resolver.

static int ares_socket_callback(int SocketHandle, int Type, struct dns_resolver *Resolver)
{
   //FMSG("Ares","Ares created socket handle %d, type %d.", SocketHandle, Type);

   if (Resolver) {
      if (Type == SOCK_STREAM) set_resolver_socket(Resolver, 0, SocketHandle);
      else if (Type == SOCK_DGRAM) set_resolver_socket(Resolver, 1, SocketHandle);
   }
   return ARES_SUCCESS;
}

static void fd_read(int FD, struct rkNetSocket *Socket)
{
   if (glAres) {
      fd_set read_fds;
      FD_ZERO(&read_fds);
      FD_SET(FD, &read_fds);
      ares_process(glAres, &read_fds, 0);
   }
   else deregister_fd(FD);
}

static void fd_write(int FD, struct rkNetSocket *Socket)
{
   if (glAres) {
      fd_set write_fds;
      FD_ZERO(&write_fds);
      FD_SET(FD, &write_fds);
      ares_process(glAres, 0, &write_fds);
   }
   else deregister_fd(FD);
}

void net_ares_resolveaddr(int IPV4, void *Data, struct dns_resolver *resolve) 
{
   if (IPV4) ares_gethostbyaddr(glAres, Data, 4, AF_INET, &ares_response, resolve);
   else ares_gethostbyaddr(glAres, Data, 16, AF_INET6, &ares_response, resolve);

   // Ares file descriptors need to be reported to the Core if they are to be processed correctly.

   int active_queries = !ares__is_list_empty(&(glAres->all_queries));
   for (int i=0; i < glAres->nservers; i++) {
      struct server_state *server = &glAres->servers[i];
      if ((active_queries) && (server->udp_socket != ARES_SOCKET_BAD)) {
         register_read_socket(server->udp_socket, reinterpret_cast<void (*)(int, void*)>(&fd_read), resolve);
      }

      if (server->tcp_socket != ARES_SOCKET_BAD) {
         register_read_socket(server->tcp_socket, reinterpret_cast<void (*)(int, void*)>(&fd_read), resolve);
         if (server->qhead) register_write_socket(server->tcp_socket, reinterpret_cast<void (*)(int, void*)>(&fd_write), resolve);
      }
   }
}

void net_resolve_name(const char *HostName, struct dns_resolver *Resolver)
{
   ares_set_socket_callback(glAres, (ares_sock_create_callback)&ares_socket_callback, Resolver);
   ares_gethostbyname(glAres, HostName, AF_INET, &ares_response, Resolver);
   ares_set_socket_callback(glAres, NULL, NULL);

   // Ares file descriptors need to be reported to the Core if they are to be processed correctly.
   // Refer to the code for ares_fds() to see where this loop came from.

   // Commented out because the registration process is probably better served through ares_socket_callback()
/*
         if (glAres->queries) {
            LONG i;
            int active_queries = !ares__is_list_empty(&(glAres->all_queries));
            for (i=0; i < glAres->nservers; i++) {
               struct server_state *server = &glAres->servers[i];
               if ((active_queries) AND (server->udp_socket != ARES_SOCKET_BAD)) {
                  RegisterFD((HOSTHANDLE)server->udp_socket, RFD_READ|RFD_SOCKET, &fd_read, resolver);
               }

               if (server->tcp_socket != ARES_SOCKET_BAD) {
                  RegisterFD((HOSTHANDLE)server->tcp_socket, RFD_READ|RFD_SOCKET, &fd_read, resolver);
                  if (server->qhead) RegisterFD((HOSTHANDLE)server->tcp_socket, RFD_WRITE|RFD_SOCKET, &fd_write, resolver);
               }
            }
         }
*/
}

#endif

//****************************************************************************

const char * net_init_ares(void)
{
   ares_library_init(ARES_LIB_INIT_ALL);

   int acode;
   if ((acode = ares_init(&glAres)) != ARES_SUCCESS) {
      return ares_strerror(acode);
   }
   else return 0;
}

void net_free_ares(void)
{
   if (glAres) {
      ares_cancel(glAres);
      ares_destroy(glAres);
      glAres = NULL;
   }

   ares_library_cleanup();
}

int net_ares_error(int Code, const char **Message)
{
   if (Message) *Message = ares_strerror(Code);

   switch(Code) {
      case ARES_ENODATA:   return ERR_NoData;
      case ARES_EFORMERR:  return ERR_InvalidData;
      case ARES_ESERVFAIL: return ERR_ConnectionAborted;
      case ARES_ENOTFOUND: return ERR_HostNotFound;
      case ARES_ENOTIMP:   return ERR_NoSupport;
      case ARES_EREFUSED:  return ERR_Cancelled;
   }

   return ERR_Failed;
}

