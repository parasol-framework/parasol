
#include <memory>

namespace parasol {

//****************************************************************************
// Resource guard for any allocation that can be freed with FreeResource()

template <class T>
class GuardedResource { // C++ wrapper for terminating resources when scope is lost
   private:
      APTR resource;
   public:
      GuardedResource(T Resource) { resource = Resource; }
      ~GuardedResource() { FreeResource(resource); }
};

template <class T>
std::unique_ptr<GuardedResource<T>> ScopedResource(T Resource)
{
   return std::make_unique<GuardedResource<T>>(Resource);
}

//****************************************************************************
// Resource guard for temporarily switching context and back when out of scope.
//
// Usage: parasol::SwitchObject(YourObject)

template <class T>
class GuardedContext { // C++ wrapper for changing the current context with a resource guard in place
   private:
      OBJECTPTR old_context;
   public:
      GuardedContext(T NewContext) { old_context = SetContext(NewContext); }
      ~GuardedContext() { SetContext(old_context); }
};

template <class T>
std::unique_ptr<GuardedContext<T>> SwitchContext(T Object)
{
   return std::make_unique<GuardedContext<T>>(Object);
}

template <class T>
std::unique_ptr<GuardedContext<T>> SwitchContext(FUNCTION *Function)
{
   return std::make_unique<GuardedContext<T>>(Function->StdC.Context);
}

//****************************************************************************

class Log { // C++ wrapper for Parasol's log functionality
   private:
      class LogReturnBranch {
         public:
            LogReturnBranch() { }
            ~LogReturnBranch() {
               LogReturn();
            }
      };

   public:
      CSTRING header;

      Log(CSTRING Header) {
         header = Header;
      }

      Log() {
         header = NULL;
      }

      std::unique_ptr<LogReturnBranch> branch(CSTRING Message, ...) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_DEBUG|VLF_BRANCH, header, Message, arg);
         va_end(arg);
         return std::make_unique<LogReturnBranch>();
      }

      #ifdef DEBUG
      std::unique_ptr<LogReturnBranch> traceBranch(CSTRING Message, ...) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_DEBUG|VLF_BRANCH, header, Message, arg);
         va_end(arg);
         return std::make_unique<LogReturnBranch>();
      }
      #else
      void traceBranch(CSTRING Message, ...) { }
      #endif

      void app(CSTRING Message, ...) { // Info level, recommended for applications only
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_DEBUG, header, Message, arg);
         va_end(arg);
      }

      void msg(CSTRING Message, ...) { // Defaults to debug level, recommended for modules
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_DEBUG, header, Message, arg);
         va_end(arg);
      }

      void warning(CSTRING Message, ...) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_WARNING, header, Message, arg);
         va_end(arg);
      }

      void error(CSTRING Message, ...) { // NB: Use for messages intended for the user, not the developer
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_ERROR, header, Message, arg);
         va_end(arg);
      }

      void debug(CSTRING Message, ...) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_DEBUG, header, Message, arg);
         va_end(arg);
      }

      void function(CSTRING Message, ...) { // Equivalent to branch() but without a new branch being created
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_DEBUG|VLF_FUNCTION, header, Message, arg);
         va_end(arg);
      }

      ERROR error(ERROR Code) { // Technically a warning
         LogError(0, Code);
         return Code;
      }

      ERROR error(LONG Header, ERROR Code) { // Technically a warning
         LogError(Header, Code);
         return Code;
      }

      ERROR warning(ERROR Code) {
         LogError(0, Code);
         return Code;
      }

      ERROR warning(LONG Header, ERROR Code) {
         LogError(Header, Code);
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
