
/*********************************************************************************************************************
** See win32_netresponse() for the Windows version.
*/

#ifdef __linux__
static void client_connect(SOCKET_HANDLE Void, APTR Data)
{
   pf::Log log(__FUNCTION__);
   auto Self = (extNetSocket *)Data;
   
   pf::SwitchContext context(Self);

   log.trace("Connection from server received.");

   int result = EHOSTUNREACH; // Default error in case getsockopt() fails
   socklen_t optlen = sizeof(result);
   getsockopt(Self->SocketHandle, SOL_SOCKET, SO_ERROR, &result, &optlen);

   // Remove the write callback

   RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD::WRITE|RFD::REMOVE, &client_connect, nullptr);

   #ifndef DISABLE_SSL
   if ((Self->ssl_handle) and (!result)) {
      // Perform the SSL handshake

      log.traceBranch("Attempting SSL handshake.");

      sslConnect(Self);
      if (Self->Error != ERR::Okay) return;

      if (Self->State IS NTC::CONNECTING_SSL) {
         RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&client_server_incoming), Self);
      }
      return;
   }
   #endif

   if (!result) {
      log.traceBranch("Connection succesful.");
      Self->setState(NTC::CONNECTED);
      RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&client_server_incoming), Self);
      return;
   }
   else {
      log.trace("getsockopt() result %d", result);

      if (result IS ECONNREFUSED)      Self->Error = ERR::ConnectionRefused;
      else if (result IS ENETUNREACH)  Self->Error = ERR::NetworkUnreachable;
      else if (result IS EHOSTUNREACH) Self->Error = ERR::HostUnreachable;
      else if (result IS ETIMEDOUT)    Self->Error = ERR::TimeOut;
      else Self->Error = ERR::Failed;

      log.error(Self->Error);

      Self->setState(NTC::DISCONNECTED);
   }
}
#endif

//********************************************************************************************************************
// If the socket is the client of a server, messages from the server will come in through here.
//
// Incoming information from the server can be read with either the Incoming callback routine (the developer is
// expected to call the Read action from this) or he can receive the information in the Subscriber's data channel.
//
// This function is called from win32_netresponse() and is managed outside of the normal message queue.

static void client_server_incoming(SOCKET_HANDLE FD, extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   pf::SwitchContext context(Self); // Set context & lock

   if (Self->Terminating) { // Set by FreeWarning()
      log.trace("[NetSocket:%d] Socket terminating...", Self->UID);
      if (Self->SocketHandle != NOHANDLE) free_socket(Self);
      return;
   }

#ifndef DISABLE_SSL
  #ifdef _WIN32
    if ((Self->WinSSL) and (Self->State IS NTC::CONNECTING_SSL)) {
      log.trace("Windows SSL handshake in progress, reading raw data.");
      std::array<char, 4096> buffer;
      int result;
      ERR error = WIN_RECEIVE(Self->SocketHandle, buffer.data(), buffer.size(), 0, &result);
      if ((error IS ERR::Okay) and (result > 0)) {
         sslHandshakeReceived(Self, buffer.data(), result);
      }
      return;
    }
  #else
    if ((Self->ssl_handle) and (Self->State IS NTC::CONNECTING_SSL)) {
      log.traceBranch("Continuing SSL handshake...");
      if ((Self->Flags & NSF::SERVER) != NSF::NIL) {
         sslAccept(Self);  // Server-side SSL handshake
      } 
      else sslConnect(Self); // Client-side SSL handshake
      return;
    }

    if (Self->SSLBusy) {
      log.trace("SSL object is busy.");
      return; // SSL object is performing a background operation (e.g. handshake)
    }
  #endif
#endif

   if (Self->IncomingRecursion) {
      log.trace("[NetSocket:%d] Recursion detected on handle %" PF64, Self->UID, (MAXINT)FD);
      if (Self->IncomingRecursion < 2) Self->IncomingRecursion++; // Indicate that there is more data to be received
      return;
   }

   log.traceBranch("[NetSocket:%d] Socket: %" PF64, Self->UID, (MAXINT)FD);

   Self->InUse++;
   Self->IncomingRecursion++;

restart:

   Self->ReadCalled = false;

   int result;
   auto error = ERR::Okay;
   if (Self->Incoming.defined()) {
      if (Self->Incoming.isC()) {
         auto routine = (ERR (*)(extNetSocket *, APTR))Self->Incoming.Routine;
         pf::SwitchContext context(Self->Incoming.Context);
         error = routine(Self, Self->Incoming.Meta);
      }
      else if (Self->Incoming.isScript()) {
         if (sc::Call(Self->Incoming, std::to_array<ScriptArg>({ { "NetSocket", Self, FD_OBJECTPTR } }), error) != ERR::Okay) error = ERR::Terminate;
      }

      if (error IS ERR::Terminate) log.trace("Termination of socket requested by channel subscriber.");
      else if (!Self->ReadCalled) log.warning("[NetSocket:%d] Subscriber did not call Read()", Self->UID);
   }

   if (!Self->ReadCalled) {
      std::array<char,512> buffer;
      int total = 0;

      do {
         error = RECEIVE(Self, Self->SocketHandle, buffer.data(), buffer.size(), 0, &result);
         total += result;
      } while (result > 0);

      if (error != ERR::Okay) error = ERR::Terminate;
   }

   if (error IS ERR::Terminate) {
      log.traceBranch("Socket %d will be terminated.", FD);
      if (Self->SocketHandle != NOHANDLE) free_socket(Self);
   }
   else if (Self->IncomingRecursion > 1) {
      // If client_server_incoming() was called again during the callback, there is more
      // data available and we should repeat our callback so that the client can receive the rest
      // of the data.

      Self->IncomingRecursion = 1;
      goto restart;
   }

   Self->InUse--;
   Self->IncomingRecursion = 0;
}

//********************************************************************************************************************
// This function sends data to the client if there is queued data waiting to go out.  Otherwise it does nothing.
//
// Note: This function will prevent the task from going to sleep if it is not managed correctly.  If
// no data is being written to the queue, the program will not be able to sleep until the client stops listening
// to the write queue.

static void client_server_outgoing(SOCKET_HANDLE Void, extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);
   
   pf::SwitchContext context(Self); // Set context & lock

   if (Self->Terminating) return;

#ifndef DISABLE_SSL
  #ifdef _WIN32
    if ((Self->WinSSL) and (Self->State IS NTC::CONNECTING_SSL)) {
      log.trace("Still connecting via SSL...");
      return;
    }
  #else
    if ((Self->ssl_handle) and (Self->State IS NTC::CONNECTING_SSL)) {
      log.trace("Still connecting via SSL...");
      return;
    }
  #endif
#endif

   if (Self->OutgoingRecursion) {
      log.trace("Recursion detected.");
      return;
   }

   log.traceBranch();

#ifndef DISABLE_SSL
  #ifndef _WIN32
    if (Self->SSLBusy) return; // SSL object is performing a background operation (e.g. handshake)
  #endif
#endif

   Self->InUse++;
   Self->OutgoingRecursion++;

   auto error = ERR::Okay;

   // Send out remaining queued data before getting new data to send

   while (!Self->WriteQueue.Buffer.empty()) {
      size_t len = Self->WriteQueue.Buffer.size() - Self->WriteQueue.Index;
      #ifndef DISABLE_SSL
         #ifdef _WIN32
            if ((!Self->WinSSL) and (len > glMaxWriteLen)) len = glMaxWriteLen;
         #else
            if ((!Self->ssl_handle) and (len > glMaxWriteLen)) len = glMaxWriteLen;
         #endif
      #else
         if (len > glMaxWriteLen) len = glMaxWriteLen;
      #endif

      if (len > 0) {
         error = SEND(Self, Self->SocketHandle, Self->WriteQueue.Buffer.data() + Self->WriteQueue.Index, &len, 0);
         if ((error != ERR::Okay) or (!len)) break;
         log.trace("[NetSocket:%d] Sent %d of %d bytes remaining on the queue.", Self->UID, int(len), int(Self->WriteQueue.Buffer.size() - Self->WriteQueue.Index));
         Self->WriteQueue.Index += len;
      }

      if (Self->WriteQueue.Index >= Self->WriteQueue.Buffer.size()) {
         Self->WriteQueue.Buffer.clear();
         Self->WriteQueue.Index = 0;
         break;
      }
   }

   // Before feeding new data into the queue, the current buffer must be empty.

   if ((Self->WriteQueue.Buffer.empty()) or 
       (Self->WriteQueue.Index >= Self->WriteQueue.Buffer.size())) {
      if (Self->Outgoing.defined()) {
         if (Self->Outgoing.isC()) {
            auto routine = (ERR (*)(extNetSocket *, APTR))Self->Outgoing.Routine;
            pf::SwitchContext context(Self->Outgoing.Context);
            error = routine(Self, Self->Outgoing.Meta);
         }
         else if (Self->Outgoing.isScript()) {
            if (sc::Call(Self->Outgoing, std::to_array<ScriptArg>({ { "NetSocket", Self, FD_OBJECTPTR } }), error) != ERR::Okay) error = ERR::Terminate;
         }

         if (error != ERR::Okay) Self->Outgoing.clear();
      }

      // If the write queue is empty and all data has been retrieved, we can remove the FD-Write registration so that
      // we don't tax the system resources.  The WriteSocket function is also dropped because it is intended to
      // be assigned temporarily.

      if ((!Self->Outgoing.defined()) and (Self->WriteQueue.Buffer.empty())) {
         log.trace("[NetSocket:%d] Write-queue listening on FD %d will now stop.", Self->UID, Self->SocketHandle);
         #ifdef __linux__
            RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD::REMOVE|RFD::WRITE|RFD::SOCKET, nullptr, nullptr);
         #elif _WIN32
            if (auto error = win_socketstate(Self->SocketHandle, -1, 0); error != ERR::Okay) log.warning(error);
            Self->WriteSocket = nullptr;
         #endif
      }
   }

   Self->InUse--;
   Self->OutgoingRecursion--;
}
