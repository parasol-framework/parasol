/*
** Value category abstractions for Phase 4 parser modernisation.
** Copyright (C) 2025 Parasol Project.
*/

#ifndef VALUE_CATEGORIES_H
#define VALUE_CATEGORIES_H

#include "parse_types.h"

struct FuncState;
class RegisterAllocator;

//********************************************************************************************************************
// ValueUse - Read-only value wrapper
//
// Represents a value being read for consumption in an operation. This is a lightweight wrapper around ExpDesc that
// provides a value-oriented API for reading values without modification.
//
// Value categories:
// - ConstantValue: Compile-time constants (nil, boolean, number, string, cdata)
// - RegisterValue: Value in a register (local, temp result)
// - IndexedValue: Table slot requiring table and key registers
// - GlobalValue: Global variable requiring name constant
//
// This class does NOT own the underlying ExpDesc and assumes the caller manages its lifetime.
//********************************************************************************************************************

class ValueUse {
public:
   // Construct from existing ExpDesc pointer (non-owning)
   explicit ValueUse(ExpDesc* Desc) : desc(Desc) {}

   // Query value category
   [[nodiscard]] bool is_constant() const { return expr_isk(this->desc); }
   [[nodiscard]] bool is_nil() const { return this->desc->k IS ExpKind::Nil; }
   [[nodiscard]] bool is_false() const { return this->desc->k IS ExpKind::False; }
   [[nodiscard]] bool is_true() const { return this->desc->k IS ExpKind::True; }
   [[nodiscard]] bool is_string() const { return this->desc->k IS ExpKind::Str; }
   [[nodiscard]] bool is_number() const { return this->desc->k IS ExpKind::Num; }
   [[nodiscard]] bool is_local() const { return this->desc->k IS ExpKind::Local; }
   [[nodiscard]] bool is_upvalue() const { return this->desc->k IS ExpKind::Upval; }
   [[nodiscard]] bool is_global() const { return this->desc->k IS ExpKind::Global; }
   [[nodiscard]] bool is_indexed() const { return this->desc->k IS ExpKind::Indexed; }
   [[nodiscard]] bool is_register() const {
      return this->desc->k IS ExpKind::Local or this->desc->k IS ExpKind::NonReloc;
   }

   // Extended falsey check (nil, false, 0, "")
   // Note: Supports Fluid's extended falsey semantics for ?? operator
   [[nodiscard]] bool is_falsey() const;

   // Access underlying ExpDesc (for interop with legacy code)
   [[nodiscard]] ExpDesc* raw() const { return this->desc; }

   // Get raw ExpKind
   [[nodiscard]] ExpKind kind() const { return this->desc->k; }

private:
   ExpDesc* desc;
};

//********************************************************************************************************************
// ValueSlot - Write target wrapper
//
// Represents a destination for storing a computed value. This is also a lightweight wrapper around ExpDesc but with
// semantics oriented toward writing/storing values.
//
// Value categories:
// - LocalSlot: Local variable register
// - TempSlot: Temporary register (RAII-released via RegisterAllocator)
// - UpvalueSlot: Upvalue index
// - IndexedSlot: Table slot with table+key registers
// - GlobalSlot: Global variable name constant
//
// This class does NOT own the underlying ExpDesc and assumes the caller manages its lifetime.
//********************************************************************************************************************

class ValueSlot {
public:
   // Construct from existing ExpDesc pointer (non-owning)
   explicit ValueSlot(ExpDesc* Desc) : desc(Desc) {}

   // Query slot category
   [[nodiscard]] bool is_local() const { return this->desc->k IS ExpKind::Local; }
   [[nodiscard]] bool is_upvalue() const { return this->desc->k IS ExpKind::Upval; }
   [[nodiscard]] bool is_global() const { return this->desc->k IS ExpKind::Global; }
   [[nodiscard]] bool is_indexed() const { return this->desc->k IS ExpKind::Indexed; }
   [[nodiscard]] bool is_temp() const {
      // Temps are typically NonReloc or Relocable results
      return this->desc->k IS ExpKind::NonReloc or this->desc->k IS ExpKind::Relocable;
   }

   // Access underlying ExpDesc (for interop with legacy code)
   [[nodiscard]] ExpDesc* raw() const { return this->desc; }

   // Get raw ExpKind
   [[nodiscard]] ExpKind kind() const { return this->desc->k; }

private:
   ExpDesc* desc;
};

//********************************************************************************************************************
// LValue - Assignment target descriptor
//
// Represents an assignable location for statements. Unlike ValueUse/ValueSlot which are thin wrappers, LValue is a
// more structured descriptor that can represent complex assignment targets.
//
// Value categories:
// - LocalLValue: Local variable
// - UpvalueLValue: Upvalue
// - IndexedLValue: Table slot (table expression + key expression)
// - MemberLValue: Table member (table expression + constant key)
// - GlobalLValue: Global variable
//
// LValue is designed for statement emission (assignments, compound assignments) where we need to both read current
// values and write new values to the same location.
//********************************************************************************************************************

enum class LValueKind : uint8_t {
   Local,      // Local variable
   Upvalue,    // Upvalue
   Global,     // Global variable
   Indexed,    // Table slot (computed key)
   Member      // Table member (constant string key)
};

class LValue {
public:
   // Construct local l-value
   static LValue make_local(BCReg Register) {
      LValue lval;
      lval.kind = LValueKind::Local;
      lval.local_reg = Register;
      return lval;
   }

   // Construct upvalue l-value
   static LValue make_upvalue(uint32_t Index) {
      LValue lval;
      lval.kind = LValueKind::Upvalue;
      lval.upvalue_index = Index;
      return lval;
   }

   // Construct global l-value
   static LValue make_global(GCstr* Name) {
      LValue lval;
      lval.kind = LValueKind::Global;
      lval.global_name = Name;
      return lval;
   }

   // Construct indexed l-value (table[key])
   static LValue make_indexed(BCReg TableReg, BCReg KeyReg) {
      LValue lval;
      lval.kind = LValueKind::Indexed;
      lval.indexed.table_reg = TableReg;
      lval.indexed.key_reg = KeyReg;
      return lval;
   }

   // Construct member l-value (table.member or table["constant"])
   static LValue make_member(BCReg TableReg, uint32_t KeyConst) {
      LValue lval;
      lval.kind = LValueKind::Member;
      lval.member.table_reg = TableReg;
      lval.member.key_const = KeyConst;
      return lval;
   }

   // Create l-value from ExpDesc (conversion utility)
   static LValue from_expdesc(const ExpDesc* Desc);

   // Query l-value kind
   [[nodiscard]] LValueKind get_kind() const { return this->kind; }
   [[nodiscard]] bool is_local() const { return this->kind IS LValueKind::Local; }
   [[nodiscard]] bool is_upvalue() const { return this->kind IS LValueKind::Upvalue; }
   [[nodiscard]] bool is_global() const { return this->kind IS LValueKind::Global; }
   [[nodiscard]] bool is_indexed() const { return this->kind IS LValueKind::Indexed; }
   [[nodiscard]] bool is_member() const { return this->kind IS LValueKind::Member; }

   // Accessors for variant data
   [[nodiscard]] BCReg get_local_reg() const { return this->local_reg; }
   [[nodiscard]] uint32_t get_upvalue_index() const { return this->upvalue_index; }
   [[nodiscard]] GCstr* get_global_name() const { return this->global_name; }
   [[nodiscard]] BCReg get_table_reg() const {
      return this->is_indexed() ? this->indexed.table_reg : this->member.table_reg;
   }
   [[nodiscard]] BCReg get_key_reg() const { return this->indexed.key_reg; }
   [[nodiscard]] uint32_t get_key_const() const { return this->member.key_const; }

private:
   LValue() = default;

   LValueKind kind;

   union {
      BCReg local_reg;
      uint32_t upvalue_index;
      GCstr* global_name;
      struct {
         BCReg table_reg;
         BCReg key_reg;
      } indexed;
      struct {
         BCReg table_reg;
         uint32_t key_const;
      } member;
   };
};

#endif
