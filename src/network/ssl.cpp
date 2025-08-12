
//********************************************************************************************************************

static void sslDisconnect(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   if (Self->SSL) {
      log.traceBranch("Closing SSL connection.");

      SSL_set_info_callback(Self->SSL, nullptr);
      
      // Perform proper bidirectional SSL shutdown
      auto shutdown_result = SSL_shutdown(Self->SSL);
      if (shutdown_result == 0) {
         // First shutdown call completed, perform second shutdown for bidirectional close
         shutdown_result = SSL_shutdown(Self->SSL);
         if (shutdown_result < 0) {
            int ssl_error = SSL_get_error(Self->SSL, shutdown_result);
            if ((ssl_error != SSL_ERROR_WANT_READ) and (ssl_error != SSL_ERROR_WANT_WRITE)) {
               log.warning("SSL_shutdown failed: %s", ERR_error_string(ssl_error, nullptr));
            }
         }
      }
      
      SSL_free(Self->SSL);
      Self->SSL = nullptr;
   }

   if (Self->CTX) {
      SSL_CTX_free(Self->CTX);
      Self->CTX = nullptr;
   }
}

//********************************************************************************************************************

static void sslMsgCallback(const ssl_st *s, int where, int ret)
{
   const char *state;
   pf::Log log(__FUNCTION__);

   LONG w = where & (~SSL_ST_MASK);

   if (w & SSL_ST_CONNECT) state = "SSL_Connect";
   else if (w & SSL_ST_ACCEPT) state = "SSL_Accept";
   else if (w & TLS_ST_BEFORE) state = "TLS_Before";
   else if (w & TLS_ST_OK) state = "TLS_OK";
   else if (w IS SSL_F_SSL_RENEGOTIATE) state = "SSL_Renegotiate";
   else state = "SSL_Undefined";

   if (where & SSL_CB_LOOP) {
      log.msg("%s: Loop: %s", state, SSL_state_string_long(s));
   }
   else if (where & SSL_CB_ALERT) {
      log.msg("%s: %s Alert: %s : %s", state, (where & SSL_CB_READ) ? "Read" : "Write", SSL_alert_type_string_long(ret), SSL_alert_desc_string_long(ret));
   }
   else if (where & SSL_CB_EXIT) {
      if (ret == 0) log.msg("%s: Failed in %s", state, SSL_state_string_long(s));
      else if (ret < 0) log.msg("%s: Error in %s", state, SSL_state_string_long(s));
   }
   else if (where & SSL_CB_HANDSHAKE_START) {
      log.msg("%s: Handshake Start: %s", state, SSL_state_string_long(s));
   }
   else if (where & SSL_CB_HANDSHAKE_DONE) {
      log.msg("%s: Handshake Done: %s", state, SSL_state_string_long(s));
   }
   else log.msg("%s: Unknown: %s", state, SSL_state_string_long(s));
}

//********************************************************************************************************************

static void sslCtxMsgCallback(SSL *s, int where, int ret) __attribute__ ((unused));
static void sslCtxMsgCallback(SSL *s, int where, int ret)
{
   sslMsgCallback(s, where, ret);
}

//********************************************************************************************************************
// This only needs to be called once to setup the unique SSL context for the NetSocket object and the locations of the
// certificates.

static ERR sslSetup(extNetSocket *Self)
{
   std::string path;
   ERR error;
   pf::Log log(__FUNCTION__);

   // Thread-safe SSL initialization
   static std::mutex ssl_init_mutex;
   static bool ssl_initialized = false;
   
   {
      std::lock_guard<std::mutex> lock(ssl_init_mutex);
      if (!ssl_initialized) {
         OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
         ssl_initialized = true;
      }
   }

   if (Self->CTX) return ERR::Okay;

   log.traceBranch();

   if ((Self->CTX = SSL_CTX_new(TLS_client_method()))) {
      //if (GetResource(RES::LOG_LEVEL) > 3) SSL_CTX_set_info_callback(Self->CTX, (void *)&sslCtxMsgCallback);

      if (ResolvePath("config:ssl/certs", RSF::NO_FILE_CHECK, &path) IS ERR::Okay) {
         if (SSL_CTX_load_verify_locations(Self->CTX, nullptr, path.c_str())) {
            if ((Self->SSL = SSL_new(Self->CTX))) {
               log.msg("SSL connectivity has been configured successfully.");

               if (GetResource(RES::LOG_LEVEL) > 3) SSL_set_info_callback(Self->SSL, &sslMsgCallback);

               return ERR::Okay;
            }
            else { log.warning("Failed to initialise new SSL object."); error = ERR::SystemCall; }
         }
         else {
            log.warning("Failed to define certificate folder: %s", path.c_str());
            error = ERR::SystemCall;
         }
      }
      else error = ERR::ResolvePath;

      SSL_CTX_free(Self->CTX);
      Self->CTX = nullptr;
   }
   else {
      log.warning("SSL_CTX_new: %s", ERR_error_string(ERR_get_error(), nullptr));
      error = ERR::SystemCall;
   }

   return error;
}

//********************************************************************************************************************

static ERR sslLinkSocket(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch();

   if ((Self->BIO = BIO_new_socket(Self->SocketHandle, BIO_NOCLOSE))) {
      SSL_set_bio(Self->SSL, Self->BIO, Self->BIO);
//      SSL_ctrl(Self->SSL, SSL_CTRL_MODE,(SSL_MODE_AUTO_RETRY), nullptr); // SSL library will process 'non-application' data automatically [good]
      SSL_ctrl(Self->SSL, SSL_CTRL_MODE,(SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER), nullptr);
      SSL_ctrl(Self->SSL, SSL_CTRL_MODE,(SSL_MODE_ENABLE_PARTIAL_WRITE), nullptr);
   }
   else log.warning("Failed to create a SSL BIO object.");

   return ERR::Okay;
}

//********************************************************************************************************************
// To establish an SSL connection, this function must be called after the initial connect() has succeeded.  If a
// NetSocket has the NSF_SSL flag set, then the connection is handled automatically.  Otherwise a plain text socket
// connection can be converted to SSL at any time (if the server is ready for it) by calling this function.
//
// The state will be changed to NTC::CONNECTED if the SSL connection is established immediately, otherwise
// NTC::CONNECTING_SSL may be used to indicate that the connection is ongoing.  If a failure occurs, the state is set to
// NTC::DISCONNECTED and the Error field is set appropriately.

static ERR sslConnect(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch();

   if (!Self->SSL) return ERR::FieldNotSet;

   auto result = SSL_connect(Self->SSL);

   if (result <= 0) {
      result = SSL_get_error(Self->SSL, result);

      // The SSL routine may respond with WANT_READ or WANT_WRITE when
      // non-blocking sockets are used.  This is technically not an error.

      switch(result) {
         case SSL_ERROR_NONE:             Self->Error = ERR::Okay;
                                          return ERR::Okay;

         case SSL_ERROR_ZERO_RETURN:      Self->Error = ERR::Disconnected; break;

         case SSL_ERROR_WANT_READ:        Self->setState(NTC::CONNECTING_SSL);
                                          return ERR::Okay;

         case SSL_ERROR_WANT_WRITE:       Self->setState(NTC::CONNECTING_SSL);
                                          return ERR::Okay;

         case SSL_ERROR_WANT_CONNECT:     Self->Error = ERR::WouldBlock; break;
         case SSL_ERROR_WANT_ACCEPT:      Self->Error = ERR::WouldBlock; break;
         case SSL_ERROR_WANT_X509_LOOKUP: Self->Error = ERR::Retry; break;
         case SSL_ERROR_SYSCALL:          Self->Error = ERR::InputOutput; break;

         case SSL_ERROR_SSL:              Self->Error = ERR::SystemCall;
                                          ERR_print_errors(Self->BIO);
                                          break;

         default:                         Self->Error = ERR::Failed;
      }

      log.warning("SSL_connect: %s (%s)", ERR_error_string(result, nullptr), GetErrorMsg(Self->Error));
      Self->setState(NTC::DISCONNECTED);
      return Self->Error;
   }
   else {
      log.trace("sslConnect:","SSL server connection successful.");
      Self->setState(NTC::CONNECTED);
      return ERR::Okay;
   }
}

//********************************************************************************************************************
// Handshaking may be required during normal read/write operations.  This routine simply tells SSL to continue with its
// handshake and then ceases monitoring of the FD.  If SSL then needs to continue its handshake then it will tell us in
// the RECEIVE() and SEND() functions.

static void ssl_handshake_write(SOCKET_HANDLE Socket, extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   log.msg("Socket: %" PF64, (MAXINT)Socket);

   if (auto result = SSL_do_handshake(Self->SSL); result == 1) { // Handshake successful, connection established
      #ifdef __linux__
         RegisterFD((HOSTHANDLE)Socket, RFD::WRITE|RFD::REMOVE|RFD::SOCKET, &ssl_handshake_write, Self);
      #elif _WIN32
         if ((Self->WriteSocket) or (Self->Outgoing.defined()) or (Self->WriteQueue.Buffer)) {
            // Do nothing, we are already listening for writes
            log.trace("Write socket is already listening for writes.");
         }
         else win_socketstate((WSW_SOCKET)Socket, -1, FALSE); // Turn off write listening
      #endif

      Self->SSLBusy = SSL_NOT_BUSY;
   }
   else switch (SSL_get_error(Self->SSL, result)) {
      case SSL_ERROR_WANT_READ:
         #ifdef _WIN32
            win_socketstate((WSW_SOCKET)Socket, TRUE, -1);
         #else
            #warning Platform support required.
         #endif
         break;

      case SSL_ERROR_WANT_WRITE:
         #ifdef _WIN32
            win_socketstate((WSW_SOCKET)Socket, -1, TRUE);
         #else
            #warning Platform support required.
         #endif
         break;

      default:
         Self->SSLBusy = SSL_NOT_BUSY;
   }
}

static void ssl_handshake_read(SOCKET_HANDLE Socket, APTR Data)
{
   pf::Log log(__FUNCTION__);
   extNetSocket *Self = reinterpret_cast<extNetSocket *>(Data);

   log.msg("Socket: %" PF64, (MAXINT)Socket);

   if (auto result = SSL_do_handshake(Self->SSL); result == 1) { // Handshake successful, connection established
      #ifdef __linux__
         RegisterFD((HOSTHANDLE)Socket, RFD::READ|RFD::REMOVE|RFD::SOCKET, &ssl_handshake_read, Self);
      #elif _WIN32
         // No need to remove any handle monitoring, client_server_incoming() will do so automatically if
         // necessary when new data arrives.
      #endif

      Self->SSLBusy = SSL_NOT_BUSY;
   }
   else switch (SSL_get_error(Self->SSL, result)) {
      case SSL_ERROR_WANT_READ:
         #ifdef _WIN32
            win_socketstate((WSW_SOCKET)Socket, TRUE, -1);
         #else
            #warning Platform support required.
         #endif
         break;

      case SSL_ERROR_WANT_WRITE:
         #ifdef _WIN32
            win_socketstate((WSW_SOCKET)Socket, -1, TRUE);
         #else
            #warning Platform support required.
         #endif
         break;

      default:
         Self->SSLBusy = SSL_NOT_BUSY;
   }
}
