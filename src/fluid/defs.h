
#define LUA_COMPILED "-- $FLUID:compiled"
constexpr int SIZE_READ = 1024;

#include <list>
#include <unordered_set>
#include <set>
#include <array>
#include <regex>
#include <parasol/strings.hpp>
#include <thread>
#include <string_view>

using namespace pf;

template <class T> T ALIGN64(T a) { return (((a) + 7) & (~7)); }
template <class T> T ALIGN32(T a) { return (((a) + 3) & (~3)); }

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

extern ankerl::unordered_dense::map<std::string, ACTIONID, CaseInsensitiveHash, CaseInsensitiveEqual> glActionLookup;
extern struct ActionTable *glActions;
extern OBJECTPTR modDisplay; // Required by fluid_input.c
extern OBJECTPTR modFluid;
extern OBJECTPTR clFluid;
extern ankerl::unordered_dense::map<std::string, uint32_t> glStructSizes;

//********************************************************************************************************************
// Helper: build a std::string_view from a Lua string argument.
// Raises a Lua error if the argument at 'idx' is not a string (delegates to luaL_checklstring).

static inline std::string_view luaL_checkstringview(lua_State *L, int idx) noexcept
{
   size_t len = 0;
   if (const char *s = luaL_checklstring(L, idx, &len)) {
      return std::string_view{s, len};
   }
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

//********************************************************************************************************************

struct struct_field {
   std::string Name;      // Field name
   std::string StructRef; // Named reference to other structure
   uint16_t Offset = 0;   // Offset to the field value.
   int  Type      = 0;    // FD flags
   int  ArraySize = 0;    // Set if the field is an array

   uint32_t nameHash() {
      if (!NameHash) NameHash = strihash(Name);
      return NameHash;
   }

   private:
   uint32_t NameHash = 0;     // Lowercase hash of the field name
};

struct struct_record {
   std::string Name;
   std::vector<struct_field> Fields;
   int Size = 0; // Total byte size of the structure
   struct_record(std::string_view pName) : Name(pName) { }
   struct_record() = default;
};

//********************************************************************************************************************
// Structure names have their own handler due to the use of colons in struct references, i.e. "OfficialStruct:SomeName"

struct struct_name {
   std::string name;
   struct_name(const std::string_view pName) {
      auto colon = pName.find(':');

      if (colon IS std::string::npos) name = pName;
      else name = pName.substr(0, colon);
   }

   bool operator==(const std::string_view &other) const {
      return (name == other);
   }

   bool operator==(const struct_name &other) const {
      return (name == other.name);
   }
};

struct struct_hash {
   std::size_t operator()(const struct_name &k) const {
      uint32_t hash = 5381;
      for (auto c : k.name) {
         if ((c >= 'A') and (c <= 'Z'));
         else if ((c >= 'a') and (c <= 'z'));
         else if ((c >= '0') and (c <= '9'));
         else break;
         hash = ((hash<<5) + hash) + uint8_t(c);
      }
      return hash;
   }

   std::size_t operator()(const std::string_view k) const {
      uint32_t hash = 5381;
      for (auto c : k) {
         if ((c >= 'A') and (c <= 'Z'));
         else if ((c >= 'a') and (c <= 'z'));
         else if ((c >= '0') and (c <= '9'));
         else break;
         hash = ((hash<<5) + hash) + uint8_t(c);
      }
      return hash;
   }
};

//********************************************************************************************************************

struct prvFluid {
   lua_State *Lua;                        // Lua instance
   std::vector<actionmonitor> ActionList; // Action subscriptions managed by subscribe()
   std::vector<eventsub> EventList;       // Event subscriptions managed by subscribeEvent()
   std::vector<datarequest> Requests;     // For drag and drop requests
   ankerl::unordered_dense::map<struct_name, struct_record, struct_hash> Structs;
   ankerl::unordered_dense::map<OBJECTID, int> StateMap;
   std::set<std::string, CaseInsensitiveMap> Includes; // Stores the status of loaded include files.
   pf::vector<std::string> Procedures;
   std::vector<std::unique_ptr<std::jthread>> Threads; // Simple mechanism for auto-joining all the threads on object destruction
   APTR     FocusEventHandle;
   struct finput *InputList;           // Managed by the input interface
   DateTime CacheDate;
   ERR      CaughtError;               // Set to -1 to enable catching of ERR results.
   PERMIT   CachePermissions;
   int      LoadedSize;
   uint8_t  Recurse;
   uint8_t  SaveCompiled;
   uint16_t Catch;                     // Operating within a catch() block if > 0
   uint16_t RequireCounter;
   int      ErrorLine;                 // Line at which the last error was thrown.
};

struct array {
   struct struct_record *StructDef; // Set if the array represents a known struct.
   int Total;        // Total number of elements
   int Type;         // FD_BYTE, FD_INT etc...
   int TypeSize;     // Byte-size of the type, e.g. int64_t == 8 bytes
   int ArraySize;    // Size of the array *in bytes*
   int AlignedSize;  // For struct alignment
   bool Allocated;
   bool ReadOnly;
   union { // TODO: Use std::variant
      double  *ptrDouble;
      int64_t *ptrLarge;
      APTR    *ptrPointer;
      STRING  *ptrString;
      float   *ptrFloat;
      int     *ptrLong;
      int16_t *ptrWord;
      uint8_t *ptrByte;
      APTR    ptrVoid;
   };
};

// This structure is created & managed through the 'struct' interface

struct fstruct {
   APTR Data;          // Pointer to the structure data
   int StructSize;    // Size of the structure
   int AlignedSize;   // 64-bit alignment size of the structure.
   struct struct_record *Def; // The structure definition
   bool Deallocate;    // Deallocate the struct when Lua collects this resource.
};

struct fprocessing {
   double Timeout;
   std::list<ObjectSignal> *Signals;
};

class fregex {
public:
   std::regex *regex_obj = nullptr;  // Compiled regex object
   std::string pattern;     // Original pattern string
   std::string error_msg;   // Error message if compilation failed
   int flags;               // Compilation flags
   fregex(std::string_view Pattern, int Flags) : pattern(Pattern), flags(Flags) { }
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
   APTR  KeyEvent;
   OBJECTID SurfaceID;
   int   InputHandle;
   int   Callback;
   int   InputValue;
   JTYPE Mask;
   BYTE  Mode;
};

enum { NUM_DOUBLE=1, NUM_FLOAT, NUM_LARGE, NUM_LONG, NUM_WORD, NUM_BYTE };

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
   READ_TABLE *ReadTable;
   WRITE_TABLE *WriteTable;
   OBJECTID UID;          // If the object is referenced externally, access is managed by ID
   uint16_t AccessCount;     // Controlled by access_object() and release_object()
   bool  Detached;        // True if the object is an external reference or is not to be garbage collected
   bool  Locked;          // Can be true ONLY if a lock has been acquired from AccessObject()
};

struct lua_ref {
   CPTR Address;
   int Ref;
};

OBJECTPTR access_object(struct object *);
std::vector<lua_ref> * alloc_references(void);
void auto_load_include(lua_State *, objMetaClass *);
ERR build_args(lua_State *, const struct FunctionField *, int, BYTE *, int *);
const char * code_reader(lua_State *, void *, size_t *);
int code_writer_id(lua_State *, CPTR, size_t, void *) __attribute__((unused));
int code_writer(lua_State *, CPTR, size_t, void *) __attribute__((unused));
ERR create_fluid(void);
void get_line(objScript *, int, STRING, int);
APTR get_meta(lua_State *Lua, int Arg, CSTRING);
void hook_debug(lua_State *, lua_Debug *) __attribute__ ((unused));
ERR load_include(objScript *, CSTRING);
int MAKESTRUCT(lua_State *);
void make_any_table(lua_State *, int, CSTRING, int, CPTR ) __attribute__((unused));
void make_array(lua_State *, int, CSTRING, APTR *, int, bool);
void make_table(lua_State *, int, int, CPTR ) __attribute__((unused));
ERR make_struct(lua_State *, std::string_view, CSTRING) __attribute__((unused));
ERR named_struct_to_table(lua_State *, std::string_view, CPTR);
void make_struct_ptr_table(lua_State *, CSTRING, int, CPTR *);
void make_struct_serial_table(lua_State *, CSTRING, int, CPTR);
CSTRING next_line(CSTRING String);
void notify_action(OBJECTPTR, ACTIONID, ERR, APTR);
void process_error(objScript *, CSTRING);
struct object * push_object(lua_State *, OBJECTPTR Object);
ERR push_object_id(lua_State *, OBJECTID ObjectID);
struct fstruct * push_struct(objScript *, APTR, std::string_view, bool, bool);
struct fstruct * push_struct_def(lua_State *, APTR, struct struct_record &, bool);
extern void register_array_class(lua_State *);
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
int fcmd_check(lua_State *);
int fcmd_raise(lua_State *);
int fcmd_get_execution_state(lua_State *);
int fcmd_msg(lua_State *);
int fcmd_print(lua_State *);
int fcmd_include(lua_State *);
int fcmd_loadfile(lua_State *);
int fcmd_exec(lua_State *);
int fcmd_nz(lua_State *);
int fcmd_require(lua_State *);
int fcmd_subscribe_event(lua_State *);
int fcmd_unsubscribe_event(lua_State *);

#ifdef __arm__
extern void armExecFunction(APTR, APTR, int);
#elif _LP64
extern void x64ExecFunction(APTR, int, int64_t *, int);
#else
extern void x86ExecFunction(APTR, APTR, int);
#endif
