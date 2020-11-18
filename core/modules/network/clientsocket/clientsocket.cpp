/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
ClientSocket: Represents a single socket connection to a client IP address.

If a @Netsocket is running in server mode then it will create a new ClientSocket object every time that a new connection
is opened by a client.  This is a very simple class that assists in the management of I/O between the client and server.
-END-

*****************************************************************************/

// Data is being received from a client.

static void clientsocket_incoming(HOSTHANDLE SocketHandle, APTR Data)
{
   parasol::Log log(__FUNCTION__);
   auto ClientSocket = (objClientSocket *)Data;
   if (!ClientSocket->Client) return;
   objNetSocket *Socket = ClientSocket->Client->NetSocket;

   Socket->InUse++;
   ClientSocket->ReadCalled = FALSE;

   log.traceBranch("Handle: " PF64() ", Socket: %d, Client: %d", (LARGE)(MAXINT)SocketHandle, Socket->Head.UniqueID, ClientSocket->Head.UniqueID);

   ERROR error;
   if (Socket->Incoming.Type) {
      if (Socket->Incoming.Type IS CALL_STDC) {
         parasol::SwitchContext context(Socket->Incoming.StdC.Context);
         auto routine = (ERROR (*)(objNetSocket *, objClientSocket *))Socket->Incoming.StdC.Routine;
         if (routine) error = routine(Socket, ClientSocket);
      }
      else if (Socket->Incoming.Type IS CALL_SCRIPT) {
         const ScriptArg args[] = {
            { "NetSocket",    FD_OBJECTPTR, { .Address = Socket } },
            { "ClientSocket", FD_OBJECTPTR, { .Address = ClientSocket } }
         };

         OBJECTPTR script;
         if ((script = Socket->Incoming.Script.Script)) {
            if (!scCallback(script, Socket->Incoming.Script.ProcedureID, args, ARRAYSIZE(args))) {
               GetLong(script, FID_Error, &error);
            }
            else error = ERR_Terminate; // Fatal error in attempting to execute the procedure
         }
      }
      else log.warning("No Incoming callback configured (got %d).", Socket->Incoming.Type);

      if (error) {
         log.msg("Received error %d, incoming callback will be terminated.", error);
         Socket->Incoming.Type = CALL_NONE;
      }

      if (error IS ERR_Terminate) {
         log.trace("Termination request received.");
         free_client_socket(Socket, ClientSocket, TRUE);
         Socket->InUse--;
         return;
      }
   }
   else log.warning("No Incoming callback configured.");

   if (ClientSocket->ReadCalled IS FALSE) {
      UBYTE buffer[80];
      log.warning("Subscriber did not call Read(), cleaning buffer.");
      LONG result;
      do { error = RECEIVE(Socket, ClientSocket->Handle, &buffer, sizeof(buffer), 0, &result); } while (result > 0);
      if (error) free_client_socket(Socket, ClientSocket, TRUE);
   }

   Socket->InUse--;
}

/*****************************************************************************
** Note that this function will prevent the task from going to sleep if it is not managed correctly.  If
** no data is being written to the queue, the program will not be able to sleep until the client stops listening
** to the write queue.
*/

static void clientsocket_outgoing(HOSTHANDLE Void, APTR Data)
{
   parasol::Log log(__FUNCTION__);
   auto ClientSocket = (objClientSocket *)Data;
   objNetSocket *Socket = ClientSocket->Client->NetSocket;

   if (Socket->Terminating) return;

#ifdef ENABLE_SSL
   if ((Socket->SSL) AND (Socket->State IS NTC_CONNECTING_SSL)) {
      log.trace("Still connecting via SSL...");
      return;
   }
#endif

   if (ClientSocket->OutgoingRecursion) {
      log.trace("Recursion detected.");
      return;
   }

   log.traceBranch("");

#ifdef ENABLE_SSL
   if (Socket->SSLBusy) return; // SSL object is performing a background operation (e.g. handshake)
#endif

   ClientSocket->InUse++;
   ClientSocket->OutgoingRecursion++;

   ERROR error = ERR_Okay;

   // Send out remaining queued data before getting new data to send

   if (ClientSocket->WriteQueue.Buffer) {
      while (ClientSocket->WriteQueue.Buffer) {
         LONG len = ClientSocket->WriteQueue.Length - ClientSocket->WriteQueue.Index;
         #ifdef ENABLE_SSL
         if ((!Socket->SSL) AND (len > glMaxWriteLen)) len = glMaxWriteLen;
         #else
         if (len > glMaxWriteLen) len = glMaxWriteLen;
         #endif

         if (len > 0) {
            error = SEND(Socket, ClientSocket->SocketHandle, (BYTE *)ClientSocket->WriteQueue.Buffer + ClientSocket->WriteQueue.Index, &len, 0);
            if ((error) OR (!len)) break;
            log.trace("[NetSocket:%d] Sent %d of %d bytes remaining on the queue.", Socket->Head.UniqueID, len, ClientSocket->WriteQueue.Length - ClientSocket->WriteQueue.Index);
            ClientSocket->WriteQueue.Index += len;
         }

         if (ClientSocket->WriteQueue.Index >= ClientSocket->WriteQueue.Length) {
            log.trace("Freeing the write queue (pos %d/%d).", ClientSocket->WriteQueue.Index, ClientSocket->WriteQueue.Length);
            FreeResource(ClientSocket->WriteQueue.Buffer);
            ClientSocket->WriteQueue.Buffer = NULL;
            ClientSocket->WriteQueue.Index = 0;
            ClientSocket->WriteQueue.Length = 0;
            break;
         }
      }
   }

   // Before feeding new data into the queue, the current buffer must be empty.

   if ((!ClientSocket->WriteQueue.Buffer) OR (ClientSocket->WriteQueue.Index >= ClientSocket->WriteQueue.Length)) {
      if (ClientSocket->Outgoing.Type) {
         if (ClientSocket->Outgoing.Type IS CALL_STDC) {
            ERROR (*routine)(objNetSocket *, objClientSocket *);
            if ((routine = reinterpret_cast<ERROR (*)(objNetSocket *, objClientSocket *)>(ClientSocket->Outgoing.StdC.Routine))) {
               parasol::SwitchContext context(ClientSocket->Outgoing.StdC.Context);
               error = routine(Socket, ClientSocket);
            }
         }
         else if (ClientSocket->Outgoing.Type IS CALL_SCRIPT) {
            OBJECTPTR script;
            const ScriptArg args[] = {
               { "NetSocket", FD_OBJECTPTR, { .Address = Socket } },
               { "ClientSocket", FD_OBJECTPTR, { .Address = ClientSocket } }
            };
            if ((script = ClientSocket->Outgoing.Script.Script)) {
               if (!scCallback(script, ClientSocket->Outgoing.Script.ProcedureID, args, ARRAYSIZE(args))) {
                  GetLong(script, FID_Error, &error);
               }
               else error = ERR_Terminate; // Fatal error in attempting to execute the procedure
            }
         }

         if (error) ClientSocket->Outgoing.Type = CALL_NONE;
      }

      // If the write queue is empty and all data has been retrieved, we can remove the FD-Write registration so that
      // we don't tax the system resources.

      if ((ClientSocket->Outgoing.Type IS CALL_NONE) AND (!ClientSocket->WriteQueue.Buffer)) {
         log.trace("[NetSocket:%d] Write-queue listening on FD %d will now stop.", Socket->Head.UniqueID, ClientSocket->SocketHandle);
         #ifdef __linux__
            RegisterFD((HOSTHANDLE)ClientSocket->SocketHandle, RFD_REMOVE|RFD_WRITE|RFD_SOCKET, NULL, NULL);
         #elif _WIN32
            win_socketstate(ClientSocket->SocketHandle, -1, 0);
         #endif
      }
   }

   ClientSocket->InUse--;
   ClientSocket->OutgoingRecursion--;
}

//****************************************************************************

static ERROR CLIENTSOCKET_Free(objClientSocket *Self, APTR Void)
{
   parasol::Log log;

   if (Self->SocketHandle) {
#ifdef __linux__
      DeregisterFD(Self->Handle);
#endif
      CLOSESOCKET(Self->Handle);
      Self->Handle = -1;
   }

   if (Self->ReadQueue.Buffer) { FreeResource(Self->ReadQueue.Buffer); Self->ReadQueue.Buffer = NULL; }
   if (Self->WriteQueue.Buffer) { FreeResource(Self->WriteQueue.Buffer); Self->WriteQueue.Buffer = NULL; }

   if (Self->Prev) {
      Self->Prev->Next = Self->Next;
      if (Self->Next) Self->Next->Prev = Self->Prev;
   }
   else {
      Self->Client->Sockets = Self->Next;
      if (Self->Next) Self->Next->Prev = NULL;
   }

   Self->Client->TotalSockets--;

   if (!Self->Client->Sockets) {
      log.msg("No more open sockets, removing client.");
      free_client(Self->Client->NetSocket, Self->Client);
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR CLIENTSOCKET_Init(objClientSocket *Self, APTR Void)
{
#ifdef __linux__
   LONG non_blocking = 1;
   ioctl(Self->Handle, FIONBIO, &non_blocking);
#endif

   Self->ConnectTime = PreciseTime() / 1000LL;

   if (Self->Client->Sockets) {
      Self->Next = Self->Client->Sockets;
      Self->Prev = NULL;
      Self->Client->Sockets->Prev = Self;
   }

   Self->Client->Sockets = Self;
   Self->Client->TotalSockets++;

#ifdef __linux__
   RegisterFD(Self->Handle, RFD_READ|RFD_SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&clientsocket_incoming), Self);
#elif _WIN32
   win_socket_reference(Self->Handle, Self);
#endif

   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
Read: Read incoming data from a client socket.

The Read action will read incoming data from the socket and write it to the provided buffer.  If the socket connection
is safe, success will always be returned by this action regardless of whether or not data was available.  Almost all
other return codes indicate permanent failure, and the socket connection will be closed when the action returns.

-ERRORS-
Okay: Read successful (if no data was on the socket, success is still indicated).
NullArgs
Disconnected: The socket connection is closed.
Failed: A permanent failure has occurred and socket has been closed.

*****************************************************************************/

static ERROR CLIENTSOCKET_Read(objClientSocket *Self, struct acRead *Args)
{
   parasol::Log log;
   if ((!Args) OR (!Args->Buffer)) return log.error(ERR_NullArgs);
   if (Self->SocketHandle IS NOHANDLE) return log.error(ERR_Disconnected);
   Self->ReadCalled = TRUE;
   if (!Args->Length) { Args->Result = 0; return ERR_Okay; }
   return RECEIVE(Self->Client->NetSocket, Self->SocketHandle, Args->Buffer, Args->Length, 0, &Args->Result);
}

/*****************************************************************************

-METHOD-
ReadClientMsg: Read a message from the socket.

This method reads messages that have been sent to the socket using Parasol Message Protocols.  Any message sent with
the WriteClientMsg method will conform to this protocol, thus simplifying message transfers between programs based on the
core platform at either point of the network link.

This method never returns a successful error code unless an entire message has been received from the sender.

-INPUT-
&ptr Message: A pointer to the message buffer will be placed here if a message has been received.
&int Length: The length of the message is returned here.
&int Progress: The number of bytes that have been read for the incoming message.
&int CRC: Indicates the CRC value that the message is expected to match.

-ERRORS-
Okay: A complete message has been read and indicated in the result parameters.
Args
NullArgs
LimitedSuccess: Some data has arrived, but the entire message is incomplete.  The length of the incoming message may be indicated in the Length parameter.
NoData: No new data was found for the socket.
BadData: The message header or tail was invalid, or the message length exceeded internally imposed limits.
AllocMemory: A message buffer could not be allocated.

*****************************************************************************/

static ERROR CLIENTSOCKET_ReadClientMsg(objClientSocket *Self, struct csReadClientMsg *Args)
{
   parasol::Log log;

   if (!Args) return log.error(ERR_NullArgs);

   log.traceBranch("Reading message.");

   Args->Message  = NULL;
   Args->Length   = 0;
   Args->CRC      = 0;
   Args->Progress = 0;

   NetQueue *queue = &Self->ReadQueue;

   if (!queue->Buffer) {
      queue->Length = 2048;
      if (AllocMemory(queue->Length, MEM_NO_CLEAR, &queue->Buffer, NULL) != ERR_Okay) {
         return ERR_AllocMemory;
      }
   }

   LONG msglen, result, magic;
   ULONG total_length;
   ERROR error;

   if (queue->Index >= sizeof(NetMsg)) { // The complete message header has been received
      msglen = be32_cpu(((NetMsg *)queue->Buffer)->Length);
      total_length = sizeof(NetMsg) + msglen + 1 + sizeof(NetMsgEnd);
   }
   else { // The message header has not been read yet
      if (!(error = acRead(Self, (BYTE *)queue->Buffer + queue->Index, sizeof(NetMsg) - queue->Index, &result))) {
         queue->Index += result;

         if (queue->Index >= sizeof(NetMsg)) {
            // We have the message header
            magic  = be32_cpu(((NetMsg *)queue->Buffer)->Magic);
            msglen = be32_cpu(((NetMsg *)queue->Buffer)->Length);

            if (magic != NETMSG_MAGIC) {
               log.warning("Incoming message does not have the magic header (received $%.8x).", magic);
               queue->Index = 0;
               return ERR_InvalidData;
            }
            else if (msglen > NETMSG_SIZE_LIMIT) {
               log.warning("Incoming message of %d ($%.8x) bytes exceeds message limit.", msglen, msglen);
               queue->Index = 0;
               return ERR_InvalidData;
            }

            total_length = sizeof(NetMsg) + msglen + 1 + sizeof(NetMsgEnd);

            // Check if the queue buffer needs to be extended

            if (total_length > queue->Length) {
               MSG("Extending queue length from %d to %d", queue->Length, total_length);
               APTR buffer;
               if (!AllocMemory(total_length, MEM_NO_CLEAR, &buffer, NULL)) {
                  if (queue->Buffer) {
                     CopyMemory(queue->Buffer, buffer, queue->Index);
                     FreeResource(queue->Buffer);
                  }
                  queue->Buffer = buffer;
                  queue->Length = total_length;
               }
               else return log.error(ERR_AllocMemory);
            }
         }
         else {
            log.trace("Succeeded in reading partial message header only (%d bytes).", result);
            return ERR_LimitedSuccess;
         }
      }
      else {
         log.trace("Read() failed, error '%s'", GetErrorMsg(error));
         return ERR_LimitedSuccess;
      }
   }

   NetMsgEnd *msgend;
   Args->Message = (BYTE *)queue->Buffer + sizeof(NetMsg);
   Args->Length = msglen;

   //log.trace("Current message is %d bytes long (raw len: %d), progress is %d bytes.", msglen, total_length, queue->Index);

   if (!(error = acRead(Self, (char *)queue->Buffer + queue->Index, total_length - queue->Index, &result))) {
      queue->Index += result;
      Args->Progress = queue->Index - sizeof(NetMsg) - sizeof(NetMsgEnd);
      if (Args->Progress < 0) Args->Progress = 0;

      // If the entire message has been read, we can report success to the user

      if (queue->Index >= total_length) {
         msgend = (NetMsgEnd *)((BYTE *)queue->Buffer + sizeof(NetMsg) + msglen + 1);
         magic = be32_cpu(msgend->Magic);
         queue->Index   = 0;
         Args->Progress = Args->Length;
         Args->CRC      = be32_cpu(msgend->CRC);

         log.trace("The entire message of %d bytes has been received.", msglen);

         if (NETMSG_MAGIC_TAIL != magic) {
            log.warning("Incoming message has an invalid tail of $%.8x, CRC $%.8x.", magic, Args->CRC);
            return ERR_InvalidData;
         }

         return ERR_Okay;
      }
      else return ERR_LimitedSuccess;
   }
   else {
      log.warning("Failed to read %d bytes off the socket, error %d.", total_length - queue->Index, error);
      queue->Index = 0;
      return error;
   }
}

/*****************************************************************************

-ACTION-
Write: Writes data to the socket.

Write raw data to a client socket with this action.  Write connections are buffered, so any
data overflow generated in a call to this action will be buffered into a software queue.  Resource limits placed on the
software queue are governed by the #MsgLimit field setting.

*****************************************************************************/

static ERROR CLIENTSOCKET_Write(objClientSocket *Self, struct acWrite *Args)
{
   parasol::Log log;

   if (!Args) return ERR_NullArgs;
   Args->Result = 0;
   if (Self->SocketHandle IS NOHANDLE) return log.error(ERR_Disconnected);

   LONG len = Args->Length;
   ERROR error = SEND(Self->Client->NetSocket, Self->SocketHandle, Args->Buffer, &len, 0);

   if ((error) OR (len < Args->Length)) {
      if (error) log.trace("SEND() Error: '%s', queuing %d/%d bytes for transfer...", GetErrorMsg(error), Args->Length - len, Args->Length);
      else log.trace("Queuing %d of %d remaining bytes for transfer...", Args->Length - len, Args->Length);
      if ((error IS ERR_DataSize) OR (error IS ERR_BufferOverflow) OR (len > 0))  {
         write_queue(Self->Client->NetSocket, &Self->WriteQueue, (BYTE *)Args->Buffer + len, Args->Length - len);
         #ifdef __linux__
            RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD_WRITE|RFD_SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&clientsocket_outgoing), Self);
         #elif _WIN32
            win_socketstate(Self->SocketHandle, -1, TRUE);
         #endif
      }
   }
   else log.trace("Successfully wrote all %d bytes to the server.", Args->Length);

   Args->Result = Args->Length;
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
WriteClientMsg: Writes a message to the socket.

Messages can be written to sockets with the WriteClientMsg method and read back by the receiver with #ReadClientMsg().  The
message data is sent through the #Write() action, so the standard process will apply (the message will be
queued and does not block if buffers are full).

-INPUT-
buf(ptr) Message: Pointer to the message to send.
bufsize Length: The length of the message.

-ERRORS-
Okay
Args
OutOfRange

*****************************************************************************/

static ERROR CLIENTSOCKET_WriteClientMsg(objClientSocket *Self, struct csWriteClientMsg *Args)
{
   parasol::Log log;

   if ((!Args) OR (!Args->Message) OR (Args->Length < 1)) return log.error(ERR_Args);
   if ((Args->Length < 1) OR (Args->Length > NETMSG_SIZE_LIMIT)) return log.error(ERR_OutOfRange);

   log.traceBranch("Message: %p, Length: %d", Args->Message, Args->Length);

   NetMsg msg = { .Magic = cpu_be32(NETMSG_MAGIC), .Length = cpu_be32(Args->Length) };
   acWrite(Self, &msg, sizeof(msg), NULL);
   acWrite(Self, Args->Message, Args->Length, NULL);

   UBYTE endbuffer[sizeof(NetMsgEnd) + 1];
   NetMsgEnd *end = (NetMsgEnd *)(endbuffer + 1);
   endbuffer[0] = 0; // This null terminator helps with message parsing
   end->Magic = cpu_be32((ULONG)NETMSG_MAGIC_TAIL);
   end->CRC   = cpu_be32(GenCRC32(0, Args->Message, Args->Length));
   acWrite(Self, &endbuffer, sizeof(endbuffer), NULL);

   return ERR_Okay;
}

//****************************************************************************

#include "clientsocket_def.c"

static const FieldArray clClientSocketFields[] = {
   { "ConnectTime", FDF_LARGE|FDF_R,    0, NULL, NULL },
   { "Prev",        FDF_OBJECT|FDF_R,   ID_CLIENTSOCKET, NULL, NULL },
   { "Next",        FDF_OBJECT|FDF_R,   ID_CLIENTSOCKET, NULL, NULL },
   { "Client",      FDF_POINTER|FDF_STRUCT|FDF_R, (MAXINT)"NetClient", NULL, NULL },
   { "UserData",    FDF_POINTER|FDF_R,  0, NULL, NULL },
   { "Outgoing",    FDF_FUNCTION|FDF_R, 0, NULL, NULL },
   { "Incoming",    FDF_FUNCTION|FDF_R, 0, NULL, NULL },
   { "MsgLen",      FDF_LONG|FDF_R,     0, NULL, NULL },
   // Virtual fields
//   { "Handle",      FDF_LONG|FDF_R|FDF_VIRTUAL,     0, (APTR)GET_ClientHandle, (APTR)SET_ClientHandle },
   END_FIELD
};

//****************************************************************************

static ERROR add_clientsocket(void)
{
   if (CreateObject(ID_METACLASS, 0, &clClientSocket,
      FID_BaseClassID|TLONG,    ID_CLIENTSOCKET,
      FID_ClassVersion|TDOUBLE, 1.0,
      FID_Name|TSTRING,         "ClientSocket",
      FID_Category|TLONG,       CCF_NETWORK,
      FID_Actions|TPTR,         clClientSocketActions,
      FID_Fields|TARRAY,        clClientSocketFields,
      FID_Size|TLONG,           sizeof(objClientSocket),
      FID_Path|TSTR,            MOD_PATH,
      TAGEND) != ERR_Okay) return ERR_CreateObject;

   return ERR_Okay;
}
