#pragma once

//  types.h
//
//  (C) Copyright 1996-2022 Paul Manias

// Function structure, typically used for defining callbacks to functions and procedures of any kind (e.g. standard C,
// Fluid).

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

inline bool operator==(const struct rkFunction &A, const struct rkFunction &B)
{
   if (A.Type == CALL_STDC) return (A.Type == B.Type) and (A.StdC.Context == B.StdC.Context) and (A.StdC.Routine == B.StdC.Routine);
   else if (A.Type == CALL_SCRIPT) return (A.Type == B.Type) and (A.Script.Script == B.Script.Script) and (A.Script.ProcedureID == B.Script.ProcedureID);
   else return (A.Type == B.Type);
}
