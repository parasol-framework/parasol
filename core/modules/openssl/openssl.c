/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-MODULE-
OpenSSL: Exports functions from the OpenSSL library.

-END-

OPENSSL SUPPORT
---------------
OpenSSL support is enabled by compiling the libssl and libcrypto libraries from source and installing them to
modules:lib/.

??? Windows: The DLL's 'libeay32.dll' and 'ssleay32.dll' must be stored in system:modules/lib/

*****************************************************************************/

//#define DEBUG

#define PRV_OPENSSL

#include <parasol/main.h>
#include <parasol/modules/openssl.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

#undef OPENSSL_SYS_WINDOWS
#undef OPENSSL_SYS_WIN32
#include <openssl/rand.h>

MODULE_COREBASE;
static OBJECTPTR glModule = NULL;

//****************************************************************************

static struct FunctionField argsGenerateRSAKey[] = {
   { "Error", FD_ERROR }, { "TotalBits", FD_LONG }, { "Password", FD_STR }, { "PrivateKey", FD_RESULT|FD_STR|FD_ALLOC }, { "PublicKey", FD_RESULT|FD_STR|FD_ALLOC },
   { NULL, 0 } };

static struct FunctionField argsGenerateRSAPublicKey[] = {
   { "Error", FD_ERROR }, { "PrivateKey", FD_STR }, { "Password", FD_STR }, { "PublicKey", FD_RESULT|FD_STR|FD_ALLOC },
   { NULL, 0 } };

static struct FunctionField argsCalcSigFromObject[] = {
   { "Error", FD_ERROR }, { "Source", FD_OBJECTPTR }, { "SrcLength", FD_LONG }, { "PrivateKey", FD_STR }, { "Password", FD_STR }, { "Digest", FD_STR }, { "Signature", FD_RESULT|FD_PTR|FD_ALLOC }, { "SigSize", FD_RESULT|FD_BUFSIZE|FD_LONG },
   { NULL, 0 } };

static struct FunctionField argsVerifySig[] = {
   { "Error", FD_ERROR }, { "Source", FD_OBJECTPTR }, { "SrcLength", FD_LONG }, { "PublicKey", FD_STR }, { "Digest", FD_STR }, { "Signature", FD_BUFFER|FD_PTR }, { "SigLength", FD_LONG|FD_BUFSIZE },
   { NULL, 0 } };

static struct Function JumpTableV1[] = {
   { sslGenerateRSAKey,       "GenerateRSAKey",    argsGenerateRSAKey },
   { sslCalcSigFromObject,    "CalcSigFromObject", argsCalcSigFromObject },
   { sslVerifySig,            "VerifySig",         argsVerifySig },
   { sslGenerateRSAPublicKey, "GenerateRSAPublicKey", argsGenerateRSAPublicKey },

   { SSL_CTX_new, "SSL_CTX_new", NULL },
   { SSLv23_client_method, "SSLv23_client_method", NULL },
   { SSL_new, "SSL_new", NULL },
   { BIO_f_ssl, "BIO_f_ssl", NULL },
   { BIO_new_buffer_ssl_connect, "BIO_new_buffer_ssl_connect", NULL },
   { BIO_new_ssl, "BIO_new_ssl", NULL },
   { BIO_new_ssl_connect, "BIO_new_ssl_connect", NULL },
   { BIO_s_connect, "BIO_s_connect", NULL },
   { BIO_s_file, "BIO_s_file", NULL },
   { BIO_s_socket, "BIO_s_socket", NULL },
   { BIO_ssl_copy_session_id, "BIO_ssl_copy_session_id", NULL },
   { BIO_ssl_shutdown, "BIO_ssl_shutdown", NULL },
   { DTLSv1_client_method, "DTLSv1_client_method", NULL },
   { DTLSv1_method, "DTLSv1_method", NULL },
   { DTLSv1_server_method, "DTLSv1_server_method", NULL },
   { SSL_CIPHER_description, "SSL_CIPHER_description", NULL },
   { SSL_CIPHER_get_bits, "SSL_CIPHER_get_bits", NULL },
   { SSL_CIPHER_get_name, "SSL_CIPHER_get_name", NULL },
   { SSL_CIPHER_get_version, "SSL_CIPHER_get_version", NULL },
   { SSL_COMP_add_compression_method, "SSL_COMP_add_compression_method", NULL },
   { SSL_COMP_get_compression_methods, "SSL_COMP_get_compression_methods", NULL },
   { SSL_COMP_get_name, "SSL_COMP_get_name", NULL },
   { SSL_CTX_add_client_CA, "SSL_CTX_add_client_CA", NULL },
   { SSL_CTX_add_session, "SSL_CTX_add_session", NULL },
   { SSL_CTX_callback_ctrl, "SSL_CTX_callback_ctrl", NULL },
   { SSL_CTX_check_private_key, "SSL_CTX_check_private_key", NULL },
   { SSL_CTX_ctrl, "SSL_CTX_ctrl", NULL },
   { SSL_CTX_flush_sessions, "SSL_CTX_flush_sessions", NULL },
   { SSL_CTX_free, "SSL_CTX_free", NULL },
   { SSL_CTX_get_cert_store, "SSL_CTX_get_cert_store", NULL },
   { SSL_CTX_get_client_CA_list, "SSL_CTX_get_client_CA_list", NULL },
   { SSL_CTX_get_client_cert_cb, "SSL_CTX_get_client_cert_cb", NULL },
   { SSL_CTX_get_ex_data, "SSL_CTX_get_ex_data", NULL },
   { SSL_CTX_get_ex_new_index, "SSL_CTX_get_ex_new_index", NULL },
   { SSL_CTX_get_info_callback, "SSL_CTX_get_info_callback", NULL },
   { SSL_CTX_get_quiet_shutdown, "SSL_CTX_get_quiet_shutdown", NULL },
   { SSL_CTX_get_timeout, "SSL_CTX_get_timeout", NULL },
   { SSL_CTX_get_verify_callback, "SSL_CTX_get_verify_callback", NULL },
   { SSL_CTX_get_verify_depth, "SSL_CTX_get_verify_depth", NULL },
   { SSL_CTX_get_verify_mode, "SSL_CTX_get_verify_mode", NULL },
   { SSL_CTX_load_verify_locations, "SSL_CTX_load_verify_locations", NULL },
   { SSL_CTX_remove_session, "SSL_CTX_remove_session", NULL },
   { SSL_CTX_sess_get_get_cb, "SSL_CTX_sess_get_get_cb", NULL },
   { SSL_CTX_sess_get_new_cb, "SSL_CTX_sess_get_new_cb", NULL },
   { SSL_CTX_sess_get_remove_cb, "SSL_CTX_sess_get_remove_cb", NULL },
   { SSL_CTX_sess_set_get_cb, "SSL_CTX_sess_set_get_cb", NULL },
   { SSL_CTX_sess_set_new_cb, "SSL_CTX_sess_set_new_cb", NULL },
   { SSL_CTX_sess_set_remove_cb, "SSL_CTX_sess_set_remove_cb", NULL },
   { SSL_CTX_sessions, "SSL_CTX_sessions", NULL },
   { SSL_CTX_set_cert_store, "SSL_CTX_set_cert_store", NULL },
   { SSL_CTX_set_cert_verify_callback, "SSL_CTX_set_cert_verify_callback", NULL },
   { SSL_CTX_set_cipher_list, "SSL_CTX_set_cipher_list", NULL },
   { SSL_CTX_set_client_CA_list, "SSL_CTX_set_client_CA_list", NULL },
   { SSL_CTX_set_client_cert_cb, "SSL_CTX_set_client_cert_cb", NULL },
   { SSL_CTX_set_cookie_generate_cb, "SSL_CTX_set_cookie_generate_cb", NULL },
   { SSL_CTX_set_cookie_verify_cb, "SSL_CTX_set_cookie_verify_cb", NULL },
   { SSL_CTX_set_default_passwd_cb, "SSL_CTX_set_default_passwd_cb", NULL },
   { SSL_CTX_set_default_passwd_cb_userdata, "SSL_CTX_set_default_passwd_cb_userdata", NULL },
   { SSL_CTX_set_default_verify_paths, "SSL_CTX_set_default_verify_paths", NULL },
   { SSL_CTX_set_ex_data, "SSL_CTX_set_ex_data", NULL },
   { SSL_CTX_set_generate_session_id, "SSL_CTX_set_generate_session_id", NULL },
   { SSL_CTX_set_info_callback, "SSL_CTX_set_info_callback", NULL },
   { SSL_CTX_set_msg_callback, "SSL_CTX_set_msg_callback", NULL },
   { SSL_CTX_set_purpose, "SSL_CTX_set_purpose", NULL },
   { SSL_CTX_set_quiet_shutdown, "SSL_CTX_set_quiet_shutdown", NULL },
   { SSL_CTX_set_session_id_context, "SSL_CTX_set_session_id_context", NULL },
   { SSL_CTX_set_ssl_version, "SSL_CTX_set_ssl_version", NULL },
   { SSL_CTX_set_timeout, "SSL_CTX_set_timeout", NULL },
   { SSL_CTX_set_tmp_dh_callback, "SSL_CTX_set_tmp_dh_callback", NULL },
   { SSL_CTX_set_tmp_rsa_callback, "SSL_CTX_set_tmp_rsa_callback", NULL },
   { SSL_CTX_set_trust, "SSL_CTX_set_trust", NULL },
   { SSL_CTX_set_verify, "SSL_CTX_set_verify", NULL },
   { SSL_CTX_set_verify_depth, "SSL_CTX_set_verify_depth", NULL },
   { SSL_CTX_use_PrivateKey, "SSL_CTX_use_PrivateKey", NULL },
   { SSL_CTX_use_PrivateKey_ASN1, "SSL_CTX_use_PrivateKey_ASN1", NULL },
   { SSL_CTX_use_PrivateKey_file, "SSL_CTX_use_PrivateKey_file", NULL },
   { SSL_CTX_use_RSAPrivateKey, "SSL_CTX_use_RSAPrivateKey", NULL },
   { SSL_CTX_use_RSAPrivateKey_ASN1, "SSL_CTX_use_RSAPrivateKey_ASN1", NULL },
   { SSL_CTX_use_RSAPrivateKey_file, "SSL_CTX_use_RSAPrivateKey_file", NULL },
   { SSL_CTX_use_certificate, "SSL_CTX_use_certificate", NULL },
   { SSL_CTX_use_certificate_ASN1, "SSL_CTX_use_certificate_ASN1", NULL },
   { SSL_CTX_use_certificate_chain_file, "SSL_CTX_use_certificate_chain_file", NULL },
   { SSL_CTX_use_certificate_file, "SSL_CTX_use_certificate_file", NULL },
   //{ SSL_SESSION_cmp, "SSL_SESSION_cmp", NULL },
   { SSL_SESSION_free, "SSL_SESSION_free", NULL },
   { SSL_SESSION_get_ex_data, "SSL_SESSION_get_ex_data", NULL },
   { SSL_SESSION_get_ex_new_index, "SSL_SESSION_get_ex_new_index", NULL },
   { SSL_SESSION_get_id, "SSL_SESSION_get_id", NULL },
   { SSL_SESSION_get_time, "SSL_SESSION_get_time", NULL },
   { SSL_SESSION_get_timeout, "SSL_SESSION_get_timeout", NULL },
   //{ SSL_SESSION_hash, "SSL_SESSION_hash", NULL },
   { SSL_SESSION_new, "SSL_SESSION_new", NULL },
   { SSL_SESSION_print, "SSL_SESSION_print", NULL },
   { SSL_SESSION_print_fp, "SSL_SESSION_print_fp", NULL },
   { SSL_SESSION_set_ex_data, "SSL_SESSION_set_ex_data", NULL },
   { SSL_SESSION_set_time, "SSL_SESSION_set_time", NULL },
   { SSL_SESSION_set_timeout, "SSL_SESSION_set_timeout", NULL },
   { SSL_accept, "SSL_accept", NULL },
   { SSL_add_client_CA, "SSL_add_client_CA", NULL },
   { SSL_add_dir_cert_subjects_to_stack, "SSL_add_dir_cert_subjects_to_stack", NULL },
   { SSL_add_file_cert_subjects_to_stack, "SSL_add_file_cert_subjects_to_stack", NULL },
   { SSL_alert_desc_string, "SSL_alert_desc_string", NULL },
   { SSL_alert_desc_string_long, "SSL_alert_desc_string_long", NULL },
   { SSL_alert_type_string, "SSL_alert_type_string", NULL },
   { SSL_alert_type_string_long, "SSL_alert_type_string_long", NULL },
   { SSL_callback_ctrl, "SSL_callback_ctrl", NULL },
   { SSL_check_private_key, "SSL_check_private_key", NULL },
   { SSL_clear, "SSL_clear", NULL },
   { SSL_connect, "SSL_connect", NULL },
   { SSL_copy_session_id, "SSL_copy_session_id", NULL },
   { SSL_ctrl, "SSL_ctrl", NULL },
   { SSL_do_handshake, "SSL_do_handshake", NULL },
   { SSL_dup, "SSL_dup", NULL },
   { SSL_dup_CA_list, "SSL_dup_CA_list", NULL },
   { SSL_free, "SSL_free", NULL },
   { SSL_get1_session, "SSL_get1_session", NULL },
   { SSL_get_SSL_CTX, "SSL_get_SSL_CTX", NULL },
   { SSL_get_certificate, "SSL_get_certificate", NULL },
   { SSL_get_cipher_list, "SSL_get_cipher_list", NULL },
   { SSL_get_ciphers, "SSL_get_ciphers", NULL },
   { SSL_get_client_CA_list, "SSL_get_client_CA_list", NULL },
   { SSL_get_current_cipher, "SSL_get_current_cipher", NULL },
   { SSL_get_current_compression, "SSL_get_current_compression", NULL },
   { SSL_get_current_expansion, "SSL_get_current_expansion", NULL },
   { SSL_get_default_timeout, "SSL_get_default_timeout", NULL },
   { SSL_get_error, "SSL_get_error", NULL },
   { SSL_get_ex_data, "SSL_get_ex_data", NULL },
   { SSL_get_ex_data_X509_STORE_CTX_idx, "SSL_get_ex_data_X509_STORE_CTX_idx", NULL },
   { SSL_get_ex_new_index, "SSL_get_ex_new_index", NULL },
   { SSL_get_fd, "SSL_get_fd", NULL },
   { SSL_get_finished, "SSL_get_finished", NULL },
   { SSL_get_info_callback, "SSL_get_info_callback", NULL },
   { SSL_get_peer_cert_chain, "SSL_get_peer_cert_chain", NULL },
   { SSL_get_peer_certificate, "SSL_get_peer_certificate", NULL },
   { SSL_get_peer_finished, "SSL_get_peer_finished", NULL },
   { SSL_get_privatekey, "SSL_get_privatekey", NULL },
   { SSL_get_quiet_shutdown, "SSL_get_quiet_shutdown", NULL },
   { SSL_get_rbio, "SSL_get_rbio", NULL },
   { SSL_get_read_ahead, "SSL_get_read_ahead", NULL },
   { SSL_get_rfd, "SSL_get_rfd", NULL },
   { SSL_get_session, "SSL_get_session", NULL },
   { SSL_get_shared_ciphers, "SSL_get_shared_ciphers", NULL },
   { SSL_get_shutdown, "SSL_get_shutdown", NULL },
   { SSL_get_ssl_method, "SSL_get_ssl_method", NULL },
   { SSL_get_verify_callback, "SSL_get_verify_callback", NULL },
   { SSL_get_verify_depth, "SSL_get_verify_depth", NULL },
   { SSL_get_verify_mode, "SSL_get_verify_mode", NULL },
   { SSL_get_verify_result, "SSL_get_verify_result", NULL },
   { SSL_get_version, "SSL_get_version", NULL },
   { SSL_get_wbio, "SSL_get_wbio", NULL },
   { SSL_get_wfd, "SSL_get_wfd", NULL },
   { SSL_has_matching_session_id, "SSL_has_matching_session_id", NULL },
   { SSL_library_init, "SSL_library_init", NULL },
   { SSL_load_client_CA_file, "SSL_load_client_CA_file", NULL },
   { SSL_load_error_strings, "SSL_load_error_strings", NULL },
   { SSL_peek, "SSL_peek", NULL },
   { SSL_pending, "SSL_pending", NULL },
   { SSL_read, "SSL_read", NULL },
   { SSL_renegotiate, "SSL_renegotiate", NULL },
   { SSL_renegotiate_pending, "SSL_renegotiate_pending", NULL },
   { SSL_rstate_string, "SSL_rstate_string", NULL },
   { SSL_rstate_string_long, "SSL_rstate_string_long", NULL },
   { SSL_set_SSL_CTX, "SSL_set_SSL_CTX", NULL },
   { SSL_set_accept_state, "SSL_set_accept_state", NULL },
   { SSL_set_bio, "SSL_set_bio", NULL },
   { SSL_set_cipher_list, "SSL_set_cipher_list", NULL },
   { SSL_set_client_CA_list, "SSL_set_client_CA_list", NULL },
   { SSL_set_connect_state, "SSL_set_connect_state", NULL },
   { SSL_set_ex_data, "SSL_set_ex_data", NULL },
   { SSL_set_fd, "SSL_set_fd", NULL },
   { SSL_set_generate_session_id, "SSL_set_generate_session_id", NULL },
   { SSL_set_info_callback, "SSL_set_info_callback", NULL },
   { SSL_set_msg_callback, "SSL_set_msg_callback", NULL },
   { SSL_set_purpose, "SSL_set_purpose", NULL },
   { SSL_set_quiet_shutdown, "SSL_set_quiet_shutdown", NULL },
   { SSL_set_read_ahead, "SSL_set_read_ahead", NULL },
   { SSL_set_rfd, "SSL_set_rfd", NULL },
   { SSL_set_session, "SSL_set_session", NULL },
   { SSL_set_session_id_context, "SSL_set_session_id_context", NULL },
   { SSL_set_shutdown, "SSL_set_shutdown", NULL },
   { SSL_set_ssl_method, "SSL_set_ssl_method", NULL },
   { SSL_set_tmp_dh_callback, "SSL_set_tmp_dh_callback", NULL },
   { SSL_set_tmp_rsa_callback, "SSL_set_tmp_rsa_callback", NULL },
   { SSL_set_trust, "SSL_set_trust", NULL },
   { SSL_set_verify, "SSL_set_verify", NULL },
   { SSL_set_verify_depth, "SSL_set_verify_depth", NULL },
   { SSL_set_verify_result, "SSL_set_verify_result", NULL },
   { SSL_set_wfd, "SSL_set_wfd", NULL },
   { SSL_shutdown, "SSL_shutdown", NULL },
   { SSL_state, "SSL_state", NULL },
   { SSL_state_string, "SSL_state_string", NULL },
   { SSL_state_string_long, "SSL_state_string_long", NULL },
   { SSL_use_PrivateKey, "SSL_use_PrivateKey", NULL },
   { SSL_use_PrivateKey_ASN1, "SSL_use_PrivateKey_ASN1", NULL },
   { SSL_use_PrivateKey_file, "SSL_use_PrivateKey_file", NULL },
   { SSL_use_RSAPrivateKey, "SSL_use_RSAPrivateKey", NULL },
   { SSL_use_RSAPrivateKey_ASN1, "SSL_use_RSAPrivateKey_ASN1", NULL },
   { SSL_use_RSAPrivateKey_file, "SSL_use_RSAPrivateKey_file", NULL },
   { SSL_use_certificate, "SSL_use_certificate", NULL },
   { SSL_use_certificate_ASN1, "SSL_use_certificate_ASN1", NULL },
   { SSL_use_certificate_file, "SSL_use_certificate_file", NULL },
   { SSL_version, "SSL_version", NULL },
   { SSL_want, "SSL_want", NULL },
   { SSL_write, "SSL_write", NULL },
   { SSLv23_method, "SSLv23_method", NULL },
   { SSLv23_server_method, "SSLv23_server_method", NULL },
   //{ SSLv2_client_method, "SSLv2_client_method", NULL },
   //{ SSLv2_method, "SSLv2_method", NULL },
   //{ SSLv2_server_method, "SSLv2_server_method", NULL },
   { SSLv3_client_method, "SSLv3_client_method", NULL },
   { SSLv3_method, "SSLv3_method", NULL },
   { SSLv3_server_method, "SSLv3_server_method", NULL },
   { TLSv1_client_method, "TLSv1_client_method", NULL },
   { TLSv1_method, "TLSv1_method", NULL },
   { TLSv1_server_method, "TLSv1_server_method", NULL },
   { X509_NAME_cmp, "X509_NAME_cmp", NULL },
   { X509_NAME_dup, "X509_NAME_dup", NULL },
   { X509_NAME_free, "X509_NAME_free", NULL },
   { X509_STORE_CTX_cleanup, "X509_STORE_CTX_cleanup", NULL },
   { X509_STORE_CTX_get0_param, "X509_STORE_CTX_get0_param", NULL },
   { X509_STORE_CTX_get_ex_new_index, "X509_STORE_CTX_get_ex_new_index", NULL },
   { X509_STORE_CTX_init, "X509_STORE_CTX_init", NULL },
   { X509_STORE_CTX_set_default, "X509_STORE_CTX_set_default", NULL },
   { X509_STORE_CTX_set_ex_data, "X509_STORE_CTX_set_ex_data", NULL },
   { X509_STORE_CTX_set_verify_cb, "X509_STORE_CTX_set_verify_cb", NULL },
   { X509_STORE_free, "X509_STORE_free", NULL },
   { X509_STORE_get_by_subject, "X509_STORE_get_by_subject", NULL },
   { X509_STORE_load_locations, "X509_STORE_load_locations", NULL },
   { X509_STORE_new, "X509_STORE_new", NULL },
   { X509_STORE_set_default_paths, "X509_STORE_set_default_paths", NULL },
   { X509_VERIFY_PARAM_free, "X509_VERIFY_PARAM_free", NULL },
   { X509_VERIFY_PARAM_get_depth, "X509_VERIFY_PARAM_get_depth", NULL },
   { X509_VERIFY_PARAM_inherit, "X509_VERIFY_PARAM_inherit", NULL },
   { X509_VERIFY_PARAM_new, "X509_VERIFY_PARAM_new", NULL },
   { X509_VERIFY_PARAM_set_depth, "X509_VERIFY_PARAM_set_depth", NULL },
   { X509_VERIFY_PARAM_set_purpose, "X509_VERIFY_PARAM_set_purpose", NULL },
   { X509_VERIFY_PARAM_set_trust, "X509_VERIFY_PARAM_set_trust", NULL },
   /*** Crypto ***/
   { ASN1_add_oid_module, "ASN1_add_oid_module", NULL },
   { ASN1_check_infinite_end, "ASN1_check_infinite_end", NULL },
   { ASN1_const_check_infinite_end, "ASN1_const_check_infinite_end", NULL },
   { ASN1_d2i_bio, "ASN1_d2i_bio", NULL },
   { ASN1_d2i_fp, "ASN1_d2i_fp", NULL },
   { ASN1_digest, "ASN1_digest", NULL },
   { ASN1_dup, "ASN1_dup", NULL },
   { ASN1_generate_nconf, "ASN1_generate_nconf", NULL },
   { ASN1_generate_v3, "ASN1_generate_v3", NULL },
   { ASN1_get_object, "ASN1_get_object", NULL },
   { ASN1_i2d_bio, "ASN1_i2d_bio", NULL },
   { ASN1_i2d_fp, "ASN1_i2d_fp", NULL },
   { ASN1_item_d2i, "ASN1_item_d2i", NULL },
   { ASN1_item_d2i_bio, "ASN1_item_d2i_bio", NULL },
   { ASN1_item_d2i_fp, "ASN1_item_d2i_fp", NULL },
   { ASN1_item_digest, "ASN1_item_digest", NULL },
   { ASN1_item_dup, "ASN1_item_dup", NULL },
   { ASN1_item_free, "ASN1_item_free", NULL },
   { ASN1_item_i2d, "ASN1_item_i2d", NULL },
   { ASN1_item_i2d_bio, "ASN1_item_i2d_bio", NULL },
   { ASN1_item_i2d_fp, "ASN1_item_i2d_fp", NULL },
   { ASN1_item_ndef_i2d, "ASN1_item_ndef_i2d", NULL },
   { ASN1_item_new, "ASN1_item_new", NULL },
   { ASN1_item_pack, "ASN1_item_pack", NULL },
   { ASN1_item_sign, "ASN1_item_sign", NULL },
   { ASN1_item_unpack, "ASN1_item_unpack", NULL },
   { ASN1_item_verify, "ASN1_item_verify", NULL },
   { ASN1_mbstring_copy, "ASN1_mbstring_copy", NULL },
   { ASN1_mbstring_ncopy, "ASN1_mbstring_ncopy", NULL },
   { ASN1_object_size, "ASN1_object_size", NULL },
   { ASN1_pack_string, "ASN1_pack_string", NULL },
   { ASN1_parse, "ASN1_parse", NULL },
   { ASN1_parse_dump, "ASN1_parse_dump", NULL },
   { ASN1_put_eoc, "ASN1_put_eoc", NULL },
   { ASN1_put_object, "ASN1_put_object", NULL },
   { ASN1_seq_pack, "ASN1_seq_pack", NULL },
   { ASN1_seq_unpack, "ASN1_seq_unpack", NULL },
   { ASN1_sign, "ASN1_sign", NULL },
   { ASN1_tag2bit, "ASN1_tag2bit", NULL },
   { ASN1_tag2str, "ASN1_tag2str", NULL },
   { ASN1_unpack_string, "ASN1_unpack_string", NULL },
   { ASN1_verify, "ASN1_verify", NULL },
   { BIO_accept, "BIO_accept", NULL },
   { BIO_callback_ctrl, "BIO_callback_ctrl", NULL },
   { BIO_clear_flags, "BIO_clear_flags", NULL },
   { BIO_copy_next_retry, "BIO_copy_next_retry", NULL },
   { BIO_ctrl, "BIO_ctrl", NULL },
   { BIO_ctrl_get_read_request, "BIO_ctrl_get_read_request", NULL },
   { BIO_ctrl_get_write_guarantee, "BIO_ctrl_get_write_guarantee", NULL },
   { BIO_ctrl_pending, "BIO_ctrl_pending", NULL },
   { BIO_ctrl_reset_read_request, "BIO_ctrl_reset_read_request", NULL },
   { BIO_ctrl_wpending, "BIO_ctrl_wpending", NULL },
   { BIO_debug_callback, "BIO_debug_callback", NULL },
   { BIO_dump, "BIO_dump", NULL },
   { BIO_dump_cb, "BIO_dump_cb", NULL },
   { BIO_dump_fp, "BIO_dump_fp", NULL },
   { BIO_dump_indent, "BIO_dump_indent", NULL },
   { BIO_dump_indent_cb, "BIO_dump_indent_cb", NULL },
   { BIO_dump_indent_fp, "BIO_dump_indent_fp", NULL },
   { BIO_dup_chain, "BIO_dup_chain", NULL },
   { BIO_f_base64, "BIO_f_base64", NULL },
   { BIO_f_buffer, "BIO_f_buffer", NULL },
   { BIO_f_cipher, "BIO_f_cipher", NULL },
   { BIO_f_md, "BIO_f_md", NULL },
   { BIO_f_nbio_test, "BIO_f_nbio_test", NULL },
   { BIO_f_null, "BIO_f_null", NULL },
   { BIO_f_reliable, "BIO_f_reliable", NULL },
   { BIO_fd_non_fatal_error, "BIO_fd_non_fatal_error", NULL },
   { BIO_fd_should_retry, "BIO_fd_should_retry", NULL },
   { BIO_find_type, "BIO_find_type", NULL },
   { BIO_free, "BIO_free", NULL },
   { BIO_free_all, "BIO_free_all", NULL },
   { BIO_get_accept_socket, "BIO_get_accept_socket", NULL },
   { BIO_get_callback, "BIO_get_callback", NULL },
   { BIO_get_callback_arg, "BIO_get_callback_arg", NULL },
   { BIO_get_ex_data, "BIO_get_ex_data", NULL },
   { BIO_get_ex_new_index, "BIO_get_ex_new_index", NULL },
   { BIO_get_host_ip, "BIO_get_host_ip", NULL },
   { BIO_get_port, "BIO_get_port", NULL },
   { BIO_get_retry_BIO, "BIO_get_retry_BIO", NULL },
   { BIO_get_retry_reason, "BIO_get_retry_reason", NULL },
   { BIO_gethostbyname, "BIO_gethostbyname", NULL },
   { BIO_gets, "BIO_gets", NULL },
   { BIO_indent, "BIO_indent", NULL },
   { BIO_int_ctrl, "BIO_int_ctrl", NULL },
   { BIO_method_name, "BIO_method_name", NULL },
   { BIO_method_type, "BIO_method_type", NULL },
   { BIO_new, "BIO_new", NULL },
   { BIO_new_accept, "BIO_new_accept", NULL },
   { BIO_new_bio_pair, "BIO_new_bio_pair", NULL },
   { BIO_new_connect, "BIO_new_connect", NULL },
   { BIO_new_dgram, "BIO_new_dgram", NULL },
   { BIO_new_fd, "BIO_new_fd", NULL },
   { BIO_new_file, "BIO_new_file", NULL },
   { BIO_new_fp, "BIO_new_fp", NULL },
   { BIO_new_mem_buf, "BIO_new_mem_buf", NULL },
   { BIO_new_socket, "BIO_new_socket", NULL },
   { BIO_next, "BIO_next", NULL },
   { BIO_nread, "BIO_nread", NULL },
   { BIO_number_read, "BIO_number_read", NULL },
   { BIO_number_written, "BIO_number_written", NULL },
   { BIO_nwrite, "BIO_nwrite", NULL },
   { BIO_pop, "BIO_pop", NULL },
   { BIO_printf, "BIO_printf", NULL },
   { BIO_ptr_ctrl, "BIO_ptr_ctrl", NULL },
   { BIO_push, "BIO_push", NULL },
   { BIO_puts, "BIO_puts", NULL },
   { BIO_read, "BIO_read", NULL },
   { BIO_set, "BIO_set", NULL },
   { BIO_set_callback, "BIO_set_callback", NULL },
   { BIO_set_callback_arg, "BIO_set_callback_arg", NULL },
   { BIO_set_cipher, "BIO_set_cipher", NULL },
   { BIO_set_ex_data, "BIO_set_ex_data", NULL },
   { BIO_set_flags, "BIO_set_flags", NULL },
   { BIO_set_tcp_ndelay, "BIO_set_tcp_ndelay", NULL },
   { BIO_snprintf, "BIO_snprintf", NULL },
   { BIO_sock_cleanup, "BIO_sock_cleanup", NULL },
   { BIO_sock_error, "BIO_sock_error", NULL },
   { BIO_sock_init, "BIO_sock_init", NULL },
   { BIO_sock_non_fatal_error, "BIO_sock_non_fatal_error", NULL },
   { BIO_sock_should_retry, "BIO_sock_should_retry", NULL },
   { BIO_socket_ioctl, "BIO_socket_ioctl", NULL },
   { BIO_socket_nbio, "BIO_socket_nbio", NULL },
   { BIO_test_flags, "BIO_test_flags", NULL },
   { BIO_vfree, "BIO_vfree", NULL },
   { BIO_vprintf, "BIO_vprintf", NULL },
   { BIO_vsnprintf, "BIO_vsnprintf", NULL },
   { BIO_write, "BIO_write", NULL },
   { BN_CTX_end, "BN_CTX_end", NULL },
   { BN_CTX_free, "BN_CTX_free", NULL },
   { BN_CTX_get, "BN_CTX_get", NULL },
   { BN_CTX_init, "BN_CTX_init", NULL },
   { BN_CTX_new, "BN_CTX_new", NULL },
   { BN_CTX_start, "BN_CTX_start", NULL },
   { BN_GENCB_call, "BN_GENCB_call", NULL },
   { BN_GF2m_add, "BN_GF2m_add", NULL },
   { BN_GF2m_arr2poly, "BN_GF2m_arr2poly", NULL },
   { BN_GF2m_mod, "BN_GF2m_mod", NULL },
   { BN_GF2m_mod_arr, "BN_GF2m_mod_arr", NULL },
   { BN_GF2m_mod_div, "BN_GF2m_mod_div", NULL },
   { BN_GF2m_mod_div_arr, "BN_GF2m_mod_div_arr", NULL },
   { BN_GF2m_mod_exp, "BN_GF2m_mod_exp", NULL },
   { BN_GF2m_mod_exp_arr, "BN_GF2m_mod_exp_arr", NULL },
   { BN_GF2m_mod_inv, "BN_GF2m_mod_inv", NULL },
   { BN_GF2m_mod_inv_arr, "BN_GF2m_mod_inv_arr", NULL },
   { BN_GF2m_mod_mul, "BN_GF2m_mod_mul", NULL },
   { BN_GF2m_mod_mul_arr, "BN_GF2m_mod_mul_arr", NULL },
   { BN_GF2m_mod_solve_quad, "BN_GF2m_mod_solve_quad", NULL },
   { BN_GF2m_mod_solve_quad_arr, "BN_GF2m_mod_solve_quad_arr", NULL },
   { BN_GF2m_mod_sqr, "BN_GF2m_mod_sqr", NULL },
   { BN_GF2m_mod_sqr_arr, "BN_GF2m_mod_sqr_arr", NULL },
   { BN_GF2m_mod_sqrt, "BN_GF2m_mod_sqrt", NULL },
   { BN_GF2m_mod_sqrt_arr, "BN_GF2m_mod_sqrt_arr", NULL },
   { BN_GF2m_poly2arr, "BN_GF2m_poly2arr", NULL },
   { BN_add, "BN_add", NULL },
   { BN_add_word, "BN_add_word", NULL },
   { BN_bin2bn, "BN_bin2bn", NULL },
   { BN_bn2bin, "BN_bn2bin", NULL },
   { BN_bn2dec, "BN_bn2dec", NULL },
   { BN_bn2hex, "BN_bn2hex", NULL },
   { BN_bn2mpi, "BN_bn2mpi", NULL },
   { BN_bntest_rand, "BN_bntest_rand", NULL },
   { BN_clear, "BN_clear", NULL },
   { BN_clear_bit, "BN_clear_bit", NULL },
   { BN_clear_free, "BN_clear_free", NULL },
   { BN_cmp, "BN_cmp", NULL },
   { BN_copy, "BN_copy", NULL },
   { BN_dec2bn, "BN_dec2bn", NULL },
   { BN_div, "BN_div", NULL },
   { BN_div_recp, "BN_div_recp", NULL },
   { BN_div_word, "BN_div_word", NULL },
   { BN_dup, "BN_dup", NULL },
   { BN_exp, "BN_exp", NULL },
   { BN_free, "BN_free", NULL },
   { BN_from_montgomery, "BN_from_montgomery", NULL },
   { BN_gcd, "BN_gcd", NULL },
   { BN_generate_prime, "BN_generate_prime", NULL },
   { BN_generate_prime_ex, "BN_generate_prime_ex", NULL },
   { BN_get0_nist_prime_192, "BN_get0_nist_prime_192", NULL },
   { BN_get0_nist_prime_224, "BN_get0_nist_prime_224", NULL },
   { BN_get0_nist_prime_256, "BN_get0_nist_prime_256", NULL },
   { BN_get0_nist_prime_384, "BN_get0_nist_prime_384", NULL },
   { BN_get0_nist_prime_521, "BN_get0_nist_prime_521", NULL },
   { BN_get_params, "BN_get_params", NULL },
   { BN_get_word, "BN_get_word", NULL },
   { BN_hex2bn, "BN_hex2bn", NULL },
   { BN_init, "BN_init", NULL },
   { BN_is_bit_set, "BN_is_bit_set", NULL },
   { BN_is_prime, "BN_is_prime", NULL },
   { BN_is_prime_ex, "BN_is_prime_ex", NULL },
   { BN_is_prime_fasttest, "BN_is_prime_fasttest", NULL },
   { BN_is_prime_fasttest_ex, "BN_is_prime_fasttest_ex", NULL },
   { BN_kronecker, "BN_kronecker", NULL },
   { BN_lshift, "BN_lshift", NULL },
   { BN_lshift1, "BN_lshift1", NULL },
   { BN_mask_bits, "BN_mask_bits", NULL },
   { BN_mod_add, "BN_mod_add", NULL },
   { BN_mod_add_quick, "BN_mod_add_quick", NULL },
   { BN_mod_exp, "BN_mod_exp", NULL },
   { BN_mod_exp2_mont, "BN_mod_exp2_mont", NULL },
   { BN_mod_exp_mont, "BN_mod_exp_mont", NULL },
   { BN_mod_exp_mont_consttime, "BN_mod_exp_mont_consttime", NULL },
   { BN_mod_exp_mont_word, "BN_mod_exp_mont_word", NULL },
   { BN_mod_exp_recp, "BN_mod_exp_recp", NULL },
   { BN_mod_exp_simple, "BN_mod_exp_simple", NULL },
   { BN_mod_inverse, "BN_mod_inverse", NULL },
   { BN_mod_lshift, "BN_mod_lshift", NULL },
   { BN_mod_lshift1, "BN_mod_lshift1", NULL },
   { BN_mod_lshift1_quick, "BN_mod_lshift1_quick", NULL },
   { BN_mod_lshift_quick, "BN_mod_lshift_quick", NULL },
   { BN_mod_mul, "BN_mod_mul", NULL },
   { BN_mod_mul_montgomery, "BN_mod_mul_montgomery", NULL },
   { BN_mod_mul_reciprocal, "BN_mod_mul_reciprocal", NULL },
   { BN_mod_sqr, "BN_mod_sqr", NULL },
   { BN_mod_sqrt, "BN_mod_sqrt", NULL },
   { BN_mod_sub, "BN_mod_sub", NULL },
   { BN_mod_sub_quick, "BN_mod_sub_quick", NULL },
   { BN_mod_word, "BN_mod_word", NULL },
   { BN_mpi2bn, "BN_mpi2bn", NULL },
   { BN_mul, "BN_mul", NULL },
   { BN_mul_word, "BN_mul_word", NULL },
   { BN_new, "BN_new", NULL },
   { BN_nist_mod_192, "BN_nist_mod_192", NULL },
   { BN_nist_mod_224, "BN_nist_mod_224", NULL },
   { BN_nist_mod_256, "BN_nist_mod_256", NULL },
   { BN_nist_mod_384, "BN_nist_mod_384", NULL },
   { BN_nist_mod_521, "BN_nist_mod_521", NULL },
   { BN_nnmod, "BN_nnmod", NULL },
   { BN_num_bits, "BN_num_bits", NULL },
   { BN_num_bits_word, "BN_num_bits_word", NULL },
   { BN_options, "BN_options", NULL },
   { BN_print, "BN_print", NULL },
   { BN_print_fp, "BN_print_fp", NULL },
   { BN_pseudo_rand, "BN_pseudo_rand", NULL },
   { BN_pseudo_rand_range, "BN_pseudo_rand_range", NULL },
   { BN_rand, "BN_rand", NULL },
   { BN_rand_range, "BN_rand_range", NULL },
   { BN_reciprocal, "BN_reciprocal", NULL },
   { BN_rshift, "BN_rshift", NULL },
   { BN_rshift1, "BN_rshift1", NULL },
   { BN_set_bit, "BN_set_bit", NULL },
   { BN_set_negative, "BN_set_negative", NULL },
   { BN_set_params, "BN_set_params", NULL },
   { BN_set_word, "BN_set_word", NULL },
   { BN_sqr, "BN_sqr", NULL },
   { BN_sub, "BN_sub", NULL },
   { BN_sub_word, "BN_sub_word", NULL },
   { BN_swap, "BN_swap", NULL },
   { BN_to_ASN1_ENUMERATED, "BN_to_ASN1_ENUMERATED", NULL },
   { BN_to_ASN1_INTEGER, "BN_to_ASN1_INTEGER", NULL },
   { BN_uadd, "BN_uadd", NULL },
   { BN_ucmp, "BN_ucmp", NULL },
   { BN_usub, "BN_usub", NULL },
   { BN_value_one, "BN_value_one", NULL },
   { BUF_MEM_free, "BUF_MEM_free", NULL },
   { BUF_MEM_grow, "BUF_MEM_grow", NULL },
   { BUF_MEM_grow_clean, "BUF_MEM_grow_clean", NULL },
   { BUF_MEM_new, "BUF_MEM_new", NULL },
   { BUF_memdup, "BUF_memdup", NULL },
   { BUF_strdup, "BUF_strdup", NULL },
   { BUF_strlcat, "BUF_strlcat", NULL },
   { BUF_strlcpy, "BUF_strlcpy", NULL },
   { BUF_strndup, "BUF_strndup", NULL },
   { CRYPTO_add_lock, "CRYPTO_add_lock", NULL },
   { CRYPTO_cleanup_all_ex_data, "CRYPTO_cleanup_all_ex_data", NULL },
   { CRYPTO_dbg_free, "CRYPTO_dbg_free", NULL },
   { CRYPTO_dbg_get_options, "CRYPTO_dbg_get_options", NULL },
   { CRYPTO_dbg_malloc, "CRYPTO_dbg_malloc", NULL },
   { CRYPTO_dbg_realloc, "CRYPTO_dbg_realloc", NULL },
   { CRYPTO_dbg_set_options, "CRYPTO_dbg_set_options", NULL },
   { CRYPTO_destroy_dynlockid, "CRYPTO_destroy_dynlockid", NULL },
   { CRYPTO_dup_ex_data, "CRYPTO_dup_ex_data", NULL },
   { CRYPTO_ex_data_new_class, "CRYPTO_ex_data_new_class", NULL },
   { CRYPTO_free, "CRYPTO_free", NULL },
   { CRYPTO_free_ex_data, "CRYPTO_free_ex_data", NULL },
   { CRYPTO_free_locked, "CRYPTO_free_locked", NULL },
   { CRYPTO_get_add_lock_callback, "CRYPTO_get_add_lock_callback", NULL },
   { CRYPTO_get_dynlock_create_callback, "CRYPTO_get_dynlock_create_callback", NULL },
   { CRYPTO_get_dynlock_destroy_callback, "CRYPTO_get_dynlock_destroy_callback", NULL },
   { CRYPTO_get_dynlock_lock_callback, "CRYPTO_get_dynlock_lock_callback", NULL },
   { CRYPTO_get_dynlock_value, "CRYPTO_get_dynlock_value", NULL },
   { CRYPTO_get_ex_data, "CRYPTO_get_ex_data", NULL },
   { CRYPTO_get_ex_data_implementation, "CRYPTO_get_ex_data_implementation", NULL },
   { CRYPTO_get_ex_new_index, "CRYPTO_get_ex_new_index", NULL },
   { CRYPTO_get_id_callback, "CRYPTO_get_id_callback", NULL },
   { CRYPTO_get_lock_name, "CRYPTO_get_lock_name", NULL },
   { CRYPTO_get_locked_mem_ex_functions, "CRYPTO_get_locked_mem_ex_functions", NULL },
   { CRYPTO_get_locked_mem_functions, "CRYPTO_get_locked_mem_functions", NULL },
   { CRYPTO_get_locking_callback, "CRYPTO_get_locking_callback", NULL },
   { CRYPTO_get_mem_debug_functions, "CRYPTO_get_mem_debug_functions", NULL },
   { CRYPTO_get_mem_debug_options, "CRYPTO_get_mem_debug_options", NULL },
   { CRYPTO_get_mem_ex_functions, "CRYPTO_get_mem_ex_functions", NULL },
   { CRYPTO_get_mem_functions, "CRYPTO_get_mem_functions", NULL },
   { CRYPTO_get_new_dynlockid, "CRYPTO_get_new_dynlockid", NULL },
   { CRYPTO_get_new_lockid, "CRYPTO_get_new_lockid", NULL },
   { CRYPTO_is_mem_check_on, "CRYPTO_is_mem_check_on", NULL },
   { CRYPTO_lock, "CRYPTO_lock", NULL },
   { CRYPTO_malloc, "CRYPTO_malloc", NULL },
   { CRYPTO_malloc_locked, "CRYPTO_malloc_locked", NULL },
   { CRYPTO_mem_ctrl, "CRYPTO_mem_ctrl", NULL },
   { CRYPTO_mem_leaks, "CRYPTO_mem_leaks", NULL },
   { CRYPTO_mem_leaks_cb, "CRYPTO_mem_leaks_cb", NULL },
   { CRYPTO_mem_leaks_fp, "CRYPTO_mem_leaks_fp", NULL },
   { CRYPTO_new_ex_data, "CRYPTO_new_ex_data", NULL },
   { CRYPTO_num_locks, "CRYPTO_num_locks", NULL },
   { CRYPTO_pop_info, "CRYPTO_pop_info", NULL },
   { CRYPTO_push_info_, "CRYPTO_push_info_", NULL },
   { CRYPTO_realloc, "CRYPTO_realloc", NULL },
   { CRYPTO_realloc_clean, "CRYPTO_realloc_clean", NULL },
   { CRYPTO_remalloc, "CRYPTO_remalloc", NULL },
   { CRYPTO_remove_all_info, "CRYPTO_remove_all_info", NULL },
   { CRYPTO_set_add_lock_callback, "CRYPTO_set_add_lock_callback", NULL },
   { CRYPTO_set_dynlock_create_callback, "CRYPTO_set_dynlock_create_callback", NULL },
   { CRYPTO_set_dynlock_destroy_callback, "CRYPTO_set_dynlock_destroy_callback", NULL },
   { CRYPTO_set_dynlock_lock_callback, "CRYPTO_set_dynlock_lock_callback", NULL },
   { CRYPTO_set_ex_data, "CRYPTO_set_ex_data", NULL },
   { CRYPTO_set_ex_data_implementation, "CRYPTO_set_ex_data_implementation", NULL },
   { CRYPTO_set_id_callback, "CRYPTO_set_id_callback", NULL },
   { CRYPTO_set_locked_mem_ex_functions, "CRYPTO_set_locked_mem_ex_functions", NULL },
   { CRYPTO_set_locked_mem_functions, "CRYPTO_set_locked_mem_functions", NULL },
   { CRYPTO_set_locking_callback, "CRYPTO_set_locking_callback", NULL },
   { CRYPTO_set_mem_debug_functions, "CRYPTO_set_mem_debug_functions", NULL },
   { CRYPTO_set_mem_debug_options, "CRYPTO_set_mem_debug_options", NULL },
   { CRYPTO_set_mem_ex_functions, "CRYPTO_set_mem_ex_functions", NULL },
   { CRYPTO_set_mem_functions, "CRYPTO_set_mem_functions", NULL },
   { CRYPTO_thread_id, "CRYPTO_thread_id", NULL },
   { DH_OpenSSL, "DH_OpenSSL", NULL },
   { DH_check, "DH_check", NULL },
   { DH_check_pub_key, "DH_check_pub_key", NULL },
   { DH_compute_key, "DH_compute_key", NULL },
   { DH_free, "DH_free", NULL },
   { DH_generate_key, "DH_generate_key", NULL },
   { DH_generate_parameters, "DH_generate_parameters", NULL },
   { DH_generate_parameters_ex, "DH_generate_parameters_ex", NULL },
   { DH_get_default_method, "DH_get_default_method", NULL },
   { DH_get_ex_data, "DH_get_ex_data", NULL },
   { DH_get_ex_new_index, "DH_get_ex_new_index", NULL },
   { DH_new, "DH_new", NULL },
   { DH_new_method, "DH_new_method", NULL },
   { DH_set_default_method, "DH_set_default_method", NULL },
   { DH_set_ex_data, "DH_set_ex_data", NULL },
   { DH_set_method, "DH_set_method", NULL },
   { DH_size, "DH_size", NULL },
   { DH_up_ref, "DH_up_ref", NULL },
   { DSA_OpenSSL, "DSA_OpenSSL", NULL },
   { DSA_SIG_free, "DSA_SIG_free", NULL },
   { DSA_SIG_new, "DSA_SIG_new", NULL },
   { DSA_do_sign, "DSA_do_sign", NULL },
   { DSA_do_verify, "DSA_do_verify", NULL },
   { DSA_dup_DH, "DSA_dup_DH", NULL },
   { DSA_free, "DSA_free", NULL },
   { DSA_generate_key, "DSA_generate_key", NULL },
   { DSA_generate_parameters, "DSA_generate_parameters", NULL },
   { DSA_generate_parameters_ex, "DSA_generate_parameters_ex", NULL },
   { DSA_get_default_method, "DSA_get_default_method", NULL },
   { DSA_get_ex_data, "DSA_get_ex_data", NULL },
   { DSA_get_ex_new_index, "DSA_get_ex_new_index", NULL },
   { DSA_new, "DSA_new", NULL },
   { DSA_new_method, "DSA_new_method", NULL },
   { DSA_print, "DSA_print", NULL },
   { DSA_print_fp, "DSA_print_fp", NULL },
   { DSA_set_default_method, "DSA_set_default_method", NULL },
   { DSA_set_ex_data, "DSA_set_ex_data", NULL },
   { DSA_set_method, "DSA_set_method", NULL },
   { DSA_sign, "DSA_sign", NULL },
   { DSA_sign_setup, "DSA_sign_setup", NULL },
   { DSA_size, "DSA_size", NULL },
   { DSA_up_ref, "DSA_up_ref", NULL },
   { DSA_verify, "DSA_verify", NULL },
   { ERR_add_error_data, "ERR_add_error_data", NULL },
   { ERR_clear_error, "ERR_clear_error", NULL },
   { ERR_error_string, "ERR_error_string", NULL },
   { ERR_error_string_n, "ERR_error_string_n", NULL },
   { ERR_free_strings, "ERR_free_strings", NULL },
   { ERR_func_error_string, "ERR_func_error_string", NULL },
   { ERR_get_err_state_table, "ERR_get_err_state_table", NULL },
   { ERR_get_error, "ERR_get_error", NULL },
   { ERR_get_error_line, "ERR_get_error_line", NULL },
   { ERR_get_error_line_data, "ERR_get_error_line_data", NULL },
   { ERR_get_implementation, "ERR_get_implementation", NULL },
   { ERR_get_next_error_library, "ERR_get_next_error_library", NULL },
   { ERR_get_state, "ERR_get_state", NULL },
   { ERR_get_string_table, "ERR_get_string_table", NULL },
   { ERR_lib_error_string, "ERR_lib_error_string", NULL },
   { ERR_load_ERR_strings, "ERR_load_ERR_strings", NULL },
   { ERR_load_crypto_strings, "ERR_load_crypto_strings", NULL },
   { ERR_load_strings, "ERR_load_strings", NULL },
   { ERR_peek_error, "ERR_peek_error", NULL },
   { ERR_peek_error_line, "ERR_peek_error_line", NULL },
   { ERR_peek_error_line_data, "ERR_peek_error_line_data", NULL },
   { ERR_peek_last_error, "ERR_peek_last_error", NULL },
   { ERR_peek_last_error_line, "ERR_peek_last_error_line", NULL },
   { ERR_peek_last_error_line_data, "ERR_peek_last_error_line_data", NULL },
   { ERR_pop_to_mark, "ERR_pop_to_mark", NULL },
   { ERR_print_errors, "ERR_print_errors", NULL },
   { ERR_print_errors_cb, "ERR_print_errors_cb", NULL },
   { ERR_print_errors_fp, "ERR_print_errors_fp", NULL },
   { ERR_put_error, "ERR_put_error", NULL },
   { ERR_reason_error_string, "ERR_reason_error_string", NULL },
   { ERR_release_err_state_table, "ERR_release_err_state_table", NULL },
   { ERR_remove_state, "ERR_remove_state", NULL },
   { ERR_set_error_data, "ERR_set_error_data", NULL },
   { ERR_set_implementation, "ERR_set_implementation", NULL },
   { ERR_set_mark, "ERR_set_mark", NULL },
   { ERR_unload_strings, "ERR_unload_strings", NULL },
   { EVP_BytesToKey, "EVP_BytesToKey", NULL },
   { EVP_CIPHER_CTX_block_size, "EVP_CIPHER_CTX_block_size", NULL },
   { EVP_CIPHER_CTX_cipher, "EVP_CIPHER_CTX_cipher", NULL },
   { EVP_CIPHER_CTX_cleanup, "EVP_CIPHER_CTX_cleanup", NULL },
   { EVP_CIPHER_CTX_ctrl, "EVP_CIPHER_CTX_ctrl", NULL },
   { EVP_CIPHER_CTX_flags, "EVP_CIPHER_CTX_flags", NULL },
   { EVP_CIPHER_CTX_free, "EVP_CIPHER_CTX_free", NULL },
   { EVP_CIPHER_CTX_get_app_data, "EVP_CIPHER_CTX_get_app_data", NULL },
   { EVP_CIPHER_CTX_init, "EVP_CIPHER_CTX_init", NULL },
   { EVP_CIPHER_CTX_iv_length, "EVP_CIPHER_CTX_iv_length", NULL },
   { EVP_CIPHER_CTX_key_length, "EVP_CIPHER_CTX_key_length", NULL },
   { EVP_CIPHER_CTX_new, "EVP_CIPHER_CTX_new", NULL },
   { EVP_CIPHER_CTX_nid, "EVP_CIPHER_CTX_nid", NULL },
   { EVP_CIPHER_CTX_rand_key, "EVP_CIPHER_CTX_rand_key", NULL },
   { EVP_CIPHER_CTX_set_app_data, "EVP_CIPHER_CTX_set_app_data", NULL },
   { EVP_CIPHER_CTX_set_key_length, "EVP_CIPHER_CTX_set_key_length", NULL },
   { EVP_CIPHER_CTX_set_padding, "EVP_CIPHER_CTX_set_padding", NULL },
   { EVP_CIPHER_asn1_to_param, "EVP_CIPHER_asn1_to_param", NULL },
   { EVP_CIPHER_block_size, "EVP_CIPHER_block_size", NULL },
   { EVP_CIPHER_flags, "EVP_CIPHER_flags", NULL },
   { EVP_CIPHER_get_asn1_iv, "EVP_CIPHER_get_asn1_iv", NULL },
   { EVP_CIPHER_iv_length, "EVP_CIPHER_iv_length", NULL },
   { EVP_CIPHER_key_length, "EVP_CIPHER_key_length", NULL },
   { EVP_CIPHER_nid, "EVP_CIPHER_nid", NULL },
   { EVP_CIPHER_param_to_asn1, "EVP_CIPHER_param_to_asn1", NULL },
   { EVP_CIPHER_set_asn1_iv, "EVP_CIPHER_set_asn1_iv", NULL },
   { EVP_CIPHER_type, "EVP_CIPHER_type", NULL },
   { EVP_Cipher, "EVP_Cipher", NULL },
   { EVP_CipherFinal, "EVP_CipherFinal", NULL },
   { EVP_CipherFinal_ex, "EVP_CipherFinal_ex", NULL },
   { EVP_CipherInit, "EVP_CipherInit", NULL },
   { EVP_CipherInit_ex, "EVP_CipherInit_ex", NULL },
   { EVP_CipherUpdate, "EVP_CipherUpdate", NULL },
   { EVP_DecodeBlock, "EVP_DecodeBlock", NULL },
   { EVP_DecodeFinal, "EVP_DecodeFinal", NULL },
   { EVP_DecodeInit, "EVP_DecodeInit", NULL },
   { EVP_DecodeUpdate, "EVP_DecodeUpdate", NULL },
   { EVP_DecryptFinal, "EVP_DecryptFinal", NULL },
   { EVP_DecryptFinal_ex, "EVP_DecryptFinal_ex", NULL },
   { EVP_DecryptInit, "EVP_DecryptInit", NULL },
   { EVP_DecryptInit_ex, "EVP_DecryptInit_ex", NULL },
   { EVP_DecryptUpdate, "EVP_DecryptUpdate", NULL },
   { EVP_Digest, "EVP_Digest", NULL },
   { EVP_DigestFinal, "EVP_DigestFinal", NULL },
   { EVP_DigestFinal_ex, "EVP_DigestFinal_ex", NULL },
   { EVP_DigestInit, "EVP_DigestInit", NULL },
   { EVP_DigestInit_ex, "EVP_DigestInit_ex", NULL },
   { EVP_DigestUpdate, "EVP_DigestUpdate", NULL },
   { EVP_EncodeBlock, "EVP_EncodeBlock", NULL },
   { EVP_EncodeFinal, "EVP_EncodeFinal", NULL },
   { EVP_EncodeInit, "EVP_EncodeInit", NULL },
   { EVP_EncodeUpdate, "EVP_EncodeUpdate", NULL },
   { EVP_EncryptFinal, "EVP_EncryptFinal", NULL },
   { EVP_EncryptFinal_ex, "EVP_EncryptFinal_ex", NULL },
   { EVP_EncryptInit, "EVP_EncryptInit", NULL },
   { EVP_EncryptInit_ex, "EVP_EncryptInit_ex", NULL },
   { EVP_EncryptUpdate, "EVP_EncryptUpdate", NULL },
   { EVP_MD_CTX_cleanup, "EVP_MD_CTX_cleanup", NULL },
   { EVP_MD_CTX_clear_flags, "EVP_MD_CTX_clear_flags", NULL },
   { EVP_MD_CTX_copy, "EVP_MD_CTX_copy", NULL },
   { EVP_MD_CTX_copy_ex, "EVP_MD_CTX_copy_ex", NULL },
   { EVP_MD_CTX_create, "EVP_MD_CTX_create", NULL },
   { EVP_MD_CTX_destroy, "EVP_MD_CTX_destroy", NULL },
   { EVP_MD_CTX_init, "EVP_MD_CTX_init", NULL },
   { EVP_MD_CTX_md, "EVP_MD_CTX_md", NULL },
   { EVP_MD_CTX_set_flags, "EVP_MD_CTX_set_flags", NULL },
   { EVP_MD_CTX_test_flags, "EVP_MD_CTX_test_flags", NULL },
   { EVP_MD_block_size, "EVP_MD_block_size", NULL },
   { EVP_MD_pkey_type, "EVP_MD_pkey_type", NULL },
   { EVP_MD_size, "EVP_MD_size", NULL },
   { EVP_MD_type, "EVP_MD_type", NULL },
   { EVP_OpenFinal, "EVP_OpenFinal", NULL },
   { EVP_OpenInit, "EVP_OpenInit", NULL },
   { EVP_PBE_CipherInit, "EVP_PBE_CipherInit", NULL },
   { EVP_PBE_alg_add, "EVP_PBE_alg_add", NULL },
   { EVP_PBE_cleanup, "EVP_PBE_cleanup", NULL },
   { EVP_PKEY_add1_attr, "EVP_PKEY_add1_attr", NULL },
   { EVP_PKEY_add1_attr_by_NID, "EVP_PKEY_add1_attr_by_NID", NULL },
   { EVP_PKEY_add1_attr_by_OBJ, "EVP_PKEY_add1_attr_by_OBJ", NULL },
   { EVP_PKEY_add1_attr_by_txt, "EVP_PKEY_add1_attr_by_txt", NULL },
   { EVP_PKEY_assign, "EVP_PKEY_assign", NULL },
   { EVP_PKEY_bits, "EVP_PKEY_bits", NULL },
   { EVP_PKEY_cmp, "EVP_PKEY_cmp", NULL },
   { EVP_PKEY_cmp_parameters, "EVP_PKEY_cmp_parameters", NULL },
   { EVP_PKEY_copy_parameters, "EVP_PKEY_copy_parameters", NULL },
   { EVP_PKEY_decrypt, "EVP_PKEY_decrypt", NULL },
   { EVP_PKEY_delete_attr, "EVP_PKEY_delete_attr", NULL },
   { EVP_PKEY_encrypt, "EVP_PKEY_encrypt", NULL },
   { EVP_PKEY_free, "EVP_PKEY_free", NULL },
   { EVP_PKEY_get1_DH, "EVP_PKEY_get1_DH", NULL },
   { EVP_PKEY_get1_DSA, "EVP_PKEY_get1_DSA", NULL },
   { EVP_PKEY_get1_RSA, "EVP_PKEY_get1_RSA", NULL },
   { EVP_PKEY_get_attr, "EVP_PKEY_get_attr", NULL },
   { EVP_PKEY_get_attr_by_NID, "EVP_PKEY_get_attr_by_NID", NULL },
   { EVP_PKEY_get_attr_by_OBJ, "EVP_PKEY_get_attr_by_OBJ", NULL },
   { EVP_PKEY_get_attr_count, "EVP_PKEY_get_attr_count", NULL },
   { EVP_PKEY_missing_parameters, "EVP_PKEY_missing_parameters", NULL },
   { EVP_PKEY_new, "EVP_PKEY_new", NULL },
   { EVP_PKEY_save_parameters, "EVP_PKEY_save_parameters", NULL },
   { EVP_PKEY_set1_DH, "EVP_PKEY_set1_DH", NULL },
   { EVP_PKEY_set1_DSA, "EVP_PKEY_set1_DSA", NULL },
   { EVP_PKEY_set1_RSA, "EVP_PKEY_set1_RSA", NULL },
   { EVP_PKEY_size, "EVP_PKEY_size", NULL },
   { EVP_PKEY_type, "EVP_PKEY_type", NULL },
   { EVP_SealFinal, "EVP_SealFinal", NULL },
   { EVP_SealInit, "EVP_SealInit", NULL },
   { EVP_SignFinal, "EVP_SignFinal", NULL },
   { EVP_VerifyFinal, "EVP_VerifyFinal", NULL },
   { EVP_add_cipher, "EVP_add_cipher", NULL },
   { EVP_add_digest, "EVP_add_digest", NULL },
   { EVP_aes_128_cbc, "EVP_aes_128_cbc", NULL },
   { EVP_aes_128_cfb, "EVP_aes_128_cfb", NULL },
   { EVP_aes_128_cfb1, "EVP_aes_128_cfb1", NULL },
   { EVP_aes_128_cfb8, "EVP_aes_128_cfb8", NULL },
   { EVP_aes_128_ecb, "EVP_aes_128_ecb", NULL },
   { EVP_aes_128_ofb, "EVP_aes_128_ofb", NULL },
   { EVP_aes_192_cbc, "EVP_aes_192_cbc", NULL },
   { EVP_aes_192_cfb, "EVP_aes_192_cfb", NULL },
   { EVP_aes_192_cfb1, "EVP_aes_192_cfb1", NULL },
   { EVP_aes_192_cfb8, "EVP_aes_192_cfb8", NULL },
   { EVP_aes_192_ecb, "EVP_aes_192_ecb", NULL },
   { EVP_aes_192_ofb, "EVP_aes_192_ofb", NULL },
   { EVP_aes_256_cbc, "EVP_aes_256_cbc", NULL },
   { EVP_aes_256_cfb, "EVP_aes_256_cfb", NULL },
   { EVP_aes_256_cfb1, "EVP_aes_256_cfb1", NULL },
   { EVP_aes_256_cfb8, "EVP_aes_256_cfb8", NULL },
   { EVP_aes_256_ecb, "EVP_aes_256_ecb", NULL },
   { EVP_aes_256_ofb, "EVP_aes_256_ofb", NULL },
   { EVP_bf_cbc, "EVP_bf_cbc", NULL },
   { EVP_bf_cfb, "EVP_bf_cfb", NULL },
   { EVP_bf_ecb, "EVP_bf_ecb", NULL },
   { EVP_bf_ofb, "EVP_bf_ofb", NULL },
   { EVP_cast5_cbc, "EVP_cast5_cbc", NULL },
   { EVP_cast5_cfb, "EVP_cast5_cfb", NULL },
   { EVP_cast5_ecb, "EVP_cast5_ecb", NULL },
   { EVP_cast5_ofb, "EVP_cast5_ofb", NULL },
   { EVP_cleanup, "EVP_cleanup", NULL },
   { EVP_des_cbc, "EVP_des_cbc", NULL },
   { EVP_des_cfb, "EVP_des_cfb", NULL },
   { EVP_des_cfb1, "EVP_des_cfb1", NULL },
   { EVP_des_cfb8, "EVP_des_cfb8", NULL },
   { EVP_des_ecb, "EVP_des_ecb", NULL },
   { EVP_des_ede, "EVP_des_ede", NULL },
   { EVP_des_ede3, "EVP_des_ede3", NULL },
   { EVP_des_ede3_cbc, "EVP_des_ede3_cbc", NULL },
   { EVP_des_ede3_cfb, "EVP_des_ede3_cfb", NULL },
   { EVP_des_ede3_cfb1, "EVP_des_ede3_cfb1", NULL },
   { EVP_des_ede3_cfb8, "EVP_des_ede3_cfb8", NULL },
   { EVP_des_ede3_ecb, "EVP_des_ede3_ecb", NULL },
   { EVP_des_ede3_ofb, "EVP_des_ede3_ofb", NULL },
   { EVP_des_ede_cbc, "EVP_des_ede_cbc", NULL },
   { EVP_des_ede_cfb, "EVP_des_ede_cfb", NULL },
   { EVP_des_ede_ecb, "EVP_des_ede_ecb", NULL },
   { EVP_des_ede_ofb, "EVP_des_ede_ofb", NULL },
   { EVP_des_ofb, "EVP_des_ofb", NULL },
   { EVP_desx_cbc, "EVP_desx_cbc", NULL },
   { EVP_dss, "EVP_dss", NULL },
   { EVP_dss1, "EVP_dss1", NULL },
   { EVP_ecdsa, "EVP_ecdsa", NULL },
   { EVP_enc_null, "EVP_enc_null", NULL },
   { EVP_get_cipherbyname, "EVP_get_cipherbyname", NULL },
   { EVP_get_digestbyname, "EVP_get_digestbyname", NULL },
   { EVP_get_pw_prompt, "EVP_get_pw_prompt", NULL },
   //{ EVP_idea_cbc, "EVP_idea_cbc", NULL },
   //{ EVP_idea_cfb, "EVP_idea_cfb", NULL },
   //{ EVP_idea_ecb, "EVP_idea_ecb", NULL },
   //{ EVP_idea_ofb, "EVP_idea_ofb", NULL },
   //{ EVP_md2, "EVP_md2", NULL },
   { EVP_md4, "EVP_md4", NULL },
   { EVP_md5, "EVP_md5", NULL },
   { EVP_md_null, "EVP_md_null", NULL },
   { EVP_rc2_40_cbc, "EVP_rc2_40_cbc", NULL },
   { EVP_rc2_64_cbc, "EVP_rc2_64_cbc", NULL },
   { EVP_rc2_cbc, "EVP_rc2_cbc", NULL },
   { EVP_rc2_cfb, "EVP_rc2_cfb", NULL },
   { EVP_rc2_ecb, "EVP_rc2_ecb", NULL },
   { EVP_rc2_ofb, "EVP_rc2_ofb", NULL },
   { EVP_rc4, "EVP_rc4", NULL },
   { EVP_read_pw_string, "EVP_read_pw_string", NULL },
   { EVP_set_pw_prompt, "EVP_set_pw_prompt", NULL },
   { EVP_sha, "EVP_sha", NULL },
   { EVP_sha1, "EVP_sha1", NULL },
   { HMAC, "HMAC", NULL },
   { HMAC_CTX_cleanup, "HMAC_CTX_cleanup", NULL },
   { HMAC_CTX_init, "HMAC_CTX_init", NULL },
   { HMAC_Final, "HMAC_Final", NULL },
   { HMAC_Init, "HMAC_Init", NULL },
   { HMAC_Init_ex, "HMAC_Init_ex", NULL },
   { HMAC_Update, "HMAC_Update", NULL },
   { OpenSSL_add_all_ciphers, "OpenSSL_add_all_ciphers", NULL },
   { OpenSSL_add_all_digests, "OpenSSL_add_all_digests", NULL },
   { PEM_ASN1_read, "PEM_ASN1_read", NULL },
   { PEM_ASN1_read_bio, "PEM_ASN1_read_bio", NULL },
   { PEM_ASN1_write, "PEM_ASN1_write", NULL },
   { PEM_ASN1_write_bio, "PEM_ASN1_write_bio", NULL },
   { PEM_SealFinal, "PEM_SealFinal", NULL },
   { PEM_SealInit, "PEM_SealInit", NULL },
   { PEM_SealUpdate, "PEM_SealUpdate", NULL },
   { PEM_SignFinal, "PEM_SignFinal", NULL },
   { PEM_SignInit, "PEM_SignInit", NULL },
   { PEM_SignUpdate, "PEM_SignUpdate", NULL },
   { PEM_X509_INFO_read, "PEM_X509_INFO_read", NULL },
   { PEM_X509_INFO_read_bio, "PEM_X509_INFO_read_bio", NULL },
   { PEM_X509_INFO_write_bio, "PEM_X509_INFO_write_bio", NULL },
   { PEM_bytes_read_bio, "PEM_bytes_read_bio", NULL },
   { PEM_def_callback, "PEM_def_callback", NULL },
   { PEM_dek_info, "PEM_dek_info", NULL },
   { PEM_do_header, "PEM_do_header", NULL },
   { PEM_get_EVP_CIPHER_INFO, "PEM_get_EVP_CIPHER_INFO", NULL },
   { PEM_proc_type, "PEM_proc_type", NULL },
   { PEM_read, "PEM_read", NULL },
   { PEM_read_DHparams, "PEM_read_DHparams", NULL },
   { PEM_read_DSAPrivateKey, "PEM_read_DSAPrivateKey", NULL },
   { PEM_read_DSA_PUBKEY, "PEM_read_DSA_PUBKEY", NULL },
   { PEM_read_DSAparams, "PEM_read_DSAparams", NULL },
   { PEM_read_NETSCAPE_CERT_SEQUENCE, "PEM_read_NETSCAPE_CERT_SEQUENCE", NULL },
   { PEM_read_PKCS7, "PEM_read_PKCS7", NULL },
   { PEM_read_PKCS8, "PEM_read_PKCS8", NULL },
   { PEM_read_PKCS8_PRIV_KEY_INFO, "PEM_read_PKCS8_PRIV_KEY_INFO", NULL },
   { PEM_read_PUBKEY, "PEM_read_PUBKEY", NULL },
   { PEM_read_PrivateKey, "PEM_read_PrivateKey", NULL },
   { PEM_read_RSAPrivateKey, "PEM_read_RSAPrivateKey", NULL },
   { PEM_read_RSAPublicKey, "PEM_read_RSAPublicKey", NULL },
   { PEM_read_RSA_PUBKEY, "PEM_read_RSA_PUBKEY", NULL },
   { PEM_read_X509, "PEM_read_X509", NULL },
   { PEM_read_X509_AUX, "PEM_read_X509_AUX", NULL },
   { PEM_read_X509_CERT_PAIR, "PEM_read_X509_CERT_PAIR", NULL },
   { PEM_read_X509_CRL, "PEM_read_X509_CRL", NULL },
   { PEM_read_X509_REQ, "PEM_read_X509_REQ", NULL },
   { PEM_read_bio, "PEM_read_bio", NULL },
   { PEM_read_bio_DHparams, "PEM_read_bio_DHparams", NULL },
   { PEM_read_bio_DSAPrivateKey, "PEM_read_bio_DSAPrivateKey", NULL },
   { PEM_read_bio_DSA_PUBKEY, "PEM_read_bio_DSA_PUBKEY", NULL },
   { PEM_read_bio_DSAparams, "PEM_read_bio_DSAparams", NULL },
   { PEM_read_bio_NETSCAPE_CERT_SEQUENCE, "PEM_read_bio_NETSCAPE_CERT_SEQUENCE", NULL },
   { PEM_read_bio_PKCS7, "PEM_read_bio_PKCS7", NULL },
   { PEM_read_bio_PKCS8, "PEM_read_bio_PKCS8", NULL },
   { PEM_read_bio_PKCS8_PRIV_KEY_INFO, "PEM_read_bio_PKCS8_PRIV_KEY_INFO", NULL },
   { PEM_read_bio_PUBKEY, "PEM_read_bio_PUBKEY", NULL },
   { PEM_read_bio_PrivateKey, "PEM_read_bio_PrivateKey", NULL },
   { PEM_read_bio_RSAPrivateKey, "PEM_read_bio_RSAPrivateKey", NULL },
   { PEM_read_bio_RSAPublicKey, "PEM_read_bio_RSAPublicKey", NULL },
   { PEM_read_bio_RSA_PUBKEY, "PEM_read_bio_RSA_PUBKEY", NULL },
   { PEM_read_bio_X509, "PEM_read_bio_X509", NULL },
   { PEM_read_bio_X509_AUX, "PEM_read_bio_X509_AUX", NULL },
   { PEM_read_bio_X509_CERT_PAIR, "PEM_read_bio_X509_CERT_PAIR", NULL },
   { PEM_read_bio_X509_CRL, "PEM_read_bio_X509_CRL", NULL },
   { PEM_read_bio_X509_REQ, "PEM_read_bio_X509_REQ", NULL },
   { PEM_write, "PEM_write", NULL },
   { PEM_write_DHparams, "PEM_write_DHparams", NULL },
   { PEM_write_DSAPrivateKey, "PEM_write_DSAPrivateKey", NULL },
   { PEM_write_DSA_PUBKEY, "PEM_write_DSA_PUBKEY", NULL },
   { PEM_write_DSAparams, "PEM_write_DSAparams", NULL },
   { PEM_write_NETSCAPE_CERT_SEQUENCE, "PEM_write_NETSCAPE_CERT_SEQUENCE", NULL },
   { PEM_write_PKCS7, "PEM_write_PKCS7", NULL },
   { PEM_write_PKCS8, "PEM_write_PKCS8", NULL },
   { PEM_write_PKCS8PrivateKey, "PEM_write_PKCS8PrivateKey", NULL },
   { PEM_write_PKCS8PrivateKey_nid, "PEM_write_PKCS8PrivateKey_nid", NULL },
   { PEM_write_PKCS8_PRIV_KEY_INFO, "PEM_write_PKCS8_PRIV_KEY_INFO", NULL },
   { PEM_write_PUBKEY, "PEM_write_PUBKEY", NULL },
   { PEM_write_PrivateKey, "PEM_write_PrivateKey", NULL },
   { PEM_write_RSAPrivateKey, "PEM_write_RSAPrivateKey", NULL },
   { PEM_write_RSAPublicKey, "PEM_write_RSAPublicKey", NULL },
   { PEM_write_RSA_PUBKEY, "PEM_write_RSA_PUBKEY", NULL },
   { PEM_write_X509, "PEM_write_X509", NULL },
   { PEM_write_X509_AUX, "PEM_write_X509_AUX", NULL },
   { PEM_write_X509_CERT_PAIR, "PEM_write_X509_CERT_PAIR", NULL },
   { PEM_write_X509_CRL, "PEM_write_X509_CRL", NULL },
   { PEM_write_X509_REQ, "PEM_write_X509_REQ", NULL },
   { PEM_write_X509_REQ_NEW, "PEM_write_X509_REQ_NEW", NULL },
   { PEM_write_bio, "PEM_write_bio", NULL },
   { PEM_write_bio_DHparams, "PEM_write_bio_DHparams", NULL },
   { PEM_write_bio_DSAPrivateKey, "PEM_write_bio_DSAPrivateKey", NULL },
   { PEM_write_bio_DSA_PUBKEY, "PEM_write_bio_DSA_PUBKEY", NULL },
   { PEM_write_bio_DSAparams, "PEM_write_bio_DSAparams", NULL },
   { PEM_write_bio_NETSCAPE_CERT_SEQUENCE, "PEM_write_bio_NETSCAPE_CERT_SEQUENCE", NULL },
   { PEM_write_bio_PKCS7, "PEM_write_bio_PKCS7", NULL },
   { PEM_write_bio_PKCS8, "PEM_write_bio_PKCS8", NULL },
   { PEM_write_bio_PKCS8PrivateKey, "PEM_write_bio_PKCS8PrivateKey", NULL },
   { PEM_write_bio_PKCS8PrivateKey_nid, "PEM_write_bio_PKCS8PrivateKey_nid", NULL },
   { PEM_write_bio_PKCS8_PRIV_KEY_INFO, "PEM_write_bio_PKCS8_PRIV_KEY_INFO", NULL },
   { PEM_write_bio_PUBKEY, "PEM_write_bio_PUBKEY", NULL },
   { PEM_write_bio_PrivateKey, "PEM_write_bio_PrivateKey", NULL },
   { PEM_write_bio_RSAPrivateKey, "PEM_write_bio_RSAPrivateKey", NULL },
   { PEM_write_bio_RSAPublicKey, "PEM_write_bio_RSAPublicKey", NULL },
   { PEM_write_bio_RSA_PUBKEY, "PEM_write_bio_RSA_PUBKEY", NULL },
   { PEM_write_bio_X509, "PEM_write_bio_X509", NULL },
   { PEM_write_bio_X509_AUX, "PEM_write_bio_X509_AUX", NULL },
   { PEM_write_bio_X509_CERT_PAIR, "PEM_write_bio_X509_CERT_PAIR", NULL },
   { PEM_write_bio_X509_CRL, "PEM_write_bio_X509_CRL", NULL },
   { PEM_write_bio_X509_REQ, "PEM_write_bio_X509_REQ", NULL },
   { PEM_write_bio_X509_REQ_NEW, "PEM_write_bio_X509_REQ_NEW", NULL },
   { PKCS7_add_attrib_smimecap, "PKCS7_add_attrib_smimecap", NULL },
   { PKCS7_add_attribute, "PKCS7_add_attribute", NULL },
   { PKCS7_add_certificate, "PKCS7_add_certificate", NULL },
   { PKCS7_add_crl, "PKCS7_add_crl", NULL },
   { PKCS7_add_recipient, "PKCS7_add_recipient", NULL },
   { PKCS7_add_recipient_info, "PKCS7_add_recipient_info", NULL },
   { PKCS7_add_signature, "PKCS7_add_signature", NULL },
   { PKCS7_add_signed_attribute, "PKCS7_add_signed_attribute", NULL },
   { PKCS7_add_signer, "PKCS7_add_signer", NULL },
   { PKCS7_cert_from_signer_info, "PKCS7_cert_from_signer_info", NULL },
   { PKCS7_content_new, "PKCS7_content_new", NULL },
   { PKCS7_ctrl, "PKCS7_ctrl", NULL },
   { PKCS7_dataDecode, "PKCS7_dataDecode", NULL },
   { PKCS7_dataFinal, "PKCS7_dataFinal", NULL },
   { PKCS7_dataInit, "PKCS7_dataInit", NULL },
   { PKCS7_dataVerify, "PKCS7_dataVerify", NULL },
   { PKCS7_decrypt, "PKCS7_decrypt", NULL },
   { PKCS7_digest_from_attributes, "PKCS7_digest_from_attributes", NULL },
   { PKCS7_dup, "PKCS7_dup", NULL },
   { PKCS7_encrypt, "PKCS7_encrypt", NULL },
   { PKCS7_free, "PKCS7_free", NULL },
   { PKCS7_get0_signers, "PKCS7_get0_signers", NULL },
   { PKCS7_get_attribute, "PKCS7_get_attribute", NULL },
   { PKCS7_get_issuer_and_serial, "PKCS7_get_issuer_and_serial", NULL },
   { PKCS7_get_signed_attribute, "PKCS7_get_signed_attribute", NULL },
   { PKCS7_get_signer_info, "PKCS7_get_signer_info", NULL },
   { PKCS7_get_smimecap, "PKCS7_get_smimecap", NULL },
   { PKCS7_new, "PKCS7_new", NULL },
   { PKCS7_set0_type_other, "PKCS7_set0_type_other", NULL },
   { PKCS7_set_attributes, "PKCS7_set_attributes", NULL },
   { PKCS7_set_cipher, "PKCS7_set_cipher", NULL },
   { PKCS7_set_content, "PKCS7_set_content", NULL },
   { PKCS7_set_digest, "PKCS7_set_digest", NULL },
   { PKCS7_set_signed_attributes, "PKCS7_set_signed_attributes", NULL },
   { PKCS7_set_type, "PKCS7_set_type", NULL },
   { PKCS7_sign, "PKCS7_sign", NULL },
   { PKCS7_signatureVerify, "PKCS7_signatureVerify", NULL },
   { PKCS7_simple_smimecap, "PKCS7_simple_smimecap", NULL },
   { PKCS7_verify, "PKCS7_verify", NULL },
   //{ RSAPrivateKey_asn1_meth, "RSAPrivateKey_asn1_meth", NULL },
   { RSAPrivateKey_dup, "RSAPrivateKey_dup", NULL },
   { RSAPublicKey_dup, "RSAPublicKey_dup", NULL },
   { RSA_PKCS1_SSLeay, "RSA_PKCS1_SSLeay", NULL },
   { RSA_X931_hash_id, "RSA_X931_hash_id", NULL },
   { RSA_blinding_off, "RSA_blinding_off", NULL },
   { RSA_blinding_on, "RSA_blinding_on", NULL },
   { RSA_check_key, "RSA_check_key", NULL },
   { RSA_flags, "RSA_flags", NULL },
   { RSA_free, "RSA_free", NULL },
   { RSA_generate_key_ex, "RSA_generate_key_ex", NULL },
   { RSA_get_default_method, "RSA_get_default_method", NULL },
   { RSA_get_ex_data, "RSA_get_ex_data", NULL },
   { RSA_get_ex_new_index, "RSA_get_ex_new_index", NULL },
   { RSA_get_method, "RSA_get_method", NULL },
   { RSA_memory_lock, "RSA_memory_lock", NULL },
   { RSA_new, "RSA_new", NULL },
   { RSA_new_method, "RSA_new_method", NULL },
   { RSA_null_method, "RSA_null_method", NULL },
   { RSA_padding_add_PKCS1_OAEP, "RSA_padding_add_PKCS1_OAEP", NULL },
   { RSA_padding_add_PKCS1_PSS, "RSA_padding_add_PKCS1_PSS", NULL },
   { RSA_padding_add_PKCS1_type_1, "RSA_padding_add_PKCS1_type_1", NULL },
   { RSA_padding_add_PKCS1_type_2, "RSA_padding_add_PKCS1_type_2", NULL },
   { RSA_padding_add_SSLv23, "RSA_padding_add_SSLv23", NULL },
   { RSA_padding_add_X931, "RSA_padding_add_X931", NULL },
   { RSA_padding_add_none, "RSA_padding_add_none", NULL },
   { RSA_padding_check_PKCS1_OAEP, "RSA_padding_check_PKCS1_OAEP", NULL },
   { RSA_padding_check_PKCS1_type_1, "RSA_padding_check_PKCS1_type_1", NULL },
   { RSA_padding_check_PKCS1_type_2, "RSA_padding_check_PKCS1_type_2", NULL },
   { RSA_padding_check_SSLv23, "RSA_padding_check_SSLv23", NULL },
   { RSA_padding_check_X931, "RSA_padding_check_X931", NULL },
   { RSA_padding_check_none, "RSA_padding_check_none", NULL },
   { RSA_print, "RSA_print", NULL },
   { RSA_print_fp, "RSA_print_fp", NULL },
   { RSA_private_decrypt, "RSA_private_decrypt", NULL },
   { RSA_private_encrypt, "RSA_private_encrypt", NULL },
   { RSA_public_decrypt, "RSA_public_decrypt", NULL },
   { RSA_public_encrypt, "RSA_public_encrypt", NULL },
   { RSA_set_default_method, "RSA_set_default_method", NULL },
   { RSA_set_ex_data, "RSA_set_ex_data", NULL },
   { RSA_set_method, "RSA_set_method", NULL },
   { RSA_setup_blinding, "RSA_setup_blinding", NULL },
   { RSA_sign, "RSA_sign", NULL },
   { RSA_sign_ASN1_OCTET_STRING, "RSA_sign_ASN1_OCTET_STRING", NULL },
   { RSA_size, "RSA_size", NULL },
   { RSA_up_ref, "RSA_up_ref", NULL },
   { RSA_verify, "RSA_verify", NULL },
   { RSA_verify_ASN1_OCTET_STRING, "RSA_verify_ASN1_OCTET_STRING", NULL },
   { RSA_verify_PKCS1_PSS, "RSA_verify_PKCS1_PSS", NULL },
   { SHA, "SHA", NULL },
   { SHA1, "SHA1", NULL },
   { SHA1_Final, "SHA1_Final", NULL },
   { SHA1_Init, "SHA1_Init", NULL },
   { SHA1_Transform, "SHA1_Transform", NULL },
   { SHA1_Update, "SHA1_Update", NULL },
   { SHA_Final, "SHA_Final", NULL },
   { SHA_Init, "SHA_Init", NULL },
   { SHA_Transform, "SHA_Transform", NULL },
   { SHA_Update, "SHA_Update", NULL },
   { SMIME_crlf_copy, "SMIME_crlf_copy", NULL },
   { SMIME_read_PKCS7, "SMIME_read_PKCS7", NULL },
   { SMIME_text, "SMIME_text", NULL },
   { SMIME_write_PKCS7, "SMIME_write_PKCS7", NULL },
   { X509_add1_ext_i2d, "X509_add1_ext_i2d", NULL },
   { X509_add1_reject_object, "X509_add1_reject_object", NULL },
   { X509_add1_trust_object, "X509_add1_trust_object", NULL },
   { X509_add_ext, "X509_add_ext", NULL },
   { X509_alias_get0, "X509_alias_get0", NULL },
   { X509_alias_set1, "X509_alias_set1", NULL },
   //{ X509_asn1_meth, "X509_asn1_meth", NULL },
   { X509_certificate_type, "X509_certificate_type", NULL },
   { X509_check_private_key, "X509_check_private_key", NULL },
   { X509_check_trust, "X509_check_trust", NULL },
   { X509_cmp, "X509_cmp", NULL },
   { X509_cmp_current_time, "X509_cmp_current_time", NULL },
   { X509_cmp_time, "X509_cmp_time", NULL },
   { X509_delete_ext, "X509_delete_ext", NULL },
   { X509_digest, "X509_digest", NULL },
   { X509_dup, "X509_dup", NULL },
   { X509_find_by_issuer_and_serial, "X509_find_by_issuer_and_serial", NULL },
   { X509_find_by_subject, "X509_find_by_subject", NULL },
   { X509_free, "X509_free", NULL },
   { X509_get0_pubkey_bitstr, "X509_get0_pubkey_bitstr", NULL },
   { X509_get_default_cert_area, "X509_get_default_cert_area", NULL },
   { X509_get_default_cert_dir, "X509_get_default_cert_dir", NULL },
   { X509_get_default_cert_dir_env, "X509_get_default_cert_dir_env", NULL },
   { X509_get_default_cert_file, "X509_get_default_cert_file", NULL },
   { X509_get_default_cert_file_env, "X509_get_default_cert_file_env", NULL },
   { X509_get_default_private_dir, "X509_get_default_private_dir", NULL },
   { X509_get_ex_data, "X509_get_ex_data", NULL },
   { X509_get_ex_new_index, "X509_get_ex_new_index", NULL },
   { X509_get_ext, "X509_get_ext", NULL },
   { X509_get_ext_by_NID, "X509_get_ext_by_NID", NULL },
   { X509_get_ext_by_OBJ, "X509_get_ext_by_OBJ", NULL },
   { X509_get_ext_by_critical, "X509_get_ext_by_critical", NULL },
   { X509_get_ext_count, "X509_get_ext_count", NULL },
   { X509_get_ext_d2i, "X509_get_ext_d2i", NULL },
   { X509_get_issuer_name, "X509_get_issuer_name", NULL },
   { X509_get_pubkey, "X509_get_pubkey", NULL },
   { X509_get_pubkey_parameters, "X509_get_pubkey_parameters", NULL },
   { X509_get_serialNumber, "X509_get_serialNumber", NULL },
   { X509_get_subject_name, "X509_get_subject_name", NULL },
   { X509_gmtime_adj, "X509_gmtime_adj", NULL },
   { X509_issuer_and_serial_cmp, "X509_issuer_and_serial_cmp", NULL },
   { X509_issuer_and_serial_hash, "X509_issuer_and_serial_hash", NULL },
   { X509_issuer_name_cmp, "X509_issuer_name_cmp", NULL },
   { X509_issuer_name_hash, "X509_issuer_name_hash", NULL },
   { X509_keyid_get0, "X509_keyid_get0", NULL },
   { X509_keyid_set1, "X509_keyid_set1", NULL },
   { X509_load_cert_crl_file, "X509_load_cert_crl_file", NULL },
   { X509_load_cert_file, "X509_load_cert_file", NULL },
   { X509_load_crl_file, "X509_load_crl_file", NULL },
   { X509_new, "X509_new", NULL },
   { X509_ocspid_print, "X509_ocspid_print", NULL },
   { X509_policy_check, "X509_policy_check", NULL },
   { X509_policy_level_get0_node, "X509_policy_level_get0_node", NULL },
   { X509_policy_level_node_count, "X509_policy_level_node_count", NULL },
   { X509_policy_node_get0_parent, "X509_policy_node_get0_parent", NULL },
   { X509_policy_node_get0_policy, "X509_policy_node_get0_policy", NULL },
   { X509_policy_node_get0_qualifiers, "X509_policy_node_get0_qualifiers", NULL },
   { X509_policy_tree_free, "X509_policy_tree_free", NULL },
   { X509_policy_tree_get0_level, "X509_policy_tree_get0_level", NULL },
   { X509_policy_tree_get0_policies, "X509_policy_tree_get0_policies", NULL },
   { X509_policy_tree_get0_user_policies, "X509_policy_tree_get0_user_policies", NULL },
   { X509_policy_tree_level_count, "X509_policy_tree_level_count", NULL },
   { X509_print, "X509_print", NULL },
   { X509_print_ex, "X509_print_ex", NULL },
   { X509_print_ex_fp, "X509_print_ex_fp", NULL },
   { X509_print_fp, "X509_print_fp", NULL },
   { X509_pubkey_digest, "X509_pubkey_digest", NULL },
   { X509_reject_clear, "X509_reject_clear", NULL },
   { X509_set_ex_data, "X509_set_ex_data", NULL },
   { X509_set_issuer_name, "X509_set_issuer_name", NULL },
   { X509_set_notAfter, "X509_set_notAfter", NULL },
   { X509_set_notBefore, "X509_set_notBefore", NULL },
   { X509_set_pubkey, "X509_set_pubkey", NULL },
   { X509_set_serialNumber, "X509_set_serialNumber", NULL },
   { X509_set_subject_name, "X509_set_subject_name", NULL },
   { X509_set_version, "X509_set_version", NULL },
   { X509_sign, "X509_sign", NULL },
   { X509_signature_print, "X509_signature_print", NULL },
   { X509_subject_name_cmp, "X509_subject_name_cmp", NULL },
   { X509_subject_name_hash, "X509_subject_name_hash", NULL },
   { X509_time_adj, "X509_time_adj", NULL },
   { X509_to_X509_REQ, "X509_to_X509_REQ", NULL },
   { X509_trust_clear, "X509_trust_clear", NULL },
   { X509_verify, "X509_verify", NULL },
   { X509_verify_cert, "X509_verify_cert", NULL },
   { X509_verify_cert_error_string, "X509_verify_cert_error_string", NULL },
   { X509at_add1_attr, "X509at_add1_attr", NULL },
   { X509at_add1_attr_by_NID, "X509at_add1_attr_by_NID", NULL },
   { X509at_add1_attr_by_OBJ, "X509at_add1_attr_by_OBJ", NULL },
   { X509at_add1_attr_by_txt, "X509at_add1_attr_by_txt", NULL },
   { X509at_delete_attr, "X509at_delete_attr", NULL },
   { X509at_get_attr, "X509at_get_attr", NULL },
   { X509at_get_attr_by_NID, "X509at_get_attr_by_NID", NULL },
   { X509at_get_attr_by_OBJ, "X509at_get_attr_by_OBJ", NULL },
   { X509at_get_attr_count, "X509at_get_attr_count", NULL },
   { X509v3_add_ext, "X509v3_add_ext", NULL },
   { X509v3_delete_ext, "X509v3_delete_ext", NULL },
   { X509v3_get_ext, "X509v3_get_ext", NULL },
   { X509v3_get_ext_by_NID, "X509v3_get_ext_by_NID", NULL },
   { X509v3_get_ext_by_OBJ, "X509v3_get_ext_by_OBJ", NULL },
   { X509v3_get_ext_by_critical, "X509v3_get_ext_by_critical", NULL },
   { X509v3_get_ext_count, "X509v3_get_ext_count", NULL },
   { NULL, NULL }
};

/*****************************************************************************
** Command: Init()
*/

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;


   GetPointer(argModule, FID_Master, &glModule);

   SSL_load_error_strings();
   ERR_load_BIO_strings();
   ERR_load_crypto_strings();
   SSL_library_init();
   OPENSSL_add_all_algorithms_noconf(); // Is this call a significant resource expense?

   return ERR_Okay;
}

/*****************************************************************************
** Command: Expunge()
*/

static ERROR CMDExpunge(void)
{
   ERR_remove_state(0);
   //ENGINE_cleanup();
   //CONF_modules_unload(1);
   ERR_free_strings();
   EVP_cleanup();
   CRYPTO_cleanup_all_ex_data();

   return ERR_Okay;
}

/*****************************************************************************
** Command: Open()
*/

static ERROR CMDOpen(OBJECTPTR Module)
{
   SetPointer(Module, FID_FunctionList, JumpTableV1);
   return ERR_Okay;
}

//****************************************************************************
/*
typedef struct pw_cb_data {
   CSTRING password;
   BIO *output;
} PW_CB_DATA;

static int genrsa_cb(int p, int n, BN_GENCB *cb)
{
   char c = '*';
   if (p == 0) c = '.';
   if (p == 1) c = '+';
   if (p == 2) c = '*';
   if (p == 3) c = '\n';
   BIO_write(cb->arg, &c, 1);
   (void)BIO_flush(cb->arg);
   return 1;
}

static int password_callback(char *buf, int bufsiz, int verify, PW_CB_DATA *cb_data)
{
   return 1;
}
*/
//****************************************************************************

static ERROR read_rsa_private(CSTRING PrivateKey, CSTRING Password, RSA **rsa_key, EVP_PKEY **sigkey)
{
   ERROR error;
   *rsa_key = NULL;
   *sigkey = NULL;
   BIO *input;
   if ((input = BIO_new_mem_buf((void *)PrivateKey, -1))) {
      if ((*sigkey = EVP_PKEY_new())) {
         if ((*rsa_key = PEM_read_bio_RSAPrivateKey(input, NULL, NULL, (void *)Password))) {
            if (EVP_PKEY_set1_RSA(*sigkey, *rsa_key) > 0) {
               // Success
               error = ERR_Okay;
            }
            else { LogErrorMsg("EVP_PKEY_set1_RSA() failed."); error = ERR_Failed; }
         }
         else { LogErrorMsg("PEM_read_bio_RSA_PUBKEY() failed."); error = ERR_Read; }
      }
      else { LogErrorMsg("EVP_PKEY_new() failed."); error = ERR_Failed; }
      BIO_free(input);
   }
   else { LogErrorMsg("BIO_new_mem_buf() failed."); error = ERR_AllocMemory; }

   return error;
}

/*****************************************************************************

-FUNCTION-
VerifySig: Private. Verify a signature against a source object, using a public key.
-END-

*****************************************************************************/

static ERROR sslVerifySig(OBJECTPTR Source, LONG SrcLength, CSTRING PublicKey, CSTRING Digest, APTR Signature, LONG SigLength)
{
   if ((!Source) OR (!PublicKey) OR (!Signature) OR (SigLength <= 0)) {
      LogF("@sslVerifySig()","Source: %p, Key: %p, Digest: %s, Signature: %p, Length: %d", Source, PublicKey, Digest, Signature, SigLength);
      return ERR_NullArgs;
   }

   ERROR error = ERR_Okay;

   if (!Digest) Digest = "sha512";

   if (!SrcLength) SrcLength = 0x7fffffff;
   else if (SrcLength < 0) return ERR_Args;

   LogF("~sslVerifySig()","Source: #%d, Length: %d, Key: %p, Digest: %s, Signature: %p, SigLen: %d", Source->UniqueID, SrcLength, PublicKey, Digest, Signature, SigLength);

   EVP_MD_CTX *mdctx;
   if ((mdctx = EVP_MD_CTX_create())) { // Create the Message Digest Context
      MSG("Parsing RSA public key.");

      const EVP_MD *md = EVP_get_digestbyname(Digest);
      if (EVP_DigestInit_ex(mdctx, md, NULL) > 0) {
         RSA *rsa_key = NULL;
         EVP_PKEY *sigkey = NULL;

         BIO *input = BIO_new_mem_buf((void *)PublicKey, -1);
         if (input) {
            if ((sigkey = EVP_PKEY_new())) {
               if ((rsa_key = PEM_read_bio_RSA_PUBKEY(input, NULL, NULL, NULL))) {
                  if (EVP_PKEY_set1_RSA(sigkey, rsa_key) > 0) {
                     // Success
                  }
                  else { LogErrorMsg("EVP_PKEY_set1_RSA() failed."); error = ERR_Failed; }
               }
               else { LogErrorMsg("PEM_read_bio_RSA_PUBKEY() failed."); error = ERR_Read; }
            }
            else { LogErrorMsg("EVP_PKEY_new() failed."); error = ERR_Failed; }

            if (!error) {
               if (EVP_DigestVerifyInit(mdctx, NULL, md, NULL, sigkey) > 0) {
                  UBYTE buffer[2048];
                  LONG bytes_read;
                  LONG total = 0;
                  while ((!acRead(Source, buffer, sizeof(buffer), &bytes_read)) AND (bytes_read > 0) AND (total < SrcLength)) {
                     if (total + bytes_read > SrcLength) bytes_read = SrcLength - total; // Reduce size if limited by SrcLength
                     total += bytes_read;
                     if (EVP_DigestVerifyUpdate(mdctx, buffer, bytes_read) <= 0) {
                        error = ERR_Failed;
                        break;
                     }
                  }

                  if (!error) {
                     if (EVP_DigestVerifyFinal(mdctx, Signature, SigLength) > 0) {
                        // Success
                     }
                     else {
                        LogErrorMsg("EVP_DigestVerifyFinal() failed: %s", ERR_reason_error_string(ERR_get_error()));
                        error = ERR_Failed;
                     }
                  }
               }
               else {
                  LogErrorMsg("EVP_DigestVerifyInit() failed: %s", ERR_reason_error_string(ERR_get_error()));
                  error = ERR_Failed;
               }
            }
         }
         else {
            LogErrorMsg("EVP_DigestInit_ex() failed: %s", ERR_reason_error_string(ERR_get_error()));
            error = ERR_Failed;
         }

         if (rsa_key) RSA_free(rsa_key);
         if (sigkey) EVP_PKEY_free(sigkey);
         if (input) BIO_free(input);
      }
      else error = ERR_Memory;

      EVP_MD_CTX_destroy(mdctx);
   }
   else error = ERR_Memory;

   LogBack();
   return error;
}

/*****************************************************************************

-FUNCTION-
CalcSigFromObject: Private. Generate a signature from a source object, using a private key.

-END-

*****************************************************************************/

static ERROR sslCalcSigFromObject(OBJECTPTR Source, LONG SrcLength, CSTRING PrivateKey, STRING Password,
   CSTRING Digest, APTR *Signature, LONG *SigSize)
{
   if ((!Source) OR (!PrivateKey) OR (!Signature)) return ERR_NullArgs;
   *Signature = NULL;

   ERROR error = ERR_Okay;

   if (!Digest) Digest = "sha512";

   if (!SrcLength) SrcLength = 0x7fffffff;
   else if (SrcLength < 0) return ERR_Args;

   LogF("~sslCalcSigFromObject()","Source: #%d, Length: %d, Key: %p, Password: %s", Source->UniqueID, SrcLength, PrivateKey, Password ? "Y" : "N");

   BIO *out;
   if ((out = BIO_new(BIO_s_mem()))) {
      EVP_MD_CTX *mdctx;
      if ((mdctx = EVP_MD_CTX_create())) { // Create the Message Digest Context
         // Create an RSA sigkey.

         MSG("Parsing RSA key.");

         RSA *rsa_key;
         EVP_PKEY *sigkey;

         error = read_rsa_private(PrivateKey, Password, &rsa_key, &sigkey);

         if (!error) {
            const EVP_MD *md = EVP_get_digestbyname(Digest);
            if (EVP_DigestInit_ex(mdctx, md, NULL) <= 0) { LogErrorMsg("EVP_DigestInit_ex() failed."); error = ERR_Failed; }

            if (!error) {
               if (EVP_DigestSignInit(mdctx, NULL /* EVP_PKEY_CTX */, md, NULL /* ENGINE */, sigkey) > 0) {
                  // Generate the signature by iterating over the source.

                  MSG("Generating the signature.");

                  UBYTE buffer[2048];
                  LONG bytes_read;
                  LONG total = 0;
                  while ((!acRead(Source, buffer, sizeof(buffer), &bytes_read)) AND (bytes_read > 0) AND (total < SrcLength)) {
                     if (total + bytes_read > SrcLength) bytes_read = SrcLength - total; // Reduce size if limited by SrcLength
                     total += bytes_read;
                     if (EVP_DigestSignUpdate(mdctx, buffer, bytes_read) <= 0) {
                        error = ERR_Failed;
                        break;
                     }
                  }

                  MSG("Processed %d bytes.  Now finalise signature and output it.", total);

                  int slen;
                  if (EVP_DigestSignFinal(mdctx, NULL, &slen) > 0) { // Get the required signature length.
                     if (!AllocMemory(slen, MEM_STRING, Signature, NULL)) { // Allocate memory for the signature based on size in slen
                        if (EVP_DigestSignFinal(mdctx, *Signature, &slen) > 0) { // Compute the signature
                           *SigSize = slen;
                        }
                        else { LogErrorMsg("EVP_DigestSignFinal() failed."); error = ERR_Failed; }
                     }
                     else error = ERR_AllocMemory;
                  }
                  else { LogErrorMsg("EVP_DigestSignFinal() failed."); error = ERR_Failed; }
               }
               else { LogErrorMsg("EVP_DigestSignInit() failed."); error = ERR_Failed; }
            }
         }

         if (rsa_key) RSA_free(rsa_key);
         if (sigkey) EVP_PKEY_free(sigkey);

         EVP_MD_CTX_destroy(mdctx);
      }
      else error = ERR_Memory;
   }
   else error = ERR_Memory;

   LogBack();
   return error;
}

/*****************************************************************************

-FUNCTION-
GenerateRSAKey: Private
-END-

*****************************************************************************/

static ERROR sslGenerateRSAKey(LONG TotalBits, CSTRING Password, STRING *PrivateKey, STRING *PublicKey)
{
   LogF("GenerateRSAKey()","Bits: %d", TotalBits);

   if (!PrivateKey) return ERR_NullArgs;
   *PrivateKey = NULL;
   if (PublicKey) *PublicKey = NULL;

   if (!TotalBits) TotalBits = 2048;
   else if ((TotalBits < 128) OR (TotalBits > 16384)) return ERR_OutOfRange;

   ERROR error = ERR_Okay;

   EVP_PKEY_CTX *ctx;
   if ((ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL))) {
      if (EVP_PKEY_keygen_init(ctx) > 0) {
         if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, TotalBits) > 0) {
            EVP_PKEY *pkey = NULL;
            if (EVP_PKEY_keygen(ctx, &pkey) > 0) {
               BIO *out;
               if ((out = BIO_new(BIO_s_mem()))) {
                  //PW_CB_DATA cb_data = { Password, out };
                  //const EVP_CIPHER *enc = NULL;

                  if (PEM_write_bio_PrivateKey(out, pkey, NULL, NULL, 0, 0, NULL)) {
                     char *start;
                     long private_len = BIO_get_mem_data(out, &start);
                     STRING private_key;
                     if (!AllocMemory(private_len+1, MEM_STRING, &private_key, NULL)) {
                        CopyMemory(start, private_key, private_len);
                        *PrivateKey = private_key;
                     }
                     else error = ERR_AllocMemory;
                  }
                  else error = ERR_Failed;

                  BIO_free_all(out);
               }
               else error = ERR_Failed;

               if ((!error) AND (PublicKey)) {
                  BIO *out;
                  if ((out = BIO_new(BIO_s_mem()))) {
                     if (PEM_write_bio_PUBKEY(out, pkey) > 0) {
                        char *start;
                        long public_len = BIO_get_mem_data(out, &start);
                        STRING public_key;
                        if (!AllocMemory(public_len+1, MEM_STRING, &public_key, NULL)) {
                           CopyMemory(start, public_key, public_len);
                           *PublicKey = public_key;
                        }
                        else error = ERR_AllocMemory;
                     }
                     else error = ERR_Failed;

                     BIO_free_all(out);
                  }
                  else error = ERR_Failed;
               }
            }
            if (pkey) EVP_PKEY_free(pkey);
         }
      }
      EVP_PKEY_CTX_free(ctx);
   }

   return error;
}

/*****************************************************************************

-FUNCTION-
GenerateRSAPublicKey: Private
-END-

*****************************************************************************/

static ERROR sslGenerateRSAPublicKey(CSTRING PrivateKey, CSTRING Password, STRING *PublicKey)
{
   LogF("GenerateRSAPublicKey()","");

   if ((!PrivateKey) OR (!PublicKey)) return ERR_NullArgs;

   RSA *rsa_key;
   EVP_PKEY *sigkey;
   ERROR error;

   if (!(error = read_rsa_private(PrivateKey, Password, &rsa_key, &sigkey))) {
      BIO *out;
      if ((out = BIO_new(BIO_s_mem()))) {
         if (PEM_write_bio_PUBKEY(out, sigkey) > 0) {
            char *start;
            long public_len = BIO_get_mem_data(out, &start);
            STRING public_key;
            if (!AllocMemory(public_len+1, MEM_STRING, &public_key, NULL)) {
               CopyMemory(start, public_key, public_len);
               *PublicKey = public_key;
               error = ERR_Okay;
            }
            else error = ERR_AllocMemory;
         }
         else error = ERR_Failed;

         BIO_free_all(out);
      }
      else error = ERR_Memory;
   }

   if (rsa_key) RSA_free(rsa_key);
   if (sigkey) EVP_PKEY_free(sigkey);
   return error;
}

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, 1.0)
