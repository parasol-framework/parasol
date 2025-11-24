// Lua parser - AST/IR node schema
// Copyright (C) 2025 Paul Manias
//
// The schema mirrors every construct currently handled by the LuaJIT parser and
// describes how child nodes, source locations and semantic attributes are stored.
//
//  *  Expressions include literals, identifiers, calls, table constructors,
//     unary/binary operators (including Fluid extensions such as `??`, ternary
//     selections and postfix increment), function literals and suffix operations
//     (field access, indexing, method calls, presence checks).
//  *  Statements cover assignments, declarations, control-flow (if/while/repeat,
//     numeric & generic for loops, break/continue), defer blocks,
//     returns, chunk/local blocks and bare expression statements.
//  *  Dedicated structs capture reusable metadata (Identifier, NameRef,
//     FunctionParameter, TableField, BlockStmt) so later work can extend the
//     IR without mutating parser internals.
//
// Nodes own their children through std::unique_ptr / std::vector so lifetime is
// explicit and node hierarchies can be transferred between passes. Every node
// stores a SourceSpan for diagnostics, and lightweight view helpers expose the
// contents of frequently iterated collections without leaking storage details.
// Extensions should prefer adding new structs + AstNodeKind tags rather than
// repurposing existing payloads; see the "Extension guidelines" comment near the
// end of this file.

// Extension guidelines
// 1. Add a new AstNodeKind entry and dedicated payload struct. Do NOT overload existing
//    payloads unless semantics are identical.
// 2. Prefer storing ownership via std::unique_ptr/std::vector so that AST lifetimes are
//    explicit. Views (StatementListView / ExpressionListView) should wrap storage instead
//    of returning raw references.
// 3. Always assign a SourceSpan when constructing nodes. Builder helpers above centralise
//    invariant checks; extend them when new nodes require validation.
// 4. Keep Identifier/NameRef semantics stable so scope resolution can be threaded through
//    later passes without rewriting node shapes.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "lexer.h"

class ParserDiagnostics;

// Forward declarations for recursive ownership.
struct ExprNode;
struct StmtNode;
struct BlockStmt;

using ExprNodePtr = std::unique_ptr<ExprNode>;
using StmtNodePtr = std::unique_ptr<StmtNode>;
using ExprNodeList = std::vector<ExprNodePtr>;
using StmtNodeList = std::vector<StmtNodePtr>;

enum class AstNodeKind : uint16_t {
   LiteralExpr,
   IdentifierExpr,
   VarArgExpr,
   UnaryExpr,
   BinaryExpr,
   UpdateExpr,
   TernaryExpr,
   PresenceExpr,
   CallExpr,
   MemberExpr,
   IndexExpr,
   TableExpr,
   FunctionExpr,
   BlockStmt,
   AssignmentStmt,
   LocalDeclStmt,
   LocalFunctionStmt,
   FunctionStmt,
   IfStmt,
   WhileStmt,
   RepeatStmt,
   NumericForStmt,
   GenericForStmt,
   BreakStmt,
   ContinueStmt,
   ReturnStmt,
   DeferStmt,
   DoStmt,
   ExpressionStmt
};

enum class LiteralKind : uint8_t {
   Nil,
   Boolean,
   Number,
   String,
   CData
};

enum class AstUnaryOperator : uint8_t {
   Negate,
   Not,
   Length,
   BitNot
};

enum class AstUpdateOperator : uint8_t {
   Increment,
   Decrement
};

enum class AstBinaryOperator : uint8_t {
   Add,
   Subtract,
   Multiply,
   Divide,
   Modulo,
   Power,
   Concat,
   NotEqual,
   Equal,
   LessThan,
   GreaterEqual,
   LessEqual,
   GreaterThan,
   BitAnd,
   BitOr,
   BitXor,
   ShiftLeft,
   ShiftRight,
   LogicalAnd,
   LogicalOr,
   IfEmpty
};

enum class NameResolution : uint8_t {
   Unresolved,
   Local,
   Upvalue,
   Global,
   Environment
};

enum class TableFieldKind : uint8_t {
   Array,
   Record,
   Computed
};

enum class AssignmentOperator : uint8_t {
   Plain,
   Add,
   Subtract,
   Multiply,
   Divide,
   Modulo,
   Concat,
   IfEmpty
};

enum class LoopStyle : uint8_t {
   WhileLoop,
   RepeatUntil
};

enum class CallDispatch : uint8_t {
   Direct,
   Method
};

//********************************************************************************************************************

struct Identifier {
   GCstr* symbol = nullptr;
   SourceSpan span{};
   bool is_blank = false;
};

struct NameRef {
   Identifier identifier;
   NameResolution resolution = NameResolution::Unresolved;
   uint16_t slot = 0;
};

struct LiteralValue {
   LiteralKind kind = LiteralKind::Nil;
   bool bool_value = false;
   lua_Number number_value = 0.0;
   GCstr* string_value = nullptr;
   TValue cdata_value{};
};

struct FunctionParameter {
   Identifier name;
   bool is_self = false;
};

struct DirectCallTarget {
   DirectCallTarget() = default;
   DirectCallTarget(const DirectCallTarget&) = delete;
   DirectCallTarget& operator=(const DirectCallTarget&) = delete;
   DirectCallTarget(DirectCallTarget&&) noexcept = default;
   DirectCallTarget& operator=(DirectCallTarget&&) noexcept = default;
   ExprNodePtr callable;
   ~DirectCallTarget();
};

struct MethodCallTarget {
   MethodCallTarget() = default;
   MethodCallTarget(const MethodCallTarget&) = delete;
   MethodCallTarget& operator=(const MethodCallTarget&) = delete;
   MethodCallTarget(MethodCallTarget&&) noexcept = default;
   MethodCallTarget& operator=(MethodCallTarget&&) noexcept = default;
   ExprNodePtr receiver;
   Identifier method;
   ~MethodCallTarget();
};

using CallTarget = std::variant<DirectCallTarget, MethodCallTarget>;

struct VarArgExprPayload {};

struct UnaryExprPayload {
   UnaryExprPayload() = default;
   UnaryExprPayload(const UnaryExprPayload&) = delete;
   UnaryExprPayload& operator=(const UnaryExprPayload&) = delete;
   UnaryExprPayload(UnaryExprPayload&&) noexcept = default;
   UnaryExprPayload& operator=(UnaryExprPayload&&) noexcept = default;
   AstUnaryOperator op = AstUnaryOperator::Negate;
   ExprNodePtr operand;
   ~UnaryExprPayload();
};

struct UpdateExprPayload {
   UpdateExprPayload() = default;
   UpdateExprPayload(const UpdateExprPayload&) = delete;
   UpdateExprPayload& operator=(const UpdateExprPayload&) = delete;
   UpdateExprPayload(UpdateExprPayload&&) noexcept = default;
   UpdateExprPayload& operator=(UpdateExprPayload&&) noexcept = default;
   AstUpdateOperator op = AstUpdateOperator::Increment;
   bool is_postfix = false;
   ExprNodePtr target;
   ~UpdateExprPayload();
};

struct BinaryExprPayload {
   BinaryExprPayload() = default;
   BinaryExprPayload(const BinaryExprPayload&) = delete;
   BinaryExprPayload& operator=(const BinaryExprPayload&) = delete;
   BinaryExprPayload(BinaryExprPayload&&) noexcept = default;
   BinaryExprPayload& operator=(BinaryExprPayload&&) noexcept = default;
   AstBinaryOperator op = AstBinaryOperator::Add;
   ExprNodePtr left;
   ExprNodePtr right;
   ~BinaryExprPayload();
};

struct TernaryExprPayload {
   TernaryExprPayload() = default;
   TernaryExprPayload(const TernaryExprPayload&) = delete;
   TernaryExprPayload& operator=(const TernaryExprPayload&) = delete;
   TernaryExprPayload(TernaryExprPayload&&) noexcept = default;
   TernaryExprPayload& operator=(TernaryExprPayload&&) noexcept = default;
   ExprNodePtr condition;
   ExprNodePtr if_true;
   ExprNodePtr if_false;
   ~TernaryExprPayload();
};

struct PresenceExprPayload {
   PresenceExprPayload() = default;
   PresenceExprPayload(const PresenceExprPayload&) = delete;
   PresenceExprPayload& operator=(const PresenceExprPayload&) = delete;
   PresenceExprPayload(PresenceExprPayload&&) noexcept = default;
   PresenceExprPayload& operator=(PresenceExprPayload&&) noexcept = default;
   ExprNodePtr value;
   ~PresenceExprPayload();
};

struct CallExprPayload {
   CallExprPayload() = default;
   CallExprPayload(const CallExprPayload&) = delete;
   CallExprPayload& operator=(const CallExprPayload&) = delete;
   CallExprPayload(CallExprPayload&&) noexcept = default;
   CallExprPayload& operator=(CallExprPayload&&) noexcept = default;
   CallTarget target;
   ExprNodeList arguments;
   bool forwards_multret = false;
   ~CallExprPayload();
};

struct MemberExprPayload {
   MemberExprPayload() = default;
   MemberExprPayload(const MemberExprPayload&) = delete;
   MemberExprPayload& operator=(const MemberExprPayload&) = delete;
   MemberExprPayload(MemberExprPayload&&) noexcept = default;
   MemberExprPayload& operator=(MemberExprPayload&&) noexcept = default;
   ExprNodePtr table;
   Identifier member;
   bool uses_method_dispatch = false;
   ~MemberExprPayload();
};

struct IndexExprPayload {
   IndexExprPayload() = default;
   IndexExprPayload(const IndexExprPayload&) = delete;
   IndexExprPayload& operator=(const IndexExprPayload&) = delete;
   IndexExprPayload(IndexExprPayload&&) noexcept = default;
   IndexExprPayload& operator=(IndexExprPayload&&) noexcept = default;
   ExprNodePtr table;
   ExprNodePtr index;
   ~IndexExprPayload();
};

struct TableField {
   TableField() = default;
   TableField(const TableField&) = delete;
   TableField& operator=(const TableField&) = delete;
   TableField(TableField&&) noexcept = default;
   TableField& operator=(TableField&&) noexcept = default;
   TableFieldKind kind = TableFieldKind::Array;
   SourceSpan span{};
   std::optional<Identifier> name;
   ExprNodePtr key;
   ExprNodePtr value;
   ~TableField();
};

struct TableExprPayload {
   TableExprPayload() = default;
   TableExprPayload(const TableExprPayload&) = delete;
   TableExprPayload& operator=(const TableExprPayload&) = delete;
   TableExprPayload(TableExprPayload&&) noexcept = default;
   TableExprPayload& operator=(TableExprPayload&&) noexcept = default;
   std::vector<TableField> fields;
   bool has_array_part = false;
   ~TableExprPayload();
};

struct FunctionExprPayload {
   FunctionExprPayload() = default;
   FunctionExprPayload(const FunctionExprPayload&) = delete;
   FunctionExprPayload& operator=(const FunctionExprPayload&) = delete;
   FunctionExprPayload(FunctionExprPayload&&) noexcept = default;
   FunctionExprPayload& operator=(FunctionExprPayload&&) noexcept = default;
   std::vector<FunctionParameter> parameters;
   bool is_vararg = false;
   std::unique_ptr<BlockStmt> body;
   ~FunctionExprPayload();
};

struct ExprNode {
   AstNodeKind kind = AstNodeKind::LiteralExpr;
   SourceSpan span{};
   std::variant<LiteralValue, NameRef, VarArgExprPayload, UnaryExprPayload,
      UpdateExprPayload, BinaryExprPayload, TernaryExprPayload,
      PresenceExprPayload, CallExprPayload, MemberExprPayload, IndexExprPayload,
      TableExprPayload, FunctionExprPayload>
      data;
};

struct IfClause {
   IfClause() = default;
   IfClause(const IfClause&) = delete;
   IfClause& operator=(const IfClause&) = delete;
   IfClause(IfClause&&) noexcept = default;
   IfClause& operator=(IfClause&&) noexcept = default;
   ExprNodePtr condition;
   std::unique_ptr<BlockStmt> block;
   ~IfClause();
};

struct AssignmentStmtPayload {
   AssignmentStmtPayload() = default;
   AssignmentStmtPayload(const AssignmentStmtPayload&) = delete;
   AssignmentStmtPayload& operator=(const AssignmentStmtPayload&) = delete;
   AssignmentStmtPayload(AssignmentStmtPayload&&) noexcept = default;
   AssignmentStmtPayload& operator=(AssignmentStmtPayload&&) noexcept = default;
   AssignmentOperator op = AssignmentOperator::Plain;
   ExprNodeList targets;
   ExprNodeList values;
   ~AssignmentStmtPayload();
};

struct LocalDeclStmtPayload {
   LocalDeclStmtPayload() = default;
   LocalDeclStmtPayload(const LocalDeclStmtPayload&) = delete;
   LocalDeclStmtPayload& operator=(const LocalDeclStmtPayload&) = delete;
   LocalDeclStmtPayload(LocalDeclStmtPayload&&) noexcept = default;
   LocalDeclStmtPayload& operator=(LocalDeclStmtPayload&&) noexcept = default;
   std::vector<Identifier> names;
   ExprNodeList values;
   ~LocalDeclStmtPayload();
};

struct LocalFunctionStmtPayload {
   LocalFunctionStmtPayload() = default;
   LocalFunctionStmtPayload(const LocalFunctionStmtPayload&) = delete;
   LocalFunctionStmtPayload& operator=(const LocalFunctionStmtPayload&) = delete;
   LocalFunctionStmtPayload(LocalFunctionStmtPayload&&) noexcept = default;
   LocalFunctionStmtPayload& operator=(LocalFunctionStmtPayload&&) noexcept = default;
   Identifier name;
   std::unique_ptr<FunctionExprPayload> function;
   ~LocalFunctionStmtPayload();
};

struct FunctionNamePath {
   std::vector<Identifier> segments;
   std::optional<Identifier> method;
};

struct FunctionStmtPayload {
   FunctionStmtPayload() = default;
   FunctionStmtPayload(const FunctionStmtPayload&) = delete;
   FunctionStmtPayload& operator=(const FunctionStmtPayload&) = delete;
   FunctionStmtPayload(FunctionStmtPayload&&) noexcept = default;
   FunctionStmtPayload& operator=(FunctionStmtPayload&&) noexcept = default;
   FunctionNamePath name;
   std::unique_ptr<FunctionExprPayload> function;
   ~FunctionStmtPayload();
};

struct IfStmtPayload {
   IfStmtPayload() = default;
   IfStmtPayload(const IfStmtPayload&) = delete;
   IfStmtPayload& operator=(const IfStmtPayload&) = delete;
   IfStmtPayload(IfStmtPayload&&) noexcept = default;
   IfStmtPayload& operator=(IfStmtPayload&&) noexcept = default;
   std::vector<IfClause> clauses;
   ~IfStmtPayload();
};

struct LoopStmtPayload {
   LoopStmtPayload() = default;
   LoopStmtPayload(const LoopStmtPayload&) = delete;
   LoopStmtPayload& operator=(const LoopStmtPayload&) = delete;
   LoopStmtPayload(LoopStmtPayload&&) noexcept = default;
   LoopStmtPayload& operator=(LoopStmtPayload&&) noexcept = default;
   LoopStyle style = LoopStyle::WhileLoop;
   ExprNodePtr condition;
   std::unique_ptr<BlockStmt> body;
   ~LoopStmtPayload();
};

struct NumericForStmtPayload {
   NumericForStmtPayload() = default;
   NumericForStmtPayload(const NumericForStmtPayload&) = delete;
   NumericForStmtPayload& operator=(const NumericForStmtPayload&) = delete;
   NumericForStmtPayload(NumericForStmtPayload&&) noexcept = default;
   NumericForStmtPayload& operator=(NumericForStmtPayload&&) noexcept = default;
   Identifier control;
   ExprNodePtr start;
   ExprNodePtr stop;
   ExprNodePtr step;
   std::unique_ptr<BlockStmt> body;
   ~NumericForStmtPayload();
};

struct GenericForStmtPayload {
   GenericForStmtPayload() = default;
   GenericForStmtPayload(const GenericForStmtPayload&) = delete;
   GenericForStmtPayload& operator=(const GenericForStmtPayload&) = delete;
   GenericForStmtPayload(GenericForStmtPayload&&) noexcept = default;
   GenericForStmtPayload& operator=(GenericForStmtPayload&&) noexcept = default;
   std::vector<Identifier> names;
   ExprNodeList iterators;
   std::unique_ptr<BlockStmt> body;
   ~GenericForStmtPayload();
};

struct ReturnStmtPayload {
   ReturnStmtPayload() = default;
   ReturnStmtPayload(const ReturnStmtPayload&) = delete;
   ReturnStmtPayload& operator=(const ReturnStmtPayload&) = delete;
   ReturnStmtPayload(ReturnStmtPayload&&) noexcept = default;
   ReturnStmtPayload& operator=(ReturnStmtPayload&&) noexcept = default;
   ExprNodeList values;
   bool forwards_call = false;
   ~ReturnStmtPayload();
};

struct BreakStmtPayload {};

struct ContinueStmtPayload {};

struct DeferStmtPayload {
   DeferStmtPayload() = default;
   DeferStmtPayload(const DeferStmtPayload&) = delete;
   DeferStmtPayload& operator=(const DeferStmtPayload&) = delete;
   DeferStmtPayload(DeferStmtPayload&&) noexcept = default;
   DeferStmtPayload& operator=(DeferStmtPayload&&) noexcept = default;
   std::unique_ptr<FunctionExprPayload> callable;
   ExprNodeList arguments;
   ~DeferStmtPayload();
};

struct DoStmtPayload {
   DoStmtPayload() = default;
   DoStmtPayload(const DoStmtPayload&) = delete;
   DoStmtPayload& operator=(const DoStmtPayload&) = delete;
   DoStmtPayload(DoStmtPayload&&) noexcept = default;
   DoStmtPayload& operator=(DoStmtPayload&&) noexcept = default;
   std::unique_ptr<BlockStmt> block;
   ~DoStmtPayload();
};

struct ExpressionStmtPayload {
   ExpressionStmtPayload() = default;
   ExpressionStmtPayload(const ExpressionStmtPayload&) = delete;
   ExpressionStmtPayload& operator=(const ExpressionStmtPayload&) = delete;
   ExpressionStmtPayload(ExpressionStmtPayload&&) noexcept = default;
   ExpressionStmtPayload& operator=(ExpressionStmtPayload&&) noexcept = default;
   ExprNodePtr expression;
   ~ExpressionStmtPayload();
};

//********************************************************************************************************************

struct StmtNode {
   AstNodeKind kind = AstNodeKind::ExpressionStmt;
   SourceSpan span{};
   std::variant<AssignmentStmtPayload, LocalDeclStmtPayload,
      LocalFunctionStmtPayload, FunctionStmtPayload, IfStmtPayload,
      LoopStmtPayload, NumericForStmtPayload, GenericForStmtPayload,
      ReturnStmtPayload, BreakStmtPayload, ContinueStmtPayload, DeferStmtPayload,
      DoStmtPayload, ExpressionStmtPayload>
      data;
};

//********************************************************************************************************************

class StatementListView {
public:
   StatementListView() = default;
   explicit StatementListView(const StmtNodeList &nodes) : storage(&nodes) { }

   class Iterator {
   public:
      using InnerIterator = StmtNodeList::const_iterator;
      Iterator() = default;
      inline explicit Iterator(InnerIterator it) : iter(it) {}
      inline const StmtNode& operator*() const { return *(*this->iter); }
      inline const StmtNode* operator->() const { return this->iter->get(); }
      inline Iterator& operator++() { ++this->iter; return *this; }
      inline bool operator==(const Iterator& other) const { return this->iter == other.iter; }
      inline bool operator!=(const Iterator& other) const { return !(*this == other); }

   private:
      InnerIterator iter{};
   };

   [[nodiscard]] const StmtNode& operator[](size_t index) const {
      lj_assertX(this->storage and index < this->storage->size(), "Statement index out of range");
      return *(*this->storage)[index];
   }

   inline Iterator begin() const { return this->storage ? Iterator(this->storage->begin()) : Iterator(); }
   inline Iterator end() const { return this->storage ? Iterator(this->storage->end()) : Iterator(); }
   inline size_t size() const { return this->storage ? this->storage->size() : 0; }
   inline bool empty() const { return this->size() == 0; }

private:
   const StmtNodeList* storage = nullptr;
};

//********************************************************************************************************************

class ExpressionListView {
public:
   ExpressionListView() = default;
   
   explicit ExpressionListView(const ExprNodeList& nodes) : storage(&nodes) { }

   class Iterator {
   public:
      using InnerIterator = ExprNodeList::const_iterator;
      Iterator() = default;
       
      explicit Iterator(InnerIterator it) : iter(it) {}
      const ExprNode& operator*() const { return *(*this->iter); }
      const ExprNode* operator->() const { return this->iter->get(); }
      Iterator& operator++() { ++this->iter; return *this; }
      bool operator==(const Iterator& other) const { return this->iter == other.iter; }
      bool operator!=(const Iterator& other) const { return !(*this == other); }

   private:
      InnerIterator iter{};
   };

   Iterator begin() const { return this->storage ? Iterator(this->storage->begin()) : Iterator(); }
   Iterator end() const { return this->storage ? Iterator(this->storage->end()) : Iterator(); }
   size_t size() const { return this->storage ? this->storage->size() : 0; }
   bool empty() const { return this->size() == 0; }

   const ExprNode & operator[](size_t index) const {
      lj_assertX(this->storage and index < this->storage->size(), "Expression index out of range");
      return *(*this->storage)[index];
   }

private:
   const ExprNodeList* storage = nullptr;
};

//********************************************************************************************************************

struct BlockStmt {
   SourceSpan span{};
   StmtNodeList statements;

   inline StatementListView view() const { return StatementListView(this->statements); }

   ~BlockStmt();
};

//********************************************************************************************************************
// Builder helpers

ExprNodePtr make_literal_expr(SourceSpan span, const LiteralValue& literal);
ExprNodePtr make_identifier_expr(SourceSpan span, const NameRef& reference);
ExprNodePtr make_vararg_expr(SourceSpan span);
ExprNodePtr make_unary_expr(SourceSpan span, AstUnaryOperator op, ExprNodePtr operand);
ExprNodePtr make_update_expr(SourceSpan span, AstUpdateOperator op, bool is_postfix, ExprNodePtr target);
ExprNodePtr make_binary_expr(SourceSpan span, AstBinaryOperator op, ExprNodePtr left, ExprNodePtr right);
ExprNodePtr make_ternary_expr(SourceSpan span, ExprNodePtr condition, ExprNodePtr if_true, ExprNodePtr if_false);
ExprNodePtr make_presence_expr(SourceSpan span, ExprNodePtr value);
ExprNodePtr make_call_expr(SourceSpan span, ExprNodePtr callee, ExprNodeList arguments, bool forwards_multret);
ExprNodePtr make_method_call_expr(SourceSpan span, ExprNodePtr receiver, Identifier method, ExprNodeList arguments, bool forwards_multret);
ExprNodePtr make_member_expr(SourceSpan span, ExprNodePtr table, Identifier member, bool uses_method_dispatch);
ExprNodePtr make_index_expr(SourceSpan span, ExprNodePtr table, ExprNodePtr index);
ExprNodePtr make_table_expr(SourceSpan span, std::vector<TableField> fields, bool has_array_part);
ExprNodePtr make_function_expr(SourceSpan span, std::vector<FunctionParameter> parameters, bool is_vararg, std::unique_ptr<BlockStmt> body);
std::unique_ptr<FunctionExprPayload> make_function_payload(std::vector<FunctionParameter> parameters, bool is_vararg, std::unique_ptr<BlockStmt> body);
std::unique_ptr<BlockStmt> make_block(SourceSpan span, StmtNodeList statements);
StmtNodePtr make_assignment_stmt(SourceSpan span, AssignmentOperator op, ExprNodeList targets, ExprNodeList values);
StmtNodePtr make_local_decl_stmt(SourceSpan span, std::vector<Identifier> names, ExprNodeList values);
StmtNodePtr make_return_stmt(SourceSpan span, ExprNodeList values, bool forwards_call);
StmtNodePtr make_expression_stmt(SourceSpan span, ExprNodePtr expression);

[[nodiscard]] size_t ast_statement_child_count(const StmtNode& node);
[[nodiscard]] size_t ast_expression_child_count(const ExprNode& node);
