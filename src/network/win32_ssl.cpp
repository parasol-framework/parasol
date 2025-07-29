/*********************************************************************************************************************

Windows-specific SSL implementation using wrapper
This file provides the same interface as ssl.cpp but uses the Windows SSL wrapper internally.

*********************************************************************************************************************/

#ifdef _WIN32
#ifdef ENABLE_SSL

//********************************************************************************************************************
// Disconnect SSL connection on Windows

static void sslDisconnect(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   if (Self->WinSSL) {
      log.traceBranch("Closing Windows SSL connection.");
      ssl_wrapper_free_context(Self->WinSSL);
      Self->WinSSL = nullptr;
   }
}

//********************************************************************************************************************
// Setup SSL context for Windows

static ERR sslSetup(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   if (Self->WinSSL) return ERR::Okay;

   log.traceBranch("Setting up Windows SSL context.");

   if (Self->WinSSL = ssl_wrapper_create_context(); !Self->WinSSL) {
      log.warning("Failed to create Windows SSL context");
      return ERR::Failed;
   }

   log.trace("Windows SSL connectivity has been configured successfully.");
   return ERR::Okay;
}

//********************************************************************************************************************
// Handle SSL handshake data from server.
//
// Handshaking can return error code 0x80090308 (SEC_E_INVALID_TOKEN), this can mean that Windows received malformed
// SSL handshake data from the server.  Win32 error 87 (ERROR_INVALID_PARAMETER) can be caused by server
// certificate/SSL configuration issues

static ERR sslHandshakeReceived(extNetSocket *Self, const void* Data, int Length)
{
   pf::Log log(__FUNCTION__);

   if (!Self->WinSSL or !Data or Length <= 0) return ERR::Args;

   log.traceBranch("Processing SSL handshake data (%d bytes)", Length);

   SSL_ERROR_CODE result = ssl_wrapper_continue_handshake(Self->WinSSL, Data, Length);

   switch (result) {
      case SSL_OK:
         log.trace("SSL handshake completed successfully.");
         Self->setState(NTC::CONNECTED);
         return ERR::Okay;

      case SSL_ERROR_CONNECTING:
         log.trace("SSL handshake continuing, waiting for more data.");
         // Stay in CONNECTING_SSL state
         return ERR::Okay;

      case SSL_ERROR_WOULD_BLOCK:
         log.trace("SSL handshake would block.");
         return ERR::Okay;

      default:
         log.warning("SSL handshake failed: %d; SecStatus: 0x%08X; WinError: %d; %s", result,
            ssl_wrapper_get_last_security_status(Self->WinSSL),
            ssl_wrapper_get_last_win32_error(Self->WinSSL),
            ssl_wrapper_get_error_description(Self->WinSSL));
         Self->Error = ERR::Failed;
         Self->setState(NTC::DISCONNECTED);
         return ERR::Failed;
   }
}

//********************************************************************************************************************
// Connect SSL using Windows wrapper

static ERR sslConnect(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Connecting SSL using Windows wrapper.");

   if (!Self->WinSSL) return ERR::FieldNotSet;

   std::string hostname = Self->Address ? Self->Address : "";
   auto result = ssl_wrapper_connect(Self->WinSSL, (void *)(size_t)Self->SocketHandle, hostname);

   switch (result) {
      case SSL_OK:
         log.trace("Windows SSL server connection successful.");
         Self->setState(NTC::CONNECTED);
         return ERR::Okay;

      case SSL_ERROR_CONNECTING:
         log.trace("Windows SSL connection in progress.");
         Self->setState(NTC::CONNECTING_SSL);
         return ERR::Okay;

      case SSL_ERROR_WOULD_BLOCK:
         Self->Error = ERR::WouldBlock;
         Self->setState(NTC::CONNECTING_SSL);
         return ERR::Okay;

      default:
         log.warning("Windows SSL connection failed with code %d; %s", result, ssl_wrapper_get_error_description(Self->WinSSL));
         log.warning("Security status: 0x%08X, Win32 error: %d",
            ssl_wrapper_get_last_security_status(Self->WinSSL),
            ssl_wrapper_get_last_win32_error(Self->WinSSL));
         Self->Error = ERR::Failed;
         Self->setState(NTC::DISCONNECTED);
         return ERR::Failed;
   }
}

#endif // ENABLE_SSL
#endif // _WIN32