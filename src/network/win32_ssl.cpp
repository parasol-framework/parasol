/*********************************************************************************************************************

Windows-specific SSL implementation using wrapper
This file provides the same interface as ssl.cpp but uses the Windows SSL wrapper internally.

*********************************************************************************************************************/

#ifdef _WIN32
#ifdef ENABLE_SSL

/*********************************************************************************************************************
** Initialize SSL wrapper for Windows (no-op since wrapper auto-initializes)
*/

static ERR sslInit(void)
{
   return ssl_wrapper_init();
}

/*********************************************************************************************************************
** Disconnect SSL connection on Windows
*/

static void sslDisconnect(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   if (Self->WinSSL) {
      log.traceBranch("Closing Windows SSL connection.");
      ssl_wrapper_free_context(Self->WinSSL);
      Self->WinSSL = nullptr;
   }
}

/*********************************************************************************************************************
** Setup SSL context for Windows
*/

static ERR sslSetup(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   if (Self->WinSSL) return ERR::Okay;

   log.traceBranch("Setting up Windows SSL context.");

   Self->WinSSL = ssl_wrapper_create_context();
   if (!Self->WinSSL) {
      log.warning("Failed to create Windows SSL context");
      return ERR::Failed;
   }

   log.msg("Windows SSL connectivity has been configured successfully.");
   return ERR::Okay;
}

/*********************************************************************************************************************
** Connect SSL using Windows wrapper
*/

static ERR sslConnect(extNetSocket *Self)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Connecting SSL using Windows wrapper.");

   if (!Self->WinSSL) return ERR::FieldNotSet;

   // Set the socket handle for the SSL wrapper
   SSL_ERROR_CODE result = ssl_wrapper_set_socket(Self->WinSSL, Self->SocketHandle);
   if (result != SSL_OK) {
      log.warning("Failed to set socket for Windows SSL: %d", result);
      Self->Error = ERR::Failed;
      Self->setState(NTC::DISCONNECTED);
      return ERR::Failed;
   }

   // Attempt the SSL connection
   result = ssl_wrapper_connect(Self->WinSSL);
   
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
         log.warning("Windows SSL connection failed: %d", result);
         Self->Error = ERR::Failed;
         Self->setState(NTC::DISCONNECTED);
         return ERR::Failed;
   }
}

#endif // ENABLE_SSL
#endif // _WIN32