#ifndef PARASOL_MAIN_HPP
#define PARASOL_MAIN_HPP 1
#ifdef __cplusplus

#include <memory>
#include <optional>

namespace parasol {

//****************************************************************************

template <class T>
class ScopedAccessMemory { // C++ wrapper for automatically releasing shared memory
   public:
      LONG id;
      T *ptr;
      ERROR error;

      ScopedAccessMemory(LONG ID, LONG Flags, LONG Milliseconds = 5000) {
         id = ID;
         error = AccessMemory(ID, Flags, Milliseconds, (APTR *)&ptr);
      }

      ~ScopedAccessMemory() { if (!error) ReleaseMemory(ptr); }

      bool granted() { return error == ERR_Okay; }

      void release() {
         if (!error) {
            ReleaseMemory(ptr);
            error = ERR_NotLocked;
         }
      }
};

//****************************************************************************

template <class T>
class ScopedObject { // C++ wrapper for automatically freeing an object
   public:
      T *obj;

      ScopedObject(T *Object) { obj = Object; }
      ScopedObject() { obj = NULL; }
      ~ScopedObject() { if (obj) acFree(obj); }
};

//****************************************************************************

template <class T>
class ScopedObjectLock { // C++ wrapper for automatically freeing an object
   public:
      ERROR error;
      T *obj;

      ScopedObjectLock(OBJECTID ObjectID, LONG Milliseconds = 3000) {
         error = AccessObject(ObjectID, Milliseconds, &obj);
      }

      ScopedObjectLock(OBJECTPTR Object, LONG Milliseconds = 3000) {
         error = AccessPrivateObject(Object, Milliseconds);
         obj = (T *)Object;
      }

      ScopedObjectLock() { obj = NULL; error = ERR_NotLocked; }
      ~ScopedObjectLock() { if (!error) ReleaseObject((OBJECTPTR)obj); }
      bool granted() { return error == ERR_Okay; }
};

//****************************************************************************

class ScopedSysLock { // C++ wrapper for terminating a system lock when scope is lost
   private:
      LONG index;

   public:
      ERROR error; // ERR_Okay is used to indicate that the lock is acquired

      ScopedSysLock(LONG Index, LONG Milliseconds) {
         error = SysLock(Index, Milliseconds);
         index = Index;
      }

      ~ScopedSysLock() { if (!error) SysUnlock(index); }

      bool granted() { return error == ERR_Okay; }

      void release() {
         if (!error) {
            SysUnlock(index);
            error = ERR_NotLocked;
         }
      }

      ERROR acquire(LONG Milliseconds) {
         if (error) error = SysLock(index, Milliseconds);
         return error;
      }
};

//****************************************************************************
// Resource guard for any allocation that can be freed with FreeResource()
//
// Usage: parasol::GuardedResource resource(thing)

template <class T>
class GuardedResource { // C++ wrapper for terminating resources when scope is lost
   private:
      APTR resource;
   public:
      GuardedResource(T Resource) { resource = Resource; }
      ~GuardedResource() { FreeResource(resource); }
};

//****************************************************************************
// Resource guard for temporarily switching context and back when out of scope.
//
// Usage: parasol::SwitchContext context(YourObject)

template <class T>
class SwitchContext { // C++ wrapper for changing the current context with a resource guard in place
   private:
      OBJECTPTR old_context;
   public:
      SwitchContext(T NewContext) {
         if (NewContext) old_context = SetContext((OBJECTPTR)NewContext);
         else old_context = NULL;
      }
      ~SwitchContext() { if (old_context) SetContext(old_context); }
};

//****************************************************************************

class Log { // C++ wrapper for Parasol's log functionality
   private:
      LONG branches = 0;

   public:
      CSTRING header;

      Log() {
         header = NULL;
      }

      Log(CSTRING Header) {
         header = Header;
      }

      ~Log() {
         while (branches > 0) { branches--; LogReturn(); }
      }

      void branch(CSTRING Message = "", ...) __attribute__((format(printf, 2, 3))) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_API|VLF_BRANCH, header, Message, arg);
         va_end(arg);
         branches++;
      }

      #ifdef DEBUG
      void traceBranch(CSTRING Message = "", ...) __attribute__((format(printf, 2, 3))) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_TRACE|VLF_BRANCH, header, Message, arg);
         va_end(arg);
         branches++;
      }
      #else
      void traceBranch(CSTRING Message = "", ...) __attribute__((format(printf, 2, 3))) { }
      #endif

      void debranch() {
         branches--;
         LogReturn();
      }

      void app(CSTRING Message, ...) { // Info level, recommended for applications only
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_INFO, header, Message, arg);
         va_end(arg);
      }

      void msg(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // Defaults to API level, recommended for modules
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_API, header, Message, arg);
         va_end(arg);
      }

      void msg(LONG Flags, CSTRING Message, ...) __attribute__((format(printf, 3, 4))) { // Defaults to API level, recommended for modules
         va_list arg;
         va_start(arg, Message);
         VLogF(Flags, header, Message, arg);
         va_end(arg);
         if (Flags & VLF_BRANCH) branches++;
      }

      void extmsg(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // Extended API message
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_EXTAPI, header, Message, arg);
         va_end(arg);
      }

      void pmsg(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // "Parent message", uses the scope of the caller
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_API, NULL, Message, arg);
         va_end(arg);
      }

      void warning(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_WARNING, header, Message, arg);
         va_end(arg);
      }

      void error(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // NB: Use for messages intended for the user, not the developer
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_ERROR, header, Message, arg);
         va_end(arg);
      }

      void debug(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_DEBUG, header, Message, arg);
         va_end(arg);
      }

      void function(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // Equivalent to branch() but without a new branch being created
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_API|VLF_FUNCTION, header, Message, arg);
         va_end(arg);
      }

      ERROR error(ERROR Code) { // Technically a warning
         #ifdef PRV_CORE_MODULE
         FuncError(header, Code);
         #else
         HeadError(header, Code);
         #endif
         return Code;
      }

      ERROR warning(ERROR Code) {
         #ifdef PRV_CORE_MODULE
         FuncError(header, Code);
         #else
         HeadError(header, Code);
         #endif
         return Code;
      }

      void trace(CSTRING Message, ...) {
         #ifdef DEBUG
            va_list arg;
            va_start(arg, Message);
            VLogF(VLF_TRACE, header, Message, arg);
            va_end(arg);
         #endif
      }

      void traceWarning(CSTRING Message, ...) {
         #ifdef DEBUG
            va_list arg;
            va_start(arg, Message);
            VLogF(VLF_WARNING, header, Message, arg);
            va_end(arg);
         #endif
      }
};

} // namespace

#endif // __cplusplus
#endif
