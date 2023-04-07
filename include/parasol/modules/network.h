#pragma once

// Name:      network.h
// Copyright: Paul Manias © 2005-2023
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_NETWORK (1)

#ifdef __cplusplus
#include <unordered_set>
#include <map>
#include <mutex>
#endif

class objClientSocket;
class objProxy;
class objNetLookup;
class objNetSocket;


#ifdef ENABLE_SSL
#include "openssl/ssl.h"
#endif

#ifdef __linux__
typedef LONG SOCKET_HANDLE;
#elif _WIN32
typedef ULONG SOCKET_HANDLE; // NOTE: declared as ULONG instead of SOCKET for now to avoid including winsock.h
#elif __APPLE__
typedef LONG SOCKET_HANDLE;
#else
#error "No support for this platform"
#endif
// Address types for the IPAddress structure.

#define IPADDR_V4 0
#define IPADDR_V6 1

struct IPAddress {
   ULONG Data[4];    // 128-bit array for supporting both V4 and V6 IP addresses.
   LONG  Type;       // Identifies the address Data value as a V4 or V6 address type.
   LONG  Pad;        // Unused padding for 64-bit alignment
};

#define NSF_SERVER 0x00000001
#define NSF_SSL 0x00000002
#define NSF_MULTI_CONNECT 0x00000004
#define NSF_SYNCHRONOUS 0x00000008
#define NSF_DEBUG 0x00000010

// Options for NetLookup

#define NLF_NO_CACHE 0x00000001

// NetSocket states

#define NTC_DISCONNECTED 0
#define NTC_CONNECTING 1
#define NTC_CONNECTING_SSL 2
#define NTC_CONNECTED 3

// Tags for SetSSL().

#define NSL_CONNECT 1

// Internal identifiers for the NetMsg structure.

#define NETMSG_SIZE_LIMIT 1048576
#define NETMSG_MAGIC 941629299
#define NETMSG_MAGIC_TAIL 2198696884

struct NetQueue {
   ULONG Index;    // The current read/write position within the buffer
   ULONG Length;   // The size of the buffer.
   APTR  Buffer;   // The buffer hosting the data
};

struct NetMsg {
   ULONG Magic;    // Standard key to recognise the message packet
   ULONG Length;   // Byte length of the message
};

struct NetMsgEnd {
   ULONG CRC;    // Checksum of the message packet
   ULONG Magic;  // Standard key to recognise the message packet
};

struct NetClient {
   char IP[8];                 // IP address in 4/8-byte format
   struct NetClient * Next;    // Next client in the chain
   struct NetClient * Prev;    // Previous client in the chain
   objNetSocket * NetSocket;   // Reference to the parent socket
   objClientSocket * Sockets;  // Pointer to a list of sockets opened with this client.
   APTR UserData;              // Free for user data storage.
   LONG TotalSockets;          // Count of all created sockets
};

// ClientSocket class definition

#define VER_CLIENTSOCKET (1.000000)

// ClientSocket methods

#define MT_csReadClientMsg -1
#define MT_csWriteClientMsg -2

struct csReadClientMsg { APTR Message; LONG Length; LONG Progress; LONG CRC;  };
struct csWriteClientMsg { APTR Message; LONG Length;  };

INLINE ERROR csReadClientMsg(APTR Ob, APTR * Message, LONG * Length, LONG * Progress, LONG * CRC) {
   struct csReadClientMsg args = { 0, 0, 0, 0 };
   ERROR error = Action(MT_csReadClientMsg, (OBJECTPTR)Ob, &args);
   if (Message) *Message = args.Message;
   if (Length) *Length = args.Length;
   if (Progress) *Progress = args.Progress;
   if (CRC) *CRC = args.CRC;
   return(error);
}

INLINE ERROR csWriteClientMsg(APTR Ob, APTR Message, LONG Length) {
   struct csWriteClientMsg args = { Message, Length };
   return(Action(MT_csWriteClientMsg, (OBJECTPTR)Ob, &args));
}


class objClientSocket : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_CLIENTSOCKET;
   static constexpr CSTRING CLASS_NAME = "ClientSocket";

   using create = pf::Create<objClientSocket>;

   LARGE    ConnectTime;         // System time for the creation of this socket
   objClientSocket * Prev;       // Previous socket in the chain
   objClientSocket * Next;       // Next socket in the chain
   struct NetClient * Client;    // Parent client structure
   APTR     UserData;            // Free for user data storage.
   FUNCTION Outgoing;            // Callback for data being sent over the socket
   FUNCTION Incoming;            // Callback for data being received from the socket
   LONG     MsgLen;              // Length of the current incoming message
   LONG     ReadCalled:1;        // TRUE if the Read action has been called

   // Action stubs

   inline ERROR init() { return InitObject(this); }
   template <class T, class U> ERROR read(APTR Buffer, T Size, U *Result) {
      static_assert(std::is_integral<U>::value, "Result value must be an integer type");
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      ERROR error;
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      if (!(error = Action(AC_Read, this, &read))) *Result = static_cast<U>(read.Result);
      else *Result = 0;
      return error;
   }
   template <class T> ERROR read(APTR Buffer, T Size) {
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      return Action(AC_Read, this, &read);
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

};

// Proxy class definition

#define VER_PROXY (1.000000)

// Proxy methods

#define MT_prxDelete -1
#define MT_prxFind -2
#define MT_prxFindNext -3

struct prxFind { LONG Port; LONG Enabled;  };

#define prxDelete(obj) Action(MT_prxDelete,(obj),0)

INLINE ERROR prxFind(APTR Ob, LONG Port, LONG Enabled) {
   struct prxFind args = { Port, Enabled };
   return(Action(MT_prxFind, (OBJECTPTR)Ob, &args));
}

#define prxFindNext(obj) Action(MT_prxFindNext,(obj),0)


class objProxy : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_PROXY;
   static constexpr CSTRING CLASS_NAME = "Proxy";

   using create = pf::Create<objProxy>;

   STRING NetworkFilter;    // The name of the network that the proxy is limited to.
   STRING GatewayFilter;    // The IP address of the gateway that the proxy is limited to.
   STRING Username;         // The username to use when authenticating against the proxy server.
   STRING Password;         // The password to use when authenticating against the proxy server.
   STRING ProxyName;        // A human readable name for the proxy server entry.
   STRING Server;           // The destination address of the proxy server - may be an IP address or resolvable domain name.
   LONG   Port;             // Defines the ports supported by this proxy.
   LONG   ServerPort;       // The port that is used for proxy server communication.
   LONG   Enabled;          // All proxies are enabled by default until this field is set to FALSE.
   LONG   Record;           // The unique ID of the current proxy record.
   LONG   Host;             // If TRUE, the proxy settings are derived from the host operating system's default settings.

   // Action stubs

   inline ERROR disable() { return Action(AC_Disable, this, NULL); }
   inline ERROR enable() { return Action(AC_Enable, this, NULL); }
   inline ERROR init() { return InitObject(this); }
   inline ERROR saveSettings() { return Action(AC_SaveSettings, this, NULL); }

   // Customised field setting

   template <class T> inline ERROR setNetworkFilter(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setGatewayFilter(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setUsername(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[13];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setPassword(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[3];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setProxyName(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setServer(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setPort(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[8];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setServerPort(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setEnabled(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setRecord(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[4];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

};

// NetLookup class definition

#define VER_NETLOOKUP (1.000000)

// NetLookup methods

#define MT_nlResolveName -1
#define MT_nlResolveAddress -2
#define MT_nlBlockingResolveName -3
#define MT_nlBlockingResolveAddress -4

struct nlResolveName { CSTRING HostName;  };
struct nlResolveAddress { CSTRING Address;  };
struct nlBlockingResolveName { CSTRING HostName;  };
struct nlBlockingResolveAddress { CSTRING Address;  };

INLINE ERROR nlResolveName(APTR Ob, CSTRING HostName) {
   struct nlResolveName args = { HostName };
   return(Action(MT_nlResolveName, (OBJECTPTR)Ob, &args));
}

INLINE ERROR nlResolveAddress(APTR Ob, CSTRING Address) {
   struct nlResolveAddress args = { Address };
   return(Action(MT_nlResolveAddress, (OBJECTPTR)Ob, &args));
}

INLINE ERROR nlBlockingResolveName(APTR Ob, CSTRING HostName) {
   struct nlBlockingResolveName args = { HostName };
   return(Action(MT_nlBlockingResolveName, (OBJECTPTR)Ob, &args));
}

INLINE ERROR nlBlockingResolveAddress(APTR Ob, CSTRING Address) {
   struct nlBlockingResolveAddress args = { Address };
   return(Action(MT_nlBlockingResolveAddress, (OBJECTPTR)Ob, &args));
}


class objNetLookup : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_NETLOOKUP;
   static constexpr CSTRING CLASS_NAME = "NetLookup";

   using create = pf::Create<objNetLookup>;

   LARGE UserData;    // Optional user data storage
   LONG  Flags;       // Optional flags

   // Action stubs

   inline ERROR init() { return InitObject(this); }

   // Customised field setting

   inline ERROR setUserData(const LARGE Value) {
      this->UserData = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const LONG Value) {
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setCallback(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

};

// NetSocket class definition

#define VER_NETSOCKET (1.000000)

// NetSocket methods

#define MT_nsConnect -1
#define MT_nsGetLocalIPAddress -2
#define MT_nsDisconnectClient -3
#define MT_nsDisconnectSocket -4
#define MT_nsReadMsg -5
#define MT_nsWriteMsg -6

struct nsConnect { CSTRING Address; LONG Port;  };
struct nsGetLocalIPAddress { struct IPAddress * Address;  };
struct nsDisconnectClient { struct NetClient * Client;  };
struct nsDisconnectSocket { objClientSocket * Socket;  };
struct nsReadMsg { APTR Message; LONG Length; LONG Progress; LONG CRC;  };
struct nsWriteMsg { APTR Message; LONG Length;  };

INLINE ERROR nsConnect(APTR Ob, CSTRING Address, LONG Port) {
   struct nsConnect args = { Address, Port };
   return(Action(MT_nsConnect, (OBJECTPTR)Ob, &args));
}

INLINE ERROR nsGetLocalIPAddress(APTR Ob, struct IPAddress * Address) {
   struct nsGetLocalIPAddress args = { Address };
   return(Action(MT_nsGetLocalIPAddress, (OBJECTPTR)Ob, &args));
}

INLINE ERROR nsDisconnectClient(APTR Ob, struct NetClient * Client) {
   struct nsDisconnectClient args = { Client };
   return(Action(MT_nsDisconnectClient, (OBJECTPTR)Ob, &args));
}

INLINE ERROR nsDisconnectSocket(APTR Ob, objClientSocket * Socket) {
   struct nsDisconnectSocket args = { Socket };
   return(Action(MT_nsDisconnectSocket, (OBJECTPTR)Ob, &args));
}

INLINE ERROR nsReadMsg(APTR Ob, APTR * Message, LONG * Length, LONG * Progress, LONG * CRC) {
   struct nsReadMsg args = { 0, 0, 0, 0 };
   ERROR error = Action(MT_nsReadMsg, (OBJECTPTR)Ob, &args);
   if (Message) *Message = args.Message;
   if (Length) *Length = args.Length;
   if (Progress) *Progress = args.Progress;
   if (CRC) *CRC = args.CRC;
   return(error);
}

INLINE ERROR nsWriteMsg(APTR Ob, APTR Message, LONG Length) {
   struct nsWriteMsg args = { Message, Length };
   return(Action(MT_nsWriteMsg, (OBJECTPTR)Ob, &args));
}


class objNetSocket : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_NETSOCKET;
   static constexpr CSTRING CLASS_NAME = "NetSocket";

   using create = pf::Create<objNetSocket>;

   struct NetClient * Clients;    // For server sockets, lists all clients connected to the server.
   APTR   UserData;               // A user-defined pointer that can be useful in action notify events.
   STRING Address;                // An IP address or domain name to connect to.
   LONG   State;                  // The current connection state of the netsocket object.
   ERROR  Error;                  // Information about the last error that occurred during a NetSocket operation
   LONG   Port;                   // The port number to use for initiating a connection.
   LONG   Flags;                  // Optional flags.
   LONG   TotalClients;           // Indicates the total number of clients currently connected to the socket (if in server mode).
   LONG   Backlog;                // The maximum number of connections that can be queued against the socket.
   LONG   ClientLimit;            // The maximum number of clients that can be connected to a server socket.
   LONG   MsgLimit;               // Limits the size of incoming and outgoing messages.

   // Action stubs

   inline ERROR dataFeed(OBJECTPTR Object, LONG Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERROR disable() { return Action(AC_Disable, this, NULL); }
   inline ERROR init() { return InitObject(this); }
   template <class T, class U> ERROR read(APTR Buffer, T Size, U *Result) {
      static_assert(std::is_integral<U>::value, "Result value must be an integer type");
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      ERROR error;
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      if (!(error = Action(AC_Read, this, &read))) *Result = static_cast<U>(read.Result);
      else *Result = 0;
      return error;
   }
   template <class T> ERROR read(APTR Buffer, T Size) {
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      return Action(AC_Read, this, &read);
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

   inline ERROR setUserData(APTR Value) {
      this->UserData = Value;
      return ERR_Okay;
   }

   template <class T> inline ERROR setAddress(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

   inline ERROR setState(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[4];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setPort(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Port = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const LONG Value) {
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setBacklog(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Backlog = Value;
      return ERR_Okay;
   }

   inline ERROR setClientLimit(const LONG Value) {
      this->ClientLimit = Value;
      return ERR_Okay;
   }

   inline ERROR setMsgLimit(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->MsgLimit = Value;
      return ERR_Okay;
   }

   inline ERROR setSocketHandle(APTR Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, 0x08000500, Value, 1);
   }

   inline ERROR setFeedback(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[19];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERROR setIncoming(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERROR setOutgoing(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

};

// These error codes for certificate validation match the OpenSSL error codes (X509 definitions)

#define SCV_OK 0
#define SCV_UNABLE_TO_GET_ISSUER_CERT 2
#define SCV_UNABLE_TO_GET_CRL 3
#define SCV_UNABLE_TO_DECRYPT_CERT_SIGNATURE 4
#define SCV_UNABLE_TO_DECRYPT_CRL_SIGNATURE 5
#define SCV_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY 6
#define SCV_CERT_SIGNATURE_FAILURE 7
#define SCV_CRL_SIGNATURE_FAILURE 8
#define SCV_CERT_NOT_YET_VALID 9
#define SCV_CERT_HAS_EXPIRED 10
#define SCV_CRL_NOT_YET_VALID 11
#define SCV_CRL_HAS_EXPIRED 12
#define SCV_ERROR_IN_CERT_NOT_BEFORE_FIELD 13
#define SCV_ERROR_IN_CERT_NOT_AFTER_FIELD 14
#define SCV_ERROR_IN_CRL_LAST_UPDATE_FIELD 15
#define SCV_ERROR_IN_CRL_NEXT_UPDATE_FIELD 16
#define SCV_OUT_OF_MEM 17
#define SCV_DEPTH_ZERO_SELF_SIGNED_CERT 18
#define SCV_SELF_SIGNED_CERT_IN_CHAIN 19
#define SCV_UNABLE_TO_GET_ISSUER_CERT_LOCALLY 20
#define SCV_UNABLE_TO_VERIFY_LEAF_SIGNATURE 21
#define SCV_CERT_CHAIN_TOO_LONG 22
#define SCV_CERT_REVOKED 23
#define SCV_INVALID_CA 24
#define SCV_PATH_LENGTH_EXCEEDED 25
#define SCV_INVALID_PURPOSE 26
#define SCV_CERT_UNTRUSTED 27
#define SCV_CERT_REJECTED 28
#define SCV_SUBJECT_ISSUER_MISMATCH 29
#define SCV_AKID_SKID_MISMATCH 30
#define SCV_AKID_ISSUER_SERIAL_MISMATCH 31
#define SCV_KEYUSAGE_NO_CERTSIGN 32
#define SCV_APPLICATION_VERIFICATION 50

INLINE ERROR nsCreate(objNetSocket **NewNetSocketOut, OBJECTID ListenerID, APTR UserData) {
   if ((*NewNetSocketOut = objNetSocket::create::global(fl::Listener(ListenerID), fl::UserData(UserData)))) return ERR_Okay;
   else return ERR_CreateObject;
}
extern struct NetworkBase *NetworkBase;
struct NetworkBase {
   ERROR (*_StrToAddress)(CSTRING String, struct IPAddress * Address);
   CSTRING (*_AddressToStr)(struct IPAddress * IPAddress);
   ULONG (*_HostToShort)(ULONG Value);
   ULONG (*_HostToLong)(ULONG Value);
   ULONG (*_ShortToHost)(ULONG Value);
   ULONG (*_LongToHost)(ULONG Value);
   ERROR (*_SetSSL)(objNetSocket * NetSocket, ...);
};

#ifndef PRV_NETWORK_MODULE
inline ERROR netStrToAddress(CSTRING String, struct IPAddress * Address) { return NetworkBase->_StrToAddress(String,Address); }
inline CSTRING netAddressToStr(struct IPAddress * IPAddress) { return NetworkBase->_AddressToStr(IPAddress); }
inline ULONG netHostToShort(ULONG Value) { return NetworkBase->_HostToShort(Value); }
inline ULONG netHostToLong(ULONG Value) { return NetworkBase->_HostToLong(Value); }
inline ULONG netShortToHost(ULONG Value) { return NetworkBase->_ShortToHost(Value); }
inline ULONG netLongToHost(ULONG Value) { return NetworkBase->_LongToHost(Value); }
template<class... Args> ERROR netSetSSL(objNetSocket * NetSocket, Args... Tags) { return NetworkBase->_SetSSL(NetSocket,Tags...); }
#endif

