/*
** Value category abstractions - implementation.
** Copyright (C) 2025 Parasol Project.
*/

#include "../runtime/lj_obj.h"
#include "../bytecode/lj_bc.h"
#include "lj_lex.h"
#include "value_categories.h"

//********************************************************************************************************************
// ValueUse implementation
//********************************************************************************************************************

bool ValueUse::is_falsey() const
{
   // Extended falsey semantics for Fluid's ?? operator:
   // - nil is falsey
   // - false is falsey
   // - 0 (numeric zero) is falsey
   // - "" (empty string) is falsey
   // All other values are truthy

   switch (this->desc->k) {
      case ExpKind::Nil:
      case ExpKind::False:
         return true;

      case ExpKind::True:
         return false;

      case ExpKind::Num: {
         // Check if number is exactly zero
         TValue* nval = &this->desc->u.nval;
         if (tvisint(nval)) {
            return intV(nval) IS 0;
         } else if (tvisnum(nval)) {
            return numV(nval) IS 0.0;
         }
         return false;
      }

      case ExpKind::Str: {
         // Check if string is empty
         GCstr* str = this->desc->u.sval;
         return str->len IS 0;
      }

      default:
         // Non-constant expressions cannot be determined at compile time
         // Return false (conservative - assume truthy)
         return false;
   }
}

//********************************************************************************************************************
// LValue implementation
//********************************************************************************************************************

LValue LValue::from_expdesc(const ExpDesc* Desc)
{
   switch (Desc->k) {
      case ExpKind::Local:
         return LValue::make_local(BCReg(Desc->u.s.info));

      case ExpKind::Upval:
         return LValue::make_upvalue(Desc->u.s.info);

      case ExpKind::Global:
         return LValue::make_global(Desc->u.sval);

      case ExpKind::Indexed: {
         // ExpDesc indexed format:
         // - info = table register
         // - aux = index register/byte/string constant (varies by indexed type)
         // For now, assume aux is a register (simplest case)
         // TODO: Handle VKINDEX variants (byte key, string constant key)
         return LValue::make_indexed(BCReg(Desc->u.s.info), BCReg(Desc->u.s.aux));
      }

      default:
         // Unsupported ExpKind for l-value (e.g., constants, relocable, void)
         // This is a programming error - caller should validate before conversion
         // For now, return a dummy local to avoid undefined behaviour
         return LValue::make_local(0);
   }
}

