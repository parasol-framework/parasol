#ifndef MODULES_HTTP
#define MODULES_HTTP 1

// Name:      http.h
// Copyright: Paul Manias © 2005-2022
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_HTTP (1)

#ifndef MODULES_NETWORK_H
#include <parasol/modules/network.h>
#endif

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

typedef class rkHTTP : public BaseClass {
   public:
   DOUBLE   DataTimeout;     // Timeout for receiving data, measured in seconds
   DOUBLE   ConnectTimeout;  // Timeout for initial connection, measured in seconds
   LARGE    Index;           // Current read/write index, relative to content length so always starts from zero
   LARGE    ContentLength;   // Content length as reported in the content length response field, -1 if streamed or unknown
   LARGE    Size;            // Content size to use when uploading data
   STRING   Host;            // Host / Domain
   STRING   Realm;
   STRING   Path;            // Path for file retrieval
   STRING   OutputFile;      // Target file for downloaded content
   STRING   InputFile;       // Source file for uploads
   STRING   UserAgent;       // User agent to pass in the HTTP headers
   APTR     UserData;        // User-specific data for the HTTP object
   OBJECTID InputObjectID;   // An object to send HTTP content
   OBJECTID OutputObjectID;  // An object to receive HTTP content
   LONG     Method;          // HTTP Request: GET, HEAD, POST, PUT..
   LONG     Port;            // Socket port number, usually port 80
   LONG     ObjectMode;      // Either DataFeed or Write mode
   LONG     Flags;           // Optional flags
   LONG     Status;          // Status code recieved in the HTML response header
   ERROR    Error;           // Result of the operation
   LONG     Datatype;        // Datatype to use when sending HTTP data to a target object
   LONG     CurrentState;    // Current state of the http get operation
   STRING   ProxyServer;     // If using a proxy server, this is the name or IP address of the server
   LONG     ProxyPort;       // The port of the proxy server
   LONG     BufferSize;      // Preferred buffer size for things like outgoing operations (sending data)

#ifdef PRV_HTTP
   FUNCTION Incoming;
   FUNCTION Outgoing;
   FUNCTION AuthCallback;
   FUNCTION StateChanged;
   std::unordered_map<std::string, std::string> *Args;
   std::unordered_map<std::string, std::string> *Headers;
   STRING Response;         // Response header buffer
   STRING URI;              // Temporary string, used only when the user reads the URI
   STRING Username;
   STRING Password;
   STRING AuthNonce;
   STRING AuthOpaque;
   STRING AuthPath;
   STRING ContentType;
   UBYTE  *RecvBuffer;      // Receive buffer - aids downloading if HTF_RECVBUFFER is defined
   UBYTE  *WriteBuffer;
   LONG   WriteSize;
   LONG   WriteOffset;
   APTR   Buffer;           // Temporary buffer for storing outgoing data
   struct rkFile *flOutput;
   struct rkFile *flInput;
   objNetSocket *Socket;    // Socket over which the communication is taking place
   UBYTE  *Chunk;           // Chunk buffer
   LONG   ChunkSize;        // Size of the chunk buffer
   LONG   ChunkBuffered;    // Number of bytes buffered, cannot exceed ChunkSize
   LONG   ChunkLen;         // Length of the current chunk being processed (applies when reading the chunk data)
   LONG   ChunkIndex;
   TIMER  TimeoutManager;
   LARGE  LastReceipt;      // Last time (microseconds) at which data was received
   LARGE  TotalSent;        // Total number of bytes sent - exists for assisting debugging only
   OBJECTID DialogWindow;
   LONG   RecvSize;
   LONG   ResponseIndex;    // Next element to write to in 'Buffer'
   LONG   SearchIndex;      // Current position of the CRLFCRLF search.
   LONG   ResponseSize;
   WORD   InputPos;         // File name parsing position in InputFile
   UBYTE  RedirectCount;
   UBYTE  AuthCNonce[10];
   UBYTE  AuthQOP[12];
   UBYTE  AuthAlgorithm[12];
   UBYTE  AuthRetries;
   UWORD  Connecting:1;
   UWORD  AuthAttempt:1;
   UWORD  AuthPreset:1;
   UWORD  AuthDigest:1;
   UWORD  SecurePath:1;
   UWORD  Tunneling:1;
   UWORD  Chunked:1;
   UWORD  MultipleInput:1;
   UWORD  ProxyDefined:1;   // TRUE if the ProxyServer has been manually set by the user
  
#endif
} objHTTP;

#endif
