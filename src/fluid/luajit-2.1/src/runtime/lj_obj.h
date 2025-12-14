// LuaJIT VM tags, values and objects.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// Portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h

#pragma once

#include "lua.h"
#include "lj_def.h"
#include "lj_arch.h"

#ifndef IS
#define IS ==
#endif

// -- Forward declarations ------------------------------------------------
// These forward declarations establish the core type hierarchy of the VM.

// GC object types
union GCobj;
struct GCstr;
struct GCudata;
struct GCcdata;
struct GCcdataVar;
struct GCproto;
struct GCupval;
union GCfunc;
struct GCfuncC;
struct GCfuncL;
struct GCtab;
struct GChead;
struct Node;

// State objects
struct lua_State;
struct global_State;
struct GCState;
struct StrInternState;

// Value types
union TValue;
// Note: FrameLink is defined inline as it's a simple union used only in TValue
struct MRef;
struct GCRef;
struct SBuf;

// External classes
class ParserDiagnostics;
class objScript;

// -- Basic type aliases --------------------------------------------------

// Memory and GC object sizes.
using MSize = uint32_t;
using GCSize = uint64_t;  // Always 64-bit

// Memory reference - always 64-bit
struct MRef {
   uint64_t ptr64;   //  True 64 bit pointer.
};

// MRef accessor functions - 64-bit only
template<typename T>
[[nodiscard]] constexpr inline T* mref_get(MRef r) noexcept
{
   return (T*)(void*)r.ptr64;
}

[[nodiscard]] constexpr inline uint64_t mrefu(MRef r) noexcept
{
   return r.ptr64;
}

template<typename T>
constexpr inline void setmref_fn(MRef& r, T* p) noexcept
{
   r.ptr64 = uint64_t((void*)p);
}

// Overload for nullptr
constexpr inline void setmref_fn(MRef& r, std::nullptr_t) noexcept
{
   r.ptr64 = 0;
}

constexpr inline void setmrefu(MRef& r, uint64_t u) noexcept
{
   r.ptr64 = u;
}

constexpr inline void setmrefr(MRef& r, MRef v) noexcept
{
   r.ptr64 = v.ptr64;
}

// Compatibility macros - thin wrappers over template functions
#define mref(r, t)      mref_get<t>(r)
#define setmref(r, p)   setmref_fn((r), (p))

//********************************************************************************************************************
// GC object references

// GCobj reference - always 64-bit
struct GCRef {
   uint64_t gcptr64;   //  True 64 bit pointer.
};

// Common GC header for all collectable objects.
// Note: This macro must remain as it defines struct fields inline.

#define GCHeader   GCRef nextgc; uint8_t marked; uint8_t gct

// This occupies 6 bytes, so use the next 2 bytes for non-32 bit fields.

// GCRef accessor functions - 64-bit only
// These just cast - they don't dereference, so can be inline functions

[[nodiscard]] inline GCobj* gcref(GCRef r) noexcept
{
   return (GCobj*)r.gcptr64;
}

template<typename T>
[[nodiscard]] inline T* gcrefp_fn(GCRef r) noexcept
{
   return (T*)(void*)r.gcptr64;
}

[[nodiscard]] constexpr inline uint64_t gcrefu(GCRef r) noexcept
{
   return r.gcptr64;
}

[[nodiscard]] constexpr inline bool gcrefeq(GCRef r1, GCRef r2) noexcept
{
   return r1.gcptr64 IS r2.gcptr64;
}

template<typename T>
inline void setgcrefp_fn(GCRef& r, T* p) noexcept
{
   r.gcptr64 = uint64_t(p);
}

// Overload for integer types (used in some low-level operations)
// Note: On 64-bit, uintptr_t is typically uint64_t, so only need one

constexpr inline void setgcrefp_fn(GCRef& r, uint64_t v) noexcept
{
   r.gcptr64 = v;
}

constexpr inline void setgcrefp_fn(GCRef& r, int v) noexcept
{
   r.gcptr64 = uint64_t(v);
}

constexpr inline void setgcrefnull(GCRef& r) noexcept
{
   r.gcptr64 = 0;
}

constexpr inline void setgcrefr(GCRef& r, GCRef v) noexcept
{
   r.gcptr64 = v.gcptr64;
}

// These dereference GCobj->gch, so must remain macros until GCobj is defined
// They will be converted to inline functions after GCobj definition

#define setgcref(r, gc)      ((r).gcptr64 = (uint64_t)&(gc)->gch)
#define setgcreft(r, gc, it) ((r).gcptr64 = (uint64_t)&(gc)->gch | (((uint64_t)(it)) << 47))

// Compatibility macro for template function

#define gcrefp(r, t)    gcrefp_fn<t>(r)
#define setgcrefp(r, p) setgcrefp_fn((r), (p))

// gcnext dereferences GCobj, will be defined as inline function after GCobj

#define gcnext(gc)   (gcref((gc)->gch.nextgc))

/* IMPORTANT NOTE:
**
** All uses of the setgcref* macros MUST be accompanied with a write barrier.
**
** This is to ensure the integrity of the incremental GC. The invariant
** to preserve is that a black object never points to a white object.
** I.e. never store a white object into a field of a black object.
**
** It's ok to LEAVE OUT the write barrier ONLY in the following cases:
** - The source is not a GC object (NULL).
** - The target is a GC root. I.e. everything in global_State.
** - The target is a lua_State field (threads are never black).
** - The target is a stack slot, see setgcV et al.
** - The target is an open upvalue, i.e. pointing to a stack slot.
** - The target is a newly created object (i.e. marked white). But make
**   sure nothing invokes the GC inbetween.
** - The target and the source are the same object (self-reference).
** - The target already contains the object (e.g. moving elements around).
**
** The most common case is a store to a stack slot. All other cases where
** a barrier has been omitted are annotated with a NOBARRIER comment.
**
** The same logic applies for stores to table slots (array part or hash
** part). ALL uses of lj_tab_set* require a barrier for the stored value
** *and* the stored key, based on the above rules. In practice this means
** a barrier is needed if *either* of the key or value are a GC object.
**
** It's ok to LEAVE OUT the write barrier in the following special cases:
** - The stored value is nil. The key doesn't matter because it's either
**   not resurrected or lj_tab_newkey() will take care of the key barrier.
** - The key doesn't matter if the *previously* stored value is guaranteed
**   to be non-nil (because the key is kept alive in the table).
** - The key doesn't matter if it's guaranteed not to be part of the table,
**   since lj_tab_newkey() takes care of the key barrier. This applies
**   trivially to new tables, but watch out for resurrected keys. Storing
**   a nil value leaves the key in the table!
**
** In case of doubt use lj_gc_anybarriert() as it's rather cheap. It's used
** by the interpreter for all table stores.
**
** Note: In contrast to Lua's GC, LuaJIT's GC does *not* specially mark
** dead keys in tables. The reference is left in, but it's guaranteed to
** be never dereferenced as long as the value is nil. It's ok if the key is
** freed or if any object subsequently gets the same address.
**
** Not destroying dead keys helps to keep key hash slots stable. This avoids
** specialization back-off for HREFK when a value flips between nil and
** non-nil and the GC gets in the way. It also allows safely hoisting
** HREF/HREFK across GC steps. Dead keys are only removed if a table is
** resized (i.e. by NEWREF) and xREF must not be CSEd across a resize.
**
** The trade-off is that a write barrier for tables must take the key into
** account, too. Implicitly resurrecting the key by storing a non-nil value
** may invalidate the incremental GC invariant.
*/

// Common type definitions

// Types for handling bytecodes. Need this here, details in lj_bc.h.
using BCIns = uint32_t;  //  Bytecode instruction.
using BCPOS = uint32_t;  //  Bytecode position.
using BCREG = uint32_t;  //  Bytecode register.
using BCLine = int32_t;  //  Bytecode line number.

// Internal assembler functions. Never call these directly from C.
using ASMFunction = void(*)(void);

// Resizable string buffer. Need this here, details in lj_buf.h.
#define SBufHeader   char *w, *e, *b; MRef L
typedef struct SBuf {
   SBufHeader;
} SBuf;

// Tags and values

// Frame link.
typedef union {
   int32_t ftsz;      //  Frame type and size of previous frame.
   MRef pcr;      //  Or PC for Lua frames.
} FrameLink;

// Tagged value.
typedef LJ_ALIGN(8) union TValue {
   uint64_t u64;      //  64 bit pattern overlaps number.
   lua_Number n;      //  Number object overlaps split tag/value object.
   GCRef gcr;      //  GCobj reference with tag.
   int64_t it64;
   struct {
      LJ_ENDIAN_LOHI(
         int32_t i;   //  Integer value.
      , uint32_t it;   //  Internal object tag. Must overlap MSW of number.
         )
   };
   int64_t ftsz;      //  Frame type and size of previous frame, or PC.

   struct {
      LJ_ENDIAN_LOHI(
         uint32_t lo;   //  Lower 32 bits of number.
      , uint32_t hi;   //  Upper 32 bits of number.
         )
   } u32;
} TValue;

using cTValue = const TValue;

[[nodiscard]] inline TValue* tvref(MRef r) noexcept
{
   return mref(r, TValue);
}

// More external and GCobj tags for internal objects.
inline constexpr int LAST_TT     = LUA_TTHREAD;
inline constexpr int LUA_TPROTO  = LAST_TT + 1;
inline constexpr int LUA_TCDATA  = LAST_TT + 2;

/*
** Format for 64 bit GC references (LJ_GC64):
**
** The upper 13 bits must be 1 (0xfff8...) for a special NaN. The next
** 4 bits hold the internal tag. The lowest 47 bits either hold a pointer,
** a zero-extended 32 bit integer or all bits set to 1 for primitive types.
**
**                     ------MSW------.------LSW------
** primitive types    |1..1|itype|1..................1|
** GC objects         |1..1|itype|-------GCRef--------|
** lightuserdata      |1..1|itype|seg|------ofs-------|
** int (LJ_DUALNUM)   |1..1|itype|0..0|-----int-------|
** number              ------------double-------------
**
** ORDER LJ_T
** Primitive types nil/false/true must be first, lightuserdata next.
** GC objects are at the end, table/userdata must be lowest.
** Also check lj_ir.h for similar ordering constraints.
*/

// Internal type tags. ORDER LJ_T
inline constexpr uint32_t LJ_TNIL      = ~0u;
inline constexpr uint32_t LJ_TFALSE    = ~1u;
inline constexpr uint32_t LJ_TTRUE     = ~2u;
inline constexpr uint32_t LJ_TLIGHTUD  = ~3u;
inline constexpr uint32_t LJ_TSTR      = ~4u;
inline constexpr uint32_t LJ_TUPVAL    = ~5u;
inline constexpr uint32_t LJ_TTHREAD   = ~6u;
inline constexpr uint32_t LJ_TPROTO    = ~7u;
inline constexpr uint32_t LJ_TFUNC     = ~8u;
inline constexpr uint32_t LJ_TTRACE    = ~9u;
inline constexpr uint32_t LJ_TCDATA    = ~10u;
inline constexpr uint32_t LJ_TTAB      = ~11u;
inline constexpr uint32_t LJ_TUDATA    = ~12u;
inline constexpr uint32_t LJ_TARRAY    = ~13u;   // Native array type
// This is just the canonical number type used in some places.
inline constexpr uint32_t LJ_TNUMX     = ~14u;

// Integers have itype == LJ_TISNUM doubles have itype < LJ_TISNUM
// Always LJ_TNUMX for 64-bit GC64 mode
inline constexpr uint32_t LJ_TISNUM = LJ_TNUMX;
inline constexpr uint32_t LJ_TISTRUECOND = LJ_TFALSE;
inline constexpr uint32_t LJ_TISPRI      = LJ_TTRUE;
inline constexpr uint32_t LJ_TISGCV      = LJ_TSTR + 1;
inline constexpr uint32_t LJ_TISTABUD    = LJ_TTAB;

// Type marker for slot holding a traversal index. Must be lightuserdata.
inline constexpr uint32_t LJ_KEYINDEX = 0xfffe7fffu;

// GC64 pointer mask - always defined for 64-bit
inline constexpr uint64_t LJ_GCVMASK = (uint64_t(1) << 47) - 1;

// Lightuserdata is segmented to stay within 47 bits
inline constexpr int LJ_LIGHTUD_BITS_SEG = 8;
inline constexpr int LJ_LIGHTUD_BITS_LO  = 47 - LJ_LIGHTUD_BITS_SEG;

// -- String object -------------------------------------------------------

typedef uint32_t LuaStrHash;   //  String hash value.
typedef uint32_t StrID;      //  String ID.

// String object header. String payload follows.
typedef struct GCstr {
   GCHeader;
   uint8_t reserved;   //  Used by lexer for fast lookup of reserved words.
   uint8_t hashalg;    //  Hash algorithm.
   StrID sid;          //  Interned string ID.
   LuaStrHash hash;    //  Hash of string.
   MSize len;          //  Size of string.
} GCstr;

// String accessor functions - strref defined after GCobj, others are simple
inline GCstr* strref(GCRef r) noexcept;  // Defined after GCobj

[[nodiscard]] inline const char* strdata(const GCstr* s) noexcept
{
   return (const char*)(s + 1);
}

[[nodiscard]] inline char* strdatawr(GCstr* s) noexcept
{
   return (char*)(s + 1);
}

// strVdata uses strV which is defined later
#define strVdata(o)   strdata(strV(o))

// -- Userdata object -----------------------------------------------------

// Userdata object. Payload follows.
typedef struct GCudata {
   GCHeader;
   uint8_t udtype;   //  Userdata type.
   uint8_t unused2;
   GCRef env;      //  Should be at same offset in GCfunc.
   MSize len;      //  Size of payload.
   GCRef metatable;   //  Must be at same offset in GCtab.
   uint32_t align1;   //  To force 8 byte alignment of the payload.
} GCudata;

// Userdata types.
enum {
   UDTYPE_USERDATA,   //  Regular userdata.
   UDTYPE_IO_FILE,    //  I/O library FILE.
   UDTYPE_FFI_CLIB,   //  FFI C library namespace.
   UDTYPE_BUFFER,     //  String buffer.
   UDTYPE_THUNK,      //  Thunk (deferred evaluation).
   UDTYPE__MAX
};

// Thunk userdata payload - stored after GCudata header
// Used for deferred/lazy evaluation of expressions
typedef struct ThunkPayload {
   GCRef deferred_func;    // The deferred closure (GCfunc)
   TValue cached_value;    // Cached resolved value
   uint8_t resolved;       // Resolution flag (0 = not resolved, 1 = resolved)
   uint8_t expected_type;  // LJ type tag for type() (LUA_TSTRING, LUA_TNUMBER, etc.)
   uint16_t padding;       // Padding for alignment
} ThunkPayload;

// Helper to get thunk payload from userdata
#define thunk_payload(u)  ((ThunkPayload *)uddata(u))

[[nodiscard]] inline void* uddata(GCudata* u) noexcept
{
   return (void*)(u + 1);
}

[[nodiscard]] inline MSize sizeudata(const GCudata* u) noexcept
{
   return sizeof(GCudata) + u->len;
}

// -- C data object -------------------------------------------------------

// C data object. Payload follows.
typedef struct GCcdata {
   GCHeader;
   uint16_t ctypeid;   //  C type ID.
} GCcdata;

// Prepended to variable-sized or realigned C data objects.
typedef struct GCcdataVar {
   uint16_t offset;   //  Offset to allocated memory (relative to GCcdata).
   uint16_t extra;    //  Extra space allocated (incl. GCcdata + GCcdatav).
   MSize len;         //  Size of payload.
} GCcdataVar;

[[nodiscard]] inline void* cdataptr(GCcdata* cd) noexcept
{
   return (void*)(cd + 1);
}

[[nodiscard]] inline bool cdataisv(const GCcdata* cd) noexcept
{
   return (cd->marked & 0x80) != 0;
}

[[nodiscard]] inline GCcdataVar* cdatav(GCcdata* cd) noexcept
{
   return (GCcdataVar*)((char*)cd - sizeof(GCcdataVar));
}

[[nodiscard]] inline MSize cdatavlen(GCcdata* cd) noexcept
{
   return check_exp(cdataisv(cd), cdatav(cd)->len);
}

[[nodiscard]] inline MSize sizecdatav(GCcdata* cd) noexcept
{
   return cdatavlen(cd) + cdatav(cd)->extra;
}

[[nodiscard]] inline void* memcdatav(GCcdata* cd) noexcept
{
   return (void*)((char*)cd - cdatav(cd)->offset);
}

// -- Prototype object ----------------------------------------------------

inline constexpr int32_t SCALE_NUM_GCO = int32_t(sizeof(lua_Number) / sizeof(GCRef));

[[nodiscard]] constexpr inline MSize round_nkgc(MSize n) noexcept
{
   return (n + SCALE_NUM_GCO - 1) & ~(SCALE_NUM_GCO - 1);
}

typedef struct GCproto {
   GCHeader;
   uint8_t numparams; //  Number of parameters.
   uint8_t framesize; //  Fixed frame size.
   MSize sizebc;      //  Number of bytecode instructions.
   uint32_t unused_gc64; // Padding for 64-bit alignment
   GCRef gclist;
   MRef k;            //  Split constant array (points to the middle).
   MRef uv;           //  Upvalue list. local slot|0x8000 or parent uv idx.
   MSize sizekgc;     //  Number of collectable constants.
   MSize sizekn;      //  Number of lua_Number constants.
   MSize sizept;      //  Total size including colocated arrays.
   uint8_t sizeuv;    //  Number of upvalues.
   uint8_t flags;     //  Miscellaneous flags (see below).
   uint16_t trace;    //  Anchor for chain of root traces.
   //  The following fields are for debugging/tracebacks only ------
   GCRef chunkname;   //  Name of the chunk this function was defined in.
   BCLine firstline;  //  First line of the function definition.
   BCLine numline;    //  Number of lines for the function definition.
   MRef lineinfo;     //  Compressed map from bytecode ins. to source line.
   MRef uvinfo;       //  Upvalue names.
   MRef varinfo;      //  Names and compressed extents of local variables.
   uint64_t closeslots;  //  Bitmap of locals with <close> attribute (max 64 slots)
} GCproto;

// Flags for prototype.
inline constexpr uint8_t PROTO_CHILD        = 0x01;   //  Has child prototypes.
inline constexpr uint8_t PROTO_VARARG       = 0x02;   //  Vararg function.
inline constexpr uint8_t PROTO_FFI          = 0x04;   //  Uses BC_KCDATA for FFI datatypes.
inline constexpr uint8_t PROTO_NOJIT        = 0x08;   //  JIT disabled for this function.
inline constexpr uint8_t PROTO_ILOOP        = 0x10;   //  Patched bytecode with ILOOP etc.
// Only used during parsing.
inline constexpr uint8_t PROTO_HAS_RETURN   = 0x20;   //  Already emitted a return.
inline constexpr uint8_t PROTO_FIXUP_RETURN = 0x40;   //  Need to fixup emitted returns.
// Top bits used for counting created closures.
inline constexpr uint8_t PROTO_CLCOUNT      = 0x20;   //  Base of saturating 3 bit counter.
inline constexpr int PROTO_CLC_BITS         = 3;
inline constexpr int PROTO_CLC_POLY         = 3 * PROTO_CLCOUNT;  //  Polymorphic threshold.

inline constexpr uint16_t PROTO_UV_LOCAL     = 0x8000;   //  Upvalue for local slot.
inline constexpr uint16_t PROTO_UV_IMMUTABLE = 0x4000;   //  Immutable upvalue.

[[nodiscard]] inline GCobj* proto_kgc(const GCproto* pt, ptrdiff_t idx) noexcept
{
   return check_exp(uintptr_t(intptr_t(idx)) >= uintptr_t(-intptr_t(pt->sizekgc)),
      gcref(mref(pt->k, GCRef)[idx]));
}

[[nodiscard]] inline TValue* proto_knumtv(const GCproto* pt, MSize idx) noexcept
{
   return check_exp(uintptr_t(idx) < pt->sizekn, &mref(pt->k, TValue)[idx]);
}

[[nodiscard]] inline BCIns* proto_bc(const GCproto* pt) noexcept
{
   return (BCIns*)((char*)pt + sizeof(GCproto));
}

[[nodiscard]] inline BCPOS proto_bcpos(const GCproto* pt, const BCIns* pc) noexcept
{
   return BCPOS(pc - proto_bc(pt));
}

[[nodiscard]] inline uint16_t* proto_uv(const GCproto* pt) noexcept
{
   return mref(pt->uv, uint16_t);
}

// Forward declarations - defined after GCobj is complete
inline GCstr* proto_chunkname(const GCproto* pt) noexcept;
inline const char* proto_chunknamestr(const GCproto* pt) noexcept;

[[nodiscard]] inline const void* proto_lineinfo(const GCproto* pt) noexcept
{
   return mref(pt->lineinfo, const void);
}

[[nodiscard]] inline const uint8_t* proto_uvinfo(const GCproto* pt) noexcept
{
   return mref(pt->uvinfo, const uint8_t);
}

[[nodiscard]] inline const uint8_t* proto_varinfo(const GCproto* pt) noexcept
{
   return mref(pt->varinfo, const uint8_t);
}

// -- Upvalue object ------------------------------------------------------

typedef struct GCupval {
   GCHeader;
   uint8_t closed;   //  Set if closed (i.e. uv->v == &uv->u.value).
   uint8_t immutable;   //  Immutable value.
   union {
      TValue tv;      //  If closed: the value itself.
      struct {  // If open: double linked list, anchored at thread.
         GCRef prev;
         GCRef next;
      };
   };
   MRef v;      //  Points to stack slot (open) or above (closed).
   uint32_t dhash;   //  Disambiguation hash: dh1 != dh2 => cannot alias.
} GCupval;

// Forward declarations - defined after GCobj is complete

inline GCupval* uvprev(GCupval* uv) noexcept;
inline GCupval* uvnext(GCupval* uv) noexcept;

[[nodiscard]] inline TValue* uvval(GCupval* uv) noexcept
{
   return mref(uv->v, TValue);
}

// Function object (closures)

// Common header for functions. env should be at same offset in GCudata.
#define GCfuncHeader \
  GCHeader; uint8_t ffid; uint8_t nupvalues; \
  GCRef env; GCRef gclist; MRef pc

typedef struct GCfuncC {
   GCfuncHeader;
   lua_CFunction f;   //  C function to be called.
   TValue upvalue[1];   //  Array of upvalues (TValue).
} GCfuncC;

typedef struct GCfuncL {
   GCfuncHeader;
   GCRef uvptr[1];   //  Array of _pointers_ to upvalue objects (GCupval).
} GCfuncL;

typedef union GCfunc {
   GCfuncC c;
   GCfuncL l;
} GCfunc;

inline constexpr uint8_t FF_LUA = 0;
inline constexpr uint8_t FF_C   = 1;

[[nodiscard]] inline bool isluafunc(const GCfunc* fn) noexcept
{
   return fn->c.ffid IS FF_LUA;
}

[[nodiscard]] inline bool iscfunc(const GCfunc* fn) noexcept
{
   return fn->c.ffid IS FF_C;
}

[[nodiscard]] inline bool isffunc(const GCfunc* fn) noexcept
{
   return fn->c.ffid > FF_C;
}

[[nodiscard]] inline GCproto* funcproto(const GCfunc* fn) noexcept
{
   return check_exp(isluafunc(fn), (GCproto*)(mref(fn->l.pc, char) - sizeof(GCproto)));
}

[[nodiscard]] constexpr inline size_t sizeCfunc(MSize n) noexcept
{
   return sizeof(GCfuncC) - sizeof(TValue) + sizeof(TValue) * n;
}

[[nodiscard]] constexpr inline size_t sizeLfunc(MSize n) noexcept
{
   return sizeof(GCfuncL) - sizeof(GCRef) + sizeof(GCRef) * n;
}

// Table object

// Hash node.

typedef struct Node {
   TValue val;      //  Value object. Must be first field.
   TValue key;      //  Key object.
   MRef next;       //  Hash chain.
} Node;

static_assert(offsetof(Node, val) == 0);

typedef struct GCtab {
   GCHeader;
   uint8_t nomm;      //  Negative cache for fast metamethods.
   int8_t colo;       //  Array colocation.
   MRef array;        //  Array part.
   GCRef gclist;
   GCRef metatable;   //  Must be at same offset in GCudata.
   MRef node;         //  Hash part.
   uint32_t asize;    //  Size of array part (keys [0, asize-1]).
   uint32_t hmask;    //  Hash part mask (size of hash part - 1).
   MRef freetop;      //  Top of free elements.
} GCtab;

[[nodiscard]] constexpr inline size_t sizetabcolo(MSize n) noexcept
{
   return n * sizeof(TValue) + sizeof(GCtab);
}

// Forward declaration - defined after GCobj is complete
inline GCtab* tabref(GCRef r) noexcept;

[[nodiscard]] constexpr inline Node* noderef(MRef r) noexcept
{
   return mref(r, Node);
}

[[nodiscard]] inline Node* nextnode(Node* n) noexcept
{
   return mref(n->next, Node);
}

[[nodiscard]] inline Node* getfreetop(const GCtab* t, Node*) noexcept
{
   return noderef(t->freetop);
}

inline void setfreetop(GCtab* t, Node*, Node* v) noexcept
{
   setmref(t->freetop, v);
}

// -- Array object --------------------------------------------------------

// Array element type constants
enum ArrayElemType : uint8_t {
   ARRAY_ELEM_BYTE    = 0,   // int8_t / uint8_t
   ARRAY_ELEM_INT16   = 1,   // int16_t
   ARRAY_ELEM_INT32   = 2,   // int32_t
   ARRAY_ELEM_INT64   = 3,   // int64_t
   ARRAY_ELEM_FLOAT   = 4,   // float
   ARRAY_ELEM_DOUBLE  = 5,   // double
   ARRAY_ELEM_PTR     = 6,   // void*
   ARRAY_ELEM_STRING  = 7,   // GCstr* (pointer to interned string)
   ARRAY_ELEM_STRUCT  = 8,   // Structured data (uses structdef)
   ARRAY_ELEM__MAX
};

// Array flags
inline constexpr uint8_t ARRAY_FLAG_READONLY  = 0x01;  // Cannot modify elements
inline constexpr uint8_t ARRAY_FLAG_EXTERNAL  = 0x02;  // Data not owned by array
inline constexpr uint8_t ARRAY_FLAG_COLOCATED = 0x04;  // Data follows GCarray header

// Forward declaration for GCarray
struct GCarray;

// Native typed array object. Fixed-size, homogeneous element storage.
typedef struct GCarray {
   GCHeader;
   uint8_t elemtype;        // Element type (ArrayElemType constants)
   uint8_t flags;           // Array flags (read-only, struct-backed, etc.)
   MRef data;               // Pointer to element storage
   GCRef gclist;            // GC list for marking
   GCRef metatable;         // Optional metatable (must be at same offset as GCtab/GCudata)
   MSize len;               // Number of elements
   MSize capacity;          // Allocated capacity (elements, not bytes)
   MSize elemsize;          // Size of each element in bytes
   GCRef structdef;         // Optional: struct definition for struct arrays
} GCarray;

// Ensure metatable field is at the same offset as in GCtab and GCudata
static_assert(offsetof(GCarray, metatable) IS offsetof(GCtab, metatable));
static_assert(offsetof(GCarray, gclist) IS offsetof(GCtab, gclist));

// Array accessor functions
[[nodiscard]] inline void* arraydata(GCarray* arr) noexcept
{
   return mref(arr->data, void);
}

[[nodiscard]] inline MSize arraylen(GCarray* arr) noexcept
{
   return arr->len;
}

[[nodiscard]] inline bool array_is_readonly(GCarray* arr) noexcept
{
   return (arr->flags & ARRAY_FLAG_READONLY) != 0;
}

[[nodiscard]] inline MSize sizearraycolo(MSize len, MSize elemsize) noexcept
{
   return MSize(sizeof(GCarray)) + len * elemsize;
}

// Forward declaration - defined after GCobj is complete
inline GCarray* arrayref(GCRef r) noexcept;

// -- State objects -------------------------------------------------------

// VM states.
enum {
   LJ_VMST_INTERP,   //  Interpreter.
   LJ_VMST_C,        //  C function.
   LJ_VMST_GC,       //  Garbage collector.
   LJ_VMST_EXIT,     //  Trace exit handler.
   LJ_VMST_RECORD,   //  Trace recorder.
   LJ_VMST_OPT,      //  Optimizer.
   LJ_VMST_ASM,      //  Assembler.
   LJ_VMST__MAX
};

#define setvmstate(g, st)   ((g)->vmstate = ~LJ_VMST_##st)

// Metamethods. ORDER MM
// CRITICAL: Metamethod order determines enum values hardcoded in lj_vm.obj.
// New metamethods must be added at the END to avoid shifting indices.
// Inserting in the middle breaks all metamethod dispatch until lj_vm.obj is rebuilt.

#ifdef LJ_HASFFI
#define MMDEF_FFI(_) _(new)
#else
#define MMDEF_FFI(_)
#endif

#define MMDEF_PAIRS(_) _(pairs) _(ipairs)

// X-macro defining all metamethods - used for enum generation and name strings
#define MMDEF(_) \
  _(index) _(newindex) _(gc) _(mode) _(eq) _(len) \
  /* Only the above (fast) metamethods are negative cached (max. 8). */ \
  _(lt) _(le) _(concat) _(call) \
  /* The following must be in ORDER ARITH. */ \
  _(add) _(sub) _(mul) _(div) _(mod) _(pow) _(unm) \
  /* The following are used in the standard libraries. */ \
  _(metatable) _(tostring) \
  _(close) MMDEF_FFI(_) MMDEF_PAIRS(_)

// Metamethod IDs - uses typedef enum because MMDEF generates conditional members
// and the X-macro pattern is required for string generation in lj_meta.cpp

typedef enum : uint8_t {
#define MMENUM(name)   MM_##name,
   MMDEF(MMENUM)
#undef MMENUM
   MM__MAX,
   MM____ = MM__MAX,
   MM_FAST = MM_len
} MMS;

// GC root IDs

typedef enum : uint32_t {
   GCROOT_MMNAME,                              // Metamethod names
   GCROOT_MMNAME_LAST = GCROOT_MMNAME + MM__MAX - 1,
   GCROOT_BASEMT,                              // Metatables for base types
   GCROOT_BASEMT_NUM = GCROOT_BASEMT + ~LJ_TNUMX,
   GCROOT_IO_INPUT,                            // Userdata for default I/O input file
   GCROOT_IO_OUTPUT,                           // Userdata for default I/O output file
   GCROOT_MAX
} GCRootID;

// GC root accessors - must remain as macros due to header ordering
#define basemt_it(g, it)    ((g)->gcroot[GCROOT_BASEMT+~(it)])
#define basemt_obj(g, o)    ((g)->gcroot[GCROOT_BASEMT+itypemap(o)])
#define mmname_str(g, mm)   (strref((g)->gcroot[GCROOT_MMNAME+int(mm)]))

// Garbage collector states. Order matters.
// Using enum class for type safety; values must match GCState.state field usage.

enum class GCPhase : uint8_t {
   Pause       = 0,
   Propagate   = 1,
   Atomic      = 2,
   SweepString = 3,
   Sweep       = 4,
   Finalize    = 5
};

// Garbage collector state.
typedef struct GCState {
   GCSize total;         // Memory currently allocated.
   GCSize threshold;     // Memory threshold.
   uint8_t currentwhite; // Current white color.
   GCPhase state;        // GC state.
   uint8_t nocdatafin;   // No cdata finalizer called.
   uint8_t lightudnum;   //  Number of lightuserdata segments - 1 (64-bit only).
   MSize sweepstr;       // Sweep position in string table.
   GCRef root;           // List of all collectable objects.
   MRef sweep;           // Sweep position in root list.
   GCRef gray;           // List of gray objects.
   GCRef grayagain;      // List of objects for atomic traversal.
   GCRef weak;           // List of weak tables (to be cleared).
   GCRef mmudata;        // List of userdata (to be finalized).
   GCSize debt;          // Debt (how much GC is behind schedule).
   GCSize estimate;      // Estimate of memory actually in use.
   MSize stepmul;        // Incremental GC step granularity.
   MSize pause;          // Pause between successive GC cycles.
   MRef lightudseg;      //  Upper bits of lightuserdata segments (64-bit).
} GCState;

// String interning state.
typedef struct StrInternState {
   GCRef* tab;       //  String hash table anchors.
   MSize mask;       //  String hash mask (size of hash table - 1).
   MSize num;        //  Number of strings in hash table.
   StrID id;         //  Next string ID.
   uint8_t idreseed; //  String ID reseed counter.
   uint8_t second;   //  String interning table uses secondary hashing.
   uint8_t unused1;
   uint8_t unused2;
   LJ_ALIGN(8) uint64_t seed;   //  Random string seed.
} StrInternState;

// Global state, shared by all threads of a Lua universe.
typedef struct global_State {
   lua_Alloc allocf;         // Memory allocator.
   void      *allocd;        // Memory allocator data.
   GCState   gc;             // Garbage collector.
   GCstr     strempty;       // Empty string.
   uint8_t   stremptyz;      // Zero terminator of empty string.
   uint8_t   hookmask;       // Hook mask.
   uint8_t   dispatchmode;   // Dispatch mode.
   uint8_t   vmevmask;       // VM event mask.
   StrInternState str;       // String interning.
   volatile int32_t vmstate; // VM state or current JIT code trace number.
   GCRef    mainthref;       // Link to main thread.
   SBuf     tmpbuf;          // Temporary string buffer.
   TValue   tmptv, tmptv2;   // Temporary TValues.
   Node     nilnode;         // Fallback 1-element hash part (nil key and value).
   TValue   registrytv;      // Anchor for registry.
   GCupval  uvhead;          // Head of double-linked list of all open upvalues.
   int32_t  hookcount;       // Instruction hook countdown.
   int32_t  hookcstart;      // Start count for instruction hook counter.
   lua_Hook hookf;           // Hook function.
   lua_CFunction wrapf;      // Wrapper for C function calls.
   lua_CFunction panic;      // Called as a last resort for errors.
   BCIns     bc_cfunc_int;   // Bytecode for internal C function calls.
   BCIns     bc_cfunc_ext;   // Bytecode for external C function calls.
   GCRef     cur_L;          // Currently executing lua_State.
   MRef      jit_base;       // Current JIT code L->base or NULL.
   MRef      ctype_state;    // Pointer to C type state.
   PRNGState prng;           // Global PRNG state.
   void     *funcnames;      // Map of GCproto* to function names (std::unordered_map<>*).
   GCRef     gcroot[GCROOT_MAX];  //  GC roots.
} global_State;

// Forward declarations - defined after GCobj is complete
inline lua_State* mainthread(global_State* g) noexcept;
inline TValue* niltv(lua_State* L) noexcept;
[[nodiscard]] constexpr inline bool tvisnil(cTValue* o) noexcept;  // Defined in TValue section

// niltvg doesn't depend on GCobj, can be defined here
[[nodiscard]] inline TValue* niltvg(global_State* g) noexcept
{
   return check_exp(tvisnil(&g->nilnode.val), &g->nilnode.val);
}

// Hook management. Hook event masks are defined in lua.h.
inline constexpr uint8_t HOOK_EVENTMASK    = 0x0f;
inline constexpr uint8_t HOOK_ACTIVE       = 0x10;
inline constexpr int HOOK_ACTIVE_SHIFT     = 4;
inline constexpr uint8_t HOOK_VMEVENT      = 0x20;
inline constexpr uint8_t HOOK_GC           = 0x40;
inline constexpr uint8_t HOOK_PROFILE      = 0x80;

[[nodiscard]] inline bool hook_active(const global_State* g) noexcept
{
   return (g->hookmask & HOOK_ACTIVE) != 0;
}

inline void hook_enter(global_State* g) noexcept
{
   g->hookmask |= HOOK_ACTIVE;
}

inline void hook_entergc(global_State* g) noexcept
{
   g->hookmask = (g->hookmask | (HOOK_ACTIVE | HOOK_GC)) & ~HOOK_PROFILE;
}

inline void hook_vmevent(global_State* g) noexcept
{
   g->hookmask |= (HOOK_ACTIVE | HOOK_VMEVENT);
}

inline void hook_leave(global_State* g) noexcept
{
   g->hookmask &= ~HOOK_ACTIVE;
}

[[nodiscard]] inline uint8_t hook_save(const global_State* g) noexcept
{
   return g->hookmask & ~HOOK_EVENTMASK;
}

inline void hook_restore(global_State* g, uint8_t h) noexcept
{
   g->hookmask = (g->hookmask & HOOK_EVENTMASK) | h;
}

// Per-thread state object.
struct lua_State {
   GCHeader;
   uint8_t dummy_ffid;  //  Fake FF_C for curr_funcisL() on dummy frames.
   uint8_t status;      //  Thread status.
   MRef    glref;       //  Link to global state.
   GCRef   gclist;      //  GC chain.
   TValue  *base;       //  Base of currently executing function.
   TValue  *top;        //  First free slot in the stack.
   MRef    maxstack;    //  Last free slot in the stack.
   MRef    stack;       //  Stack base.
   GCRef   openupval;   //  List of open upvalues in the stack.
   GCRef   env;         //  Thread environment (table of globals).
   void    *cframe;     //  End of C stack frame chain.
   MSize   stacksize;   //  True stack size (incl. LJ_STACK_EXTRA).
   class objScript *script;
   bool    protected_globals; // Becomes true once all global constants are initialised
   uint8_t resolving_thunk;  // Flag to prevent recursive thunk resolution
   ParserDiagnostics *parser_diagnostics; // Stores ParserDiagnostics* during parsing errors
   TValue close_err;  // Current error for __close handlers (nil if no error)

   // Constructor/destructor not actually used as yet.
/*
   lua_State(class objScript* pScript) : Script(pScript), protected_globals(false) {

   }

   ~lua_State() {
   }
*/
};

[[nodiscard]] inline global_State * G(lua_State* L) noexcept
{
   return mref(L->glref, global_State);
}

[[nodiscard]] inline TValue * registry(lua_State* L) noexcept
{
   return &G(L)->registrytv;
}

// Forward declarations - defined after GCobj is complete
// Functions to access the currently executing (Lua) function.

inline GCfunc * curr_func(lua_State *) noexcept;
inline bool curr_funcisL(lua_State *) noexcept;
inline GCproto * curr_proto(lua_State *) noexcept;
inline TValue * curr_topL(lua_State *) noexcept;
inline TValue * curr_top(lua_State *) noexcept;

#if defined(LUA_USE_ASSERT) || defined(LUA_USE_APICHECK)
LJ_FUNC_NORET void lj_assert_fail(global_State* g, const char* file, int line,
   const char* func, const char* fmt, ...);
#endif

//********************************************************************************************************************
// GC header for generic access to common fields of GC objects.

typedef struct GChead {
   GCHeader;
   uint8_t unused1;
   uint8_t unused2;
   GCRef env;
   GCRef gclist;
   GCRef metatable;
} GChead;

// The env field SHOULD be at the same offset for all GC objects.
static_assert(offsetof(GChead, env) == offsetof(GCfuncL, env));
static_assert(offsetof(GChead, env) == offsetof(GCudata, env));

// The metatable field MUST be at the same offset for all GC objects.
static_assert(offsetof(GChead, metatable) == offsetof(GCtab, metatable));
static_assert(offsetof(GChead, metatable) == offsetof(GCudata, metatable));

// The gclist field MUST be at the same offset for all GC objects.
static_assert(offsetof(GChead, gclist) == offsetof(lua_State, gclist));
static_assert(offsetof(GChead, gclist) == offsetof(GCproto, gclist));
static_assert(offsetof(GChead, gclist) == offsetof(GCfuncL, gclist));
static_assert(offsetof(GChead, gclist) == offsetof(GCtab, gclist));

typedef union GCobj {
   GChead gch;
   GCstr str;
   GCupval uv;
   lua_State th;
   GCproto pt;
   GCfunc fn;
   GCcdata cd;
   GCtab tab;
   GCarray arr;
   GCudata ud;
} GCobj;

// Functions to convert a GCobj pointer into a specific value.
[[nodiscard]] inline GCstr* gco2str(GCobj* o) noexcept
{
   return check_exp(o->gch.gct IS ~LJ_TSTR, &o->str);
}

[[nodiscard]] inline GCupval* gco2uv(GCobj* o) noexcept
{
   return check_exp(o->gch.gct IS ~LJ_TUPVAL, &o->uv);
}

[[nodiscard]] inline lua_State* gco2th(GCobj* o) noexcept
{
   return check_exp(o->gch.gct IS ~LJ_TTHREAD, &o->th);
}

[[nodiscard]] inline GCproto* gco2pt(GCobj* o) noexcept
{
   return check_exp(o->gch.gct IS ~LJ_TPROTO, &o->pt);
}

[[nodiscard]] inline GCfunc* gco2func(GCobj* o) noexcept
{
   return check_exp(o->gch.gct IS ~LJ_TFUNC, &o->fn);
}

[[nodiscard]] inline GCcdata* gco2cd(GCobj* o) noexcept
{
   return check_exp(o->gch.gct IS ~LJ_TCDATA, &o->cd);
}

[[nodiscard]] inline GCtab* gco2tab(GCobj* o) noexcept
{
   return check_exp(o->gch.gct IS ~LJ_TTAB, &o->tab);
}

[[nodiscard]] inline GCudata* gco2ud(GCobj* o) noexcept
{
   return check_exp(o->gch.gct IS ~LJ_TUDATA, &o->ud);
}

[[nodiscard]] inline GCarray* gco2arr(GCobj* o) noexcept
{
   return check_exp(o->gch.gct IS ~LJ_TARRAY, &o->arr);
}

// Convert any collectable object into a GCobj pointer.
template<typename T>
[[nodiscard]] inline GCobj* obj2gco(T* v) noexcept
{
   return (GCobj*)v;
}

// -- Deferred inline function definitions (require complete GCobj type) --

// gcval: get GC object pointer from tagged value (just pointer math, no dereference)

[[nodiscard]] inline GCobj* gcval(cTValue* o) noexcept { return (GCobj*)(gcrefu(o->gcr) & LJ_GCVMASK); }

// String accessors

[[nodiscard]] inline GCstr* strref(GCRef r) noexcept { return &gcref(r)->str; }

// Prototype accessors

[[nodiscard]] inline GCstr* proto_chunkname(const GCproto* pt) noexcept { return strref(pt->chunkname); }
[[nodiscard]] inline const char* proto_chunknamestr(const GCproto* pt) noexcept { return strdata(proto_chunkname(pt)); }

// Table accessors
[[nodiscard]] inline GCtab* tabref(GCRef r) noexcept { return &gcref(r)->tab; }

// Array accessors
[[nodiscard]] inline GCarray* arrayref(GCRef r) noexcept { return &gcref(r)->arr; }

// Thread/state accessors

[[nodiscard]] inline lua_State* mainthread(global_State* g) noexcept { return &gcref(g->mainthref)->th; }

// Function accessors for currently executing function

[[nodiscard]] inline GCfunc* curr_func(lua_State* L) noexcept { return &gcval(L->base - 2)->fn; }
[[nodiscard]] inline bool curr_funcisL(lua_State* L) noexcept { return isluafunc(curr_func(L)); }
[[nodiscard]] inline GCproto* curr_proto(lua_State* L) noexcept { return funcproto(curr_func(L)); }
[[nodiscard]] inline TValue* curr_topL(lua_State* L) noexcept { return L->base + curr_proto(L)->framesize; }
[[nodiscard]] inline TValue* curr_top(lua_State* L) noexcept { return curr_funcisL(L) ? curr_topL(L) : L->top; }

// Upvalue list navigation

[[nodiscard]] inline GCupval* uvprev(GCupval* uv) noexcept { return &gcref(uv->prev)->uv; }
[[nodiscard]] inline GCupval* uvnext(GCupval* uv) noexcept { return &gcref(uv->next)->uv; }

// niltv defined at end of file (needs tvisnil which is defined below)

//********************************************************************************************************************
// TValue getters/setters

// Type test functions - 64-bit GC64 mode only

[[nodiscard]] constexpr inline uint32_t itype(cTValue* o) noexcept { return uint32_t(o->it64 >> 47); }
[[nodiscard]] constexpr inline bool tvisnil(cTValue* o) noexcept { return o->it64 IS -1; }
[[nodiscard]] constexpr inline bool tvisfalse(cTValue* o) noexcept { return itype(o) IS LJ_TFALSE; }
[[nodiscard]] constexpr inline bool tvistrue(cTValue* o) noexcept { return itype(o) IS LJ_TTRUE; }
[[nodiscard]] constexpr inline bool tvisbool(cTValue* o) noexcept { return tvisfalse(o) or tvistrue(o); }
[[nodiscard]] constexpr inline bool tvislightud(cTValue* o) noexcept { return itype(o) IS LJ_TLIGHTUD; }
[[nodiscard]] constexpr inline bool tvisstr(cTValue* o) noexcept { return itype(o) IS LJ_TSTR; }
[[nodiscard]] constexpr inline bool tvisfunc(cTValue* o) noexcept { return itype(o) IS LJ_TFUNC; }
[[nodiscard]] constexpr inline bool tvisthread(cTValue* o) noexcept { return itype(o) IS LJ_TTHREAD; }
[[nodiscard]] constexpr inline bool tvisproto(cTValue* o) noexcept { return itype(o) IS LJ_TPROTO; }
[[nodiscard]] constexpr inline bool tviscdata(cTValue* o) noexcept { return itype(o) IS LJ_TCDATA; }
[[nodiscard]] constexpr inline bool tvistab(cTValue* o) noexcept { return itype(o) IS LJ_TTAB; }
[[nodiscard]] constexpr inline bool tvisudata(cTValue* o) noexcept { return itype(o) IS LJ_TUDATA; }
[[nodiscard]] constexpr inline bool tvisarray(cTValue* o) noexcept { return itype(o) IS LJ_TARRAY; }
[[nodiscard]] constexpr inline bool tvisnumber(cTValue* o) noexcept { return itype(o) <= LJ_TISNUM; }
[[nodiscard]] constexpr inline bool tvisint(cTValue* o) noexcept { return LJ_DUALNUM and itype(o) IS LJ_TISNUM; }
[[nodiscard]] constexpr inline bool tvisnum(cTValue* o) noexcept { return itype(o) < LJ_TISNUM; }
[[nodiscard]] constexpr inline bool tvistruecond(cTValue* o) noexcept { return itype(o) < LJ_TISTRUECOND; }
[[nodiscard]] constexpr inline bool tvispri(cTValue* o) noexcept { return itype(o) >= LJ_TISPRI; }
[[nodiscard]] constexpr inline bool tvistabud(cTValue* o) noexcept { return itype(o) <= LJ_TISTABUD; }
[[nodiscard]] constexpr inline bool tvisgcv(cTValue* o) noexcept { return (itype(o) - LJ_TISGCV) > (LJ_TNUMX - LJ_TISGCV); }

// Special functions to test numbers for NaN, +0, -0, +1 and raw equality.

[[nodiscard]] constexpr inline bool tvisnan(cTValue* o) noexcept { return o->n != o->n; }
[[nodiscard]] constexpr inline bool tviszero(cTValue* o) noexcept { return (o->u64 << 1) IS 0; }
[[nodiscard]] constexpr inline bool tvispzero(cTValue* o) noexcept { return o->u64 IS 0; }
[[nodiscard]] constexpr inline bool tvismzero(cTValue* o) noexcept { return o->u64 IS U64x(80000000, 00000000); }
[[nodiscard]] constexpr inline bool tvispone(cTValue* o) noexcept { return o->u64 IS U64x(3ff00000, 00000000); }
[[nodiscard]] constexpr inline bool rawnumequal(cTValue* o1, cTValue* o2) noexcept { return o1->u64 IS o2->u64; }

// Convert internal type to type map index - 64-bit GC64 mode

[[nodiscard]] constexpr inline uint32_t itypemap(cTValue* o) noexcept { return tvisnumber(o) ? ~LJ_TNUMX : ~itype(o); }

// Functions to get tagged values (gcval is defined in deferred section above)

[[nodiscard]] inline int boolV(cTValue* o) noexcept { return check_exp(tvisbool(o), int(LJ_TFALSE - itype(o))); }

// Lightuserdata segment/offset extraction - 64-bit only

[[nodiscard]] constexpr inline uint64_t lightudseg(uint64_t u) noexcept { return (u >> LJ_LIGHTUD_BITS_LO) & ((1 << LJ_LIGHTUD_BITS_SEG) - 1); }
[[nodiscard]] constexpr inline uint64_t lightudlo(uint64_t u) noexcept { return u & ((uint64_t(1) << LJ_LIGHTUD_BITS_LO) - 1); }
[[nodiscard]] constexpr inline uint32_t lightudup(uint64_t p) noexcept { return uint32_t((p >> LJ_LIGHTUD_BITS_LO) << (LJ_LIGHTUD_BITS_LO - 32)); }

// Lightuserdata value extraction - 64-bit only
[[nodiscard]] static LJ_AINLINE void* lightudV(global_State* g, cTValue* o) noexcept
{
   uint64_t u = o->u64;
   uint64_t seg = lightudseg(u);
   uint32_t* segmap = mref(g->gc.lightudseg, uint32_t);
   lj_assertG(tvislightud(o), "lightuserdata expected");
   lj_assertG(seg <= g->gc.lightudnum, "bad lightuserdata segment %d", seg);
   return (void*)(((uint64_t)segmap[seg] << 32) | lightudlo(u));
}

[[nodiscard]] inline GCobj* gcV(cTValue* o) noexcept
{
   return check_exp(tvisgcv(o), gcval(o));
}

[[nodiscard]] inline GCstr* strV(cTValue* o) noexcept
{
   return check_exp(tvisstr(o), &gcval(o)->str);
}

[[nodiscard]] inline GCfunc* funcV(cTValue* o) noexcept
{
   return check_exp(tvisfunc(o), &gcval(o)->fn);
}

[[nodiscard]] inline lua_State* threadV(cTValue* o) noexcept
{
   return check_exp(tvisthread(o), &gcval(o)->th);
}

[[nodiscard]] inline GCproto* protoV(cTValue* o) noexcept
{
   return check_exp(tvisproto(o), &gcval(o)->pt);
}

[[nodiscard]] inline GCcdata* cdataV(cTValue* o) noexcept
{
   return check_exp(tviscdata(o), &gcval(o)->cd);
}

[[nodiscard]] inline GCtab* tabV(cTValue* o) noexcept
{
   return check_exp(tvistab(o), &gcval(o)->tab);
}

[[nodiscard]] inline GCudata* udataV(cTValue* o) noexcept
{
   return check_exp(tvisudata(o), &gcval(o)->ud);
}

[[nodiscard]] inline GCarray* arrayV(cTValue* o) noexcept
{
   return check_exp(tvisarray(o), &gcval(o)->arr);
}

[[nodiscard]] inline lua_Number numV(cTValue* o) noexcept
{
   return check_exp(tvisnum(o), o->n);
}

[[nodiscard]] inline int32_t intV(cTValue* o) noexcept
{
   return check_exp(tvisint(o), int32_t(o->i));
}

// Functions to set tagged values.

inline void setitype(TValue* o, uint32_t i) noexcept
{
   o->it = i << 15;
}

inline void setnilV(TValue* o) noexcept
{
   o->it64 = -1;
}

inline void setpriV(TValue* o, uint32_t x) noexcept
{
   // Avoid C4319 warning by doing negation at 64-bit level: use ~ on the shifted value.
   o->it64 = int64_t(~(((uint64_t(x) ^ 0xFFFFFFFFULL)) << 47));
}

inline void setboolV(TValue* o, int x) noexcept
{
   o->it64 = int64_t(~(uint64_t(x + 1) << 47));
}

inline void setrawlightudV(TValue* o, void* p)
{
   o->u64 = (uint64_t)p | (((uint64_t)LJ_TLIGHTUD) << 47);
}

#define contptr(f)      ((void *)(f))
#define setcont(o, f)      ((o)->u64 = (uint64_t)(uintptr_t)contptr(f))

inline void checklivetv(lua_State* L, TValue* o, const char* msg) noexcept
{
   UNUSED(L); UNUSED(o); UNUSED(msg);
#if LUA_USE_ASSERT
   if (tvisgcv(o)) {
      lj_assertL(~itype(o) IS gcval(o)->gch.gct,
         "mismatch of TValue type %d vs GC type %d",
         ~itype(o), gcval(o)->gch.gct);
      // Copy of isdead check from lj_gc.h to avoid circular include.
      lj_assertL(!(gcval(o)->gch.marked & (G(L)->gc.currentwhite ^ 3) & 3), msg);
   }
#endif
}

inline void setgcVraw(TValue* o, GCobj* v, uint32_t IType) noexcept
{
   setgcreft(o->gcr, v, IType);
}

inline void setgcV(lua_State* L, TValue* o, GCobj* v, uint32_t It) noexcept
{
   setgcVraw(o, v, It);
   checklivetv(L, o, "store to dead GC object");
}

inline void setstrV(lua_State* L, TValue* o, const GCstr* v) noexcept
{
   setgcV(L, o, obj2gco(v), LJ_TSTR);
}

inline void setthreadV(lua_State* L, TValue* o, const lua_State* v) noexcept
{
   setgcV(L, o, obj2gco(v), LJ_TTHREAD);
}

inline void setprotoV(lua_State* L, TValue* o, const GCproto* v) noexcept
{
   setgcV(L, o, obj2gco(v), LJ_TPROTO);
}

inline void setfuncV(lua_State* L, TValue* o, const GCfunc* v) noexcept
{
   setgcV(L, o, obj2gco(v), LJ_TFUNC);
}

inline void setcdataV(lua_State* L, TValue* o, const GCcdata* v) noexcept { setgcV(L, o, obj2gco(v), LJ_TCDATA); }
inline void settabV(lua_State* L, TValue* o, const GCtab* v) noexcept { setgcV(L, o, obj2gco(v), LJ_TTAB); }
inline void setudataV(lua_State* L, TValue* o, const GCudata* v) noexcept { setgcV(L, o, obj2gco(v), LJ_TUDATA); }
inline void setarrayV(lua_State* L, TValue* o, const GCarray* v) noexcept { setgcV(L, o, obj2gco(v), LJ_TARRAY); }
constexpr inline void setnumV(TValue* o, lua_Number x) noexcept { o->n = x; }
inline void setnanV(TValue* o) noexcept { o->u64 = U64x(fff80000, 00000000); }
inline void setpinfV(TValue* o) noexcept { o->u64 = U64x(7ff00000, 00000000); }
inline void setminfV(TValue* o) noexcept { o->u64 = U64x(fff00000, 00000000); }

inline void setintV(TValue* o, int32_t i) noexcept {
#if LJ_DUALNUM
   o->i = uint32_t(i);
   setitype(o, LJ_TISNUM);
#else
   o->n = lua_Number(i);
#endif
}

inline void setint64V(TValue* o, int64_t i) noexcept {
   if (LJ_DUALNUM and LJ_LIKELY(i IS int64_t(int32_t(i)))) setintV(o, int32_t(i));
   else setnumV(o, lua_Number(i));
}

inline void setintptrV(TValue* o, intptr_t i) noexcept { setint64V(o, i); }

// Copy tagged values.
inline void copyTV(lua_State* L, TValue* o1, const TValue* o2)
{
   *o1 = *o2;
   checklivetv(L, o1, "copy of dead GC object");
}

//********************************************************************************************************************
// Number to integer conversion

[[nodiscard]] inline int32_t lj_num2bit(lua_Number n) noexcept
{
   TValue o;
   o.n = n + 6755399441055744.0;  //  2^52 + 2^51
   return (int32_t)o.u32.lo;
}

[[nodiscard]] constexpr inline int32_t lj_num2int(lua_Number n) noexcept
{
   return (int32_t)(n);
}

// This must match the JIT backend behavior. In particular for archs that don't have a common hardware instruction for
// this conversion.  Note that signed FP to unsigned int conversions have an undefined result and should never be
// relied upon in portable FFI code.  See also: C99 or C11 standard, 6.3.1.4, footnote of (1).

[[nodiscard]] inline uint64_t lj_num2u64(lua_Number n) noexcept {
#if LJ_TARGET_X86ORX64
   int64_t i = (int64_t)n;
   if (i < 0) i = (int64_t)(n - 18446744073709551616.0);
   return (uint64_t)i;
#else
   return (uint64_t)n;
#endif
}

[[nodiscard]] inline int32_t numberVint(cTValue* o) noexcept {
   if (LJ_LIKELY(tvisint(o))) return intV(o);
   else return lj_num2int(numV(o));
}

[[nodiscard]] inline lua_Number numberVnum(cTValue* o) noexcept {
   if (LJ_UNLIKELY(tvisint(o))) return (lua_Number)intV(o);
   else return numV(o);
}

// Names and maps for internal and external object tags.
LJ_DATA const char* const lj_obj_typename[1 + LUA_TCDATA + 1];
LJ_DATA const char* const lj_obj_itypename[~LJ_TNUMX + 1];

[[nodiscard]] inline const char* lj_typename(cTValue* o) noexcept {
   return lj_obj_itypename[itypemap(o)];
}

// Compare two objects without calling metamethods.
LJ_FUNC int LJ_FASTCALL lj_obj_equal(cTValue* o1, cTValue* o2);
LJ_FUNC const void* LJ_FASTCALL lj_obj_ptr(global_State* g, cTValue* o);

// Late deferred function definitions (need tvisnil, G)

[[nodiscard]] inline TValue* niltv(lua_State* L) noexcept {
   return check_exp(tvisnil(&G(L)->nilnode.val), &G(L)->nilnode.val);
}
