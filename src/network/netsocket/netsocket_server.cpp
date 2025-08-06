
//********************************************************************************************************************
// Called when a socket handle detects a new client wanting to connect to it.

static void server_client_connect(SOCKET_HANDLE FD, extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);
   UBYTE ip[8];
   SOCKET_HANDLE clientfd;

   log.traceBranch("FD: %d", FD);

   pf::SwitchContext context(Self);

   // Check client limit before accepting to prevent resource exhaustion
   if (Self->TotalClients >= Self->ClientLimit) {
      log.error(ERR::ArrayFull);
      return;
   }

   if (Self->IPV6) {
      #ifdef __linux__
         // For dual-stack sockets, use sockaddr_storage to handle both IPv4 and IPv6
         struct sockaddr_storage addr_storage;
         socklen_t len = sizeof(addr_storage);
         clientfd = accept(FD, (struct sockaddr *)&addr_storage, &len);
         if (clientfd IS NOHANDLE) return;

         int nodelay = 1;
         setsockopt(clientfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

         if (addr_storage.ss_family IS AF_INET6) { // IPv6 connection
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr_storage;
            ip[0] = addr6->sin6_addr.s6_addr[0];
            ip[1] = addr6->sin6_addr.s6_addr[1];
            ip[2] = addr6->sin6_addr.s6_addr[2];
            ip[3] = addr6->sin6_addr.s6_addr[3];
            ip[4] = addr6->sin6_addr.s6_addr[4];
            ip[5] = addr6->sin6_addr.s6_addr[5];
            ip[6] = addr6->sin6_addr.s6_addr[6];
            ip[7] = addr6->sin6_addr.s6_addr[7];
            log.trace("Accepted IPv6 client connection");
         }
         else if (addr_storage.ss_family IS AF_INET) { // IPv4 connection on dual-stack socket
            struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr_storage;
            uint32_t ipv4_addr = net::LongToHost(addr4->sin_addr.s_addr);
            ip[0] = ipv4_addr & 0xff;
            ip[1] = (ipv4_addr >> 8) & 0xff;
            ip[2] = (ipv4_addr >> 16) & 0xff;
            ip[3] = (ipv4_addr >> 24) & 0xff;
            ip[4] = ip[5] = ip[6] = ip[7] = 0;
            log.trace("Accepted IPv4 client connection on dual-stack socket");
         }
         else {
            log.warning("Unsupported address family: %d", addr_storage.ss_family);
            close(clientfd);
            return;
         }
      #elif _WIN32
         // Windows IPv6 dual-stack accept using wrapper function
         int family;
         struct sockaddr_storage addr_storage;
         int len = sizeof(addr_storage);
         clientfd = win_accept_ipv6(Self, FD, (struct sockaddr *)&addr_storage, &len, &family);
         if (clientfd IS NOHANDLE) return;

         if (family IS AF_INET6) { // IPv6 connection
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr_storage;
            ip[0] = addr6->sin6_addr.s6_addr[0];
            ip[1] = addr6->sin6_addr.s6_addr[1];
            ip[2] = addr6->sin6_addr.s6_addr[2];
            ip[3] = addr6->sin6_addr.s6_addr[3];
            ip[4] = addr6->sin6_addr.s6_addr[4];
            ip[5] = addr6->sin6_addr.s6_addr[5];
            ip[6] = addr6->sin6_addr.s6_addr[6];
            ip[7] = addr6->sin6_addr.s6_addr[7];
            log.trace("Accepted IPv6 client connection on Windows");
         }
         else if (family IS AF_INET) { // IPv4 connection on dual-stack socket
            struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr_storage;
            uint32_t ipv4_addr = net::LongToHost(addr4->sin_addr.s_addr);
            ip[0] = ipv4_addr & 0xff;
            ip[1] = (ipv4_addr >> 8) & 0xff;
            ip[2] = (ipv4_addr >> 16) & 0xff;
            ip[3] = (ipv4_addr >> 24) & 0xff;
            ip[4] = ip[5] = ip[6] = ip[7] = 0;
            log.trace("Accepted IPv4 client connection on dual-stack socket (Windows)");
         }
         else {
            log.warning("Unsupported address family on Windows: %d", family);
            CLOSESOCKET(clientfd);
            return;
         }
      #else
         #warning Platform requires IPV6 support.
         return;
      #endif
   }
   else {
      struct sockaddr_in addr;

      #ifdef __linux__
         socklen_t len = sizeof(addr);
         clientfd = accept(FD, (struct sockaddr *)&addr, &len);

         if (clientfd != NOHANDLE) {
            int nodelay = 1;
            setsockopt(clientfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
         }
      #elif _WIN32
         int len = sizeof(addr);
         clientfd = win_accept(Self, FD, (struct sockaddr *)&addr, &len);
      #endif

      if (clientfd IS NOHANDLE) {
         log.warning("accept() failed to return an FD.");
         return;
      }

      ip[0] = addr.sin_addr.s_addr;
      ip[1] = addr.sin_addr.s_addr>>8;
      ip[2] = addr.sin_addr.s_addr>>16;
      ip[3] = addr.sin_addr.s_addr>>24;
      ip[4] = 0;
      ip[5] = 0;
      ip[6] = 0;
      ip[7] = 0;
   }

   // Check if this IP address already has a client structure from an earlier socket connection.
   // (One NetClient represents a single IP address; Multiple ClientSockets can connect from that IP address)

   objNetClient *client_ip;
   for (client_ip=Self->Clients; client_ip; client_ip=client_ip->Next) {
      if (((LARGE *)&ip)[0] IS ((LARGE *)&client_ip->IP)[0]) break;
   }

   if (!client_ip) {
      if (NewObject(CLASSID::NETCLIENT, &client_ip) IS ERR::Okay) {
         if (InitObject(client_ip) != ERR::Okay) {
            FreeResource(client_ip);
            CLOSESOCKET(clientfd);
            return;
         }
      }
      else {
         CLOSESOCKET(clientfd);
         return;
      }

      client_ip->NetSocket = Self;
      ((LARGE *)&client_ip->IP)[0] = ((LARGE *)&ip)[0];
      client_ip->TotalConnections = 0;
      Self->TotalClients++;

      if (!Self->Clients) Self->Clients = client_ip;
      else {
         if (Self->LastClient) Self->LastClient->Next = client_ip;
         if (Self->Clients) Self->Clients->Prev = Self->LastClient;
      }
      Self->LastClient = client_ip;
   }
   else if (client_ip->TotalConnections >= Self->SocketLimit) {
      log.warning("Socket limit of %d reached for IP %d.%d.%d.%d", Self->SocketLimit, client_ip->IP[0], client_ip->IP[1], client_ip->IP[2], client_ip->IP[3]);
      CLOSESOCKET(clientfd);
      return;
   }

   if ((Self->Flags & NSF::MULTI_CONNECT) IS NSF::NIL) { // Check if the IP is already registered and alive
      if (client_ip->Connections) {
         // Check if the client is alive by writing to it.  If the client is dead, remove it and continue with the new connection.

         log.msg("Preventing second connection attempt from IP %d.%d.%d.%d", client_ip->IP[0], client_ip->IP[1], client_ip->IP[2], client_ip->IP[3]);
         CLOSESOCKET(clientfd);
         return;
      }
   }

   // Socket Management

   extClientSocket *client_socket;
   if (NewObject(CLASSID::CLIENTSOCKET, &client_socket) IS ERR::Okay) {
      client_socket->Handle = clientfd;
      client_socket->Client = client_ip;
      InitObject(client_socket);
   }
   else {
      CLOSESOCKET(clientfd);
      if (!client_ip->Connections) free_client(Self, client_ip);
      return;
   }

   if (Self->Feedback.isC()) {
      pf::SwitchContext context(Self->Feedback.Context);
      auto routine = (void (*)(extNetSocket *, objClientSocket *, NTC, APTR))Self->Feedback.Routine;
      if (routine) routine(Self, client_socket, NTC::CONNECTED, Self->Feedback.Meta);
   }
   else if (Self->Feedback.isScript()) {
      sc::Call(Self->Feedback, std::to_array<ScriptArg>({
         { "NetSocket",    Self, FD_OBJECTPTR },
         { "ClientSocket", client_socket, FD_OBJECTPTR },
         { "State",        int(NTC::CONNECTED) }
      }));
   }

   log.trace("Total clients: %d", Self->TotalClients);
}

/*********************************************************************************************************************
** Terminates all connections to the client and removes associated resources.
*/

static void free_client(extNetSocket *Self, objNetClient *Client)
{
   pf::Log log(__FUNCTION__);
   static THREADVAR BYTE recursive = 0;

   if (!Client) return;
   if (recursive) return;

   recursive++;

   log.branch("%d:%d:%d:%d, Connections: %d", Client->IP[0], Client->IP[1], Client->IP[2], Client->IP[3], Client->TotalConnections);

   // Free all sockets (connections) related to this client IP

   while (Client->Connections) {
      objClientSocket *current_socket = Client->Connections;
      free_client_socket(Self, (extClientSocket *)Client->Connections, TRUE);
      if (Client->Connections IS current_socket) {
         log.warning("Resource management error detected in Client->Sockets");
         break;
      }
   }

   if (Client->Prev) {
      Client->Prev->Next = Client->Next;
      if (Client->Next) Client->Next->Prev = Client->Prev;
   }
   else {
      Self->Clients = Client->Next;
      if ((Self->Clients) and (Self->Clients->Next)) Self->Clients->Next->Prev = NULL;
   }

   FreeResource(Client);

   Self->TotalClients--;

   recursive--;
}

//********************************************************************************************************************
// Terminates the connection to the client and removes associated resources.

static void free_client_socket(extNetSocket *ServerSocket, extClientSocket *ClientSocket, BYTE Signal)
{
   pf::Log log(__FUNCTION__);

   if (!ClientSocket) return;

   log.branch("Handle: %d, NetSocket: %d, ClientSocket: %d", ClientSocket->Handle, ServerSocket->UID, ClientSocket->UID);

   if (Signal) {
      if (ServerSocket->Feedback.isC()) {
         pf::SwitchContext context(ServerSocket->Feedback.Context);
         auto routine = (void (*)(extNetSocket *, objClientSocket *, NTC, APTR))ServerSocket->Feedback.Routine;
         if (routine) routine(ServerSocket, ClientSocket, NTC::DISCONNECTED, ServerSocket->Feedback.Meta);
      }
      else if (ServerSocket->Feedback.isScript()) {
         sc::Call(ServerSocket->Feedback, std::to_array<ScriptArg>({
            { "NetSocket",    ServerSocket, FD_OBJECTPTR },
            { "ClientSocket", ClientSocket, FD_OBJECTPTR },
            { "State",        int(NTC::DISCONNECTED) }
         }));
      }
   }

   FreeResource(ClientSocket);
}
