#ifndef MODULES_NETWORK
#define MODULES_NETWORK 1

// Name:      network.h
// Copyright: Paul Manias © 2005-2017
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_NETWORK (1)


#if __has_include(<openssl/ssl.h>)
   #define ENABLE_SSL
#else
   #undef ENABLE_SSL
#endif

#ifdef ENABLE_SSL
#include "openssl/ssl.h"
#endif

#ifdef __linux__
typedef LONG SOCKET_HANDLE;
#elif _WIN32
typedef ULONG SOCKET_HANDLE; // NOTE: declared as ULONG instead of SOCKET for now to avoid including winsock.h
#else
#error "No support for this platform"
#endif
  
// Address types for the IPAddress structure.

#define IPADDR_V4 0
#define IPADDR_V6 1

struct IPAddress {
   ULONG Data[4];    // 128-bit array for supporting both V4 and V6 IP addresses.
   LONG  Type;
};

#define NSF_SERVER 0x00000001
#define NSF_SSL 0x00000002
#define NSF_MULTI_CONNECT 0x00000004
#define NSF_ASYNC_RESOLVE 0x00000008
#define NSF_DEBUG 0x00000010

// NetSocket states

#define NTC_DISCONNECTED 0
#define NTC_CONNECTING 1
#define NTC_CONNECTING_SSL 2
#define NTC_CONNECTED 3

// Tags for SetSSL().

#define NSL_CONNECT 1

// Internal identifiers for the NetMsg structure.

#define NETMSG_MAGIC 941629299
#define NETMSG_SIZE_LIMIT 1048576
#define NETMSG_MAGIC_TAIL 2198696884

struct NetQueue {
   ULONG Index;    // The current read/write position within the buffer
   ULONG Length;   // The size of the buffer.
   APTR  Buffer;   // The buffer hosting the data
};

struct NetMsg {
   ULONG Magic;
   ULONG Length;
};

struct NetMsgEnd {
   ULONG CRC;
   ULONG Magic;
};

// ClientSocket class definition

#define VER_CLIENTSOCKET (1.000000)

typedef struct rkClientSocket {
   OBJECT_HEADER
   LARGE    ConnectTime;            // System time for the creation of this socket
   struct rkClientSocket * Prev;    // Previous socket in the chain
   struct rkClientSocket * Next;    // Next socket in the chain
   struct rkNetClient * Client;     // Parent client structure
   APTR     UserData;               // Free for user data storage.
   FUNCTION Outgoing;               // Callback for data being sent over the socket
   FUNCTION Incoming;               // Callback for data being received from the socket
   LONG     Handle;                 // Socket FD
   LONG     MsgLen;                 // Length of the current incoming message
   LONG     ReadCalled:1;           // TRUE if the Read action has been called

#ifdef PRV_CLIENTSOCKET
    struct NetQueue WriteQueue; // Writes to the network socket are queued here in a buffer
    struct NetQueue ReadQueue;  // Read queue, often used for reading whole messages
  
#endif
} objClientSocket;

struct rkNetClient {
   char IP[8];                        // IP address in 4/8-byte format
   struct rkNetClient * Next;         // Next client in the chain
   struct rkNetClient * Prev;         // Previous client in the chain
   struct rkNetSocket * NetSocket;    // Reference to the parent socket
   struct rkClientSocket * Sockets;   // Pointer to a list of sockets opened with this client.
   APTR UserData;                     // Free for user data storage.
   LONG TotalSockets;                 // Count of all created sockets
};

// Proxy class definition

#define VER_PROXY (1.000000)

typedef struct rkProxy {
   OBJECT_HEADER
   STRING NetworkFilter;
   STRING GatewayFilter;
   STRING Username;
   STRING Password;
   STRING ProxyName;
   STRING Server;
   LONG   Port;
   LONG   ServerPort;
   LONG   Enabled;
   LONG   Record;
   LONG   Host;

#ifdef PRV_PROXY
   char Section[40];
   char FindPort[16];
   BYTE  FindEnabled;
   UBYTE Find:1;
  
#endif
} objProxy;

// Proxy methods

#define MT_PrxDelete -1
#define MT_PrxFind -2
#define MT_PrxFindNext -3

struct prxFind { LONG Port; LONG Enabled;  };

#define prxDelete(obj) Action(MT_PrxDelete,(obj),0)

INLINE ERROR prxFind(APTR Ob, LONG Port, LONG Enabled) {
   struct prxFind args = { Port, Enabled };
   return(Action(MT_PrxFind, Ob, &args));
}

#define prxFindNext(obj) Action(MT_PrxFindNext,(obj),0)


// NetSocket class definition

#define VER_NETSOCKET (1.000000)

typedef struct rkNetSocket {
   OBJECT_HEADER
   struct rkNetClient * Clients;             // ServerMode - Attached clients
   struct rkClientSocket * CurrentSocket;    // ServerMode - The current client being operated on (e.g. for Read/Write activity)
   APTR   UserData;                          // Pointer to user data that can be used during events
   STRING Address;                           // Connect the socket to this remote address (if client)
   LONG   State;                             // Connection state
   ERROR  Error;                             // The last NetSocket error can be read from this field
   LONG   Port;                              // Connect socket to this remote port if client, or local port if server
   LONG   Flags;                             // Optional flags
   LONG   TotalClients;                      // Total number of clients registered (if socket is a server)
   LONG   Backlog;                           // Server connection backlog if in server mode
   LONG   ClientLimit;                       // Limit the number of connected clients to this value
   LONG   MsgLimit;                          // Limit the size of messages to this value

#ifdef PRV_NETSOCKET
   SOCKET_HANDLE SocketHandle;   // Handle of the socket
   FUNCTION Outgoing;
   FUNCTION Incoming;
   FUNCTION Feedback;
   struct rkNetClient *LastClient;
   struct NetQueue WriteQueue;
   struct NetQueue ReadQueue;
   UBYTE  ReadCalled:1;          // The Read() action sets this to TRUE whenever called.
   UBYTE  IPV6:1;
   UBYTE  Terminating:1;         // Set to TRUE when the NetSocket is marked for deletion.
   UBYTE  ExternalSocket:1;      // Set to TRUE if the SocketHandle field was set manually by the client.
   UBYTE  InUse;                 // Recursion counter to signal that the object is doing something.
   UBYTE  SSLBusy;               // Tracks the current actions of SSL handshaking.
   UBYTE  IncomingRecursion;     // Used by netsocket_client to prevent recursive handling of incoming data.
   UBYTE  OutgoingRecursion;
   #ifdef _WIN32
      #ifdef NO_NETRECURSION
         WORD WinRecursion; // For win32_netresponse()
      #endif
      void (*ReadSocket)(HOSTHANDLE FD, struct rkNetSocket *);
      void (*WriteSocket)(HOSTHANDLE FD, struct rkNetSocket *);
   #endif
   #ifdef ENABLE_SSL
      SSL *SSL;
      SSL_CTX *CTX;
      BIO *BIO;
   #endif
  
#endif
} objNetSocket;

// NetSocket methods

#define MT_nsConnect -1
#define MT_nsGetLocalIPAddress -2
#define MT_nsDisconnectClient -3
#define MT_nsDisconnectSocket -4
#define MT_nsReadMsg -5
#define MT_nsWriteMsg -6

struct nsConnect { CSTRING Address; LONG Port;  };
struct nsGetLocalIPAddress { struct IPAddress * Address;  };
struct nsDisconnectClient { struct rkNetClient * Client;  };
struct nsDisconnectSocket { struct rkClientSocket * Socket;  };
struct nsReadMsg { APTR Message; LONG Length; LONG Progress; LONG CRC;  };
struct nsWriteMsg { APTR Message; LONG Length;  };

INLINE ERROR nsConnect(APTR Ob, CSTRING Address, LONG Port) {
   struct nsConnect args = { Address, Port };
   return(Action(MT_nsConnect, Ob, &args));
}

INLINE ERROR nsGetLocalIPAddress(APTR Ob, struct IPAddress * Address) {
   struct nsGetLocalIPAddress args = { Address };
   return(Action(MT_nsGetLocalIPAddress, Ob, &args));
}

INLINE ERROR nsDisconnectClient(APTR Ob, struct rkNetClient * Client) {
   struct nsDisconnectClient args = { Client };
   return(Action(MT_nsDisconnectClient, Ob, &args));
}

INLINE ERROR nsDisconnectSocket(APTR Ob, struct rkClientSocket * Socket) {
   struct nsDisconnectSocket args = { Socket };
   return(Action(MT_nsDisconnectSocket, Ob, &args));
}

INLINE ERROR nsReadMsg(APTR Ob, APTR * Message, LONG * Length, LONG * Progress, LONG * CRC) {
   struct nsReadMsg args = { 0, 0, 0, 0 };
   ERROR error = Action(MT_nsReadMsg, Ob, &args);
   if (Message) *Message = args.Message;
   if (Length) *Length = args.Length;
   if (Progress) *Progress = args.Progress;
   if (CRC) *CRC = args.CRC;
   return(error);
}

INLINE ERROR nsWriteMsg(APTR Ob, APTR Message, LONG Length) {
   struct nsWriteMsg args = { Message, Length };
   return(Action(MT_nsWriteMsg, Ob, &args));
}


// These error codes for certificate validation match the OpenSSL error codes (X509 definitions)

#define SCV_UNABLE_TO_GET_ISSUER_CERT 2
#define SCV_SELF_SIGNED_CERT_IN_CHAIN 19
#define SCV_CERT_REJECTED 28
#define SCV_UNABLE_TO_GET_ISSUER_CERT_LOCALLY 20
#define SCV_SUBJECT_ISSUER_MISMATCH 29
#define SCV_CERT_CHAIN_TOO_LONG 22
#define SCV_INVALID_CA 24
#define SCV_CERT_HAS_EXPIRED 10
#define SCV_UNABLE_TO_DECRYPT_CERT_SIGNATURE 4
#define SCV_AKID_SKID_MISMATCH 30
#define SCV_ERROR_IN_CRL_LAST_UPDATE_FIELD 15
#define SCV_PATH_LENGTH_EXCEEDED 25
#define SCV_AKID_ISSUER_SERIAL_MISMATCH 31
#define SCV_ERROR_IN_CRL_NEXT_UPDATE_FIELD 16
#define SCV_CERT_REVOKED 23
#define SCV_KEYUSAGE_NO_CERTSIGN 32
#define SCV_ERROR_IN_CERT_NOT_BEFORE_FIELD 13
#define SCV_UNABLE_TO_DECRYPT_CRL_SIGNATURE 5
#define SCV_UNABLE_TO_VERIFY_LEAF_SIGNATURE 21
#define SCV_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY 6
#define SCV_CRL_NOT_YET_VALID 11
#define SCV_CERT_NOT_YET_VALID 9
#define SCV_ERROR_IN_CERT_NOT_AFTER_FIELD 14
#define SCV_OK 0
#define SCV_APPLICATION_VERIFICATION 50
#define SCV_CERT_SIGNATURE_FAILURE 7
#define SCV_DEPTH_ZERO_SELF_SIGNED_CERT 18
#define SCV_INVALID_PURPOSE 26
#define SCV_CERT_UNTRUSTED 27
#define SCV_OUT_OF_MEM 17
#define SCV_CRL_SIGNATURE_FAILURE 8
#define SCV_CRL_HAS_EXPIRED 12
#define SCV_UNABLE_TO_GET_CRL 3

INLINE ERROR nsCreate(objNetSocket **NewNetSocketOut, OBJECTID ListenerID, APTR UserData) {
   return(CreateObject(ID_NETSOCKET, 0, NewNetSocketOut,
      FID_Listener|TLONG, ListenerID,
      FID_UserData|TPTR, UserData,
      TAGEND));
}
  
struct NetworkBase {
   ERROR (*_StrToAddress)(CSTRING, struct IPAddress *);
   CSTRING (*_AddressToStr)(struct IPAddress *);
   ULONG (*_HostToShort)(ULONG);
   ULONG (*_HostToLong)(ULONG);
   ULONG (*_ShortToHost)(ULONG);
   ULONG (*_LongToHost)(ULONG);
   ERROR (*_SetSSL)(struct rkNetSocket *, ...);
   ERROR (*_ResolveName)(CSTRING, LONG, FUNCTION *, LARGE);
   ERROR (*_ResolveAddress)(CSTRING, LONG, FUNCTION *, LARGE);
};

#ifndef PRV_NETWORK_MODULE
#define netStrToAddress(...) (NetworkBase->_StrToAddress)(__VA_ARGS__)
#define netAddressToStr(...) (NetworkBase->_AddressToStr)(__VA_ARGS__)
#define netHostToShort(...) (NetworkBase->_HostToShort)(__VA_ARGS__)
#define netHostToLong(...) (NetworkBase->_HostToLong)(__VA_ARGS__)
#define netShortToHost(...) (NetworkBase->_ShortToHost)(__VA_ARGS__)
#define netLongToHost(...) (NetworkBase->_LongToHost)(__VA_ARGS__)
#define netSetSSL(...) (NetworkBase->_SetSSL)(__VA_ARGS__)
#define netResolveName(...) (NetworkBase->_ResolveName)(__VA_ARGS__)
#define netResolveAddress(...) (NetworkBase->_ResolveAddress)(__VA_ARGS__)
#endif

#endif
