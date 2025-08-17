#pragma once

#include <parasol/system/errors.h>

#ifndef _WINSOCK2API_
typedef void * SOCKET;
#endif

typedef struct ssl_context * SSL_HANDLE;

// Error codes - mirror Parasol ERR enum values
typedef enum {
    SSL_OK = 0,
    SSL_ERROR_ARGS = -2,
    SSL_ERROR_FAILED = -1,
    SSL_ERROR_MEMORY = -8,
    SSL_ERROR_WOULD_BLOCK = -18,
    SSL_ERROR_DISCONNECTED = -28,
    SSL_ERROR_CONNECTING = -100  // Custom code for SSL handshake in progress
} SSL_ERROR_CODE;

ERR ssl_wrapper_init(void);
void ssl_cleanup(void);
SSL_HANDLE ssl_create_context(const std::string &, bool ValidateCredentials = true, bool ServerMode = false);
void ssl_free_context(SSL_HANDLE ssl);
SSL_ERROR_CODE ssl_connect(SSL_HANDLE, void *, const std::string &);
SSL_ERROR_CODE ssl_continue_handshake(SSL_HANDLE, const void *, int, int &);
SSL_ERROR_CODE ssl_accept_handshake(SSL_HANDLE, const void *, int, std::vector<unsigned char>&);
void ssl_set_socket(SSL_HANDLE ssl, void* socket_handle);
bool ssl_has_decrypted_data(SSL_HANDLE ssl);
int ssl_read_internal(SSL_HANDLE ssl, void* buffer, int buffer_size, int &);
SSL_ERROR_CODE ssl_read(SSL_HANDLE ssl, void* buffer, int buffer_size, int* bytes_read);
SSL_ERROR_CODE ssl_write(SSL_HANDLE ssl, const void* buffer, size_t buffer_size, size_t* bytes_sent);
void ssl_get_error(SSL_HANDLE, const char **Message = nullptr);
uint32_t ssl_last_win32_error(SSL_HANDLE);
int ssl_last_security_status(SSL_HANDLE);
const char* ssl_error_description(SSL_HANDLE);
bool ssl_get_verify_result(SSL_HANDLE);

// SSL Debug callback function type
typedef void (*SSL_DEBUG_CALLBACK)(const char* message, int level);

// Debug levels
enum SSL_DEBUG_LEVEL {
   SSL_DEBUG_ERROR = 0,
   SSL_DEBUG_WARNING = 1,
   SSL_DEBUG_INFO = 2,
   SSL_DEBUG_TRACE = 3
};

// Debug functions
void ssl_set_debug_callback(SSL_DEBUG_CALLBACK callback);
void ssl_debug_handshake(SSL_HANDLE ssl, const char* operation);
