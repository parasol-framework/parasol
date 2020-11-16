/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
HTTP: Provides a complete working implementation of HTTP.

The HTTP class provides a way of interacting with servers that support the HTTP protocol.  Supported HTTP methods
include GET, POST, PUT, DELETE, COPY, MOVE, MKCOL and more.  The following features are included:

<list type="unsorted">
<li>Handling of errors and HTTP status codes.</li>
<li>Monitoring of the server communication process.</li>
<li>Data transfer monitoring.</li>
<li>Sending and receiving in chunks of data.</li>
<li>Background processing of all HTTP instructions.</li>
<li>Data streaming.</li>
<li>User authentication, either automated or with user login dialogs.</li>
</list>

For information on command execution and a technical overview of HTTP processing, please refer to the
#Activate() action.

<header>Sending Content</>

There are a variety of ways to send content to a server when using methods such as PUT and POST.  Content can be sent
from objects by setting the #InputObject field.  To send content from files, set the #InputFile
field.  To send string content, use an #InputFile location that starts with 'string:' followed by the text
based content to send.

<header>Receiving Content</>

There are three possible methods for content download - two of which are relevant to script usage.  This first example
downloads content to a temporary file for further processing:

<pre>
&lt;http src="http://www.parasol.ws/index.html" method="get" outputfile="temp:index.html"/&gt;
</pre>

This example uses data feeds to push the downloaded data to another object in text format:

<pre>
&lt;scintilla name="doc"/&gt;
&lt;http src="http://www.parasol.ws/index.html" method="get" outputobject="[doc]" objectmode="datafeed" datafeed="text"/&gt;
</pre>

Note that the target object needs to support the datatype that you specify, or it will ignore the incoming data.  The
default datatype is RAW (binary format), but the most commonly supported datatype is TEXT.

The third method is to use function callbacks.  This method is supported by C/C++ and the Fluid programming language.
Refer to the #Incoming field for further information on receiving data through callbacks.

<header>Progress Reports</>

To receive progress reports when uploading or downloading data from a server, subscribe to the #Index field,
which indicates the current read/write position within the overall data stream (indicated by the
#ContentLength).  Alternatively, use a timer and read the #Index periodically.

<header>SSL Support (HTTPS)</>

Secure sockets are supported and can be enabled by setting the #Port to 443 prior to connection, or by using
https:// in URI strings.  Methods of communication remain unchanged when using SSL, as encrypted communication is
handled transparently.

-END-

For information about the HTTP protocol, please refer to the official protocol web page:

   http://www.w3.org/Protocols/rfc2616/rfc2616.html

*****************************************************************************/

//#define DEBUG
#define PRV_HTTP

#include <stdio.h>

#include <parasol/main.h>
#include <parasol/modules/http.h>
#include <parasol/modules/widget.h>
#include <parasol/modules/network.h>

#include "md5.c"

#define MAX_AUTH_RETRIES 5
#define CRLF "\r\n"
#define HASHLEN 16
#define HASHHEXLEN 32
typedef char HASH[HASHLEN];
typedef char HASHHEX[HASHHEXLEN+1];

#define BUFFER_READ_SIZE 16384  // Dictates how many bytes are read from the network socket at a time.  Do not make this greater than 64k
#define BUFFER_WRITE_SIZE 16384 // Dictates how many bytes are written to the network socket at a time.  Do not make this greater than 64k

#define SET_ERROR(http, code) { (http)->Error = (code); log.trace("SetHTTPError","Code: %d, Msg: %s", code, GetErrorMsg(code)); }

static ERROR create_http_class(void);

MODULE_COREBASE;
static struct NetworkBase *NetworkBase;
static OBJECTPTR modNetwork = NULL;
static OBJECTPTR clHTTP = NULL;
static objProxy *glProxy = NULL;

extern UBYTE glAuthScript[];
static LONG glAuthScriptLength;

static ERROR HTTP_ActionNotify(objHTTP *, struct acActionNotify *);
static ERROR HTTP_Activate(objHTTP *, APTR);
static ERROR HTTP_Deactivate(objHTTP *, APTR);
static ERROR HTTP_Free(objHTTP *, APTR);
static ERROR HTTP_GetVar(objHTTP *, struct acGetVar *);
static ERROR HTTP_Init(objHTTP *, APTR);
static ERROR HTTP_NewObject(objHTTP *, APTR);
static ERROR HTTP_SetVar(objHTTP *, struct acSetVar *);
static ERROR HTTP_Write(objHTTP *, struct acWrite *);

#include "http_def.c"

//****************************************************************************

static const FieldDef clStatus[] = {
   { "Continue",                 HTS_CONTINUE },
   { "Switching Protocols",      HTS_SWITCH_PROTOCOLS },
   { "Okay",                     HTS_OKAY },
   { "Created",                  HTS_CREATED },
   { "Accepted",                 HTS_ACCEPTED },
   { "Unverified Content",       HTS_UNVERIFIED_CONTENT },
   { "No Content",               HTS_NO_CONTENT },
   { "Reset Content",            HTS_RESET_CONTENT },
   { "Partial Content",          HTS_PARTIAL_CONTENT },
   { "Multiple Choices",         HTS_MULTIPLE_CHOICES },
   { "Moved Permanently",        HTS_MOVED_PERMANENTLY },
   { "Found",                    HTS_FOUND },
   { "See Other",                HTS_SEE_OTHER },
   { "Not Modified",             HTS_NOT_MODIFIED },
   { "Use Proxy",                HTS_USE_PROXY },
   { "Temporary Redirect",       HTS_TEMP_REDIRECT },
   { "Bad Request",              HTS_BAD_REQUEST },
   { "Unauthorised",             HTS_UNAUTHORISED },
   { "Payment Required",         HTS_PAYMENT_REQUIRED },
   { "Forbidden",                HTS_FORBIDDEN },
   { "Not Found",                HTS_NOT_FOUND },
   { "Method Not Allowed",       HTS_METHOD_NOT_ALLOWED },
   { "Not Acceptable",           HTS_NOT_ACCEPTABLE },
   { "Proxy Authentication Required", HTS_PROXY_AUTHENTICATION },
   { "Request Timeout",          HTS_REQUEST_TIMEOUT },
   { "Conflict",                 HTS_CONFLICT },
   { "Gone",                     HTS_GONE },
   { "Length Required",          HTS_LENGTH_REQUIRED },
   { "Precondition Failed",      HTS_PRECONDITION_FAILED },
   { "Request Entity Too Large", HTS_ENTITY_TOO_LARGE },
   { "Request-URI Too Long",     HTS_URI_TOO_LONG },
   { "Unsupported Media Type",   HTS_UNSUPPORTED_MEDIA },
   { "Out of Range",             HTS_OUT_OF_RANGE },
   { "Expectation Failed",       HTS_EXPECTATION_FAILED },
   { "Internal Server Error",    HTS_SERVER_ERROR },
   { "Not Implemented",          HTS_NOT_IMPLEMENTED },
   { "Bad Gateway",              HTS_BAD_GATEWAY },
   { "Service Unavailable",      HTS_SERVICE_UNAVAILABLE },
   { "Gateway Timeout",          HTS_GATEWAY_TIMEOUT },
   { "HTTP Version Unsupported", HTS_VERSION_UNSUPPORTED },
   { NULL, 0 }
};

//****************************************************************************

static CSTRING adv_crlf(CSTRING);
static ERROR check_incoming_end(objHTTP *);
static ERROR parse_file(objHTTP *, STRING Buffer, LONG Size);
static ERROR parse_response(objHTTP *, STRING);
static ERROR process_data(objHTTP *, APTR, LONG);
static LONG  extract_value(CSTRING, STRING *);
static void  writehex(HASH, HASHHEX);
static void  digest_calc_ha1(objHTTP *, HASHHEX);
static void  digest_calc_response(objHTTP *, CSTRING, CSTRING, HASHHEX, HASHHEX, HASHHEX);
static ERROR write_socket(objHTTP *, APTR Buffer, LONG Length, LONG *Result);
static LONG  set_http_method(objHTTP *, STRING Buffer, LONG Size, CSTRING Method);
static void  socket_feedback(objNetSocket *, LONG);
static ERROR SET_Path(objHTTP *, CSTRING);
static ERROR SET_Location(objHTTP *, CSTRING);
static ERROR timeout_manager(objHTTP *, LARGE, LARGE);

//****************************************************************************

INLINE CSTRING GETSTATUS(LONG Code) __attribute__((unused));

INLINE CSTRING GETSTATUS(LONG Code)
{
   for (WORD i=0; clStatus[i].Name; i++) {
      if (clStatus[i].Value IS Code) return clStatus[i].Name;
   }
   return "Unrecognised Status Code";
}

//****************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (LoadModule("network", MODVERSION_NETWORK, &modNetwork, &NetworkBase) != ERR_Okay) return ERR_InitModule;

   if (!CreateObject(ID_PROXY, 0, &glProxy, TAGEND)) {
   }

   return create_http_class();
}

static ERROR CMDExpunge(void)
{
   if (clHTTP)     { acFree(clHTTP);     clHTTP     = NULL; }
   if (glProxy)    { acFree(glProxy);    glProxy    = NULL; }
   if (modNetwork) { acFree(modNetwork); modNetwork = NULL; }
   return ERR_Okay;
}

/*****************************************************************************
** Callback: NetSocket.Outgoing
*/

static ERROR socket_outgoing(objNetSocket *Socket, OBJECTPTR Context)
{
   parasol::Log log(__FUNCTION__);

   #define CHUNK_LENGTH_OFFSET 16
   #define CHUNK_TAIL 2 // CRLF

   log.traceBranch("Socket: %p, Object: %d", Socket, Context->UniqueID);

   auto Self = (objHTTP *)Socket->UserData;

   LONG total_out = 0;

   if (!Self->Buffer) {
      if (Self->BufferSize < BUFFER_WRITE_SIZE) Self->BufferSize = BUFFER_WRITE_SIZE;
      if (Self->BufferSize > 0xffff) Self->BufferSize = 0xffff;

      if (AllocMemory(Self->BufferSize, MEM_DATA|MEM_NO_CLEAR, &Self->Buffer, NULL)) {
         return ERR_AllocMemory;
      }
   }

   ERROR error;
redo_upload:
   Self->WriteBuffer = (UBYTE *)Self->Buffer;
   Self->WriteSize   = Self->BufferSize;
   if (Self->Chunked) {
      Self->WriteBuffer += CHUNK_LENGTH_OFFSET;
      Self->WriteSize   -= CHUNK_LENGTH_OFFSET + CHUNK_TAIL;
   }

   if (Self->State != HGS_SENDING_CONTENT) {
      SetLong(Self, FID_State, HGS_SENDING_CONTENT);
   }

   LONG len = 0;
   if (Self->Outgoing.Type != CALL_NONE) {
      if (Self->Outgoing.Type IS CALL_STDC) {
         auto routine = (ERROR (*)(rkHTTP *, APTR, LONG, LONG *))Self->Outgoing.StdC.Routine;
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
            error = scCallback(script, Self->Outgoing.Script.ProcedureID, args, ARRAYSIZE(args));
            if (!error) GetLong(script, FID_Error, &error);
            if (!error) len = Self->WriteOffset;
            else {
               log.warning("Procedure " PF64() " failed, aborting HTTP call.", Self->Outgoing.Script.ProcedureID);
               error = ERR_Failed; // Fatal error in attempting to execute the procedure
            }
         }
      }
      else error = ERR_Failed;

      if (len > Self->WriteSize) { // Sanity check, this should never happen if the client uses valid code.
         log.warning("Returned length exceeds buffer size!  %d > %d", len, Self->WriteSize);
         len = Self->WriteSize;
         error = ERR_BufferOverflow;
      }
      else if ((error) AND (error != ERR_Terminate)) log.warning("Outgoing callback error: %s", GetErrorMsg(error));
   }
   else if (Self->flInput) {
      if (Self->Flags & HTF_DEBUG) log.msg("Sending content from an Input file.");

      error = acRead(Self->flInput, Self->WriteBuffer, Self->WriteSize, &len);

      if (error) log.warning("Input file read error: %s", GetErrorMsg(error));

      LARGE size;
      GetLarge(Self->flInput, FID_Size, &size);

      if (Self->flInput->Position IS size) {
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

   if (((!error) OR (error IS ERR_Terminate)) AND (len)) {
      LONG result, csize;
      ERROR writeerror;

      log.trace("Writing %d bytes (of expected " PF64() ") to socket.  Chunked: %d", len, Self->ContentLength, Self->Chunked);

      if (Self->Chunked) {
         if (len & 0xf000)      { csize = 4+2; StrFormat(Self->WriteBuffer-6, 5, "%.4x", len); }
         else if (len & 0x0f00) { csize = 3+2; StrFormat(Self->WriteBuffer-5, 4, "%.3x", len); }
         else if (len & 0x00f0) { csize = 2+2; StrFormat(Self->WriteBuffer-4, 3, "%.2x", len); }
         else { csize = 1+2; StrFormat(Self->WriteBuffer-3, 2, "%.1x", len); }

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

      SetLarge(Self, FID_Index, Self->Index + len);

      if (writeerror) {
         log.warning("write_socket() failed: %s", GetErrorMsg(writeerror));
         error = writeerror;
      }

      log.trace("Outgoing index now " PF64() " of " PF64(), Self->Index, Self->ContentLength);
   }
   else log.trace("Finishing (an error occurred, or there is no more content to write to socket).");

   if ((error) AND (error != ERR_Terminate)) {
      if (error != ERR_TimeOut) {
         SetLong(Self, FID_State, HGS_TERMINATED);
         SET_ERROR(Self, error);
         return ERR_Terminate;
      }
      // ERR_TimeOut: The upload process may continue
   }
   else {
      // Check for multiple input files

      if ((Self->MultipleInput) AND (!Self->flInput)) {
         /*if (Self->Flags & HTF_DEBUG)*/ log.msg("Sequential input stream has uploaded " PF64() "/" PF64() " bytes.", Self->Index, Self->ContentLength);

         // Open the next file

         if (!parse_file(Self, (STRING)Self->Buffer, Self->BufferSize)) {
            if (!(CreateObject(ID_FILE, NF_INTEGRAL, &Self->flInput,
                  FID_Path|TSTR,   (CSTRING)Self->Buffer,
                  FID_Flags|TLONG, FL_READ,
                  TAGEND))) {

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

      if (((Self->ContentLength > 0) AND (Self->Index >= Self->ContentLength)) OR (error IS ERR_Terminate)) {
         LONG result;

         if (Self->Chunked) write_socket(Self, (UBYTE *)"0\r\n\r\n", 5, &result);

         if (Self->Flags & HTF_DEBUG) log.msg("Transfer complete - sent " PF64() " bytes.", Self->TotalSent);
         SetLong(Self, FID_State, HGS_SEND_COMPLETE);
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
      FUNCTION callback;
      SET_FUNCTION_STDC(callback, (APTR)&timeout_manager);
      SubscribeTimer(time_limit, &callback, &Self->TimeoutManager);
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
   auto Self = (objHTTP *)Socket->UserData;

   if (Self->State >= HGS_COMPLETED) {
      // Erroneous data received from server while we are in a completion/resting state.  Returning a terminate message
      // will cause the socket object to close the connection to the server so that we stop receiving erroneous data.

      log.warning("Unexpected data incoming from server - terminating socket.");
      return ERR_Terminate;
   }

   if (Self->State IS HGS_SENDING_CONTENT) {
      if (Self->ContentLength IS -1) {
         log.warning("Incoming data while streaming content - " PF64() " bytes already written.", Self->Index);
      }
      else if (Self->Index < Self->ContentLength) {
         log.warning("Incoming data while sending content - only " PF64() "/" PF64() " bytes written!", Self->Index, Self->ContentLength);
      }
   }

   if ((Self->State IS HGS_SENDING_CONTENT) OR (Self->State IS HGS_SEND_COMPLETE)) {
      log.trace("Switching state from sending content to reading header.");
      SetLong(Self, FID_State, HGS_READING_HEADER);
      Self->Index = 0;
   }

   if ((Self->State IS HGS_READING_HEADER) OR (Self->State IS HGS_AUTHENTICATING)) {
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

               if ((Self->State IS HGS_AUTHENTICATING) AND (Self->Status != 401)) {
                  log.msg("Authentication successful, reactivating...");
                  Self->SecurePath = FALSE;
                  SetLong(Self, FID_State, HGS_AUTHENTICATED);
                  DelayMsg(AC_Activate, Self->Head.UniqueID, NULL);
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
                        if (!StrCompare("http:", (CSTRING)buffer, 5, 0)) {
                           SetString(Self, FID_Location, (CSTRING)buffer);
                        }
                        else SetString(Self, FID_Path, (CSTRING)buffer);
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

               if ((!Self->ContentLength) OR (Self->ContentLength < -1)) {
                  log.msg("Reponse header received, no content imminent.");
                  SetLong(Self, FID_State, HGS_COMPLETED);
                  return ERR_Terminate;
               }

               log.msg("Complete response header has been received.  Incoming Content: " PF64(), Self->ContentLength);

               if (Self->State != HGS_READING_CONTENT) {
                  SetLong(Self, FID_State, HGS_READING_CONTENT);
               }

               Self->AuthDigest = FALSE;
               if ((Self->Status IS 401) AND (Self->AuthRetries < MAX_AUTH_RETRIES)) {
                  Self->AuthRetries++;

                  if (Self->Password) {
                     // Destroy the current password if it was entered by the user (therefore is invalid) or if it was
                     // preset and second authorisation attempt failed (in the case of preset passwords, two
                     // authorisation attempts are required in order to receive the 401 from the server first).

                     if ((Self->AuthPreset IS FALSE) OR (Self->AuthRetries >= 2)) {
                        for (LONG i=0; Self->Password[i]; i++) Self->Password[i] = 0xff;
                        FreeResource(Self->Password);
                        Self->Password = NULL;
                     }
                  }

                  CSTRING auth;
                  if ((auth = VarGetString(Self->Args, "WWW-Authenticate"))) {
                     if (!StrCompare("Digest", auth, 6, 0)) {
                        log.trace("Digest authentication mode.");

                        if (Self->Realm)      { FreeResource(Self->Realm);      Self->Realm = NULL; }
                        if (Self->AuthNonce)  { FreeResource(Self->AuthNonce);  Self->AuthNonce = NULL; }
                        if (Self->AuthOpaque) { FreeResource(Self->AuthOpaque); Self->AuthOpaque = NULL; }

                        Self->AuthAlgorithm[0] = 0;
                        Self->AuthDigest = TRUE;

                        LONG i = 6;
                        while ((auth[i]) AND (auth[i] <= 0x20)) i++;

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
                                    while ((auth[i]) AND (auth[i] <= 0x20)) i++;
                                    if (auth[i] IS '"') {
                                       i++;
                                       while ((auth[i]) AND (auth[i] != '"')) i++;
                                       if (auth[i] IS '"') i++;
                                    }
                                    else i++;
                                 }
                                 else i++;
                              }

                              while (auth[i] > 0x20) i++;
                              while ((auth[i]) AND (auth[i] <= 0x20)) i++;
                           }
                        }
                     }
                     else log.trace("Basic authentication mode.");
                  }
                  else log.msg("Authenticate method unknown.");

                  SetLong(Self, FID_State, HGS_AUTHENTICATING);

                  if ((!Self->Password) AND (!(Self->Flags & HTF_NO_DIALOG))) {
                     // Pop up a dialog requesting the user to authorise himself with the http server.  The user will
                     // need to respond to the dialog before we can repost the HTTP request.

                     ERROR error;
                     STRING scriptfile;
                     if (!AllocMemory(glAuthScriptLength+1, MEM_STRING|MEM_NO_CLEAR, &scriptfile, NULL)) {
                        CopyMemory(glAuthScript, scriptfile, glAuthScriptLength);
                        scriptfile[glAuthScriptLength] = 0;

                        OBJECTPTR script;
                        if (!CreateObject(ID_SCRIPT, NF_INTEGRAL, &script,
                              FID_String|TSTR, scriptfile,
                              TAGEND)) {
                           AdjustLogLevel(1);
                           error = acActivate(script);
                           AdjustLogLevel(-1);
                           acFree(script);
                        }
                        else error = ERR_CreateObject;

                        FreeResource(scriptfile);
                     }
                     else error = ERR_AllocMemory;
                  }
                  else ActionMsg(AC_Activate, Self->Head.UniqueID, NULL);

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

               if ((Self->Status < 200) OR (Self->Status >= 300)) {
                  if (Self->State != HGS_READING_CONTENT) {
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
   else if (Self->State IS HGS_READING_CONTENT) {
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
         // \r\n (indicates end) OR 0\r\n (indicates end of chunks with further HTTP tags following)
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
                  SetLong(Self, FID_State, HGS_COMPLETED);
                  return ERR_Terminate;
               }
               else if (Self->Error) {
                  log.warning("Read() returned error %d whilst reading content.", Self->Error);
                  SetLong(Self, FID_State, HGS_COMPLETED);
                  return ERR_Terminate;
               }
               else if ((!len) AND (Self->ChunkIndex >= Self->ChunkBuffered)) {
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
                     if ((Self->Chunk[i] IS '\r') AND (Self->Chunk[i+1] IS '\n')) {
                        Self->Chunk[i] = 0;
                        Self->ChunkLen = StrToHex((CSTRING)Self->Chunk + Self->ChunkIndex);
                        Self->Chunk[i] = '\r';

                        if (Self->ChunkLen <= 0) {
                           if (Self->Chunk[Self->ChunkIndex] IS '0') {
                              // A line of "0\r\n" indicates an end to the chunks, followed by optional data for
                              // interpretation.

                              log.msg("End of chunks reached, optional data follows.");
                              SetLong(Self, FID_State, HGS_COMPLETED);
                              return ERR_Terminate;
                           }
                           else {
                              // We have reached the terminating line (CRLF on an empty line)
                              log.msg("Received all chunked content.");
                              SetLong(Self, FID_State, HGS_COMPLETED);
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

                  while ((Self->ChunkLen < 0) AND (Self->ChunkIndex < Self->ChunkBuffered)) {
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
                  if ((Self->Error IS ERR_Disconnected) AND (Self->ContentLength IS -1)) {
                     log.trace("Received all streamed content (disconnected by peer).");
                     SetLong(Self, FID_State, HGS_COMPLETED);
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
         FUNCTION callback;
         SET_FUNCTION_STDC(callback, (APTR)&timeout_manager);
         SubscribeTimer(Self->DataTimeout, &callback, &Self->TimeoutManager);
      }

      if (Self->Error) return ERR_Terminate;
   }
   else {
      UBYTE buffer[512];
      // Indeterminate data received from HTTP server

      if ((!acRead(Socket, buffer, sizeof(buffer)-1, &len)) AND (len > 0)) {
         buffer[len] = 0;
         log.warning("WARNING: Received data whilst in state %d.", Self->State);
         log.warning("Content (%d bytes) Follows:\n%.80s", len, buffer);
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR HTTP_ActionNotify(objHTTP *Self, struct acActionNotify *Args)
{
   parasol::Log log;

   if (!Args) return ERR_NullArgs;
   if (Args->Error != ERR_Okay) return ERR_Okay;

   if (Args->ActionID IS AC_Free) {
      if (Args->ObjectID IS Self->DialogWindow) {
         Self->DialogWindow = 0;
         if ((Self->Username) AND (Self->Password)) { // Make a second attempt at resolving the HTTP request
            HTTP_Activate(Self, NULL);
         }
         else {
            log.msg("No username and password provided, deactivating...");
            SetLong(Self, FID_State, HGS_TERMINATED);
         }
         return ERR_Okay;
      }
      else if ((Self->Outgoing.Type IS CALL_SCRIPT) AND (Self->Outgoing.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->Outgoing.Type = CALL_NONE;
         return ERR_Okay;
      }
      else if ((Self->StateChanged.Type IS CALL_SCRIPT) AND (Self->StateChanged.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->StateChanged.Type = CALL_NONE;
         return ERR_Okay;
      }
      else if ((Self->Incoming.Type IS CALL_SCRIPT) AND (Self->Incoming.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->Incoming.Type = CALL_NONE;
         return ERR_Okay;
      }
      else if ((Self->AuthCallback.Type IS CALL_SCRIPT) AND (Self->AuthCallback.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->AuthCallback.Type = CALL_NONE;
         return ERR_Okay;
      }
   }
   return log.warning(ERR_NoSupport);
}

/*****************************************************************************

-ACTION-
Activate: Executes an HTTP method.

This action starts an HTTP operation against a target server.  Based on the desired #Method, an HTTP request
will be sent to the target server and the action will immediately return whilst the HTTP object will wait for a
response from the server.  If the server fails to respond within the time period indicated by the #ConnectTimeout,
the HTTP object will be deactivated (for further details, refer to the #Deactivate() action).

Successful interpretation of the HTTP request at the server will result in a response being received, followed by file
data (if applicable). The HTTP response code will be stored in the #Status field.  The HTTP object will
automatically parse the response data and store the received values in the HTTP object as variable fields.  It is
possible to be alerted to the complete receipt of a response by listening to the #State field, or waiting for
the Deactivate action to kick in.

Following a response, incoming data can be managed in a number of ways. It may be streamed to an object referenced by
the #OutputObject field through data feeds.  It can be written to the target object if the #ObjectMode is set to
READ_WRITE.  Finally it can be received through C style callbacks if the #Incoming field is set.

On completion of an HTTP request, the #Deactivate() action is called, regardless of the level of success.

-ERRORS-
Okay:   The HTTP get operation was successfully started.
Failed: The HTTP get operation failed immediately for an unspecified reason.
File:   Failed to create a target file if the File field was set.
Write:   Failed to write data to the HTTP NetSocket.
CreateObject: Failed to create a NetSocket object.
HostNotFound: DNS resolution of the domain name in the URI failed.
-END-

*****************************************************************************/

static ERROR parse_file(objHTTP *Self, STRING Buffer, LONG Size)
{
   LONG i;
   LONG pos = Self->InputPos;
   for (i=0; (i < Size-1) AND (Self->InputFile[pos]);) {
      if (Self->InputFile[pos] IS '"') {
         pos++;
         while ((i < Size-1) AND (Self->InputFile[pos]) AND (Self->InputFile[pos] != '"')) {
            Buffer[i++] = Self->InputFile[pos++];
         }
         if (Self->InputFile[pos] IS '"') pos++;
      }
      else if (Self->InputFile[pos] IS '|') {
         pos++;
         while ((Self->InputFile[pos]) AND (Self->InputFile[pos] <= 0x20)) pos++;
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

static ERROR HTTP_Activate(objHTTP *Self, APTR Void)
{
   parasol::Log log;
   char cmd[2048];
   LONG len, resume_from, i;
   ERROR result;

   if (!(Self->Head.Flags & NF_INITIALISED)) return log.warning(ERR_NotInitialised);

   log.branch("Host: %s, Port: %d, Path: %s, Proxy: %s, SSL: %d", Self->Host, Self->Port, Self->Path, Self->ProxyServer, (Self->Flags & HTF_SSL) ? 1 : 0);

   if (Self->TimeoutManager) { UpdateTimer(Self->TimeoutManager, 0); Self->TimeoutManager = 0; }

   SET_ERROR(Self, ERR_Okay);
   Self->ResponseIndex = 0;
   Self->SearchIndex   = 0;
   Self->Index         = 0;
   Self->State         = 0;
   Self->Status        = 0;
   Self->TotalSent     = 0;
   Self->Tunneling     = FALSE;
   Self->Flags        &= ~(HTF_MOVED|HTF_REDIRECTED);

   if ((Self->Socket) AND (Self->Socket->State IS NTC_DISCONNECTED)) {
      SetPointer(Self->Socket, FID_Feedback, NULL);
      acFree(Self->Socket);
      Self->Socket = NULL;
      Self->SecurePath = TRUE;
   }

   if (Self->Response) { FreeResource(Self->Response); Self->Response = NULL; }
   if (Self->flInput) { acFree(Self->flInput); Self->flInput = NULL; }
   if (Self->flOutput) {  acFree(Self->flOutput); Self->flOutput = NULL; }

   if (Self->RecvBuffer) {
      FreeResource(Self->RecvBuffer);
      Self->RecvBuffer = NULL;
      Self->RecvSize = 0;
   }

   resume_from = 0;

   if ((Self->ProxyServer) AND (Self->Flags & HTF_SSL) AND (!Self->Socket)) {
      // SSL tunnelling is required.  Send a CONNECT request to the proxy and
      // then we will follow this up with the actual HTTP requests.

      log.trace("SSL tunnelling is required.");

      len = StrFormat(cmd, sizeof(cmd), "CONNECT %s:%d HTTP/1.1%sHost: %s%sUser-Agent: %s%sProxy-Connection: keep-alive%sConnection: keep-alive%s", Self->Host, Self->Port, CRLF, Self->Host, CRLF, Self->UserAgent, CRLF, CRLF, CRLF);
      Self->Tunneling = TRUE;

      //set auth "Proxy-Authorization: Basic [base64::encode $opts(proxyUser):$opts(proxyPass)]"
   }
   else {
      if (Self->Method IS HTM_COPY) {
         // Copies a source (indicated by Path) to a Destination.  The Destination is referenced as an variable field.

         CSTRING dest, overwrite;
         if ((dest = VarGetString(Self->Args, "Destination"))) {
            len = set_http_method(Self, cmd, sizeof(cmd), "COPY");
            len += StrFormat(cmd+len, sizeof(cmd)-len, "Destination: http://%s/%s%s", Self->Host, dest, CRLF);
            if ((overwrite = VarGetString(Self->Args, "Overwrite"))) {
               // If the overwrite is 'F' then copy will fail if the destination exists
               len += StrFormat(cmd, sizeof(cmd), "Overwrite: %s%s", overwrite, CRLF);
            }
         }
         else {
            log.warning("HTTP COPY request requires a destination path.");
            SET_ERROR(Self, ERR_FieldNotSet);
            return Self->Error;
         }
      }
      else if (Self->Method IS HTM_DELETE) {
         len = set_http_method(Self, cmd, sizeof(cmd), "DELETE");
      }
      else if (Self->Method IS HTM_GET) {
         len = set_http_method(Self, cmd, sizeof(cmd), "GET");
         if (Self->Index) len += StrFormat(cmd+len, sizeof(cmd)-len, "Range: bytes=" PF64() "-%s", Self->Index, CRLF);
      }
      else if (Self->Method IS HTM_LOCK) {
         len = 0;
      }
      else if (Self->Method IS HTM_MK_COL) {
        len = set_http_method(Self, cmd, sizeof(cmd), "MKCOL");
      }
      else if (Self->Method IS HTM_MOVE) {
         // Moves a source (indicated by Path) to a Destination.  The Destination is referenced as a variable field.

         CSTRING dest;
         if ((dest = VarGetString(Self->Args, "Destination"))) {
            len = set_http_method(Self, cmd, sizeof(cmd), "MOVE");
            len += StrFormat(cmd+len, sizeof(cmd)-len, "Destination: http://%s/%s%s", Self->Host, dest, CRLF);
         }
         else {
            log.warning("HTTP MOVE request requires a destination path.");
            SET_ERROR(Self, ERR_FieldNotSet);
            return Self->Error;
         }
      }
      else if (Self->Method IS HTM_OPTIONS) {
         if ((!Self->Path) OR ((Self->Path[0] IS '*') AND (!Self->Path[1]))) {
            len = StrFormat(cmd, sizeof(cmd), "OPTIONS * HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\n", Self->Host, Self->UserAgent);
         }
         else len = set_http_method(Self, cmd, sizeof(cmd), "OPTIONS");
      }
      else if ((Self->Method IS HTM_POST) OR (Self->Method IS HTM_PUT)) {
         log.trace("POST/PUT request being processed.");

         Self->Chunked = FALSE;

         if ((!(Self->Flags & HTF_NO_HEAD)) AND ((Self->SecurePath) OR (Self->State IS HGS_AUTHENTICATING))) {
            log.trace("Executing HEAD statement for authentication.");
            len = set_http_method(Self, cmd, sizeof(cmd), "HEAD");
            SetLong(Self, FID_State, HGS_AUTHENTICATING);
         }
         else {
            // You can post data from a file source or an object.  In the case of an object it is possible to preset
            // the content-length, although we will attempt to read the amount to transfer from the object's Size
            // field, if supported.  An Outgoing routine can be specified for customised output.
            //
            // To post data from a string, use an InputFile setting as follows:  string:data=to&send

            if (Self->Outgoing.Type != CALL_NONE) {
               // User has specified an Outgoing function.  No preparation is necessary.  It is recommended that
               // ContentLength is set beforehand if the amount of data to be sent is known, otherwise
               // the developer should set ContentLength to -1.

            }
            else if (Self->InputFile) {
               ERROR error;

               if (Self->MultipleInput) {
                  MSG("Multiple input files detected.");
                  Self->InputPos = 0;
                  parse_file(Self, cmd, sizeof(cmd));
                  error = CreateObject(ID_FILE, NF_INTEGRAL, &Self->flInput,
                     FID_Path|TSTR,   cmd,
                     FID_Flags|TLONG, FL_READ,
                     TAGEND);
               }
               else {
                  error = CreateObject(ID_FILE, NF_INTEGRAL, &Self->flInput,
                     FID_Path|TSTR,   Self->InputFile,
                     FID_Flags|TLONG, FL_READ,
                     TAGEND);
               }

               if (!error) {
                  Self->Index = 0;
                  if (!Self->Size) {
                     GetLarge(Self->flInput, FID_Size, &Self->ContentLength);
                  }
                  else Self->ContentLength = Self->Size;
               }
               else {
                  SET_ERROR(Self, ERR_File);
                  return log.warning(Self->Error);
               }
            }
            else if (Self->InputObjectID) {
               if (!Self->Size) {
                  OBJECTPTR input;
                  if (!AccessObject(Self->InputObjectID, 3000, &input)) {
                     LARGE len;
                     if (!GetLarge(input, FID_Size, &len)) Self->ContentLength = len;
                     ReleaseObject(input);
                  }
               }
               else Self->ContentLength = Self->Size;
            }
            else {
               log.warning("No data source specified for POST/PUT method.");
               SET_ERROR(Self, ERR_FieldNotSet);
               return Self->Error;
            }

            len = set_http_method(Self, cmd, sizeof(cmd), (Self->Method IS HTM_POST) ? "POST" : "PUT");

            if (Self->ContentLength >= 0) {
               len += StrFormat(cmd+len, sizeof(cmd)-len, "Content-length: " PF64() "\r\n", Self->ContentLength);
            }
            else {
               log.msg("Content-length not defined for POST/PUT (transfer will be streamed).");

               // Using chunked encoding for post/put will help the server manage streaming
               // uploads, and may even be of help when the content length is known.

               if (!(Self->Flags & HTF_RAW)) {
                  len += StrCopy("Transfer-Encoding: chunked\r\n", cmd+len, sizeof(cmd)-len);
                  Self->Chunked = TRUE;
               }
            }

            if (Self->ContentType) {
               log.trace("User content type: %s", Self->ContentType);
               len += StrFormat(cmd+len, sizeof(cmd)-len, "Content-type: %s\r\n", Self->ContentType);
            }
            else if (Self->Method IS HTM_POST) {
               len += StrCopy("Content-type: application/x-www-form-urlencoded\r\n", cmd+len, sizeof(cmd)-len);
            }
            else len += StrCopy("Content-type: application/binary\r\n", cmd+len, sizeof(cmd)-len);
         }
      }
      else if (Self->Method IS HTM_UNLOCK) {
         len = 0;

      }
      else {
         log.warning("HTTP method no. %d not understood.", Self->Method);
         SET_ERROR(Self, ERR_Failed);
         return Self->Error;
      }

      // Authentication support.  At least one attempt to get the resource (Retries > 0) is required before we can pass the
      // username and password, as it is necessary to be told the method of authentication required (in the case of digest
      // authentication, the nonce value is also required from the server).

      if ((Self->AuthRetries > 0) AND (Self->Username) AND (Self->Password)) {
         if (Self->AuthDigest) {
            UBYTE nonce_count[9] = "00000001";
            HASHHEX HA1, HA2 = "", response;

            for (i=0; i < 8; i++) Self->AuthCNonce[i] = '0' + RandomNumber(10);
            Self->AuthCNonce[i] = 0;

            digest_calc_ha1(Self, HA1);
            digest_calc_response(Self, cmd, (CSTRING)nonce_count, HA1, HA2, response);

            len += StrCopy("Authorization: Digest ", cmd+len, sizeof(cmd)-len);
            len += StrFormat(cmd+len, sizeof(cmd)-len, "username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"/%s\", qop=%s, nc=%s, cnonce=\"%s\", response=\"%s\"",
               Self->Username, Self->Realm, Self->AuthNonce, Self->Path, Self->AuthQOP, nonce_count, Self->AuthCNonce, response);

            if (Self->AuthOpaque) len += StrFormat(cmd+len, sizeof(cmd)-len, ", opaque=\"%s\"", Self->AuthOpaque);

            len += StrCopy("\r\n", cmd+len, sizeof(cmd)-len);
         }
         else {
            char buffer[256];

            len += StrCopy("Authorization: Basic ", cmd+len, sizeof(cmd)-len);
            StrFormat(buffer, sizeof(buffer), "%s:%s", Self->Username, Self->Password);
            len += StrBase64Encode(buffer, StrLength(buffer), cmd+len, sizeof(cmd)-len);
            len += StrCopy("\r\n", cmd+len, sizeof(cmd)-len);
         }

         // Clear the password.  This has the effect of resetting the authentication attempt in case the credentials are wrong.

   /*
         for (i=0; Self->Password[i]; i++) Self->Password[i] = 0;
         FreeResource(Self->Password);
         Self->Password = NULL;
   */
      }

      // Add any custom headers

      if (Self->State != HGS_AUTHENTICATING) {
         CSTRING key = NULL, value;
         LONG value_len;
         while (!VarIterate(Self->Headers, key, &key, &value, &value_len)) {
            log.trace("Custom header: %s: %s", key, value);
            len += StrFormat(cmd+len, sizeof(cmd)-len, "%s: %s\r\n", key, value);
         }
      }

      if (Self->Flags & HTF_DEBUG) log.msg("HTTP REQUEST HEADER\n%s", cmd);
   }

   // Terminating line feed

   len += StrCopy(CRLF, cmd+len, sizeof(cmd)-len);

   if (!Self->Socket) {
      if (NewObject(ID_NETSOCKET, NF_INTEGRAL, &Self->Socket) != ERR_Okay) {
         log.warning("Failed to create NetSocket.");
         SET_ERROR(Self, ERR_NewObject);
         return log.warning(Self->Error);
      }

      SetFields(Self->Socket,
         FID_UserData|TPTR, Self,
         FID_Incoming|TPTR, &socket_incoming,
         FID_Feedback|TPTR, &socket_feedback,
         TAGEND);

      // If we using straight SSL without tunnelling, set the SSL flag now so that SSL is automatically engaged on connection.

      if ((Self->Flags & HTF_SSL) AND (!Self->Tunneling)) {
         Self->Socket->Flags |= NSF_SSL;
      }

      if (acInit(Self->Socket) != ERR_Okay) {
         SET_ERROR(Self, ERR_Init);
         return log.warning(Self->Error);
      }
   }
   else {
      log.trace("Re-using existing socket/server connection.");

      SetPointer(Self->Socket, FID_Incoming, (APTR)&socket_incoming);
      SetPointer(Self->Socket, FID_Feedback, (APTR)&socket_feedback);
   }

   if (!Self->Tunneling) {
      if (Self->State != HGS_AUTHENTICATING) {
         if ((Self->Method IS HTM_PUT) OR (Self->Method IS HTM_POST)) {
            SetPointer(Self->Socket, FID_Outgoing, (APTR)&socket_outgoing);
         }
         else SetPointer(Self->Socket, FID_Outgoing, NULL);
      }
      else SetPointer(Self->Socket, FID_Outgoing, NULL);
   }

   // Buffer the HTTP command string to the socket (will write on connect if we're not connected already).

   if (!write_socket(Self, cmd, len, NULL)) {
      if (Self->Socket->State IS NTC_DISCONNECTED) {
         if ((result = nsConnect(Self->Socket, Self->ProxyServer ? Self->ProxyServer : Self->Host, Self->ProxyServer ? Self->ProxyPort : Self->Port)) IS ERR_Okay) {
            Self->Connecting = TRUE;

            if (Self->TimeoutManager) UpdateTimer(Self->TimeoutManager, Self->ConnectTimeout);
            else {
               FUNCTION callback;
               SET_FUNCTION_STDC(callback, (APTR)&timeout_manager);
               SubscribeTimer(Self->ConnectTimeout, &callback, &Self->TimeoutManager);
            }

            return ERR_Okay;
         }
         else if (result IS ERR_HostNotFound) {
            SET_ERROR(Self, ERR_HostNotFound);
            return log.warning(Self->Error);
         }
         else {
            SET_ERROR(Self, ERR_Failed);
            return log.warning(Self->Error);
         }
      }
      else return ERR_Okay;
   }
   else {
      SET_ERROR(Self, ERR_Write);
      return log.warning(Self->Error);
   }
}

/*****************************************************************************
-ACTION-
Deactivate: Cancels the current download.  Can also signal the end to a download if subscribed.

Following the completion of an HTTP request, the Deactivate action will be called internally to signal an end to the
process.  By listening to the Deactivate action, you are given the opportunity to respond to the end of an HTTP request.

If child objects are initialised to the HTTP object, they will be activated automatically.  This feature is provided to
assist scripted usage of the HTTP object.

Active HTTP requests can be manually cancelled by calling the Deactivate action at any time.
-END-
*****************************************************************************/

static ERROR HTTP_Deactivate(objHTTP *Self, APTR Void)
{
   parasol::Log log;

   log.branch("Closing connection to server & signalling children.");

   if (Self->State < HGS_COMPLETED) SetLong(Self, FID_State, HGS_TERMINATED);

   // Closing files is important for dropping the file locks

   if (Self->flInput) { acFree(Self->flInput); Self->flInput = NULL; }
   if (Self->flOutput) { acFree(Self->flOutput); Self->flOutput = NULL; }

   // Free up the outgoing buffer since it is only needed during transfers and will be reallocated as necessary.

   if (Self->Buffer) { FreeResource(Self->Buffer); Self->Buffer = NULL; }
   if (Self->TimeoutManager) { UpdateTimer(Self->TimeoutManager, 0); Self->TimeoutManager = 0; }

   if (Self->Socket) {
      // The socket object is removed if it has been closed at the server, OR if our HTTP object is closing prematurely
      // (for example due to a timeout, or an early call to Deactivate).  This prevents any more incoming data from the
      // server being processed when we don't want it.

      if ((Self->Socket->State IS NTC_DISCONNECTED) OR (Self->State IS HGS_TERMINATED)) {
         log.msg("Terminating socket (disconnected).");
         SetPointer(Self->Socket, FID_Feedback, NULL);
         acFree(Self->Socket);
         Self->Socket = NULL;
         Self->SecurePath = TRUE;
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR HTTP_Free(objHTTP *Self, APTR Args)
{
   if (Self->Args) { FreeResource(Self->Args); Self->Args = NULL; }
   if (Self->Headers) { FreeResource(Self->Headers); Self->Headers = NULL; }

   if (Self->Socket)     {
      SetPointer(Self->Socket, FID_Feedback, NULL);
      acFree(Self->Socket);
      Self->Socket = NULL;
   }

   if (Self->TimeoutManager) { UpdateTimer(Self->TimeoutManager, 0); Self->TimeoutManager = 0; }

   if (Self->flInput)     { acFree(Self->flInput);           Self->flInput = NULL; }
   if (Self->flOutput)    { acFree(Self->flOutput);          Self->flOutput = NULL; }
   if (Self->Buffer)      { FreeResource(Self->Buffer);      Self->Buffer = NULL; }
   if (Self->Chunk)       { FreeResource(Self->Chunk);       Self->Chunk = NULL; }
   if (Self->Path)        { FreeResource(Self->Path);        Self->Path = NULL; }
   if (Self->InputFile)   { FreeResource(Self->InputFile);   Self->InputFile = NULL; }
   if (Self->OutputFile)  { FreeResource(Self->OutputFile);  Self->OutputFile = NULL; }
   if (Self->Host)        { FreeResource(Self->Host);        Self->Host = NULL; }
   if (Self->Response)    { FreeResource(Self->Response);    Self->Response = NULL; }
   if (Self->UserAgent)   { FreeResource(Self->UserAgent);   Self->UserAgent = NULL; }
   if (Self->Username)    { FreeResource(Self->Username);    Self->Username = NULL; }
   if (Self->AuthNonce)   { FreeResource(Self->AuthNonce);   Self->AuthNonce = NULL; }
   if (Self->Realm)       { FreeResource(Self->Realm);       Self->Realm = NULL; }
   if (Self->AuthOpaque)  { FreeResource(Self->AuthOpaque);  Self->AuthOpaque = NULL; }
   if (Self->AuthPath)    { FreeResource(Self->AuthPath);    Self->AuthPath = NULL; }
   if (Self->ContentType) { FreeResource(Self->ContentType); Self->ContentType = NULL; }
   if (Self->RecvBuffer)  { FreeResource(Self->RecvBuffer);  Self->RecvBuffer = NULL; }
   if (Self->ProxyServer) { FreeResource(Self->ProxyServer); Self->ProxyServer = NULL; }

   if (Self->Password) {
      for (LONG i=0; Self->Password[i]; i++) Self->Password[i] = 0xff;
      FreeResource(Self->Password);
      Self->Password = NULL;
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
GetVar: Entries in the HTTP response header can be read as variable fields.
-END-
*****************************************************************************/

static ERROR HTTP_GetVar(objHTTP *Self, struct acGetVar *Args)
{
   if (!Args) return ERR_NullArgs;

   CSTRING value;
   LONG len;
   if (!VarGet(Self->Args, Args->Field, &value, &len)) {
      CopyMemory(value, Args->Buffer, len);
      return ERR_Okay;
   }

   if (!VarGet(Self->Headers, Args->Field, &value, &len)) {
      CopyMemory(value, Args->Buffer, len);
      return ERR_Okay;
   }

   return ERR_UnsupportedField;
}

//****************************************************************************

static ERROR HTTP_Init(objHTTP *Self, APTR Args)
{
   parasol::Log log;

   if (!Self->ProxyDefined) {
      if (glProxy) {
         if (!prxFind(glProxy, Self->Port, TRUE)) {
            if (Self->ProxyServer) FreeResource(Self->ProxyServer);
            Self->ProxyServer = StrClone(glProxy->Server);
            Self->ProxyPort   = glProxy->ServerPort; // NB: Default is usually 8080

            log.msg("Using preset proxy server '%s:%d'", Self->ProxyServer, Self->ProxyPort);
         }
      }
      else log.msg("Global proxy configuration object is missing.");
   }
   else log.msg("Proxy pre-defined by user.");

   return ERR_Okay;
}

//****************************************************************************

static ERROR HTTP_NewObject(objHTTP *Self, APTR Args)
{
   Self->Error          = ERR_Okay;
   Self->UserAgent      = StrClone("Parasol Client");
   Self->DataTimeout    = 5.0;
   Self->ConnectTimeout = 10.0;
   Self->Datatype       = DATA_RAW;
   Self->BufferSize     = 16 * 1024;
   StrCopy("auth", (STRING)Self->AuthQOP, sizeof(Self->AuthQOP));
   StrCopy("md5", (STRING)Self->AuthAlgorithm, sizeof(Self->AuthAlgorithm));
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
SetVar: Options to pass in the HTTP method header can be set as variable fields.
-END-
*****************************************************************************/

static ERROR HTTP_SetVar(objHTTP *Self, struct acSetVar *Args)
{
   if (!Args) return ERR_NullArgs;

   return VarSetString(Self->Headers, Args->Field, Args->Value);
}

//****************************************************************************
// Writing to an HTTP object's outgoing buffer is possible if the Outgoing callback function is active.

static ERROR HTTP_Write(objHTTP *Self, struct acWrite *Args)
{
   if ((!Args) OR (!Args->Buffer)) return ERR_NullArgs;

   if ((Self->WriteBuffer) AND (Self->WriteSize > 0)) {
      LONG len = Args->Length;
      if (Self->WriteOffset + len > Self->WriteSize) {
         len = Self->WriteSize - Self->WriteOffset;
      }

      if (len > 0) {
         CopyMemory(Args->Buffer, Self->WriteBuffer + Self->WriteOffset, len);
         Self->WriteOffset += len;
         Args->Result = len;
         if (Args->Result != Args->Length) return ERR_LimitedSuccess;
         else return ERR_Okay;
      }
      else {
         Args->Result = 0;
         return ERR_BufferOverflow;
      }
   }
   else return ERR_BadState;
}

/*****************************************************************************
-FIELD-
AuthCallback: Private.  This field is reserved for future use.

*****************************************************************************/

static ERROR GET_AuthCallback(objHTTP *Self, FUNCTION **Value)
{
   if (Self->AuthCallback.Type != CALL_NONE) {
      *Value = &Self->AuthCallback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_AuthCallback(objHTTP *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->AuthCallback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->AuthCallback.Script.Script, AC_Free);
      Self->AuthCallback = *Value;
      if (Self->AuthCallback.Type IS CALL_SCRIPT) SubscribeAction(Self->AuthCallback.Script.Script, AC_Free);
   }
   else Self->AuthCallback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
BufferSize: Indicates the preferred buffer size for data operations.

The default buffer size for HTTP data operations is indicated here.  It affects the size of the temporary buffer that
is used for storing outgoing data (PUT and POST operations).

Note that the actual buffer size may not reflect the exact size that you set here.

*****************************************************************************/

static ERROR SET_BufferSize(objHTTP *Self, LONG Value)
{
   if (Value < 2 * 1024) Value = 2 * 1024;
   Self->BufferSize = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
ConnectTimeout: The initial connection timeout value, measured in seconds.

The timeout for connect operations is specified here.  In the event of a timeout, the HTTP object will be deactivated
and the #Error field will be updated to a value of ERR_TimeOut.

The timeout value is measured in seconds.

-FIELD-
ContentLength: The byte length of incoming or outgoing content.

HTTP servers will return a ContentLength value in their response headers when retrieving information.  This value is
defined here once the response header is processed.  The ContentLength may be set to -1 if the content is being
streamed from the server.

When posting data to a server, set the ContentLength to the amount of data that you intend to send to the server.  If
streaming data of unknown length, set the ContentLength to -1.

-FIELD-
ContentType: Defines the content-type for PUT and POST methods.

The ContentType should be set prior to sending a PUT or POST request.  If NULL, the default content type for POST
methods will be set to 'application/x-www-form-urlencoded'.  For PUT requests the default of 'application/binary' will
be applied.

*****************************************************************************/

static ERROR GET_ContentType(objHTTP *Self, STRING *Value)
{
   *Value = Self->ContentType;
   return ERR_Okay;
}

static ERROR SET_ContentType(objHTTP *Self, CSTRING Value)
{
   if (Self->ContentType) { FreeResource(Self->ContentType); Self->ContentType = NULL; }
   if (Value) Self->ContentType = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
DataTimeout: The data timeout value, relevant when receiving or sending data.

A timeout for send and receive operations is required to prevent prolonged waiting during data transfer operations.
This is essential when interacting with servers that stream data with indeterminate content lengths.  It should be
noted that a timeout does not necessarily indicate failure if the content is being streamed from the server
(#ContentLength is set to -1).

In the event of a timeout, the HTTP object will be deactivated and the #Error field will be updated to a value
of ERR_TimeOut.

The timeout value is measured in seconds.

-FIELD-
Datatype: The default datatype format to use when passing data to a target object.

When streaming downloaded content to an object, the default datatype is RAW (binary mode).  An alternative is to send
the data as TEXT or XML by changing the Datatype field value.

The receiving object can identify the data as HTTP information by checking the class ID of the sender.

-FIELD-
Error: The error code received for the most recently executed HTTP command.

On completion of an HTTP request, the most appropriate error code will be stored here.  If the request was successful
then the value will be zero (ERR_Okay). It should be noted that certain error codes may not necessarily indicate
failure - for instance, an ERR_TimeOut error may be received on termination of streamed content.  For genuine HTML
error codes, see the #Status field.

-FIELD-
Flags: Optional flags.

-FIELD-
Host: The targeted HTTP server is specified here, either by name or IP address.

The HTTP server to target for HTTP requests is defined here.  To change the host post-initialisation, set the
#Location.

*****************************************************************************/

static ERROR SET_Host(objHTTP *Self, CSTRING Value)
{
   if (Self->Host) { FreeResource(Self->Host); Self->Host = NULL; }
   Self->Host = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Incoming: A callback routine can be defined here for incoming data.

Data can be received from an HTTP request by setting a callback routine in the Incoming field.  The format for the
callback routine is `ERROR Function(*HTTP, APTR Data, LONG Length)`.

If an error code of ERR_Terminate is returned by the callback routine, the currently executing HTTP request will be
cancelled.

*****************************************************************************/

static ERROR GET_Incoming(objHTTP *Self, FUNCTION **Value)
{
   if (Self->Incoming.Type != CALL_NONE) {
      *Value = &Self->Incoming;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Incoming(objHTTP *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->Incoming.Type IS CALL_SCRIPT) UnsubscribeAction(Self->Incoming.Script.Script, AC_Free);
      Self->Incoming = *Value;
      if (Self->Incoming.Type IS CALL_SCRIPT) SubscribeAction(Self->Incoming.Script.Script, AC_Free);
   }
   else Self->Incoming.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Index: Indicates download progress in terms of bytes received.

If an HTTP GET request is executed, the Index field will reflect the number of bytes that have been received.  This
field is updated continuously until either the download is complete or cancelled.

The Index value will always start from zero when downloading, even in resume mode.

The Index field can be monitored for changes so that progress during send and receive transmissions can be tracked.

-FIELD-
InputFile: To upload HTTP content from a file, set a file path here.

HTTP content can be streamed from a source file when a POST command is executed. To do so, set the InputFile field to
the file path that contains the source data.  The path is not opened or checked for validity until the POST command is
executed by the HTTP object.

An alternative is to set the #InputObject for abstracting the data source.

*****************************************************************************/

static ERROR SET_InputFile(objHTTP *Self, CSTRING Value)
{
   parasol::Log log;

   log.trace("InputFile: %.80s", Value);

   if (Self->InputFile) { FreeResource(Self->InputFile);  Self->InputFile = NULL; }

   Self->MultipleInput = FALSE;
   Self->InputPos = 0;
   if ((Value) AND (*Value)) {
      Self->InputFile = StrClone(Value);

      // Check if the path contains multiple inputs, separated by the pipe symbol.

      for (LONG i=0; Self->InputFile[i]; i++) {
         if (Self->InputFile[i] IS '"') {
            i++;
            while ((Self->InputFile[i]) AND (Self->InputFile[i] != '"')) i++;
            if (!Self->InputFile[i]) break;
         }
         else if (Self->InputFile[i] IS '|') {
            Self->MultipleInput = TRUE;
            break;
         }
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
InputObject: Allows data to be sent from an object on execution of a POST command.

HTTP content can be streamed from a source object when a POST command is executed.  To do so, set the InputObject to an
object that supports the #Read() action.  The provided object ID is not checked for validity until the POST
command is executed by the HTTP object.

-FIELD-
Location: A valid HTTP URI must be specified here.

The URI of the HTTP source must be specified here.  The string must start with `http://` or `https://`, followed by the
host name, HTTP path and port number if required. The values mentioned will be broken down and stored in the
#Host, #Path and #Port fields respectively.  Note that if the port is not defined in the URI, the Port field is reset
to the default (80 for HTTP or 443 for HTTPS).

If desired, you can elect to set the #Host, #Path and #Port fields separately if setting a
URI string is inconvenient.
-END-

*****************************************************************************/

static ERROR GET_Location(objHTTP *Self, STRING *Value)
{
   parasol::Log log;

   Self->AuthRetries = 0; // Reset the retry counter

   if (Self->URI) { FreeResource(Self->URI); Self->URI = NULL; }

   OBJECTPTR context = SetContext(Self);
   LONG len = 7 + StrLength(Self->Host) + 16 + StrLength(Self->Path) + 1;
   ERROR error = AllocMemory(len, MEM_STRING|MEM_NO_CLEAR, &Self->URI, NULL);
   SetContext(context);

   if (!error) {
      if (Self->Port IS 80) StrFormat(Self->URI, len, "http://%s/%s", Self->Host, Self->Path); // http
      else if (Self->Port IS 443) {
         StrFormat(Self->URI, len, "https://%s/%s", Self->Host, Self->Path); // https
         Self->Flags |= HTF_SSL;
      }
      else if (Self->Port IS 21) StrFormat(Self->URI, len, "ftp://%s/%s", Self->Host, Self->Path); // ftp
      else StrFormat(Self->URI, len, "http://%s:%d/%s", Self->Host, Self->Port, Self->Path);
      *Value = Self->URI;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return log.warning(ERR_AllocMemory);
   }
}

static ERROR SET_Location(objHTTP *Self, CSTRING Value)
{
   parasol::Log log;

   if (Self->Head.Flags & NF_INITIALISED) {
      if (Self->TimeoutManager) { UpdateTimer(Self->TimeoutManager, 0); Self->TimeoutManager = 0; }

      // Free the current socket if the entire URI changes

      if (Self->Socket) {
         SetPointer(Self->Socket, FID_Feedback, NULL);
         acFree(Self->Socket);
         Self->Socket = NULL;
      }

      log.msg("%s", Value);
   }

   CSTRING str = Value;

   Self->Port = 80;

   if (!StrCompare("http://", str, 7, 0)) str += 7;
   else if (!StrCompare("https://", str, 8, 0)) {
      str += 8;
      Self->Port = 443;
      Self->Flags |= HTF_SSL;
   }

   if (Self->Host) { FreeResource(Self->Host); Self->Host = NULL; }
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }

   // Parse host name

   LONG len, i;
   for (len=0; (str[len]) AND (str[len] != ':') AND (str[len] != '/'); len++);

   if (AllocMemory(len+1, MEM_STRING|MEM_NO_CLEAR, &Self->Host, NULL) != ERR_Okay) {
      return ERR_AllocMemory;
   }

   CharCopy(str, Self->Host, len);
   Self->Host[len] = 0;

   str += len;

   // Parse port number

   if (*str IS ':') {
      str++;
      i = StrToInt(str);
      if (i) {
         Self->Port = i;
         if (Self->Port IS 443) Self->Flags |= HTF_SSL;
      }
   }

   while ((*str) AND (*str != '/')) str++;

   // Parse absolute path

   if (*str) {
      SET_Path(Self, str+1);
      return ERR_Okay;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Method: The HTTP instruction to execute is defined here (defaults to GET).

*****************************************************************************/

static ERROR SET_Method(objHTTP *Self, LONG Value)
{
   // Changing/Setting the method results in a reset of the variable fields
   if (Self->Args) { FreeResource(Self->Args); Self->Args = NULL; }
   if (Self->Headers) { FreeResource(Self->Headers); Self->Headers = NULL; }
   Self->Method = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
ObjectMode: The access mode used when passing data to a targeted object.

This field is relevant when the #OutputObject field has been set for receiving incoming data. The method of
communication used against the target object can be defined through the ObjectMode. The default setting is DATA_FEED,
which passes data through the data feed system (see also the #Datatype to define the type of data being
sent to the object).  The alternative method is READ_WRITE, which uses the Write action to send data to the targeted
object.

-FIELD-
Outgoing: Outgoing data can be managed using a function callback if this field is set.

Outgoing data can be managed manually by providing the HTTP object with an outgoing callback routine.  The format for
the callback routine is `ERROR Function(*HTTP, APTR Buffer, LONG BufferSize, LONG *Result)`

Outgoing content is placed in the Buffer address and must not exceed the indicated BufferSize.  The total number of
bytes placed in the Buffer must be indicated in the Result parameter before the callback routine returns.

If an error code of ERR_Terminate is returned by the callback routine, any remaining data will be sent and the transfer
will be treated as having completed successfully.  Use ERR_TimeOut if data cannot be returned in a reasonable time
frame.  All other error codes apart from ERR_Okay indicate failure.

*****************************************************************************/

static ERROR GET_Outgoing(objHTTP *Self, FUNCTION **Value)
{
   if (Self->Outgoing.Type != CALL_NONE) {
      *Value = &Self->Outgoing;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Outgoing(objHTTP *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->Outgoing.Type IS CALL_SCRIPT) UnsubscribeAction(Self->Outgoing.Script.Script, AC_Free);
      Self->Outgoing = *Value;
      if (Self->Outgoing.Type IS CALL_SCRIPT) SubscribeAction(Self->Outgoing.Script.Script, AC_Free);
   }
   else Self->Outgoing.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
OutputFile: To download HTTP content to a file, set a file path here.

HTTP content can be streamed to a target file during transfer.  To do so, set the OutputFile field to the destination
file name that will receive data.  If the file already exists, it will be overwritten unless the RESUME flag has been
set in the #Flags field.

*****************************************************************************/

static ERROR SET_OutputFile(objHTTP *Self, CSTRING Value)
{
   if (Self->OutputFile) { FreeResource(Self->OutputFile); Self->OutputFile = NULL; }
   Self->OutputFile = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
OutputObject: Incoming data can be sent to the object referenced in this field.

HTTP content can be streamed to a target object during incoming data transfers. To do so, set the OutputObject to an
object that supports data feeds and/or the #Write() action. The type of method used for passing data to the
output object is determined by the setting in the #ObjectMode field.

The provided object ID is not checked for validity until the POST command is executed by the HTTP object.

-FIELD-
Password: The password to use when authenticating access to the server.

A password may be preset if authorisation is required against the HTTP server for access to a particular resource.
Note that if authorisation is required and no username and password has been preset, the HTTP object will automatically
present a dialog box to the user to request the relevant information.

A 401 status code is returned in the event of an authorisation failure.

*****************************************************************************/

static ERROR SET_Password(objHTTP *Self, CSTRING Value)
{
   if (Self->Password) { FreeResource(Self->Password); Self->Password = NULL; }
   Self->Password = StrClone(Value);
   Self->AuthPreset = TRUE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Path: The HTTP path targeted at the host server.

The path to target at the host server is specified here.  If no path is set, the server root will be targeted.  It is
not necessary to set the path if one has been specified in the #Location.

If spaces are discovered in the path, they will be converted to the '%20' HTTP escape code automatically.  No other
automatic conversions are operated when setting the Path field.

*****************************************************************************/

static ERROR SET_Path(objHTTP *Self, CSTRING Value)
{
   Self->AuthRetries = 0; // Reset the retry counter

   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }

   if (Value) {
      while (*Value IS '/') Value++; // Skip '/' prefix

      LONG len = 0;
      LONG i;
      for (i=0; Value[i]; i++) {
         if (Value[i] IS ' ') len += 3;
         else len++;
      }

      if (!AllocMemory(len+1, MEM_STRING|MEM_NO_CLEAR, &Self->Path, NULL)) {
         LONG len = 0;
         for (i=0; Value[i]; i++) {
            if (Value[i] IS ' ') {
               Self->Path[len++] = '%';
               Self->Path[len++] = '2';
               Self->Path[len++] = '0';
            }
            else Self->Path[len++] = Value[i];
         }
         Self->Path[len] = 0;

         // Check if this path has been authenticated against the server yet by comparing it to AuthPath.  We need to
         // do this if a PUT instruction is executed against the path and we're not authenticated yet.

         while ((len > 0) AND (Self->Path[len-1] != '/')) len--;

         Self->SecurePath = TRUE;
         if (Self->AuthPath) {
            for (i=0; Self->AuthPath[i]; i++);
            while ((i > 0) AND (Self->AuthPath[i-1] != '/')) i--;

            if (i IS len) {
               if (!StrCompare(Self->Path, Self->AuthPath, len, 0)) {
                  // No change to the current path
                  Self->SecurePath = FALSE;
               }
            }
         }

         Self->AuthPath = StrClone(Self->Path);
         Self->AuthPath[len] = 0;

         return ERR_Okay;
      }
      else return ERR_AllocMemory;
   }
   else return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Port: The HTTP port to use when targeting a server.

The Port to target at the HTTP server is defined here.  The default for HTTP requests is port 80.  To change the port
number, set the #Location.

-FIELD-
ProxyPort: The port to use when communicating with the proxy server.

If the ProxyServer field has been set, the ProxyPort must be set to the port number used by the proxy server for all
requests.  By default the ProxyPort is set to 8080 which is commonly used for proxy communications.

-FIELD-
ProxyServer: The targeted HTTP server is specified here, either by name or IP address.

If a proxy server will receive the HTTP request, set the name or IP address of the server here.  To specify the port
that the proxy server uses to receive requests, see the #ProxyPort field.

*****************************************************************************/

static ERROR SET_ProxyServer(objHTTP *Self, CSTRING Value)
{
   if (Self->ProxyServer) { FreeResource(Self->ProxyServer); Self->ProxyServer = NULL; }
   if ((Value) AND (Value[0])) Self->ProxyServer = StrClone(Value);
   Self->ProxyDefined = TRUE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Realm: Identifies the realm during HTTP authentication.

During the user authentication process, a realm name may be returned by the HTTP server.  The Realm field will reflect
this name string.

*****************************************************************************/

static ERROR SET_Realm(objHTTP *Self, CSTRING Value)
{
   if (Self->Realm) { FreeResource(Self->Realm); Self->Realm = NULL; }
   if (Value) Self->Realm = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
RecvBuffer: Refers to a data buffer that is used to store all incoming content.

If the RECV_BUFFER flag is set, all content received from the HTTP server will be stored in a managed buffer
that is referred to by this field.  This field can be read at any time.  It will be set to  NULL if no data has been
received. The buffer address and all content is reset whenever the HTTP object is activated.

The buffer is null-terminated if you wish to use it as a string.

*****************************************************************************/

static ERROR GET_RecvBuffer(objHTTP *Self, UBYTE **Value, LONG *Elements)
{
   *Value = Self->RecvBuffer;
   *Elements = Self->RecvSize;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Size: Set this field to define the length of a data transfer when issuing a POST command.

Prior to the execution of a POST command, it is recommended that you set the Size field to explicitly define the length
of the data transfer.  If this field is not set, the HTTP object will attempt to determine the byte size of the
transfer by reading the size from the source file or object.

-FIELD-
State: Indicates the current state of an HTTP object during its interaction with an HTTP server.

The State is a readable field that tracks the current state of the client in its relationship with the target HTTP
server.  The default state is READING_HEADER.  Changes to the state can be monitored through the
#StateChanged field.

On completion of an HTTP request, the state will be changed to either COMPLETED or TERMINATED.

*****************************************************************************/

static ERROR SET_State(objHTTP *Self, LONG Value)
{
   parasol::Log log;

   if ((Value < 0) OR (Value >= HGS_END)) return log.warning(ERR_OutOfRange);

   if (Self->Flags & HTF_DEBUG) log.msg("New State: %s, Currently: %s", clHTTPState[Value].Name, clHTTPState[Self->State].Name);

   if ((Value >= HGS_COMPLETED) AND (Self->State < HGS_COMPLETED)) {
      Self->State = Value;
      if (Self->Socket) DelayMsg(AC_Deactivate, Self->Head.UniqueID, NULL);
   }
   else Self->State = Value;

   if (Self->StateChanged.Type != CALL_NONE) {
      if (Self->StateChanged.Type IS CALL_STDC) {
         auto routine = (ERROR (*)(rkHTTP *, LONG))Self->StateChanged.StdC.Routine;
         Self->Error = routine(Self, Self->State);
      }
      else if (Self->StateChanged.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = Self->StateChanged.Script.Script)) {
            const ScriptArg args[] = {
               { "HTTP", FD_OBJECTID, { .Long = Self->Head.UniqueID } },
               { "State", FD_LONG, { .Long = Self->State } }
            };

            if (!scCallback(script, Self->StateChanged.Script.ProcedureID, args, ARRAYSIZE(args))) {
               GetLong(script, FID_Error, &Self->Error);
            }
            else SET_ERROR(Self, ERR_Terminate); // Fatal error in attempting to execute the procedure
         }
         else SET_ERROR(Self, ERR_Terminate);
      }

      if ((Self->Error IS ERR_Terminate) AND (Self->State != HGS_TERMINATED) AND (Self->State != HGS_COMPLETED)) {
         log.branch("State changing to HGS_TERMINATED (terminate message received).");
         SET_State(Self, HGS_TERMINATED);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
StateChanged: A callback routine can be defined here for monitoring changes to the HTTP state.

Define a callback routine in StateChanged in order to receive notifications of any change to the #State of an
HTTP object.  The format for the routine is `ERROR Function(*HTTP, LONG State)`.

If an error code of ERR_Terminate is returned by the callback routine, the currently executing HTTP request will be
cancelled.

*****************************************************************************/

static ERROR GET_StateChanged(objHTTP *Self, FUNCTION **Value)
{
   if (Self->StateChanged.Type != CALL_NONE) {
      *Value = &Self->StateChanged;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_StateChanged(objHTTP *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->StateChanged.Type IS CALL_SCRIPT) UnsubscribeAction(Self->StateChanged.Script.Script, AC_Free);
      Self->StateChanged = *Value;
      if (Self->StateChanged.Type IS CALL_SCRIPT) SubscribeAction(Self->StateChanged.Script.Script, AC_Free);
   }
   else Self->StateChanged.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Status: Indicates the HTTP status code returned on completion of an HTTP request.

-FIELD-
UserAgent: Specifies the name of the user-agent string that is sent in HTTP requests.

This field describe the 'user-agent' value that will be sent in HTTP requests.  The default value is 'Parasol Client'.

*****************************************************************************/

static ERROR SET_UserAgent(objHTTP *Self, CSTRING Value)
{
   if (Self->UserAgent) { FreeResource(Self->UserAgent); Self->UserAgent = NULL; }
   Self->UserAgent = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
UserData: An unused field value that is useful for storing private data.

-FIELD-
Username: The username to use when authenticating access to the server.

A username can be preset before executing an HTTP method against a secure server zone.  The supplied credentials will
only be passed to the HTTP server if it asks for authorisation.  The username provided should be accompanied by a
#Password.

In the event that a username or password is not supplied, or if the supplied credentials are invalid, the user will be
presented with a dialog box and asked to enter the correct username and password.
-END-

*****************************************************************************/

static ERROR SET_Username(objHTTP *Self, CSTRING Value)
{
   if (Self->Username) { FreeResource(Self->Username); Self->Username = NULL; }
   Self->Username = StrClone(Value);
   return ERR_Okay;
}

//****************************************************************************

static CSTRING adv_crlf(CSTRING String)
{
   while (*String) {
      if ((*String IS '\r') AND (String[1] IS '\n')) {
         String += 2;
         return String;
      }
      String++;
   }
   return String;
}

//****************************************************************************

static ERROR parse_response(objHTTP *Self, STRING Buffer)
{
   parasol::Log log;

   if (Self->Args) { FreeResource(Self->Args); Self->Args = NULL; }

   if (Self->Flags & HTF_DEBUG) log.msg("HTTP RESPONSE HEADER\n%s", Buffer);

   // First line: HTTP/1.1 200 OK

   if (StrCompare("HTTP/", Buffer, 5, 0) != ERR_Okay) {
      log.warning("Invalid response header, missing 'HTTP/'");
      return ERR_InvalidHTTPResponse;
   }

   CSTRING str = Buffer;
   //LONG majorv = StrToInt(str); // Currently unused
   while ((*str) AND (*str != '.')) str++;
   if (*str IS '.') str++;
   else return ERR_InvalidHTTPResponse;

   //LONG minorv = StrToInt(str); // Currently unused
   while (*str > 0x20) str++;
   while ((*str) AND (*str <= 0x20)) str++;

   Self->Status = StrToInt(str);

   str = adv_crlf(str);

   if (Self->ProxyServer) Self->ContentLength = -1; // Some proxy servers (Squid) strip out information like 'transfer-encoding' yet pass all the requested content anyway :-/
   else Self->ContentLength = 0;
   Self->Chunked = FALSE;

   // Parse response fields

   log.msg("HTTP response header received, status code %d", Self->Status);

   char field[60], value[300];
   while (*str) {
      LONG i;
      for (i=0; (*str) AND (*str != ':') AND (*str != '\r') AND (*str != '\n'); i++) {
         field[i] = *str++;
      }
      field[i] = 0;

      if (*str IS ':') {
         str++;
         while ((*str) AND (*str <= 0x20)) str++;

         for (i=0; (*str) AND (*str != '\r') AND (*str != '\n'); i++) {
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

         VarSetString(Self->Args, field, value);
      }
      else str = adv_crlf(str);
   }

   return ERR_Okay;
}

//****************************************************************************
// Sends some data specified in the arguments to the listener

static ERROR process_data(objHTTP *Self, APTR Buffer, LONG Length)
{
   parasol::Log log(__FUNCTION__);

   log.trace("Buffer: %p, Length: %d", Buffer, Length);

   if (!Length) return ERR_Okay;

   SetLarge(Self, FID_Index, Self->Index + Length); // Use Set() so that field subscribers can track progress with field monitoring

   if ((!Self->flOutput) AND (Self->OutputFile)) {
      LONG flags, type;

      if (Self->Flags & HTF_RESUME) {
         if ((!AnalysePath(Self->OutputFile, &type)) AND (type IS LOC_FILE)) {
            flags = 0;
         }
         else flags = FL_NEW;
      }
      else flags = FL_NEW;

      if (!CreateObject(ID_FILE, NF_INTEGRAL, &Self->flOutput,
            FID_Path|TSTR,   Self->OutputFile,
            FID_Flags|TLONG, flags|FL_WRITE,
            TAGEND)) {
         if (Self->Flags & HTF_RESUME) {
            acSeekEnd(Self->flOutput, 0);
            SetLarge(Self, FID_Index, 0);
         }
      }
      else SET_ERROR(Self, ERR_CreateFile);
   }

   if (Self->flOutput) {
      acWrite(Self->flOutput, Buffer, Length, NULL);
   }

   if (Self->Incoming.Type != CALL_NONE) {
      log.trace("Incoming callback is set.");

      if (Self->Incoming.Type IS CALL_STDC) {
         auto routine = (ERROR (*)(rkHTTP *, APTR, LONG))Self->Incoming.StdC.Routine;
         Self->Error = routine(Self, Buffer, Length);
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
            if (!scCallback(script, Self->Incoming.Script.ProcedureID, args, ARRAYSIZE(args))) {
               GetLong(script, FID_Error, &Self->Error);
            }
            else SET_ERROR(Self, ERR_Terminate); // Fatal error in attempting to execute the procedure
         }
         else SET_ERROR(Self, ERR_Terminate);
      }

      if (Self->Error IS ERR_Terminate) {
         parasol::Log log(__FUNCTION__);
         log.branch("State changing to HGS_TERMINATED (terminate message received).");
         SetLong(Self, FID_State, HGS_TERMINATED);
      }
   }

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

   if (Self->OutputObjectID) {
      if (Self->ObjectMode IS HOM_DATA_FEED) {
         struct acDataFeed data = {
            .ObjectID = Self->Head.UniqueID,
            .DataType = Self->Datatype,
            .Buffer   = Buffer,
            .Size     = Length
         };
         ActionMsg(AC_DataFeed, Self->OutputObjectID, &data);
      }
      else if (Self->ObjectMode IS HOM_READ_WRITE) {
         acWriteID(Self->OutputObjectID, Buffer, Length);
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

   while ((*String) AND (*String != '=') AND (*String != ',')) String++;
   if (*String IS '=') {
      String++;
      if (*String IS '"') {
         String++;
         for (i=0; (String[i]) AND (String[i] != '"'); i++);

         if (!AllocMemory(i+1, MEM_STRING|MEM_NO_CLEAR, &value, NULL)) {
            CopyMemory(String, value, i);
            value[i] = 0;
         }
         String += i + 1; // Skip "
         while ((*String) AND (*String != ',')) String++;
         if (*String IS ',') String++;
         while ((*String) AND (*String <= 0x20)) String++;
      }
      else {
         for (i=0; (String[i]) AND (String[i] != ','); i++);

         if (!AllocMemory(i+1, MEM_STRING|MEM_NO_CLEAR, &value, NULL)) {
            CopyMemory(String, value, i);
            value[i] = 0;
         }
         String += i;
         if (*String IS ',') String++;
         while ((*String) AND (*String <= 0x20)) String++;
      }
   }

   *Result = value;
   return String - start;
}

//****************************************************************************

static void writehex(HASH Bin, HASHHEX Hex)
{
   ULONG i;

   for (i=0; i < HASHLEN; i++) {
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

static void digest_calc_ha1(objHTTP *Self, HASHHEX SessionKey)
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

static void digest_calc_response(objHTTP *Self, CSTRING Request, CSTRING NonceCount, HASHHEX HA1, HASHHEX HEntity, HASHHEX Response)
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
   while ((*Request) AND (*Request <= 0x20)) Request++;

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

static ERROR write_socket(objHTTP *Self, APTR Buffer, LONG Length, LONG *Result)
{
   parasol::Log log(__FUNCTION__);

   if (Length > 0) {
      //log.trace("Length: %d", Length);

      if (Self->Flags & HTF_DEBUG_SOCKET) {
         log.msg("SOCKET-OUTGOING: LEN: %d", Length);
         for (LONG i=0; i < Length; i++) if ((((UBYTE *)Buffer)[i] < 128) AND (((UBYTE *)Buffer)[i] >= 10)) {
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

static ERROR timeout_manager(objHTTP *Self, LARGE Elapsed, LARGE CurrentTime)
{
   parasol::Log log(__FUNCTION__);

   log.warning("Timeout detected - disconnecting from server (connect %.2fs, data %.2fs).", Self->ConnectTimeout, Self->DataTimeout);
   Self->TimeoutManager = 0;
   SET_ERROR(Self, ERR_TimeOut);
   SetLong(Self, FID_State, HGS_TERMINATED);
   return ERR_Terminate;
}

//****************************************************************************
// Returns ERR_True if the transmission is complete and also sets status to HGS_COMPLETED, otherwise ERR_False.

static ERROR check_incoming_end(objHTTP *Self)
{
   parasol::Log log(__FUNCTION__);

   if (Self->State IS HGS_AUTHENTICATING) return ERR_False;
   if (Self->State >= HGS_COMPLETED) return ERR_True;

   if ((Self->ContentLength != -1) AND (Self->Index >= Self->ContentLength)) {
      log.trace("Transmission over.");
      if (Self->Index > Self->ContentLength) log.warning("Warning: received too much content.");
      SetLong(Self, FID_State, HGS_COMPLETED);
      return ERR_True;
   }
   else {
      log.trace("Transmission continuing.");
      return ERR_False;
   }
}

//****************************************************************************

static LONG set_http_method(objHTTP *Self, STRING Buffer, LONG Size, CSTRING Method)
{
   if ((Self->ProxyServer) AND (!(Self->Flags & HTF_SSL))) {
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

static void socket_feedback(objNetSocket *Socket, LONG State)
{
   parasol::Log log(__FUNCTION__);
   objHTTP *Self = (objHTTP *)CurrentContext();

   if (State IS NTC_CONNECTING) {
      log.msg("Waiting for connection...");

      if (Self->TimeoutManager) UpdateTimer(Self->TimeoutManager, Self->ConnectTimeout);
      else {
         FUNCTION callback;
         SET_FUNCTION_STDC(callback, (APTR)&timeout_manager);
         SubscribeTimer(Self->ConnectTimeout, &callback, &Self->TimeoutManager);
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

      log.msg("Disconnected from socket while in state %s.", clHTTPState[Self->State].Name);

      if (Self->TimeoutManager) { UpdateTimer(Self->TimeoutManager, 0); Self->TimeoutManager = 0; }

      if (Self->Connecting) {
         Self->Connecting = FALSE;

         SET_ERROR(Self, Socket->Error);
         log.branch("Deactivating (connect failure message received).");
         SetField(Self, FID_State, HGS_TERMINATED);
         return;
      }
      else Self->Connecting = FALSE;

      if (Self->State >= HGS_COMPLETED) {
         return;
      }
      else if (Self->State IS HGS_READING_HEADER) {
         SET_ERROR(Self, Socket->Error ? Socket->Error : ERR_Disconnected);
         log.trace("Received broken header as follows:\n%s", Self->Response);
         SetField(Self, FID_State, HGS_TERMINATED);
      }
      else if (Self->State IS HGS_SEND_COMPLETE) {
         // Disconnection on completion of sending data should be no big deal
         SET_ERROR(Self, Socket->Error ? Socket->Error : ERR_Okay);
         SetLong(Self, FID_State, HGS_COMPLETED);
      }
      else if (Self->State IS HGS_SENDING_CONTENT) {
         SET_ERROR(Self, Socket->Error ? Socket->Error : ERR_Disconnected);

         // If the socket is not active, then the disconnection is a result of destroying the object (e.g. due to a redirect).

         log.branch("State changing to TERMINATED due to disconnection.");
         SetLong(Self, FID_State, HGS_TERMINATED);
      }
      else if (Self->State IS HGS_READING_CONTENT) {
         LONG len;

         // Unread data can remain on the socket following disconnection, so try to read anything that's been left.

         if (Self->Chunked) {
            log.traceWarning("Support code required to read chunked data following a disconnected socket.");
         }
         else if ((Self->ContentLength IS -1) OR (Self->Index < Self->ContentLength)) {
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
               SetLong(Self, FID_State, HGS_COMPLETED);
            }
            else {
               SET_ERROR(Self, Socket->Error);
               SetField(Self, FID_State, HGS_TERMINATED);
            }
         }
         else if (Self->Index < Self->ContentLength) {
            log.warning("Disconnected before all content was downloaded (" PF64() " of " PF64() ")", Self->Index, Self->ContentLength);
            SET_ERROR(Self, Socket->Error ? Socket->Error : ERR_Disconnected);
            SetField(Self, FID_State, HGS_TERMINATED);
         }
         else {
            log.trace("Orderly shutdown, received " PF64() " of the expected " PF64() " bytes.", Self->Index, Self->ContentLength);
            SetField(Self, FID_State, HGS_COMPLETED);
         }
      }
      else if (Self->State IS HGS_AUTHENTICATING) {
         if (Self->DialogWindow) {
            // The HTTP socket was closed because the user is taking too long
            // to authenticate with the dialog window.  We will close the socket
            // and create a new one once the user responds to the dialog.

            SetPointer(Self->Socket, FID_Feedback, NULL);
            acFree(Socket);
            Self->Socket = NULL;
            Self->SecurePath = TRUE;
            return;
         }

         SetField(Self, FID_State, HGS_TERMINATED);
      }
   }
   else if (Self->State >= HGS_COMPLETED) {
      // If the state is set to HGS_COMPLETED or HGS_TERMINATED, our code should have returned ERR_Terminate to switch
      // off the socket.  This section is entered if we forgot to do that.

      log.warning("Warning - socket channel was not closed correctly (didn't return ERR_Terminate).");
   }
}

static const FieldArray clFields[] = {
   { "DataTimeout",    FDF_DOUBLE|FDF_RW,          0, NULL, NULL },
   { "ConnectTimeout", FDF_DOUBLE|FDF_RW,          0, NULL, NULL },
   { "Index",          FDF_LARGE|FDF_RW,           0, NULL, NULL }, // Writeable only because we update it using SetField()
   { "ContentLength",  FDF_LARGE|FDF_RW,           0, NULL, NULL },
   { "Size",           FDF_LARGE|FDF_RW,           0, NULL, NULL },
   { "Host",           FDF_STRING|FDF_RI,          0, NULL, (APTR)SET_Host },
   { "Realm",          FDF_STRING|FDF_RW,          0, NULL, (APTR)SET_Realm },
   { "Path",           FDF_STRING|FDF_RW,          0, NULL, (APTR)SET_Path },
   { "OutputFile",     FDF_STRING|FDF_RW,          0, NULL, (APTR)SET_OutputFile },
   { "InputFile",      FDF_STRING|FDF_RW,          0, NULL, (APTR)SET_InputFile },
   { "UserAgent",      FDF_STRING|FDF_RW,          0, NULL, (APTR)SET_UserAgent },
   { "UserData",       FDF_POINTER|FDF_RW,         0, NULL, NULL },
   { "InputObject",    FDF_LONG|FDF_OBJECT|FDF_RW, 0, NULL, NULL },
   { "OutputObject",   FDF_LONG|FDF_OBJECT|FDF_RW, 0, NULL, NULL },
   { "Method",         FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clHTTPMethod, NULL, (APTR)SET_Method },
   { "Port",           FDF_LONG|FDF_RW,            0, NULL, NULL },
   { "ObjectMode",     FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clHTTPObjectMode, NULL, NULL },
   { "Flags",          FDF_LONGFLAGS|FDF_RW,       (MAXINT)&clHTTPFlags, NULL, NULL },
   { "Status",         FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clStatus, NULL, NULL },
   { "Error",          FDF_LONG|FDF_RW,            0, NULL, NULL },
   { "Datatype",       FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clHTTPDatatype, NULL, NULL },
   { "State",          FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clHTTPState, NULL, (APTR)SET_State },
   { "ProxyServer",    FDF_STRING|FDF_RW,          0, NULL, (APTR)SET_ProxyServer },
   { "ProxyPort",      FDF_LONG|FDF_RW,            0, NULL, NULL },
   { "BufferSize",     FDF_LONG|FDF_RW,            0, NULL, (APTR)SET_BufferSize },
   // Virtual fields
   { "AuthCallback",   FDF_FUNCTIONPTR|FDF_RW,   0, (APTR)GET_AuthCallback, (APTR)SET_AuthCallback },
   { "ContentType",    FDF_STRING|FDF_RW,        0, (APTR)GET_ContentType, (APTR)SET_ContentType },
   { "Incoming",       FDF_FUNCTIONPTR|FDF_RW,   0, (APTR)GET_Incoming, (APTR)SET_Incoming },
   { "Location",       FDF_STRING|FDF_RW,        0, (APTR)GET_Location, (APTR)SET_Location },
   { "Outgoing",       FDF_FUNCTIONPTR|FDF_RW,   0, (APTR)GET_Outgoing, (APTR)SET_Outgoing },
   { "RecvBuffer",     FDF_ARRAY|FDF_BYTE|FDF_R, 0, (APTR)GET_RecvBuffer, NULL },
   { "Src",            FDF_STRING|FDF_SYNONYM|FDF_RW, 0, (APTR)GET_Location, (APTR)SET_Location },
   { "StateChanged",   FDF_FUNCTIONPTR|FDF_RW,   0, (APTR)GET_StateChanged, (APTR)SET_StateChanged },
   { "Username",       FDF_STRING|FDF_W,         0, NULL, (APTR)SET_Username },
   { "Password",       FDF_STRING|FDF_W,         0, NULL, (APTR)SET_Password },
   END_FIELD
};

static ERROR create_http_class(void)
{
   return CreateObject(ID_METACLASS, 0, &clHTTP,
      FID_BaseClassID|TLONG,   ID_HTTP,
      FID_ClassVersion|TFLOAT, VER_HTTP,
      FID_Name|TSTR,      "HTTP",
      FID_Category|TLONG, CCF_NETWORK,
      FID_Actions|TPTR,   clHTTPActions,
      FID_Fields|TARRAY,  clFields,
      FID_Size|TLONG,     sizeof(objHTTP),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND);
}

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, MODVERSION_HTTP)
