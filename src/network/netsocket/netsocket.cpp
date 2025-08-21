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

<header>SSL Server Certificates</>

For SSL server sockets, custom certificates can be specified using the #SSLCertificate field. Both PEM and PKCS#12
formats are supported across all platforms.

Example with PKCS#12 certificate:

```
netsocket = obj.new('netsocket', {
   flags = 'SERVER|SSL',
   port = 8443,
   sslCertificate = 'config:ssl/server.p12',
   sslKeyPassword = 'password123'
})
```

Example with PEM certificate and separate private key:

```
netsocket = obj.new('netsocket', {
   flags = 'SERVER|SSL',
   port = 8443,
   sslCertificate = 'config:ssl/server.crt',
   sslPrivateKey = 'config:ssl/server.key'
})
```

If no custom certificate is specified, the framework will automatically use a localhost self-signed certificate
for development purposes.  For production use, always specify a proper certificate signed by a trusted CA.

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

static void netsocket_incoming(SOCKET_HANDLE, extNetSocket *);
static void netsocket_outgoing(SOCKET_HANDLE, extNetSocket *);
static void server_incoming_from_client(HOSTHANDLE, extClientSocket *);
static void clientsocket_outgoing(HOSTHANDLE, extClientSocket *);
static void free_client(extNetSocket *, objNetClient *);
static void server_accept_client(SOCKET_HANDLE, extNetSocket *);
static void free_socket(extNetSocket *);
static CSTRING netsocket_state(NTC Value);

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

   if (!Self->Handle) return log.warning(ERR::NotInitialised);

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

   Self->setState(NTC::RESOLVING);

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
      Socket->Error = ERR::HostNotFound;
      Socket->setState(NTC::DISCONNECTED);
      return;
   }

   log.msg("Received callback on DNS resolution.  Handle: %d", Socket->Handle);

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

         if ((Socket->Error = win_connect(Socket->Handle, (struct sockaddr *)&server_address6, sizeof(server_address6))) != ERR::Okay) {
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

         int result = connect(Socket->Handle, (struct sockaddr *)&server_address6, sizeof(server_address6));

         if (result IS -1) {
            if (errno IS EINPROGRESS) {
               log.trace("IPv6 connection in progress...");
            }
            else if ((errno IS EWOULDBLOCK) or (errno IS EAGAIN)) {
               log.trace("IPv6 connect() attempt would block or need to try again.");
            }
            else {
               log.warning("IPv6 Connect() failed: %s", strerror(errno));
               Socket->Error = ERR::SystemCall;
               Socket->setState(NTC::DISCONNECTED);
               return;
            }

            Socket->setState(NTC::CONNECTING);
            RegisterFD((HOSTHANDLE)Socket->Handle, RFD::READ|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&netsocket_incoming, Socket);
            RegisterFD((HOSTHANDLE)Socket->Handle, RFD::WRITE|RFD::SOCKET, &client_connect, Socket);
         }
         else {
            log.trace("IPv6 connect() successful.");
            Socket->setState(NTC::CONNECTED);
            RegisterFD((HOSTHANDLE)Socket->Handle, RFD::READ|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&netsocket_incoming, Socket);
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

         int result = connect(Socket->Handle, (struct sockaddr *)&server_address6, sizeof(server_address6));

         if (result IS -1) {
            if (errno IS EINPROGRESS) {
               log.trace("IPv4-mapped IPv6 connection in progress...");
            }
            else if ((errno IS EWOULDBLOCK) or (errno IS EAGAIN)) {
               log.trace("IPv4-mapped IPv6 connect() attempt would block or need to try again.");
            }
            else {
               log.warning("IPv4-mapped IPv6 Connect() failed: %s", strerror(errno));
               Socket->Error = ERR::SystemCall;
               Socket->setState(NTC::DISCONNECTED);
               return;
            }

            Socket->setState(NTC::CONNECTING);
            RegisterFD((HOSTHANDLE)Socket->Handle, RFD::READ|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&netsocket_incoming, Socket);
            RegisterFD((HOSTHANDLE)Socket->Handle, RFD::WRITE|RFD::SOCKET, &client_connect, Socket);
         }
         else {
            log.trace("IPv4-mapped IPv6 connect() successful.");
            Socket->setState(NTC::CONNECTED);
            RegisterFD((HOSTHANDLE)Socket->Handle, RFD::READ|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&netsocket_incoming, Socket);
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

         if (win_connect(Socket->Handle, (struct sockaddr *)&server_address6, sizeof(server_address6)) IS ERR::Okay) {
            log.trace("IPv4-mapped IPv6 connection initiated successfully");
            Socket->setState(NTC::CONNECTING);
         }
         else {
            log.trace("IPv4-mapped IPv6 connect() failed");
            Socket->Error = ERR::SystemCall;
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
   int result = connect(Socket->Handle, (struct sockaddr *)&server_address, sizeof(server_address));

   if (result IS -1) {
      if (errno IS EINPROGRESS) {
         log.trace("Connection in progress...");
      }
      else if ((errno IS EWOULDBLOCK) or (errno IS EAGAIN)) {
         log.trace("connect() attempt would block or need to try again.");
      }
      else {
         log.warning("Connect() failed: %s", strerror(errno));
         Socket->Error = ERR::SystemCall;
         Socket->setState(NTC::DISCONNECTED);
         return;
      }

      Socket->setState(NTC::CONNECTING);
      RegisterFD((HOSTHANDLE)Socket->Handle, RFD::READ|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&netsocket_incoming, Socket);

      // The write queue will be signalled once the connection process is completed.

      RegisterFD((HOSTHANDLE)Socket->Handle, RFD::WRITE|RFD::SOCKET, &client_connect, Socket);
   }
   else {
      log.trace("connect() successful.");

      Socket->setState(NTC::CONNECTED);
      RegisterFD((HOSTHANDLE)Socket->Handle, RFD::READ|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&netsocket_incoming, Socket);
   }

#elif _WIN32
   if ((Socket->Error = win_connect(Socket->Handle, (struct sockaddr *)&server_address, sizeof(server_address))) != ERR::Okay) {
      log.warning("connect() failed: %s", GetErrorMsg(Socket->Error));
      return;
   }

   Socket->setState(NTC::CONNECTING); // Connection isn't complete yet - see win32_netresponse() code for NTE_CONNECT
#endif
}

//********************************************************************************************************************
// TODO: Accept data-feed content for writing to the socket.

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
   int result = shutdown(Self->Handle, 2);
#elif _WIN32
   int result = win_shutdown(Self->Handle, 2);
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
obj(NetClient) Client: The client to be disconnected.

-ERRORS-
Okay
NullArgs
WrongClass: The Client object is not of type `NetClient`.
-END-

*********************************************************************************************************************/

static ERR NETSOCKET_DisconnectClient(extNetSocket *Self, struct ns::DisconnectClient *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Client)) return ERR::NullArgs;

   if (Args->Client->classID() != CLASSID::NETCLIENT) return log.warning(ERR::WrongClass);

   log.branch("Disconnecting client #%d", Args->Client->UID);
   free_client(Self, Args->Client);
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
DisconnectSocket: Disconnects a single socket that is connected to a client IP address.

This method will disconnect a socket connection for a given client.  If #Feedback is defined, a `DISCONNECTED`
state message will also be issued.

NOTE: To terminate the connection of a socket acting as the client, either free the object or return/raise
`ERR::Terminate` during #Incoming feedback.

-INPUT-
obj(ClientSocket) Socket: The client socket to be disconnected.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERR NETSOCKET_DisconnectSocket(extNetSocket *Self, struct ns::DisconnectSocket *Args)
{
   pf::Log log;
   if ((!Args) or (!Args->Socket)) return log.warning(ERR::NullArgs);
   if (Args->Socket->classID() != CLASSID::CLIENTSOCKET) return log.warning(ERR::WrongClass);
   FreeResource(Args->Socket); // Disconnects & sends a Feedback message
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR NETSOCKET_Free(extNetSocket *Self)
{
#ifndef DISABLE_SSL
   sslDisconnect(Self);
#endif

   if (Self->Address)        { FreeResource(Self->Address); Self->Address = nullptr; }
   if (Self->NetLookup)      { FreeResource(Self->NetLookup); Self->NetLookup = nullptr; }
   if (Self->SSLCertificate) { FreeResource(Self->SSLCertificate); Self->SSLCertificate = nullptr; }
   if (Self->SSLKeyPassword) { FreeResource(Self->SSLKeyPassword); Self->SSLKeyPassword = nullptr; }
   if (Self->SSLPrivateKey)  { FreeResource(Self->SSLPrivateKey); Self->SSLPrivateKey = nullptr; }

   if (Self->Feedback.isScript()) UnsubscribeAction(Self->Feedback.Context, AC::Free);
   if (Self->Incoming.isScript()) UnsubscribeAction(Self->Incoming.Context, AC::Free);
   if (Self->Outgoing.isScript()) UnsubscribeAction(Self->Outgoing.Context, AC::Free);

   while (Self->Clients) free_client(Self, Self->Clients);

   free_socket(Self);

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
         pf::Log().msg("NetSocket in use, cannot free yet (request delayed).");
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
   result = getsockname(Self->Handle, (struct sockaddr *)&addr_storage, &addr_length);
#elif _WIN32
   int addr_length = sizeof(addr_storage);
   result = win_getsockname(Self->Handle, (struct sockaddr *)&addr_storage, &addr_length);
#endif

   if (!result) {
      if (addr_storage.ss_family IS AF_INET6) {
         auto addr6 = (struct sockaddr_in6 *)&addr_storage;
         pf::copymem(Args->Address->Data, &addr6->sin6_addr.s6_addr, 16);
         Args->Address->Type = IPADDR::V6;
      }
      else if (addr_storage.ss_family IS AF_INET) {
         auto addr4 = (struct sockaddr_in *)&addr_storage;
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
   else return log.warning(ERR::SystemCall);
}

//********************************************************************************************************************

static ERR NETSOCKET_Init(extNetSocket *Self)
{
   pf::Log log;
   ERR error;

   if (Self->Handle != NOHANDLE) return ERR::Okay; // The socket has been pre-configured by the developer

#ifndef DISABLE_SSL
   // Initialise SSL ahead of any connections being made.

   if ((Self->Flags & NSF::SSL) != NSF::NIL) {
      if ((error = sslSetup(Self)) != ERR::Okay) return error;
   }
#endif

#ifdef __linux__

   // Create socket - IPv6 dual-stack if available, otherwise IPv4
   if ((Self->Handle = socket(PF_INET6, SOCK_STREAM, 0)) != NOHANDLE) {
      Self->IPV6 = true;

      // Enable dual-stack mode (accept both IPv4 and IPv6)
      int v6only = 0;
      if (setsockopt(Self->Handle, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) != 0) {
         log.warning("Failed to set dual-stack mode: %s", strerror(errno));
      }

      int nodelay = 1;
      setsockopt(Self->Handle, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
   }
   else if ((Self->Handle = socket(PF_INET, SOCK_STREAM, 0)) != NOHANDLE) {
      Self->IPV6 = false;

      int nodelay = 1;
      setsockopt(Self->Handle, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
   }
   else {
      log.warning("Failed to create socket: %s", strerror(errno));
      return ERR::SystemCall;
   }

   // Put the socket into non-blocking mode, this is required when registering it as an FD and also prevents connect()
   // calls from going to sleep.

   if (fcntl(Self->Handle, F_SETFL, fcntl(Self->Handle, F_GETFL) | O_NONBLOCK)) return log.warning(ERR::SystemCall);

   // Set the send timeout so that connect() will timeout after a reasonable time

   //int timeout = 30000;
   //result = setsockopt(Self->Handle, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
   //assert(result IS 0);

#elif _WIN32

   // Try IPv6 dual-stack socket first, then fall back to IPv4
   bool is_ipv6;
   Self->Handle = win_socket_ipv6(Self, true, false, is_ipv6);
   if (Self->Handle IS NOHANDLE) return ERR::SystemCall;
   Self->IPV6 = is_ipv6;
   log.msg("Created socket on Windows (handle: %d) IPV6: %d", Self->Handle, int(is_ipv6));

#endif

   if ((Self->Flags & NSF::SERVER) != NSF::NIL) {
      if (!Self->Port) return log.warning(ERR::FieldNotSet);
      Self->State = NTC::MULTISTATE; // Permanent value to indicate that the socket serves multiple clients.

      if (Self->IPV6) {
         #ifdef __linux__
            struct sockaddr_in6 addr;

            pf::clearmem(&addr, sizeof(addr));
            addr.sin6_family = AF_INET6;
            addr.sin6_port   = net::HostToShort(Self->Port); // Must be passed in in network byte order
            addr.sin6_addr   = in6addr_any;   // Must be passed in in network byte order

            int result;
            int value = 1;
            setsockopt(Self->Handle, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

            if ((result = bind(Self->Handle, (struct sockaddr *)&addr, sizeof(addr))) != -1) {
               listen(Self->Handle, Self->Backlog);
               RegisterFD((HOSTHANDLE)Self->Handle, RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&server_accept_client), Self);
               return ERR::Okay;
            }
            else if (result IS EADDRINUSE) return log.warning(ERR::InUse);
            else return log.warning(ERR::SystemCall);
         #elif _WIN32
            // Windows IPv6 dual-stack server binding
            struct sockaddr_in6 addr;
            pf::clearmem(&addr, sizeof(addr));
            addr.sin6_family = AF_INET6;
            addr.sin6_port   = net::HostToShort(Self->Port);
            addr.sin6_addr   = in6addr_any;

            if ((error = win_bind(Self->Handle, (struct sockaddr *)&addr, sizeof(addr))) IS ERR::Okay) {
               if ((error = win_listen(Self->Handle, Self->Backlog)) IS ERR::Okay) {
                  return ERR::Okay;
               }
               else {
                  log.warning("Listen failed on port %d, error: %s", Self->Port, GetErrorMsg(error));
                  return error;
               }
            }
            else {
               log.warning("Bind failed on port %d, error: %s", Self->Port, GetErrorMsg(error));
               return error;
            }
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
            setsockopt(Self->Handle, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

            if ((result = bind(Self->Handle, (struct sockaddr *)&addr, sizeof(addr))) != -1) {
               listen(Self->Handle, Self->Backlog);
               RegisterFD((HOSTHANDLE)Self->Handle, RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&server_accept_client), Self);
               return ERR::Okay;
            }
            else if (result IS EADDRINUSE) return log.warning(ERR::InUse);
            else {
               log.warning("bind() failed with error: %s", strerror(errno));
               return ERR::SystemCall;
            }
         #elif _WIN32
            if ((error = win_bind(Self->Handle, (struct sockaddr *)&addr, sizeof(addr))) IS ERR::Okay) {
               if ((error = win_listen(Self->Handle, Self->Backlog)) IS ERR::Okay) {
                  return ERR::Okay;
               }
               else {
                  log.warning("Listen failed on port %d, error: %s", Self->Port, GetErrorMsg(error));
                  return error;
               }
            }
            else {
               log.warning("Bind failed on port %d, error: %s", Self->Port, GetErrorMsg(error));
               return error;
            }
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

Because NetSocket objects are non-blocking, reading from the socket is normally performed in the #Incoming
callback.  Reading from the socket when no data is available will result in an immediate return with no output.

-ERRORS-
Okay: Read successful (if no data was on the socket, success is still indicated).
NullArgs
Disconnected: The socket connection is closed.
InvalidState: The socket is not in a state that allows reading (e.g. during SSL handshake).
Failed: A permanent failure has occurred and socket has been closed.

*********************************************************************************************************************/

static ERR NETSOCKET_Read(extNetSocket *Self, struct acRead *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Buffer)) return log.warning(ERR::NullArgs);

   if ((Self->Flags & NSF::SERVER) != NSF::NIL) { // Not allowed - client must read from the ClientSocket.
      return ERR::NoSupport;
   }

   if (Self->Handle IS NOHANDLE) return log.warning(ERR::Disconnected);

   Self->ReadCalled = true;
   Args->Result = 0;

   if (!Args->Length) return ERR::Okay;

#ifndef DISABLE_SSL
   if (Self->SSLHandle) {
      #ifdef _WIN32
         // If we're in the middle of SSL handshake, return nothing.  The automated incoming data handler is managing the object state.
         if (Self->State IS NTC::HANDSHAKING) return log.traceWarning(ERR::InvalidState);
         else if (Self->State != NTC::CONNECTED) return log.warning(ERR::Disconnected);

         int bytes_read = 0;
         if (auto error = ssl_read(Self->SSLHandle, Args->Buffer, Args->Length, &bytes_read); error IS SSL_OK) {
            Args->Result = bytes_read;
            return ERR::Okay;
         }
         else if (error IS SSL_ERROR_DISCONNECTED) return log.traceWarning(ERR::Disconnected);
         else if (error IS SSL_ERROR_WOULD_BLOCK) return ERR::Okay; // Not considered an error.
         else {
            log.warning("Windows SSL read error (code %d)", error);
            return ERR::Failed;
         }
      #else // OpenSSL
         bool read_blocked;
         int pending;

         if (Self->HandshakeStatus IS SHS::WRITE) ssl_handshake_write(Self->Handle, Self);
         else if (Self->HandshakeStatus IS SHS::READ) ssl_handshake_read(Self->Handle, Self);

         if (Self->HandshakeStatus != SHS::NIL) { // Still handshaking
            log.trace("SSL handshake still in progress.");
            return ERR::Okay;
         }

         auto Buffer = Args->Buffer;
         auto BufferSize = Args->Length;
         do {
            read_blocked = false;
            if (auto result = SSL_read(Self->SSLHandle, Buffer, BufferSize); result <= 0) {
               auto ssl_error = SSL_get_error(Self->SSLHandle, result);
               switch (ssl_error) {
                  case SSL_ERROR_ZERO_RETURN: return log.traceWarning(ERR::Disconnected);

                  case SSL_ERROR_WANT_READ: read_blocked = true; break;

                   case SSL_ERROR_WANT_WRITE:
                     // WANT_WRITE is returned if we're trying to rehandshake and the write operation would block.  We
                     // need to wait on the socket to be writeable, then restart the read when it is.

                      log.msg("SSL socket handshake requested by server.");
                      Self->HandshakeStatus = SHS::WRITE;
                      RegisterFD((HOSTHANDLE)Self->Handle, RFD::WRITE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(ssl_handshake_write<extNetSocket>), Self);
                      return ERR::Okay;

                   case SSL_ERROR_SYSCALL:
                   default:
                      log.warning("SSL read failed with error %d: %s", ssl_error, ERR_error_string(ssl_error, nullptr));
                      return ERR::Read;
               }
            }
            else {
               Args->Result += result;
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

            RegisterFD((HOSTHANDLE)Self->Handle, RFD::RECALL|RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&netsocket_incoming), Self);
         }

         return ERR::Okay;
      #endif
   }
#endif

#ifdef __linux__
   auto bytes_received = recv(Self->Handle, Args->Buffer, Args->Length, 0);

   if (bytes_received > 0) {
      Args->Result = bytes_received;
      return ERR::Okay;
   }

   if (bytes_received IS 0) { // man recv() says: The return value is 0 when the peer has performed an orderly shutdown.
      return ERR::Disconnected;
   }
   else if ((errno IS EAGAIN) or (errno IS EINTR)) return ERR::Okay;
   else {
      log.warning("recv() failed: %s", strerror(errno));
      return ERR::SystemCall;
   }
#elif _WIN32
   size_t result;
   auto error = WIN_RECEIVE(Self->Handle, (char *)Args->Buffer, Args->Length, &result);
   Args->Result = result;
   return error;
#else
   #error No support for RECEIVE()
#endif
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
      log.warning("Write to the ClientSocket objects of this server.");
      return ERR::NoSupport;
   }

   if ((Self->Handle IS NOHANDLE) or (Self->State != NTC::CONNECTED)) { // Queue the write prior to server connection
      log.trace("Saving %d bytes to queue.", Args->Length);
      Self->WriteQueue.write(Args->Buffer, std::min<size_t>(Args->Length, Self->MsgLimit));
      return ERR::Okay;
   }

   // Note that if a write queue has been setup, there is no way that we can write to the server until the queue has
   // been exhausted.  Thus we have add more data to the queue if it already exists.

   size_t len;
   ERR error;
   if (Self->WriteQueue.Buffer.empty()) { // No prior buffer to send
      len = Args->Length;
      error = send_data(Self, Args->Buffer, &len);
      // len now reflects the total bytes that were sent to the server.
   }
   else { // Content remains in the write queue
      len = 0;
      error = ERR::BufferOverflow;
   }

   if ((error != ERR::Okay) or (len < size_t(Args->Length))) {
      if ((error IS ERR::DataSize) or (error IS ERR::BufferOverflow) or (len > 0))  {
         // Put data into the write queue and register the socket for write events
         log.trace("Error: '%s', queuing %d/%d bytes for transfer...", GetErrorMsg(error), Args->Length - len, Args->Length);
         Self->WriteQueue.write((BYTE *)Args->Buffer + len, std::min<size_t>(Args->Length - len, Self->MsgLimit));
         #ifdef __linux__
            RegisterFD((HOSTHANDLE)Self->Handle, RFD::WRITE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&netsocket_outgoing), Self);
         #elif _WIN32
            win_socketstate(Self->Handle, std::nullopt, true);
         #endif
      }
      else {
         Self->ErrorCountdown--;
         if (!Self->ErrorCountdown) Self->setState(NTC::DISCONNECTED);
         return error;
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

Incoming connections to NetSocket objects are queued until they are answered by the object.  Setting the Backlog
adjusts the maximum number of connections on the queue, which otherwise defaults to 10.

If the backlog is exceeded, subsequent connections to the socket should expect a connection refused error.

-FIELD-
ClientLimit: The maximum number of clients (unique IP addresses) that can be connected to a server socket.

The ClientLimit value limits the maximum number of IP addresses that can be connected to the socket at any one time.
For socket limits per client, see the #SocketLimit field.

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
typically connection messages.

In server mode, the function must follow the prototype `Function(*NetSocket, *ClientSocket, NTC State)`.  Otherwise
`Function(*NetSocket, NTC State)`.

The `NetSocket` parameter refers to the NetSocket object to which the function is subscribed.  In server mode,
`ClientSocket` refers to the @ClientSocket on which the state has changed.

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
function prototype for C++ is `ERR Incoming(*NetSocket, APTR Meta)`.  For Fluid use `function Incoming(NetSocket)`.

The `NetSocket` parameter refers to the NetSocket object.  `Meta` is optional userdata from the `FUNCTION`.

Retrieve data from the socket with the #Read() action. Reading at least some of the data from the socket is
compulsory - if the function does not do this then the data will be cleared from the socket when the function returns.
If the callback function returns/raises `ERR::Terminate` then the Incoming field will be cleared and the function
will no longer be called.  All other error codes are ignored.

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
In client mode the function must be in the format `ERR Outgoing(*NetSocket, APTR Meta)`.  In server mode the function
format is `ERR Outgoing(*NetSocket, *ClientSocket, APTR Meta)`.

To send data to the NetSocket object, call the #Write() action.  If the callback function returns an error other
than `ERR::Okay` then the Outgoing field will be cleared and the function will no longer be called.

*********************************************************************************************************************/

static ERR GET_Outgoing(extNetSocket *Self, FUNCTION **Value)
{
   if (Self->Outgoing.defined()) {
      *Value = &Self->Outgoing;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_Outgoing(extNetSocket *Self, FUNCTION *Value)
{
   pf::Log log;

   if (Self->Outgoing.isScript()) UnsubscribeAction(Self->Outgoing.Context, AC::Free);
   Self->Outgoing = *Value;
   if (Self->Outgoing.isScript()) SubscribeAction(Self->Outgoing.Context, AC::Free, C_FUNCTION(notify_free_outgoing));

   if (Self->initialised()) {
      if ((Self->Handle != NOHANDLE) and (Self->State IS NTC::CONNECTED)) {
         // Setting the Outgoing field after connectivity is established will put the socket into streamed write mode.

         #ifdef __linux__
            RegisterFD((HOSTHANDLE)Self->Handle, RFD::WRITE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&netsocket_outgoing), Self);
         #elif _WIN32
            win_socketstate(Self->Handle, std::nullopt, true);
         #endif
      }
      else log.trace("Will not listen for socket-writes (no socket handle, or state %d != NTC::CONNECTED).", Self->State);
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
Handle: Platform specific reference to the network socket handle.

*********************************************************************************************************************/

static ERR GET_Handle(extNetSocket *Self, APTR *Value)
{
   *Value = (APTR)(MAXINT)Self->Handle;
   return ERR::Okay;
}

static ERR SET_Handle(extNetSocket *Self, APTR Value)
{
   // The user can set Handle prior to initialisation in order to create a NetSocket object that is linked to a
   // socket created from outside the core platform code base.

   Self->Handle = (SOCKET_HANDLE)(MAXINT)Value;
   Self->ExternalSocket = true;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
SSLCertificate: SSL certificate file to use if in server mode.

Set SSLCertificate to the path of an SSL certificate file to use when the NetSocket is in server mode.  The
certificate file must be in a supported format such as PEM, CRT, or P12.  If no certificate is defined, the
NetSocket will either self-sign or use a localhost certificate, if available.

*********************************************************************************************************************/

static ERR SET_SSLCertificate(extNetSocket *Self, CSTRING Value)
{
   if (Self->SSLCertificate) { FreeResource(Self->SSLCertificate); Self->SSLCertificate = nullptr; }

   if ((Value) and (*Value)) {
      pf::Log log(__FUNCTION__);

      LOC type;
      if ((AnalysePath(Value, &type) IS ERR::Okay) and (type IS LOC::FILE)) {
         // Check file extension for supported formats
         std::string path(Value);
         std::string ext = path.substr(path.find_last_of(".") + 1);
         std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

         if ((ext IS "pem") or (ext IS "crt") or (ext IS "cert") or (ext IS "p12") or (ext IS "pfx")) {
            Self->SSLCertificate = pf::strclone(Value);
         }
         else return log.warning(ERR::InvalidData);
      }
      else return log.warning(ERR::FileNotFound);
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
SSLPrivateKey: Private key file to use if in server mode.

Set SSLPrivateKey to the path of an SSL private key file to use when the NetSocket is in server mode.  The
private key file must be in a supported format such as PEM or KEY.  If no private key is defined, the NetSocket
will either self-sign or use a localhost private key, if available.

*********************************************************************************************************************/

static ERR SET_SSLPrivateKey(extNetSocket *Self, CSTRING Value)
{
   if (Self->SSLPrivateKey) { FreeResource(Self->SSLPrivateKey); Self->SSLPrivateKey = nullptr; }

   if ((Value) and (*Value)) {
      pf::Log log(__FUNCTION__);

      LOC type;
      if ((AnalysePath(Value, &type) IS ERR::Okay) and (type IS LOC::FILE)) {
         // Check file extension for supported formats
         std::string path(Value);
         std::string ext = path.substr(path.find_last_of(".") + 1);
         std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
         if ((ext IS "pem") or (ext IS "key")) {
            Self->SSLPrivateKey = pf::strclone(Value);
         }
         else return log.warning(ERR::InvalidData);
      }
      else return log.warning(ERR::FileNotFound);
   }
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
SSLKeyPassword: SSL private key password.

If the SSL private key is encrypted, set this field to the password required to decrypt it.  If the private key is
not encrypted, this field can be left empty.

*********************************************************************************************************************/

static ERR SET_SSLKeyPassword(extNetSocket *Self, CSTRING Value)
{
   if (Self->SSLKeyPassword) { FreeResource(Self->SSLKeyPassword); Self->SSLKeyPassword = nullptr; }
   if ((Value) and (*Value)) Self->SSLKeyPassword = pf::strclone(Value);
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
State: The current connection state of the NetSocket object.

The State reflects the connection state of the NetSocket.  If the #Feedback field is defined with a function, it will
be called automatically whenever the state is changed.  Note that the ClientSocket parameter will be NULL when the
Feedback function is called.

Note that in server mode this State value should not be used as it cannot reflect the state of all connected
client sockets.  Each @ClientSocket carries its own independent State value for use instead.

*********************************************************************************************************************/

static ERR GET_State(extNetSocket *Self, NTC &Value)
{
   if ((Self->Flags & NSF::SERVER) != NSF::NIL) {
      pf::Log().warning("Reading the State of a server socket is a probable defect.");
      Value = NTC::MULTISTATE;
   }
   else Value = Self->State;
   return ERR::Okay;
}

static ERR SET_State(extNetSocket *Self, NTC Value)
{
   pf::Log log;

   if ((Self->Flags & NSF::SERVER) != NSF::NIL) {
      return log.warning(ERR::Immutable);
   }

   if (Value != Self->State) {
      log.branch("State changed from %s to %s", netsocket_state(Self->State), netsocket_state(Value));

      #ifndef DISABLE_SSL
      if ((Self->State IS NTC::HANDSHAKING) and (Value IS NTC::CONNECTED)) {
         // SSL connection has just been established

         bool ssl_valid = true;

         #ifdef _WIN32
         if ((Self->SSLHandle) and ((Self->Flags & NSF::SERVER) IS NSF::NIL)) {
            // Only perform certificate validation if DISABLE_SERVER_VERIFY flag is not set
            if ((Self->Flags & NSF::DISABLE_SERVER_VERIFY) != NSF::NIL) {
               log.trace("SSL certificate validation skipped.");
            }
            else ssl_valid = ssl_get_verify_result(Self->SSLHandle);
         }
         #else
         if (Self->SSLHandle) {
            // Only perform certificate validation if DISABLE_SERVER_VERIFY flag is not set
            if ((Self->Flags & NSF::DISABLE_SERVER_VERIFY) != NSF::NIL) {
               log.trace("SSL certificate validation skipped.");
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
            if (Self->Feedback.defined()) {
               if (Self->Feedback.isC()) {
                  pf::SwitchContext context(Self->Feedback.Context);
                  auto routine = (void (*)(extNetSocket *, NTC, APTR))Self->Feedback.Routine;
                  if (routine) routine(Self, Self->State, Self->Feedback.Meta);
               }
               else if (Self->Feedback.isScript()) {
                  sc::Call(Self->Feedback, std::to_array<ScriptArg>({
                     { "NetSocket", Self, FD_OBJECTPTR },
                     { "State", int(Self->State) }
                  }));
               }
            }
            return ERR::Security;
         }
      }
      #endif

      Self->State = Value;

      if (Self->Feedback.defined()) {
         log.traceBranch("Reporting state change to subscriber, operation %d, context %p.", int(Self->State), Self->Feedback.Context);

         if (Self->Feedback.isC()) {
            pf::SwitchContext context(Self->Feedback.Context);
            auto routine = (void (*)(extNetSocket *, NTC, APTR))Self->Feedback.Routine;
            if (routine) routine(Self, Self->State, Self->Feedback.Meta);
         }
         else if (Self->Feedback.isScript()) {
            sc::Call(Self->Feedback, std::to_array<ScriptArg>({
               { "NetSocket", Self, FD_OBJECTPTR },
               { "State",     int(Self->State) }
            }));
         }
      }

      if ((Self->State IS NTC::CONNECTED) and ((!Self->WriteQueue.Buffer.empty()) or (Self->Outgoing.defined()))) {
         log.msg("Sending queued data to server on connection.");
         #ifdef __linux__
            RegisterFD((HOSTHANDLE)Self->Handle, RFD::WRITE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&netsocket_outgoing), Self);
         #elif _WIN32
            win_socketstate(Self->Handle, std::nullopt, true);
         #endif
      }
   }

   SetResourcePtr(RES::EXCEPTION_HANDLER, nullptr); // Stop winsock from fooling with the Core exception handler

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

*********************************************************************************************************************/

static void free_socket(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   log.branch("Handle: %d", Self->Handle);

   if (Self->Handle != NOHANDLE) {
      log.trace("Deregistering socket.");
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
      DeregisterFD((HOSTHANDLE)Self->Handle);
#pragma GCC diagnostic warning "-Wint-to-pointer-cast"

      if (!Self->ExternalSocket) { CLOSESOCKET_THREADED(Self->Handle); Self->Handle = NOHANDLE; }
   }

   Self->WriteQueue.Buffer.clear();
   Self->WriteQueue.Index = 0;

   if (!Self->terminating()) {
      if (Self->State != NTC::DISCONNECTED) Self->setState(NTC::DISCONNECTED);
   }

   log.trace("Resetting exception handler.");

   SetResourcePtr(RES::EXCEPTION_HANDLER, nullptr); // Stop winsock from fooling with our exception handler
}

//********************************************************************************************************************
// Store data in the queue

ERR NetQueue::write(CPTR Message, size_t Length)
{
   pf::Log log(__FUNCTION__);

   if (!Message) return log.warning(ERR::NullArgs);
   if (Length <= 0) return ERR::Okay;

   // Security: Check for maximum buffer size to prevent memory exhaustion
   constexpr size_t MAX_QUEUE_SIZE = 16 * 1024 * 1024; // 16MB limit
   if (Length > MAX_QUEUE_SIZE) return log.warning(ERR::DataSize);

   if (!Buffer.empty()) { // Add data to existing queue
      size_t remaining_data = Buffer.size() - Index;

      if (Index > 8192) { // Compact the queue
         if (remaining_data > 0) Buffer.erase(Buffer.begin(), Buffer.begin() + Index);
         else Buffer.clear();
         Index = 0;
      }

      // Security: Check for integer overflow and buffer size limits
      if (Buffer.size() > MAX_QUEUE_SIZE - Length) return log.warning(ERR::BufferOverflow);

      size_t old_size = Buffer.size();
      Buffer.resize(old_size + Length);
      pf::copymem(Message, Buffer.data() + old_size, Length);
   }
   else {
      Buffer.resize(Length);
      Index = 0;
      pf::copymem(Message, Buffer.data(), Length);
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
void win32_netresponse(OBJECTPTR SocketObject, SOCKET_HANDLE Handle, int Message, ERR Error)
{
   pf::Log log(__FUNCTION__);

   extNetSocket *Socket;
   extClientSocket *ClientSocket;

   if (SocketObject->terminating()) { // Sanity check
      log.warning(ERR::MarkedForDeletion);
      return;
   }

   if (SocketObject->classID() IS CLASSID::CLIENTSOCKET) {
      ClientSocket = (extClientSocket *)SocketObject;
      Socket = (extNetSocket *)ClientSocket->Client->Owner;
      if (ClientSocket->Handle != Handle) {
         log.warning(ERR::SanityCheckFailed);
         return;
      }
   }
   else {
      Socket = (extNetSocket *)SocketObject;
      ClientSocket = nullptr;
      if (Socket->Handle != Handle) {
         log.warning(ERR::SanityCheckFailed);
         return;
      }
   }

   #if defined(_DEBUG)
   static constexpr const char* const msg[] = { "None", "Write", "Read", "Accept", "Connect", "Close" };
   log.traceBranch("[%d:%d:%p], %s, Error %d, InUse: %d, WinRecursion: %d", Socket->UID, Handle, ClientSocket, msg[Message], int(Error), Socket->InUse, Socket->WinRecursion);
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

      if (Socket->WinRecursion) log.traceWarning(ERR::Recursion);
      else {
         Socket->WinRecursion++;
         #pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
         if (ClientSocket) server_incoming_from_client((HOSTHANDLE)Handle, ClientSocket);
         else netsocket_incoming(0, Socket);
         #pragma GCC diagnostic warning "-Wint-to-pointer-cast"
         Socket->WinRecursion--;
      }
   }
   else if (Message IS NTE_WRITE) {
      if (Error != ERR::Okay) log.warning("Socket failed on outgoing data, error %d.", int(Error));

      if (Socket->WinRecursion) log.traceWarning(ERR::Recursion);
      else {
         Socket->WinRecursion++;
         #pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
         if (ClientSocket) clientsocket_outgoing((HOSTHANDLE)Handle, ClientSocket);
         else netsocket_outgoing(0, Socket);
         #pragma GCC diagnostic warning "-Wint-to-pointer-cast"
         Socket->WinRecursion--;
      }
   }
   else if (Message IS NTE_CLOSE) {
      if (ClientSocket) {
         log.branch("Client socket closed.");
         FreeResource(ClientSocket);
         // Note: Disconnection feedback is sent to the NetSocket by the ClientSocket destructor.
      }
      else {
         log.branch("Connection closed by host, error %d.", int(Error));

         // Prevent multiple close messages from the same socket
         if (Socket->State IS NTC::DISCONNECTED) {
            log.trace("Ignoring duplicate close message for socket %d", Handle);
            Socket->InUse--;
            return;
         }

         Socket->setState(NTC::DISCONNECTED);
         free_socket(Socket);
      }
   }
   else if (Message IS NTE_ACCEPT) {
      log.traceBranch("Accept message received for new client %d.", Handle);
      server_accept_client(Socket->Handle, Socket);
   }
   else if (Message IS NTE_CONNECT) {
      if (Error IS ERR::Okay) {
         if (ClientSocket) { // Server mode - connect message shouldn't be received for ClientSocket
            log.warning("Unexpected connect message for ClientSocket, ignoring.");
            Socket->InUse--;
            return;
         }
         else {
            log.traceBranch("Connection to host granted.");
            #ifndef DISABLE_SSL
               if (Socket->SSLHandle) sslConnect(Socket);
               else Socket->setState(NTC::CONNECTED);
            #else
               Socket->setState(NTC::CONNECTED);
            #endif
         }
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
// Called when a server socket handle detects a new client wanting to connect to it.
// Used by Win32 (Windows message loop) & Linux (FD hook)

static void server_accept_client(SOCKET_HANDLE FD, extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);
   uint8_t ip[8];
   SOCKET_HANDLE clientfd;

   log.traceBranch("FD: %d", FD);

   pf::SwitchContext context(Self);

   // Check client limit before accepting to prevent resource exhaustion
   if ((Self->TotalClients >= Self->ClientLimit) or (Self->TotalClients >= glSocketLimit)) {
      log.error(ERR::ArrayFull);
      return;
   }

   // Basic rate limiting - prevent connection floods

   time_t current_time = time(nullptr);
   static time_t last_accept = 0;
   static int accept_count = 0;

   if (current_time != last_accept) {
      accept_count = 1;
      last_accept = current_time;
   }
   else {
      accept_count++;
      if (accept_count > 100) { // Maximum 100 accepts per second
         log.warning("Connection rate limit exceeded, rejecting connection");
         return;
      }
   }

   if (Self->IPV6) {
      #ifdef __linux__
         // For dual-stack sockets, use sockaddr_storage to handle both IPv4 and IPv6
         struct sockaddr_storage addr_storage;
         socklen_t len = sizeof(addr_storage);
         clientfd = accept(FD, (struct sockaddr *)&addr_storage, &len);
         if (clientfd IS NOHANDLE) return;

         int nodelay = 1;
         setsockopt(clientfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

         if (addr_storage.ss_family IS AF_INET6) { // IPv6 connection
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr_storage;
            ip[0] = addr6->sin6_addr.s6_addr[0];
            ip[1] = addr6->sin6_addr.s6_addr[1];
            ip[2] = addr6->sin6_addr.s6_addr[2];
            ip[3] = addr6->sin6_addr.s6_addr[3];
            ip[4] = addr6->sin6_addr.s6_addr[4];
            ip[5] = addr6->sin6_addr.s6_addr[5];
            ip[6] = addr6->sin6_addr.s6_addr[6];
            ip[7] = addr6->sin6_addr.s6_addr[7];
            log.trace("Accepted IPv6 client connection");
         }
         else if (addr_storage.ss_family IS AF_INET) { // IPv4 connection on dual-stack socket
            struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr_storage;
            uint32_t ipv4_addr = net::LongToHost(addr4->sin_addr.s_addr);
            ip[0] = ipv4_addr & 0xff;
            ip[1] = (ipv4_addr >> 8) & 0xff;
            ip[2] = (ipv4_addr >> 16) & 0xff;
            ip[3] = (ipv4_addr >> 24) & 0xff;
            ip[4] = ip[5] = ip[6] = ip[7] = 0;
            log.trace("Accepted IPv4 client connection on dual-stack socket");
         }
         else {
            log.warning("Unsupported address family: %d", addr_storage.ss_family);
            close(clientfd);
            return;
         }
      #elif _WIN32
         // Windows IPv6 dual-stack accept using wrapper function
         int family;
         struct sockaddr_storage addr_storage;
         int len = sizeof(addr_storage);
         clientfd = win_accept_ipv6(Self, FD, (struct sockaddr *)&addr_storage, &len, &family);
         if (clientfd IS NOHANDLE) return;

         if (family IS AF_INET6) { // IPv6 connection
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr_storage;
            ip[0] = addr6->sin6_addr.s6_addr[0];
            ip[1] = addr6->sin6_addr.s6_addr[1];
            ip[2] = addr6->sin6_addr.s6_addr[2];
            ip[3] = addr6->sin6_addr.s6_addr[3];
            ip[4] = addr6->sin6_addr.s6_addr[4];
            ip[5] = addr6->sin6_addr.s6_addr[5];
            ip[6] = addr6->sin6_addr.s6_addr[6];
            ip[7] = addr6->sin6_addr.s6_addr[7];
            log.trace("Accepted IPv6 client connection on Windows");
         }
         else if (family IS AF_INET) { // IPv4 connection on dual-stack socket
            struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr_storage;
            uint32_t ipv4_addr = net::LongToHost(addr4->sin_addr.s_addr);
            ip[0] = ipv4_addr & 0xff;
            ip[1] = (ipv4_addr >> 8) & 0xff;
            ip[2] = (ipv4_addr >> 16) & 0xff;
            ip[3] = (ipv4_addr >> 24) & 0xff;
            ip[4] = ip[5] = ip[6] = ip[7] = 0;
            log.trace("Accepted IPv4 client connection on dual-stack socket (Windows)");
         }
         else {
            log.warning("Unsupported address family on Windows: %d", family);
            CLOSESOCKET(clientfd);
            return;
         }
      #else
         #warning Platform requires IPV6 support.
         return;
      #endif
   }
   else {
      struct sockaddr_in addr;

      #ifdef __linux__
         socklen_t len = sizeof(addr);
         clientfd = accept(FD, (struct sockaddr *)&addr, &len);

         if (clientfd != NOHANDLE) {
            int nodelay = 1;
            setsockopt(clientfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
         }
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

   objNetClient *client_ip;
   for (client_ip=Self->Clients; client_ip; client_ip=client_ip->Next) {
      if (((LARGE *)&ip)[0] IS ((LARGE *)&client_ip->IP)[0]) break;
   }

   if (!client_ip) {
      if (NewObject(CLASSID::NETCLIENT, &client_ip) IS ERR::Okay) {
         if (InitObject(client_ip) != ERR::Okay) {
            FreeResource(client_ip);
            CLOSESOCKET(clientfd);
            return;
         }
      }
      else {
         CLOSESOCKET(clientfd);
         return;
      }

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
   else if (client_ip->TotalConnections >= Self->SocketLimit) {
      log.warning("Socket limit of %d reached for IP %d.%d.%d.%d", Self->SocketLimit, client_ip->IP[0], client_ip->IP[1], client_ip->IP[2], client_ip->IP[3]);
      CLOSESOCKET(clientfd);
      return;
   }

   if ((Self->Flags & NSF::MULTI_CONNECT) IS NSF::NIL) { // Check if the IP is already registered and alive
      if (client_ip->Connections) {
         log.msg("Preventing second connection attempt from IP %d.%d.%d.%d", client_ip->IP[0], client_ip->IP[1], client_ip->IP[2], client_ip->IP[3]);
         CLOSESOCKET(clientfd);
         return;
      }
   }

   // Socket Management

   extClientSocket *client_socket;
   if (NewObject(CLASSID::CLIENTSOCKET, &client_socket) IS ERR::Okay) {
      client_socket->Handle = clientfd;
      client_socket->Client = client_ip;
      if (InitObject(client_socket) IS ERR::Okay) {
         // Note that if the connection is over SSL then handshaking won't have
         // completed yet, in which case the connection feedback will be sent in a later state change.

         if (client_socket->State IS NTC::CONNECTED) {
            if (Self->Feedback.isC()) {
               pf::SwitchContext context(Self->Feedback.Context);
               auto routine = (void (*)(extNetSocket *, objClientSocket *, NTC, APTR))Self->Feedback.Routine;
               if (routine) routine(Self, client_socket, client_socket->State, Self->Feedback.Meta);
            }
            else if (Self->Feedback.isScript()) {
               sc::Call(Self->Feedback, std::to_array<ScriptArg>({
                  { "NetSocket",    Self, FD_OBJECTPTR },
                  { "ClientSocket", client_socket, FD_OBJECTPTR },
                  { "State",        int(client_socket->State) }
               }));
            }
         }
      }
      else log.warning(ERR::Init);
   }
   else {
      CLOSESOCKET(clientfd);
      if (!client_ip->Connections) free_client(Self, client_ip);
      return;
   }

   log.trace("Total clients: %d", Self->TotalClients);
}

//********************************************************************************************************************
// Terminates all connections for a client IP address and removes associated resources.

static void free_client(extNetSocket *Socket, objNetClient *Client)
{
   pf::Log log(__FUNCTION__);
   static THREADVAR int8_t recursive = 0;

   if (!Client) return;
   if ((Socket->Flags & NSF::SERVER) IS NSF::NIL) return; // Must be a server

   if (recursive) return;
   recursive++;

   log.branch("%d:%d:%d:%d, Connections: %d", Client->IP[0], Client->IP[1], Client->IP[2], Client->IP[3], Client->TotalConnections);

   // Free all sockets (connections) related to this client IP

   while (Client->Connections) {
      objClientSocket *current_socket = Client->Connections;
      FreeResource(current_socket); // Disconnects & sends a Feedback message
      if (Client->Connections IS current_socket) { // Sanity check
         log.warning("Resource management error detected in Client->Sockets");
         break;
      }
   }

   if (Client->Prev) {
      Client->Prev->Next = Client->Next;
      if (Client->Next) Client->Next->Prev = Client->Prev;
   }
   else {
      Socket->Clients = Client->Next;
      if ((Socket->Clients) and (Socket->Clients->Next)) Socket->Clients->Next->Prev = NULL;
   }

   FreeResource(Client);

   Socket->TotalClients--;

   recursive--;
}

//********************************************************************************************************************
// See win32_netresponse() for the Windows version.

#ifdef __linux__
static void client_connect(SOCKET_HANDLE Void, APTR Data)
{
   pf::Log log(__FUNCTION__);
   auto Self = (extNetSocket *)Data;

   pf::SwitchContext context(Self);

   log.trace("Connection from server received.");

   int result = EHOSTUNREACH; // Default error in case getsockopt() fails
   socklen_t optlen = sizeof(result);
   getsockopt(Self->Handle, SOL_SOCKET, SO_ERROR, &result, &optlen);

   // Remove the write callback

   RegisterFD((HOSTHANDLE)Self->Handle, RFD::WRITE|RFD::REMOVE, &client_connect, nullptr);

   #ifndef DISABLE_SSL
   if ((Self->SSLHandle) and (!result)) {
      // Perform the SSL handshake

      log.traceBranch("Attempting SSL handshake.");

      sslConnect(Self);
      if (Self->Error != ERR::Okay) return;

      if (Self->State IS NTC::HANDSHAKING) {
         RegisterFD((HOSTHANDLE)Self->Handle, RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&netsocket_incoming), Self);
      }
      return;
   }
   #endif

   if (!result) {
      log.traceBranch("Connection succesful.");
      Self->setState(NTC::CONNECTED);
      RegisterFD((HOSTHANDLE)Self->Handle, RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(&netsocket_incoming), Self);
      return;
   }
   else {
      log.trace("getsockopt() result %d", result);

      if (result IS ECONNREFUSED)      Self->Error = ERR::ConnectionRefused;
      else if (result IS ENETUNREACH)  Self->Error = ERR::NetworkUnreachable;
      else if (result IS EHOSTUNREACH) Self->Error = ERR::HostUnreachable;
      else if (result IS ETIMEDOUT)    Self->Error = ERR::TimeOut;
      else Self->Error = ERR::SystemCall;

      log.error(Self->Error);

      Self->setState(NTC::DISCONNECTED);
   }
}
#endif

//********************************************************************************************************************
// If the socket is the client of a server, messages from the server will come in through here.
//
// Incoming information from the server can be read with either the Incoming callback routine (the developer is
// expected to call the Read action from this).
//
// This function is called from win32_netresponse() and is managed outside of the normal message queue.

static void netsocket_incoming(SOCKET_HANDLE FD, extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   pf::SwitchContext context(Self); // Set context & lock

   if ((Self->Flags & NSF::SERVER) != NSF::NIL) { // Sanity check
      log.warning("Invalid call from server socket.");
      return;
   }

   if (Self->Terminating) { // Set by FreeWarning()
      log.trace("Socket terminating...", Self->UID);
      if (Self->Handle != NOHANDLE) free_socket(Self);
      return;
   }

#ifndef DISABLE_SSL
  #ifdef _WIN32
   if ((Self->SSLHandle) and (Self->State IS NTC::HANDSHAKING)) {
      pf::Log log(__FUNCTION__);
      log.traceBranch("Windows SSL handshake in progress, reading raw data.");
      size_t result;
      std::vector<uint8_t> buffer;
      if (ERR error = WIN_APPEND(Self->Handle, buffer, 4096, result); error IS ERR::Okay) {
         sslHandshakeReceived(Self, buffer.data(), int(buffer.size()));

         if ((Self->State != NTC::CONNECTED) or (!ssl_has_decrypted_data(Self->SSLHandle) and !ssl_has_encrypted_data(Self->SSLHandle))) {
            // In most cases we return without further processing unless we're definitely connected and
            // there is data sitting in the queue or SSL has data available (decrypted or encrypted).
            return;
         }
      }
      else {
         log.warning(error);
         return;
      }
   }

  #else
    if ((Self->SSLHandle) and (Self->State IS NTC::HANDSHAKING)) {
      log.traceBranch("Continuing SSL handshake...");
      sslConnect(Self);
      return;
    }

    if (Self->HandshakeStatus != SHS::NIL) { // TODO: Check State is not HANDSHAKING instead
      log.trace("SSL is handshaking.");
      return;
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

   // The Incoming callback will normally be defined by the user and is expected to call the Read() action.
   // Otherwise we clear the unprocessed content.

   Self->ReadCalled = false;
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
      log.trace("Clearing unprocessed data from socket %d", Self->UID);

      std::array<char,1024> buffer;
      int total = 0;
      int result;
      do {
         error = acRead(Self, buffer.data(), buffer.size(), &result);
         total += result;
      } while (result > 0);

      if (error != ERR::Okay) error = ERR::Terminate;
   }

   if (error IS ERR::Terminate) {
      log.traceBranch("Socket %d will be terminated.", FD);
      if (Self->Handle != NOHANDLE) free_socket(Self);
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
//
// Called from either the Windows messaging logic or a Linux FD subscription.

static void netsocket_outgoing(SOCKET_HANDLE Void, extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   pf::SwitchContext context(Self); // Set context & lock

   if (Self->Terminating) return;

   if (Self->State IS NTC::HANDSHAKING) {
      log.trace("Handshaking...");
      return;
   }

   if (Self->OutgoingRecursion) {
      log.traceWarning(ERR::Recursion);
      return;
   }

   log.traceBranch();

   Self->InUse++;
   Self->OutgoingRecursion++;

   auto error = ERR::Okay;

   // Send out remaining queued data before getting new data to send

   while (!Self->WriteQueue.Buffer.empty()) {
      size_t len = Self->WriteQueue.Buffer.size() - Self->WriteQueue.Index;
      #ifndef DISABLE_SSL
         if ((!Self->SSLHandle) and (len > glMaxWriteLen)) len = glMaxWriteLen;
      #else
         if (len > glMaxWriteLen) len = glMaxWriteLen;
      #endif

      if (len > 0) {
         error = send_data(Self, Self->WriteQueue.Buffer.data() + Self->WriteQueue.Index, &len);
         if ((error != ERR::Okay) or (!len)) break;
         log.trace("Sent %d of %d bytes from the queue.", Self->UID, int(len), int(Self->WriteQueue.Buffer.size() - Self->WriteQueue.Index));
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
         log.trace("Write-queue listening on socket %d will now stop.", Self->UID, Self->Handle);
         #ifdef __linux__
            RegisterFD((HOSTHANDLE)Self->Handle, RFD::REMOVE|RFD::WRITE|RFD::SOCKET, nullptr, nullptr);
         #elif _WIN32
            if (auto error = win_socketstate(Self->Handle, std::nullopt, false); error != ERR::Okay) log.warning(error);
         #endif
      }

      if (error != ERR::Okay) {
         Self->ErrorCountdown--;
         if (!Self->ErrorCountdown) Self->setState(NTC::DISCONNECTED);
      }
   }

   Self->InUse--;
   Self->OutgoingRecursion--;
}

//********************************************************************************************************************

#include "netsocket_def.c"

//********************************************************************************************************************

static const FieldArray clSocketFields[] = {
   { "Clients",        FDF_OBJECT|FDF_R, nullptr, nullptr, CLASSID::NETCLIENT },
   { "ClientData",     FDF_POINTER|FDF_RW },
   { "Address",        FDF_STRING|FDF_RI, nullptr, SET_Address },
   { "SSLCertificate", FDF_STRING|FDF_RI, nullptr, SET_SSLCertificate },
   { "SSLPrivateKey",  FDF_STRING|FDF_RI, nullptr, SET_SSLPrivateKey },
   { "SSLKeyPassword", FDF_STRING|FDF_RI, nullptr, SET_SSLKeyPassword },
   { "State",          FDF_INT|FDF_LOOKUP|FDF_RW, GET_State, SET_State, &clNetSocketState },
   { "Error",          FDF_INT|FDF_R },
   { "Port",           FDF_INT|FDF_RI },
   { "Flags",          FDF_INTFLAGS|FDF_RW, nullptr, nullptr, &clNetSocketFlags },
   { "TotalClients",   FDF_INT|FDF_R },
   { "Backlog",        FDF_INT|FDF_RI },
   { "ClientLimit",    FDF_INT|FDF_RW },
   { "SocketLimit",    FDF_INT|FDF_RW },
   { "MsgLimit",       FDF_INT|FDF_RI },
   // Virtual fields
   { "Handle",         FDF_POINTER|FDF_RI,     GET_Handle, SET_Handle },
   { "Feedback",       FDF_FUNCTIONPTR|FDF_RW, GET_Feedback, SET_Feedback },
   { "Incoming",       FDF_FUNCTIONPTR|FDF_RW, GET_Incoming, SET_Incoming },
   { "Outgoing",       FDF_FUNCTIONPTR|FDF_W,  GET_Outgoing, SET_Outgoing },
   { "OutQueueSize",   FDF_INT|FDF_R,          GET_OutQueueSize },
   END_FIELD
};

//********************************************************************************************************************

static CSTRING netsocket_state(NTC Value) {
   return clNetSocketState[int(Value)].Name;
}

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
