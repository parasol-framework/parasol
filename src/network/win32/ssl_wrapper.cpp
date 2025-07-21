/*********************************************************************************************************************

Windows SSL Wrapper Implementation
Pure Windows implementation that avoids all Parasol headers to prevent conflicts.

*********************************************************************************************************************/

#ifdef _WIN32

// Define required constants to avoid header conflicts
#define WIN32_LEAN_AND_MEAN
#define SECURITY_WIN32
#define NOMINMAX

// Only include Windows headers - NO Parasol headers
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <schannel.h>
#include <sspi.h>
#include <security.h>
#include <wincrypt.h>
#include <cstdlib>
#include <cstring>

#include "ssl_wrapper.h"

#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "crypt32.lib")

// Internal SSL context structure
struct ssl_context {
    CredHandle credentials;
    CtxtHandle context;
    SecPkgContext_StreamSizes stream_sizes;
    SOCKET socket_handle;
    unsigned char* io_buffer;
    SSL_ERROR_CODE last_error;
    bool credentials_acquired;
    bool context_initialized;
};

// Global state
static bool g_ssl_initialized = false;
static HCERTSTORE g_cert_store = nullptr;

// Buffer size for SSL operations
#define SSL_IO_BUFFER_SIZE 0x10000

/*********************************************************************************************************************
** Initialize SSL wrapper
*/

SSL_ERROR_CODE ssl_wrapper_init(void)
{
   if (g_ssl_initialized) return SSL_OK;

   g_cert_store = CertOpenSystemStoreA(0, "MY");
   if (!g_cert_store) return SSL_ERROR_FAILED;
   g_ssl_initialized = true;
   return SSL_OK;
}

/*********************************************************************************************************************
** Cleanup SSL wrapper
*/

void ssl_wrapper_cleanup(void)
{
   if (!g_ssl_initialized) return;

   if (g_cert_store) {
      CertCloseStore(g_cert_store, 0);
      g_cert_store = nullptr;
   }

   g_ssl_initialized = false;
}

/*********************************************************************************************************************
** Create SSL context
*/

SSL_HANDLE ssl_wrapper_create_context(void)
{
   if (!g_ssl_initialized) {
      if (ssl_wrapper_init() != SSL_OK) return nullptr;
   }

   ssl_context* ctx = (ssl_context*)malloc(sizeof(ssl_context));
   if (!ctx) return nullptr;

   memset(ctx, 0, sizeof(ssl_context));
   ctx->socket_handle = INVALID_SOCKET;
   ctx->last_error = SSL_OK;
   ctx->credentials_acquired = false;
   ctx->context_initialized = false;

   // Allocate I/O buffer
   ctx->io_buffer = (unsigned char*)malloc(SSL_IO_BUFFER_SIZE);
   if (!ctx->io_buffer) {
      free(ctx);
      return nullptr;
   }

   return ctx;
}

/*********************************************************************************************************************
** Free SSL context
*/

void ssl_wrapper_free_context(SSL_HANDLE ssl)
{
   if (!ssl) return;

   auto ctx = (ssl_context*)ssl;

   if (ctx->context_initialized) DeleteSecurityContext(&ctx->context);
   if (ctx->credentials_acquired) FreeCredentialsHandle(&ctx->credentials);
   if (ctx->io_buffer) free(ctx->io_buffer);
   free(ctx);
}

/*********************************************************************************************************************
** Set socket for SSL context
*/

SSL_ERROR_CODE ssl_wrapper_set_socket(SSL_HANDLE ssl, SOCKET socket_handle)
{
   if (!ssl || socket_handle == INVALID_SOCKET) return SSL_ERROR_ARGS;

   auto ctx = (ssl_context*)ssl;
   ctx->socket_handle = socket_handle;

   // Acquire credentials
   SCHANNEL_CRED cred_data;
   memset(&cred_data, 0, sizeof(cred_data));
   cred_data.dwVersion = SCHANNEL_CRED_VERSION;
   cred_data.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_SERVERNAME_CHECK;
   cred_data.grbitEnabledProtocols = SP_PROT_TLS1_2 | SP_PROT_TLS1_3;

   TimeStamp expiry;
   SECURITY_STATUS status = AcquireCredentialsHandleA(
       nullptr, UNISP_NAME, SECPKG_CRED_OUTBOUND,
       nullptr, &cred_data, nullptr, nullptr,
       &ctx->credentials, &expiry);

   if (status != SEC_E_OK) {
      ctx->last_error = SSL_ERROR_FAILED;
      return SSL_ERROR_FAILED;
   }

   ctx->credentials_acquired = true;
   return SSL_OK;
}

/*********************************************************************************************************************
** Perform SSL connect
*/

SSL_ERROR_CODE ssl_wrapper_connect(SSL_HANDLE ssl)
{
   if (!ssl) return SSL_ERROR_ARGS;

   ssl_context* ctx = (ssl_context*)ssl;
   if (!ctx->credentials_acquired) return SSL_ERROR_FAILED;

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

   TimeStamp expiry;
   DWORD out_flags;

   SECURITY_STATUS status = InitializeSecurityContextA(
      &ctx->credentials, nullptr, nullptr, flags,
      0, SECURITY_NATIVE_DREP, nullptr, 0,
      &ctx->context, &out_buffer_desc, &out_flags, &expiry);

   if (status != SEC_I_CONTINUE_NEEDED) {
      ctx->last_error = SSL_ERROR_FAILED;
      return SSL_ERROR_FAILED;
   }

   ctx->context_initialized = true;

   // Send initial handshake data
   if (out_buffer.cbBuffer > 0 && out_buffer.pvBuffer != nullptr) {
      int sent = send(ctx->socket_handle, (char*)out_buffer.pvBuffer, out_buffer.cbBuffer, 0);
      FreeContextBuffer(out_buffer.pvBuffer);

      if (sent == SOCKET_ERROR) {
         int error = WSAGetLastError();
         if (error == WSAEWOULDBLOCK) {
            ctx->last_error = SSL_ERROR_WOULD_BLOCK;
            return SSL_ERROR_WOULD_BLOCK;
         }
         ctx->last_error = SSL_ERROR_FAILED;
         return SSL_ERROR_FAILED;
      }
   }

   // For simplicity, return connecting status - full handshake would need more rounds
   ctx->last_error = SSL_ERROR_CONNECTING;
   return SSL_ERROR_CONNECTING;
}

/*********************************************************************************************************************
** Read data from SSL connection
*/

int ssl_wrapper_read(SSL_HANDLE ssl, void* buffer, int buffer_size)
{
   if (!ssl || !buffer || buffer_size <= 0) return -1;

   auto ctx = (ssl_context*)ssl;
   if (!ctx->context_initialized) return -1;

   // Simplified implementation - real implementation would need proper decryption
   int received = recv(ctx->socket_handle, (char*)buffer, buffer_size, 0);
   if (received == SOCKET_ERROR) {
      int error = WSAGetLastError();
      if (error == WSAEWOULDBLOCK) ctx->last_error = SSL_ERROR_WOULD_BLOCK;
      else ctx->last_error = SSL_ERROR_FAILED;
      return -1;
   }

   return received;
}

/*********************************************************************************************************************
** Write data to SSL connection
*/

int ssl_wrapper_write(SSL_HANDLE ssl, const void* buffer, int buffer_size)
{
   if (!ssl || !buffer || buffer_size <= 0) return -1;

   auto ctx = (ssl_context*)ssl;
   if (!ctx->context_initialized) return -1;

   // Simplified implementation - real implementation would need proper encryption
   int sent = send(ctx->socket_handle, (const char*)buffer, buffer_size, 0);
   if (sent == SOCKET_ERROR) {
      int error = WSAGetLastError();
      if (error == WSAEWOULDBLOCK) ctx->last_error = SSL_ERROR_WOULD_BLOCK;
      else ctx->last_error = SSL_ERROR_FAILED;
      return -1;
   }

   return sent;
}

/*********************************************************************************************************************
** Get last error
*/

SSL_ERROR_CODE ssl_wrapper_get_error(SSL_HANDLE ssl)
{
   if (!ssl) return SSL_ERROR_ARGS;
   return ((ssl_context*)ssl)->last_error;
}

#endif // _WIN32