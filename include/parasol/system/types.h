#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

//  types.h
//
//  (C) Copyright 1996-2020 Paul Manias

#ifdef  __cplusplus
extern "C" {
#endif

struct CoreBase;

/*****************************************************************************
** Function structure, typically used for defining callbacks to functions and procedures of any kind (e.g. standard C,
** Fluid).
*/

enum {
   CALL_NONE=0,
   CALL_STDC,
   CALL_SCRIPT
};

typedef struct rkFunction {
   unsigned char Type;
   unsigned char PadA;
   unsigned short ID; // Unused.  Unique identifier for the function.
   union {
      struct {
         OBJECTPTR Context;
         void * Routine;
      } StdC;

      struct {
         OBJECTPTR Script;
         LARGE ProcedureID; // Function identifier, usually a hash
      } Script;
   };
} rkFunction, FUNCTION;

#ifdef __cplusplus
INLINE  bool operator==(const struct rkFunction &A, const struct rkFunction &B)
{
   if (A.Type == CALL_STDC) return (A.Type == B.Type) and (A.StdC.Context == B.StdC.Context) and (A.StdC.Routine == B.StdC.Routine);
   else if (A.Type == CALL_SCRIPT) return (A.Type == B.Type) and (A.Script.Script == B.Script.Script) and (A.Script.ProcedureID == B.Script.ProcedureID);
   else return (A.Type == B.Type);
}
#endif

#define SET_FUNCTION_STDC(call, func)           (call).Type = CALL_STDC;   (call).StdC.Routine = (func); (call).StdC.Context = CurrentContext();
#define SET_FUNCTION_SCRIPT(call, script, proc) (call).Type = CALL_SCRIPT; (call).Script.Script = (script);  (call).Script.ProcedureID = proc;

#ifdef  __cplusplus
}
#endif

#endif  // SYSTEM_TYPES_H
