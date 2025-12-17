// Miscellaneous object handling.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h

#define lj_obj_c
#define LUA_CORE

#include "lj_obj.h"
#include <parasol/main.h>

//********************************************************************************************************************
// Object type names.

LJ_DATADEF CSTRING const lj_obj_typename[] = {  // ORDER LUA_T
  "no value", "nil", "boolean", "userdata", "number", "string",
  "table", "function", "userdata", "thread", "proto", "cdata", "array"
};

LJ_DATADEF CSTRING const lj_obj_itypename[] = {  // ORDER LJ_T
  "nil", "boolean", "boolean", "userdata", "string", "upval", "thread",
  "proto", "function", "trace", "cdata", "table", "userdata", "array", "number"
};

//********************************************************************************************************************
// Compare two objects without calling metamethods.

int LJ_FASTCALL lj_obj_equal(cTValue *LHS, cTValue *RHS)
{
   if (itype(LHS) IS itype(RHS)) {
      if (tvispri(LHS)) return 1;
      if (!tvisnum(LHS)) return gcrefeq(LHS->gcr, RHS->gcr);
   }
   else if (!tvisnumber(LHS) or !tvisnumber(RHS)) return 0;
   return numberVnum(LHS) IS numberVnum(RHS);
}

//********************************************************************************************************************
// Return pointer to object or its object data.

const void * LJ_FASTCALL lj_obj_ptr(global_State *g, cTValue *o)
{
   if (tvisudata(o)) return uddata(udataV(o));
   else if (tvislightud(o)) return lightudV(g, o);
   else if (LJ_HASFFI and tviscdata(o)) return cdataptr(cdataV(o));
   else if (tvisarray(o)) return arrayV(o)->arraydata();
   else if (tvisgcv(o)) return gcV(o);
   else return nullptr;
}

//********************************************************************************************************************

[[nodiscard]] int GCarray::type_flags() const noexcept 
{
   switch(elemtype) {
      case AET::_BYTE:       return FD_BYTE;
      case AET::_INT16:      return FD_WORD;
      case AET::_INT32:      return FD_INT;
      case AET::_INT64:      return FD_INT64;
      case AET::_FLOAT:      return FD_FLOAT;
      case AET::_DOUBLE:     return FD_DOUBLE;
      case AET::_PTR:        return FD_POINTER;
      case AET::_CSTRING:    return FD_STRING;
      case AET::_STRING_CPP: return FD_STRING | FD_CPP;
      case AET::_STRING_GC:  return FD_STRING;
      case AET::_STRUCT:     return FD_STRUCT;
      default:               return 0;
   }
}
