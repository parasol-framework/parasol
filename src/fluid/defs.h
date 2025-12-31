#pragma once

#define LUA_COMPILED "-- $FLUID:compiled"
constexpr int SIZE_READ = 1024;

#include <list>
#include <unordered_set>
#include <set>
#include <array>
#include <shared_mutex>
#include <parasol/strings.hpp>
#include <parasol/modules/regex.h>
#include <parasol/modules/fluid.h>
#include <thread>
#include <string_view>
#include <span>
#include <concepts>

#include "lj_obj.h"
#include "lauxlib.h"

using namespace pf;

template <class T> T ALIGN64(T a) { return (((a) + 7) & (~7)); }
template <class T> T ALIGN32(T a) { return (((a) + 3) & (~3)); }

extern CSTRING const glBytecodeNames[];
extern bool glPrintMsg;

//********************************************************************************************************************

[[maybe_unused]] static AET ff_to_aet(int Type)
{
   if (Type & FD_POINTER)     return AET::PTR;
   else if (Type & FD_OBJECT) return AET::STRUCT;
   else if (Type & FD_STRING) {
      if (Type & FD_CPP) return AET::STR_CPP;
      else return AET::CSTR;
   }
   else if (Type & FD_FLOAT)   return AET::FLOAT;
   else if (Type & FD_DOUBLE)  return AET::DOUBLE;
   else if (Type & FD_INT64)   return AET::INT64;
   else if (Type & FD_INT)     return AET::INT32;
   else if (Type & FD_WORD)    return AET::INT16;
   else if (Type & FD_BYTE)    return AET::BYTE;
   else return AET::MAX;
}

//********************************************************************************************************************

struct CaseInsensitiveMap {
   bool operator() (const std::string &lhs, const std::string &rhs) const {
      return ::strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
   }
};

struct CaseInsensitiveHash {
   std::size_t operator()(const std::string& s) const noexcept {
      std::string lower = s;
      std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
      return std::hash<std::string>{}(lower);
   }
};

struct CaseInsensitiveEqual {
   bool operator()(const std::string& lhs, const std::string& rhs) const noexcept {
      return ::strcasecmp(lhs.c_str(), rhs.c_str()) == 0;
   }
};

struct CaseInsensitiveHashView {
   std::size_t operator()(std::string_view s) const noexcept {
      std::size_t hash = 5381;
      for (char c : s) {
         hash = ((hash << 5) + hash) + std::tolower(static_cast<unsigned char>(c));
      }
      return hash;
   }
};

struct CaseInsensitiveEqualView {
   bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
      return iequals(lhs, rhs);
   }
};

extern ankerl::unordered_dense::map<std::string_view, ACTIONID, CaseInsensitiveHashView, CaseInsensitiveEqualView> glActionLookup;
extern struct ActionTable *glActions;
extern OBJECTPTR modDisplay; // Required by fluid_input.c
extern OBJECTPTR modFluid;
extern OBJECTPTR modRegex;
extern OBJECTPTR glFluidContext;
extern OBJECTPTR clFluid;
extern JOF glJitOptions;
extern ankerl::unordered_dense::map<std::string_view, uint32_t> glStructSizes;
extern ankerl::unordered_dense::map<struct_name, struct_record, struct_hash> glStructs;

//********************************************************************************************************************
// Compile-time constant value (64-bit integer or double)

struct FluidConstant {
   enum class Type : uint8_t { Int64, Double };
   Type type;
   union {
      int64_t i64;
      double f64;
   } value;

   constexpr FluidConstant(int64_t v) : type(Type::Int64) { value.i64 = v; }
   constexpr FluidConstant(double v) : type(Type::Double) { value.f64 = v; }

   [[nodiscard]] constexpr lua_Number to_number() const {
      return (type IS Type::Int64) ? lua_Number(value.i64) : value.f64;
   }
};

// Global constant registry - case-sensitive, owns string keys
// Protected by glConstantMutex for thread-safe access
extern ankerl::unordered_dense::map<uint32_t, FluidConstant> glConstantRegistry;
extern std::shared_mutex glConstantMutex;

//********************************************************************************************************************
// Helper: build a std::string_view from a Lua string argument.
// Raises a Lua error if the argument at 'idx' is not a string (delegates to luaL_checklstring).

static inline std::string_view lua_checkstringview(lua_State *L, int idx)
{
   size_t len = 0;
   if (auto s = luaL_checklstring(L, idx, &len)) return std::string_view{s, len};
   else return std::string_view{};
}

// This version doesn't raise an error if the argument is not a string.

static inline std::string_view lua_tostringview(lua_State *L, int idx)
{
   size_t len = 0;
   if (auto s = lua_tolstring(L, idx, &len)) return std::string_view{s, len};
   else return std::string_view{};
}

//********************************************************************************************************************
// Standard hash computation, but stops when it encounters a character outside of A-Za-z0-9 range
// Note that struct name hashes are case sensitive.

inline uint32_t STRUCTHASH(CSTRING String)
{
   uint32_t hash = 5381;
   uint8_t c;
   while ((c = *String++)) {
      if ((c >= 'A') and (c <= 'Z'));
      else if ((c >= 'a') and (c <= 'z'));
      else if ((c >= '0') and (c <= '9'));
      else break;
      hash = ((hash<<5) + hash) + c;
   }
   return hash;
}

//********************************************************************************************************************

struct code_reader_handle {
   objFile *File;
   APTR Buffer;
};

struct actionmonitor {
   struct object *Object;      // Fluid.obj original passed in for the subscription.
   const FunctionField *Args;  // The args of the action/method are stored here so that we can build the arg value table later.
   int     Function;          // Index of function to call back.
   int     Reference;         // A custom reference to pass to the callback (optional)
   ACTIONID ActionID;          // Action being monitored.
   OBJECTID ObjectID;          // Object being monitored

   actionmonitor() {}

   ~actionmonitor() {
      if (ObjectID) {
         pf::Log log(__FUNCTION__);
         log.trace("Unsubscribe action %s from object #%d", glActions[int(ActionID)].Name, ObjectID);
         OBJECTPTR obj;
         if (AccessObject(ObjectID, 3000, &obj) IS ERR::Okay) {
            UnsubscribeAction(obj, ActionID);
            ReleaseObject(obj);
         }
      }
   }

   actionmonitor(actionmonitor &&move) noexcept :
      Object(move.Object), Args(move.Args), Function(move.Function), Reference(move.Reference), ActionID(move.ActionID), ObjectID(move.ObjectID) {
      move.ObjectID = 0;
   }

   actionmonitor& operator=(actionmonitor &&move) = default;
};

//********************************************************************************************************************

struct eventsub {
   int    Function;     // Lua function index
   EVENTID EventID;      // Event message ID
   APTR    EventHandle;

   eventsub(int pFunction, EVENTID pEventID, APTR pEventHandle) :
      Function(pFunction), EventID(pEventID), EventHandle(pEventHandle) { }

   ~eventsub() {
      if (EventHandle) UnsubscribeEvent(EventHandle);
   }

   eventsub(eventsub &&move) noexcept :
      Function(move.Function), EventID(move.EventID), EventHandle(move.EventHandle) {
      move.EventHandle = nullptr;
   }

   eventsub& operator=(eventsub &&move) = default;
};

//********************************************************************************************************************

struct datarequest {
   OBJECTID SourceID;
   int Callback;
   int64_t TimeCreated;

   datarequest(OBJECTID pSourceID, int pCallback) : SourceID(pSourceID), Callback(pCallback) {
      TimeCreated = PreciseTime();
   }
};

#include "struct_def.h"

//********************************************************************************************************************
// Variable information captured during parsing when JOF::DIAGNOSE is enabled.

struct VariableInfo {
   BCLine line;
   BCLine column;
   std::string scope;
   std::string name;
   FluidType type;
   bool is_global;
};

//********************************************************************************************************************

struct prvFluid {
   lua_State *Lua;                        // Lua instance
   std::vector<actionmonitor> ActionList; // Action subscriptions managed by subscribe()
   std::vector<eventsub> EventList;       // Event subscriptions managed by subscribeEvent()
   std::vector<datarequest> Requests;     // For drag and drop requests
   ankerl::unordered_dense::map<OBJECTID, int> StateMap;
   pf::vector<std::string> Procedures;
   std::vector<std::unique_ptr<std::jthread>> Threads; // Simple mechanism for auto-joining all the threads on object destruction
   APTR     FocusEventHandle;
   struct finput *InputList;           // Managed by the input interface
   DateTime CacheDate;
   ERR      CaughtError;               // Set to -1 to enable catching of ERR results.
   PERMIT   CachePermissions;
   JOF      JitOptions;
   int      LoadedSize;
   int      MainChunkRef;              // Registry reference to the main chunk for post-execution analysis
   uint8_t  Recurse;
   uint8_t  SaveCompiled;
   uint16_t Catch;                     // Operating within a catch() block if > 0
   uint16_t RequireCounter;
   int      ErrorLine;                 // Line at which the last error was thrown.
   int      CatchDepth = -1;           // Lua stack frame count for scope isolation in catch().
                                       // Set by fcmd_catch() via lua_getstack() frame counting.
                                       // Only calls at exactly this depth throw exceptions.
   std::vector<VariableInfo> CapturedVariables; // Variable declarations captured during parsing (JOF::DIAGNOSE)
};

// This structure is created & managed through the 'struct' interface

struct fstruct {
   APTR Data;          // Pointer to the structure data
   int StructSize;     // Size of the structure
   int AlignedSize;    // 64-bit alignment size of the structure.
   struct struct_record *Def; // The structure definition
   bool Deallocate;    // Deallocate the struct when Lua collects this resource.
};

struct fprocessing {
   double Timeout;
   std::list<ObjectSignal> *Signals;
};

class fregex {
public:
   Regex *regex_obj = nullptr;  // Compiled regex object
   std::string pattern;     // Original pattern string
   std::string error_msg;   // Error message if compilation failed
   REGEX flags;             // Compilation flags
   fregex(std::string_view Pattern, REGEX Flags) : pattern(Pattern), flags(Flags) { }
};

struct metafield {
   uint32_t ID;
   int GetFunction;
   int SetFunction;
};

constexpr int FIM_KEYBOARD = 1;
constexpr int FIM_DEVICE   = 2;

struct finput {
   objScript *Script;
   struct finput *Next;
   APTR   KeyEvent;
   OBJECTID SurfaceID;
   int    InputHandle;
   int    Callback;
   int    InputValue;
   JTYPE  Mask;
   int8_t Mode;
};

enum { NUM_DOUBLE=1, NUM_FLOAT, NUM_INT64, NUM_INT, NUM_INT16, NUM_BYTE };

struct fnumber { // TODO: Use std::variant
   int Type;     // Expressed as an FD_ flag.
   union {
      double  f64;
      float   f32;
      int64_t i64;
      int     i32;
      int16_t i16;
      int8_t  i8;
   };
};

struct module {
   struct Function *Functions = nullptr;
   OBJECTPTR Module = nullptr;
   ankerl::unordered_dense::map<uint32_t, int> FunctionMap; // Hash map for O(1) function lookup

   ~module() {
      if (Module) FreeResource(Module);
   }
};

constexpr uint32_t simple_hash(CSTRING String, uint32_t Hash = 5381) {
   while (auto c = *String++) Hash = ((Hash<<5) + Hash) + c;
   return Hash;
}

constexpr uint32_t char_hash(char Char, uint32_t Hash = 5381) {
   Hash = ((Hash<<5) + Hash) + Char;
   return Hash;
}

//********************************************************************************************************************
// obj_read is used to build efficient customised jump tables for object calls.

struct obj_read {
   typedef int JUMP(lua_State *, const struct obj_read &, struct object *);

   uint32_t Hash;
   JUMP *Call;
   APTR Data;

   auto operator<=>(const obj_read &Other) const {
       if (Hash < Other.Hash) return -1;
       if (Hash > Other.Hash) return 1;
       return 0;
   }

   obj_read(uint32_t pHash, JUMP pJump, APTR pData) : Hash(pHash), Call(pJump), Data(pData) { }
   obj_read(uint32_t pHash, JUMP pJump) : Hash(pHash), Call(pJump) { }
   obj_read(uint32_t pHash) : Hash(pHash) { }
};

inline auto read_hash = [](const obj_read &a, const obj_read &b) { return a.Hash < b.Hash; };

typedef std::set<obj_read, decltype(read_hash)> READ_TABLE;

//********************************************************************************************************************

[[maybe_unused]] [[nodiscard]] constexpr CSTRING next_line(CSTRING String) noexcept
{
   if (!String) return nullptr;

   while ((*String) and (*String != '\n') and (*String != '\r')) String++;
   while (*String IS '\r') String++;
   if (*String IS '\n') String++;
   while (*String IS '\r') String++;
   if (*String) return String;
   else return nullptr;
}

//********************************************************************************************************************

struct obj_write {
   typedef ERR JUMP(lua_State *, OBJECTPTR, struct Field *, int);

   uint32_t Hash;
   JUMP *Call;
   struct Field *Field;

   auto operator<=>(const obj_write &Other) const {
       if (Hash < Other.Hash) return -1;
       if (Hash > Other.Hash) return 1;
       return 0;
   }

   obj_write(uint32_t pHash, JUMP pJump, struct Field *pField) : Hash(pHash), Call(pJump), Field(pField) { }
   obj_write(uint32_t pHash, JUMP pJump) : Hash(pHash), Call(pJump) { }
   obj_write(uint32_t pHash) : Hash(pHash) { }
};

inline auto write_hash = [](const obj_write &a, const obj_write &b) { return a.Hash < b.Hash; };

typedef std::set<obj_write, decltype(write_hash)> WRITE_TABLE;

//********************************************************************************************************************

struct object {
   OBJECTPTR ObjectPtr;   // If the object is local then we can have the address
   objMetaClass *Class;   // Direct pointer to the object's class
   READ_TABLE   *ReadTable;
   WRITE_TABLE  *WriteTable;
   OBJECTID UID;          // If the object is referenced externally, access is managed by ID
   uint16_t AccessCount;  // Controlled by access_object() and release_object()
   bool  Detached;        // True if the object is an external reference or is not to be garbage collected
   bool  Locked;          // Can be true ONLY if a lock has been acquired from AccessObject()
};

struct lua_ref {
   CPTR Address;
   int Ref;
};

OBJECTPTR access_object(struct object *);
std::vector<lua_ref> * alloc_references(void);
void load_include_for_class(lua_State *, objMetaClass *);
ERR build_args(lua_State *, const struct FunctionField *, int, int8_t *, int *);
const char * code_reader(lua_State *, void *, size_t *);
[[maybe_unused]] int code_writer_id(lua_State *, CPTR, size_t, void *);
[[maybe_unused]] int code_writer(lua_State *, CPTR, size_t, void *);
ERR create_fluid(void);
void get_line(objScript *, int, STRING, int);
APTR get_meta(lua_State *Lua, int Arg, CSTRING);
void hook_debug(lua_State *, lua_Debug *) __attribute__ ((unused));
ERR load_include(objScript *, CSTRING);
int MAKESTRUCT(lua_State *);
[[maybe_unused]] void make_any_array(lua_State *, int, std::string_view, int, CPTR);
[[maybe_unused]] void make_array(lua_State *, AET, int = 0, CPTR = nullptr, std::string_view = {});
[[maybe_unused]] ERR make_struct(objScript *, std::string_view, CSTRING);
ERR named_struct_to_table(lua_State *, std::string_view, CPTR);
void make_struct_ptr_array(lua_State *, std::string_view, int, CPTR *);
void make_struct_serial_array(lua_State *, std::string_view, int, CPTR);
void notify_action(OBJECTPTR, ACTIONID, ERR, APTR);
void process_error(objScript *, CSTRING);
struct object * push_object(lua_State *, OBJECTPTR Object);
ERR push_object_id(lua_State *, OBJECTID ObjectID);
struct fstruct * push_struct(objScript *, APTR, std::string_view, bool, bool);
struct fstruct * push_struct_def(lua_State *, APTR, struct struct_record &, bool);
extern void register_io_class(lua_State *);
extern void register_input_class(lua_State *);
extern void register_object_class(lua_State *);
extern void register_module_class(lua_State *);
extern void register_number_class(lua_State *);
extern void register_processing_class(lua_State *);
extern void register_regex_class(lua_State *);
extern void register_struct_class(lua_State *);
extern void register_thread_class(lua_State *);
//static void register_widget_class(lua_State *);
void release_object(struct object *);
void new_module(lua_State *, objModule *);
ERR struct_to_table(lua_State *, std::vector<lua_ref> &, struct struct_record &, CPTR);
ERR table_to_struct(lua_State *, std::string_view, APTR *);
ERR keyvalue_to_table(lua_State *, const KEYVALUE *);
ERR msg_thread_script_callback(APTR Custom, int MsgID, int MsgType, APTR Message, int MsgSize);

int fcmd_arg(lua_State *);
int fcmd_catch(lua_State *);
int fcmd_catch_handler(lua_State *);
int fcmd_try(lua_State *);
int fcmd_check(lua_State *);
int fcmd_raise(lua_State *);
int fcmd_get_execution_state(lua_State *);
int fcmd_msg(lua_State *);
int fcmd_print(lua_State *);
int fcmd_include(lua_State *);
int fcmd_loadfile(lua_State *);
int fcmd_exec(lua_State *);
int fcmd_require(lua_State *);
int fcmd_subscribe_event(lua_State *);
int fcmd_unsubscribe_event(lua_State *);

#ifdef __arm__
extern void armExecFunction(APTR, APTR, int);
#else
extern void x64ExecFunction(APTR, int, int64_t *, int);
#endif
