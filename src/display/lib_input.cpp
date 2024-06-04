/*********************************************************************************************************************

-CATEGORY-
Name: Input
-END-

*********************************************************************************************************************/

#include "defs.h"

static std::unordered_map<LONG, InputCallback> glInputCallbacks;
static std::vector<std::pair<LONG, InputCallback>> glNewSubscriptions;
EventBuffer glInputEvents;
namespace gfx{
/*********************************************************************************************************************

-FUNCTION-
GetInputTypeName: Returns the string name for an input type.

This function converts `JET` integer constants to their string equivalent.

-INPUT-
int(JET) Type: JET type integer.

-RESULT-
cstr: A string describing the input `Type` is returned, or `NULL` if the `Type` is invalid.

*********************************************************************************************************************/

CSTRING GetInputTypeName(JET Type)
{
   if ((LONG(Type) < 1) or (LONG(Type) >= LONG(JET::END))) return NULL;
   return glInputNames[LONG(Type)];
}

/*********************************************************************************************************************

-FUNCTION-
SubscribeInput: Subscribe to incoming input messages for any active surface object.

The SubscribeInput() function provides a systematic way of receiving input events as they occur.  Coverage is limited
to device events that are linked to the display (i.e. events from track pads, mouse pointers, graphics tablets and
touch screens).  Keyboard devices are not included.

The client is required to remove the subscription with ~UnsubscribeInput() once tracking is no longer required.

Input events can be filtered so that they are received in relation to surfaces and devices.  An input mask can also be
applied so that only certain types of events are received.

A callback is required for receiving the input events.  The following C++ code illustrates a method for processing
events in the callback:

<pre>
ERR consume_input_events(const InputEvent *Events, LONG Handle)
{
   for (auto e=Events; e; e=e->Next) {
      if (((e->Flags & JTYPE::BUTTON) != JTYPE::NIL) and (e->Value > 0)) {
         process_click(Self, e->RecipientID, e->X, e->Y);
      }
   }

   return ERR::Okay;
}
</pre>

All processable events are referenced in the !InputEvent structure in the `Events` parameter.

`JET` constants are as follows and take note of `CROSSED_IN` and `CROSSED_OUT` which are software generated and not
a device event:

<types lookup="JET"/>

The `JTYPE` values for the `Flags` field are as follows.  Note that these flags also serve as input masks for the
SubscribeInput() function, so to receive a message of the given type the appropriate `JTYPE` flag must have been set in the
original subscription call.

<types lookup="JTYPE"/>

-INPUT-
ptr(func) Callback: Reference to a callback function that will receive input messages.
oid SurfaceFilter: Optional.  Only the input messages that match the given @Surface ID will be received.
int(JTYPE) Mask: Combine #JTYPE flags to define the input messages required by the client.  Set to `0xffffffff` if all messages are required.
oid DeviceFilter: Optional.  Only the input messages that match the given device ID will be received.  NOTE - Support not yet implemented, set to zero.
&int Handle: A handle for the subscription is returned here.

-ERRORS-
Okay:
NullArgs:

*********************************************************************************************************************/

ERR SubscribeInput(FUNCTION *Callback, OBJECTID SurfaceFilter, JTYPE InputMask, OBJECTID DeviceFilter, LONG *Handle)
{
   static LONG counter = 1;
   pf::Log log(__FUNCTION__);

   if ((!Callback) or (!Handle)) return log.warning(ERR::NullArgs);

   log.branch("Surface Filter: #%d, Mask: $%.4x", SurfaceFilter, LONG(InputMask));

   const std::lock_guard<std::recursive_mutex> lock(glInputLock);

   *Handle = counter++;

   const InputCallback is = {
      .SurfaceFilter = SurfaceFilter,
      .InputMask     = (InputMask IS JTYPE::NIL) ? JTYPE(0xffff) : JTYPE(InputMask),
      .Callback      = *Callback
   };

   if (glInputEvents.processing) glNewSubscriptions.push_back(std::make_pair(*Handle, is));
   else glInputCallbacks.emplace(*Handle, is);

   return ERR::Okay;
}

/*********************************************************************************************************************

-FUNCTION-
UnsubscribeInput: Removes an input subscription.

This function removes an input subscription that has been created with ~SubscribeInput().

-INPUT-
int Handle: Reference to a handle returned by ~SubscribeInput().

-ERRORS-
Okay
NullArgs
NotFound
-END-

*********************************************************************************************************************/

ERR UnsubscribeInput(LONG Handle)
{
   pf::Log log(__FUNCTION__);

   if (!Handle) return log.warning(ERR::NullArgs);

   log.branch("Handle: %d", Handle);

   const std::lock_guard<std::recursive_mutex> lock(glInputLock);

   auto it = glInputCallbacks.find(Handle);
   if (it IS glInputCallbacks.end()) return log.warning(ERR::NotFound);
   else {
      if (glInputEvents.processing) { // Cannot erase during input processing
         ClearMemory(&it->second, sizeof(it->second));
      }
      else glInputCallbacks.erase(it);
   }

   return ERR::Okay;
}

} // namespace

//********************************************************************************************************************
// This routine is called on every cycle of ProcessMessages() so that we can check if there are input events
// that need to be processed.
//
// Input events are sent to each subscriber as a dynamically constructed linked-list of filtered input events.

void input_event_loop(HOSTHANDLE FD, APTR Data) // Data is not defined
{
   glInputLock.lock();

   if ((glInputEvents.empty()) or (glInputEvents.processing)) {
      glInputLock.unlock();
      return;
   }

   // retarget() ensures that incoming input events during callback will target a secondary buffer and prevent an
   // unstable std::vector

   auto events = glInputEvents.retarget();

   glInputEvents.processing = true;

   for (const auto & [ handle, sub ] : glInputCallbacks) {
      InputEvent *last = NULL, *first = NULL;
      for (auto &event : events) {
         if (((event.RecipientID IS sub.SurfaceFilter) or (!sub.SurfaceFilter)) and ((event.Flags & sub.InputMask) != JTYPE::NIL)) {
            if (last) last->Next = &event;
            else first = &event;
            last = &event;
         }
      }

      if (first) {
         last->Next = NULL;

         glInputLock.unlock();

         auto &cb = sub.Callback;
         if (sub.Callback.isC()) {
            pf::ScopedObjectLock lock(cb.Context, 2000); // Ensure that the object can't be removed until after input processing
            if (lock.granted()) {
               pf::SwitchContext ctx(cb.Context);
               auto func = (ERR (*)(InputEvent *, LONG, APTR))cb.Routine;
               func(first, handle, cb.Meta);
            }
         }
         else if (sub.Callback.isScript()) {
            sc::Call(sub.Callback, std::to_array<ScriptArg>({
               { "Events:InputEvent", first, FD_PTR|FDF_STRUCT },
               { "Handle", handle }
            }));
         }

         glInputLock.lock();
      }
   }

   glInputEvents.processing = false;

   if (!glNewSubscriptions.empty()) {
      for (auto &sub : glNewSubscriptions) glInputCallbacks[sub.first] = sub.second;
      glNewSubscriptions.clear();
   }

   glInputLock.unlock();
}

