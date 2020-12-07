/*****************************************************************************

The source code of the Parasol Framework is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

-CATEGORY-
Name: Messages
-END-

*****************************************************************************/

#ifdef __unix__
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#ifdef _WIN32
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#endif

#include "defs.h"

static void wake_task(LONG Index, CSTRING);
#ifdef _WIN32
static ERROR sleep_task(LONG, BYTE);
#else
static ERROR sleep_task(LONG);
ERROR write_nonblock(LONG Handle, APTR Data, LONG Size, LARGE EndTime);
#endif

static void msghandler_free(APTR Address)
{
   parasol::Log log("RemoveMsgHandler");
   log.trace("Handle: %p", Address);

   ThreadLock lock(TL_MSGHANDLER, 5000);
   if (lock.granted()) {
      MsgHandler *h = (MsgHandler *)Address;
      if (h IS glLastMsgHandler) glLastMsgHandler = h->Prev;
      if (h IS glMsgHandlers) glMsgHandlers = h->Next;
      if (h->Next) h->Next->Prev = h->Prev;
      if (h->Prev) h->Prev->Next = h->Next;
   }
}

static ResourceManager glResourceMsgHandler = {
   "MsgHandler",
   &msghandler_free
};

//#define DBG_INCOMING TRUE  // Print incoming message information

#ifdef _WIN32
static THREADVAR UBYTE tlMsgSent = FALSE;
#endif

#define MAX_MSEC 1000

/*****************************************************************************

-FUNCTION-
AddMsgHandler: Adds a new message handler for processing incoming messages.

This function allows handlers to be added for the interception of incoming messages.  Message handling works as follows:

During a call to ProcessMessages(), each incoming message will be scanned to determine if a message handler is able to
process that message.  All handlers that accept the message type will be called with a copy of the message structure
and any additional data.  The message is then removed from the message queue.

When calling AddMsgHandler(), you can provide an optional Custom pointer that will have meaning to the handler.  The
MsgType acts as a filter so that only messages with the same type identifier will be passed to the handler.  The
Routine parameter must point to the function handler, which will follow this definition:

<pre>ERROR handler(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)</pre>

The handler must return ERR_Okay if the message was handled.  This means that the message will not be passed to message
handlers that are yet to receive the message.  Throw ERR_NothingDone if the message has been ignored or ERR_Continue
if the message was processed but may be analysed by other handlers.  Throw ERR_Terminate to break the current
ProcessMessages() loop.  When using Fluid, this is best achieved by writing `check(errorcode)` in the handler.

The handler will be identified by a unique pointer returned in the Handle parameter.  This handle will be garbage
collected or can be passed to FreeResource() once it is no longer required.

-INPUT-
ptr Custom: A custom pointer that will be passed to the message handler when messages are received.
int MsgType: The message type that the handler wishes to intercept.  If zero, all incoming messages are passed to the handler.
ptr(func) Routine: Refers to the function that will handle incoming messages.
!resource(MsgHandler) Handle:  The resulting handle of the new message handler - this will be needed for FreeResource().

-ERRORS-
Okay: Message handler successfully processed.
NullArgs
AllocMemory
-END-

*****************************************************************************/

ERROR AddMsgHandler(APTR Custom, LONG MsgType, FUNCTION *Routine, MsgHandler **Handle)
{
   parasol::Log log(__FUNCTION__);

   if (!Routine) return log.warning(ERR_NullArgs);

   log.branch("Custom: %p, MsgType: %d, Routine: %p, Type: %d", Custom, MsgType, Routine, Routine->Type);

   ThreadLock lock(TL_MSGHANDLER, 5000);
   if (lock.granted()) {
      MsgHandler *handler;
      if (!AllocMemory(sizeof(MsgHandler), MEM_MANAGED, (APTR *)&handler, NULL)) {
         set_memory_manager(handler, &glResourceMsgHandler);

         handler->Prev     = NULL;
         handler->Next     = NULL;
         handler->Custom   = Custom;
         handler->MsgType  = MsgType;
         handler->Function = *Routine;

         if (!glMsgHandlers) glMsgHandlers = handler;
         else {
            if (glLastMsgHandler) glLastMsgHandler->Next = handler;
            handler->Prev = glLastMsgHandler;
         }

         glLastMsgHandler = handler;

         if (Handle) *Handle = handler;
         return ERR_Okay;
      }
      else return log.warning(ERR_AllocMemory);
   }
   else return log.warning(ERR_Lock);
}

/*****************************************************************************

-FUNCTION-
GetMessage: Reads messages from message queues.

The GetMessage() function is used to read messages that have been stored in message queues.  You can use this function
to read the next immediate message stored on the queue, or the first message on the queue that matches a particular
Type.  It is also possible to call this function in a loop to clear out all messages, until an error code other than
ERR_Okay is returned.

Messages will often (although not always) carry data that is relevant to the message type.  To retrieve this data you
need to supply a buffer, preferably one that is large enough to receive all the data that you expect from your
messages.  If the buffer is too small, the message data will be cut off to fit inside the buffer.

Message data is written to the supplied buffer with a Message structure, which is immediately followed
up with the actual message data.

-INPUT-
mem Queue:  The memory ID of the message queue is specified here.  If zero, the message queue of the local task will be used.
int Type:   The message type that you would like to receive, or zero if you would like to receive the next message on the queue.
int Flags:  This argument is reserved for future use.  Set it to zero.
buf(ptr) Buffer: Pointer to a buffer that is large enough to hold the incoming message information.  If set to NULL then all accompanying message data will be destroyed.
bufsize Size:   The byte-size of the buffer that you have supplied.

-ERRORS-
Okay:
Args:
AccessMemory: Failed to gain access to the message queue.
Search: No more messages are left on the queue, or no messages that match the given Type are on the queue.
-END-

*****************************************************************************/

ERROR GetMessage(MEMORYID MessageMID, LONG Type, LONG Flags, APTR Buffer, LONG BufferSize)
{
   parasol::Log log(__FUNCTION__);

   //log.branch("Queue: %d, Type: %d, Flags: $%.8x, Size: %d", MessageMID, Type, Flags, BufferSize);

   if (!MessageMID) MessageMID = glTaskMessageMID;

   // If no buffer has been provided, drive the Size to 0

   if (!Buffer) BufferSize = 0;

   MessageHeader *header;
   if (Flags & MSF_ADDRESS) header = (MessageHeader *)(MAXINT)MessageMID;
   else if (AccessMemory(MessageMID, MEM_READ_WRITE, 2000, (void **)&header) != ERR_Okay) {
      return ERR_AccessMemory;
   }

   TaskMessage *msg = (TaskMessage *)header->Buffer;
   TaskMessage *prevmsg = NULL;
   LONG j = 0;
   while (j < header->Count) {
      if (!msg->Type) goto next;

      if (Flags & MSF_MESSAGE_ID) {
         // The Type argument actually refers to a unique message ID when MSF_MESSAGE_ID is used
         if (msg->UniqueID != Type) goto next;
      }
      else if ((Type) and (msg->Type != Type)) goto next;

      // Copy the message to the buffer

      if ((Buffer) and ((size_t)BufferSize >= sizeof(Message))) {
         LONG len;
         ((Message *)Buffer)->UniqueID = msg->UniqueID;
         ((Message *)Buffer)->Type     = msg->Type;
         ((Message *)Buffer)->Size     = msg->DataSize;
         ((Message *)Buffer)->Time     = msg->Time;
         BufferSize -= sizeof(Message);
         if (BufferSize < msg->DataSize) {
            ((Message *)Buffer)->Size = BufferSize;
            len = BufferSize;
         }
         else len = msg->DataSize;
         CopyMemory(msg + 1, ((BYTE *)Buffer) + sizeof(Message), len);
      }

      // Remove the message from the buffer

      if (prevmsg) {
         if (msg->NextMsg) prevmsg->NextMsg += msg->NextMsg;
         else prevmsg->NextMsg = 0;
      }
      else {
         msg->Type = 0;
         msg->DataSize = 0;
      }

      // Decrement the message count

      header->CompressReset = 0;
      header->Count--;
      if (header->Count IS 0) header->NextEntry = 0;

      if (!(Flags & MSF_ADDRESS)) ReleaseMemoryID(MessageMID);
      return ERR_Okay;

next:
      if (msg->Type) j++;
      prevmsg = msg;
      msg = (TaskMessage *)ResolveAddress(msg, msg->NextMsg);
   }

   if (!(Flags & MSF_ADDRESS)) ReleaseMemoryID(MessageMID);
   return ERR_Search;
}

/*****************************************************************************

-FUNCTION-
ProcessMessages: Processes system messages that are queued in the task's message buffer.

The ProcessMessages() function is used to process the task's message queue.  Messages are dispatched to message
handlers in the order in which they arrived and the queue is emptied so that space is available for more messages.

Responding to incoming messages is a vital process - the queue is the standard means of communication between your task
and the rest of the system and other tasks within it.  Failing to call the ProcessMessages() function on a regular
basis may cause a back-log of messages to be generated, as well as causing problems with areas such as the graphical
interface. If an area of your program is likely to loop continuously for a measurable period of time without returning,
consider calling ProcessMessages() at a rate of 50 times per second to ensure that incoming messages are processed.

User messages that are on the queue are passed to message handlers.  If no message handler exists to interpret the
message, then it is removed from the queue without being processed. Message handlers are added with the
~AddMsgHandler() function.  If a message handler returns the error code ERR_Terminate, then ProcessMessages()
will stop processing the queue and returns immediately with ERR_Okay.

If a message with a MSGID_QUIT ID is found on the queue, then the function returns immediately with the error code
ERR_Terminate.  This indicates that you must stop the program as soon as possible and exit gracefully.

-INPUT-
int Flags:   Optional flags are specified here (currently no flags are provided).
int TimeOut: A TimeOut value, measured in milliseconds.  If zero, the function will return as soon as all messages on the queue are processed.  If less than zero, the function does not return until a request for termination is received or a user message requires processing.

-ERRORS-
Okay:
Terminate: A MSGID_QUIT message type was found on the message queue.
TimeOut:
-END-

*****************************************************************************/

ERROR ProcessMessages(LONG Flags, LONG TimeOut)
{
   parasol::Log log(__FUNCTION__);

   // Message processing is only possible from the main thread (for system design and synchronisation reasons)
   if ((!tlMainThread) and (!tlThreadWriteMsg)) return log.warning(ERR_OutsideMainThread);

   // Ensure that all resources allocated by sub-routines are assigned to the Task object by default.
   parasol::SwitchContext ctx(glCurrentTask);

   // This recursion blocker prevents ProcessMessages() from being called to breaking point.  Excessive nesting can
   // occur on occasions where ProcessMessages() sends an action to an object that performs some activity before it
   // makes a nested call to ProcessMessages(), which in turn might result in more processing and then eventually
   // a recursive effect if we are unlucky enough.
   //
   // You can also use this feature to block messages from being processed, e.g. during notifications.  Simply set
   // tlMsgRecursion to 255.

   if (tlMsgRecursion >= 255) {
      //log.msg("Do not call this function when inside a notification routine.");
   }
   else if (tlMsgRecursion > 8) {
      return ERR_Recursion;
   }

   tlMsgRecursion++;

   LARGE timeout_end;
   if (TimeOut IS -1) timeout_end = 0x7fffffffffffffffLL; // Infinite loop
   else timeout_end = PreciseTime() + ((LARGE)TimeOut * 1000LL);

   log.traceBranch("Flags: $%.8x, TimeOut: %d", Flags, TimeOut);

   ERROR returncode = ERR_Okay;
   Message *msg = NULL;
   LONG msgbufsize = 0;
   BYTE breaking = FALSE;
   ERROR error;

   #ifdef DEBUG
      LARGE periodic = PreciseTime();
   #endif

   if (!tlMainThread) { // Message handler for threads.
      UBYTE buffer[2048];
      LONG offset = 0;

      while (TRUE) {
         #ifdef _WIN32
            LARGE timeout = (timeout_end - PreciseTime()) / 1000LL;
            if (timeout > 0x7fffffff) timeout = -1; // Infinite.
            else if (timeout < 0) timeout = 0;

            if ((winWaitForObjects(1, &tlThreadReadMsg, timeout, FALSE)) >= 0) {
               LONG len = sizeof(buffer) - offset;
               LONG pipe_result = winReadPipe(tlThreadReadMsg, buffer+offset, &len);
               if (pipe_result IS -2) { // Pipe broken.
                  log.warning(ERR_SystemCall);
                  break;
               }
               else log.warning(ERR_SystemCall);

               offset += len;
            }

         #else
/*
            // Untested: We want to use select() so as not to indefinitely block on the call to read().

            fd_set rfd, wrd, efd;
            FD_ZERO(&rfd);
            FD_ZERO(&wfd);
            FD_ZERO(&efd);
            FD_SET(tlThreadReadMsg, &rfd);

            struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
            if (select(tlThreadReadMsg + 1, &rfd, &wfd, &efd, &timeout) == 1) {

            }
            else { // Timeout
                break;
            }
*/

            LONG len = read(tlThreadReadMsg, buffer+offset, sizeof(buffer)-offset);
            if (len IS -1) {
               log.warning("[Thread] Call to read() broken, thread will stop.");
               break;
            }

            offset += len;
         #endif

         if ((size_t)offset >= sizeof(Message)) {
            Message *msg = (Message *)buffer;
            if ((size_t)offset >= sizeof(Message) + msg->Size) {
               log.msg("[Thread] Received message of %d bytes.", msg->Size);

               tlCurrentMsg = msg; // This global variable is available through GetResourcePtr(RES_CURRENTMSG)

               // Handle the message here.

               if ((msg->Type IS MSGID_BREAK) and (tlMsgRecursion > 1)) breaking = TRUE; // MSGID_BREAK will break out of recursive calls to ProcessMessages() only

               ThreadLock lock(TL_MSGHANDLER, 5000);
               if (lock.granted()) {
                  // Message handlers will execute within the context of the main task and not the function's context
                  for (auto handler=glMsgHandlers; handler; handler=handler->Next) {
                     if ((!handler->MsgType) or (handler->MsgType IS msg->Type)) {
                        ERROR result = ERR_NoSupport;
                        if (handler->Function.Type IS CALL_STDC) {
                           auto msghandler = (LONG (*)(APTR, LONG, LONG, APTR, LONG))handler->Function.StdC.Routine;
                           if (msg->Size) result = msghandler(handler->Custom, msg->UniqueID, msg->Type, msg + 1, msg->Size);
                           else result = msghandler(handler->Custom, msg->UniqueID, msg->Type, NULL, 0);
                        }
                        else if (handler->Function.Type IS CALL_SCRIPT) {
                           OBJECTPTR script;
                           if ((script = handler->Function.Script.Script)) {
                              const ScriptArg args[] = {
                                 { "Custom",   FD_PTR,  { .Address  = handler->Custom } },
                                 { "UniqueID", FD_LONG, { .Long = msg->UniqueID } },
                                 { "Type",     FD_LONG, { .Long = msg->Type } },
                                 { "Data",     FD_PTR|FD_BUFFER,  { .Address  = msg + 1} },
                                 { "Size",     FD_LONG|FD_BUFSIZE, { .Long = msg->Size } }
                              };
                              if (scCallback(script, handler->Function.Script.ProcedureID, args, ARRAYSIZE(args), &result)) result = ERR_Terminate;
                           }
                        }
                        else log.warning("Handler uses function type %d, not understood.", handler->Function.Type);

                        if (result IS ERR_Okay) { // If the message was handled, do not pass it to anyone else
                           break;
                        }
                        else if (result IS ERR_Terminate) {
                           log.trace("Terminate request received from message handler.");
                           timeout_end = 0; // Set to zero to indicate loop terminated
                           break;
                        }
                     }
                  }
               }

               tlCurrentMsg = NULL;
               offset = 0;
            }
         }

         if ((glTaskState IS TSTATE_STOPPING) or (breaking)) {
            log.trace("Breaking message loop.");
            break;
         }

         if (PreciseTime() >= timeout_end) {
            if (TimeOut) {
               log.trace("Breaking message loop - timeout of %dms.", TimeOut);
               if (timeout_end > 0) returncode = ERR_TimeOut;
            }
            break;
         }
      } // while(TRUE)
   }
   else do { // Standard message handler for the core process.
      // Call all objects on the timer list (managed by SubscribeTimer()).  To manage timer locking cleanly, the loop
      // is restarted after each client call.  To prevent more than one call per cycle, the glTimerCycle is used to
      // prevent secondary calls.

      glTimerCycle++;
timer_cycle:
      if ((glTaskState != TSTATE_STOPPING) and (!thread_lock(TL_TIMER, 200))) {
         LARGE current_time = PreciseTime();
         for (CoreTimer *timer=glTimers; timer; timer=timer->Next) {
            if (current_time < timer->NextCall) continue;
            if (timer->Cycle IS glTimerCycle) continue;

            LARGE elapsed = current_time - timer->LastCall;

            timer->NextCall += timer->Interval;
            if (timer->NextCall < current_time) timer->NextCall = current_time;
            timer->LastCall = current_time;
            timer->Cycle = glTimerCycle;

            //log.trace("Subscriber: %d, Interval: %d, Time: " PF64(), timer->SubscriberID, timer->Interval, current_time);

            timer->Locked = TRUE; // Prevents termination of the structure irrespective of having a TL_TIMER lock.

            UBYTE relock = FALSE;
            if (timer->Routine.Type IS CALL_STDC) { // C/C++ callback
               OBJECTPTR subscriber;
               if (!AccessObject(timer->SubscriberID, 50, &subscriber)) {
                  parasol::SwitchContext context(subscriber);

                  auto routine = (ERROR (*)(OBJECTPTR, LARGE, LARGE))timer->Routine.StdC.Routine;
                  thread_unlock(TL_TIMER);
                  relock = TRUE;

                  error = routine(subscriber, elapsed, current_time);

                  ReleaseObject(subscriber);
               }
               else error = ERR_AccessObject;
            }
            else if (timer->Routine.Type IS CALL_SCRIPT) { // Script callback
               objScript *script;
               if ((script = (objScript *)timer->Routine.Script.Script)) {
                  const ScriptArg scargs[] = {
                     { "Subscriber",  FDF_OBJECTID, { .Long = timer->SubscriberID } },
                     { "Elapsed",     FD_LARGE, { .Large = elapsed } },
                     { "CurrentTime", FD_LARGE, { .Large = current_time } }
                  };

                  thread_unlock(TL_TIMER);
                  relock = TRUE;

                  if (scCallback(script, timer->Routine.Script.ProcedureID, scargs, ARRAYSIZE(scargs), &error)) error = ERR_Terminate;
               }
               else error = ERR_SystemCorrupt;
            }
            else error = ERR_Terminate;

            timer->Locked = FALSE;

            if (error IS ERR_Terminate) UpdateTimer(timer, 0);

            if (relock) goto timer_cycle;
         } // for

         thread_unlock(TL_TIMER);
      }

      // This routine pulls out all messages and processes them

      WORD msgcount = 0;
      BYTE repass = FALSE;
      while (1) {
         MessageHeader *msgbuffer;
         BYTE msgfound = FALSE;
         if (!AccessMemory(glTaskMessageMID, MEM_READ_WRITE, 2000, (void **)&msgbuffer)) {
            if (msgbuffer->Count) {
               TaskMessage *scanmsg = (TaskMessage *)msgbuffer->Buffer;
               TaskMessage *prevmsg = NULL;

               #ifdef DEBUG
               if (PreciseTime() - periodic > 1000000LL) {
                  periodic = PreciseTime();
                  log.trace("Message count: %d", msgbuffer->Count);
               }
               #endif

               while (1) {
                  if (!scanmsg->Type); // Ignore removed messages.
                  else if ((scanmsg->DataSize < 0) or (scanmsg->DataSize > 1024 * 1024)) { // Check message validity
                     log.warning("Invalid message found in queue: Type: %d, Size: %d", scanmsg->Type, scanmsg->DataSize);
                     scanmsg->Type = 0;
                     scanmsg->DataSize = 0;
                  }
                  else { // Message found.  Process it.
                     if ((msg) and ((size_t)msgbufsize < sizeof(Message) + scanmsg->DataSize)) { // Is our message buffer large enough?
                        log.trace("Freeing message buffer for expansion %d < %d + %d", msgbufsize, sizeof(Message), scanmsg->DataSize);
                        FreeResource(msg);
                        msg = NULL;
                     }

                     if (!msg) {
                        #define DEFAULT_MSGBUFSIZE 16384

                        if (sizeof(Message) + scanmsg->DataSize  > DEFAULT_MSGBUFSIZE) {
                           msgbufsize = sizeof(Message) + scanmsg->DataSize;
                        }
                        else msgbufsize = DEFAULT_MSGBUFSIZE;

                        if (AllocMemory(msgbufsize, MEM_NO_CLEAR, (APTR *)&msg, NULL) != ERR_Okay) break;
                     }

                     ((Message *)msg)->UniqueID = scanmsg->UniqueID;
                     ((Message *)msg)->Type     = scanmsg->Type;
                     ((Message *)msg)->Size     = scanmsg->DataSize;
                     ((Message *)msg)->Time     = scanmsg->Time;
                     CopyMemory(scanmsg + 1, ((BYTE *)msg) + sizeof(Message), scanmsg->DataSize);

                     // Remove the message from the buffer

                     if (prevmsg) {
                        if (scanmsg->NextMsg) prevmsg->NextMsg += scanmsg->NextMsg;
                        else prevmsg->NextMsg = 0;
                     }
                     else {
                        scanmsg->Type = 0;
                        scanmsg->DataSize = 0;
                     }

                     // Decrement the message count

                     msgbuffer->CompressReset = 0;
                     msgbuffer->Count--;
                     if (msgbuffer->Count IS 0) msgbuffer->NextEntry = 0;
                     msgfound = TRUE;
                     break;
                  }

                  // Go to next message
                  prevmsg = scanmsg;
                  if (scanmsg->NextMsg) scanmsg = (TaskMessage *)ResolveAddress(scanmsg, scanmsg->NextMsg);
                  else break;
               } // while(1)
            }

            ReleaseMemoryID(glTaskMessageMID);
         }

         if (!msgfound) break;

         tlCurrentMsg = (Message *)msg; // This global variable is available through GetResourcePtr(RES_CURRENTMSG)

         if ((msg->Type IS MSGID_BREAK) and (tlMsgRecursion > 1)) breaking = TRUE; // MSGID_BREAK will break out of recursive calls to ProcessMessages() only

         ThreadLock lock(TL_MSGHANDLER, 5000);
         if (lock.granted()) {
            for (MsgHandler *handler=glMsgHandlers; handler; handler=handler->Next) {
               if ((!handler->MsgType) or (handler->MsgType IS msg->Type)) {
                  ERROR result = ERR_NoSupport;
                  if (handler->Function.Type IS CALL_STDC) {
                     auto msghandler = (LONG (*)(APTR, LONG, LONG, APTR, LONG))handler->Function.StdC.Routine;
                     if (msg->Size) result = msghandler(handler->Custom, msg->UniqueID, msg->Type, msg + 1, msg->Size);
                     else result = msghandler(handler->Custom, msg->UniqueID, msg->Type, NULL, 0);
                  }
                  else if (handler->Function.Type IS CALL_SCRIPT) {
                     OBJECTPTR script;
                     if ((script = handler->Function.Script.Script)) {
                        const ScriptArg args[] = {
                           { "Custom",   FD_PTR,  { .Address  = handler->Custom } },
                           { "UniqueID", FD_LONG, { .Long = msg->UniqueID } },
                           { "Type",     FD_LONG, { .Long = msg->Type } },
                           { "Data",     FD_PTR|FD_BUFFER, { .Address = msg + 1} },
                           { "Size",     FD_LONG|FD_BUFSIZE, { .Long = msg->Size } }
                        };
                        if (scCallback(script, handler->Function.Script.ProcedureID, args, ARRAYSIZE(args), &result)) result = ERR_Terminate;
                     }
                  }
                  else log.warning("Handler uses function type %d, not understood.", handler->Function.Type);

                  if (result IS ERR_Okay) { // If the message was handled, do not pass it to anyone else
                     break;
                  }
                  else if (result IS ERR_Terminate) { // Terminate the ProcessMessages() loop, but don't quit the program
                     log.trace("Terminate request received from message handler.");
                     timeout_end = 0; // Set to zero to indicate loop terminated
                     break;
                  }
               }
            }
         }

         tlCurrentMsg = NULL;

         if (++msgcount > 30) {
            repass = TRUE;
            break; // Break if there are a lot of messages, so that we can call message hooks etc
         }
      } // while(1)

      // This code is used to validate suspect processes

      if (glValidateProcessID) {
         validate_process(glValidateProcessID);
         glValidateProcessID = 0;
      }

      #ifdef _WIN32
         // Process any incoming window messages that occurred during our earlier processing. The hook for glNetProcessMessages() is found
         // in the network module and is required to prevent flooding of the Windows message queue.

         if (tlMainThread) {
            tlMsgSent = FALSE;
            if (glNetProcessMessages) glNetProcessMessages(NETMSG_START, NULL);
            winProcessMessages();
            if (glNetProcessMessages) glNetProcessMessages(NETMSG_END, NULL);

            if (tlMsgSent) repass = TRUE; // We may have placed a message on our own queue - make sure we go back and process it rather than sleeping
         }
      #endif

      LARGE wait = 0;
      if ((repass) or (breaking) or (glTaskState IS TSTATE_STOPPING));
      else if (timeout_end > 0) {
         // Wait for someone to communicate with us, or stall until an interrupt is due.

         LARGE sleep_time = timeout_end;
         {
            ThreadLock lock(TL_TIMER, 200);
            if (lock.granted()) {
               for (CoreTimer *timer = glTimers; timer; timer=timer->Next) {
                  if (timer->NextCall < sleep_time) sleep_time = timer->NextCall;
               }
            }
         }

         wait = sleep_time - PreciseTime();
         if (wait > 60 * 60 * 1000000LL) wait = 60 * 60 * 1000000LL; // One hour (limit required for 64-bit to 32-bit reduction)

         if (wait < 0) wait = 0;
      }

      #ifdef _WIN32
         if (tlMainThread) {
            tlMessageBreak = TRUE;  // Break if the host OS sends us a native message
            sleep_task(wait/1000LL, FALSE); // Event if wait is zero, we still need to clear FD's and call FD hooks
            tlMessageBreak = FALSE;

            if (wait) {
               if (glNetProcessMessages) glNetProcessMessages(NETMSG_START, NULL);
               winProcessMessages();
               if (glNetProcessMessages) glNetProcessMessages(NETMSG_END, NULL);
            }
         }
         else {
         }
      #else
         sleep_task(wait/1000LL); // Event if wait is zero, we still need to clear FD's and call FD hooks
      #endif

      // Continue the loop?

      if (repass) continue; // There are messages left unprocessed
      if ((glTaskState IS TSTATE_STOPPING) or (breaking)) {
         log.trace("Breaking message loop.");
         break;
      }

      if (PreciseTime() >= timeout_end) {
         if (TimeOut) {
            log.trace("Breaking message loop - timeout of %dms.", TimeOut);
            if (timeout_end > 0) returncode = ERR_TimeOut;
         }
         break;
      }

   } while (1);

   if (glTaskState IS TSTATE_STOPPING) returncode = ERR_Terminate;

   if (msg) FreeResource(msg);

   tlMsgRecursion--;
   return returncode;
}

/*****************************************************************************

-FUNCTION-
ScanMessages: Scans a message queue for multiple occurrences of a message type.

Use the ScanMessages() function to scan a message queue for information without affecting the state of the queue.  To
use this function, you need to establish a connection to the queue by using the ~AccessMemory() function
first to gain access.  Then make repeated calls to ScanMessages() to analyse the queue until it returns an error code
other than ERR_Okay.  Use ~ReleaseMemory() to let go of the message queue when you are done with it.

Here is an example that scans the queue of the active task:

<pre>
if (!AccessMemory(GetResource(RES_MESSAGEQUEUE), MEM_READ, &queue)) {
   while (!ScanMessages(queue, &index, MSGID_QUIT, NULL, NULL)) {
      ...
   }
   ReleaseMemory(queue);
}
</pre>

Messages will often (although not always) carry data that is relevant to the message type.  To retrieve this data you
need to supply a buffer, preferably one that is large enough to receive all the data that you expect from your
messages.  If the buffer is too small, the message data will be cut off to fit inside the buffer.

Message data is written to the supplied buffer with a Message structure (struct Message), which is immediately followed
up with the actual message data.  The message structure includes the following fields:

-INPUT-
ptr Queue:  An address pointer for a message queue (use AccessMemory() to get an address from a message queue ID).
&int Index: Pointer to a 32-bit value that must initially be set to zero.  The ScanMessages() function will automatically update this variable with each call so that it can remember its analysis position.
int Type:   The message type to filter for, or zero to scan all messages in the queue.
buf(ptr) Buffer: Pointer to a buffer that is large enough to hold the message information.  Set to NULL if you are not interested in the message data.
bufsize Size:   The byte-size of the buffer that you have supplied.

-ERRORS-
Okay:   The next message in the queue was successfully analysed.
NullArgs:
Search: No more messages are left on the queue, or no messages that match the given Type are on the queue.
-END-

*****************************************************************************/

ERROR ScanMessages(APTR MessageQueue, LONG *Index, LONG Type, APTR Buffer, LONG BufferSize)
{
   parasol::Log log(__FUNCTION__);

   if ((!MessageQueue) or (!Index)) return log.warning(ERR_NullArgs);
   if (!Buffer) BufferSize = 0;

   MessageHeader *header = (MessageHeader *)MessageQueue;
   TaskMessage *msg, *prevmsg = NULL;

   LONG j = 0;

   if (*Index > 0) {
      msg = (TaskMessage *)header->Buffer;
      while ((j < header->Count) and (j < *Index)) {
         if (msg->Type) j++;
         prevmsg = msg;
         if (!msg->NextMsg) break;
         msg = (TaskMessage *)ResolveAddress(msg, msg->NextMsg);
      }
   }
   else if (*Index < 0) {
      *Index = -1;
      return ERR_Search;
   }
   else msg = (TaskMessage *)header->Buffer;

   while (j < header->Count) {
      if ((msg->Type) and ((msg->Type IS Type) or (!Type))) {
         if ((Buffer) and ((size_t)BufferSize >= sizeof(Message))) {
            ((Message *)Buffer)->UniqueID = msg->UniqueID;
            ((Message *)Buffer)->Type     = msg->Type;
            ((Message *)Buffer)->Size     = msg->DataSize;
            ((Message *)Buffer)->Time     = msg->Time;

            BufferSize -= sizeof(Message);
            if (BufferSize < msg->DataSize) {
               ((Message *)Buffer)->Size = BufferSize;
               CopyMemory(msg + 1, ((BYTE *)Buffer) + sizeof(Message), BufferSize);
            }
            else CopyMemory(msg + 1, ((BYTE *)Buffer) + sizeof(Message), msg->DataSize);
         }

         *Index = j + 1;
         return ERR_Okay;
      }

      if (msg->Type) j++;
      prevmsg = msg;
      if (!msg->NextMsg) break;
      msg = (TaskMessage *)ResolveAddress(msg, msg->NextMsg);
   }

   *Index = -1;
   return ERR_Search;
}

/*****************************************************************************

-FUNCTION-
SendMessage: Send messages to message queues.

The SendMessage() function is used to write messages to message queues. To write a message to a queue, you must know
the memory ID of that queue and be able to provide this function with a message type ID that the queue's handler
will understand.  If necessary, you can also attach data to the message if it has relevance to the message type.

If the message queue is found to be full, or if the size of your message is larger than the total size of the queue,
this function will immediately return with an ERR_ArrayFull error code.  If you are prepared to wait for the queue
handler to process the waiting messages, specify WAIT in the Flags parameter.  There is a maximum time-out period
of 10 seconds in case the task responsible for handling the queue is failing to process its messages.

-INPUT-
mem Queue: The memory ID of the destination message queue.  If zero, the local message queue is targeted.
int Type:  The message Type/ID being sent.  Unique type ID's can be obtained from ~AllocateID().
int(MSF) Flags: Optional flags.
buf(ptr) Data:  Pointer to the data that will be written to the queue.  Set to NULL if there is no data to write.
bufsize Size:   The byte-size of the data being written to the message queue.

-ERRORS-
Okay:         The message was successfully written to the message queue.
Args:
ArrayFull:    The message queue is full.
TimeOut:      The message queue is full and the queue handler has failed to process them over a reasonable time period.
AccessMemory: Access to the message queue memory was denied.
-END-

*****************************************************************************/

static LONG glUniqueMsgID = 1;

static void view_messages(MessageHeader *Header) __attribute__ ((unused));

static void view_messages(MessageHeader *Header)
{
   parasol::Log log("Messages");

   log.warning("Count: %d, Next: %d", Header->Count, Header->NextEntry);

   auto msg = (TaskMessage *)Header->Buffer;
   WORD count = 0;
   while (count < Header->Count) {
      if (msg->Type) {
         if (msg->Type IS MSGID_ACTION) {
            ActionMessage *action = (ActionMessage *)(msg + 1);
            if (action->ActionID > 0) log.warning("Action: %s, Object: %d, Args: %d [Size: %d, Next: %d]", ActionTable[action->ActionID].Name, action->ObjectID, action->SendArgs, msg->DataSize, msg->NextMsg);
            else log.warning("Method: %d, Object: %d, Args: %d [Size: %d, Next: %d]", action->ActionID, action->ObjectID, action->SendArgs, msg->DataSize, msg->NextMsg);
         }
         else log.warning("Type: %d, Size: %d, Next: %d", msg->Type, msg->DataSize, msg->NextMsg);
         count++;
      }
      msg = (TaskMessage *)ResolveAddress(msg, msg->NextMsg);
   }
}

ERROR SendMessage(MEMORYID MessageMID, LONG Type, LONG Flags, APTR Data, LONG Size)
{
   parasol::Log log(__FUNCTION__);

   if (glLogLevel >= 9) log.function("MessageMID: %d, Type: %d, Data: %p, Size: %d", MessageMID, Type, Data, Size);

   if (Type IS MSGID_QUIT) log.function("A quit message is being posted to queue #%d, context #%d.", MessageMID, tlContext->Object->UniqueID);

   if ((!Type) or (Size < 0)) return log.warning(ERR_Args);

   if (!MessageMID) {
      MessageMID = glTaskMessageMID;
      if (!MessageMID) return ERR_NullArgs;
   }

   if (!Data) { // If no data has been provided, drive the Size to 0
      if (Size) log.warning("Message size indicated but no data provided.");
      Size = 0;
   }

   LONG msgsize = (Size + 3) & (~3); // Raise the size if not long-word aligned

   LONG i;
   ERROR error;
   TaskMessage *msg, *prevmsg;
   MessageHeader *header;
   if (!(error = AccessMemory(MessageMID, MEM_READ_WRITE, 2000, (void **)&header))) {
      if (Flags & (MSF_NO_DUPLICATE|MSF_UPDATE)) {
         msg = (TaskMessage *)header->Buffer;
         prevmsg = NULL;
         i = 0;
         while (i < header->Count) {
            if (msg->Type IS Type) {
               if (Flags & MSF_NO_DUPLICATE) {
                  ReleaseMemoryID(MessageMID);
                  return ERR_Okay;
               }
               else {
                  // Delete the existing message type before adding the new one when the MF_UPDATE flag has been specified.

                  if (prevmsg) {
                     if (msg->NextMsg) prevmsg->NextMsg += msg->NextMsg;
                     else prevmsg->NextMsg = 0;
                  }
                  else {
                     msg->UniqueID = 0;
                     msg->Type     = 0;
                     msg->DataSize = 0;
                     msg->Time     = 0;
                  }

                  // Decrement the message count

                  header->Count--;
                  if (header->Count IS 0) header->NextEntry = 0;
                  break;
               }
            }
            if (msg->Type) i++;
            prevmsg = msg;
            msg = (TaskMessage *)ResolveAddress(msg, msg->NextMsg);
         }
      }

      if ((header->NextEntry + sizeof(TaskMessage) + msgsize) >= SIZE_MSGBUFFER) {

         if (header->CompressReset) {
            // Do nothing if we've already tried compression and no messages have been pulled off the queue since that time.
            log.warning("Message buffer %d is at capacity.", MessageMID);
            ReleaseMemoryID(MessageMID);
            return ERR_ArrayFull;
         }

         // Compress the message buffer
         // Suspect that this compression routine corrupts the message queue.

//         LONG j, nextentry, nextoffset;
//         log.msg("Compressing message buffer #%d.", MessageMID);
//
//         msg = (TaskMessage *)header->Buffer;
//         nextentry = 0; // Set the next entry point to position zero to start the compression
//         j = 0;
//         while (j < header->Count) {
//            nextoffset = msg->NextMsg; // Get the offset to the next message
//            if (msg->Type) {
//               CopyMemory(msg, header->Buffer + nextentry, sizeof(TaskMessage) + msg->DataSize);
//               nextentry += sizeof(TaskMessage) + ((msg->DataSize + 3) & ~3);
//               j++;
//            }
//            if (!nextoffset) break;
//            msg = (TaskMessage *)ResolveAddress(msg, nextoffset);
//         }
//         header->NextEntry = nextentry;

         // This routine is slower than the normal compression technique, but is tested as working.

         MessageHeader *buffer;
         if (!AllocMemory(sizeof(MessageHeader), MEM_DATA|MEM_NO_CLEAR, (APTR *)&buffer, NULL)) {
            // Compress the message buffer to a temporary data store

            buffer->NextEntry = 0;
            buffer->TaskIndex = header->TaskIndex;
            /*
            #ifdef _WIN32
               buffer->MsgProcess = header->MsgProcess;
               buffer->MsgSemaphore = header->MsgSemaphore;
            #else
               buffer->Mutex = header->Mutex;
            #endif
            */

            auto srcmsg  = (TaskMessage *)header->Buffer;
            auto destmsg = (TaskMessage *)buffer->Buffer;
            for (buffer->Count=0; buffer->Count < header->Count;) {
               if (srcmsg->Type) {
                  CopyMemory(srcmsg, destmsg, sizeof(TaskMessage) + srcmsg->DataSize);
                  destmsg->NextMsg = sizeof(TaskMessage) + ((srcmsg->DataSize + 3) & ~3);
                  buffer->NextEntry += destmsg->NextMsg;
                  destmsg = (TaskMessage *)ResolveAddress(destmsg, destmsg->NextMsg);
                  buffer->Count++;
               }
               srcmsg = (TaskMessage *)ResolveAddress(srcmsg, srcmsg->NextMsg);
            }

            CopyMemory(buffer, header, sizeof(MessageHeader)); // Copy our new message buffer over the old one
            FreeResource(buffer);
         }

         log.debug("Buffer compressed to %d bytes, %d messages on the queue.", header->NextEntry, header->Count);

         // Check if space is now available

         if ((header->NextEntry + sizeof(TaskMessage) + msgsize) >= SIZE_MSGBUFFER) {
            log.warning("Message buffer %d is at capacity and I cannot compress the queue.", MessageMID);

            //view_messages(header);

            header->CompressReset = 1;
            ReleaseMemoryID(MessageMID);
            return ERR_ArrayFull;
         }
      }

      // Set up the message entry

      msg = (TaskMessage *)(header->Buffer + header->NextEntry);
      msg->UniqueID = __sync_add_and_fetch(&glUniqueMsgID, 1);
      msg->Type     = Type;
      msg->DataSize = Size;
      msg->NextMsg  = sizeof(TaskMessage) + msgsize;
      msg->Time     = PreciseTime();

      // Copy the message data, if given

      if ((Data) and (msgsize)) CopyMemory(Data, msg + 1, Size);

      header->NextEntry += msg->NextMsg;
      header->Count++;
      header->CompressReset = 0;

      // Alert the foreign process to indicate that there are messages available.

      WORD taskindex = header->TaskIndex;
      ReleaseMemoryID(MessageMID);
      wake_task(taskindex, __func__);

      #ifdef _WIN32
         tlMsgSent = TRUE;
      #endif

      return ERR_Okay;
   }
   else {
      log.warning("Could not gain access to message port #%d: %s", MessageMID, glMessages[error]);
      return error; // Important that the original AccessMemory() error is returned (some code depends on this for detailed clarification)
   }
}

/*****************************************************************************
** This is the equivalent internal routine to SendMessage(), for the purpose of sending messages to other threads.
*/

#ifdef _WIN32
ERROR send_thread_msg(WINHANDLE Handle, LONG Type, APTR Data, LONG Size)
#else
ERROR send_thread_msg(LONG Handle, LONG Type, APTR Data, LONG Size)
#endif
{
   parasol::Log log(__FUNCTION__);
   ERROR error;

   log.function("Type: %d, Data: %p, Size: %d", Type, Data, Size);

   TaskMessage msg;
   msg.UniqueID = __sync_add_and_fetch(&glUniqueMsgID, 1);
   msg.Type     = Type;
   msg.DataSize = Size;
   msg.NextMsg  = sizeof(TaskMessage) + Size;
   msg.Time     = PreciseTime();

#ifdef _WIN32
   LONG write = sizeof(msg);
   if (!winWritePipe(Handle, &msg, &write)) { // Write the header first.
      if ((Data) and (Size > 0)) {
         // Write the main message.
         write = Size;
         if (!winWritePipe(Handle, Data, &write)) {
            error = ERR_Okay;
         }
         else error = ERR_Write;
      }
      else error = ERR_Okay;
   }
   else error = ERR_Write;
#else
   LARGE end_time = (PreciseTime()/1000LL) + 10000LL;
   error = write_nonblock(Handle, &msg, sizeof(msg), end_time);
   if ((!error) and (Data) and (Size > 0)) { // Write the main message.
      error = write_nonblock(Handle, Data, Size, end_time);
   }
#endif

   if (error) log.warning(error);
   return error;
}

/*****************************************************************************
** Internal: write_nonblock()
**
** Simplifies the process of writing to an FD that is set to non-blocking mode (typically a socket or pipe).  An
** end-time is required so that a timeout will be signalled if the reader isn't keeping the buffer clear.
*/

#ifdef __unix__
ERROR write_nonblock(LONG Handle, APTR Data, LONG Size, LARGE EndTime)
{
   LONG offset = 0;
   ERROR error = ERR_Okay;

   while ((offset < Size) and (!error)) {
      LONG write_size = Size;
      if (write_size > 1024) write_size = 1024;  // Limiting the size will make the chance of an EWOULDBLOCK error less likely.
      LONG len = write(Handle, (char *)Data+offset, write_size - offset);
      if (len >= 0) offset += len;
      if (offset IS Size) break;

      if (len IS -1) { // An error occurred.
         if ((errno IS EAGAIN) or (errno IS EWOULDBLOCK)) { // The write() failed because it would have blocked.  Try again!
            fd_set wfds;
            struct timeval tv;
            while (((PreciseTime()/1000LL) < EndTime) and (!error)) {
               FD_ZERO(&wfds);
               FD_SET(Handle, &wfds);
               tv.tv_sec = (EndTime - (PreciseTime()/1000LL)) / 1000LL;
               tv.tv_usec = 0;
               LONG total = select(1, &wfds, NULL, NULL, &tv);
               if (total IS -1) error = ERR_SystemCall;
               else if (!total) error = ERR_TimeOut;
               else break;
            }
         }
         else if ((errno IS EINVAL) or (errno IS EBADF) or (errno IS EPIPE)) { error = ERR_InvalidHandle; break; }
         else { error = ERR_Write; break; }
      }

      if ((PreciseTime()/1000LL) > EndTime) {
         error = ERR_TimeOut;
         break;
      }
   }

   return error;
}
#endif

/*****************************************************************************

-FUNCTION-
UpdateMessage: Updates the data of any message that is queued.

The UpdateMessage() function provides a facility for updating the content of existing messages on a queue.  You are
required to know the ID of the message that you are going to update and will need to provide the new message Type
and/or Data that is to be set against the message.

Messages can be deleted from the queue by setting the Type to -1.  There is no need to provide buffer information
when deleting a message.

If you supply a data buffer, then its size should equal that of the data already set against the message.  The size
will be trimmed if it exceeds that of the existing message, as this function cannot expand the size of the queue.

-INPUT-
ptr Queue:   Must refer to the target message queue.
int Message: The ID of the message that will be updated.
int Type:    Defines the type of the message.  If set to -1, the message will be deleted.
buf(ptr) Data: Pointer to a buffer that contains the new data for the message.
bufsize Size: The byte-size of the buffer that has been supplied.  It must not exceed the size of the message that is being updated.

-ERRORS-
Okay:   The message was successfully updated.
NullArgs:
Search: The ID that you supplied does not refer to a message in the queue.
-END-

*****************************************************************************/

ERROR UpdateMessage(APTR Queue, LONG MessageID, LONG Type, APTR Buffer, LONG BufferSize)
{
   parasol::Log log(__FUNCTION__);

   if ((!Queue) or (!MessageID)) return log.warning(ERR_NullArgs);

   auto header = (MessageHeader *)Queue;
   auto msg = (TaskMessage *)header->Buffer;
   TaskMessage *prevmsg = NULL;
   LONG j = 0;
   while (j < header->Count) {
      if (msg->UniqueID IS MessageID) {
         if (Buffer) {
            LONG len = (BufferSize > msg->DataSize) ? msg->DataSize : BufferSize;
            CopyMemory(Buffer, msg + 1, len);
         }

         if (Type IS -1) { // Delete the message from the queue
            if (msg->Type) {
               msg->Type = 0;
               header->Count--;
            }
         }
         else if (Type) msg->Type = Type;

         return ERR_Okay;
      }

      if (msg->Type) j++;
      prevmsg = msg;
      msg = (TaskMessage *)ResolveAddress(msg, msg->NextMsg);
   }

   return log.warning(ERR_Search);
}

/*****************************************************************************
** Function: sleep_task() - Unix version
*/

#ifdef __unix__
ERROR sleep_task(LONG Timeout)
{
   parasol::Log log(__FUNCTION__);

   if (!tlMainThread) {
      log.warning("Only the main thread can call this function.");
      return ERR_Failed;
   }
   else if (tlPublicLockCount > 0) {
      log.warning("You cannot sleep while still holding %d global locks!", tlPublicLockCount);
      return ERR_Okay;
   }
   else if (tlPrivateLockCount != 0) {
      char buffer[120];
      WORD pos = 0;
      for (LONG i=0; (i < glNextPrivateAddress) and ((size_t)pos < sizeof(buffer)-1); i++) {
         if (glPrivateMemory[i].AccessCount > 0) {
            pos += snprintf(buffer+pos, sizeof(buffer)-pos, "%d.%d ", glPrivateMemory[i].MemoryID, glPrivateMemory[i].AccessCount);
         }
      }

      if (pos > 0) log.warning("WARNING - Sleeping with %d private locks held (%s)", tlPrivateLockCount, buffer);
   }

   struct timeval tv;
   struct timespec time;
   fd_set fread, fwrite;

   //log.trace("Time-out: %d", Timeout);

   LONG maxfd = -1;
   if (glTotalFDs > 0) {
      FD_ZERO(&fread);
      FD_ZERO(&fwrite);
      for (LONG i=0; i < glTotalFDs; i++) {
         if (glFDTable[i].Flags & RFD_STOP_RECURSE) continue; // This is an internally managed flag to prevent recursion
         if (glFDTable[i].Flags & RFD_READ) FD_SET(glFDTable[i].FD, &fread);
         if (glFDTable[i].Flags & RFD_WRITE) FD_SET(glFDTable[i].FD, &fwrite);
         //log.trace("Listening to %d, Read: %d, Write: %d, Routine: %p, Flags: $%.2x", glFDTable[i].FD, (glFDTable[i].Flags & RFD_READ) ? 1 : 0, (glFDTable[i].Flags & RFD_WRITE) ? 1 : 0, glFDTable[i].Routine, glFDTable[i].Flags);
         if (glFDTable[i].FD > maxfd) maxfd = glFDTable[i].FD;

         if (glFDTable[i].FD IS glX11FD) {
            // This sub-routine is required for managing X11's FD, which doesn't seem to wake our task if we call select() while there is data sitting
            // in the FD (you can test this by running Exodus for X11).  Therefore we process the FD before going to sleep.  This is a bit of a stop gap
            // measure until we figure out what is wrong with the FD (assuming there is some other way to fix it).

            if (glFDTable[i].Routine) glFDTable[i].Routine(glFDTable[i].FD, glFDTable[i].Data);
         }
         else if (glFDTable[i].Flags & RFD_RECALL) {
            // If the RECALL flag is set against an FD, it was done so because the subscribed routine needs to manually check
            // for incoming/outgoing data.  These are considered 'one-off' checks, so the subscriber will need to set the RECALL flag
            // again if it wants this service maintained.
            //
            // See the SSL support routines as an example of this requirement.

            glFDTable[i].Flags &= ~RFD_RECALL; // Turn off the recall flag as each call is a one-off

            if (!(glFDTable[i].Flags & RFD_ALLOW_RECURSION)) {
               glFDTable[i].Flags |= RFD_STOP_RECURSE;
            }

            if (glFDTable[i].Routine) {
               glFDTable[i].Routine(glFDTable[i].FD, glFDTable[i].Data);

               if (glFDTable[i].Flags & RFD_RECALL) {
                  // If the RECALL flag was reapplied by the subscriber, we need to employ a reduced timeout so that the subscriber doesn't get 'stuck'.

                  if (Timeout > 10) Timeout = 10;
               }
            }

            glFDTable[i].Flags &= ~RFD_STOP_RECURSE;
         }
      }
   }

   LONG result = 0;
   if (Timeout < 0) { // Sleep indefinitely
      if (glTotalFDs > 0) result = select(maxfd + 1, &fread, &fwrite, NULL, NULL);
      else pause();
   }
   else if (Timeout IS 0) { // A zero second timeout means that we just poll the FD's and call them if they have data.  This is really useful for periodically flushing the FD's.
      if (glTotalFDs > 0) {
         tv.tv_sec = 0;
         tv.tv_usec = 0;
         result = select(maxfd + 1, &fread, &fwrite, NULL, &tv);
      }
   }
   else {
      if (glTotalFDs > 0) {
         tv.tv_sec = Timeout / 1000;
         tv.tv_usec = (Timeout - (tv.tv_sec * 1000)) * 1000;
         result = select(maxfd + 1, &fread, &fwrite, NULL, &tv);
      }
      else {
         if (Timeout > MAX_MSEC) Timeout = MAX_MSEC; // Do not sleep too long, in case the Linux kernel doesn't wake us when signalled (kernel 2.6)
         time.tv_sec  = Timeout/1000;
         time.tv_nsec = (Timeout - (time.tv_sec * 1000)) * 1000;
         nanosleep(&time, NULL);
      }
   }

   UBYTE buffer[64];
   if (result > 0) {
      for (LONG i=0; i < glTotalFDs; i++) {
         if (glFDTable[i].Flags & RFD_READ) {  // Readable FD support
            if (FD_ISSET(glFDTable[i].FD, &fread)) {
               if (!(glFDTable[i].Flags & RFD_ALLOW_RECURSION)) {
                  glFDTable[i].Flags |= RFD_STOP_RECURSE;
               }

               if (glFDTable[i].Routine) {
                  glFDTable[i].Routine(glFDTable[i].FD, glFDTable[i].Data);
               }
               else if (glFDTable[i].FD IS glSocket) {
                  socklen_t socklen;
                  struct sockaddr_un *sockpath = get_socket_path(glProcessID, &socklen);
                  recvfrom(glSocket, &buffer, sizeof(buffer), 0, (struct sockaddr *)sockpath, &socklen);
               }
               else while (read(glFDTable[i].FD, &buffer, sizeof(buffer)) > 0);
            }

            glFDTable[i].Flags &= ~RFD_STOP_RECURSE;
         }

         if (glFDTable[i].Flags & RFD_WRITE) { // Writeable FD support
            if (FD_ISSET(glFDTable[i].FD, &fwrite)) {
               if (!(glFDTable[i].Flags & RFD_ALLOW_RECURSION)) {
                  glFDTable[i].Flags |= RFD_STOP_RECURSE;
               }

               if (glFDTable[i].Routine) {
                  glFDTable[i].Routine(glFDTable[i].FD, glFDTable[i].Data);
               }
            }

            glFDTable[i].Flags &= ~RFD_STOP_RECURSE;
         }
      }
   }
   else if (result IS -1) {
      if (errno IS EINTR); // Interrupt caught during sleep
      else if (errno IS EBADF) {
         struct stat info;

         // At least one of the file descriptors is invalid - it is most likely that the file descriptor was closed and the
         // code responsible did not de-register the descriptor.

         for (LONG i=0; i < glTotalFDs; i++) {
            if (fstat(glFDTable[i].FD, &info) < 0) {
               if (errno IS EBADF) {
                  log.warning("FD %d was closed without a call to deregister it.", glFDTable[i].FD);
                  RegisterFD(glFDTable[i].FD, RFD_REMOVE|RFD_READ|RFD_WRITE|RFD_EXCEPT, NULL, NULL);
                  break;
               }
            }
         }
      }
      else log.warning("select() error %d: %s", errno, strerror(errno));
   }

   return ERR_Okay;
}
#endif

/*****************************************************************************
** Function: sleep_task() - Windows version
*/

#ifdef _WIN32
ERROR sleep_task(LONG Timeout, BYTE SystemOnly)
{
   parasol::Log log(__FUNCTION__);

   if (!tlMainThread) {
      log.warning("Only the main thread can call this function.");
      return ERR_Failed;
   }
   else if (tlPublicLockCount > 0) {
      log.warning("You cannot sleep while still holding %d global locks!", tlPublicLockCount);
      return ERR_Okay;
   }
   else if (tlPrivateLockCount != 0) {
      char buffer[120];
      WORD pos = 0;
      for (LONG i=0; (i < glNextPrivateAddress) and ((size_t)pos < sizeof(buffer)-1); i++) {
         if (glPrivateMemory[i].AccessCount > 0) {
            pos += snprintf(buffer+pos, sizeof(buffer)-pos, "#%d +%d ", glPrivateMemory[i].MemoryID, glPrivateMemory[i].AccessCount);
         }
      }

      if (pos > 0) log.warning("WARNING - Sleeping with %d private locks held (%s)", tlPrivateLockCount, buffer);
   }

   LARGE time_end;
   LONG i;

   //log.traceBranch("Time-out: %d, TotalFDs: %d", Timeout, glTotalFDs);

   if (Timeout < 0) {
      Timeout = -1; // A value of -1 means to wait indefinitely
      time_end = 0x7fffffffffffffffLL;
   }
   else time_end = (PreciseTime()/1000LL) + Timeout;

   while (1) {
      // About this process: it will wait until either:
      //   Something is received on a registered WINHANDLE
      //   The thread-lock is released by another task (see wake_task).
      //   A window message is received (if tlMessageBreak is TRUE)

      WINHANDLE handles[glTotalFDs+2]; // +1 for thread-lock, +1 for validation lock.
      LONG lookup[glTotalFDs+2]; // Lookup into glFDTable

      handles[0] = get_threadlock();
      handles[1] = glValidationSemaphore;
      lookup[0] = 0; // Zero disables the lookup.
      lookup[1] = 0;
      LONG total = 2;

      if ((SystemOnly) and (!tlMessageBreak)) {
         log.trace("Sleeping on process semaphore only.");
      }
      else {
         for (LONG i=0; i < glTotalFDs; i++) {
            if (glFDTable[i].Flags & RFD_SOCKET) continue; // Ignore network socket FDs (triggered as normal windows messages)

            log.trace("Listening to %d, Read: %d, Write: %d, Routine: %p, Flags: $%.2x", (LONG)(MAXINT)glFDTable[i].FD, (glFDTable[i].Flags & RFD_READ) ? 1 : 0, (glFDTable[i].Flags & RFD_WRITE) ? 1 : 0, glFDTable[i].Routine, glFDTable[i].Flags);

            if (glFDTable[i].Flags & (RFD_READ|RFD_WRITE|RFD_EXCEPT)) {
               lookup[total] = i;
               handles[total++] = glFDTable[i].FD;
            }
            else {
               log.warning("FD " PF64() " has no READ/WRITE/EXCEPT flag setting - de-registering.", (LARGE)glFDTable[i].FD);
               RegisterFD(glFDTable[i].FD, RFD_REMOVE|RFD_READ|RFD_WRITE|RFD_EXCEPT, NULL, NULL);
               i--;
            }
         }
      }

      if (Timeout > 0) log.trace("Sleeping on %d handles for up to %dms.  MsgBreak: %d", total, Timeout, tlMessageBreak);

      LONG sleeptime = time_end - (PreciseTime()/1000LL);
      if (sleeptime < 0) sleeptime = 0;

      i = winWaitForObjects(total, handles, sleeptime, tlMessageBreak);

      // Return Codes/Reasons for breaking:
      //
      //   -1 = Timed out
      //   -2 = Error (usually a bad handle, like a recently freed handle is still on handle list).
      //   -3 = Message received in windows message queue or event system.
      //    0 = Task semaphore signalled
      //   >0 = Handle signalled

      if (i IS 1) {
         log.trace("Process validation request signalled.");
         if (glValidateProcessID) {
            validate_process(glValidateProcessID);
            glValidateProcessID = 0;
         }
      }
      else if ((i > 1) and (i < total)) {
         log.trace("WaitForObjects() Handle: %d (%d) of %d, Timeout: %d, Break: %d", i, lookup[i], total, Timeout, tlMessageBreak);

         // Process only the handle routine that was signalled: NOTE: This is potentially an issue if the handle is early on in the list and is being frequently
         // signalled - it will mean that the other handles aren't going to get signalled until the earlier one stops being signalled.

         LONG ifd = lookup[i];
         if (glFDTable[ifd].Routine) glFDTable[ifd].Routine(glFDTable[ifd].FD, glFDTable[ifd].Data);

         if ((glTotalFDs > 1) and (ifd < glTotalFDs-1)) { // Move the most recently signalled handle to the end of the queue
            FDTable last = glFDTable[glTotalFDs-1];
            glFDTable[glTotalFDs-1] = glFDTable[ifd];
            glFDTable[ifd] = last;
         }

         break;
      }
      else if (i IS -2) {
         log.warning("WaitForObjects() failed, bad handle " PF64() ".  Deregistering automatically.", (LARGE)handles[0]);
         RegisterFD((HOSTHANDLE)handles[0], RFD_REMOVE|RFD_READ|RFD_WRITE|RFD_EXCEPT, NULL, NULL);
      }
      else if (i IS -4) {
         log.warning("WaitForObjects() failure - error not handled.");
      }
      else if (i IS -1) {
         // On Windows we can sometimes receive a timeout return code despite no change to the system time, so don't break.
         // The most common reason for this is that a callback-based interrupt that uses a timer has been triggered.
      }
      else {
         //log.trace("A message in the windows message queue needs processing.");
         break; // Message in windows message queue that needs to be processed, so break
      }

      LARGE systime = (PreciseTime()/1000LL);
      if (systime < time_end) Timeout = time_end - systime;
      else break;
   }

   return ERR_Okay;
}

#endif // _WIN32

/*****************************************************************************
** This function complements sleep_task().  It is useful for waking the main thread of a process when it is waiting for
** new messages to come in.
**
** It's not a good idea to call wake_task() while locks are active because the Core might choose to instantly switch
** to the foreign task when we wake it up.  Having a lock would then increase the likelihood of delays and time-outs.
*/

#ifdef __unix__ // TLS data for the wake_task() socket.
static pthread_key_t keySocket;
static pthread_once_t keySocketOnce = PTHREAD_ONCE_INIT;
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
static void thread_socket_free(void *Socket) { close(PTR_TO_HOST(Socket)); }
static void thread_socket_init(void) { pthread_key_create(&keySocket, thread_socket_free); }
#endif

void wake_task(LONG TaskIndex, CSTRING Caller)
{
   parasol::Log log(__FUNCTION__);

   if (TaskIndex < 0) return;
   if (!shTasks[TaskIndex].ProcessID) return;

   if (tlPublicLockCount > 0) {
      if (glProgramStage != STAGE_SHUTDOWN) log.warning("[Process %d] Warning: Do not call me when holding %d global locks.  (Caller: %s) - Try function trace.", glProcessID, tlPublicLockCount, Caller);
   }

#ifdef __unix__

   // Sockets are the preferred method because they use FD's.  This plays nice with the traditional message and
   // locking system employed by sleep_task() and can be used in conjunction with FD's for things like incoming
   // network messages.
   //
   // NOTE: If sockets are not available on the host then you can use a mutex for sleeping, BUT this would mean that
   // every FD has to be given its own thread for processing.

   UBYTE msg = 1;

   // Each thread gets its own comm socket for dispatch, because allowing them to all use the same socket has been
   // discovered to cause problems.  The use of pthread keys also ensures that the socket FD is automatically closed
   // when the thread is removed.

   #pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
   #pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
   HOSTHANDLE tlSendSocket;
   pthread_once(&keySocketOnce, thread_socket_init);
   if (!(tlSendSocket = PTR_TO_HOST(pthread_getspecific(keySocket)))) {
      if ((tlSendSocket = socket(PF_UNIX, SOCK_DGRAM, 0)) != -1) {
         fcntl(tlSendSocket, F_SETFL, O_NONBLOCK);
         pthread_setspecific(keySocket, (void *)tlSendSocket);
      }
      else {
         log.warning("Failed to create a new socket communication point.");
         return;
      }
   }

   // Place a single character in the destination task's socket to indicate that there are messages to be processed.

   socklen_t socklen;
   struct sockaddr_un *sockpath = get_socket_path(shTasks[TaskIndex].ProcessID, &socklen);
   if (sendto(tlSendSocket, &msg, sizeof(msg), MSG_DONTWAIT, (struct sockaddr *)sockpath, socklen) IS -1) {
      if (errno != EAGAIN) {
         log.warning("sendto(%d) from %d failed: %s", shTasks[TaskIndex].ProcessID, glProcessID, strerror(errno));
         glValidateProcessID = shTasks[TaskIndex].ProcessID;
      }
   }

#elif _WIN32

   wake_waitlock(shTasks[TaskIndex].Lock, shTasks[TaskIndex].ProcessID, 1);

#endif
}
