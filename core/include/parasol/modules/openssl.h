#ifndef  MODULES_OPENSSL_H
#define  MODULES_OPENSSL_H

/*
**   openssl.h
**
**   (C) Copyright 2009-2015 Paul Manias
*/

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/dsa.h>
#include <openssl/asn1.h>
#include <openssl/dtls1.h>
#include <openssl/hmac.h>

#ifndef PRV_OPENSSL

struct OpenSSLBase {
   ERROR (*GenerateRSAKey)(LONG, CSTRING, STRING *, STRING *);
   ERROR (*CalcSigFromObject)(OBJECTPTR, LONG, CSTRING, STRING, CSTRING, APTR *, LONG *);
   ERROR (*VerifySig)(OBJECTPTR, LONG, CSTRING, CSTRING, APTR, LONG);
   ERROR (*GenerateRSAPublicKey)(CSTRING, CSTRING, STRING *);

   SSL_CTX * (*SSL_CTX_new)(SSL_METHOD *meth);
   SSL_METHOD * (*SSLv23_client_method)(void);
   SSL *  (*SSL_new)(SSL_CTX *ctx);
   void (*BIO_f_ssl)(void);
   void (*BIO_new_buffer_ssl_connect)(void);
   void (*BIO_new_ssl)(void);
   void (*BIO_new_ssl_connect)(void);
   void (*BIO_s_connect)(void);
   void (*BIO_s_file)(void);
   void (*BIO_s_socket)(void);
   void (*BIO_ssl_copy_session_id)(void);
   void (*BIO_ssl_shutdown)(void);
   void (*DTLSv1_client_method)(void);
   void (*DTLSv1_method)(void);
   void (*DTLSv1_server_method)(void);
   void (*SSL_CIPHER_description)(void);
   void (*SSL_CIPHER_get_bits)(void);
   void (*SSL_CIPHER_get_name)(void);
   void (*SSL_CIPHER_get_version)(void);
   void (*SSL_COMP_add_compression_method)(void);
   void (*SSL_COMP_get_compression_methods)(void);
   void (*SSL_COMP_get_name)(void);
   void (*SSL_CTX_add_client_CA)(void);
   void (*SSL_CTX_add_session)(void);
   void (*SSL_CTX_callback_ctrl)(void);
   void (*SSL_CTX_check_private_key)(void);
   void (*SSL_CTX_ctrl)(void);
   void (*SSL_CTX_flush_sessions)(void);
   void (*SSL_CTX_free)(SSL_CTX *);
   void (*SSL_CTX_get_cert_store)(void);
   void (*SSL_CTX_get_client_CA_list)(void);
   void (*SSL_CTX_get_client_cert_cb)(void);
   void (*SSL_CTX_get_ex_data)(void);
   void (*SSL_CTX_get_ex_new_index)(void);
   void (*SSL_CTX_get_info_callback)(void);
   void (*SSL_CTX_get_quiet_shutdown)(void);
   void (*SSL_CTX_get_timeout)(void);
   void (*SSL_CTX_get_verify_callback)(void);
   void (*SSL_CTX_get_verify_depth)(void);
   void (*SSL_CTX_get_verify_mode)(void);
   int  (*SSL_CTX_load_verify_locations)(SSL_CTX *ctx, const char *CAfile, const char *CApath);
   void (*SSL_CTX_remove_session)(void);
   void (*SSL_CTX_sess_get_get_cb)(void);
   void (*SSL_CTX_sess_get_new_cb)(void);
   void (*SSL_CTX_sess_get_remove_cb)(void);
   void (*SSL_CTX_sess_set_get_cb)(void);
   void (*SSL_CTX_sess_set_new_cb)(void);
   void (*SSL_CTX_sess_set_remove_cb)(void);
   void (*SSL_CTX_sessions)(void);
   void (*SSL_CTX_set_cert_store)(void);
   void (*SSL_CTX_set_cert_verify_callback)(void);
   void (*SSL_CTX_set_cipher_list)(void);
   void (*SSL_CTX_set_client_CA_list)(void);
   void (*SSL_CTX_set_client_cert_cb)(void);
   void (*SSL_CTX_set_cookie_generate_cb)(void);
   void (*SSL_CTX_set_cookie_verify_cb)(void);
   void (*SSL_CTX_set_default_passwd_cb)(void);
   void (*SSL_CTX_set_default_passwd_cb_userdata)(void);
   void (*SSL_CTX_set_default_verify_paths)(void);
   void (*SSL_CTX_set_ex_data)(void);
   void (*SSL_CTX_set_generate_session_id)(void);
   void (*SSL_CTX_set_info_callback)(SSL_CTX *ctx, void (*cb)(const SSL *ssl,int type,int val));
   void (*SSL_CTX_set_msg_callback)(void);
   void (*SSL_CTX_set_purpose)(void);
   void (*SSL_CTX_set_quiet_shutdown)(void);
   void (*SSL_CTX_set_session_id_context)(void);
   void (*SSL_CTX_set_ssl_version)(void);
   void (*SSL_CTX_set_timeout)(void);
   void (*SSL_CTX_set_tmp_dh_callback)(void);
   void (*SSL_CTX_set_tmp_rsa_callback)(void);
   void (*SSL_CTX_set_trust)(void);
   void (*SSL_CTX_set_verify)(void);
   void (*SSL_CTX_set_verify_depth)(void);
   void (*SSL_CTX_use_PrivateKey)(void);
   void (*SSL_CTX_use_PrivateKey_ASN1)(void);
   void (*SSL_CTX_use_PrivateKey_file)(void);
   void (*SSL_CTX_use_RSAPrivateKey)(void);
   void (*SSL_CTX_use_RSAPrivateKey_ASN1)(void);
   void (*SSL_CTX_use_RSAPrivateKey_file)(void);
   void (*SSL_CTX_use_certificate)(void);
   void (*SSL_CTX_use_certificate_ASN1)(void);
   void (*SSL_CTX_use_certificate_chain_file)(void);
   void (*SSL_CTX_use_certificate_file)(void);
   //void (*SSL_SESSION_cmp)(void);
   void (*SSL_SESSION_free)(void);
   void (*SSL_SESSION_get_ex_data)(void);
   void (*SSL_SESSION_get_ex_new_index)(void);
   void (*SSL_SESSION_get_id)(void);
   void (*SSL_SESSION_get_time)(void);
   void (*SSL_SESSION_get_timeout)(void);
   //void (*SSL_SESSION_hash)(void);
   void (*SSL_SESSION_new)(void);
   void (*SSL_SESSION_print)(void);
   void (*SSL_SESSION_print_fp)(void);
   void (*SSL_SESSION_set_ex_data)(void);
   void (*SSL_SESSION_set_time)(void);
   void (*SSL_SESSION_set_timeout)(void);
   int  (*SSL_accept)(SSL *ssl);
   void (*SSL_add_client_CA)(void);
   void (*SSL_add_dir_cert_subjects_to_stack)(void);
   void (*SSL_add_file_cert_subjects_to_stack)(void);
   const char * (*SSL_alert_desc_string)(int value);
   const char * (*SSL_alert_desc_string_long)(int value);
   const char * (*SSL_alert_type_string)(int value);
   const char * (*SSL_alert_type_string_long)(int value);
   void (*SSL_callback_ctrl)(void);
   void (*SSL_check_private_key)(void);
   void (*SSL_clear)(void);
   int  (*SSL_connect)(SSL *ssl);
   void (*SSL_copy_session_id)(void);
   long (*SSL_ctrl)(SSL *ssl,int cmd, long larg, void *parg);
   int  (*SSL_do_handshake)(SSL *s);
   void (*SSL_dup)(void);
   void (*SSL_dup_CA_list)(void);
   void (*SSL_free)(SSL *ssl);
   void (*SSL_get1_session)(void);
   void (*SSL_get_SSL_CTX)(void);
   void (*SSL_get_certificate)(void);
   void (*SSL_get_cipher_list)(void);
   void (*SSL_get_ciphers)(void);
   void (*SSL_get_client_CA_list)(void);
   void (*SSL_get_current_cipher)(void);
   void (*SSL_get_current_compression)(void);
   void (*SSL_get_current_expansion)(void);
   void (*SSL_get_default_timeout)(void);
   int  (*SSL_get_error)(const SSL *s,int ret_code);
   void (*SSL_get_ex_data)(void);
   void (*SSL_get_ex_data_X509_STORE_CTX_idx)(void);
   void (*SSL_get_ex_new_index)(void);
   void (*SSL_get_fd)(void);
   void (*SSL_get_finished)(void);
   void (*SSL_get_info_callback)(void);
   void (*SSL_get_peer_cert_chain)(void);
   void (*SSL_get_peer_certificate)(void);
   void (*SSL_get_peer_finished)(void);
   void (*SSL_get_privatekey)(void);
   void (*SSL_get_quiet_shutdown)(void);
   void (*SSL_get_rbio)(void);
   void (*SSL_get_read_ahead)(void);
   void (*SSL_get_rfd)(void);
   void (*SSL_get_session)(void);
   void (*SSL_get_shared_ciphers)(void);
   void (*SSL_get_shutdown)(void);
   void (*SSL_get_ssl_method)(void);
   void (*SSL_get_verify_callback)(void);
   void (*SSL_get_verify_depth)(void);
   void (*SSL_get_verify_mode)(void);
   long (*SSL_get_verify_result)(const SSL *ssl);
   void (*SSL_get_version)(void);
   void (*SSL_get_wbio)(void);
   void (*SSL_get_wfd)(void);
   void (*SSL_has_matching_session_id)(void);
   void (*SSL_library_init)(void);
   void (*SSL_load_client_CA_file)(void);
   void (*SSL_load_error_strings)(void);
   int  (*SSL_peek)(SSL *ssl,void *buf,int num);
   int  (*SSL_pending)(const SSL *s);
   int  (*SSL_read)(SSL *ssl,void *buf,int num);
   void (*SSL_renegotiate)(void);
   void (*SSL_renegotiate_pending)(void);
   void (*SSL_rstate_string)(void);
   void (*SSL_rstate_string_long)(void);
   void (*SSL_set_SSL_CTX)(void);
   void (*SSL_set_accept_state)(void);
   void (*SSL_set_bio)(SSL *s, BIO *rbio,BIO *wbio);
   void (*SSL_set_cipher_list)(void);
   void (*SSL_set_client_CA_list)(void);
   void (*SSL_set_connect_state)(void);
   void (*SSL_set_ex_data)(void);
   void (*SSL_set_fd)(void);
   void (*SSL_set_generate_session_id)(void);
   void (*SSL_set_info_callback)(SSL *ssl, void (*cb)(const SSL *ssl,int type,int val));
   void (*SSL_set_msg_callback)(void);
   void (*SSL_set_purpose)(void);
   void (*SSL_set_quiet_shutdown)(void);
   void (*SSL_set_read_ahead)(void);
   void (*SSL_set_rfd)(void);
   void (*SSL_set_session)(void);
   void (*SSL_set_session_id_context)(void);
   void (*SSL_set_shutdown)(void);
   void (*SSL_set_ssl_method)(void);
   void (*SSL_set_tmp_dh_callback)(void);
   void (*SSL_set_tmp_rsa_callback)(void);
   void (*SSL_set_trust)(void);
   void (*SSL_set_verify)(void);
   void (*SSL_set_verify_depth)(void);
   void (*SSL_set_verify_result)(void);
   void (*SSL_set_wfd)(void);
   int  (*SSL_shutdown)(SSL *s);
   void (*SSL_state)(void);
   void (*SSL_state_string)(void);
   const char * (*SSL_state_string_long)(const SSL *s);
   void (*SSL_use_PrivateKey)(void);
   void (*SSL_use_PrivateKey_ASN1)(void);
   void (*SSL_use_PrivateKey_file)(void);
   void (*SSL_use_RSAPrivateKey)(void);
   void (*SSL_use_RSAPrivateKey_ASN1)(void);
   void (*SSL_use_RSAPrivateKey_file)(void);
   void (*SSL_use_certificate)(void);
   void (*SSL_use_certificate_ASN1)(void);
   void (*SSL_use_certificate_file)(void);
   void (*SSL_version)(void);
   void (*SSL_want)(void);
   int  (*SSL_write)(SSL *ssl,const void *buf,int num);
   void (*SSLv23_method)(void);
   void (*SSLv23_server_method)(void);
   //void (*SSLv2_client_method)(void);
   //void (*SSLv2_method)(void);
   //void (*SSLv2_server_method)(void);
   void (*SSLv3_client_method)(void);
   void (*SSLv3_method)(void);
   void (*SSLv3_server_method)(void);
   void (*TLSv1_client_method)(void);
   void (*TLSv1_method)(void);
   void (*TLSv1_server_method)(void);
   void (*X509_NAME_cmp)(void);
   void (*X509_NAME_dup)(void);
   void (*X509_NAME_free)(void);
   void (*X509_STORE_CTX_cleanup)(void);
   void (*X509_STORE_CTX_get0_param)(void);
   void (*X509_STORE_CTX_get_ex_new_index)(void);
   void (*X509_STORE_CTX_init)(void);
   void (*X509_STORE_CTX_set_default)(void);
   void (*X509_STORE_CTX_set_ex_data)(void);
   void (*X509_STORE_CTX_set_verify_cb)(void);
   void (*X509_STORE_free)(void);
   void (*X509_STORE_get_by_subject)(void);
   void (*X509_STORE_load_locations)(void);
   void (*X509_STORE_new)(void);
   void (*X509_STORE_set_default_paths)(void);
   void (*X509_VERIFY_PARAM_free)(void);
   void (*X509_VERIFY_PARAM_get_depth)(void);
   void (*X509_VERIFY_PARAM_inherit)(void);
   void (*X509_VERIFY_PARAM_new)(void);
   void (*X509_VERIFY_PARAM_set_depth)(void);
   void (*X509_VERIFY_PARAM_set_purpose)(void);
   void (*X509_VERIFY_PARAM_set_trust)(void);
   // Crypto
   void (*ASN1_add_oid_module)(void);
   void (*ASN1_check_infinite_end)(void);
   void (*ASN1_const_check_infinite_end)(void);
   void (*ASN1_d2i_bio)(void);
   void (*ASN1_d2i_fp)(void);
   void (*ASN1_digest)(void);
   void (*ASN1_dup)(void);
   void (*ASN1_generate_nconf)(void);
   void (*ASN1_generate_v3)(void);
   void (*ASN1_get_object)(void);
   void (*ASN1_i2d_bio)(void);
   void (*ASN1_i2d_fp)(void);
   void (*ASN1_item_d2i)(void);
   void (*ASN1_item_d2i_bio)(void);
   void (*ASN1_item_d2i_fp)(void);
   void (*ASN1_item_digest)(void);
   void (*ASN1_item_dup)(void);
   void (*ASN1_item_free)(void);
   void (*ASN1_item_i2d)(void);
   void (*ASN1_item_i2d_bio)(void);
   void (*ASN1_item_i2d_fp)(void);
   void (*ASN1_item_ndef_i2d)(void);
   void (*ASN1_item_new)(void);
   void (*ASN1_item_pack)(void);
   void (*ASN1_item_sign)(void);
   void (*ASN1_item_unpack)(void);
   void (*ASN1_item_verify)(void);
   void (*ASN1_mbstring_copy)(void);
   void (*ASN1_mbstring_ncopy)(void);
   void (*ASN1_object_size)(void);
   void (*ASN1_pack_string)(void);
   void (*ASN1_parse)(void);
   void (*ASN1_parse_dump)(void);
   void (*ASN1_put_eoc)(void);
   void (*ASN1_put_object)(void);
   void (*ASN1_seq_pack)(void);
   void (*ASN1_seq_unpack)(void);
   void (*ASN1_sign)(void);
   void (*ASN1_tag2bit)(void);
   void (*ASN1_tag2str)(void);
   void (*ASN1_unpack_string)(void);
   void (*ASN1_verify)(void);
   void (*BIO_accept)(void);
   void (*BIO_callback_ctrl)(void);
   void (*BIO_clear_flags)(void);
   void (*BIO_copy_next_retry)(void);
   void (*BIO_ctrl)(void);
   void (*BIO_ctrl_get_read_request)(void);
   void (*BIO_ctrl_get_write_guarantee)(void);
   void (*BIO_ctrl_pending)(void);
   void (*BIO_ctrl_reset_read_request)(void);
   void (*BIO_ctrl_wpending)(void);
   void (*BIO_debug_callback)(void);
   void (*BIO_dump)(void);
   void (*BIO_dump_cb)(void);
   void (*BIO_dump_fp)(void);
   void (*BIO_dump_indent)(void);
   void (*BIO_dump_indent_cb)(void);
   void (*BIO_dump_indent_fp)(void);
   void (*BIO_dup_chain)(void);
   void (*BIO_f_base64)(void);
   void (*BIO_f_buffer)(void);
   void (*BIO_f_cipher)(void);
   void (*BIO_f_md)(void);
   void (*BIO_f_nbio_test)(void);
   void (*BIO_f_null)(void);
   void (*BIO_f_reliable)(void);
   void (*BIO_fd_non_fatal_error)(void);
   void (*BIO_fd_should_retry)(void);
   void (*BIO_find_type)(void);
   void (*BIO_free)(void);
   void (*BIO_free_all)(void);
   void (*BIO_get_accept_socket)(void);
   void (*BIO_get_callback)(void);
   void (*BIO_get_callback_arg)(void);
   void (*BIO_get_ex_data)(void);
   void (*BIO_get_ex_new_index)(void);
   void (*BIO_get_host_ip)(void);
   void (*BIO_get_port)(void);
   void (*BIO_get_retry_BIO)(void);
   void (*BIO_get_retry_reason)(void);
   void (*BIO_gethostbyname)(void);
   void (*BIO_gets)(void);
   void (*BIO_indent)(void);
   void (*BIO_int_ctrl)(void);
   void (*BIO_method_name)(void);
   void (*BIO_method_type)(void);
   void (*BIO_new)(void);
   void (*BIO_new_accept)(void);
   void (*BIO_new_bio_pair)(void);
   void (*BIO_new_connect)(void);
   void (*BIO_new_dgram)(void);
   void (*BIO_new_fd)(void);
   void (*BIO_new_file)(void);
   void (*BIO_new_fp)(void);
   void (*BIO_new_mem_buf)(void);
   void *  (*BIO_new_socket)(int sock, int close_flag);
   void (*BIO_next)(void);
   void (*BIO_nread)(void);
   void (*BIO_number_read)(void);
   void (*BIO_number_written)(void);
   void (*BIO_nwrite)(void);
   void (*BIO_pop)(void);
   void (*BIO_printf)(void);
   void (*BIO_ptr_ctrl)(void);
   void (*BIO_push)(void);
   void (*BIO_puts)(void);
   void (*BIO_read)(void);
   void (*BIO_set)(void);
   void (*BIO_set_callback)(void);
   void (*BIO_set_callback_arg)(void);
   void (*BIO_set_cipher)(void);
   void (*BIO_set_ex_data)(void);
   void (*BIO_set_flags)(void);
   void (*BIO_set_tcp_ndelay)(void);
   void (*BIO_snprintf)(void);
   void (*BIO_sock_cleanup)(void);
   void (*BIO_sock_error)(void);
   void (*BIO_sock_init)(void);
   void (*BIO_sock_non_fatal_error)(void);
   void (*BIO_sock_should_retry)(void);
   void (*BIO_socket_ioctl)(void);
   void (*BIO_socket_nbio)(void);
   void (*BIO_test_flags)(void);
   void (*BIO_vfree)(void);
   void (*BIO_vprintf)(void);
   void (*BIO_vsnprintf)(void);
   void (*BIO_write)(void);
   void (*BN_CTX_end)(void);
   void (*BN_CTX_free)(void);
   void (*BN_CTX_get)(void);
   void (*BN_CTX_init)(void);
   void (*BN_CTX_new)(void);
   void (*BN_CTX_start)(void);
   void (*BN_GENCB_call)(void);
   void (*BN_GF2m_add)(void);
   void (*BN_GF2m_arr2poly)(void);
   void (*BN_GF2m_mod)(void);
   void (*BN_GF2m_mod_arr)(void);
   void (*BN_GF2m_mod_div)(void);
   void (*BN_GF2m_mod_div_arr)(void);
   void (*BN_GF2m_mod_exp)(void);
   void (*BN_GF2m_mod_exp_arr)(void);
   void (*BN_GF2m_mod_inv)(void);
   void (*BN_GF2m_mod_inv_arr)(void);
   void (*BN_GF2m_mod_mul)(void);
   void (*BN_GF2m_mod_mul_arr)(void);
   void (*BN_GF2m_mod_solve_quad)(void);
   void (*BN_GF2m_mod_solve_quad_arr)(void);
   void (*BN_GF2m_mod_sqr)(void);
   void (*BN_GF2m_mod_sqr_arr)(void);
   void (*BN_GF2m_mod_sqrt)(void);
   void (*BN_GF2m_mod_sqrt_arr)(void);
   void (*BN_GF2m_poly2arr)(void);
   void (*BN_add)(void);
   void (*BN_add_word)(void);
   void (*BN_bin2bn)(void);
   void (*BN_bn2bin)(void);
   void (*BN_bn2dec)(void);
   void (*BN_bn2hex)(void);
   void (*BN_bn2mpi)(void);
   void (*BN_bntest_rand)(void);
   void (*BN_clear)(void);
   void (*BN_clear_bit)(void);
   void (*BN_clear_free)(void);
   void (*BN_cmp)(void);
   void (*BN_copy)(void);
   void (*BN_dec2bn)(void);
   void (*BN_div)(void);
   void (*BN_div_recp)(void);
   void (*BN_div_word)(void);
   void (*BN_dup)(void);
   void (*BN_exp)(void);
   void (*BN_free)(BIGNUM *);
   void (*BN_from_montgomery)(void);
   void (*BN_gcd)(void);
   void (*BN_generate_prime)(void);
   void (*BN_generate_prime_ex)(void);
   void (*BN_get0_nist_prime_192)(void);
   void (*BN_get0_nist_prime_224)(void);
   void (*BN_get0_nist_prime_256)(void);
   void (*BN_get0_nist_prime_384)(void);
   void (*BN_get0_nist_prime_521)(void);
   void (*BN_get_params)(void);
   void (*BN_get_word)(void);
   void (*BN_hex2bn)(void);
   void (*BN_init)(void);
   void (*BN_is_bit_set)(void);
   void (*BN_is_prime)(void);
   void (*BN_is_prime_ex)(void);
   void (*BN_is_prime_fasttest)(void);
   void (*BN_is_prime_fasttest_ex)(void);
   void (*BN_kronecker)(void);
   void (*BN_lshift)(void);
   void (*BN_lshift1)(void);
   void (*BN_mask_bits)(void);
   void (*BN_mod_add)(void);
   void (*BN_mod_add_quick)(void);
   void (*BN_mod_exp)(void);
   void (*BN_mod_exp2_mont)(void);
   void (*BN_mod_exp_mont)(void);
   void (*BN_mod_exp_mont_consttime)(void);
   void (*BN_mod_exp_mont_word)(void);
   void (*BN_mod_exp_recp)(void);
   void (*BN_mod_exp_simple)(void);
   void (*BN_mod_inverse)(void);
   void (*BN_mod_lshift)(void);
   void (*BN_mod_lshift1)(void);
   void (*BN_mod_lshift1_quick)(void);
   void (*BN_mod_lshift_quick)(void);
   void (*BN_mod_mul)(void);
   void (*BN_mod_mul_montgomery)(void);
   void (*BN_mod_mul_reciprocal)(void);
   void (*BN_mod_sqr)(void);
   void (*BN_mod_sqrt)(void);
   void (*BN_mod_sub)(void);
   void (*BN_mod_sub_quick)(void);
   void (*BN_mod_word)(void);
   void (*BN_mpi2bn)(void);
   void (*BN_mul)(void);
   void (*BN_mul_word)(void);
   BIGNUM * (*BN_new)(void);
   void (*BN_nist_mod_192)(void);
   void (*BN_nist_mod_224)(void);
   void (*BN_nist_mod_256)(void);
   void (*BN_nist_mod_384)(void);
   void (*BN_nist_mod_521)(void);
   void (*BN_nnmod)(void);
   void (*BN_num_bits)(void);
   void (*BN_num_bits_word)(void);
   void (*BN_options)(void);
   void (*BN_print)(void);
   void (*BN_print_fp)(void);
   void (*BN_pseudo_rand)(void);
   void (*BN_pseudo_rand_range)(void);
   void (*BN_rand)(void);
   void (*BN_rand_range)(void);
   void (*BN_reciprocal)(void);
   void (*BN_rshift)(void);
   void (*BN_rshift1)(void);
   void (*BN_set_bit)(void);
   void (*BN_set_negative)(void);
   void (*BN_set_params)(void);
   int  (*BN_set_word)(BIGNUM *a, unsigned long w);
   void (*BN_sqr)(void);
   void (*BN_sub)(void);
   void (*BN_sub_word)(void);
   void (*BN_swap)(void);
   void (*BN_to_ASN1_ENUMERATED)(void);
   void (*BN_to_ASN1_INTEGER)(void);
   void (*BN_uadd)(void);
   void (*BN_ucmp)(void);
   void (*BN_usub)(void);
   void (*BN_value_one)(void);
   void (*BUF_MEM_free)(void);
   void (*BUF_MEM_grow)(void);
   void (*BUF_MEM_grow_clean)(void);
   void (*BUF_MEM_new)(void);
   void (*BUF_memdup)(void);
   void (*BUF_strdup)(void);
   void (*BUF_strlcat)(void);
   void (*BUF_strlcpy)(void);
   void (*BUF_strndup)(void);
   void (*CRYPTO_add_lock)(void);
   void (*CRYPTO_cleanup_all_ex_data)(void);
   void (*CRYPTO_dbg_free)(void);
   void (*CRYPTO_dbg_get_options)(void);
   void (*CRYPTO_dbg_malloc)(void);
   void (*CRYPTO_dbg_realloc)(void);
   void (*CRYPTO_dbg_set_options)(void);
   void (*CRYPTO_destroy_dynlockid)(void);
   void (*CRYPTO_dup_ex_data)(void);
   void (*CRYPTO_ex_data_new_class)(void);
   void (*CRYPTO_free)(void);
   void (*CRYPTO_free_ex_data)(void);
   void (*CRYPTO_free_locked)(void);
   void (*CRYPTO_get_add_lock_callback)(void);
   void (*CRYPTO_get_dynlock_create_callback)(void);
   void (*CRYPTO_get_dynlock_destroy_callback)(void);
   void (*CRYPTO_get_dynlock_lock_callback)(void);
   void (*CRYPTO_get_dynlock_value)(void);
   void (*CRYPTO_get_ex_data)(void);
   void (*CRYPTO_get_ex_data_implementation)(void);
   void (*CRYPTO_get_ex_new_index)(void);
   void (*CRYPTO_get_id_callback)(void);
   void (*CRYPTO_get_lock_name)(void);
   void (*CRYPTO_get_locked_mem_ex_functions)(void);
   void (*CRYPTO_get_locked_mem_functions)(void);
   void (*CRYPTO_get_locking_callback)(void);
   void (*CRYPTO_get_mem_debug_functions)(void);
   void (*CRYPTO_get_mem_debug_options)(void);
   void (*CRYPTO_get_mem_ex_functions)(void);
   void (*CRYPTO_get_mem_functions)(void);
   void (*CRYPTO_get_new_dynlockid)(void);
   void (*CRYPTO_get_new_lockid)(void);
   void (*CRYPTO_is_mem_check_on)(void);
   void (*CRYPTO_lock)(void);
   void (*CRYPTO_malloc)(void);
   void (*CRYPTO_malloc_locked)(void);
   void (*CRYPTO_mem_ctrl)(void);
   void (*CRYPTO_mem_leaks)(void);
   void (*CRYPTO_mem_leaks_cb)(void);
   void (*CRYPTO_mem_leaks_fp)(void);
   void (*CRYPTO_new_ex_data)(void);
   void (*CRYPTO_num_locks)(void);
   void (*CRYPTO_pop_info)(void);
   void (*CRYPTO_push_info_)(void);
   void (*CRYPTO_realloc)(void);
   void (*CRYPTO_realloc_clean)(void);
   void (*CRYPTO_remalloc)(void);
   void (*CRYPTO_remove_all_info)(void);
   void (*CRYPTO_set_add_lock_callback)(void);
   void (*CRYPTO_set_dynlock_create_callback)(void);
   void (*CRYPTO_set_dynlock_destroy_callback)(void);
   void (*CRYPTO_set_dynlock_lock_callback)(void);
   void (*CRYPTO_set_ex_data)(void);
   void (*CRYPTO_set_ex_data_implementation)(void);
   void (*CRYPTO_set_id_callback)(void);
   void (*CRYPTO_set_locked_mem_ex_functions)(void);
   void (*CRYPTO_set_locked_mem_functions)(void);
   void (*CRYPTO_set_locking_callback)(void);
   void (*CRYPTO_set_mem_debug_functions)(void);
   void (*CRYPTO_set_mem_debug_options)(void);
   void (*CRYPTO_set_mem_ex_functions)(void);
   void (*CRYPTO_set_mem_functions)(void);
   void (*CRYPTO_thread_id)(void);
   void (*DH_OpenSSL)(void);
   void (*DH_check)(void);
   void (*DH_check_pub_key)(void);
   void (*DH_compute_key)(void);
   void (*DH_free)(void);
   void (*DH_generate_key)(void);
   void (*DH_generate_parameters)(void);
   void (*DH_generate_parameters_ex)(void);
   void (*DH_get_default_method)(void);
   void (*DH_get_ex_data)(void);
   void (*DH_get_ex_new_index)(void);
   void (*DH_new)(void);
   void (*DH_new_method)(void);
   void (*DH_set_default_method)(void);
   void (*DH_set_ex_data)(void);
   void (*DH_set_method)(void);
   void (*DH_size)(void);
   void (*DH_up_ref)(void);
   void (*DSA_OpenSSL)(void);
   void (*DSA_SIG_free)(void);
   void (*DSA_SIG_new)(void);
   void (*DSA_do_sign)(void);
   void (*DSA_do_verify)(void);
   void (*DSA_dup_DH)(void);
   void (*DSA_free)(void);
   void (*DSA_generate_key)(void);
   void (*DSA_generate_parameters)(void);
   void (*DSA_generate_parameters_ex)(void);
   void (*DSA_get_default_method)(void);
   void (*DSA_get_ex_data)(void);
   void (*DSA_get_ex_new_index)(void);
   void (*DSA_new)(void);
   void (*DSA_new_method)(void);
   void (*DSA_print)(void);
   void (*DSA_print_fp)(void);
   void (*DSA_set_default_method)(void);
   void (*DSA_set_ex_data)(void);
   void (*DSA_set_method)(void);
   void (*DSA_sign)(void);
   void (*DSA_sign_setup)(void);
   void (*DSA_size)(void);
   void (*DSA_up_ref)(void);
   void (*DSA_verify)(void);
   void (*ECDH_OpenSSL)(void);
   void (*ECDH_compute_key)(void);
   void (*ECDH_get_default_method)(void);
   void (*ECDH_get_ex_data)(void);
   void (*ECDH_get_ex_new_index)(void);
   void (*ECDH_set_default_method)(void);
   void (*ECDH_set_ex_data)(void);
   void (*ECDH_set_method)(void);
   void (*ECDSA_OpenSSL)(void);
   void (*ECDSA_SIG_free)(void);
   void (*ECDSA_SIG_new)(void);
   void (*ECDSA_do_sign)(void);
   void (*ECDSA_do_sign_ex)(void);
   void (*ECDSA_do_verify)(void);
   void (*ECDSA_get_default_method)(void);
   void (*ECDSA_get_ex_data)(void);
   void (*ECDSA_get_ex_new_index)(void);
   void (*ECDSA_set_default_method)(void);
   void (*ECDSA_set_ex_data)(void);
   void (*ECDSA_set_method)(void);
   void (*ECDSA_sign)(void);
   void (*ECDSA_sign_ex)(void);
   void (*ECDSA_sign_setup)(void);
   void (*ECDSA_size)(void);
   void (*ECDSA_verify)(void);
   void (*ERR_add_error_data)(void);
   void (*ERR_clear_error)(void);
   char * (*ERR_error_string)(unsigned long e,char *buf);
   void (*ERR_error_string_n)(void);
   void (*ERR_free_strings)(void);
   void (*ERR_func_error_string)(void);
   void (*ERR_get_err_state_table)(void);
   unsigned long (*ERR_get_error)(void);
   unsigned long (*ERR_get_error_line)(const char **file,int *line);
   void (*ERR_get_error_line_data)(void);
   void (*ERR_get_implementation)(void);
   void (*ERR_get_next_error_library)(void);
   void (*ERR_get_state)(void);
   void (*ERR_get_string_table)(void);
   void (*ERR_lib_error_string)(void);
   void (*ERR_load_ERR_strings)(void);
   void (*ERR_load_crypto_strings)(void);
   void (*ERR_load_strings)(void);
   void (*ERR_peek_error)(void);
   void (*ERR_peek_error_line)(void);
   void (*ERR_peek_error_line_data)(void);
   void (*ERR_peek_last_error)(void);
   void (*ERR_peek_last_error_line)(void);
   void (*ERR_peek_last_error_line_data)(void);
   void (*ERR_pop_to_mark)(void);
   void   (*ERR_print_errors)(BIO *bp);
   void (*ERR_print_errors_cb)(void);
   void (*ERR_print_errors_fp)(void);
   void (*ERR_put_error)(void);
   void (*ERR_reason_error_string)(void);
   void (*ERR_release_err_state_table)(void);
   void (*ERR_remove_state)(void);
   void (*ERR_set_error_data)(void);
   void (*ERR_set_implementation)(void);
   void (*ERR_set_mark)(void);
   void (*ERR_unload_strings)(void);
   void (*EVP_BytesToKey)(void);
   void (*EVP_CIPHER_CTX_block_size)(void);
   void (*EVP_CIPHER_CTX_cipher)(void);
   void (*EVP_CIPHER_CTX_cleanup)(void);
   void (*EVP_CIPHER_CTX_ctrl)(void);
   void (*EVP_CIPHER_CTX_flags)(void);
   void (*EVP_CIPHER_CTX_free)(void);
   void (*EVP_CIPHER_CTX_get_app_data)(void);
   void (*EVP_CIPHER_CTX_init)(void);
   void (*EVP_CIPHER_CTX_iv_length)(void);
   void (*EVP_CIPHER_CTX_key_length)(void);
   void (*EVP_CIPHER_CTX_new)(void);
   void (*EVP_CIPHER_CTX_nid)(void);
   void (*EVP_CIPHER_CTX_rand_key)(void);
   void (*EVP_CIPHER_CTX_set_app_data)(void);
   void (*EVP_CIPHER_CTX_set_key_length)(void);
   void (*EVP_CIPHER_CTX_set_padding)(void);
   void (*EVP_CIPHER_asn1_to_param)(void);
   void (*EVP_CIPHER_block_size)(void);
   void (*EVP_CIPHER_flags)(void);
   void (*EVP_CIPHER_get_asn1_iv)(void);
   void (*EVP_CIPHER_iv_length)(void);
   void (*EVP_CIPHER_key_length)(void);
   void (*EVP_CIPHER_nid)(void);
   void (*EVP_CIPHER_param_to_asn1)(void);
   void (*EVP_CIPHER_set_asn1_iv)(void);
   void (*EVP_CIPHER_type)(void);
   void (*EVP_Cipher)(void);
   void (*EVP_CipherFinal)(void);
   void (*EVP_CipherFinal_ex)(void);
   void (*EVP_CipherInit)(void);
   void (*EVP_CipherInit_ex)(void);
   void (*EVP_CipherUpdate)(void);
   void (*EVP_DecodeBlock)(void);
   void (*EVP_DecodeFinal)(void);
   void (*EVP_DecodeInit)(void);
   void (*EVP_DecodeUpdate)(void);
   void (*EVP_DecryptFinal)(void);
   void (*EVP_DecryptFinal_ex)(void);
   void (*EVP_DecryptInit)(void);
   void (*EVP_DecryptInit_ex)(void);
   void (*EVP_DecryptUpdate)(void);
   void (*EVP_Digest)(void);
   void (*EVP_DigestFinal)(void);
   void (*EVP_DigestFinal_ex)(void);
   void (*EVP_DigestInit)(void);
   void (*EVP_DigestInit_ex)(void);
   void (*EVP_DigestUpdate)(void);
   void (*EVP_EncodeBlock)(void);
   void (*EVP_EncodeFinal)(void);
   void (*EVP_EncodeInit)(void);
   void (*EVP_EncodeUpdate)(void);
   void (*EVP_EncryptFinal)(void);
   void (*EVP_EncryptFinal_ex)(void);
   void (*EVP_EncryptInit)(void);
   void (*EVP_EncryptInit_ex)(void);
   void (*EVP_EncryptUpdate)(void);
   void (*EVP_MD_CTX_cleanup)(void);
   void (*EVP_MD_CTX_clear_flags)(void);
   void (*EVP_MD_CTX_copy)(void);
   void (*EVP_MD_CTX_copy_ex)(void);
   void (*EVP_MD_CTX_create)(void);
   void (*EVP_MD_CTX_destroy)(void);
   void (*EVP_MD_CTX_init)(void);
   void (*EVP_MD_CTX_md)(void);
   void (*EVP_MD_CTX_set_flags)(void);
   void (*EVP_MD_CTX_test_flags)(void);
   void (*EVP_MD_block_size)(void);
   void (*EVP_MD_pkey_type)(void);
   void (*EVP_MD_size)(void);
   void (*EVP_MD_type)(void);
   void (*EVP_OpenFinal)(void);
   void (*EVP_OpenInit)(void);
   void (*EVP_PBE_CipherInit)(void);
   void (*EVP_PBE_alg_add)(void);
   void (*EVP_PBE_cleanup)(void);
   void (*EVP_PKEY_add1_attr)(void);
   void (*EVP_PKEY_add1_attr_by_NID)(void);
   void (*EVP_PKEY_add1_attr_by_OBJ)(void);
   void (*EVP_PKEY_add1_attr_by_txt)(void);
   void (*EVP_PKEY_assign)(void);
   void (*EVP_PKEY_bits)(void);
   void (*EVP_PKEY_cmp)(void);
   void (*EVP_PKEY_cmp_parameters)(void);
   void (*EVP_PKEY_copy_parameters)(void);
   void (*EVP_PKEY_decrypt)(void);
   void (*EVP_PKEY_delete_attr)(void);
   void (*EVP_PKEY_encrypt)(void);
   void (*EVP_PKEY_free)(void);
   void (*EVP_PKEY_get1_DH)(void);
   void (*EVP_PKEY_get1_DSA)(void);
   void (*EVP_PKEY_get1_RSA)(void);
   void (*EVP_PKEY_get_attr)(void);
   void (*EVP_PKEY_get_attr_by_NID)(void);
   void (*EVP_PKEY_get_attr_by_OBJ)(void);
   void (*EVP_PKEY_get_attr_count)(void);
   void (*EVP_PKEY_missing_parameters)(void);
   void (*EVP_PKEY_new)(void);
   void (*EVP_PKEY_save_parameters)(void);
   void (*EVP_PKEY_set1_DH)(void);
   void (*EVP_PKEY_set1_DSA)(void);
   void (*EVP_PKEY_set1_RSA)(void);
   void (*EVP_PKEY_size)(void);
   void (*EVP_PKEY_type)(void);
   void (*EVP_SealFinal)(void);
   void (*EVP_SealInit)(void);
   void (*EVP_SignFinal)(void);
   void (*EVP_VerifyFinal)(void);
   void (*EVP_add_cipher)(void);
   void (*EVP_add_digest)(void);
   void (*EVP_aes_128_cbc)(void);
   void (*EVP_aes_128_cfb)(void);
   void (*EVP_aes_128_cfb1)(void);
   void (*EVP_aes_128_cfb8)(void);
   void (*EVP_aes_128_ecb)(void);
   void (*EVP_aes_128_ofb)(void);
   void (*EVP_aes_192_cbc)(void);
   void (*EVP_aes_192_cfb)(void);
   void (*EVP_aes_192_cfb1)(void);
   void (*EVP_aes_192_cfb8)(void);
   void (*EVP_aes_192_ecb)(void);
   void (*EVP_aes_192_ofb)(void);
   void (*EVP_aes_256_cbc)(void);
   void (*EVP_aes_256_cfb)(void);
   void (*EVP_aes_256_cfb1)(void);
   void (*EVP_aes_256_cfb8)(void);
   void (*EVP_aes_256_ecb)(void);
   void (*EVP_aes_256_ofb)(void);
   void (*EVP_bf_cbc)(void);
   void (*EVP_bf_cfb)(void);
   void (*EVP_bf_ecb)(void);
   void (*EVP_bf_ofb)(void);
   void (*EVP_cast5_cbc)(void);
   void (*EVP_cast5_cfb)(void);
   void (*EVP_cast5_ecb)(void);
   void (*EVP_cast5_ofb)(void);
   void (*EVP_cleanup)(void);
   void (*EVP_des_cbc)(void);
   void (*EVP_des_cfb)(void);
   void (*EVP_des_cfb1)(void);
   void (*EVP_des_cfb8)(void);
   void (*EVP_des_ecb)(void);
   void (*EVP_des_ede)(void);
   void (*EVP_des_ede3)(void);
   void (*EVP_des_ede3_cbc)(void);
   void (*EVP_des_ede3_cfb)(void);
   void (*EVP_des_ede3_cfb1)(void);
   void (*EVP_des_ede3_cfb8)(void);
   void (*EVP_des_ede3_ecb)(void);
   void (*EVP_des_ede3_ofb)(void);
   void (*EVP_des_ede_cbc)(void);
   void (*EVP_des_ede_cfb)(void);
   void (*EVP_des_ede_ecb)(void);
   void (*EVP_des_ede_ofb)(void);
   void (*EVP_des_ofb)(void);
   void (*EVP_desx_cbc)(void);
   void (*EVP_dss)(void);
   void (*EVP_dss1)(void);
   void (*EVP_ecdsa)(void);
   void (*EVP_enc_null)(void);
   void (*EVP_get_cipherbyname)(void);
   void (*EVP_get_digestbyname)(void);
   void (*EVP_get_pw_prompt)(void);
   //void (*EVP_idea_cbc)(void);
   //void (*EVP_idea_cfb)(void);
   //void (*EVP_idea_ecb)(void);
   //void (*EVP_idea_ofb)(void);
   //void (*EVP_md2)(void);
   void (*EVP_md4)(void);
   void (*EVP_md5)(void);
   void (*EVP_md_null)(void);
   void (*EVP_rc2_40_cbc)(void);
   void (*EVP_rc2_64_cbc)(void);
   void (*EVP_rc2_cbc)(void);
   void (*EVP_rc2_cfb)(void);
   void (*EVP_rc2_ecb)(void);
   void (*EVP_rc2_ofb)(void);
   void (*EVP_rc4)(void);
   void (*EVP_read_pw_string)(void);
   void (*EVP_set_pw_prompt)(void);
   void (*EVP_sha)(void);
   void (*EVP_sha1)(void);
   void (*HMAC)(void);
   void (*HMAC_CTX_cleanup)(void);
   void (*HMAC_CTX_init)(void);
   void (*HMAC_Final)(void);
   void (*HMAC_Init)(void);
   void (*HMAC_Init_ex)(void);
   void (*HMAC_Update)(void);
   void (*OpenSSL_add_all_ciphers)(void);
   void (*OpenSSL_add_all_digests)(void);
   void (*PEM_ASN1_read)(void);
   void (*PEM_ASN1_read_bio)(void);
   void (*PEM_ASN1_write)(void);
   void (*PEM_ASN1_write_bio)(void);
   void (*PEM_SealFinal)(void);
   void (*PEM_SealInit)(void);
   void (*PEM_SealUpdate)(void);
   void (*PEM_SignFinal)(void);
   void (*PEM_SignInit)(void);
   void (*PEM_SignUpdate)(void);
   void (*PEM_X509_INFO_read)(void);
   void (*PEM_X509_INFO_read_bio)(void);
   void (*PEM_X509_INFO_write_bio)(void);
   void (*PEM_bytes_read_bio)(void);
   void (*PEM_def_callback)(void);
   void (*PEM_dek_info)(void);
   void (*PEM_do_header)(void);
   void (*PEM_get_EVP_CIPHER_INFO)(void);
   void (*PEM_proc_type)(void);
   void (*PEM_read)(void);
   void (*PEM_read_DHparams)(void);
   void (*PEM_read_DSAPrivateKey)(void);
   void (*PEM_read_DSA_PUBKEY)(void);
   void (*PEM_read_DSAparams)(void);
   void (*PEM_read_NETSCAPE_CERT_SEQUENCE)(void);
   void (*PEM_read_PKCS7)(void);
   void (*PEM_read_PKCS8)(void);
   void (*PEM_read_PKCS8_PRIV_KEY_INFO)(void);
   void (*PEM_read_PUBKEY)(void);
   void (*PEM_read_PrivateKey)(void);
   void (*PEM_read_RSAPrivateKey)(void);
   void (*PEM_read_RSAPublicKey)(void);
   void (*PEM_read_RSA_PUBKEY)(void);
   void (*PEM_read_X509)(void);
   void (*PEM_read_X509_AUX)(void);
   void (*PEM_read_X509_CERT_PAIR)(void);
   void (*PEM_read_X509_CRL)(void);
   void (*PEM_read_X509_REQ)(void);
   void (*PEM_read_bio)(void);
   void (*PEM_read_bio_DHparams)(void);
   void (*PEM_read_bio_DSAPrivateKey)(void);
   void (*PEM_read_bio_DSA_PUBKEY)(void);
   void (*PEM_read_bio_DSAparams)(void);
   void (*PEM_read_bio_NETSCAPE_CERT_SEQUENCE)(void);
   void (*PEM_read_bio_PKCS7)(void);
   void (*PEM_read_bio_PKCS8)(void);
   void (*PEM_read_bio_PKCS8_PRIV_KEY_INFO)(void);
   void (*PEM_read_bio_PUBKEY)(void);
   void (*PEM_read_bio_PrivateKey)(void);
   void (*PEM_read_bio_RSAPrivateKey)(void);
   void (*PEM_read_bio_RSAPublicKey)(void);
   void (*PEM_read_bio_RSA_PUBKEY)(void);
   void (*PEM_read_bio_X509)(void);
   void (*PEM_read_bio_X509_AUX)(void);
   void (*PEM_read_bio_X509_CERT_PAIR)(void);
   void (*PEM_read_bio_X509_CRL)(void);
   void (*PEM_read_bio_X509_REQ)(void);
   void (*PEM_write)(void);
   void (*PEM_write_DHparams)(void);
   void (*PEM_write_DSAPrivateKey)(void);
   void (*PEM_write_DSA_PUBKEY)(void);
   void (*PEM_write_DSAparams)(void);
   void (*PEM_write_NETSCAPE_CERT_SEQUENCE)(void);
   void (*PEM_write_PKCS7)(void);
   void (*PEM_write_PKCS8)(void);
   void (*PEM_write_PKCS8PrivateKey)(void);
   void (*PEM_write_PKCS8PrivateKey_nid)(void);
   void (*PEM_write_PKCS8_PRIV_KEY_INFO)(void);
   void (*PEM_write_PUBKEY)(void);
   void (*PEM_write_PrivateKey)(void);
   void (*PEM_write_RSAPrivateKey)(void);
   void (*PEM_write_RSAPublicKey)(void);
   void (*PEM_write_RSA_PUBKEY)(void);
   void (*PEM_write_X509)(void);
   void (*PEM_write_X509_AUX)(void);
   void (*PEM_write_X509_CERT_PAIR)(void);
   void (*PEM_write_X509_CRL)(void);
   void (*PEM_write_X509_REQ)(void);
   void (*PEM_write_X509_REQ_NEW)(void);
   void (*PEM_write_bio)(void);
   void (*PEM_write_bio_DHparams)(void);
   void (*PEM_write_bio_DSAPrivateKey)(void);
   void (*PEM_write_bio_DSA_PUBKEY)(void);
   void (*PEM_write_bio_DSAparams)(void);
   void (*PEM_write_bio_NETSCAPE_CERT_SEQUENCE)(void);
   void (*PEM_write_bio_PKCS7)(void);
   void (*PEM_write_bio_PKCS8)(void);
   void (*PEM_write_bio_PKCS8PrivateKey)(void);
   void (*PEM_write_bio_PKCS8PrivateKey_nid)(void);
   void (*PEM_write_bio_PKCS8_PRIV_KEY_INFO)(void);
   void (*PEM_write_bio_PUBKEY)(void);
   void (*PEM_write_bio_PrivateKey)(void);
   void (*PEM_write_bio_RSAPrivateKey)(void);
   void (*PEM_write_bio_RSAPublicKey)(void);
   void (*PEM_write_bio_RSA_PUBKEY)(void);
   void (*PEM_write_bio_X509)(void);
   void (*PEM_write_bio_X509_AUX)(void);
   void (*PEM_write_bio_X509_CERT_PAIR)(void);
   void (*PEM_write_bio_X509_CRL)(void);
   void (*PEM_write_bio_X509_REQ)(void);
   void (*PEM_write_bio_X509_REQ_NEW)(void);
   void (*PKCS7_add_attrib_smimecap)(void);
   void (*PKCS7_add_attribute)(void);
   void (*PKCS7_add_certificate)(void);
   void (*PKCS7_add_crl)(void);
   void (*PKCS7_add_recipient)(void);
   void (*PKCS7_add_recipient_info)(void);
   void (*PKCS7_add_signature)(void);
   void (*PKCS7_add_signed_attribute)(void);
   void (*PKCS7_add_signer)(void);
   void (*PKCS7_cert_from_signer_info)(void);
   void (*PKCS7_content_new)(void);
   void (*PKCS7_ctrl)(void);
   void (*PKCS7_dataDecode)(void);
   void (*PKCS7_dataFinal)(void);
   void (*PKCS7_dataInit)(void);
   void (*PKCS7_dataVerify)(void);
   void (*PKCS7_decrypt)(void);
   void (*PKCS7_digest_from_attributes)(void);
   void (*PKCS7_dup)(void);
   void (*PKCS7_encrypt)(void);
   void (*PKCS7_free)(void);
   void (*PKCS7_get0_signers)(void);
   void (*PKCS7_get_attribute)(void);
   void (*PKCS7_get_issuer_and_serial)(void);
   void (*PKCS7_get_signed_attribute)(void);
   void (*PKCS7_get_signer_info)(void);
   void (*PKCS7_get_smimecap)(void);
   void (*PKCS7_new)(void);
   void (*PKCS7_set0_type_other)(void);
   void (*PKCS7_set_attributes)(void);
   void (*PKCS7_set_cipher)(void);
   void (*PKCS7_set_content)(void);
   void (*PKCS7_set_digest)(void);
   void (*PKCS7_set_signed_attributes)(void);
   void (*PKCS7_set_type)(void);
   void (*PKCS7_sign)(void);
   void (*PKCS7_signatureVerify)(void);
   void (*PKCS7_simple_smimecap)(void);
   void (*PKCS7_verify)(void);
   //void (*RSAPrivateKey_asn1_meth)(void);
   void (*RSAPrivateKey_dup)(void);
   void (*RSAPublicKey_dup)(void);
   void (*RSA_PKCS1_SSLeay)(void);
   void (*RSA_X931_hash_id)(void);
   void (*RSA_blinding_off)(void);
   void (*RSA_blinding_on)(void);
   void (*RSA_check_key)(void);
   void (*RSA_flags)(void);
   void (*RSA_free)(RSA *rsa);
   int  (*RSA_generate_key_ex)(RSA *rsa, int bits, BIGNUM *e, BN_GENCB *cb);
   void (*RSA_get_default_method)(void);
   void (*RSA_get_ex_data)(void);
   void (*RSA_get_ex_new_index)(void);
   void (*RSA_get_method)(void);
   void (*RSA_memory_lock)(void);
   RSA * (*RSA_new)(void);
   void (*RSA_new_method)(void);
   void (*RSA_null_method)(void);
   void (*RSA_padding_add_PKCS1_OAEP)(void);
   void (*RSA_padding_add_PKCS1_PSS)(void);
   void (*RSA_padding_add_PKCS1_type_1)(void);
   void (*RSA_padding_add_PKCS1_type_2)(void);
   void (*RSA_padding_add_SSLv23)(void);
   void (*RSA_padding_add_X931)(void);
   void (*RSA_padding_add_none)(void);
   void (*RSA_padding_check_PKCS1_OAEP)(void);
   void (*RSA_padding_check_PKCS1_type_1)(void);
   void (*RSA_padding_check_PKCS1_type_2)(void);
   void (*RSA_padding_check_SSLv23)(void);
   void (*RSA_padding_check_X931)(void);
   void (*RSA_padding_check_none)(void);
   void (*RSA_print)(void);
   void (*RSA_print_fp)(void);
   void (*RSA_private_decrypt)(void);
   void (*RSA_private_encrypt)(void);
   void (*RSA_public_decrypt)(void);
   void (*RSA_public_encrypt)(void);
   void (*RSA_set_default_method)(void);
   void (*RSA_set_ex_data)(void);
   void (*RSA_set_method)(void);
   void (*RSA_setup_blinding)(void);
   void (*RSA_sign)(void);
   void (*RSA_sign_ASN1_OCTET_STRING)(void);
   void (*RSA_size)(void);
   void (*RSA_up_ref)(void);
   void (*RSA_verify)(void);
   void (*RSA_verify_ASN1_OCTET_STRING)(void);
   void (*RSA_verify_PKCS1_PSS)(void);
   void (*SHA)(void);
   void (*SHA1)(void);
   void (*SHA1_Final)(void);
   void (*SHA1_Init)(void);
   void (*SHA1_Transform)(void);
   void (*SHA1_Update)(void);
   void (*SHA_Final)(void);
   void (*SHA_Init)(void);
   void (*SHA_Transform)(void);
   void (*SHA_Update)(void);
   void (*SMIME_crlf_copy)(void);
   void (*SMIME_read_PKCS7)(void);
   void (*SMIME_text)(void);
   void (*SMIME_write_PKCS7)(void);
   void (*X509_add1_ext_i2d)(void);
   void (*X509_add1_reject_object)(void);
   void (*X509_add1_trust_object)(void);
   void (*X509_add_ext)(void);
   void (*X509_alias_get0)(void);
   void (*X509_alias_set1)(void);
   //void (*X509_asn1_meth)(void);
   void (*X509_certificate_type)(void);
   void (*X509_check_private_key)(void);
   void (*X509_check_trust)(void);
   void (*X509_cmp)(void);
   void (*X509_cmp_current_time)(void);
   void (*X509_cmp_time)(void);
   void (*X509_delete_ext)(void);
   void (*X509_digest)(void);
   void (*X509_dup)(void);
   void (*X509_find_by_issuer_and_serial)(void);
   void (*X509_find_by_subject)(void);
   void (*X509_free)(void);
   void (*X509_get0_pubkey_bitstr)(void);
   void (*X509_get_default_cert_area)(void);
   void (*X509_get_default_cert_dir)(void);
   void (*X509_get_default_cert_dir_env)(void);
   void (*X509_get_default_cert_file)(void);
   void (*X509_get_default_cert_file_env)(void);
   void (*X509_get_default_private_dir)(void);
   void (*X509_get_ex_data)(void);
   void (*X509_get_ex_new_index)(void);
   void (*X509_get_ext)(void);
   void (*X509_get_ext_by_NID)(void);
   void (*X509_get_ext_by_OBJ)(void);
   void (*X509_get_ext_by_critical)(void);
   void (*X509_get_ext_count)(void);
   void (*X509_get_ext_d2i)(void);
   void (*X509_get_issuer_name)(void);
   void (*X509_get_pubkey)(void);
   void (*X509_get_pubkey_parameters)(void);
   void (*X509_get_serialNumber)(void);
   void (*X509_get_subject_name)(void);
   void (*X509_gmtime_adj)(void);
   void (*X509_issuer_and_serial_cmp)(void);
   void (*X509_issuer_and_serial_hash)(void);
   void (*X509_issuer_name_cmp)(void);
   void (*X509_issuer_name_hash)(void);
   void (*X509_keyid_get0)(void);
   void (*X509_keyid_set1)(void);
   void (*X509_load_cert_crl_file)(void);
   void (*X509_load_cert_file)(void);
   void (*X509_load_crl_file)(void);
   void (*X509_new)(void);
   void (*X509_ocspid_print)(void);
   void (*X509_policy_check)(void);
   void (*X509_policy_level_get0_node)(void);
   void (*X509_policy_level_node_count)(void);
   void (*X509_policy_node_get0_parent)(void);
   void (*X509_policy_node_get0_policy)(void);
   void (*X509_policy_node_get0_qualifiers)(void);
   void (*X509_policy_tree_free)(void);
   void (*X509_policy_tree_get0_level)(void);
   void (*X509_policy_tree_get0_policies)(void);
   void (*X509_policy_tree_get0_user_policies)(void);
   void (*X509_policy_tree_level_count)(void);
   void (*X509_print)(void);
   void (*X509_print_ex)(void);
   void (*X509_print_ex_fp)(void);
   void (*X509_print_fp)(void);
   void (*X509_pubkey_digest)(void);
   void (*X509_reject_clear)(void);
   void (*X509_set_ex_data)(void);
   void (*X509_set_issuer_name)(void);
   void (*X509_set_notAfter)(void);
   void (*X509_set_notBefore)(void);
   void (*X509_set_pubkey)(void);
   void (*X509_set_serialNumber)(void);
   void (*X509_set_subject_name)(void);
   void (*X509_set_version)(void);
   void (*X509_sign)(void);
   void (*X509_signature_print)(void);
   void (*X509_subject_name_cmp)(void);
   void (*X509_subject_name_hash)(void);
   void (*X509_time_adj)(void);
   void (*X509_to_X509_REQ)(void);
   void (*X509_trust_clear)(void);
   void (*X509_verify)(void);
   void (*X509_verify_cert)(void);
   void (*X509_verify_cert_error_string)(void);
   void (*X509at_add1_attr)(void);
   void (*X509at_add1_attr_by_NID)(void);
   void (*X509at_add1_attr_by_OBJ)(void);
   void (*X509at_add1_attr_by_txt)(void);
   void (*X509at_delete_attr)(void);
   void (*X509at_get_attr)(void);
   void (*X509at_get_attr_by_NID)(void);
   void (*X509at_get_attr_by_OBJ)(void);
   void (*X509at_get_attr_count)(void);
   void (*X509v3_add_ext)(void);
   void (*X509v3_delete_ext)(void);
   void (*X509v3_get_ext)(void);
   void (*X509v3_get_ext_by_NID)(void);
   void (*X509v3_get_ext_by_OBJ)(void);
   void (*X509v3_get_ext_by_critical)(void);
   void (*X509v3_get_ext_count)(void);
};

#define SSL_CTX_new                            (OpenSSLBase->SSL_CTX_new)
#define SSLv23_client_method                   (OpenSSLBase->SSLv23_client_method)
#define SSL_new                                (OpenSSLBase->SSL_new)
#define BIO_f_ssl                              (OpenSSLBase->BIO_f_ssl)
#define BIO_new_buffer_ssl_connect             (OpenSSLBase->BIO_new_buffer_ssl_connect)
#define BIO_new_ssl                            (OpenSSLBase->BIO_new_ssl)
#define BIO_new_ssl_connect                    (OpenSSLBase->BIO_new_ssl_connect)
#define BIO_s_connect                          (OpenSSLBase->BIO_s_connect)
#define BIO_s_file                             (OpenSSLBase->BIO_s_file)
#define BIO_s_socket                           (OpenSSLBase->BIO_s_socket)
#define BIO_ssl_copy_session_id                (OpenSSLBase->BIO_ssl_copy_session_id)
#define BIO_ssl_shutdown                       (OpenSSLBase->BIO_ssl_shutdown)
#define DTLSv1_client_method                   (OpenSSLBase->DTLSv1_client_method)
#define DTLSv1_method                          (OpenSSLBase->DTLSv1_method)
#define DTLSv1_server_method                   (OpenSSLBase->DTLSv1_server_method)
#define SSL_CIPHER_description                 (OpenSSLBase->SSL_CIPHER_description)
#define SSL_CIPHER_get_bits                    (OpenSSLBase->SSL_CIPHER_get_bits)
#define SSL_CIPHER_get_name                    (OpenSSLBase->SSL_CIPHER_get_name)
#define SSL_CIPHER_get_version                 (OpenSSLBase->SSL_CIPHER_get_version)
#define SSL_COMP_add_compression_method        (OpenSSLBase->SSL_COMP_add_compression_method)
#define SSL_COMP_get_compression_methods       (OpenSSLBase->SSL_COMP_get_compression_methods)
#define SSL_COMP_get_name                      (OpenSSLBase->SSL_COMP_get_name)
#define SSL_CTX_add_client_CA                  (OpenSSLBase->SSL_CTX_add_client_CA)
#define SSL_CTX_add_session                    (OpenSSLBase->SSL_CTX_add_session)
#define SSL_CTX_callback_ctrl                  (OpenSSLBase->SSL_CTX_callback_ctrl)
#define SSL_CTX_check_private_key              (OpenSSLBase->SSL_CTX_check_private_key)
#define SSL_CTX_ctrl                           (OpenSSLBase->SSL_CTX_ctrl)
#define SSL_CTX_flush_sessions                 (OpenSSLBase->SSL_CTX_flush_sessions)
#define SSL_CTX_free                           (OpenSSLBase->SSL_CTX_free)
#define SSL_CTX_get_cert_store                 (OpenSSLBase->SSL_CTX_get_cert_store)
#define SSL_CTX_get_client_CA_list             (OpenSSLBase->SSL_CTX_get_client_CA_list)
#define SSL_CTX_get_client_cert_cb             (OpenSSLBase->SSL_CTX_get_client_cert_cb)
#define SSL_CTX_get_ex_data                    (OpenSSLBase->SSL_CTX_get_ex_data)
#define SSL_CTX_get_ex_new_index               (OpenSSLBase->SSL_CTX_get_ex_new_index)
#define SSL_CTX_get_info_callback              (OpenSSLBase->SSL_CTX_get_info_callback)
#define SSL_CTX_get_quiet_shutdown             (OpenSSLBase->SSL_CTX_get_quiet_shutdown)
#define SSL_CTX_get_timeout                    (OpenSSLBase->SSL_CTX_get_timeout)
#define SSL_CTX_get_verify_callback            (OpenSSLBase->SSL_CTX_get_verify_callback)
#define SSL_CTX_get_verify_depth               (OpenSSLBase->SSL_CTX_get_verify_depth)
#define SSL_CTX_get_verify_mode                (OpenSSLBase->SSL_CTX_get_verify_mode)
#define SSL_CTX_load_verify_locations          (OpenSSLBase->SSL_CTX_load_verify_locations)
#define SSL_CTX_remove_session                 (OpenSSLBase->SSL_CTX_remove_session)
#define SSL_CTX_sess_get_get_cb                (OpenSSLBase->SSL_CTX_sess_get_get_cb)
#define SSL_CTX_sess_get_new_cb                (OpenSSLBase->SSL_CTX_sess_get_new_cb)
#define SSL_CTX_sess_get_remove_cb             (OpenSSLBase->SSL_CTX_sess_get_remove_cb)
#define SSL_CTX_sess_set_get_cb                (OpenSSLBase->SSL_CTX_sess_set_get_cb)
#define SSL_CTX_sess_set_new_cb                (OpenSSLBase->SSL_CTX_sess_set_new_cb)
#define SSL_CTX_sess_set_remove_cb             (OpenSSLBase->SSL_CTX_sess_set_remove_cb)
#define SSL_CTX_sessions                       (OpenSSLBase->SSL_CTX_sessions)
#define SSL_CTX_set_cert_store                 (OpenSSLBase->SSL_CTX_set_cert_store)
#define SSL_CTX_set_cert_verify_callback       (OpenSSLBase->SSL_CTX_set_cert_verify_callback)
#define SSL_CTX_set_cipher_list                (OpenSSLBase->SSL_CTX_set_cipher_list)
#define SSL_CTX_set_client_CA_list             (OpenSSLBase->SSL_CTX_set_client_CA_list)
#define SSL_CTX_set_client_cert_cb             (OpenSSLBase->SSL_CTX_set_client_cert_cb)
#define SSL_CTX_set_cookie_generate_cb         (OpenSSLBase->SSL_CTX_set_cookie_generate_cb)
#define SSL_CTX_set_cookie_verify_cb           (OpenSSLBase->SSL_CTX_set_cookie_verify_cb)
#define SSL_CTX_set_default_passwd_cb          (OpenSSLBase->SSL_CTX_set_default_passwd_cb)
#define SSL_CTX_set_default_passwd_cb_userdata (OpenSSLBase->SSL_CTX_set_default_passwd_cb_userdata)
#define SSL_CTX_set_default_verify_paths       (OpenSSLBase->SSL_CTX_set_default_verify_paths)
#define SSL_CTX_set_ex_data                    (OpenSSLBase->SSL_CTX_set_ex_data)
#define SSL_CTX_set_generate_session_id        (OpenSSLBase->SSL_CTX_set_generate_session_id)
#define SSL_CTX_set_info_callback              (OpenSSLBase->SSL_CTX_set_info_callback)
#define SSL_CTX_set_msg_callback               (OpenSSLBase->SSL_CTX_set_msg_callback)
#define SSL_CTX_set_purpose                    (OpenSSLBase->SSL_CTX_set_purpose)
#define SSL_CTX_set_quiet_shutdown             (OpenSSLBase->SSL_CTX_set_quiet_shutdown)
#define SSL_CTX_set_session_id_context         (OpenSSLBase->SSL_CTX_set_session_id_context)
#define SSL_CTX_set_ssl_version                (OpenSSLBase->SSL_CTX_set_ssl_version)
#define SSL_CTX_set_timeout                    (OpenSSLBase->SSL_CTX_set_timeout)
#define SSL_CTX_set_tmp_dh_callback            (OpenSSLBase->SSL_CTX_set_tmp_dh_callback)
#define SSL_CTX_set_tmp_rsa_callback           (OpenSSLBase->SSL_CTX_set_tmp_rsa_callback)
#define SSL_CTX_set_trust                      (OpenSSLBase->SSL_CTX_set_trust)
#define SSL_CTX_set_verify                     (OpenSSLBase->SSL_CTX_set_verify)
#define SSL_CTX_set_verify_depth               (OpenSSLBase->SSL_CTX_set_verify_depth)
#define SSL_CTX_use_PrivateKey                 (OpenSSLBase->SSL_CTX_use_PrivateKey)
#define SSL_CTX_use_PrivateKey_ASN1            (OpenSSLBase->SSL_CTX_use_PrivateKey_ASN1)
#define SSL_CTX_use_PrivateKey_file            (OpenSSLBase->SSL_CTX_use_PrivateKey_file)
#define SSL_CTX_use_RSAPrivateKey              (OpenSSLBase->SSL_CTX_use_RSAPrivateKey)
#define SSL_CTX_use_RSAPrivateKey_ASN1         (OpenSSLBase->SSL_CTX_use_RSAPrivateKey_ASN1)
#define SSL_CTX_use_RSAPrivateKey_file         (OpenSSLBase->SSL_CTX_use_RSAPrivateKey_file)
#define SSL_CTX_use_certificate                (OpenSSLBase->SSL_CTX_use_certificate)
#define SSL_CTX_use_certificate_ASN1           (OpenSSLBase->SSL_CTX_use_certificate_ASN1)
#define SSL_CTX_use_certificate_chain_file     (OpenSSLBase->SSL_CTX_use_certificate_chain_file)
#define SSL_CTX_use_certificate_file           (OpenSSLBase->SSL_CTX_use_certificate_file)
#define SSL_SESSION_cmp                        (OpenSSLBase->SSL_SESSION_cmp)
#define SSL_SESSION_free                       (OpenSSLBase->SSL_SESSION_free)
#define SSL_SESSION_get_ex_data                (OpenSSLBase->SSL_SESSION_get_ex_data)
#define SSL_SESSION_get_ex_new_index           (OpenSSLBase->SSL_SESSION_get_ex_new_index)
#define SSL_SESSION_get_id                     (OpenSSLBase->SSL_SESSION_get_id)
#define SSL_SESSION_get_time                   (OpenSSLBase->SSL_SESSION_get_time)
#define SSL_SESSION_get_timeout                (OpenSSLBase->SSL_SESSION_get_timeout)
#define SSL_SESSION_hash                       (OpenSSLBase->SSL_SESSION_hash)
#define SSL_SESSION_new                        (OpenSSLBase->SSL_SESSION_new)
#define SSL_SESSION_print                      (OpenSSLBase->SSL_SESSION_print)
#define SSL_SESSION_print_fp                   (OpenSSLBase->SSL_SESSION_print_fp)
#define SSL_SESSION_set_ex_data                (OpenSSLBase->SSL_SESSION_set_ex_data)
#define SSL_SESSION_set_time                   (OpenSSLBase->SSL_SESSION_set_time)
#define SSL_SESSION_set_timeout                (OpenSSLBase->SSL_SESSION_set_timeout)
#define SSL_accept                             (OpenSSLBase->SSL_accept)
#define SSL_add_client_CA                      (OpenSSLBase->SSL_add_client_CA)
#define SSL_add_dir_cert_subjects_to_stack     (OpenSSLBase->SSL_add_dir_cert_subjects_to_stack)
#define SSL_add_file_cert_subjects_to_stack    (OpenSSLBase->SSL_add_file_cert_subjects_to_stack)
#define SSL_alert_desc_string                  (OpenSSLBase->SSL_alert_desc_string)
#define SSL_alert_desc_string_long             (OpenSSLBase->SSL_alert_desc_string_long)
#define SSL_alert_type_string                  (OpenSSLBase->SSL_alert_type_string)
#define SSL_alert_type_string_long             (OpenSSLBase->SSL_alert_type_string_long)
#define SSL_callback_ctrl                      (OpenSSLBase->SSL_callback_ctrl)
#define SSL_check_private_key                  (OpenSSLBase->SSL_check_private_key)
#define SSL_clear                              (OpenSSLBase->SSL_clear)
#define SSL_connect                            (OpenSSLBase->SSL_connect)
#define SSL_copy_session_id                    (OpenSSLBase->SSL_copy_session_id)
#define SSL_ctrl                               (OpenSSLBase->SSL_ctrl)
#define SSL_do_handshake                       (OpenSSLBase->SSL_do_handshake)
#define SSL_dup                                (OpenSSLBase->SSL_dup)
#define SSL_dup_CA_list                        (OpenSSLBase->SSL_dup_CA_list)
#define SSL_free                               (OpenSSLBase->SSL_free)
#define SSL_get1_session                       (OpenSSLBase->SSL_get1_session)
#define SSL_get_SSL_CTX                        (OpenSSLBase->SSL_get_SSL_CTX)
#define SSL_get_certificate                    (OpenSSLBase->SSL_get_certificate)
#define SSL_get_cipher_list                    (OpenSSLBase->SSL_get_cipher_list)
#define SSL_get_ciphers                        (OpenSSLBase->SSL_get_ciphers)
#define SSL_get_client_CA_list                 (OpenSSLBase->SSL_get_client_CA_list)
#define SSL_get_current_cipher                 (OpenSSLBase->SSL_get_current_cipher)
#define SSL_get_current_compression            (OpenSSLBase->SSL_get_current_compression)
#define SSL_get_current_expansion              (OpenSSLBase->SSL_get_current_expansion)
#define SSL_get_default_timeout                (OpenSSLBase->SSL_get_default_timeout)
#define SSL_get_error                          (OpenSSLBase->SSL_get_error)
#define SSL_get_ex_data                        (OpenSSLBase->SSL_get_ex_data)
#define SSL_get_ex_data_X509_STORE_CTX_idx     (OpenSSLBase->SSL_get_ex_data_X509_STORE_CTX_idx)
#define SSL_get_ex_new_index                   (OpenSSLBase->SSL_get_ex_new_index)
#define SSL_get_fd                             (OpenSSLBase->SSL_get_fd)
#define SSL_get_finished                       (OpenSSLBase->SSL_get_finished)
#define SSL_get_info_callback                  (OpenSSLBase->SSL_get_info_callback)
#define SSL_get_peer_cert_chain                (OpenSSLBase->SSL_get_peer_cert_chain)
#define SSL_get_peer_certificate               (OpenSSLBase->SSL_get_peer_certificate)
#define SSL_get_peer_finished                  (OpenSSLBase->SSL_get_peer_finished)
#define SSL_get_privatekey                     (OpenSSLBase->SSL_get_privatekey)
#define SSL_get_quiet_shutdown                 (OpenSSLBase->SSL_get_quiet_shutdown)
#define SSL_get_rbio                           (OpenSSLBase->SSL_get_rbio)
#define SSL_get_read_ahead                     (OpenSSLBase->SSL_get_read_ahead)
#define SSL_get_rfd                            (OpenSSLBase->SSL_get_rfd)
#define SSL_get_session                        (OpenSSLBase->SSL_get_session)
#define SSL_get_shared_ciphers                 (OpenSSLBase->SSL_get_shared_ciphers)
#define SSL_get_shutdown                       (OpenSSLBase->SSL_get_shutdown)
#define SSL_get_ssl_method                     (OpenSSLBase->SSL_get_ssl_method)
#define SSL_get_verify_callback                (OpenSSLBase->SSL_get_verify_callback)
#define SSL_get_verify_depth                   (OpenSSLBase->SSL_get_verify_depth)
#define SSL_get_verify_mode                    (OpenSSLBase->SSL_get_verify_mode)
#define SSL_get_verify_result                  (OpenSSLBase->SSL_get_verify_result)
#define SSL_get_version                        (OpenSSLBase->SSL_get_version)
#define SSL_get_wbio                           (OpenSSLBase->SSL_get_wbio)
#define SSL_get_wfd                            (OpenSSLBase->SSL_get_wfd)
#define SSL_has_matching_session_id            (OpenSSLBase->SSL_has_matching_session_id)
#define SSL_library_init                       (OpenSSLBase->SSL_library_init)
#define SSL_load_client_CA_file                (OpenSSLBase->SSL_load_client_CA_file)
#define SSL_load_error_strings                 (OpenSSLBase->SSL_load_error_strings)
#define SSL_peek                               (OpenSSLBase->SSL_peek)
#define SSL_pending                            (OpenSSLBase->SSL_pending)
#define SSL_read                               (OpenSSLBase->SSL_read)
#define SSL_renegotiate                        (OpenSSLBase->SSL_renegotiate)
#define SSL_renegotiate_pending                (OpenSSLBase->SSL_renegotiate_pending)
#define SSL_rstate_string                      (OpenSSLBase->SSL_rstate_string)
#define SSL_rstate_string_long                 (OpenSSLBase->SSL_rstate_string_long)
#define SSL_set_SSL_CTX                        (OpenSSLBase->SSL_set_SSL_CTX)
#define SSL_set_accept_state                   (OpenSSLBase->SSL_set_accept_state)
#define SSL_set_bio                            (OpenSSLBase->SSL_set_bio)
#define SSL_set_cipher_list                    (OpenSSLBase->SSL_set_cipher_list)
#define SSL_set_client_CA_list                 (OpenSSLBase->SSL_set_client_CA_list)
#define SSL_set_connect_state                  (OpenSSLBase->SSL_set_connect_state)
#define SSL_set_ex_data                        (OpenSSLBase->SSL_set_ex_data)
#define SSL_set_fd                             (OpenSSLBase->SSL_set_fd)
#define SSL_set_generate_session_id            (OpenSSLBase->SSL_set_generate_session_id)
#define SSL_set_info_callback                  (OpenSSLBase->SSL_set_info_callback)
#define SSL_set_msg_callback                   (OpenSSLBase->SSL_set_msg_callback)
#define SSL_set_purpose                        (OpenSSLBase->SSL_set_purpose)
#define SSL_set_quiet_shutdown                 (OpenSSLBase->SSL_set_quiet_shutdown)
#define SSL_set_read_ahead                     (OpenSSLBase->SSL_set_read_ahead)
#define SSL_set_rfd                            (OpenSSLBase->SSL_set_rfd)
#define SSL_set_session                        (OpenSSLBase->SSL_set_session)
#define SSL_set_session_id_context             (OpenSSLBase->SSL_set_session_id_context)
#define SSL_set_shutdown                       (OpenSSLBase->SSL_set_shutdown)
#define SSL_set_ssl_method                     (OpenSSLBase->SSL_set_ssl_method)
#define SSL_set_tmp_dh_callback                (OpenSSLBase->SSL_set_tmp_dh_callback)
#define SSL_set_tmp_rsa_callback               (OpenSSLBase->SSL_set_tmp_rsa_callback)
#define SSL_set_trust                          (OpenSSLBase->SSL_set_trust)
#define SSL_set_verify                         (OpenSSLBase->SSL_set_verify)
#define SSL_set_verify_depth                   (OpenSSLBase->SSL_set_verify_depth)
#define SSL_set_verify_result                  (OpenSSLBase->SSL_set_verify_result)
#define SSL_set_wfd                            (OpenSSLBase->SSL_set_wfd)
#define SSL_shutdown                           (OpenSSLBase->SSL_shutdown)
#define SSL_state                              (OpenSSLBase->SSL_state)
#define SSL_state_string                       (OpenSSLBase->SSL_state_string)
#define SSL_state_string_long                  (OpenSSLBase->SSL_state_string_long)
#define SSL_use_PrivateKey                     (OpenSSLBase->SSL_use_PrivateKey)
#define SSL_use_PrivateKey_ASN1                (OpenSSLBase->SSL_use_PrivateKey_ASN1)
#define SSL_use_PrivateKey_file                (OpenSSLBase->SSL_use_PrivateKey_file)
#define SSL_use_RSAPrivateKey                  (OpenSSLBase->SSL_use_RSAPrivateKey)
#define SSL_use_RSAPrivateKey_ASN1             (OpenSSLBase->SSL_use_RSAPrivateKey_ASN1)
#define SSL_use_RSAPrivateKey_file             (OpenSSLBase->SSL_use_RSAPrivateKey_file)
#define SSL_use_certificate                    (OpenSSLBase->SSL_use_certificate)
#define SSL_use_certificate_ASN1               (OpenSSLBase->SSL_use_certificate_ASN1)
#define SSL_use_certificate_file               (OpenSSLBase->SSL_use_certificate_file)
#define SSL_version                            (OpenSSLBase->SSL_version)
#define SSL_want                               (OpenSSLBase->SSL_want)
#define SSL_write                              (OpenSSLBase->SSL_write)
#define SSLv23_method                          (OpenSSLBase->SSLv23_method)
#define SSLv23_server_method                   (OpenSSLBase->SSLv23_server_method)
#define SSLv2_client_method                    (OpenSSLBase->SSLv2_client_method)
#define SSLv2_method                           (OpenSSLBase->SSLv2_method)
#define SSLv2_server_method                    (OpenSSLBase->SSLv2_server_method)
#define SSLv3_client_method                    (OpenSSLBase->SSLv3_client_method)
#define SSLv3_method                           (OpenSSLBase->SSLv3_method)
#define SSLv3_server_method                    (OpenSSLBase->SSLv3_server_method)
#define TLSv1_client_method                    (OpenSSLBase->TLSv1_client_method)
#define TLSv1_method                           (OpenSSLBase->TLSv1_method)
#define TLSv1_server_method                    (OpenSSLBase->TLSv1_server_method)
#define X509_NAME_cmp                          (OpenSSLBase->X509_NAME_cmp)
#define X509_NAME_dup                          (OpenSSLBase->X509_NAME_dup)
#define X509_NAME_free                         (OpenSSLBase->X509_NAME_free)
#define X509_STORE_CTX_cleanup                 (OpenSSLBase->X509_STORE_CTX_cleanup)
#define X509_STORE_CTX_get0_param              (OpenSSLBase->X509_STORE_CTX_get0_param)
#define X509_STORE_CTX_get_ex_new_index        (OpenSSLBase->X509_STORE_CTX_get_ex_new_index)
#define X509_STORE_CTX_init                    (OpenSSLBase->X509_STORE_CTX_init)
#define X509_STORE_CTX_set_default             (OpenSSLBase->X509_STORE_CTX_set_default)
#define X509_STORE_CTX_set_ex_data             (OpenSSLBase->X509_STORE_CTX_set_ex_data)
#define X509_STORE_CTX_set_verify_cb           (OpenSSLBase->X509_STORE_CTX_set_verify_cb)
#define X509_STORE_free                        (OpenSSLBase->X509_STORE_free)
#define X509_STORE_get_by_subject              (OpenSSLBase->X509_STORE_get_by_subject)
#define X509_STORE_load_locations              (OpenSSLBase->X509_STORE_load_locations)
#define X509_STORE_new                         (OpenSSLBase->X509_STORE_new)
#define X509_STORE_set_default_paths           (OpenSSLBase->X509_STORE_set_default_paths)
#define X509_VERIFY_PARAM_free                 (OpenSSLBase->X509_VERIFY_PARAM_free)
#define X509_VERIFY_PARAM_get_depth            (OpenSSLBase->X509_VERIFY_PARAM_get_depth)
#define X509_VERIFY_PARAM_inherit              (OpenSSLBase->X509_VERIFY_PARAM_inherit)
#define X509_VERIFY_PARAM_new                  (OpenSSLBase->X509_VERIFY_PARAM_new)
#define X509_VERIFY_PARAM_set_depth            (OpenSSLBase->X509_VERIFY_PARAM_set_depth)
#define X509_VERIFY_PARAM_set_purpose          (OpenSSLBase->X509_VERIFY_PARAM_set_purpose)
#define X509_VERIFY_PARAM_set_trust            (OpenSSLBase->X509_VERIFY_PARAM_set_trust)

// Crypto

#define ASN1_add_oid_module                     (OpenSSLBase->ASN1_add_oid_module)
#define ASN1_check_infinite_end                 (OpenSSLBase->ASN1_check_infinite_end)
#define ASN1_const_check_infinite_end           (OpenSSLBase->ASN1_const_check_infinite_end)
#define ASN1_d2i_bio                            (OpenSSLBase->ASN1_d2i_bio)
#define ASN1_d2i_fp                             (OpenSSLBase->ASN1_d2i_fp)
#define ASN1_digest                             (OpenSSLBase->ASN1_digest)
#define ASN1_dup                                (OpenSSLBase->ASN1_dup)
#define ASN1_generate_nconf                     (OpenSSLBase->ASN1_generate_nconf)
#define ASN1_generate_v3                        (OpenSSLBase->ASN1_generate_v3)
#define ASN1_get_object                         (OpenSSLBase->ASN1_get_object)
#define ASN1_i2d_bio                            (OpenSSLBase->ASN1_i2d_bio)
#define ASN1_i2d_fp                             (OpenSSLBase->ASN1_i2d_fp)
#define ASN1_item_d2i                           (OpenSSLBase->ASN1_item_d2i)
#define ASN1_item_d2i_bio                       (OpenSSLBase->ASN1_item_d2i_bio)
#define ASN1_item_d2i_fp                        (OpenSSLBase->ASN1_item_d2i_fp)
#define ASN1_item_digest                        (OpenSSLBase->ASN1_item_digest)
#define ASN1_item_dup                           (OpenSSLBase->ASN1_item_dup)
#define ASN1_item_free                          (OpenSSLBase->ASN1_item_free)
#define ASN1_item_i2d                           (OpenSSLBase->ASN1_item_i2d)
#define ASN1_item_i2d_bio                       (OpenSSLBase->ASN1_item_i2d_bio)
#define ASN1_item_i2d_fp                        (OpenSSLBase->ASN1_item_i2d_fp)
#define ASN1_item_ndef_i2d                      (OpenSSLBase->ASN1_item_ndef_i2d)
#define ASN1_item_new                           (OpenSSLBase->ASN1_item_new)
#define ASN1_item_pack                          (OpenSSLBase->ASN1_item_pack)
#define ASN1_item_sign                          (OpenSSLBase->ASN1_item_sign)
#define ASN1_item_unpack                        (OpenSSLBase->ASN1_item_unpack)
#define ASN1_item_verify                        (OpenSSLBase->ASN1_item_verify)
#define ASN1_mbstring_copy                      (OpenSSLBase->ASN1_mbstring_copy)
#define ASN1_mbstring_ncopy                     (OpenSSLBase->ASN1_mbstring_ncopy)
#define ASN1_object_size                        (OpenSSLBase->ASN1_object_size)
#define ASN1_pack_string                        (OpenSSLBase->ASN1_pack_string)
#define ASN1_parse                              (OpenSSLBase->ASN1_parse)
#define ASN1_parse_dump                         (OpenSSLBase->ASN1_parse_dump)
#define ASN1_put_eoc                            (OpenSSLBase->ASN1_put_eoc)
#define ASN1_put_object                         (OpenSSLBase->ASN1_put_object)
#define ASN1_seq_pack                           (OpenSSLBase->ASN1_seq_pack)
#define ASN1_seq_unpack                         (OpenSSLBase->ASN1_seq_unpack)
#define ASN1_sign                               (OpenSSLBase->ASN1_sign)
#define ASN1_tag2bit                            (OpenSSLBase->ASN1_tag2bit)
#define ASN1_tag2str                            (OpenSSLBase->ASN1_tag2str)
#define ASN1_unpack_string                      (OpenSSLBase->ASN1_unpack_string)
#define ASN1_verify                             (OpenSSLBase->ASN1_verify)
#define BIO_accept                              (OpenSSLBase->BIO_accept)
#define BIO_callback_ctrl                       (OpenSSLBase->BIO_callback_ctrl)
#define BIO_clear_flags                         (OpenSSLBase->BIO_clear_flags)
#define BIO_copy_next_retry                     (OpenSSLBase->BIO_copy_next_retry)
#define BIO_ctrl                                (OpenSSLBase->BIO_ctrl)
#define BIO_ctrl_get_read_request               (OpenSSLBase->BIO_ctrl_get_read_request)
#define BIO_ctrl_get_write_guarantee            (OpenSSLBase->BIO_ctrl_get_write_guarantee)
#define BIO_ctrl_pending                        (OpenSSLBase->BIO_ctrl_pending)
#define BIO_ctrl_reset_read_request             (OpenSSLBase->BIO_ctrl_reset_read_request)
#define BIO_ctrl_wpending                       (OpenSSLBase->BIO_ctrl_wpending)
#define BIO_debug_callback                      (OpenSSLBase->BIO_debug_callback)
#define BIO_dump                                (OpenSSLBase->BIO_dump)
#define BIO_dump_cb                             (OpenSSLBase->BIO_dump_cb)
#define BIO_dump_fp                             (OpenSSLBase->BIO_dump_fp)
#define BIO_dump_indent                         (OpenSSLBase->BIO_dump_indent)
#define BIO_dump_indent_cb                      (OpenSSLBase->BIO_dump_indent_cb)
#define BIO_dump_indent_fp                      (OpenSSLBase->BIO_dump_indent_fp)
#define BIO_dup_chain                           (OpenSSLBase->BIO_dup_chain)
#define BIO_f_base64                            (OpenSSLBase->BIO_f_base64)
#define BIO_f_buffer                            (OpenSSLBase->BIO_f_buffer)
#define BIO_f_cipher                            (OpenSSLBase->BIO_f_cipher)
#define BIO_f_md                                (OpenSSLBase->BIO_f_md)
#define BIO_f_nbio_test                         (OpenSSLBase->BIO_f_nbio_test)
#define BIO_f_null                              (OpenSSLBase->BIO_f_null)
#define BIO_f_reliable                          (OpenSSLBase->BIO_f_reliable)
#define BIO_fd_non_fatal_error                  (OpenSSLBase->BIO_fd_non_fatal_error)
#define BIO_fd_should_retry                     (OpenSSLBase->BIO_fd_should_retry)
#define BIO_find_type                           (OpenSSLBase->BIO_find_type)
#define BIO_free                                (OpenSSLBase->BIO_free)
#define BIO_free_all                            (OpenSSLBase->BIO_free_all)
#define BIO_get_accept_socket                   (OpenSSLBase->BIO_get_accept_socket)
#define BIO_get_callback                        (OpenSSLBase->BIO_get_callback)
#define BIO_get_callback_arg                    (OpenSSLBase->BIO_get_callback_arg)
#define BIO_get_ex_data                         (OpenSSLBase->BIO_get_ex_data)
#define BIO_get_ex_new_index                    (OpenSSLBase->BIO_get_ex_new_index)
#define BIO_get_host_ip                         (OpenSSLBase->BIO_get_host_ip)
#define BIO_get_port                            (OpenSSLBase->BIO_get_port)
#define BIO_get_retry_BIO                       (OpenSSLBase->BIO_get_retry_BIO)
#define BIO_get_retry_reason                    (OpenSSLBase->BIO_get_retry_reason)
#define BIO_gethostbyname                       (OpenSSLBase->BIO_gethostbyname)
#define BIO_gets                                (OpenSSLBase->BIO_gets)
#define BIO_indent                              (OpenSSLBase->BIO_indent)
#define BIO_int_ctrl                            (OpenSSLBase->BIO_int_ctrl)
#define BIO_method_name                         (OpenSSLBase->BIO_method_name)
#define BIO_method_type                         (OpenSSLBase->BIO_method_type)
#define BIO_new                                 (OpenSSLBase->BIO_new)
#define BIO_new_accept                          (OpenSSLBase->BIO_new_accept)
#define BIO_new_bio_pair                        (OpenSSLBase->BIO_new_bio_pair)
#define BIO_new_connect                         (OpenSSLBase->BIO_new_connect)
#define BIO_new_dgram                           (OpenSSLBase->BIO_new_dgram)
#define BIO_new_fd                              (OpenSSLBase->BIO_new_fd)
#define BIO_new_file                            (OpenSSLBase->BIO_new_file)
#define BIO_new_fp                              (OpenSSLBase->BIO_new_fp)
#define BIO_new_mem_buf                          (OpenSSLBase->BIO_new_mem_buf)
#define BIO_new_socket                           (OpenSSLBase->BIO_new_socket)
#define BIO_next                                 (OpenSSLBase->BIO_next)
#define BIO_nread                                (OpenSSLBase->BIO_nread)
#define BIO_number_read                          (OpenSSLBase->BIO_number_read)
#define BIO_number_written                       (OpenSSLBase->BIO_number_written)
#define BIO_nwrite                               (OpenSSLBase->BIO_nwrite)
#define BIO_pop                                  (OpenSSLBase->BIO_pop)
#define BIO_printf                               (OpenSSLBase->BIO_printf)
#define BIO_ptr_ctrl                             (OpenSSLBase->BIO_ptr_ctrl)
#define BIO_push                                 (OpenSSLBase->BIO_push)
#define BIO_puts                                 (OpenSSLBase->BIO_puts)
#define BIO_read                                 (OpenSSLBase->BIO_read)
#define BIO_set                                  (OpenSSLBase->BIO_set)
#define BIO_set_callback                         (OpenSSLBase->BIO_set_callback)
#define BIO_set_callback_arg                     (OpenSSLBase->BIO_set_callback_arg)
#define BIO_set_cipher                           (OpenSSLBase->BIO_set_cipher)
#define BIO_set_ex_data                          (OpenSSLBase->BIO_set_ex_data)
#define BIO_set_flags                            (OpenSSLBase->BIO_set_flags)
#define BIO_set_tcp_ndelay                       (OpenSSLBase->BIO_set_tcp_ndelay)
#define BIO_snprintf                             (OpenSSLBase->BIO_snprintf)
#define BIO_sock_cleanup                         (OpenSSLBase->BIO_sock_cleanup)
#define BIO_sock_error                           (OpenSSLBase->BIO_sock_error)
#define BIO_sock_init                            (OpenSSLBase->BIO_sock_init)
#define BIO_sock_non_fatal_error                 (OpenSSLBase->BIO_sock_non_fatal_error)
#define BIO_sock_should_retry                    (OpenSSLBase->BIO_sock_should_retry)
#define BIO_socket_ioctl                         (OpenSSLBase->BIO_socket_ioctl)
#define BIO_socket_nbio                          (OpenSSLBase->BIO_socket_nbio)
#define BIO_test_flags                           (OpenSSLBase->BIO_test_flags)
#define BIO_vfree                                (OpenSSLBase->BIO_vfree)
#define BIO_vprintf                              (OpenSSLBase->BIO_vprintf)
#define BIO_vsnprintf                            (OpenSSLBase->BIO_vsnprintf)
#define BIO_write                                (OpenSSLBase->BIO_write)
#define BN_CTX_end                               (OpenSSLBase->BN_CTX_end)
#define BN_CTX_free                              (OpenSSLBase->BN_CTX_free)
#define BN_CTX_get                               (OpenSSLBase->BN_CTX_get)
#define BN_CTX_init                              (OpenSSLBase->BN_CTX_init)
#define BN_CTX_new                               (OpenSSLBase->BN_CTX_new)
#define BN_CTX_start                             (OpenSSLBase->BN_CTX_start)
#define BN_GENCB_call                            (OpenSSLBase->BN_GENCB_call)
#define BN_GF2m_add                              (OpenSSLBase->BN_GF2m_add)
#define BN_GF2m_arr2poly                         (OpenSSLBase->BN_GF2m_arr2poly)
#define BN_GF2m_mod                              (OpenSSLBase->BN_GF2m_mod)
#define BN_GF2m_mod_arr                          (OpenSSLBase->BN_GF2m_mod_arr)
#define BN_GF2m_mod_div                          (OpenSSLBase->BN_GF2m_mod_div)
#define BN_GF2m_mod_div_arr                      (OpenSSLBase->BN_GF2m_mod_div_arr)
#define BN_GF2m_mod_exp                          (OpenSSLBase->BN_GF2m_mod_exp)
#define BN_GF2m_mod_exp_arr                      (OpenSSLBase->BN_GF2m_mod_exp_arr)
#define BN_GF2m_mod_inv                          (OpenSSLBase->BN_GF2m_mod_inv)
#define BN_GF2m_mod_inv_arr                      (OpenSSLBase->BN_GF2m_mod_inv_arr)
#define BN_GF2m_mod_mul                          (OpenSSLBase->BN_GF2m_mod_mul)
#define BN_GF2m_mod_mul_arr                      (OpenSSLBase->BN_GF2m_mod_mul_arr)
#define BN_GF2m_mod_solve_quad                   (OpenSSLBase->BN_GF2m_mod_solve_quad)
#define BN_GF2m_mod_solve_quad_arr               (OpenSSLBase->BN_GF2m_mod_solve_quad_arr)
#define BN_GF2m_mod_sqr                          (OpenSSLBase->BN_GF2m_mod_sqr)
#define BN_GF2m_mod_sqr_arr                      (OpenSSLBase->BN_GF2m_mod_sqr_arr)
#define BN_GF2m_mod_sqrt                         (OpenSSLBase->BN_GF2m_mod_sqrt)
#define BN_GF2m_mod_sqrt_arr                     (OpenSSLBase->BN_GF2m_mod_sqrt_arr)
#define BN_GF2m_poly2arr                         (OpenSSLBase->BN_GF2m_poly2arr)
#define BN_add                                   (OpenSSLBase->BN_add)
#define BN_add_word                              (OpenSSLBase->BN_add_word)
#define BN_bin2bn                                (OpenSSLBase->BN_bin2bn)
#define BN_bn2bin                                (OpenSSLBase->BN_bn2bin)
#define BN_bn2dec                                (OpenSSLBase->BN_bn2dec)
#define BN_bn2hex                                (OpenSSLBase->BN_bn2hex)
#define BN_bn2mpi                                (OpenSSLBase->BN_bn2mpi)
#define BN_bntest_rand                           (OpenSSLBase->BN_bntest_rand)
#define BN_clear                                 (OpenSSLBase->BN_clear)
#define BN_clear_bit                             (OpenSSLBase->BN_clear_bit)
#define BN_clear_free                            (OpenSSLBase->BN_clear_free)
#define BN_cmp                                   (OpenSSLBase->BN_cmp)
#define BN_copy                                  (OpenSSLBase->BN_copy)
#define BN_dec2bn                                (OpenSSLBase->BN_dec2bn)
#define BN_div                                   (OpenSSLBase->BN_div)
#define BN_div_recp                              (OpenSSLBase->BN_div_recp)
#define BN_div_word                              (OpenSSLBase->BN_div_word)
#define BN_dup                                   (OpenSSLBase->BN_dup)
#define BN_exp                                   (OpenSSLBase->BN_exp)
#define BN_free                                  (OpenSSLBase->BN_free)
#define BN_from_montgomery                       (OpenSSLBase->BN_from_montgomery)
#define BN_gcd                                   (OpenSSLBase->BN_gcd)
#define BN_generate_prime                        (OpenSSLBase->BN_generate_prime)
#define BN_generate_prime_ex                     (OpenSSLBase->BN_generate_prime_ex)
#define BN_get0_nist_prime_192                   (OpenSSLBase->BN_get0_nist_prime_192)
#define BN_get0_nist_prime_224                   (OpenSSLBase->BN_get0_nist_prime_224)
#define BN_get0_nist_prime_256                   (OpenSSLBase->BN_get0_nist_prime_256)
#define BN_get0_nist_prime_384                   (OpenSSLBase->BN_get0_nist_prime_384)
#define BN_get0_nist_prime_521                   (OpenSSLBase->BN_get0_nist_prime_521)
#define BN_get_params                            (OpenSSLBase->BN_get_params)
#define BN_get_word                              (OpenSSLBase->BN_get_word)
#define BN_hex2bn                                (OpenSSLBase->BN_hex2bn)
#define BN_init                                  (OpenSSLBase->BN_init)
#define BN_is_bit_set                            (OpenSSLBase->BN_is_bit_set)
#define BN_is_prime                              (OpenSSLBase->BN_is_prime)
#define BN_is_prime_ex                           (OpenSSLBase->BN_is_prime_ex)
#define BN_is_prime_fasttest                     (OpenSSLBase->BN_is_prime_fasttest)
#define BN_is_prime_fasttest_ex                  (OpenSSLBase->BN_is_prime_fasttest_ex)
#define BN_kronecker                             (OpenSSLBase->BN_kronecker)
#define BN_lshift                                (OpenSSLBase->BN_lshift)
#define BN_lshift1                               (OpenSSLBase->BN_lshift1)
#define BN_mask_bits                             (OpenSSLBase->BN_mask_bits)
#define BN_mod_add                               (OpenSSLBase->BN_mod_add)
#define BN_mod_add_quick                         (OpenSSLBase->BN_mod_add_quick)
#define BN_mod_exp                               (OpenSSLBase->BN_mod_exp)
#define BN_mod_exp2_mont                         (OpenSSLBase->BN_mod_exp2_mont)
#define BN_mod_exp_mont                          (OpenSSLBase->BN_mod_exp_mont)
#define BN_mod_exp_mont_consttime                (OpenSSLBase->BN_mod_exp_mont_consttime)
#define BN_mod_exp_mont_word                     (OpenSSLBase->BN_mod_exp_mont_word)
#define BN_mod_exp_recp                          (OpenSSLBase->BN_mod_exp_recp)
#define BN_mod_exp_simple                        (OpenSSLBase->BN_mod_exp_simple)
#define BN_mod_inverse                           (OpenSSLBase->BN_mod_inverse)
#define BN_mod_lshift                            (OpenSSLBase->BN_mod_lshift)
#define BN_mod_lshift1                           (OpenSSLBase->BN_mod_lshift1)
#define BN_mod_lshift1_quick                     (OpenSSLBase->BN_mod_lshift1_quick)
#define BN_mod_lshift_quick                      (OpenSSLBase->BN_mod_lshift_quick)
#define BN_mod_mul                               (OpenSSLBase->BN_mod_mul)
#define BN_mod_mul_montgomery                    (OpenSSLBase->BN_mod_mul_montgomery)
#define BN_mod_mul_reciprocal                    (OpenSSLBase->BN_mod_mul_reciprocal)
#define BN_mod_sqr                               (OpenSSLBase->BN_mod_sqr)
#define BN_mod_sqrt                              (OpenSSLBase->BN_mod_sqrt)
#define BN_mod_sub                               (OpenSSLBase->BN_mod_sub)
#define BN_mod_sub_quick                         (OpenSSLBase->BN_mod_sub_quick)
#define BN_mod_word                              (OpenSSLBase->BN_mod_word)
#define BN_mpi2bn                                (OpenSSLBase->BN_mpi2bn)
#define BN_mul                                   (OpenSSLBase->BN_mul)
#define BN_mul_word                              (OpenSSLBase->BN_mul_word)
#define BN_new                                   (OpenSSLBase->BN_new)
#define BN_nist_mod_192                          (OpenSSLBase->BN_nist_mod_192)
#define BN_nist_mod_224                          (OpenSSLBase->BN_nist_mod_224)
#define BN_nist_mod_256                          (OpenSSLBase->BN_nist_mod_256)
#define BN_nist_mod_384                          (OpenSSLBase->BN_nist_mod_384)
#define BN_nist_mod_521                          (OpenSSLBase->BN_nist_mod_521)
#define BN_nnmod                                 (OpenSSLBase->BN_nnmod)
#define BN_num_bits                              (OpenSSLBase->BN_num_bits)
#define BN_num_bits_word                         (OpenSSLBase->BN_num_bits_word)
#define BN_options                               (OpenSSLBase->BN_options)
#define BN_print                                 (OpenSSLBase->BN_print)
#define BN_print_fp                              (OpenSSLBase->BN_print_fp)
#define BN_pseudo_rand                           (OpenSSLBase->BN_pseudo_rand)
#define BN_pseudo_rand_range                     (OpenSSLBase->BN_pseudo_rand_range)
#define BN_rand                                  (OpenSSLBase->BN_rand)
#define BN_rand_range                            (OpenSSLBase->BN_rand_range)
#define BN_reciprocal                            (OpenSSLBase->BN_reciprocal)
#define BN_rshift                                (OpenSSLBase->BN_rshift)
#define BN_rshift1                               (OpenSSLBase->BN_rshift1)
#define BN_set_bit                               (OpenSSLBase->BN_set_bit)
#define BN_set_negative                          (OpenSSLBase->BN_set_negative)
#define BN_set_params                            (OpenSSLBase->BN_set_params)
#define BN_set_word                              (OpenSSLBase->BN_set_word)
#define BN_sqr                                   (OpenSSLBase->BN_sqr)
#define BN_sub                                   (OpenSSLBase->BN_sub)
#define BN_sub_word                              (OpenSSLBase->BN_sub_word)
#define BN_swap                                  (OpenSSLBase->BN_swap)
#define BN_to_ASN1_ENUMERATED                    (OpenSSLBase->BN_to_ASN1_ENUMERATED)
#define BN_to_ASN1_INTEGER                       (OpenSSLBase->BN_to_ASN1_INTEGER)
#define BN_uadd                                  (OpenSSLBase->BN_uadd)
#define BN_ucmp                                  (OpenSSLBase->BN_ucmp)
#define BN_usub                                  (OpenSSLBase->BN_usub)
#define BN_value_one                             (OpenSSLBase->BN_value_one)
#define BUF_MEM_free                             (OpenSSLBase->BUF_MEM_free)
#define BUF_MEM_grow                             (OpenSSLBase->BUF_MEM_grow)
#define BUF_MEM_grow_clean                       (OpenSSLBase->BUF_MEM_grow_clean)
#define BUF_MEM_new                              (OpenSSLBase->BUF_MEM_new)
#define BUF_memdup                               (OpenSSLBase->BUF_memdup)
#define BUF_strdup                               (OpenSSLBase->BUF_strdup)
#define BUF_strlcat                              (OpenSSLBase->BUF_strlcat)
#define BUF_strlcpy                              (OpenSSLBase->BUF_strlcpy)
#define BUF_strndup                              (OpenSSLBase->BUF_strndup)
#define CRYPTO_add_lock                          (OpenSSLBase->CRYPTO_add_lock)
#define CRYPTO_cleanup_all_ex_data               (OpenSSLBase->CRYPTO_cleanup_all_ex_data)
#define CRYPTO_dbg_free                          (OpenSSLBase->CRYPTO_dbg_free)
#define CRYPTO_dbg_get_options                   (OpenSSLBase->CRYPTO_dbg_get_options)
#define CRYPTO_dbg_malloc                        (OpenSSLBase->CRYPTO_dbg_malloc)
#define CRYPTO_dbg_realloc                       (OpenSSLBase->CRYPTO_dbg_realloc)
#define CRYPTO_dbg_set_options                   (OpenSSLBase->CRYPTO_dbg_set_options)
#define CRYPTO_destroy_dynlockid                 (OpenSSLBase->CRYPTO_destroy_dynlockid)
#define CRYPTO_dup_ex_data                       (OpenSSLBase->CRYPTO_dup_ex_data)
#define CRYPTO_ex_data_new_class                 (OpenSSLBase->CRYPTO_ex_data_new_class)
#define CRYPTO_free                              (OpenSSLBase->CRYPTO_free)
#define CRYPTO_free_ex_data                      (OpenSSLBase->CRYPTO_free_ex_data)
#define CRYPTO_free_locked                       (OpenSSLBase->CRYPTO_free_locked)
#define CRYPTO_get_add_lock_callback             (OpenSSLBase->CRYPTO_get_add_lock_callback)
#define CRYPTO_get_dynlock_create_callback       (OpenSSLBase->CRYPTO_get_dynlock_create_callback)
#define CRYPTO_get_dynlock_destroy_callback      (OpenSSLBase->CRYPTO_get_dynlock_destroy_callback)
#define CRYPTO_get_dynlock_lock_callback         (OpenSSLBase->CRYPTO_get_dynlock_lock_callback)
#define CRYPTO_get_dynlock_value                 (OpenSSLBase->CRYPTO_get_dynlock_value)
#define CRYPTO_get_ex_data                       (OpenSSLBase->CRYPTO_get_ex_data)
#define CRYPTO_get_ex_data_implementation        (OpenSSLBase->CRYPTO_get_ex_data_implementation)
#define CRYPTO_get_ex_new_index                  (OpenSSLBase->CRYPTO_get_ex_new_index)
#define CRYPTO_get_id_callback                   (OpenSSLBase->CRYPTO_get_id_callback)
#define CRYPTO_get_lock_name                     (OpenSSLBase->CRYPTO_get_lock_name)
#define CRYPTO_get_locked_mem_ex_functions       (OpenSSLBase->CRYPTO_get_locked_mem_ex_functions)
#define CRYPTO_get_locked_mem_functions          (OpenSSLBase->CRYPTO_get_locked_mem_functions)
#define CRYPTO_get_locking_callback              (OpenSSLBase->CRYPTO_get_locking_callback)
#define CRYPTO_get_mem_debug_functions           (OpenSSLBase->CRYPTO_get_mem_debug_functions)
#define CRYPTO_get_mem_debug_options             (OpenSSLBase->CRYPTO_get_mem_debug_options)
#define CRYPTO_get_mem_ex_functions              (OpenSSLBase->CRYPTO_get_mem_ex_functions)
#define CRYPTO_get_mem_functions                 (OpenSSLBase->CRYPTO_get_mem_functions)
#define CRYPTO_get_new_dynlockid                 (OpenSSLBase->CRYPTO_get_new_dynlockid)
#define CRYPTO_get_new_lockid                    (OpenSSLBase->CRYPTO_get_new_lockid)
#define CRYPTO_is_mem_check_on                   (OpenSSLBase->CRYPTO_is_mem_check_on)
#define CRYPTO_lock                              (OpenSSLBase->CRYPTO_lock)
#define CRYPTO_malloc                            (OpenSSLBase->CRYPTO_malloc)
#define CRYPTO_malloc_locked                     (OpenSSLBase->CRYPTO_malloc_locked)
#define CRYPTO_mem_ctrl                          (OpenSSLBase->CRYPTO_mem_ctrl)
#define CRYPTO_mem_leaks                         (OpenSSLBase->CRYPTO_mem_leaks)
#define CRYPTO_mem_leaks_cb                      (OpenSSLBase->CRYPTO_mem_leaks_cb)
#define CRYPTO_mem_leaks_fp                      (OpenSSLBase->CRYPTO_mem_leaks_fp)
#define CRYPTO_new_ex_data                       (OpenSSLBase->CRYPTO_new_ex_data)
#define CRYPTO_num_locks                         (OpenSSLBase->CRYPTO_num_locks)
#define CRYPTO_pop_info                          (OpenSSLBase->CRYPTO_pop_info)
#define CRYPTO_push_info_                        (OpenSSLBase->CRYPTO_push_info_)
#define CRYPTO_realloc                           (OpenSSLBase->CRYPTO_realloc)
#define CRYPTO_realloc_clean                     (OpenSSLBase->CRYPTO_realloc_clean)
#define CRYPTO_remalloc                          (OpenSSLBase->CRYPTO_remalloc)
#define CRYPTO_remove_all_info                   (OpenSSLBase->CRYPTO_remove_all_info)
#define CRYPTO_set_add_lock_callback             (OpenSSLBase->CRYPTO_set_add_lock_callback)
#define CRYPTO_set_dynlock_create_callback       (OpenSSLBase->CRYPTO_set_dynlock_create_callback)
#define CRYPTO_set_dynlock_destroy_callback      (OpenSSLBase->CRYPTO_set_dynlock_destroy_callback)
#define CRYPTO_set_dynlock_lock_callback         (OpenSSLBase->CRYPTO_set_dynlock_lock_callback)
#define CRYPTO_set_ex_data                       (OpenSSLBase->CRYPTO_set_ex_data)
#define CRYPTO_set_ex_data_implementation        (OpenSSLBase->CRYPTO_set_ex_data_implementation)
#define CRYPTO_set_id_callback                   (OpenSSLBase->CRYPTO_set_id_callback)
#define CRYPTO_set_locked_mem_ex_functions       (OpenSSLBase->CRYPTO_set_locked_mem_ex_functions)
#define CRYPTO_set_locked_mem_functions          (OpenSSLBase->CRYPTO_set_locked_mem_functions)
#define CRYPTO_set_locking_callback              (OpenSSLBase->CRYPTO_set_locking_callback)
#define CRYPTO_set_mem_debug_functions           (OpenSSLBase->CRYPTO_set_mem_debug_functions)
#define CRYPTO_set_mem_debug_options             (OpenSSLBase->CRYPTO_set_mem_debug_options)
#define CRYPTO_set_mem_ex_functions              (OpenSSLBase->CRYPTO_set_mem_ex_functions)
#define CRYPTO_set_mem_functions                 (OpenSSLBase->CRYPTO_set_mem_functions)
#define CRYPTO_thread_id                         (OpenSSLBase->CRYPTO_thread_id)
#define DH_OpenSSL                               (OpenSSLBase->DH_OpenSSL)
#define DH_check                                 (OpenSSLBase->DH_check)
#define DH_check_pub_key                         (OpenSSLBase->DH_check_pub_key)
#define DH_compute_key                           (OpenSSLBase->DH_compute_key)
#define DH_free                                  (OpenSSLBase->DH_free)
#define DH_generate_key                          (OpenSSLBase->DH_generate_key)
#define DH_generate_parameters                   (OpenSSLBase->DH_generate_parameters)
#define DH_generate_parameters_ex                (OpenSSLBase->DH_generate_parameters_ex)
#define DH_get_default_method                    (OpenSSLBase->DH_get_default_method)
#define DH_get_ex_data                           (OpenSSLBase->DH_get_ex_data)
#define DH_get_ex_new_index                      (OpenSSLBase->DH_get_ex_new_index)
#define DH_new                                   (OpenSSLBase->DH_new)
#define DH_new_method                            (OpenSSLBase->DH_new_method)
#define DH_set_default_method                    (OpenSSLBase->DH_set_default_method)
#define DH_set_ex_data                           (OpenSSLBase->DH_set_ex_data)
#define DH_set_method                            (OpenSSLBase->DH_set_method)
#define DH_size                                  (OpenSSLBase->DH_size)
#define DH_up_ref                                (OpenSSLBase->DH_up_ref)
#define DSA_OpenSSL                              (OpenSSLBase->DSA_OpenSSL)
#define DSA_SIG_free                             (OpenSSLBase->DSA_SIG_free)
#define DSA_SIG_new                              (OpenSSLBase->DSA_SIG_new)
#define DSA_do_sign                              (OpenSSLBase->DSA_do_sign)
#define DSA_do_verify                            (OpenSSLBase->DSA_do_verify)
#define DSA_dup_DH                               (OpenSSLBase->DSA_dup_DH)
#define DSA_free                                 (OpenSSLBase->DSA_free)
#define DSA_generate_key                         (OpenSSLBase->DSA_generate_key)
#define DSA_generate_parameters                  (OpenSSLBase->DSA_generate_parameters)
#define DSA_generate_parameters_ex               (OpenSSLBase->DSA_generate_parameters_ex)
#define DSA_get_default_method                   (OpenSSLBase->DSA_get_default_method)
#define DSA_get_ex_data                          (OpenSSLBase->DSA_get_ex_data)
#define DSA_get_ex_new_index                     (OpenSSLBase->DSA_get_ex_new_index)
#define DSA_new                                  (OpenSSLBase->DSA_new)
#define DSA_new_method                           (OpenSSLBase->DSA_new_method)
#define DSA_print                                (OpenSSLBase->DSA_print)
#define DSA_print_fp                             (OpenSSLBase->DSA_print_fp)
#define DSA_set_default_method                   (OpenSSLBase->DSA_set_default_method)
#define DSA_set_ex_data                          (OpenSSLBase->DSA_set_ex_data)
#define DSA_set_method                           (OpenSSLBase->DSA_set_method)
#define DSA_sign                                 (OpenSSLBase->DSA_sign)
#define DSA_sign_setup                           (OpenSSLBase->DSA_sign_setup)
#define DSA_size                                 (OpenSSLBase->DSA_size)
#define DSA_up_ref                               (OpenSSLBase->DSA_up_ref)
#define DSA_verify                               (OpenSSLBase->DSA_verify)
#define ECDH_OpenSSL                             (OpenSSLBase->ECDH_OpenSSL)
#define ECDH_compute_key                         (OpenSSLBase->ECDH_compute_key)
#define ECDH_get_default_method                  (OpenSSLBase->ECDH_get_default_method)
#define ECDH_get_ex_data                         (OpenSSLBase->ECDH_get_ex_data)
#define ECDH_get_ex_new_index                    (OpenSSLBase->ECDH_get_ex_new_index)
#define ECDH_set_default_method                  (OpenSSLBase->ECDH_set_default_method)
#define ECDH_set_ex_data                         (OpenSSLBase->ECDH_set_ex_data)
#define ECDH_set_method                          (OpenSSLBase->ECDH_set_method)
#define ECDSA_OpenSSL                            (OpenSSLBase->ECDSA_OpenSSL)
#define ECDSA_SIG_free                           (OpenSSLBase->ECDSA_SIG_free)
#define ECDSA_SIG_new                            (OpenSSLBase->ECDSA_SIG_new)
#define ECDSA_do_sign                            (OpenSSLBase->ECDSA_do_sign)
#define ECDSA_do_sign_ex                         (OpenSSLBase->ECDSA_do_sign_ex)
#define ECDSA_do_verify                          (OpenSSLBase->ECDSA_do_verify)
#define ECDSA_get_default_method                 (OpenSSLBase->ECDSA_get_default_method)
#define ECDSA_get_ex_data                        (OpenSSLBase->ECDSA_get_ex_data)
#define ECDSA_get_ex_new_index                   (OpenSSLBase->ECDSA_get_ex_new_index)
#define ECDSA_set_default_method                 (OpenSSLBase->ECDSA_set_default_method)
#define ECDSA_set_ex_data                        (OpenSSLBase->ECDSA_set_ex_data)
#define ECDSA_set_method                         (OpenSSLBase->ECDSA_set_method)
#define ECDSA_sign                               (OpenSSLBase->ECDSA_sign)
#define ECDSA_sign_ex                            (OpenSSLBase->ECDSA_sign_ex)
#define ECDSA_sign_setup                         (OpenSSLBase->ECDSA_sign_setup)
#define ECDSA_size                               (OpenSSLBase->ECDSA_size)
#define ECDSA_verify                             (OpenSSLBase->ECDSA_verify)
#define ERR_add_error_data                       (OpenSSLBase->ERR_add_error_data)
#define ERR_clear_error                          (OpenSSLBase->ERR_clear_error)
#define ERR_error_string                         (OpenSSLBase->ERR_error_string)
#define ERR_error_string_n                       (OpenSSLBase->ERR_error_string_n)
#define ERR_free_strings                         (OpenSSLBase->ERR_free_strings)
#define ERR_func_error_string                    (OpenSSLBase->ERR_func_error_string)
#define ERR_get_err_state_table                  (OpenSSLBase->ERR_get_err_state_table)
#define ERR_get_error                            (OpenSSLBase->ERR_get_error)
#define ERR_get_error_line                       (OpenSSLBase->ERR_get_error_line)
#define ERR_get_error_line_data                  (OpenSSLBase->ERR_get_error_line_data)
#define ERR_get_implementation                   (OpenSSLBase->ERR_get_implementation)
#define ERR_get_next_error_library               (OpenSSLBase->ERR_get_next_error_library)
#define ERR_get_state                            (OpenSSLBase->ERR_get_state)
#define ERR_get_string_table                     (OpenSSLBase->ERR_get_string_table)
#define ERR_lib_error_string                     (OpenSSLBase->ERR_lib_error_string)
#define ERR_load_ERR_strings                     (OpenSSLBase->ERR_load_ERR_strings)
#define ERR_load_crypto_strings                  (OpenSSLBase->ERR_load_crypto_strings)
#define ERR_load_strings                         (OpenSSLBase->ERR_load_strings)
#define ERR_peek_error                           (OpenSSLBase->ERR_peek_error)
#define ERR_peek_error_line                      (OpenSSLBase->ERR_peek_error_line)
#define ERR_peek_error_line_data                 (OpenSSLBase->ERR_peek_error_line_data)
#define ERR_peek_last_error                      (OpenSSLBase->ERR_peek_last_error)
#define ERR_peek_last_error_line                 (OpenSSLBase->ERR_peek_last_error_line)
#define ERR_peek_last_error_line_data            (OpenSSLBase->ERR_peek_last_error_line_data)
#define ERR_pop_to_mark                          (OpenSSLBase->ERR_pop_to_mark)
#define ERR_print_errors                         (OpenSSLBase->ERR_print_errors)
#define ERR_print_errors_cb                      (OpenSSLBase->ERR_print_errors_cb)
#define ERR_print_errors_fp                      (OpenSSLBase->ERR_print_errors_fp)
#define ERR_put_error                            (OpenSSLBase->ERR_put_error)
#define ERR_reason_error_string                  (OpenSSLBase->ERR_reason_error_string)
#define ERR_release_err_state_table              (OpenSSLBase->ERR_release_err_state_table)
#define ERR_remove_state                         (OpenSSLBase->ERR_remove_state)
#define ERR_set_error_data                       (OpenSSLBase->ERR_set_error_data)
#define ERR_set_implementation                   (OpenSSLBase->ERR_set_implementation)
#define ERR_set_mark                             (OpenSSLBase->ERR_set_mark)
#define ERR_unload_strings                       (OpenSSLBase->ERR_unload_strings)
#define EVP_BytesToKey                           (OpenSSLBase->EVP_BytesToKey)
#define EVP_CIPHER_CTX_block_size                (OpenSSLBase->EVP_CIPHER_CTX_block_size)
#define EVP_CIPHER_CTX_cipher                    (OpenSSLBase->EVP_CIPHER_CTX_cipher)
#define EVP_CIPHER_CTX_cleanup                   (OpenSSLBase->EVP_CIPHER_CTX_cleanup)
#define EVP_CIPHER_CTX_ctrl                      (OpenSSLBase->EVP_CIPHER_CTX_ctrl)
#define EVP_CIPHER_CTX_flags                     (OpenSSLBase->EVP_CIPHER_CTX_flags)
#define EVP_CIPHER_CTX_free                      (OpenSSLBase->EVP_CIPHER_CTX_free)
#define EVP_CIPHER_CTX_get_app_data              (OpenSSLBase->EVP_CIPHER_CTX_get_app_data)
#define EVP_CIPHER_CTX_init                      (OpenSSLBase->EVP_CIPHER_CTX_init)
#define EVP_CIPHER_CTX_iv_length                 (OpenSSLBase->EVP_CIPHER_CTX_iv_length)
#define EVP_CIPHER_CTX_key_length                (OpenSSLBase->EVP_CIPHER_CTX_key_length)
#define EVP_CIPHER_CTX_new                       (OpenSSLBase->EVP_CIPHER_CTX_new)
#define EVP_CIPHER_CTX_nid                       (OpenSSLBase->EVP_CIPHER_CTX_nid)
#define EVP_CIPHER_CTX_rand_key                  (OpenSSLBase->EVP_CIPHER_CTX_rand_key)
#define EVP_CIPHER_CTX_set_app_data              (OpenSSLBase->EVP_CIPHER_CTX_set_app_data)
#define EVP_CIPHER_CTX_set_key_length            (OpenSSLBase->EVP_CIPHER_CTX_set_key_length)
#define EVP_CIPHER_CTX_set_padding               (OpenSSLBase->EVP_CIPHER_CTX_set_padding)
#define EVP_CIPHER_asn1_to_param                 (OpenSSLBase->EVP_CIPHER_asn1_to_param)
#define EVP_CIPHER_block_size                    (OpenSSLBase->EVP_CIPHER_block_size)
#define EVP_CIPHER_flags                         (OpenSSLBase->EVP_CIPHER_flags)
#define EVP_CIPHER_get_asn1_iv                   (OpenSSLBase->EVP_CIPHER_get_asn1_iv)
#define EVP_CIPHER_iv_length                     (OpenSSLBase->EVP_CIPHER_iv_length)
#define EVP_CIPHER_key_length                    (OpenSSLBase->EVP_CIPHER_key_length)
#define EVP_CIPHER_nid                           (OpenSSLBase->EVP_CIPHER_nid)
#define EVP_CIPHER_param_to_asn1                 (OpenSSLBase->EVP_CIPHER_param_to_asn1)
#define EVP_CIPHER_set_asn1_iv                   (OpenSSLBase->EVP_CIPHER_set_asn1_iv)
#define EVP_CIPHER_type                          (OpenSSLBase->EVP_CIPHER_type)
#define EVP_Cipher                               (OpenSSLBase->EVP_Cipher)
#define EVP_CipherFinal                          (OpenSSLBase->EVP_CipherFinal)
#define EVP_CipherFinal_ex                       (OpenSSLBase->EVP_CipherFinal_ex)
#define EVP_CipherInit                           (OpenSSLBase->EVP_CipherInit)
#define EVP_CipherInit_ex                        (OpenSSLBase->EVP_CipherInit_ex)
#define EVP_CipherUpdate                         (OpenSSLBase->EVP_CipherUpdate)
#define EVP_DecodeBlock                          (OpenSSLBase->EVP_DecodeBlock)
#define EVP_DecodeFinal                          (OpenSSLBase->EVP_DecodeFinal)
#define EVP_DecodeInit                           (OpenSSLBase->EVP_DecodeInit)
#define EVP_DecodeUpdate                         (OpenSSLBase->EVP_DecodeUpdate)
#define EVP_DecryptFinal                         (OpenSSLBase->EVP_DecryptFinal)
#define EVP_DecryptFinal_ex                      (OpenSSLBase->EVP_DecryptFinal_ex)
#define EVP_DecryptInit                          (OpenSSLBase->EVP_DecryptInit)
#define EVP_DecryptInit_ex                       (OpenSSLBase->EVP_DecryptInit_ex)
#define EVP_DecryptUpdate                        (OpenSSLBase->EVP_DecryptUpdate)
#define EVP_Digest                               (OpenSSLBase->EVP_Digest)
#define EVP_DigestFinal                          (OpenSSLBase->EVP_DigestFinal)
#define EVP_DigestFinal_ex                       (OpenSSLBase->EVP_DigestFinal_ex)
#define EVP_DigestInit                           (OpenSSLBase->EVP_DigestInit)
#define EVP_DigestInit_ex                        (OpenSSLBase->EVP_DigestInit_ex)
#define EVP_DigestUpdate                         (OpenSSLBase->EVP_DigestUpdate)
#define EVP_EncodeBlock                          (OpenSSLBase->EVP_EncodeBlock)
#define EVP_EncodeFinal                          (OpenSSLBase->EVP_EncodeFinal)
#define EVP_EncodeInit                           (OpenSSLBase->EVP_EncodeInit)
#define EVP_EncodeUpdate                         (OpenSSLBase->EVP_EncodeUpdate)
#define EVP_EncryptFinal                         (OpenSSLBase->EVP_EncryptFinal)
#define EVP_EncryptFinal_ex                      (OpenSSLBase->EVP_EncryptFinal_ex)
#define EVP_EncryptInit                          (OpenSSLBase->EVP_EncryptInit)
#define EVP_EncryptInit_ex                       (OpenSSLBase->EVP_EncryptInit_ex)
#define EVP_EncryptUpdate                        (OpenSSLBase->EVP_EncryptUpdate)
#define EVP_MD_CTX_cleanup                       (OpenSSLBase->EVP_MD_CTX_cleanup)
#define EVP_MD_CTX_clear_flags                   (OpenSSLBase->EVP_MD_CTX_clear_flags)
#define EVP_MD_CTX_copy                          (OpenSSLBase->EVP_MD_CTX_copy)
#define EVP_MD_CTX_copy_ex                       (OpenSSLBase->EVP_MD_CTX_copy_ex)
#define EVP_MD_CTX_create                        (OpenSSLBase->EVP_MD_CTX_create)
#define EVP_MD_CTX_destroy                       (OpenSSLBase->EVP_MD_CTX_destroy)
#define EVP_MD_CTX_init                          (OpenSSLBase->EVP_MD_CTX_init)
#define EVP_MD_CTX_md                            (OpenSSLBase->EVP_MD_CTX_md)
#define EVP_MD_CTX_set_flags                     (OpenSSLBase->EVP_MD_CTX_set_flags)
#define EVP_MD_CTX_test_flags                    (OpenSSLBase->EVP_MD_CTX_test_flags)
#define EVP_MD_block_size                        (OpenSSLBase->EVP_MD_block_size)
#define EVP_MD_pkey_type                         (OpenSSLBase->EVP_MD_pkey_type)
#define EVP_MD_size                              (OpenSSLBase->EVP_MD_size)
#define EVP_MD_type                              (OpenSSLBase->EVP_MD_type)
#define EVP_OpenFinal                            (OpenSSLBase->EVP_OpenFinal)
#define EVP_OpenInit                             (OpenSSLBase->EVP_OpenInit)
#define EVP_PBE_CipherInit                       (OpenSSLBase->EVP_PBE_CipherInit)
#define EVP_PBE_alg_add                          (OpenSSLBase->EVP_PBE_alg_add)
#define EVP_PBE_cleanup                          (OpenSSLBase->EVP_PBE_cleanup)
#define EVP_PKEY_add1_attr                       (OpenSSLBase->EVP_PKEY_add1_attr)
#define EVP_PKEY_add1_attr_by_NID                (OpenSSLBase->EVP_PKEY_add1_attr_by_NID)
#define EVP_PKEY_add1_attr_by_OBJ                (OpenSSLBase->EVP_PKEY_add1_attr_by_OBJ)
#define EVP_PKEY_add1_attr_by_txt                (OpenSSLBase->EVP_PKEY_add1_attr_by_txt)
#define EVP_PKEY_assign                          (OpenSSLBase->EVP_PKEY_assign)
#define EVP_PKEY_bits                            (OpenSSLBase->EVP_PKEY_bits)
#define EVP_PKEY_cmp                             (OpenSSLBase->EVP_PKEY_cmp)
#define EVP_PKEY_cmp_parameters                  (OpenSSLBase->EVP_PKEY_cmp_parameters)
#define EVP_PKEY_copy_parameters                 (OpenSSLBase->EVP_PKEY_copy_parameters)
#define EVP_PKEY_decrypt                         (OpenSSLBase->EVP_PKEY_decrypt)
#define EVP_PKEY_delete_attr                     (OpenSSLBase->EVP_PKEY_delete_attr)
#define EVP_PKEY_encrypt                         (OpenSSLBase->EVP_PKEY_encrypt)
#define EVP_PKEY_free                            (OpenSSLBase->EVP_PKEY_free)
#define EVP_PKEY_get1_DH                         (OpenSSLBase->EVP_PKEY_get1_DH)
#define EVP_PKEY_get1_DSA                        (OpenSSLBase->EVP_PKEY_get1_DSA)
#define EVP_PKEY_get1_RSA                        (OpenSSLBase->EVP_PKEY_get1_RSA)
#define EVP_PKEY_get_attr                        (OpenSSLBase->EVP_PKEY_get_attr)
#define EVP_PKEY_get_attr_by_NID                 (OpenSSLBase->EVP_PKEY_get_attr_by_NID)
#define EVP_PKEY_get_attr_by_OBJ                 (OpenSSLBase->EVP_PKEY_get_attr_by_OBJ)
#define EVP_PKEY_get_attr_count                  (OpenSSLBase->EVP_PKEY_get_attr_count)
#define EVP_PKEY_missing_parameters              (OpenSSLBase->EVP_PKEY_missing_parameters)
#define EVP_PKEY_new                             (OpenSSLBase->EVP_PKEY_new)
#define EVP_PKEY_save_parameters                 (OpenSSLBase->EVP_PKEY_save_parameters)
#define EVP_PKEY_set1_DH                         (OpenSSLBase->EVP_PKEY_set1_DH)
#define EVP_PKEY_set1_DSA                        (OpenSSLBase->EVP_PKEY_set1_DSA)
#define EVP_PKEY_set1_RSA                        (OpenSSLBase->EVP_PKEY_set1_RSA)
#define EVP_PKEY_size                            (OpenSSLBase->EVP_PKEY_size)
#define EVP_PKEY_type                            (OpenSSLBase->EVP_PKEY_type)
#define EVP_SealFinal                            (OpenSSLBase->EVP_SealFinal)
#define EVP_SealInit                             (OpenSSLBase->EVP_SealInit)
#define EVP_SignFinal                            (OpenSSLBase->EVP_SignFinal)
#define EVP_VerifyFinal                          (OpenSSLBase->EVP_VerifyFinal)
#define EVP_add_cipher                           (OpenSSLBase->EVP_add_cipher)
#define EVP_add_digest                           (OpenSSLBase->EVP_add_digest)
#define EVP_aes_128_cbc                          (OpenSSLBase->EVP_aes_128_cbc)
#define EVP_aes_128_cfb1                         (OpenSSLBase->EVP_aes_128_cfb1)
#define EVP_aes_128_cfb8                         (OpenSSLBase->EVP_aes_128_cfb8)
#define EVP_aes_128_ecb                          (OpenSSLBase->EVP_aes_128_ecb)
#define EVP_aes_128_ofb                          (OpenSSLBase->EVP_aes_128_ofb)
#define EVP_aes_192_cbc                          (OpenSSLBase->EVP_aes_192_cbc)
#define EVP_aes_192_cfb1                         (OpenSSLBase->EVP_aes_192_cfb1)
#define EVP_aes_192_cfb8                         (OpenSSLBase->EVP_aes_192_cfb8)
#define EVP_aes_192_ecb                          (OpenSSLBase->EVP_aes_192_ecb)
#define EVP_aes_192_ofb                          (OpenSSLBase->EVP_aes_192_ofb)
#define EVP_aes_256_cbc                          (OpenSSLBase->EVP_aes_256_cbc)
#define EVP_aes_256_cfb1                         (OpenSSLBase->EVP_aes_256_cfb1)
#define EVP_aes_256_cfb8                         (OpenSSLBase->EVP_aes_256_cfb8)
#define EVP_aes_256_ecb                          (OpenSSLBase->EVP_aes_256_ecb)
#define EVP_aes_256_ofb                          (OpenSSLBase->EVP_aes_256_ofb)
#define EVP_bf_cbc                               (OpenSSLBase->EVP_bf_cbc)
#define EVP_bf_ecb                               (OpenSSLBase->EVP_bf_ecb)
#define EVP_bf_ofb                               (OpenSSLBase->EVP_bf_ofb)
#define EVP_cast5_cbc                            (OpenSSLBase->EVP_cast5_cbc)
#define EVP_cast5_ecb                            (OpenSSLBase->EVP_cast5_ecb)
#define EVP_cast5_ofb                            (OpenSSLBase->EVP_cast5_ofb)
#define EVP_cleanup                              (OpenSSLBase->EVP_cleanup)
#define EVP_des_cbc                              (OpenSSLBase->EVP_des_cbc)
#define EVP_des_cfb1                             (OpenSSLBase->EVP_des_cfb1)
#define EVP_des_cfb8                             (OpenSSLBase->EVP_des_cfb8)
#define EVP_des_ecb                              (OpenSSLBase->EVP_des_ecb)
#define EVP_des_ede                              (OpenSSLBase->EVP_des_ede)
#define EVP_des_ede3                             (OpenSSLBase->EVP_des_ede3)
#define EVP_des_ede3_cbc                         (OpenSSLBase->EVP_des_ede3_cbc)
#define EVP_des_ede3_cfb1                        (OpenSSLBase->EVP_des_ede3_cfb1)
#define EVP_des_ede3_cfb8                        (OpenSSLBase->EVP_des_ede3_cfb8)
#define EVP_des_ede3_ecb                         (OpenSSLBase->EVP_des_ede3_ecb)
#define EVP_des_ede3_ofb                         (OpenSSLBase->EVP_des_ede3_ofb)
#define EVP_des_ede_cbc                          (OpenSSLBase->EVP_des_ede_cbc)
#define EVP_des_ede_ecb                          (OpenSSLBase->EVP_des_ede_ecb)
#define EVP_des_ede_ofb                          (OpenSSLBase->EVP_des_ede_ofb)
#define EVP_des_ofb                              (OpenSSLBase->EVP_des_ofb)
#define EVP_desx_cbc                             (OpenSSLBase->EVP_desx_cbc)
#define EVP_dss                                  (OpenSSLBase->EVP_dss)
#define EVP_dss1                                 (OpenSSLBase->EVP_dss1)
#define EVP_ecdsa                                (OpenSSLBase->EVP_ecdsa)
#define EVP_enc_null                             (OpenSSLBase->EVP_enc_null)
#define EVP_get_cipherbyname                     (OpenSSLBase->EVP_get_cipherbyname)
#define EVP_get_digestbyname                     (OpenSSLBase->EVP_get_digestbyname)
#define EVP_get_pw_prompt                        (OpenSSLBase->EVP_get_pw_prompt)
#define EVP_idea_cbc                             (OpenSSLBase->EVP_idea_cbc)
#define EVP_idea_ecb                             (OpenSSLBase->EVP_idea_ecb)
#define EVP_idea_ofb                             (OpenSSLBase->EVP_idea_ofb)
#define EVP_md2                                  (OpenSSLBase->EVP_md2)
#define EVP_md4                                  (OpenSSLBase->EVP_md4)
#define EVP_md5                                  (OpenSSLBase->EVP_md5)
#define EVP_md_null                              (OpenSSLBase->EVP_md_null)
#define EVP_rc2_40_cbc                           (OpenSSLBase->EVP_rc2_40_cbc)
#define EVP_rc2_64_cbc                           (OpenSSLBase->EVP_rc2_64_cbc)
#define EVP_rc2_cbc                              (OpenSSLBase->EVP_rc2_cbc)
#define EVP_rc2_ecb                              (OpenSSLBase->EVP_rc2_ecb)
#define EVP_rc2_ofb                              (OpenSSLBase->EVP_rc2_ofb)
#define EVP_rc4                                  (OpenSSLBase->EVP_rc4)
#define EVP_read_pw_string                       (OpenSSLBase->EVP_read_pw_string)
#define EVP_set_pw_prompt                        (OpenSSLBase->EVP_set_pw_prompt)
#define EVP_sha                                  (OpenSSLBase->EVP_sha)
#define EVP_sha1                                 (OpenSSLBase->EVP_sha1)
#define HMAC                                     (OpenSSLBase->HMAC)
#define HMAC_CTX_cleanup                         (OpenSSLBase->HMAC_CTX_cleanup)
#define HMAC_CTX_init                            (OpenSSLBase->HMAC_CTX_init)
#define HMAC_Final                               (OpenSSLBase->HMAC_Final)
#define HMAC_Init                                (OpenSSLBase->HMAC_Init)
#define HMAC_Init_ex                             (OpenSSLBase->HMAC_Init_ex)
#define HMAC_Update                              (OpenSSLBase->HMAC_Update)
#define OpenSSL_add_all_ciphers                  (OpenSSLBase->OpenSSL_add_all_ciphers)
#define OpenSSL_add_all_digests                  (OpenSSLBase->OpenSSL_add_all_digests)
#define PEM_ASN1_read                            (OpenSSLBase->PEM_ASN1_read)
#define PEM_ASN1_read_bio                        (OpenSSLBase->PEM_ASN1_read_bio)
#define PEM_ASN1_write                           (OpenSSLBase->PEM_ASN1_write)
#define PEM_ASN1_write_bio                       (OpenSSLBase->PEM_ASN1_write_bio)
#define PEM_SealFinal                            (OpenSSLBase->PEM_SealFinal)
#define PEM_SealInit                             (OpenSSLBase->PEM_SealInit)
#define PEM_SealUpdate                           (OpenSSLBase->PEM_SealUpdate)
#define PEM_SignFinal                            (OpenSSLBase->PEM_SignFinal)
#define PEM_SignInit                             (OpenSSLBase->PEM_SignInit)
#define PEM_SignUpdate                           (OpenSSLBase->PEM_SignUpdate)
#define PEM_X509_INFO_read                       (OpenSSLBase->PEM_X509_INFO_read)
#define PEM_X509_INFO_read_bio                   (OpenSSLBase->PEM_X509_INFO_read_bio)
#define PEM_X509_INFO_write_bio                  (OpenSSLBase->PEM_X509_INFO_write_bio)
#define PEM_bytes_read_bio                       (OpenSSLBase->PEM_bytes_read_bio)
#define PEM_def_callback                         (OpenSSLBase->PEM_def_callback)
#define PEM_dek_info                             (OpenSSLBase->PEM_dek_info)
#define PEM_do_header                            (OpenSSLBase->PEM_do_header)
#define PEM_get_EVP_CIPHER_INFO                  (OpenSSLBase->PEM_get_EVP_CIPHER_INFO)
#define PEM_proc_type                            (OpenSSLBase->PEM_proc_type)
#define PEM_read                                 (OpenSSLBase->PEM_read)
#define PEM_read_DHparams                        (OpenSSLBase->PEM_read_DHparams)
#define PEM_read_DSAPrivateKey                   (OpenSSLBase->PEM_read_DSAPrivateKey)
#define PEM_read_DSA_PUBKEY                      (OpenSSLBase->PEM_read_DSA_PUBKEY)
#define PEM_read_DSAparams                       (OpenSSLBase->PEM_read_DSAparams)
#define PEM_read_NETSCAPE_CERT_SEQUENCE          (OpenSSLBase->PEM_read_NETSCAPE_CERT_SEQUENCE)
#define PEM_read_PKCS7                           (OpenSSLBase->PEM_read_PKCS7)
#define PEM_read_PKCS8                           (OpenSSLBase->PEM_read_PKCS8)
#define PEM_read_PKCS8_PRIV_KEY_INFO             (OpenSSLBase->PEM_read_PKCS8_PRIV_KEY_INFO)
#define PEM_read_PUBKEY                          (OpenSSLBase->PEM_read_PUBKEY)
#define PEM_read_PrivateKey                      (OpenSSLBase->PEM_read_PrivateKey)
#define PEM_read_RSAPrivateKey                   (OpenSSLBase->PEM_read_RSAPrivateKey)
#define PEM_read_RSAPublicKey                    (OpenSSLBase->PEM_read_RSAPublicKey)
#define PEM_read_RSA_PUBKEY                      (OpenSSLBase->PEM_read_RSA_PUBKEY)
#define PEM_read_X509                            (OpenSSLBase->PEM_read_X509)
#define PEM_read_X509_AUX                        (OpenSSLBase->PEM_read_X509_AUX)
#define PEM_read_X509_CERT_PAIR                  (OpenSSLBase->PEM_read_X509_CERT_PAIR)
#define PEM_read_X509_CRL                        (OpenSSLBase->PEM_read_X509_CRL)
#define PEM_read_X509_REQ                        (OpenSSLBase->PEM_read_X509_REQ)
#define PEM_read_bio                             (OpenSSLBase->PEM_read_bio)
#define PEM_read_bio_DHparams                    (OpenSSLBase->PEM_read_bio_DHparams)
#define PEM_read_bio_DSAPrivateKey               (OpenSSLBase->PEM_read_bio_DSAPrivateKey)
#define PEM_read_bio_DSA_PUBKEY                  (OpenSSLBase->PEM_read_bio_DSA_PUBKEY)
#define PEM_read_bio_DSAparams                   (OpenSSLBase->PEM_read_bio_DSAparams)
#define PEM_read_bio_NETSCAPE_CERT_SEQUENCE      (OpenSSLBase->PEM_read_bio_NETSCAPE_CERT_SEQUENCE)
#define PEM_read_bio_PKCS7                       (OpenSSLBase->PEM_read_bio_PKCS7)
#define PEM_read_bio_PKCS8                       (OpenSSLBase->PEM_read_bio_PKCS8)
#define PEM_read_bio_PKCS8_PRIV_KEY_INFO         (OpenSSLBase->PEM_read_bio_PKCS8_PRIV_KEY_INFO)
#define PEM_read_bio_PUBKEY                      (OpenSSLBase->PEM_read_bio_PUBKEY)
#define PEM_read_bio_PrivateKey                  (OpenSSLBase->PEM_read_bio_PrivateKey)
#define PEM_read_bio_RSAPrivateKey               (OpenSSLBase->PEM_read_bio_RSAPrivateKey)
#define PEM_read_bio_RSAPublicKey                (OpenSSLBase->PEM_read_bio_RSAPublicKey)
#define PEM_read_bio_RSA_PUBKEY                  (OpenSSLBase->PEM_read_bio_RSA_PUBKEY)
#define PEM_read_bio_X509                        (OpenSSLBase->PEM_read_bio_X509)
#define PEM_read_bio_X509_AUX                    (OpenSSLBase->PEM_read_bio_X509_AUX)
#define PEM_read_bio_X509_CERT_PAIR              (OpenSSLBase->PEM_read_bio_X509_CERT_PAIR)
#define PEM_read_bio_X509_CRL                    (OpenSSLBase->PEM_read_bio_X509_CRL)
#define PEM_read_bio_X509_REQ                    (OpenSSLBase->PEM_read_bio_X509_REQ)
#define PEM_write                                (OpenSSLBase->PEM_write)
#define PEM_write_DHparams                       (OpenSSLBase->PEM_write_DHparams)
#define PEM_write_DSAPrivateKey                  (OpenSSLBase->PEM_write_DSAPrivateKey)
#define PEM_write_DSA_PUBKEY                     (OpenSSLBase->PEM_write_DSA_PUBKEY)
#define PEM_write_DSAparams                      (OpenSSLBase->PEM_write_DSAparams)
#define PEM_write_NETSCAPE_CERT_SEQUENCE         (OpenSSLBase->PEM_write_NETSCAPE_CERT_SEQUENCE)
#define PEM_write_PKCS7                          (OpenSSLBase->PEM_write_PKCS7)
#define PEM_write_PKCS8                          (OpenSSLBase->PEM_write_PKCS8)
#define PEM_write_PKCS8PrivateKey                (OpenSSLBase->PEM_write_PKCS8PrivateKey)
#define PEM_write_PKCS8PrivateKey_nid            (OpenSSLBase->PEM_write_PKCS8PrivateKey_nid)
#define PEM_write_PKCS8_PRIV_KEY_INFO            (OpenSSLBase->PEM_write_PKCS8_PRIV_KEY_INFO)
#define PEM_write_PUBKEY                         (OpenSSLBase->PEM_write_PUBKEY)
#define PEM_write_PrivateKey                     (OpenSSLBase->PEM_write_PrivateKey)
#define PEM_write_RSAPrivateKey                  (OpenSSLBase->PEM_write_RSAPrivateKey)
#define PEM_write_RSAPublicKey                   (OpenSSLBase->PEM_write_RSAPublicKey)
#define PEM_write_RSA_PUBKEY                     (OpenSSLBase->PEM_write_RSA_PUBKEY)
#define PEM_write_X509                           (OpenSSLBase->PEM_write_X509)
#define PEM_write_X509_AUX                       (OpenSSLBase->PEM_write_X509_AUX)
#define PEM_write_X509_CERT_PAIR                 (OpenSSLBase->PEM_write_X509_CERT_PAIR)
#define PEM_write_X509_CRL                       (OpenSSLBase->PEM_write_X509_CRL)
#define PEM_write_X509_REQ                       (OpenSSLBase->PEM_write_X509_REQ)
#define PEM_write_X509_REQ_NEW                   (OpenSSLBase->PEM_write_X509_REQ_NEW)
#define PEM_write_bio                            (OpenSSLBase->PEM_write_bio)
#define PEM_write_bio_DHparams                   (OpenSSLBase->PEM_write_bio_DHparams)
#define PEM_write_bio_DSAPrivateKey              (OpenSSLBase->PEM_write_bio_DSAPrivateKey)
#define PEM_write_bio_DSA_PUBKEY                 (OpenSSLBase->PEM_write_bio_DSA_PUBKEY)
#define PEM_write_bio_DSAparams                  (OpenSSLBase->PEM_write_bio_DSAparams)
#define PEM_write_bio_NETSCAPE_CERT_SEQUENCE     (OpenSSLBase->PEM_write_bio_NETSCAPE_CERT_SEQUENCE)
#define PEM_write_bio_PKCS7                      (OpenSSLBase->PEM_write_bio_PKCS7)
#define PEM_write_bio_PKCS8                      (OpenSSLBase->PEM_write_bio_PKCS8)
#define PEM_write_bio_PKCS8PrivateKey            (OpenSSLBase->PEM_write_bio_PKCS8PrivateKey)
#define PEM_write_bio_PKCS8PrivateKey_nid        (OpenSSLBase->PEM_write_bio_PKCS8PrivateKey_nid)
#define PEM_write_bio_PKCS8_PRIV_KEY_INFO        (OpenSSLBase->PEM_write_bio_PKCS8_PRIV_KEY_INFO)
#define PEM_write_bio_PUBKEY                     (OpenSSLBase->PEM_write_bio_PUBKEY)
#define PEM_write_bio_PrivateKey                 (OpenSSLBase->PEM_write_bio_PrivateKey)
#define PEM_write_bio_RSAPrivateKey              (OpenSSLBase->PEM_write_bio_RSAPrivateKey)
#define PEM_write_bio_RSAPublicKey               (OpenSSLBase->PEM_write_bio_RSAPublicKey)
#define PEM_write_bio_RSA_PUBKEY                 (OpenSSLBase->PEM_write_bio_RSA_PUBKEY)
#define PEM_write_bio_X509                       (OpenSSLBase->PEM_write_bio_X509)
#define PEM_write_bio_X509_AUX                   (OpenSSLBase->PEM_write_bio_X509_AUX)
#define PEM_write_bio_X509_CERT_PAIR             (OpenSSLBase->PEM_write_bio_X509_CERT_PAIR)
#define PEM_write_bio_X509_CRL                   (OpenSSLBase->PEM_write_bio_X509_CRL)
#define PEM_write_bio_X509_REQ                   (OpenSSLBase->PEM_write_bio_X509_REQ)
#define PEM_write_bio_X509_REQ_NEW               (OpenSSLBase->PEM_write_bio_X509_REQ_NEW)
#define PKCS7_add_attrib_smimecap                (OpenSSLBase->PKCS7_add_attrib_smimecap)
#define PKCS7_add_attribute                      (OpenSSLBase->PKCS7_add_attribute)
#define PKCS7_add_certificate                    (OpenSSLBase->PKCS7_add_certificate)
#define PKCS7_add_crl                            (OpenSSLBase->PKCS7_add_crl)
#define PKCS7_add_recipient                      (OpenSSLBase->PKCS7_add_recipient)
#define PKCS7_add_recipient_info                 (OpenSSLBase->PKCS7_add_recipient_info)
#define PKCS7_add_signature                      (OpenSSLBase->PKCS7_add_signature)
#define PKCS7_add_signed_attribute               (OpenSSLBase->PKCS7_add_signed_attribute)
#define PKCS7_add_signer                         (OpenSSLBase->PKCS7_add_signer)
#define PKCS7_cert_from_signer_info              (OpenSSLBase->PKCS7_cert_from_signer_info)
#define PKCS7_content_new                        (OpenSSLBase->PKCS7_content_new)
#define PKCS7_ctrl                               (OpenSSLBase->PKCS7_ctrl)
#define PKCS7_dataDecode                         (OpenSSLBase->PKCS7_dataDecode)
#define PKCS7_dataFinal                          (OpenSSLBase->PKCS7_dataFinal)
#define PKCS7_dataInit                           (OpenSSLBase->PKCS7_dataInit)
#define PKCS7_dataVerify                         (OpenSSLBase->PKCS7_dataVerify)
#define PKCS7_decrypt                            (OpenSSLBase->PKCS7_decrypt)
#define PKCS7_digest_from_attributes             (OpenSSLBase->PKCS7_digest_from_attributes)
#define PKCS7_dup                                (OpenSSLBase->PKCS7_dup)
#define PKCS7_encrypt                            (OpenSSLBase->PKCS7_encrypt)
#define PKCS7_free                               (OpenSSLBase->PKCS7_free)
#define PKCS7_get0_signers                       (OpenSSLBase->PKCS7_get0_signers)
#define PKCS7_get_attribute                      (OpenSSLBase->PKCS7_get_attribute)
#define PKCS7_get_issuer_and_serial              (OpenSSLBase->PKCS7_get_issuer_and_serial)
#define PKCS7_get_signed_attribute               (OpenSSLBase->PKCS7_get_signed_attribute)
#define PKCS7_get_signer_info                    (OpenSSLBase->PKCS7_get_signer_info)
#define PKCS7_get_smimecap                       (OpenSSLBase->PKCS7_get_smimecap)
#define PKCS7_new                                (OpenSSLBase->PKCS7_new)
#define PKCS7_set0_type_other                    (OpenSSLBase->PKCS7_set0_type_other)
#define PKCS7_set_attributes                     (OpenSSLBase->PKCS7_set_attributes)
#define PKCS7_set_cipher                         (OpenSSLBase->PKCS7_set_cipher)
#define PKCS7_set_content                        (OpenSSLBase->PKCS7_set_content)
#define PKCS7_set_digest                         (OpenSSLBase->PKCS7_set_digest)
#define PKCS7_set_signed_attributes              (OpenSSLBase->PKCS7_set_signed_attributes)
#define PKCS7_set_type                           (OpenSSLBase->PKCS7_set_type)
#define PKCS7_sign                               (OpenSSLBase->PKCS7_sign)
#define PKCS7_signatureVerify                    (OpenSSLBase->PKCS7_signatureVerify)
#define PKCS7_simple_smimecap                    (OpenSSLBase->PKCS7_simple_smimecap)
#define PKCS7_verify                             (OpenSSLBase->PKCS7_verify)
#define RSAPrivateKey_asn1_meth                  (OpenSSLBase->RSAPrivateKey_asn1_meth)
#define RSAPrivateKey_dup                        (OpenSSLBase->RSAPrivateKey_dup)
#define RSAPublicKey_dup                         (OpenSSLBase->RSAPublicKey_dup)
#define RSA_PKCS1_SSLeay                         (OpenSSLBase->RSA_PKCS1_SSLeay)
#define RSA_X931_hash_id                         (OpenSSLBase->RSA_X931_hash_id)
#define RSA_blinding_off                         (OpenSSLBase->RSA_blinding_off)
#define RSA_blinding_on                          (OpenSSLBase->RSA_blinding_on)
#define RSA_check_key                            (OpenSSLBase->RSA_check_key)
#define RSA_flags                                (OpenSSLBase->RSA_flags)
#define RSA_free                                 (OpenSSLBase->RSA_free)
#define RSA_generate_key_ex                      (OpenSSLBase->RSA_generate_key_ex)
#define RSA_get_default_method                   (OpenSSLBase->RSA_get_default_method)
#define RSA_get_ex_data                          (OpenSSLBase->RSA_get_ex_data)
#define RSA_get_ex_new_index                     (OpenSSLBase->RSA_get_ex_new_index)
#define RSA_get_method                           (OpenSSLBase->RSA_get_method)
#define RSA_memory_lock                          (OpenSSLBase->RSA_memory_lock)
#define RSA_new                                  (OpenSSLBase->RSA_new)
#define RSA_new_method                           (OpenSSLBase->RSA_new_method)
#define RSA_null_method                          (OpenSSLBase->RSA_null_method)
#define RSA_padding_add_PKCS1_OAEP               (OpenSSLBase->RSA_padding_add_PKCS1_OAEP)
#define RSA_padding_add_PKCS1_PSS                (OpenSSLBase->RSA_padding_add_PKCS1_PSS)
#define RSA_padding_add_PKCS1_type_1             (OpenSSLBase->RSA_padding_add_PKCS1_type_1)
#define RSA_padding_add_PKCS1_type_2             (OpenSSLBase->RSA_padding_add_PKCS1_type_2)
#define RSA_padding_add_SSLv23                   (OpenSSLBase->RSA_padding_add_SSLv23)
#define RSA_padding_add_X931                     (OpenSSLBase->RSA_padding_add_X931)
#define RSA_padding_add_none                     (OpenSSLBase->RSA_padding_add_none)
#define RSA_padding_check_PKCS1_OAEP             (OpenSSLBase->RSA_padding_check_PKCS1_OAEP)
#define RSA_padding_check_PKCS1_type_1           (OpenSSLBase->RSA_padding_check_PKCS1_type_1)
#define RSA_padding_check_PKCS1_type_2           (OpenSSLBase->RSA_padding_check_PKCS1_type_2)
#define RSA_padding_check_SSLv23                 (OpenSSLBase->RSA_padding_check_SSLv23)
#define RSA_padding_check_X931                   (OpenSSLBase->RSA_padding_check_X931)
#define RSA_padding_check_none                   (OpenSSLBase->RSA_padding_check_none)
#define RSA_print                                (OpenSSLBase->RSA_print)
#define RSA_print_fp                             (OpenSSLBase->RSA_print_fp)
#define RSA_private_decrypt                      (OpenSSLBase->RSA_private_decrypt)
#define RSA_private_encrypt                      (OpenSSLBase->RSA_private_encrypt)
#define RSA_public_decrypt                       (OpenSSLBase->RSA_public_decrypt)
#define RSA_public_encrypt                       (OpenSSLBase->RSA_public_encrypt)
#define RSA_set_default_method                   (OpenSSLBase->RSA_set_default_method)
#define RSA_set_ex_data                          (OpenSSLBase->RSA_set_ex_data)
#define RSA_set_method                           (OpenSSLBase->RSA_set_method)
#define RSA_setup_blinding                       (OpenSSLBase->RSA_setup_blinding)
#define RSA_sign                                 (OpenSSLBase->RSA_sign)
#define RSA_sign_ASN1_OCTET_STRING               (OpenSSLBase->RSA_sign_ASN1_OCTET_STRING)
#define RSA_size                                 (OpenSSLBase->RSA_size)
#define RSA_up_ref                               (OpenSSLBase->RSA_up_ref)
#define RSA_verify                               (OpenSSLBase->RSA_verify)
#define RSA_verify_ASN1_OCTET_STRING             (OpenSSLBase->RSA_verify_ASN1_OCTET_STRING)
#define RSA_verify_PKCS1_PSS                     (OpenSSLBase->RSA_verify_PKCS1_PSS)
#define SHA                                      (OpenSSLBase->SHA)
#define SHA1                                     (OpenSSLBase->SHA1)
#define SHA1_Final                               (OpenSSLBase->SHA1_Final)
#define SHA1_Init                                (OpenSSLBase->SHA1_Init)
#define SHA1_Transform                           (OpenSSLBase->SHA1_Transform)
#define SHA1_Update                              (OpenSSLBase->SHA1_Update)
#define SHA_Final                                (OpenSSLBase->SHA_Final)
#define SHA_Init                                 (OpenSSLBase->SHA_Init)
#define SHA_Transform                            (OpenSSLBase->SHA_Transform)
#define SHA_Update                               (OpenSSLBase->SHA_Update)
#define SMIME_crlf_copy                          (OpenSSLBase->SMIME_crlf_copy)
#define SMIME_read_PKCS7                         (OpenSSLBase->SMIME_read_PKCS7)
#define SMIME_text                               (OpenSSLBase->SMIME_text)
#define SMIME_write_PKCS7                        (OpenSSLBase->SMIME_write_PKCS7)
#define X509_add1_ext_i2d                        (OpenSSLBase->X509_add1_ext_i2d)
#define X509_add1_reject_object                  (OpenSSLBase->X509_add1_reject_object)
#define X509_add1_trust_object                   (OpenSSLBase->X509_add1_trust_object)
#define X509_add_ext                             (OpenSSLBase->X509_add_ext)
#define X509_alias_get0                          (OpenSSLBase->X509_alias_get0)
#define X509_alias_set1                          (OpenSSLBase->X509_alias_set1)
#define X509_asn1_meth                           (OpenSSLBase->X509_asn1_meth)
#define X509_certificate_type                    (OpenSSLBase->X509_certificate_type)
#define X509_check_private_key                   (OpenSSLBase->X509_check_private_key)
#define X509_check_trust                         (OpenSSLBase->X509_check_trust)
#define X509_cmp                                 (OpenSSLBase->X509_cmp)
#define X509_cmp_current_time                    (OpenSSLBase->X509_cmp_current_time)
#define X509_cmp_time                            (OpenSSLBase->X509_cmp_time)
#define X509_delete_ext                          (OpenSSLBase->X509_delete_ext)
#define X509_digest                              (OpenSSLBase->X509_digest)
#define X509_dup                                 (OpenSSLBase->X509_dup)
#define X509_find_by_issuer_and_serial           (OpenSSLBase->X509_find_by_issuer_and_serial)
#define X509_find_by_subject                     (OpenSSLBase->X509_find_by_subject)
#define X509_free                                (OpenSSLBase->X509_free)
#define X509_get0_pubkey_bitstr                  (OpenSSLBase->X509_get0_pubkey_bitstr)
#define X509_get_default_cert_area               (OpenSSLBase->X509_get_default_cert_area)
#define X509_get_default_cert_dir                (OpenSSLBase->X509_get_default_cert_dir)
#define X509_get_default_cert_dir_env            (OpenSSLBase->X509_get_default_cert_dir_env)
#define X509_get_default_cert_file               (OpenSSLBase->X509_get_default_cert_file)
#define X509_get_default_cert_file_env           (OpenSSLBase->X509_get_default_cert_file_env)
#define X509_get_default_private_dir             (OpenSSLBase->X509_get_default_private_dir)
#define X509_get_ex_data                         (OpenSSLBase->X509_get_ex_data)
#define X509_get_ex_new_index                    (OpenSSLBase->X509_get_ex_new_index)
#define X509_get_ext                             (OpenSSLBase->X509_get_ext)
#define X509_get_ext_by_NID                      (OpenSSLBase->X509_get_ext_by_NID)
#define X509_get_ext_by_OBJ                      (OpenSSLBase->X509_get_ext_by_OBJ)
#define X509_get_ext_by_critical                 (OpenSSLBase->X509_get_ext_by_critical)
#define X509_get_ext_count                       (OpenSSLBase->X509_get_ext_count)
#define X509_get_ext_d2i                         (OpenSSLBase->X509_get_ext_d2i)
#define X509_get_issuer_name                     (OpenSSLBase->X509_get_issuer_name)
#define X509_get_pubkey                          (OpenSSLBase->X509_get_pubkey)
#define X509_get_pubkey_parameters               (OpenSSLBase->X509_get_pubkey_parameters)
#define X509_get_serialNumber                    (OpenSSLBase->X509_get_serialNumber)
#define X509_get_subject_name                    (OpenSSLBase->X509_get_subject_name)
#define X509_gmtime_adj                          (OpenSSLBase->X509_gmtime_adj)
#define X509_issuer_and_serial_cmp               (OpenSSLBase->X509_issuer_and_serial_cmp)
#define X509_issuer_and_serial_hash              (OpenSSLBase->X509_issuer_and_serial_hash)
#define X509_issuer_name_cmp                     (OpenSSLBase->X509_issuer_name_cmp)
#define X509_issuer_name_hash                    (OpenSSLBase->X509_issuer_name_hash)
#define X509_keyid_get0                          (OpenSSLBase->X509_keyid_get0)
#define X509_keyid_set1                          (OpenSSLBase->X509_keyid_set1)
#define X509_load_cert_crl_file                  (OpenSSLBase->X509_load_cert_crl_file)
#define X509_load_cert_file                      (OpenSSLBase->X509_load_cert_file)
#define X509_load_crl_file                       (OpenSSLBase->X509_load_crl_file)
#define X509_new                                 (OpenSSLBase->X509_new)
#define X509_ocspid_print                        (OpenSSLBase->X509_ocspid_print)
#define X509_policy_check                        (OpenSSLBase->X509_policy_check)
#define X509_policy_level_get0_node              (OpenSSLBase->X509_policy_level_get0_node)
#define X509_policy_level_node_count             (OpenSSLBase->X509_policy_level_node_count)
#define X509_policy_node_get0_parent             (OpenSSLBase->X509_policy_node_get0_parent)
#define X509_policy_node_get0_policy             (OpenSSLBase->X509_policy_node_get0_policy)
#define X509_policy_node_get0_qualifiers         (OpenSSLBase->X509_policy_node_get0_qualifiers)
#define X509_policy_tree_free                    (OpenSSLBase->X509_policy_tree_free)
#define X509_policy_tree_get0_level              (OpenSSLBase->X509_policy_tree_get0_level)
#define X509_policy_tree_get0_policies           (OpenSSLBase->X509_policy_tree_get0_policies)
#define X509_policy_tree_get0_user_policies      (OpenSSLBase->X509_policy_tree_get0_user_policies)
#define X509_policy_tree_level_count             (OpenSSLBase->X509_policy_tree_level_count)
#define X509_print                               (OpenSSLBase->X509_print)
#define X509_print_ex                            (OpenSSLBase->X509_print_ex)
#define X509_print_ex_fp                         (OpenSSLBase->X509_print_ex_fp)
#define X509_print_fp                            (OpenSSLBase->X509_print_fp)
#define X509_pubkey_digest                       (OpenSSLBase->X509_pubkey_digest)
#define X509_reject_clear                        (OpenSSLBase->X509_reject_clear)
#define X509_set_ex_data                         (OpenSSLBase->X509_set_ex_data)
#define X509_set_issuer_name                     (OpenSSLBase->X509_set_issuer_name)
#define X509_set_notAfter                        (OpenSSLBase->X509_set_notAfter)
#define X509_set_notBefore                       (OpenSSLBase->X509_set_notBefore)
#define X509_set_pubkey                          (OpenSSLBase->X509_set_pubkey)
#define X509_set_serialNumber                    (OpenSSLBase->X509_set_serialNumber)
#define X509_set_subject_name                    (OpenSSLBase->X509_set_subject_name)
#define X509_set_version                         (OpenSSLBase->X509_set_version)
#define X509_sign                                (OpenSSLBase->X509_sign)
#define X509_signature_print                     (OpenSSLBase->X509_signature_print)
#define X509_subject_name_cmp                    (OpenSSLBase->X509_subject_name_cmp)
#define X509_subject_name_hash                   (OpenSSLBase->X509_subject_name_hash)
#define X509_time_adj                            (OpenSSLBase->X509_time_adj)
#define X509_to_X509_REQ                         (OpenSSLBase->X509_to_X509_REQ)
#define X509_trust_clear                         (OpenSSLBase->X509_trust_clear)
#define X509_verify                              (OpenSSLBase->X509_verify)
#define X509_verify_cert                         (OpenSSLBase->X509_verify_cert)
#define X509_verify_cert_error_string            (OpenSSLBase->X509_verify_cert_error_string)
#define X509at_add1_attr                         (OpenSSLBase->X509at_add1_attr)
#define X509at_add1_attr_by_NID                  (OpenSSLBase->X509at_add1_attr_by_NID)
#define X509at_add1_attr_by_OBJ                  (OpenSSLBase->X509at_add1_attr_by_OBJ)
#define X509at_add1_attr_by_txt                  (OpenSSLBase->X509at_add1_attr_by_txt)
#define X509at_delete_attr                       (OpenSSLBase->X509at_delete_attr)
#define X509at_get_attr                          (OpenSSLBase->X509at_get_attr)
#define X509at_get_attr_by_NID                   (OpenSSLBase->X509at_get_attr_by_NID)
#define X509at_get_attr_by_OBJ                   (OpenSSLBase->X509at_get_attr_by_OBJ)
#define X509at_get_attr_count                    (OpenSSLBase->X509at_get_attr_count)
#define X509v3_add_ext                           (OpenSSLBase->X509v3_add_ext)
#define X509v3_delete_ext                        (OpenSSLBase->X509v3_delete_ext)
#define X509v3_get_ext                           (OpenSSLBase->X509v3_get_ext)
#define X509v3_get_ext_by_NID                    (OpenSSLBase->X509v3_get_ext_by_NID)
#define X509v3_get_ext_by_OBJ                    (OpenSSLBase->X509v3_get_ext_by_OBJ)
#define X509v3_get_ext_by_critical               (OpenSSLBase->X509v3_get_ext_by_critical)
#define X509v3_get_ext_count                     (OpenSSLBase->X509v3_get_ext_count)

#else

static ERROR sslGenerateRSAKey(LONG, CSTRING, STRING *, STRING *);
static ERROR sslCalcSigFromObject(OBJECTPTR, LONG, CSTRING, STRING, CSTRING, APTR *, LONG *);
static ERROR sslVerifySig(OBJECTPTR, LONG, CSTRING, CSTRING, APTR, LONG);
static ERROR sslGenerateRSAPublicKey(CSTRING, CSTRING, STRING *);

#endif // PRV_OPENSSL

#endif // MODULES_OPENSSL_H
