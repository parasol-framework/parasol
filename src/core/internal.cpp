/*********************************************************************************************************************

Functions that are internal to the Core.

*********************************************************************************************************************/

#ifdef __unix__
 #include <signal.h>
 #include <sys/wait.h>
#endif

#include "defs.h"

using namespace pf;

//********************************************************************************************************************

#ifdef __APPLE__
struct sockaddr_un * get_socket_path(LONG ProcessID, socklen_t *Size)
{
   // OSX doesn't support anonymous sockets, so we use /tmp instead.
   static THREADVAR struct sockaddr_un tlSocket;
   tlSocket.sun_family = AF_UNIX;
   *Size = sizeof(sa_family_t) + snprintf(tlSocket.sun_path, sizeof(tlSocket.sun_path), "/tmp/parasol.%d", ProcessID) + 1;
   return &tlSocket;
}
#elif __unix__
struct sockaddr_un * get_socket_path(LONG ProcessID, socklen_t *Size)
{
   static THREADVAR struct sockaddr_un tlSocket;
   static THREADVAR bool init = false;

   if (!init) {
      tlSocket.sun_family = AF_UNIX;
      clearmem(tlSocket.sun_path, sizeof(tlSocket.sun_path));
      tlSocket.sun_path[0] = '\0';
      tlSocket.sun_path[1] = 'p';
      tlSocket.sun_path[2] = 's';
      tlSocket.sun_path[3] = 'l';
      init = true;
   }

   ((LONG *)(tlSocket.sun_path+4))[0] = ProcessID;
   *Size = sizeof(sa_family_t) + 4 + sizeof(LONG);
   return &tlSocket;
}
#endif

//********************************************************************************************************************
// Fast lookup for matching file extensions with a valid class handler.

CLASSID lookup_class_by_ext(CLASSID Filter, std::string_view Ext)
{
   if (glWildClassMapTotal != std::ssize(glClassDB)) {
      for (auto it = glClassDB.begin(); it != glClassDB.end(); it++) {
         if ((Filter != CLASSID::NIL) and ((it->second.ClassID != Filter) and (it->second.ParentID != Filter))) continue;

         if (auto &rec = it->second; !rec.Match.empty()) {
            std::vector<std::string> list;
            pf::split(rec.Match, std::back_inserter(list), '|');
            for (auto & wild : list) {
               if (wild.starts_with("*.")) {
                  glWildClassMap.emplace(pf::strihash(wild.c_str() + 2), it->first);
               }
            }
         }
      }

      glWildClassMapTotal = glClassDB.size();
   }

   auto hash = pf::strihash(Ext);
   if (glWildClassMap.contains(hash)) {
      return glWildClassMap[hash];
   }

   return CLASSID::NIL;
}

//********************************************************************************************************************

ERR process_janitor(OBJECTID SubscriberID, LONG Elapsed, LONG TotalElapsed)
{
   if (glTasks.empty()) {
      glJanitorActive = false;
      return ERR::Terminate;
   }

#ifdef __unix__
   pf::Log log(__FUNCTION__);

   // Call waitpid() to check for zombie processes first.  This covers all processes within our own context, so our child processes, children of those children etc.

   // However, it can be 'blocked' from certain processes, e.g. those started from ZTerm.  Such processes are discovered in the second search routine.

   LONG childprocess, status;
   while ((childprocess = waitpid(-1, &status, WNOHANG)) > 0) {
      log.warning("Zombie process #%d discovered.", childprocess);

      for (auto &task : glTasks) {
         if (childprocess IS task.ProcessID) {
            task.ReturnCode = WEXITSTATUS(status);
            task.Returned   = true;
            validate_process(childprocess);
            break;
         }
      }
   }

   // Check all registered processes to see which ones are alive.  This routine can manage all processes, although exhibits
   // some problems with zombies, hence the earlier waitpid() routine to clean up such processes.

   for (auto &task : glTasks) {
      if ((kill(task.ProcessID, 0) IS -1) and (errno IS ESRCH)) {
         validate_process(task.ProcessID);
      }
   }

#elif _WIN32
   for (auto &task : glTasks) {
      if (!winCheckProcessExists(task.ProcessID)) {
         validate_process(task.ProcessID);
      }
   }
#endif

   return ERR::Okay;
}

/*********************************************************************************************************************

copy_args: Used for turning argument structures into sendable messages.

This function searches an argument structure for pointer and string types.  If it encounters them, it attempts to
convert them to a format that can be passed to other memory spaces.

A PTR|RESULT followed by a PTRSIZE indicates that the user has to supply a buffer to the function.  It is assumed that
the function will fill the buffer with data, which means that a result set has to be returned to the caller.  Example:

<pre>
Read(Bytes (FD_LONG), Buffer (FD_PTRRESULT), BufferSize (FD_PTRSIZE), &BytesRead (FD_LONGRESULT));
</pre>

A standard PTR followed by a PTRSIZE indicates that the user has to supply a buffer to the function.  It is assumed
that this is one-way traffic only, and the function will not fill the buffer with data.  Example:

<pre>
Write(Bytes (FD_LONG, Buffer (FD_PTR), BufferSize (FD_PTRSIZE), &BytesWritten (FD_LONGRESULT));
</pre>

If the function will return a memory block of its own, it must return the block as a MEMORYID, not a PTR.  The
allocation must be made using the object's MemFlags, as the action messaging functions will change between
public|untracked and private memory flags as necessary.  Example:

  Read(Bytes (FD_LONG), &BufferMID (FD_LONGRESULT), &BufferSize (FD_LONGRESULT));

*********************************************************************************************************************/

ERR copy_args(const struct FunctionField *Args, LONG ArgsSize, BYTE *ArgsBuffer, BYTE *Buffer, LONG BufferSize,
                    LONG *NewSize, CSTRING ActionName)
{
   pf::Log log("CopyArguments");
   BYTE *src, *data;
   LONG j, len;
   ERR error;
   STRING str;

   if ((!Args) or (!ArgsBuffer) or (!Buffer)) return log.warning(ERR::NullArgs);

   for (LONG i=0; (i < ArgsSize); i++) { // Copy the arguments to the buffer
      if (i >= BufferSize) return log.warning(ERR::BufferOverflow);
      Buffer[i] = ArgsBuffer[i];
   }

   LONG pos = 0;
   LONG offset = ArgsSize;
   for (LONG i=0; Args[i].Name; i++) {
      // If the current byte position in the argument structure exceeds the size of that structure, break immediately.

      if (pos >= ArgsSize) {
         log.error("Invalid action definition for \"%s\".  Amount of arguments exceeds limit of %d bytes.", ActionName, ArgsSize);
         break;
      }

      // Process the argument depending on its type

      if (Args[i].Type & FD_STR) { // Copy the string and make sure that there is enough space for it to fit inside the buffer.
         j = offset;
         if ((str = ((STRING *)(ArgsBuffer + pos))[0])) {
            for (len=0; (str[len]) and (offset < BufferSize); len++) Buffer[offset++] = str[len];
            if (offset < BufferSize) {
               Buffer[offset++] = 0;
               ((STRING *)(Buffer + pos))[0] = str;
            }
            else { error = ERR::BufferOverflow; goto looperror; }
         }
         else ((STRING *)(Buffer + pos))[0] = NULL;

         pos += sizeof(STRING);
      }
      else if (Args[i].Type & FD_PTR) {
         if (Args[i].Type & (FD_LONG|FD_PTRSIZE)) { // Pointer to long.
            if ((size_t)offset < (BufferSize - sizeof(LONG))) {
               ((LONG *)Buffer)[offset] = ((LONG *)(ArgsBuffer + pos))[0];
               ((APTR *)(Buffer + pos))[0] = ArgsBuffer + offset;
               offset += sizeof(LONG);
            }
            else { error = ERR::BufferOverflow; goto looperror; }
         }
         else if (Args[i].Type & (FD_DOUBLE|FD_LARGE)) { // Pointer to large/double
            if ((size_t)offset < (BufferSize - sizeof(LARGE))) {
               ((LARGE *)Buffer)[offset] = ((LARGE *)(ArgsBuffer + pos))[0];
               ((APTR *)(Buffer + pos))[0] = ArgsBuffer + offset;
               offset += sizeof(LARGE);
            }
            else { error = ERR::BufferOverflow; goto looperror; }
         }
         else {
            // There are two types of pointer references:

            // 1. Receive Pointers
            //    If FD_RESULT is used, this indicates that there is a result to be stored in a buffer setup by the
            //    caller.  The size of this is determined by a following FD_PTRSIZE.

            // 2. Send Pointers
            //    Standard FD_PTR types must be followed by an FD_PTRSIZE that indicates the amount of data that needs
            //    to be passed to the other task.  A public memory block is allocated and filled with data for this
            //    particular type.

            if (!(Args[i+1].Type & FD_PTRSIZE)) {
               // If no PTRSIZE is specified, send a warning
               log.warning("Warning: Argument \"%s\" is not followed up with a PTRSIZE definition.", Args[i].Name);
               ((APTR *)(Buffer + pos))[0] = NULL;
            }
            else {
               LONG memsize = ((LONG *)(ArgsBuffer + pos + sizeof(APTR)))[0];
               if (memsize > 0) {
                  if (Args[i].Type & FD_RESULT) { // "Receive" pointer type: Prepare a buffer so that we can accept a result
                     APTR mem;
                     if (AllocMemory(memsize, MEM::NO_CLEAR, &mem, NULL) IS ERR::Okay) {
                        ((APTR *)(Buffer + pos))[0] = mem;
                     }
                     else { error = ERR::AllocMemory; goto looperror; }
                  }
                  else {
                     // "Send" pointer type: Prepare the argument structure for sending data to the other task
                     if ((src = ((BYTE **)(ArgsBuffer + pos))[0])) { // Get the data source pointer
                        if (memsize > MSG_MAXARGSIZE) {
                           // For large data areas, we need to allocate them as public memory blocks
                           if (AllocMemory(memsize, MEM::NO_CLEAR, (void **)&data, NULL) IS ERR::Okay) {
                              ((APTR *)(Buffer + pos))[0] = data;
                              copymem(src, data, memsize);
                           }
                           else { error = ERR::AllocMemory; goto looperror; }
                        }
                        else {
                           ((APTR *)(Buffer + pos))[0] = ArgsBuffer + offset; // Record the address at which we are going to write the data
                           for (LONG len=0; len < memsize; len++) {
                              if (offset >= BufferSize) {
                                 error = ERR::BufferOverflow;
                                 goto looperror;
                              }
                              else Buffer[offset++] = src[len];
                           }
                        }
                     }
                     else ((LONG *)(Buffer + pos))[0] = 0;
                  }
               }
               else ((LONG *)(Buffer + pos))[0] = 0;
            }
         }
         pos += sizeof(APTR);
      }
      else if (Args[i].Type & (FD_LONG|FD_PTRSIZE)) pos += sizeof(LONG);
      else if (Args[i].Type & (FD_DOUBLE|FD_LARGE)) pos += sizeof(LARGE);
      else log.warning("Bad type definition for argument \"%s\".", Args[i].Name);
   }

   *NewSize = offset;
   return ERR::Okay;

looperror:
   // When an error occurs inside the loop, we back-track through the position at where the error occurred and free any
   // memory allocations that have been made.

   return log.warning(error);
}

//********************************************************************************************************************
// This version of free_ptr_args() is for thread-based execution.  Used by thread_action() in lib_actions.c

void local_free_args(APTR Parameters, const struct FunctionField *Args)
{
   LONG pos = 0;
   for (LONG i=0; Args[i].Name; i++) {
      if ((Args[i].Type & FD_PTR) and (Args[i+1].Type & FD_PTRSIZE)) {
         LONG size = ((LONG *)((BYTE *)Parameters + pos + sizeof(APTR)))[0];
         if ((Args[i].Type & FD_RESULT) or (size > MSG_MAXARGSIZE)) {
            APTR pointer;
            if ((pointer = ((APTR *)((BYTE *)Parameters + pos))[0])) {
               ((APTR *)((BYTE *)Parameters + pos))[0] = 0;
               FreeResource(pointer);
            }
         }
         pos += sizeof(APTR);
      }
      else if (Args[i].Type & (FD_DOUBLE|FD_LARGE)) pos += sizeof(LARGE);
      else pos += sizeof(LONG);
   }
}

//********************************************************************************************************************
// Resolves pointers and strings within an ActionMessage structure.

ERR resolve_args(APTR Parameters, const struct FunctionField *Args)
{
   pf::Log log(__FUNCTION__);

   auto Buffer = (BYTE *)Parameters;
   LONG pos = 0;
   for (LONG i=0; Args[i].Name; i++) {
      if (Args[i].Type & FD_STR) {
         // Replace the offset with a pointer
         if (((LONG *)(Buffer + pos))[0]) {
            ((STRING *)(Buffer + pos))[0] = Buffer + ((LONG *)(Buffer + pos))[0];
         }
         pos += sizeof(STRING);
      }
      else if ((Args[i].Type & FD_PTR) and (Args[i+1].Type & FD_PTRSIZE)) {
         LONG size = ((LONG *)(Buffer + pos + sizeof(APTR)))[0];
         if ((Args[i].Type & FD_RESULT) or (size > MSG_MAXARGSIZE)) {
            // Gain exclusive access to the public memory block that was allocated for this argument, and store the pointer to it.
            // The memory block will need to be released by the routine that called our function.

            if (auto mid = ((MEMORYID *)(Buffer + pos))[0]) {
               log.warning("Bad memory ID #%d for arg \"%s\", not a public allocation.", mid, Args[i].Name);
               return ERR::AccessMemory;
            }
         }
         else if (((LONG *)(Buffer + pos))[0] > 0) {
            ((APTR *)(Buffer + pos))[0] = Buffer + ((LONG *)(Buffer + pos))[0];
         }
         else ((APTR *)(Buffer + pos))[0] = NULL;

         pos += sizeof(APTR);
      }
      else if (Args[i].Type & (FD_DOUBLE|FD_LARGE)) pos += sizeof(LARGE);
      else pos += sizeof(LONG);
   }
   return ERR::Okay;
}
