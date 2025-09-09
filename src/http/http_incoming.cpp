// Functions for processing incoming HTTP data.

constexpr int CHUNK_BUFFER_SIZE = 32 * 1024;

//********************************************************************************************************************

static ERR read_incoming_header(extHTTP *Self, objNetSocket *Socket)
{
   pf::Log log(__FUNCTION__);

   while (true) {
      if (Self->Response.empty()) Self->Response.resize(512);

      if (Self->ResponseIndex >= std::ssize(Self->Response)) {
         if (Self->Response.size() >= MAX_HEADER_SIZE) {
            log.warning("HTTP response header exceeds maximum size of %d bytes", MAX_HEADER_SIZE);
            SET_ERROR(log, Self, ERR::InvalidHTTPResponse);
            return ERR::Terminate;
         }
         Self->Response.resize(std::min(Self->Response.size() + 1024, size_t(MAX_HEADER_SIZE)));
      }

      int len;
      Self->Error = acRead(Socket, Self->Response.data() + Self->ResponseIndex, Self->Response.size() - Self->ResponseIndex, &len);

      if (Self->Error != ERR::Okay) {
         log.warning(Self->Error);
         return ERR::Terminate;
      }

      if (!len) break; // No more incoming data
      
      #ifdef DEBUG_SOCKET
         if (glDebugFile) glDebugFile->write(Self->Response.data() + Self->ResponseIndex, len);
      #endif
      
      Self->ResponseIndex += len;

      // Advance search for terminated double CRLF
      // Start searching from max(0, SearchIndex - 3) to handle "\r\n\r\n" split across read boundaries

      int search_start = (Self->SearchIndex >= 3) ? Self->SearchIndex - 3 : 0;
      for (int i = search_start; i + 4 <= Self->ResponseIndex; i++) {
         if (strncmp(Self->Response.c_str() + i, "\r\n\r\n", 4)) continue;

         auto response_header = std::string_view(Self->Response.c_str(), i);
         if (parse_response(Self, response_header) != ERR::Okay) {
            SET_ERROR(log, Self, log.warning(ERR::InvalidHTTPResponse));
            return ERR::Terminate;
         }

         if (Self->Tunneling) {
            if (Self->Status IS HTS::OKAY) {
               // Proxy tunnel established.  Convert the socket to an SSL connection, then send the HTTP command.

               // Set SSL verification flags before enabling SSL
               if ((Self->Flags & HTF::DISABLE_SERVER_VERIFY) != HTF::NIL) {
                  Socket->Flags |= NSF::DISABLE_SERVER_VERIFY;
               }

               if (net::SetSSL(Socket, "EnableSSL", nullptr) IS ERR::Okay) {
                  return acActivate(Self);
               }
               else {
                  SET_ERROR(log, Self, log.warning(ERR::ConnectionAborted));
                  return ERR::Terminate;
               }
            }
            else {
               SET_ERROR(log, Self, log.warning(ERR::ProxySSLTunnel));
               return ERR::Terminate;
            }
         }

         if ((Self->CurrentState IS HGS::AUTHENTICATING) and (Self->Status != HTS::UNAUTHORISED)) {
            log.msg("Authentication successful, reactivating...");
            Self->SecurePath = false;
            Self->setCurrentState(HGS::AUTHENTICATED);
            QueueAction(AC::Activate, Self->UID);
            return ERR::Okay;
         }

         if (Self->Status IS HTS::MOVED_PERMANENTLY) {
            if ((Self->Flags & HTF::MOVED) != HTF::NIL) {
               // Chaining of MovedPermanently messages is disallowed (could cause circular referencing).

               log.warning("Sequential MovedPermanently messages are not supported.");
            }
            else {
               char buffer[512];
               if (acGetKey(Self, "Location", buffer, sizeof(buffer)) IS ERR::Okay) {
                  log.msg("MovedPermanently to %s", buffer);
                  if (pf::startswith("http:", buffer)) Self->setLocation(buffer);
                  else if (pf::startswith("https:", buffer)) Self->setLocation(buffer);
                  else Self->setPath(buffer);
                  acActivate(Self); // Try again
                  Self->Flags |= HTF::MOVED;
                  return ERR::Okay;
               }
               else {
                  Self->Flags |= HTF::MOVED;
                  log.warning("Invalid MovedPermanently HTTP response received (no location specified).");
               }
            }
         }
         else if (Self->Status IS HTS::TEMP_REDIRECT) {
            if ((Self->Flags & HTF::REDIRECTED) != HTF::NIL) {
               // Chaining of TempRedirect messages is disallowed (could cause circular referencing).

               log.warning("Sequential TempRedirect messages are not supported.");
            }
            else Self->Flags |= HTF::REDIRECTED;
         }

         if ((Self->ContentLength IS 0) and (!Self->Chunked)) {
            log.msg("Response header received, no content imminent.");
            Self->setCurrentState(HGS::COMPLETED);
            return ERR::Terminate;
         }

         log.msg("Complete response header has been received.  Incoming Content: %" PRId64, Self->ContentLength);

         if (Self->CurrentState != HGS::READING_CONTENT) {
            Self->setCurrentState(HGS::READING_CONTENT);
         }

         Self->AuthDigest = false;
         if ((Self->Status IS HTS::UNAUTHORISED) and (Self->AuthRetries < MAX_AUTH_RETRIES)) {
            Self->AuthRetries++;

            if (!Self->Password.empty()) {
               // Destroy the current password if it was entered by the user (therefore is invalid) or if it was
               // preset and second authorisation attempt failed (in the case of preset passwords, two
               // authorisation attempts are required in order to receive the 401 from the server first).

               if ((Self->AuthPreset IS false) or (Self->AuthRetries >= 2)) {
                  if (!Self->Password.empty()) {
                     secure_clear_memory(const_cast<char*>(Self->Password.data()), Self->Password.size());
                  }
                  Self->Password.clear();
               }
            }

            std::string &authenticate = Self->Args["WWW-Authenticate"];
            if (!authenticate.empty()) {
               if (pf::startswith("Digest", authenticate)) {
                  log.trace("Digest authentication mode.");

                  Self->Realm.clear();
                  Self->AuthNonce.clear();
                  Self->AuthOpaque.clear();
                  Self->AuthAlgorithm.clear();
                  Self->AuthDigest = true;

                  int i = 6;
                  while ((authenticate[i]) and (authenticate[i] <= 0x20)) i++;

                  while (authenticate[i]) {
                     std::string_view auth (authenticate.begin()+i, authenticate.end());

                     if (pf::startswith("realm=", auth))       i += extract_value(auth, Self->Realm);
                     else if (pf::startswith("nonce=", auth))  i += extract_value(auth, Self->AuthNonce);
                     else if (pf::startswith("opaque=", auth)) i += extract_value(auth, Self->AuthOpaque);
                     else if (pf::startswith("algorithm=", auth)) i += extract_value(auth, Self->AuthAlgorithm);
                     else if (pf::startswith("qop=", auth)) {
                        std::string value;
                        i += extract_value(auth, value);
                        if (value.find("auth-int") != std::string::npos) Self->AuthQOP = "auth-int";
                        else Self->AuthQOP = "auth";
                     }
                     else {
                        while (authenticate[i] > 0x20) {
                           if (authenticate[i] IS '=') {
                              i++;
                              while ((authenticate[i]) and (authenticate[i] <= 0x20)) i++;
                              if (authenticate[i] IS '"') {
                                 i++;
                                 while ((authenticate[i]) and (authenticate[i] != '"')) i++;
                                 if (authenticate[i] IS '"') i++;
                              }
                              else i++;
                           }
                           else i++;
                        }

                        while (authenticate[i] > 0x20) i++;
                        while ((authenticate[i]) and (authenticate[i] <= 0x20)) i++;
                     }
                  }
               }
               else log.trace("Basic authentication mode.");
            }
            else log.msg("Authenticate method unknown.");

            Self->setCurrentState(HGS::AUTHENTICATING);

            ERR error = ERR::Okay;
            if ((Self->Password.empty()) and ((Self->Flags & HTF::NO_DIALOG) IS HTF::NIL)) {
               // Pop up a dialog requesting the user to authorise himself with the http server.  The user will
               // need to respond to the dialog before we can repost the HTTP request.

               // TODO: Needs a rewrite using the dialog script
               std::string scriptfile((const char *)glAuthScript, 0, glAuthScriptLength);

               objScript::create script = { fl::String(scriptfile) };
               if (script.ok()) {
                  AdjustLogLevel(1);
                  error = script->activate();
                  AdjustLogLevel(-1);
               }
               else error = ERR::CreateObject;
            }
            else acActivate(Self);

            return error;
         }

         len = Self->ResponseIndex - (i + 4);

         if (Self->Chunked) {
            log.trace("Content to be received in chunks.");
            Self->ChunkIndex = 0; // Number of bytes processed for the current chunk
            Self->ChunkRemaining   = 0;  // Length of the first chunk is unknown at this stage
            Self->ChunkBuffered = len;
            Self->Chunk.resize(len > CHUNK_BUFFER_SIZE ? len : CHUNK_BUFFER_SIZE);
            if (len > 0) pf::copymem(Self->Response.data() + i + 4, Self->Chunk.data(), len);
            Self->SearchIndex = 0;
         }
         else {
            log.trace("%" PRId64 " bytes of content is incoming.  Bytes Buffered: %d, Index: %" PRId64, Self->ContentLength, len, Self->Index);

            if (len > 0) output_incoming_data(Self, Self->Response.data() + i + 4, len);
         }

         check_incoming_end(Self);

         Self->Response.clear(); // Buffer no longer required, response key-values are in the Args table.

         // Note that status check comes after processing of content, as it is legal for content to be attached
         // with bad status codes (e.g. SOAP does this).

         if ((int(Self->Status) < 200) or (int(Self->Status) >= 300)) {
            if (Self->CurrentState != HGS::READING_CONTENT) {
               if (Self->Status IS HTS::UNAUTHORISED) log.warning("Exhausted maximum number of retries.");
               else log.warning("Status code %d != 2xx", int(Self->Status));

               SET_ERROR(log, Self, ERR::Failed);
               return ERR::Terminate;
            }
            else log.warning("Status code %d != 2xx.  Receiving content...", int(Self->Status));
         }

         return ERR::Okay; // Response header has been read, process any remaining data
      }

      // Update SearchIndex to avoid re-scanning, but preserve boundary detection capability
      Self->SearchIndex = (Self->ResponseIndex >= 3) ? Self->ResponseIndex - 3 : 0;
      log.trace("Partial HTTP header received, awaiting full header...");
   }
   
   return ERR::Continue; // More data needed
}

//********************************************************************************************************************
// Data chunk mode.  Store received data in a chunk buffer.  As long as we know the entire size of the chunk, all 
// data can be immediately passed onto our subscribers.
//
// Chunked data is passed as follows:
//
// ChunkSize\r\n
// Data....
// ChunkSize\r\n
// Data...
// \r\n (indicates end) or 0\r\n (indicates end of chunks with further HTTP tags following)
//
// ChunkIndex:     Current read position within the buffer.
// ChunkBuffered:  Number of bytes currently buffered.
// ChunkRemaining: Unprocessed bytes in this chunk (decreases as bytes are processed).

static ERR read_incoming_chunks(extHTTP *Self, objNetSocket *Socket)
{
   int i, count, len;

   for (count = 2; count > 0; count--) { // Make multiple passes in case there's more data than fits in the buffer
      pf::Log log("http_incoming");
      log.traceBranch("Receiving content (chunk mode) Index: %d/%d/%d, Remaining: %d", Self->ChunkIndex, Self->ChunkBuffered, int(Self->Chunk.size()), Self->ChunkRemaining);

      // Compress or clear the buffer

      if (Self->ChunkIndex > 0) {
         if (Self->ChunkBuffered > Self->ChunkIndex) {
            log.trace("Compressing the chunk buffer.");
            pf::copymem(Self->Chunk.data() + Self->ChunkIndex, Self->Chunk.data(), Self->ChunkBuffered - Self->ChunkIndex);
            Self->ChunkBuffered -= Self->ChunkIndex;
         }
         else Self->ChunkBuffered = 0;
         Self->ChunkIndex = 0;
      }

      // Fill the chunk buffer

      if (Self->ChunkBuffered < int(Self->Chunk.size())) {
         Self->Error = acRead(Socket, Self->Chunk.data() + Self->ChunkBuffered, Self->Chunk.size() - Self->ChunkBuffered, &len);
         
         #ifdef DEBUG_SOCKET
            if ((glDebugFile) and (len)) glDebugFile->write(Self->Chunk.data() + Self->ChunkBuffered, len);
         #endif

         log.trace("Filling the chunk buffer: Read %d bytes.", len);

         if (Self->Error IS ERR::Disconnected) {
            log.detail("Received all chunked content (disconnected by peer).");
            Self->setCurrentState(HGS::COMPLETED);
            return ERR::Terminate;
         }
         else if (Self->Error != ERR::Okay) {
            log.warning("Read() returned error %d whilst reading content.", int(Self->Error));
            Self->setCurrentState(HGS::COMPLETED);
            return ERR::Terminate;
         }
         else if ((!len) and (Self->ChunkIndex >= Self->ChunkBuffered)) {
            log.detail("Nothing left to read.");
            return ERR::Okay;
         }
         else Self->ChunkBuffered += len;
      }

      while (Self->ChunkIndex < Self->ChunkBuffered) {
         log.trace("Status: Index: %d/%d, CurrentChunk: %d", Self->ChunkIndex, Self->ChunkBuffered, Self->ChunkRemaining);

         if (!Self->ChunkRemaining) {
            // Read the next chunk header.  It is assumed that the format is:
            //
            // ChunkSize\r\n
            // Data...

            log.trace("Examining chunk header (%d bytes buffered).", Self->ChunkBuffered - Self->ChunkIndex);

            // Search for \r\n, handling split boundaries
            // Start from ChunkIndex but check up to ChunkBuffered
            // to handle the case where buffer ends with '\r'
            bool found_crlf = false;
            constexpr int MAX_CHUNK_HEADER_SIZE = 128; // Hex chunk size shouldn't exceed this
            
            for (i = Self->ChunkIndex; i < Self->ChunkBuffered; i++) {
               // Check if we've searched too far without finding \r\n (prevent DoS)
               if ((i - Self->ChunkIndex) > MAX_CHUNK_HEADER_SIZE) {
                  log.warning("Chunk header exceeds maximum size of %d bytes", MAX_CHUNK_HEADER_SIZE);
                  Self->setCurrentState(HGS::TERMINATED);
                  return ERR::Terminate;
               }

               if (Self->Chunk[i] IS '\r') {
                  // Check if we have the next byte
                  if (i + 1 < Self->ChunkBuffered) {
                     if (Self->Chunk[i + 1] IS '\n') {
                        found_crlf = true;
                        
                        Self->Chunk[i] = 0;
                        int64_t temp_chunk_len = strtoll((CSTRING)Self->Chunk.data() + Self->ChunkIndex, nullptr, 16);
                        Self->Chunk[i] = '\r';

                        // Validate chunk length
                        if (temp_chunk_len < 0 or temp_chunk_len > MAX_CHUNK_LENGTH) {
                           if (temp_chunk_len > 0) {
                              log.warning("Chunk length %d exceeds maximum %d terminating", int(temp_chunk_len), int(MAX_CHUNK_LENGTH));
                              Self->setCurrentState(HGS::TERMINATED);
                              return ERR::Terminate;
                           }
                        }
                        Self->ChunkRemaining = temp_chunk_len;

                        if (Self->ChunkRemaining <= 0) {
                           if (Self->Chunk[Self->ChunkIndex] IS '0') {
                              // A line of "0\r\n" indicates an end to the chunks, followed by optional data for
                              // interpretation.

                              log.detail("End of chunks reached, optional data follows.");
                              Self->setCurrentState(HGS::COMPLETED);
                              return ERR::Terminate;
                           }
                           else {
                              // We have reached the terminating line (CRLF on an empty line)
                              log.trace("Received all chunked content.");
                              Self->setCurrentState(HGS::COMPLETED);
                              return ERR::Terminate;
                           }
                        }

                        log.trace("Next chunk length is %d bytes.", Self->ChunkRemaining);
                        Self->ChunkIndex = i + 2; // \r\n
                        break;
                     }
                  }
                  else {
                     // We have '\r' at the end of the buffer - need more data
                     // The next read might start with '\n'
                     break;
                  }
               }
            }

            // Quit the main loop if we still don't have a chunk length (more data needs to be read from the HTTP socket).

            if (!Self->ChunkRemaining) break;
         }

         if (Self->ChunkRemaining > 0) {
            len = Self->ChunkBuffered - Self->ChunkIndex;
            if (len > Self->ChunkRemaining) len = Self->ChunkRemaining; // Cannot process more bytes than the expected chunk length

            log.trace("%d bytes yet to process, outputting %d bytes", Self->ChunkRemaining, len);

            Self->ChunkRemaining -= len;
            output_incoming_data(Self, Self->Chunk.data() + Self->ChunkIndex, len);

            Self->ChunkIndex += len;

            if (!Self->ChunkRemaining) { // The end of the chunk binary is followed with a CRLF
               log.trace("A complete chunk has been processed.");
               Self->ChunkRemaining = -2;
            }
         }

         if (Self->ChunkRemaining < 0) {
            log.trace("Skipping %d bytes.", -Self->ChunkRemaining);

            while ((Self->ChunkRemaining < 0) and (Self->ChunkIndex < Self->ChunkBuffered)) {
               Self->ChunkIndex++;
               Self->ChunkRemaining++;
            }

            if (Self->ChunkRemaining < 0) break; // If we did not receive all the bytes, break to continue processing until more bytes are ready
         }
      }
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR read_incoming_content(extHTTP* Self, objNetSocket* Socket)
{
   pf::Log log(__FUNCTION__);

   std::vector<char> buffer(BUFFER_READ_SIZE);

   // Maximum number of times that this subroutine can loop (on a fast network we could otherwise download indefinitely).
   // A limit of 64K per read session is acceptable with a time limit of 1/200 frames.

   int looplimit = (64 * 1024) / BUFFER_READ_SIZE;
   int64_t timelimit = PreciseTime() + 5000000LL;

   while (true) {
      int len = BUFFER_READ_SIZE;
      if (Self->ContentLength != -1) {
         if (len > Self->ContentLength - Self->Index) len = Self->ContentLength - Self->Index;
      }

      if ((Self->Error = acRead(Socket, buffer.data(), len, &len)) != ERR::Okay) {
         #ifdef DEBUG_SOCKET
            if (glDebugFile) glDebugFile->write(buffer.data(), len);
         #endif

         if ((Self->Error IS ERR::Disconnected) and (Self->ContentLength IS -1)) {
            log.trace("Received all streamed content (disconnected by peer).");
            Self->setCurrentState(HGS::COMPLETED);
            return ERR::Terminate;
         }
         else {
            log.warning("Read() returned error %d whilst reading content.", int(Self->Error));
            return ERR::Terminate;
         }
      }

      if (!len) break; // No more incoming data right now

      output_incoming_data(Self, buffer.data(), len);
      if (check_incoming_end(Self) IS ERR::True) {
         return ERR::Terminate;
      }

      if (--looplimit <= 0) break; // Looped many times, need to break
      if (PreciseTime() > timelimit) break; // Time limit reached
   }

   Self->LastReceipt = PreciseTime();

   if (Self->TimeoutManager) UpdateTimer(Self->TimeoutManager, Self->DataTimeout);
   else SubscribeTimer(Self->DataTimeout, C_FUNCTION(timeout_manager), &Self->TimeoutManager);

   if (Self->Error != ERR::Okay) return ERR::Terminate;
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR parse_response(extHTTP *Self, std::string_view Response)
{
   pf::Log log(__FUNCTION__);

   Self->Args.clear();

   log.detail("HTTP RESPONSE HEADER\n%.*s", int(Response.size()), Response.data());

   // First line: HTTP/1.1 200 OK

   if (!Response.starts_with("HTTP/")) {
      log.warning("Invalid response header, missing 'HTTP/'");
      return ERR::InvalidHTTPResponse;
   }

   //int majorv = StrToInt(str); // Currently unused
   auto b = Response.find_first_of('.');
   if (b IS std::string::npos) return ERR::InvalidHTTPResponse;
   b++;

   //int minorv = StrToInt(str); // Currently unused
   while ((b < Response.size()) and (Response[b] > 0x20)) b++;
   while ((b < Response.size()) and (Response[b] <= 0x20)) b++;

   int code = 0;
   auto [ ptr, error ] = std::from_chars(Response.data() + b, Response.data() + Response.size(), code);
   if (error IS std::errc()) Self->Status = HTS(code);
   else Self->Status = HTS::NIL;

   if (Self->ProxyServer) Self->ContentLength = -1; // Some proxy servers (Squid) strip out information like 'transfer-encoding' yet pass all the requested content anyway :-/
   else Self->ContentLength = 0;
   Self->Chunked = false;

   // Parse response fields

   auto end_line = Response.find("\r\n");
   if (end_line IS std::string::npos) return ERR::Okay;
   Response.remove_prefix(end_line + 2);

   log.msg("HTTP response header received, status code %d", code);

   while (!Response.empty()) {
      auto i = Response.find_first_of(":\r");

      if (i IS std::string::npos) break;
      if (Response[i] IS ':') { // Detected "Key: Value"
         auto field = std::string(Response.substr(0, i));
         std::transform(field.begin(), field.end(), field.begin(),
            [](unsigned char c){ return char(std::tolower(c)); });

         i++;
         while ((i < Response.size()) and (Response[i] <= 0x20)) i++;

         auto end_line = Response.find_first_of("\r", i);
         std::string_view value;
         if (end_line IS std::string::npos) {
            value = Response.substr(i);
         }
         else value = Response.substr(i, end_line - i);

         Self->Args[field] = value;

         if (end_line IS std::string::npos) break;
         Response.remove_prefix(end_line + 2);
      }
      else Response.remove_prefix(i + 2);
   }
   
   if (auto it = Self->Args.find("content-length"); it != Self->Args.end()) {
      auto &value = it->second;
      Self->ContentLength = 0;
      int64_t temp_length = 0;
      auto [ ptr, error ] = std::from_chars(value.data(), value.data() + value.size(), temp_length);
      if (error IS std::errc() and temp_length >= 0 and temp_length <= MAX_CONTENT_LENGTH) {
         Self->ContentLength = temp_length;
      }
      else {
         log.warning("Invalid or excessive Content-Length: %.*s", int(value.size()), value.data());
         Self->ContentLength = -1; // Treat as streaming
      }
   }

   if (auto it = Self->Args.find("transfer-encoding"); it != Self->Args.end()) {
      auto &value = it->second;
      if (pf::iequals(value, "chunked")) {
         if ((Self->Flags & HTF::RAW) IS HTF::NIL) Self->Chunked = true;
         Self->ContentLength = -1;
      }
   }

   return ERR::Okay;
}

//********************************************************************************************************************
// Sends buffered data to the listener

static ERR output_incoming_data(extHTTP *Self, APTR Buffer, int Length)
{
   pf::Log log(__FUNCTION__);

   log.trace("Buffer: %p, Length: %d", Buffer, Length);

   if (!Length) return ERR::Okay;

   Self->setIndex(Self->Index + Length); // Use Set() so that field subscribers can track progress with field monitoring

   if ((!Self->flOutput) and (Self->OutputFile)) {
      FL flags;
      LOC type;

      if ((Self->Flags & HTF::RESUME) != HTF::NIL) {
         if ((AnalysePath(Self->OutputFile, &type) IS ERR::Okay) and (type IS LOC::FILE)) {
            flags = FL::NIL;
         }
         else flags = FL::NEW;
      }
      else flags = FL::NEW;

      if ((Self->flOutput = objFile::create::local(fl::Path(Self->OutputFile), fl::Flags(flags|FL::WRITE)))) {
         if ((Self->Flags & HTF::RESUME) != HTF::NIL) {
            acSeekEnd(Self->flOutput, 0);
            Self->setIndex(0);
         }
      }
      else SET_ERROR(log, Self, ERR::CreateFile);
   }

   if (Self->flOutput) Self->flOutput->write(Buffer, Length, nullptr);

   if ((Self->Flags & HTF::RECV_BUFFER) != HTF::NIL) {
      Self->RecvBuffer.append(std::string_view((char *)Buffer, Length));
   }

   if (Self->Incoming.defined()) {
      log.trace("Incoming callback is set.");

      ERR error;
      if (Self->Incoming.isC()) {
         auto routine = (ERR (*)(extHTTP *, APTR, int, APTR))Self->Incoming.Routine;
         error = routine(Self, Buffer, Length, Self->Incoming.Meta);
      }
      else if (Self->Incoming.isScript()) {
         // For speed, the client will receive a direct pointer to the buffer memory via the 'mem' interface.

         log.trace("Calling script procedure %" PRId64, Self->Incoming.ProcedureID);

         if (sc::Call(Self->Incoming, std::to_array<ScriptArg>({
               { "HTTP",       Self,   FD_OBJECTPTR },
               { "Buffer",     Buffer, FD_PTRBUFFER },
               { "BufferSize", Length, FD_INT|FD_BUFSIZE }
            }), error) != ERR::Okay) error = ERR::Terminate;
      }
      else error = ERR::InvalidValue;

      if (error > ERR::ExceptionThreshold) SET_ERROR(log, Self, error);

      if (error IS ERR::Terminate) {
         pf::Log log(__FUNCTION__);
         log.branch("Client changing state to HGS::TERMINATED.");
         Self->setCurrentState(HGS::TERMINATED);
      }
   }

   if (Self->OutputObjectID) {
      if (Self->ObjectMode IS HOM::DATA_FEED) {
         pf::ScopedObjectLock output(Self->OutputObjectID);
         if (output.granted()) acDataFeed(*output, Self, Self->Datatype, Buffer, Length);
      }
      else if (Self->ObjectMode IS HOM::READ_WRITE) {
         pf::ScopedObjectLock output(Self->OutputObjectID);
         if (output.granted()) acWrite(*output, Buffer, Length);
      }
   }

   return Self->Error;
}

//********************************************************************************************************************
// Callback: NetSocket.Incoming

static ERR socket_incoming(objNetSocket *Socket)
{
   pf::Log log("http_incoming");

   auto Self = (extHTTP *)Socket->ClientData;

   if (Self->classID() != CLASSID::HTTP) return log.warning(ERR::SystemCorrupt);
   
   #ifdef DEBUG_SOCKET
      if (!glDebugFile) glDebugFile = objFile::create::untracked({ fl::Path("temp:http-incoming-log.raw"), fl::Flags(FL::NEW|FL::WRITE) });
   #endif

restart:

   if (Self->CurrentState >= HGS::COMPLETED) {
      // Erroneous data received from server while we are in a completion/resting state.  Returning a terminate message
      // will cause the socket object to close the connection to the server so that we stop receiving erroneous data.

      log.warning("Unexpected data incoming from server - terminating socket.");
      return ERR::Terminate;
   }

   if (Self->CurrentState IS HGS::SENDING_CONTENT) {
      // Sanity check failed - we should not be receiving data while we are sending content to the server.
      if (Self->ContentLength IS -1) {
         log.warning("Incoming data while streaming content - %" PRId64 " bytes already written.", Self->Index);
      }
      else if (Self->Index < Self->ContentLength) {
         log.warning("Incoming data while sending content - only %" PRId64 "/%" PRId64 " bytes written!", Self->Index, Self->ContentLength);
      }
   }

   if ((Self->CurrentState IS HGS::SENDING_CONTENT) or (Self->CurrentState IS HGS::SEND_COMPLETE)) {
      log.trace("Transition SENDING_CONTENT -> READING_HEADER.");
      Self->setCurrentState(HGS::READING_HEADER);
      Self->Index = 0;
   }

   if ((Self->CurrentState IS HGS::READING_HEADER) or (Self->CurrentState IS HGS::AUTHENTICATING)) {
      auto error = read_incoming_header(Self, Socket);
      if (error IS ERR::Okay) goto restart; // Header read, process any remaining data
      else return error;
   }
   else if (Self->CurrentState IS HGS::READING_CONTENT) {
      if (Self->Chunked) return read_incoming_chunks(Self, Socket);
      else return read_incoming_content(Self, Socket);
   }
   else { // Unexpected data received from HTTP server
      std::string buffer;
      buffer.resize(512);
      int len;
      if ((acRead(Socket, buffer.data(), buffer.size(), &len) IS ERR::Okay) and (len > 0)) {
         log.warning("WARNING: Received data whilst in state %d.", int(Self->CurrentState));
         log.warning("Content (%d bytes) Follows:\n%.80s", len, buffer.c_str());
      }
      return ERR::Terminate;
   }

   return ERR::Okay;
}
