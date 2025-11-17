#include "parser/ast_nodes.h"

#include <utility>


SourceSpan AstSpan::enclosing_span() const
{
   SourceSpan span = this->start;
   span.column = this->end.column;
   span.offset = this->end.offset;
   span.line = this->end.line ? this->end.line : span.line;
   return span;
}

StatementView::StatementView(const StmtList& statements)
   : storage(&statements)
{
}

StatementView::const_iterator StatementView::begin() const
{
   if (!this->storage) return {};
   return this->storage->begin();
}

StatementView::const_iterator StatementView::end() const
{
   if (!this->storage) return {};
   return this->storage->end();
}

bool StatementView::empty() const
{
   return !this->storage || this->storage->empty();
}

size_t StatementView::size() const
{
   return this->storage ? this->storage->size() : 0;
}

ParameterView::ParameterView(const TokenList& parameters)
   : storage(&parameters)
{
}

ParameterView::const_iterator ParameterView::begin() const
{
   if (!this->storage) return {};
   return this->storage->begin();
}

ParameterView::const_iterator ParameterView::end() const
{
   if (!this->storage) return {};
   return this->storage->end();
}

bool ParameterView::empty() const
{
   return !this->storage || this->storage->empty();
}

size_t ParameterView::size() const
{
   return this->storage ? this->storage->size() : 0;
}

namespace {

[[nodiscard]] static ExprPtr make_simple_expr(AstNodeKind kind, SourceSpan span)
{
   auto node = std::make_unique<ExprNode>();
   node->kind = kind;
   node->span = span;
   return node;
}

}

ExprPtr make_literal_expr(const Token& token)
{
   lj_assertX(token.is_literal(), "literal token required for literal expression");
   auto node = make_simple_expr(AstNodeKind::LiteralExpr, token.span());
   node->data = LiteralNode{ token, token.span() };
   return node;
}

ExprPtr make_identifier_expr(const Token& token)
{
   lj_assertX(token.is_identifier(), "identifier token required for identifier expression");
   auto node = make_simple_expr(AstNodeKind::IdentifierExpr, token.span());
   node->data = IdentifierNode{ token, token.span() };
   return node;
}

ExprPtr make_vararg_expr(const Token& token)
{
   auto node = make_simple_expr(AstNodeKind::VarArgExpr, token.span());
   node->data = VarArgNode{ token, token.span() };
   return node;
}

ExprPtr make_unary_expr(const Token& op, ExprPtr operand, SourceSpan span)
{
   lj_assertX(operand != nullptr, "operand must not be null");
   auto node = make_simple_expr(AstNodeKind::UnaryExpr, span);
   node->data = UnaryExprNode{ op, std::move(operand), span };
   return node;
}

ExprPtr make_binary_expr(const Token& op, ExprPtr left, ExprPtr right, SourceSpan span)
{
   lj_assertX(left != nullptr, "left operand must not be null");
   lj_assertX(right != nullptr, "right operand must not be null");
   auto node = make_simple_expr(AstNodeKind::BinaryExpr, span);
   node->data = BinaryExprNode{ op, std::move(left), std::move(right), span };
   return node;
}

ExprPtr make_ternary_expr(ExprPtr condition, const Token& true_sep, ExprPtr true_branch,
   const Token& false_sep, ExprPtr false_branch, SourceSpan span)
{
   lj_assertX(condition != nullptr, "ternary condition must not be null");
   lj_assertX(true_branch != nullptr, "ternary true branch must not be null");
   lj_assertX(false_branch != nullptr, "ternary false branch must not be null");
   auto node = make_simple_expr(AstNodeKind::TernaryExpr, span);
   node->data = TernaryExprNode{ std::move(condition), true_sep, std::move(true_branch), false_sep,
      std::move(false_branch), span };
   return node;
}

ExprPtr make_call_expr(ExprPtr callee, ExprList arguments, SourceSpan span,
   std::optional<Token> method_name)
{
   lj_assertX(callee != nullptr, "call callee must not be null");
   auto node = make_simple_expr(AstNodeKind::CallExpr, span);
   node->data = CallExprNode{ std::move(callee), std::move(arguments), std::move(method_name), span };
   return node;
}

ExprPtr make_index_expr(ExprPtr table, ExprPtr index, SourceSpan span)
{
   lj_assertX(table != nullptr, "table expression must not be null");
   lj_assertX(index != nullptr, "index expression must not be null");
   auto node = make_simple_expr(AstNodeKind::IndexExpr, span);
   node->data = IndexExprNode{ std::move(table), std::move(index), span };
   return node;
}

ExprPtr make_table_expr(std::vector<TableFieldNode> fields, SourceSpan span)
{
   auto node = make_simple_expr(AstNodeKind::TableCtorExpr, span);
   node->data = TableCtorExprNode{ std::move(fields), span };
   return node;
}

ExprPtr make_function_expr(FunctionSignatureNode signature, BlockPtr body, SourceSpan span)
{
   lj_assertX(body != nullptr, "function body must not be null");
   auto node = make_simple_expr(AstNodeKind::FunctionExpr, span);
   node->data = FunctionExprNode{ std::move(signature), std::move(body), span };
   return node;
}

StmtPtr make_assignment_stmt(const Token& op, ExprList targets, ExprList values, SourceSpan span)
{
   lj_assertX(!targets.empty(), "assignment requires at least one target");
   auto node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::AssignmentStmt;
   node->span = span;
   node->data = AssignmentStmtNode{ std::move(targets), std::move(values), op, span };
   return node;
}

StmtPtr make_local_stmt(TokenList names, ExprList values, SourceSpan span)
{
   lj_assertX(!names.empty(), "local statement requires at least one binding");
   auto node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::LocalStmt;
   node->span = span;
   node->data = LocalStmtNode{ std::move(names), std::move(values), span };
   return node;
}

StmtPtr make_expression_stmt(ExprPtr expression, SourceSpan span)
{
   lj_assertX(expression != nullptr, "expression statement requires an expression");
   auto node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::ExpressionStmt;
   node->span = span;
   node->data = ExpressionStmtNode{ std::move(expression), span };
   return node;
}

StmtPtr make_if_stmt(std::vector<IfBranchNode> branches, BlockPtr else_branch, SourceSpan span)
{
   lj_assertX(!branches.empty(), "if statement requires at least one branch");
   auto node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::IfStmt;
   node->span = span;
   node->data = IfStmtNode{ std::move(branches), std::move(else_branch), span };
   return node;
}

StmtPtr make_while_stmt(const Token& keyword, ExprPtr condition, BlockPtr body, SourceSpan span)
{
   lj_assertX(condition != nullptr, "while statement requires a condition");
   lj_assertX(body != nullptr, "while statement requires a body");
   auto node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::WhileStmt;
   node->span = span;
   node->data = WhileStmtNode{ keyword, std::move(condition), std::move(body), span };
   return node;
}

StmtPtr make_repeat_stmt(const Token& keyword, BlockPtr body, ExprPtr condition, SourceSpan span)
{
   lj_assertX(body != nullptr, "repeat statement requires a body");
   lj_assertX(condition != nullptr, "repeat statement requires a condition");
   auto node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::RepeatStmt;
   node->span = span;
   node->data = RepeatStmtNode{ keyword, std::move(body), std::move(condition), span };
   return node;
}

StmtPtr make_numeric_for_stmt(const Token& variable, ExprPtr initial, ExprPtr limit,
   ExprPtr step, BlockPtr body, SourceSpan span)
{
   lj_assertX(initial != nullptr, "numeric for requires an initial expression");
   lj_assertX(limit != nullptr, "numeric for requires a limit expression");
   lj_assertX(body != nullptr, "numeric for requires a loop body");
   auto node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::NumericForStmt;
   node->span = span;
   node->data = NumericForStmtNode{ variable, std::move(initial), std::move(limit), std::move(step),
      std::move(body), span };
   return node;
}

StmtPtr make_generic_for_stmt(TokenList variables, ExprList expressions, BlockPtr body,
   SourceSpan span)
{
   lj_assertX(!variables.empty(), "generic for requires loop variables");
   lj_assertX(body != nullptr, "generic for requires a loop body");
   auto node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::GenericForStmt;
   node->span = span;
   node->data = GenericForStmtNode{ std::move(variables), std::move(expressions), std::move(body), span };
   return node;
}

StmtPtr make_function_stmt(std::vector<Token> path, FunctionExprNode function, SourceSpan span)
{
   lj_assertX(!path.empty(), "function statements require a qualified name");
   auto node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::FunctionStmt;
   node->span = span;
   node->data = FunctionStmtNode{ std::move(path), std::move(function), span };
   return node;
}

StmtPtr make_return_stmt(const Token& keyword, ExprList values, SourceSpan span)
{
   auto node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::ReturnStmt;
   node->span = span;
   node->data = ReturnStmtNode{ keyword, std::move(values), span };
   return node;
}

StmtPtr make_break_stmt(const Token& keyword)
{
   auto node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::BreakStmt;
   node->span = keyword.span();
   node->data = BreakStmtNode{ keyword, keyword.span() };
   return node;
}

StmtPtr make_continue_stmt(const Token& keyword)
{
   auto node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::ContinueStmt;
   node->span = keyword.span();
   node->data = ContinueStmtNode{ keyword, keyword.span() };
   return node;
}

StmtPtr make_defer_stmt(const Token& keyword, BlockPtr body, SourceSpan span)
{
   lj_assertX(body != nullptr, "defer statement requires a body block");
   auto node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::DeferStmt;
   node->span = span;
   node->data = DeferStmtNode{ keyword, std::move(body), span };
   return node;
}

BlockPtr make_block(StmtList statements, SourceSpan span)
{
   auto block = std::make_unique<BlockNode>();
   block->span = span;
   block->statements = std::move(statements);
   return block;
}

