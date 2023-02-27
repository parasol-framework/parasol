
#include "defs.h"

static std::unordered_map<LONG, InputCallback> glInputCallbacks;
EventBuffer glInputEvents;

/*********************************************************************************************************************

-FUNCTION-
GetInputTypeName: Returns the string name for an input type.

This function converts JET integer constants to their string equivalent.

-INPUT-
int(JET) Type: JET type integer.

-RESULT-
cstr: A string describing the input type is returned or NULL if the Type is invalid.

*********************************************************************************************************************/

CSTRING gfxGetInputTypeName(LONG Type)
{
   if ((Type < 1) or (Type >= JET_END)) return NULL;
   return glInputNames[Type];
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
ERROR consume_input_events(const InputEvent *Events, LONG Handle)
{
   for (auto event=Events; event; event=event->Next) {
      if ((event->Flags & JTYPE_BUTTON) and (event->Value > 0)) {
         process_click(Self, event->RecipientID, event->X, event->Y);
      }
   }

   return ERR_Okay;
}
</pre>

All processable events are referenced in the InputEvent structure in the Events parameter.  The structure format is as
follows:

<fields>
<fld type="*InputEvent" name="Next">The next input event in the list.</>
<fld type="UWORD" name="Type">This value is set to a JET constant that describes the input event.</>
<fld type="UWORD" name="Flags">Flags provide a broad description of the event type and can also provide more specific information relevant to the event (see JTYPE flags).</>
<fld type="DOUBLE" name="Value">The value associated with the Type</>
<fld type="OBJECTID" name="RecipientID">The surface that the input message is being conveyed to.</>
<fld type="OBJECTID" name="OverID">The surface that was directly under the mouse pointer at the time of the event.</>
<fld type="DOUBLE" name="AbsX">Absolute horizontal coordinate of the mouse pointer (relative to the top left of the display).</>
<fld type="DOUBLE" name="AbsY">Absolute vertical coordinate of the mouse pointer (relative to the top left of the display).</>
<fld type="DOUBLE" name="OverX">Horizontal pointer coordinate, usually relative to the surface that the pointer is positioned over.  If a mouse button is held or the pointer is anchored, the coordinates are relative to the Recipient surface.</>
<fld type="DOUBLE" name="OverY">Vertical pointer coordinate.</>
<fld type="LARGE" name="Timestamp">Millisecond counter at which the input was recorded, or as close to it as possible.</>
<fld type="OBJECTID" name="DeviceID">Reference to the hardware device that this event originated from.  There is no guarantee that the DeviceID is a reference to a publicly accessible object.</>
</>

JET constants are as follows and take note of `ENTERED_SURFACE` and `LEFT_SURFACE` which are software generated and not
a device event:

<types lookup="JET"/>

The JTYPE values for the Flags field are as follows.  Note that these flags also serve as input masks for the
SubscribeInput() function, so to receive a message of the given type the appropriate JTYPE flag must have been set in the
original subscription call.

<types lookup="JTYPE"/>

-INPUT-
ptr(func) Callback: Reference to a callback function that will receive input messages.
oid SurfaceFilter: Optional.  Only the input messages that match the given surface ID will be received.
int(JTYPE) Mask: Combine JTYPE flags to define the input messages required by the client.  Set to 0xffffffff if all messages are desirable.
oid DeviceFilter: Optional.  Only the input messages that match the given device ID will be received.  NOTE - Support not yet implemented, set to zero.
&int Handle: A handle for the subscription is returned here.

-ERRORS-
Okay:
NullArgs:

*********************************************************************************************************************/

ERROR gfxSubscribeInput(FUNCTION *Callback, OBJECTID SurfaceFilter, LONG InputMask, OBJECTID DeviceFilter, LONG *Handle)
{
   static LONG counter = 1;
   pf::Log log(__FUNCTION__);

   if ((!Callback) or (!Handle)) return log.warning(ERR_NullArgs);

   log.branch("Surface Filter: #%d, Mask: $%.4x", SurfaceFilter, InputMask);

   const std::lock_guard<std::recursive_mutex> lock(glInputLock);

   *Handle = counter++;

   const InputCallback is = {
      .SurfaceFilter = SurfaceFilter,
      .InputMask     = (!InputMask) ? WORD(0xffff) : WORD(InputMask),
      .Callback      = *Callback
   };

   glInputCallbacks.emplace(*Handle, is);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
UnsubscribeInput: Removes an input subscription.

This function removes an input subscription that has been created with ~SubscribeInput().

-INPUT-
int Handle: Reference to a handle returned by SubscribeInput().

-ERRORS-
Okay
NullArgs
NotFound
-END-

*********************************************************************************************************************/

ERROR gfxUnsubscribeInput(LONG Handle)
{
   pf::Log log(__FUNCTION__);

   if (!Handle) return log.warning(ERR_NullArgs);

   log.branch("Handle: %d", Handle);

   const std::lock_guard<std::recursive_mutex> lock(glInputLock);

   auto it = glInputCallbacks.find(Handle);
   if (it IS glInputCallbacks.end()) return log.warning(ERR_NotFound);
   else glInputCallbacks.erase(it);

   return ERR_Okay;
}

//********************************************************************************************************************
// This routine is called on every cycle of ProcessMessages() so that we can check if there are input events
// that need to be processed.
//
// Input events are sent to each subscriber as a dynamically constructed linked-list of filtered input events.

void input_event_loop(HOSTHANDLE FD, APTR Data) // Data is not defined
{
   glInputLock.lock();

   if (glInputEvents.empty()) {
      glInputLock.unlock();
      return;
   }

   // retarget() ensures that incoming input events during callback will target a secondary buffer and prevent an
   // unstable std::vector

   auto events = glInputEvents.retarget();

   for (const auto & [ handle, sub ] : glInputCallbacks) {
      InputEvent *last = NULL, *first = NULL;
      for (auto &event : events) {
         if (((event.RecipientID IS sub.SurfaceFilter) or (!sub.SurfaceFilter)) and (event.Flags & sub.InputMask)) {
            if (last) last->Next = &event;
            else first = &event;
            last = &event;
         }
      }

      if (first) {
         last->Next = NULL;

         glInputLock.unlock();

         auto &cb = sub.Callback;
         if (cb.Type IS CALL_STDC) {
            pf::ScopedObjectLock lock(cb.StdC.Context, 2000); // Ensure that the object can't be removed until after input processing
            if (lock.granted()) {
               pf::SwitchContext ctx(cb.StdC.Context);
               auto func = (ERROR (*)(InputEvent *, LONG))cb.StdC.Routine;
               func(first, handle);
            }
         }
         else if (cb.Type IS CALL_SCRIPT) {
            const ScriptArg args[] = {
               { "Events:InputEvent", FD_PTR|FDF_STRUCT, { .Address  = first } },
               { "Handle", FD_LONG, { .Long = handle } },
            };
            ERROR result;
            scCallback(cb.Script.Script, cb.Script.ProcedureID, args, ARRAYSIZE(args), &result);
         }

         glInputLock.lock();
      }
   }

   glInputLock.unlock();
}

