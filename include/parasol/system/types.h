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

#ifdef  __cplusplus
}
#endif

#endif  // SYSTEM_TYPES_H
