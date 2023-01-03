
//****************************************************************************

static void socket_feedback(objNetSocket *Socket, objClientSocket *Client, LONG State)
{
   parasol::Log log("http_feedback");

   log.msg("Socket: %p, Client: %p, State: %d, Context: %d", Socket, Client, State, CurrentContext()->UID);

   auto Self = (extHTTP *)Socket->UserData; //(extHTTP *)CurrentContext();
   if (Self->ClassID != ID_HTTP) { log.warning(ERR_SystemCorrupt); return; }

   if (State IS NTC_CONNECTING) {
      log.msg("Waiting for connection...");

      if (Self->TimeoutManager) UpdateTimer(Self->TimeoutManager, Self->ConnectTimeout);
      else {
         auto call = make_function_stdc(timeout_manager);
         SubscribeTimer(Self->ConnectTimeout, &call, &Self->TimeoutManager);
      }

      Self->Connecting = TRUE;
   }
   else if (State IS NTC_CONNECTED) {
      // The GET request has been pre-written to the socket on its creation, so we don't need to do anything further
      // here.

      log.msg("Connection confirmed.");
      if (Self->TimeoutManager) { UpdateTimer(Self->TimeoutManager, 0); Self->TimeoutManager = 0; }
      Self->Connecting = FALSE;
   }
   else if (State IS NTC_DISCONNECTED) {
      // Socket disconnected.  The HTTP state must change to either COMPLETED (completed naturally) or TERMINATED
      // (abnormal termination) to correctly inform the user as to what has happened.

      log.msg("Disconnected from socket while in state %s.", clHTTPCurrentState[Self->CurrentState].Name);

      if (Self->TimeoutManager) { UpdateTimer(Self->TimeoutManager, 0); Self->TimeoutManager = 0; }

      if (Self->Connecting) {
         Self->Connecting = FALSE;

         SET_ERROR(Self, Socket->Error);
         log.branch("Deactivating (connect failure message received).");
         SetField(Self, FID_CurrentState, HGS_TERMINATED);
         return;
      }
      else Self->Connecting = FALSE;

      if (Self->CurrentState >= HGS_COMPLETED) {
         return;
      }
      else if (Self->CurrentState IS HGS_READING_HEADER) {
         SET_ERROR(Self, Socket->Error ? Socket->Error : ERR_Disconnected);
         log.trace("Received broken header as follows:\n%s", Self->Response);
         SetField(Self, FID_CurrentState, HGS_TERMINATED);
      }
      else if (Self->CurrentState IS HGS_SEND_COMPLETE) {
         // Disconnection on completion of sending data should be no big deal
         SET_ERROR(Self, Socket->Error ? Socket->Error : ERR_Okay);
         Self->set(FID_CurrentState, HGS_COMPLETED);
      }
      else if (Self->CurrentState IS HGS_SENDING_CONTENT) {
         SET_ERROR(Self, Socket->Error ? Socket->Error : ERR_Disconnected);

         // If the socket is not active, then the disconnection is a result of destroying the object (e.g. due to a redirect).

         log.branch("State changing to TERMINATED due to disconnection.");
         Self->set(FID_CurrentState, HGS_TERMINATED);
      }
      else if (Self->CurrentState IS HGS_READING_CONTENT) {
         LONG len;

         // Unread data can remain on the socket following disconnection, so try to read anything that's been left.

         if (Self->Chunked) {
            log.traceWarning("Support code required to read chunked data following a disconnected socket.");
         }
         else if ((Self->ContentLength IS -1) or (Self->Index < Self->ContentLength)) {
            UBYTE *buffer;

            if (!AllocMemory(BUFFER_READ_SIZE, MEM_DATA|MEM_NO_CLEAR, &buffer, NULL)) {
               while (1) {
                  len = sizeof(buffer);
                  if (Self->ContentLength != -1) {
                     if (len > Self->ContentLength - Self->Index) len = Self->ContentLength - Self->Index;
                  }

                  if ((Self->Error = acRead(Socket, buffer, len, &len))) {
                     log.warning("Read() returned error: %s", GetErrorMsg(Self->Error));
                  }

                  if (!len) { // No more incoming data
                     if (Self->Flags & HTF_DEBUG_SOCKET) {
                        log.msg("Received %d bytes of content in this content reading session.", len);
                     }
                     break;
                  }

                  process_data(Self, buffer, len);
                  if (check_incoming_end(Self) IS ERR_True) break;
               }

               FreeResource(buffer);
            }
         }

         if (Self->ContentLength IS -1) {
            if (Socket->Error IS ERR_Okay) {
               log.msg("Orderly shutdown while streaming data.");
               Self->set(FID_CurrentState, HGS_COMPLETED);
            }
            else {
               SET_ERROR(Self, Socket->Error);
               SetField(Self, FID_CurrentState, HGS_TERMINATED);
            }
         }
         else if (Self->Index < Self->ContentLength) {
            log.warning("Disconnected before all content was downloaded (" PF64() " of " PF64() ")", Self->Index, Self->ContentLength);
            SET_ERROR(Self, Socket->Error ? Socket->Error : ERR_Disconnected);
            SetField(Self, FID_CurrentState, HGS_TERMINATED);
         }
         else {
            log.trace("Orderly shutdown, received " PF64() " of the expected " PF64() " bytes.", Self->Index, Self->ContentLength);
            SetField(Self, FID_CurrentState, HGS_COMPLETED);
         }
      }
      else if (Self->CurrentState IS HGS_AUTHENTICATING) {
         if (Self->DialogWindow) {
            // The HTTP socket was closed because the user is taking too long
            // to authenticate with the dialog window.  We will close the socket
            // and create a new one once the user responds to the dialog.

            Self->Socket->set(FID_Feedback, (APTR)NULL);
            acFree(Socket);
            Self->Socket = NULL;
            Self->SecurePath = TRUE;
            return;
         }

         SetField(Self, FID_CurrentState, HGS_TERMINATED);
      }
   }
   else if (Self->CurrentState >= HGS_COMPLETED) {
      // If the state is set to HGS_COMPLETED or HGS_TERMINATED, our code should have returned ERR_Terminate to switch
      // off the socket.  This section is entered if we forgot to do that.

      log.warning("Warning - socket channel was not closed correctly (didn't return ERR_Terminate).");
   }
}

/*****************************************************************************
** Callback: NetSocket.Outgoing
*/

static ERROR socket_outgoing(objNetSocket *Socket)
{
   parasol::Log log("http_outgoing");

   #define CHUNK_LENGTH_OFFSET 16
   #define CHUNK_TAIL 2 // CRLF

   auto Self = (extHTTP *)Socket->UserData;
   if (Self->ClassID != ID_HTTP) return log.warning(ERR_SystemCorrupt);

   log.traceBranch("Socket: %p, Object: %d, State: %d", Socket, CurrentContext()->UID, Self->CurrentState);

   LONG total_out = 0;

   if (!Self->Buffer) {
      if (Self->BufferSize < BUFFER_WRITE_SIZE) Self->BufferSize = BUFFER_WRITE_SIZE;
      if (Self->BufferSize > 0xffff) Self->BufferSize = 0xffff;

      if (AllocMemory(Self->BufferSize, MEM_DATA|MEM_NO_CLEAR, &Self->Buffer, NULL)) {
         return ERR_AllocMemory;
      }
   }

   ERROR error = ERR_Okay;
redo_upload:
   Self->WriteBuffer = (UBYTE *)Self->Buffer;
   Self->WriteSize   = Self->BufferSize;
   if (Self->Chunked) {
      Self->WriteBuffer += CHUNK_LENGTH_OFFSET;
      Self->WriteSize   -= CHUNK_LENGTH_OFFSET + CHUNK_TAIL;
   }

   if (Self->CurrentState != HGS_SENDING_CONTENT) {
      Self->set(FID_CurrentState, HGS_SENDING_CONTENT);
   }

   LONG len = 0;
   if (Self->Outgoing.Type != CALL_NONE) {
      if (Self->Outgoing.Type IS CALL_STDC) {
         auto routine = (ERROR (*)(extHTTP *, APTR, LONG, LONG *))Self->Outgoing.StdC.Routine;
         error = routine(Self, Self->WriteBuffer, Self->WriteSize, &len);
      }
      else if (Self->Outgoing.Type IS CALL_SCRIPT) {
         // For a script to write to the buffer, it needs to make a call to the Write() action.
         OBJECTPTR script;
         if ((script = Self->Outgoing.Script.Script)) {
            const ScriptArg args[] = {
               { "HTTP",       FD_OBJECTPTR, { .Address = Self } },
               { "BufferSize", FD_LONG,      { .Long = Self->WriteSize } }
            };
            if (scCallback(script, Self->Outgoing.Script.ProcedureID, args, ARRAYSIZE(args), &error)) error = ERR_Failed;
            if (error > ERR_ExceptionThreshold) {
               log.warning("Procedure " PF64() " failed, aborting HTTP call.", Self->Outgoing.Script.ProcedureID);
            }
            else len = Self->WriteOffset;
         }
      }
      else error = ERR_InvalidValue;

      if (len > Self->WriteSize) { // Sanity check, this should never happen if the client uses valid code.
         log.warning("Returned length exceeds buffer size!  %d > %d", len, Self->WriteSize);
         len = Self->WriteSize;
         error = ERR_BufferOverflow;
      }
      else if (error > ERR_ExceptionThreshold) log.warning("Outgoing callback error: %s", GetErrorMsg(error));
   }
   else if (Self->flInput) {
      if (Self->Flags & HTF_DEBUG) log.msg("Sending content from an Input file.");

      error = acRead(Self->flInput, Self->WriteBuffer, Self->WriteSize, &len);

      if (error) log.warning("Input file read error: %s", GetErrorMsg(error));

      LARGE size;
      Self->flInput->get(FID_Size, &size);

      if ((Self->flInput->Position IS size) or (len IS 0)) {
         log.trace("All file content read (%d bytes) - freeing file.", (LONG)size);
         acFree(Self->flInput);
         Self->flInput = NULL;
         if (!error) error = ERR_Terminate;
      }
   }
   else if (Self->InputObjectID) {
      if (Self->Flags & HTF_DEBUG) log.msg("Sending content from InputObject #%d.", Self->InputObjectID);

      OBJECTPTR object;
      if (!(error = AccessObject(Self->InputObjectID, 100, &object))) {
         error = acRead(object, Self->WriteBuffer, Self->WriteSize, &len);
         ReleaseObject(object);
      }

      if (error) log.warning("Input object read error: %s", GetErrorMsg(error));
   }
   else {
      if (Self->MultipleInput) error = ERR_NoData;
      else error = ERR_Terminate;

      log.warning("Method %d: No input fields are defined for me to send data to the server.", Self->Method);
   }

   if (((!error) or (error IS ERR_Terminate)) and (len)) {
      LONG result, csize;
      ERROR writeerror;

      log.trace("Writing %d bytes (of expected " PF64() ") to socket.  Chunked: %d", len, Self->ContentLength, Self->Chunked);

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

      Self->set(FID_Index, Self->Index + len);

      if (writeerror) {
         log.warning("write_socket() failed: %s", GetErrorMsg(writeerror));
         error = writeerror;
      }

      log.trace("Outgoing index now " PF64() " of " PF64(), Self->Index, Self->ContentLength);
   }
   else log.trace("Finishing (an error occurred (%d), or there is no more content to write to socket).", error);

   if ((error) and (error != ERR_Terminate)) {
      if (error != ERR_TimeOut) {
         Self->set(FID_CurrentState, HGS_TERMINATED);
         SET_ERROR(Self, error);
         return ERR_Terminate;
      }
      // ERR_TimeOut: The upload process may continue
   }
   else {
      // Check for multiple input files

      if ((Self->MultipleInput) and (!Self->flInput)) {
         /*if (Self->Flags & HTF_DEBUG)*/ log.msg("Sequential input stream has uploaded " PF64() "/" PF64() " bytes.", Self->Index, Self->ContentLength);

         // Open the next file

         if (!parse_file(Self, (STRING)Self->Buffer, Self->BufferSize)) {
            if ((Self->flInput = objFile::create::integral(fl::Path((CSTRING)Self->Buffer), fl::Flags(FL_READ)))) {
               if (total_out < Self->BufferSize) goto redo_upload; // Upload as much as possible in each pass
               else goto continue_upload;
            }
         }
      }

      // Check if the upload is complete - either Index >= ContentLength or ERR_Terminate has been given as the return code.
      //
      // Note: On completion of an upload, the HTTP server will normally send back a message to confirm completion of
      // the upload, therefore the state is not changed to HGS_COMPLETED.
      //
      // In the case where the server does not respond to completion of the upload, the timeout would eventually take care of it.

      if (((Self->ContentLength > 0) and (Self->Index >= Self->ContentLength)) or (error IS ERR_Terminate)) {
         LONG result;

         if (Self->Chunked) write_socket(Self, (UBYTE *)"0\r\n\r\n", 5, &result);

         if (Self->Flags & HTF_DEBUG) log.msg("Transfer complete - sent " PF64() " bytes.", Self->TotalSent);
         Self->set(FID_CurrentState, HGS_SEND_COMPLETE);
         return ERR_Terminate;
      }
      else {
         if (Self->Flags & HTF_DEBUG) log.msg("Sent " PF64() " bytes of " PF64(), Self->Index, Self->ContentLength);
      }
   }

   // Data timeout when uploading is high due to content buffering
continue_upload:
   Self->LastReceipt = PreciseTime();

   DOUBLE time_limit = (Self->DataTimeout > 30) ? Self->DataTimeout : 30;

   if (Self->TimeoutManager) UpdateTimer(Self->TimeoutManager, time_limit);
   else {
      auto call = make_function_stdc(timeout_manager);
      SubscribeTimer(time_limit, &call, &Self->TimeoutManager);
   }

   Self->WriteBuffer = NULL;
   Self->WriteSize = 0;

   if (Self->Error) return ERR_Terminate;
   return ERR_Okay;
}

/*****************************************************************************
** Callback: NetSocket.Incoming
*/

static ERROR socket_incoming(objNetSocket *Socket)
{
   parasol::Log log("http_incoming");
   LONG len;
   auto Self = (extHTTP *)Socket->UserData;

   log.msg("Context: %d", CurrentContext()->UID);

   if (Self->ClassID != ID_HTTP) return log.warning(ERR_SystemCorrupt);

   if (Self->CurrentState >= HGS_COMPLETED) {
      // Erroneous data received from server while we are in a completion/resting state.  Returning a terminate message
      // will cause the socket object to close the connection to the server so that we stop receiving erroneous data.

      log.warning("Unexpected data incoming from server - terminating socket.");
      return ERR_Terminate;
   }

   if (Self->CurrentState IS HGS_SENDING_CONTENT) {
      if (Self->ContentLength IS -1) {
         log.warning("Incoming data while streaming content - " PF64() " bytes already written.", Self->Index);
      }
      else if (Self->Index < Self->ContentLength) {
         log.warning("Incoming data while sending content - only " PF64() "/" PF64() " bytes written!", Self->Index, Self->ContentLength);
      }
   }

   if ((Self->CurrentState IS HGS_SENDING_CONTENT) or (Self->CurrentState IS HGS_SEND_COMPLETE)) {
      log.trace("Switching state from sending content to reading header.");
      Self->set(FID_CurrentState, HGS_READING_HEADER);
      Self->Index = 0;
   }

   if ((Self->CurrentState IS HGS_READING_HEADER) or (Self->CurrentState IS HGS_AUTHENTICATING)) {
      log.trace("HTTP received data, reading header.");

      while (1) {
         if (!Self->Response) {
            Self->ResponseSize = 256;
            if (AllocMemory(Self->ResponseSize + 1, MEM_STRING|MEM_NO_CLEAR, &Self->Response, NULL) != ERR_Okay) {
               SET_ERROR(Self, log.warning(ERR_AllocMemory));
               return ERR_Terminate;
            }
         }

         if (Self->ResponseIndex >= Self->ResponseSize) {
            Self->ResponseSize += 256;
            if (ReallocMemory(Self->Response, Self->ResponseSize + 1, &Self->Response, NULL) != ERR_Okay) {
               SET_ERROR(Self, log.warning(ERR_ReallocMemory));
               return ERR_Terminate;
            }
         }

         Self->Error = acRead(Socket, Self->Response+Self->ResponseIndex, Self->ResponseSize - Self->ResponseIndex, &len);

         if (Self->Error) {
            log.warning(Self->Error);
            return ERR_Terminate;
         }

         if (len < 1) break; // No more incoming data
         Self->ResponseIndex += len;
         Self->Response[Self->ResponseIndex] = 0;

         // Advance search for terminated double CRLF

         for (; Self->SearchIndex+4 <= Self->ResponseIndex; Self->SearchIndex++) {
            if (!StrCompare(Self->Response + Self->SearchIndex, "\r\n\r\n", 4, STR_MATCH_CASE)) {
               Self->Response[Self->SearchIndex] = 0; // Terminate the header at the CRLF point

               if (parse_response(Self, Self->Response) != ERR_Okay) {
                  SET_ERROR(Self, log.warning(ERR_InvalidHTTPResponse));
                  return ERR_Terminate;
               }

               if (Self->Tunneling) {
                  if (Self->Status IS 200) {
                     // Proxy tunnel established.  Convert the socket to an SSL connection, then send the HTTP command.

                     if (!netSetSSL(Socket, NSL_CONNECT, TRUE, TAGEND)) {
                        return acActivate(Self);
                     }
                     else {
                        SET_ERROR(Self, log.warning(ERR_ConnectionAborted));
                        return ERR_Terminate;
                     }
                  }
                  else {
                     SET_ERROR(Self, log.warning(ERR_ProxySSLTunnel));
                     return ERR_Terminate;
                  }
               }

               if ((Self->CurrentState IS HGS_AUTHENTICATING) and (Self->Status != 401)) {
                  log.msg("Authentication successful, reactivating...");
                  Self->SecurePath = FALSE;
                  Self->set(FID_CurrentState, HGS_AUTHENTICATED);
                  DelayMsg(AC_Activate, Self->UID, NULL);
                  return ERR_Okay;
               }

               if (Self->Status IS HTS_MOVED_PERMANENTLY) {
                  if (Self->Flags & HTF_MOVED) {
                     // Chaining of MovedPermanently messages is disallowed (could cause circular referencing).

                     log.warning("Sequential MovedPermanently messages are not supported.");
                  }
                  else {
                     char buffer[512];
                     if (!acGetVar(Self, "Location", buffer, sizeof(buffer))) {
                        log.msg("MovedPermanently to %s", buffer);
                        if (!StrCompare("http:", buffer, 5, 0)) Self->set(FID_Location, buffer);
                        else Self->set(FID_Path, buffer);
                        acActivate(Self); // Try again
                        Self->Flags |= HTF_MOVED;
                        return ERR_Okay;
                     }
                     else {
                        Self->Flags |= HTF_MOVED;
                        log.warning("Invalid MovedPermanently HTTP response received (no location specified).");
                     }
                  }
               }
               else if (Self->Status IS HTS_TEMP_REDIRECT) {
                  if (Self->Flags & HTF_REDIRECTED) {
                     // Chaining of TempRedirect messages is disallowed (could cause circular referencing).

                     log.warning("Sequential TempRedirect messages are not supported.");
                  }
                  else Self->Flags |= HTF_REDIRECTED;
               }

               if ((!Self->ContentLength) or (Self->ContentLength < -1)) {
                  log.msg("Reponse header received, no content imminent.");
                  Self->set(FID_CurrentState, HGS_COMPLETED);
                  return ERR_Terminate;
               }

               log.msg("Complete response header has been received.  Incoming Content: " PF64(), Self->ContentLength);

               if (Self->CurrentState != HGS_READING_CONTENT) {
                  Self->set(FID_CurrentState, HGS_READING_CONTENT);
               }

               Self->AuthDigest = FALSE;
               if ((Self->Status IS 401) and (Self->AuthRetries < MAX_AUTH_RETRIES)) {
                  Self->AuthRetries++;

                  if (Self->Password) {
                     // Destroy the current password if it was entered by the user (therefore is invalid) or if it was
                     // preset and second authorisation attempt failed (in the case of preset passwords, two
                     // authorisation attempts are required in order to receive the 401 from the server first).

                     if ((Self->AuthPreset IS FALSE) or (Self->AuthRetries >= 2)) {
                        for (LONG i=0; Self->Password[i]; i++) Self->Password[i] = 0xff;
                        FreeResource(Self->Password);
                        Self->Password = NULL;
                     }
                  }

                  std::string& authenticate = Self->Args[0]["WWW-Authenticate"];
                  if (!authenticate.empty()) {
                     CSTRING auth = authenticate.c_str();
                     if (!StrCompare("Digest", auth, 6, 0)) {
                        log.trace("Digest authentication mode.");

                        if (Self->Realm)      { FreeResource(Self->Realm);      Self->Realm = NULL; }
                        if (Self->AuthNonce)  { FreeResource(Self->AuthNonce);  Self->AuthNonce = NULL; }
                        if (Self->AuthOpaque) { FreeResource(Self->AuthOpaque); Self->AuthOpaque = NULL; }

                        Self->AuthAlgorithm[0] = 0;
                        Self->AuthDigest = TRUE;

                        LONG i = 6;
                        while ((auth[i]) and (auth[i] <= 0x20)) i++;

                        while (auth[i]) {
                           if (!StrCompare("realm=", auth+i, 0, 0))       i += extract_value(auth+i, &Self->Realm);
                           else if (!StrCompare("nonce=", auth+i, 0, 0))  i += extract_value(auth+i, &Self->AuthNonce);
                           else if (!StrCompare("opaque=", auth+i, 0, 0)) i += extract_value(auth+i, &Self->AuthOpaque);
                           else if (!StrCompare("algorithm=", auth+i, 0, 0)) {
                              STRING value;
                              i += extract_value(auth+i, &value);
                              StrCopy(value, (STRING)Self->AuthAlgorithm, sizeof(Self->AuthAlgorithm));
                              FreeResource(value);
                           }
                           else if (!StrCompare("qop=", auth+i, 0, 0)) {
                              STRING value;
                              i += extract_value(auth+i, &value);
                              if (StrSearch("auth-int", value, 0) >= 0) {
                                 StrCopy("auth-int", (STRING)Self->AuthQOP, sizeof(Self->AuthQOP));
                              }
                              else StrCopy("auth", (STRING)Self->AuthQOP, sizeof(Self->AuthQOP));
                              FreeResource(value);
                           }
                           else {
                              while (auth[i] > 0x20) {
                                 if (auth[i] IS '=') {
                                    i++;
                                    while ((auth[i]) and (auth[i] <= 0x20)) i++;
                                    if (auth[i] IS '"') {
                                       i++;
                                       while ((auth[i]) and (auth[i] != '"')) i++;
                                       if (auth[i] IS '"') i++;
                                    }
                                    else i++;
                                 }
                                 else i++;
                              }

                              while (auth[i] > 0x20) i++;
                              while ((auth[i]) and (auth[i] <= 0x20)) i++;
                           }
                        }
                     }
                     else log.trace("Basic authentication mode.");
                  }
                  else log.msg("Authenticate method unknown.");

                  Self->set(FID_CurrentState, HGS_AUTHENTICATING);

                  if ((!Self->Password) and (!(Self->Flags & HTF_NO_DIALOG))) {
                     // Pop up a dialog requesting the user to authorise himself with the http server.  The user will
                     // need to respond to the dialog before we can repost the HTTP request.

                     ERROR error;
                     STRING scriptfile;
                     if (!AllocMemory(glAuthScriptLength+1, MEM_STRING|MEM_NO_CLEAR, &scriptfile, NULL)) {
                        CopyMemory(glAuthScript, scriptfile, glAuthScriptLength);
                        scriptfile[glAuthScriptLength] = 0;

                        objScript::create script = { fl::String(scriptfile) };
                        if (script.ok()) {
                           AdjustLogLevel(1);
                           error = script->activate();
                           AdjustLogLevel(-1);
                        }
                        else error = ERR_CreateObject;

                        FreeResource(scriptfile);
                     }
                     else error = ERR_AllocMemory;
                  }
                  else ActionMsg(AC_Activate, Self->UID, NULL);

                  return ERR_Okay;
               }

               len = Self->ResponseIndex - (Self->SearchIndex + 4);

               if (Self->Chunked) {
                  log.trace("Content to be received in chunks.");
                  Self->ChunkSize  = 4096;
                  Self->ChunkIndex = 0; // Number of bytes processed for the current chunk
                  Self->ChunkLen   = 0;  // Length of the first chunk is unknown at this stage
                  Self->ChunkBuffered = len;
                  if (len > Self->ChunkSize) Self->ChunkSize = len;
                  if (!AllocMemory(Self->ChunkSize, MEM_DATA|MEM_NO_CLEAR, &Self->Chunk, NULL)) {
                     if (len > 0) CopyMemory(Self->Response + Self->SearchIndex + 4, Self->Chunk, len);
                  }
                  else {
                     SET_ERROR(Self, log.warning(ERR_AllocMemory));
                     return ERR_Terminate;
                  }

                  Self->SearchIndex = 0;
               }
               else {
                  log.trace(PF64() " bytes of content is incoming.  Bytes Buffered: %d, Index: " PF64(), Self->ContentLength, len, Self->Index);

                  if (len > 0) process_data(Self, Self->Response + Self->SearchIndex + 4, len);
               }

               check_incoming_end(Self);

               FreeResource(Self->Response);
               Self->Response = NULL;

               // Note that status check comes after processing of content, as it is legal for content to be attached
               // with bad status codes (e.g. SOAP does this).

               if ((Self->Status < 200) or (Self->Status >= 300)) {
                  if (Self->CurrentState != HGS_READING_CONTENT) {
                     if (Self->Status IS 401) log.warning("Exhausted maximum number of retries.");
                     else log.warning("Status code %d != 2xx", Self->Status);

                     SET_ERROR(Self, ERR_Failed);
                     return ERR_Terminate;
                  }
                  else log.warning("Status code %d != 2xx.  Receiving content...", Self->Status);
               }

               return ERR_Okay;
            }
         }
      }
   }
   else if (Self->CurrentState IS HGS_READING_CONTENT) {
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
            parasol::Log log("http_incoming");
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

               if (Self->Error IS ERR_Disconnected) {
                  log.msg("Received all chunked content (disconnected by peer).");
                  Self->set(FID_CurrentState, HGS_COMPLETED);
                  return ERR_Terminate;
               }
               else if (Self->Error) {
                  log.warning("Read() returned error %d whilst reading content.", Self->Error);
                  Self->set(FID_CurrentState, HGS_COMPLETED);
                  return ERR_Terminate;
               }
               else if ((!len) and (Self->ChunkIndex >= Self->ChunkBuffered)) {
                  log.msg("Nothing left to read.");
                  return ERR_Okay;
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
                              Self->set(FID_CurrentState, HGS_COMPLETED);
                              return ERR_Terminate;
                           }
                           else {
                              // We have reached the terminating line (CRLF on an empty line)
                              log.msg("Received all chunked content.");
                              Self->set(FID_CurrentState, HGS_COMPLETED);
                              return ERR_Terminate;
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
         UBYTE *buffer;

         // Maximum number of times that this subroutine can loop (on a fast network we could otherwise download indefinitely).
         // A limit of 64K per read session is acceptable with a time limit of 1/200 frames.

         if (!AllocMemory(BUFFER_READ_SIZE, MEM_DATA|MEM_NO_CLEAR, &buffer, NULL)) {
            LONG looplimit = (64 * 1024) / BUFFER_READ_SIZE;
            LARGE timelimit = PreciseTime() + 5000000LL;

            while (1) {
               len = BUFFER_READ_SIZE;
               if (Self->ContentLength != -1) {
                  if (len > Self->ContentLength - Self->Index) len = Self->ContentLength - Self->Index;
               }

               if ((Self->Error = acRead(Socket, buffer, len, &len))) {
                  if ((Self->Error IS ERR_Disconnected) and (Self->ContentLength IS -1)) {
                     log.trace("Received all streamed content (disconnected by peer).");
                     Self->set(FID_CurrentState, HGS_COMPLETED);
                     FreeResource(buffer);
                     return ERR_Terminate;
                  }
                  else {
                     FreeResource(buffer);
                     log.warning("Read() returned error %d whilst reading content.", Self->Error);
                     return ERR_Terminate;
                  }
               }

               if (!len) break; // No more incoming data right now

               process_data(Self, buffer, len);
               if (check_incoming_end(Self) IS ERR_True) {
                  FreeResource(buffer);
                  return ERR_Terminate;
               }

               if (--looplimit <= 0) break; // Looped many times, need to break
               if (PreciseTime() > timelimit) break; // Time limit reached
            }

            FreeResource(buffer);
         }
      }

      Self->LastReceipt = PreciseTime();

      if (Self->TimeoutManager) UpdateTimer(Self->TimeoutManager, Self->DataTimeout);
      else {
         auto call = make_function_stdc(timeout_manager);
         SubscribeTimer(Self->DataTimeout, &call, &Self->TimeoutManager);
      }

      if (Self->Error) return ERR_Terminate;
   }
   else {
      char buffer[512];
      // Indeterminate data received from HTTP server

      if ((!acRead(Socket, buffer, sizeof(buffer)-1, &len)) and (len > 0)) {
         buffer[len] = 0;
         log.warning("WARNING: Received data whilst in state %d.", Self->CurrentState);
         log.warning("Content (%d bytes) Follows:\n%.80s", len, buffer);
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static CSTRING adv_crlf(CSTRING String)
{
   while (*String) {
      if ((*String IS '\r') and (String[1] IS '\n')) {
         String += 2;
         return String;
      }
      String++;
   }
   return String;
}

//****************************************************************************

static ERROR parse_response(extHTTP *Self, CSTRING Buffer)
{
   parasol::Log log;

   if (Self->Args) Self->Args->clear();
   else {
      Self->Args = new (std::nothrow) std::unordered_map<std::string, std::string>;
      if (!Self->Args) return log.warning(ERR_Memory);
   }

   if (Self->Flags & HTF_DEBUG) log.msg("HTTP RESPONSE HEADER\n%s", Buffer);

   // First line: HTTP/1.1 200 OK

   if (StrCompare("HTTP/", Buffer, 5, 0) != ERR_Okay) {
      log.warning("Invalid response header, missing 'HTTP/'");
      return ERR_InvalidHTTPResponse;
   }

   CSTRING str = Buffer;
   //LONG majorv = StrToInt(str); // Currently unused
   while ((*str) and (*str != '.')) str++;
   if (*str IS '.') str++;
   else return ERR_InvalidHTTPResponse;

   //LONG minorv = StrToInt(str); // Currently unused
   while (*str > 0x20) str++;
   while ((*str) and (*str <= 0x20)) str++;

   Self->Status = StrToInt(str);

   str = adv_crlf(str);

   if (Self->ProxyServer) Self->ContentLength = -1; // Some proxy servers (Squid) strip out information like 'transfer-encoding' yet pass all the requested content anyway :-/
   else Self->ContentLength = 0;
   Self->Chunked = FALSE;

   // Parse response fields

   log.msg("HTTP response header received, status code %d", Self->Status);

   char field[80], value[300];
   while (*str) {
      LONG i;
      for (i=0; (*str) and (*str != ':') and (*str != '\r') and (*str != '\n'); i++) {
         field[i] = *str++;
      }
      field[i] = 0;

      if (*str IS ':') {
         str++;
         while ((*str) and (*str <= 0x20)) str++;

         for (i=0; (*str) and (*str != '\r') and (*str != '\n'); i++) {
            value[i] = *str++;
         }
         value[i] = 0;

         if (!StrMatch(field, "Content-Length")) {
            Self->ContentLength = StrToInt(value);
         }
         else if (!StrMatch(field, "Transfer-Encoding")) {
            if (!StrMatch(value, "chunked")) {
               if (!(Self->Flags & HTF_RAW)) Self->Chunked = TRUE;
               Self->ContentLength = -1;
            }
         }

         Self->Args[0][field] = value;
      }
      else str = adv_crlf(str);
   }

   return ERR_Okay;
}

//****************************************************************************
// Sends some data specified in the arguments to the listener

static ERROR process_data(extHTTP *Self, APTR Buffer, LONG Length)
{
   parasol::Log log(__FUNCTION__);

   log.trace("Buffer: %p, Length: %d", Buffer, Length);

   if (!Length) return ERR_Okay;

   Self->set(FID_Index, (LARGE)Self->Index + (LARGE)Length); // Use Set() so that field subscribers can track progress with field monitoring

   if ((!Self->flOutput) and (Self->OutputFile)) {
      LONG flags, type;

      if (Self->Flags & HTF_RESUME) {
         if ((!AnalysePath(Self->OutputFile, &type)) and (type IS LOC_FILE)) {
            flags = 0;
         }
         else flags = FL_NEW;
      }
      else flags = FL_NEW;

      if ((Self->flOutput = objFile::create::integral(fl::Path(Self->OutputFile), fl::Flags(flags|FL_WRITE)))) {
         if (Self->Flags & HTF_RESUME) {
            acSeekEnd(Self->flOutput, 0);
            Self->set(FID_Index, 0);
         }
      }
      else SET_ERROR(Self, ERR_CreateFile);
   }

   if (Self->flOutput) Self->flOutput->write(Buffer, Length, NULL);

   if (Self->Flags & HTF_RECV_BUFFER) {
      if (!Self->RecvBuffer) {
         Self->RecvSize = Length;
         if (!AllocMemory(Length+1, MEM_DATA|MEM_NO_CLEAR, &Self->RecvBuffer, NULL)) {
            CopyMemory(Buffer, Self->RecvBuffer, Self->RecvSize);
            ((STRING)Self->RecvBuffer)[Self->RecvSize] = 0;
         }
         else SET_ERROR(Self, ERR_AllocMemory);
      }
      else if (!ReallocMemory(Self->RecvBuffer, Self->RecvSize + Length + 1, &Self->RecvBuffer, NULL)) {
         CopyMemory(Buffer, Self->RecvBuffer + Self->RecvSize, Length);
         Self->RecvSize += Length;
         ((STRING)Self->RecvBuffer)[Self->RecvSize] = 0;
      }
      else SET_ERROR(Self, ERR_ReallocMemory);
   }

   if (Self->Incoming.Type != CALL_NONE) {
      log.trace("Incoming callback is set.");

      ERROR error;
      if (Self->Incoming.Type IS CALL_STDC) {
         auto routine = (ERROR (*)(extHTTP *, APTR, LONG))Self->Incoming.StdC.Routine;
         error = routine(Self, Buffer, Length);
      }
      else if (Self->Incoming.Type IS CALL_SCRIPT) {
         // For speed, the client will receive a direct pointer to the buffer memory via the 'mem' interface.

         log.trace("Calling script procedure " PF64(), Self->Incoming.Script.ProcedureID);

         OBJECTPTR script;
         if ((script = Self->Incoming.Script.Script)) {
            const ScriptArg args[] = {
               { "HTTP",       FD_OBJECTPTR, { .Address = Self } },
               { "Buffer",     FD_PTRBUFFER, { .Address = Buffer } },
               { "BufferSize", FD_LONG|FD_BUFSIZE, { .Long = Length } }
            };
            if (scCallback(script, Self->Incoming.Script.ProcedureID, args, ARRAYSIZE(args), &error)) error = ERR_Terminate;
         }
         else error = ERR_Terminate;
      }
      else error = ERR_InvalidValue;

      if (error) SET_ERROR(Self, error);

      if (Self->Error IS ERR_Terminate) {
         parasol::Log log(__FUNCTION__);
         log.branch("State changing to HGS_TERMINATED (terminate message received).");
         Self->set(FID_CurrentState, HGS_TERMINATED);
      }
   }

   if (Self->OutputObjectID) {
      if (Self->ObjectMode IS HOM_DATA_FEED) {
         struct acDataFeed data = {
            .ObjectID = Self->UID,
            .DataType = Self->Datatype,
            .Buffer   = Buffer,
            .Size     = Length
         };
         ActionMsg(AC_DataFeed, Self->OutputObjectID, &data);
      }
      else if (Self->ObjectMode IS HOM_READ_WRITE) {
         acWrite(Self->OutputObjectID, Buffer, Length);
      }
   }

   return Self->Error;
}

//****************************************************************************

static LONG extract_value(CSTRING String, STRING *Result)
{
   LONG i;

   CSTRING start = String;
   STRING value = NULL;

   while ((*String) and (*String != '=') and (*String != ',')) String++;
   if (*String IS '=') {
      String++;
      if (*String IS '"') {
         String++;
         for (i=0; (String[i]) and (String[i] != '"'); i++);

         if (!AllocMemory(i+1, MEM_STRING|MEM_NO_CLEAR, &value, NULL)) {
            CopyMemory(String, value, i);
            value[i] = 0;
         }
         String += i + 1; // Skip "
         while ((*String) and (*String != ',')) String++;
         if (*String IS ',') String++;
         while ((*String) and (*String <= 0x20)) String++;
      }
      else {
         for (i=0; (String[i]) and (String[i] != ','); i++);

         if (!AllocMemory(i+1, MEM_STRING|MEM_NO_CLEAR, &value, NULL)) {
            CopyMemory(String, value, i);
            value[i] = 0;
         }
         String += i;
         if (*String IS ',') String++;
         while ((*String) and (*String <= 0x20)) String++;
      }
   }

   *Result = value;
   return String - start;
}

//****************************************************************************

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

//****************************************************************************
// Calculate H(A1) as per spec

static void digest_calc_ha1(extHTTP *Self, HASHHEX SessionKey)
{
   MD5_CTX md5;
   HASH HA1;

   MD5Init(&md5);

   if (Self->Username) MD5Update(&md5, (UBYTE *)Self->Username, StrLength(Self->Username));

   MD5Update(&md5, (UBYTE *)":", 1);

   if (Self->Realm) MD5Update(&md5, (UBYTE *)Self->Realm, StrLength(Self->Realm));

   MD5Update(&md5, (UBYTE *)":", 1);

   if (Self->Password) MD5Update(&md5, (UBYTE *)Self->Password, StrLength(Self->Password));

   MD5Final((UBYTE *)HA1, &md5);

   if (!StrMatch((CSTRING)Self->AuthAlgorithm, "md5-sess")) {
      MD5Init(&md5);
      MD5Update(&md5, (UBYTE *)HA1, HASHLEN);
      MD5Update(&md5, (UBYTE *)":", 1);
      if (Self->AuthNonce) MD5Update(&md5, (UBYTE *)Self->AuthNonce, StrLength((CSTRING)Self->AuthNonce));
      MD5Update(&md5, (UBYTE *)":", 1);
      MD5Update(&md5, (UBYTE *)Self->AuthCNonce, StrLength((CSTRING)Self->AuthCNonce));
      MD5Final((UBYTE *)HA1, &md5);
   }

   writehex(HA1, SessionKey);
}

//****************************************************************************
// Calculate request-digest/response-digest as per HTTP Digest spec

static void digest_calc_response(extHTTP *Self, CSTRING Request, CSTRING NonceCount, HASHHEX HA1, HASHHEX HEntity, HASHHEX Response)
{
   parasol::Log log;
   MD5_CTX md5;
   HASH HA2;
   HASH RespHash;
   HASHHEX HA2Hex;
   LONG i;

   // Calculate H(A2)

   MD5Init(&md5);

   for (i=0; Request[i] > 0x20; i++);
   MD5Update(&md5, (UBYTE *)Request, i); // Compute MD5 from the name of the HTTP method that we are calling
   Request += i;
   while ((*Request) and (*Request <= 0x20)) Request++;

   MD5Update(&md5, (UBYTE *)":", 1);

   for (i=0; Request[i] > 0x20; i++);
   MD5Update(&md5, (UBYTE *)Request, i); // Compute MD5 from the path of the HTTP method that we are calling

   if (!StrMatch((CSTRING)Self->AuthQOP, "auth-int")) {
      MD5Update(&md5, (UBYTE *)":", 1);
      MD5Update(&md5, (UBYTE *)HEntity, HASHHEXLEN);
   }

   MD5Final((UBYTE *)HA2, &md5);
   writehex(HA2, HA2Hex);

   // Calculate response:  HA1Hex:Nonce:NonceCount:CNonce:auth:HA2Hex

   MD5Init(&md5);
   MD5Update(&md5, (UBYTE *)HA1, HASHHEXLEN);
   MD5Update(&md5, (UBYTE *)":", 1);
   MD5Update(&md5, (UBYTE *)Self->AuthNonce, StrLength((CSTRING)Self->AuthNonce));
   MD5Update(&md5, (UBYTE *)":", 1);

   if (Self->AuthQOP[0]) {
      MD5Update(&md5, (UBYTE *)NonceCount, StrLength((CSTRING)NonceCount));
      MD5Update(&md5, (UBYTE *)":", 1);
      MD5Update(&md5, (UBYTE *)Self->AuthCNonce, StrLength((CSTRING)Self->AuthCNonce));
      MD5Update(&md5, (UBYTE *)":", 1);
      MD5Update(&md5, (UBYTE *)Self->AuthQOP, StrLength((CSTRING)Self->AuthQOP));
      MD5Update(&md5, (UBYTE *)":", 1);
   }

   MD5Update(&md5, (UBYTE *)HA2Hex, HASHHEXLEN);
   MD5Final((UBYTE *)RespHash, &md5);
   writehex(RespHash, Response);

   log.trace("%s:%s:%s:%s:%s:%s", HA1, Self->AuthNonce, NonceCount, Self->AuthCNonce, Self->AuthQOP, HA2Hex);
}

//****************************************************************************

static ERROR write_socket(extHTTP *Self, APTR Buffer, LONG Length, LONG *Result)
{
   parasol::Log log(__FUNCTION__);

   if (Length > 0) {
      //log.trace("Length: %d", Length);

      if (Self->Flags & HTF_DEBUG_SOCKET) {
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
      return ERR_Okay;
   }
}

/*****************************************************************************
** The timer is used for managing time-outs on connection to and the receipt of data from the http server.  If the
** timer is activated then we close the current socket.  It should be noted that if the content is streamed, then
** it is not unusual for the client to remain unnotified even in the event of a complete transfer.  Because of this,
** the client should check if the content is streamed in the event of a timeout and not necessarily assume failure.
*/

static ERROR timeout_manager(extHTTP *Self, LARGE Elapsed, LARGE CurrentTime)
{
   parasol::Log log(__FUNCTION__);

   log.warning("Timeout detected - disconnecting from server (connect %.2fs, data %.2fs).", Self->ConnectTimeout, Self->DataTimeout);
   Self->TimeoutManager = 0;
   SET_ERROR(Self, ERR_TimeOut);
   Self->set(FID_CurrentState, HGS_TERMINATED);
   return ERR_Terminate;
}

//****************************************************************************
// Returns ERR_True if the transmission is complete and also sets status to HGS_COMPLETED, otherwise ERR_False.

static ERROR check_incoming_end(extHTTP *Self)
{
   parasol::Log log(__FUNCTION__);

   if (Self->CurrentState IS HGS_AUTHENTICATING) return ERR_False;
   if (Self->CurrentState >= HGS_COMPLETED) return ERR_True;

   if ((Self->ContentLength != -1) and (Self->Index >= Self->ContentLength)) {
      log.trace("Transmission over.");
      if (Self->Index > Self->ContentLength) log.warning("Warning: received too much content.");
      Self->set(FID_CurrentState, HGS_COMPLETED);
      return ERR_True;
   }
   else {
      log.trace("Transmission continuing.");
      return ERR_False;
   }
}

//****************************************************************************

static LONG set_http_method(extHTTP *Self, STRING Buffer, LONG Size, CSTRING Method)
{
   if ((Self->ProxyServer) and (!(Self->Flags & HTF_SSL))) {
      // Normal proxy request without SSL tunneling
      return StrFormat(Buffer, Size, "%s %s://%s:%d/%s HTTP/1.1%sHost: %s%sUser-Agent: %s%s",
         Method, (Self->Port IS 443) ? "https" : "http", Self->Host, Self->Port, Self->Path ? Self->Path : (STRING)"", CRLF, Self->Host, CRLF, Self->UserAgent, CRLF);
   }
   else {
      return StrFormat(Buffer, Size, "%s /%s HTTP/1.1%sHost: %s%sUser-Agent: %s%s",
         Method, Self->Path ? Self->Path : (STRING)"", CRLF, Self->Host, CRLF, Self->UserAgent, CRLF);
   }
}

//****************************************************************************

static ERROR parse_file(extHTTP *Self, STRING Buffer, LONG Size)
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

   if (i >= Size-1) return ERR_BufferOverflow;
   if (!i) return ERR_EmptyString;
   return ERR_Okay;
}
