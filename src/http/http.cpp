/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
HTTP: Provides a complete working implementation of HTTP.

The HTTP class provides a way of interacting with servers that support the HTTP protocol.  Supported HTTP methods
include `GET`, `POST`, `PUT`, `DELETE`, `COPY`, `MOVE`, `MKCOL` and more.  The following features are included:

<list type="bullet">
<li>Handling of errors and HTTP status codes.</li>
<li>Monitoring of the server communication process.</li>
<li>Data transfer monitoring.</li>
<li>Sending and receiving in chunks of data.</li>
<li>Background processing of all HTTP instructions.</li>
<li>Data streaming.</li>
<li>User authentication, either automated or with user login dialogs.</li>
</list>

For information on command execution and a technical overview of HTTP processing, please refer to the #Activate()
action.

<header>Sending Content</>

There are a variety of ways to send content to a server when using methods such as `PUT` and `POST`.  Content can be
sent from objects by setting the #InputObject field.  To send content from files, set the #InputFile field.  To send
string content, use an #InputFile location that starts with `string:` followed by the text to send.

<header>Receiving Content</>

There are three possible methods for content download.  This first example downloads content to a temporary file for
further processing:

<pre>
http = obj.new('http', {
   src        = 'http://www.parasol.ws/index.html',
   method     = 'get',
   outputFile = 'temp:index.html',
   stateChanged = function(HTTP, State)
      if (State == HGS::COMPLETED) then print(content) end
   end
})

http.acActivate()
</pre>

This example uses data feeds to push the downloaded data to another object in text format:

<pre>
doc = obj.new('scintilla')
http = obj.new('http', {
   src        = 'http://www.parasol.ws/index.html',
   method     = 'get',
   dataFeed   = 'text'
   objectMode = 'datafeed'
   outputObject = doc
})
http.acActivate()
</pre>

Note that the target object needs to support the datatype that you specify, or it will ignore the incoming data.  The
default datatype is `RAW` (binary format), but the most commonly supported datatype is `TEXT`.

The third method is to use function callbacks.  Refer to the #Incoming field for further information on receiving
data through callbacks.

<header>Progress Monitoring</>

Progress of a data transfer can be monitored through the #Index field.  If the callback features are not being used for
a data transfer, consider using a timer to read from the #Index periodically.

<header>SSL Support (HTTPS)</>

Secure sockets are supported and can be enabled by setting the #Port to 443 prior to connection, or by using `https://`
in URI strings.  Methods of communication remain unchanged when using SSL, as encrypted communication is handled
transparently.

-END-

For information about the HTTP protocol, please refer to the official protocol web page:

   http://www.w3.org/Protocols/rfc2616/rfc2616.html

*********************************************************************************************************************/

//#define DEBUG
#define PRV_HTTP

#include <stdio.h>
#include <unordered_map>
#include <map>
#include <sstream>
#include <charconv>

#include <parasol/main.h>
#include <parasol/modules/http.h>
//#include <parasol/modules/display.h>
#include <parasol/modules/network.h>
#include <parasol/strings.hpp>
#include "../link/base64.h"

#include "md5.c"

#define CRLF "\r\n"
constexpr int MAX_AUTH_RETRIES = 5;
constexpr int HASHLEN = 16;
constexpr int HASHHEXLEN = 32;
typedef char HASH[HASHLEN];
typedef char HASHHEX[HASHHEXLEN+1];

constexpr int BUFFER_READ_SIZE = 16384;  // Dictates how many bytes are read from the network socket at a time.  Do not make this greater than 64k
constexpr int BUFFER_WRITE_SIZE = 16384; // Dictates how many bytes are written to the network socket at a time.  Do not make this greater than 64k

template <class T> void SET_ERROR(pf::Log log, T http, ERR code) { http->Error = code; log.detail("Set error code %d: %s", LONG(code), GetErrorMsg(code)); }

static ERR create_http_class(void);

JUMPTABLE_CORE
JUMPTABLE_NETWORK
static OBJECTPTR modNetwork = nullptr;
static OBJECTPTR clHTTP = nullptr;
static objProxy *glProxy = nullptr;

extern "C" uint8_t glAuthScript[];
static int glAuthScriptLength;

class extHTTP : public objHTTP {
   public:
   FUNCTION Incoming;
   FUNCTION Outgoing;
   FUNCTION AuthCallback;
   FUNCTION StateChanged;
   std::unordered_map<std::string, std::string> Args;
   std::unordered_map<std::string, std::string> Headers;
   std::string Response;   // Response header buffer
   std::string URI;        // Temporary string, used only when the user reads the URI
   std::string Username;
   std::string Password;
   std::string Realm;
   std::string AuthNonce;
   std::string AuthOpaque;
   std::string AuthPath;    // Equivalent to Path, sans file name
   std::string ContentType;
   std::string AuthQOP;
   std::string AuthAlgorithm;
   std::string AuthCNonce;
   std::vector<char> RecvBuffer; // Receive buffer - aids downloading if HTF::RECVBUFFER is defined
   uint8_t  *WriteBuffer;
   APTR     Buffer;           // Temporary buffer for storing outgoing data
   objFile  *flOutput;
   objFile  *flInput;
   objNetSocket *Socket;    // Socket over which the communication is taking place
   uint8_t  *Chunk;           // Chunk buffer
   int      WriteSize;
   int      WriteOffset;
   int      ChunkSize;        // Size of the chunk buffer
   int      ChunkBuffered;    // Number of bytes buffered, cannot exceed ChunkSize
   int      ChunkLen;         // Length of the current chunk being processed (applies when reading the chunk data)
   int      ChunkIndex;
   TIMER    TimeoutManager;
   int64_t  LastReceipt;      // Last time (microseconds) at which data was received
   int64_t  TotalSent;        // Total number of bytes sent - exists for assisting debugging only
   OBJECTID DialogWindow;
   int      ResponseIndex;    // Next element to write to in 'Buffer'
   int      SearchIndex;      // Current position of the CRLFCRLF search.
   WORD     InputPos;         // File name parsing position in InputFile
   uint8_t  RedirectCount;
   uint8_t  AuthRetries;
   uint16_t Connecting:1;
   uint16_t AuthAttempt:1;
   uint16_t AuthPreset:1;
   uint16_t AuthDigest:1;
   uint16_t SecurePath:1;
   uint16_t Tunneling:1;
   uint16_t Chunked:1;
   uint16_t MultipleInput:1;
   uint16_t ProxyDefined:1;   // TRUE if the ProxyServer has been manually set by the user
};

static ERR HTTP_Activate(extHTTP *);
static ERR HTTP_Deactivate(extHTTP *);
static ERR HTTP_Free(extHTTP *);
static ERR HTTP_GetKey(extHTTP *, struct acGetKey *);
static ERR HTTP_Init(extHTTP *);
static ERR HTTP_NewPlacement(extHTTP *);
static ERR HTTP_SetKey(extHTTP *, struct acSetKey *);
static ERR HTTP_Write(extHTTP *, struct acWrite *);

#include "http_def.c"

//********************************************************************************************************************

static const FieldDef clStatus[] = {
   { "Continue",                 HTS::CONTINUE },
   { "Switching Protocols",      HTS::SWITCH_PROTOCOLS },
   { "Okay",                     HTS::OKAY },
   { "Created",                  HTS::CREATED },
   { "Accepted",                 HTS::ACCEPTED },
   { "Unverified Content",       HTS::UNVERIFIED_CONTENT },
   { "No Content",               HTS::NO_CONTENT },
   { "Reset Content",            HTS::RESET_CONTENT },
   { "Partial Content",          HTS::PARTIAL_CONTENT },
   { "Multiple Choices",         HTS::MULTIPLE_CHOICES },
   { "Moved Permanently",        HTS::MOVED_PERMANENTLY },
   { "Found",                    HTS::FOUND },
   { "See Other",                HTS::SEE_OTHER },
   { "Not Modified",             HTS::NOT_MODIFIED },
   { "Use Proxy",                HTS::USE_PROXY },
   { "Temporary Redirect",       HTS::TEMP_REDIRECT },
   { "Bad Request",              HTS::BAD_REQUEST },
   { "Unauthorised",             HTS::UNAUTHORISED },
   { "Payment Required",         HTS::PAYMENT_REQUIRED },
   { "Forbidden",                HTS::FORBIDDEN },
   { "Not Found",                HTS::NOT_FOUND },
   { "Method Not Allowed",       HTS::METHOD_NOT_ALLOWED },
   { "Not Acceptable",           HTS::NOT_ACCEPTABLE },
   { "Proxy Authentication Required", HTS::PROXY_AUTHENTICATION },
   { "Request Timeout",          HTS::REQUEST_TIMEOUT },
   { "Conflict",                 HTS::CONFLICT },
   { "Gone",                     HTS::GONE },
   { "Length Required",          HTS::LENGTH_REQUIRED },
   { "Precondition Failed",      HTS::PRECONDITION_FAILED },
   { "Request Entity Too Large", HTS::ENTITY_TOO_LARGE },
   { "Request-URI Too Long",     HTS::URI_TOO_LONG },
   { "Unsupported Media Type",   HTS::UNSUPPORTED_MEDIA },
   { "Out of Range",             HTS::OUT_OF_RANGE },
   { "Expectation Failed",       HTS::EXPECTATION_FAILED },
   { "Internal Server Error",    HTS::SERVER_ERROR },
   { "Not Implemented",          HTS::NOT_IMPLEMENTED },
   { "Bad Gateway",              HTS::BAD_GATEWAY },
   { "Service Unavailable",      HTS::SERVICE_UNAVAILABLE },
   { "Gateway Timeout",          HTS::GATEWAY_TIMEOUT },
   { "HTTP Version Unsupported", HTS::VERSION_UNSUPPORTED },
   { nullptr, 0 }
};

//********************************************************************************************************************

static ERR  check_incoming_end(extHTTP *);
static ERR  parse_file(extHTTP *, STRING, LONG);
static void parse_file(extHTTP *, std::ostringstream &);
static ERR  parse_response(extHTTP *, std::string_view);
static ERR  process_data(extHTTP *, APTR, LONG);
static int extract_value(std::string_view, std::string &);
static void writehex(HASH, HASHHEX);
static void digest_calc_ha1(extHTTP *, HASHHEX);
static void digest_calc_response(extHTTP *, std::string, CSTRING, HASHHEX, HASHHEX, HASHHEX);
static ERR  write_socket(extHTTP *, CPTR, LONG, int *);
static void set_http_method(extHTTP *, CSTRING, std::ostringstream &);
static ERR  SET_Path(extHTTP *, CSTRING);
static ERR  SET_Location(extHTTP *, CSTRING);
static ERR  timeout_manager(extHTTP *, int64_t, int64_t);
static void socket_feedback(objNetSocket *, objClientSocket *, NTC);
static ERR  socket_incoming(objNetSocket *);
static ERR  socket_outgoing(objNetSocket *);

/*   if (Object->UID IS Self->DialogWindow) {
      Self->DialogWindow = 0;
      if ((Self->Username) and (Self->Password)) { // Make a second attempt at resolving the HTTP request
         HTTP_Activate(Self, nullptr);
      }
      else {
         log.msg("No username and password provided, deactivating...");
         Self->setCurrentState(HGS::TERMINATED);
      }
   }
*/

//********************************************************************************************************************

INLINE CSTRING GETSTATUS(LONG Code) __attribute__((unused));

INLINE CSTRING GETSTATUS(LONG Code)
{
   for (WORD i=0; clStatus[i].Name; i++) {
      if (clStatus[i].Value IS Code) return clStatus[i].Name;
   }
   return "Unrecognised Status Code";
}

//********************************************************************************************************************

static ERR MODInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (objModule::load("network", &modNetwork, &NetworkBase) != ERR::Okay) return ERR::InitModule;

   glProxy = objProxy::create::global();

   return create_http_class();
}

//********************************************************************************************************************

static ERR MODExpunge(void)
{
   if (clHTTP)     { FreeResource(clHTTP);     clHTTP     = nullptr; }
   if (glProxy)    { FreeResource(glProxy);    glProxy    = nullptr; }
   if (modNetwork) { FreeResource(modNetwork); modNetwork = nullptr; }
   return ERR::Okay;
}

//********************************************************************************************************************

static void notify_free_outgoing(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   ((extHTTP *)CurrentContext())->Outgoing.clear();
}

static void notify_free_state_changed(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   ((extHTTP *)CurrentContext())->StateChanged.clear();
}

static void notify_free_incoming(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   ((extHTTP *)CurrentContext())->Incoming.clear();
}

static void notify_free_auth_callback(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   ((extHTTP *)CurrentContext())->AuthCallback.clear();
}

/*********************************************************************************************************************

-ACTION-
Activate: Executes an HTTP method.

This action starts an HTTP operation against a target server.  Based on the desired #Method, an HTTP request
will be sent to the target server and the action will immediately return whilst the HTTP object will wait for a
response from the server.  If the server fails to respond within the time period indicated by the #ConnectTimeout,
the HTTP object will be deactivated (for further details, refer to the #Deactivate() action).

Successful interpretation of the HTTP request at the server will result in a response being received, followed by file
data (if applicable). The HTTP response code will be stored in the #Status field.  The HTTP object will
automatically parse the response data and store the received values in the HTTP object as variable fields.  It is
possible to be alerted to the complete receipt of a response by listening to the #CurrentState field, or waiting for
the Deactivate action to kick in.

Following a response, incoming data can be managed in a number of ways. It may be streamed to an object referenced by
the #OutputObject field through data feeds.  It can be written to the target object if the #ObjectMode is set to
`READ_WRITE`.  Finally it can be received through C style callbacks if the #Incoming field is set.

On completion of an HTTP request, the #Deactivate() action is called, regardless of the level of success.

-ERRORS-
Okay:   The HTTP get operation was successfully started.
Failed: The HTTP get operation failed immediately for an unspecified reason.
File:   Failed to create a target file if the File field was set.
Write:  Failed to write data to the HTTP @NetSocket.
CreateObject: Failed to create a @NetSocket object.
HostNotFound: DNS resolution of the domain name in the URI failed.
-END-

*********************************************************************************************************************/

static ERR HTTP_Activate(extHTTP *Self)
{
   pf::Log log;
   int i;

   if (!Self->initialised()) return log.warning(ERR::NotInitialised);

   log.branch("Host: %s, Port: %d, Path: %s, Proxy: %s, SSL: %d", Self->Host, Self->Port, Self->Path, Self->ProxyServer, ((Self->Flags & HTF::SSL) != HTF::NIL) ? 1 : 0);

   if (Self->TimeoutManager) { UpdateTimer(Self->TimeoutManager, 0); Self->TimeoutManager = 0; }

   SET_ERROR(log, Self, ERR::Okay);
   Self->ResponseIndex = 0;
   Self->SearchIndex   = 0;
   Self->Index         = 0;
   Self->CurrentState  = HGS::NIL;
   Self->Status        = HTS::NIL;
   Self->TotalSent     = 0;
   Self->Tunneling     = false;
   Self->Flags        &= ~(HTF::MOVED|HTF::REDIRECTED);

   if ((Self->Socket) and (Self->Socket->State IS NTC::DISCONNECTED)) {
      Self->Socket->set(FID_Feedback, (APTR)nullptr);
      FreeResource(Self->Socket);
      Self->Socket = nullptr;
      Self->SecurePath = true;
   }

   Self->Response.clear();
   if (Self->flInput)  { FreeResource(Self->flInput); Self->flInput = nullptr; }
   if (Self->flOutput) { FreeResource(Self->flOutput); Self->flOutput = nullptr; }

   Self->RecvBuffer.resize(0);

   std::ostringstream cmd;

   if ((Self->ProxyServer) and ((Self->Flags & HTF::SSL) != HTF::NIL) and (!Self->Socket)) {
      // SSL tunnelling is required.  Send a CONNECT request to the proxy and
      // then we will follow this up with the actual HTTP requests.

      log.trace("SSL tunnelling is required.");

      cmd << "CONNECT " << Self->Host << ":" << Self->Port << " HTTP/1.1" << CRLF;
      cmd << "Host: " << Self->Host << CRLF;
      cmd << "User-Agent: " << Self->UserAgent << CRLF;
      cmd << "Proxy-Connection: keep-alive" << CRLF;
      cmd << "Connection: keep-alive" << CRLF;

      Self->Tunneling = true;

      //set auth "Proxy-Authorization: Basic [base64::encode $opts(proxyUser):$opts(proxyPass)]"
   }
   else {
      if (Self->Method IS HTM::COPY) {
         // Copies a source (indicated by Path) to a Destination.  The Destination is referenced as an variable field.

         if (Self->Args.contains("Destination")) {
            set_http_method(Self, "COPY", cmd);
            cmd << "Destination: http://" << Self->Host << "/" << Self->Args["Destination"] << CRLF;

            auto & overwrite = Self->Args["Overwrite"];
            if (!overwrite.empty()) {
               // If the overwrite is 'F' then copy will fail if the destination exists
               cmd << "Overwrite: " << overwrite << CRLF;
            }
         }
         else {
            log.warning("HTTP COPY request requires a destination path.");
            SET_ERROR(log, Self, ERR::FieldNotSet);
            return Self->Error;
         }
      }
      else if (Self->Method IS HTM::DELETE) {
         set_http_method(Self, "DELETE", cmd);
      }
      else if (Self->Method IS HTM::GET) {
         set_http_method(Self, "GET", cmd);
         if (Self->Index) cmd << "Range: bytes=" << Self->Index << "-" << CRLF;
      }
      else if (Self->Method IS HTM::LOCK) {

      }
      else if (Self->Method IS HTM::MKCOL) {
         set_http_method(Self, "MKCOL", cmd);
      }
      else if (Self->Method IS HTM::MOVE) {
         // Moves a source (indicated by Path) to a Destination.  The Destination is referenced as a variable field.

         if (Self->Args.contains("Destination")) {
            set_http_method(Self, "MOVE", cmd);
            cmd << "Destination: http://" << Self->Host << "/" << Self->Args["Destination"] << CRLF;
         }
         else {
            log.warning("HTTP MOVE request requires a destination path.");
            SET_ERROR(log, Self, ERR::FieldNotSet);
            return Self->Error;
         }
      }
      else if (Self->Method IS HTM::OPTIONS) {
         if ((!Self->Path) or ((Self->Path[0] IS '*') and (!Self->Path[1]))) {
            cmd << "OPTIONS * HTTP/1.1" << CRLF;
            cmd << "Host: " << Self->Host << CRLF;
            cmd << "User-Agent: " << Self->UserAgent << CRLF;
         }
         else set_http_method(Self, "OPTIONS", cmd);
      }
      else if ((Self->Method IS HTM::POST) or (Self->Method IS HTM::PUT)) {
         log.trace("POST/PUT request being processed.");

         Self->Chunked = false;

         if (((Self->Flags & HTF::NO_HEAD) IS HTF::NIL) and ((Self->SecurePath) or (Self->CurrentState IS HGS::AUTHENTICATING))) {
            log.trace("Executing HEAD statement for authentication.");
            set_http_method(Self, "HEAD", cmd);
            Self->setCurrentState(HGS::AUTHENTICATING);
         }
         else {
            // You can post data from a file source or an object.  In the case of an object it is possible to preset
            // the content-length, although we will attempt to read the amount to transfer from the object's Size
            // field, if supported.  An Outgoing routine can be specified for customised output.
            //
            // To post data from a string, use an InputFile setting as follows:  string:data=to&send

            if (Self->Outgoing.defined()) {
               // User has specified an Outgoing function.  No preparation is necessary.  It is recommended that
               // ContentLength is set beforehand if the amount of data to be sent is known, otherwise
               // the developer should set ContentLength to -1.

            }
            else if (Self->InputFile) {
               if (Self->MultipleInput) {
                  log.trace("Multiple input files detected.");
                  Self->InputPos = 0;
                  parse_file(Self, cmd);
                  Self->flInput = objFile::create::local(fl::Path(cmd.str().c_str()), fl::Flags(FL::READ));
               }
               else Self->flInput = objFile::create::local(fl::Path(Self->InputFile), fl::Flags(FL::READ));

               if (Self->flInput) {
                  Self->Index = 0;
                  if (!Self->Size) {
                     Self->flInput->get(FID_Size, Self->ContentLength); // Use the file's size as ContentLength
                     if (!Self->ContentLength) { // If the file is empty or size is indeterminate then assume nothing is being posted
                        SET_ERROR(log, Self, ERR::NoData);
                        return Self->Error;
                     }
                  }
                  else Self->ContentLength = Self->Size; // Allow the developer to define the ContentLength
               }
               else {
                  SET_ERROR(log, Self, ERR::File);
                  return log.warning(Self->Error);
               }
            }
            else if (Self->InputObjectID) {
               if (!Self->Size) {
                  pf::ScopedObjectLock<Object> input(Self->InputObjectID, 3000);
                  if (input.granted()) {
                     input->get(FID_Size, Self->ContentLength);
                  }
               }
               else Self->ContentLength = Self->Size;
            }
            else {
               log.warning("No data source specified for POST/PUT method.");
               SET_ERROR(log, Self, ERR::FieldNotSet);
               return Self->Error;
            }

            set_http_method(Self, (Self->Method IS HTM::POST) ? "POST" : "PUT", cmd);

            if (Self->ContentLength >= 0) {
               cmd << "Content-length: " << Self->ContentLength << CRLF;
            }
            else {
               log.msg("Content-length not defined for POST/PUT (transfer will be streamed).");

               // Using chunked encoding for post/put will help the server manage streaming
               // uploads, and may even be of help when the content length is known.

               if ((Self->Flags & HTF::RAW) IS HTF::NIL) {
                  cmd << "Transfer-Encoding: chunked" << CRLF;
                  Self->Chunked = true;
               }
            }

            if (!Self->ContentType.empty()) {
               log.trace("User content type: %s", Self->ContentType.c_str());
               cmd << "Content-type: " << Self->ContentType << CRLF;
            }
            else if (Self->Method IS HTM::POST) {
               cmd << "Content-type: application/x-www-form-urlencoded" << CRLF;
            }
            else cmd << "Content-type: application/binary" << CRLF;
         }
      }
      else if (Self->Method IS HTM::UNLOCK) {

      }
      else {
         log.warning("HTTP method no. %d not understood.", LONG(Self->Method));
         SET_ERROR(log, Self, ERR::Failed);
         return Self->Error;
      }

      // Authentication support.  At least one attempt to get the resource (Retries > 0) is required before we can pass the
      // username and password, as it is necessary to be told the method of authentication required (in the case of digest
      // authentication, the nonce value is also required from the server).

      if ((Self->AuthRetries > 0) and (!Self->Username.empty()) and (!Self->Password.empty())) {
         if (Self->AuthDigest) {
            char nonce_count[9] = "00000001";
            HASHHEX HA1, HA2 = "", response;

            Self->AuthCNonce.resize(8);
            for (i=0; i < 8; i++) Self->AuthCNonce[i] = '0' + (rand() % 10);

            digest_calc_ha1(Self, HA1);
            digest_calc_response(Self, cmd.str(), nonce_count, HA1, HA2, response);

            cmd << "Authorization: Digest ";
            cmd << "username=\"" << Self->Username << "\", realm=\"" << Self->Realm << "\", ";
            cmd << "nonce=\"" << Self->AuthNonce << "\", uri=\"/" << Self->Path << "\", ";
            cmd << "qop=" << Self->AuthQOP << ", nc=" << nonce_count << ", ";
            cmd << "cnonce=\"" << Self->AuthCNonce << "\", response=\"" << response << "\"";

            if (!Self->AuthOpaque.empty()) cmd << ", opaque=\"" << Self->AuthOpaque << "\"";

            cmd << CRLF;
         }
         else {
            std::string buffer = Self->Username + ':' + Self->Password;
            std::vector<char> output(buffer.size() * 2);

            pf::BASE64ENCODE state;

            cmd << "Authorization: Basic ";
            auto len = pf::Base64Encode(&state, buffer.c_str(), buffer.size(), output.data(), buffer.length() * 2);
            cmd.write(output.data(), len);
            cmd << CRLF;
         }

         // Clear the password.  This has the effect of resetting the authentication attempt in case the credentials are wrong.

   /*
         for (i=0; Self->Password[i]; i++) Self->Password[i] = 0;
         Self->Password.clear();
   */
      }

      // Add any custom headers

      if (Self->CurrentState != HGS::AUTHENTICATING) {
         for (const auto& [k, v] : Self->Headers) {
            log.trace("Custom header: %s: %s", k.c_str(), v.c_str());
            cmd << k << ": " << v << CRLF;
         }
      }

      if ((Self->Flags & HTF::LOG_ALL) != HTF::NIL) log.msg("HTTP REQUEST HEADER\n%s", cmd.str().c_str());
   }

   cmd << CRLF; // Terminating line feed

   if (!Self->Socket) {
      // If we're using straight SSL without tunnelling, set the SSL flag now so that SSL is automatically engaged on connection.

      auto flags = (((Self->Flags & HTF::SSL) != HTF::NIL) and (!Self->Tunneling)) ? NSF::SSL : NSF::NIL;

      if (!(Self->Socket = objNetSocket::create::local(
            fl::ClientData(Self),
            fl::Incoming(C_FUNCTION(socket_incoming)),
            fl::Feedback(C_FUNCTION(socket_feedback)),
            fl::Flags(flags)))) {
         SET_ERROR(log, Self, ERR::CreateObject);
         return log.warning(Self->Error);
      }
   }
   else {
      log.trace("Re-using existing socket/server connection.");

      Self->Socket->setIncoming(C_FUNCTION(socket_incoming));
      Self->Socket->setFeedback(C_FUNCTION(socket_feedback));
   }

   if (!Self->Tunneling) {
      if (Self->CurrentState != HGS::AUTHENTICATING) {
         if ((Self->Method IS HTM::PUT) or (Self->Method IS HTM::POST)) {
            Self->Socket->setOutgoing(C_FUNCTION(socket_outgoing));
         }
         else Self->Socket->set(FID_Outgoing, (APTR)nullptr);
      }
      else Self->Socket->set(FID_Outgoing, (APTR)nullptr);
   }

   // Buffer the HTTP command string to the socket (will write on connect if we're not connected already).

   auto cstr = cmd.str();
   if (write_socket(Self, cstr.c_str(), cstr.length(), nullptr) IS ERR::Okay) {
      if (Self->Socket->State IS NTC::DISCONNECTED) {
         if (auto result = Self->Socket->connect(Self->ProxyServer ? Self->ProxyServer : Self->Host, Self->ProxyServer ? Self->ProxyPort : Self->Port); result IS ERR::Okay) {
            Self->Connecting = true;

            if (Self->TimeoutManager) UpdateTimer(Self->TimeoutManager, Self->ConnectTimeout);
            else SubscribeTimer(Self->ConnectTimeout, C_FUNCTION(timeout_manager), &Self->TimeoutManager);

            return ERR::Okay;
         }
         else if (result IS ERR::HostNotFound) {
            SET_ERROR(log, Self, ERR::HostNotFound);
            return log.warning(Self->Error);
         }
         else {
            SET_ERROR(log, Self, ERR::Failed);
            return log.warning(Self->Error);
         }
      }
      else return ERR::Okay;
   }
   else {
      SET_ERROR(log, Self, ERR::Write);
      return log.warning(Self->Error);
   }
}

/*********************************************************************************************************************
-ACTION-
Deactivate: Cancels the current download.  Can also signal the end to a download if subscribed.

Following the completion of an HTTP request, the Deactivate() action will be called internally to signal an end to the
process.  By listening to the Deactivate action(), you are given the opportunity to respond to the end of an HTTP request.

If child objects are initialised to the HTTP object, they will be activated automatically.  This feature is provided to
assist scripted usage of the HTTP object.

Active HTTP requests can be manually cancelled by calling the Deactivate() action at any time.
-END-
*********************************************************************************************************************/

static ERR HTTP_Deactivate(extHTTP *Self)
{
   pf::Log log;

   log.branch("Closing connection to server & signaling children.");

   if (Self->CurrentState < HGS::COMPLETED) Self->setCurrentState(HGS::TERMINATED);

   // Closing files is important for dropping the file locks

   if (Self->flInput) { FreeResource(Self->flInput); Self->flInput = nullptr; }
   if (Self->flOutput) { FreeResource(Self->flOutput); Self->flOutput = nullptr; }

   // Free up the outgoing buffer since it is only needed during transfers and will be reallocated as necessary.

   if (Self->Buffer) { FreeResource(Self->Buffer); Self->Buffer = nullptr; }
   if (Self->TimeoutManager) { UpdateTimer(Self->TimeoutManager, 0); Self->TimeoutManager = 0; }

   if (Self->Socket) {
      // The socket object is removed if it has been closed at the server, or if our HTTP object is closing prematurely
      // (for example due to a timeout, or an early call to Deactivate).  This prevents any more incoming data from the
      // server being processed when we don't want it.

      if ((Self->Socket->State IS NTC::DISCONNECTED) or (Self->CurrentState IS HGS::TERMINATED)) {
         log.msg("Terminating socket (disconnected).");
         Self->Socket->set(FID_Feedback, (APTR)nullptr);
         FreeResource(Self->Socket);
         Self->Socket = nullptr;
         Self->SecurePath = true;
      }
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR HTTP_Free(extHTTP *Self)
{
   if (Self->Socket) {
      Self->Socket->set(FID_Feedback, (APTR)nullptr);
      FreeResource(Self->Socket);
      Self->Socket = nullptr;
   }

   if (Self->AuthCallback.isScript()) UnsubscribeAction(Self->AuthCallback.Context, AC::Free);
   if (Self->Incoming.isScript())     UnsubscribeAction(Self->Incoming.Context, AC::Free);
   if (Self->StateChanged.isScript()) UnsubscribeAction(Self->StateChanged.Context, AC::Free);     
   if (Self->Outgoing.isScript())     UnsubscribeAction(Self->Outgoing.Context, AC::Free);

   if (Self->TimeoutManager) { UpdateTimer(Self->TimeoutManager, 0); Self->TimeoutManager = 0; }

   if (Self->flInput)     { FreeResource(Self->flInput);     Self->flInput = nullptr; }
   if (Self->flOutput)    { FreeResource(Self->flOutput);    Self->flOutput = nullptr; }
   if (Self->Buffer)      { FreeResource(Self->Buffer);      Self->Buffer = nullptr; }
   if (Self->Chunk)       { FreeResource(Self->Chunk);       Self->Chunk = nullptr; }
   if (Self->Path)        { FreeResource(Self->Path);        Self->Path = nullptr; }
   if (Self->InputFile)   { FreeResource(Self->InputFile);   Self->InputFile = nullptr; }
   if (Self->OutputFile)  { FreeResource(Self->OutputFile);  Self->OutputFile = nullptr; }
   if (Self->Host)        { FreeResource(Self->Host);        Self->Host = nullptr; }
   if (Self->UserAgent)   { FreeResource(Self->UserAgent);   Self->UserAgent = nullptr; }
   if (Self->ProxyServer) { FreeResource(Self->ProxyServer); Self->ProxyServer = nullptr; }

   for (LONG i=0; i < std::ssize(Self->Password); i++) Self->Password[i] = char(0xff);

   Self->~extHTTP();
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
GetKey: Entries in the HTTP response header can be read as key-values.
-END-
*********************************************************************************************************************/

static ERR HTTP_GetKey(extHTTP *Self, struct acGetKey *Args)
{
   if (!Args) return ERR::NullArgs;

   if (Self->Args.contains(Args->Key)) {
      pf::strcopy(Self->Args[Args->Key], Args->Value, Args->Size);
      return ERR::Okay;
   }

   if (Self->Headers.contains(Args->Key)) {
      pf::strcopy(Self->Headers[Args->Key], Args->Value, Args->Size);
      return ERR::Okay;
   }

   return ERR::UnsupportedField;
}

//********************************************************************************************************************

static ERR HTTP_Init(extHTTP *Self)
{
   pf::Log log;

   if (!Self->ProxyDefined) {
      if (glProxy) {
         if (glProxy->find(Self->Port, true) IS ERR::Okay) {
            if (Self->ProxyServer) FreeResource(Self->ProxyServer);
            Self->ProxyServer = pf::strclone(glProxy->Server);
            Self->ProxyPort   = glProxy->ServerPort; // NB: Default is usually 8080

            log.msg("Using preset proxy server '%s:%d'", Self->ProxyServer, Self->ProxyPort);
         }
      }
      else log.msg("Global proxy configuration object is missing.");
   }
   else log.msg("Proxy pre-defined by user.");

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR HTTP_NewPlacement(extHTTP *Self)
{
   new (Self) extHTTP;
   Self->Error          = ERR::Okay;
   Self->UserAgent      = pf::strclone("Parasol Client");
   Self->DataTimeout    = 5.0;
   Self->ConnectTimeout = 10.0;
   Self->Datatype       = DATA::RAW;
   Self->BufferSize     = 16 * 1024;
   Self->AuthQOP        = "auth";
   Self->AuthAlgorithm  = "md5";
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
SetKey: Options to pass in the HTTP method header can be set as key-values.
-END-
*********************************************************************************************************************/

static ERR HTTP_SetKey(extHTTP *Self, struct acSetKey *Args)
{
   if (!Args) return ERR::NullArgs;

   Self->Headers[Args->Key] = Args->Value;
   return ERR::Okay;
}

//********************************************************************************************************************
// Writing to an HTTP object's outgoing buffer is possible if the Outgoing callback function is active.

static ERR HTTP_Write(extHTTP *Self, struct acWrite *Args)
{
   if ((!Args) or (!Args->Buffer)) return ERR::NullArgs;

   if ((Self->WriteBuffer) and (Self->WriteSize > 0)) {
      int len = Args->Length;
      if (Self->WriteOffset + len > Self->WriteSize) {
         len = Self->WriteSize - Self->WriteOffset;
      }

      if (len > 0) {
         pf::copymem(Args->Buffer, Self->WriteBuffer + Self->WriteOffset, len);
         Self->WriteOffset += len;
         Args->Result = len;
         if (Args->Result != Args->Length) return ERR::LimitedSuccess;
         else return ERR::Okay;
      }
      else {
         Args->Result = 0;
         return ERR::BufferOverflow;
      }
   }
   else return ERR::InvalidState;
}

/*********************************************************************************************************************
-FIELD-
AuthCallback: Private.  This field is reserved for future use.

*********************************************************************************************************************/

static ERR GET_AuthCallback(extHTTP *Self, FUNCTION **Value)
{
   if (Self->AuthCallback.defined()) {
      *Value = &Self->AuthCallback;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_AuthCallback(extHTTP *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->AuthCallback.isScript()) UnsubscribeAction(Self->AuthCallback.Context, AC::Free);
      Self->AuthCallback = *Value;
      if (Self->AuthCallback.isScript()) {
         SubscribeAction(Self->AuthCallback.Context, AC::Free, C_FUNCTION(notify_free_auth_callback));
      }
   }
   else Self->AuthCallback.clear();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
BufferSize: Indicates the preferred buffer size for data operations.

The default buffer size for HTTP data operations is indicated here.  It affects the size of the temporary buffer that
is used for storing outgoing data (`PUT` and `POST` operations).

Note that the actual buffer size may not reflect the exact size that you set here.

*********************************************************************************************************************/

static ERR SET_BufferSize(extHTTP *Self, int Value)
{
   if (Value < 2 * 1024) Value = 2 * 1024;
   Self->BufferSize = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
ConnectTimeout: The initial connection timeout value, measured in seconds.

The timeout for connect operations is specified here.  In the event of a timeout, the HTTP object will be deactivated
and the #Error field will be updated to a value of `ERR::TimeOut`.

The timeout value is measured in seconds.

-FIELD-
ContentLength: The byte length of incoming or outgoing content.

HTTP servers will return a ContentLength value in their response headers when retrieving information.  This value is
defined here once the response header is processed.  The ContentLength may be set to `-1` if the content is being
streamed from the server.

Note that if posting data to a server with an #InputFile or #InputObject as the source, the #Size field will have
priority and override any existing value in ContentLength.  In all other cases the ContentLength can be set
directly and a setting of `-1` can be used for streaming.

-FIELD-
ContentType: Defines the content-type for `PUT` and `POST` methods.

The ContentType should be set prior to sending a `PUT` or `POST` request.  If `NULL`, the default content type for
`POST` methods will be set to `application/x-www-form-urlencoded`.  For `PUT` requests the default of
`application/binary` will be applied.

*********************************************************************************************************************/

static ERR GET_ContentType(extHTTP *Self, STRING *Value)
{
   *Value = Self->ContentType.data();
   return ERR::Okay;
}

static ERR SET_ContentType(extHTTP *Self, CSTRING Value)
{
   if (Value) Self->ContentType.assign(Value);
   else Self->ContentType.clear();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
CurrentState: Indicates the current state of an HTTP object during its interaction with an HTTP server.

The CurrentState is a readable field that tracks the current state of the client in its relationship with the target HTTP
server.  The default state is `READING_HEADER`.  Changes to the state can be monitored through the #StateChanged field.

On completion of an HTTP request, the state will be changed to either `COMPLETED` or `TERMINATED`.

*********************************************************************************************************************/

static ERR SET_CurrentState(extHTTP *Self, HGS Value)
{
   pf::Log log;

   if ((LONG(Value) < 0) or (LONG(Value) >= LONG(HGS::END))) return log.warning(ERR::OutOfRange);

   if ((Self->Flags & HTF::LOG_ALL) != HTF::NIL) log.msg("New State: %s, Currently: %s", clHTTPCurrentState[LONG(Value)].Name, clHTTPCurrentState[LONG(Self->CurrentState)].Name);

   if ((Value >= HGS::COMPLETED) and (Self->CurrentState < HGS::COMPLETED)) {
      Self->CurrentState = Value;
      if (Self->Socket) QueueAction(AC::Deactivate, Self->UID);
   }
   else Self->CurrentState = Value;

   if (Self->StateChanged.defined()) {
      ERR error;
      if (Self->StateChanged.isC()) {
         auto routine = (ERR (*)(extHTTP *, HGS, APTR))Self->StateChanged.Routine;
         error = routine(Self, Self->CurrentState, Self->StateChanged.Meta);
      }
      else if (Self->StateChanged.isScript()) {
         if (sc::Call(Self->StateChanged, std::to_array<ScriptArg>({
            { "HTTP", Self->UID, FD_OBJECTID },
            { "State", LONG(Self->CurrentState) }
         }), error) != ERR::Okay) error = ERR::Terminate;
      }
      else error = ERR::Okay;

      if (error > ERR::ExceptionThreshold) SET_ERROR(log, Self, error);

      if (error IS ERR::Terminate) {
         if (Self->CurrentState IS HGS::SENDING_CONTENT) {
            // Stop sending and expect a response from the server.  If the client doesn't care about the response
            // then a subsequent ERR::Terminate code can be returned on notification of this state change.
            SET_CurrentState(Self, HGS::SEND_COMPLETE);
         }
         else if ((Self->CurrentState != HGS::TERMINATED) and (Self->CurrentState != HGS::COMPLETED)) {
            log.branch("State changing to HGS::COMPLETED (ERR::Terminate received).");
            SET_CurrentState(Self, HGS::COMPLETED);
         }
      }
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
DataTimeout: The data timeout value, relevant when receiving or sending data.

A timeout for send and receive operations is required to prevent prolonged waiting during data transfer operations.
This is essential when interacting with servers that stream data with indeterminate content lengths.  It should be
noted that a timeout does not necessarily indicate failure if the content is being streamed from the server
(#ContentLength is set to `-1`).

In the event of a timeout, the HTTP object will be deactivated and the #Error field will be updated to a value
of `ERR::TimeOut`.

The timeout value is measured in seconds.

-FIELD-
Datatype: The default datatype format to use when passing data to a target object.

When streaming downloaded content to an object, the default datatype is `RAW` (binary mode).  An alternative is to
send the data as `TEXT` or `XML` by changing the Datatype field value.

The receiving object can identify the data as HTTP information by checking the class ID of the sender.

-FIELD-
Error: The error code received for the most recently executed HTTP command.

On completion of an HTTP request, the most appropriate error code will be stored here.  If the request was successful
then the value will be zero (`ERR::Okay`). It should be noted that certain error codes may not necessarily indicate
failure - for instance, an `ERR::TimeOut` error may be received on termination of streamed content.  For genuine HTML
error codes, see the #Status field.

-FIELD-
Flags: Optional flags.

-FIELD-
Host: The targeted HTTP server is specified here, either by name or IP address.

The HTTP server to target for HTTP requests is defined here.  To change the host post-initialisation, set the
#Location.

*********************************************************************************************************************/

static ERR SET_Host(extHTTP *Self, CSTRING Value)
{
   if (Self->Host) { FreeResource(Self->Host); Self->Host = nullptr; }
   Self->Host = pf::strclone(Value);
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Incoming: A callback routine can be defined here for incoming data.

Data can be received from an HTTP request by setting a callback routine in the Incoming field.  The format for the
callback routine is `ERR Function(*HTTP, APTR Data, int Length)`.

If an error code of `ERR::Terminate` is returned by the callback routine, the currently executing HTTP request will be
cancelled.

*********************************************************************************************************************/

static ERR GET_Incoming(extHTTP *Self, FUNCTION **Value)
{
   if (Self->Incoming.defined()) {
      *Value = &Self->Incoming;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_Incoming(extHTTP *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->Incoming.isScript()) UnsubscribeAction(Self->Incoming.Context, AC::Free);
      Self->Incoming = *Value;
      if (Self->Incoming.isScript()) {
         SubscribeAction(Self->Incoming.Context, AC::Free, C_FUNCTION(notify_free_incoming));
      }
   }
   else Self->Incoming.clear();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Index: Indicates download progress in terms of bytes received.

If an HTTP `GET` request is executed, the Index field will reflect the number of bytes that have been received.
This field is updated continuously until either the download is complete or cancelled.

The Index value will always start from zero when downloading, even in resume mode.

The Index field can be monitored for changes so that progress during send and receive transmissions can be tracked.

-FIELD-
InputFile: To upload HTTP content from a file, set a file path here.

HTTP content can be streamed from a source file when a `POST` command is executed. To do so, set the InputFile
field to the file path that contains the source data.  The path is not opened or checked for validity until the
`POST` command is executed by the HTTP object.

An alternative is to set the #InputObject for abstracting the data source.

*********************************************************************************************************************/

static ERR SET_InputFile(extHTTP *Self, CSTRING Value)
{
   pf::Log log;

   log.trace("InputFile: %.80s", Value);

   if (Self->InputFile) { FreeResource(Self->InputFile);  Self->InputFile = nullptr; }

   Self->MultipleInput = false;
   Self->InputPos = 0;
   if ((Value) and (*Value)) {
      Self->InputFile = pf::strclone(Value);

      // Check if the path contains multiple inputs, separated by the pipe symbol.

      for (LONG i=0; Self->InputFile[i]; i++) {
         if (Self->InputFile[i] IS '"') {
            i++;
            while ((Self->InputFile[i]) and (Self->InputFile[i] != '"')) i++;
            if (!Self->InputFile[i]) break;
         }
         else if (Self->InputFile[i] IS '|') {
            Self->MultipleInput = true;
            break;
         }
      }
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
InputObject: Allows data to be sent from an object on execution of a `POST` command.

HTTP content can be streamed from a source object when a `POST` command is executed.  To do so, set the InputObject
to an object that supports the #Read() action.  The provided object ID is not checked for validity until the `POST`
command is executed by the HTTP object.

-FIELD-
Location: A valid HTTP URI must be specified here.

The URI of the HTTP source must be specified here.  The string must start with `http://` or `https://`, followed by the
host name, HTTP path and port number if required. The values mentioned will be broken down and stored in the
#Host, #Path and #Port fields respectively.  Note that if the port is not defined in the URI, the #Port field is reset
to the default (`80` for HTTP or `443` for HTTPS).

An alternative to setting the Location is to set the #Host, #Path and #Port separately.
-END-

*********************************************************************************************************************/

static ERR GET_Location(extHTTP *Self, STRING *Value)
{
   Self->AuthRetries = 0; // Reset the retry counter

   std::ostringstream str;

   if (Self->Port IS 80) str << "http://" << Self->Host << '/' << Self->Path; // http
   else if (Self->Port IS 443) {
      str << "https://" << Self->Host << '/' << Self->Path; // https
      Self->Flags |= HTF::SSL;
   }
   else if (Self->Port IS 21) str << "ftp://" << Self->Host << '/' << Self->Path; // ftp
   else str << "http://" << Self->Host << ':' << Self->Port << '/' << Self->Path;

   Self->URI = str.str();
   *Value = Self->URI.data();
   return ERR::Okay;
}

static ERR SET_Location(extHTTP *Self, CSTRING Value)
{
   pf::Log log;

   if (Self->initialised()) {
      if (Self->TimeoutManager) { UpdateTimer(Self->TimeoutManager, 0); Self->TimeoutManager = 0; }

      // Free the current socket if the entire URI changes

      if (Self->Socket) {
         Self->Socket->set(FID_Feedback, (APTR)nullptr);
         FreeResource(Self->Socket);
         Self->Socket = nullptr;
      }

      log.msg("%s", Value);
   }

   CSTRING str = Value;

   Self->Port = 80;

   if (pf::startswith("http://", str)) str += 7;
   else if (pf::startswith("https://", str)) {
      str += 8;
      Self->Port = 443;
      Self->Flags |= HTF::SSL;
   }

   if (Self->Host) { FreeResource(Self->Host); Self->Host = nullptr; }
   if (Self->Path) { FreeResource(Self->Path); Self->Path = nullptr; }

   // Parse host name

   int len;
   for (len=0; (str[len]) and (str[len] != ':') and (str[len] != '/'); len++);

   if (AllocMemory(len+1, MEM::STRING|MEM::NO_CLEAR, &Self->Host) != ERR::Okay) {
      return ERR::AllocMemory;
   }

   pf::copymem(str, Self->Host, len);
   Self->Host[len] = 0;

   str += len;

   // Parse port number

   if (*str IS ':') {
      str++;
      if (auto i = strtol(str, nullptr, 0)) {
         Self->Port = i;
         if (Self->Port IS 443) Self->Flags |= HTF::SSL;
      }
   }

   while ((*str) and (*str != '/')) str++;

   if (*str) { // Parse absolute path
      SET_Path(Self, str+1);
      return ERR::Okay;
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Method: The HTTP instruction to execute is defined here (defaults to `GET`).

*********************************************************************************************************************/

static ERR SET_Method(extHTTP *Self, HTM Value)
{
   // Changing/Setting the method results in a reset of the variable fields
   Self->Args.clear();
   Self->Headers.clear();
   Self->Method = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
ObjectMode: The access mode used when passing data to a targeted object.

This field is relevant when the #OutputObject field has been set for receiving incoming data. The method of
communication used against the target object can be defined through the ObjectMode. The default setting is
`DATA::FEED`, which passes data through the data feed system (see also the #Datatype to define the type of data being
sent to the object).  The alternative method is `READ_WRITE`, which uses the Write action to send data to the targeted
object.

-FIELD-
Outgoing: Outgoing data can be managed using a function callback if this field is set.

Outgoing data can be managed manually by providing the HTTP object with an outgoing callback routine.  The C prototype
for the callback routine is `ERR Function(*HTTP, APTR Buffer, int BufferSize, int *Result)`.  For Fluid use
`function(HTTP, Buffer, BufferSize)`.

Outgoing content is placed in the `Buffer` address and must not exceed the indicated `BufferSize`.  The total number of
bytes placed in the `Buffer` must be indicated in the Result parameter before the callback routine returns.

If an error code of `ERR::Terminate` is returned by the callback routine, any remaining data will be sent and the transfer
will be treated as having completed successfully.  Use `ERR::TimeOut` if data cannot be returned in a reasonable time
frame.  All other error codes apart from `ERR::Okay` indicate failure.

*********************************************************************************************************************/

static ERR GET_Outgoing(extHTTP *Self, FUNCTION **Value)
{
   if (Self->Outgoing.defined()) {
      *Value = &Self->Outgoing;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_Outgoing(extHTTP *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->Outgoing.isScript()) UnsubscribeAction(Self->Outgoing.Context, AC::Free);
      Self->Outgoing = *Value;
      if (Self->Outgoing.isScript()) {
         SubscribeAction(Self->Outgoing.Context, AC::Free, C_FUNCTION(notify_free_outgoing));
      }
   }
   else Self->Outgoing.clear();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
OutputFile: To download HTTP content to a file, set a file path here.

HTTP content can be streamed to a target file during transfer.  To do so, set the OutputFile field to the destination
file name that will receive data.  If the file already exists, it will be overwritten unless the `RESUME` flag has
been set in the #Flags field.

*********************************************************************************************************************/

static ERR SET_OutputFile(extHTTP *Self, CSTRING Value)
{
   if (Self->OutputFile) { FreeResource(Self->OutputFile); Self->OutputFile = nullptr; }
   Self->OutputFile = pf::strclone(Value);
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
OutputObject: Incoming data can be sent to the object referenced in this field.

HTTP content can be streamed to a target object during incoming data transfers. To do so, set the OutputObject to an
object that supports data feeds and/or the #Write() action. The type of method used for passing data to the
output object is determined by the setting in the #ObjectMode field.

The provided object ID is not checked for validity until the `POST` command is executed by the HTTP object.

-FIELD-
Password: The password to use when authenticating access to the server.

A password may be preset if authorisation is required against the HTTP server for access to a particular resource.
Note that if authorisation is required and no username and password has been preset, the HTTP object will automatically
present a dialog box to the user to request the relevant information.

A `401` status code is returned in the event of an authorisation failure.

*********************************************************************************************************************/

static ERR SET_Password(extHTTP *Self, CSTRING Value)
{
   Self->Password.assign(Value);
   Self->AuthPreset = true;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Path: The HTTP path targeted at the host server.

The path to target at the host server is specified here.  If no path is set, the server root will be targeted.  It is
not necessary to set the path if one has been specified in the #Location.

If spaces are discovered in the path, they will be converted to the `%20` HTTP escape code automatically.  No other
automatic conversions are operated when setting the Path field.

*********************************************************************************************************************/

static ERR SET_Path(extHTTP *Self, CSTRING Value)
{
   Self->AuthRetries = 0; // Reset the retry counter

   if (Self->Path) { FreeResource(Self->Path); Self->Path = nullptr; }

   if (!Value) return ERR::Okay;

   while (*Value IS '/') Value++; // Skip '/' prefix

   int len = 0;
   for (LONG i=0; Value[i]; i++) { // Compute the length with consideration to escape codes
      if (Value[i] IS ' ') len += 3; // '%20'
      else len++;
   }

   if (AllocMemory(len+1, MEM::STRING|MEM::NO_CLEAR, &Self->Path) IS ERR::Okay) {
      int len = 0;
      for (LONG i=0; Value[i]; i++) {
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

      auto pview = std::string_view(Self->Path, len);
      auto folder_len = pview.find_last_of('/');
      if (folder_len IS std::string::npos) folder_len = 0;

      Self->SecurePath = true;
      if (!Self->AuthPath.empty()) {
         if (Self->AuthPath.size() IS folder_len) {
            pview.remove_suffix(pview.size() - folder_len);
            if (pview == Self->AuthPath) { // No change to the current path
               Self->SecurePath = false;
            }
         }
      }

      Self->AuthPath.assign(Self->Path, folder_len);
      return ERR::Okay;
   }
   else return ERR::AllocMemory;
}

/*********************************************************************************************************************

-FIELD-
Port: The HTTP port to use when targeting a server.

The Port to target at the HTTP server is defined here.  The default for HTTP requests is port `80`.  To change the port
number, set the #Location.

-FIELD-
ProxyPort: The port to use when communicating with the proxy server.

If the ProxyServer field has been set, the ProxyPort must be set to the port number used by the proxy server for all
requests.  By default the ProxyPort is set to `8080` which is commonly used for proxy communications.

-FIELD-
ProxyServer: The targeted HTTP server is specified here, either by name or IP address.

If a proxy server will receive the HTTP request, set the name or IP address of the server here.  To specify the port
that the proxy server uses to receive requests, see the #ProxyPort field.

*********************************************************************************************************************/

static ERR SET_ProxyServer(extHTTP *Self, CSTRING Value)
{
   if (Self->ProxyServer) { FreeResource(Self->ProxyServer); Self->ProxyServer = nullptr; }
   if ((Value) and (Value[0])) Self->ProxyServer = pf::strclone(Value);
   Self->ProxyDefined = true;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Realm: Identifies the realm during HTTP authentication.

During the user authentication process, a realm name may be returned by the HTTP server and this will be reflected
here.

*********************************************************************************************************************/

static ERR GET_Realm(extHTTP *Self, CSTRING *Value)
{
   if (Self->Realm.empty()) *Value = nullptr;
   else *Value = Self->Realm.c_str();
   return ERR::Okay;
}

static ERR SET_Realm(extHTTP *Self, CSTRING Value)
{
   if (Value) Self->Realm.assign(Value);
   else Self->Realm.clear();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
RecvBuffer: Refers to a data buffer that is used to store all incoming content.

If the `RECV_BUFFER` flag is set, all content received from the HTTP server will be stored in a managed buffer
that is referred to by this field.  This field can be read at any time.  It will be set to `NULL` if no data has been
received. The buffer address and all content is reset whenever the HTTP object is activated.

The buffer is null-terminated if you wish to use it as a string.

*********************************************************************************************************************/

static ERR GET_RecvBuffer(extHTTP *Self, uint8_t **Value, int *Elements)
{
   *Value = (uint8_t *)Self->RecvBuffer.data();
   *Elements = Self->RecvBuffer.size();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Size: Set this field to define the length of a data transfer when issuing a `POST` command.

Prior to the execution of a `POST` command it is recommended that you set the Size field to explicitly define the
length of the data transfer.  If this field is not set, the HTTP object will attempt to determine the byte size of
the transfer by reading the size from the source file or object.

-FIELD-
StateChanged: A callback routine can be defined here for monitoring changes to the HTTP state.

Define a callback routine in StateChanged in order to receive notifications of any change to the #CurrentState of an
HTTP object.  The format for the routine is `ERR Function(*HTTP, HGS State)`.

If an error code of `ERR::Terminate` is returned by the callback routine, the currently executing HTTP request will be
cancelled.

*********************************************************************************************************************/

static ERR GET_StateChanged(extHTTP *Self, FUNCTION **Value)
{
   if (Self->StateChanged.defined()) {
      *Value = &Self->StateChanged;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_StateChanged(extHTTP *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->StateChanged.isScript()) UnsubscribeAction(Self->StateChanged.Context, AC::Free);
      Self->StateChanged = *Value;
      if (Self->StateChanged.isScript()) {
         SubscribeAction(Self->StateChanged.Context, AC::Free, C_FUNCTION(notify_free_state_changed));
      }
   }
   else Self->StateChanged.clear();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Status: Indicates the HTTP status code returned on completion of an HTTP request.

-FIELD-
UserAgent: Specifies the name of the user-agent string that is sent in HTTP requests.

This field describe the `user-agent` value that will be sent in HTTP requests.  The default value is `Parasol Client`.

*********************************************************************************************************************/

static ERR SET_UserAgent(extHTTP *Self, CSTRING Value)
{
   if (Self->UserAgent) { FreeResource(Self->UserAgent); Self->UserAgent = nullptr; }
   Self->UserAgent = pf::strclone(Value);
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
ClientData: This unused field value can be used for storing private data.

-FIELD-
Username: The username to use when authenticating access to the server.

A username can be preset before executing an HTTP method against a secure server zone.  The supplied credentials will
only be passed to the HTTP server if it asks for authorisation.  The username provided should be accompanied by a
#Password.

In the event that a username or password is not supplied, or if the supplied credentials are invalid, the user will be
presented with a dialog box and asked to enter the correct username and password.
-END-

*********************************************************************************************************************/

static ERR SET_Username(extHTTP *Self, CSTRING Value)
{
   Self->Username.assign(Value);
   return ERR::Okay;
}

//********************************************************************************************************************

#include "http_functions.cpp"

static const FieldArray clFields[] = {
   { "DataTimeout",    FDF_DOUBLE|FDF_RW },
   { "ConnectTimeout", FDF_DOUBLE|FDF_RW },
   { "Index",          FDF_INT64|FDF_RW }, // Writeable only because we update it using SetField()
   { "ContentLength",  FDF_INT64|FDF_RW },
   { "Size",           FDF_INT64|FDF_RW },
   { "Host",           FDF_STRING|FDF_RI, nullptr, SET_Host },
   { "Path",           FDF_STRING|FDF_RW, nullptr, SET_Path },
   { "OutputFile",     FDF_STRING|FDF_RW, nullptr, SET_OutputFile },
   { "InputFile",      FDF_STRING|FDF_RW, nullptr, SET_InputFile },
   { "UserAgent",      FDF_STRING|FDF_RW, nullptr, SET_UserAgent },
   { "ClientData",     FDF_POINTER|FDF_RW },
   { "InputObject",    FDF_OBJECTID|FDF_RW },
   { "OutputObject",   FDF_OBJECTID|FDF_RW },
   { "Method",         FDF_INT|FDF_LOOKUP|FDF_RW, nullptr, SET_Method, &clHTTPMethod },
   { "Port",           FDF_INT|FDF_RW },
   { "ObjectMode",     FDF_INT|FDF_LOOKUP|FDF_RW, nullptr, nullptr, &clHTTPObjectMode },
   { "Flags",          FDF_INTFLAGS|FDF_RW, nullptr, nullptr, &clHTTPFlags },
   { "Status",         FDF_INT|FDF_LOOKUP|FDF_RW, nullptr, nullptr, &clStatus },
   { "Error",          FDF_INT|FDF_RW },
   { "Datatype",       FDF_INT|FDF_LOOKUP|FDF_RW, nullptr, nullptr, &clHTTPDatatype },
   { "CurrentState",   FDF_INT|FDF_LOOKUP|FDF_RW, nullptr, SET_CurrentState, &clHTTPCurrentState },
   { "ProxyServer",    FDF_STRING|FDF_RW, nullptr, SET_ProxyServer },
   { "ProxyPort",      FDF_INT|FDF_RW },
   { "BufferSize",     FDF_INT|FDF_RW, nullptr, SET_BufferSize },
   // Virtual fields
   { "AuthCallback",   FDF_FUNCTIONPTR|FDF_RW,   GET_AuthCallback, SET_AuthCallback },
   { "ContentType",    FDF_STRING|FDF_RW,        GET_ContentType, SET_ContentType },
   { "Incoming",       FDF_FUNCTIONPTR|FDF_RW,   GET_Incoming, SET_Incoming },
   { "Location",       FDF_STRING|FDF_RW,        GET_Location, SET_Location },
   { "Outgoing",       FDF_FUNCTIONPTR|FDF_RW,   GET_Outgoing, SET_Outgoing },
   { "Realm",          FDF_STRING|FDF_RW,        GET_Realm, SET_Realm },
   { "RecvBuffer",     FDF_ARRAY|FDF_BYTE|FDF_R, GET_RecvBuffer },
   { "Src",            FDF_STRING|FDF_SYNONYM|FDF_RW, GET_Location, SET_Location },
   { "StateChanged",   FDF_FUNCTIONPTR|FDF_RW,   GET_StateChanged, SET_StateChanged },
   { "Username",       FDF_STRING|FDF_W,         nullptr, SET_Username },
   { "Password",       FDF_STRING|FDF_W,         nullptr, SET_Password },
   END_FIELD
};

//********************************************************************************************************************

static ERR create_http_class(void)
{
   clHTTP = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::HTTP),
      fl::ClassVersion(VER_HTTP),
      fl::Name("HTTP"),
      fl::Category(CCF::NETWORK),
      fl::Actions(clHTTPActions),
      fl::Fields(clFields),
      fl::Size(sizeof(extHTTP)),
      fl::Path(MOD_PATH));

   return clHTTP ? ERR::Okay : ERR::AddClass;
}

//********************************************************************************************************************

PARASOL_MOD(MODInit, nullptr, nullptr, MODExpunge, MOD_IDL, nullptr)
extern "C" struct ModHeader * register_http_module() { return &ModHeader; }
