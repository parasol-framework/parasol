#ifndef PARASOL_MAIN_H
#define PARASOL_MAIN_H TRUE

//   main.h
//
//   General include file for all programs.
//
//   Copyright 1996-2020 © Paul Manias

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#pragma warning (disable : 4244 4311 4312) // Disable annoying VC++ typecast warnings
#endif

#ifndef SYSTEM_TYPES_H
#include <parasol/system/types.h>
#endif

#ifndef SYSTEM_REGISTRY_H
#include <parasol/system/registry.h>
#endif

#ifndef SYSTEM_ERRORS_H
#include <parasol/system/errors.h>
#endif

#ifndef SYSTEM_FIELDS_H
#include <parasol/system/fields.h>
#endif

#ifndef __GNUC__
#define __attribute__(a)
#endif

//****************************************************************************

#define VER_CORE (1.0f)  // Core version + revision
#define REV_CORE (0)     // Core revision as a whole number

#define MODULE_COREBASE struct CoreBase *CoreBase = 0;

#ifndef STRINGIFY
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#endif

#define MOD_IDL NULL

#ifdef MOD_NAME
#define PARASOL_MOD(init,close,open,expunge,version) MODULE_HEADER = { MODULE_HEADER_VERSION, MHF_DEFAULT, version, VER_CORE, MOD_IDL, init, close, open, expunge, NULL, TOSTRING(MOD_NAME), NULL };
#define MOD_PATH ("modules:" TOSTRING(MOD_NAME))
#endif

#ifdef DEBUG
#define MSG(...)  LogF(0,__VA_ARGS__)
#define FMSG(...) LogF(__VA_ARGS__)
#define STEP()    LogBack()
#else
#define MSG(...)
#define FMSG(...)
#define STEP()
#endif
#define LOG FMSG
#define LOGBACK STEP

#ifdef _WIN32
#ifndef _WINDEF_H
#define __export __declspec(dllexport)
#endif
#else
#define __export
#endif

#define EXPORT __export

#ifndef __arm__
//#define __pentium__   // Is this a Pentium CPU? (auto-defined by GCC, use -mpentium)
#define __x86__         // Does the CPU support the x86 instruction set? (i486 minimum)
#endif

#define __corebase__  // Use CoreBase to make function calls

#ifdef __arm__
#define CPU_PC CPU_ARMEABI
#elif __pentium__
#define CPU_PC CPU_I686 // What is the minimum required CPU for the compiled code?
#else
#define CPU_PC CPU_I686
#endif

#ifndef REVERSE_BYTEORDER
#define REVERSE_BYTEORDER TRUE      // Reverse byte order / little endian (true for Intel and ARM CPU's)
#endif

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN TRUE
#endif

#define PRIVATE_FIELDS

#undef  NULL    // Turn off any previous definition of NULL
#define NULL 0  // NULL is a value of 0

#define skipwhitespace(a) while ((*(a) > 0) && (*(a) <= 0x20)) (a)++;

#define ARRAYSIZE(a) ((LONG)(sizeof(a)/sizeof(a[0])))

#undef MIN
#undef MAX
#undef MID
#undef ABS
#undef SGN

#define ROUNDUP(a,b) (((a) + (b)) - ((a) % (b))) // ROUNDUP(Number, Alignment) e.g. (14,8) = 16
#define MIN(x,y)     (((x) < (y)) ? (x) : (y))
#define MAX(x,y)     (((x) > (y)) ? (x) : (y))
#define MID(x,y,z)   MAX((x), MIN((y), (z)))
#define ABS(x)       (((x) >= 0) ? (x) : (-(x)))
#define SGN(x)       ((x<0)?-1:((x>0)?1:0))
#define LCASE(a)     ((((a) >= 'A') AND ((a) <= 'Z')) ? ((a) - 'A' + 'a') : (a))
#define UCASE(a)     ((((a) >= 'a') AND ((a) <= 'z')) ? ((a) - 'a' + 'A') : (a))

#define AlignLarge(a) (((a) + 7) & (~7))
#define AlignLong(a)  (((a) + 3) & (~3))
#define AlignWord(a)  (((a) + 1) & (~1))

#define ALIGN64(a) (((a) + 7) & (~7))
#define ALIGN32(a) (((a) + 3) & (~3))
#define ALIGN16(a) (((a) + 1) & (~1))

#define CODE_MEMH 0x4D454D48L
#define CODE_MEMT 0x4D454D54L

#ifdef __CYGWIN__
#define PF64() "%lld"
#elif _WIN32
 #ifdef PRId64
  #define PF64() "%"PRId64
 #elif _LP64 // msys gcc-64 accepts %lld, but gcc-32 only %I64
  #define PF64() "%lld"
 #else
  #define PF64() "%I64d"
 #endif
#else
#define PF64() "%lld"
#endif

/*****************************************************************************
** Endian management routines.
*/

#ifdef REVERSE_BYTEORDER

// CPU is little endian (Intel, ARM)

#define rd_long(a)    (((a)[0]<<24)|((a)[1]<<16)|((a)[2]<<8)|((a)[3]))
#define rd_word(a)    (((a)[0]<<8)|((a)[1]))
#define wrb_long(a,b) ((LONG *)(b))[0] = (a)
#define wrb_word(a,b) ((WORD *)(b))[0] = (a)

#define cpu_le32(x) (x)
#define le32_cpu(x) (x)
#define cpu_le16(x) (x)
#define le16_cpu(x) (x)

INLINE ULONG cpu_be32(ULONG x) {
   return ((((UBYTE)x)<<24)|(((UBYTE)(x>>8))<<16)|((x>>8) & 0xff00)|(x>>24));
}

#define be32_cpu(x) ((x<<24)|((x<<8) & 0xff0000)|((x>>8) & 0xff00)|(x>>24))
#define cpu_be16(x) ((x<<8)|(x>>8))
#define be16_cpu(x) ((x<<8)|(x>>8))

#else

// CPU is big endian (Motorola)

#define rd_long(a)   (a)
#define rd_word(a)   (a)
#define wd_long(a,b) ((LONG *)(b))[0] = (a)
#define wd_word(a,b) ((WORD *)(b))[0] = (a)
#define wrb_long(a,b) (b)[0] = (UBYTE)(a); (b)[1] = (UBYTE)((a)>>8); (b)[2] = (UBYTE)((a)>>16); (b)[3] = (UBYTE)((a)>>24)
#define wrb_word(a,b) (b)[0] = (UBYTE)(a); (b)[1] = (UBYTE)((a)>>8)

#define cpu_le32(x) ((x<<24)|((x<<8) & 0xff0000)|((x>>8) & 0xff00)|(x>>24))
#define le32_cpu(x) ((x<<24)|((x<<8) & 0xff0000)|((x>>8) & 0xff00)|(x>>24))
#define cpu_le16(x) ((x<<8)|(x>>8))
#define le16_cpu(x) ((x<<8)|(x>>8))
#define cpu_be32(x) (x)
#define be32_cpu(x) (x)
#define cpu_be16(x) (x)
#define be16_cpu(x) (x)
#endif

// Fast float-2-int conversion, with rounding to the nearest integer (F2I) and truncation (F2T)

#if defined(__GNUC__) && defined(__x86__)

INLINE LONG F2I(DOUBLE val) {
   // This will round if the CPU is kept in its default rounding mode
   LONG ret;
   asm ("fistpl %0" : "=m" (ret) : "t" (val) : "st");
   return(ret);
}

#else

INLINE LONG F2I(DOUBLE val) {
   DOUBLE t = val + 6755399441055744.0;
   return *((int *)(&t));
}

#endif

INLINE LONG F2T(DOUBLE val) // For numbers no larger than 16 bit, standard (LONG) is faster than F2T().
{
   if ((val > 32767.0) OR (val < -32767.0)) return((LONG)val);
   else {
      val = val + (68719476736.0 * 1.5);
#ifdef REVERSE_BYTEORDER
      return ((LONG *)(APTR)&val)[0]>>16;
#else
      return ((LONG *)&val)[1]>>16;
#endif
   }
}

//#define F2T(a) LONG(a)

// Structures to pass to OpenCore()

struct OpenTag {
   LONG Tag;
   union {
      LONG Long;
      LARGE Large;
      APTR Pointer;
      CSTRING String;
   } Value;
};

struct OpenInfo {
   LONG    Flags;           // OPF flags need to be set for fields that have been defined in this structure.
   CSTRING Name;            // OPF_NAME
   CSTRING Copyright;       // OPF_COPYRIGHT
   CSTRING Date;            // OPF_DATE
   CSTRING Author;          // OPF_AUTHOR
   FLOAT   CoreVersion;     // OPF_CORE_VERSION
   LONG    JumpTable;       // OPF_JUMPTABLE
   LONG    MaxDepth;        // OPF_MAX_DEPTH
   LONG    Detail;          // OPF_DETAIL
   CSTRING *Args;           // OPF_ARGS
   LONG    ArgCount;        // OPF_ARGS
   ERROR   Error;           // OPF_ERROR
   FLOAT   CompiledAgainst; // OPF_COMPILED_AGAINST
   CSTRING SystemPath;      // OPF_SYSTEM_PATH
   CSTRING ModulePath;      // OPF_MODULE_PATH
   CSTRING RootPath;        // OPF_ROOT_PATH
   struct OpenTag *Options; // OPF_OPTIONS Typecast to va_list (defined in stdarg.h)
};

/*****************************************************************************
** Flags for defining fields, methods, actions and functions.  CLASSDEF's can only be used in field definitions for
** classes.  FUNCDEF's can only be used in argument definitions for methods, actions and functions.
*/

#undef FD_READ
#undef FD_WRITE

#ifdef _LP64     // LP64 means that that we're on 64-bit platform
#define FD_PTR64 FD_POINTER
#else
#define FD_PTR64 0
#endif

// Field flags for classes.  These are intended to simplify field definitions, e.g. using FDF_BYTEARRAY combines
// FD_ARRAY with FD_BYTE.  DO NOT use these for function definitions, they are not intended to be compatible.

// Sizes/Types

#define FT_POINTER  FD_POINTER
#define FT_FLOAT    FD_FLOAT
#define FT_LONG     FD_LONG
#define FT_DOUBLE   FD_DOUBLE
#define FT_LARGE    FD_LARGE
#define FT_STRING   (FD_POINTER|FD_STRING)
#define FT_UNLISTED FD_UNLISTED
#define FT_VARIABLE FD_VARIABLE

// Class field definitions.  See core.h for all FD definitions.

#define FDF_BYTE       FD_BYTE
#define FDF_WORD       FD_WORD     // Field is word sized (16-bit)
#define FDF_LONG       FD_LONG     // Field is long sized (32-bit)
#define FDF_DOUBLE     FD_DOUBLE   // Field is double floating point sized (64-bit)
#define FDF_LARGE      FD_LARGE    // Field is large sized (64-bit)
#define FDF_POINTER    FD_POINTER  // Field is an address pointer (typically 32-bit)
#define FDF_ARRAY      FD_ARRAY    // Field is a pointer to an array
#define FDF_PTR        FD_POINTER
#define FDF_VARIABLE   FD_VARIABLE
#define FDF_SYNONYM    FD_SYNONYM

#define FDF_UNSIGNED    (FD_UNSIGNED)
#define FDF_FUNCTION    (FD_FUNCTION)           // sizeof(struct rkFunction) - use FDF_FUNCTIONPTR for sizeof(APTR)
#define FDF_FUNCTIONPTR (FD_FUNCTION|FD_POINTER)
#define FDF_STRUCT      (FD_STRUCT)
#define FDF_RESOURCE    (FD_RESOURCE)
#define FDF_OBJECT      (FD_POINTER|FD_OBJECT)   // Field refers to another object
#define FDF_OBJECTID    (FD_LONG|FD_OBJECT)      // Field refers to another object by ID
#define FDF_INTEGRAL    (FD_POINTER|FD_INTEGRAL) // Field refers to an integral object
#define FDF_STRING      (FD_POINTER|FD_STRING)   // Field points to a string.  NB: Ideally want to remove the FD_POINTER as it should be redundant
#define FDF_STR         (FDF_STRING)
#define FDF_PERCENTAGE  FD_PERCENTAGE
#define FDF_FLAGS       FD_FLAGS                // Field contains flags
#define FDF_ALLOC       FD_ALLOC                // Field is a dynamic allocation - either a memory block or object
#define FDF_LOOKUP      FD_LOOKUP               // Lookup names for values in this field
#define FDF_READ        FD_READ                 // Field is readable
#define FDF_WRITE       FD_WRITE                // Field is writeable
#define FDF_INIT        FD_INIT                 // Field can only be written prior to Init()
#define FDF_SYSTEM      FD_SYSTEM
#define FDF_ERROR       (FD_LONG|FD_ERROR)
#define FDF_REQUIRED    FD_REQUIRED
#define FDF_RGB         (FD_RGB|FD_BYTE|FD_ARRAY)
#define FDF_R           (FD_READ)
#define FDF_W           (FD_WRITE)
#define FDF_RW          (FD_READ|FD_WRITE)
#define FDF_RI          (FD_READ|FD_INIT)
#define FDF_I           (FD_INIT)
#define FDF_VIRTUAL     FD_VIRTUAL
#define FDF_LONGFLAGS   (FDF_LONG|FDF_FLAGS)
#define FDF_FIELDTYPES  (FD_LONG|FD_DOUBLE|FD_LARGE|FD_POINTER|FD_VARIABLE|FD_BYTE|FD_ARRAY|FD_FUNCTION)

// These constants have to match the FD* constants << 32

#define TDOUBLE   0x8000000000000000LL
#define TLONG     0x4000000000000000LL
#define TVAR      0x2000000000000000LL
#define TPTR      0x0800000000000000LL
#define TLARGE    0x0400000000000000LL
#define TFUNCTION 0x0200000000000000LL
#define TSTR      0x0080000000000000LL
#define TRELATIVE 0x0020000000000000LL
#define TARRAY    0x0000100000000000LL
#define TFLOAT    TDOUBLE
#define TPERCENT  TRELATIVE
#define TAGEND    0LL
#define TAGDIVERT -1LL
#define TSTRING   TSTR
#define TREL      TRELATIVE

/*****************************************************************************
** Header used for all objects.
*/

struct Head { // Must be 64-bit aligned
   struct rkMetaClass *Class;   // Class pointer, resolved on AccessObject()
   struct Stats *Stats;         // Stats pointer, resolved on AccessObject() [Private]
   APTR  ChildPrivate;          // Address for the ChildPrivate structure, if allocated
   APTR  CreatorMeta;           // The creator (via NewObject) is permitted to store a custom data pointer here.
   CLASSID ClassID;             // Reference to the object's class, used to resolve the Class pointer
   CLASSID SubID;               // Reference to the object's sub-class, used to resolve the Class pointer
   OBJECTID UniqueID;           // Unique object identifier
   OBJECTID OwnerID;            // Refers to the owner of this object
   WORD Flags;                  // Object flags
   WORD MemFlags;               // Recommended memory allocation flags
   OBJECTID TaskID;             // The process that this object belongs to
   volatile LONG  ThreadID;     // Managed by prv_access() and prv_release() - set by get_thread_id()
   #ifdef _WIN32
      WINHANDLE ThreadMsg;      // Pipe for sending messages to the owner thread.
   #else
      LONG ThreadMsg;
   #endif
   UBYTE ThreadPending;         // ActionThread() increments this.
   volatile BYTE Queue;         // Managed by prv_access() and prv_release()
   volatile BYTE SleepQueue;    //
   volatile BYTE Locked;        // Set if locked by AccessObject()/AccessPrivateObject()
   BYTE ActionDepth;            // Incremented each time an action or method is called on the object
} __attribute__ ((aligned (8)));

#define ClassName(a) ((struct rkMetaClass *)(((OBJECTPTR)(a))->Class))->Name

#define OBJECT_HEADER struct Head Head;

#define ResolveAddress(a,b)  ((APTR)(((BYTE *)(a)) + (b)))

#define FreeFromLL(a,b,c) if ((a)->Prev) (a)->Prev->Next = (a)->Next; \
                          if ((a)->Next) (a)->Next->Prev = (a)->Prev; \
                          if ((a) == (b)) { \
                             (c) = (void *)((a)->Next); \
                             if ((a)->Next) (a)->Next->Prev = 0; \
                          } \
                          (a)->Prev = 0; \
                          (a)->Next = 0;

#define nextutf8(str) if (*(str)) for (++(str); (*(str) & 0xc0) IS 0x80; (str)++);

#include <parasol/modules/core.h>

INLINE LONG IntToStr(LARGE Integer, STRING String, LONG StringSize) {
   return StrFormat(String, StringSize, PF64(), Integer);
}

#ifdef  __cplusplus
}
#endif

#endif // PARASOL_MAIN_H
