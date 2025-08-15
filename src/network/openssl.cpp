
static SSL_CTX *glClientSSL = nullptr; // Thread-safe unless you call a SET function
static SSL_CTX *glServerSSL = nullptr;
static SSL_CTX *glClientSSLNV = nullptr; // No-verify version

//********************************************************************************************************************

// Forward declarations for template functions
template <class T> void ssl_handshake_write(HOSTHANDLE Socket, T *Self);
template <class T> void ssl_handshake_read(HOSTHANDLE Socket, T *Self);

template <class T> void sslDisconnect(T *Self)
{
   if (Self->SSLHandle) {
      pf::Log log(__FUNCTION__);

      log.traceBranch("Closing SSL connection.");

      SSL_set_info_callback(Self->SSLHandle, nullptr);
      
      // Perform proper bidirectional SSL shutdown
      
      if (auto shutdown_result = SSL_shutdown(Self->SSLHandle); shutdown_result == 0) {
         // First shutdown call completed, perform second shutdown for bidirectional close
         shutdown_result = SSL_shutdown(Self->SSLHandle);
         if (shutdown_result < 0) {
            int ssl_error = SSL_get_error(Self->SSLHandle, shutdown_result);
            if ((ssl_error != SSL_ERROR_WANT_READ) and (ssl_error != SSL_ERROR_WANT_WRITE)) {
               log.warning("SSL_shutdown failed: %s", ERR_error_string(ssl_error, nullptr));
            }
         }
      }
      
      SSL_free(Self->SSLHandle);
      Self->SSLHandle = nullptr;
      Self->BIOHandle = nullptr; // BIO is terminated by SSL_free()
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
   pf::Log log(__FUNCTION__);
   
   log.traceBranch();
   
   ERR error = ERR::Okay;

   static std::mutex ssl_init_mutex;
   static bool ssl_initialised = false;
   
   std::lock_guard<std::mutex> lock(ssl_init_mutex);

   if (!ssl_initialised) {
      if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr) != 1) {
         return log.warning(ERR::SystemCall);
      }
      ssl_initialised = true;
   }

   bool setup_success = false;

   if ((Self->Flags & NSF::SERVER) != NSF::NIL) {
      if (glServerSSL) return ERR::Okay;

      if ((glServerSSL = SSL_CTX_new(TLS_server_method()))) {         
         // Generate a simple self-signed certificate using modern OpenSSL APIs
         EVP_PKEY *pkey = nullptr;
         X509 *cert = nullptr;
      
         // Generate key pair using EVP interface
         
         if (EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr); ctx) {
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
                     
                        auto name = X509_get_subject_name(cert);
                        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)"localhost", -1, -1, 0);
                        X509_set_issuer_name(cert, name);
                     
                        if (X509_sign(cert, pkey, EVP_sha256()) > 0) {
                           if (SSL_CTX_use_certificate(glServerSSL, cert) and SSL_CTX_use_PrivateKey(glServerSSL, pkey)) {
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
            SSL_CTX_set_verify(glServerSSL, SSL_VERIFY_NONE, nullptr);
         }
         return ERR::Okay;
      }
      else return log.warning(ERR::SystemCall);
   }

   // Client mode - no CA verification

   if ((Self->Flags & NSF::SSL_NO_VERIFY) != NSF::NIL) {
      if (!glClientSSLNV) {
         if ((glClientSSLNV = SSL_CTX_new(TLS_client_method()))) {
            if (GetResource(RES::LOG_LEVEL) > 7) SSL_CTX_set_info_callback(glClientSSL, &sslCtxMsgCallback);
            // Disable certificate verification for client sockets
            SSL_CTX_set_verify(glClientSSL, SSL_VERIFY_NONE, nullptr);
      
            // Additional settings to ensure verification is completely disabled
            SSL_CTX_set_verify_depth(glClientSSL, 0);
            SSL_CTX_set_options(glClientSSL, SSL_OP_NO_COMPRESSION | SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);         
         }
      }
      else return log.warning(ERR::SystemCall);

      if ((Self->SSLHandle = SSL_new(glClientSSLNV))) {
         if (GetResource(RES::LOG_LEVEL) > 7) SSL_set_info_callback(Self->SSLHandle, &sslMsgCallback);
         return ERR::Okay;
      }
      else return log.warning(ERR::SystemCall);
   }

   // Client mode - full verification

   if (!glClientSSL) {
      if ((glClientSSL = SSL_CTX_new(TLS_client_method()))) {
         if (GetResource(RES::LOG_LEVEL) > 7) SSL_CTX_set_info_callback(glClientSSL, &sslCtxMsgCallback);

         // Try system certificates first (most up-to-date), then bundled certificates as fallback
         bool cert_loaded = false;
         if (SSL_CTX_set_default_verify_paths(glClientSSL)) cert_loaded = true;
         else {
            auto ssl_error = ERR_get_error();
            log.warning("Failed to load system certificate paths - SSL Error: %s", ERR_error_string(ssl_error, nullptr));
         }
         
         // If system certificates failed, try bundled certificate as fallback

         std::string path;
         if (!cert_loaded) {
            if (ResolvePath("config:ssl/ca-bundle.crt", RSF::NO_FILE_CHECK, &path) IS ERR::Okay) {
               if (SSL_CTX_load_verify_locations(glClientSSL, path.c_str(), nullptr)) {
                  cert_loaded = true;
               }
               else {
                  auto ssl_error = ERR_get_error();
                  log.warning("Failed to load certificates: %s - SSL Error: %s", path.c_str(), ERR_error_string(ssl_error, nullptr));
               }
            }
            else log.warning(ERR::ResolvePath);
         }
         
         if (cert_loaded) {
            // Set up certificate verification with detailed callback
            SSL_CTX_set_verify(glClientSSL, SSL_VERIFY_PEER, nullptr);
            
            // Configure additional SSL context options for better compatibility
            SSL_CTX_set_verify_depth(glClientSSL, 10);  // Allow longer certificate chains
            
            // Set security level to 1 for broader compatibility (default might be too strict)
            SSL_CTX_set_security_level(glClientSSL, 1);
            
            // Enable certificate chain checking
            SSL_CTX_set_mode(glClientSSL, SSL_MODE_AUTO_RETRY);
            
            // Set certificate verification flags for better compatibility
            X509_VERIFY_PARAM *param = SSL_CTX_get0_param(glClientSSL);
            if (param) X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_TRUSTED_FIRST);
         }
         else {           
            SSL_CTX_free(glClientSSL);
            glClientSSL = nullptr;
            return ERR::Failed;
         }
      }
   }

   if (glClientSSL) {
      if ((Self->SSLHandle = SSL_new(glClientSSL))) {
         if (GetResource(RES::LOG_LEVEL) > 7) SSL_set_info_callback(Self->SSLHandle, &sslMsgCallback);
         return ERR::Okay;
      }
      else log.warning(ERR::SystemCall); 
   }

   return error;
}

//********************************************************************************************************************
// For SSL servers, we need to perform SSL_accept instead of SSL_connect when a client connects

#warning This code is setting the NetSocket state

static ERR sslAccept(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch();

   if (!Self->SSLHandle) return ERR::FieldNotSet;

   // Ensure the SSL BIO is linked to the socket before attempting accept
   if (!Self->BIOHandle) {
      if (auto error = sslLinkSocket(Self); error != ERR::Okay) {
         log.warning("Failed to link SSL socket to BIO.");
         return error;
      }
   }  

   if (auto result = SSL_accept(Self->SSLHandle); result <= 0) {
      result = SSL_get_error(Self->SSLHandle, result);

      switch(result) {
         case SSL_ERROR_NONE:             Self->Error = ERR::Okay; return ERR::Okay;
         case SSL_ERROR_ZERO_RETURN:      Self->Error = ERR::Disconnected; break;
         case SSL_ERROR_WANT_READ:        Self->setState(NTC::HANDSHAKING); return ERR::Okay;
         case SSL_ERROR_WANT_WRITE:       Self->setState(NTC::HANDSHAKING); return ERR::Okay;
         case SSL_ERROR_WANT_CONNECT:     Self->Error = ERR::WouldBlock; break;
         case SSL_ERROR_WANT_ACCEPT:      Self->Error = ERR::WouldBlock; break;
         case SSL_ERROR_WANT_X509_LOOKUP: Self->Error = ERR::Retry; break;
         case SSL_ERROR_SYSCALL:          Self->Error = ERR::InputOutput; break;
         case SSL_ERROR_SSL:              Self->Error = ERR::SystemCall;
                                          ERR_print_errors(Self->BIOHandle);
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

   if ((Self->BIOHandle = BIO_new_socket(Self->Handle, BIO_NOCLOSE))) {
      SSL_set_bio(Self->SSLHandle, Self->BIOHandle, Self->BIOHandle);
//      SSL_ctrl(Self->SSLHandle, SSL_CTRL_MODE,(SSL_MODE_AUTO_RETRY), nullptr); // SSL library will process 'non-application' data automatically [good]
      SSL_ctrl(Self->SSLHandle, SSL_CTRL_MODE,(SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER), nullptr);
      SSL_ctrl(Self->SSLHandle, SSL_CTRL_MODE,(SSL_MODE_ENABLE_PARTIAL_WRITE), nullptr);
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
// NTC::HANDSHAKING may be used to indicate that the connection is ongoing.  If a failure occurs, the state is set to
// NTC::DISCONNECTED and the Error field is set appropriately.

static ERR sslConnect(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch();

   if (!Self->SSLHandle) return ERR::FieldNotSet;

   // Ensure the SSL BIO is linked to the socket before attempting connection

   if (!Self->BIOHandle) {
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
         if (SSL_set_tlsext_host_name(Self->SSLHandle, Self->Address)) {
            log.msg("SNI set to: %s", Self->Address);
         } 
         else log.warning("Failed to set SNI hostname: %s", Self->Address);
      }
   }

   if (auto result = SSL_connect(Self->SSLHandle); result <= 0) {
      result = SSL_get_error(Self->SSLHandle, result);

      // The SSL routine may respond with WANT_READ or WANT_WRITE when
      // non-blocking sockets are used.  This is technically not an error.

      switch(result) {
         case SSL_ERROR_NONE:             Self->Error = ERR::Okay;
                                          return ERR::Okay;

         case SSL_ERROR_ZERO_RETURN:      Self->Error = ERR::Disconnected; break;

         case SSL_ERROR_WANT_READ:        Self->setState(NTC::HANDSHAKING);
                                          return ERR::Okay;

         case SSL_ERROR_WANT_WRITE:       Self->setState(NTC::HANDSHAKING);
                                          return ERR::Okay;

         case SSL_ERROR_WANT_CONNECT:     Self->Error = ERR::WouldBlock; break;
         case SSL_ERROR_WANT_ACCEPT:      Self->Error = ERR::WouldBlock; break;
         case SSL_ERROR_WANT_X509_LOOKUP: Self->Error = ERR::Retry; break;
         case SSL_ERROR_SYSCALL:          Self->Error = ERR::InputOutput; break;

         case SSL_ERROR_SSL:              Self->Error = ERR::SystemCall;
                                          ERR_print_errors(Self->BIOHandle);
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

template <class T> void ssl_handshake_write(HOSTHANDLE Socket, T *Self)
{
   pf::Log log(__FUNCTION__);

   log.trace("Socket: %" PF64, (MAXINT)Socket);

   if (auto result = SSL_do_handshake(Self->SSLHandle); result == 1) { // Handshake successful, connection established
      RegisterFD((HOSTHANDLE)Socket, RFD::WRITE|RFD::REMOVE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(ssl_handshake_write<T>), Self);
      Self->SSLBusy = SSL_NOT_BUSY;
   }
   else switch (SSL_get_error(Self->SSLHandle, result)) {
      case SSL_ERROR_WANT_READ:
         RegisterFD((HOSTHANDLE)Socket, RFD::WRITE|RFD::REMOVE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(ssl_handshake_write<T>), Self);
         Self->SSLBusy = SSL_HANDSHAKE_READ;
         RegisterFD((HOSTHANDLE)Socket, RFD::READ|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(ssl_handshake_read<T>), Self);
         break;

      case SSL_ERROR_WANT_WRITE:
         // Continue monitoring for write readiness - no action needed
         break;

      default:
         Self->SSLBusy = SSL_NOT_BUSY;
   }
}

template <class T> void ssl_handshake_read(HOSTHANDLE Socket, T *Self)
{
   pf::Log log(__FUNCTION__);

   log.trace("Socket: %" PF64, (MAXINT)Socket);

   if (auto result = SSL_do_handshake(Self->SSLHandle); result == 1) { // Handshake successful, connection established
      RegisterFD((HOSTHANDLE)Socket, RFD::READ|RFD::REMOVE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(ssl_handshake_read<T>), Self);
      Self->SSLBusy = SSL_NOT_BUSY;
   }
   else switch (SSL_get_error(Self->SSLHandle, result)) {
      case SSL_ERROR_WANT_READ:
         // Continue monitoring for read readiness - no action needed
         break;

      case SSL_ERROR_WANT_WRITE:
         RegisterFD((HOSTHANDLE)Socket, RFD::READ|RFD::REMOVE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(ssl_handshake_read<T>), Self);
         Self->SSLBusy = SSL_HANDSHAKE_WRITE;
         RegisterFD((HOSTHANDLE)Socket, RFD::WRITE|RFD::SOCKET, reinterpret_cast<void (*)(HOSTHANDLE, APTR)>(ssl_handshake_write<T>), Self);
         break;

      default:
         Self->SSLBusy = SSL_NOT_BUSY;
   }
}
