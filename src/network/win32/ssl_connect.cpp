
//********************************************************************************************************************
// Perform SSL connect and handshake

SSL_ERROR_CODE ssl_connect(SSL_HANDLE SSL, void *SocketHandle, const std::string &HostName)
{
   if ((!SSL) or ((SOCKET)SocketHandle == INVALID_SOCKET)) return SSL_ERROR_ARGS;

   SSL->socket_handle = (SOCKET)SocketHandle;
   SSL->hostname = HostName;

   if (SSL->context_initialised) return SSL_ERROR_CONNECTING; // Already in handshake process

   // Acquire credentials
   SCHANNEL_CRED cred_data{};
   cred_data.dwVersion = SCHANNEL_CRED_VERSION;

   if (SSL->validate_credentials) cred_data.dwFlags = SCH_CRED_AUTO_CRED_VALIDATION;
   else cred_data.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION;

   cred_data.grbitEnabledProtocols = 0; // Use system defaults

   TimeStamp expiry;
   auto status = AcquireCredentialsHandle(nullptr, const_cast<char*>(UNISP_NAME), SECPKG_CRED_OUTBOUND,
      nullptr, &cred_data, nullptr, nullptr, &SSL->credentials, &expiry);

   if (status != SEC_E_OK) {
      set_error_status(SSL, status, "AcquireCredentialsHandle");
      return SSL_ERROR_FAILED;
   }

   SSL->credentials_acquired = true;

   DWORD flags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
                 ISC_REQ_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;

   SecBuffer out_buffer;
   out_buffer.pvBuffer = nullptr;
   out_buffer.BufferType = SECBUFFER_TOKEN;
   out_buffer.cbBuffer = 0;

   SecBufferDesc out_buffer_desc;
   out_buffer_desc.cBuffers = 1;
   out_buffer_desc.pBuffers = &out_buffer;
   out_buffer_desc.ulVersion = SECBUFFER_VERSION;

   DWORD out_flags;
   status = InitializeSecurityContext(
      &SSL->credentials, nullptr, const_cast<char*>(HostName.c_str()), flags,
      0, SECURITY_NATIVE_DREP, nullptr, 0,
      &SSL->context, &out_buffer_desc, &out_flags, &expiry);

   if (status != SEC_I_CONTINUE_NEEDED) {
      set_error_status(SSL, status, "InitializeSecurityContext");
      return SSL_ERROR_FAILED;
   }

   SSL->context_initialised = true;

   // Send initial handshake data
   if ((out_buffer.cbBuffer > 0) and (out_buffer.pvBuffer != nullptr)) {
      int sent = send(SSL->socket_handle, (char*)out_buffer.pvBuffer, out_buffer.cbBuffer, 0);
      FreeContextBuffer(out_buffer.pvBuffer);

      if (sent == SOCKET_ERROR) {
         int error = WSAGetLastError();
         SSL->last_win32_error = error;
         if (error == WSAEWOULDBLOCK) {
            SSL->error_description = "SSL handshake send would block (WSAEWOULDBLOCK)";
            return SSL_ERROR_WOULD_BLOCK;
         }
         SSL->error_description = "SSL handshake send failed: " + std::to_string(error);
         return SSL_ERROR_FAILED;
      }
   }

   // For simplicity, return connecting status - full handshake would need more rounds
   return SSL_ERROR_CONNECTING;
}

//********************************************************************************************************************
// Server-side handshake handling using AcceptSecurityContext

SSL_ERROR_CODE ssl_accept_handshake(SSL_HANDLE SSL, const void* ClientData, int DataLength, std::vector<unsigned char>& ResponseData)
{
   if ((!SSL) or (!ClientData) or (!SSL->server_certificate) or (DataLength <= 0)) {
      return SSL_ERROR_ARGS;
   }

   ResponseData.clear();
   
   ssl_debug_log(SSL_DEBUG_TRACE, "SSL Accept Handshake - Processing %d bytes from client", DataLength);

   // Acquire server credentials if not already done
   if (!SSL->credentials_acquired) {
      SCHANNEL_CRED cred_data;
      memset(&cred_data, 0, sizeof(cred_data));
      cred_data.dwVersion = SCHANNEL_CRED_VERSION;
      // For localhost testing, use more permissive flags to ignore certificate validation issues
      cred_data.dwFlags = SCH_CRED_NO_SYSTEM_MAPPER | SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_MANUAL_CRED_VALIDATION | 
                        SCH_CRED_IGNORE_NO_REVOCATION_CHECK | SCH_CRED_IGNORE_REVOCATION_OFFLINE;
      cred_data.cCreds = 1;
      cred_data.paCred = &SSL->server_certificate;

      TimeStamp expiry;
      auto status = AcquireCredentialsHandle(
         nullptr, const_cast<char*>(UNISP_NAME), SECPKG_CRED_INBOUND,  // INBOUND for server
         nullptr, &cred_data, nullptr, nullptr,
         &SSL->credentials, &expiry);

      if (status != SEC_E_OK) {
         set_error_status(SSL, status, "AcquireCredentialsHandle (server)");
         debug_security_status(status, "AcquireCredentialsHandle (server)");
         return SSL_ERROR_FAILED;
      }
      
      ssl_debug_log(SSL_DEBUG_INFO, "SSL Accept Handshake - Server credentials acquired successfully");

      SSL->credentials_acquired = true;
   }

   // Setup input buffer with client data
   SecBuffer in_buffers[2];
   in_buffers[0].pvBuffer   = const_cast<void*>(ClientData);
   in_buffers[0].cbBuffer   = DataLength;
   in_buffers[0].BufferType = SECBUFFER_TOKEN;
   in_buffers[1].pvBuffer   = nullptr;
   in_buffers[1].cbBuffer   = 0;
   in_buffers[1].BufferType = SECBUFFER_EMPTY;

   SecBufferDesc in_buffer_desc;
   in_buffer_desc.cBuffers  = 2;
   in_buffer_desc.pBuffers  = in_buffers;
   in_buffer_desc.ulVersion = SECBUFFER_VERSION;

   // Setup output buffer for response
   SecBuffer out_buffer;
   out_buffer.pvBuffer   = nullptr;
   out_buffer.BufferType = SECBUFFER_TOKEN;
   out_buffer.cbBuffer   = 0;

   SecBufferDesc out_buffer_desc;
   out_buffer_desc.cBuffers  = 1;
   out_buffer_desc.pBuffers  = &out_buffer;
   out_buffer_desc.ulVersion = SECBUFFER_VERSION;

   DWORD context_attr;
   TimeStamp expiry;

   SECURITY_STATUS status;
   if (!SSL->context_initialised) {
      // Initial server-side handshake with permissive flags for localhost testing
      status = AcceptSecurityContext(
         &SSL->credentials,
         nullptr,  // No existing context
         &in_buffer_desc,
         ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY |
         ASC_REQ_EXTENDED_ERROR | ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM |
         ASC_REQ_MUTUAL_AUTH,  // Allow but don't require mutual auth for localhost
         SECURITY_NATIVE_DREP,
         &SSL->context,
         &out_buffer_desc,
         &context_attr,
         &expiry);

      if (status == SEC_E_OK or status == SEC_I_CONTINUE_NEEDED) {
         SSL->context_initialised = true;
      }
   }
   else {
      // Continue handshake with existing context
      status = AcceptSecurityContext(
         &SSL->credentials,
         &SSL->context,  // Use existing context
         &in_buffer_desc,
         ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY |
         ASC_REQ_EXTENDED_ERROR | ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM |
         ASC_REQ_MUTUAL_AUTH,  // Allow but don't require mutual auth for localhost
         SECURITY_NATIVE_DREP,
         &SSL->context,
         &out_buffer_desc,
         &context_attr,
         &expiry);
   }

   debug_security_status(status, "AcceptSecurityContext");
   
   if (status == SEC_E_OK) { // Handshake completed successfully
      ssl_debug_log(SSL_DEBUG_INFO, "SSL Accept Handshake - Server handshake completed successfully");
      debug_ssl_handshake_state(SSL, "ServerHandshakeComplete");
      auto stream_status = QueryContextAttributes(&SSL->context, SECPKG_ATTR_STREAM_SIZES, &SSL->stream_sizes);
      if (stream_status != SEC_E_OK) {
         SSL->last_security_status = stream_status;
         SSL->error_description = "Failed to query SSL stream sizes after server handshake completion";
         return SSL_ERROR_FAILED;
      }
      SSL->error_description = "Server SSL handshake completed successfully";

      // If there's response data, copy it to vector
      if (out_buffer.pvBuffer and out_buffer.cbBuffer > 0) {
         ssl_debug_log(SSL_DEBUG_TRACE, "SSL Accept Handshake - Sending final %d bytes to client", out_buffer.cbBuffer);
         ResponseData.resize(out_buffer.cbBuffer);
         memcpy(ResponseData.data(), out_buffer.pvBuffer, out_buffer.cbBuffer);
         FreeContextBuffer(out_buffer.pvBuffer);
      }
      else ssl_debug_log(SSL_DEBUG_INFO, "SSL Accept Handshake - No final response data to send");

      return SSL_OK;
   }
   else if (status == SEC_I_CONTINUE_NEEDED) {
      // More handshake data needed
      ssl_debug_log(SSL_DEBUG_TRACE, "SSL Accept Handshake - Continue needed, more exchanges required");
      SSL->error_description = "Server SSL handshake needs more data";

      // Copy response data to send back to client
      if (out_buffer.pvBuffer and out_buffer.cbBuffer > 0) {
         ssl_debug_log(SSL_DEBUG_TRACE, "SSL Accept Handshake - Sending %d bytes response to client", out_buffer.cbBuffer);
         ResponseData.resize(out_buffer.cbBuffer);
         memcpy(ResponseData.data(), out_buffer.pvBuffer, out_buffer.cbBuffer);
         FreeContextBuffer(out_buffer.pvBuffer);
      }
      else ssl_debug_log(SSL_DEBUG_WARNING, "SSL Accept Handshake - Continue needed but no response data generated");

      return SSL_ERROR_CONNECTING;
   }
   else {
      // Handshake failed
      set_error_status(SSL, status, "AcceptSecurityContext");
      if (out_buffer.pvBuffer) FreeContextBuffer(out_buffer.pvBuffer);
      return SSL_ERROR_FAILED;
   }
}
