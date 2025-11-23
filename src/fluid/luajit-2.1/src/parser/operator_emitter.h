// Copyright (C) 2025 Paul Manias
//
// Operator emission facade that translates AST operator payloads into allocator/CFG-aware bytecode emission.
//
// This class manages register allocation via RegisterAllocator and control flow via ControlFlowGraph, eliminating
// direct freereg manipulation.

#pragma once

#include <optional>

#include "parser/parse_types.h"

class RegisterAllocator;
class ControlFlowGraph;

class OperatorEmitter {
public:
   explicit OperatorEmitter(FuncState* State, RegisterAllocator* Allocator, ControlFlowGraph* Cfg);

   // Constant folding for arithmetic operators
   // Returns true if folding succeeded and result is in e1
   [[nodiscard]] bool fold_constant_arith(BinOpr opr, ExpDesc* e1, ExpDesc* e2);

   // Emit unary operator (negate, not, length, bitnot)
   // Accepts operand as ExpDesc, emits bytecode, modifies operand in-place
   void emit_unary(int op, ExpDesc* operand);

   // Prepare left operand for binary operation
   // Must be called BEFORE evaluating right operand
   // Discharges LHS to appropriate form (register for comparisons, numeric constant or register for arithmetic)
   void emit_binop_left(BinOpr opr, ExpDesc* left);

   // Emit arithmetic binary operator (add, sub, mul, div, mod, pow, concat)
   // Accepts left/right operands, emits bytecode, stores result in left
   // Uses constant folding when possible
   void emit_binary_arith(BinOpr opr, ExpDesc* left, ExpDesc* right);

   // Emit comparison operator (eq, ne, lt, le, gt, ge)
   // Emits comparison bytecode with jump, stores jump in left operand
   void emit_comparison(BinOpr opr, ExpDesc* left, ExpDesc* right);

   // Emit bitwise binary operator (band, bor, bxor, shl, shr)
   // Treated as arithmetic operators with integer coercion
   void emit_binary_bitwise(BinOpr opr, ExpDesc* left, ExpDesc* right);

   // Logical short-circuit operators - preparation phase
   // These are called BEFORE evaluating RHS to set up short-circuit jumps
   // CFG-based implementation with structured control flow edges
   void prepare_logical_and(ExpDesc* left);
   void prepare_logical_or(ExpDesc* left);
   void prepare_if_empty(ExpDesc* left);

   // Logical short-circuit operators - completion phase
   // These are called AFTER evaluating RHS to complete the operation
   // CFG-based implementation with edge merging and result handling
   void complete_logical_and(ExpDesc* left, ExpDesc* right);
   void complete_logical_or(ExpDesc* left, ExpDesc* right);
   void complete_if_empty(ExpDesc* left, ExpDesc* right);

   // CONCAT operator - preparation phase
   // Called BEFORE evaluating RHS to discharge left to consecutive register
   void prepare_concat(ExpDesc* left);

   // CONCAT operator - completion phase
   // Called AFTER evaluating RHS to emit BC_CAT instruction with chaining support
   void complete_concat(ExpDesc* left, ExpDesc* right);

private:
   FuncState* func_state;
   RegisterAllocator* allocator;
   ControlFlowGraph* cfg;

   // Internal helper to discharge operands for arithmetic operations
   void discharge_arith_operands(BinOpr opr, ExpDesc* e1, ExpDesc* e2, BCReg& rb, BCReg& rc, uint32_t& op);
};
