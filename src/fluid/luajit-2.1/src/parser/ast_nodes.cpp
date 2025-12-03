#include "parser/ast_nodes.h"

#include "lj_def.h"

FluidType parse_type_name(std::string_view Name)
{
   struct TypeName {
      std::string_view name;
      FluidType type;
   };

   constexpr TypeName names[] = {
      { "any",      FluidType::Any },
      { "nil",      FluidType::Nil },
      { "bool",     FluidType::Bool },
      { "boolean",  FluidType::Bool },
      { "num",      FluidType::Num },
      { "number",   FluidType::Num },
      { "str",      FluidType::Str },
      { "string",   FluidType::Str },
      { "table",    FluidType::Table },
      { "func",     FluidType::Func },
      { "function", FluidType::Func },
      { "thread",   FluidType::Thread },
      { "cdata",    FluidType::CData },
      { "obj",      FluidType::Object },
      { "object",   FluidType::Object }
   };

   for (const auto &entry : names) {
      if (Name IS entry.name) return entry.type;
   }

   return FluidType::Unknown;
}

std::string_view type_name(FluidType Type)
{
   switch (Type) {
      case FluidType::Nil:    return "nil";
      case FluidType::Bool:   return "bool";
      case FluidType::Num:    return "num";
      case FluidType::Str:    return "str";
      case FluidType::Table:  return "table";
      case FluidType::Func:   return "func";
      case FluidType::Thread: return "thread";
      case FluidType::CData:  return "cdata";
      case FluidType::Object: return "obj";
      case FluidType::Any:
      default: return "any";
   }
}

namespace {

[[nodiscard]] inline bool ensure_operand(const ExprNodePtr &node)
{
   return node != nullptr;
}

inline void assert_node(bool condition, CSTRING message)
{
   lj_assertX(condition, message);
}

[[nodiscard]] inline size_t block_child_count(const std::unique_ptr<BlockStmt> &block)
{
   return block ? block->view().size() : 0;
}

struct CallTargetChildCounter {
   [[nodiscard]] size_t operator()(const DirectCallTarget &Target) const {
      return Target.callable ? 1 : 0;
   }

   [[nodiscard]] size_t operator()(const MethodCallTarget &Target) const {
      return Target.receiver ? 1 : 0;
   }

   [[nodiscard]] size_t operator()(const SafeMethodCallTarget &Target) const
   {
      return Target.receiver ? 1 : 0;
   }
};

struct ExpressionChildCounter {
   [[nodiscard]] inline size_t operator()(const LiteralValue &) const { return 0; }
   [[nodiscard]] inline size_t operator()(const NameRef &) const { return 0; }
   [[nodiscard]] inline size_t operator()(const VarArgExprPayload &) const { return 0; }

   [[nodiscard]] inline size_t operator()(const UnaryExprPayload &Payload) const
   {
      return Payload.operand ? 1 : 0;
   }

   [[nodiscard]] inline size_t operator()(const UpdateExprPayload &Payload) const
   {
      return Payload.target ? 1 : 0;
   }

   [[nodiscard]] inline size_t operator()(const BinaryExprPayload &Payload) const
   {
      size_t total = Payload.left ? 1 : 0;
      if (Payload.right) total++;
      return total;
   }

   [[nodiscard]] inline size_t operator()(const TernaryExprPayload &Payload) const
   {
      size_t total = Payload.condition ? 1 : 0;
      if (Payload.if_true) total++;
      if (Payload.if_false) total++;
      return total;
   }

   [[nodiscard]] inline size_t operator()(const PresenceExprPayload &Payload) const
   {
      return Payload.value ? 1 : 0;
   }

   [[nodiscard]] inline size_t operator()(const PipeExprPayload &Payload) const
   {
      size_t total = Payload.lhs ? 1 : 0;
      if (Payload.rhs_call) total++;
      return total;
   }

   [[nodiscard]] inline size_t operator()(const CallExprPayload &Payload) const
   {
      size_t total = std::visit(CallTargetChildCounter{}, Payload.target);
      total += Payload.arguments.size();
      return total;
   }

   [[nodiscard]] inline size_t operator()(const MemberExprPayload &Payload) const
   {
      return Payload.table ? 1 : 0;
   }

   [[nodiscard]] inline size_t operator()(const IndexExprPayload &Payload) const
   {
      size_t total = Payload.table ? 1 : 0;
      if (Payload.index) total++;
      return total;
   }

   [[nodiscard]] inline size_t operator()(const SafeMemberExprPayload &Payload) const
   {
      return Payload.table ? 1 : 0;
   }

   [[nodiscard]] inline size_t operator()(const SafeIndexExprPayload &Payload) const
   {
      size_t total = Payload.table ? 1 : 0;
      if (Payload.index) total++;
      return total;
   }

   [[nodiscard]] inline size_t operator()(const TableExprPayload &Payload) const
   {
      size_t total = 0;
      for (const TableField &field : Payload.fields) {
         if (field.key) total++;
         if (field.value) total++;
      }
      return total;
   }

   [[nodiscard]] inline size_t operator()(const FunctionExprPayload &Payload) const
   {
      return block_child_count(Payload.body);
   }
};

struct StatementChildCounter {
   [[nodiscard]] inline size_t operator()(const AssignmentStmtPayload &Payload) const
   {
      return Payload.targets.size() + Payload.values.size();
   }

   [[nodiscard]] inline size_t operator()(const LocalDeclStmtPayload &Payload) const
   {
      return Payload.values.size();
   }

   [[nodiscard]] inline size_t operator()(const LocalFunctionStmtPayload &Payload) const
   {
      return Payload.function ? block_child_count(Payload.function->body) : 0;
   }

   [[nodiscard]] inline size_t operator()(const FunctionStmtPayload &Payload) const
   {
      return Payload.function ? block_child_count(Payload.function->body) : 0;
   }

   [[nodiscard]] inline size_t operator()(const IfStmtPayload &Payload) const
   {
      size_t total = 0;
      for (const IfClause& clause : Payload.clauses) {
         if (clause.condition) total++;
         total += block_child_count(clause.block);
      }
      return total;
   }

   [[nodiscard]] inline size_t operator()(const LoopStmtPayload &Payload) const
   {
      size_t total = Payload.condition ? 1 : 0;
      total += block_child_count(Payload.body);
      return total;
   }

   [[nodiscard]] inline size_t operator()(const NumericForStmtPayload &Payload) const
   {
      size_t total = 0;
      if (Payload.start) total++;
      if (Payload.stop) total++;
      if (Payload.step) total++;
      total += block_child_count(Payload.body);
      return total;
   }

   [[nodiscard]] inline size_t operator()(const GenericForStmtPayload &Payload) const
   {
      size_t total = Payload.iterators.size();
      total += block_child_count(Payload.body);
      return total;
   }

   [[nodiscard]] inline size_t operator()(const ReturnStmtPayload &Payload) const
   {
      return Payload.values.size();
   }

   [[nodiscard]] inline size_t operator()(const BreakStmtPayload &) const { return 0; }
   [[nodiscard]] inline size_t operator()(const ContinueStmtPayload &) const { return 0; }

   [[nodiscard]] inline size_t operator()(const DeferStmtPayload &Payload) const
   {
      size_t total = Payload.arguments.size();
      if (Payload.callable) {
         total += block_child_count(Payload.callable->body);
      }
      return total;
   }

   [[nodiscard]] inline size_t operator()(const DoStmtPayload &Payload) const
   {
      return block_child_count(Payload.block);
   }

   [[nodiscard]] inline size_t operator()(const ConditionalShorthandStmtPayload &Payload) const
   {
      size_t total = Payload.condition ? 1 : 0;
      if (Payload.body) total += ast_statement_child_count(*Payload.body);
      return total;
   }

   [[nodiscard]] inline size_t operator()(const ExpressionStmtPayload &Payload) const
   {
      return Payload.expression ? 1 : 0;
   }
};

}  // namespace

DirectCallTarget::~DirectCallTarget() = default;
MethodCallTarget::~MethodCallTarget() = default;
SafeMethodCallTarget::~SafeMethodCallTarget() = default;
UnaryExprPayload::~UnaryExprPayload() = default;
UpdateExprPayload::~UpdateExprPayload() = default;
BinaryExprPayload::~BinaryExprPayload() = default;
TernaryExprPayload::~TernaryExprPayload() = default;
PresenceExprPayload::~PresenceExprPayload() = default;
PipeExprPayload::~PipeExprPayload() = default;
CallExprPayload::~CallExprPayload() = default;
MemberExprPayload::~MemberExprPayload() = default;
IndexExprPayload::~IndexExprPayload() = default;
SafeMemberExprPayload::~SafeMemberExprPayload() = default;
SafeIndexExprPayload::~SafeIndexExprPayload() = default;
TableField::~TableField() = default;
TableExprPayload::~TableExprPayload() = default;
FunctionExprPayload::~FunctionExprPayload() = default;
IfClause::~IfClause() = default;
AssignmentStmtPayload::~AssignmentStmtPayload() = default;
LocalDeclStmtPayload::~LocalDeclStmtPayload() = default;
LocalFunctionStmtPayload::~LocalFunctionStmtPayload() = default;
FunctionStmtPayload::~FunctionStmtPayload() = default;
IfStmtPayload::~IfStmtPayload() = default;
LoopStmtPayload::~LoopStmtPayload() = default;
NumericForStmtPayload::~NumericForStmtPayload() = default;
GenericForStmtPayload::~GenericForStmtPayload() = default;
ReturnStmtPayload::~ReturnStmtPayload() = default;
DeferStmtPayload::~DeferStmtPayload() = default;
DoStmtPayload::~DoStmtPayload() = default;
ExpressionStmtPayload::~ExpressionStmtPayload() = default;
ConditionalShorthandStmtPayload::~ConditionalShorthandStmtPayload() = default;
BlockStmt::~BlockStmt() = default;

ExprNodePtr make_literal_expr(SourceSpan Span, const LiteralValue &Literal)
{
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::LiteralExpr;
   node->span = Span;
   node->data = Literal;
   return node;
}

ExprNodePtr make_identifier_expr(SourceSpan Span, const NameRef &Reference)
{
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::IdentifierExpr;
   node->span = Span;
   node->data = Reference;
   return node;
}

ExprNodePtr make_vararg_expr(SourceSpan Span)
{
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::VarArgExpr;
   node->span = Span;
   node->data.emplace<VarArgExprPayload>();
   return node;
}

ExprNodePtr make_unary_expr(SourceSpan Span, AstUnaryOperator op, ExprNodePtr Operand)
{
   assert_node(ensure_operand(Operand), "unary expression requires operand");
   UnaryExprPayload payload;
   payload.op = op;
   payload.operand = std::move(Operand);
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::UnaryExpr;
   node->span = Span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_update_expr(SourceSpan Span, AstUpdateOperator op, bool is_postfix,
   ExprNodePtr target)
{
   assert_node(ensure_operand(target), "update expression requires target");
   UpdateExprPayload payload;
   payload.op = op;
   payload.is_postfix = is_postfix;
   payload.target = std::move(target);
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::UpdateExpr;
   node->span = Span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_binary_expr(SourceSpan Span, AstBinaryOperator op, ExprNodePtr left,
   ExprNodePtr right)
{
   assert_node(ensure_operand(left) and ensure_operand(right), "binary expression requires operands");
   BinaryExprPayload payload;
   payload.op = op;
   payload.left = std::move(left);
   payload.right = std::move(right);
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::BinaryExpr;
   node->span = Span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_ternary_expr(SourceSpan Span, ExprNodePtr condition, ExprNodePtr if_true, ExprNodePtr if_false)
{
   assert_node(ensure_operand(condition) and ensure_operand(if_true) and ensure_operand(if_false),
      "ternary expression requires three operands");
   TernaryExprPayload payload;
   payload.condition = std::move(condition);
   payload.if_true = std::move(if_true);
   payload.if_false = std::move(if_false);
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::TernaryExpr;
   node->span = Span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_presence_expr(SourceSpan Span, ExprNodePtr value)
{
   assert_node(ensure_operand(value), "presence expression requires operand");
   PresenceExprPayload payload;
   payload.value = std::move(value);
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::PresenceExpr;
   node->span = Span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_pipe_expr(SourceSpan Span, ExprNodePtr lhs, ExprNodePtr rhs_call, uint32_t limit)
{
   assert_node(ensure_operand(lhs) and ensure_operand(rhs_call), "pipe expression requires lhs and rhs_call");
   PipeExprPayload payload;
   payload.lhs = std::move(lhs);
   payload.rhs_call = std::move(rhs_call);
   payload.limit = limit;
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::PipeExpr;
   node->span = Span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_call_expr(SourceSpan Span, ExprNodePtr callee, ExprNodeList arguments, bool forwards_multret)
{
   assert_node(ensure_operand(callee), "call expression requires callee");
   CallExprPayload payload;
   DirectCallTarget target;
   target.callable = std::move(callee);
   payload.target = std::move(target);
   payload.arguments = std::move(arguments);
   payload.forwards_multret = forwards_multret;
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::CallExpr;
   node->span = Span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_method_call_expr(SourceSpan Span, ExprNodePtr receiver, Identifier method, ExprNodeList arguments,
   bool forwards_multret)
{
   assert_node(ensure_operand(receiver), "method call requires receiver");
   CallExprPayload payload;
   MethodCallTarget target;
   target.receiver = std::move(receiver);
   target.method = method;
   payload.target = std::move(target);
   payload.arguments = std::move(arguments);
   payload.forwards_multret = forwards_multret;
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::CallExpr;
   node->span = Span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_safe_method_call_expr(SourceSpan Span, ExprNodePtr receiver, Identifier method, ExprNodeList arguments,
   bool forwards_multret)
{
   assert_node(ensure_operand(receiver), "safe method call requires receiver");
   CallExprPayload payload;
   SafeMethodCallTarget target;
   target.receiver = std::move(receiver);
   target.method = method;
   payload.target = std::move(target);
   payload.arguments = std::move(arguments);
   payload.forwards_multret = forwards_multret;
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::SafeCallExpr;
   node->span = Span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_member_expr(SourceSpan Span, ExprNodePtr Table, Identifier member, bool uses_method_dispatch)
{
   assert_node(ensure_operand(Table), "member expression requires table value");
   MemberExprPayload payload;
   payload.table = std::move(Table);
   payload.member = member;
   payload.uses_method_dispatch = uses_method_dispatch;
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::MemberExpr;
   node->span = Span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_index_expr(SourceSpan Span, ExprNodePtr Table, ExprNodePtr index)
{
   assert_node(ensure_operand(Table) and ensure_operand(index), "index expression requires operands");
   IndexExprPayload payload;
   payload.table = std::move(Table);
   payload.index = std::move(index);
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::IndexExpr;
   node->span = Span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_safe_member_expr(SourceSpan Span, ExprNodePtr Table, Identifier Member)
{
   assert_node(ensure_operand(Table), "safe member expression requires table value");
   SafeMemberExprPayload payload;
   payload.table = std::move(Table);
   payload.member = Member;
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::SafeMemberExpr;
   node->span = Span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_safe_index_expr(SourceSpan Span, ExprNodePtr Table, ExprNodePtr Index)
{
   assert_node(ensure_operand(Table) and ensure_operand(Index), "safe index expression requires operands");
   SafeIndexExprPayload payload;
   payload.table = std::move(Table);
   payload.index = std::move(Index);
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::SafeIndexExpr;
   node->span = Span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_table_expr(SourceSpan Span, std::vector<TableField> fields, bool has_array_part)
{
   TableExprPayload payload;
   payload.fields = std::move(fields);
   payload.has_array_part = has_array_part;
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::TableExpr;
   node->span = Span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_function_expr(SourceSpan Span, std::vector<FunctionParameter> parameters, bool is_vararg, std::unique_ptr<BlockStmt> body)
{
   assert_node(body != nullptr, "function literal body required");
   FunctionExprPayload payload;
   payload.parameters = std::move(parameters);
   payload.is_vararg = is_vararg;
   payload.body = std::move(body);
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::FunctionExpr;
   node->span = Span;
   node->data = std::move(payload);
   return node;
}

std::unique_ptr<FunctionExprPayload> make_function_payload(std::vector<FunctionParameter> parameters,
   bool is_vararg, std::unique_ptr<BlockStmt> body)
{
   assert_node(body != nullptr, "function body required");
   auto payload = std::make_unique<FunctionExprPayload>();
   payload->parameters = std::move(parameters);
   payload->is_vararg = is_vararg;
   payload->body = std::move(body);
   return payload;
}

std::unique_ptr<BlockStmt> make_block(SourceSpan Span, StmtNodeList statements)
{
   auto block = std::make_unique<BlockStmt>();
   block->span = Span;
   block->statements = std::move(statements);
   return block;
}

StmtNodePtr make_assignment_stmt(SourceSpan Span, AssignmentOperator op, ExprNodeList targets, ExprNodeList values)
{
   StmtNodePtr node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::AssignmentStmt;
   node->span = Span;
   AssignmentStmtPayload payload;
   payload.op = op;
   payload.targets = std::move(targets);
   payload.values = std::move(values);
   node->data = std::move(payload);
   return node;
}

StmtNodePtr make_local_decl_stmt(SourceSpan Span, std::vector<Identifier> names, ExprNodeList values)
{
   StmtNodePtr node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::LocalDeclStmt;
   node->span = Span;
   LocalDeclStmtPayload payload;
   payload.names = std::move(names);
   payload.values = std::move(values);
   node->data = std::move(payload);
   return node;
}

StmtNodePtr make_return_stmt(SourceSpan Span, ExprNodeList values, bool forwards_call)
{
   StmtNodePtr node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::ReturnStmt;
   node->span = Span;
   ReturnStmtPayload payload;
   payload.values = std::move(values);
   payload.forwards_call = forwards_call;
   node->data = std::move(payload);
   return node;
}

StmtNodePtr make_expression_stmt(SourceSpan Span, ExprNodePtr expression)
{
   StmtNodePtr node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::ExpressionStmt;
   node->span = Span;
   ExpressionStmtPayload payload;
   payload.expression = std::move(expression);
   node->data = std::move(payload);
   return node;
}

size_t ast_statement_child_count(const StmtNode &Node)
{
   return std::visit(StatementChildCounter{}, Node.data);
}

size_t ast_expression_child_count(const ExprNode &Node)
{
   return std::visit(ExpressionChildCounter{}, Node.data);
}
