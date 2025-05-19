#pragma once

// Name:      network.h
// Copyright: Paul Manias © 2005-2025
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

// Address types for the IPAddress structure.

enum class IPADDR : int {
   NIL = 0,
   V4 = 0,
   V6 = 1,
};

enum class NSF : uint32_t {
   NIL = 0,
   SERVER = 0x00000001,
   SSL = 0x00000002,
   MULTI_CONNECT = 0x00000004,
   SYNCHRONOUS = 0x00000008,
   LOG_ALL = 0x00000010,
};

DEFINE_ENUM_FLAG_OPERATORS(NSF)

// Options for NetLookup

enum class NLF : uint32_t {
   NIL = 0,
   NO_CACHE = 0x00000001,
};

DEFINE_ENUM_FLAG_OPERATORS(NLF)

// NetSocket states

enum class NTC : int {
   NIL = 0,
   DISCONNECTED = 0,
   CONNECTING = 1,
   CONNECTING_SSL = 2,
   CONNECTED = 3,
};

// Tags for SetSSL().

enum class NSL : int {
   NIL = 0,
   CONNECT = 1,
};

// Internal identifiers for the NetMsg structure.

#define NETMSG_SIZE_LIMIT 1048576
#define NETMSG_MAGIC 941629299
#define NETMSG_MAGIC_TAIL 2198696884

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
struct IPAddress {
   ULONG  Data[4];   // 128-bit array for supporting both V4 and V6 IP addresses.
   IPADDR Type;      // Identifies the address Data value as a V4 or V6 address type.
   int    Pad;       // Unused padding for 64-bit alignment
};

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
   APTR ClientData;            // Free for user data storage.
   int  TotalSockets;          // Count of all created sockets
};

// ClientSocket class definition

#define VER_CLIENTSOCKET (1.000000)

// ClientSocket methods

namespace cs {
struct ReadClientMsg { APTR Message; int Length; int Progress; int CRC; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct WriteClientMsg { APTR Message; int Length; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objClientSocket : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::CLIENTSOCKET;
   static constexpr CSTRING CLASS_NAME = "ClientSocket";

   using create = pf::Create<objClientSocket>;

   int64_t  ConnectTime;         // System time for the creation of this socket
   objClientSocket * Prev;       // Previous socket in the chain
   objClientSocket * Next;       // Next socket in the chain
   struct NetClient * Client;    // Parent client structure
   APTR     ClientData;          // Free for user data storage.
   FUNCTION Outgoing;            // Callback for data being sent over the socket
   FUNCTION Incoming;            // Callback for data being received from the socket
   int      MsgLen;              // Length of the current incoming message
   int      ReadCalled:1;        // TRUE if the Read action has been called

   // Action stubs

   inline ERR init() noexcept { return InitObject(this); }
   template <class T, class U> ERR read(APTR Buffer, T Size, U *Result) noexcept {
      static_assert(std::is_integral<U>::value, "Result value must be an integer type");
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      const int bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      if (auto error = Action(AC::Read, this, &read); error IS ERR::Okay) {
         *Result = static_cast<U>(read.Result);
         return ERR::Okay;
      }
      else { *Result = 0; return error; }
   }
   template <class T> ERR read(APTR Buffer, T Size) noexcept {
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      const int bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      return Action(AC::Read, this, &read);
   }
   inline ERR write(CPTR Buffer, int Size, int *Result = NULL) noexcept {
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (auto error = Action(AC::Write, this, &write); error IS ERR::Okay) {
         if (Result) *Result = write.Result;
         return ERR::Okay;
      }
      else {
         if (Result) *Result = 0;
         return error;
      }
   }
   inline ERR write(std::string Buffer, int *Result = NULL) noexcept {
      struct acWrite write = { (BYTE *)Buffer.c_str(), int(Buffer.size()) };
      if (auto error = Action(AC::Write, this, &write); error IS ERR::Okay) {
         if (Result) *Result = write.Result;
         return ERR::Okay;
      }
      else {
         if (Result) *Result = 0;
         return error;
      }
   }
   inline int writeResult(CPTR Buffer, int Size) noexcept {
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (Action(AC::Write, this, &write) IS ERR::Okay) return write.Result;
      else return 0;
   }
   inline ERR readClientMsg(APTR * Message, int * Length, int * Progress, int * CRC) noexcept {
      struct cs::ReadClientMsg args = { (APTR)0, (int)0, (int)0, (int)0 };
      ERR error = Action(AC(-1), this, &args);
      if (Message) *Message = args.Message;
      if (Length) *Length = args.Length;
      if (Progress) *Progress = args.Progress;
      if (CRC) *CRC = args.CRC;
      return(error);
   }
   inline ERR writeClientMsg(APTR Message, int Length) noexcept {
      struct cs::WriteClientMsg args = { Message, Length };
      return(Action(AC(-2), this, &args));
   }

   // Customised field setting

};

// Proxy class definition

#define VER_PROXY (1.000000)

// Proxy methods

namespace prx {
struct DeleteRecord { static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Find { int Port; int Enabled; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct FindNext { static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objProxy : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::PROXY;
   static constexpr CSTRING CLASS_NAME = "Proxy";

   using create = pf::Create<objProxy>;

   STRING NetworkFilter;    // The name of the network that the proxy is limited to.
   STRING GatewayFilter;    // The IP address of the gateway that the proxy is limited to.
   STRING Username;         // The username to use when authenticating against the proxy server.
   STRING Password;         // The password to use when authenticating against the proxy server.
   STRING ProxyName;        // A human readable name for the proxy server entry.
   STRING Server;           // The destination address of the proxy server - may be an IP address or resolvable domain name.
   int    Port;             // Defines the ports supported by this proxy.
   int    ServerPort;       // The port that is used for proxy server communication.
   int    Enabled;          // All proxies are enabled by default until this field is set to false.
   int    Record;           // The unique ID of the current proxy record.
   int    Host;             // If true, the proxy settings are derived from the host operating system's default settings.

   // Action stubs

   inline ERR disable() noexcept { return Action(AC::Disable, this, NULL); }
   inline ERR enable() noexcept { return Action(AC::Enable, this, NULL); }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR saveSettings() noexcept { return Action(AC::SaveSettings, this, NULL); }
   inline ERR deleteRecord() noexcept {
      return(Action(AC(-1), this, NULL));
   }
   inline ERR find(int Port, int Enabled) noexcept {
      struct prx::Find args = { Port, Enabled };
      return(Action(AC(-2), this, &args));
   }
   inline ERR findNext() noexcept {
      return(Action(AC(-3), this, NULL));
   }

   // Customised field setting

   template <class T> inline ERR setNetworkFilter(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setGatewayFilter(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setUsername(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[13];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setPassword(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[3];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setProxyName(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setServer(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERR setPort(const int Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[8];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setServerPort(const int Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[10];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setEnabled(const int Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setRecord(const int Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[4];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

};

// NetLookup class definition

#define VER_NETLOOKUP (1.000000)

// NetLookup methods

namespace nl {
struct ResolveName { CSTRING HostName; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ResolveAddress { CSTRING Address; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct BlockingResolveName { CSTRING HostName; static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct BlockingResolveAddress { CSTRING Address; static const AC id = AC(-4); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objNetLookup : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::NETLOOKUP;
   static constexpr CSTRING CLASS_NAME = "NetLookup";

   using create = pf::Create<objNetLookup>;

   int64_t ClientData;    // Optional user data storage
   NLF     Flags;         // Optional flags

   // Action stubs

   inline ERR init() noexcept { return InitObject(this); }
   inline ERR resolveName(CSTRING HostName) noexcept {
      struct nl::ResolveName args = { HostName };
      return(Action(AC(-1), this, &args));
   }
   inline ERR resolveAddress(CSTRING Address) noexcept {
      struct nl::ResolveAddress args = { Address };
      return(Action(AC(-2), this, &args));
   }
   inline ERR blockingResolveName(CSTRING HostName) noexcept {
      struct nl::BlockingResolveName args = { HostName };
      return(Action(AC(-3), this, &args));
   }
   inline ERR blockingResolveAddress(CSTRING Address) noexcept {
      struct nl::BlockingResolveAddress args = { Address };
      return(Action(AC(-4), this, &args));
   }

   // Customised field setting

   inline ERR setClientData(const int64_t Value) noexcept {
      this->ClientData = Value;
      return ERR::Okay;
   }

   inline ERR setFlags(const NLF Value) noexcept {
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setCallback(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

};

// NetSocket class definition

#define VER_NETSOCKET (1.000000)

// NetSocket methods

namespace ns {
struct Connect { CSTRING Address; int Port; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct GetLocalIPAddress { struct IPAddress * Address; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct DisconnectClient { struct NetClient * Client; static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct DisconnectSocket { objClientSocket * Socket; static const AC id = AC(-4); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ReadMsg { APTR Message; int Length; int Progress; int CRC; static const AC id = AC(-5); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct WriteMsg { APTR Message; int Length; static const AC id = AC(-6); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objNetSocket : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::NETSOCKET;
   static constexpr CSTRING CLASS_NAME = "NetSocket";

   using create = pf::Create<objNetSocket>;

   struct NetClient * Clients;    // For server sockets, lists all clients connected to the server.
   APTR   ClientData;             // A client-defined value that can be useful in action notify events.
   STRING Address;                // An IP address or domain name to connect to.
   NTC    State;                  // The current connection state of the netsocket object.
   ERR    Error;                  // Information about the last error that occurred during a NetSocket operation
   int    Port;                   // The port number to use for initiating a connection.
   NSF    Flags;                  // Optional flags.
   int    TotalClients;           // Indicates the total number of clients currently connected to the socket (if in server mode).
   int    Backlog;                // The maximum number of connections that can be queued against the socket.
   int    ClientLimit;            // The maximum number of clients that can be connected to a server socket.
   int    MsgLimit;               // Limits the size of incoming and outgoing messages.

   // Action stubs

   inline ERR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, int Size) noexcept {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC::DataFeed, this, &args);
   }
   inline ERR disable() noexcept { return Action(AC::Disable, this, NULL); }
   inline ERR init() noexcept { return InitObject(this); }
   template <class T, class U> ERR read(APTR Buffer, T Size, U *Result) noexcept {
      static_assert(std::is_integral<U>::value, "Result value must be an integer type");
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      const int bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      if (auto error = Action(AC::Read, this, &read); error IS ERR::Okay) {
         *Result = static_cast<U>(read.Result);
         return ERR::Okay;
      }
      else { *Result = 0; return error; }
   }
   template <class T> ERR read(APTR Buffer, T Size) noexcept {
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      const int bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      return Action(AC::Read, this, &read);
   }
   inline ERR write(CPTR Buffer, int Size, int *Result = NULL) noexcept {
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (auto error = Action(AC::Write, this, &write); error IS ERR::Okay) {
         if (Result) *Result = write.Result;
         return ERR::Okay;
      }
      else {
         if (Result) *Result = 0;
         return error;
      }
   }
   inline ERR write(std::string Buffer, int *Result = NULL) noexcept {
      struct acWrite write = { (BYTE *)Buffer.c_str(), int(Buffer.size()) };
      if (auto error = Action(AC::Write, this, &write); error IS ERR::Okay) {
         if (Result) *Result = write.Result;
         return ERR::Okay;
      }
      else {
         if (Result) *Result = 0;
         return error;
      }
   }
   inline int writeResult(CPTR Buffer, int Size) noexcept {
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (Action(AC::Write, this, &write) IS ERR::Okay) return write.Result;
      else return 0;
   }
   inline ERR connect(CSTRING Address, int Port) noexcept {
      struct ns::Connect args = { Address, Port };
      return(Action(AC(-1), this, &args));
   }
   inline ERR getLocalIPAddress(struct IPAddress * Address) noexcept {
      struct ns::GetLocalIPAddress args = { Address };
      return(Action(AC(-2), this, &args));
   }
   inline ERR disconnectClient(struct NetClient * Client) noexcept {
      struct ns::DisconnectClient args = { Client };
      return(Action(AC(-3), this, &args));
   }
   inline ERR disconnectSocket(objClientSocket * Socket) noexcept {
      struct ns::DisconnectSocket args = { Socket };
      return(Action(AC(-4), this, &args));
   }
   inline ERR readMsg(APTR * Message, int * Length, int * Progress, int * CRC) noexcept {
      struct ns::ReadMsg args = { (APTR)0, (int)0, (int)0, (int)0 };
      ERR error = Action(AC(-5), this, &args);
      if (Message) *Message = args.Message;
      if (Length) *Length = args.Length;
      if (Progress) *Progress = args.Progress;
      if (CRC) *CRC = args.CRC;
      return(error);
   }
   inline ERR writeMsg(APTR Message, int Length) noexcept {
      struct ns::WriteMsg args = { Message, Length };
      return(Action(AC(-6), this, &args));
   }

   // Customised field setting

   inline ERR setClientData(APTR Value) noexcept {
      this->ClientData = Value;
      return ERR::Okay;
   }

   template <class T> inline ERR setAddress(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

   inline ERR setState(const NTC Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[4];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setPort(const int Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Port = Value;
      return ERR::Okay;
   }

   inline ERR setFlags(const NSF Value) noexcept {
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setBacklog(const int Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Backlog = Value;
      return ERR::Okay;
   }

   inline ERR setClientLimit(const int Value) noexcept {
      this->ClientLimit = Value;
      return ERR::Okay;
   }

   inline ERR setMsgLimit(const int Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->MsgLimit = Value;
      return ERR::Okay;
   }

   inline ERR setSocketHandle(APTR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, 0x08000500, Value, 1);
   }

   inline ERR setFeedback(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[20];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERR setIncoming(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERR setOutgoing(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

};

inline ERR nsCreate(objNetSocket **NewNetSocketOut, OBJECTID ListenerID, APTR ClientData) {
   if ((*NewNetSocketOut = objNetSocket::create::global(fl::Listener(ListenerID), fl::ClientData(ClientData)))) return ERR::Okay;
   else return ERR::CreateObject;
}
#ifdef PARASOL_STATIC
#define JUMPTABLE_NETWORK static struct NetworkBase *NetworkBase;
#else
#define JUMPTABLE_NETWORK struct NetworkBase *NetworkBase;
#endif

struct NetworkBase {
#ifndef PARASOL_STATIC
   ERR (*_StrToAddress)(CSTRING String, struct IPAddress *Address);
   CSTRING (*_AddressToStr)(struct IPAddress *IPAddress);
   ULONG (*_HostToShort)(ULONG Value);
   ULONG (*_HostToLong)(ULONG Value);
   ULONG (*_ShortToHost)(ULONG Value);
   ULONG (*_LongToHost)(ULONG Value);
   ERR (*_SetSSL)(objNetSocket *NetSocket, ...);
#endif // PARASOL_STATIC
};

#ifndef PRV_NETWORK_MODULE
#ifndef PARASOL_STATIC
extern struct NetworkBase *NetworkBase;
namespace net {
inline ERR StrToAddress(CSTRING String, struct IPAddress *Address) { return NetworkBase->_StrToAddress(String,Address); }
inline CSTRING AddressToStr(struct IPAddress *IPAddress) { return NetworkBase->_AddressToStr(IPAddress); }
inline ULONG HostToShort(ULONG Value) { return NetworkBase->_HostToShort(Value); }
inline ULONG HostToLong(ULONG Value) { return NetworkBase->_HostToLong(Value); }
inline ULONG ShortToHost(ULONG Value) { return NetworkBase->_ShortToHost(Value); }
inline ULONG LongToHost(ULONG Value) { return NetworkBase->_LongToHost(Value); }
template<class... Args> ERR SetSSL(objNetSocket *NetSocket, Args... Tags) { return NetworkBase->_SetSSL(NetSocket,Tags...); }
} // namespace
#else
namespace net {
extern ERR StrToAddress(CSTRING String, struct IPAddress *Address);
extern CSTRING AddressToStr(struct IPAddress *IPAddress);
extern ULONG HostToShort(ULONG Value);
extern ULONG HostToLong(ULONG Value);
extern ULONG ShortToHost(ULONG Value);
extern ULONG LongToHost(ULONG Value);
extern ERR SetSSL(objNetSocket *NetSocket, ...);
} // namespace
#endif // PARASOL_STATIC
#endif

