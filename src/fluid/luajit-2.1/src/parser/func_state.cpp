
#include "func_state.h"
#include "lexer.h"

[[nodiscard]] VarInfo & FuncState::var_get(int32_t Slot) {
   lj_assertX(Slot >= 0 and Slot < int32_t(varmap.size()), "variable slot out of range");
   return ls->vstack[varmap[Slot]];
}

[[nodiscard]] const VarInfo& FuncState::var_get(int32_t Slot) const {
   lj_assertX(Slot >= 0 and Slot < int32_t(varmap.size()), "variable slot out of range");
   return ls->vstack[varmap[Slot]];
}
