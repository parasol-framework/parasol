
#define LUA_COMPILED "-- $FLUID:compiled"
#define VER_FLUID 1.0
#define SIZE_READ 1024

#include <list>

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

struct prvFluid {
   lua_State *Lua;                   // Lua instance
   struct actionmonitor *ActionList; // Action subscriptions managed by subscribe()
   struct eventsub *EventList;       // Event subscriptions managed by subscribeEvent()
   struct finput *InputList;         // Managed by the input interface
   struct datarequest *Requests;     // For drag and drop requests
   KeyStore *Structs;
   KeyStore *Includes;               // Stores the status of loaded include files.
   APTR   FocusEventHandle;
   std::unordered_map<OBJECTID, LONG> *StateMap;
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
   struct structentry *StructDef; // Set if the array represents a known struct.
   LONG Total;        // Total number of elements
   LONG Type;         // FD_BYTE, FD_LONG etc...
   LONG TypeSize;     // Byte-size of the type, e.g. LARGE == 8 bytes
   LONG ArraySize;    // Size of the array *in bytes*
   LONG AlignedSize;  // For struct alignment
   UBYTE Allocated:1;
   UBYTE ReadOnly:1;
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

// These structures are allocated by MAKESTRUCT
// structentry is for structure definitions that define field types and names

struct structentry {
   LONG Total;     // Total number of fields in the structure
   LONG Size;      // Total byte size of the structure
   ULONG NameHash; // Name of the structure expressed as a lowercase hash.
   // Description of structure fields then follows.
};

struct structdef_field {
   UWORD Length;        // Length of this structdef_field entry.
   UWORD Offset;        // Offset to the field value.
   ULONG NameHash;      // Lowercase hash of the field name
   LONG  StructOffset;  // Index to the structure name, if this is a struct reference.
   LONG  Type;          // FD flags
   LONG  ArraySize;     // Set if the field is an array
   // Field name and optional structure name follow
};

// This structure is created & managed through the 'struct' interface

struct fstruct {
   APTR Data;          // Pointer to the structure data
   LONG StructSize;    // Size of the structure
   LONG AlignedSize;   // 64-bit alignment size of the structure.
   struct structentry *Def; // The structure definition
   UBYTE Deallocate:1;  // Deallocate the struct when Lua collects this resource.
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

struct fwidget {
   objMetaClass *Class;
   struct metafield *Fields;
   lua_State *Lua;
   LONG InputMask;
   struct {
      LONG Activate;
      LONG Deactivate;
      LONG Disable;
      LONG Draw;
      LONG Enable;
      LONG Free;
      LONG Hide;
      LONG Input;
      LONG Keyboard;
      LONG Focus;
      LONG New;
      LONG MoveToBack;
      LONG MoveToFront;
      LONG Redimension;
      LONG Resize;
      LONG Show;
   } AC;
   WORD TotalFields;
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

struct object {
   OBJECTPTR prvObject;       // If the object is private we can have the address
   objMetaClass *Class;       // Direct pointer to the module's class
   OBJECTID  ObjectID;        // If the object is referenced externally, access is managed by ID
   CLASSID   ClassID;         // Class identifier
   UBYTE     Detached:1;      // TRUE if the object is an external reference or is not to be garbage collected
   UBYTE     Locked:1;        // Can be TRUE only if a lock has been acquired from AccessObjectID()
   UBYTE     DelayCall:1;     // If TRUE, the next action/method call is to be delayed.
   ULONG     AccessCount;     // Controlled by access_object() and release_object()
};

struct references {
   LONG Index;
   struct {
      CPTR Address;
      LONG Ref;
   } List[16384];
};

extern struct KeyStore *glActionLookup;
extern struct ActionTable *glActions;
extern OBJECTPTR modDisplay; // Required by fluid_input.c
extern OBJECTPTR modFluid;
extern struct DisplayBase *DisplayBase;
extern OBJECTPTR clFluid;

OBJECTPTR access_object(struct object *);
struct references * alloc_references(void);
void auto_load_include(lua_State *Lua, objMetaClass *MetaClass);
ERROR build_args(lua_State *, const struct FunctionField *, LONG, BYTE *, LONG *);
void clear_subscriptions(objScript *);
const char * code_reader(lua_State *, void *, size_t *);
int code_writer_id(lua_State *, CPTR, size_t, void *) __attribute__((unused));
int code_writer(lua_State *, CPTR, size_t, void *) __attribute__((unused));
ERROR create_fluid(void);
void free_references(lua_State *, struct references *);
void get_line(objScript *, LONG, STRING, LONG);
APTR get_meta(lua_State *Lua, LONG Arg, CSTRING);
void hook_debug(lua_State *, lua_Debug *) __attribute__ ((unused));
ERROR load_include(objScript *, CSTRING);
int MAKESTRUCT(lua_State *);
void make_any_table(lua_State *, LONG Type, CSTRING, LONG Elements, CPTR ) __attribute__((unused));
void make_array(lua_State *Lua, LONG FieldType, CSTRING StructName, APTR *List, LONG Total, BYTE Cache);
void make_table(lua_State *, LONG Type, LONG Elements, CPTR ) __attribute__((unused));
int make_struct(lua_State *, CSTRING, CSTRING) __attribute__((unused));
ERROR named_struct_to_table(lua_State *, CSTRING, CPTR);
void make_struct_ptr_table(lua_State *, CSTRING, LONG, CPTR *);
void make_struct_serial_table(lua_State *, CSTRING, LONG, CPTR);
CSTRING next_line(CSTRING String);
void notify_action(OBJECTPTR, ACTIONID, ERROR, APTR);
void process_error(objScript *Self, CSTRING Procedure);
struct object * push_object(lua_State *, OBJECTPTR Object);
ERROR push_object_id(lua_State *, OBJECTID ObjectID);
struct fstruct * push_struct(objScript *, APTR, CSTRING, BYTE, BYTE);
struct fstruct * push_struct_def(lua_State *, APTR, struct structentry *, BYTE);
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
ERROR struct_to_table(lua_State *, struct references *, struct structentry *, CPTR);

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

//****************************************************************************
// Standard hash computation, but stops when it encounters a character outside of A-Za-z0-9 range
// Note that struct name hashes are case sensitive.

INLINE ULONG STRUCTHASH(CSTRING String)
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
