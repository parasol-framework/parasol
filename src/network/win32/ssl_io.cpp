
//********************************************************************************************************************
// Read data from SSL connection (post-handshake).  Designed for non-blocking sockets only.

SSL_ERROR_CODE ssl_read(SSL_HANDLE SSL, void *Buffer, int BufferSize, int* BytesRead)
{
   if ((!SSL) or (!Buffer) or (BufferSize <= 0) or (!BytesRead)) {
      if (BytesRead) *BytesRead = 0;
      return SSL_ERROR_ARGS;
   }

   *BytesRead = 0;
   if (!SSL->context_initialised) return SSL_ERROR_FAILED;

   // First, check if we have leftover decrypted data from previous calls
   if (SSL->decrypted_buffer.size() > SSL->decrypted_buffer_offset) {
      size_t available = SSL->decrypted_buffer.size() - SSL->decrypted_buffer_offset;
      size_t to_copy = std::min(size_t(BufferSize), available);

      memcpy(Buffer, SSL->decrypted_buffer.data() + SSL->decrypted_buffer_offset, to_copy);
      SSL->decrypted_buffer_offset += to_copy;

      // If we've consumed all leftover data, reset the buffer
      if (SSL->decrypted_buffer_offset >= SSL->decrypted_buffer.size()) {
         SSL->decrypted_buffer.reset();
         SSL->decrypted_buffer_offset = 0;
      }

      *BytesRead = int(to_copy);
      return SSL_OK;
   }

   while (true) {
      // Try to decrypt any data we already have in the receive buffer
      if (!SSL->recv_buffer.empty()) {
         // Validate SSL context before attempting decryption
         if (!SSL->context_initialised) {
            ssl_debug_log(SSL_DEBUG_ERROR, "SSL read attempted with uninitialized context");
            return SSL_ERROR_FAILED;
         }

         // Validate stream sizes are set (indicates successful handshake completion)
         if (SSL->stream_sizes.cbMaximumMessage == 0) {
            ssl_debug_log(SSL_DEBUG_ERROR, "SSL read attempted before stream sizes were initialized - handshake may not be complete");
            return SSL_ERROR_FAILED;
         }

         std::array<SecBuffer, 4> buffers;
         auto recv_data = SSL->recv_buffer.used_data_mutable();
         buffers[0].pvBuffer = recv_data.data();
         buffers[0].cbBuffer = (uint32_t)recv_data.size();
         buffers[0].BufferType = SECBUFFER_DATA;

         buffers[1].BufferType = SECBUFFER_EMPTY;
         buffers[2].BufferType = SECBUFFER_EMPTY;
         buffers[3].BufferType = SECBUFFER_EMPTY;

         SecBufferDesc bufferDesc;
         bufferDesc.ulVersion = SECBUFFER_VERSION;
         bufferDesc.cBuffers = (uint32_t)buffers.size();
         bufferDesc.pBuffers = buffers.data();

         ssl_debug_log(SSL_DEBUG_TRACE, "SSL read attempting DecryptMessage with %zu bytes", recv_data.size());
         SECURITY_STATUS status = DecryptMessage(&SSL->context, &bufferDesc, 0, nullptr);
         ssl_debug_log(SSL_DEBUG_TRACE, "SSL DecryptMessage returned status: 0x%08X", status);

         if (status == SEC_E_OK) {
            // Successfully decrypted data - reset retry counter
            int extra_bytes = 0;
            unsigned char* decrypted_data = nullptr;
            size_t decrypted_size = 0;

            // Find decrypted data and extra buffers
            for (const auto& buf : buffers) {
               if (buf.BufferType == SECBUFFER_DATA and buf.pvBuffer) {
                  decrypted_data = (unsigned char*)buf.pvBuffer;
                  decrypted_size = buf.cbBuffer;
               }
               else if (buf.BufferType == SECBUFFER_EXTRA and buf.cbBuffer > 0) {
                  // Save any extra encrypted data for next read
                  extra_bytes = int(buf.cbBuffer);
               }
            }

            // Handle the decrypted data efficiently without temporary copies
            if (decrypted_data and decrypted_size > 0) {
               if (decrypted_size <= size_t(BufferSize)) {
                  // All decrypted data fits in user buffer - copy directly
                  memcpy(Buffer, decrypted_data, decrypted_size);

                  // Handle leftover encrypted data after copying decrypted data
                  if (extra_bytes > 0) { // Move extra data to beginning of buffer
                     SSL->recv_buffer.compact(SSL->recv_buffer.size() - extra_bytes);
                  }
                  else SSL->recv_buffer.reset();

                  *BytesRead = int(decrypted_size);
                  return SSL_OK;
               }
               else {
                  // Decrypted data is larger than user buffer
                  memcpy(Buffer, decrypted_data, BufferSize);

                  // Store remaining data in decrypted buffer - ensure sufficient capacity
                  size_t remaining = decrypted_size - BufferSize;
                  SSL->decrypted_buffer.ensure_capacity(remaining);
                  SSL->decrypted_buffer.reset();
                  std::span<const unsigned char> remaining_data(decrypted_data + BufferSize, remaining);
                  SSL->decrypted_buffer.append(remaining_data);
                  SSL->decrypted_buffer_offset = 0;

                  // Handle leftover encrypted data
                  if (extra_bytes > 0) {
                     SSL->recv_buffer.compact(SSL->recv_buffer.size() - extra_bytes);
                  }
                  else SSL->recv_buffer.reset();

                  *BytesRead = BufferSize;
                  return SSL_OK;
               }
            }
            else { // No decrypted data but successful status - handle extra bytes
               if (extra_bytes > 0) SSL->recv_buffer.compact(SSL->recv_buffer.size() - extra_bytes);
               else SSL->recv_buffer.reset();
            }
         }
         else if (status == SEC_E_INCOMPLETE_MESSAGE) {
            // Need more encrypted data to complete the SSL record
            // Don't attempt decryption again until we have more data
            ssl_debug_log(SSL_DEBUG_TRACE, "SSL read incomplete message - need more encrypted data (current buffer: %zu bytes)", SSL->recv_buffer.size());
            // Fall through to receive more data, but don't loop back to decryption immediately
         }
         else { // Decryption failed
            SSL->last_security_status = status;
            if (status == SEC_E_DECRYPT_FAILURE) { // Connection likely closed
              return SSL_ERROR_DISCONNECTED;
            }
            else if (status == 0x90321) { // Wrong credential handle - context corruption
               ssl_debug_log(SSL_DEBUG_ERROR, "SSL read wrong credential handle - SSL context corrupted");
               SSL->recv_buffer.reset();
               set_error_status(SSL, status, "DecryptMessage (wrong credential handle)");
               return SSL_ERROR_FAILED;
            }
            else if (status == SEC_E_INVALID_TOKEN) {
               // This can happen if:
               // You fed in garbage or truncated bytes (common if you didn't wait for a full record and passed partial data).
               // The peer sent malformed TLS (e.g., not really TLS, wrong port).
               // The state machine is out of sync (e.g., you called ISC/ASC at the wrong time).
               // There's a protocol/cipher mismatch that manifests as the peer aborting the handshake.

               SSL->recv_buffer.reset();
               set_error_status(SSL, status, "DecryptMessage (invalid token)");
               return SSL_ERROR_DISCONNECTED;
            }
            set_error_status(SSL, status, "DecryptMessage");
            return SSL_ERROR_FAILED;
         }
      } // (!SSL->recv_buffer.empty())

      // Try to receive more encrypted data (socket is always non-blocking)
      size_t space_available = SSL->recv_buffer.available();
      if (space_available == 0) {
         // Buffer full but no complete message - expand buffer if possible
         if (SSL->recv_buffer.capacity() < SSL_IO_BUFFER_SIZE) {
            SSL->recv_buffer.resize(std::min(SSL->recv_buffer.capacity() * 2, SSL_IO_BUFFER_SIZE));
            space_available = SSL->recv_buffer.available();
         }
         else {
            SSL->error_description = "SSL receive buffer overflow";
            return SSL_ERROR_FAILED;
         }
      }

      auto available_space = SSL->recv_buffer.available_space();
      auto received = recv(SSL->socket_handle, (char*)available_space.data(), int(space_available), 0);

      if (received == SOCKET_ERROR) {
         auto error = WSAGetLastError();
         if (error == WSAEWOULDBLOCK) {
            // No more data available on non-blocking socket
            return SSL_ERROR_WOULD_BLOCK;
         }

         return SSL->process_recv_error(received, "ssl_read");
      }
      else if (received == 0) { // Connection closed gracefully
         return SSL_ERROR_DISCONNECTED;
      }
      else { // Successfully received encrypted data
         SSL->recv_buffer.advance_used(received);
         ssl_debug_log(SSL_DEBUG_TRACE, "SSL read received %d bytes, total buffered: %zu", received, SSL->recv_buffer.size());
      }
   } // while(true)

   // Should not reach here in normal operation
   return SSL_ERROR_FAILED;
}

//********************************************************************************************************************
// Write data to SSL connection

SSL_ERROR_CODE ssl_write(SSL_HANDLE SSL, const void* Buffer, size_t BufferSize, size_t* bytes_sent)
{
   if ((!SSL) or (!Buffer) or (BufferSize == 0) or (!bytes_sent)) return SSL_ERROR_ARGS;

   *bytes_sent = 0;
   if (!SSL->context_initialised) return SSL_ERROR_FAILED;

   // Calculate required buffer sizes based on stream sizes
   size_t header_size = SSL->stream_sizes.cbHeader;
   size_t trailer_size = SSL->stream_sizes.cbTrailer;
   size_t max_message_size = SSL->stream_sizes.cbMaximumMessage;

   // Limit the data size to what SSL can handle in one record
   size_t data_to_send = std::min(BufferSize, max_message_size);
   size_t total_size = header_size + data_to_send + trailer_size;

   // Ensure our send buffer is large enough - use capacity check for efficiency
   SSL->send_buffer.ensure_capacity(total_size);
   SSL->send_buffer.resize(total_size);

   // Set up SecBuffer structures for encryption
   std::array<SecBuffer, 4> buffers;

   // Header buffer
   buffers[0].pvBuffer = SSL->send_buffer.data();
   buffers[0].cbBuffer = (uint32_t)header_size;
   buffers[0].BufferType = SECBUFFER_STREAM_HEADER;

   // Data buffer - copy user data after header
   buffers[1].pvBuffer = SSL->send_buffer.data() + header_size;
   buffers[1].cbBuffer = (uint32_t)data_to_send;
   buffers[1].BufferType = SECBUFFER_DATA;
   memcpy(buffers[1].pvBuffer, Buffer, data_to_send);

   // Trailer buffer
   buffers[2].pvBuffer = SSL->send_buffer.data() + header_size + data_to_send;
   buffers[2].cbBuffer = (uint32_t)trailer_size;
   buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;

   // Empty buffer
   buffers[3].pvBuffer = nullptr;
   buffers[3].cbBuffer = 0;
   buffers[3].BufferType = SECBUFFER_EMPTY;

   SecBufferDesc bufferDesc;
   bufferDesc.ulVersion = SECBUFFER_VERSION;
   bufferDesc.cBuffers = (uint32_t)buffers.size();
   bufferDesc.pBuffers = buffers.data();

   auto status = EncryptMessage(&SSL->context, 0, &bufferDesc, 0);

   if (status != SEC_E_OK) {
      set_error_status(SSL, status, "EncryptMessage");
      return SSL_ERROR_FAILED;
   }

   DWORD encrypted_size = buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer;

   // Send the encrypted data
   ssl_debug_log(SSL_DEBUG_INFO, "SSL Write - sending %lu encrypted bytes to socket %d", encrypted_size, (int)SSL->socket_handle);
   auto sent = send(SSL->socket_handle, (const char*)SSL->send_buffer.data(), encrypted_size, 0);

   if (sent == SOCKET_ERROR) {
      auto error = WSAGetLastError();
      SSL->last_win32_error = error;
      if (error == WSAEWOULDBLOCK) {
         SSL->error_description = "SSL write would block (WSAEWOULDBLOCK)";
         return SSL_ERROR_WOULD_BLOCK;
      }
      else {
         SSL->error_description = "SSL write failed: " + std::to_string(error);
         return SSL_ERROR_FAILED;
      }
   }
   else if (sent != int(encrypted_size)) {
      // Partial send - this is problematic for SSL records
      SSL->error_description = "SSL partial write - SSL record boundary violated";
      return SSL_ERROR_FAILED;
   }

   *bytes_sent = data_to_send;
   return SSL_OK;
}
