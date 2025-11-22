// Copyright (C) 2025 Paul Manias
//
// IR emission pass that lowers AST nodes to LuaJIT bytecode.

#pragma once

#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "parser/ast_nodes.h"
#include "parser/parser_context.h"
#include "parser/parse_control_flow.h"
#include "parser/parse_regalloc.h"
#include "parser/parse_types.h"

struct LocalBindingEntry {
   GCstr *symbol = nullptr;
   BCReg slot = 0;
   uint32_t depth = 0;
};

struct BlockBinding {
   GCstr *symbol = nullptr;
   BCReg slot = 0;
};

class LocalBindingTable {
public:
   LocalBindingTable();

   void push_scope();
   void pop_scope();
   void add(GCstr* symbol, BCReg slot);
   [[nodiscard]] std::optional<BCReg> resolve(GCstr *) const;

private:
   std::vector<LocalBindingEntry> bindings;
   std::vector<size_t> scope_marks;
   uint32_t depth = 0;
};

class LocalBindingScope {
public:
   explicit LocalBindingScope(LocalBindingTable &table);
   ~LocalBindingScope();

   LocalBindingScope(const LocalBindingScope &) = delete;
   LocalBindingScope& operator=(const LocalBindingScope &) = delete;

private:
   LocalBindingTable& table;
};

struct IrEmitUnit {
};

// IR emission context that bundles allocator, CFG, and FuncState

class IrEmissionContext {
public:
   explicit IrEmissionContext(FuncState* State);

   [[nodiscard]] RegisterAllocator & allocator();
   [[nodiscard]] ControlFlowGraph & cfg();
   [[nodiscard]] FuncState * state() const;

private:
   FuncState* func_state;
   RegisterAllocator register_allocator;
   ControlFlowGraph control_flow_graph;
};

class IrEmitter {
public:
   explicit IrEmitter(ParserContext& context);

   ParserResult<IrEmitUnit> emit_chunk(const BlockStmt& chunk);

private:
   ParserContext& ctx;
   FuncState& func_state;
   LexState& lex_state;
   ControlFlowGraph control_flow;
   LocalBindingTable binding_table;

   ParserResult<IrEmitUnit> emit_block(const BlockStmt& block, FuncScopeFlag flags = FuncScopeFlag::None);
   ParserResult<IrEmitUnit> emit_block_with_bindings(const BlockStmt& block, FuncScopeFlag flags, std::span<const BlockBinding> bindings);
   ParserResult<IrEmitUnit> emit_statement(const StmtNode& stmt);
   ParserResult<IrEmitUnit> emit_expression_stmt(const ExpressionStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_return_stmt(const ReturnStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_local_decl_stmt(const LocalDeclStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_local_function_stmt(const LocalFunctionStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_function_stmt(const FunctionStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_if_stmt(const IfStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_while_stmt(const LoopStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_repeat_stmt(const LoopStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_numeric_for_stmt(const NumericForStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_generic_for_stmt(const GenericForStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_defer_stmt(const DeferStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_break_stmt(const BreakStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_continue_stmt(const ContinueStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_assignment_stmt(const AssignmentStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_plain_assignment(std::vector<ExpDesc> targets, const ExprNodeList& values);
   ParserResult<IrEmitUnit> emit_compound_assignment(AssignmentOperator op, ExpDesc target, const ExprNodeList& values);
   ParserResult<IrEmitUnit> emit_if_empty_assignment(ExpDesc target, const ExprNodeList& values);

   ParserResult<ExpDesc> emit_expression(const ExprNode& expr);
   ParserResult<ExpDesc> emit_literal_expr(const LiteralValue& literal);
   ParserResult<ExpDesc> emit_identifier_expr(const NameRef& reference);
   ParserResult<ExpDesc> emit_vararg_expr();
   ParserResult<ExpDesc> emit_unary_expr(const UnaryExprPayload& payload);
   ParserResult<ExpDesc> emit_update_expr(const UpdateExprPayload& payload);
   ParserResult<ExpDesc> emit_binary_expr(const BinaryExprPayload& payload);
   ParserResult<ExpDesc> emit_ternary_expr(const TernaryExprPayload& payload);
   ParserResult<ExpDesc> emit_presence_expr(const PresenceExprPayload& payload);
   ParserResult<ExpDesc> emit_member_expr(const MemberExprPayload& payload);
   ParserResult<ExpDesc> emit_index_expr(const IndexExprPayload& payload);
   ParserResult<ExpDesc> emit_call_expr(const CallExprPayload& payload);
   ParserResult<ExpDesc> emit_table_expr(const TableExprPayload& payload);
   ParserResult<ExpDesc> emit_function_expr(const FunctionExprPayload& payload);
   ParserResult<ExpDesc> emit_expression_list(const ExprNodeList& expressions, BCReg& count);
   ParserResult<ExpDesc> emit_lvalue_expr(const ExprNode& expr);
   ParserResult<ControlFlowEdge> emit_condition_jump(const ExprNode& expr);
   ParserResult<ExpDesc> emit_function_lvalue(const FunctionNamePath& path);
   ParserResult<std::vector<ExpDesc>> prepare_assignment_targets(const ExprNodeList& targets);
   void update_local_binding(GCstr* symbol, BCReg slot);
   std::optional<BCReg> resolve_local(GCstr* symbol) const;
   void materialise_to_next_reg(ExpDesc& expression, std::string_view usage);
   void materialise_to_reg(ExpDesc& expression, BCReg slot, std::string_view usage);
   void release_expression(ExpDesc& expression, std::string_view usage);
   void ensure_register_floor(std::string_view usage);
   void ensure_register_balance(std::string_view usage);

   ParserResult<IrEmitUnit> unsupported_stmt(AstNodeKind kind, const SourceSpan& span);
   ParserResult<ExpDesc> unsupported_expr(AstNodeKind kind, const SourceSpan& span);

   ParserError make_error(ParserErrorCode code, std::string_view message) const;
};

