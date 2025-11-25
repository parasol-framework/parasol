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
// Adjust LHS/RHS of an assignment.
// Used by AST pipeline (ir_emitter.cpp) for assignment statements, local declarations, and for loops.

void LexState::assign_adjust(BCReg nvars, BCReg nexps, ExpDesc *Expr)
{
   FuncState* fs = this->fs;
   RegisterAllocator allocator(fs);
   int32_t extra = int32_t(nvars) - int32_t(nexps);
   if (Expr->k IS ExpKind::Call) {
      extra++;  // Compensate for the ExpKind::Call itself.
      if (extra < 0) extra = 0;
      setbc_b(bcptr(fs, Expr), extra + 1);  // Fixup call results.
      if (extra > 1) allocator.reserve(BCReg(extra) - 1);
   }
   else {
      if (Expr->k != ExpKind::Void) {
         ExpressionValue value(fs, *Expr);
         value.to_next_reg(allocator);
         *Expr = value.legacy();
      }
      if (extra > 0) {  // Leftover LHS are set to nil.
         BCReg reg = fs->freereg;
         allocator.reserve(BCReg(extra));
         bcemit_nil(fs, reg, BCReg(extra));
      }
   }

   if (nexps > nvars) fs->freereg -= nexps - nvars;  // Drop leftover regs.
}

//********************************************************************************************************************
// Summarise accumulated diagnostics when abort_on_error is disabled.
// Used by parser.cpp's flush_non_fatal_errors().

static void raise_accumulated_diagnostics(ParserContext &Context)
{
   auto entries = Context.diagnostics().entries();
   if (entries.empty()) return;

   auto summary = std::format("parser reported {} {}:\n", entries.size(), entries.size() == 1 ? "error" : "errors");

   for (const auto& diagnostic : entries) {
      SourceSpan span = diagnostic.token.span();
      std::string_view message = diagnostic.message.empty() ? "unexpected token" : diagnostic.message;
      summary += std::format("   line {}:{} - {}\n", span.line, span.column, message);
   }

   lua_State *L = &Context.lua();

   // Store diagnostic information in lua_State before throwing
   L->parser_diagnostics = new ParserDiagnostics(Context.diagnostics());

   GCstr *message = lj_str_new(L, summary.data(), summary.size());
   setstrV(L, L->top++, message);
   lj_err_throw(L, LUA_ERRSYNTAX);
}

//********************************************************************************************************************
// Snapshot return register state.
// Used by AST pipeline (ir_emitter.cpp) for return statement handling.

static void snapshot_return_regs(FuncState* fs, BCIns* ins)
{
   BCOp op = bc_op(*ins);

   if (op IS BC_RET1) {
      BCReg src = bc_a(*ins);
      if (src < fs->nactvar) {
         RegisterAllocator allocator(fs);
         BCReg dst = fs->freereg;
         allocator.reserve(1);
         bcemit_AD(fs, BC_MOV, dst, src);
         setbc_a(ins, dst);
      }
   }
   else if (op IS BC_RET) {
      BCReg base = bc_a(*ins);
      BCReg nres = bc_d(*ins);
      BCReg top = base + nres - 1;
      if (top < fs->nactvar) {
         BCReg dst = fs->freereg;
         RegisterAllocator allocator(fs);
         allocator.reserve(nres);
         for (BCReg i = 0; i < nres; ++i) {
            bcemit_AD(fs, BC_MOV, dst + i, base + i);
         }
         setbc_a(ins, dst);
      }
   }
   else if (op IS BC_RETM) {
      BCReg base = bc_a(*ins);
      BCReg nfixed = bc_d(*ins);
      if (base < fs->nactvar) {
         BCReg dst = fs->freereg;
         RegisterAllocator allocator(fs);
         allocator.reserve(nfixed);
         for (BCReg i = 0; i < nfixed; ++i) {
            bcemit_AD(fs, BC_MOV, dst + i, base + i);
         }
         setbc_a(ins, dst);
      }
   }
}
