#pragma once

// Name:      webrtc.h
// Copyright: Paul Manias © 2026
// Generator: idl-c

#include <kotuku/main.h>

#define MODVERSION_WEBRTC (1)

class objWebChannel;
class objPeerConnection;

// PeerConnection transport states.

enum class RTCS : int {
   NIL = 0,
   NEW = 0,
   CONNECTING = 1,
   CONNECTED = 2,
   DISCONNECTED = 3,
   FAILED = 4,
   CLOSED = 5,
};

// PeerConnection signalling states.

enum class RTCSIG : int {
   NIL = 0,
   STABLE = 0,
   HAVE_LOCAL_OFFER = 1,
   HAVE_REMOTE_OFFER = 2,
   CLOSED = 3,
};

// ICE agent states.

enum class RTCICE : int {
   NIL = 0,
   NEW = 0,
   CHECKING = 1,
   CONNECTED = 2,
   COMPLETED = 3,
   DISCONNECTED = 4,
   FAILED = 5,
};

// ICE gathering states.

enum class RTCGATHER : int {
   NIL = 0,
   NEW = 0,
   GATHERING = 1,
   COMPLETE = 2,
};

// PeerConnection ICE policy.

enum class RTCF : uint32_t {
   NIL = 0,
   RELAY_ONLY = 0x00000001,
   NO_HOST = 0x00000002,
   NO_MDNS = 0x00000004,
   DISABLE_IPV6 = 0x00000008,
};

DEFINE_ENUM_FLAG_OPERATORS(RTCF)

// WebChannel lifecycle states.

enum class WCS : int {
   NIL = 0,
   CONNECTING = 0,
   OPEN = 1,
   CLOSING = 2,
   CLOSED = 3,
};

// WebChannel complete-message types.

enum class WCM : int {
   NIL = 0,
   TEXT = 1,
   BINARY = 2,
};

// WebChannel class definition

#define VER_WEBCHANNEL (1.000000)

// WebChannel methods

namespace wch {
struct Send { std::span<const int8_t> Data; WCM Type; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Close { static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objWebChannel : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::WEBCHANNEL;
   static constexpr CSTRING CLASS_NAME = "WebChannel";

   using create = kt::Create<objWebChannel>;
   objWebChannel(objMetaClass *pClass, OBJECTID pUID) noexcept : Object(pClass, pUID) {}

   std::string Label;                     // Immutable UTF-8 label.
   std::string Protocol;                  // Immutable UTF-8 protocol.
   int64_t BufferedAmountLowThreshold;    // Mutable threshold in bytes, default zero, range 0 to 16777216.
   int     Negotiated;                    // Explicitly negotiated without DCEP.
   int     Ordered;                       // Preserve message order.
   int     MaxRetransmits;                // -1 for omitted, otherwise 0 to 65535 retries.
   int     MaxPacketLifeTime;             // -1 for omitted, otherwise lifetime in milliseconds from enqueue.
   WCS     State;                         // Channel lifecycle state.
   ERR     Error;                         // Most recent channel error.

   // Action stubs

   inline ERR init() noexcept { return InitObject(this); }
   inline ERR send(std::span<const int8_t> Data, WCM Type) noexcept {
      struct wch::Send args = { Data, Type };
      return Action(AC(-1), this, &args);
   }
   inline ERR close() noexcept {
      return Action(AC(-2), this, nullptr);
   }

   // Customised field getting


   // Customised field setting

};

// PeerConnection class definition

#define VER_PEERCONNECTION (1.000000)

// PeerConnection methods

namespace rtc {
struct CreateOffer { static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct CreateAnswer { static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SetLocalDescription { std::string_view Description; static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SetRemoteDescription { std::string_view Description; static const AC id = AC(-4); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct AddIceCandidate { std::string_view Candidate; static const AC id = AC(-5); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Close { static const AC id = AC(-6); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct RestartIce { static const AC id = AC(-7); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct CreateWebChannel { std::string_view Label; std::string_view Protocol; int Ordered; int Negotiated; int ID; int MaxRetransmits; int MaxPacketLifeTime; objWebChannel *Channel; static const AC id = AC(-8); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct GetStats { STRING Stats; static const AC id = AC(-9); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objPeerConnection : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::PEERCONNECTION;
   static constexpr CSTRING CLASS_NAME = "PeerConnection";

   using create = kt::Create<objPeerConnection>;
   objPeerConnection(objMetaClass *pClass, OBJECTID pUID) noexcept : Object(pClass, pUID) {}

   std::string LocalDescription;          // Current local SDP.
   std::string RemoteDescription;         // Current remote SDP.
   std::string CertificateFingerprint;    // SHA-256 fingerprint of the local DTLS certificate.
   std::string StunServer;                // Optional stun: URI or address:port; DNS resolution runs during gathering.
   APTR      ClientData;                  // User data.
   RTCS      State;                       // Aggregate ICE and DTLS state.
   RTCSIG    SignalingState;              // Offer/answer negotiation state.
   RTCICE    IceState;                    // ICE agent state.
   RTCGATHER GatheringState;              // Local ICE gathering state.
   ERR       Error;                       // Most recent asynchronous transport error.
   int       ReceiveLimit;                // Constructor-only receive capacity in bytes, default 262144, range 1 to 16777216.
   RTCF      Flags;                       // Configures the constructor-only ICE policy.

   // Action stubs

   inline ERR init() noexcept { return InitObject(this); }
   inline ERR createOffer() noexcept {
      return Action(AC(-1), this, nullptr);
   }
   inline ERR createAnswer() noexcept {
      return Action(AC(-2), this, nullptr);
   }
   inline ERR setLocalDescription(const std::string_view &Description) noexcept {
      struct rtc::SetLocalDescription args = { Description };
      return Action(AC(-3), this, &args);
   }
   inline ERR setRemoteDescription(const std::string_view &Description) noexcept {
      struct rtc::SetRemoteDescription args = { Description };
      return Action(AC(-4), this, &args);
   }
   inline ERR addIceCandidate(const std::string_view &Candidate) noexcept {
      struct rtc::AddIceCandidate args = { Candidate };
      return Action(AC(-5), this, &args);
   }
   inline ERR close() noexcept {
      return Action(AC(-6), this, nullptr);
   }
   inline ERR restartIce() noexcept {
      return Action(AC(-7), this, nullptr);
   }
   inline ERR createWebChannel(const std::string_view &Label, const std::string_view &Protocol, int Ordered, int Negotiated, int ID, int MaxRetransmits, int MaxPacketLifeTime, objWebChannel ** Channel) noexcept {
      struct rtc::CreateWebChannel args = { Label, Protocol, Ordered, Negotiated, ID, MaxRetransmits, MaxPacketLifeTime, (objWebChannel *)0 };
      ERR error = Action(AC(-8), this, &args);
      if (Channel) *Channel = args.Channel;
      return error;
   }
   inline ERR getStats(STRING * Stats) noexcept {
      struct rtc::GetStats args = { (STRING)0 };
      ERR error = Action(AC(-9), this, &args);
      if (Stats) *Stats = args.Stats;
      return error;
   }

   // Customised field getting


   // Customised field setting

};