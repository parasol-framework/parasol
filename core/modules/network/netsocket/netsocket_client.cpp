
/*****************************************************************************
** See win32_netresponse() for the Windows version.
*/

#ifdef __linux__
static void client_connect(SOCKET_HANDLE Void, APTR Data)
{
   objNetSocket *Self = (objNetSocket *)Data;

   FMSG("client_connect()","Connection from server received.");

   LONG result = EHOSTUNREACH; // Default error in case getsockopt() fails
   socklen_t optlen = sizeof(result);
   getsockopt(Self->SocketHandle, SOL_SOCKET, SO_ERROR, &result, &optlen);

   // Remove the write callback

   RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD_WRITE|RFD_REMOVE, &client_connect, NULL);

   #ifdef ENABLE_SSL
   if ((Self->SSL) AND (!result)) {
      // Perform the SSL handshake

      FMSG("~client_connect","Attempting SSL handshake.");

         sslConnect(Self);

      STEP();

      if (Self->Error) return;

      if (Self->State IS NTC_CONNECTING_SSL) {
         RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD_READ|RFD_SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&client_server_incoming), Self);
      }

      return;
   }
   #endif

   if (!result) {
      FMSG("~client_connect","Connection succesful.");

         SetLong(Self, FID_State, NTC_CONNECTED);
         RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD_READ|RFD_SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&client_server_incoming), Self);

      STEP();
      return;
   }
   else {
      FMSG("client_connect","getsockopt() result %d", result);

      if (result IS ECONNREFUSED)      Self->Error = ERR_ConnectionRefused;
      else if (result IS ENETUNREACH)  Self->Error = ERR_NetworkUnreachable;
      else if (result IS EHOSTUNREACH) Self->Error = ERR_HostUnreachable;
      else if (result IS ETIMEDOUT)    Self->Error = ERR_TimeOut;
      else Self->Error = ERR_Failed;

      PostError(Self->Error);

      SetLong(Self, FID_State, NTC_DISCONNECTED);
   }
}
#endif

/*****************************************************************************
** If the socket is the client of a server, messages from the server will come in through here.
**
** Incoming information from the server can be read with either the Incoming callback routine (the developer is
** expected to call the Read action from this) or he can receive the information in the Subscriber's data channel.
**
** This function is called from win32_netresponse() and is managed outside of the normal message queue.
*/

static void client_server_incoming(SOCKET_HANDLE FD, struct rkNetSocket *Data)
{
   objNetSocket *Self = Data;

   if (Self->Terminating) {
      FMSG("client_incoming","[NetSocket:%d] Socket terminating...", Self->Head.UniqueID);
      if (Self->SocketHandle != NOHANDLE) free_socket(Self);
      return;
   }

#ifdef ENABLE_SSL
   if ((Self->SSL) AND (Self->State IS NTC_CONNECTING_SSL)) {
      FMSG("~client_incoming","Continuing SSL communication...");
         sslConnect(Self);
      STEP();
      return;
   }

   if (Self->SSLBusy) {
      FMSG("client_incoming","SSL object is busy.");
      return; // SSL object is performing a background operation (e.g. handshake)
   }
#endif

   if (Self->IncomingRecursion) {
      FMSG("client_incoming","[NetSocket:%d] Recursion detected on handle %p.", Self->Head.UniqueID, FD);
      if (Self->IncomingRecursion < 2) Self->IncomingRecursion++; // Indicate that there is more data to be received
      return;
   }

   FMSG("~client_incoming()","[NetSocket:%d] Socket: %p", Self->Head.UniqueID, FD);

   Self->InUse++;
   Self->IncomingRecursion++;

restart:

   Self->ReadCalled = FALSE;

   LONG result;
   ERROR error = ERR_Okay;
   if (Self->Incoming.Type) {
      if (Self->Incoming.Type IS CALL_STDC) {
         ERROR (*routine)(objNetSocket *);
         if ((routine = reinterpret_cast<ERROR (*)(objNetSocket *)>(Self->Incoming.StdC.Routine))) {
            OBJECTPTR context = SetContext(Self->Incoming.StdC.Context);
               error = routine(Self);
            SetContext(context);
         }
      }
      else if (Self->Incoming.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         const struct ScriptArg args[] = { { "NetSocket", FD_OBJECTPTR, { .Address = Self } } };
         if ((script = Self->Incoming.Script.Script)) {
            if (!scCallback(script, Self->Incoming.Script.ProcedureID, args, ARRAYSIZE(args))) {
               GetLong(script, FID_Error, &error);
            }
            else error = ERR_Terminate; // Fatal error in attempting to execute the procedure
         }
      }

      if (error IS ERR_Terminate) MSG("Termination of socket requested by channel subscriber.");
      else if (!Self->ReadCalled) LogF("@client_incoming","[NetSocket:%d] Subscriber did not call Read()", Self->Head.UniqueID);
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
      LogF("~client_incoming","Termination of socket requested.");
      if (Self->SocketHandle != NOHANDLE) free_socket(Self);
      LogBack();
   }
   else if (Self->IncomingRecursion > 1) {
      // If client_server_incoming() was called again during the callback, that means that there is more
      // data is available and we should repeat our callback so that the client can receive the rest
      // of the data.

      Self->IncomingRecursion = 1;
      goto restart;
   }

   Self->InUse--;
   Self->IncomingRecursion = 0;

   STEP();
}

/*****************************************************************************
** If the socket is a client of a server, this routine will be called when there is empty space available on the socket
** for writing data to the server.
**
** It should be noted that this function will prevent the task from going to sleep if it is not managed correctly.  If
** no data is being written to the queue, the program will not be able to sleep until the client stops listening
** to the write queue.
*/

static void client_server_outgoing(SOCKET_HANDLE Void, struct rkNetSocket *Data)
{
   objNetSocket *Self = Data;
   ERROR error;
   LONG len;

   if (Self->Terminating) return;

#ifdef ENABLE_SSL
   if ((Self->SSL) AND (Self->State IS NTC_CONNECTING_SSL)) {
      FMSG("client_outgoing","Still connecting via SSL...");
      return;
   }
#endif

   if (Self->OutgoingRecursion) {
      FMSG("client_outgoing()","Recursion detected.");
      return;
   }

   FMSG("~client_outgoing()","");

#ifdef ENABLE_SSL
   if (Self->SSLBusy) { STEP(); return; } // SSL object is performing a background operation (e.g. handshake)
#endif

   Self->InUse++;
   Self->OutgoingRecursion++;

   error = ERR_Okay;

   // Send out remaining queued data before getting new data to send

   if (Self->WriteQueue.Buffer) {
      while (Self->WriteQueue.Buffer) {
         len = Self->WriteQueue.Length - Self->WriteQueue.Index;
         #ifdef ENABLE_SSL
         if ((!Self->SSL) AND (len > glMaxWriteLen)) len = glMaxWriteLen;
         #else
         if (len > glMaxWriteLen) len = glMaxWriteLen;
         #endif

         if (len > 0) {
            error = SEND(Self, Self->SocketHandle, (BYTE *)Self->WriteQueue.Buffer + Self->WriteQueue.Index, &len, 0);

            if ((error) OR (!len)) {
               break;
            }

            FMSG("client_out","[NetSocket:%d] Sent %d of %d bytes remaining on the queue.", Self->Head.UniqueID, len, Self->WriteQueue.Length-Self->WriteQueue.Index);

            Self->WriteQueue.Index += len;
         }

         if (Self->WriteQueue.Index >= Self->WriteQueue.Length) {
            FMSG("client_out","Freeing the write queue (pos %d/%d).", Self->WriteQueue.Index, Self->WriteQueue.Length);
            FreeResource(Self->WriteQueue.Buffer);
            Self->WriteQueue.Buffer = NULL;
            Self->WriteQueue.Index = 0;
            Self->WriteQueue.Length = 0;
            break;
         }
      }
   }

   // Before feeding new data into the queue, the current buffer must be empty.

   if ((!Self->WriteQueue.Buffer) OR (Self->WriteQueue.Index >= Self->WriteQueue.Length)) {
      if (Self->Outgoing.Type) {
         if (Self->Outgoing.Type IS CALL_STDC) {
            ERROR (*routine)(objNetSocket *);
            if ((routine = reinterpret_cast<ERROR (*)(objNetSocket *)>(Self->Outgoing.StdC.Routine))) {
               OBJECTPTR context = SetContext(Self->Outgoing.StdC.Context);
                  error = routine(Self);
               SetContext(context);
            }
         }
         else if (Self->Outgoing.Type IS CALL_SCRIPT) {
            OBJECTPTR script;
            const struct ScriptArg args[] = { { "NetSocket", FD_OBJECTPTR, { .Address = Self } } };
            if ((script = Self->Outgoing.Script.Script)) {
               if (!scCallback(script, Self->Outgoing.Script.ProcedureID, args, ARRAYSIZE(args))) {
                  GetLong(script, FID_Error, &error);
               }
               else error = ERR_Terminate; // Fatal error in attempting to execute the procedure
            }
         }

         if (error) Self->Outgoing.Type = CALL_NONE;
      }

      // If the write queue is empty and all data has been retrieved, we can remove the FD-Write registration so that
      // we don't tax the system resources.

      if ((Self->Outgoing.Type IS CALL_NONE) AND (!Self->WriteQueue.Buffer)) {
         FMSG("client_out","[NetSocket:%d] Write-queue listening on FD %d will now stop.", Self->Head.UniqueID, Self->SocketHandle);
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

   STEP();
}
