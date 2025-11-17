#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "parser/token_types.h"

// -CLASS- AST schema overview
// The Phase 2 parser rewrite models Fluid syntax as an abstract syntax tree (AST)
// to separate syntax analysis from bytecode emission. The types in this header
// describe every syntactic construct that the LuaJIT parser is currently able to
// recognise. Nodes carry SourceSpan metadata for diagnostics and own their
// children via std::unique_ptr/std::vector to guarantee deterministic lifetimes.
// Each node advertises its role through AstNodeKind so later passes can perform
// targeted dispatch without inspecting the variant payload directly.

struct ExprNode;
struct StmtNode;
struct BlockNode;

using ExprPtr = std::unique_ptr<ExprNode>;
using StmtPtr = std::unique_ptr<StmtNode>;
using BlockPtr = std::unique_ptr<BlockNode>;
using ExprList = std::vector<ExprPtr>;
using StmtList = std::vector<StmtPtr>;
using TokenList = std::vector<Token>;

struct AstSpan {
   SourceSpan start;
   SourceSpan end;

   [[nodiscard]] SourceSpan enclosing_span() const;
};

enum class AstNodeKind : uint16_t {
   Invalid = 0,
   Block,
   LiteralExpr,
   IdentifierExpr,
   VarArgExpr,
   UnaryExpr,
   BinaryExpr,
   TernaryExpr,
   CallExpr,
   IndexExpr,
   TableField,
   TableCtorExpr,
   FunctionExpr,
   AssignmentStmt,
   LocalStmt,
   ExpressionStmt,
   IfStmt,
   WhileStmt,
   RepeatStmt,
   NumericForStmt,
   GenericForStmt,
   FunctionStmt,
   ReturnStmt,
   BreakStmt,
   ContinueStmt,
   DeferStmt
};

struct IdentifierNode {
   Token name;
   SourceSpan span;
};

struct LiteralNode {
   Token literal;
   SourceSpan span;
};

struct VarArgNode {
   Token dots;
   SourceSpan span;
};

struct UnaryExprNode {
   Token op;
   ExprPtr operand;
   SourceSpan span;
};

struct BinaryExprNode {
   Token op;
   ExprPtr left;
   ExprPtr right;
   SourceSpan span;
};

struct TernaryExprNode {
   ExprPtr condition;
   Token true_separator;
   ExprPtr true_branch;
   Token false_separator;
   ExprPtr false_branch;
   SourceSpan span;
};

struct CallExprNode {
   ExprPtr callee;
   ExprList arguments;
   std::optional<Token> method_name;
   SourceSpan span;
};

struct IndexExprNode {
   ExprPtr table;
   ExprPtr index;
   SourceSpan span;
};

struct TableFieldNode {
   std::optional<ExprPtr> key;
   ExprPtr value;
   bool is_array_entry = false;
   SourceSpan span;
};

struct TableCtorExprNode {
   std::vector<TableFieldNode> fields;
   SourceSpan span;
};

struct FunctionSignatureNode {
   TokenList parameters;
   bool is_vararg = false;
   SourceSpan span;
};

struct FunctionExprNode {
   FunctionSignatureNode signature;
   BlockPtr body;
   SourceSpan span;
};

struct ExprNode {
   AstNodeKind kind = AstNodeKind::Invalid;
   std::variant<LiteralNode, IdentifierNode, VarArgNode, UnaryExprNode, BinaryExprNode,
      TernaryExprNode, CallExprNode, IndexExprNode, TableCtorExprNode, FunctionExprNode> data;
   SourceSpan span{};
};

struct AssignmentStmtNode {
   ExprList targets;
   ExprList values;
   Token op;
   SourceSpan span;
};

struct LocalStmtNode {
   TokenList names;
   ExprList values;
   SourceSpan span;
};

struct ExpressionStmtNode {
   ExprPtr expression;
   SourceSpan span;
};

struct IfBranchNode {
   ExprPtr condition;
   BlockPtr body;
   SourceSpan span;
};

struct IfStmtNode {
   std::vector<IfBranchNode> branches;
   BlockPtr else_branch;
   SourceSpan span;
};

struct WhileStmtNode {
   Token keyword;
   ExprPtr condition;
   BlockPtr body;
   SourceSpan span;
};

struct RepeatStmtNode {
   Token keyword;
   BlockPtr body;
   ExprPtr condition;
   SourceSpan span;
};

struct NumericForStmtNode {
   Token variable;
   ExprPtr initial;
   ExprPtr limit;
   ExprPtr step;
   BlockPtr body;
   SourceSpan span;
};

struct GenericForStmtNode {
   TokenList variables;
   ExprList expressions;
   BlockPtr body;
   SourceSpan span;
};

struct FunctionStmtNode {
   std::vector<Token> path;
   FunctionExprNode function;
   SourceSpan span;
};

struct ReturnStmtNode {
   Token keyword;
   ExprList values;
   SourceSpan span;
};

struct BreakStmtNode {
   Token keyword;
   SourceSpan span;
};

struct ContinueStmtNode {
   Token keyword;
   SourceSpan span;
};

struct DeferStmtNode {
   Token keyword;
   BlockPtr body;
   SourceSpan span;
};

struct StmtNode {
   AstNodeKind kind = AstNodeKind::Invalid;
   std::variant<AssignmentStmtNode, LocalStmtNode, ExpressionStmtNode, IfStmtNode,
      WhileStmtNode, RepeatStmtNode, NumericForStmtNode, GenericForStmtNode,
      FunctionStmtNode, ReturnStmtNode, BreakStmtNode, ContinueStmtNode, DeferStmtNode> data;
   SourceSpan span{};
};

struct BlockNode {
   StmtList statements;
   SourceSpan span;
};

class StatementView {
public:
   StatementView() = default;
   explicit StatementView(const StmtList& statements);

   using const_iterator = StmtList::const_iterator;

   [[nodiscard]] const_iterator begin() const;
   [[nodiscard]] const_iterator end() const;
   [[nodiscard]] bool empty() const;
   [[nodiscard]] size_t size() const;

private:
   const StmtList* storage = nullptr;
};

class ParameterView {
public:
   ParameterView() = default;
   explicit ParameterView(const TokenList& parameters);

   using const_iterator = TokenList::const_iterator;

   [[nodiscard]] const_iterator begin() const;
   [[nodiscard]] const_iterator end() const;
   [[nodiscard]] bool empty() const;
   [[nodiscard]] size_t size() const;

private:
   const TokenList* storage = nullptr;
};

ExprPtr make_literal_expr(const Token& token);
ExprPtr make_identifier_expr(const Token& token);
ExprPtr make_vararg_expr(const Token& token);
ExprPtr make_unary_expr(const Token& op, ExprPtr operand, SourceSpan span);
ExprPtr make_binary_expr(const Token& op, ExprPtr left, ExprPtr right, SourceSpan span);
ExprPtr make_ternary_expr(ExprPtr condition, const Token& true_sep, ExprPtr true_branch,
   const Token& false_sep, ExprPtr false_branch, SourceSpan span);
ExprPtr make_call_expr(ExprPtr callee, ExprList arguments, SourceSpan span,
   std::optional<Token> method_name = std::nullopt);
ExprPtr make_index_expr(ExprPtr table, ExprPtr index, SourceSpan span);
ExprPtr make_table_expr(std::vector<TableFieldNode> fields, SourceSpan span);
ExprPtr make_function_expr(FunctionSignatureNode signature, BlockPtr body, SourceSpan span);

StmtPtr make_assignment_stmt(const Token& op, ExprList targets, ExprList values, SourceSpan span);
StmtPtr make_local_stmt(TokenList names, ExprList values, SourceSpan span);
StmtPtr make_expression_stmt(ExprPtr expression, SourceSpan span);
StmtPtr make_if_stmt(std::vector<IfBranchNode> branches, BlockPtr else_branch, SourceSpan span);
StmtPtr make_while_stmt(const Token& keyword, ExprPtr condition, BlockPtr body, SourceSpan span);
StmtPtr make_repeat_stmt(const Token& keyword, BlockPtr body, ExprPtr condition, SourceSpan span);
StmtPtr make_numeric_for_stmt(const Token& variable, ExprPtr initial, ExprPtr limit,
   ExprPtr step, BlockPtr body, SourceSpan span);
StmtPtr make_generic_for_stmt(TokenList variables, ExprList expressions, BlockPtr body,
   SourceSpan span);
StmtPtr make_function_stmt(std::vector<Token> path, FunctionExprNode function, SourceSpan span);
StmtPtr make_return_stmt(const Token& keyword, ExprList values, SourceSpan span);
StmtPtr make_break_stmt(const Token& keyword);
StmtPtr make_continue_stmt(const Token& keyword);
StmtPtr make_defer_stmt(const Token& keyword, BlockPtr body, SourceSpan span);

BlockPtr make_block(StmtList statements, SourceSpan span);

