
/*****************************************************************************
** Initialise SSL for the first time.  NB: Refer to MODExpunge() for the resource termination code.
*/

static ERROR sslInit(void)
{
   if (ssl_init) return ERR_Okay;

   FMSG("~sslInit()","");

   OBJECTPTR context = SetContext(glModule);

   if (LoadModule("openssl", 1.0, &modOpenSSL, &OpenSSLBase) != ERR_Okay) {
      SetContext(context);
      STEP();
      return ERR_LoadModule;
   }

   SetContext(context);

   ssl_init = TRUE;

   STEP();
   return ERR_Okay;
}

//****************************************************************************

static void sslDisconnect(objNetSocket *Self)
{
   if (Self->SSL) {
      FMSG("~sslDisconnect()","Closing SSL connection.");

         SSL_set_info_callback(Self->SSL, NULL);
         SSL_shutdown(Self->SSL);
         SSL_free(Self->SSL);
         Self->SSL = NULL;

      STEP();
   }

   if (Self->CTX) {
      SSL_CTX_free(Self->CTX);
      Self->CTX = NULL;
   }

   STEP();
}

//****************************************************************************

static void sslMsgCallback(SSL *s, int where, int ret)
{
   const char *str;
   LONG w = where & (~SSL_ST_MASK);

   if (w & SSL_ST_CONNECT) str = "SSL_Connect";
   else if (w & SSL_ST_ACCEPT) str = "SSL_Accept";
   else if (w & SSL_ST_BEFORE) str = "SSL_Before";
   else if (w & SSL_ST_OK) str = "SSL_OK";
   else if (w IS SSL_ST_RENEGOTIATE) str = "SSL_Renegotiate";
   else str = "SSL_Undefined";

   if (where & SSL_CB_LOOP) {
      LogF(str, "Loop: %s", SSL_state_string_long(s));
   }
   else if (where & SSL_CB_ALERT) {
      LogF(str, "%s Alert: %s : %s", (where & SSL_CB_READ) ? "Read" : "Write", SSL_alert_type_string_long(ret), SSL_alert_desc_string_long(ret));
   }
   else if (where & SSL_CB_EXIT) {
      if (ret == 0) LogF(str,"Failed in %s", SSL_state_string_long(s));
      else if (ret < 0) LogF(str, "Error in %s", SSL_state_string_long(s));
   }
   else if (where & SSL_CB_HANDSHAKE_START) {
      LogF(str, "Handshake Start: %s", SSL_state_string_long(s));
   }
   else if (where & SSL_CB_HANDSHAKE_DONE) {
      LogF(str, "Handshake Done: %s", SSL_state_string_long(s));
   }
   else LogF(str, "Unknown: %s", SSL_state_string_long(s));
}

//****************************************************************************

static void sslCtxMsgCallback(SSL *s, int where, int ret) __attribute__ ((unused));
static void sslCtxMsgCallback(SSL *s, int where, int ret)
{
   sslMsgCallback(s, where, ret);
}

/*****************************************************************************
** This only needs to be called once to setup the unique SSL context for the NetSocket object and the locations of the
** certificates.
*/

static ERROR sslSetup(objNetSocket *Self)
{
   STRING path;
   ERROR error;

   if (!ssl_init) {
      error = sslInit();
      if (error) return error;
   }

   if (Self->CTX) return ERR_Okay;

   FMSG("~sslSetup()", "");

   if ((Self->CTX = SSL_CTX_new(SSLv23_client_method()))) {
      //if (GetResource(RES_LOG_LEVEL > 3)) SSL_CTX_set_info_callback(Self->CTX, (void *)&sslCtxMsgCallback);

      if (!ResolvePath("config:ssl/certs", RSF_NO_FILE_CHECK, &path)) {
         if (SSL_CTX_load_verify_locations(Self->CTX, NULL, path)) {
            FreeResource(path);

            if ((Self->SSL = SSL_new(Self->CTX))) {
               LogMsg("SSL connectivity has been configured successfully.");

               if (GetResource(RES_LOG_LEVEL > 3)) SSL_set_info_callback(Self->SSL, (void *)&sslMsgCallback);

               STEP();
               return ERR_Okay;
            }
            else { LogErrorMsg("Failed to initialise new SSL object."); error = ERR_Failed; }
         }
         else {
            FreeResource(path);
            LogErrorMsg("Failed to define certificate folder: %s", path);
            error = ERR_Failed;
         }
      }
      else error = ERR_ResolvePath;

      SSL_CTX_free(Self->CTX);
      Self->CTX = NULL;
   }
   else {
      LogErrorMsg("SSL_CTX_new: %s", ERR_error_string(ERR_get_error(), NULL));
      error = ERR_Failed;
   }

   STEP();
   return error;
}

//****************************************************************************

static ERROR sslLinkSocket(objNetSocket *Self)
{
   FMSG("~sslLinkSocket()","");

   if ((Self->BIO = BIO_new_socket(Self->SocketHandle, BIO_NOCLOSE))) {
      SSL_set_bio(Self->SSL, Self->BIO, Self->BIO);
//      SSL_ctrl(Self->SSL, SSL_CTRL_MODE,(SSL_MODE_AUTO_RETRY), NULL);
      SSL_ctrl(Self->SSL, SSL_CTRL_MODE,(SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER), NULL);
      SSL_ctrl(Self->SSL, SSL_CTRL_MODE,(SSL_MODE_ENABLE_PARTIAL_WRITE), NULL);
   }
   else LogErrorMsg("Failed to create a SSL BIO object.");

   STEP();
   return ERR_Okay;
}

//****************************************************************************
// To establish an SSL connection, this function must be called after the initial connect() has succeeded.  If a
// NetSocket has the NSF_SSL flag set, then the connection is handled automatically.  Otherwise a plain text socket
// connection can be converted to SSL at any time (if the server is ready for it) by calling this function.
//
// The state will be changed to NTC_CONNECTED if the SSL connection is established immediately, otherwise
// NTC_CONNECTING_SSL may be used to indicate that the connection is ongoing.  If a failure occurs, the state is set to
// NTC_DISCONNECTED and the Error field is set appropriately.

static ERROR sslConnect(objNetSocket *Self)
{
   FMSG("~sslConnect()","");

   if (!Self->SSL) { STEP(); return ERR_Failed; }

   LONG result = SSL_connect(Self->SSL);

   if (result <= 0) {
      result = SSL_get_error(Self->SSL, result);

      // The SSL routine may respond with WANT_READ or WANT_WRITE when
      // non-blocking sockets are used.  This is technically not an error.

      switch(result) {
         case SSL_ERROR_NONE:             Self->Error = ERR_Okay;
                                          STEP();
                                          return ERR_Okay;

         case SSL_ERROR_ZERO_RETURN:      Self->Error = ERR_Disconnected; break;

         case SSL_ERROR_WANT_READ:        SetLong(Self, FID_State, NTC_CONNECTING_SSL);
                                          STEP();
                                          return ERR_Okay;

         case SSL_ERROR_WANT_WRITE:       SetLong(Self, FID_State, NTC_CONNECTING_SSL);
                                          STEP();
                                          return ERR_Okay;

         case SSL_ERROR_WANT_CONNECT:     Self->Error = ERR_WouldBlock; break;
         case SSL_ERROR_WANT_ACCEPT:      Self->Error = ERR_WouldBlock; break;
         case SSL_ERROR_WANT_X509_LOOKUP: Self->Error = ERR_Retry; break;
         case SSL_ERROR_SYSCALL:          Self->Error = ERR_InputOutput; break;

         case SSL_ERROR_SSL:              Self->Error = ERR_SystemCall;
                                          ERR_print_errors(Self->BIO);
                                          break;

         default:                         Self->Error = ERR_Failed;
      }

      LogErrorMsg("SSL_connect: %s (%s)", ERR_error_string(result, NULL), GetErrorMsg(Self->Error));

      SetLong(Self, FID_State, NTC_DISCONNECTED);

      STEP();
      return Self->Error;
   }
   else {
      FMSG("sslConnect:","SSL server connection successful.");
      SetLong(Self, FID_State, NTC_CONNECTED);
      STEP();
      return ERR_Okay;
   }
}

/****************************************************************************
** Handshaking may be required during normal read/write operations.  This routine simply tells SSL to continue with its
** handshake and then ceases monitoring of the FD.  If SSL then needs to continue its handshake then it will tell us in
** the RECEIVE() and SEND() functions.
*/

static void ssl_handshake_write(HOSTHANDLE Socket, APTR Data)
{
   objNetSocket *Self = Data;
   LONG result;

   LogF("ssl_handshake_write()","Socket: %d", Socket);

   if ((result = SSL_do_handshake(Self->SSL)) == 1) {
      // Handshake successful, connection established

      #ifdef __linux__
         RegisterFD((HOSTHANDLE)Socket, RFD_WRITE|RFD_REMOVE|RFD_SOCKET, &ssl_handshake_write, Self);
      #elif _WIN32
         if ((Self->WriteSocket) OR (Self->Outgoing.Type != CALL_NONE) OR (Self->WriteQueue.Buffer)) {
            // Do nothing, we are already listening for writes
         }
         else win_socketstate(Socket, -1, FALSE); // Turn off write listening
      #endif

      Self->SSLBusy = SSL_NOT_BUSY;
   }
   else switch (SSL_get_error(Self->SSL, result)) {
      case SSL_ERROR_WANT_READ:
         #ifdef _WIN32
            win_socketstate(Socket, TRUE, -1);
         #else
            #warning Platform support required.
         #endif
         break;

      case SSL_ERROR_WANT_WRITE:
         #ifdef _WIN32
            win_socketstate(Socket, -1, TRUE);
         #else
            #warning Platform support required.
         #endif
         break;

      default:
         Self->SSLBusy = SSL_NOT_BUSY;
   }
}

static void ssl_handshake_read(HOSTHANDLE Socket, APTR Data)
{
   objNetSocket *Self = Data;
   LONG result;

   LogF("ssl_handshake_read()","Socket: %d", Socket);

   if ((result = SSL_do_handshake(Self->SSL)) == 1) {
      // Handshake successful, connection established

      #ifdef __linux__
         RegisterFD((HOSTHANDLE)Socket, RFD_READ|RFD_REMOVE|RFD_SOCKET, &ssl_handshake_read, Self);
      #elif _WIN32
         // No need to remove any handle monitoring, client_server_incoming() will do so automatically if
         // necessary when new data arrives.
      #endif

      Self->SSLBusy = SSL_NOT_BUSY;
   }
   else switch (SSL_get_error(Self->SSL, result)) {
      case SSL_ERROR_WANT_READ:
         #ifdef _WIN32
            win_socketstate(Socket, TRUE, -1);
         #else
            #warning Platform support required.
         #endif
         break;

      case SSL_ERROR_WANT_WRITE:
         #ifdef _WIN32
            win_socketstate(Socket, -1, TRUE);
         #else
            #warning Platform support required.
         #endif
         break;

      default:
         Self->SSLBusy = SSL_NOT_BUSY;
   }
}
