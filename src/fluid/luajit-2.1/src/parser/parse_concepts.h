// C++20 Concepts for LuaJIT Parser Type Constraints
// Copyright (C) 2025 Paul Manias

#pragma once

#include <concepts>
#include <type_traits>

// ExpressionDescriptor: Concept for expression descriptor types
//
// Ensures a type has the required members for expression handling.
// This provides compile-time validation and better error messages.

template<typename T>
concept ExpressionDescriptor = requires(T e) {
   { e.k } -> std::convertible_to<ExpKind>;
   { e.t } -> std::convertible_to<BCPos>;
   { e.f } -> std::convertible_to<BCPos>;
   requires sizeof(e.u) > 0;  // Has union member
};

// ConstExpressionDescriptor: Stricter concept for const expression access

template<typename T>
concept ConstExpressionDescriptor = ExpressionDescriptor<T> and requires(const T* e) {
   { e->k } -> std::convertible_to<ExpKind>;
   { e->t } -> std::convertible_to<BCPos>;
   { e->f } -> std::convertible_to<BCPos>;
};

// RegisterType: Concept for bytecode register types
// Ensures type is compatible with BCReg operations.

template<typename T>
concept RegisterType = std::same_as<T, BCReg> or std::is_convertible_v<T, BCReg>;

// UnsignedRegisterType: Stricter concept for unsigned register operations

template<typename T>
concept UnsignedRegisterType = RegisterType<T> and std::unsigned_integral<T>;

// PositionType: Concept for bytecode position types

template<typename T>
concept PositionType = std::same_as<T, BCPos> or std::is_convertible_v<T, BCPos>;

// IndexType: Concept for variable/upvalue index types

template<typename T>
concept IndexType = std::same_as<T, VarIndex> or std::same_as<T, MSize> or
                    (std::unsigned_integral<T> and sizeof(T) >= sizeof(VarIndex));

// FunctionState: Concept for function state pointers
// Ensures pointer to valid FuncState structure.

template<typename T>
concept FunctionState = std::is_pointer_v<T> and
                        std::same_as<std::remove_pointer_t<T>, FuncState>;

// LexerState: Concept for lexer state pointers

template<typename T>
concept LexerState = std::is_pointer_v<T> and
                     std::same_as<std::remove_pointer_t<T>, LexState>;

// BinaryOperator: Concept for binary operator types

template<typename T>
concept BinaryOperator = std::same_as<T, BinOpr> or
                         (std::integral<T> and sizeof(T) <= sizeof(BinOpr));

// UnaryOperator: Concept for unary operator types
// Note: Currently unused - UnOpr type not defined in parser.

// template<typename T>
// concept UnaryOperator = std::same_as<T, UnOpr> or
//                         (std::integral<T> and sizeof(T) <= sizeof(UnOpr));

// BytecodeInstruction: Concept for bytecode instruction types

template<typename T>
concept BytecodeInstruction = std::same_as<T, BCIns> or
                              (std::unsigned_integral<T> and sizeof(T) IS sizeof(BCIns));

// BytecodeOpcode: Concept for bytecode opcode types

template<typename T>
concept BytecodeOpcode = std::same_as<T, BCOp> or
                         (std::integral<T> and sizeof(T) <= sizeof(BCOp));

// GCString: Concept for GC-managed string types

template<typename T>
concept GCString = std::is_pointer_v<T> and
                   std::same_as<std::remove_pointer_t<T>, GCstr>;

// NumericValue: Concept for numeric constant values

template<typename T>
concept NumericValue = std::is_arithmetic_v<T>;

// IntegralValue: Concept for integer values

template<typename T>
concept IntegralValue = std::integral<T>;

// FloatingValue: Concept for floating-point values

template<typename T>
concept FloatingValue = std::floating_point<T>;

// ValidExpKind: Concept to validate expression kinds at compile-time
// Note: This checks the underlying type, not the value itself.
// Runtime validation still required for actual values.

template<typename T>
concept ValidExpKind = std::same_as<T, ExpKind> or
                       (std::integral<T> and sizeof(T) <= sizeof(ExpKind));

// ExpressionHandler: Composite concept for functions that handle expressions

template<typename F>
concept ExpressionHandler = requires(F f, ExpDesc* e) {
   { f(e) };
};

// ConstExpressionPredicate: Concept for predicates on const expressions

template<typename F>
concept ConstExpressionPredicate = requires(F f, const ExpDesc* e) {
   { f(e) } -> std::convertible_to<bool>;
};

// Compile-time validation of concept satisfaction
// These static assertions ensure that the core types satisfy their intended concepts,
// providing early detection of interface changes.

static_assert(ExpressionDescriptor<ExpDesc>, "ExpDesc must satisfy ExpressionDescriptor concept");
static_assert(RegisterType<BCReg>, "BCReg must satisfy RegisterType concept");
static_assert(BytecodeOpcode<BCOp>, "BCOp must satisfy BytecodeOpcode concept");
static_assert(FunctionState<FuncState*>, "FunctionState* must satisfy FunctionState concept");
