// AST builder that threads typed tokens through the parser and produces the schema declared in ast_nodes.h without 
// touching FuncState/bytecode state.
//
// The top-level parse contract is `parse_chunk()` which returns ownership of the root BlockStmt describing the 
// current chunk; `IrEmitter::emit_chunk()` consumes that BlockStmt to generate bytecode, so the builder and emitter 
// can evolve independently while sharing a single AST boundary.

#pragma once

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "parser/ast_nodes.h"
#include "parser/parser_context.h"

class AstBuilder {
public:
   explicit AstBuilder(ParserContext& context);

   ParserResult<std::unique_ptr<BlockStmt>> parse_chunk();
   ParserResult<ExprNodePtr> parse_expression(uint8_t precedence = 0);
   ParserResult<ExprNodeList> parse_expression_list();

private:
   ParserContext& ctx;

   struct BinaryOpInfo {
      AstBinaryOperator op = AstBinaryOperator::Add;
      uint8_t left = 0;
      uint8_t right = 0;
   };

   [[nodiscard]] ParserResult<std::unique_ptr<BlockStmt>> parse_block(std::span<const TokenKind> terminators);
   ParserResult<StmtNodePtr> parse_statement();
   ParserResult<StmtNodePtr> parse_local();
   ParserResult<StmtNodePtr> parse_global();
   ParserResult<StmtNodePtr> parse_function_stmt();
   ParserResult<StmtNodePtr> parse_if();
   ParserResult<StmtNodePtr> parse_while();
   ParserResult<StmtNodePtr> parse_repeat();
   ParserResult<StmtNodePtr> parse_for();
   ParserResult<StmtNodePtr> parse_do();
   ParserResult<StmtNodePtr> parse_defer();
   ParserResult<StmtNodePtr> parse_return();
   ParserResult<StmtNodePtr> parse_expression_stmt();
   ParserResult<ExprNodePtr> parse_unary();
   ParserResult<ExprNodePtr> parse_primary();
   ParserResult<ExprNodePtr> parse_suffixed(ExprNodePtr base);
   ParserResult<ExprNodePtr> parse_arrow_function(ExprNodeList parameters);
   ParserResult<ExprNodePtr> parse_function_literal(const Token& function_token, bool is_thunk = false);
   ParserResult<ExprNodePtr> parse_table_literal();
   ParserResult<ReturnStmtPayload> parse_return_payload(const Token& return_token, bool same_line_only);
   ParserResult<FluidType> parse_return_type_annotation();

   ParserResult<std::vector<Identifier>> parse_name_list();
   struct ParameterListResult {
      std::vector<FunctionParameter> parameters;
      bool is_vararg = false;
   };

   static inline SourceSpan combine_spans(const SourceSpan& start, const SourceSpan& end) {
      SourceSpan span = start;
      span.offset = end.offset;
      span.line = end.line;
      span.column = end.column;
      return span;
   }

   ParserResult<ParameterListResult> parse_parameter_list(bool allow_optional);
   ParserResult<std::vector<TableField>> parse_table_fields(bool* has_array_part);
   ParserResult<ExprNodeList> parse_call_arguments(bool* forwards_multret);

   struct ResultFilterInfo {
      uint64_t keep_mask = 0;
      uint8_t explicit_count = 0;
      bool trailing_keep = false;
   };

   ParserResult<ResultFilterInfo> parse_result_filter_pattern();
   ParserResult<ExprNodePtr> parse_result_filter_expr(const Token& start_token);
   ParserResult<std::unique_ptr<BlockStmt>> parse_scoped_block(std::initializer_list<TokenKind> terminators);

   [[nodiscard]] bool at_end_of_block(std::span<const TokenKind> terminators) const;
   [[nodiscard]] bool is_statement_start(TokenKind kind) const;
   [[nodiscard]] static Identifier make_identifier(const Token& token);
   [[nodiscard]] static LiteralValue make_literal(const Token& token);
   [[nodiscard]] inline SourceSpan span_from(const Token& token) { return token.span(); }
   [[nodiscard]] inline SourceSpan span_from(const Token& start, const Token& end) const { return combine_spans(start.span(), end.span()); }
   [[nodiscard]] std::optional<BinaryOpInfo> match_binary_operator(const Token& token) const;

};
