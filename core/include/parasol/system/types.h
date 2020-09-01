#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

#ifdef  __cplusplus
extern "C" {
#endif

//  types.h
//
//  (C) Copyright 1996-2015 Paul Manias

#ifdef __CYGWIN__
#define _WIN32 1
#undef __unix__
#undef __unix
#endif

#if defined(_MSC_VER)
#define INLINE static __inline
#else
#define INLINE static inline
#endif

#define THREADVAR __thread

struct CoreBase;

/*****************************************************************************
** Standard data types.
*/

#ifdef _WIN32
typedef void * HOSTHANDLE; // Windows defines a pointer type for native handles
#else
typedef int HOSTHANDLE;    // Unix systems use FD descriptors for handles
#endif

#ifndef XMD_H
#if !defined(_WINDEF_H) && !defined(_WINDEF_)
typedef char               BYTE;     // Signed 8-bit quantity
#endif
#endif

typedef unsigned char      UBYTE;    // Unsigned 8-bit quantity
typedef unsigned short     UWORD;    // Unsigned 16-bit quantity

#if !defined(_WINDEF_H) && !defined(_WINDEF_)
typedef short              WORD;     // Signed 16-bit quantity
typedef int                LONG;     // Signed 32-bit quantity
typedef unsigned int       ULONG;    // Unsigned 32-bit quantity
#endif

typedef float              FLOAT;    // Signed 32-bit floating point
typedef double             DOUBLE;   // Signed 64-bit floating point

/* If your compiler does not accept 'long long' definitions, try changing the
** LARGE type setting to __int64.
*/

#if defined(_MSC_VER)
typedef __int64            LARGE;
#else
typedef long long          LARGE;    // Signed 64-bit quantity
#endif

#ifdef _WIN32
typedef void * WINHANDLE;
#else
typedef void * WINHANDLE;
#endif

#undef MAXINT

#if defined(_LP64) || defined(__x86_64__)
typedef LARGE MAXINT;
 #if defined(_MSC_VER)
typedef unsigned __int64   UMAXINT;
 #else
typedef unsigned long long UMAXINT;
 #endif
#else
typedef int MAXINT;
typedef unsigned int UMAXINT;
#endif

/*****************************************************************************
** Pointer types.
*/

#define L64PTR(a)        (APTR)((MAXINT)(a))  // Converts a 64-bit integer to a pointer
#define PTRL64(a)        (LARGE)((MAXINT)(a)) // Converts a pointer to an integer

typedef void * TIMER;
typedef const void * CPTR;  // Pointer with read-only content
typedef void * APTR;        // 32 or 64 bit untyped pointer
#ifdef __cplusplus
typedef char * STRING;   // string pointer (NULL terminated)
typedef char * SSTRING;  // signed string pointer (NULL terminated)
typedef const char * CSTRING;
typedef const char * CSSTRING; // Obsolete
#else
typedef char * STRING;   // string pointer (NULL terminated)
typedef char * STRING;   // string pointer (NULL terminated)
typedef char * SSTRING;  // signed string pointer (NULL terminated)
typedef const char * CSTRING;
typedef const char * CSSTRING; // Obsolete
#endif
typedef void * OBJECT;           // Object pointer
typedef struct Head * OBJECTPTR; // Object pointer
typedef LARGE EVENTID;

#if _WIN32
typedef void * APTR64;    // 64-bit untyped pointer
typedef char * STRING64;  // 64-bit string pointer (NULL terminated)
typedef void * OBJECT64;  // 64-bit untyped object pointer
typedef struct Head * OBJECTPTR64; // 64-bit typed object pointer
#else
typedef void * APTR64 __attribute__ ((aligned (8)));    // 64-bit untyped pointer
typedef char * STRING64 __attribute__ ((aligned (8)));  // 64-bit string pointer (NULL terminated)
typedef void * OBJECT64 __attribute__ ((aligned (8)));  // 64-bit untyped object pointer
typedef struct Head * OBJECTPTR64 __attribute__ ((aligned (8))); // 64-bit typed object pointer
#endif

/*****************************************************************************
** System specific types.
*/

#undef ERROR
#undef OBJECTID
typedef LONG ERROR;     // Standard error code
#ifndef __OBJECTID_DEFINED
typedef LONG OBJECTID;  // 32-bit object ID
#endif
typedef LONG MEMORYID;  // 32-bit memory ID
typedef ULONG CLASSID;  // 32-bit class ID
typedef LONG ACTIONID;  // 32-bit action ID
typedef LARGE FIELD;    // 64-bit field ID - used for flag combos

/*****************************************************************************
** Function structure, typically used for defining callbacks to functions and procedures of any kind (e.g. standard C,
** Fluid, DML).
*/

enum {
   CALL_NONE=0,
   CALL_STDC,
   CALL_SCRIPT
};

typedef struct rkFunction {
   unsigned char Type;
   unsigned char PadA;
   unsigned short ID; // Unique identifier for the function.
   union {
      struct {
         void * Context;
         void * Routine;
      } StdC;

      struct {
         OBJECTPTR Script;
         LARGE ProcedureID; // Function identifier, usually a hash
      } Script;
   };
} rkFunction, FUNCTION;

#define SET_FUNCTION_STDC(call, func)           (call).Type = CALL_STDC;   (call).StdC.Routine = (func); (call).StdC.Context = CurrentContext();
#define SET_FUNCTION_SCRIPT(call, script, proc) (call).Type = CALL_SCRIPT; (call).Script.Script = (script);  (call).Script.ProcedureID = proc;

/*****************************************************************************
** Special keyword definitions.
*/

#undef NULL
#define NULL 0

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef IS
#define IS ==
#endif

#ifndef AND
#define AND &&
#endif

#ifndef OR
#define OR ||
#endif

#ifndef YES
#define YES TRUE
#endif

#ifndef NO
#define NO FALSE
#endif

#ifdef  __cplusplus
}
#endif

#endif  // SYSTEM_TYPES_H
