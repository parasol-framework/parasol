/*********************************************************************************************************************

Windows SSL Wrapper Interface
C-style wrapper interface that isolates Windows Schannel SSL from Parasol headers.

*********************************************************************************************************************/

#ifndef WIN32_SSL_WRAPPER_H
#define WIN32_SSL_WRAPPER_H

#include <parasol/system/errors.h>

// Forward declare socket handle type to avoid Windows header dependencies

#ifndef _WINSOCK2API_
typedef void * SOCKET;
#endif

// Opaque handle for SSL context - hides Windows-specific structures
typedef struct ssl_context* SSL_HANDLE;

// Error codes - matching Parasol ERR enum values
typedef enum {
    SSL_OK = 0,
    SSL_ERROR_ARGS = -2,
    SSL_ERROR_FAILED = -1,
    SSL_ERROR_MEMORY = -8,
    SSL_ERROR_WOULD_BLOCK = -18,
    SSL_ERROR_DISCONNECTED = -28,
    SSL_ERROR_CONNECTING = -100  // Custom code for SSL handshake in progress
} SSL_ERROR_CODE;

// SSL wrapper functions - C interface to avoid header conflicts
ERR ssl_wrapper_init(void);
void ssl_wrapper_cleanup(void);

SSL_HANDLE ssl_wrapper_create_context(void);
void ssl_wrapper_free_context(SSL_HANDLE ssl);

SSL_ERROR_CODE ssl_wrapper_connect(SSL_HANDLE, void *, const std::string&);
SSL_ERROR_CODE ssl_wrapper_continue_handshake(SSL_HANDLE, const void*, int);

int ssl_wrapper_read(SSL_HANDLE ssl, void* buffer, int buffer_size);
int ssl_wrapper_write(SSL_HANDLE ssl, const void* buffer, int buffer_size);

// Get last error for the SSL handle
SSL_ERROR_CODE ssl_wrapper_get_error(SSL_HANDLE ssl);

// Get detailed Windows error information
uint32_t ssl_wrapper_get_last_win32_error(SSL_HANDLE ssl);
int ssl_wrapper_get_last_security_status(SSL_HANDLE ssl);

// Get human-readable error description
const char* ssl_wrapper_get_error_description(SSL_HANDLE ssl);

// Get certificate verification result
int ssl_wrapper_get_verify_result(SSL_HANDLE ssl);

#endif // WIN32_SSL_WRAPPER_H