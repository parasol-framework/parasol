/*********************************************************************************************************************

Windows SSL Wrapper Interface
C-style wrapper interface that isolates Windows Schannel SSL from Parasol headers.

*********************************************************************************************************************/

#ifndef WIN32_SSL_WRAPPER_H
#define WIN32_SSL_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declare socket handle type to avoid Windows header dependencies
#ifdef _WIN32
  // On Windows, SOCKET is defined by winsock2.h as UINT_PTR
  #include <winsock2.h>
#else
  typedef int SOCKET;
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
SSL_ERROR_CODE ssl_wrapper_init(void);
void ssl_wrapper_cleanup(void);

SSL_HANDLE ssl_wrapper_create_context(void);
void ssl_wrapper_free_context(SSL_HANDLE ssl);

SSL_ERROR_CODE ssl_wrapper_set_socket(SSL_HANDLE ssl, SOCKET socket_handle);
SSL_ERROR_CODE ssl_wrapper_connect(SSL_HANDLE ssl);

int ssl_wrapper_read(SSL_HANDLE ssl, void* buffer, int buffer_size);
int ssl_wrapper_write(SSL_HANDLE ssl, const void* buffer, int buffer_size);

// Get last error for the SSL handle
SSL_ERROR_CODE ssl_wrapper_get_error(SSL_HANDLE ssl);

#ifdef __cplusplus
}
#endif

#endif // WIN32_SSL_WRAPPER_H