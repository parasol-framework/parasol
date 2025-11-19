#include "parser/ast_nodes.h"

#include "lj_def.h"

namespace {

[[nodiscard]] static bool ensure_operand(const ExprNodePtr& node)
{
   return node != nullptr;
}

static void assert_node(bool condition, const char* message)
{
   lj_assertX(condition, message);
}

[[nodiscard]] static size_t block_child_count(const std::unique_ptr<BlockStmt>& block)
{
   return block ? block->view().size() : 0;
}

struct CallTargetChildCounter {
   [[nodiscard]] size_t operator()(const DirectCallTarget& target) const
   {
      return target.callable ? 1 : 0;
   }

   [[nodiscard]] size_t operator()(const MethodCallTarget& target) const
   {
      return target.receiver ? 1 : 0;
   }
};

struct ExpressionChildCounter {
   [[nodiscard]] size_t operator()(const LiteralValue&) const { return 0; }
   [[nodiscard]] size_t operator()(const NameRef&) const { return 0; }
   [[nodiscard]] size_t operator()(const VarArgExprPayload&) const { return 0; }

   [[nodiscard]] size_t operator()(const UnaryExprPayload& payload) const
   {
      return payload.operand ? 1 : 0;
   }

   [[nodiscard]] size_t operator()(const UpdateExprPayload& payload) const
   {
      return payload.target ? 1 : 0;
   }

   [[nodiscard]] size_t operator()(const BinaryExprPayload& payload) const
   {
      size_t total = payload.left ? 1 : 0;
      if (payload.right) total++;
      return total;
   }

   [[nodiscard]] size_t operator()(const TernaryExprPayload& payload) const
   {
      size_t total = payload.condition ? 1 : 0;
      if (payload.if_true) total++;
      if (payload.if_false) total++;
      return total;
   }

   [[nodiscard]] size_t operator()(const PresenceExprPayload& payload) const
   {
      return payload.value ? 1 : 0;
   }

   [[nodiscard]] size_t operator()(const CallExprPayload& payload) const
   {
      size_t total = std::visit(CallTargetChildCounter{}, payload.target);
      total += payload.arguments.size();
      return total;
   }

   [[nodiscard]] size_t operator()(const MemberExprPayload& payload) const
   {
      return payload.table ? 1 : 0;
   }

   [[nodiscard]] size_t operator()(const IndexExprPayload& payload) const
   {
      size_t total = payload.table ? 1 : 0;
      if (payload.index) total++;
      return total;
   }

   [[nodiscard]] size_t operator()(const TableExprPayload& payload) const
   {
      size_t total = 0;
      for (const TableField& field : payload.fields) {
         if (field.key) total++;
         if (field.value) total++;
      }
      return total;
   }

   [[nodiscard]] size_t operator()(const FunctionExprPayload& payload) const
   {
      return block_child_count(payload.body);
   }
};

struct StatementChildCounter {
   [[nodiscard]] size_t operator()(const AssignmentStmtPayload& payload) const
   {
      return payload.targets.size() + payload.values.size();
   }

   [[nodiscard]] size_t operator()(const LocalDeclStmtPayload& payload) const
   {
      return payload.values.size();
   }

   [[nodiscard]] size_t operator()(const LocalFunctionStmtPayload& payload) const
   {
      return payload.function ? block_child_count(payload.function->body) : 0;
   }

   [[nodiscard]] size_t operator()(const FunctionStmtPayload& payload) const
   {
      return payload.function ? block_child_count(payload.function->body) : 0;
   }

   [[nodiscard]] size_t operator()(const IfStmtPayload& payload) const
   {
      size_t total = 0;
      for (const IfClause& clause : payload.clauses) {
         if (clause.condition) total++;
         total += block_child_count(clause.block);
      }
      return total;
   }

   [[nodiscard]] size_t operator()(const LoopStmtPayload& payload) const
   {
      size_t total = payload.condition ? 1 : 0;
      total += block_child_count(payload.body);
      return total;
   }

   [[nodiscard]] size_t operator()(const NumericForStmtPayload& payload) const
   {
      size_t total = 0;
      if (payload.start) total++;
      if (payload.stop) total++;
      if (payload.step) total++;
      total += block_child_count(payload.body);
      return total;
   }

   [[nodiscard]] size_t operator()(const GenericForStmtPayload& payload) const
   {
      size_t total = payload.iterators.size();
      total += block_child_count(payload.body);
      return total;
   }

   [[nodiscard]] size_t operator()(const ReturnStmtPayload& payload) const
   {
      return payload.values.size();
   }

   [[nodiscard]] size_t operator()(const BreakStmtPayload&) const { return 0; }
   [[nodiscard]] size_t operator()(const ContinueStmtPayload&) const { return 0; }

   [[nodiscard]] size_t operator()(const DeferStmtPayload& payload) const
   {
      size_t total = payload.arguments.size();
      if (payload.callable) {
         total += block_child_count(payload.callable->body);
      }
      return total;
   }

   [[nodiscard]] size_t operator()(const DoStmtPayload& payload) const
   {
      return block_child_count(payload.block);
   }

   [[nodiscard]] size_t operator()(const ExpressionStmtPayload& payload) const
   {
      return payload.expression ? 1 : 0;
   }
};

}  // namespace

DirectCallTarget::~DirectCallTarget() = default;
MethodCallTarget::~MethodCallTarget() = default;
UnaryExprPayload::~UnaryExprPayload() = default;
UpdateExprPayload::~UpdateExprPayload() = default;
BinaryExprPayload::~BinaryExprPayload() = default;
TernaryExprPayload::~TernaryExprPayload() = default;
PresenceExprPayload::~PresenceExprPayload() = default;
CallExprPayload::~CallExprPayload() = default;
MemberExprPayload::~MemberExprPayload() = default;
IndexExprPayload::~IndexExprPayload() = default;
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
BlockStmt::~BlockStmt() = default;

StatementListView::StatementListView(const StmtNodeList& nodes)
   : storage(&nodes)
{
}

StatementListView::Iterator::Iterator(InnerIterator it) : iter(it) {}

const StmtNode& StatementListView::Iterator::operator*() const
{
   return *(*this->iter);
}

const StmtNode* StatementListView::Iterator::operator->() const
{
   return this->iter->get();
}

StatementListView::Iterator& StatementListView::Iterator::operator++()
{
   ++this->iter;
   return *this;
}

bool StatementListView::Iterator::operator==(const Iterator& other) const
{
   return this->iter == other.iter;
}

bool StatementListView::Iterator::operator!=(const Iterator& other) const
{
   return !(*this == other);
}

StatementListView::Iterator StatementListView::begin() const
{
   return this->storage ? Iterator(this->storage->begin()) : Iterator();
}

StatementListView::Iterator StatementListView::end() const
{
   return this->storage ? Iterator(this->storage->end()) : Iterator();
}

size_t StatementListView::size() const
{
   return this->storage ? this->storage->size() : 0;
}

bool StatementListView::empty() const
{
   return this->size() == 0;
}

const StmtNode& StatementListView::operator[](size_t index) const
{
   lj_assertX(this->storage and index < this->storage->size(), "statement index out of range");
   return *(*this->storage)[index];
}

ExpressionListView::ExpressionListView(const ExprNodeList& nodes)
   : storage(&nodes)
{
}

ExpressionListView::Iterator::Iterator(InnerIterator it) : iter(it) {}

const ExprNode& ExpressionListView::Iterator::operator*() const
{
   return *(*this->iter);
}

const ExprNode* ExpressionListView::Iterator::operator->() const
{
   return this->iter->get();
}

ExpressionListView::Iterator& ExpressionListView::Iterator::operator++()
{
   ++this->iter;
   return *this;
}

bool ExpressionListView::Iterator::operator==(const Iterator& other) const
{
   return this->iter == other.iter;
}

bool ExpressionListView::Iterator::operator!=(const Iterator& other) const
{
   return !(*this == other);
}

ExpressionListView::Iterator ExpressionListView::begin() const
{
   return this->storage ? Iterator(this->storage->begin()) : Iterator();
}

ExpressionListView::Iterator ExpressionListView::end() const
{
   return this->storage ? Iterator(this->storage->end()) : Iterator();
}

size_t ExpressionListView::size() const
{
   return this->storage ? this->storage->size() : 0;
}

bool ExpressionListView::empty() const
{
   return this->size() == 0;
}

const ExprNode& ExpressionListView::operator[](size_t index) const
{
   lj_assertX(this->storage and index < this->storage->size(), "expression index out of range");
   return *(*this->storage)[index];
}

StatementListView BlockStmt::view() const
{
   return StatementListView(this->statements);
}

ExprNodePtr make_literal_expr(SourceSpan span, const LiteralValue& literal)
{
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::LiteralExpr;
   node->span = span;
   node->data = literal;
   return node;
}

ExprNodePtr make_identifier_expr(SourceSpan span, const NameRef& reference)
{
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::IdentifierExpr;
   node->span = span;
   node->data = reference;
   return node;
}

ExprNodePtr make_vararg_expr(SourceSpan span)
{
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::VarArgExpr;
   node->span = span;
   node->data.emplace<VarArgExprPayload>();
   return node;
}

ExprNodePtr make_unary_expr(SourceSpan span, AstUnaryOperator op, ExprNodePtr operand)
{
   assert_node(ensure_operand(operand), "unary expression requires operand");
   UnaryExprPayload payload;
   payload.op = op;
   payload.operand = std::move(operand);
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::UnaryExpr;
   node->span = span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_update_expr(SourceSpan span, AstUpdateOperator op, bool is_postfix,
   ExprNodePtr target)
{
   assert_node(ensure_operand(target), "update expression requires target");
   UpdateExprPayload payload;
   payload.op = op;
   payload.is_postfix = is_postfix;
   payload.target = std::move(target);
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::UpdateExpr;
   node->span = span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_binary_expr(SourceSpan span, AstBinaryOperator op, ExprNodePtr left,
   ExprNodePtr right)
{
   assert_node(ensure_operand(left) and ensure_operand(right), "binary expression requires operands");
   BinaryExprPayload payload;
   payload.op = op;
   payload.left = std::move(left);
   payload.right = std::move(right);
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::BinaryExpr;
   node->span = span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_ternary_expr(SourceSpan span, ExprNodePtr condition,
   ExprNodePtr if_true, ExprNodePtr if_false)
{
   assert_node(ensure_operand(condition) and ensure_operand(if_true) and ensure_operand(if_false),
      "ternary expression requires three operands");
   TernaryExprPayload payload;
   payload.condition = std::move(condition);
   payload.if_true = std::move(if_true);
   payload.if_false = std::move(if_false);
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::TernaryExpr;
   node->span = span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_presence_expr(SourceSpan span, ExprNodePtr value)
{
   assert_node(ensure_operand(value), "presence expression requires operand");
   PresenceExprPayload payload;
   payload.value = std::move(value);
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::PresenceExpr;
   node->span = span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_call_expr(SourceSpan span, ExprNodePtr callee,
   ExprNodeList arguments, bool forwards_multret)
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
   node->span = span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_method_call_expr(SourceSpan span, ExprNodePtr receiver,
   Identifier method, ExprNodeList arguments, bool forwards_multret)
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
   node->span = span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_member_expr(SourceSpan span, ExprNodePtr table, Identifier member,
   bool uses_method_dispatch)
{
   assert_node(ensure_operand(table), "member expression requires table value");
   MemberExprPayload payload;
   payload.table = std::move(table);
   payload.member = member;
   payload.uses_method_dispatch = uses_method_dispatch;
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::MemberExpr;
   node->span = span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_index_expr(SourceSpan span, ExprNodePtr table, ExprNodePtr index)
{
   assert_node(ensure_operand(table) and ensure_operand(index), "index expression requires operands");
   IndexExprPayload payload;
   payload.table = std::move(table);
   payload.index = std::move(index);
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::IndexExpr;
   node->span = span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_table_expr(SourceSpan span, std::vector<TableField> fields,
   bool has_array_part)
{
   TableExprPayload payload;
   payload.fields = std::move(fields);
   payload.has_array_part = has_array_part;
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::TableExpr;
   node->span = span;
   node->data = std::move(payload);
   return node;
}

ExprNodePtr make_function_expr(SourceSpan span, std::vector<FunctionParameter> parameters,
   bool is_vararg, std::unique_ptr<BlockStmt> body)
{
   assert_node(body != nullptr, "function literal body required");
   FunctionExprPayload payload;
   payload.parameters = std::move(parameters);
   payload.is_vararg = is_vararg;
   payload.body = std::move(body);
   ExprNodePtr node = std::make_unique<ExprNode>();
   node->kind = AstNodeKind::FunctionExpr;
   node->span = span;
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

std::unique_ptr<BlockStmt> make_block(SourceSpan span, StmtNodeList statements)
{
   auto block = std::make_unique<BlockStmt>();
   block->span = span;
   block->statements = std::move(statements);
   return block;
}

StmtNodePtr make_assignment_stmt(SourceSpan span, AssignmentOperator op,
   ExprNodeList targets, ExprNodeList values)
{
   StmtNodePtr node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::AssignmentStmt;
   node->span = span;
   AssignmentStmtPayload payload;
   payload.op = op;
   payload.targets = std::move(targets);
   payload.values = std::move(values);
   node->data = std::move(payload);
   return node;
}

StmtNodePtr make_local_decl_stmt(SourceSpan span, std::vector<Identifier> names,
   ExprNodeList values)
{
   StmtNodePtr node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::LocalDeclStmt;
   node->span = span;
   LocalDeclStmtPayload payload;
   payload.names = std::move(names);
   payload.values = std::move(values);
   node->data = std::move(payload);
   return node;
}

StmtNodePtr make_return_stmt(SourceSpan span, ExprNodeList values, bool forwards_call)
{
   StmtNodePtr node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::ReturnStmt;
   node->span = span;
   ReturnStmtPayload payload;
   payload.values = std::move(values);
   payload.forwards_call = forwards_call;
   node->data = std::move(payload);
   return node;
}

StmtNodePtr make_expression_stmt(SourceSpan span, ExprNodePtr expression)
{
   StmtNodePtr node = std::make_unique<StmtNode>();
   node->kind = AstNodeKind::ExpressionStmt;
   node->span = span;
   ExpressionStmtPayload payload;
   payload.expression = std::move(expression);
   node->data = std::move(payload);
   return node;
}

size_t ast_statement_child_count(const StmtNode& node)
{
   return std::visit(StatementChildCounter{}, node.data);
}

size_t ast_expression_child_count(const ExprNode& node)
{
   return std::visit(ExpressionChildCounter{}, node.data);
}
