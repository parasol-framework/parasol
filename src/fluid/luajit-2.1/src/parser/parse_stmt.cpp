// Copyright (C) 2025 Paul Manias
// Shared helper functions for AST parser

#include <format>
#include <span>
#include <string>
#include <vector>

#include "parser/parser_context.h"
#include "parser/parse_value.h"
#include "parser/parse_regalloc.h"
#include "parser/parse_control_flow.h"

//********************************************************************************************************************
// Snapshot return register state.
// Used by ir_emitter for return statement handling.

static void snapshot_return_regs(FuncState* fs, BCIns* ins)
{
   BCOp op = bc_op(*ins);

   if (op IS BC_RET1) {
      BCREG src = bc_a(*ins);
      if (src < fs->nactvar) {
         RegisterAllocator allocator(fs);
         BCREG dst = fs->freereg;
         allocator.reserve(1);
         bcemit_AD(fs, BC_MOV, dst, src);
         setbc_a(ins, dst);
      }
   }
   else if (op IS BC_RET) {
      BCREG base = bc_a(*ins);
      BCREG nres = bc_d(*ins);
      BCREG top = base + nres - 1;
      if (top < fs->nactvar) {
         BCREG dst = fs->freereg;
         RegisterAllocator allocator(fs);
         allocator.reserve(nres);
         for (BCREG i = 0; i < nres; ++i) {
            bcemit_AD(fs, BC_MOV, dst + i, base + i);
         }
         setbc_a(ins, dst);
      }
   }
   else if (op IS BC_RETM) {
      BCREG base = bc_a(*ins);
      BCREG nfixed = bc_d(*ins);
      if (base < fs->nactvar) {
         BCREG dst = fs->freereg;
         RegisterAllocator allocator(fs);
         allocator.reserve(nfixed);
         for (BCREG i = 0; i < nfixed; ++i) {
            bcemit_AD(fs, BC_MOV, dst + i, base + i);
         }
         setbc_a(ins, dst);
      }
   }
}
