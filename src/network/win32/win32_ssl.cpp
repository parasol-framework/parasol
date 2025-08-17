/*********************************************************************************************************************

Windows-specific SSL implementation using wrapper
This file provides the same interface as ssl.cpp but uses the Windows SSL wrapper internally.

*********************************************************************************************************************/

//********************************************************************************************************************
// Disconnect SSL connection on Windows

template <class T> void sslDisconnect(T *Self)
{
   if (Self->SSLHandle) {
      pf::Log(__FUNCTION__).traceBranch("Closing Windows SSL connection.");
      ssl_free_context(Self->SSLHandle);
      Self->SSLHandle = nullptr;
   }
}

//********************************************************************************************************************
// SSL Debug callback - forwards debug messages from the Windows wrapper to Parasol log system

static void ssl_debug_to_parasol_log(const char* message, int level)
{
   pf::Log log("SSL");
   
   switch (level) {
      case SSL_DEBUG_ERROR: log.warning("%s", message); break;
      case SSL_DEBUG_WARNING: log.warning("%s", message); break;
      case SSL_DEBUG_INFO: log.msg("%s", message); break;
      case SSL_DEBUG_TRACE: log.trace("%s", message); break;
      default: log.trace("%s", message); break;
   }
}

//********************************************************************************************************************
// Setup SSL context for Windows

static ERR sslSetup(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   if (Self->SSLHandle) return ERR::Okay;

   log.traceBranch("Setting up Windows SSL context.");

   // Set up SSL debug callback once

   if (GetResource(RES::LOG_LEVEL) >= 5) {
      static bool debug_callback_set = false;
      if (!debug_callback_set) {
         ssl_set_debug_callback(ssl_debug_to_parasol_log);
         debug_callback_set = true;
      }
   }

   bool validate_cert = (Self->Flags & NSF::SSL_NO_VERIFY) != NSF::NIL ? false : true;
   bool server_mode = ((Self->Flags & NSF::SERVER) != NSF::NIL);
   if (Self->SSLHandle = ssl_create_context(glCertPath, validate_cert, server_mode); !Self->SSLHandle) {
      log.warning("Failed to create Windows SSL server context");
      return ERR::Failed;
   }
   else return ERR::Okay;
}

//********************************************************************************************************************
// Handle SSL handshake data.
//
// Handshaking can return error code 0x80090308 (SEC_E_INVALID_TOKEN), this can mean that Windows received malformed
// SSL handshake data from the server.  Win32 error 87 (ERROR_INVALID_PARAMETER) can be caused by server
// certificate/SSL configuration issues

template <class T> ERR sslHandshakeReceived(T *Self, const void *Data, int Length, int *BytesConsumed = nullptr)
{
   pf::Log log(__FUNCTION__);

   if (!Self->SSLHandle or !Data or Length <= 0) return ERR::Args;
   if (BytesConsumed) *BytesConsumed = 0;

   log.traceBranch("Processing SSL handshake data (%d bytes)", Length);

   int handshake_consumed = 0;
   SSL_ERROR_CODE result = ssl_continue_handshake(Self->SSLHandle, Data, Length, handshake_consumed);

   if (BytesConsumed) *BytesConsumed = handshake_consumed;

   switch (result) {
      case SSL_OK:
         log.trace("SSL handshake completed successfully.");
         Self->setState(NTC::CONNECTED);
         return ERR::Okay;

      case SSL_ERROR_CONNECTING:
         log.trace("SSL handshake continuing, waiting for more data.");
         // Stay in HANDSHAKING state
         return ERR::Okay;

      case SSL_ERROR_WOULD_BLOCK:
         log.trace("SSL handshake would block.");
         return ERR::Okay;

      default:
         log.warning("SSL handshake failed: %d; SecStatus: 0x%08X; WinError: %d; %s", result,
            ssl_last_security_status(Self->SSLHandle),
            ssl_last_win32_error(Self->SSLHandle),
            ssl_error_description(Self->SSLHandle));
         Self->setState(NTC::DISCONNECTED);
         return ERR::Failed;
   }
}

//********************************************************************************************************************
// Handle SSL handshake data from client

template <class T> ERR sslServerHandshakeReceived(T *Self, const void* Data, int Length, std::vector<unsigned char>& ResponseData)
{
   pf::Log log(__FUNCTION__);

   if (!Self->SSLHandle or !Data or Length <= 0) return ERR::Args;

   log.traceBranch("Processing server-side SSL handshake data (%d bytes)", Length);

   SSL_ERROR_CODE result = ssl_accept_handshake(Self->SSLHandle, Data, Length, ResponseData);

   switch (result) {
      case SSL_OK:
         log.trace("Server SSL handshake completed successfully.");
         Self->setState(NTC::CONNECTED);
         return ERR::Okay;

      case SSL_ERROR_CONNECTING:
         log.trace("Server SSL handshake continuing, sending response to client.");
         // Server needs to send response data back to client
         // Stay in HANDSHAKING state
         return ERR::Okay;

      case SSL_ERROR_WOULD_BLOCK:
         log.trace("Server SSL handshake would block.");
         return ERR::Okay;

      default:
         log.warning("Server SSL handshake failed: %d; SecStatus: 0x%08X; WinError: %d; %s", result,
            ssl_last_security_status(Self->SSLHandle),
            ssl_last_win32_error(Self->SSLHandle),
            ssl_error_description(Self->SSLHandle));
         
         ResponseData.clear(); // Clear any partial response data
         Self->setState(NTC::DISCONNECTED);
         return ERR::Failed;
   }
}

//********************************************************************************************************************
// Connect SSL using Windows wrapper - called on receipt of a NTE_CONNECT message

template <class T> ERR sslConnect(T *Self)
{
   pf::Log log(__FUNCTION__);
  
   log.traceBranch("Attempting SSL handshake.");

   if (!Self->SSLHandle) return ERR::FieldNotSet;

   std::string hostname = Self->Address ? Self->Address : "";
   auto result = ssl_connect(Self->SSLHandle, (void *)(size_t)Self->Handle, hostname);

   switch (result) {
      case SSL_OK:
         log.trace("Windows SSL server connection successful.");
         Self->setState(NTC::CONNECTED);
         return ERR::Okay;

      case SSL_ERROR_CONNECTING:
         log.trace("Windows SSL connection in progress.");
         Self->setState(NTC::HANDSHAKING);
         return ERR::Okay;

      case SSL_ERROR_WOULD_BLOCK:
         Self->setState(NTC::HANDSHAKING);
         return ERR::Okay;

      default:
         log.warning("Windows SSL connection failed with code %d; %s", result, ssl_error_description(Self->SSLHandle));
         log.warning("Security status: 0x%08X, Win32 error: %d",
            ssl_last_security_status(Self->SSLHandle),
            ssl_last_win32_error(Self->SSLHandle));
         Self->setState(NTC::DISCONNECTED);
         return ERR::Failed;
   }
}
