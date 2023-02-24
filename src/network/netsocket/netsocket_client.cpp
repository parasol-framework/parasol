
/*********************************************************************************************************************
** See win32_netresponse() for the Windows version.
*/

#ifdef __linux__
static void client_connect(SOCKET_HANDLE Void, APTR Data)
{
   pf::Log log(__FUNCTION__);
   extNetSocket *Self = (extNetSocket *)Data;

   log.trace("Connection from server received.");

   LONG result = EHOSTUNREACH; // Default error in case getsockopt() fails
   socklen_t optlen = sizeof(result);
   getsockopt(Self->SocketHandle, SOL_SOCKET, SO_ERROR, &result, &optlen);

   pf::SwitchContext context(Self);

   // Remove the write callback

   RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD_WRITE|RFD_REMOVE, &client_connect, NULL);

   #ifdef ENABLE_SSL
   if ((Self->SSL) and (!result)) {
      // Perform the SSL handshake

      log.traceBranch("Attempting SSL handshake.");

      sslConnect(Self);
      if (Self->Error) return;

      if (Self->State IS NTC_CONNECTING_SSL) {
         RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD_READ|RFD_SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&client_server_incoming), Self);
      }
      return;
   }
   #endif

   if (!result) {
      log.traceBranch("Connection succesful.");
      Self->set(FID_State, NTC_CONNECTED);
      RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD_READ|RFD_SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&client_server_incoming), Self);
      return;
   }
   else {
      FMSG("client_connect","getsockopt() result %d", result);

      if (result IS ECONNREFUSED)      Self->Error = ERR_ConnectionRefused;
      else if (result IS ENETUNREACH)  Self->Error = ERR_NetworkUnreachable;
      else if (result IS EHOSTUNREACH) Self->Error = ERR_HostUnreachable;
      else if (result IS ETIMEDOUT)    Self->Error = ERR_TimeOut;
      else Self->Error = ERR_Failed;

      log.error(Self->Error);

      Self->set(FID_State, NTC_DISCONNECTED);
   }
}
#endif

/*********************************************************************************************************************
** If the socket is the client of a server, messages from the server will come in through here.
**
** Incoming information from the server can be read with either the Incoming callback routine (the developer is
** expected to call the Read action from this) or he can receive the information in the Subscriber's data channel.
**
** This function is called from win32_netresponse() and is managed outside of the normal message queue.
*/

static void client_server_incoming(SOCKET_HANDLE FD, extNetSocket *Data)
{
   pf::Log log(__FUNCTION__);
   extNetSocket *Self = Data;

   pf::SwitchContext context(Self);

   if (Self->Terminating) {
      log.trace("[NetSocket:%d] Socket terminating...", Self->UID);
      if (Self->SocketHandle != NOHANDLE) free_socket(Self);
      return;
   }

#ifdef ENABLE_SSL
   if ((Self->SSL) and (Self->State IS NTC_CONNECTING_SSL)) {
      log.traceBranch("Continuing SSL communication...");
      sslConnect(Self);
      return;
   }

   if (Self->SSLBusy) {
      log.trace("SSL object is busy.");
      return; // SSL object is performing a background operation (e.g. handshake)
   }
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

   Self->ReadCalled = FALSE;

   LONG result;
   ERROR error = ERR_Okay;
   if (Self->Incoming.Type) {
      if (Self->Incoming.Type IS CALL_STDC) {
         auto routine = (ERROR (*)(extNetSocket *))Self->Incoming.StdC.Routine;
         if (routine) {
            pf::SwitchContext context(Self->Incoming.StdC.Context);
            error = routine(Self);
         }
      }
      else if (Self->Incoming.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         const ScriptArg args[] = { { "NetSocket", FD_OBJECTPTR, { .Address = Self } } };

         if ((script = Self->Incoming.Script.Script)) {
            if (scCallback(script, Self->Incoming.Script.ProcedureID, args, ARRAYSIZE(args), &error)) error = ERR_Terminate;
         }
      }

      if (error IS ERR_Terminate) log.trace("Termination of socket requested by channel subscriber.");
      else if (!Self->ReadCalled) log.warning("[NetSocket:%d] Subscriber did not call Read()", Self->UID);
   }

   if (!Self->ReadCalled) {
      char buffer[80];
      LONG total = 0;

      do {
         error = RECEIVE(Self, Self->SocketHandle, &buffer, sizeof(buffer), 0, &result);
         total += result;
      } while (result > 0);

      if (error) error = ERR_Terminate;
   }

   if (error IS ERR_Terminate) {
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

/*********************************************************************************************************************
** If the socket refers to a client, this routine will be called when there is empty space available on the socket
** for writing data to the server.
**
** It should be noted that this function will prevent the task from going to sleep if it is not managed correctly.  If
** no data is being written to the queue, the program will not be able to sleep until the client stops listening
** to the write queue.
*/

static void client_server_outgoing(SOCKET_HANDLE Void, extNetSocket *Data)
{
   pf::Log log(__FUNCTION__);
   extNetSocket *Self = Data;

   if (Self->Terminating) return;

#ifdef ENABLE_SSL
   if ((Self->SSL) and (Self->State IS NTC_CONNECTING_SSL)) {
      log.trace("Still connecting via SSL...");
      return;
   }
#endif

   if (Self->OutgoingRecursion) {
      log.trace("Recursion detected.");
      return;
   }

   pf::SwitchContext context(Self);

   log.traceBranch("");

#ifdef ENABLE_SSL
   if (Self->SSLBusy) return; // SSL object is performing a background operation (e.g. handshake)
#endif

   Self->InUse++;
   Self->OutgoingRecursion++;

   ERROR error = ERR_Okay;

   // Send out remaining queued data before getting new data to send

   if (Self->WriteQueue.Buffer) {
      while (Self->WriteQueue.Buffer) {
         LONG len = Self->WriteQueue.Length - Self->WriteQueue.Index;
         #ifdef ENABLE_SSL
         if ((!Self->SSL) and (len > glMaxWriteLen)) len = glMaxWriteLen;
         #else
         if (len > glMaxWriteLen) len = glMaxWriteLen;
         #endif

         if (len > 0) {
            error = SEND(Self, Self->SocketHandle, (BYTE *)Self->WriteQueue.Buffer + Self->WriteQueue.Index, &len, 0);
            if ((error) or (!len)) break;
            log.trace("[NetSocket:%d] Sent %d of %d bytes remaining on the queue.", Self->UID, len, Self->WriteQueue.Length-Self->WriteQueue.Index);
            Self->WriteQueue.Index += len;
         }

         if (Self->WriteQueue.Index >= Self->WriteQueue.Length) {
            log.trace("Freeing the write queue (pos %d/%d).", Self->WriteQueue.Index, Self->WriteQueue.Length);
            FreeResource(Self->WriteQueue.Buffer);
            Self->WriteQueue.Buffer = NULL;
            Self->WriteQueue.Index = 0;
            Self->WriteQueue.Length = 0;
            break;
         }
      }
   }

   // Before feeding new data into the queue, the current buffer must be empty.

   if ((!Self->WriteQueue.Buffer) or (Self->WriteQueue.Index >= Self->WriteQueue.Length)) {
      if (Self->Outgoing.Type) {
         if (Self->Outgoing.Type IS CALL_STDC) {
            auto routine = (ERROR (*)(extNetSocket *))Self->Outgoing.StdC.Routine;
            if (routine) {
               pf::SwitchContext context(Self->Outgoing.StdC.Context);
               error = routine(Self);
            }
         }
         else if (Self->Outgoing.Type IS CALL_SCRIPT) {
            OBJECTPTR script;
            const ScriptArg args[] = { { "NetSocket", FD_OBJECTPTR, { .Address = Self } } };
            if ((script = Self->Outgoing.Script.Script)) {
               if (scCallback(script, Self->Outgoing.Script.ProcedureID, args, ARRAYSIZE(args), &error)) error = ERR_Terminate;
            }
         }

         if (error) Self->Outgoing.Type = CALL_NONE;
      }

      // If the write queue is empty and all data has been retrieved, we can remove the FD-Write registration so that
      // we don't tax the system resources.

      if ((Self->Outgoing.Type IS CALL_NONE) and (!Self->WriteQueue.Buffer)) {
         log.trace("[NetSocket:%d] Write-queue listening on FD %d will now stop.", Self->UID, Self->SocketHandle);
         #ifdef __linux__
            RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD_REMOVE|RFD_WRITE|RFD_SOCKET, NULL, NULL);
         #elif _WIN32
            win_socketstate(Self->SocketHandle, -1, 0);
            Self->WriteSocket = NULL;
         #endif
      }
   }

   Self->InUse--;
   Self->OutgoingRecursion--;
}
