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
#include <prsht.h>
#include <cryptuiapi.h>
#include <cstring>
#include <vector>
#include <string>
#include <sstream>
#include <array>
#include <algorithm>
#include <span>

#include "ssl_wrapper.h"

static void ssl_debug_log(int level, const char* format, ...);
extern "C" void ssl_debug_to_parasol_log(const char* message, int level);

// Define provider names if not available
#ifndef MS_ENH_RSA_AES_PROV
#define MS_ENH_RSA_AES_PROV "Microsoft Enhanced RSA and AES Cryptographic Provider"
#endif

// Buffer size for SSL operations - optimized for SSL record sizes
constexpr size_t SSL_IO_BUFFER_SIZE = 0x8000;  // 32KB - 2x max SSL record size (16KB)
constexpr size_t SSL_INITIAL_BUFFER_SIZE = 0x4000;  // 16KB initial size
constexpr size_t SSL_MAX_RECORD_SIZE = 0x4000;  // 16KB max SSL record size
constexpr size_t SSL_RECORD_HEADER_SIZE = 5;  // SSL record header size
constexpr size_t MIN_SSL_RECORD_SIZE = 32;  // Conservative minimum for a valid SSL record
constexpr int MAX_INVALID_TOKEN_RETRIES = 3;  // Maximum retries for invalid token errors

// Modern buffer management class for SSL operations
class SSLBuffer {
private:
   std::vector<unsigned char> data_;
   size_t used_ = 0;
   
public:
   explicit SSLBuffer(size_t initial_size = SSL_INITIAL_BUFFER_SIZE) 
      : data_(initial_size) {}
   
   // Get available space for writing
   std::span<unsigned char> available_space() {
      return std::span<unsigned char>(data_.data() + used_, data_.size() - used_);
   }
   
   // Get used data for reading
   std::span<const unsigned char> used_data() const {
      return std::span<const unsigned char>(data_.data(), used_);
   }
   
   // Get mutable view of used data
   std::span<unsigned char> used_data_mutable() {
      return std::span<unsigned char>(data_.data(), used_);
   }
   
   // Advance the used counter after writing data
   void advance_used(size_t bytes) {
      if (used_ + bytes <= data_.size()) used_ += bytes;
   }
   
   // Ensure buffer has at least min_size capacity
   void ensure_capacity(size_t min_size) {
      if (data_.size() < min_size) {
         data_.resize(std::min(min_size, SSL_IO_BUFFER_SIZE));
      }
   }
   
   void reset() { used_ = 0; }
   
   void clear() {
      used_ = 0;
      data_.clear();
      data_.resize(SSL_INITIAL_BUFFER_SIZE);
   }
   
   bool append(std::span<const unsigned char> data) {
      size_t total_needed = used_ + data.size();
      if (total_needed > SSL_IO_BUFFER_SIZE) return false; // Would exceed maximum buffer size     
      if (data_.size() < total_needed) data_.resize(total_needed);    
      std::memcpy(data_.data() + used_, data.data(), data.size());
      used_ += data.size();
      return true;
   }
   
   // Remove bytes from beginning of buffer
   void consume_front(size_t bytes) {
      if (bytes >= used_) reset();
      else {
         std::memmove(data_.data(), data_.data() + bytes, used_ - bytes);
         used_ -= bytes;
      }
   }
   
   // Move remaining data to front and update used count
   void compact(size_t bytes_consumed) {
      if (bytes_consumed >= used_) reset();
      else {
         size_t remaining = used_ - bytes_consumed;
         std::memmove(data_.data(), data_.data() + bytes_consumed, remaining);
         used_ = remaining;
      }
   }
   
   unsigned char * data() { return data_.data(); }
   const unsigned char * data() const { return data_.data(); }
   size_t size() const { return used_; }
   size_t capacity() const { return data_.size(); }
   bool empty() const { return used_ == 0; }
   size_t available() const { return data_.size() - used_; }
   
   void reserve(size_t new_cap) {
      if (new_cap <= SSL_IO_BUFFER_SIZE and new_cap > data_.size()) {
         data_.reserve(new_cap);
      }
   }
   
   void resize(size_t new_size) {
      if (new_size <= SSL_IO_BUFFER_SIZE) {
         data_.resize(new_size);
         if (used_ > new_size) used_ = new_size;
      }
   }
};

struct ssl_context {
   CredHandle credentials;
   CtxtHandle context;
   SecPkgContext_StreamSizes stream_sizes;
   SOCKET socket_handle;
   SSLBuffer io_buffer;
   SSLBuffer recv_buffer;                      // Persistent buffer for incomplete SSL messages
   SSLBuffer send_buffer;                      // Buffer for SSL encryption
   SSLBuffer decrypted_buffer;                 // Buffer for leftover decrypted data
   size_t decrypted_buffer_offset;             // Bytes already returned to user
   bool error_description_dirty;               // True if error description needs regeneration
   SECURITY_STATUS last_security_status;
   DWORD last_win32_error;
   std::string error_description;
   std::string hostname;
   bool validate_credentials;
   bool credentials_acquired;
   bool context_initialised;
   bool is_server_mode;                        // True for server-side SSL, false for client-side
   PCCERT_CONTEXT server_certificate;          // Server certificate for server-side SSL

   ssl_context() 
      : socket_handle(INVALID_SOCKET)
      , io_buffer(SSL_INITIAL_BUFFER_SIZE)
      , recv_buffer(SSL_INITIAL_BUFFER_SIZE)
      , send_buffer(SSL_INITIAL_BUFFER_SIZE)
      , decrypted_buffer(SSL_MAX_RECORD_SIZE)
      , decrypted_buffer_offset(0)
      , error_description_dirty(false)
      , last_security_status(SEC_E_OK)
      , last_win32_error(0)
      , error_description("No error")
      , validate_credentials(true)
      , credentials_acquired(false)
      , context_initialised(false)
      , is_server_mode(false)
      , server_certificate(nullptr)
   {
      // Pre-allocate buffers with optimized sizes
      io_buffer.reserve(SSL_IO_BUFFER_SIZE);
      recv_buffer.reserve(SSL_IO_BUFFER_SIZE);
      send_buffer.reserve(SSL_IO_BUFFER_SIZE);
      decrypted_buffer.reserve(SSL_MAX_RECORD_SIZE);
   }

   ~ssl_context() {
      if (context_initialised) DeleteSecurityContext(&context);
      if (credentials_acquired) FreeCredentialsHandle(&credentials);
      if (server_certificate) CertFreeCertificateContext(server_certificate);
   }

   SSL_ERROR_CODE process_recv_error(int Result, std::string Process)
   {
      if (!Result) {
         error_description = "Connection closed by server during " + Process;
         return SSL_ERROR_DISCONNECTED;
      }
      else if (Result == -1) {
         last_win32_error = WSAGetLastError();
         if (last_win32_error == WSAEWOULDBLOCK) {
            error_description = "Socket would block during " + Process + " and is in non-blocking mode.";
            return SSL_ERROR_WOULD_BLOCK;
         }
         else {
            error_description = "Failed to receive response during " + Process + ": " + std::to_string(last_win32_error);
            return SSL_ERROR_FAILED;
         }
      }

      return SSL_OK;
   }
};

//********************************************************************************************************************

static bool glSSLInitialised = false;
static bool glLoggingEnabled = false;
//static HCERTSTORE g_cert_store = nullptr;
static SSL_DEBUG_CALLBACK g_debug_callback = nullptr;

static PCCERT_CONTEXT load_pem_certificate(const std::string &);
static PCCERT_CONTEXT load_pkcs12_certificate(const std::string &);

#include "ssl_certs.cpp"

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
      case SECURITY_STATUS(0x90321): return "Wrong credential handle";
      case SECURITY_STATUS(0x80090317): return "No context available";
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

   const char *operation = Ctx->error_description.c_str(); // Stored operation
   const char *status_desc = get_status_description(Ctx->last_security_status);

   std::stringstream stream;
   stream << operation << ":" << status_desc << "(Status: " << (uint32_t)Ctx->last_security_status << ", Win32: "
          << Ctx->last_win32_error << ")";
   Ctx->error_description = stream.str();
   Ctx->error_description_dirty = false;
}

//********************************************************************************************************************
// SSL Debug logging function

static void ssl_debug_log(int level, const char* format, ...)
{
   if (!glLoggingEnabled) return;
   
   char buffer[1024];
   va_list args;
   va_start(args, format);
   vsnprintf(buffer, sizeof(buffer), format, args);
   va_end(args);
   
   ssl_debug_to_parasol_log(buffer, level);
}

//********************************************************************************************************************
// Debug handshake state using QueryContextAttributes

static void debug_ssl_handshake_state(ssl_context* SSL, const char* operation)
{
   if (!glLoggingEnabled or !SSL->context_initialised) return;
   
   ssl_debug_log(SSL_DEBUG_INFO, "SSL Debug [%s] - Context initialized, querying attributes...", operation);
   
   // Query connection info
   SecPkgContext_ConnectionInfo conn_info;
   if (QueryContextAttributes(&SSL->context, SECPKG_ATTR_CONNECTION_INFO, &conn_info) == SEC_E_OK) {
      ssl_debug_log(SSL_DEBUG_INFO, "SSL Debug [%s] - Protocol: 0x%X, Cipher: 0x%X, Hash: 0x%X, KeyExch: 0x%X", 
                   operation, conn_info.dwProtocol, conn_info.aiCipher, conn_info.aiHash, conn_info.aiExch);
   }
   else ssl_debug_log(SSL_DEBUG_WARNING, "SSL Debug [%s] - Failed to query connection info", operation);
   
   // Query cipher info (Windows 10+)
   SecPkgContext_CipherInfo cipher_info;
   if (QueryContextAttributes(&SSL->context, SECPKG_ATTR_CIPHER_INFO, &cipher_info) == SEC_E_OK) {
      ssl_debug_log(SSL_DEBUG_INFO, "SSL Debug [%s] - Cipher Suite: %S", operation, cipher_info.szCipherSuite);
   }
   
   // Query key info
   SecPkgContext_KeyInfo key_info;
   if (QueryContextAttributes(&SSL->context, SECPKG_ATTR_KEY_INFO, &key_info) == SEC_E_OK) {
      ssl_debug_log(SSL_DEBUG_INFO, "SSL Debug [%s] - Signature: %S, Encryption: %S", 
                   operation, key_info.sSignatureAlgorithmName, key_info.sEncryptAlgorithmName);
   }
}

//********************************************************************************************************************
// Enhanced security status debugging

static void debug_security_status(SECURITY_STATUS status, const char* operation)
{
   if (!g_debug_callback) return;
   
   ssl_debug_log(SSL_DEBUG_INFO, "SSL Debug - %s: Status=0x%08X (%s), Win32=%d", 
                operation, status, get_status_description(status), GetLastError());
   
   // Additional detailed error info
   switch (status) {
      case SEC_E_CERT_UNKNOWN:
         ssl_debug_log(SSL_DEBUG_WARNING, "  Certificate issue detected - server may not have valid certificate");
         break;
      case SEC_E_INVALID_TOKEN:
         ssl_debug_log(SSL_DEBUG_WARNING, "  Invalid handshake token - possible protocol mismatch or malformed data");
         break;
      case SEC_E_INCOMPLETE_MESSAGE:
         ssl_debug_log(SSL_DEBUG_TRACE, "  Incomplete SSL message - need more handshake data");
         break;
      case SEC_I_CONTINUE_NEEDED:
         ssl_debug_log(SSL_DEBUG_TRACE, "  SSL handshake continuing - more exchanges needed");
         break;
      case SEC_E_UNTRUSTED_ROOT:
         ssl_debug_log(SSL_DEBUG_WARNING, "  Untrusted root certificate - self-signed or unknown CA");
         break;
      case SEC_E_NO_CREDENTIALS:
         ssl_debug_log(SSL_DEBUG_ERROR, "  No credentials available for SSL context");
         break;
   }
}

//********************************************************************************************************************
// Called on module expunge

void ssl_cleanup(void)
{
   if (!glSSLInitialised) return;

   //if (g_cert_store) { CertCloseStore(g_cert_store, 0); g_cert_store = nullptr; }

   glSSLInitialised = false;
}

//********************************************************************************************************************
// Create SSL context

SSL_HANDLE ssl_create_context(const std::string &CertPath, bool ValidateCredentials, bool ServerMode)
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

      // Try to get a server certificate for localhost testing (Windows does not have reliable support for self-signed certs)
      // You can use mkcert to generate a local certificate for testing.
      // First try to load mkcert-generated PKCS#12 certificate (includes private key)

      ctx->server_certificate = load_pkcs12_certificate(CertPath + "localhost.p12");
      
      if (ctx->server_certificate) {
         ssl_debug_log(SSL_DEBUG_INFO, "Loaded mkcert PKCS#12 certificate for localhost");
      }
      else {
         ssl_debug_log(SSL_DEBUG_INFO, "mkcert PKCS#12 not found, trying PEM certificate");
         ctx->server_certificate = load_pem_certificate(CertPath + "localhost.pem");
         
         if (ctx->server_certificate) {
            ssl_debug_log(SSL_DEBUG_INFO, "Loaded mkcert PEM certificate for localhost");
         }
         else {
            ssl_debug_log(SSL_DEBUG_WARNING, "Failed to find server certificate for localhost");
            delete ctx;
            return nullptr; // No valid server certificate found
         }
      }
   }

   return ctx;
}

//********************************************************************************************************************

void ssl_free_context(SSL_HANDLE SSL)
{
   delete SSL; // Destructor handles all cleanup
}

//********************************************************************************************************************

void ssl_get_error(SSL_HANDLE SSL, const char **Message)
{
   if (Message) *Message = ssl_error_description(SSL);
}

//********************************************************************************************************************

uint32_t ssl_last_win32_error(SSL_HANDLE SSL)
{
   return ((ssl_context*)SSL)->last_win32_error;
}

//********************************************************************************************************************
// Debug handshake public function

void ssl_debug_handshake(SSL_HANDLE SSL, const char* operation)
{
   debug_ssl_handshake_state(SSL, operation);
}

//********************************************************************************************************************
// Set socket handle for server-side SSL contexts

void ssl_set_socket(SSL_HANDLE SSL, void* socket_handle)
{
   if (!socket_handle) return;
   SSL->socket_handle = (SOCKET)(size_t)socket_handle;
   ssl_debug_log(SSL_DEBUG_TRACE, "SSL socket handle set: %d", (int)SSL->socket_handle);
}

//********************************************************************************************************************
// Check if SSL context has decrypted application data ready

bool ssl_has_decrypted_data(SSL_HANDLE SSL)
{
   return SSL->decrypted_buffer.size() > SSL->decrypted_buffer_offset;
}

//********************************************************************************************************************
// Check if SSL context has encrypted data ready for decryption

bool ssl_has_encrypted_data(SSL_HANDLE SSL)
{
   return !SSL->recv_buffer.empty();
}

//********************************************************************************************************************
// Get last security status

int ssl_last_security_status(SSL_HANDLE SSL)
{
   return int(((ssl_context*)SSL)->last_security_status);
}

//********************************************************************************************************************
// Get human-readable error description

const char* ssl_error_description(SSL_HANDLE SSL)
{
   generate_error_description(SSL); // Generate description if needed
   return SSL->error_description.c_str();
}

void ssl_enable_logging()
{
   glLoggingEnabled = true;
}

#include "ssl_handshake.cpp"
#include "ssl_io.cpp"
#include "ssl_connect.cpp"

#endif // _WIN32