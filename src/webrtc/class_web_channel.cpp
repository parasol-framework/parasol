/*********************************************************************************************************************
-CLASS-
WebChannel: Exchanges complete text or binary messages over its parent PeerConnection.

Create channels with PeerConnection.CreateWebChannel; direct construction is unsupported.  Label, Protocol,
Negotiated, Ordered and reliability fields are immutable.  ID is -1 until assigned; State starts CONNECTING.
Local OnOpen follows DCEP acknowledgement, or SCTP establishment for a negotiated channel.  Remote channels report
PeerConnection.OnWebChannel first, then OnOpen, then complete messages.  Process framework messages to drive delivery.
Callbacks run after transport processing, never from Send.  They may send, close or free the channel or parent.
Use this API on the owning event-loop thread; marshal work from other threads.
Explicit free suppresses further notifications; normal closure gives OnClose once; failure gives OnError then OnClose.
-END-
*********************************************************************************************************************/

class extWebChannel : public objWebChannel {
public:
   extPeerConnection *Parent = nullptr;
   uint64_t Handle = 0;
   int ID = -1;
   int64_t BufferedAmount = 0, MaxMessageSize = 0;
   FUNCTION OnOpen, OnMessage, OnClose, OnError, OnBufferedAmountLow;
   bool OpenReported = false, CloseReported = false, ErrorReported = false;
   extWebChannel(objMetaClass *ClassPtr, OBJECTID ObjectID) : objWebChannel(ClassPtr, ObjectID) {
      ID = -1; State = WCS::CONNECTING; Error = ERR::Okay;
      BufferedAmount = 0; BufferedAmountLowThreshold = 0; MaxMessageSize = 0;
      Negotiated = 0; Ordered = 1; MaxRetransmits = MaxPacketLifeTime = -1;
   }
   ~extWebChannel() {
      if (Parent) {
         Parent->Channels.erase(Handle);
         if (Parent->Connection) Parent->Connection->detach_channel(Handle);
      }
      for (auto field : {&OnOpen, &OnMessage, &OnClose, &OnError, &OnBufferedAmountLow}) {
         if (field->defined()) { field->unpin(); field->clear(); }
      }
   }
};

extPeerConnection::~extPeerConnection() {
   if (TimerHandle) { UpdateTimer(TimerHandle, 0); TimerHandle = nullptr; }
   for (auto &[handle, child] : Channels) child->Parent = nullptr;
   Connection.reset();
   for (auto field : {&Feedback, &OnIceCandidate, &OnWebChannel, &OnRestartRequired}) {
      if (field->defined()) { field->unpin(); field->clear(); }
   }
}

static ERR rtc_free_message(APTR, int, MSGID, std::span<std::byte> Message) {
   if (Message.size() IS sizeof(OBJECTID)) {
      OBJECTID id;
      std::memcpy(&id, Message.data(), sizeof(id));
      FreeResource(id);
   }
   return ERR::Okay;
}
static void rtc_release_pin(Object *Target) {
   bool collect = Target->collecting() and not Target->defined(NF::FREE);
   Target->unpin();
   if (collect and not Target->isPinned()) {
      auto id = Target->UID;
      (void)SendMessage(glRtcFreeMsgID, MSF::NIL, std::span((const int8_t *)&id, sizeof(id)));
   }
}
static ERR WEBCHANNEL_Init(extWebChannel *Self) { return Self->Parent ? ERR::Okay : ERR::InvalidState; }

static ERR make_web_channel(extPeerConnection *Parent, uint64_t Handle, extWebChannel * &Result) {
   Result = nullptr;
   auto record = Parent->Connection->channel(Handle);
   if (not record) return ERR::InvalidState;
   OBJECTPTR object;
   auto error = NewObject(CLASSID::WEBCHANNEL, NF::NIL, &object);
   if (error != ERR::Okay) return error;
   auto child = (extWebChannel *)object;
   child->Parent = Parent; child->Handle = Handle;
   child->Label = record->options.label; child->Protocol = record->options.protocol; child->ID = record->id;
   child->Negotiated = record->options.negotiated; child->Ordered = record->options.ordered;
   child->MaxRetransmits = int(record->options.reliability.max_retransmits.value_or(uint32_t(-1)));
   child->MaxPacketLifeTime = int(record->options.reliability.max_lifetime_ms.value_or(uint32_t(-1)));
   error = SetOwner(child, Parent);
   if (error IS ERR::Okay) error = InitObject(child);
   if (error != ERR::Okay) { FreeResource(child); return error; }
   Parent->Channels[Handle] = child;
   Result = child;
   return ERR::Okay;
}

static void channel_notify(extWebChannel *Self, FUNCTION &Field, const rtc::PublicChannelEvent &Event) {
   if (Self->collecting() or not Self->Parent or Self->Parent->collecting()) return;
   if (Field.stale()) { Field.unpin(); Field.clear(); }
   if (not Field.defined()) return;
   auto callback = Field;
   callback.pin();
   auto lifetime = kt::Defer([&]() { callback.unpin(); });
   ERR error = ERR::Okay;
   if (callback.isC()) {
      kt::SwitchContext context(callback.Context);
      if (Event.type IS rtc::SctpChannelEventType::MESSAGE and not Event.low) {
         auto routine = (ERR (*)(objWebChannel *, const void *, int, WCM, APTR))callback.Routine;
         error = routine(Self, Event.payload.data(), int(Event.payload.size()),
            Event.text ? WCM::TEXT : WCM::BINARY, callback.Meta);
      } else if (Event.type IS rtc::SctpChannelEventType::ERROR) {
         auto routine = (void (*)(objWebChannel *, ERR, APTR))callback.Routine;
         routine(Self, Event.error, callback.Meta);
      } else {
         auto routine = (void (*)(objWebChannel *, APTR))callback.Routine;
         routine(Self, callback.Meta);
      }
   } else if (callback.isScript()) {
      if (Event.type IS rtc::SctpChannelEventType::MESSAGE and not Event.low) {
         std::span<std::byte> span((std::byte *)Event.payload.data(), Event.payload.size());
         sc::Call(callback, std::to_array<ScriptArg>({
            { "WebChannel", Self, FD_OBJECTPTR }, { "Buffer", &span, FDF_SPAN|FD_BYTE },
            { "Type", int(Event.text ? WCM::TEXT : WCM::BINARY) }
         }), error);
      } else if (Event.type IS rtc::SctpChannelEventType::ERROR) {
         sc::Call(callback, std::to_array<ScriptArg>({
            { "WebChannel", Self, FD_OBJECTPTR }, { "Error", int(Event.error) }
         }), error);
      } else sc::Call(callback, std::to_array<ScriptArg>({ { "WebChannel", Self, FD_OBJECTPTR } }), error);
   }
   if (error > ERR::ExceptionThreshold) kt::Log(__FUNCTION__).warning(error);
}
static void peer_report_channels(extPeerConnection *Self) {
   if (Self->Dispatching) return;
   Self->Dispatching = true;
   auto dispatch = kt::Defer([&]() { Self->Dispatching = false; });
   for (auto &[handle, child] : Self->Channels) {
      if (auto record = Self->Connection->channel(handle)) {
         child->ID = record->id; child->BufferedAmount = int64_t(record->buffered);
         child->MaxMessageSize = int64_t(Self->Connection->max_message_size());
         if (record->state IS rtc::SctpChannelState::CLOSING) child->State = WCS::CLOSING;
      }
   }
   auto events = Self->Connection->take_channel_events();
   auto credit = kt::Defer([&]() { Self->Connection->release_channel_events(events); });
   for (auto &event : events) {
      if (Self->collecting()) break;
      auto it = Self->Channels.find(event.handle);
      extWebChannel *child = it IS Self->Channels.end() ? nullptr : it->second;
      if (event.type IS rtc::SctpChannelEventType::REMOTE_OPEN) {
         if (make_web_channel(Self, event.handle, child) != ERR::Okay) {
            Self->Connection->detach_channel(event.handle); continue;
         }
      }
      if (not child or child->collecting()) continue;
      child->pin();
      auto lifetime = kt::Defer([&]() { rtc_release_pin(child); });
      if (event.type IS rtc::SctpChannelEventType::REMOTE_OPEN) {
         if (Self->OnWebChannel.stale()) { Self->OnWebChannel.unpin(); Self->OnWebChannel.clear(); }
         auto callback = Self->OnWebChannel;
         if (callback.defined()) {
            callback.pin();
            auto release = kt::Defer([&]() { callback.unpin(); });
            if (callback.isC()) {
               kt::SwitchContext context(callback.Context);
               auto routine = (void (*)(objPeerConnection *, objWebChannel *, APTR))callback.Routine;
               routine(Self, child, callback.Meta);
            } else sc::Call(callback, std::to_array<ScriptArg>({
               { "PeerConnection", Self, FD_OBJECTPTR }, { "WebChannel", child, FD_OBJECTPTR }
            }));
         }
      } else if (event.type IS rtc::SctpChannelEventType::OPEN) {
         if (child->State != WCS::CONNECTING or child->OpenReported) continue;
         child->State = WCS::OPEN; child->OpenReported = true;
         channel_notify(child, child->OnOpen, event);
      } else if (event.type IS rtc::SctpChannelEventType::ERROR) {
         if (child->ErrorReported or child->CloseReported) continue;
         child->ErrorReported = true;
         child->Error = event.error; child->State = WCS::CLOSED;
         channel_notify(child, child->OnError, event);
      } else if (event.type IS rtc::SctpChannelEventType::CLOSE) {
         if (child->CloseReported) continue;
         child->State = WCS::CLOSED; child->CloseReported = true;
         channel_notify(child, child->OnClose, event);
      } else if (child->State IS WCS::OPEN) {
         channel_notify(child, event.low ? child->OnBufferedAmountLow : child->OnMessage, event);
      }
   }
}

/*********************************************************************************************************************
-METHOD-
Send: Atomically copies a complete message into a bounded, non-blocking send queue.

Requires OPEN.  Tiri accepts strings (including embedded NUL), array<byte>, or nil for empty data; tables and other
arrays are unsupported.  Text must be valid UTF-8.  Queue exhaustion rejects the whole message without changing
BufferedAmount.  A successful return does not imply acknowledgement.  Callbacks are deferred until message processing.

-INPUT-
array(char) Data: Complete immutable message to copy.
int(WCM) Type: TEXT or BINARY.
-ERRORS-
Okay
InvalidState
InvalidValue
InvalidData
OutOfRange
BufferOverflow
-END-
*********************************************************************************************************************/
static ERR WEBCHANNEL_Send(extWebChannel *Self, struct wch::Send *Args) {
   if (not Args) return ERR::NullArgs;
   if (not Self->Parent or Self->Parent->collecting() or Self->collecting() or Self->State != WCS::OPEN) {
      return ERR::InvalidState;
   }
   if (Args->Type != WCM::TEXT and Args->Type != WCM::BINARY) return ERR::InvalidValue;
   auto error = Self->Parent->Connection->send_channel(Self->Handle,
      {(const uint8_t *)Args->Data.data(), Args->Data.size()}, Args->Type IS WCM::TEXT);
   if (auto record = Self->Parent->Connection->channel(Self->Handle)) Self->BufferedAmount = int64_t(record->buffered);
   return error;
}
/*********************************************************************************************************************
-METHOD-
Close: Starts bidirectional stream reset, rejecting new sends immediately.

Idempotent.  Accepted reliable messages drain before reset; partial messages retain their abandonment policy.
Close completes within five seconds or fails the association with Timeout.  Pending unassigned channels close locally.
Normal closure reports OnClose once; failure reports OnError first.  Explicit free suppresses callbacks.
-ERRORS-
Okay
-END-
*********************************************************************************************************************/
static ERR WEBCHANNEL_Close(extWebChannel *Self) {
   if (Self->State IS WCS::CLOSED or Self->State IS WCS::CLOSING) return ERR::Okay;
   if (not Self->Parent) { Self->State = WCS::CLOSED; return ERR::Okay; }
   auto error = Self->Parent->Connection->close_channel(Self->Handle);
   if (error IS ERR::Okay) Self->State = WCS::CLOSING;
   return error;
}

/*********************************************************************************************************************
-FIELD-
ID: Read-only stream ID, or -1 before automatic allocation after authenticated DTLS role selection.
-END-
*********************************************************************************************************************/
static ERR GET_ID(extWebChannel *Self, int &Value) {
   auto record = Self->Parent ? Self->Parent->Connection->channel(Self->Handle) : nullptr;
   Value = record ? record->id : Self->ID; return ERR::Okay;
}
/*********************************************************************************************************************
-FIELD-
MaxMessageSize: Effective directional send capacity, capped at 16 MiB, or zero before remote SDP is committed.

An absent remote maximum defaults to 65536; a remote zero is bounded by the local 16 MiB implementation capacity.
-END-
*********************************************************************************************************************/
static ERR GET_MaxMessageSize(extWebChannel *Self, int64_t &Value) {
   Value = Self->Parent ? int64_t(Self->Parent->Connection->max_message_size()) : 0; return ERR::Okay;
}
/*********************************************************************************************************************
-FIELD-
BufferedAmount: Original application bytes awaiting successful acceptance by the DTLS adapter.

Includes unsent fragments; decreases on adapter acceptance or abandonment/discard.  Excludes DCEP, empty-message filler,
headers and retransmission copies.  Acknowledgement releases a separate retained-memory budget.  Sending is limited
by 16 MiB per-channel and 32 MiB association retained storage, plus finite message and fragment counts.
-END-
*********************************************************************************************************************/
static ERR GET_BufferedAmount(extWebChannel *Self, int64_t &Value) {
   auto record = Self->Parent ? Self->Parent->Connection->channel(Self->Handle) : nullptr;
   Value = record ? int64_t(record->buffered) : 0; return ERR::Okay;
}
/*********************************************************************************************************************
-FIELD-
BufferedAmountLowThreshold: Mutable threshold in bytes, default zero, range 0 to 16777216.

Changing the threshold does not emit a notification.  OnBufferedAmountLow reports only a crossing from above to at or
below it during transport progress or abandonment.  Notifications are suppressed after closing.
-END-
*********************************************************************************************************************/
static ERR SET_BufferedAmountLowThreshold(extWebChannel *Self, int64_t Value) {
   if (Value < 0 or Value > 16777216) return ERR::OutOfRange;
   if (not Self->Parent) return ERR::InvalidState;
   auto error = Self->Parent->Connection->set_threshold(Self->Handle, size_t(Value));
   if (error IS ERR::Okay) Self->BufferedAmountLowThreshold = Value;
   return error;
}

/*********************************************************************************************************************
-FIELD-
OnOpen: Receives successful channel opening once.  Tiri receives (WebChannel); C receives (objWebChannel *, APTR Meta).
-END-
*********************************************************************************************************************/
static ERR GET_OnOpen(extWebChannel *Self, FUNCTION * &Value) {
   if (not Self->OnOpen.defined()) return ERR::FieldNotSet;
   Value = &Self->OnOpen; return ERR::Okay;
}
static ERR SET_OnOpen(extWebChannel *Self, FUNCTION *Value) {
   if (Self->OnOpen.defined()) { Self->OnOpen.unpin(); Self->OnOpen.clear(); }
   if (Value) { Self->OnOpen = *Value; Self->OnOpen.pin(); }
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
OnClose: Receives terminal closure once.  Tiri receives (WebChannel); C receives (objWebChannel *, APTR Meta).
-END-
*********************************************************************************************************************/
static ERR GET_OnClose(extWebChannel *Self, FUNCTION * &Value) {
   if (not Self->OnClose.defined()) return ERR::FieldNotSet;
   Value = &Self->OnClose; return ERR::Okay;
}
static ERR SET_OnClose(extWebChannel *Self, FUNCTION *Value) {
   if (Self->OnClose.defined()) { Self->OnClose.unpin(); Self->OnClose.clear(); }
   if (Value) { Self->OnClose = *Value; Self->OnClose.pin(); }
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
OnError: Receives a channel failure before OnClose.  Tiri receives (WebChannel, Error); C receives
(objWebChannel *, ERR, APTR Meta).  State and Error are updated before notification.
-END-
*********************************************************************************************************************/
static ERR GET_OnError(extWebChannel *Self, FUNCTION * &Value) {
   if (not Self->OnError.defined()) return ERR::FieldNotSet;
   Value = &Self->OnError; return ERR::Okay;
}
static ERR SET_OnError(extWebChannel *Self, FUNCTION *Value) {
   if (Self->OnError.defined()) { Self->OnError.unpin(); Self->OnError.clear(); }
   if (Value) { Self->OnError = *Value; Self->OnError.pin(); }
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
OnMessage: Receives a complete message.  Tiri receives (WebChannel, Buffer, Type).
Buffer is a borrowed array<byte>
valid only during this callback; copy or clone it before retention.  C receives
(objWebChannel *, const void *Data, int Length, WCM Type, APTR Meta), returning ERR.
Callback errors are logged and ignored.  Empty messages have zero application bytes.
-END-
*********************************************************************************************************************/
static ERR GET_OnMessage(extWebChannel *Self, FUNCTION * &Value) {
   if (not Self->OnMessage.defined()) return ERR::FieldNotSet;
   Value = &Self->OnMessage; return ERR::Okay;
}
static ERR SET_OnMessage(extWebChannel *Self, FUNCTION *Value) {
   if (Self->OnMessage.defined()) { Self->OnMessage.unpin(); Self->OnMessage.clear(); }
   if (Value) { Self->OnMessage = *Value; Self->OnMessage.pin(); }
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
OnBufferedAmountLow: Receives downward threshold crossings while OPEN.  Tiri receives (WebChannel); C receives
(objWebChannel *, APTR Meta).  A callback may send again; its subsequent crossing is deferred to a later pump.
-END-
*********************************************************************************************************************/
static ERR GET_OnBufferedAmountLow(extWebChannel *Self, FUNCTION * &Value) {
   if (not Self->OnBufferedAmountLow.defined()) return ERR::FieldNotSet;
   Value = &Self->OnBufferedAmountLow; return ERR::Okay;
}
static ERR SET_OnBufferedAmountLow(extWebChannel *Self, FUNCTION *Value) {
   if (Self->OnBufferedAmountLow.defined()) { Self->OnBufferedAmountLow.unpin(); Self->OnBufferedAmountLow.clear(); }
   if (Value) { Self->OnBufferedAmountLow = *Value; Self->OnBufferedAmountLow.pin(); }
   return ERR::Okay;
}

#include "class_web_channel_def.c"

static const FieldArray clWebChannelFields[] = {
   { "Label", FDF_CPPSTRING|FDF_R },
   { "Protocol", FDF_CPPSTRING|FDF_R },
   { "BufferedAmountLowThreshold", FDF_INT64|FDF_RW, nullptr, SET_BufferedAmountLowThreshold },
   { "ID", FDF_VIRTUAL|FDF_INT|FDF_R, GET_ID },
   { "Negotiated", FDF_INT|FDF_R },
   { "Ordered", FDF_INT|FDF_R },
   { "MaxRetransmits", FDF_INT|FDF_R },
   { "MaxPacketLifeTime", FDF_INT|FDF_R },
   { "State", FDF_INT|FDF_LOOKUP|FDF_R, nullptr, nullptr, &clWebChannelState },
   { "Error", FDF_ERROR|FDF_R },
   { "MaxMessageSize", FDF_VIRTUAL|FDF_INT64|FDF_R, GET_MaxMessageSize },
   { "BufferedAmount", FDF_VIRTUAL|FDF_INT64|FDF_R, GET_BufferedAmount },
   { "OnOpen", FDF_VIRTUAL|FDF_FUNCTION|FDF_RW, GET_OnOpen, SET_OnOpen },
   { "OnClose", FDF_VIRTUAL|FDF_FUNCTION|FDF_RW, GET_OnClose, SET_OnClose },
   { "OnError", FDF_VIRTUAL|FDF_FUNCTION|FDF_RW, GET_OnError, SET_OnError },
   { "OnMessage", FDF_VIRTUAL|FDF_FUNCTION|FDF_RW, GET_OnMessage, SET_OnMessage },
   { "OnBufferedAmountLow", FDF_VIRTUAL|FDF_FUNCTION|FDF_RW, GET_OnBufferedAmountLow, SET_OnBufferedAmountLow },
   END_FIELD
};
static ERR add_web_channel_class() {
   glWebChannelClass = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::WEBCHANNEL), fl::ClassVersion(VER_WEBCHANNEL), fl::Name("WebChannel"),
      fl::Category(CCF::NETWORK), fl::Actions(clWebChannelActions), fl::Methods(clWebChannelMethods),
      fl::Fields(clWebChannelFields), fl::Size(sizeof(extWebChannel)), fl::Path(MOD_PATH));
   return glWebChannelClass ? ERR::Okay : ERR::AddClass;
}
