#pragma once

//  types.h
//
//  (C) Copyright 1996-2023 Paul Manias

#include <type_traits>
#include <utility>

//********************************************************************************************************************

template <class Tag, typename T>
class strong_typedef {
   public:
      // Constructors
      strong_typedef() : val() { }
      constexpr explicit strong_typedef(const T &Value) : val(Value) { }

      // Accessors
      explicit operator T&() noexcept { return val; }
      explicit operator const T&() const noexcept { return val; }

   private:
      T val;
};

struct SCALE : strong_typedef<SCALE, DOUBLE> {
    // Make constructors available
    using strong_typedef::strong_typedef;
};

//********************************************************************************************************************
// Function structure, typically used for defining callbacks to functions and procedures of any kind (e.g. standard C,
// Fluid).

enum {
   CALL_NONE=0,
   CALL_STDC,
   CALL_SCRIPT
};

struct FUNCTION {
   unsigned char Type;
   unsigned char PadA;
   unsigned short ID; // Unused.  Unique identifier for the function.
   OBJECTPTR Context; // The context at the time the function was created, or a Script reference
   void * Meta;       // Additional meta data provided by the client.
   union {
      void * Routine;    // CALL_STDC: Pointer to a C routine
      LARGE ProcedureID; // CALL_SCRIPT: Function identifier, usually a hash
   };

   FUNCTION() : Type(0) { }

   template <class T> FUNCTION(T *pRoutine);
   template <class T> FUNCTION(T *pRoutine, OBJECTPTR pContext, APTR pMeta);
   template <class T> FUNCTION(T *pRoutine, APTR pMeta);

   FUNCTION(class objScript *pScript, LARGE pProcedure) {
      Type = CALL_SCRIPT;
      Context = (OBJECTPTR)pScript;
      ProcedureID = pProcedure;
   }

   void clear() { Type = CALL_NONE; }
   bool isC() const { return Type IS CALL_STDC; }
   bool isScript() const { return Type IS CALL_SCRIPT; }
   bool defined() const { return Type != CALL_NONE; }
};

inline bool operator==(const struct FUNCTION &A, const struct FUNCTION &B)
{
   if (A.Type == CALL_STDC) return (A.Type == B.Type) and (A.Context == B.Context) and (A.Routine == B.Routine);
   else if (A.Type == CALL_SCRIPT) return (A.Type == B.Type) and (A.Context == B.Context) and (A.ProcedureID == B.ProcedureID);
   else return (A.Type == B.Type);
}
