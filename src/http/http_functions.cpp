
//********************************************************************************************************************

static void socket_feedback(objNetSocket *Socket, objClientSocket *Client, NTC State)
{
   pf::Log log("http_feedback");

   log.msg("Socket: %p, Client: %p, State: %d, Context: %d", Socket, Client, LONG(State), CurrentContext()->UID);

   auto Self = (extHTTP *)Socket->ClientData; //(extHTTP *)CurrentContext();
   if (Self->classID() != CLASSID::HTTP) { log.warning(ERR::SystemCorrupt); return; }

   if (State IS NTC::CONNECTING) {
      log.msg("Waiting for connection...");

      if (Self->TimeoutManager) UpdateTimer(Self->TimeoutManager, Self->ConnectTimeout);
      else SubscribeTimer(Self->ConnectTimeout, C_FUNCTION(timeout_manager), &Self->TimeoutManager);

      Self->Connecting = TRUE;
   }
   else if (State IS NTC::CONNECTED) {
      // The GET request has been pre-written to the socket on its creation, so we don't need to do anything further
      // here.

      log.msg("Connection confirmed.");
      if (Self->TimeoutManager) { UpdateTimer(Self->TimeoutManager, 0); Self->TimeoutManager = 0; }
      Self->Connecting = FALSE;
   }
   else if (State IS NTC::DISCONNECTED) {
      // Socket disconnected.  The HTTP state must change to either COMPLETED (completed naturally) or TERMINATED
      // (abnormal termination) to correctly inform the user as to what has happened.

      log.msg("Disconnected from socket while in state %s.", clHTTPCurrentState[LONG(Self->CurrentState)].Name);

      if (Self->TimeoutManager) { UpdateTimer(Self->TimeoutManager, 0); Self->TimeoutManager = 0; }

      if (Self->Connecting) {
         Self->Connecting = FALSE;

         SET_ERROR(log, Self, Socket->Error);
         log.branch("Deactivating (connect failure message received).");
         SetField(Self, FID_CurrentState, HGS::TERMINATED);
         return;
      }
      else Self->Connecting = FALSE;

      if (Self->CurrentState >= HGS::COMPLETED) {
         return;
      }
      else if (Self->CurrentState IS HGS::READING_HEADER) {
         SET_ERROR(log, Self, Socket->Error != ERR::Okay ? Socket->Error : ERR::Disconnected);
         log.trace("Received broken header as follows:\n%s", Self->Response.c_str());
         SetField(Self, FID_CurrentState, HGS::TERMINATED);
      }
      else if (Self->CurrentState IS HGS::SEND_COMPLETE) {
         // Disconnection on completion of sending data should be no big deal
         SET_ERROR(log, Self, Socket->Error != ERR::Okay ? Socket->Error : ERR::Okay);
         Self->setCurrentState(HGS::COMPLETED);
      }
      else if (Self->CurrentState IS HGS::SENDING_CONTENT) {
         SET_ERROR(log, Self, Socket->Error != ERR::Okay ? Socket->Error : ERR::Disconnected);

         // If the socket is not active, then the disconnection is a result of destroying the object (e.g. due to a redirect).

         log.branch("State changing to TERMINATED due to disconnection.");
         Self->setCurrentState(HGS::TERMINATED);
      }
      else if (Self->CurrentState IS HGS::READING_CONTENT) {
         // Unread data can remain on the socket following disconnection, so try to read anything that's been left.

         if (Self->Chunked) {
            log.traceWarning("Support code required to read chunked data following a disconnected socket.");
         }
         else if ((Self->ContentLength IS -1) or (Self->Index < Self->ContentLength)) {
            std::vector<char> buffer(BUFFER_READ_SIZE);

            while (true) {
               LONG len = buffer.size();
               if (Self->ContentLength != -1) {
                  if (len > Self->ContentLength - Self->Index) len = Self->ContentLength - Self->Index;
               }

               if ((Self->Error = acRead(Socket, buffer.data(), len, &len)) != ERR::Okay) {
                  log.warning("Read() returned error: %s", GetErrorMsg(Self->Error));
               }

               if (!len) { // No more incoming data
                  if ((Self->Flags & HTF::DEBUG_SOCKET) != HTF::NIL) {
                     log.msg("Received %d bytes of content in this content reading session.", len);
                  }
                  break;
               }

               process_data(Self, buffer.data(), len);
               if (check_incoming_end(Self) IS ERR::True) break;
            }
         }

         if (Self->ContentLength IS -1) {
            if (Socket->Error IS ERR::Okay) {
               log.msg("Orderly shutdown while streaming data.");
               Self->setCurrentState(HGS::COMPLETED);
            }
            else {
               SET_ERROR(log, Self, Socket->Error);
               SetField(Self, FID_CurrentState, HGS::TERMINATED);
            }
         }
         else if (Self->Index < Self->ContentLength) {
            log.warning("Disconnected before all content was downloaded (%" PF64 " of %" PF64 ")", Self->Index, Self->ContentLength);
            SET_ERROR(log, Self, Socket->Error != ERR::Okay ? Socket->Error : ERR::Disconnected);
            SetField(Self, FID_CurrentState, HGS::TERMINATED);
         }
         else {
            log.trace("Orderly shutdown, received %" PF64 " of the expected %" PF64 " bytes.", Self->Index, Self->ContentLength);
            SetField(Self, FID_CurrentState, HGS::COMPLETED);
         }
      }
      else if (Self->CurrentState IS HGS::AUTHENTICATING) {
         if (Self->DialogWindow) {
            // The HTTP socket was closed because the user is taking too long
            // to authenticate with the dialog window.  We will close the socket
            // and create a new one once the user responds to the dialog.

            Self->Socket->set(FID_Feedback, (APTR)NULL);
            FreeResource(Socket);
            Self->Socket = NULL;
            Self->SecurePath = TRUE;
            return;
         }

         SetField(Self, FID_CurrentState, HGS::TERMINATED);
      }
   }
   else if (Self->CurrentState >= HGS::COMPLETED) {
      // If the state is set to HGS::COMPLETED or HGS::TERMINATED, our code should have returned ERR::Terminate to switch
      // off the socket.  This section is entered if we forgot to do that.

      log.warning("Warning - socket channel was not closed correctly (didn't return ERR::Terminate).");
   }
}

//********************************************************************************************************************
// Callback: NetSocket.Outgoing

static ERR socket_outgoing(objNetSocket *Socket)
{
   pf::Log log("http_outgoing");

   static const LONG CHUNK_LENGTH_OFFSET = 16;
   static const LONG CHUNK_TAIL = 2; // CRLF

   auto Self = (extHTTP *)Socket->ClientData;
   if (Self->classID() != CLASSID::HTTP) return log.warning(ERR::SystemCorrupt);

   log.traceBranch("Socket: %p, Object: %d, State: %d", Socket, CurrentContext()->UID, LONG(Self->CurrentState));

   LONG total_out = 0;

   if (!Self->Buffer) {
      if (Self->BufferSize < BUFFER_WRITE_SIZE) Self->BufferSize = BUFFER_WRITE_SIZE;
      if (Self->BufferSize > 0xffff) Self->BufferSize = 0xffff;

      if (AllocMemory(Self->BufferSize, MEM::DATA|MEM::NO_CLEAR, &Self->Buffer) != ERR::Okay) {
         return ERR::AllocMemory;
      }
   }

   ERR error = ERR::Okay;
redo_upload:
   Self->WriteBuffer = (UBYTE *)Self->Buffer;
   Self->WriteSize   = Self->BufferSize;
   if (Self->Chunked) {
      Self->WriteBuffer += CHUNK_LENGTH_OFFSET;
      Self->WriteSize   -= CHUNK_LENGTH_OFFSET + CHUNK_TAIL;
   }

   if (Self->CurrentState != HGS::SENDING_CONTENT) {
      Self->setCurrentState(HGS::SENDING_CONTENT);
   }

   LONG len = 0;
   if (Self->Outgoing.defined()) {
      if (Self->Outgoing.isC()) {
         auto routine = (ERR (*)(extHTTP *, APTR, LONG, LONG *, APTR))Self->Outgoing.Routine;
         error = routine(Self, Self->WriteBuffer, Self->WriteSize, &len, Self->Outgoing.Meta);
      }
      else if (Self->Outgoing.isScript()) {
         // For a script to write to the buffer, it needs to make a call to the Write() action.
         if (scCall(Self->Outgoing, std::to_array<ScriptArg>({
               { "HTTP",       Self, FD_OBJECTPTR },
               { "BufferSize", Self->WriteSize }
            }), error) != ERR::Okay) error = ERR::Failed;
         if (error > ERR::ExceptionThreshold) {
            log.warning("Procedure %" PF64 " failed, aborting HTTP call.", Self->Outgoing.ProcedureID);
         }
         else len = Self->WriteOffset;
      }
      else error = ERR::InvalidValue;

      if (len > Self->WriteSize) { // Sanity check, this should never happen if the client uses valid code.
         log.warning("Returned length exceeds buffer size!  %d > %d", len, Self->WriteSize);
         len = Self->WriteSize;
         error = ERR::BufferOverflow;
      }
      else if (error > ERR::ExceptionThreshold) log.warning("Outgoing callback error: %s", GetErrorMsg(error));
   }
   else if (Self->flInput) {
      if ((Self->Flags & HTF::LOG_ALL) != HTF::NIL) log.msg("Sending content from an Input file.");

      error = acRead(Self->flInput, Self->WriteBuffer, Self->WriteSize, &len);

      if (error != ERR::Okay) log.warning("Input file read error: %s", GetErrorMsg(error));

      LARGE size;
      Self->flInput->get(FID_Size, &size);

      if ((Self->flInput->Position IS size) or (len IS 0)) {
         log.trace("All file content read (%d bytes) - freeing file.", (LONG)size);
         FreeResource(Self->flInput);
         Self->flInput = NULL;
         if (error IS ERR::Okay) error = ERR::Terminate;
      }
   }
   else if (Self->InputObjectID) {
      if ((Self->Flags & HTF::LOG_ALL) != HTF::NIL) log.msg("Sending content from InputObject #%d.", Self->InputObjectID);

      pf::ScopedObjectLock object(Self->InputObjectID, 100);
      if (object.granted()) error = acRead(*object, Self->WriteBuffer, Self->WriteSize, &len);

      if (error != ERR::Okay) log.warning("Input object read error: %s", GetErrorMsg(error));
   }
   else {
      if (Self->MultipleInput) error = ERR::NoData;
      else error = ERR::Terminate;

      log.warning("Method %d: No input fields are defined for me to send data to the server.", LONG(Self->Method));
   }

   if (((error IS ERR::Okay) or (error IS ERR::Terminate)) and (len)) {
      LONG result, csize;
      ERR writeerror;

      log.trace("Writing %d bytes (of expected %" PF64 ") to socket.  Chunked: %d", len, Self->ContentLength, Self->Chunked);

      if (Self->Chunked) {
         if (len & 0xf000)      { csize = 4+2; snprintf((char *)Self->WriteBuffer-6, 5, "%.4x", len); }
         else if (len & 0x0f00) { csize = 3+2; snprintf((char *)Self->WriteBuffer-5, 4, "%.3x", len); }
         else if (len & 0x00f0) { csize = 2+2; snprintf((char *)Self->WriteBuffer-4, 3, "%.2x", len); }
         else { csize = 1+2; snprintf((char *)Self->WriteBuffer-3, 2, "%.1x", len); }

         Self->WriteBuffer[-1] = '\n';
         Self->WriteBuffer[-2] = '\r';

         Self->WriteBuffer[len] = '\r';
         Self->WriteBuffer[len+1] = '\n';

         // Note: If the result were to come back as less than the length we intended to write,
         // it would screw up the entire sending process when using chunks.  However we don't
         // have to worry as the NetSocket will buffer up to 1 MB of data at a time - so we're
         // safe so long as we're only sending data when the outgoing socket is empty.

         writeerror = write_socket(Self, Self->WriteBuffer-csize, csize + len + CHUNK_TAIL, &result);
         len = result - csize - CHUNK_TAIL;
      }
      else {
         writeerror = write_socket(Self, Self->WriteBuffer, len, &result);
         if (len != result) log.warning("Only sent %d of %d bytes.", len, result);
         len = result;
      }

      total_out += result;
      Self->TotalSent += result;

      Self->setIndex(Self->Index + len);

      if (writeerror != ERR::Okay) {
         log.warning("write_socket() failed: %s", GetErrorMsg(writeerror));
         error = writeerror;
      }

      log.trace("Outgoing index now %" PF64 " of %" PF64, Self->Index, Self->ContentLength);
   }
   else log.trace("Finishing (an error occurred (%d), or there is no more content to write to socket).", error);

   if ((error != ERR::Okay) and (error != ERR::Terminate)) {
      if (error != ERR::TimeOut) {
         Self->setCurrentState(HGS::TERMINATED);
         SET_ERROR(log, Self, error);
         return ERR::Terminate;
      }
      // ERR::TimeOut: The upload process may continue
   }
   else {
      // Check for multiple input files

      if ((Self->MultipleInput) and (!Self->flInput)) {
         /*if ((Self->Flags & HTF::LOG_ALL) != HTF::NIL)*/ log.msg("Sequential input stream has uploaded %" PF64 "/%" PF64 " bytes.", Self->Index, Self->ContentLength);

         // Open the next file

         if (parse_file(Self, (STRING)Self->Buffer, Self->BufferSize) IS ERR::Okay) {
            if ((Self->flInput = objFile::create::local(fl::Path((CSTRING)Self->Buffer), fl::Flags(FL::READ)))) {
               if (total_out < Self->BufferSize) goto redo_upload; // Upload as much as possible in each pass
               else goto continue_upload;
            }
         }
      }

      // Check if the upload is complete - either Index >= ContentLength or ERR::Terminate has been given as the return code.
      //
      // Note: On completion of an upload, the HTTP server will normally send back a message to confirm completion of
      // the upload, therefore the state is not changed to HGS::COMPLETED.
      //
      // In the case where the server does not respond to completion of the upload, the timeout would eventually take care of it.

      if (((Self->ContentLength > 0) and (Self->Index >= Self->ContentLength)) or (error IS ERR::Terminate)) {
         LONG result;

         if (Self->Chunked) write_socket(Self, (UBYTE *)"0\r\n\r\n", 5, &result);

         if ((Self->Flags & HTF::LOG_ALL) != HTF::NIL) log.msg("Transfer complete - sent %" PF64 " bytes.", Self->TotalSent);
         Self->setCurrentState(HGS::SEND_COMPLETE);
         return ERR::Terminate;
      }
      else {
         if ((Self->Flags & HTF::LOG_ALL) != HTF::NIL) log.msg("Sent %" PF64 " bytes of %" PF64, Self->Index, Self->ContentLength);
      }
   }

   // Data timeout when uploading is high due to content buffering
continue_upload:
   Self->LastReceipt = PreciseTime();

   DOUBLE time_limit = (Self->DataTimeout > 30) ? Self->DataTimeout : 30;

   if (Self->TimeoutManager) UpdateTimer(Self->TimeoutManager, time_limit);
   else SubscribeTimer(time_limit, C_FUNCTION(timeout_manager), &Self->TimeoutManager);

   Self->WriteBuffer = NULL;
   Self->WriteSize = 0;

   if (Self->Error != ERR::Okay) return ERR::Terminate;
   return ERR::Okay;
}

//********************************************************************************************************************
// Callback: NetSocket.Incoming

static ERR socket_incoming(objNetSocket *Socket)
{
   pf::Log log("http_incoming");
   LONG len;
   auto Self = (extHTTP *)Socket->ClientData;

   log.msg("Context: %d", CurrentContext()->UID);

   if (Self->classID() != CLASSID::HTTP) return log.warning(ERR::SystemCorrupt);

   if (Self->CurrentState >= HGS::COMPLETED) {
      // Erroneous data received from server while we are in a completion/resting state.  Returning a terminate message
      // will cause the socket object to close the connection to the server so that we stop receiving erroneous data.

      log.warning("Unexpected data incoming from server - terminating socket.");
      return ERR::Terminate;
   }

   if (Self->CurrentState IS HGS::SENDING_CONTENT) {
      if (Self->ContentLength IS -1) {
         log.warning("Incoming data while streaming content - %" PF64 " bytes already written.", Self->Index);
      }
      else if (Self->Index < Self->ContentLength) {
         log.warning("Incoming data while sending content - only %" PF64 "/%" PF64 " bytes written!", Self->Index, Self->ContentLength);
      }
   }

   if ((Self->CurrentState IS HGS::SENDING_CONTENT) or (Self->CurrentState IS HGS::SEND_COMPLETE)) {
      log.trace("Switching state from sending content to reading header.");
      Self->setCurrentState(HGS::READING_HEADER);
      Self->Index = 0;
   }

   if ((Self->CurrentState IS HGS::READING_HEADER) or (Self->CurrentState IS HGS::AUTHENTICATING)) {
      log.trace("HTTP received data, reading header.");

      // Note that the response always inserts a hidden null-terminator for printing as a string.

      while (true) {
         if (Self->Response.empty()) Self->Response.resize(256);

         if (Self->ResponseIndex + 1 >= std::ssize(Self->Response)) {
            Self->Response.resize(Self->Response.size() + 256);
         }

         Self->Error = acRead(Socket, Self->Response.data() + Self->ResponseIndex, 
            Self->Response.size() - 1 - Self->ResponseIndex, &len);

         if (Self->Error != ERR::Okay) {
            log.warning(Self->Error);
            return ERR::Terminate;
         }

         if (len < 1) break; // No more incoming data
         Self->ResponseIndex += len;
         Self->Response[Self->ResponseIndex] = 0;

         // Advance search for terminated double CRLF

         for (; Self->SearchIndex+4 <= Self->ResponseIndex; Self->SearchIndex++) {
            if (!strncmp(Self->Response.c_str() + Self->SearchIndex, "\r\n\r\n", 4)) {
               Self->Response[Self->SearchIndex] = 0; // Terminate the header at the CRLF point

               if (parse_response(Self, Self->Response) != ERR::Okay) {
                  SET_ERROR(log, Self, log.warning(ERR::InvalidHTTPResponse));
                  return ERR::Terminate;
               }

               if (Self->Tunneling) {
                  if (Self->Status IS HTS::OKAY) {
                     // Proxy tunnel established.  Convert the socket to an SSL connection, then send the HTTP command.

                     if (netSetSSL(Socket, NSL::CONNECT, TRUE, TAGEND) IS ERR::Okay) {
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
                  QueueAction(AC_Activate, Self->UID);
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

               if ((!Self->ContentLength) or (Self->ContentLength < -1)) {
                  log.msg("Reponse header received, no content imminent.");
                  Self->setCurrentState(HGS::COMPLETED);
                  return ERR::Terminate;
               }

               log.msg("Complete response header has been received.  Incoming Content: %" PF64, Self->ContentLength);

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
                        for (LONG i=0; i < std::ssize(Self->Password); i++) Self->Password[i] = char(0xff);
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

                        LONG i = 6;
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

               len = Self->ResponseIndex - (Self->SearchIndex + 4);

               if (Self->Chunked) {
                  log.trace("Content to be received in chunks.");
                  Self->ChunkSize  = 4096;
                  Self->ChunkIndex = 0; // Number of bytes processed for the current chunk
                  Self->ChunkLen   = 0;  // Length of the first chunk is unknown at this stage
                  Self->ChunkBuffered = len;
                  if (len > Self->ChunkSize) Self->ChunkSize = len;
                  if (AllocMemory(Self->ChunkSize, MEM::DATA|MEM::NO_CLEAR, &Self->Chunk) IS ERR::Okay) {
                     if (len > 0) CopyMemory(Self->Response.data() + Self->SearchIndex + 4, Self->Chunk, len);
                  }
                  else {
                     SET_ERROR(log, Self, log.warning(ERR::AllocMemory));
                     return ERR::Terminate;
                  }

                  Self->SearchIndex = 0;
               }
               else {
                  log.trace("%" PF64 " bytes of content is incoming.  Bytes Buffered: %d, Index: %" PF64, Self->ContentLength, len, Self->Index);

                  if (len > 0) process_data(Self, Self->Response.data() + Self->SearchIndex + 4, len);
               }

               check_incoming_end(Self);

               Self->Response.clear();

               // Note that status check comes after processing of content, as it is legal for content to be attached
               // with bad status codes (e.g. SOAP does this).

               if ((LONG(Self->Status) < 200) or (LONG(Self->Status) >= 300)) {
                  if (Self->CurrentState != HGS::READING_CONTENT) {
                     if (Self->Status IS HTS::UNAUTHORISED) log.warning("Exhausted maximum number of retries.");
                     else log.warning("Status code %d != 2xx", LONG(Self->Status));

                     SET_ERROR(log, Self, ERR::Failed);
                     return ERR::Terminate;
                  }
                  else log.warning("Status code %d != 2xx.  Receiving content...", LONG(Self->Status));
               }

               return ERR::Okay;
            }
         }
      }
   }
   else if (Self->CurrentState IS HGS::READING_CONTENT) {
      if (Self->Chunked) {
         // Data chunk mode.  Store received data in a chunk buffer.  As long as we know the entire size of the
         // chunk, all data can be immediately passed onto our subscribers.
         //
         // Chunked data is passed as follows:
         //
         // ChunkSize\r\n
         // Data....
         // ChunkSize\r\n
         // Data...
         // \r\n (indicates end) or 0\r\n (indicates end of chunks with further HTTP tags following)
         //
         // ChunkIndex:    Current read position within the buffer.
         // ChunkSize:     Size of the chunk buffer.
         // ChunkBuffered: Number of bytes currently buffered.
         // ChunkLen:      Expected length of the next chunk (decreases as bytes are processed).

         LONG i, count;
         for (count=2; count > 0; count--) { //while (Self->ChunkIndex < Self->ChunkBuffered) {
            pf::Log log("http_incoming");
            log.traceBranch("Receiving content (chunk mode) Index: %d/%d/%d, Length: %d", Self->ChunkIndex, Self->ChunkBuffered, Self->ChunkSize, Self->ChunkLen);

            // Compress the buffer

            if (Self->ChunkIndex > 0) {
               //log.msg("Compressing the chunk buffer.");
               if (Self->ChunkBuffered > Self->ChunkIndex) {
                  CopyMemory(Self->Chunk + Self->ChunkIndex, Self->Chunk, Self->ChunkBuffered - Self->ChunkIndex);
               }
               Self->ChunkBuffered -= Self->ChunkIndex;
               Self->ChunkIndex = 0;
            }

            // Fill the chunk buffer

            if (Self->ChunkBuffered < Self->ChunkSize) {
               Self->Error = acRead(Socket, Self->Chunk + Self->ChunkBuffered, Self->ChunkSize - Self->ChunkBuffered, &len);

               //log.msg("Filling the chunk buffer: Read %d bytes.", len);

               if (Self->Error IS ERR::Disconnected) {
                  log.msg("Received all chunked content (disconnected by peer).");
                  Self->setCurrentState(HGS::COMPLETED);
                  return ERR::Terminate;
               }
               else if (Self->Error != ERR::Okay) {
                  log.warning("Read() returned error %d whilst reading content.", LONG(Self->Error));
                  Self->setCurrentState(HGS::COMPLETED);
                  return ERR::Terminate;
               }
               else if ((!len) and (Self->ChunkIndex >= Self->ChunkBuffered)) {
                  log.msg("Nothing left to read.");
                  return ERR::Okay;
               }
               else Self->ChunkBuffered += len;
            }

            while (Self->ChunkIndex < Self->ChunkBuffered) {
               //log.msg("Status: Index: %d/%d, CurrentChunk: %d", Self->ChunkIndex, Self->ChunkBuffered, Self->ChunkLen);

               if (!Self->ChunkLen) {
                  // Read the next chunk header.  It is assumed that the format is:
                  //
                  // ChunkSize\r\n
                  // Data...

                  log.msg("Examining chunk header (%d bytes buffered).", Self->ChunkBuffered - Self->ChunkIndex);

                  for (i=Self->ChunkIndex; i < Self->ChunkBuffered-1; i++) {
                     if ((Self->Chunk[i] IS '\r') and (Self->Chunk[i+1] IS '\n')) {
                        Self->Chunk[i] = 0;
                        Self->ChunkLen = strtoll((CSTRING)Self->Chunk + Self->ChunkIndex, NULL, 0);
                        Self->Chunk[i] = '\r';

                        if (Self->ChunkLen <= 0) {
                           if (Self->Chunk[Self->ChunkIndex] IS '0') {
                              // A line of "0\r\n" indicates an end to the chunks, followed by optional data for
                              // interpretation.

                              log.msg("End of chunks reached, optional data follows.");
                              Self->setCurrentState(HGS::COMPLETED);
                              return ERR::Terminate;
                           }
                           else {
                              // We have reached the terminating line (CRLF on an empty line)
                              log.msg("Received all chunked content.");
                              Self->setCurrentState(HGS::COMPLETED);
                              return ERR::Terminate;
                           }
                        }

                        log.msg("Next chunk length is %d bytes.", Self->ChunkLen);
                        Self->ChunkIndex = i + 2; // \r\n
                        break;
                     }
                  }

                  // Quit the main loop if we still don't have a chunk length (more data needs to be read from the HTTP socket).

                  if (!Self->ChunkLen) break;
               }

               if (Self->ChunkLen > 0) {
                  len = Self->ChunkBuffered - Self->ChunkIndex;
                  if (len > Self->ChunkLen) len = Self->ChunkLen; // Cannot process more bytes than the expected chunk length

                  //log.msg("%d bytes left to process in current chunk, sending %d bytes", Self->ChunkLen, len);

                  Self->ChunkLen -= len;
                  process_data(Self, Self->Chunk+Self->ChunkIndex, len);

                  Self->ChunkIndex += len;

                  if (!Self->ChunkLen) {
                     // The end of the chunk binary is followed with a CRLF
                     //log.msg("A complete chunk has been processed.");
                     Self->ChunkLen = -2;
                  }
               }

               if (Self->ChunkLen < 0) {
                  //log.msg("Skipping %d bytes.", -Self->ChunkLen);

                  while ((Self->ChunkLen < 0) and (Self->ChunkIndex < Self->ChunkBuffered)) {
                     Self->ChunkIndex++;
                     Self->ChunkLen++;
                  }

                  if (Self->ChunkLen < 0) break; // If we did not receive all the bytes, break to continue processing until more bytes are ready
               }
            }
         }
      }
      else {
         std::vector<char> buffer(BUFFER_READ_SIZE);

         // Maximum number of times that this subroutine can loop (on a fast network we could otherwise download indefinitely).
         // A limit of 64K per read session is acceptable with a time limit of 1/200 frames.

         LONG looplimit = (64 * 1024) / BUFFER_READ_SIZE;
         LARGE timelimit = PreciseTime() + 5000000LL;

         while (true) {
            len = BUFFER_READ_SIZE;
            if (Self->ContentLength != -1) {
               if (len > Self->ContentLength - Self->Index) len = Self->ContentLength - Self->Index;
            }

            if ((Self->Error = acRead(Socket, buffer.data(), len, &len)) != ERR::Okay) {
               if ((Self->Error IS ERR::Disconnected) and (Self->ContentLength IS -1)) {
                  log.trace("Received all streamed content (disconnected by peer).");
                  Self->setCurrentState(HGS::COMPLETED);
                  return ERR::Terminate;
               }
               else {
                  log.warning("Read() returned error %d whilst reading content.", LONG(Self->Error));
                  return ERR::Terminate;
               }
            }

            if (!len) break; // No more incoming data right now

            process_data(Self, buffer.data(), len);
            if (check_incoming_end(Self) IS ERR::True) {
               return ERR::Terminate;
            }

            if (--looplimit <= 0) break; // Looped many times, need to break
            if (PreciseTime() > timelimit) break; // Time limit reached
         }
      }

      Self->LastReceipt = PreciseTime();

      if (Self->TimeoutManager) UpdateTimer(Self->TimeoutManager, Self->DataTimeout);
      else SubscribeTimer(Self->DataTimeout, C_FUNCTION(timeout_manager), &Self->TimeoutManager);

      if (Self->Error != ERR::Okay) return ERR::Terminate;
   }
   else {
      char buffer[512];
      // Indeterminate data received from HTTP server

      if ((acRead(Socket, buffer, sizeof(buffer)-1, &len) IS ERR::Okay) and (len > 0)) {
         buffer[len] = 0;
         log.warning("WARNING: Received data whilst in state %d.", LONG(Self->CurrentState));
         log.warning("Content (%d bytes) Follows:\n%.80s", len, buffer);
      }
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR parse_response(extHTTP *Self, std::string_view Response)
{
   pf::Log log;

   Self->Args.clear();

   if ((Self->Flags & HTF::LOG_ALL) != HTF::NIL) {
      log.msg("HTTP RESPONSE HEADER\n%s", std::string(Response).c_str());
   }

   // First line: HTTP/1.1 200 OK

   if (!Response.starts_with("HTTP/")) {
      log.warning("Invalid response header, missing 'HTTP/'");
      return ERR::InvalidHTTPResponse;
   }

   //LONG majorv = StrToInt(str); // Currently unused
   auto b = Response.find_first_of('.');
   if (b IS std::string::npos) return ERR::InvalidHTTPResponse;
   b++;

   //LONG minorv = StrToInt(str); // Currently unused
   while ((b < Response.size()) and (Response[b] > 0x20)) b++;
   while ((b < Response.size()) and (Response[b] <= 0x20)) b++;
   
   LONG code = 0;
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

      if (i IS std::string::npos) return ERR::Okay;
      else if (Response[i] IS ':') { // Detected "Key: Value"
         auto field = Response.substr(0, i);
         i++;
         while ((i < Response.size()) and (Response[i] <= 0x20)) i++;

         auto end_line = Response.find('\r', i);
         auto value = Response.substr(i, end_line - i);

         if (pf::iequals(field, "Content-Length")) {
            Self->ContentLength = 0;
            auto [ ptr, error ] = std::from_chars(value.data(), value.data() + value.size(), Self->ContentLength);
         }
         else if (pf::iequals(field, "Transfer-Encoding")) {
            if (pf::iequals(value, "chunked")) {
               if ((Self->Flags & HTF::RAW) IS HTF::NIL) Self->Chunked = true;
               Self->ContentLength = -1;
            }
         }

         Self->Args[std::string(field)] = value;

         if (end_line IS std::string::npos) Response = std::string_view();
         else Response.remove_prefix(end_line + 2);
      }
      else Response.remove_prefix(i + 2);
   }

   return ERR::Okay;
}

//********************************************************************************************************************
// Sends some data specified in the arguments to the listener

static ERR process_data(extHTTP *Self, APTR Buffer, LONG Length)
{
   pf::Log log(__FUNCTION__);

   log.trace("Buffer: %p, Length: %d", Buffer, Length);

   if (!Length) return ERR::Okay;

   Self->setIndex((LARGE)Self->Index + (LARGE)Length); // Use Set() so that field subscribers can track progress with field monitoring

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

   if (Self->flOutput) Self->flOutput->write(Buffer, Length, NULL);

   if ((Self->Flags & HTF::RECV_BUFFER) != HTF::NIL) {
      Self->RecvBuffer.resize(Length+1);
      CopyMemory(Buffer, Self->RecvBuffer.data(), Self->RecvBuffer.size());
      Self->RecvBuffer[Length] = 0;
   }

   if (Self->Incoming.defined()) {
      log.trace("Incoming callback is set.");

      ERR error;
      if (Self->Incoming.isC()) {
         auto routine = (ERR (*)(extHTTP *, APTR, LONG, APTR))Self->Incoming.Routine;
         error = routine(Self, Buffer, Length, Self->Incoming.Meta);
      }
      else if (Self->Incoming.isScript()) {
         // For speed, the client will receive a direct pointer to the buffer memory via the 'mem' interface.

         log.trace("Calling script procedure %" PF64, Self->Incoming.ProcedureID);

         if (scCall(Self->Incoming, std::to_array<ScriptArg>({
               { "HTTP",       Self,   FD_OBJECTPTR },
               { "Buffer",     Buffer, FD_PTRBUFFER },
               { "BufferSize", Length, FD_LONG|FD_BUFSIZE }
            }), error) != ERR::Okay) error = ERR::Terminate;
      }
      else error = ERR::InvalidValue;

      if (error != ERR::Okay) SET_ERROR(log, Self, error);

      if (Self->Error IS ERR::Terminate) {
         pf::Log log(__FUNCTION__);
         log.branch("State changing to HGS::TERMINATED (terminate message received).");
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

static LONG extract_value(std::string_view String, std::string &Result)
{
   if (auto s = String.find_first_of("=,"); s IS std::string::npos) {
      Result.clear();
      return String.size();
   }
   else if (String[s] IS '=') {
      s++;
      if (String[s] IS '"') {
         s++;
         if (auto i = String.find('"', s); i != std::string::npos) {
            Result.assign(String, s, i-s);
            s = i + 1; // Skip "
            if (auto comma = String.find(','); comma != std::string::npos) {
               s = comma + 1;
               while ((s < String.size()) and (String[s] <= 0x20)) s++;
            }
            return s;
         }
         else return String.size();
      }
      else if (auto i = String.find(','); i IS std::string::npos) {
         Result.assign(String, s);
         return String.size();
      }
      else {
         Result.assign(String, s, i-s);
         s = i + 1;
         while ((s < String.size()) and (String[s] <= 0x20)) s++;
         return s;
      }
   }
   else {
      Result.clear();
      return s;
   }
}

//********************************************************************************************************************

static void writehex(HASH Bin, HASHHEX Hex)
{
   for (ULONG i=0; i < HASHLEN; i++) {
      UBYTE j = (Bin[i] >> 4) & 0xf;

      if (j <= 9) Hex[i<<1] = (j + '0');
      else Hex[i*2] = (j + 'a' - 10);

      j = Bin[i] & 0xf;

      if (j <= 9) Hex[(i<<1)+1] = (j + '0');
      else Hex[(i<<1)+1] = (j + 'a' - 10);
   }
   Hex[HASHHEXLEN] = 0;
}

//********************************************************************************************************************
// Calculate H(A1) as per spec

static void digest_calc_ha1(extHTTP *Self, HASHHEX SessionKey)
{
   MD5_CTX md5;
   HASH HA1;

   MD5Init(&md5);

   if (!Self->Username.empty()) MD5Update(&md5, (UBYTE *)Self->Username.c_str(), Self->Username.size());

   MD5Update(&md5, (UBYTE *)":", 1);

   if (!Self->Realm.empty()) MD5Update(&md5, (UBYTE *)Self->Realm.c_str(), Self->Realm.size());

   MD5Update(&md5, (UBYTE *)":", 1);

   if (!Self->Password.empty()) MD5Update(&md5, (UBYTE *)Self->Password.c_str(), Self->Password.size());

   MD5Final((UBYTE *)HA1, &md5);

   if (pf::iequals(Self->AuthAlgorithm, "md5-sess")) {
      MD5Init(&md5);
      MD5Update(&md5, (UBYTE *)HA1, HASHLEN);
      MD5Update(&md5, (UBYTE *)":", 1);
      if (!Self->AuthNonce.empty()) MD5Update(&md5, (UBYTE *)Self->AuthNonce.c_str(), Self->AuthNonce.size());
      MD5Update(&md5, (UBYTE *)":", 1);
      MD5Update(&md5, (UBYTE *)Self->AuthCNonce.c_str(), Self->AuthCNonce.size());
      MD5Final((UBYTE *)HA1, &md5);
   }

   writehex(HA1, SessionKey);
}

//********************************************************************************************************************
// Calculate request-digest/response-digest as per HTTP Digest spec

static void digest_calc_response(extHTTP *Self, std::string Request, CSTRING NonceCount, HASHHEX HA1, HASHHEX HEntity, HASHHEX Response)
{
   pf::Log log;
   MD5_CTX md5;
   HASH HA2;
   HASH RespHash;
   HASHHEX HA2Hex;
   LONG i;

   // Calculate H(A2)

   MD5Init(&md5);

   auto req = Request.c_str();
   for (i=0; req[i] > 0x20; i++);
   MD5Update(&md5, (UBYTE *)req, i); // Compute MD5 from the name of the HTTP method that we are calling
   while ((req[i]) and (req[i] <= 0x20)) i++; // Skip whitespace

   MD5Update(&md5, (UBYTE *)":", 1);

   req += i;
   for (i=0; req[i] > 0x20; i++);
   MD5Update(&md5, (UBYTE *)req, i); // Compute MD5 from the path of the HTTP method that we are calling

   if (pf::iequals(Self->AuthQOP, "auth-int")) {
      MD5Update(&md5, (UBYTE *)":", 1);
      MD5Update(&md5, (UBYTE *)HEntity, HASHHEXLEN);
   }

   MD5Final((UBYTE *)HA2, &md5);
   writehex(HA2, HA2Hex);

   // Calculate response:  HA1Hex:Nonce:NonceCount:CNonce:auth:HA2Hex

   MD5Init(&md5);
   MD5Update(&md5, (UBYTE *)HA1, HASHHEXLEN);
   MD5Update(&md5, (UBYTE *)":", 1);
   MD5Update(&md5, (UBYTE *)Self->AuthNonce.c_str(), Self->AuthNonce.size());
   MD5Update(&md5, (UBYTE *)":", 1);

   if (!Self->AuthQOP.empty()) {
      MD5Update(&md5, (UBYTE *)NonceCount, StrLength((CSTRING)NonceCount));
      MD5Update(&md5, (UBYTE *)":", 1);
      MD5Update(&md5, (UBYTE *)Self->AuthCNonce.c_str(), Self->AuthCNonce.size());
      MD5Update(&md5, (UBYTE *)":", 1);
      MD5Update(&md5, (UBYTE *)Self->AuthQOP.c_str(), Self->AuthQOP.size());
      MD5Update(&md5, (UBYTE *)":", 1);
   }

   MD5Update(&md5, (UBYTE *)HA2Hex, HASHHEXLEN);
   MD5Final((UBYTE *)RespHash, &md5);
   writehex(RespHash, Response);

   log.trace("%s:%s:%s:%s:%s:%s", HA1, Self->AuthNonce.c_str(), NonceCount, Self->AuthCNonce.c_str(), Self->AuthQOP.c_str(), HA2Hex);
}

//********************************************************************************************************************

static ERR write_socket(extHTTP *Self, CPTR Buffer, LONG Length, LONG *Result)
{
   pf::Log log(__FUNCTION__);

   if (Length > 0) {
      //log.trace("Length: %d", Length);

      if ((Self->Flags & HTF::DEBUG_SOCKET) != HTF::NIL) {
         log.msg("SOCKET-OUTGOING: LEN: %d", Length);
         for (LONG i=0; i < Length; i++) if ((((UBYTE *)Buffer)[i] < 128) and (((UBYTE *)Buffer)[i] >= 10)) {
            printf("%c", ((STRING)Buffer)[i]);
         }
         else printf("?");
         printf("\n");
      }

      return acWrite(Self->Socket, Buffer, Length, Result);
   }
   else {
      *Result = 0;
      log.traceWarning("Warning - empty write_socket() call.");
      return ERR::Okay;
   }
}

/*********************************************************************************************************************
** The timer is used for managing time-outs on connection to and the receipt of data from the http server.  If the
** timer is activated then we close the current socket.  It should be noted that if the content is streamed, then
** it is not unusual for the client to remain unnotified even in the event of a complete transfer.  Because of this,
** the client should check if the content is streamed in the event of a timeout and not necessarily assume failure.
*/

static ERR timeout_manager(extHTTP *Self, LARGE Elapsed, LARGE CurrentTime)
{
   pf::Log log(__FUNCTION__);

   log.warning("Timeout detected - disconnecting from server (connect %.2fs, data %.2fs).", Self->ConnectTimeout, Self->DataTimeout);
   Self->TimeoutManager = 0;
   SET_ERROR(log, Self, ERR::TimeOut);
   Self->setCurrentState(HGS::TERMINATED);
   return ERR::Terminate;
}

//********************************************************************************************************************
// Returns ERR::True if the transmission is complete and also sets status to HGS::COMPLETED, otherwise ERR::False.

static ERR check_incoming_end(extHTTP *Self)
{
   pf::Log log(__FUNCTION__);

   if (Self->CurrentState IS HGS::AUTHENTICATING) return ERR::False;
   if (Self->CurrentState >= HGS::COMPLETED) return ERR::True;

   if ((Self->ContentLength != -1) and (Self->Index >= Self->ContentLength)) {
      log.trace("Transmission over.");
      if (Self->Index > Self->ContentLength) log.warning("Warning: received too much content.");
      Self->setCurrentState(HGS::COMPLETED);
      return ERR::True;
   }
   else {
      log.trace("Transmission continuing.");
      return ERR::False;
   }
}

//********************************************************************************************************************

static void set_http_method(extHTTP *Self, CSTRING Method, std::ostringstream &Cmd)
{
   if ((Self->ProxyServer) and ((Self->Flags & HTF::SSL) IS HTF::NIL)) {
      // Normal proxy request without SSL tunneling
      Cmd << Method << " " << ((Self->Port IS 443) ? "https" : "http") << "://" << Self->Host << ":" <<
         Self->Port << "/" << (Self->Path ? Self->Path : "") << " HTTP/1.1" << CRLF;
   }
   else Cmd << Method << " /" << (Self->Path ? Self->Path : (STRING)"") << " HTTP/1.1" << CRLF;

   Cmd << "Host: " << Self->Host << CRLF;
   Cmd << "User-Agent: " << Self->UserAgent << CRLF;
}

//********************************************************************************************************************

static ERR parse_file(extHTTP *Self, STRING Buffer, LONG Size)
{
   LONG i;
   LONG pos = Self->InputPos;
   for (i=0; (i < Size-1) and (Self->InputFile[pos]);) {
      if (Self->InputFile[pos] IS '"') {
         pos++;
         while ((i < Size-1) and (Self->InputFile[pos]) and (Self->InputFile[pos] != '"')) {
            Buffer[i++] = Self->InputFile[pos++];
         }
         if (Self->InputFile[pos] IS '"') pos++;
      }
      else if (Self->InputFile[pos] IS '|') {
         pos++;
         while ((Self->InputFile[pos]) and (Self->InputFile[pos] <= 0x20)) pos++;
         break;
      }
      else Buffer[i++] = Self->InputFile[pos++];
   }
   Buffer[i] = 0;
   Self->InputPos = pos;

   if (i >= Size-1) return ERR::BufferOverflow;
   if (!i) return ERR::EmptyString;
   return ERR::Okay;
}

//********************************************************************************************************************

static void parse_file(extHTTP *Self, std::ostringstream &Cmd)
{
   LONG pos = Self->InputPos;
   while (Self->InputFile[pos]) {
      if (Self->InputFile[pos] IS '"') {
         pos++;
         while ((Self->InputFile[pos]) and (Self->InputFile[pos] != '"')) {
            Cmd.put(Self->InputFile[pos++]);
         }
         if (Self->InputFile[pos] IS '"') pos++;
      }
      else if (Self->InputFile[pos] IS '|') {
         pos++;
         while ((Self->InputFile[pos]) and (Self->InputFile[pos] <= 0x20)) pos++;
         break;
      }
      else Cmd.put(Self->InputFile[pos++]);
   }
   Self->InputPos = pos;
}
