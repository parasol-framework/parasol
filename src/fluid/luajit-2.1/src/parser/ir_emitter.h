// IR emission pass that lowers AST nodes to LuaJIT bytecode.
// Part of Phase 2 Step 3 documented in docs/plans/PARSER_P2.md. This visitor
// consumes the BlockStmt returned by AstBuilder::parse_chunk(), providing the
// parse/emission handshake described in docs/plans/PARSER_P2.md Step 4.

#pragma once

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "parser/ast_nodes.h"
#include "parser/parser_context.h"
#include "parser/parse_types.h"

struct IrEmitUnit {
};

class IrEmitter {
public:
   explicit IrEmitter(ParserContext& context);

   ParserResult<IrEmitUnit> emit_chunk(const BlockStmt& chunk);

private:
   ParserContext& ctx;
   FuncState& func_state;
   LexState& lex_state;
   std::vector<std::pair<GCstr*, BCReg>> local_bindings; // TODO: Could this be an unordered map?

   ParserResult<IrEmitUnit> emit_block(const BlockStmt& block, FuncScopeFlag flags = FuncScopeFlag::None);
   ParserResult<IrEmitUnit> emit_statement(const StmtNode& stmt);
   ParserResult<IrEmitUnit> emit_expression_stmt(const ExpressionStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_return_stmt(const ReturnStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_local_decl_stmt(const LocalDeclStmtPayload& payload);
   ParserResult<IrEmitUnit> emit_assignment_stmt(const AssignmentStmtPayload& payload);

   ParserResult<ExpDesc> emit_expression(const ExprNode& expr);
   ParserResult<ExpDesc> emit_literal_expr(const LiteralValue& literal);
   ParserResult<ExpDesc> emit_identifier_expr(const NameRef& reference);
   ParserResult<ExpDesc> emit_vararg_expr();
   ParserResult<ExpDesc> emit_unary_expr(const UnaryExprPayload& payload);
   ParserResult<ExpDesc> emit_binary_expr(const BinaryExprPayload& payload);
   ParserResult<ExpDesc> emit_presence_expr(const PresenceExprPayload& payload);
   ParserResult<ExpDesc> emit_member_expr(const MemberExprPayload& payload);
   ParserResult<ExpDesc> emit_index_expr(const IndexExprPayload& payload);
   ParserResult<ExpDesc> emit_call_expr(const CallExprPayload& payload);
   ParserResult<ExpDesc> emit_expression_list(const ExprNodeList& expressions, BCReg& count);
   std::optional<BCReg> resolve_local(GCstr* symbol) const;

   ParserResult<IrEmitUnit> unsupported_stmt(AstNodeKind kind, const SourceSpan& span);
   ParserResult<ExpDesc> unsupported_expr(AstNodeKind kind, const SourceSpan& span);

   ParserError make_error(ParserErrorCode code, std::string_view message) const;
};

