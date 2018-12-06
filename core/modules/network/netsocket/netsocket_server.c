
//****************************************************************************
// This routine will be called when there is some activity occurring on a server socket.

static void server_client_connect(SOCKET_HANDLE FD, APTR Data)
{
   objNetSocket *Self = Data;
   UBYTE ip[8];
   LONG clientfd;
   int len;

   FMSG("~socket_connect()","FD: %d", FD);

   if (Self->IPV6) {
#ifdef __linux__

      #ifdef ENABLE_SSL_ACCEPT
         // NB: I think we can just use a standard accept() and then wait
         // for it to communicate via SSL or create an SSL connection object
         // for it???  See BIO_s_accept() documentation for other ideas.

         if (Self->SSL) {
            LONG result;
            if ((result = xtSSL_accept(Self->SSL)) != 1) {
               LogErrorMsg("SSL_accept: %s", xtERR_error_string(xtSSL_get_error(Self->SSL, result), NULL));
            }

            LogErrorMsg("No support for retrieving IPV6 address and client handle yet.");

            clientfd = NOHANDLE;

         }
         else {
            struct sockaddr_in6 addr;
            len = sizeof(addr);
            clientfd = accept(FD, (struct sockaddr *)&addr, &len);
            if (clientfd IS NOHANDLE) { STEP(); return; }
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
         len = sizeof(addr);
         clientfd = accept(FD, (struct sockaddr *)&addr, &len);
         if (clientfd IS NOHANDLE) { STEP(); return; }
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
      STEP();
      return;
#endif
   }
   else {
      struct sockaddr_in addr;

      len = sizeof(addr);

      #ifdef __linux__
         clientfd = accept(FD, (struct sockaddr *)&addr, &len);
      #elif _WIN32
         clientfd = win_accept(Self, FD, (struct sockaddr *)&addr, &len);
      #endif

      if (clientfd IS NOHANDLE) {
         LogF("@server_connect","accept() failed to return an FD.");
         STEP();
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

   if (Self->TotalClients >= Self->ClientLimit) {
      CLOSESOCKET(clientfd);
      PostError(ERR_ArrayFull);
      STEP();
      return;
   }

   // Check if this IP address already has a client structure from an earlier socket connection.

   struct rkNetClient *client;
   for (client=Self->Clients; client; client=client->Next) {
      if (((LARGE *)&ip)[0] IS ((LARGE *)&client->IP)[0]) break;
   }

   if (!client) {
      if (AllocMemory(sizeof(struct rkNetClient), MEM_DATA, &client, NULL) != ERR_Okay) {
         CLOSESOCKET(clientfd);
         STEP();
         return;
      }

      client->NetSocket = Self;
      ((LARGE *)&client->IP)[0] = ((LARGE *)&ip)[0];
      client->TotalSockets = 0;
      Self->TotalClients++;

      if (!Self->Clients) Self->Clients = client;
      else {
         if (Self->LastClient) Self->LastClient->Next = client;
         if (Self->Clients) Self->Clients->Prev = Self->LastClient;
      }
      Self->LastClient = client;
   }

   if (!(Self->Flags & NSF_MULTI_CONNECT)) { // Check if the IP is already registered and alive
      if (client->Sockets) {
         // Check if the client is alive by writing to it.  If the client is dead, remove it and continue with the new connection.

         LogF("socket_connect","Preventing second connection attempt from IP %d.%d.%d.%d\n", client->IP[0], client->IP[1], client->IP[2], client->IP[3]);
         CLOSESOCKET(clientfd);
         STEP();
         return;
      }
   }

   // Socket Management

   objClientSocket *socket;
   if (AllocMemory(sizeof(objClientSocket), MEM_DATA, &socket, NULL) != ERR_Okay) {
      CLOSESOCKET(clientfd);
      if (!client->Sockets) free_client(Self, client);
      STEP();
      return;
   }

#ifdef __linux__
   LONG non_blocking;
   non_blocking = 1;
   ioctl(clientfd, FIONBIO, &non_blocking);
#endif

   socket->Handle = clientfd;
   socket->ConnectTime = PreciseTime() / 1000LL;
   socket->Client = client;

   if (client->Sockets) {
      socket->Next = client->Sockets;
      socket->Prev = NULL;
      client->Sockets->Prev = socket;
   }

   client->Sockets = socket;
   client->TotalSockets++;

#ifdef __linux__
   RegisterFD(clientfd, RFD_READ|RFD_SOCKET, &server_client_incoming, socket);
#elif _WIN32
   // Not necessary to call win_socketstate() as win_accept() sets this up for us automatically.
#endif

   if (Self->Feedback.Type) {
      objClientSocket *save_socket = Self->CurrentSocket;
      Self->CurrentSocket = socket;

      if (Self->Feedback.Type IS CALL_STDC) {
         void (*routine)(objNetSocket *, LONG);
         OBJECTPTR context = SetContext(Self->Feedback.StdC.Context);
            routine = Self->Feedback.StdC.Routine;
            routine(Self, NTC_CONNECTED);
         SetContext(context);
      }
      else if (Self->Feedback.Type IS CALL_SCRIPT) {
         const struct ScriptArg args[] = {
            { "NetSocket", FD_OBJECTPTR, { .Address = Self } },
            { "State",     FD_LONG,      { .Long = NTC_CONNECTED } }
         };

         OBJECTPTR script;
         if ((script = Self->Feedback.Script.Script)) {
            scCallback(script, Self->Feedback.Script.ProcedureID, args, ARRAYSIZE(args));
         }
      }

      Self->CurrentSocket = save_socket;
   }

   FMSG("socket_connect:","Total clients: %d", Self->TotalClients);

   STEP();
}

//****************************************************************************
// If the socket is a server, messages from clients will come in through here.

static void server_client_incoming(SOCKET_HANDLE FD, APTR Data)
{
   objClientSocket *Socket = Data;

   if ((!Socket) OR (!Socket->Client)) return;

   objNetSocket *Self = Socket->Client->NetSocket;
   Self->InUse++;
   Socket->ReadCalled = FALSE;

   FMSG("~server_incoming:","Handle: %d", FD);

   // In raw messaging mode, we tell the app to read from the client with this callback.  The app calls
   // Read or ReadMsg to retrieve information from the client.

   ERROR error;
   if (Socket->Incoming.Type) {
      Self->CurrentSocket = Socket;

         if (Socket->Incoming.Type IS CALL_STDC) {
            ERROR (*routine)(objNetSocket *, objClientSocket *);
            OBJECTPTR context = SetContext(Socket->Incoming.StdC.Context);
               routine = Socket->Incoming.StdC.Routine;
               error = routine(Self, Socket);
            SetContext(context);
         }
         else if (Socket->Incoming.Type IS CALL_SCRIPT) {
            const struct ScriptArg args[] = {
               { "NetSocket", FD_OBJECTPTR, { .Address = Self } },
               { "Socket",    FD_POINTER,   { .Address = Socket } }
            };

            OBJECTPTR script;
            if ((script = Socket->Incoming.Script.Script)) {
               if (!scCallback(script, Socket->Incoming.Script.ProcedureID, args, ARRAYSIZE(args))) {
                  GetLong(script, FID_Error, &error);
               }
               else error = ERR_Terminate; // Fatal error in attempting to execute the procedure
            }
         }
         else LogF("@server_incoming","No callback configured (got %d).", Socket->Incoming.Type);

      Self->CurrentSocket = NULL;

      if (error) Socket->Incoming.Type = CALL_NONE;

      if (error IS ERR_Terminate) {
         FMSG("server_incoming:","Termination request received.");
         free_client_socket(Self, Socket, TRUE);
         Self->InUse--;
         STEP();
         return;
      }
   }
   else LogF("@server_incoming","No callback configured.");

   if (Socket->ReadCalled IS FALSE) {
      UBYTE buffer[80];
      LogF("@server_incoming:","Subscriber did not call Read(), cleaning buffer.");
      LONG result;
      do { error = RECEIVE(Self, Socket->Handle, &buffer, sizeof(buffer), 0, &result); } while (result > 0);
      if (error) free_client_socket(Self, Socket, TRUE);
   }

   Self->InUse--;

   STEP();
}

/*****************************************************************************
** If the socket is a server and has data queued against a client, this routine is called.
*/

static void server_client_outgoing(SOCKET_HANDLE FD, APTR Data)
{
   objClientSocket *Socket = Data;
   objNetSocket *Self;
   ERROR error;
   LONG len;

   if ((!Socket) OR (!Socket->Client)) {
      FMSG("@server_outgoing()","No Socket or Socket->Client.");
      return;
   }

   Self = Socket->Client->NetSocket;

   if (Self->Terminating) return;

   FMSG("~server_outgoing()","%d", FD);

   Self->InUse++;

   error = ERR_Okay;

   // Send out remaining queued data before getting new data to send

   if (Socket->WriteQueue.Buffer) {
      while (Socket->WriteQueue.Buffer) {
         len = Socket->WriteQueue.Length - Socket->WriteQueue.Index;
         if (len > glMaxWriteLen) len = glMaxWriteLen;

         if (len > 0) {
            error = SEND(Self, FD, (BYTE *)Socket->WriteQueue.Buffer + Socket->WriteQueue.Index, &len, 0);

            if ((error) OR (!len)) break;

            FMSG("server_out:","[NetSocket:%d] Sent %d of %d bytes remaining on the queue.", Self->Head.UniqueID, len, Socket->WriteQueue.Length-Socket->WriteQueue.Index);

            Socket->WriteQueue.Index += len;
         }

         if (Socket->WriteQueue.Index >= Socket->WriteQueue.Length) {
            FMSG("server_out:","Terminating the write queue (pos %d/%d).", Socket->WriteQueue.Index, Socket->WriteQueue.Length);
            FreeResource(Socket->WriteQueue.Buffer);
            Socket->WriteQueue.Buffer = NULL;
            Socket->WriteQueue.Index  = 0;
            Socket->WriteQueue.Length = 0;
            break;
         }
      }
   }

   // Before feeding new data into the queue, the current buffer must be empty.

   if ((!Socket->WriteQueue.Buffer) OR (Socket->WriteQueue.Index >= Socket->WriteQueue.Length)) {
      if (Socket->Outgoing.Type != CALL_NONE) {
         Self->CurrentSocket = Socket;

            if (Socket->Outgoing.Type IS CALL_STDC) {
               ERROR (*routine)(objNetSocket *, objClientSocket *);
               routine = Socket->Outgoing.StdC.Routine;
               OBJECTPTR context = SetContext(Socket->Outgoing.StdC.Context);
                  error = routine(Self, Socket);
               SetContext(context);
            }
            else if (Socket->Outgoing.Type IS CALL_SCRIPT) {
               OBJECTPTR script;
               const struct ScriptArg args[] = {
                  { "NetSocket", FD_OBJECTPTR, { .Address = Self } },
                  { "Socket",    FD_OBJECTPTR, { .Address = Socket } }
               };

               if ((script = Socket->Outgoing.Script.Script)) {
                  if (!scCallback(script, Socket->Outgoing.Script.ProcedureID, args, ARRAYSIZE(args))) {
                     GetLong(script, FID_Error, &error);
                  }
                  else error = ERR_Terminate; // Fatal error in attempting to execute the procedure
               }
            }

            if (error) Socket->Outgoing.Type = CALL_NONE;

         Self->CurrentSocket = NULL;
      }

      // If the write queue is empty and all data has been retrieved, we can remove the FD-Write registration so that
      // we don't tax the system resources.

      if (!Socket->WriteQueue.Buffer) {
         FMSG("server_out","[NetSocket:%d] Write-queue listening on FD %d will now stop.", Self->Head.UniqueID, FD);
         RegisterFD((HOSTHANDLE)FD, RFD_REMOVE|RFD_WRITE|RFD_SOCKET, NULL, NULL);
         #ifdef _WIN32
         win_socketstate(FD, -1, 0);
         #endif
      }
   }
   else FMSG("server_out","Outgoing buffer is not empty.");

   Self->InUse--;

   STEP();
}

/*****************************************************************************
** Terminates the connection to the client and removes associated resources.
*/

static void free_client(objNetSocket *Self, struct rkNetClient *Client)
{
   static THREADVAR BYTE recursive = 0;

   if (!Client) return;
   if (recursive) return;

   recursive++;

   LogF("~free_client()","%d:%d:%d:%d, Sockets: %d", Client->IP[0], Client->IP[1], Client->IP[2], Client->IP[3], Client->TotalSockets);

   // Free all sockets related to this client

   while (Client->Sockets) free_client_socket(Self, Client->Sockets, TRUE);

   if (Client->Prev) {
      Client->Prev->Next = Client->Next;
      if (Client->Next) Client->Next->Prev = Client->Prev;
   }
   else {
      Self->Clients = Client->Next;
      if ((Self->Clients) AND (Self->Clients->Next)) Self->Clients->Next->Prev = NULL;
   }

   FreeResource(Client);

   Self->TotalClients--;

   recursive--;
   LogBack();
}

/*****************************************************************************
** Terminates the connection to the client and removes associated resources.
*/

static void free_client_socket(objNetSocket *Self, objClientSocket *Socket, BYTE Signal)
{
   if (!Socket) return;

   struct rkNetClient *client = Socket->Client;

   LogF("~free_socket()","Handle: %d, Client-Total: %d", Socket->Handle, client->TotalSockets);

   if ((Signal) AND (Self->Feedback.Type)) {
      if (Self->Feedback.Type IS CALL_STDC) {
         void (*routine)(objNetSocket *, LONG);
         OBJECTPTR context = SetContext(Self->Feedback.StdC.Context);
            routine = Self->Feedback.StdC.Routine;
            routine(Self, NTC_DISCONNECTED);
         SetContext(context);
      }
      else if (Self->Feedback.Type IS CALL_SCRIPT) {
         const struct ScriptArg args[] = {
            { "NetSocket", FD_OBJECTPTR, { .Address = Self } },
            { "State",     FD_LONG,      { .Long = NTC_DISCONNECTED } }
         };

         OBJECTPTR script;
         if ((script = Self->Feedback.Script.Script)) {
            scCallback(script, Self->Feedback.Script.ProcedureID, args, ARRAYSIZE(args));
         }
      }
   }

   if (Socket->Handle) {
#ifdef __linux__
      DeregisterFD(Socket->Handle);
#endif
      CLOSESOCKET(Socket->Handle);
      Socket->Handle = -1;
   }

   if (Socket->ReadQueue.Buffer) { FreeResource(Socket->ReadQueue.Buffer); Socket->ReadQueue.Buffer = NULL; }
   if (Socket->WriteQueue.Buffer) { FreeResource(Socket->WriteQueue.Buffer); Socket->WriteQueue.Buffer = NULL; }

   if (Socket->Prev) {
      Socket->Prev->Next = Socket->Next;
      if (Socket->Next) Socket->Next->Prev = Socket->Prev;
   }
   else {
      client->Sockets = Socket->Next;
      if (Socket->Next) Socket->Next->Prev = NULL;
   }

   FreeResource(Socket);

   client->TotalSockets--;

   if (!client->Sockets) {
      LogMsg("No more open sockets, removing client.");
      free_client(Self, client);
   }

   LogBack();
}
