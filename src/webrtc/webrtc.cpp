/*********************************************************************************************************************

-MODULE-
WebRTC: Establishes native peer connections over ICE and DTLS.

Negotiates one bundled data-channel media section and exchanges bounded text and binary messages through WebChannel.

-END-

*********************************************************************************************************************/

#include <kotuku/main.h>
#include <kotuku/modules/network.h>
#include <kotuku/modules/webrtc.h>
#include <kotuku/modules/module.h>
#include <kotuku/modules/script.h>

#include <memory>
#include <cstring>

#include "peer_connection.h"

JUMPTABLE_CORE
JUMPTABLE_NETWORK

static OBJECTPTR modNetwork = nullptr;
static OBJECTPTR clPeerConnection = nullptr;
static OBJECTPTR glWebChannelClass = nullptr;
static MSGID glRtcFreeMsgID = MSGID::NIL;
static MsgHandler *glRtcFreeHandle = nullptr;

#include "class_peer_connection.cpp"

static ERR MODInit(OBJECTPTR, struct CoreBase *Core) {
   CoreBase = Core;
   if (objModule::load("network", &modNetwork, &NetworkBase) != ERR::Okay) return ERR::InitModule;
   glRtcFreeMsgID = MSGID(AllocateID(IDTYPE::MESSAGE));
   auto handler = C_FUNCTION(rtc_free_message);
   if (AddMsgHandler(glRtcFreeMsgID, &handler, &glRtcFreeHandle) != ERR::Okay) return ERR::InitModule;
   auto error = add_web_channel_class();
   return error IS ERR::Okay ? add_peer_connection_class() : error;
}

static ERR MODExpunge() {
   if (clPeerConnection) { FreeResource(clPeerConnection); clPeerConnection = nullptr; }
   if (glWebChannelClass) { FreeResource(glWebChannelClass); glWebChannelClass = nullptr; }
   if (glRtcFreeHandle) { FreeResource(glRtcFreeHandle); glRtcFreeHandle = nullptr; }
   if (modNetwork) { FreeResource(modNetwork); modNetwork = nullptr; }
   return ERR::Okay;
}

KOTUKU_MOD(MODInit, nullptr, nullptr, MODExpunge, nullptr, MOD_IDL, nullptr)
extern "C" struct ModHeader * register_webrtc_module() { return &ModHeader; }
