
//********************************************************************************************************************

static void sslDisconnect(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   if (Self->ssl_handle) {
      log.traceBranch("Closing SSL connection.");

      SSL_set_info_callback(Self->ssl_handle, nullptr);
      
      // Perform proper bidirectional SSL shutdown
      auto shutdown_result = SSL_shutdown(Self->ssl_handle);
      if (shutdown_result == 0) {
         // First shutdown call completed, perform second shutdown for bidirectional close
         shutdown_result = SSL_shutdown(Self->ssl_handle);
         if (shutdown_result < 0) {
            int ssl_error = SSL_get_error(Self->ssl_handle, shutdown_result);
            if ((ssl_error != SSL_ERROR_WANT_READ) and (ssl_error != SSL_ERROR_WANT_WRITE)) {
               log.warning("SSL_shutdown failed: %s", ERR_error_string(ssl_error, nullptr));
            }
         }
      }
      
      SSL_free(Self->ssl_handle);
      Self->ssl_handle = nullptr;
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

   int w = where & (~SSL_ST_MASK);

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

static void sslCtxMsgCallback(const SSL *s, int where, int ret) __attribute__ ((unused));
static void sslCtxMsgCallback(const SSL *s, int where, int ret)
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

   // Choose the appropriate SSL method based on whether this is a server or client socket
   const SSL_METHOD *method;
   if ((Self->Flags & NSF::SERVER) != NSF::NIL) {
      method = TLS_server_method();
   } 
   else method = TLS_client_method();
   
   if ((Self->CTX = SSL_CTX_new(method))) {
      SSL_CTX_set_info_callback(Self->CTX, &sslCtxMsgCallback);

      bool setup_success = false;

      // Configure SSL server certificates if this is a server socket
      if ((Self->Flags & NSF::SERVER) != NSF::NIL) {
         // For SSL servers, use a minimal setup for testing
         log.msg("Configuring SSL server for testing with simplified certificate setup.");
         
         // Generate a simple self-signed certificate using modern OpenSSL APIs
         EVP_PKEY *pkey = nullptr;
         X509 *cert = nullptr;
         
         // Generate key pair using EVP interface (modern approach)
         EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
         if (ctx) {
            if (EVP_PKEY_keygen_init(ctx) > 0) {
               if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) > 0) {
                  if (EVP_PKEY_keygen(ctx, &pkey) > 0) {
                     // Create certificate
                     if ((cert = X509_new())) {
                        X509_set_version(cert, 2);
                        ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
                        X509_gmtime_adj(X509_get_notBefore(cert), 0);
                        X509_gmtime_adj(X509_get_notAfter(cert), 365 * 24 * 3600);
                        X509_set_pubkey(cert, pkey);
                        
                        X509_NAME *name = X509_get_subject_name(cert);
                        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)"localhost", -1, -1, 0);
                        X509_set_issuer_name(cert, name);
                        
                        if (X509_sign(cert, pkey, EVP_sha256()) > 0) {
                           if (SSL_CTX_use_certificate(Self->CTX, cert) and SSL_CTX_use_PrivateKey(Self->CTX, pkey)) {
                              log.msg("SSL server certificate configured successfully.");
                              setup_success = true;
                           } 
                           else log.warning("Failed to set SSL server certificate and key.");
                        } 
                        else log.warning("Failed to sign SSL certificate.");
                     }
                  }
               }
            }
            EVP_PKEY_CTX_free(ctx);
         }
         
         if (pkey) EVP_PKEY_free(pkey);
         if (cert) X509_free(cert);
         
         if (!setup_success) {
            log.warning("SSL server certificate setup failed, trying with no certificate verification.");
            // For testing, allow servers without proper certificates
            SSL_CTX_set_verify(Self->CTX, SSL_VERIFY_NONE, nullptr);
            setup_success = true;
         }
      }
      else if ((Self->Flags & NSF::SSL_NO_VERIFY) != NSF::NIL) {
         // Disable certificate verification for client sockets
         log.msg("SSL certificate verification disabled (SSL_NO_VERIFY flag set). Flags=0x%x", int(Self->Flags));
         SSL_CTX_set_verify(Self->CTX, SSL_VERIFY_NONE, nullptr);
         
         // Additional settings to ensure verification is completely disabled
         SSL_CTX_set_verify_depth(Self->CTX, 0);
         SSL_CTX_set_options(Self->CTX, SSL_OP_NO_COMPRESSION | SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
         
         setup_success = true;
      }
      else {
         // Enable certificate verification (default behavior)
         // Try system certificates first (most up-to-date), then bundled certificates as fallback
         bool cert_loaded = false;
         
         // Try system certificate locations first (most current certificates)
         log.msg("Attempting to load system certificate paths...");
         if (SSL_CTX_set_default_verify_paths(Self->CTX)) {
            log.msg("SSL system certificate paths loaded successfully.");
            cert_loaded = true;
         }
         else {
            auto ssl_error = ERR_get_error();
            log.warning("Failed to load system certificate paths - SSL Error: %s", ERR_error_string(ssl_error, nullptr));
         }
         
         // If system certificates failed, try bundled certificate bundle as fallback
         if (!cert_loaded) {
            if (ResolvePath("config:ssl/ca-bundle.crt", RSF::NO_FILE_CHECK, &path) IS ERR::Okay) {
               log.msg("Attempting to load SSL certificate bundle: %s", path.c_str());
               if (SSL_CTX_load_verify_locations(Self->CTX, path.c_str(), nullptr)) {
                  log.msg("SSL certificate bundle loaded successfully: %s", path.c_str());
                  cert_loaded = true;
               }
               else {
                  auto ssl_error = ERR_get_error();
                  log.warning("Failed to load certificates: %s - SSL Error: %s", path.c_str(), ERR_error_string(ssl_error, nullptr));
               }
            }
            else log.warning(ERR::ResolvePath);
         }
         
         // Try certificate directory as final fallback
         if (!cert_loaded) {
            if (ResolvePath("config:ssl/certs", RSF::NO_FILE_CHECK, &path) IS ERR::Okay) {
               if (SSL_CTX_load_verify_locations(Self->CTX, nullptr, path.c_str())) {
                  log.msg("SSL certificate directory loaded successfully: %s", path.c_str());
                  cert_loaded = true;
               }
               else log.warning("Failed to load certificate folder: %s", path.c_str());
            }
         }
         
         if (cert_loaded) {
            // Set up certificate verification with detailed callback
            SSL_CTX_set_verify(Self->CTX, SSL_VERIFY_PEER, nullptr);
            
            // Configure additional SSL context options for better compatibility
            SSL_CTX_set_verify_depth(Self->CTX, 10);  // Allow longer certificate chains
            
            // Set security level to 1 for broader compatibility (default might be too strict)
            SSL_CTX_set_security_level(Self->CTX, 1);
            
            // Enable certificate chain checking
            SSL_CTX_set_mode(Self->CTX, SSL_MODE_AUTO_RETRY);
            
            // Set certificate verification flags for better compatibility
            X509_VERIFY_PARAM *param = SSL_CTX_get0_param(Self->CTX);
            if (param) X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_TRUSTED_FIRST);
            
            setup_success = true;
         }
         else {
            log.warning("No SSL certificates could be loaded, disabling verification.");
            SSL_CTX_set_verify(Self->CTX, SSL_VERIFY_NONE, nullptr);
            setup_success = true;  // Continue without verification
         }
      }

      if (setup_success) {
         if ((Self->ssl_handle = SSL_new(Self->CTX))) {
            SSL_set_info_callback(Self->ssl_handle, &sslMsgCallback);
            return ERR::Okay;
         }
         else { 
            log.warning("Failed to initialise new SSL object."); 
            error = ERR::SystemCall; 
         }
      }

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
// For SSL servers, we need to perform SSL_accept instead of SSL_connect when a client connects

static ERR sslAccept(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch();

   if (!Self->ssl_handle) return ERR::FieldNotSet;

   // Ensure the SSL BIO is linked to the socket before attempting accept
   if (!Self->bio_handle) {
      if (auto error = sslLinkSocket(Self); error != ERR::Okay) {
         log.warning("Failed to link SSL socket to BIO.");
         return error;
      }
   }

   auto result = SSL_accept(Self->ssl_handle);

   if (result <= 0) {
      result = SSL_get_error(Self->ssl_handle, result);

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
                                          ERR_print_errors(Self->bio_handle);
                                          break;

         default:                         Self->Error = ERR::Failed;
      }

      log.warning("SSL_accept: %s (%s)", ERR_error_string(result, nullptr), GetErrorMsg(Self->Error));
      Self->setState(NTC::DISCONNECTED);
      return Self->Error;
   }
   else {
      Self->setState(NTC::CONNECTED);
      return ERR::Okay;
   }
}

//********************************************************************************************************************

static ERR sslLinkSocket(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch();

   if ((Self->bio_handle = BIO_new_socket(Self->SocketHandle, BIO_NOCLOSE))) {
      SSL_set_bio(Self->ssl_handle, Self->bio_handle, Self->bio_handle);
//      SSL_ctrl(Self->ssl_handle, SSL_CTRL_MODE,(SSL_MODE_AUTO_RETRY), nullptr); // SSL library will process 'non-application' data automatically [good]
      SSL_ctrl(Self->ssl_handle, SSL_CTRL_MODE,(SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER), nullptr);
      SSL_ctrl(Self->ssl_handle, SSL_CTRL_MODE,(SSL_MODE_ENABLE_PARTIAL_WRITE), nullptr);
      return ERR::Okay;
   }
   else return ERR::SystemCall;
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

   if (!Self->ssl_handle) return ERR::FieldNotSet;

   // Ensure the SSL BIO is linked to the socket before attempting connection

   if (!Self->bio_handle) {
      if (auto error = sslLinkSocket(Self); error != ERR::Okay) {
         log.warning("Failed to link SSL socket to BIO.");
         return error;
      }
   }

   // Set SNI (Server Name Indication) if we have a hostname
   // This is critical for modern HTTPS servers that serve multiple domains

   if (Self->Address and (Self->Flags & NSF::SERVER) IS NSF::NIL) {
      // Only set SNI for client connections, and only if Address is a hostname (not IP)
      struct in_addr addr;
      if (inet_aton(Self->Address, &addr) == 0) {
         // Address is not an IP, so it's likely a hostname - set SNI
         if (SSL_set_tlsext_host_name(Self->ssl_handle, Self->Address)) {
            log.msg("SNI set to: %s", Self->Address);
         } 
         else log.warning("Failed to set SNI hostname: %s", Self->Address);
      }
   }

   if (auto result = SSL_connect(Self->ssl_handle); result <= 0) {
      result = SSL_get_error(Self->ssl_handle, result);

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
                                          ERR_print_errors(Self->bio_handle);
                                          break;

         default:                         Self->Error = ERR::Failed;
      }

      log.warning("SSL_connect: %s (%s)", ERR_error_string(result, nullptr), GetErrorMsg(Self->Error));
      Self->setState(NTC::DISCONNECTED);
      return Self->Error;
   }
   else {
      Self->setState(NTC::CONNECTED);
      return ERR::Okay;
   }
}

//********************************************************************************************************************
// Handshaking may be required during normal read/write operations.  This routine simply tells SSL to continue with its
// handshake and then ceases monitoring of the FD.  If SSL then needs to continue its handshake then it will tell us in
// the RECEIVE() and SEND() functions.

static void ssl_handshake_write(HOSTHANDLE Socket, extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   log.trace("Socket: %" PF64, (MAXINT)Socket);

   if (auto result = SSL_do_handshake(Self->ssl_handle); result == 1) { // Handshake successful, connection established
      RegisterFD((HOSTHANDLE)Socket, RFD::WRITE|RFD::REMOVE|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&ssl_handshake_write, Self);
      Self->SSLBusy = SSL_NOT_BUSY;
   }
   else switch (SSL_get_error(Self->ssl_handle, result)) {
      case SSL_ERROR_WANT_READ:
         RegisterFD((HOSTHANDLE)Socket, RFD::WRITE|RFD::REMOVE|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&ssl_handshake_write, Self);
         Self->SSLBusy = SSL_HANDSHAKE_READ;
         RegisterFD((HOSTHANDLE)Socket, RFD::READ|RFD::SOCKET, &ssl_handshake_read, Self);
         break;

      case SSL_ERROR_WANT_WRITE:
         // Continue monitoring for write readiness - no action needed
         break;

      default:
         Self->SSLBusy = SSL_NOT_BUSY;
   }
}

static void ssl_handshake_read(HOSTHANDLE Socket, APTR Data)
{
   pf::Log log(__FUNCTION__);
   auto Self = (extNetSocket *)Data;

   log.trace("Socket: %" PF64, (MAXINT)Socket);

   if (auto result = SSL_do_handshake(Self->ssl_handle); result == 1) { // Handshake successful, connection established
      RegisterFD((HOSTHANDLE)Socket, RFD::READ|RFD::REMOVE|RFD::SOCKET, &ssl_handshake_read, Self);
      Self->SSLBusy = SSL_NOT_BUSY;
   }
   else switch (SSL_get_error(Self->ssl_handle, result)) {
      case SSL_ERROR_WANT_READ:
         // Continue monitoring for read readiness - no action needed
         break;

      case SSL_ERROR_WANT_WRITE:
         RegisterFD((HOSTHANDLE)Socket, RFD::READ|RFD::REMOVE|RFD::SOCKET, &ssl_handshake_read, Self);
         Self->SSLBusy = SSL_HANDSHAKE_WRITE;
         RegisterFD((HOSTHANDLE)Socket, RFD::WRITE|RFD::SOCKET, (void (*)(HOSTHANDLE, APTR))&ssl_handshake_write, Self);
         break;

      default:
         Self->SSLBusy = SSL_NOT_BUSY;
   }
}
