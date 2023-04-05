#pragma once

// Name:      http.h
// Copyright: Paul Manias © 2005-2023
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_HTTP (1)

#include <parasol/modules/network.h>

class objHTTP;

// Output mode.

#define HOM_DATA_FEED 0
#define HOM_READ_WRITE 1
#define HOM_READ 1
#define HOM_WRITE 1

// Options for defining an HTTP object's state.

#define HGS_READING_HEADER 0
#define HGS_AUTHENTICATING 1
#define HGS_AUTHENTICATED 2
#define HGS_SENDING_CONTENT 3
#define HGS_SEND_COMPLETE 4
#define HGS_READING_CONTENT 5
#define HGS_COMPLETED 6
#define HGS_TERMINATED 7
#define HGS_END 8

// The HTTP Method to use when the object is activated.

#define HTM_GET 0
#define HTM_POST 1
#define HTM_PUT 2
#define HTM_HEAD 3
#define HTM_DELETE 4
#define HTM_TRACE 5
#define HTM_MK_COL 6
#define HTM_B_COPY 7
#define HTM_B_DELETE 8
#define HTM_B_MOVE 9
#define HTM_B_PROP_FIND 10
#define HTM_B_PROP_PATCH 11
#define HTM_COPY 12
#define HTM_LOCK 13
#define HTM_MOVE 14
#define HTM_NOTIFY 15
#define HTM_OPTIONS 16
#define HTM_POLL 17
#define HTM_PROP_FIND 18
#define HTM_PROP_PATCH 19
#define HTM_SEARCH 20
#define HTM_SUBSCRIBE 21
#define HTM_UNLOCK 22
#define HTM_UNSUBSCRIBE 23

// HTTP status codes

#define HTS_CONTINUE 100
#define HTS_SWITCH_PROTOCOLS 101
#define HTS_OKAY 200
#define HTS_CREATED 201
#define HTS_ACCEPTED 202
#define HTS_UNVERIFIED_CONTENT 203
#define HTS_NO_CONTENT 204
#define HTS_RESET_CONTENT 205
#define HTS_PARTIAL_CONTENT 206
#define HTS_MULTIPLE_CHOICES 300
#define HTS_MOVED_PERMANENTLY 301
#define HTS_FOUND 302
#define HTS_SEE_OTHER 303
#define HTS_NOT_MODIFIED 304
#define HTS_USE_PROXY 305
#define HTS_TEMP_REDIRECT 307
#define HTS_BAD_REQUEST 400
#define HTS_UNAUTHORISED 401
#define HTS_PAYMENT_REQUIRED 402
#define HTS_FORBIDDEN 403
#define HTS_NOT_FOUND 404
#define HTS_METHOD_NOT_ALLOWED 405
#define HTS_NOT_ACCEPTABLE 406
#define HTS_PROXY_AUTHENTICATION 407
#define HTS_REQUEST_TIMEOUT 408
#define HTS_CONFLICT 409
#define HTS_GONE 410
#define HTS_LENGTH_REQUIRED 411
#define HTS_PRECONDITION_FAILED 412
#define HTS_ENTITY_TOO_LARGE 413
#define HTS_URI_TOO_LONG 414
#define HTS_UNSUPPORTED_MEDIA 415
#define HTS_OUT_OF_RANGE 416
#define HTS_EXPECTATION_FAILED 417
#define HTS_SERVER_ERROR 500
#define HTS_NOT_IMPLEMENTED 501
#define HTS_BAD_GATEWAY 502
#define HTS_SERVICE_UNAVAILABLE 503
#define HTS_GATEWAY_TIMEOUT 504
#define HTS_VERSION_UNSUPPORTED 505

// HTTP flags

#define HTF_RESUME 0x00000001
#define HTF_MESSAGE 0x00000002
#define HTF_MOVED 0x00000004
#define HTF_REDIRECTED 0x00000008
#define HTF_NO_HEAD 0x00000010
#define HTF_NO_DIALOG 0x00000020
#define HTF_RAW 0x00000040
#define HTF_DEBUG_SOCKET 0x00000080
#define HTF_RECV_BUFFER 0x00000100
#define HTF_DEBUG 0x00000200
#define HTF_SSL 0x00000400

// HTTP class definition

#define VER_HTTP (1.000000)

class objHTTP : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_HTTP;
   static constexpr CSTRING CLASS_NAME = "HTTP";

   using create = pf::Create<objHTTP>;

   DOUBLE   DataTimeout;     // The data timeout value, relevant when receiving or sending data.
   DOUBLE   ConnectTimeout;  // The initial connection timeout value, measured in seconds.
   LARGE    Index;           // Indicates download progress in terms of bytes received.
   LARGE    ContentLength;   // The byte length of incoming or outgoing content.
   LARGE    Size;            // Set this field to define the length of a data transfer when issuing a POST command.
   STRING   Host;            // The targeted HTTP server is specified here, either by name or IP address.
   STRING   Realm;           // Identifies the realm during HTTP authentication.
   STRING   Path;            // The HTTP path targeted at the host server.
   STRING   OutputFile;      // To download HTTP content to a file, set a file path here.
   STRING   InputFile;       // To upload HTTP content from a file, set a file path here.
   STRING   UserAgent;       // Specifies the name of the user-agent string that is sent in HTTP requests.
   APTR     UserData;        // An unused field value that is useful for storing private data.
   OBJECTID InputObjectID;   // Allows data to be sent from an object on execution of a POST command.
   OBJECTID OutputObjectID;  // Incoming data can be sent to the object referenced in this field.
   LONG     Method;          // The HTTP instruction to execute is defined here (defaults to GET).
   LONG     Port;            // The HTTP port to use when targeting a server.
   LONG     ObjectMode;      // The access mode used when passing data to a targeted object.
   LONG     Flags;           // Optional flags.
   LONG     Status;          // Indicates the HTTP status code returned on completion of an HTTP request.
   ERROR    Error;           // The error code received for the most recently executed HTTP command.
   LONG     Datatype;        // The default datatype format to use when passing data to a target object.
   LONG     CurrentState;    // Indicates the current state of an HTTP object during its interaction with an HTTP server.
   STRING   ProxyServer;     // The targeted HTTP server is specified here, either by name or IP address.
   LONG     ProxyPort;       // The port to use when communicating with the proxy server.
   LONG     BufferSize;      // Indicates the preferred buffer size for data operations.

   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR deactivate() { return Action(AC_Deactivate, this, NULL); }
   inline ERROR getVar(CSTRING FieldName, STRING Buffer, LONG Size) {
      struct acGetVar args = { FieldName, Buffer, Size };
      ERROR error = Action(AC_GetVar, this, &args);
      if ((error) and (Buffer)) Buffer[0] = 0;
      return error;
   }
   inline ERROR init() { return InitObject(this); }
   inline ERROR acSetVar(CSTRING FieldName, CSTRING Value) {
      struct acSetVar args = { FieldName, Value };
      return Action(AC_SetVar, this, &args);
   }
   inline ERROR write(CPTR Buffer, LONG Size, LONG *Result = NULL) {
      ERROR error;
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (!(error = Action(AC_Write, this, &write))) {
         if (Result) *Result = write.Result;
      }
      else if (Result) *Result = 0;
      return error;
   }
   inline ERROR write(std::string Buffer, LONG *Result = NULL) {
      ERROR error;
      struct acWrite write = { (BYTE *)Buffer.c_str(), LONG(Buffer.size()) };
      if (!(error = Action(AC_Write, this, &write))) {
         if (Result) *Result = write.Result;
      }
      else if (Result) *Result = 0;
      return error;
   }
   inline LONG writeResult(CPTR Buffer, LONG Size) {
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (!Action(AC_Write, this, &write)) return write.Result;
      else return 0;
   }

   // Customised field setting

   inline ERROR setDataTimeout(const DOUBLE Value) {
      this->DataTimeout = Value;
      return ERR_Okay;
   }

   inline ERROR setConnectTimeout(const DOUBLE Value) {
      this->ConnectTimeout = Value;
      return ERR_Okay;
   }

   inline ERROR setIndex(const LARGE Value) {
      this->Index = Value;
      return ERR_Okay;
   }

   inline ERROR setContentLength(const LARGE Value) {
      this->ContentLength = Value;
      return ERR_Okay;
   }

   inline ERROR setSize(const LARGE Value) {
      this->Size = Value;
      return ERR_Okay;
   }

   inline ERROR setHost(STRING Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[21];
      return field->WriteValue(target, field, 0x08800500, Value, 1);
   }

   inline ERROR setRealm(STRING Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, 0x08800300, Value, 1);
   }

   inline ERROR setPath(STRING Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[23];
      return field->WriteValue(target, field, 0x08800300, Value, 1);
   }

   inline ERROR setOutputFile(STRING Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[2];
      return field->WriteValue(target, field, 0x08800300, Value, 1);
   }

   inline ERROR setInputFile(STRING Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x08800300, Value, 1);
   }

   inline ERROR setUserAgent(STRING Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[31];
      return field->WriteValue(target, field, 0x08800300, Value, 1);
   }

   inline ERROR setUserData(APTR Value) {
      this->UserData = Value;
      return ERR_Okay;
   }

   inline ERROR setInputObject(const OBJECTID Value) {
      this->InputObjectID = Value;
      return ERR_Okay;
   }

   inline ERROR setOutputObject(const OBJECTID Value) {
      this->OutputObjectID = Value;
      return ERR_Okay;
   }

   inline ERROR setMethod(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setPort(const LONG Value) {
      this->Port = Value;
      return ERR_Okay;
   }

   inline ERROR setObjectMode(const LONG Value) {
      this->ObjectMode = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const LONG Value) {
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setStatus(const LONG Value) {
      this->Status = Value;
      return ERR_Okay;
   }

   inline ERROR setError(const ERROR Value) {
      this->Error = Value;
      return ERR_Okay;
   }

   inline ERROR setDatatype(const LONG Value) {
      this->Datatype = Value;
      return ERR_Okay;
   }

   inline ERROR setCurrentState(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[17];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setProxyServer(STRING Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[34];
      return field->WriteValue(target, field, 0x08800300, Value, 1);
   }

   inline ERROR setProxyPort(const LONG Value) {
      this->ProxyPort = Value;
      return ERR_Okay;
   }

   inline ERROR setBufferSize(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[32];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setAuthCallback(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[26];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERROR setContentType(STRING Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[33];
      return field->WriteValue(target, field, 0x08800300, Value, 1);
   }

   inline ERROR setIncoming(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[19];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERROR setLocation(STRING Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[20];
      return field->WriteValue(target, field, 0x08800300, Value, 1);
   }

   inline ERROR setOutgoing(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERROR setStateChanged(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[3];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERROR setUsername(STRING Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[38];
      return field->WriteValue(target, field, 0x08800200, Value, 1);
   }

   inline ERROR setPassword(STRING Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, 0x08800200, Value, 1);
   }

};

