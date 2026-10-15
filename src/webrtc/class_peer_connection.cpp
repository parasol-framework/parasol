/*********************************************************************************************************************

-CLASS-
PeerConnection: Negotiates one bundled WebRTC transport.

Create an offer or set a remote offer, then set both local and remote descriptions.  The connection gathers host
and relay candidates and performs full ICE connectivity checks before DTLS.  Process framework messages to drive sockets
and timers.  Host candidates use random mDNS names by default and are exposed only after registration succeeds.
Use the NO_MDNS flag to permit numeric host signalling.  Unavailable multicast suppresses hosts without a numeric
fallback; configure a TURN server when a relay fallback is needed.  CreateWebChannel reserves a child before
negotiation or opens one on the existing association.
After a connection is established, RestartIce generates a new offer and credentials on the existing transport.

-END-

*********************************************************************************************************************/

#include <chrono>
#include <algorithm>
#include <deque>
#include <string>
#include <utility>

class extWebChannel;

class extPeerConnection : public objPeerConnection {
public:
   std::unique_ptr<rtc::PeerConnection> Connection;
   RTCS ReportedState = RTCS::NEW;
   RTCICE ReportedIceState = RTCICE::NEW;
   FUNCTION Feedback;
   FUNCTION OnIceCandidate;
   FUNCTION OnWebChannel;
   FUNCTION OnRestartRequired;
   uint64_t ReportedRestartGeneration = 0;
   std::map<uint64_t, extWebChannel *> Channels;
   std::string ServerConfiguration;
   bool Dispatching = false;
   APTR TimerHandle = nullptr;
   std::deque<std::pair<uint64_t, std::string>> PendingCandidates;

   extPeerConnection(objMetaClass *ClassPtr, OBJECTID ObjectID) : objPeerConnection(ClassPtr, ObjectID) {
      State = RTCS::NEW;
      SignalingState = RTCSIG::STABLE;
      IceState = RTCICE::NEW;
      GatheringState = RTCGATHER::NEW;
      Error = ERR::Okay;
      ClientData = nullptr;
      ReceiveLimit = 262144;
      Flags = RTCF::NIL;
   }

   ~extPeerConnection();

};

#include "class_web_channel.cpp"

static uint64_t peer_now_us() {
   return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}

static void peer_refresh(extPeerConnection *Self) {
   if (not Self->Connection) return;
   Self->LocalDescription = Self->Connection->local_description();
   Self->RemoteDescription = Self->Connection->remote_description();
   Self->CertificateFingerprint = Self->Connection->certificate_fingerprint();
   Self->State = RTCS(int(Self->Connection->state()));
   Self->SignalingState = RTCSIG(int(Self->Connection->signaling_state()));
   Self->IceState = RTCICE(int(Self->Connection->ice_state()));
   Self->GatheringState = RTCGATHER(int(Self->Connection->gathering_state()));
   if (Self->Connection->gathering_error() != ERR::Okay) Self->Error = Self->Connection->gathering_error();
}

static void peer_report(extPeerConnection *Self) {
   if (Self->State IS Self->ReportedState and Self->IceState IS Self->ReportedIceState) return;
   Self->ReportedState = Self->State;
   Self->ReportedIceState = Self->IceState;
   if (Self->Feedback.stale()) { Self->Feedback.unpin(); Self->Feedback.clear(); }
   if (not Self->Feedback.defined()) return;
   auto callback = Self->Feedback;
   callback.pin();
   Self->pin();
   auto lifetime = kt::Defer([&]() { callback.unpin(); rtc_release_pin(Self); });
   if (callback.isC()) {
      auto routine = (void (*)(extPeerConnection *, RTCS, RTCICE, APTR))callback.Routine;
      kt::SwitchContext context(callback.Context);
      routine(Self, Self->State, Self->IceState, callback.Meta);
   } else if (callback.isScript()) {
      sc::Call(callback, std::to_array<ScriptArg>({
         { "PeerConnection", Self, FD_OBJECTPTR },
         { "State", int(Self->State) },
         { "IceState", int(Self->IceState) }
      }));
   }
}

static void peer_report_candidate(extPeerConnection *Self) {
   if (Self->Connection and not Self->Connection->candidate_routes_ready()) {
      Self->PendingCandidates.clear(); return;
   }
   if (Self->PendingCandidates.empty()) return;
   if (Self->OnIceCandidate.stale()) { Self->OnIceCandidate.unpin(); Self->OnIceCandidate.clear(); }
   if (not Self->OnIceCandidate.defined()) { Self->PendingCandidates.clear(); return; }
   auto callback = Self->OnIceCandidate;
   callback.pin();
   Self->pin();
   auto lifetime = kt::Defer([&]() { callback.unpin(); rtc_release_pin(Self); });
   while (not Self->PendingCandidates.empty() and not Self->collecting()) {
      if (not Self->Connection or Self->Connection->state() IS rtc::PeerState::CLOSED) {
         Self->PendingCandidates.clear(); return;
      }
      auto event = std::move(Self->PendingCandidates.front());
      Self->PendingCandidates.pop_front();
      if (event.first != Self->Connection->candidate_generation()) continue;
      auto candidate = std::move(event.second);
      if (callback.isC()) {
         auto routine = (void (*)(extPeerConnection *, CSTRING, APTR))callback.Routine;
         kt::SwitchContext context(callback.Context);
         routine(Self, candidate.c_str(), callback.Meta);
      } else if (callback.isScript()) {
         sc::Call(callback, std::to_array<ScriptArg>({
            { "PeerConnection", Self, FD_OBJECTPTR }, { "Candidate", candidate }
         }));
      }
   }
}

static void peer_report_restart(extPeerConnection *Self) {
   if (not Self->Connection or not Self->Connection->restart_required() or
       Self->Connection->state() IS rtc::PeerState::CLOSED or Self->collecting()) return;
   auto generation = Self->Connection->restart_generation();
   if (Self->ReportedRestartGeneration IS generation) return;
   if (Self->OnRestartRequired.stale()) { Self->OnRestartRequired.unpin(); Self->OnRestartRequired.clear(); }
   if (not Self->OnRestartRequired.defined()) return;
   Self->ReportedRestartGeneration = generation;
   auto callback = Self->OnRestartRequired;
   callback.pin(); Self->pin();
   auto lifetime = kt::Defer([&]() { callback.unpin(); rtc_release_pin(Self); });
   if (callback.isC()) {
      auto routine = (void (*)(extPeerConnection *, int64_t, APTR))callback.Routine;
      kt::SwitchContext context(callback.Context);
      routine(Self, int64_t(generation), callback.Meta);
   } else if (callback.isScript()) {
      sc::Call(callback, std::to_array<ScriptArg>({
         { "PeerConnection", Self, FD_OBJECTPTR }, { "Generation", int64_t(generation) }
      }));
   }
}

static ERR peer_timer(extPeerConnection *Self, int64_t, int64_t) {
   if (not Self->Connection or Self->Dispatching or Self->collecting()) return ERR::Okay;
   Self->pin();
   auto lifetime = kt::Defer([&]() { rtc_release_pin(Self); });
   auto error = Self->Connection->state() IS rtc::PeerState::CLOSED ? ERR::Okay :
      Self->Connection->tick(peer_now_us());
   if (error != ERR::Okay) Self->Error = error;
   auto due = Self->Connection->timeout_us(peer_now_us());
   auto interval = due < 0 ? 0.05 : std::clamp(double(due) / 1000000.0, 0.001, 0.05);
   UpdateTimer(Self->TimerHandle, interval);
   peer_refresh(Self);
   peer_report_candidate(Self);
   if (Self->collecting()) { Self->TimerHandle = nullptr; return ERR::Terminate; }
   peer_report(Self);
   if (not Self->collecting()) peer_report_restart(Self);
   if (not Self->collecting()) peer_report_channels(Self);
   if (Self->collecting() or (Self->State IS RTCS::CLOSED and
       Self->Connection->timeout_us(peer_now_us()) < 0)) {
      Self->TimerHandle = nullptr; return ERR::Terminate;
   }
   return ERR::Okay;
}

static ERR PEERCONNECTION_Init(extPeerConnection *Self) {
   rtc::PeerCallbacks callbacks;
   callbacks.candidate = [Self](std::string_view Value) {
      Self->PendingCandidates.emplace_back(Self->Connection->candidate_generation(), Value);
   };
   // A negative interval only brings the next tick forward; peer_timer() restores the deadline-derived interval.
   callbacks.wake = [Self]() { if (Self->TimerHandle) UpdateTimer(Self->TimerHandle, -0.001); };
   Self->Connection = std::make_unique<rtc::PeerConnection>(std::move(callbacks));
   Self->Connection->enable_channels();
   auto error = Self->Connection->set_ice_servers(Self->ServerConfiguration);
   Self->ServerConfiguration.clear();
   if (error != ERR::Okay) return error;
   error = Self->Connection->set_policy(uint32_t(Self->Flags));
   if (error != ERR::Okay) return error;
   error = Self->Connection->set_receive_limit(Self->ReceiveLimit);
   if (error != ERR::Okay) return error;
   kt::SwitchContext context(Self);
   return SubscribeTimer(0.05, C_FUNCTION(peer_timer), &Self->TimerHandle);
}

/*********************************************************************************************************************

-METHOD-
CreateOffer: Generates a data-channel SDP offer and starts candidate gathering.

Set #IceServers for TURN or #StunServer for legacy STUN before calling this method.  StunServer accepts a `stun:` URI
with a host name or numeric address, defaulting to port 3478, or a bare address and port.  Host names are resolved
asynchronously; this method returns the initial offer while gathering continues.  Default host candidates arrive
after mDNS registration and are absent from the initial offer.  Read
#LocalDescription after this method, then pass it to #SetLocalDescription().  Additional candidates are reported
through #OnIceCandidate.  Forward these candidates through application signalling, or wait for #GatheringState to
become COMPLETE before reading and setting the full #LocalDescription.  Gathering completes after local mDNS,
server-name resolution, STUN and TURN jobs finish or fail.

-ERRORS-
Okay
InvalidState
Failed
-END-

*********************************************************************************************************************/

static ERR PEERCONNECTION_CreateOffer(extPeerConnection *Self) {
   if (not Self->Connection->has_ice_servers() and not Self->StunServer.empty()) {
      auto configured = Self->Connection->set_stun_server(Self->StunServer);
      if (configured != ERR::Okay) return configured;
   }
   auto error = Self->Connection->create_offer();
   if (error != ERR::Okay) return error;
   peer_refresh(Self);
   peer_report_candidate(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
RestartIce: Generates a new offer with fresh ICE credentials on an established DTLS transport.

Read #LocalDescription, commit it with #SetLocalDescription(), and signal it to the remote peer.  The remote peer
accepts the changed ICE credentials with #SetRemoteDescription(), creates an answer, and commits that answer.  The
existing DTLS session and certificate are retained.  Local sockets, mDNS records and TURN allocations are
re-gathered; wait for #GatheringState or forward trickle candidates before completing the exchange.  #State returns to CONNECTING until ICE selects a new route.

-ERRORS-
Okay
InvalidState
Failed
-END-

*********************************************************************************************************************/

static ERR PEERCONNECTION_RestartIce(extPeerConnection *Self) {
   auto error = Self->Connection->restart_ice();
   peer_refresh(Self);
   peer_report(Self);
   return error;
}

/*********************************************************************************************************************

-METHOD-
CreateAnswer: Generates a data-channel SDP answer for the current remote offer.

Set #IceServers for TURN or #StunServer for legacy STUN before calling this method.  StunServer accepts a `stun:` URI
with a host name or numeric address, defaulting to port 3478, or a bare address and port.  Host names are resolved
asynchronously; this method returns the initial answer while gathering continues.  Default host candidates arrive
after mDNS registration and are absent from the initial answer.  Read
#LocalDescription after this method, then pass it to #SetLocalDescription().  Additional candidates are reported
through #OnIceCandidate.  Forward these candidates through application signalling, or wait for #GatheringState to
become COMPLETE before reading and setting the full #LocalDescription.  Gathering completes after local mDNS,
server-name resolution, STUN and TURN jobs finish or fail.

-ERRORS-
Okay
InvalidState
Failed
-END-

*********************************************************************************************************************/

static ERR PEERCONNECTION_CreateAnswer(extPeerConnection *Self) {
   if (not Self->Connection->has_ice_servers() and not Self->StunServer.empty() and
       Self->Connection->state() IS rtc::PeerState::NEW) {
      auto configured = Self->Connection->set_stun_server(Self->StunServer);
      if (configured != ERR::Okay) return configured;
   }
   auto error = Self->Connection->create_answer();
   if (error != ERR::Okay) return error;
   peer_refresh(Self);
   peer_report_candidate(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
SetLocalDescription: Commits the offer or answer returned in #LocalDescription.

-INPUT-
strview Description: The local SDP returned by #CreateOffer(), #CreateAnswer() or #RestartIce().

-ERRORS-
Okay
InvalidState
-END-

*********************************************************************************************************************/

static ERR PEERCONNECTION_SetLocalDescription(extPeerConnection *Self, struct rtc::SetLocalDescription *Args) {
   if (not Args) return ERR::NullArgs;
   auto error = Self->Connection->set_local_description(Args->Description);
   peer_refresh(Self);
   peer_report(Self);
   return error;
}

/*********************************************************************************************************************

-METHOD-
SetRemoteDescription: Applies a remote SDP offer or answer.

-INPUT-
strview Description: The remote SDP.

-ERRORS-
Okay
InvalidData
InvalidState
-END-

*********************************************************************************************************************/

static ERR PEERCONNECTION_SetRemoteDescription(extPeerConnection *Self, struct rtc::SetRemoteDescription *Args) {
   if (not Args) return ERR::NullArgs;
   auto error = Self->Connection->set_remote_description(Args->Description);
   peer_refresh(Self);
   peer_report(Self);
   return error;
}

/*********************************************************************************************************************

-METHOD-
AddIceCandidate: Adds a remote UDP candidate received after the remote description, or ends the remote candidate list.

Single-label .local host names resolve asynchronously on local multicast interfaces; arbitrary DNS host names are
not resolved here.  The original candidate name remains in the remote description.  End-of-candidates closes
input while already accepted resolutions finish within three seconds.  Later input and mismatched ICE ufrags are
rejected.  Restart and close cancel outstanding resolution.

-INPUT-
strview Candidate: The `candidate:` value without `a=`.  Empty or `end-of-candidates` ends remote candidates.

-ERRORS-
Okay
InvalidData
InvalidState
BufferOverflow
-END-

*********************************************************************************************************************/

static ERR PEERCONNECTION_AddIceCandidate(extPeerConnection *Self, struct rtc::AddIceCandidate *Args) {
   if (not Args) return ERR::NullArgs;
   return Self->Connection->add_ice_candidate(Args->Candidate);
}

/*********************************************************************************************************************

-METHOD-
Close: Closes the peer connection and its UDP socket.

-ERRORS-
Okay
-END-

*********************************************************************************************************************/

static ERR PEERCONNECTION_Close(extPeerConnection *Self) {
   Self->Connection->close();
   for (auto &[handle, child] : Self->Channels) {
      if (child->State != WCS::CLOSED) child->State = WCS::CLOSING;
   }
   peer_refresh(Self);
   peer_report(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Feedback: Receives changes to the peer connection or ICE state.

The Tiri callback receives `(PeerConnection, State, IceState)`.  `State` is an `RTCS` value and `IceState` is an
`RTCICE` value.  A C callback receives `(objPeerConnection *, RTCS, RTCICE, APTR)`, with the final argument set to
the callback's metadata pointer.  Set this field before creating an offer or answer to observe all state changes.

-END-

*********************************************************************************************************************/

static ERR GET_Feedback(extPeerConnection *Self, FUNCTION * &Value) {
   if (not Self->Feedback.defined()) return ERR::FieldNotSet;
   Value = &Self->Feedback;
   return ERR::Okay;
}

static ERR SET_Feedback(extPeerConnection *Self, FUNCTION *Value) {
   if (Self->Feedback.defined()) { Self->Feedback.unpin(); Self->Feedback.clear(); }
   if (Value) { Self->Feedback = *Value; Self->Feedback.pin(); }
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
OnIceCandidate: Receives each local ICE candidate as it is gathered.

The Tiri callback receives `(PeerConnection, Candidate)`, where `Candidate` is an SDP candidate line without the
`a=` prefix.  A C callback receives `(objPeerConnection *, CSTRING, APTR)`, with the final argument set to the
callback's metadata pointer.  Default host candidates arrive asynchronously after mDNS registration; server-reflexive
and relay candidates may also arrive later.  NO_MDNS permits numeric host callbacks during offer or answer creation.
Set this field before creating an offer or answer and forward candidates through application signalling.

-END-

*********************************************************************************************************************/

static ERR GET_OnIceCandidate(extPeerConnection *Self, FUNCTION * &Value) {
   if (not Self->OnIceCandidate.defined()) return ERR::FieldNotSet;
   Value = &Self->OnIceCandidate;
   return ERR::Okay;
}

static ERR SET_OnIceCandidate(extPeerConnection *Self, FUNCTION *Value) {
   if (Self->OnIceCandidate.defined()) { Self->OnIceCandidate.unpin(); Self->OnIceCandidate.clear(); }
   if (Value) { Self->OnIceCandidate = *Value; Self->OnIceCandidate.pin(); }
   return ERR::Okay;
}


/*********************************************************************************************************************
-FIELD-
RestartRequired: Nonzero when an interface change requires an application-signalled ICE restart.

In Tiri, compare `Peer.restartRequired != 0`; zero means no pending request.
The owner task detects link and address changes, coalescing bursts for 250 milliseconds (at most one second).
Unsupported subscriptions use two-second polling.  Changed interfaces conservatively retire all local routes;
recovery retains the certificate, DTLS session, SCTP association, channel handles and accepted reliable queues.
Call #RestartIce() while signalling is stable, wait for gathering or forward trickle candidates, and exchange
and commit the offer and answer.  A remote restart offer can also satisfy the request.  This field clears only
when a newer negotiated ICE generation selects a usable pair.  Without recovery, path loss fails after 30 seconds.
-END-
*********************************************************************************************************************/
static ERR GET_RestartRequired(extPeerConnection *Self, int *Value) {
   *Value = Self->Connection and Self->Connection->restart_required();
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
OnRestartRequired: Deferred notification that the application must signal an ICE restart.

Tiri receives `(PeerConnection, Generation)`.  C receives `(objPeerConnection *, int64_t, APTR)` with callback
metadata last.  Generation identifies the retired local candidate generation.  #RestartRequired is set before
notification.  Bursts coalesce into one pending request; a late-installed callback receives that request.
No offer is created automatically.  Keep the request pending during unstable signalling; automatic glare
handling is outside this API.  Close and destruction cancel notification.
-END-
*********************************************************************************************************************/
static ERR GET_OnRestartRequired(extPeerConnection *Self, FUNCTION * &Value) {
   if (not Self->OnRestartRequired.defined()) return ERR::FieldNotSet;
   Value = &Self->OnRestartRequired;
   return ERR::Okay;
}
static ERR SET_OnRestartRequired(extPeerConnection *Self, FUNCTION *Value) {
   if (Self->OnRestartRequired.defined()) { Self->OnRestartRequired.unpin(); Self->OnRestartRequired.clear(); }
   if (Value) { Self->OnRestartRequired = *Value; Self->OnRestartRequired.pin(); }
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
ReceiveLimit: Constructor-only receive capacity in bytes, default 262144, range 1 to 16777216.

The local SDP advertises this capacity.  Sending uses the remote peer's independent limit, capped at 16 MiB.
-END-
*********************************************************************************************************************/

/*********************************************************************************************************************
-METHOD-
CreateWebChannel: Reserves a child channel or opens one on the current association without SDP renegotiation.

The returned object belongs to this PeerConnection.  Dropping a Tiri reference leaves it alive; explicitly freeing it
initiates stream reset and suppresses its callbacks.  Freeing the parent destroys all children.
Before DTLS role selection, an automatic ID remains -1.  Once authenticated SCTP connects, local in-band channels
receive even IDs for a DTLS client and odd IDs for a server.  Explicit IDs are reserved immediately; a later parity
or stream-count failure reports OnError followed by OnClose.  Constructor options become read-only child fields.
In Tiri, import 'net/webrtc' and use webRTC.createChannel(Peer, Label, Protocol, Ordered, Negotiated, ID,
MaxRetransmits, MaxPacketLifeTime) for defaults.  Direct mtCreateWebChannel calls require all seven inputs.

-INPUT-
strview Label: UTF-8 label, default empty, at most 65535 bytes.
strview Protocol: UTF-8 protocol, default empty, at most 65535 bytes.
int Ordered: True for ordered delivery (default).
int Negotiated: False for DCEP (default); true requires an explicit ID on both peers.
int ID: -1 for automatic assignment (default), otherwise 0 to 65534.
int MaxRetransmits: -1 omitted (default), otherwise 0 to 65535.  Zero allows no retransmissions.
int MaxPacketLifeTime: -1 omitted (default), otherwise 0 to 65535 milliseconds from enqueue; zero expires unsent.
&obj(WebChannel) Channel: Owned child handle; does not transfer ownership to the caller.

-ERRORS-
Okay
InvalidState
InvalidValue: Both reliability options supplied, an ID collision or known parity mismatch.
InvalidData: Invalid UTF-8.
OutOfRange
BufferOverflow
NoSupport
-END-
*********************************************************************************************************************/

static ERR PEERCONNECTION_CreateWebChannel(extPeerConnection *Self, struct rtc::CreateWebChannel *Args) {
   if (not Args) return ERR::NullArgs;
   Args->Channel = nullptr;
   if (not Self->Connection or Self->collecting()) return ERR::InvalidState;
   if (Args->ID < -1 or Args->ID > 65534 or Args->MaxRetransmits < -1 or Args->MaxRetransmits > 65535 or
       Args->MaxPacketLifeTime < -1 or Args->MaxPacketLifeTime > 65535) return ERR::OutOfRange;
   if ((Args->Ordered != 0 and Args->Ordered != 1) or (Args->Negotiated != 0 and Args->Negotiated != 1)) {
      return ERR::InvalidValue;
   }
   rtc::SctpChannelOptions options;
   options.label = Args->Label; options.protocol = Args->Protocol;
   options.ordered = Args->Ordered; options.negotiated = Args->Negotiated;
   if (Args->ID >= 0) options.id = uint16_t(Args->ID);
   if (Args->MaxRetransmits >= 0) options.reliability.max_retransmits = uint32_t(Args->MaxRetransmits);
   if (Args->MaxPacketLifeTime >= 0) options.reliability.max_lifetime_ms = uint32_t(Args->MaxPacketLifeTime);
   uint64_t handle;
   auto error = Self->Connection->create_channel(options, handle);
   if (error != ERR::Okay) return error;
   extWebChannel *channel;
   error = make_web_channel(Self, handle, channel);
   if (error != ERR::Okay) { Self->Connection->detach_channel(handle); return error; }
   Args->Channel = channel;
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
OnWebChannel: Receives remote in-band channels before their OnOpen and message callbacks.

Tiri receives (PeerConnection, WebChannel); C receives (objPeerConnection *, objWebChannel *, APTR Meta).
Install callbacks on the child here.  Explicitly negotiated channels do not produce this event.
-END-
*********************************************************************************************************************/

static ERR GET_OnWebChannel(extPeerConnection *Self, FUNCTION * &Value) {
   if (not Self->OnWebChannel.defined()) return ERR::FieldNotSet;
   Value = &Self->OnWebChannel; return ERR::Okay;
}
static ERR SET_OnWebChannel(extPeerConnection *Self, FUNCTION *Value) {
   if (Self->OnWebChannel.defined()) { Self->OnWebChannel.unpin(); Self->OnWebChannel.clear(); }
   if (Value) { Self->OnWebChannel = *Value; Self->OnWebChannel.pin(); }
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
IceServers: Configures STUN and TURN servers through a constructor-only JSON array.

Supply objects containing url, username and credential strings.  Supported URLs are stun:host:port,
turn:host:port?transport=udp, turn:host:port?transport=tcp and turns:host:port?transport=tcp.
IPv6 literals require brackets.  STUN/TURN default to port 3478; TURNS defaults to 5349.
TURN requires nonempty credentials.  STUN credentials are unsupported.  The array is limited to eight servers
and 16384 UTF-8 bytes.  Unknown or duplicate keys, invalid types and invalid URLs fail initialisation.

A nonempty array takes precedence over #StunServer; an empty array retains the legacy fallback.
This field cannot be read.  Credentials are copied privately and never included in SDP or candidate callbacks.
Gathering continues asynchronously and a failed server does not discard routes from successful servers.
-END-
*********************************************************************************************************************/
static ERR SET_IceServers(extPeerConnection *Self, const std::string_view &Value) {
   if (Self->Connection) return ERR::InvalidState;
   std::vector<rtc::IceServer> parsed;
   auto error = rtc::parse_ice_servers(Value, parsed);
   if (error IS ERR::Okay) Self->ServerConfiguration.assign(Value);
   return error;
}

/*********************************************************************************************************************
-FIELD-
Flags: Configures the constructor-only ICE policy.
-END-
*********************************************************************************************************************/

/*********************************************************************************************************************
-METHOD-
GetStats: Returns an immutable versioned JSON statistics snapshot.

Calls are valid from initialisation through Close.  Each call returns a copied UTF-8 document; subsequent calls,
restart and destruction cannot change it.  C++ callers release the result with FreeResource(); Tiri receives a string.
Version 1 uses monotonic microseconds for timestampUs, iceRttUs and SCTP rtoUs, and bytes for cwndBytes/pathMtuBytes.
Timestamp, generation and cumulative counters are decimal strings to preserve 64-bit precision in JSON clients.
Unavailable selectedRoute, route, iceRttUs and sctp values are NULL, including the selected path after close.

Connection counters preserve accepted inner datagram bytes/packets across restart and close.  They include ICE,
consent, DTLS headers/control, application data and retransmissions, excluding rejected input and failed sends.
TURN byte totals include framing and maintenance at the Network adapter boundary, excluding IP/TCP/TLS headers.
A partial stream write counts each accepted byte once.  A selected route reports inner totals for that local/peer
pair and outer totals for its allocation (which may also serve other checks).  Route IDs are opaque and change on
restart.  Privacy policies filter endpoint identities exactly as signalling does; credentials are never returned.

-INPUT-
!str Stats: Allocated UTF-8 JSON snapshot; release with FreeResource().
-ERRORS-
Okay
NullArgs
NotInitialised
AllocMemory
InUse
-END-
*********************************************************************************************************************/

static ERR PEERCONNECTION_GetStats(extPeerConnection *Self, struct rtc::GetStats *Args) {
   if (not Args) return ERR::NullArgs;
   Args->Stats = nullptr;
   if (not Self->Connection) return ERR::NotInitialised;
   auto text = Self->Connection->stats();
   if (AllocResource(text.size() + 1, MEM::NO_CLEAR, (APTR *)&Args->Stats, nullptr) != ERR::Okay) return ERR::AllocMemory;
   // Results belong to the caller, rather than the peer action's current allocation context.
   auto error = TrackResource(GetMemoryID(Args->Stats), nullptr, 0, nullptr);
   if (error != ERR::Okay) { FreeResource(Args->Stats); Args->Stats = nullptr; return error; }
   std::memcpy(Args->Stats, text.c_str(), text.size() + 1);
   return ERR::Okay;
}

#include "class_peer_connection_def.c"

static const FieldArray clPeerConnectionFields[] = {
   { "LocalDescription", FDF_CPPSTRING|FDF_R },
   { "RemoteDescription", FDF_CPPSTRING|FDF_R },
   { "CertificateFingerprint", FDF_CPPSTRING|FDF_R },
   { "StunServer", FDF_CPPSTRING|FDF_RW },
   { "IceServers", FDF_VIRTUAL|FDF_CPPSTRING|FDF_I, nullptr, SET_IceServers },
   { "ClientData", FDF_POINTER|FDF_RW },
   { "State", FDF_INT|FDF_LOOKUP|FDF_R, nullptr, nullptr, &clPeerConnectionState },
   { "SignalingState", FDF_INT|FDF_LOOKUP|FDF_R, nullptr, nullptr, &clPeerConnectionSignalingState },
   { "IceState", FDF_INT|FDF_LOOKUP|FDF_R, nullptr, nullptr, &clPeerConnectionIceState },
   { "GatheringState", FDF_INT|FDF_LOOKUP|FDF_R, nullptr, nullptr, &clPeerConnectionGatheringState },
   { "Error", FDF_ERROR|FDF_R },
   { "ReceiveLimit", FDF_INT|FDF_RI },
   { "Flags", FDF_INT|FDF_FLAGS|FDF_RI, nullptr, nullptr, &clPeerConnectionFlags },
   { "Feedback", FDF_VIRTUAL|FDF_FUNCTION|FDF_RW, GET_Feedback, SET_Feedback },
   { "OnIceCandidate", FDF_VIRTUAL|FDF_FUNCTION|FDF_RW, GET_OnIceCandidate, SET_OnIceCandidate },
   { "RestartRequired", FDF_VIRTUAL|FDF_INT|FDF_R, GET_RestartRequired },
   { "OnRestartRequired", FDF_VIRTUAL|FDF_FUNCTION|FDF_RW, GET_OnRestartRequired, SET_OnRestartRequired },
   { "OnWebChannel", FDF_VIRTUAL|FDF_FUNCTION|FDF_RW, GET_OnWebChannel, SET_OnWebChannel },
   END_FIELD
};

static ERR add_peer_connection_class() {
   clPeerConnection = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::PEERCONNECTION),
      fl::ClassVersion(VER_PEERCONNECTION),
      fl::Name("PeerConnection"),
      fl::Category(CCF::NETWORK),
      fl::Actions(clPeerConnectionActions),
      fl::Methods(clPeerConnectionMethods),
      fl::Fields(clPeerConnectionFields),
      fl::Size(sizeof(extPeerConnection)),
      fl::Path(MOD_PATH));
   return clPeerConnection ? ERR::Okay : ERR::AddClass;
}
