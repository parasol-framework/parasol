
//********************************************************************************************************************
// Continue SSL handshake with server response.  Ref: sslHandshakeReceived()

SSL_ERROR_CODE ssl_continue_handshake(SSL_HANDLE SSL, const void *ServerData, int DataLength, int &ConsumedOut)
{
   if ((!SSL) or (!ServerData) or (DataLength <= 0)) return SSL_ERROR_ARGS;
   ConsumedOut = 0;
   
   if (!SSL->context_initialised) return SSL_ERROR_FAILED;
   

   // Append new handshake data to receive buffer to handle fragmentation
   std::span<const unsigned char> server_data_span(static_cast<const unsigned char*>(ServerData), DataLength);
   if (!SSL->recv_buffer.append(server_data_span)) {
      SSL->error_description = "SSL handshake data exceeds maximum buffer size";
      return SSL_ERROR_FAILED;
   }

   while (!SSL->recv_buffer.empty()) {
      // Setup input buffers using accumulated handshake data
      std::array<SecBuffer, 2> in_buffers;
      auto recv_data = SSL->recv_buffer.used_data();
      in_buffers[0].pvBuffer = const_cast<unsigned char*>(recv_data.data());
      in_buffers[0].cbBuffer = (uint32_t)recv_data.size();
      in_buffers[0].BufferType = SECBUFFER_TOKEN;

      in_buffers[1].pvBuffer = nullptr;
      in_buffers[1].cbBuffer = 0;
      in_buffers[1].BufferType = SECBUFFER_EMPTY;

      SecBufferDesc in_buffer_desc;
      in_buffer_desc.cBuffers = (uint32_t)in_buffers.size();
      in_buffer_desc.pBuffers = in_buffers.data();
      in_buffer_desc.ulVersion = SECBUFFER_VERSION;

      // Output buffer for next handshake message
      SecBuffer out_buffer;
      out_buffer.pvBuffer = nullptr;
      out_buffer.BufferType = SECBUFFER_TOKEN;
      out_buffer.cbBuffer = 0;

      SecBufferDesc out_buffer_desc;
      out_buffer_desc.cBuffers = 1;
      out_buffer_desc.pBuffers = &out_buffer;
      out_buffer_desc.ulVersion = SECBUFFER_VERSION;

      DWORD flags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
                    ISC_REQ_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;

      TimeStamp expiry;
      DWORD out_flags;

      auto status = InitializeSecurityContext(
         &SSL->credentials, &SSL->context, const_cast<char*>(SSL->hostname.c_str()), flags,
         0, SECURITY_NATIVE_DREP, &in_buffer_desc, 0,
         &SSL->context, &out_buffer_desc, &out_flags, &expiry);
      
      // Check if we consumed some of the input data and handle extra data
      size_t bytes_consumed = SSL->recv_buffer.size();
      
      // Check for extra data in input buffer
      for (const auto& buf : in_buffers) {
         if (buf.BufferType == SECBUFFER_EXTRA and buf.cbBuffer > 0) {
            if (buf.cbBuffer <= SSL->recv_buffer.size()) {
               bytes_consumed = SSL->recv_buffer.size() - buf.cbBuffer;
               SSL->recv_buffer.compact(bytes_consumed);
            }
            else {
               bytes_consumed = SSL->recv_buffer.size();
               SSL->recv_buffer.reset();
            }
            break;
         }
      }

      // Handle different handshake states
      if (status == SEC_E_OK) {

         if (out_buffer.cbBuffer > 0 and out_buffer.pvBuffer != nullptr) {
            int sent = send(SSL->socket_handle, (char*)out_buffer.pvBuffer, out_buffer.cbBuffer, 0);
            FreeContextBuffer(out_buffer.pvBuffer);

            if (sent == SOCKET_ERROR) {
               int error = WSAGetLastError();
               SSL->last_win32_error = error;
               SSL->error_description = "SSL handshake final send failed, WSA error: " + std::to_string(error);
               return SSL_ERROR_FAILED;
            }
         }

         auto stream_status = QueryContextAttributes(&SSL->context, SECPKG_ATTR_STREAM_SIZES, &SSL->stream_sizes);
         if (stream_status != SEC_E_OK) {
            SSL->last_security_status = stream_status;
            SSL->error_description = "Failed to query SSL stream sizes after handshake completion";
            return SSL_ERROR_FAILED;
         }

         bool found_valid_extra = false;
         for (const auto& buf : in_buffers) {
            if (buf.BufferType == SECBUFFER_EXTRA and buf.cbBuffer > 0 and buf.cbBuffer < SSL->recv_buffer.size() + bytes_consumed) {
               found_valid_extra = true;
               break;
            }
         }
         
         if (!SSL->recv_buffer.empty()) {
            if (!found_valid_extra) {
               SSL->recv_buffer.reset();
            }
         }

         cache_connection_info(SSL);
         
         SSL->error_description = "SSL handshake completed successfully";
         ConsumedOut = int(bytes_consumed);
         return SSL_OK;
      }
      else if (status == SEC_I_CONTINUE_NEEDED) {
         if ((out_buffer.cbBuffer > 0) and (out_buffer.pvBuffer != nullptr)) {
            int sent = send(SSL->socket_handle, (char*)out_buffer.pvBuffer, out_buffer.cbBuffer, 0);
            FreeContextBuffer(out_buffer.pvBuffer);

            if (sent == SOCKET_ERROR) {
               int error = WSAGetLastError();
               SSL->last_win32_error = error;
               if (error == WSAEWOULDBLOCK) {
                  SSL->error_description = "SSL handshake continue send would block (WSAEWOULDBLOCK)";
                  return SSL_ERROR_WOULD_BLOCK;
               }
               SSL->error_description = "SSL handshake continue send failed; WSA error: " + std::to_string(error);
               return SSL_ERROR_FAILED;
            }
         }

         ConsumedOut = int(bytes_consumed); 
         if (bytes_consumed < SSL->recv_buffer.size()) {
            SSL->recv_buffer.compact(bytes_consumed);
         }
         else {
            SSL->recv_buffer.reset();
            return SSL_NEED_DATA;
         }
      }
      else if (status == SEC_E_INCOMPLETE_MESSAGE) {
         SSL->error_description = "SSL handshake incomplete message - waiting for more data";
         ConsumedOut = int(bytes_consumed);
         return SSL_NEED_DATA;
      }
      else {
         set_error_status(SSL, status, "InitializeSecurityContext (continue)");
         SSL->recv_buffer.reset();
         return SSL_ERROR_FAILED;
      }
   }

   return SSL_NEED_DATA;
}
