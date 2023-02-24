
#include "defs.h"

/******************************************************************************

-FUNCTION-
GetInputTypeName: Returns the string name for an input type.

This function converts JET integer constants to their string equivalent.  Refer to ~SubscribeInput() for a
list of JET constants.

-INPUT-
int(JET) Type: JET type integer.

-RESULT-
cstr: A string describing the input type is returned or NULL if the Type is invalid.

******************************************************************************/

CSTRING gfxGetInputTypeName(LONG Type)
{
   if ((Type < 1) or (Type >= JET_END)) return NULL;
   return glInputNames[Type];
}

/******************************************************************************

-FUNCTION-
SubscribeInput: Subscribe to incoming input messages for any active surface object.

The SubscribeInput() function provides a systematic way of receiving input events as they occur.  Coverage is limited
to device events that are linked to the display (i.e. events from track pads, mouse pointers, graphics tablets and
touch screens).  Keyboard devices are not included.

The client is required to remove the subscription with ~UnsubscribeInput() once tracking is no longer required.

Input events can be filtered so that they are received in relation to surfaces and devices.  An input mask can also be
applied so that only certain types of events are received.

A callback is required for receiving the input events.  The following C/C++ code illustrates a method for processing
events in the callback:

<pre>
ERROR consume_input_events(const struct InputEvent *Events, LONG Handle)
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

******************************************************************************/

ERROR gfxSubscribeInput(FUNCTION *Callback, OBJECTID SurfaceFilter, LONG InputMask, OBJECTID DeviceFilter, LONG *Handle)
{
   #define CHUNK_INPUT 50
   pf::Log log(__FUNCTION__);

   if ((!Callback) or (!Handle)) return log.warning(ERR_NullArgs);

   log.branch("Surface Filter: #%d, Mask: $%.4x, Handle: %d", SurfaceFilter, InputMask, glSharedControl->InputIDCounter+1);

   // Allocate the subscription array if it does not exist.  NB: The memory is untracked and will be removed by the
   // last task that cleans up the memory resource pool.

   if (!glSharedControl->InputMID) {
      if (AllocMemory(sizeof(InputSubscription) * CHUNK_INPUT, MEM_PUBLIC|MEM_UNTRACKED, NULL, &glSharedControl->InputMID)) {
         return log.warning(ERR_AllocMemory);
      }
      glSharedControl->InputSize = CHUNK_INPUT;
   }

   // Add the process to the subscription list.  Note that access to InputMID acts as a lock for variables like InputTotal.

   InputSubscription *list, *newlist;
   if (!AccessMemoryID(glSharedControl->InputMID, MEM_READ_WRITE, 2000, &list)) {
      if (glSharedControl->InputTotal >= glSharedControl->InputSize) {
         log.msg("Input array needs to be expanded from %d entries.", glSharedControl->InputSize);

         MEMORYID newlistid;
         if (AllocMemory(sizeof(InputSubscription) * (glSharedControl->InputSize + CHUNK_INPUT), MEM_PUBLIC|MEM_UNTRACKED, (APTR *)&newlist, &newlistid)) {
            ReleaseMemory(list);
            return ERR_AllocMemory;
         }

         CopyMemory(list, newlist, sizeof(InputSubscription) * glSharedControl->InputSize);

         ReleaseMemory(list);

         FreeResourceID(glSharedControl->InputMID);
         glSharedControl->InputMID = newlistid;
         glSharedControl->InputSize += CHUNK_INPUT;
         list = newlist;
      }

      LONG i = glSharedControl->InputTotal;
      list[i].SurfaceFilter = SurfaceFilter;
      list[i].ProcessID = ((objTask *)CurrentTask())->ProcessID;

      if (!InputMask) list[i].InputMask = 0xffff;
      else list[i].InputMask = InputMask;

      list[i].Handle = __sync_add_and_fetch(&glSharedControl->InputIDCounter, 1);
      *Handle = list[i].Handle;

      __sync_fetch_and_add(&glSharedControl->InputTotal, 1);

      ReleaseMemory(list);

      const InputCallback is = {
         .SurfaceFilter = SurfaceFilter,
         .InputMask     = (WORD)InputMask,
         .Callback      = *Callback
      };

      glInputCallbacks.emplace(*Handle, is);
      return ERR_Okay;
   }
   else return log.warning(ERR_AccessMemory);
}

/******************************************************************************

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

******************************************************************************/

ERROR gfxUnsubscribeInput(LONG Handle)
{
   pf::Log log(__FUNCTION__);

   if (!Handle) return log.warning(ERR_NullArgs);

   log.branch("Handle: %d", Handle);

   {
      auto it = glInputCallbacks.find(Handle);
      if (it IS glInputCallbacks.end()) return log.warning(ERR_NotFound);
      else glInputCallbacks.erase(it);
   }

   InputSubscription *list;
   if (!AccessMemoryID(glSharedControl->InputMID, MEM_READ_WRITE, 2000, &list)) {
      bool removed = false;
      for (LONG i=glSharedControl->InputTotal-1; i >= 0; i--) {
         if (list[i].Handle != Handle) continue;

         removed = true;
         if (i+1 < glSharedControl->InputTotal) { // Remove by compacting the list
            CopyMemory(list+i+1, list+i, sizeof(InputSubscription) * (glSharedControl->InputTotal - i - 1));
         }

         __sync_fetch_and_sub(&glSharedControl->InputTotal, 1);
         break;
      }

      ReleaseMemory(list);

      if (!glSharedControl->InputTotal) {
         log.trace("Freeing subscriber memory (last subscription removed)");
         FreeResourceID(glSharedControl->InputMID);
         glSharedControl->InputMID   = 0;
         glSharedControl->InputSize  = 0;
         glSharedControl->InputTotal = 0;
      }

      if (!removed) return log.warning(ERR_NotFound);
      else return ERR_Okay;
   }
   else return log.warning(ERR_AccessMemory);
}

//********************************************************************************************************************
// This routine is called on every cycle of ProcessMessages() so that we can check if there are input events
// that need to be processed.

void input_event_loop(HOSTHANDLE FD, APTR Data) // Data is not defined
{
   static ULONG current_index = 0;
   pf::Log log(__FUNCTION__);

   if (current_index IS glInputEvents->IndexCounter) return; // Check if there are events to consume

   // Check for underflow in case this process hasn't been active enough in consuming events.

   if ((glInputEvents->IndexCounter > MAX_INPUTMSG) and (current_index < glInputEvents->IndexCounter - MAX_INPUTMSG + 1)) {
      current_index = glInputEvents->IndexCounter - MAX_INPUTMSG + 1;
   }

   ULONG max_events = glInputEvents->IndexCounter - current_index;
   InputEvent events[max_events];

   //log.traceBranch("Index: %u/%u (%u events)", current_index, glInputEvents->IndexCounter, max_events);

   std::unordered_map<LONG, InputCallback> copyInputCallbacks(glInputCallbacks); // In case of modification

   for (const auto & [ handle, sub ] : copyInputCallbacks) {
      // Construct a linked list of filtered input events for this subscription.

      LONG total_events = 0;
      for (ULONG i=0; i < max_events; i++) {
         LONG e = (current_index + i) & (MAX_INPUTMSG - 1); // Modulo cheat works as long as MAX_INPUTMSG is a ^2

         if (((glInputEvents->Msgs[e].RecipientID IS sub.SurfaceFilter) or (!sub.SurfaceFilter)) and
             (glInputEvents->Msgs[e].Flags & sub.InputMask)) {
            events[total_events] = glInputEvents->Msgs[e];
            events[total_events].Next = &events[total_events + 1];
            total_events++;
         }
      }

      //log.msg("Handle: %d, Filter: #%d, Mask: $%.8x, Events: %d", handle, sub.SurfaceFilter, sub.InputMask, total_events);

      if (total_events > 0) {
         events[total_events-1].Next = NULL;

         auto &cb = sub.Callback;
         if (cb.Type IS CALL_STDC) {
            pf::ScopedObjectLock lock(cb.StdC.Context, 2000); // Ensure that the object can't be removed until after input processing
            if (lock.granted()) {
               pf::SwitchContext ctx(cb.StdC.Context);
               auto func = (ERROR (*)(InputEvent *, LONG))cb.StdC.Routine;
               func(events, handle);
            }
         }
         else if (cb.Type IS CALL_SCRIPT) {
            OBJECTPTR script;
            if ((script = cb.Script.Script)) {
               const ScriptArg args[] = {
                  { "Events:InputEvent", FD_PTR|FDF_STRUCT, { .Address  = events } },
                  { "Handle", FD_LONG, { .Long = handle } },
               };
               ERROR result;
               scCallback(script, cb.Script.ProcedureID, args, ARRAYSIZE(args), &result);
            }
         }
      }
   }

   current_index = glInputEvents->IndexCounter;
}

