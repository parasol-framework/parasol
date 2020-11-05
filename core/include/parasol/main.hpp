
#include <memory>

namespace parasol {

//****************************************************************************
// Resource guard for temporarily switching context and back when out of scope.
//
// Usage: parasol::SwitchObject(YourObject)

template <class T>
class Context { // C++ wrapper for changing the current context with a resource guard in place
   private:
      OBJECTPTR old_context;
   public:
      Context(T NewContext) { old_context = SetContext(NewContext); }
      ~Context() { SetContext(old_context); }
};

template <class T>
std::unique_ptr<Context<T>> SwitchContext(T Object)
{
   return std::make_unique<Context<T>>(Object);
}

template <class T>
std::unique_ptr<Context<T>> SwitchContext(FUNCTION *Function)
{
   return std::make_unique<Context<T>>(Function->StdC.Context);
}

//****************************************************************************

class Log { // C++ wrapper for Parasol's log functionality
   private:
      class LogReturnDummy {
         public:
            LogReturnDummy() { }
            ~LogReturnDummy() { }
      };

      class LogReturnBranch: public LogReturnDummy {
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
         VLogF(VLF_BRANCH, header, Message, arg);
         va_end(arg);
         return std::make_unique<LogReturnBranch>();
      }

      std::unique_ptr<LogReturnDummy> traceBranch(CSTRING Message, ...) {
         #ifdef DEBUG
            va_list arg;
            va_start(arg, Message);
            VLogF(VLF_BRANCH, header, Message, arg);
            va_end(arg);
            return std::make_unique<LogReturnBranch>();
         #else
            return std::make_unique<LogReturnDummy>();
         #endif
      }

      void msg(CSTRING Message, ...) {
         va_list arg;
         va_start(arg, Message);
         VLogF(0, header, Message, arg);
         va_end(arg);
      }

      void warning(CSTRING Message, ...) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_WARNING, header, Message, arg);
         va_end(arg);
      }

      void error(CSTRING Message, ...) {
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

      void function(CSTRING Message, ...) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_FUNCTION, header, Message, arg);
         va_end(arg);
      }

      void action(CSTRING Message, ...) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_FUNCTION, header, Message, arg);
         va_end(arg);
      }

      void method(CSTRING Message, ...) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_FUNCTION, header, Message, arg);
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
            VLogF(0, header, Message, arg);
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
