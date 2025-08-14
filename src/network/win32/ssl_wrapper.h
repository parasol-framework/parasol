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
void ssl_wrapper_cleanup(void);
SSL_HANDLE ssl_wrapper_create_context(bool ValidateCredentials = true, bool ServerMode = false);
void ssl_wrapper_free_context(SSL_HANDLE ssl);
SSL_ERROR_CODE ssl_wrapper_connect(SSL_HANDLE, void *, const std::string &);
SSL_ERROR_CODE ssl_wrapper_continue_handshake(SSL_HANDLE, const void *, int);
SSL_ERROR_CODE ssl_wrapper_accept_handshake(SSL_HANDLE, const void *, int, void **, int *);  // Server-side handshake
int ssl_wrapper_read(SSL_HANDLE ssl, void* buffer, int buffer_size);
SSL_ERROR_CODE ssl_wrapper_write(SSL_HANDLE ssl, const void* buffer, size_t buffer_size, size_t* bytes_sent);
SSL_ERROR_CODE ssl_wrapper_get_error(SSL_HANDLE, const char **Message = nullptr);
uint32_t ssl_wrapper_get_last_win32_error(SSL_HANDLE);
int ssl_wrapper_get_last_security_status(SSL_HANDLE);
const char* ssl_wrapper_get_error_description(SSL_HANDLE);
int ssl_wrapper_get_verify_result(SSL_HANDLE);
