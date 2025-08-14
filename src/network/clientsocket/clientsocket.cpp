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

//********************************************************************************************************************
// Forward declaration of template function from network.cpp

template<typename T>
static ERR send_data(T *Self, CPTR Buffer, size_t *Length);

//********************************************************************************************************************
// Read function specifically for ClientSocket connections

static ERR receive_from_client(extClientSocket *Self, APTR Buffer, size_t BufferSize, size_t *Result)
{
   pf::Log log(__FUNCTION__);

   if (!BufferSize) return ERR::Okay;
   
#ifndef DISABLE_SSL
   if (Self->SSLHandle) {
   #ifdef _WIN32
       // If we're in the middle of SSL handshake, read raw data for handshake processing
       if (Self->State IS NTC::HANDSHAKING) {
          log.trace("Windows SSL handshake in progress, reading raw data.");
          ERR error = WIN_RECEIVE(Self->Handle, Buffer, BufferSize, 0, Result);
          if ((error IS ERR::Okay) and (*Result > 0)) {
             sslHandshakeReceived(Self, Buffer, *Result);
          }
          return error;
       }
       else { // Normal SSL data read for established connections
          if (auto result = ssl_wrapper_read(Self->SSLHandle, Buffer, BufferSize); result > 0) {
             *Result = result;
             return ERR::Okay;
          }
          else if (!result) {
             return ERR::Disconnected;
          }
          else {
             CSTRING msg;
             auto error = ssl_wrapper_get_error(Self->SSLHandle, &msg);
             if (error IS SSL_ERROR_WOULD_BLOCK) {
                log.traceWarning("No more data to read from the SSL socket.");
                return ERR::Okay;
             }
             else {
                log.warning("Windows SSL read error: %s", msg);
                return ERR::Failed;
             }
          }
       }
   #else // OpenSSL
      bool read_blocked;
      int pending;

      if (Self->SSLBusy IS SSL_HANDSHAKE_WRITE) ssl_handshake_write(Self->Handle, Self);
      else if (Self->SSLBusy IS SSL_HANDSHAKE_READ) ssl_handshake_read(Self->Handle, Self);

      if (Self->SSLBusy != SSL_NOT_BUSY) return ERR::Okay;

      log.traceBranch("BufferSize: %d", int(BufferSize));
      
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
                  return ERR::Okay; // No data available yet

               case SSL_ERROR_WANT_WRITE:
                  // WANT_WRITE is returned if we're trying to rehandshake and the write operation would block.  We
                  // need to wait on the socket to be writeable, then restart the read when it is.

                  log.msg("SSL socket handshake requested by server.");
                  Self->SSLBusy = SSL_HANDSHAKE_WRITE;
                  RegisterFD((HOSTHANDLE)Self->Handle, RFD::WRITE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(ssl_handshake_write<extClientSocket>), Self);
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

         RegisterFD((HOSTHANDLE)Self->Handle, RFD::RECALL|RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&server_incoming_from_client), Self);
      }

      return ERR::Okay;
   #endif
   }
#endif // DISABLE_SSL
      
#ifdef __linux__
   {
      int result = recv(Self->Handle, Buffer, BufferSize, 0);

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
   }
#elif _WIN32
   return WIN_RECEIVE(Self->Handle, Buffer, BufferSize, 0, Result);
#else
   #error No support for RECEIVE()
#endif
}

//********************************************************************************************************************
// Data has arrived from a client's socket handle.

static void server_incoming_from_client(HOSTHANDLE Handle, extClientSocket *client)
{
   pf::Log log(__FUNCTION__);
   if (!client->Client) return;
   auto Socket = (extNetSocket *)(client->Client->Owner);

   if (client->Handle IS NOHANDLE) {
      log.warning("Invalid state - socket closed but receiving data.");
      return;
   }

#ifndef DISABLE_SSL
   #ifdef _WIN32
     // TODO?
   #else
      if (client->State IS NTC::HANDSHAKING) {
         // Continue SSL handshake for this ClientSocket
         auto result = SSL_accept(client->SSLHandle);
         if (result == 1) {
            log.msg("SSL handshake completed for client %d", client->UID);
            client->setState(NTC::CONNECTED);
         }
         else {
            auto ssl_error = SSL_get_error(client->SSLHandle, result);
            if ((ssl_error == SSL_ERROR_WANT_READ) or (ssl_error == SSL_ERROR_WANT_WRITE)) {
               log.trace("SSL handshake continuing for client %d...", client->UID);
               // Handshake will continue on next data arrival
            }
            else {
               log.warning("SSL handshake failed for client %d: %s", client->UID, ERR_error_string(ssl_error, nullptr));
               client->setState(NTC::DISCONNECTED);
            }
         }
         return;
      }
   #endif
#endif

   Socket->InUse++;
   client->ReadCalled = false;

   log.traceBranch("Handle: %" PF64 ", Socket: %d, Client: %d", (LARGE)(MAXINT)Handle, Socket->UID, client->UID);

   ERR error = ERR::Okay;
   if (Socket->Incoming.defined()) {
      if (Socket->Incoming.isC()) {
         pf::SwitchContext context(Socket->Incoming.Context);
         auto routine = (ERR (*)(extNetSocket *, extClientSocket *, APTR))Socket->Incoming.Routine;
         error = routine(Socket, client, Socket->Incoming.Meta);
      }
      else if (Socket->Incoming.isScript()) {
         if (sc::Call(Socket->Incoming, std::to_array<ScriptArg>({
               { "NetSocket",    Socket, FD_OBJECTPTR },
               { "ClientSocket", client, FD_OBJECTPTR }
            }), error) != ERR::Okay) error = ERR::Terminate;
         if (error IS ERR::Exception) error = ERR::Terminate; // assert() and error() are taken seriously
      }
      else error = ERR::InvalidValue;
   }
   else log.traceWarning("No Incoming callback configured.");

   if (!client->ReadCalled) error = ERR::Terminate;

   if (error IS ERR::Terminate) {
      log.trace("Terminating socket, failed to read incoming data.");
      FreeResource(client); // Disconnect & send Feedback message
   }

   Socket->InUse--;
}

//********************************************************************************************************************
// Note that this function will prevent the task from going to sleep if it is not managed correctly.  If
// no data is being written to the queue, the program will not be able to sleep until the client stops listening
// to the write queue.

static void clientsocket_outgoing(HOSTHANDLE Void, extClientSocket *ClientSocket)
{
   pf::Log log(__FUNCTION__);
   auto Socket = (extNetSocket *)(ClientSocket->Client->Owner);

   if (Socket->Terminating) return;

#ifndef DISABLE_SSL
  #ifdef _WIN32
    if ((ClientSocket->SSLHandle) and (ClientSocket->State IS NTC::HANDSHAKING)) {
      log.trace("Still connecting via SSL...");
      return;
    }
  #else
    if ((ClientSocket->SSLHandle) and (ClientSocket->State IS NTC::HANDSHAKING)) {
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

#ifndef DISABLE_SSL
  #ifndef _WIN32
    if (ClientSocket->SSLBusy) return; // SSL object is performing a background operation (e.g. handshake)
  #endif
#endif

   ClientSocket->InUse++;
   ClientSocket->OutgoingRecursion++;

   auto error = ERR::Okay;

   // Send out remaining queued data before getting new data to send

   while (!ClientSocket->WriteQueue.Buffer.empty()) {
      size_t len = ClientSocket->WriteQueue.Buffer.size() - ClientSocket->WriteQueue.Index;
      #ifndef DISABLE_SSL
         #ifdef _WIN32
            if ((!ClientSocket->SSLHandle) and (len > glMaxWriteLen)) len = glMaxWriteLen;
         #else
            if ((!ClientSocket->SSLHandle) and (len > glMaxWriteLen)) len = glMaxWriteLen;
         #endif
      #else
         if (len > glMaxWriteLen) len = glMaxWriteLen;
      #endif

      if (len > 0) {
         error = send_data(ClientSocket, ClientSocket->WriteQueue.Buffer.data() + ClientSocket->WriteQueue.Index, &len);
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
         log.trace("[NetSocket:%d] Write-queue listening on FD %d will now stop.", Socket->UID, ClientSocket->Handle);
         #ifdef __linux__
            RegisterFD((HOSTHANDLE)ClientSocket->Handle, RFD::REMOVE|RFD::WRITE|RFD::SOCKET, nullptr, nullptr);
         #elif _WIN32
            win_socketstate(ClientSocket->Handle, -1, 0);
         #endif
      }
   }

   ClientSocket->InUse--;
   ClientSocket->OutgoingRecursion--;
}

//********************************************************************************************************************
// Disconnect a client socket and report it through the NetSocket server.

static void disconnect(extClientSocket *Self)
{
   pf::Log log(__FUNCTION__);

   if (Self->Handle) {
      log.branch("Disconnecting socket handle %d", Self->Handle);

#ifdef __linux__
      DeregisterFD(Self->Handle);
#endif
      CLOSESOCKET_THREADED(Self->Handle);
      Self->Handle = -1;
   }

   auto owner = (extNetSocket *)Self->Owner;
   if ((owner) and (owner->classID() IS CLASSID::NETSOCKET)) {
      if (owner->Feedback.defined()) {
         log.traceBranch("Reporting client disconnection to NetSocket %p.", owner);

         if (owner->Feedback.isC()) {
            pf::SwitchContext context(owner->Feedback.Context);
            auto routine = (void (*)(extNetSocket *, objClientSocket *, NTC, APTR))owner->Feedback.Routine;
            if (routine) routine(owner, Self, NTC::DISCONNECTED, owner->Feedback.Meta);
         }
         else if (owner->Feedback.isScript()) {
            sc::Call(owner->Feedback, std::to_array<ScriptArg>({
               { "NetSocket",    owner, FD_OBJECTPTR },
               { "ClientSocket", APTR(Self), FD_OBJECTPTR },
               { "State",        int(NTC::DISCONNECTED) }
            }));
         }
      }
   }
}

//********************************************************************************************************************

static ERR CLIENTSOCKET_Free(extClientSocket *Self)
{
   pf::Log log;

#ifndef DISABLE_SSL
   sslDisconnect(Self);
#endif

   disconnect(Self);

   if (Self->Client) { // If undefined, ClientSocket was never initialised
      pf::ScopedObjectLock lock(Self->Client);
      if (lock.granted()) {
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
            free_client((extNetSocket *)Self->Client->Owner, Self->Client);
         }
      }
   }

   Self->~extClientSocket();
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR CLIENTSOCKET_Init(extClientSocket *Self)
{
   pf::Log log;
   if (!Self->Client) return log.warning(ERR::FieldNotSet);

   pf::ScopedObjectLock lock(Self->Client);
   if (!lock.granted()) return ERR::Lock;

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

#ifndef DISABLE_SSL
   // If the parent NetSocket is an SSL server, set up SSL for this client socket
   #ifdef _WIN32
      // Not supported here
   #else
      auto netSocket = (extNetSocket *)(Self->Client->Owner);
      if (((netSocket->Flags & NSF::SSL) != NSF::NIL) and ((netSocket->Flags & NSF::SERVER) != NSF::NIL) and netSocket->CTX) {
         if (auto client_ssl = SSL_new(netSocket->CTX)) {
            if (auto client_bio = BIO_new_socket(Self->Handle, BIO_NOCLOSE)) {
               SSL_set_bio(client_ssl, client_bio, client_bio);

               // Store SSL handles in the ClientSocket
               Self->SSLHandle = client_ssl;
               Self->BIOHandle = client_bio;

               Self->setState(NTC::HANDSHAKING);

               auto result = SSL_accept(client_ssl);
               if (result == 1) {
                  // SSL handshake completed successfully
                  log.msg("SSL client handshake completed successfully.");
                  Self->setState(NTC::CONNECTED);
               }
               else {
                  auto ssl_error = SSL_get_error(client_ssl, result);
                  if ((ssl_error == SSL_ERROR_WANT_READ) or (ssl_error == SSL_ERROR_WANT_WRITE)) {
                     log.msg("SSL handshake in progress...");
                     // Handshake will continue asynchronously
                  }
                  else {
                     log.warning("SSL client handshake failed: %s", ERR_error_string(ssl_error, nullptr));
                     Self->SSLHandle = nullptr;
                     Self->BIOHandle = nullptr;
                     SSL_free(client_ssl);
                     return ERR::SystemCall;
                  }
               }
            }
            else {
               SSL_free(client_ssl);
               return log.warning(ERR::SystemCall);
            }
         }
         else return log.warning(ERR::SystemCall);
      }
   #endif
#endif

#ifdef __linux__
   RegisterFD(Self->Handle, RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&server_incoming_from_client), Self);
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
   if (Self->Handle IS NOHANDLE) {
      // Lack of a handle means that disconnection has already been processed, so the client code
      // shouldn't be calling us (client probably needs to be plugged into the feedback mechanisms)
      return log.warning(ERR::Disconnected);
   }
   Self->ReadCalled = true;
   if (!Args->Length) { Args->Result = 0; return ERR::Okay; }

   size_t result = 0;
   auto error = receive_from_client(Self, Args->Buffer, Args->Length, &result);
   Args->Result = result;

   if (error IS ERR::Disconnected) {
      // Detecting a disconnection on read is normal, now handle disconnection gracefully.
      log.msg("Client disconnection detected.");
      disconnect(Self);
   }
   return error;
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
   if (Self->Handle IS NOHANDLE) return log.error(ERR::Disconnected);

   size_t len = Args->Length;
   ERR error = send_data(Self, Args->Buffer, &len);

   if ((error != ERR::Okay) or (len < size_t(Args->Length))) {
      if (error != ERR::Okay) log.trace("send_to_server() Error: '%s', queuing %d/%d bytes for transfer...", GetErrorMsg(error), Args->Length - len, Args->Length);
      else log.trace("Queuing %d of %d remaining bytes for transfer...", Args->Length - len, Args->Length);
      if ((error IS ERR::DataSize) or (error IS ERR::BufferOverflow) or (len > 0))  {
         Self->WriteQueue.write((BYTE *)Args->Buffer + len, Args->Length - len);
         #ifdef __linux__
            RegisterFD((HOSTHANDLE)Self->Handle, RFD::WRITE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&clientsocket_outgoing), Self);
         #elif _WIN32
            win_socketstate(Self->Handle, -1, true);
         #endif
      }
   }
   else log.trace("Successfully wrote all %d bytes to the server.", Args->Length);

   Args->Result = Args->Length;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Client: Parent client object (IP address).

-FIELD-
ClientData: Available for client data storage.

-FIELD-
ConnectTime: System time for the creation of this socket.

-FIELD-
Incoming: Callback for data being received from the client.

-FIELD-
Next: Next socket in the chain. 

-FIELD-
Outgoing: Callback for data ready to be sent to the client.

-FIELD-
Prev: Previous socket in the chain.

-FIELD-
State: The current connection state of the ClientSocket object.

The State reflects the connection state of the NetSocket.  If the #Feedback field is defined with a function, it will
be called automatically whenever the state is changed.  Note that the ClientSocket parameter will be NULL when the 
Feedback function is called.

Note that in server mode this State value should not be used as it cannot reflect the state of all connected
client sockets.  Each @ClientSocket carries its own independent State value for use instead.

*********************************************************************************************************************/

static ERR CS_SET_State(extClientSocket *Self, NTC Value)
{
   pf::Log log;

   if (Value != Self->State) {
      auto Socket = (extNetSocket *)(Self->Client->Owner);

      if ((Socket->Flags & NSF::LOG_ALL) != NSF::NIL) log.msg("State changed from %d to %d", int(Self->State), int(Value));

      #ifndef DISABLE_SSL
      if ((Self->State IS NTC::HANDSHAKING) and (Value IS NTC::CONNECTED)) {
         // SSL connection has just been established

         bool ssl_valid = true;

         #ifdef _WIN32
         if ((Self->SSLHandle) and ((Socket->Flags & NSF::SERVER) IS NSF::NIL)) {
            // Only perform certificate validation if SSL_NO_VERIFY flag is not set
            if ((Socket->Flags & NSF::SSL_NO_VERIFY) != NSF::NIL) {
               log.trace("SSL certificate validation skipped (SSL_NO_VERIFY flag set).");
            }
            else {
               if (ssl_wrapper_get_verify_result(Self->SSLHandle) != 0) ssl_valid = false;
               else log.trace("SSL certificate validation successful.");
            }
         }
         #else
         if (Self->SSLHandle) {
            // Only perform certificate validation if SSL_NO_VERIFY flag is not set
            if ((Socket->Flags & NSF::SSL_NO_VERIFY) != NSF::NIL) {
               log.trace("SSL certificate validation skipped (SSL_NO_VERIFY flag set).");
            }
            else {
               if (SSL_get_verify_result(Self->SSLHandle) != X509_V_OK) ssl_valid = false;
               else log.trace("SSL certificate validation successful.");
            }
         }
         #endif
         
         if (!ssl_valid) {
            log.warning("SSL certificate validation failed.");
            Self->Error = ERR::Security;
            Self->State = NTC::DISCONNECTED;
            if (Socket->Feedback.defined()) {
               if (Socket->Feedback.isC()) {
                  pf::SwitchContext context(Socket->Feedback.Context);
                  auto routine = (void (*)(extNetSocket *, objClientSocket *, NTC, APTR))Socket->Feedback.Routine;
                  if (routine) routine(Socket, Self, NTC::DISCONNECTED, Socket->Feedback.Meta);
               }
               else if (Socket->Feedback.isScript()) {
                  sc::Call(Socket->Feedback, std::to_array<ScriptArg>({
                     { "NetSocket", Socket, FD_OBJECTPTR },
                     { "ClientSocket", OBJECTPTR(Self), FD_OBJECTPTR },
                     { "State", int(NTC::DISCONNECTED) }
                  }));
               }
            }
            return ERR::Security;
         }
      }
      #endif

      Self->State = Value;

      if (Socket->Feedback.defined()) {
         log.traceBranch("Reporting state change to subscriber, operation %d, context %p.", int(Self->State), Socket->Feedback.Context);

         if (Socket->Feedback.isC()) {
            pf::SwitchContext context(Socket->Feedback.Context);
            auto routine = (void (*)(extNetSocket *, objClientSocket *, NTC, APTR))Socket->Feedback.Routine;
            if (routine) routine(Socket, Self, Self->State, Socket->Feedback.Meta);
         }
         else if (Socket->Feedback.isScript()) {
            sc::Call(Socket->Feedback, std::to_array<ScriptArg>({
               { "NetSocket",    Socket, FD_OBJECTPTR },
               { "ClientSocket", OBJECTPTR(Self), FD_OBJECTPTR },
               { "State",        int(Self->State) }
            }));
         }
      }

      if ((Self->State IS NTC::CONNECTED) and ((!Self->WriteQueue.Buffer.empty()) or (Self->Outgoing.defined()))) {
         log.msg("Sending queued data to server on connection.");
         #ifdef __linux__
            RegisterFD((HOSTHANDLE)Self->Handle, RFD::WRITE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&clientsocket_outgoing), Self);
         #elif _WIN32
            win_socketstate(Self->Handle, -1, true);
         #endif
      }
   }

   SetResourcePtr(RES::EXCEPTION_HANDLER, nullptr); // Stop winsock from fooling with our exception handler

   return ERR::Okay;
}

//********************************************************************************************************************

#include "clientsocket_def.c"

static const FieldArray clClientSocketFields[] = {
   { "ConnectTime", FDF_INT64|FDF_R },
   { "Prev",        FDF_OBJECT|FDF_R, nullptr, nullptr, CLASSID::CLIENTSOCKET },
   { "Next",        FDF_OBJECT|FDF_R, nullptr, nullptr, CLASSID::CLIENTSOCKET },
   { "Client",      FDF_OBJECT|FDF_R, nullptr, nullptr, CLASSID::NETCLIENT },
   { "ClientData",  FDF_POINTER|FDF_R },
   { "Outgoing",    FDF_FUNCTION|FDF_R },
   { "Incoming",    FDF_FUNCTION|FDF_R },
   { "State",       FDF_INT|FDF_LOOKUP|FDF_RW, nullptr, CS_SET_State, &clNetSocketState },
   { "Error",       FDF_INT|FDF_R },
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
