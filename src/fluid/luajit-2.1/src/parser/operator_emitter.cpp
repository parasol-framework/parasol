// Copyright (C) 2025 Paul Manias

#define LUA_CORE

#include "lj_obj.h"
#include "bytecode/lj_bc.h"
#include "parser/lj_lex.h"

#include <parasol/main.h>

#include "parser/operator_emitter.h"
#include "parser/parse_internal.h"

// Legacy helpers are declared as extern in parse_internal.h

OperatorEmitter::OperatorEmitter(FuncState* State, RegisterAllocator* Allocator, ControlFlowGraph* Cfg)
   : func_state(State), allocator(Allocator), cfg(Cfg)
{
}

//********************************************************************************************************************
// Constant folding for arithmetic operators
// Facade wrapper over legacy foldarith() function

bool OperatorEmitter::fold_constant_arith(BinOpr opr, ExpDesc* e1, ExpDesc* e2)
{
   return foldarith(opr, e1, e2) != 0;
}

//********************************************************************************************************************
// Emit unary operator
// Facade wrapper over legacy bcemit_unop() function

void OperatorEmitter::emit_unary(int op, ExpDesc* operand)
{
   bcemit_unop(this->func_state, BCOp(op), operand);
}

//********************************************************************************************************************
// Emit arithmetic binary operator
// Facade wrapper over legacy bcemit_arith() function

void OperatorEmitter::emit_binary_arith(BinOpr opr, ExpDesc* left, ExpDesc* right)
{
   bcemit_arith(this->func_state, opr, left, right);
}

//********************************************************************************************************************
// Emit comparison operator
// Facade wrapper over legacy bcemit_comp() function

void OperatorEmitter::emit_comparison(BinOpr opr, ExpDesc* left, ExpDesc* right)
{
   bcemit_comp(this->func_state, opr, left, right);
}

//********************************************************************************************************************
// Emit bitwise binary operator
// Bitwise operators use the same emission as arithmetic operators

void OperatorEmitter::emit_binary_bitwise(BinOpr opr, ExpDesc* left, ExpDesc* right)
{
   // Bitwise operators use the same emission as arithmetic operators
   bcemit_arith(this->func_state, opr, left, right);
}
