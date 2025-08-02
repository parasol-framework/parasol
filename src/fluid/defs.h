
#define LUA_COMPILED "-- $FLUID:compiled"
#define VER_FLUID 1.0
#define SIZE_READ 1024

#include <list>
#include <unordered_set>
#include <set>
#include <array>
#include <parasol/strings.hpp>
#include <thread>

using namespace pf;

#define ALIGN64(a) (((a) + 7) & (~7))
#define ALIGN32(a) (((a) + 3) & (~3))

//********************************************************************************************************************

struct CaseInsensitiveMap {
   bool operator() (const std::string &lhs, const std::string &rhs) const {
      return ::strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
   }
};

extern std::map<std::string, ACTIONID, CaseInsensitiveMap> glActionLookup;
extern struct ActionTable *glActions;
extern OBJECTPTR modDisplay; // Required by fluid_input.c
extern OBJECTPTR modFluid;
extern OBJECTPTR clFluid;
extern std::unordered_map<std::string, ULONG> glStructSizes;

//********************************************************************************************************************
// Standard hash computation, but stops when it encounters a character outside of A-Za-z0-9 range
// Note that struct name hashes are case sensitive.

inline ULONG STRUCTHASH(CSTRING String)
{
   ULONG hash = 5381;
   UBYTE c;
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
   LONG     Function;          // Index of function to call back.
   LONG     Reference;         // A custom reference to pass to the callback (optional)
   ACTIONID ActionID;          // Action being monitored.
   OBJECTID ObjectID;          // Object being monitored

   actionmonitor() {}

   ~actionmonitor() {
      if (ObjectID) {
         pf::Log log(__FUNCTION__);
         log.trace("Unsubscribe action %s from object #%d", glActions[LONG(ActionID)].Name, ObjectID);
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
   LONG    Function;     // Lua function index
   EVENTID EventID;      // Event message ID
   APTR    EventHandle;

   eventsub(LONG pFunction, EVENTID pEventID, APTR pEventHandle) :
      Function(pFunction), EventID(pEventID), EventHandle(pEventHandle) { }

   ~eventsub() {
      if (EventHandle) UnsubscribeEvent(EventHandle);
   }

   eventsub(eventsub &&move) noexcept :
      Function(move.Function), EventID(move.EventID), EventHandle(move.EventHandle) {
      move.EventHandle = NULL;
   }

   eventsub& operator=(eventsub &&move) = default;
};

//********************************************************************************************************************

struct datarequest {
   OBJECTID SourceID;
   LONG Callback;
   LARGE TimeCreated;

   datarequest(OBJECTID pSourceID, LONG pCallback) : SourceID(pSourceID), Callback(pCallback) {
      TimeCreated = PreciseTime();
   }
};

//********************************************************************************************************************

struct struct_field {
   std::string Name;       // Field name
   std::string StructRef;  // Named reference to other structure
   UWORD Offset    = 0;    // Offset to the field value.
   LONG  Type      = 0;    // FD flags
   LONG  ArraySize = 0;    // Set if the field is an array

   ULONG nameHash() {
      if (!NameHash) NameHash = strihash(Name);
      return NameHash;
   }

   private:
   ULONG NameHash = 0;     // Lowercase hash of the field name
};

struct struct_record {
   std::string Name;
   std::vector<struct_field> Fields;
   LONG Size = 0; // Total byte size of the structure
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
      ULONG hash = 5381;
      for (auto c : k.name) {
         if ((c >= 'A') and (c <= 'Z'));
         else if ((c >= 'a') and (c <= 'z'));
         else if ((c >= '0') and (c <= '9'));
         else break;
         hash = ((hash<<5) + hash) + UBYTE(c);
      }
      return hash;
   }

   std::size_t operator()(const std::string_view k) const {
      ULONG hash = 5381;
      for (auto c : k) {
         if ((c >= 'A') and (c <= 'Z'));
         else if ((c >= 'a') and (c <= 'z'));
         else if ((c >= '0') and (c <= '9'));
         else break;
         hash = ((hash<<5) + hash) + UBYTE(c);
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
   std::unordered_map<struct_name, struct_record, struct_hash> Structs;
   std::unordered_map<OBJECTID, LONG> StateMap;
   std::set<std::string, CaseInsensitiveMap> Includes; // Stores the status of loaded include files.
   pf::vector<std::string> Procedures;
   std::vector<std::unique_ptr<std::jthread>> Threads; // Simple mechanism for auto-joining all the threads on object destruction
   APTR   FocusEventHandle;
   struct finput *InputList;         // Managed by the input interface
   DateTime CacheDate;
   ERR  CaughtError;               // Set to -1 to enable catching of ERR results.
   PERMIT CachePermissions;
   LONG   LoadedSize;
   UBYTE  Recurse;
   UBYTE  SaveCompiled;
   UWORD  Catch;                     // Operating within a catch() block if > 0
   UWORD  RequireCounter;
   LONG   ErrorLine;                 // Line at which the last error was thrown.
};

struct array {
   struct struct_record *StructDef; // Set if the array represents a known struct.
   LONG Total;        // Total number of elements
   LONG Type;         // FD_BYTE, FD_INT etc...
   LONG TypeSize;     // Byte-size of the type, e.g. LARGE == 8 bytes
   LONG ArraySize;    // Size of the array *in bytes*
   LONG AlignedSize;  // For struct alignment
   bool Allocated;
   bool ReadOnly;
   union {
      DOUBLE *ptrDouble;
      LARGE  *ptrLarge;
      APTR   *ptrPointer;
      STRING *ptrString;
      FLOAT  *ptrFloat;
      LONG   *ptrLong;
      WORD   *ptrWord;
      UBYTE  *ptrByte;
      APTR   ptrVoid;
   };
};

// This structure is created & managed through the 'struct' interface

struct fstruct {
   APTR Data;          // Pointer to the structure data
   LONG StructSize;    // Size of the structure
   LONG AlignedSize;   // 64-bit alignment size of the structure.
   struct struct_record *Def; // The structure definition
   bool Deallocate;    // Deallocate the struct when Lua collects this resource.
};

struct fprocessing {
   DOUBLE Timeout;
   std::list<ObjectSignal> *Signals;
};

struct metafield {
   ULONG ID;
   LONG GetFunction;
   LONG SetFunction;
};

#define FIM_KEYBOARD 1
#define FIM_DEVICE 2

struct finput {
   objScript *Script;
   struct finput *Next;
   APTR  KeyEvent;
   OBJECTID SurfaceID;
   LONG  InputHandle;
   LONG  Callback;
   LONG  InputValue;
   JTYPE Mask;
   BYTE  Mode;
};

enum { NUM_DOUBLE=1, NUM_FLOAT, NUM_LARGE, NUM_LONG, NUM_WORD, NUM_BYTE };

struct fnumber {
   LONG Type;      // Expressed as an FD_ flag.
   union {
      DOUBLE f64;
      FLOAT  f32;
      LARGE  i64;
      LONG   i32;
      WORD   i16;
      BYTE   i8;
   };
};

struct module {
   struct Function *Functions;
   OBJECTPTR Module;
};

inline ULONG simple_hash(CSTRING String, ULONG Hash = 5381) {
   while (auto c = *String++) Hash = ((Hash<<5) + Hash) + c;
   return Hash;
}

//********************************************************************************************************************
// obj_read is used to build efficient customised jump tables for object calls.

struct obj_read {
   typedef int JUMP(lua_State *, const struct obj_read &, struct object *);

   ULONG Hash;
   JUMP *Call;
   APTR Data;

   auto operator<=>(const obj_read &Other) const {
       if (Hash < Other.Hash) return -1;
       if (Hash > Other.Hash) return 1;
       return 0;
   }

   obj_read(ULONG pHash, JUMP pJump, APTR pData) : Hash(pHash), Call(pJump), Data(pData) { }
   obj_read(ULONG pHash, JUMP pJump) : Hash(pHash), Call(pJump) { }
   obj_read(ULONG pHash) : Hash(pHash) { }
};

inline auto read_hash = [](const obj_read &a, const obj_read &b) { return a.Hash < b.Hash; };

typedef std::set<obj_read, decltype(read_hash)> READ_TABLE;

//********************************************************************************************************************

struct obj_write {
   typedef ERR JUMP(lua_State *, OBJECTPTR, struct Field *, LONG);

   ULONG Hash;
   JUMP *Call;
   struct Field *Field;

   auto operator<=>(const obj_write &Other) const {
       if (Hash < Other.Hash) return -1;
       if (Hash > Other.Hash) return 1;
       return 0;
   }

   obj_write(ULONG pHash, JUMP pJump, struct Field *pField) : Hash(pHash), Call(pJump), Field(pField) { }
   obj_write(ULONG pHash, JUMP pJump) : Hash(pHash), Call(pJump) { }
   obj_write(ULONG pHash) : Hash(pHash) { }
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
   UWORD AccessCount;     // Controlled by access_object() and release_object()
   bool  Detached;        // True if the object is an external reference or is not to be garbage collected
   bool  Locked;          // Can be true ONLY if a lock has been acquired from AccessObject()
   bool  DelayCall;       // If true, the next action/method call is to be delayed.
};

struct lua_ref {
   CPTR Address;
   LONG Ref;
};

OBJECTPTR access_object(struct object *);
std::vector<lua_ref> * alloc_references(void);
void auto_load_include(lua_State *, objMetaClass *);
ERR build_args(lua_State *, const struct FunctionField *, LONG, BYTE *, LONG *);
const char * code_reader(lua_State *, void *, size_t *);
int code_writer_id(lua_State *, CPTR, size_t, void *) __attribute__((unused));
int code_writer(lua_State *, CPTR, size_t, void *) __attribute__((unused));
ERR create_fluid(void);
void get_line(objScript *, LONG, STRING, LONG);
APTR get_meta(lua_State *Lua, LONG Arg, CSTRING);
void hook_debug(lua_State *, lua_Debug *) __attribute__ ((unused));
ERR load_include(objScript *, CSTRING);
int MAKESTRUCT(lua_State *);
void make_any_table(lua_State *, LONG, CSTRING, LONG, CPTR ) __attribute__((unused));
void make_array(lua_State *, LONG, CSTRING, APTR *, LONG, bool);
void make_table(lua_State *, LONG, LONG, CPTR ) __attribute__((unused));
ERR make_struct(lua_State *, std::string_view, CSTRING) __attribute__((unused));
ERR named_struct_to_table(lua_State *, std::string_view, CPTR);
void make_struct_ptr_table(lua_State *, CSTRING, LONG, CPTR *);
void make_struct_serial_table(lua_State *, CSTRING, LONG, CPTR);
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
extern void register_struct_class(lua_State *);
extern void register_thread_class(lua_State *);
//static void register_widget_class(lua_State *);
void release_object(struct object *);
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
extern void armExecFunction(APTR, APTR, LONG);
#elif _LP64
extern void x64ExecFunction(APTR, LONG, LARGE *, LONG);
#else
extern void x86ExecFunction(APTR, APTR, LONG);
#endif
