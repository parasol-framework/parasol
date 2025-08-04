/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
NetSocket: Manages network connections via TCP/IP sockets.

The NetSocket class provides a simple way of managing TCP/IP socket communications.  Connections from a single client
to the server and from the server to multiple clients are supported.  SSL functionality is also integrated.

The design of the NetSocket class caters to asynchronous (non-blocking) communication.  This is achieved primarily
through callback fields - connection alerts are managed by #Feedback, incoming data is received through #Incoming
and readiness for outgoing data is supported by #Outgoing.

<header>Client-Server Connections</>

After a connection has been established, data may be written using any of the following methods:

<list type="bullet">
<li>Write directly to the socket with the #Write() action.</li>
<li>Subscribe to the socket by referring to a routine in the #Outgoing field.  The routine will be called to
initially fill the internal write buffer, thereafter it will be called whenever the buffer is empty.</li>
</ul>

It is possible to write to a NetSocket object before the connection to a server is established.  Doing so will buffer
the data in the socket until the connection with the server has been initiated, at which point the data will be
immediately sent.

<header>Server-Client Connections</>

To accept incoming client connections, create a NetSocket object with the `SERVER` flag set and define the #Port value
on which to listen for new clients.  If multiple connections from a single client IP address are allowed, set the
`MULTI_CONNECT` flag.

When a new connection is detected, the #Feedback function will be called as `Feedback(*NetSocket, *ClientSocket, LONG State)`

The NetSocket parameter refers to the original NetSocket object, @ClientSocket applies if a client connection is
involved and the State value will be set to `NTC::CONNECTED`.  If a client disconnects, the #Feedback function will be
called in the same manner but with a State value of `NTC::DISCONNECTED`.

Information on all active connections can be read from the #Clients field.  This contains a linked list of IP
addresses and their connections to the server port.

To send data to a client, write it to the target @ClientSocket.

All data that is received from client sockets will be passed to the #Incoming feedback routine with a reference to a @ClientSocket.
-END-

*********************************************************************************************************************/

// The MaxWriteLen cannot exceed the size of the network queue on the host platform, otherwise all send attempts will
// return 'could block' error codes.  Note that when using SSL, the write length is an SSL library imposition.

static size_t glMaxWriteLen = 16 * 1024;

//********************************************************************************************************************
// Prototypes for internal methods

#ifdef __linux__
static void client_connect(HOSTHANDLE, APTR);
#endif

static void client_server_incoming(SOCKET_HANDLE, extNetSocket *);
static void client_server_outgoing(SOCKET_HANDLE, extNetSocket *);
static void clientsocket_incoming(HOSTHANDLE, APTR);
static void clientsocket_outgoing(HOSTHANDLE, APTR);
static void free_client(extNetSocket *, NetClient *);
static void free_client_socket(extNetSocket *, extClientSocket *, BYTE);
static void server_client_connect(SOCKET_HANDLE, extNetSocket *);
static void free_socket(extNetSocket *);

//********************************************************************************************************************

static void notify_free_feedback(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   ((extNetSocket *)CurrentContext())->Feedback.clear();
}

static void notify_free_incoming(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   ((extNetSocket *)CurrentContext())->Incoming.clear();
}

static void notify_free_outgoing(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   ((extNetSocket *)CurrentContext())->Outgoing.clear();
}

/*********************************************************************************************************************

-METHOD-
Connect: Connects a NetSocket to an address.

This method initiates the connection process with a target IP address.  The address to connect to can be specified either
as a domain name, in which case the domain name is first resolved to an IP address, or the address can be specified in
standard IP notation.

This method is non-blocking.  It will return immediately and the connection will be resolved once the server responds
to the connection request or an error occurs.  Client code should subscribe to the #State field to respond to changes to
the connection state.

Pre-Condition: Must be in a connection state of `NTC::DISCONNECTED`

Post-Condition: If this method returns `ERR::Okay`, will be in state `NTC::CONNECTING`.

-INPUT-
cstr Address: String containing either a domain name (e.g. `www.google.com`) or an IP address (e.g. `123.123.123.123`)
int Port: Remote port to connect to.

-ERRORS-
Okay: The NetSocket connecting process was successfully started.
Args: Address was NULL, or Port was not in the required range.
InvalidState: The NetSocket was not in the state `NTC::DISCONNECTED`.
HostNotFound: Host name resolution failed.
Failed: The connect failed for some other reason.
-END-

*********************************************************************************************************************/

static void connect_name_resolved_nl(objNetLookup *, ERR, const std::string &, const std::vector<IPAddress> &);
static void connect_name_resolved(extNetSocket *, ERR, const std::string &, const std::vector<IPAddress> &);

static ERR NETSOCKET_Connect(extNetSocket *Self, struct ns::Connect *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Address) or (Args->Port <= 0) or (Args->Port >= 65536)) return log.warning(ERR::Args);

   if ((Self->Flags & NSF::SERVER) != NSF::NIL) return ERR::Failed;

   if (!Self->SocketHandle) return log.warning(ERR::NotInitialised);

   if (Self->State != NTC::DISCONNECTED) {
      log.warning("Attempt to connect when socket is not in disconnected state");
      return ERR::InvalidState;
   }

   log.branch("Address: %s, Port: %d", Args->Address, Args->Port);

   if (Args->Address != Self->Address) {
      if (Self->Address) FreeResource(Self->Address);
      Self->Address = pf::strclone(Args->Address);
   }
   Self->Port = Args->Port;

   IPAddress server_ip;
   if (net::StrToAddress(Self->Address, &server_ip) IS ERR::Okay) { // The address is an IP string, no resolution is necessary
      std::vector<IPAddress> list;
      list.emplace_back(server_ip);
      connect_name_resolved(Self, ERR::Okay, "", list);
   }
   else { // Assume address is a domain name, perform name resolution
      log.msg("Attempting to resolve domain name '%s'...", Self->Address);

      if (!Self->NetLookup) {
         if (!(Self->NetLookup = extNetLookup::create::local())) return ERR::CreateObject;
      }

      ((extNetLookup *)Self->NetLookup)->Callback = C_FUNCTION(connect_name_resolved_nl);
      if (Self->NetLookup->resolveName(Self->Address) != ERR::Okay) {
         return log.warning(Self->Error = ERR::HostNotFound);
      }
   }

   return ERR::Okay;
}

//********************************************************************************************************************
// This function is called on completion of nlResolveName().

static void connect_name_resolved_nl(objNetLookup *NetLookup, ERR Error, const std::string &HostName, const std::vector<IPAddress> &IPs)
{
   connect_name_resolved((extNetSocket *)CurrentContext(), Error, HostName, IPs);
}

static void connect_name_resolved(extNetSocket *Socket, ERR Error, const std::string &HostName, const std::vector<IPAddress> &IPs)
{
   pf::Log log(__FUNCTION__);
   struct sockaddr_in server_address;

   if (Error != ERR::Okay) {
      log.warning("DNS resolution failed: %s", GetErrorMsg(Error));
      return;
   }

   log.msg("Received callback on DNS resolution.  Handle: %d", Socket->SocketHandle);

   // Start connect()

   if (IPs.empty()) {
      log.warning("No IP addresses resolved for %s", HostName.c_str());
      Socket->Error = ERR::HostNotFound;
      Socket->setState(NTC::DISCONNECTED);
      return;
   }

   // Find an appropriate address for our socket type
   const IPAddress *addr = nullptr;
   
   // If we have an IPv4 socket, prefer IPv4 addresses
   if (!Socket->IPV6) {
      for (const auto &ip : IPs) {
         if (ip.Type IS IPADDR::V4) {
            addr = &ip;
            break;
         }
      }
   } 
   else { // For IPv6 sockets, use the first address (could be IPv4 or IPv6)
      addr = &IPs[0];
      if ((!addr->Data[0]) and (!addr->Data[1]) and (!addr->Data[2]) and (!addr->Data[3])) {
         log.traceWarning("Failed sanity check, incoming IP address is empty.");
         Socket->Error = log.warning(ERR::InvalidData);
         return;
      }
   }
   
   if (!addr) {
      log.warning("Of %d addresses, no compatible IP address found for socket type (IPv6: %s)", int(IPs.size()), Socket->IPV6 ? "true" : "false");
      Socket->Error = ERR::HostNotFound;
      Socket->setState(NTC::DISCONNECTED);
      return;
   }
   
   if (addr->Type IS IPADDR::V6) { // Pure IPv6 connection
      #ifdef _WIN32
         struct sockaddr_in6 server_address6;
         pf::clearmem(&server_address6, sizeof(server_address6));
         server_address6.sin6_family = AF_INET6;
         server_address6.sin6_port = net::HostToShort((UWORD)Socket->Port);
         pf::copymem((void *)addr->Data, &server_address6.sin6_addr.s6_addr, 16);
         
         if ((Socket->Error = win_connect(Socket->SocketHandle, (struct sockaddr *)&server_address6, sizeof(server_address6))) != ERR::Okay) {
            if (Socket->Error IS ERR::BufferOverflow) {
               log.trace("IPv6 connection in progress...");
               Socket->setState(NTC::CONNECTING);
            }
            else {
               log.warning("IPv6 connect() failed: %s", GetErrorMsg(Socket->Error));
               Socket->setState(NTC::DISCONNECTED);
               return;
            }
         }
         else {
            log.trace("IPv6 connect() successful.");
            Socket->setState(NTC::CONNECTED);
         }
         return;
      #else
         struct sockaddr_in6 server_address6;
         pf::clearmem(&server_address6, sizeof(server_address6));
         server_address6.sin6_family = AF_INET6;
         server_address6.sin6_port = net::HostToShort((UWORD)Socket->Port);
         pf::copymem(&server_address6.sin6_addr.s6_addr, (void *)addr->Data, 16);
         
         int result = connect(Socket->SocketHandle, (struct sockaddr *)&server_address6, sizeof(server_address6));
         
         if (result IS -1) {
            if (errno IS EINPROGRESS) {
               log.trace("IPv6 connection in progress...");
            }
            else if ((errno IS EWOULDBLOCK) or (errno IS EAGAIN)) {
               log.trace("IPv6 connect() attempt would block or need to try again.");
            }
            else {
               log.warning("IPv6 Connect() failed: %s", strerror(errno));
               Socket->Error = ERR::Failed;
               Socket->setState(NTC::DISCONNECTED);
               return;
            }

            Socket->setState(NTC::CONNECTING);
            RegisterFD((HOSTHANDLE)Socket->SocketHandle, RFD::READ|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&client_server_incoming, Socket);
            RegisterFD((HOSTHANDLE)Socket->SocketHandle, RFD::WRITE|RFD::SOCKET, &client_connect, Socket);
         }
         else {
            log.trace("IPv6 connect() successful.");
            Socket->setState(NTC::CONNECTED);
            RegisterFD((HOSTHANDLE)Socket->SocketHandle, RFD::READ|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&client_server_incoming, Socket);
         }
      #endif
      return;
   }
   
   // IPv4 connection to dual-stack socket - use IPv4-mapped IPv6 address
   if ((Socket->IPV6) and (addr->Type IS IPADDR::V4)) {
      // Use IPv4-mapped IPv6 address for dual-stack socket
      #ifdef __linux__
         struct sockaddr_in6 server_address6;
         pf::clearmem(&server_address6, sizeof(server_address6));
         server_address6.sin6_family = AF_INET6;
         server_address6.sin6_port = net::HostToShort((UWORD)Socket->Port);
         
         // Create IPv4-mapped IPv6 address (::ffff:x.x.x.x)
         server_address6.sin6_addr.s6_addr[10] = 0xff;
         server_address6.sin6_addr.s6_addr[11] = 0xff;
         *((uint32_t*)&server_address6.sin6_addr.s6_addr[12]) = net::HostToLong(addr->Data[0]);
         
         int result = connect(Socket->SocketHandle, (struct sockaddr *)&server_address6, sizeof(server_address6));
         
         if (result IS -1) {
            if (errno IS EINPROGRESS) {
               log.trace("IPv4-mapped IPv6 connection in progress...");
            }
            else if ((errno IS EWOULDBLOCK) or (errno IS EAGAIN)) {
               log.trace("IPv4-mapped IPv6 connect() attempt would block or need to try again.");
            }
            else {
               log.warning("IPv4-mapped IPv6 Connect() failed: %s", strerror(errno));
               Socket->Error = ERR::Failed;
               Socket->setState(NTC::DISCONNECTED);
               return;
            }

            Socket->setState(NTC::CONNECTING);
            RegisterFD((HOSTHANDLE)Socket->SocketHandle, RFD::READ|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&client_server_incoming, Socket);
            RegisterFD((HOSTHANDLE)Socket->SocketHandle, RFD::WRITE|RFD::SOCKET, &client_connect, Socket);
         }
         else {
            log.trace("IPv4-mapped IPv6 connect() successful.");
            Socket->setState(NTC::CONNECTED);
            RegisterFD((HOSTHANDLE)Socket->SocketHandle, RFD::READ|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&client_server_incoming, Socket);
         }
         return;
      #elif _WIN32
         // Windows IPv6 dual-stack socket connecting to IPv4 address
         struct sockaddr_in6 server_address6;
         pf::clearmem(&server_address6, sizeof(server_address6));
         server_address6.sin6_family = AF_INET6;
         server_address6.sin6_port = net::HostToShort((UWORD)Socket->Port);
         
         // Create IPv4-mapped IPv6 address (::ffff:x.x.x.x)
         server_address6.sin6_addr.s6_addr[10] = 0xff;
         server_address6.sin6_addr.s6_addr[11] = 0xff;
         *((uint32_t*)&server_address6.sin6_addr.s6_addr[12]) = net::HostToLong(addr->Data[0]);
         
         if (win_connect(Socket->SocketHandle, (struct sockaddr *)&server_address6, sizeof(server_address6)) IS ERR::Okay) {
            log.trace("IPv4-mapped IPv6 connection initiated successfully");
            Socket->setState(NTC::CONNECTING);
         }
         else {
            log.trace("IPv4-mapped IPv6 connect() failed");
            Socket->Error = ERR::Failed;
            Socket->setState(NTC::DISCONNECTED);
         }
         return;
      #endif
   }
   
   // Pure IPv4 connection
   pf::clearmem(&server_address, sizeof(struct sockaddr_in));
   server_address.sin_family = AF_INET;
   server_address.sin_port = net::HostToShort((UWORD)Socket->Port);
   server_address.sin_addr.s_addr = net::HostToLong(addr->Data[0]);

#ifdef __linux__
   int result = connect(Socket->SocketHandle, (struct sockaddr *)&server_address, sizeof(server_address));

   if (result IS -1) {
      if (errno IS EINPROGRESS) {
         log.trace("Connection in progress...");
      }
      else if ((errno IS EWOULDBLOCK) or (errno IS EAGAIN)) {
         log.trace("connect() attempt would block or need to try again.");
      }
      else {
         log.warning("Connect() failed: %s", strerror(errno));
         Socket->Error = ERR::Failed;
         Socket->setState(NTC::DISCONNECTED);
         return;
      }

      Socket->setState( NTC::CONNECTING);
      RegisterFD((HOSTHANDLE)Socket->SocketHandle, RFD::READ|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&client_server_incoming, Socket);

      // The write queue will be signalled once the connection process is completed.

      RegisterFD((HOSTHANDLE)Socket->SocketHandle, RFD::WRITE|RFD::SOCKET, &client_connect, Socket);
   }
   else {
      log.trace("connect() successful.");

      Socket->setState(NTC::CONNECTED);
      RegisterFD((HOSTHANDLE)Socket->SocketHandle, RFD::READ|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&client_server_incoming, Socket);
   }

#elif _WIN32
   if ((Socket->Error = win_connect(Socket->SocketHandle, (struct sockaddr *)&server_address, sizeof(server_address))) != ERR::Okay) {
      log.warning("connect() failed: %s", GetErrorMsg(Socket->Error));
      return;
   }

   Socket->setState(NTC::CONNECTING); // Connection isn't complete yet - see win32_netresponse() code for NTE_CONNECT
#endif
}

//********************************************************************************************************************
// Action: DataFeed

static ERR NETSOCKET_DataFeed(extNetSocket *Self, struct acDataFeed *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   return ERR::Okay;
}

/*********************************************************************************************************************

-ACTION-
Disable: Disables sending and receiving on the socket.

This method will stop all sending and receiving of data over the socket.  This is irreversible.

-ERRORS-
Okay
Failed: Shutdown operation failed.

*********************************************************************************************************************/

static ERR NETSOCKET_Disable(extNetSocket *Self)
{
   pf::Log log;

   log.trace("");

#ifdef __linux__
   int result = shutdown(Self->SocketHandle, 2);
#elif _WIN32
   int result = win_shutdown(Self->SocketHandle, 2);
#endif

   if (result) { // Zero is success on both platforms
      log.warning("shutdown() failed.");
      return ERR::SystemCall;
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
DisconnectClient: Disconnects all sockets connected to a specific client IP.

For server sockets with client IP connections, this method will terminate all socket connections made to a specific
client IP and free the resources allocated to it.  If #Feedback is defined, a `DISCONNECTED` state message will also
be issued for each socket connection.

If only one socket connection needs to be disconnected, please use #DisconnectSocket().

-INPUT-
struct(*NetClient) Client: The client to be disconnected.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERR NETSOCKET_DisconnectClient(extNetSocket *Self, struct ns::DisconnectClient *Args)
{
   if ((!Args) or (!Args->Client)) return ERR::NullArgs;
   free_client(Self, Args->Client);
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
DisconnectSocket: Disconnects a single socket that is connected to a client IP address.

This method will disconnect a socket connection for a given client.  If #Feedback is defined, a `DISCONNECTED`
state message will also be issued.

-INPUT-
obj(ClientSocket) Socket: The client socket to be disconnected.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERR NETSOCKET_DisconnectSocket(extNetSocket *Self, struct ns::DisconnectSocket *Args)
{
   if ((!Args) or (!Args->Socket)) return ERR::NullArgs;
   free_client_socket(Self, (extClientSocket *)(Args->Socket), true);
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR NETSOCKET_Free(extNetSocket *Self)
{
#ifdef ENABLE_SSL
   sslDisconnect(Self);
#endif

   if (Self->Address) { FreeResource(Self->Address); Self->Address = nullptr; }
   if (Self->NetLookup) { FreeResource(Self->NetLookup); Self->NetLookup = nullptr; }

   free_socket(Self);

   // Remove all client resources

   while (Self->Clients) free_client(Self, Self->Clients);

   Self->~extNetSocket();
   return ERR::Okay;
}

//********************************************************************************************************************
// If a netsocket object is about to be freed, ensure that we are not using the netsocket object in one of our message
// handlers.  We can still delay the free request in any case.

static ERR NETSOCKET_FreeWarning(extNetSocket *Self)
{
   if (Self->InUse) {
      if (!Self->Terminating) { // Check terminating state to prevent flooding of the message queue
         pf::Log log;
         log.msg("NetSocket in use, cannot free yet (request delayed).");
         Self->Terminating = true;
         SendMessage(MSGID::FREE, MSF::NIL, &Self->UID, sizeof(OBJECTID));
      }
      return ERR::InUse;
   }
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
GetLocalIPAddress: Returns the IP address that the socket is locally bound to.

This method performs the POSIX equivalent of `getsockname()`.  It returns the current address to which the NetSocket
is bound.

-INPUT-
struct(*IPAddress) Address:  Pointer to an IPAddress structure which will be set to the result of the query if successful.

-ERRORS-
Okay
NullArgs
Failed
-END-

*********************************************************************************************************************/

static ERR NETSOCKET_GetLocalIPAddress(extNetSocket *Self, struct ns::GetLocalIPAddress *Args)
{
   pf::Log log;

   log.traceBranch();

   if ((!Args) or (!Args->Address)) return log.warning(ERR::NullArgs);

   struct sockaddr_storage addr_storage;
   int result;

#ifdef __linux__
   socklen_t addr_length = sizeof(addr_storage);
   result = getsockname(Self->SocketHandle, (struct sockaddr *)&addr_storage, &addr_length);
#elif _WIN32
   int addr_length = sizeof(addr_storage);
   result = win_getsockname(Self->SocketHandle, (struct sockaddr *)&addr_storage, &addr_length);
#endif

   if (!result) {
      if (addr_storage.ss_family IS AF_INET6) {
         struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr_storage;
         pf::copymem(Args->Address->Data, &addr6->sin6_addr.s6_addr, 16);
         Args->Address->Type = IPADDR::V6;
      }
      else if (addr_storage.ss_family IS AF_INET) {
         struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr_storage;
         Args->Address->Data[0] = net::LongToHost(addr4->sin_addr.s_addr);
         Args->Address->Data[1] = 0;
         Args->Address->Data[2] = 0;
         Args->Address->Data[3] = 0;
         Args->Address->Type = IPADDR::V4;
      }
      else {
         log.warning("Unsupported address family: %d", addr_storage.ss_family);
         return ERR::Failed;
      }
      return ERR::Okay;
   }
   else return log.warning(ERR::Failed);
}

//********************************************************************************************************************

static ERR NETSOCKET_Init(extNetSocket *Self)
{
   pf::Log log;
   ERR error;

   if (Self->SocketHandle != (SOCKET_HANDLE)-1) return ERR::Okay; // The socket has been pre-configured by the developer

#ifdef ENABLE_SSL
   // Initialise the SSL structures (do not perform any connection yet).  Notice the NSF::SSL flag is required
   // for an SSL request, but future SSL checks will instead be reliant on the SSL field having a value.

   if ((Self->Flags & NSF::SSL) != NSF::NIL) {
      if ((error = sslSetup(Self)) != ERR::Okay) return error;
   }
#endif

   // Create the underlying socket

#ifdef __linux__

   // Create socket - IPv6 dual-stack if available, otherwise IPv4
   if ((Self->SocketHandle = socket(PF_INET6, SOCK_STREAM, 0)) != NOHANDLE) {
      Self->IPV6 = true;
      
      // Enable dual-stack mode (accept both IPv4 and IPv6)
      int v6only = 0;
      if (setsockopt(Self->SocketHandle, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) != 0) {
         log.warning("Failed to set dual-stack mode: %s", strerror(errno));
      }
      
      log.trace("Created IPv6 dual-stack socket");
   }
   else if ((Self->SocketHandle = socket(PF_INET, SOCK_STREAM, 0)) != NOHANDLE) {
      Self->IPV6 = false;
      log.trace("Created IPv4 socket");
   }
   else {
      log.warning("Failed to create socket: %s", strerror(errno));
      return ERR::Failed;
   }

   // Put the socket into non-blocking mode, this is required when registering it as an FD and also prevents connect()
   // calls from going to sleep.

   #if 0
      // Was there any reason to use ioctl() when we have fcntl()???
      uint32_t non_blocking = 1;
      int result = ioctl(Self->SocketHandle, FIONBIO, &non_blocking);
      if (result) return log.warning(ERR::Failed);
   #else
      if (fcntl(Self->SocketHandle, F_SETFL, fcntl(Self->SocketHandle, F_GETFL) | O_NONBLOCK)) return log.warning(ERR::Failed);
   #endif

   // Set the send timeout so that connect() will timeout after a reasonable time

   //int timeout = 30000;
   //result = setsockopt(Self->SocketHandle, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
   //assert(result IS 0);

#elif _WIN32

   // Try IPv6 dual-stack socket first, then fall back to IPv4
   bool is_ipv6;
   Self->SocketHandle = win_socket_ipv6(Self, true, false, is_ipv6);
   if (Self->SocketHandle IS NOHANDLE) return ERR::SystemCall;
   Self->IPV6 = is_ipv6;
   log.msg("Created socket on Windows (handle: %d) IPV6: %d", Self->SocketHandle, int(is_ipv6));

#endif

   if ((Self->Flags & NSF::SERVER) != NSF::NIL) {
      if (!Self->Port) return log.warning(ERR::FieldNotSet);

      if (Self->IPV6) {
         #ifdef __linux__
            // IPV6

            struct sockaddr_in6 addr;

            pf::clearmem(&addr, sizeof(addr));
            addr.sin6_family = AF_INET6;
            addr.sin6_port   = net::HostToShort(Self->Port); // Must be passed in in network byte order
            addr.sin6_addr   = in6addr_any;   // Must be passed in in network byte order

            int result;
            int value = 1;
            setsockopt(Self->SocketHandle, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

            if ((result = bind(Self->SocketHandle, (struct sockaddr *)&addr, sizeof(addr))) != -1) {
               listen(Self->SocketHandle, Self->Backlog);
               RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&server_client_connect), Self);
               return ERR::Okay;
            }
            else if (result IS EADDRINUSE) return log.warning(ERR::InUse);
            else return log.warning(ERR::Failed);
         #elif _WIN32
            // Windows IPv6 dual-stack server binding
            struct sockaddr_in6 addr;
            pf::clearmem(&addr, sizeof(addr));
            addr.sin6_family = AF_INET6;
            addr.sin6_port   = net::HostToShort(Self->Port);
            addr.sin6_addr   = in6addr_any;
            
            if ((error = win_bind(Self->SocketHandle, (struct sockaddr *)&addr, sizeof(addr))) IS ERR::Okay) {
               if ((error = win_listen(Self->SocketHandle, Self->Backlog)) IS ERR::Okay) {
                  return ERR::Okay;
               }
               else return log.warning(error);
            }
            else return log.warning(error);
         #else
            return ERR::NoSupport;
         #endif
      }
      else {
         // IPV4
         struct sockaddr_in addr;
         pf::clearmem(&addr, sizeof(addr));
         addr.sin_family = AF_INET;
         addr.sin_port   = net::HostToShort(Self->Port); // Must be passed in in network byte order
         addr.sin_addr.s_addr   = INADDR_ANY;   // Must be passed in in network byte order

         #ifdef __linux__
            int result;
            int value = 1;
            setsockopt(Self->SocketHandle, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

            if ((result = bind(Self->SocketHandle, (struct sockaddr *)&addr, sizeof(addr))) != -1) {
               listen(Self->SocketHandle, Self->Backlog);
               RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&server_client_connect), Self);
               return ERR::Okay;
            }
            else if (result IS EADDRINUSE) return log.warning(ERR::InUse);
            else {
               log.warning("bind() failed with error: %s", strerror(errno));
               return ERR::Failed;
            }
         #elif _WIN32
            if ((error = win_bind(Self->SocketHandle, (struct sockaddr *)&addr, sizeof(addr))) IS ERR::Okay) {
               if ((error = win_listen(Self->SocketHandle, Self->Backlog)) IS ERR::Okay) {
                  return ERR::Okay;
               }
               else return log.warning(error);
            }
            else return log.warning(error);
         #endif
      }
   }
   else if ((Self->Address) and (Self->Port > 0)) {
      if ((error = Self->connect(Self->Address, Self->Port)) != ERR::Okay) {
         return error;
      }
      else return ERR::Okay;
   }
   else return ERR::Okay;
}

//********************************************************************************************************************

static ERR NETSOCKET_NewObject(extNetSocket *Self)
{
   Self->SocketHandle = NOHANDLE;
   Self->Error        = ERR::Okay;
   Self->Backlog      = 10;
   Self->State        = NTC::DISCONNECTED;
   Self->MsgLimit     = 1024768;
   Self->ClientLimit  = 1024;
   #ifdef _WIN32
      Self->WriteSocket = nullptr;
      Self->ReadSocket = nullptr;
   #endif
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR NETSOCKET_NewPlacement(extNetSocket * Self)
{
   new (Self) extNetSocket;
   return ERR::Okay;
}

/*********************************************************************************************************************

-ACTION-
Read: Read information from the socket.

The Read() action will read incoming data from the socket and write it to the provided buffer.  If the socket connection
is safe, success will always be returned by this action regardless of whether or not data was available.  Almost all
other return codes indicate permanent failure and the socket connection will be closed when the action returns.

-ERRORS-
Okay: Read successful (if no data was on the socket, success is still indicated).
NullArgs
Disconnected: The socket connection is closed.
Failed: A permanent failure has occurred and socket has been closed.

*********************************************************************************************************************/

static ERR NETSOCKET_Read(extNetSocket *Self, struct acRead *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Buffer)) return log.warning(ERR::NullArgs);

   if ((Self->Flags & NSF::SERVER) != NSF::NIL) {
      // Not allowed - client must read from the ClientSocket.
      return ERR::NoSupport;
   }
   else { // Read from the server that we're connected to
      if (Self->SocketHandle IS NOHANDLE) return log.warning(ERR::Disconnected);

      Self->ReadCalled = true;

      if (!Args->Length) { Args->Result = 0; return ERR::Okay; }

      ERR error = RECEIVE(Self, Self->SocketHandle, Args->Buffer, Args->Length, 0, &Args->Result);

      if (error != ERR::Okay) {
         log.branch("Freeing socket, error '%s'", GetErrorMsg(error));
         free_socket(Self);
      }

      return error;
   }
}

/*********************************************************************************************************************

-ACTION-
Write: Writes data to the socket.

Writing data to a socket will send raw data to the remote client or server.  Write connections are buffered, so any
data overflow generated in a call to this action will be buffered into a software queue.  Resource limits placed on
the software queue are governed by the #MsgLimit field setting.

Do not use this action if in server mode.  Instead, write to the @ClientSocket object that will receive the data.

It is possible to write to a socket in advance of any connection being made. The netsocket will queue the data
and automatically send it once the first connection has been made.

*********************************************************************************************************************/

static ERR NETSOCKET_Write(extNetSocket *Self, struct acWrite *Args)
{
   pf::Log log;

   if (!Args) return ERR::NullArgs;

   Args->Result = 0;

   if ((Self->Flags & NSF::SERVER) != NSF::NIL) {
      log.warning("DEPRECATED: Write to the target ClientSocket object rather than the NetSocket");
      return ERR::NoSupport;
   }

   if ((Self->SocketHandle IS NOHANDLE) or (Self->State != NTC::CONNECTED)) { // Queue the write prior to server connection
      log.trace("Writing %d bytes to server (queued for connection).", Args->Length);
      Self->write_queue(Self->WriteQueue, Args->Buffer, Args->Length);
      return ERR::Okay;
   }

   // Note that if a write queue has been setup, there is no way that we can write to the server until the queue has
   // been exhausted.  Thus we have add more data to the queue if it already exists.

   size_t len;
   ERR error;
   if (Self->WriteQueue.Buffer.empty()) { // No prior buffer to send
      len = Args->Length;
      error = SEND(Self, Self->SocketHandle, Args->Buffer, &len, 0);
   }
   else {
      len = 0;
      error = ERR::BufferOverflow;
   }

   if ((error != ERR::Okay) or (len < size_t(Args->Length))) {
      if (error != ERR::Okay) log.trace("Error: '%s', queuing %d/%d bytes for transfer...", GetErrorMsg(error), Args->Length - len, Args->Length);
      else log.trace("Queuing %d of %d remaining bytes for transfer...", Args->Length - len, Args->Length);
      if ((error IS ERR::DataSize) or (error IS ERR::BufferOverflow) or (len > 0))  {
         Self->write_queue(Self->WriteQueue, (BYTE *)Args->Buffer + len, Args->Length - len);
         #ifdef __linux__
            RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD::WRITE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&client_server_outgoing), Self);
         #elif _WIN32
            win_socketstate(Self->SocketHandle, -1, true);
            Self->WriteSocket = &client_server_outgoing;
         #endif
      }
   }
   else log.trace("Successfully wrote all %d bytes to the server.", Args->Length);

   Args->Result = Args->Length;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Address: An IP address or domain name to connect to.

If this field is set with an IP address or domain name prior to initialisation, an attempt to connect to that location
will be made when the NetSocket is initialised.  Post-initialisation this field cannot be set by the client, however
calls to #Connect() will result in it being updated so that it always reflects the named address of the current
connection.

*********************************************************************************************************************/

static ERR SET_Address(extNetSocket *Self, CSTRING Value)
{
   if (Self->Address) { FreeResource(Self->Address); Self->Address = nullptr; }
   if (Value) Self->Address = pf::strclone(Value);
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Backlog: The maximum number of connections that can be queued against the socket.

Incoming connections to NetSocket objects are queued until they are answered by the object.  As there is a limit to the
number of connections that can be queued, you can adjust the backlog by writing this field.  The default setting is 10
connections.

If the backlog is exceeded, subsequent connections to the socket will typically be met with a connection refused error.

-FIELD-
ClientLimit: The maximum number of clients that can be connected to a server socket.

-FIELD-
Clients: For server sockets, lists all clients connected to the server.

-FIELD-
Error: Information about the last error that occurred during a NetSocket operation

This field describes the last error that occurred during a NetSocket operation:

In the case where a NetSocket object enters the `NTC::DISCONNECTED` state from the `NTC::CONNECTED` state, this field
can be used to determine how a TCP connection was closed.

<types type="Error">
<type name="ERR::Okay">The connection was closed gracefully.  All data sent by the peer has been received.</>
<type name="ERR::Disconnected">The connection was broken in a non-graceful fashion. Data may be lost.</>
<type name="ERR::TimeOut">The connect operation timed out.</>
<type name="ERR::ConnectionRefused">The connection was refused by the remote host.  Note: This error will not occur on Windows, and instead the Error field will be set to `ERR::Failed`.</>
<type name="ERR::NetworkUnreachable">The network was unreachable.  Note: This error will not occur on Windows, and instead the Error field will be set to `ERR::Failed`.</>
<type name="ERR::HostUnreachable">No path to host was found.  Note: This error will not occur on Windows, and instead the Error field will be set to `ERR::Failed`.</>
<type name="ERR::Failed">An unspecified error occurred.</>
</>

-FIELD-
Feedback: A callback trigger for when the state of the NetSocket is changed.

The client can define a function in this field to receive notifications whenever the state of the socket changes -
typically a new connection or a disconnect.  This includes activity both from the server and the client side if the 
`ClientSocket` value is set on receipt.

The function must be in the format `Function(*NetSocket, *ClientSocket, NTC State)`

The `NetSocket` parameter refers to the NetSocket object to which the function is subscribed.

*********************************************************************************************************************/

static ERR GET_Feedback(extNetSocket *Self, FUNCTION **Value)
{
   if (Self->Feedback.defined()) {
      *Value = &Self->Feedback;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_Feedback(extNetSocket *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->Feedback.isScript()) UnsubscribeAction(Self->Feedback.Context, AC::Free);
      Self->Feedback = *Value;
      if (Self->Feedback.isScript()) {
         SubscribeAction(Self->Feedback.Context, AC::Free, C_FUNCTION(notify_free_feedback));
      }
   }
   else Self->Feedback.clear();

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Flags: Optional flags.

-FIELD-
Incoming: Callback that is triggered when the socket receives data.

The Incoming field can be set with a custom function that will be called whenever the socket receives data.  The
function prototype is `ERR Incoming(*NetSocket, OBJECTPTR Context)`.

The `NetSocket` parameter refers to the NetSocket object.  The `Context` refers to the object that set the `Incoming` field.

Retrieve data from the socket with the #Read() action. Reading at least some of the data from the socket is
compulsory - if the function does not do this then the data will be cleared from the socket when the function returns.
If the callback function returns `ERR::Terminate` then the Incoming field will be cleared and the function will no
longer be called.  All other error codes are ignored.

*********************************************************************************************************************/

static ERR GET_Incoming(extNetSocket *Self, FUNCTION **Value)
{
   if (Self->Incoming.defined()) {
      *Value = &Self->Incoming;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_Incoming(extNetSocket *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->Incoming.isScript()) UnsubscribeAction(Self->Incoming.Context, AC::Free);
      Self->Incoming = *Value;
      if (Self->Incoming.isScript()) {
         SubscribeAction(Self->Incoming.Context, AC::Free, C_FUNCTION(notify_free_incoming));
      }
   }
   else Self->Incoming.clear();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Outgoing: Callback that is triggered when a socket is ready to send data.

The Outgoing field can be set with a custom function that will be called whenever the socket is ready to send data.
The function must be in the format `ERR Outgoing(*NetSocket, OBJECTPTR Context)`

The NetSocket parameter refers to the NetSocket object.  The Context refers to the object that set the Outgoing field.

To send data to the NetSocket object, call the #Write() action.  If the callback function returns
`ERR::Terminate` then the Outgoing field will be cleared and the function will no longer be called.  All other error
codes are ignored.

The Outgoing field is ineffective if the NetSocket is in server mode (target a connected client socket instead).

*********************************************************************************************************************/

static ERR GET_Outgoing(extNetSocket *Self, FUNCTION **Value)
{
   if (Self->Incoming.defined()) {
      *Value = &Self->Incoming;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_Outgoing(extNetSocket *Self, FUNCTION *Value)
{
   pf::Log log;

   if ((Self->Flags & NSF::SERVER) != NSF::NIL) {
      return log.warning(ERR::NoSupport);
   }
   else {
      if (Self->Outgoing.isScript()) UnsubscribeAction(Self->Outgoing.Context, AC::Free);
      Self->Outgoing = *Value;
      if (Self->Outgoing.isScript()) SubscribeAction(Self->Outgoing.Context, AC::Free, C_FUNCTION(notify_free_outgoing));

      if (Self->initialised()) {
         if ((Self->SocketHandle != NOHANDLE) and (Self->State IS NTC::CONNECTED)) {
            // Setting the Outgoing field after connectivity is established will put the socket into streamed write mode.

            #ifdef __linux__
               RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD::WRITE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&client_server_outgoing), Self);
            #elif _WIN32
               win_socketstate(Self->SocketHandle, -1, true);
               Self->WriteSocket = &client_server_outgoing;
            #endif
         }
         else log.trace("Will not listen for socket-writes (no socket handle, or state %d != NTC::CONNECTED).", Self->State);
      }
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
MsgLimit: Limits the size of incoming and outgoing data packets.

This field limits the size of incoming and outgoing message queues (each socket connection receives two queues assigned
to both incoming and outgoing messages).  The size is defined in bytes.  Sending or receiving messages that overflow
the queue results in the connection being terminated with an error.

The default setting is 1 megabyte.

-FIELD-
OutQueueSize: The number of bytes on the socket's outgoing queue.

*********************************************************************************************************************/

static ERR GET_OutQueueSize(extNetSocket *Self, int *Value)
{
   *Value = Self->WriteQueue.Buffer.size();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Port: The port number to use for initiating a connection.

-FIELD-
SocketHandle: Platform specific reference to the network socket handle.

*********************************************************************************************************************/

static ERR GET_SocketHandle(extNetSocket *Self, APTR *Value)
{
   *Value = (APTR)(MAXINT)Self->SocketHandle;
   return ERR::Okay;
}

static ERR SET_SocketHandle(extNetSocket *Self, APTR Value)
{
   // The user can set SocketHandle prior to initialisation in order to create a NetSocket object that is linked to a
   // socket created from outside the core platform code base.

   Self->SocketHandle = (SOCKET_HANDLE)(MAXINT)Value;
   Self->ExternalSocket = true;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
State: The current connection state of the NetSocket object.

*********************************************************************************************************************/

static ERR SET_State(extNetSocket *Self, NTC Value)
{
   pf::Log log;

   if (Value != Self->State) {
      if ((Self->Flags & NSF::LOG_ALL) != NSF::NIL) log.msg("State changed from %d to %d", int(Self->State), int(Value));

      #ifdef ENABLE_SSL
      if ((Self->State IS NTC::CONNECTING_SSL) and (Value IS NTC::CONNECTED)) {
         // SSL connection has just been established

         #ifdef _WIN32
         if (Self->WinSSL) {
            if (ssl_wrapper_get_verify_result(Self->WinSSL) != 0) { // Handle the failed verification
               log.trace("SSL certification was not validated.");
            }
            else log.trace("SSL certification is valid.");
         }
         #else
         if (Self->SSL) {
            if (SSL_get_verify_result(Self->SSL) != X509_V_OK) { // Handle the failed verification
               log.trace("SSL certification was not validated.");
            }
            else log.trace("SSL certification is valid.");
         }
         #endif
      }
      #endif

      Self->State = Value;

      if (Self->Feedback.defined()) {
         log.traceBranch("Reporting state change to subscriber, operation %d, context %p.", int(Self->State), Self->Feedback.Context);

         if (Self->Feedback.isC()) {
            pf::SwitchContext context(Self->Feedback.Context);
            auto routine = (void (*)(extNetSocket *, objClientSocket *, NTC, APTR))Self->Feedback.Routine;
            if (routine) routine(Self, nullptr, Self->State, Self->Feedback.Meta);
         }
         else if (Self->Feedback.isScript()) {
            sc::Call(Self->Feedback, std::to_array<ScriptArg>({
               { "NetSocket",    Self, FD_OBJECTPTR },
               { "ClientSocket", APTR(nullptr), FD_OBJECTPTR },
               { "State",        int(Self->State) }
            }));
         }
      }

      if ((Self->State IS NTC::CONNECTED) and ((!Self->WriteQueue.Buffer.empty()) or (Self->Outgoing.defined()))) {
         log.msg("Sending queued data to server on connection.");
         #ifdef __linux__
            RegisterFD((HOSTHANDLE)Self->SocketHandle, RFD::WRITE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&client_server_outgoing), Self);
         #elif _WIN32
            win_socketstate(Self->SocketHandle, -1, true);
            Self->WriteSocket = &client_server_outgoing;
         #endif
      }
   }

   SetResourcePtr(RES::EXCEPTION_HANDLER, nullptr); // Stop winsock from fooling with our exception handler

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
TotalClients: Indicates the total number of clients currently connected to the socket (if in server mode).

In server mode, the NetSocket will maintain a count of the total number of clients currently connected to the socket.
You can read the total number of connections from this field.

In client mode, this field is always set to zero.

-FIELD-
ClientData: A client-defined value that can be useful in action notify events.

This is a free-entry field value that can store client data for future reference.

-FIELD-
ValidCert: Indicates certificate validity if the socket is encrypted with a certificate.

After an encrypted connection has been made to a server, the ValidCert field can be used to determine the validity of
the server's certificate.

If encrypted communication is not supported, `ERR::NoSupport` is returned.  If the certificate is valid or the
connection is not encrypted, a value of zero is returned to indicate that the connection is valid.
-END-

*********************************************************************************************************************/

static ERR GET_ValidCert(extNetSocket *Self, int *Value)
{
#ifdef ENABLE_SSL
   #ifdef _WIN32
   if ((Self->WinSSL) and (Self->State IS NTC::CONNECTED)) {
      *Value = ssl_wrapper_get_verify_result(Self->WinSSL);
   }
   else *Value = 0;
   #else
   if ((Self->SSL) and (Self->State IS NTC::CONNECTED)) {
      *Value = SSL_get_verify_result(Self->SSL);
   }
   else *Value = 0;
   #endif

   return ERR::Okay;
#else
   return ERR::NoSupport;
#endif
}

//********************************************************************************************************************

static void free_socket(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   log.branch("Handle: %d", Self->SocketHandle);

   if (Self->SocketHandle != NOHANDLE) {
      log.trace("Deregistering socket.");
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
      DeregisterFD((HOSTHANDLE)Self->SocketHandle);
#pragma GCC diagnostic warning "-Wint-to-pointer-cast"

      if (!Self->ExternalSocket) { CLOSESOCKET(Self->SocketHandle); Self->SocketHandle = NOHANDLE; }
   }

   #ifdef _WIN32
      Self->WriteSocket = nullptr;
      Self->ReadSocket  = nullptr;
   #endif

   Self->ReadQueue.Buffer.clear();
   Self->WriteQueue.Buffer.clear();
   Self->ReadQueue.Index = 0;
   Self->WriteQueue.Index = 0;

   if (!Self->terminating()) {
      if (Self->State != NTC::DISCONNECTED) {
         log.traceBranch("Changing state to disconnected.");
         Self->setState(NTC::DISCONNECTED);
      }
   }

   log.trace("Resetting exception handler.");

   SetResourcePtr(RES::EXCEPTION_HANDLER, nullptr); // Stop winsock from fooling with our exception handler
}

//********************************************************************************************************************
// Store data in the write queue

ERR extNetSocket::write_queue(NetQueue &Queue, CPTR Message, size_t Length)
{
   pf::Log log(__FUNCTION__);

   if (!Message) return log.warning(ERR::NullArgs);
   if (Length <= 0) return ERR::Okay;
   if (Length > size_t(MsgLimit)) return log.warning(ERR::BufferOverflow);

   if (!Queue.Buffer.empty()) { // Add data to existing queue
      uint32_t remaining_data = Queue.Buffer.size() - Queue.Index;

      if (Queue.Index > 8192) { // Compact the queue
         if (remaining_data > 0) Queue.Buffer.erase(Queue.Buffer.begin(), Queue.Buffer.begin() + Queue.Index);
         else Queue.Buffer.clear();
         Queue.Index = 0;
      }

      Queue.Buffer.resize(Queue.Buffer.size() + Length);
      pf::copymem(Message, Queue.Buffer.data() + Queue.Buffer.size() - Length, Length);
   }
   else {
      Queue.Buffer.resize(Length);
      Queue.Index = 0;
      pf::copymem(Message, Queue.Buffer.data(), Length);
   }

   return ERR::Okay;
}

//********************************************************************************************************************
// This function is called from winsockwrappers.c whenever a network event occurs on a NetSocket.  Callbacks
// set against the NetSocket object will send/receive data on the socket.
//
// Recursion typically occurs on calls to ProcessMessages() during incoming and outgoing data transmissions.  This is
// not important if the same transmission message is being repeated, but does require careful management if, for
// example, a disconnection were to occur during a read/write operation for example.  Using DataFeeds is a more
// reliable method of managing recursion problems, but burdens the message queue.

#ifdef _WIN32
void win32_netresponse(OBJECTPTR SocketObject, SOCKET_HANDLE SocketHandle, int Message, ERR Error)
{
   pf::Log log(__FUNCTION__);

   extNetSocket *Socket;
   objClientSocket *ClientSocket;

   if (SocketObject->classID() IS CLASSID::CLIENTSOCKET) {
      ClientSocket = (objClientSocket *)SocketObject;
      Socket = (extNetSocket *)ClientSocket->Client->NetSocket;
   }
   else {
      Socket = (extNetSocket *)SocketObject;
      ClientSocket = nullptr;
   }

   #if defined(_DEBUG) || defined(ENABLE_TRACE_MSGS)
   static constexpr const char* const msg[] = { "None", "Write", "Read", "Accept", "Connect", "Close" };
   log.traceBranch("[%d:%d:%p], %s, Error %d, InUse: %d, WinRecursion: %d", Socket->UID, SocketHandle, ClientSocket, msg[Message], int(Error), Socket->InUse, Socket->WinRecursion);
   #endif

   // Safety first

   pf::ScopedObjectLock lock(Socket);
   if (!lock.granted()) return;

   pf::ScopedObjectLock lock_client(ClientSocket); // Not locked if ClientSocket is nullptr
   if ((ClientSocket) and (!lock_client.granted())) return;

   pf::SwitchContext context(Socket);

   Socket->InUse++;

   if (Message IS NTE_READ) {
      if (Error != ERR::Okay) log.warning("Socket failed on incoming data, error %d.", int(Error));

      #ifdef NO_NETRECURSION
         if (Socket->WinRecursion) {
            log.trace("Recursion detected (read request)");
            Socket->InUse--;
            return;
         }
         else {
            Socket->WinRecursion++;
            #pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
            if (ClientSocket) clientsocket_incoming((HOSTHANDLE)SocketHandle, ClientSocket);
            else if (Socket->ReadSocket) Socket->ReadSocket(0, Socket);
            else client_server_incoming(0, Socket);
            #pragma GCC diagnostic warning "-Wint-to-pointer-cast"
            Socket->WinRecursion--;
         }
      #else
         if (Socket->ReadSocket) Socket->ReadSocket(0, Socket);
         else client_server_incoming(0, Socket);
      #endif
   }
   else if (Message IS NTE_WRITE) {
      if (Error != ERR::Okay) log.warning("Socket failed on outgoing data, error %d.", int(Error));

      #ifdef NO_NETRECURSION
         if (Socket->WinRecursion) {
            log.trace("Recursion detected (write request)");
            Socket->InUse--;
            return;
         }
         else {
            Socket->WinRecursion++;
            #pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
            if (ClientSocket) clientsocket_outgoing((HOSTHANDLE)SocketHandle, ClientSocket);
            else if (Socket->WriteSocket) Socket->WriteSocket(0, Socket);
            else client_server_outgoing(0, Socket);
            #pragma GCC diagnostic warning "-Wint-to-pointer-cast"
            Socket->WinRecursion--;
         }
      #else
         if (Socket->WriteSocket) Socket->WriteSocket(0, Socket);
         else client_server_outgoing(0, Socket);
      #endif
   }
   else if (Message IS NTE_CLOSE) {
      if (ClientSocket) {
         if ((Socket->Flags & NSF::LOG_ALL) != NSF::NIL) log.msg("Connection closed by client.");
         FreeResource(ClientSocket);
      }
      else {
         if ((Socket->Flags & NSF::LOG_ALL) != NSF::NIL) log.msg("Connection closed by server, error %d.", int(Error));

         // Prevent multiple close messages from the same socket
         if (Socket->State IS NTC::DISCONNECTED) {
            log.trace("Ignoring duplicate close message for socket %d", SocketHandle);
            Socket->InUse--;
            return;
         }

         Socket->setState(NTC::DISCONNECTED);
         free_socket(Socket);
      }
   }
   else if (Message IS NTE_ACCEPT) {
      log.traceBranch("Accept message received for new client %d.", SocketHandle);
      server_client_connect(Socket->SocketHandle, Socket);
   }
   else if (Message IS NTE_CONNECT) {
      if (Error IS ERR::Okay) {
         if ((Socket->Flags & NSF::LOG_ALL) != NSF::NIL) log.msg("Connection to server granted.");

         #ifdef ENABLE_SSL
            if (Socket->WinSSL) {
               log.traceBranch("Attempting SSL handshake.");
               sslConnect(Socket);

               if (Socket->State IS NTC::CONNECTING_SSL) {
                  // In Windows, socket handles are managed in win_messages().
                  //RegisterFD((HOSTHANDLE)Socket->SocketHandle, RFD::READ|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&client_server_incoming, Socket);
               }
            }
            else Socket->setState(NTC::CONNECTED);
         #else
            Socket->setState(NTC::CONNECTED);
         #endif
      }
      else {
         log.msg("Connection state changed, error: %s", GetErrorMsg(Error));
         Socket->Error = Error;
         Socket->setState(NTC::DISCONNECTED);
      }
   }

   Socket->InUse--;
}
#endif

//********************************************************************************************************************

static const FieldDef clValidCert[] = {
   { "Okay",                          SCV_OK },                                 // The operation was successful.
   { "UnableToGetIssuerCert",         SCV_UNABLE_TO_GET_ISSUER_CERT },          // unable to get issuer certificate the issuer certificate could not be found: this occurs if the issuer certificate of an untrusted certificate cannot be found.
   { "UnableToGetCRL",                SCV_UNABLE_TO_GET_CRL },                  // unable to get certificate CRL the CRL of a certificate could not be found. Unused.
   { "UnableToDecryptCertSignature",  SCV_UNABLE_TO_DECRYPT_CERT_SIGNATURE },   // unable to decrypt certificate's signature the certificate signature could not be decrypted. This means that the actual signature value could not be determined rather than it not matching the expected value, this is only meaningful for RSA keys.
   { "UnableToDecryptCRLSignature",   SCV_UNABLE_TO_DECRYPT_CRL_SIGNATURE },    // unable to decrypt CRL's signature the CRL signature could not be decrypted: this means that the actual signature value could not be determined rather than it not matching the expected value. Unused.
   { "UnableToDecodeIssuerPublicKey", SCV_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY }, // unable to decode issuer public key the public key in the certificate SubjectPublicKeyInfo could not be read.
   { "CertSignatureFailure",          SCV_CERT_SIGNATURE_FAILURE },             // certificate signature failure the signature of the certificate is invalid.
   { "CRLSignuatreFailure",           SCV_CRL_SIGNATURE_FAILURE },              // CRL signature failure the signature of the certificate is invalid. Unused.
   { "CertNotYetValid",               SCV_CERT_NOT_YET_VALID },                 // certificate is not yet valid the certificate is not yet valid: the notBefore date is after the current time.
   { "CertHasExpired",                SCV_CERT_HAS_EXPIRED },                   // certificate has expired the certificate has expired: that is the notAfter date is before the current time.
   { "CRLNotYetValid",                SCV_CRL_NOT_YET_VALID },                  // CRL is not yet valid the CRL is not yet valid. Unused.
   { "CRLHasExpired",                 SCV_CRL_HAS_EXPIRED },                    // CRL has expired the CRL has expired. Unused.
   { "ErrorInCertNotBeforeField",     SCV_ERROR_IN_CERT_NOT_BEFORE_FIELD },     // format error in certificate's notBefore field the certificate notBefore field contains an invalid time.
   { "ErrorInCertNotAfterField",      SCV_ERROR_IN_CERT_NOT_AFTER_FIELD },      // format error in certificate's notAfter field the certificate notAfter field contains an invalid time.
   { "ErrorInCRLLastUpdateField",     SCV_ERROR_IN_CRL_LAST_UPDATE_FIELD },     // format error in CRL's lastUpdate field the CRL lastUpdate field contains an invalid time. Unused.
   { "ErrorInCRLNextUpdateField",     SCV_ERROR_IN_CRL_NEXT_UPDATE_FIELD },     // format error in CRL's nextUpdate field the CRL nextUpdate field contains an invalid time. Unused.
   { "OutOfMemory",                   SCV_OUT_OF_MEM },                         // out of memory an error occurred trying to allocate memory. This should never happen.
   { "DepthZeroSelfSignedCert",       SCV_DEPTH_ZERO_SELF_SIGNED_CERT },        // self signed certificate the passed certificate is self signed and the same certificate cannot be found in the list of trusted certificates.
   { "SelfSignedCertInChain",         SCV_SELF_SIGNED_CERT_IN_CHAIN },          // self signed certificate in certificate chain the certificate chain could be built up using the untrusted certificates but the root could not be found locally.
   { "UnableToGetIssuerCertLocally",  SCV_UNABLE_TO_GET_ISSUER_CERT_LOCALLY },  // unable to get local issuer certificate the issuer certificate of a locally looked up certificate could not be found. This normally means the list of trusted certificates is not complete.
   { "UnableToVertifyLeafSignature",  SCV_UNABLE_TO_VERIFY_LEAF_SIGNATURE },    // unable to verify the first certificate no signatures could be verified because the chain contains only one certificate and it is not self signed.
   { "CertChainTooLong",              SCV_CERT_CHAIN_TOO_LONG },                // certificate chain too long the certificate chain length is greater than the supplied maximum depth. Unused.
   { "CertRevoked",                   SCV_CERT_REVOKED },                       // certificate revoked the certificate has been revoked. Unused.
   { "InvalidCA",                     SCV_INVALID_CA },                         // invalid CA certificate a CA certificate is invalid. Either it is not a CA or its extensions are not consistent with the supplied purpose.
   { "PathLengthExceeded",            SCV_PATH_LENGTH_EXCEEDED },               // path length constraint exceeded the basicConstraints pathlength parameter has been exceeded.
   { "InvalidPurpose",                SCV_INVALID_PURPOSE },                    // unsupported certificate purpose the supplied certificate cannot be used for the specified purpose.
   { "CertUntrusted",                 SCV_CERT_UNTRUSTED },                     // certificate not trusted the root CA is not marked as trusted for the specified purpose.
   { "CertRejected",                  SCV_CERT_REJECTED },                      // certificate rejected the root CA is marked to reject the specified purpose.
   { "SubjectIssuerMismatch",         SCV_SUBJECT_ISSUER_MISMATCH },            // subject issuer mismatch the current candidate issuer certificate was rejected because its subject name did not match the issuer name of the current certificate. Only dis-played when the -issuer_checks option is set.
   { "KeyMismatch",                   SCV_AKID_SKID_MISMATCH },                 // authority and subject key identifier mismatch the current candidate issuer certificate was rejected because its subject key identifier was present and did not match the authority key identifier current certificate. Only displayed when the -issuer_checks option is set.
   { "KeyIssuerSerialMismatch",       SCV_AKID_ISSUER_SERIAL_MISMATCH },        // authority and issuer serial number mismatch the current candidate issuer certificate was rejected because its issuer name and serial number was present and did not match the authority key identifier of the current certificate. Only displayed when the -issuer_checks option is set.
   { "KeyUsageNoCertSign",            SCV_KEYUSAGE_NO_CERTSIGN },               // key usage does not include certificate signing the current candidate issuer certificate was rejected because its keyUsage extension does not permit certificate signing.
   { "ApplicationVerification",       SCV_APPLICATION_VERIFICATION },           // application verification failure an application specific error. Unused.
   { 0, 0 },
};

#include "netsocket_def.c"

static const FieldArray clSocketFields[] = {
   { "Clients",          FDF_POINTER|FDF_STRUCT|FDF_R, nullptr, nullptr, "NetClient" },
   { "ClientData",       FDF_POINTER|FDF_RW },
   { "Address",          FDF_STRING|FDF_RI, nullptr, SET_Address },
   { "State",            FDF_INT|FDF_LOOKUP|FDF_RW, nullptr, SET_State, &clNetSocketState },
   { "Error",            FDF_INT|FDF_R },
   { "Port",             FDF_INT|FDF_RI },
   { "Flags",            FDF_INTFLAGS|FDF_RW, nullptr, nullptr, &clNetSocketFlags },
   { "TotalClients",     FDF_INT|FDF_R },
   { "Backlog",          FDF_INT|FDF_RI },
   { "ClientLimit",      FDF_INT|FDF_RW },
   { "MsgLimit",         FDF_INT|FDF_RI },
   // Virtual fields
   { "SocketHandle",     FDF_POINTER|FDF_RI,     GET_SocketHandle, SET_SocketHandle },
   { "Feedback",         FDF_FUNCTIONPTR|FDF_RW, GET_Feedback, SET_Feedback },
   { "Incoming",         FDF_FUNCTIONPTR|FDF_RW, GET_Incoming, SET_Incoming },
   { "Outgoing",         FDF_FUNCTIONPTR|FDF_W,  GET_Outgoing, SET_Outgoing },
   { "OutQueueSize",     FDF_INT|FDF_R,          GET_OutQueueSize },
   { "ValidCert",        FDF_INT|FDF_LOOKUP,     GET_ValidCert, nullptr, &clValidCert },
   END_FIELD
};

#include "netsocket_server.cpp"
#include "netsocket_client.cpp"

//********************************************************************************************************************

static ERR init_netsocket(void)
{
   clNetSocket = objMetaClass::create::global(
      fl::ClassVersion(VER_NETSOCKET),
      fl::Name("NetSocket"),
      fl::Category(CCF::NETWORK),
      fl::Actions(clNetSocketActions),
      fl::Methods(clNetSocketMethods),
      fl::Fields(clSocketFields),
      fl::Size(sizeof(extNetSocket)),
      fl::Path(MOD_PATH));

   return clNetSocket ? ERR::Okay : ERR::AddClass;
}
