/*
** C++20 Concepts for LuaJIT Parser Type Constraints
** Copyright (C) 2025 Paul Manias. See Copyright Notice in parasol.h
*/

#ifndef _LJ_PARSE_CONCEPTS_H
#define _LJ_PARSE_CONCEPTS_H

#include <concepts>
#include <type_traits>

// -- Expression Type Concepts ----------------------------------------------

/*
** ExpressionDescriptor: Concept for expression descriptor types
**
** Ensures a type has the required members for expression handling.
** This provides compile-time validation and better error messages.
*/
template<typename T>
concept ExpressionDescriptor = requires(T e) {
   { e.k } -> std::convertible_to<ExpKind>;
   { e.t } -> std::convertible_to<BCPos>;
   { e.f } -> std::convertible_to<BCPos>;
   requires sizeof(e.u) > 0;  // Has union member
};

/*
** ConstExpressionDescriptor: Stricter concept for const expression access
*/
template<typename T>
concept ConstExpressionDescriptor = ExpressionDescriptor<T> and requires(const T* e) {
   { e->k } -> std::convertible_to<ExpKind>;
   { e->t } -> std::convertible_to<BCPos>;
   { e->f } -> std::convertible_to<BCPos>;
};

// -- Register Type Concepts ------------------------------------------------

/*
** RegisterType: Concept for bytecode register types
**
** Ensures type is compatible with BCReg operations.
*/
template<typename T>
concept RegisterType = std::is_same_v<T, BCReg> or std::is_convertible_v<T, BCReg>;

/*
** UnsignedRegisterType: Stricter concept for unsigned register operations
*/
template<typename T>
concept UnsignedRegisterType = RegisterType<T> and std::unsigned_integral<T>;

// -- Position Type Concepts ------------------------------------------------

/*
** PositionType: Concept for bytecode position types
*/
template<typename T>
concept PositionType = std::is_same_v<T, BCPos> or std::is_convertible_v<T, BCPos>;

// -- Index Type Concepts ---------------------------------------------------

/*
** IndexType: Concept for variable/upvalue index types
*/
template<typename T>
concept IndexType = std::is_same_v<T, VarIndex> or std::is_same_v<T, MSize> or
                    (std::unsigned_integral<T> and sizeof(T) >= sizeof(VarIndex));

// -- State Type Concepts ---------------------------------------------------

/*
** FunctionState: Concept for function state pointers
**
** Ensures pointer to valid FuncState structure.
*/
template<typename T>
concept FunctionState = std::is_pointer_v<T> and
                        std::is_same_v<std::remove_pointer_t<T>, FuncState>;

/*
** LexerState: Concept for lexer state pointers
*/
template<typename T>
concept LexerState = std::is_pointer_v<T> and
                     std::is_same_v<std::remove_pointer_t<T>, LexState>;

// -- Operator Concepts -----------------------------------------------------

/*
** BinaryOperator: Concept for binary operator types
*/
template<typename T>
concept BinaryOperator = std::is_same_v<T, BinOpr> or
                         (std::integral<T> and sizeof(T) <= sizeof(BinOpr));

/*
** UnaryOperator: Concept for unary operator types
** Note: Currently unused - UnOpr type not defined in parser.
*/
// template<typename T>
// concept UnaryOperator = std::is_same_v<T, UnOpr> or
//                         (std::integral<T> and sizeof(T) <= sizeof(UnOpr));

// -- Bytecode Instruction Concepts -----------------------------------------

/*
** BytecodeInstruction: Concept for bytecode instruction types
*/
template<typename T>
concept BytecodeInstruction = std::is_same_v<T, BCIns> or
                              (std::unsigned_integral<T> and sizeof(T) == sizeof(BCIns));

/*
** BytecodeOpcode: Concept for bytecode opcode types
*/
template<typename T>
concept BytecodeOpcode = std::is_same_v<T, BCOp> or
                         (std::integral<T> and sizeof(T) <= sizeof(BCOp));

// -- String Type Concepts --------------------------------------------------

/*
** GCString: Concept for GC-managed string types
*/
template<typename T>
concept GCString = std::is_pointer_v<T> and
                   std::is_same_v<std::remove_pointer_t<T>, GCstr>;

// -- Numeric Concepts ------------------------------------------------------

/*
** NumericValue: Concept for numeric constant values
*/
template<typename T>
concept NumericValue = std::is_arithmetic_v<T>;

/*
** IntegralValue: Concept for integer values
*/
template<typename T>
concept IntegralValue = std::integral<T>;

/*
** FloatingValue: Concept for floating-point values
*/
template<typename T>
concept FloatingValue = std::floating_point<T>;

// -- Validation Helper Concepts --------------------------------------------

/*
** ValidExpKind: Concept to validate expression kinds at compile-time
**
** Note: This checks the underlying type, not the value itself.
** Runtime validation still required for actual values.
*/
template<typename T>
concept ValidExpKind = std::is_same_v<T, ExpKind> or
                       (std::integral<T> and sizeof(T) <= sizeof(ExpKind));

// -- Composite Concepts ----------------------------------------------------

/*
** ExpressionHandler: Composite concept for functions that handle expressions
*/
template<typename F>
concept ExpressionHandler = requires(F f, ExpDesc* e) {
   { f(e) };
};

/*
** ConstExpressionPredicate: Concept for predicates on const expressions
*/
template<typename F>
concept ConstExpressionPredicate = requires(F f, const ExpDesc* e) {
   { f(e) } -> std::convertible_to<bool>;
};

#endif
