
#define LUA_COMPILED "-- $FLUID:compiled"
#define VER_FLUID 1.0
#define SIZE_READ 1024

#include <list>
#include <unordered_set>
#include <set>
#include <array>

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

struct CaseInsensitiveMap {
   bool operator() (const std::string &lhs, const std::string &rhs) const {
      return ::strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
   }
};

//********************************************************************************************************************

struct code_reader_handle {
   objFile *File;
   APTR Buffer;
};

struct actionmonitor {
   struct actionmonitor *Prev;
   struct actionmonitor *Next;
   struct object *Object;      // Fluid.obj original passed in for the subscription.
   const struct FunctionField *Args; // The args of the action/method are stored here so that we can build the arg value table later.
   LONG Function;              // Index of function to call back.
   LONG ActionID;              // Action being monitored.
   LONG Reference;             // A custom reference to pass to the callback (optional)
   OBJECTID ObjectID;          // Object being monitored
};

struct eventsub {
   struct eventsub *Prev;
   struct eventsub *Next;
   LONG Function;     // Lua function index
   EVENTID EventID;   // Event message ID
   APTR EventHandle;
};

//********************************************************************************************************************

struct struct_field {
   std::string Name;       // Field name
   std::string StructRef;  // Named reference to other structure
   UWORD Offset    = 0;    // Offset to the field value.
   LONG  Type      = 0;    // FD flags
   LONG  ArraySize = 0;    // Set if the field is an array

   ULONG nameHash() {
      if (!NameHash) NameHash = StrHash(Name);
      return NameHash;
   }

   private:
   ULONG NameHash = 0;     // Lowercase hash of the field name
};

struct struct_record {
   std::string Name;
   std::vector<struct_field> Fields;
   LONG Size = 0; // Total byte size of the structure
   struct_record(const std::string pName) : Name(pName) { }
   struct_record() = default;
};

//********************************************************************************************************************
// Structure names have their own handler due to the use of colons in struct references, i.e. "OfficialStruct:SomeName"

struct struct_name {
   std::string name;
   struct_name(const std::string pName) {
      auto colon = pName.find(':');

      if (colon IS std::string::npos) name = pName;
      else name = pName.substr(0, colon);
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
};

//********************************************************************************************************************

struct prvFluid {
   lua_State *Lua;                   // Lua instance
   struct actionmonitor *ActionList; // Action subscriptions managed by subscribe()
   struct eventsub *EventList;       // Event subscriptions managed by subscribeEvent()
   struct finput *InputList;         // Managed by the input interface
   struct datarequest *Requests;     // For drag and drop requests
   std::unordered_map<struct_name, struct_record, struct_hash> Structs;
   std::set<std::string, CaseInsensitiveMap> Includes; // Stores the status of loaded include files.
   APTR   FocusEventHandle;
   std::unordered_map<OBJECTID, LONG> StateMap;
   DateTime CacheDate;
   ERROR  CaughtError;               // Set to -1 to enable catching of ERROR results.
   LONG   CachePermissions;
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
   LONG Type;         // FD_BYTE, FD_LONG etc...
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

// This structure is created & managed through the 'memory' interface
// DEPRECATED

struct memory {
   union {
      APTR Memory;
      APTR Address;
   };
   MEMORYID MemoryID;
   UBYTE    Linked:1;      // TRUE if the memory is an external reference
   UBYTE    ElementSize;
   LONG     ArrayType;
   LONG     IndexType;
   UBYTE    IndexSize;    // Byte size of each index element (e.g. LONG = 4)
   LONG     MemorySize;   // Size of the allocated memory
   LONG     MemFlags;
   ULONG    AccessCount;
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
   APTR KeyEvent;
   OBJECTID SurfaceID;
   LONG InputHandle;
   LONG Callback;
   LONG InputValue;
   LONG Mask;
   BYTE Mode;
};

struct datarequest {
   struct datarequest *Next;
   OBJECTID SourceID;
   LONG Callback;
   LARGE TimeCreated;
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

//********************************************************************************************************************
// object_jump is used to build efficient customised jump tables for object calls.

typedef int JUMP(lua_State *, const struct object_jump &);

struct object_jump {
   ULONG Hash;
   int (*Call)(lua_State *, const object_jump &);
   APTR Data;

   static ULONG hash(CSTRING String, ULONG Hash = 5381) {
      while (auto c = *String++) Hash = ((Hash<<5) + Hash) + c;
      return Hash;
   }

   auto operator<=>(const object_jump &Other) const {
       if (Hash < Other.Hash) return -1;
       if (Hash > Other.Hash) return 1;
       return 0;
   }

   object_jump(ULONG pHash, const JUMP pJump, APTR pData) : Hash(pHash), Call(pJump), Data(pData) { }
   object_jump(ULONG pHash, const JUMP pJump) : Hash(pHash), Call(pJump) { }
   object_jump(ULONG pHash) : Hash(pHash) { }
};

inline auto object_hash = [](const object_jump &a, const object_jump &b) { return a.Hash < b.Hash; };

typedef std::set<object_jump, decltype(object_hash)> JUMP_TABLE;

//********************************************************************************************************************

struct object {
   OBJECTPTR ObjectPtr;   // If the object is local then we can have the address
   objMetaClass *Class;   // Direct pointer to the object's class
   JUMP_TABLE *Jump;
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

extern std::map<std::string, ACTIONID, CaseInsensitiveMap> glActionLookup;
extern struct ActionTable *glActions;
extern OBJECTPTR modDisplay; // Required by fluid_input.c
extern OBJECTPTR modFluid;
extern struct DisplayBase *DisplayBase;
extern OBJECTPTR clFluid;
extern std::unordered_map<std::string, ULONG> glStructSizes;

OBJECTPTR access_object(struct object *);
std::vector<lua_ref> * alloc_references(void);
void auto_load_include(lua_State *, objMetaClass *);
ERROR build_args(lua_State *, const struct FunctionField *, LONG, BYTE *, LONG *);
void clear_subscriptions(objScript *);
const char * code_reader(lua_State *, void *, size_t *);
int code_writer_id(lua_State *, CPTR, size_t, void *) __attribute__((unused));
int code_writer(lua_State *, CPTR, size_t, void *) __attribute__((unused));
ERROR create_fluid(void);
void get_line(objScript *, LONG, STRING, LONG);
APTR get_meta(lua_State *Lua, LONG Arg, CSTRING);
void hook_debug(lua_State *, lua_Debug *) __attribute__ ((unused));
ERROR load_include(objScript *, CSTRING);
int MAKESTRUCT(lua_State *);
void make_any_table(lua_State *, LONG, CSTRING, LONG, CPTR ) __attribute__((unused));
void make_array(lua_State *, LONG, CSTRING, APTR *, LONG, bool);
void make_table(lua_State *, LONG, LONG, CPTR ) __attribute__((unused));
int make_struct(lua_State *, const std::string &, CSTRING) __attribute__((unused));
ERROR named_struct_to_table(lua_State *, const std::string, CPTR);
void make_struct_ptr_table(lua_State *, CSTRING, LONG, CPTR *);
void make_struct_serial_table(lua_State *, CSTRING, LONG, CPTR);
CSTRING next_line(CSTRING String);
void notify_action(OBJECTPTR, ACTIONID, ERROR, APTR);
void process_error(objScript *, CSTRING);
struct object * push_object(lua_State *, OBJECTPTR Object);
ERROR push_object_id(lua_State *, OBJECTID ObjectID);
struct fstruct * push_struct(objScript *, APTR, const std::string &, bool, bool);
struct fstruct * push_struct_def(lua_State *, APTR, struct struct_record &, bool);
extern void register_array_class(lua_State *);
extern void register_input_class(lua_State *);
extern void register_object_class(lua_State *);
extern void register_module_class(lua_State *);
extern void register_number_class(lua_State *);
extern void register_processing_class(lua_State *);
extern void register_struct_class(lua_State *);
extern void register_thread_class(lua_State *);
//static void register_widget_class(lua_State *);
void release_object(struct object *);
ERROR struct_to_table(lua_State *, std::vector<lua_ref> &, struct struct_record &, CPTR);

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
