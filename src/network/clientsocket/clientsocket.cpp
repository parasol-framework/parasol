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

   ERR error = ERR::Okay;

   // Send out remaining queued data before getting new data to send

   if (ClientSocket->WriteQueue.Buffer) {
      while (ClientSocket->WriteQueue.Buffer) {
         int len = ClientSocket->WriteQueue.Length - ClientSocket->WriteQueue.Index;
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
            error = SEND(Socket, ClientSocket->SocketHandle, (BYTE *)ClientSocket->WriteQueue.Buffer + ClientSocket->WriteQueue.Index, &len, 0);
            if ((error != ERR::Okay) or (!len)) break;
            log.trace("[NetSocket:%d] Sent %d of %d bytes remaining on the queue.", Socket->UID, len, ClientSocket->WriteQueue.Length - ClientSocket->WriteQueue.Index);
            ClientSocket->WriteQueue.Index += len;
         }

         if (ClientSocket->WriteQueue.Index >= ClientSocket->WriteQueue.Length) {
            log.trace("Freeing the write queue (pos %d/%d).", ClientSocket->WriteQueue.Index, ClientSocket->WriteQueue.Length);
            FreeResource(ClientSocket->WriteQueue.Buffer);
            ClientSocket->WriteQueue.Buffer = nullptr;
            ClientSocket->WriteQueue.Index = 0;
            ClientSocket->WriteQueue.Length = 0;
            break;
         }
      }
   }

   // Before feeding new data into the queue, the current buffer must be empty.

   if ((!ClientSocket->WriteQueue.Buffer) or (ClientSocket->WriteQueue.Index >= ClientSocket->WriteQueue.Length)) {
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

      if ((!ClientSocket->Outgoing.defined()) and (!ClientSocket->WriteQueue.Buffer)) {
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

   if (Self->ReadQueue.Buffer) { FreeResource(Self->ReadQueue.Buffer); Self->ReadQueue.Buffer = nullptr; }
   if (Self->WriteQueue.Buffer) { FreeResource(Self->WriteQueue.Buffer); Self->WriteQueue.Buffer = nullptr; }

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

-METHOD-
ReadClientMsg: Read a message from the socket.

This method reads messages that have been sent to the socket using Parasol Message Protocols.  Any message sent with
the #WriteClientMsg() method will conform to this protocol, thus simplifying message transfers between programs based 
on the core platform at either point of the network link.

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

*********************************************************************************************************************/

static ERR CLIENTSOCKET_ReadClientMsg(extClientSocket *Self, struct cs::ReadClientMsg *Args)
{
   pf::Log log;

   if (!Args) return log.error(ERR::NullArgs);

   log.traceBranch("Reading message.");

   Args->Message  = nullptr;
   Args->Length   = 0;
   Args->CRC      = 0;
   Args->Progress = 0;

   NetQueue &queue = Self->ReadQueue;

   if (!queue.Buffer) {
      queue.Length = 2048;
      if (AllocMemory(queue.Length, MEM::NO_CLEAR, &queue.Buffer) != ERR::Okay) {
         return ERR::AllocMemory;
      }
   }

   int msglen, result, magic;
   uint32_t total_length;
   ERR error;

   if (queue.Index >= sizeof(NetMsg)) { // The complete message header has been received
      msglen = htonl(((NetMsg *)queue.Buffer)->Length);
      total_length = sizeof(NetMsg) + msglen + 1 + sizeof(NetMsgEnd);
   }
   else { // The message header has not been read yet
      if ((error = acRead(Self, (BYTE *)queue.Buffer + queue.Index, sizeof(NetMsg) - queue.Index, &result)) IS ERR::Okay) {
         queue.Index += result;

         if (queue.Index >= sizeof(NetMsg)) {
            // We have the message header
            magic  = htonl(((NetMsg *)queue.Buffer)->Magic);
            msglen = htonl(((NetMsg *)queue.Buffer)->Length);

            if (magic != NETMSG_MAGIC) {
               log.warning("Incoming message does not have the magic header (received $%.8x).", magic);
               queue.Index = 0;
               return ERR::InvalidData;
            }
            else if (msglen > NETMSG_SIZE_LIMIT) {
               log.warning("Incoming message of %d ($%.8x) bytes exceeds message limit.", msglen, msglen);
               queue.Index = 0;
               return ERR::InvalidData;
            }

            total_length = sizeof(NetMsg) + msglen + 1 + sizeof(NetMsgEnd);

            // Check if the queue buffer needs to be extended

            if (total_length > queue.Length) {
               log.trace("Extending queue length from %d to %d", queue.Length, total_length);
               APTR buffer;
               if (AllocMemory(total_length, MEM::NO_CLEAR, &buffer) IS ERR::Okay) {
                  if (queue.Buffer) {
                     pf::copymem(queue.Buffer, buffer, queue.Index);
                     FreeResource(queue.Buffer);
                  }
                  queue.Buffer = buffer;
                  queue.Length = total_length;
               }
               else return log.error(ERR::AllocMemory);
            }
         }
         else {
            log.trace("Succeeded in reading partial message header only (%d bytes).", result);
            return ERR::LimitedSuccess;
         }
      }
      else {
         log.trace("Read() failed, error '%s'", GetErrorMsg(error));
         return ERR::LimitedSuccess;
      }
   }

   NetMsgEnd *msgend;
   Args->Message = (BYTE *)queue.Buffer + sizeof(NetMsg);
   Args->Length = msglen;

   //log.trace("Current message is %d bytes long (raw len: %d), progress is %d bytes.", msglen, total_length, queue.Index);

   if ((error = acRead(Self, (char *)queue.Buffer + queue.Index, total_length - queue.Index, &result)) IS ERR::Okay) {
      queue.Index += result;
      Args->Progress = queue.Index - sizeof(NetMsg) - sizeof(NetMsgEnd);
      if (Args->Progress < 0) Args->Progress = 0;

      // If the entire message has been read, we can report success to the user

      if (queue.Index >= total_length) {
         msgend = (NetMsgEnd *)((BYTE *)queue.Buffer + sizeof(NetMsg) + msglen + 1);
         magic = htonl(msgend->Magic);
         queue.Index   = 0;
         Args->Progress = Args->Length;
         Args->CRC      = htonl(msgend->CRC);

         log.trace("The entire message of %d bytes has been received.", msglen);

         if (NETMSG_MAGIC_TAIL != magic) {
            log.warning("Incoming message has an invalid tail of $%.8x, CRC $%.8x.", magic, Args->CRC);
            return ERR::InvalidData;
         }

         return ERR::Okay;
      }
      else return ERR::LimitedSuccess;
   }
   else {
      log.warning("Failed to read %d bytes off the socket, error %d.", total_length - queue.Index, LONG(error));
      queue.Index = 0;
      return error;
   }
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

   int len = Args->Length;
   ERR error = SEND((extNetSocket *)(Self->Client->NetSocket), Self->SocketHandle, Args->Buffer, &len, 0);

   if ((error != ERR::Okay) or (len < Args->Length)) {
      if (error != ERR::Okay) log.trace("SEND() Error: '%s', queuing %d/%d bytes for transfer...", GetErrorMsg(error), Args->Length - len, Args->Length);
      else log.trace("Queuing %d of %d remaining bytes for transfer...", Args->Length - len, Args->Length);
      if ((error IS ERR::DataSize) or (error IS ERR::BufferOverflow) or (len > 0))  {
         write_queue((extNetSocket *)(Self->Client->NetSocket), &Self->WriteQueue, (BYTE *)Args->Buffer + len, Args->Length - len);
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

/*********************************************************************************************************************

-METHOD-
WriteClientMsg: Writes a message to the socket.

Messages can be written to sockets with the WriteClientMsg method and read back by the receiver with #ReadClientMsg().  
The message data is sent through the #Write() action, so the standard process will apply (the message will be
queued and does not block if buffers are full).

-INPUT-
buf(ptr) Message: Pointer to the message to send.
bufsize Length: The length of the `Message`.

-ERRORS-
Okay
Args
OutOfRange

*********************************************************************************************************************/

static ERR CLIENTSOCKET_WriteClientMsg(extClientSocket *Self, struct cs::WriteClientMsg *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Message) or (Args->Length < 1)) return log.error(ERR::Args);
   if ((Args->Length < 1) or (Args->Length > NETMSG_SIZE_LIMIT)) return log.error(ERR::OutOfRange);

   log.traceBranch("Message: %p, Length: %d", Args->Message, Args->Length);

   NetMsg msg = { .Magic = htonl(NETMSG_MAGIC), .Length = htonl(Args->Length) };
   acWrite(Self, &msg, sizeof(msg), nullptr);
   acWrite(Self, Args->Message, Args->Length, nullptr);

   uint8_t endbuffer[sizeof(NetMsgEnd) + 1];
   NetMsgEnd *end = (NetMsgEnd *)(endbuffer + 1);
   endbuffer[0] = 0; // This null terminator helps with message parsing
   end->Magic = htonl((uint32_t)NETMSG_MAGIC_TAIL);
   end->CRC   = htonl(GenCRC32(0, Args->Message, Args->Length));
   acWrite(Self, &endbuffer, sizeof(endbuffer), nullptr);

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
   { "MsgLen",      FDF_INT|FDF_R },
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
