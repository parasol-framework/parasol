
//********************************************************************************************************************
// Load PKCS#12 certificate with private key (for mkcert certificates)

static PCCERT_CONTEXT load_pkcs12_certificate(const std::string &Path)
{
   ssl_debug_log(SSL_DEBUG_TRACE, "Attempting to load PKCS#12 certificate: %s", Path.c_str());
   
   // Open and read the PKCS#12 file
   HANDLE p12_file = CreateFileA(Path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
   if (p12_file == INVALID_HANDLE_VALUE) {
      ssl_debug_log(SSL_DEBUG_TRACE, "PKCS#12 file not found: %s", Path.c_str());
      return nullptr;
   }
   
   DWORD p12_size = GetFileSize(p12_file, nullptr);
   if (p12_size == INVALID_FILE_SIZE) {
      ssl_debug_log(SSL_DEBUG_WARNING, "Failed to get PKCS#12 file size");
      CloseHandle(p12_file);
      return nullptr;
   }
   
   std::vector<BYTE> p12_data(p12_size);
   DWORD bytes_read;
   if (!ReadFile(p12_file, p12_data.data(), p12_size, &bytes_read, nullptr) or bytes_read != p12_size) {
      ssl_debug_log(SSL_DEBUG_WARNING, "Failed to read PKCS#12 file");
      CloseHandle(p12_file);
      return nullptr;
   }
   CloseHandle(p12_file);
   
   // Create CRYPT_DATA_BLOB for PKCS#12 data
   CRYPT_DATA_BLOB pfx_blob;
   pfx_blob.cbData = p12_size;
   pfx_blob.pbData = p12_data.data();
   
   // Import PKCS#12 into certificate store (no password)
   HCERTSTORE pfx_store = PFXImportCertStore(&pfx_blob, L"", CRYPT_EXPORTABLE | CRYPT_USER_KEYSET);
   if (!pfx_store) {
      DWORD error = GetLastError();
      ssl_debug_log(SSL_DEBUG_WARNING, "Failed to import PKCS#12, error: %d (0x%08X)", error, error);
      return nullptr;
   }
   
   ssl_debug_log(SSL_DEBUG_INFO, "Successfully imported PKCS#12 certificate store");
   
   // Find the certificate in the imported store
   PCCERT_CONTEXT cert_context = CertEnumCertificatesInStore(pfx_store, nullptr);
   if (!cert_context) {
      ssl_debug_log(SSL_DEBUG_WARNING, "No certificates found in PKCS#12 store");
      CertCloseStore(pfx_store, 0);
      return nullptr;
   }
   
   // Duplicate the certificate context so we can close the PFX store
   PCCERT_CONTEXT final_cert = CertDuplicateCertificateContext(cert_context);
   
   // Import the certificate (with private key) into the personal store
   HCERTSTORE personal_store = CertOpenSystemStore(0, "MY");
   if (personal_store) {
      if (CertAddCertificateContextToStore(personal_store, cert_context, CERT_STORE_ADD_REPLACE_EXISTING, nullptr)) {
         ssl_debug_log(SSL_DEBUG_INFO, "PKCS#12 certificate with private key added to personal store");
      }
      else ssl_debug_log(SSL_DEBUG_WARNING, "Failed to add PKCS#12 certificate to personal store, error: %d", GetLastError());
      CertCloseStore(personal_store, 0);
   }
   
   CertCloseStore(pfx_store, 0);
   ssl_debug_log(SSL_DEBUG_INFO, "PKCS#12 certificate loaded successfully");
   
   return final_cert;
}

//********************************************************************************************************************
// Load PEM certificate and private key (for mkcert certificates)

static PCCERT_CONTEXT load_pem_certificate(const std::string &Path)
{
   ssl_debug_log(SSL_DEBUG_TRACE, "Attempting to load PEM certificate: %s", Path.c_str());
   
   // Open and read the certificate file
   HANDLE cert_file = CreateFileA(Path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
   if (cert_file == INVALID_HANDLE_VALUE) {
      ssl_debug_log(SSL_DEBUG_TRACE, "Certificate file not found: %s", Path.c_str());
      return nullptr;
   }
   
   DWORD cert_size = GetFileSize(cert_file, nullptr);
   if (cert_size == INVALID_FILE_SIZE) {
      ssl_debug_log(SSL_DEBUG_WARNING, "Failed to get certificate file size");
      CloseHandle(cert_file);
      return nullptr;
   }
   
   std::vector<BYTE> cert_data(cert_size + 1); // +1 for null terminator
   DWORD bytes_read;
   if (!ReadFile(cert_file, cert_data.data(), cert_size, &bytes_read, nullptr) or bytes_read != cert_size) {
      ssl_debug_log(SSL_DEBUG_WARNING, "Failed to read certificate file");
      CloseHandle(cert_file);
      return nullptr;
   }
   cert_data[cert_size] = 0; // Null terminate for PEM processing
   CloseHandle(cert_file);
   
   // Convert PEM to DER format
   DWORD der_size = 0;
   if (!CryptStringToBinaryA((const char*)cert_data.data(), 0, CRYPT_STRING_BASE64HEADER, nullptr, &der_size, nullptr, nullptr)) {
      ssl_debug_log(SSL_DEBUG_WARNING, "Failed to decode PEM certificate, error: %d", GetLastError());
      return nullptr;
   }
   
   std::vector<BYTE> der_data(der_size);
   if (!CryptStringToBinaryA((const char*)cert_data.data(), 0, CRYPT_STRING_BASE64HEADER, der_data.data(), &der_size, nullptr, nullptr)) {
      ssl_debug_log(SSL_DEBUG_WARNING, "Failed to convert PEM to DER, error: %d", GetLastError());
      return nullptr;
   }
   
   // Create certificate context from DER data
   PCCERT_CONTEXT cert_context = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, der_data.data(), der_size);
   if (!cert_context) {
      ssl_debug_log(SSL_DEBUG_WARNING, "Failed to create certificate context, error: %d", GetLastError());
      return nullptr;
   }
   
   ssl_debug_log(SSL_DEBUG_INFO, "Successfully loaded PEM certificate");
   
   // For now, we'll return the certificate context without the private key
   // Windows SSL will use the certificate from the store with its associated private key
   // We should import both the certificate and private key to the Windows certificate store
   
   // Open the personal certificate store
   HCERTSTORE cert_store = CertOpenSystemStore(0, "MY");
   if (!cert_store) {
      ssl_debug_log(SSL_DEBUG_WARNING, "Failed to open certificate store");
      CertFreeCertificateContext(cert_context);
      return nullptr;
   }
   
   // Check if certificate is already in store
   PCCERT_CONTEXT existing_cert = CertFindCertificateInStore(cert_store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_EXISTING, cert_context, nullptr);
   if (existing_cert) {
      ssl_debug_log(SSL_DEBUG_INFO, "Certificate already exists in store");
      CertFreeCertificateContext(cert_context);
      CertCloseStore(cert_store, 0);
      return existing_cert;
   }
   
   // Add certificate to store (this is needed for Windows SSL to find the private key)
   if (!CertAddCertificateContextToStore(cert_store, cert_context, CERT_STORE_ADD_REPLACE_EXISTING, nullptr)) {
      ssl_debug_log(SSL_DEBUG_WARNING, "Failed to add certificate to store, error: %d", GetLastError());
      CertFreeCertificateContext(cert_context);
      CertCloseStore(cert_store, 0);
      return nullptr;
   }
   
   ssl_debug_log(SSL_DEBUG_INFO, "Certificate added to Windows certificate store");
   CertCloseStore(cert_store, 0);
   
   return cert_context;
}

//********************************************************************************************************************
// Get certificate verification result

bool ssl_get_verify_result(SSL_HANDLE SSL)
{
   if (!SSL) return false;

   // Check if SSL context is properly initialized for certificate validation
   if (!SSL->context_initialised) return false;

   // Query certificate context from the established SSL connection
   PCCERT_CONTEXT cert_context = nullptr;
   SECURITY_STATUS status = QueryContextAttributes(&SSL->context, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &cert_context);

   if (status != SEC_E_OK) { // Failed to get certificate context - connection is not secure
      SSL->last_security_status = status;
      return false; // Certificate validation failed
   }

   if (!cert_context) return false; // No certificate presented by server

   // Query connection info to check if certificate validation succeeded
   SecPkgContext_ConnectionInfo conn_info;
   status = QueryContextAttributes(&SSL->context, SECPKG_ATTR_CONNECTION_INFO, &conn_info);

   if (status != SEC_E_OK) {
      CertFreeCertificateContext(cert_context);
      SSL->last_security_status = status;
      return false;
   }

   // With SCH_CRED_AUTO_CRED_VALIDATION, Windows should have validated the certificate
   // Check if we have a valid cipher suite (indicates successful validation)

   if (conn_info.aiCipher == 0 or conn_info.aiHash == 0) {
      CertFreeCertificateContext(cert_context);
      return false; // Invalid cipher negotiation indicates cert issues
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

   bool result = true; // Default to success (X509_V_OK equivalent)

   if (!chain_result or !chain_context) {
      result = false; // Certificate chain validation failed
   }
   else { // Check chain trust status
      CERT_TRUST_STATUS trust_status = chain_context->TrustStatus;
      if (trust_status.dwErrorStatus != CERT_TRUST_NO_ERROR) result = false;
   }

   if (chain_context) CertFreeCertificateChain(chain_context);
   CertFreeCertificateContext(cert_context);

   return result;
}
