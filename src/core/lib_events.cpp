/*********************************************************************************************************************

The source code of the Parasol Framework is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

-CATEGORY-
Name: Events
-END-

*********************************************************************************************************************/

#include <stdlib.h>

#include "defs.h"

static const std::array<CSTRING, LONG(EVG::END)> glEventGroups = {
   NULL,
   "filesystem",
   "network",
   "system",
   "gui",
   "display",
   "io",
   "hardware",
   "audio",
   "user",
   "power",
   "class",
   "app",
   "android"
};

struct eventsub {
   struct eventsub *Next;
   struct eventsub *Prev;
   EVENTID  EventID;
   EVENTID  EventMask;
   void     (*Callback)(APTR Custom, APTR Info, LONG Size, APTR Meta);
   APTR     CallbackMeta;
   EVG      Group;
   UBYTE    Called;
   OBJECTID ContextID;
   APTR     Custom;

   inline CSTRING groupName() {
      return glEventGroups[UBYTE(Group)];
   }
};

static struct eventsub *glEventList = NULL;
static UBYTE glCallSignal = 0;
static bool glEventListAltered = false;

static std::unordered_map<ULONG, std::string> glEventNames;

//********************************************************************************************************************

void free_events(void)
{
   pf::Log log("Core");

   log.function("Freeing the event list.");

   eventsub *event = glEventList;
   while (event) {
      log.trace("Freeing event %p", event);
      eventsub *next = event->Next;
      free(event);
      event = next;
   }

   glEventList = NULL;
}

/*********************************************************************************************************************

-FUNCTION-
BroadcastEvent: Broadcast an event to all event listeners in the system.

Use BroadcastEvent() to broadcast an event to all listeners for that event in the system.  An Event structure is
required that must start with a 64-bit EventID acquired from ~GetEventID(), followed by any required data that is
relevant to that event.  The C/C++ template is as follows:

<pre>
typedef struct rkEvent {
   EVENTID EventID;
   // Data follows
} rkEvent;
</pre>

This document does not describe the available system events.  For more information about them, please refer to the
System Events Reference manual.

-INPUT-
ptr Event: Pointer to an event structure.
int EventSize: The size of the Event structure, in bytes.

-ERRORS-
Okay
NullArgs

*********************************************************************************************************************/

ERR BroadcastEvent(APTR Event, LONG EventSize)
{
   pf::Log log(__FUNCTION__);

   if ((!Event) or ((size_t)EventSize < sizeof(rkEvent))) return ERR::NullArgs;

   LONG groupmask = 1<<((((rkEvent *)Event)->EventID>>56) & 0xff);

   if (glEventMask & groupmask) {
      log.trace("Broadcasting event $%.8x%.8x",
         (ULONG)(((rkEvent *)Event)->EventID>>32 & 0xffffffff),
         (ULONG)(((rkEvent *)Event)->EventID));
      SendMessage(MSGID_EVENT, MSF::NIL, Event, EventSize);
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FUNCTION-
GetEventID: Generates unique event ID's suitable for event broadcasting.

Use GetEventID() to generate a 64-bit event identifier.  This identifier can be used for broadcasting and subscribing
to events.  Events are described in three parts - Group, SubGroup and the Event name, or in string format
`group.subgroup.event`.

The Group is strictly limited to one of the following definitions:

<types lookup="EVG"/>

The SubGroup and Event parameters are string-based and there are no restrictions on naming.  If a SubGroup or Event
name is NULL, this will act as a wildcard for subscribing to multiple events.  For example, subscribing to the network
group with SubGroup and Event set to NULL will allow for a subscription to all network events that are broadcast.  A
Group setting of zero is not allowed.

-INPUT-
int(EVG) Group: The group to which the event belongs.
cstr SubGroup: The sub-group to which the event belongs (case-sensitive).
cstr Event:    The name of the event (case-sensitive).

-RESULT-
large: The EventID is returned as a 64-bit integer.

*********************************************************************************************************************/

LARGE GetEventID(EVG Group, CSTRING SubGroup, CSTRING Event)
{
   pf::Log log(__FUNCTION__);

   if (Group IS EVG::NIL) return 0;

   auto hash_subgroup = StrHash(SubGroup, true) & 0x00ffffff;
   auto hash_event = StrHash(Event, true);

   LARGE event_id = LARGE(UBYTE(Group))<<56;
   if ((SubGroup) and (SubGroup[0] != '*')) event_id |= LARGE(hash_subgroup)<<32;
   if ((Event) and (Event[0] != '*')) event_id |= hash_event;

   glEventNames[hash_subgroup] = SubGroup;
   glEventNames[hash_event]    = Event;

   log.traceBranch("Group: %d, SubGroup: %s, Event: %s, Result: $%.8x%.8x",
      LONG(Group), SubGroup, Event, ULONG(event_id>>32), ULONG(event_id));

   return event_id;
}

/*********************************************************************************************************************

-FUNCTION-
SubscribeEvent: Subscribe to a system event.

Use the SubscribeEvent() function to listen for system events.  An EventID (obtainable from ~GetEventID())
must be provided, as well as a reference to a function that will be called each time that the event is broadcast.

An event handle will be returned in the Handle parameter to identify the subscription.  This must be retained to later
unsubscribe from the event with the ~UnsubscribeEvent() function.

The format for the Callback function is `Function(APTR Custom, APTR Event, LONG Size)`, where 'Event' is the
event structure that matches to the subscribed EventID.

-INPUT-
large Event:  An event identifier.
ptr(func) Callback: The function that will be subscribed to the event.
ptr Custom:   A custom pointer that will be sent to the callback function, set to NULL if not required.
&ptr Handle:  Pointer to an address that will receive the event handle.

-ERRORS-
Okay
NullArgs
AllocMemory

*********************************************************************************************************************/

ERR SubscribeEvent(LARGE EventID, FUNCTION *Callback, APTR Custom, APTR *Handle)
{
   pf::Log log(__FUNCTION__);

   if ((!Callback) or (!EventID) or (!Handle)) return ERR::NullArgs;

   if (!Callback->isC()) return ERR::Args; // Currently only StdC callbacks are accepted.

   auto gid = EVG(UBYTE(EventID>>56));

   if ((LONG(gid) < 1) or (LONG(gid) >= LONG(EVG::END))) {
      return log.warning(ERR::Args);
   }

   if (auto event = (struct eventsub *)malloc(sizeof(struct eventsub))) {
      LARGE mask = 0xff00000000000000LL;
      if (EventID & 0x00ffffff00000000LL) mask |= 0x00ffffff00000000LL;
      if (EventID & 0x00000000ffffffffLL) mask |= 0x00000000ffffffffLL;

      OBJECTPTR context = CurrentContext();
      event->EventID   = EventID;
      event->Callback  = (void (*)(APTR, APTR, LONG, APTR))Callback->Routine;
      event->CallbackMeta = Callback->Meta;
      event->Group     = gid;
      event->ContextID = context->UID;
      event->Next      = glEventList;
      event->Prev      = NULL;
      event->Custom    = Custom;
      event->EventMask = mask;

      if (glEventList) glEventList->Prev = event;
      glEventList = event;

      glEventMask |= 1<<UBYTE(event->Group);

      auto it_subgroup = glEventNames.find(ULONG(EventID>>32) & 0x00ffffff);
      auto it_name = glEventNames.find(ULONG(EventID));
      if ((it_subgroup != glEventNames.end()) and (it_name != glEventNames.end())) {
         log.function("Handle: %p, Mask: $%.8x, %s.%s.%s",
            event, glEventMask, event->groupName(), it_subgroup->second.c_str(), it_name->second.c_str());
      }
      else log.function("Handle: %p, Mask: $%.8x", event, glEventMask);

      *Handle = event;

      return ERR::Okay;
   }
   else return ERR::AllocMemory;
}

/*********************************************************************************************************************

-FUNCTION-
UnsubscribeEvent: Removes an event subscription.

Use UnsubscribeEvent() to remove an existing event subscription.  A valid handle returned from the ~SubscribeEvent()
function must be provided.

-INPUT-
ptr Handle: An event handle returned from SubscribeEvent()
-END-

*********************************************************************************************************************/

void UnsubscribeEvent(APTR Handle)
{
   pf::Log log(__FUNCTION__);

   if (!Handle) return;
   if (!glEventList) return; // All events have already been freed (i.e. Core is closing)

   auto event = (eventsub *)Handle;
   auto it_subgroup = glEventNames.find(ULONG(event->EventID>>32) & 0x00ffffff);
   auto it_name = glEventNames.find(ULONG(event->EventID));

   if ((it_subgroup != glEventNames.end()) and (it_name != glEventNames.end())) {
      log.function("Handle: %p, %s.%s.%s", event, event->groupName(), it_subgroup->second.c_str(), it_name->second.c_str());
   }
   else log.function("Handle: %p, Group: %s", event, event->groupName());

   if (event->Prev) event->Prev->Next = event->Next;
   if (event->Next) event->Next->Prev = event->Prev;
   if (event IS glEventList) glEventList = event->Next;

   // All events belong to a group.  Check if this is the last event that belongs to this group, in which case we need
   // to turn off the event group bit.

   auto scan = glEventList;
   while (scan) {
      if (scan->Group IS event->Group) break;
      scan = scan->Next;
   }
   if (!scan) glEventMask = glEventMask & (~(1<<UBYTE(event->Group)));

   free(event);

   glEventListAltered = true;
}

/*********************************************************************************************************************
** ProcessMessages() will call this function whenever a MSGID_EVENT message is received.
*/

ERR msg_event(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   pf::Log log(__FUNCTION__);

   if ((!Message) or ((size_t)MsgSize < sizeof(rkEvent))) return ERR::Okay;

   rkEvent *eventmsg = (rkEvent *)Message;

   log.msg(VLF::DETAIL|VLF::BRANCH, "Event $%.8x%8x has been received.", (LONG)((eventmsg->EventID>>32)& 0xffffffff),
      (LONG)(eventmsg->EventID & 0xffffffff));

   struct eventsub *event;
   glCallSignal++;
restart:
   event = glEventList;
   while (event) {
      if (event->Called IS glCallSignal);
      else if ((eventmsg->EventID & event->EventMask) IS event->EventID) {
         log.trace("Found listener %p for this event.", event->Callback);

         event->Called = glCallSignal;

         glEventListAltered = false;

         pf::ScopedObjectLock lock(event->ContextID, 3000);
         if (lock.granted()) {
            pf::SwitchContext ctx(lock.obj);
            event->Callback(event->Custom, Message, MsgSize, event->CallbackMeta);
         }

         if (glEventListAltered) goto restart;
      }

      event = event->Next;
   }

   return ERR::Okay;
}
