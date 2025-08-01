
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
      #ifdef ENABLE_SSL_ACCEPT
         if (Self->SSL) {
            LONG result;
            if ((result = xtSSL_accept(Self->SSL)) != 1) {
               log.warning("SSL_accept: %s", xtERR_error_string(xtSSL_get_error(Self->SSL, result), NULL));
            }

            log.warning("No support for retrieving IPV6 address and client handle yet.");

            clientfd = NOHANDLE;
         }
         else {
            struct sockaddr_in6 addr;
            socklen_t len = sizeof(addr);
            clientfd = accept(FD, (struct sockaddr *)&addr, &len);
            if (clientfd IS NOHANDLE) return;
            ip[0] = addr.sin6_addr.s6_addr[0];
            ip[1] = addr.sin6_addr.s6_addr[1];
            ip[2] = addr.sin6_addr.s6_addr[2];
            ip[3] = addr.sin6_addr.s6_addr[3];
            ip[4] = addr.sin6_addr.s6_addr[4];
            ip[5] = addr.sin6_addr.s6_addr[5];
            ip[6] = addr.sin6_addr.s6_addr[6];
            ip[7] = addr.sin6_addr.s6_addr[7];
         }
      #else
         struct sockaddr_in6 addr;
         socklen_t len = sizeof(addr);
         clientfd = accept(FD, (struct sockaddr *)&addr, &len);
         if (clientfd IS NOHANDLE) return;
         ip[0] = addr.sin6_addr.s6_addr[0];
         ip[1] = addr.sin6_addr.s6_addr[1];
         ip[2] = addr.sin6_addr.s6_addr[2];
         ip[3] = addr.sin6_addr.s6_addr[3];
         ip[4] = addr.sin6_addr.s6_addr[4];
         ip[5] = addr.sin6_addr.s6_addr[5];
         ip[6] = addr.sin6_addr.s6_addr[6];
         ip[7] = addr.sin6_addr.s6_addr[7];
      #endif
#else
      log.warning("IPV6 not supported yet.");
      return;
#endif
   }
   else {
      struct sockaddr_in addr;

      #ifdef __linux__
         socklen_t len = sizeof(addr);
         clientfd = accept(FD, (struct sockaddr *)&addr, &len);
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

   struct NetClient *client_ip;
   for (client_ip=Self->Clients; client_ip; client_ip=client_ip->Next) {
      if (((LARGE *)&ip)[0] IS ((LARGE *)&client_ip->IP)[0]) break;
   }

   if (!client_ip) {
      if (AllocMemory(sizeof(struct NetClient), MEM::DATA, &client_ip) != ERR::Okay) {
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

   if ((Self->Flags & NSF::MULTI_CONNECT) IS NSF::NIL) { // Check if the IP is already registered and alive
      if (client_ip->Connections) {
         // Check if the client is alive by writing to it.  If the client is dead, remove it and continue with the new connection.

         log.msg("Preventing second connection attempt from IP %d.%d.%d.%d\n", client_ip->IP[0], client_ip->IP[1], client_ip->IP[2], client_ip->IP[3]);
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
         { "State",        LONG(NTC::CONNECTED) }
      }));
   }

   log.trace("Total clients: %d", Self->TotalClients);
}

/*********************************************************************************************************************
** Terminates all connections to the client and removes associated resources.
*/

static void free_client(extNetSocket *Self, struct NetClient *Client)
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

static void free_client_socket(extNetSocket *Socket, extClientSocket *ClientSocket, BYTE Signal)
{
   pf::Log log(__FUNCTION__);

   if (!ClientSocket) return;

   log.branch("Handle: %d, NetSocket: %d, ClientSocket: %d", ClientSocket->SocketHandle, Socket->UID, ClientSocket->UID);

   if (Signal) {
      if (Socket->Feedback.isC()) {
         pf::SwitchContext context(Socket->Feedback.Context);
         auto routine = (void (*)(extNetSocket *, objClientSocket *, NTC, APTR))Socket->Feedback.Routine;
         if (routine) routine(Socket, ClientSocket, NTC::DISCONNECTED, Socket->Feedback.Meta);
      }
      else if (Socket->Feedback.isScript()) {
         sc::Call(Socket->Feedback, std::to_array<ScriptArg>({
            { "NetSocket",    Socket, FD_OBJECTPTR },
            { "ClientSocket", ClientSocket, FD_OBJECTPTR },
            { "State",        LONG(NTC::DISCONNECTED) }
         }));
      }
   }

   FreeResource(ClientSocket);
}
