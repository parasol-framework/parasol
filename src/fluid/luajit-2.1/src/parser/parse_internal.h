// Copyright (C) 2025 Paul Manias
//
// NOT TO BE INCLUDED OUTSIDE THE PARSER.

#pragma once

#include <iterator>
#include <ranges>
#include <string_view>
#include <cstdint>

enum class TokenKind : uint16_t;

// Constants (lj_parse_constants.cpp)

// Exported for use by OperatorEmitter facade
extern BCReg const_num(FuncState *, ExpDesc* e);
extern BCReg const_str(FuncState *, ExpDesc* e);

static BCReg const_gc(FuncState *, GCobj* gc, uint32_t itype);

// Jump list handling (lj_parse_constants.cpp)

class JumpListView : public std::ranges::view_interface<JumpListView> {
public:
   class Iterator {
   public:
      using iterator_category = std::forward_iterator_tag;
      using difference_type = ptrdiff_t;
      using value_type = BCPos;

      Iterator(FuncState *State, BCPos Position) : func_state(State), position(Position) { }

      [[nodiscard]] BCPos operator*() const { return this->position; }
      Iterator& operator++() { this->position = next(this->func_state, this->position); return *this; }
      [[nodiscard]] bool operator==(const Iterator& Other) const { return position IS Other.position; }
      [[nodiscard]] bool operator!=(const Iterator& Other) const { return not(position IS Other.position); }

      private:
      FuncState* func_state;
      BCPos position;
   };

   JumpListView(FuncState* State, BCPos Head) : func_state(State), list_head(Head) { }

   [[nodiscard]] inline Iterator begin() const { return Iterator(func_state, list_head); }
   [[nodiscard]] inline Iterator end() const { return Iterator(func_state, NO_JMP); }
   [[nodiscard]] inline bool empty() const { return list_head IS NO_JMP; }
   [[nodiscard]] inline BCPos head() const { return list_head; }
   [[nodiscard]] inline BCPos next(BCPos Position) const { return next(func_state, Position); }

   // Enable range-based algorithms via ADL
   friend auto begin(const JumpListView& v) { return v.begin(); }
   friend auto end(const JumpListView& v) { return v.end(); }

   [[nodiscard]] static inline BCPos next(FuncState* State, BCPos Position) {
      ptrdiff_t delta = bc_j(State->bcbase[Position].ins);
      if (BCPos(delta) IS NO_JMP) return NO_JMP;
      return BCPos((ptrdiff_t(Position) + 1) + delta);
   }

   [[nodiscard]] bool produces_values() const;
   [[nodiscard]] bool patch_test_register(BCPos Position, BCReg Register) const;
   void drop_values() const;
   [[nodiscard]] BCPos append(BCPos Other) const;
   void patch_with_value(BCPos ValueTarget, BCReg Register, BCPos DefaultTarget) const;
   void patch_to_here() const;
   void patch_to(BCPos Target) const;
   void patch_head(BCPos Destination) const;

private:
   void patch_instruction(BCPos Position, BCPos Destination) const;

   FuncState* func_state;
   BCPos list_head;
};

// Consume a flag from an expression, clearing it and returning whether it was set.
// Use this when an operator takes ownership of a flagged value.

[[nodiscard]] static inline bool expr_consume_flag(ExpDesc* Expression, ExprFlag Flag)
{
   if (has_flag(Expression->flags, Flag)) {
      Expression->flags &= ~Flag;
      return true;
   }
   return false;
}

// Register allocation (lj_parse_regalloc.cpp)

static void bcreg_bump(FuncState *, BCReg n);
static void bcreg_reserve(FuncState *, BCReg n);
static void bcreg_free(FuncState *, BCReg reg);
static void expr_free(FuncState *, ExpDesc* e);

// Bytecode emission (lj_parse_regalloc.cpp)

// Exported for use by OperatorEmitter facade
extern BCPos bcemit_INS(FuncState *, BCIns ins);

// Bytecode emission helper functions.

template<typename Op>
static inline BCPos bcemit_ABC(FuncState *fs, Op o, BCReg a, BCReg b, BCReg c) {
   return bcemit_INS(fs, BCINS_ABC(o, a, b, c));
}

template<typename Op>
static inline BCPos bcemit_AD(FuncState *fs, Op o, BCReg a, BCReg d) {
   return bcemit_INS(fs, BCINS_AD(o, a, d));
}

template<typename Op>
static inline BCPos bcemit_AJ(FuncState *fs, Op o, BCReg a, BCPos j) {
   return bcemit_INS(fs, BCINS_AJ(o, a, j));
}

static void expr_discharge(FuncState *, ExpDesc* e);
static void bcemit_nil(FuncState *, BCReg from, BCReg n);
static void expr_toreg_nobranch(FuncState *, ExpDesc* e, BCReg reg);
static void expr_toreg(FuncState *, ExpDesc* e, BCReg reg);
static void expr_tonextreg(FuncState *, ExpDesc* e);
static BCReg expr_toanyreg(FuncState *, ExpDesc* e);
static void expr_toval(FuncState *, ExpDesc* e);
static void bcemit_store(FuncState *, ExpDesc* var, ExpDesc* e);
static void bcemit_method(FuncState *, ExpDesc* e, ExpDesc* key);
// These are now exported (non-static) for use by OperatorEmitter facade
extern BCPos bcemit_jmp(FuncState *);
extern void invertcond(FuncState *, ExpDesc* e);
extern BCPos bcemit_branch(FuncState *, ExpDesc* e, int cond);

// These remain static (legacy parser only)
static void bcemit_branch_t(FuncState *, ExpDesc* e);

// Operators (lj_parse_operators.cpp)

// These are now exported (non-static) for use by OperatorEmitter facade
extern int foldarith(BinOpr opr, ExpDesc* e1, ExpDesc* e2);
extern void bcemit_arith(FuncState *, BinOpr opr, ExpDesc* e1, ExpDesc* e2);
extern void bcemit_comp(FuncState *, BinOpr opr, ExpDesc* e1, ExpDesc* e2);
extern void bcemit_unop(FuncState *, BCOp op, ExpDesc* e);

// These are internal helpers
static void bcemit_shift_call_at_base(FuncState *, std::string_view fname, ExpDesc* lhs, ExpDesc* rhs, BCReg base);
static void bcemit_bit_call(FuncState *, std::string_view fname, ExpDesc* lhs, ExpDesc* rhs);
extern void bcemit_unary_bit_call(FuncState *, std::string_view fname, ExpDesc* arg);

// Variables and scope (lj_parse_scope.cpp)

[[nodiscard]] static int is_blank_identifier(GCstr* name);
[[nodiscard]] static std::optional<BCReg> var_lookup_local(FuncState *, GCstr* n);
[[nodiscard]] static MSize var_lookup_uv(FuncState *, MSize vidx, ExpDesc* e);
[[nodiscard]] static MSize var_lookup_(FuncState *, GCstr* name, ExpDesc* e, int first);

// Function scope (lj_parse_scope.cpp)

static void fscope_begin(FuncState *, FuncScope* bl, FuncScopeFlag flags);
static void execute_defers(FuncState *, BCReg limit);
static void fscope_end(FuncState *);
static void fscope_uvmark(FuncState *, BCReg level);

#include "parse_raii.h"
#include "parse_regalloc.h"
#include "parse_concepts.h"

// Function state (lj_parse_scope.cpp)

static void fs_fixup_bc(FuncState *, GCproto* pt, BCIns* bc, MSize n);
static void fs_fixup_uv2(FuncState *, GCproto* pt);
static void fs_fixup_k(FuncState *, GCproto* pt, void* kptr);
static void fs_fixup_uv1(FuncState *, GCproto* pt, uint16_t* uv);
[[nodiscard]] static size_t fs_prep_line(FuncState *, BCLine numline);
static void fs_fixup_line(FuncState *, GCproto* pt, void* lineinfo, BCLine numline);
[[nodiscard]] static int bcopisret(BCOp op);
static void fs_fixup_ret(FuncState *);

// Expressions (lj_parse_expr.cpp)

static void expr_index(FuncState *, ExpDesc* t, ExpDesc* e);
static void expr_kvalue(FuncState *, TValue* v, ExpDesc* e);

// Statements (lj_parse_stmt.cpp)

static void snapshot_return_regs(FuncState *, BCIns* ins);
