// Value category abstractions for Phase 4 parser modernisation.
// Copyright (C) 2025 Paul Manias

#pragma once

#include <variant>

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

class ValueUse {
public:
   // Construct from existing ExpDesc pointer (non-owning)
   explicit ValueUse(ExpDesc* Desc) : desc(Desc) {}

   // Query value category
   [[nodiscard]] bool is_nil() const { return this->desc->k IS ExpKind::Nil; }
   [[nodiscard]] bool is_false() const { return this->desc->k IS ExpKind::False; }
   [[nodiscard]] bool is_true() const { return this->desc->k IS ExpKind::True; }
   [[nodiscard]] bool is_string() const { return this->desc->k IS ExpKind::Str; }
   [[nodiscard]] bool is_number() const { return this->desc->k IS ExpKind::Num; }
   [[nodiscard]] bool is_local() const { return this->desc->k IS ExpKind::Local; }
   [[nodiscard]] bool is_upvalue() const { return this->desc->k IS ExpKind::Upval; }
   [[nodiscard]] bool is_global() const { return this->desc->k IS ExpKind::Global; }
   [[nodiscard]] bool is_indexed() const { return this->desc->k IS ExpKind::Indexed; }
   [[nodiscard]] bool is_register() const { return this->desc->k IS ExpKind::Local or this->desc->k IS ExpKind::NonReloc; }

   // Extended falsey check (nil, false, 0, "")
   // Supports Fluid's extended falsey semantics for ?? operator
   [[nodiscard]] bool is_falsey() const;

   // Access underlying ExpDesc (for interop with legacy code)
   [[nodiscard]] ExpDesc* raw() const { return this->desc; }

   // Get raw ExpKind
   [[nodiscard]] ExpKind kind() const { return this->desc->k; }

private:
   ExpDesc *desc;
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

// LValue variant types - strongly typed alternatives for each l-value category
struct LocalLValue { BCReg reg; };
struct UpvalueLValue { uint32_t index; };
struct GlobalLValue { GCstr* name; };
struct IndexedLValue { BCReg table_reg; BCReg key_reg; };
struct MemberLValue { BCReg table_reg; uint32_t key_const; };

class LValue {
public:
   LValue() : data(std::monostate{}) {}

   // Construct local l-value
   static LValue make_local(BCReg Register) {
      LValue lval;
      lval.data = LocalLValue{Register};
      return lval;
   }

   // Construct upvalue l-value
   static LValue make_upvalue(uint32_t Index) {
      LValue lval;
      lval.data = UpvalueLValue{Index};
      return lval;
   }

   // Construct global l-value
   static LValue make_global(GCstr* Name) {
      LValue lval;
      lval.data = GlobalLValue{Name};
      return lval;
   }

   // Construct indexed l-value (table[key])
   static LValue make_indexed(BCReg TableReg, BCReg KeyReg) {
      LValue lval;
      lval.data = IndexedLValue{TableReg, KeyReg};
      return lval;
   }

   // Construct member l-value (table.member or table["constant"])
   static LValue make_member(BCReg TableReg, uint32_t KeyConst) {
      LValue lval;
      lval.data = MemberLValue{TableReg, KeyConst};
      return lval;
   }

   // Create l-value from ExpDesc (conversion utility)
   static LValue from_expdesc(const ExpDesc* Desc);

   // Query l-value kind
   [[nodiscard]] bool is_local() const { return std::holds_alternative<LocalLValue>(this->data); }
   [[nodiscard]] bool is_upvalue() const { return std::holds_alternative<UpvalueLValue>(this->data); }
   [[nodiscard]] bool is_global() const { return std::holds_alternative<GlobalLValue>(this->data); }
   [[nodiscard]] bool is_indexed() const { return std::holds_alternative<IndexedLValue>(this->data); }
   [[nodiscard]] bool is_member() const { return std::holds_alternative<MemberLValue>(this->data); }

   // Accessors for variant data
   [[nodiscard]] BCReg get_local_reg() const { return std::get<LocalLValue>(this->data).reg; }
   [[nodiscard]] uint32_t get_upvalue_index() const { return std::get<UpvalueLValue>(this->data).index; }
   [[nodiscard]] GCstr* get_global_name() const { return std::get<GlobalLValue>(this->data).name; }
   [[nodiscard]] BCReg get_table_reg() const {
      if (this->is_indexed()) return std::get<IndexedLValue>(this->data).table_reg;
      return std::get<MemberLValue>(this->data).table_reg;
   }
   [[nodiscard]] BCReg get_key_reg() const { return std::get<IndexedLValue>(this->data).key_reg; }
   [[nodiscard]] uint32_t get_key_const() const { return std::get<MemberLValue>(this->data).key_const; }

private:
   std::variant<std::monostate, LocalLValue, UpvalueLValue, GlobalLValue, IndexedLValue, MemberLValue> data;
};
