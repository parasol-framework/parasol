#pragma once

// Name:      http.h
// Copyright: Paul Manias © 2005-2024
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_HTTP (1)

#include <parasol/modules/network.h>

class objHTTP;

// Output mode.

enum class HOM : LONG {
   NIL = 0,
   DATA_FEED = 0,
   READ_WRITE = 1,
   READ = 1,
   WRITE = 1,
};

// Options for defining an HTTP object's state.

enum class HGS : LONG {
   NIL = 0,
   READING_HEADER = 0,
   AUTHENTICATING = 1,
   AUTHENTICATED = 2,
   SENDING_CONTENT = 3,
   SEND_COMPLETE = 4,
   READING_CONTENT = 5,
   COMPLETED = 6,
   TERMINATED = 7,
   END = 8,
};

// The HTTP Method to use when the object is activated.

enum class HTM : LONG {
   NIL = 0,
   GET = 0,
   POST = 1,
   PUT = 2,
   HEAD = 3,
   DELETE = 4,
   TRACE = 5,
   MK_COL = 6,
   B_COPY = 7,
   B_DELETE = 8,
   B_MOVE = 9,
   B_PROP_FIND = 10,
   B_PROP_PATCH = 11,
   COPY = 12,
   LOCK = 13,
   MOVE = 14,
   NOTIFY = 15,
   OPTIONS = 16,
   POLL = 17,
   PROP_FIND = 18,
   PROP_PATCH = 19,
   SEARCH = 20,
   SUBSCRIBE = 21,
   UNLOCK = 22,
   UNSUBSCRIBE = 23,
};

// HTTP status codes

enum class HTS : LONG {
   NIL = 0,
   CONTINUE = 100,
   SWITCH_PROTOCOLS = 101,
   OKAY = 200,
   CREATED = 201,
   ACCEPTED = 202,
   UNVERIFIED_CONTENT = 203,
   NO_CONTENT = 204,
   RESET_CONTENT = 205,
   PARTIAL_CONTENT = 206,
   MULTIPLE_CHOICES = 300,
   MOVED_PERMANENTLY = 301,
   FOUND = 302,
   SEE_OTHER = 303,
   NOT_MODIFIED = 304,
   USE_PROXY = 305,
   TEMP_REDIRECT = 307,
   BAD_REQUEST = 400,
   UNAUTHORISED = 401,
   PAYMENT_REQUIRED = 402,
   FORBIDDEN = 403,
   NOT_FOUND = 404,
   METHOD_NOT_ALLOWED = 405,
   NOT_ACCEPTABLE = 406,
   PROXY_AUTHENTICATION = 407,
   REQUEST_TIMEOUT = 408,
   CONFLICT = 409,
   GONE = 410,
   LENGTH_REQUIRED = 411,
   PRECONDITION_FAILED = 412,
   ENTITY_TOO_LARGE = 413,
   URI_TOO_LONG = 414,
   UNSUPPORTED_MEDIA = 415,
   OUT_OF_RANGE = 416,
   EXPECTATION_FAILED = 417,
   SERVER_ERROR = 500,
   NOT_IMPLEMENTED = 501,
   BAD_GATEWAY = 502,
   SERVICE_UNAVAILABLE = 503,
   GATEWAY_TIMEOUT = 504,
   VERSION_UNSUPPORTED = 505,
};

// HTTP flags

enum class HTF : ULONG {
   NIL = 0,
   RESUME = 0x00000001,
   MESSAGE = 0x00000002,
   MOVED = 0x00000004,
   REDIRECTED = 0x00000008,
   NO_HEAD = 0x00000010,
   NO_DIALOG = 0x00000020,
   RAW = 0x00000040,
   DEBUG_SOCKET = 0x00000080,
   RECV_BUFFER = 0x00000100,
   LOG_ALL = 0x00000200,
   SSL = 0x00000400,
};

DEFINE_ENUM_FLAG_OPERATORS(HTF)

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
   HTM      Method;          // The HTTP instruction to execute is defined here (defaults to GET).
   LONG     Port;            // The HTTP port to use when targeting a server.
   HOM      ObjectMode;      // The access mode used when passing data to a targeted object.
   HTF      Flags;           // Optional flags.
   HTS      Status;          // Indicates the HTTP status code returned on completion of an HTTP request.
   ERROR    Error;           // The error code received for the most recently executed HTTP command.
   DATA     Datatype;        // The default datatype format to use when passing data to a target object.
   HGS      CurrentState;    // Indicates the current state of an HTTP object during its interaction with an HTTP server.
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

   template <class T> inline ERROR setHost(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[21];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setRealm(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setPath(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[23];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setOutputFile(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[2];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setInputFile(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setUserAgent(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[31];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setUserData(APTR Value) {
      this->UserData = Value;
      return ERR_Okay;
   }

   inline ERROR setInputObject(OBJECTID Value) {
      this->InputObjectID = Value;
      return ERR_Okay;
   }

   inline ERROR setOutputObject(OBJECTID Value) {
      this->OutputObjectID = Value;
      return ERR_Okay;
   }

   inline ERROR setMethod(const HTM Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setPort(const LONG Value) {
      this->Port = Value;
      return ERR_Okay;
   }

   inline ERROR setObjectMode(const HOM Value) {
      this->ObjectMode = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const HTF Value) {
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setStatus(const HTS Value) {
      this->Status = Value;
      return ERR_Okay;
   }

   inline ERROR setError(const ERROR Value) {
      this->Error = Value;
      return ERR_Okay;
   }

   inline ERROR setDatatype(const DATA Value) {
      this->Datatype = Value;
      return ERR_Okay;
   }

   inline ERROR setCurrentState(const HGS Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[17];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   template <class T> inline ERROR setProxyServer(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[34];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
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

   template <class T> inline ERROR setContentType(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[33];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setIncoming(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[19];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   template <class T> inline ERROR setLocation(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[20];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
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

   template <class T> inline ERROR setUsername(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[38];
      return field->WriteValue(target, field, 0x08800200, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setPassword(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, 0x08800200, to_cstring(Value), 1);
   }

};

