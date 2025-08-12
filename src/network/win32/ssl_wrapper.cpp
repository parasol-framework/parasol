/*********************************************************************************************************************

Windows SSL Wrapper Implementation
Pure Windows implementation that avoids all Parasol headers to prevent conflicts.

*********************************************************************************************************************/

#ifdef _WIN32

// Define required constants to avoid header conflicts
#define WIN32_LEAN_AND_MEAN
#define SECURITY_WIN32
#define NOMINMAX

#include <winsock2.h>
#include <windows.h>
#include <schannel.h>
#include <sspi.h>
#include <security.h>
#include <wincrypt.h>
#include <cstring>
#include <vector>
#include <string>
#include <sstream>
#include <array>
#include <algorithm>

#include "ssl_wrapper.h"

// Buffer size for SSL operations - optimized for SSL record sizes
constexpr size_t SSL_IO_BUFFER_SIZE = 0x8000;  // 32KB - 2x max SSL record size (16KB)
constexpr size_t SSL_INITIAL_BUFFER_SIZE = 0x4000;  // 16KB initial size
constexpr size_t SSL_MAX_RECORD_SIZE = 0x4000;  // 16KB max SSL record size

// Internal SSL context structure
struct ssl_context {
   CredHandle credentials;
   CtxtHandle context;
   SecPkgContext_StreamSizes stream_sizes;
   SOCKET socket_handle;
   std::vector<unsigned char> io_buffer;
   std::vector<unsigned char> recv_buffer;     // Persistent buffer for incomplete SSL messages
   size_t recv_buffer_used;                    // Bytes currently in recv_buffer
   std::vector<unsigned char> send_buffer;     // Buffer for SSL encryption
   std::vector<unsigned char> decrypted_buffer; // Buffer for leftover decrypted data
   size_t decrypted_buffer_used;               // Bytes of decrypted data available
   size_t decrypted_buffer_offset;             // Bytes already returned to user
   bool error_description_dirty;               // True if error description needs regeneration
   SSL_ERROR_CODE last_error;
   SECURITY_STATUS last_security_status;
   DWORD last_win32_error;
   std::string error_description;
   std::string hostname;
   bool validate_credentials;
   bool credentials_acquired;
   bool context_initialised;
   bool is_server_mode;                        // True for server-side SSL, false for client-side
   PCCERT_CONTEXT server_certificate;          // Server certificate for server-side SSL

   ssl_context() {
      socket_handle            = INVALID_SOCKET;
      last_error               = SSL_OK;
      last_security_status     = SEC_E_OK;
      last_win32_error         = 0;
      error_description        = "No error";
      validate_credentials     = true;
      credentials_acquired     = false;
      context_initialised      = false;
      recv_buffer_used         = 0;
      decrypted_buffer_used    = 0;
      decrypted_buffer_offset  = 0;
      error_description_dirty  = false;
      is_server_mode           = false;
      server_certificate       = nullptr;
      // Pre-allocate buffers with optimized sizes
      io_buffer.reserve(SSL_IO_BUFFER_SIZE);
      recv_buffer.reserve(SSL_IO_BUFFER_SIZE);
      send_buffer.reserve(SSL_IO_BUFFER_SIZE);
      decrypted_buffer.reserve(SSL_MAX_RECORD_SIZE);
      io_buffer.resize(SSL_INITIAL_BUFFER_SIZE);
      recv_buffer.resize(SSL_INITIAL_BUFFER_SIZE);
      send_buffer.resize(SSL_INITIAL_BUFFER_SIZE);
   }

   ~ssl_context() {
      if (context_initialised) DeleteSecurityContext(&context);
      if (credentials_acquired) FreeCredentialsHandle(&credentials);
   }

   SSL_ERROR_CODE process_recv_error(int Result, std::string Process)
   {
      if (!Result) {
         last_error = SSL_ERROR_DISCONNECTED;
         error_description = "Connection closed by server during " + Process;
      }
      else if (Result == -1) {
         last_win32_error = WSAGetLastError();
         if (last_win32_error == WSAEWOULDBLOCK) {
            last_error = SSL_ERROR_WOULD_BLOCK;
            error_description = "Socket would block during " + Process + " and is in non-blocking mode.";
         }
         else {
            last_error = SSL_ERROR_FAILED;
            error_description = "Failed to receive response during " + Process + ": " + std::to_string(last_win32_error);
         }
      }
      else last_error = SSL_OK;

      return last_error;
   }
};

//********************************************************************************************************************

static bool glSSLInitialised = false;
//static HCERTSTORE g_cert_store = nullptr;

static PCCERT_CONTEXT create_self_signed_certificate();

//********************************************************************************************************************
// Helper function to convert SECURITY_STATUS to description

static const char* get_status_description(SECURITY_STATUS status)
{
   switch (status) {
      case SEC_E_OK: return "Success";
      case SEC_E_INSUFFICIENT_MEMORY: return "Insufficient memory";
      case SEC_E_INVALID_HANDLE: return "Invalid handle";
      case SEC_E_UNSUPPORTED_FUNCTION: return "Unsupported function";
      case SEC_E_TARGET_UNKNOWN: return "Target unknown";
      case SEC_E_INTERNAL_ERROR: return "Internal error";
      case SEC_E_SECPKG_NOT_FOUND: return "Security package not found";
      case SEC_E_NOT_OWNER: return "Not owner";
      case SEC_E_CANNOT_INSTALL: return "Cannot install";
      case SEC_E_INVALID_TOKEN: return "Invalid token";
      case SEC_E_CANNOT_PACK: return "Cannot pack";
      case SEC_E_QOP_NOT_SUPPORTED: return "QOP not supported";
      case SEC_E_NO_IMPERSONATION: return "No impersonation";
      case SEC_E_LOGON_DENIED: return "Logon denied";
      case SEC_E_UNKNOWN_CREDENTIALS: return "Unknown credentials";
      case SEC_E_NO_CREDENTIALS: return "No credentials";
      case SEC_E_MESSAGE_ALTERED: return "Message altered";
      case SEC_E_OUT_OF_SEQUENCE: return "Out of sequence";
      case SEC_E_NO_AUTHENTICATING_AUTHORITY: return "No authenticating authority";
      case SEC_E_INCOMPLETE_MESSAGE: return "Incomplete message";
      case SEC_E_INCOMPLETE_CREDENTIALS: return "Incomplete credentials";
      case SEC_E_BUFFER_TOO_SMALL: return "Buffer too small";
      case SEC_E_WRONG_PRINCIPAL: return "Wrong principal";
      case SEC_E_TIME_SKEW: return "Time skew";
      case SEC_E_UNTRUSTED_ROOT: return "Untrusted root certificate";
      case SEC_E_ILLEGAL_MESSAGE: return "Illegal message";
      case SEC_E_CERT_UNKNOWN: return "Certificate unknown";
      case SEC_E_CERT_EXPIRED: return "Certificate expired";
      case SEC_E_ENCRYPT_FAILURE: return "Encrypt failure";
      case SEC_E_DECRYPT_FAILURE: return "Decrypt failure";
      case SEC_E_ALGORITHM_MISMATCH: return "Algorithm mismatch";
      case SEC_E_SECURITY_QOS_FAILED: return "Security QOS failed";
      case SEC_E_UNFINISHED_CONTEXT_DELETED: return "Unfinished context deleted";
      case SEC_E_INVALID_PARAMETER: return "Invalid parameter";
      case SEC_I_CONTINUE_NEEDED: return "Continue needed";
      case SEC_I_COMPLETE_NEEDED: return "Complete needed";
      case SEC_I_COMPLETE_AND_CONTINUE: return "Complete and continue";
      case SEC_I_LOCAL_LOGON: return "Local logon";
      default: return "Unknown status";
   }
}

// Set error status - lazy description generation

static void set_error_status(ssl_context* Ctx, SECURITY_STATUS Status, const char* Operation)
{
   Ctx->last_security_status = Status;
   Ctx->last_win32_error = GetLastError();
   Ctx->error_description_dirty = true;
   // Store operation for later description generation
   Ctx->error_description = Operation; // Temporary storage
}

// Generate error description only when requested

static void generate_error_description(ssl_context* Ctx)
{
   if (!Ctx->error_description_dirty) return;

   const char* operation = Ctx->error_description.c_str(); // Stored operation
   const char* status_desc = get_status_description(Ctx->last_security_status);

   std::stringstream stream;
   stream << operation << ":" << status_desc << "(Status: " << (uint32_t)Ctx->last_security_status << ", Win32: "
          << Ctx->last_win32_error << ")";
   Ctx->error_description = stream.str();
   Ctx->error_description_dirty = false;
}

//********************************************************************************************************************

void ssl_wrapper_cleanup(void)
{
   if (!glSSLInitialised) return;

   //if (g_cert_store) { CertCloseStore(g_cert_store, 0); g_cert_store = nullptr; }

   glSSLInitialised = false;
}

//********************************************************************************************************************
// Create SSL context

SSL_HANDLE ssl_wrapper_create_context(bool ValidateCredentials, bool ServerMode)
{
   if (!glSSLInitialised) {
      // The certificate store would be needed if you want to:
      // 1. Client Certificate Authentication - Select client certificates for mutual TLS
      // 2. Custom Certificate Validation - Manual certificate chain validation
      // 3. Certificate Enumeration - Browse available certificates
      // 4. Certificate Installation - Add new certificates programmatically

      //g_cert_store = CertOpenSystemStore(0, "MY");
      //if (!g_cert_store) return ERR::Failed;

      glSSLInitialised = true;
   }

   ssl_context *ctx = new (std::nothrow) ssl_context;
   if (ctx) ctx->validate_credentials = ValidateCredentials;

   if (ServerMode) {
      ctx->is_server_mode = true;
      ctx->validate_credentials = ValidateCredentials;

      // Try to get a server certificate for localhost testing
      ctx->server_certificate = create_self_signed_certificate();
   }

   return ctx;
}

//********************************************************************************************************************
// Free SSL context

void ssl_wrapper_free_context(SSL_HANDLE SSL)
{
   if (!SSL) return;

   // Clean up server certificate if it exists
   if (SSL->server_certificate) {
      CertFreeCertificateContext(SSL->server_certificate);
   }

   delete SSL;
}

//********************************************************************************************************************
// Perform SSL connect and handshake

SSL_ERROR_CODE ssl_wrapper_connect(SSL_HANDLE SSL, void *SocketHandle, const std::string &HostName)
{
   if ((!SSL) or ((SOCKET)SocketHandle == INVALID_SOCKET)) return SSL_ERROR_ARGS;

   SSL->socket_handle = (SOCKET)SocketHandle;
   SSL->hostname = HostName;

   if (SSL->context_initialised) return SSL_ERROR_CONNECTING; // Already in handshake process

   // Acquire credentials
   SCHANNEL_CRED cred_data{};
   cred_data.dwVersion = SCHANNEL_CRED_VERSION;

   if (SSL->validate_credentials) {
      cred_data.dwFlags = SCH_CRED_AUTO_CRED_VALIDATION;
   }
   else cred_data.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION;

   cred_data.grbitEnabledProtocols = 0; // Use system defaults

   TimeStamp expiry;
   auto status = AcquireCredentialsHandle(
      nullptr, const_cast<char*>(UNISP_NAME), SECPKG_CRED_OUTBOUND,
      nullptr, &cred_data, nullptr, nullptr,
      &SSL->credentials, &expiry);

   if (status != SEC_E_OK) {
      set_error_status(SSL, status, "AcquireCredentialsHandle");
      SSL->last_error = SSL_ERROR_FAILED;
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
      SSL->last_error = SSL_ERROR_FAILED;
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
            SSL->last_error = SSL_ERROR_WOULD_BLOCK;
            return SSL_ERROR_WOULD_BLOCK;
         }
         SSL->error_description = "SSL handshake send failed: %d" + std::to_string(error);
         SSL->last_error = SSL_ERROR_FAILED;
         return SSL_ERROR_FAILED;
      }
   }

   // For simplicity, return connecting status - full handshake would need more rounds
   SSL->last_error = SSL_ERROR_CONNECTING;
   return SSL_ERROR_CONNECTING;
}

//********************************************************************************************************************
// Continue SSL handshake with server response.  Ref: sslHandshakeReceived()

SSL_ERROR_CODE ssl_wrapper_continue_handshake(SSL_HANDLE SSL, const void *ServerData, int DataLength)
{
   if ((!SSL) or (!ServerData) or (DataLength <= 0)) return SSL_ERROR_ARGS;

   if (!SSL->context_initialised) return SSL_ERROR_FAILED;

   // Append new handshake data to receive buffer to handle fragmentation
   size_t total_needed = SSL->recv_buffer_used + DataLength;
   if (SSL->recv_buffer.size() < total_needed) {
      if (total_needed > SSL_IO_BUFFER_SIZE) {
         SSL->error_description = "SSL handshake data exceeds maximum buffer size";
         SSL->last_error = SSL_ERROR_FAILED;
         return SSL_ERROR_FAILED;
      }
      SSL->recv_buffer.resize(total_needed);
   }

   // Append new data to existing buffer
   memcpy(SSL->recv_buffer.data() + SSL->recv_buffer_used, ServerData, DataLength);
   SSL->recv_buffer_used += DataLength;

   while (SSL->recv_buffer_used > 0) {
      // Setup input buffers using accumulated handshake data
      std::array<SecBuffer, 2> in_buffers;
      in_buffers[0].pvBuffer = SSL->recv_buffer.data();
      in_buffers[0].cbBuffer = (ULONG)SSL->recv_buffer_used;
      in_buffers[0].BufferType = SECBUFFER_TOKEN;

      in_buffers[1].pvBuffer = nullptr;
      in_buffers[1].cbBuffer = 0;
      in_buffers[1].BufferType = SECBUFFER_EMPTY;

      SecBufferDesc in_buffer_desc;
      in_buffer_desc.cBuffers = (ULONG)in_buffers.size();
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
      size_t bytes_consumed = SSL->recv_buffer_used;
      
      // Check for extra data in input buffer
      for (const auto& buf : in_buffers) {
         if (buf.BufferType == SECBUFFER_EXTRA and buf.cbBuffer > 0) {
            // There's extra data beyond what was consumed
            bytes_consumed = SSL->recv_buffer_used - buf.cbBuffer;
            break;
         }
      }

      // Handle different handshake states
      if (status == SEC_E_OK) {
         // Handshake completed successfully

         // Send any final handshake data if present
         if (out_buffer.cbBuffer > 0 and out_buffer.pvBuffer != nullptr) {
            int sent = send(SSL->socket_handle, (char*)out_buffer.pvBuffer, out_buffer.cbBuffer, 0);
            FreeContextBuffer(out_buffer.pvBuffer);

            if (sent == SOCKET_ERROR) {
               int error = WSAGetLastError();
               SSL->last_win32_error = error;
               SSL->error_description = "SSL handshake final send failed, WSA error: " + std::to_string(error);
               SSL->last_error = SSL_ERROR_FAILED;
               return SSL_ERROR_FAILED;
            }
         }

         // Get stream sizes for future read/write operations
         QueryContextAttributes(&SSL->context, SECPKG_ATTR_STREAM_SIZES, &SSL->stream_sizes);

         // Handle any leftover data in buffer
         if (bytes_consumed < SSL->recv_buffer_used) {
            size_t remaining = SSL->recv_buffer_used - bytes_consumed;
            memmove(SSL->recv_buffer.data(), SSL->recv_buffer.data() + bytes_consumed, remaining);
            SSL->recv_buffer_used = remaining;
         }
         else SSL->recv_buffer_used = 0;

         SSL->error_description = "SSL handshake completed successfully";
         SSL->last_error = SSL_OK;
         return SSL_OK;
      }
      else if (status == SEC_I_CONTINUE_NEEDED) {
         // More handshake data needed - consume input and send response

         // Send handshake response if present
         if ((out_buffer.cbBuffer > 0) and (out_buffer.pvBuffer != nullptr)) {
            int sent = send(SSL->socket_handle, (char*)out_buffer.pvBuffer, out_buffer.cbBuffer, 0);
            FreeContextBuffer(out_buffer.pvBuffer);

            if (sent == SOCKET_ERROR) {
               int error = WSAGetLastError();
               SSL->last_win32_error = error;
               if (error == WSAEWOULDBLOCK) {
                  SSL->error_description = "SSL handshake continue send would block (WSAEWOULDBLOCK)";
                  SSL->last_error = SSL_ERROR_WOULD_BLOCK;
                  return SSL_ERROR_WOULD_BLOCK;
               }
               SSL->error_description = "SSL handshake continue send failed; WSA error: " + std::to_string(error);
               SSL->last_error = SSL_ERROR_FAILED;
               return SSL_ERROR_FAILED;
            }
         }

         // Handle consumed data
         if (bytes_consumed < SSL->recv_buffer_used) {
            size_t remaining = SSL->recv_buffer_used - bytes_consumed;
            memmove(SSL->recv_buffer.data(), SSL->recv_buffer.data() + bytes_consumed, remaining);
            SSL->recv_buffer_used = remaining;
         }
         else {
            SSL->recv_buffer_used = 0;
            // Need more handshake data from server
            SSL->last_error = SSL_ERROR_CONNECTING;
            return SSL_ERROR_CONNECTING;
         }
      }
      else if (status == SEC_E_INCOMPLETE_MESSAGE) {
         // Need more handshake data to complete the message
         SSL->last_error = SSL_ERROR_CONNECTING;
         SSL->error_description = "SSL handshake incomplete message - waiting for more data";
         return SSL_ERROR_CONNECTING;
      }
      else { // Handshake failed
         set_error_status(SSL, status, "InitializeSecurityContext (continue)");
         SSL->last_error = SSL_ERROR_FAILED;
         SSL->recv_buffer_used = 0; // Clear buffer on failure
         return SSL_ERROR_FAILED;
      }
   }

   // Should not reach here, but return connecting state as safe fallback
   SSL->last_error = SSL_ERROR_CONNECTING;
   return SSL_ERROR_CONNECTING;
}

//********************************************************************************************************************
// Read data from SSL connection (post-handshake).  Designed for non-blocking sockets only.

int ssl_wrapper_read(SSL_HANDLE SSL, void *Buffer, int BufferSize)
{
   if ((!SSL) or (!Buffer) or (BufferSize <= 0)) return -1;

   if (!SSL->context_initialised) return -1;

   // First, check if we have leftover decrypted data from previous calls
   if (SSL->decrypted_buffer_used > SSL->decrypted_buffer_offset) {
      size_t available = SSL->decrypted_buffer_used - SSL->decrypted_buffer_offset;
      size_t to_copy = std::min(size_t(BufferSize), available);

      memcpy(Buffer, SSL->decrypted_buffer.data() + SSL->decrypted_buffer_offset, to_copy);
      SSL->decrypted_buffer_offset += to_copy;

      // If we've consumed all leftover data, reset the buffer
      if (SSL->decrypted_buffer_offset >= SSL->decrypted_buffer_used) {
         SSL->decrypted_buffer_used = 0;
         SSL->decrypted_buffer_offset = 0;
      }

      SSL->last_error = SSL_OK;
      return int(to_copy);
   }

   while (true) {
      // Try to decrypt any data we already have in the receive buffer
      if (SSL->recv_buffer_used > 0) {
         std::array<SecBuffer, 4> buffers;
         buffers[0].pvBuffer = SSL->recv_buffer.data();
         buffers[0].cbBuffer = (ULONG)SSL->recv_buffer_used;
         buffers[0].BufferType = SECBUFFER_DATA;

         buffers[1].BufferType = SECBUFFER_EMPTY;
         buffers[2].BufferType = SECBUFFER_EMPTY;
         buffers[3].BufferType = SECBUFFER_EMPTY;

         SecBufferDesc bufferDesc;
         bufferDesc.ulVersion = SECBUFFER_VERSION;
         bufferDesc.cBuffers = (ULONG)buffers.size();
         bufferDesc.pBuffers = buffers.data();

         SECURITY_STATUS status = DecryptMessage(&SSL->context, &bufferDesc, 0, nullptr);

         if (status == SEC_E_OK) {
            // Successfully decrypted data
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
                     memmove(SSL->recv_buffer.data(), (char*)SSL->recv_buffer.data() + (SSL->recv_buffer_used - extra_bytes), extra_bytes);
                     SSL->recv_buffer_used = extra_bytes;
                  }
                  else SSL->recv_buffer_used = 0;

                  SSL->last_error = SSL_OK;
                  return int(decrypted_size);
               }
               else {
                  // Decrypted data is larger than user buffer
                  memcpy(Buffer, decrypted_data, BufferSize);

                  // Store remaining data in decrypted buffer - ensure sufficient capacity
                  size_t remaining = decrypted_size - BufferSize;
                  if (SSL->decrypted_buffer.capacity() < remaining) {
                     SSL->decrypted_buffer.reserve(std::max(remaining, SSL->decrypted_buffer.capacity() * 2));
                  }
                  if (SSL->decrypted_buffer.size() < remaining) {
                     SSL->decrypted_buffer.resize(remaining);
                  }
                  memcpy(SSL->decrypted_buffer.data(), decrypted_data + BufferSize, remaining);
                  SSL->decrypted_buffer_used = remaining;
                  SSL->decrypted_buffer_offset = 0;

                  // Handle leftover encrypted data
                  if (extra_bytes > 0) {
                     memmove(SSL->recv_buffer.data(), (char*)SSL->recv_buffer.data() + (SSL->recv_buffer_used - extra_bytes), extra_bytes);
                     SSL->recv_buffer_used = extra_bytes;
                  }
                  else SSL->recv_buffer_used = 0;

                  SSL->last_error = SSL_OK;
                  return BufferSize;
               }
            }
            else {
               // No decrypted data but successful status - handle extra bytes
               if (extra_bytes > 0) {
                  memmove(SSL->recv_buffer.data(), (char*)SSL->recv_buffer.data() + (SSL->recv_buffer_used - extra_bytes), extra_bytes);
                  SSL->recv_buffer_used = extra_bytes;
               }
               else SSL->recv_buffer_used = 0;
            }
         }
         else if (status == SEC_E_INCOMPLETE_MESSAGE) {
            // Need more encrypted data - since socket is non-blocking, we can safely try recv()
            // Fall through to receive more data
         }
         else { // Decryption failed
            SSL->last_security_status = status;
            if (status == SEC_E_DECRYPT_FAILURE) { // Connection likely closed
               SSL->last_error = SSL_ERROR_DISCONNECTED;
               return 0;
            }
            set_error_status(SSL, status, "DecryptMessage");
            SSL->last_error = SSL_ERROR_FAILED;
            return -1;
         }
      }

      // Try to receive more encrypted data (socket is always non-blocking)
      size_t space_available = SSL->recv_buffer.size() - SSL->recv_buffer_used;
      if (space_available == 0) {
         // Buffer full but no complete message - expand buffer if possible
         if (SSL->recv_buffer.size() < SSL_IO_BUFFER_SIZE) {
            SSL->recv_buffer.resize(std::min(SSL->recv_buffer.size() * 2, SSL_IO_BUFFER_SIZE));
            space_available = SSL->recv_buffer.size() - SSL->recv_buffer_used;
         }
         else {
            SSL->error_description = "SSL receive buffer overflow";
            SSL->last_error = SSL_ERROR_FAILED;
            return -1;
         }
      }

      auto received = recv(SSL->socket_handle, (char*)SSL->recv_buffer.data() + SSL->recv_buffer_used, int(space_available), 0);

      if (received == SOCKET_ERROR) {
         auto error = WSAGetLastError();
         if (error == WSAEWOULDBLOCK) {
            // No more data available on non-blocking socket
            // If we have any buffered data, try decryption once more, otherwise return would-block
            if (SSL->recv_buffer_used > 0) continue; // Try to decrypt existing buffer data
            SSL->last_error = SSL_ERROR_WOULD_BLOCK;
            return -1;
         }

         return SSL->process_recv_error(received, "ssl_wrapper_read");
      }
      else if (received == 0) { // Connection closed gracefully
         SSL->last_error = SSL_ERROR_DISCONNECTED;
         return 0;
      }
      else { // Successfully received encrypted data
         SSL->recv_buffer_used += received;
      }
   }
}

//********************************************************************************************************************
// Write data to SSL connection

int ssl_wrapper_write(SSL_HANDLE SSL, const void* Buffer, int BufferSize)
{
   if (!SSL or !Buffer or BufferSize <= 0) return -1;

   if (!SSL->context_initialised) return -1;

   // Calculate required buffer sizes based on stream sizes
   size_t header_size = SSL->stream_sizes.cbHeader;
   size_t trailer_size = SSL->stream_sizes.cbTrailer;
   size_t max_message_size = SSL->stream_sizes.cbMaximumMessage;

   // Limit the data size to what SSL can handle in one record
   int data_to_send = std::min(BufferSize, int(max_message_size));
   size_t total_size = header_size + data_to_send + trailer_size;

   // Ensure our send buffer is large enough - use capacity check for efficiency
   if (SSL->send_buffer.capacity() < total_size) {
      SSL->send_buffer.reserve(std::max(total_size, SSL->send_buffer.capacity() * 2));
   }
   if (SSL->send_buffer.size() < total_size) SSL->send_buffer.resize(total_size);

   // Set up SecBuffer structures for encryption
   std::array<SecBuffer, 4> buffers;

   // Header buffer
   buffers[0].pvBuffer = SSL->send_buffer.data();
   buffers[0].cbBuffer = header_size;
   buffers[0].BufferType = SECBUFFER_STREAM_HEADER;

   // Data buffer - copy user data after header
   buffers[1].pvBuffer = SSL->send_buffer.data() + header_size;
   buffers[1].cbBuffer = data_to_send;
   buffers[1].BufferType = SECBUFFER_DATA;
   memcpy(buffers[1].pvBuffer, Buffer, data_to_send);

   // Trailer buffer
   buffers[2].pvBuffer = SSL->send_buffer.data() + header_size + data_to_send;
   buffers[2].cbBuffer = trailer_size;
   buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;

   // Empty buffer
   buffers[3].pvBuffer = nullptr;
   buffers[3].cbBuffer = 0;
   buffers[3].BufferType = SECBUFFER_EMPTY;

   SecBufferDesc bufferDesc;
   bufferDesc.ulVersion = SECBUFFER_VERSION;
   bufferDesc.cBuffers = (ULONG)buffers.size();
   bufferDesc.pBuffers = buffers.data();

   auto status = EncryptMessage(&SSL->context, 0, &bufferDesc, 0);

   if (status != SEC_E_OK) {
      set_error_status(SSL, status, "EncryptMessage");
      SSL->last_error = SSL_ERROR_FAILED;
      return -1;
   }

   DWORD encrypted_size = buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer;

   // Send the encrypted data
   auto sent = send(SSL->socket_handle, (const char*)SSL->send_buffer.data(), encrypted_size, 0);

   if (sent == SOCKET_ERROR) {
      auto error = WSAGetLastError();
      SSL->last_win32_error = error;
      if (error == WSAEWOULDBLOCK) {
         SSL->last_error = SSL_ERROR_WOULD_BLOCK;
         SSL->error_description = "SSL write would block (WSAEWOULDBLOCK)";
      }
      else {
         SSL->last_error = SSL_ERROR_FAILED;
         SSL->error_description = "SSL write failed: " + std::to_string(error);
      }
      return -1;
   }
   else if (sent != int(encrypted_size)) {
      // Partial send - this is problematic for SSL records
      SSL->last_error = SSL_ERROR_FAILED;
      SSL->error_description = "SSL partial write - SSL record boundary violated";
      return -1;
   }

   SSL->last_error = SSL_OK;
   return data_to_send;
}

//********************************************************************************************************************
// Get last error

SSL_ERROR_CODE ssl_wrapper_get_error(SSL_HANDLE SSL)
{
   if (!SSL) return SSL_ERROR_ARGS;
   return ((ssl_context*)SSL)->last_error;
}

//********************************************************************************************************************
// Get detailed Windows error information

uint32_t ssl_wrapper_get_last_win32_error(SSL_HANDLE SSL)
{
   if (!SSL) return 0;
   return ((ssl_context*)SSL)->last_win32_error;
}

//********************************************************************************************************************
// Get last security status

int ssl_wrapper_get_last_security_status(SSL_HANDLE SSL)
{
   if (!SSL) return 0;
   return int(((ssl_context*)SSL)->last_security_status);
}

//********************************************************************************************************************
// Get human-readable error description

const char* ssl_wrapper_get_error_description(SSL_HANDLE SSL)
{
   if (!SSL) return "Invalid SSL handle";
   generate_error_description(SSL); // Generate description if needed
   return SSL->error_description.c_str();
}

//********************************************************************************************************************
// Get certificate verification result

int ssl_wrapper_get_verify_result(SSL_HANDLE SSL)
{
   if (!SSL) return -1;

   // Check if SSL context is properly initialized for certificate validation
   if (!SSL->context_initialised) return -1;

   // Query certificate context from the established SSL connection
   PCCERT_CONTEXT cert_context = nullptr;
   SECURITY_STATUS status = QueryContextAttributes(&SSL->context, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &cert_context);

   if (status != SEC_E_OK) { // Failed to get certificate context - connection is not secure
      SSL->last_security_status = status;
      SSL->last_error = SSL_ERROR_FAILED;
      return -1; // Certificate validation failed
   }

   if (!cert_context) return -1; // No certificate presented by server

   // Query connection info to check if certificate validation succeeded
   SecPkgContext_ConnectionInfo conn_info;
   status = QueryContextAttributes(&SSL->context, SECPKG_ATTR_CONNECTION_INFO, &conn_info);

   if (status != SEC_E_OK) {
      CertFreeCertificateContext(cert_context);
      SSL->last_security_status = status;
      return -1;
   }

   // With SCH_CRED_AUTO_CRED_VALIDATION, Windows should have validated the certificate
   // Check if we have a valid cipher suite (indicates successful validation)

   if (conn_info.aiCipher == 0 or conn_info.aiHash == 0) {
      CertFreeCertificateContext(cert_context);
      return -1; // Invalid cipher negotiation indicates cert issues
   }

   // Additional validation: Check certificate chain trust
   CERT_CHAIN_PARA chain_para{};
   chain_para.cbSize = sizeof(CERT_CHAIN_PARA);

   PCCERT_CHAIN_CONTEXT chain_context = nullptr;
   BOOL chain_result = CertGetCertificateChain(
      nullptr,           // use default chain engine
      cert_context,      // certificate to validate
      nullptr,           // use current time
      cert_context->hCertStore, // additional store
      &chain_para,       // chain building parameters
      0,                 // flags
      nullptr,           // reserved
      &chain_context     // returned chain context
   );

   int result = 0; // Default to success (X509_V_OK equivalent)

   if (!chain_result or !chain_context) {
      result = -1; // Certificate chain validation failed
   }
   else {
      // Check chain trust status
      CERT_TRUST_STATUS trust_status = chain_context->TrustStatus;

      // Check for any critical errors in certificate chain
      if (trust_status.dwErrorStatus != CERT_TRUST_NO_ERROR) {
         if (trust_status.dwErrorStatus & CERT_TRUST_IS_NOT_TIME_VALID) {
            result = 10; // Certificate expired (similar to X509_V_ERR_CERT_HAS_EXPIRED)
         }
         else if (trust_status.dwErrorStatus & CERT_TRUST_IS_UNTRUSTED_ROOT) {
            result = 19; // Untrusted root (similar to X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN)
         }
         else if (trust_status.dwErrorStatus & CERT_TRUST_IS_PARTIAL_CHAIN) {
            result = 2;  // Unable to get issuer certificate
         }
         else if (trust_status.dwErrorStatus & CERT_TRUST_IS_REVOKED) {
            result = 23; // Certificate revoked
         }
         else {
            result = -1; // General certificate error
         }
      }
   }

   if (chain_context) CertFreeCertificateChain(chain_context);
   CertFreeCertificateContext(cert_context);

   return result;
}

//********************************************************************************************************************
// Create or find a self-signed certificate for localhost testing

static PCCERT_CONTEXT create_self_signed_certificate()
{
   // Try to find existing localhost certificate first
   HCERTSTORE cert_store = CertOpenSystemStore(0, "MY");
   if (!cert_store) return nullptr;

   PCCERT_CONTEXT cert_context = nullptr;

   // Look for existing localhost certificate
   while ((cert_context = CertEnumCertificatesInStore(cert_store, cert_context)) != nullptr) {
      DWORD name_len = CertGetNameString(cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nullptr, 0);
      if (name_len > 1) {
         std::vector<char> name_buffer(name_len);
         CertGetNameString(cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, name_buffer.data(), name_len);

         std::string cert_name(name_buffer.data());
         if (cert_name == "localhost" or cert_name == "127.0.0.1") {
            // Found a suitable certificate for localhost
            CertCloseStore(cert_store, 0);
            return CertDuplicateCertificateContext(cert_context);
         }
      }
   }

   // No suitable certificate found, create a self-signed one for testing
   // This is a simplified approach - in production, proper certificates should be used

   CertCloseStore(cert_store, 0);
   return nullptr; // For now, return null - we'll implement cert creation if needed
}

//********************************************************************************************************************
// Server-side handshake handling using AcceptSecurityContext

SSL_ERROR_CODE ssl_wrapper_accept_handshake(SSL_HANDLE SSL, const void* ClientData, int DataLength, void** ResponseData, int* ResponseLength)
{
   if (!SSL or !ClientData or DataLength <= 0 or !ResponseData or !ResponseLength) {
      return SSL_ERROR_ARGS;
   }

   *ResponseData = nullptr;
   *ResponseLength = 0;

   // Acquire server credentials if not already done
   if (!SSL->credentials_acquired) {
      SCHANNEL_CRED cred_data;
      memset(&cred_data, 0, sizeof(cred_data));
      cred_data.dwVersion = SCHANNEL_CRED_VERSION;
      cred_data.dwFlags = SCH_CRED_NO_SYSTEM_MAPPER | SCH_CRED_NO_DEFAULT_CREDS;

      // If we have a server certificate, use it
      if (SSL->server_certificate) {
         cred_data.cCreds = 1;
         cred_data.paCred = &SSL->server_certificate;
      }

      TimeStamp expiry;
      auto status = AcquireCredentialsHandle(
         nullptr, const_cast<char*>(UNISP_NAME), SECPKG_CRED_INBOUND,  // INBOUND for server
         nullptr, &cred_data, nullptr, nullptr,
         &SSL->credentials, &expiry);

      if (status != SEC_E_OK) {
         set_error_status(SSL, status, "AcquireCredentialsHandle (server)");
         SSL->last_error = SSL_ERROR_FAILED;
         return SSL_ERROR_FAILED;
      }

      SSL->credentials_acquired = true;
   }

   // Setup input buffer with client data
   SecBuffer in_buffers[2];
   in_buffers[0].pvBuffer = const_cast<void*>(ClientData);
   in_buffers[0].cbBuffer = DataLength;
   in_buffers[0].BufferType = SECBUFFER_TOKEN;
   in_buffers[1].pvBuffer = nullptr;
   in_buffers[1].cbBuffer = 0;
   in_buffers[1].BufferType = SECBUFFER_EMPTY;

   SecBufferDesc in_buffer_desc;
   in_buffer_desc.cBuffers = 2;
   in_buffer_desc.pBuffers = in_buffers;
   in_buffer_desc.ulVersion = SECBUFFER_VERSION;

   // Setup output buffer for response
   SecBuffer out_buffer;
   out_buffer.pvBuffer = nullptr;
   out_buffer.BufferType = SECBUFFER_TOKEN;
   out_buffer.cbBuffer = 0;

   SecBufferDesc out_buffer_desc;
   out_buffer_desc.cBuffers = 1;
   out_buffer_desc.pBuffers = &out_buffer;
   out_buffer_desc.ulVersion = SECBUFFER_VERSION;

   DWORD context_attr;
   TimeStamp expiry;

   SECURITY_STATUS status;
   if (!SSL->context_initialised) {
      // Initial server-side handshake
      status = AcceptSecurityContext(
         &SSL->credentials,
         nullptr,  // No existing context
         &in_buffer_desc,
         ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY |
         ASC_REQ_EXTENDED_ERROR | ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM,
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
         ASC_REQ_EXTENDED_ERROR | ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM,
         SECURITY_NATIVE_DREP,
         &SSL->context,
         &out_buffer_desc,
         &context_attr,
         &expiry);
   }

   if (status == SEC_E_OK) {
      // Handshake completed successfully
      QueryContextAttributes(&SSL->context, SECPKG_ATTR_STREAM_SIZES, &SSL->stream_sizes);
      SSL->error_description = "Server SSL handshake completed successfully";
      SSL->last_error = SSL_OK;

      // If there's response data, copy it
      if (out_buffer.pvBuffer and out_buffer.cbBuffer > 0) {
         *ResponseData = malloc(out_buffer.cbBuffer);
         if (*ResponseData) {
            memcpy(*ResponseData, out_buffer.pvBuffer, out_buffer.cbBuffer);
            *ResponseLength = out_buffer.cbBuffer;
         }
         FreeContextBuffer(out_buffer.pvBuffer);
      }

      return SSL_OK;
   }
   else if (status == SEC_I_CONTINUE_NEEDED) {
      // More handshake data needed
      SSL->last_error = SSL_ERROR_CONNECTING;
      SSL->error_description = "Server SSL handshake needs more data";

      // Copy response data to send back to client
      if (out_buffer.pvBuffer and out_buffer.cbBuffer > 0) {
         *ResponseData = malloc(out_buffer.cbBuffer);
         if (*ResponseData) {
            memcpy(*ResponseData, out_buffer.pvBuffer, out_buffer.cbBuffer);
            *ResponseLength = out_buffer.cbBuffer;
         }
         FreeContextBuffer(out_buffer.pvBuffer);
      }

      return SSL_ERROR_CONNECTING;
   }
   else {
      // Handshake failed
      set_error_status(SSL, status, "AcceptSecurityContext");
      SSL->last_error = SSL_ERROR_FAILED;

      if (out_buffer.pvBuffer) FreeContextBuffer(out_buffer.pvBuffer);

      return SSL_ERROR_FAILED;
   }
}

#endif // _WIN32