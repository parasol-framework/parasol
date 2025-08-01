/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
ClientSocket: Represents a single socket connection to a client IP address.

If a @Netsocket is running in server mode then it will create a new ClientSocket object every time that a new connection
is opened by a client.  This is a very simple class that assists in the management of I/O between the client and server.
-END-

*********************************************************************************************************************/

// Data is being received from a client.

static void clientsocket_incoming(HOSTHANDLE SocketHandle, APTR Data)
{
   pf::Log log(__FUNCTION__);
   auto ClientSocket = (extClientSocket *)Data;
   if (!ClientSocket->Client) return;
   auto Socket = (extNetSocket *)(ClientSocket->Client->NetSocket);

   Socket->InUse++;
   ClientSocket->ReadCalled = FALSE;

   log.traceBranch("Handle: %" PF64 ", Socket: %d, Client: %d", (LARGE)(MAXINT)SocketHandle, Socket->UID, ClientSocket->UID);

   ERR error = ERR::Okay;
   if (Socket->Incoming.defined()) {
      if (Socket->Incoming.isC()) {
         pf::SwitchContext context(Socket->Incoming.Context);
         auto routine = (ERR (*)(extNetSocket *, extClientSocket *, APTR))Socket->Incoming.Routine;
         error = routine(Socket, ClientSocket, Socket->Incoming.Meta);
      }
      else if (Socket->Incoming.isScript()) {
         if (sc::Call(Socket->Incoming, std::to_array<ScriptArg>({
               { "NetSocket",    Socket, FD_OBJECTPTR },
               { "ClientSocket", ClientSocket, FD_OBJECTPTR }
            }), error) != ERR::Okay) error = ERR::Terminate;
      }
      else error = ERR::InvalidValue;

      if (error != ERR::Okay) {
         log.msg("Received error %d, incoming callback will be terminated.", LONG(error));
         Socket->Incoming.clear();
      }

      if (error IS ERR::Terminate) {
         log.trace("Termination request received.");
         free_client_socket(Socket, ClientSocket, TRUE);
         Socket->InUse--;
         return;
      }
   }
   else log.warning("No Incoming callback configured.");

   if (ClientSocket->ReadCalled IS FALSE) {
      uint8_t buffer[80];
      log.warning("Subscriber did not call Read(), cleaning buffer.");
      int result;
      do { error = RECEIVE(Socket, ClientSocket->Handle, &buffer, sizeof(buffer), 0, &result); } while (result > 0);
      if (error != ERR::Okay) free_client_socket(Socket, ClientSocket, TRUE);
   }

   Socket->InUse--;
}

//********************************************************************************************************************
// Note that this function will prevent the task from going to sleep if it is not managed correctly.  If
// no data is being written to the queue, the program will not be able to sleep until the client stops listening
// to the write queue.

static void clientsocket_outgoing(HOSTHANDLE Void, APTR Data)
{
   pf::Log log(__FUNCTION__);
   auto ClientSocket = (extClientSocket *)Data;
   auto Socket = (extNetSocket *)(ClientSocket->Client->NetSocket);

   if (Socket->Terminating) return;

#ifdef ENABLE_SSL
  #ifdef _WIN32
    if ((Socket->WinSSL) and (Socket->State IS NTC::CONNECTING_SSL)) {
      log.trace("Still connecting via SSL...");
      return;
    }
  #else
    if ((Socket->SSL) and (Socket->State IS NTC::CONNECTING_SSL)) {
      log.trace("Still connecting via SSL...");
      return;
    }
  #endif
#endif

   if (ClientSocket->OutgoingRecursion) {
      log.trace("Recursion detected.");
      return;
   }

   log.traceBranch();

#ifdef ENABLE_SSL
  #ifndef _WIN32
    if (Socket->SSLBusy) return; // SSL object is performing a background operation (e.g. handshake)
  #endif
#endif

   ClientSocket->InUse++;
   ClientSocket->OutgoingRecursion++;

   auto error = ERR::Okay;

   // Send out remaining queued data before getting new data to send

   while (!ClientSocket->WriteQueue.Buffer.empty()) {
      size_t len = ClientSocket->WriteQueue.Buffer.size() - ClientSocket->WriteQueue.Index;
      #ifdef ENABLE_SSL
         #ifdef _WIN32
            if ((!Socket->WinSSL) and (len > glMaxWriteLen)) len = glMaxWriteLen;
         #else
            if ((!Socket->SSL) and (len > glMaxWriteLen)) len = glMaxWriteLen;
         #endif
      #else
         if (len > glMaxWriteLen) len = glMaxWriteLen;
      #endif

      if (len > 0) {
         error = SEND(Socket, ClientSocket->SocketHandle, ClientSocket->WriteQueue.Buffer.data() + ClientSocket->WriteQueue.Index, &len, 0);
         if ((error != ERR::Okay) or (!len)) break;
         ClientSocket->WriteQueue.Index += len;
      }

      if (ClientSocket->WriteQueue.Index >= ClientSocket->WriteQueue.Buffer.size()) {
         ClientSocket->WriteQueue.Buffer.clear();
         ClientSocket->WriteQueue.Index = 0;
         break;
      }
   }

   // Before feeding new data into the queue, the current buffer must be empty.

   if ((ClientSocket->WriteQueue.Buffer.empty()) or
       (ClientSocket->WriteQueue.Index >= ClientSocket->WriteQueue.Buffer.size())) {
      if (ClientSocket->Outgoing.defined()) {
         if (ClientSocket->Outgoing.isC()) {
            auto routine = (ERR (*)(extNetSocket *, extClientSocket *, APTR))(ClientSocket->Outgoing.Routine);
            pf::SwitchContext context(ClientSocket->Outgoing.Context);
            error = routine(Socket, ClientSocket, ClientSocket->Outgoing.Meta);
         }
         else if (ClientSocket->Outgoing.isScript()) {
            if (sc::Call(ClientSocket->Outgoing, std::to_array<ScriptArg>({
                  { "NetSocket", Socket, FD_OBJECTPTR },
                  { "ClientSocket", ClientSocket, FD_OBJECTPTR }
               }), error) != ERR::Okay) error = ERR::Terminate;
         }

         if (error != ERR::Okay) ClientSocket->Outgoing.clear();
      }

      // If the write queue is empty and all data has been retrieved, we can remove the FD-Write registration so that
      // we don't tax the system resources.

      if ((!ClientSocket->Outgoing.defined()) and (ClientSocket->WriteQueue.Buffer.empty())) {
         log.trace("[NetSocket:%d] Write-queue listening on FD %d will now stop.", Socket->UID, ClientSocket->SocketHandle);
         #ifdef __linux__
            RegisterFD((HOSTHANDLE)ClientSocket->SocketHandle, RFD::REMOVE|RFD::WRITE|RFD::SOCKET, nullptr, nullptr);
         #elif _WIN32
            win_socketstate(ClientSocket->SocketHandle, -1, 0);
         #endif
      }
   }

   ClientSocket->InUse--;
   ClientSocket->OutgoingRecursion--;
}

//********************************************************************************************************************

static ERR CLIENTSOCKET_Free(extClientSocket *Self)
{
   pf::Log log;

   if (Self->SocketHandle) {
#ifdef __linux__
      DeregisterFD(Self->Handle);
#endif
      CLOSESOCKET(Self->Handle);
      Self->Handle = -1;
   }

   if (Self->Prev) {
      Self->Prev->Next = Self->Next;
      if (Self->Next) Self->Next->Prev = Self->Prev;
   }
   else {
      Self->Client->Connections = Self->Next;
      if (Self->Next) Self->Next->Prev = nullptr;
   }

   Self->Client->TotalConnections--;

   if (!Self->Client->Connections) {
      log.msg("No more connections for this IP, removing client.");
      free_client((extNetSocket *)Self->Client->NetSocket, Self->Client);
   }

   Self->~extClientSocket();
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR CLIENTSOCKET_Init(extClientSocket *Self)
{
#ifdef __linux__
   int non_blocking = 1;
   ioctl(Self->Handle, FIONBIO, &non_blocking);
#endif

   Self->ConnectTime = PreciseTime() / 1000LL;

   if (Self->Client->Connections) {
      Self->Next = Self->Client->Connections;
      Self->Prev = nullptr;
      Self->Client->Connections->Prev = Self;
   }

   Self->Client->Connections = Self;
   Self->Client->TotalConnections++;

#ifdef __linux__
   RegisterFD(Self->Handle, RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&clientsocket_incoming), Self);
#elif _WIN32
   win_socket_reference(Self->Handle, Self);
#endif

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR CLIENTSOCKET_NewPlacement(extClientSocket *Self)
{
   new (Self) extClientSocket;
   return ERR::Okay;
}

/*********************************************************************************************************************

-ACTION-
Read: Read incoming data from a client socket.

The Read() action will read incoming data from the socket and write it to the provided buffer.  If the socket connection
is safe, success will always be returned by this action regardless of whether or not data was available.  Almost all
other return codes indicate permanent failure, and the socket connection will be closed when the action returns.

-ERRORS-
Okay: Read successful (if no data was on the socket, success is still indicated).
NullArgs
Disconnected: The socket connection is closed.
Failed: A permanent failure has occurred and socket has been closed.

*********************************************************************************************************************/

static ERR CLIENTSOCKET_Read(extClientSocket *Self, struct acRead *Args)
{
   pf::Log log;
   if ((!Args) or (!Args->Buffer)) return log.error(ERR::NullArgs);
   if (Self->SocketHandle IS NOHANDLE) return log.error(ERR::Disconnected);
   Self->ReadCalled = TRUE;
   if (!Args->Length) { Args->Result = 0; return ERR::Okay; }
   return RECEIVE((extNetSocket *)(Self->Client->NetSocket), Self->SocketHandle, Args->Buffer, Args->Length, 0, &Args->Result);
}

/*********************************************************************************************************************

-ACTION-
Write: Writes data to the socket.

Write raw data to a client socket with this action.  Write connections are buffered, so any data overflow generated
in a call to this action will be buffered into a software queue.  Resource limits placed on the software queue are
governed by the @NetSocket.MsgLimit value.

*********************************************************************************************************************/

static ERR CLIENTSOCKET_Write(extClientSocket *Self, struct acWrite *Args)
{
   pf::Log log;

   if (!Args) return ERR::NullArgs;
   Args->Result = 0;
   if (Self->SocketHandle IS NOHANDLE) return log.error(ERR::Disconnected);

   size_t len = Args->Length;
   ERR error = SEND((extNetSocket *)(Self->Client->NetSocket), Self->SocketHandle, Args->Buffer, &len, 0);

   if ((error != ERR::Okay) or (len < size_t(Args->Length))) {
      if (error != ERR::Okay) log.trace("SEND() Error: '%s', queuing %d/%d bytes for transfer...", GetErrorMsg(error), Args->Length - len, Args->Length);
      else log.trace("Queuing %d of %d remaining bytes for transfer...", Args->Length - len, Args->Length);
      if ((error IS ERR::DataSize) or (error IS ERR::BufferOverflow) or (len > 0))  {
         ((extNetSocket *)(Self->Client->NetSocket))->write_queue(Self->WriteQueue, (BYTE *)Args->Buffer + len, Args->Length - len);
         #ifdef __linux__
            RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD::WRITE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&clientsocket_outgoing), Self);
         #elif _WIN32
            win_socketstate(Self->SocketHandle, -1, TRUE);
         #endif
      }
   }
   else log.trace("Successfully wrote all %d bytes to the server.", Args->Length);

   Args->Result = Args->Length;
   return ERR::Okay;
}

//********************************************************************************************************************

#include "clientsocket_def.c"

static const FieldArray clClientSocketFields[] = {
   { "ConnectTime", FDF_INT64|FDF_R },
   { "Prev",        FDF_OBJECT|FDF_R, nullptr, nullptr, CLASSID::CLIENTSOCKET },
   { "Next",        FDF_OBJECT|FDF_R, nullptr, nullptr, CLASSID::CLIENTSOCKET },
   { "Client",      FDF_POINTER|FDF_STRUCT|FDF_R, nullptr, nullptr, "NetClient" },
   { "ClientData",  FDF_POINTER|FDF_R },
   { "Outgoing",    FDF_FUNCTION|FDF_R },
   { "Incoming",    FDF_FUNCTION|FDF_R },
   // Virtual fields
//   { "Handle", FDF_INT|FDF_R|FDF_VIRTUAL, GET_ClientHandle, SET_ClientHandle },
   END_FIELD
};

//********************************************************************************************************************

static ERR init_clientsocket(void)
{
   clClientSocket = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::CLIENTSOCKET),
      fl::ClassVersion(1.0),
      fl::Name("ClientSocket"),
      fl::Category(CCF::NETWORK),
      fl::Actions(clClientSocketActions),
      fl::Fields(clClientSocketFields),
      fl::Size(sizeof(extClientSocket)),
      fl::Path(MOD_PATH));

   return clClientSocket ? ERR::Okay : ERR::AddClass;
}
