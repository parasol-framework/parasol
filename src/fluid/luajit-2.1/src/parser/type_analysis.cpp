#include "parser/type_checker.h"

#include <format>

#include "parser/parser_context.h"

namespace {

[[nodiscard]] InferredType infer_literal_type(const LiteralValue& Literal)
{
   InferredType result;
   result.is_constant = true;

   switch (Literal.kind) {
      case LiteralKind::Nil:
         result.primary = FluidType::Nil;
         result.is_nullable = true;
         break;
      case LiteralKind::Boolean: result.primary = FluidType::Bool; break;
      case LiteralKind::Number:  result.primary = FluidType::Num; break;
      case LiteralKind::String:  result.primary = FluidType::Str; break;
      case LiteralKind::CData:   result.primary = FluidType::CData; break;
   }
   return result;
}

} // namespace

class TypeAnalyser {
public:
   explicit TypeAnalyser(ParserContext &Context) : ctx_(Context) {}
   void analyse_module(const BlockStmt &);
   [[nodiscard]] const std::vector<TypeDiagnostic>& diagnostics() const { return this->diagnostics_; }

private:
   void push_scope();
   void pop_scope();
   [[nodiscard]] TypeCheckScope & current_scope();
   [[nodiscard]] const TypeCheckScope & current_scope() const;

   void analyse_block(const BlockStmt &);
   void analyse_statement(const StmtNode &);
   void analyse_assignment(const AssignmentStmtPayload &);
   void analyse_local_decl(const LocalDeclStmtPayload &);
   void analyse_local_function(const LocalFunctionStmtPayload &);
   void analyse_function_stmt(const FunctionStmtPayload &);
   void analyse_function_payload(const FunctionExprPayload &);
   void analyse_expression(const ExprNode &);
   void analyse_call_expr(const CallExprPayload &);
   void check_arguments(const FunctionExprPayload &, const CallExprPayload &);
   void check_argument_type(const ExprNode &, FluidType, size_t);

   [[nodiscard]] InferredType infer_expression_type(const ExprNode &) const;
   [[nodiscard]] std::optional<InferredType> resolve_identifier(GCstr *) const;
   [[nodiscard]] const FunctionExprPayload * resolve_call_target(const CallTarget &) const;
   [[nodiscard]] const FunctionExprPayload * resolve_function(GCstr *) const;
   void fix_local_type(GCstr *, FluidType);

   ParserContext &ctx_;
   std::vector<TypeCheckScope> scope_stack_{};
   std::vector<TypeDiagnostic> diagnostics_{};
};

void TypeAnalyser::push_scope() {
   this->scope_stack_.emplace_back();
}

void TypeAnalyser::pop_scope() {
   if (not this->scope_stack_.empty()) this->scope_stack_.pop_back();
}

TypeCheckScope & TypeAnalyser::current_scope() {
   if (this->scope_stack_.empty()) this->push_scope();
   return this->scope_stack_.back();
}

const TypeCheckScope & TypeAnalyser::current_scope() const {
   lj_assertX(not this->scope_stack_.empty(), "type analysis scope stack is empty");
   return this->scope_stack_.back();
}

void TypeAnalyser::analyse_module(const BlockStmt &Module)
{
   this->push_scope();
   this->analyse_block(Module);
   this->pop_scope();
}

void TypeAnalyser::analyse_block(const BlockStmt &Block)
{
   for (const auto& statement : Block.view()) {
      this->analyse_statement(statement);
   }
}

void TypeAnalyser::analyse_statement(const StmtNode &Statement)
{
   switch (Statement.kind) {
      case AstNodeKind::AssignmentStmt: {
         auto* payload = std::get_if<AssignmentStmtPayload>(&Statement.data);
         if (payload) this->analyse_assignment(*payload);
         break;
      }
      case AstNodeKind::LocalDeclStmt: {
         auto* payload = std::get_if<LocalDeclStmtPayload>(&Statement.data);
         if (payload) this->analyse_local_decl(*payload);
         break;
      }
      case AstNodeKind::LocalFunctionStmt: {
         auto* payload = std::get_if<LocalFunctionStmtPayload>(&Statement.data);
         if (payload) this->analyse_local_function(*payload);
         break;
      }
      case AstNodeKind::FunctionStmt: {
         auto* payload = std::get_if<FunctionStmtPayload>(&Statement.data);
         if (payload) this->analyse_function_stmt(*payload);
         break;
      }
      case AstNodeKind::IfStmt: {
         auto* payload = std::get_if<IfStmtPayload>(&Statement.data);
         if (payload) {
            for (const auto& clause : payload->clauses) {
               if (clause.condition) this->analyse_expression(*clause.condition);
               if (clause.block) {
                  this->push_scope();
                  this->analyse_block(*clause.block);
                  this->pop_scope();
               }
            }
         }
         break;
      }
      case AstNodeKind::WhileStmt:
      case AstNodeKind::RepeatStmt: {
         auto* payload = std::get_if<LoopStmtPayload>(&Statement.data);
         if (payload) {
            if (payload->condition) this->analyse_expression(*payload->condition);
            if (payload->body) {
               this->push_scope();
               this->analyse_block(*payload->body);
               this->pop_scope();
            }
         }
         break;
      }
      case AstNodeKind::NumericForStmt: {
         auto* payload = std::get_if<NumericForStmtPayload>(&Statement.data);
         if (payload) {
            if (payload->start) this->analyse_expression(*payload->start);
            if (payload->stop) this->analyse_expression(*payload->stop);
            if (payload->step) this->analyse_expression(*payload->step);
            if (payload->body) {
               this->push_scope();
               // For loop control variable is implicitly typed as num
               if (payload->control.symbol) {
                  InferredType loop_var;
                  loop_var.primary = FluidType::Num;
                  this->current_scope().declare_local(payload->control.symbol, loop_var);
               }
               this->analyse_block(*payload->body);
               this->pop_scope();
            }
         }
         break;
      }
      case AstNodeKind::GenericForStmt: {
         auto* payload = std::get_if<GenericForStmtPayload>(&Statement.data);
         if (payload) {
            for (const auto& iterator : payload->iterators) {
               this->analyse_expression(*iterator);
            }
            if (payload->body) {
               this->push_scope();
               // Declare loop variables in the for loop's scope
               for (const auto& name : payload->names) {
                  if (name.symbol) {
                     InferredType loop_var;
                     loop_var.primary = FluidType::Any;  // Type depends on iterator
                     this->current_scope().declare_local(name.symbol, loop_var);
                  }
               }
               this->analyse_block(*payload->body);
               this->pop_scope();
            }
         }
         break;
      }
      case AstNodeKind::ReturnStmt: {
         auto* payload = std::get_if<ReturnStmtPayload>(&Statement.data);
         if (payload) {
            for (const auto& value : payload->values) {
               this->analyse_expression(*value);
            }
         }
         break;
      }
      case AstNodeKind::DeferStmt: {
         auto* payload = std::get_if<DeferStmtPayload>(&Statement.data);
         if (payload) {
            if (payload->callable) this->analyse_function_payload(*payload->callable);
            for (const auto& argument : payload->arguments) {
               this->analyse_expression(*argument);
            }
         }
         break;
      }
      case AstNodeKind::DoStmt: {
         auto* payload = std::get_if<DoStmtPayload>(&Statement.data);
         if (payload and payload->block) {
            this->push_scope();
            this->analyse_block(*payload->block);
            this->pop_scope();
         }
         break;
      }
      case AstNodeKind::ExpressionStmt: {
         auto* payload = std::get_if<ExpressionStmtPayload>(&Statement.data);
         if (payload and payload->expression) this->analyse_expression(*payload->expression);
         break;
      }
      default:
         break;
   }
}

void TypeAnalyser::analyse_assignment(const AssignmentStmtPayload &Payload)
{
   for (size_t i = 0; i < Payload.targets.size(); ++i) {
      const auto& target_ptr = Payload.targets[i];
      if (not target_ptr) continue;
      const auto& target = *target_ptr;

      // Only check local variable assignments
      if (target.kind IS AstNodeKind::IdentifierExpr) {
         auto* name_ref = std::get_if<NameRef>(&target.data);
         if (not name_ref) continue;

         auto existing = this->resolve_identifier(name_ref->identifier.symbol);
         if (not existing) continue;

         if (i >= Payload.values.size()) continue;
         InferredType value_type = this->infer_expression_type(*Payload.values[i]);

         if (existing->is_fixed) {
            // Fixed type: check compatibility
            if (existing->primary IS FluidType::Any) continue; // 'any' accepts everything including nil
            if (value_type.primary IS FluidType::Nil) continue;  // Nil is always allowed as a "clear" operation

            if (value_type.primary != FluidType::Any and value_type.primary != existing->primary) {
               // Type mismatch
               TypeDiagnostic diag;
               diag.location = target.span;
               diag.expected = existing->primary;
               diag.actual = value_type.primary;
               diag.code = ParserErrorCode::TypeMismatchAssignment;
               diag.message = std::format("cannot assign '{}' to variable of type '{}'",
                  type_name(value_type.primary), type_name(existing->primary));
               this->diagnostics_.push_back(std::move(diag));
            }
         }
         else {
            // Unfixed variable: first non-nil assignment fixes the type
            // But don't fix if the variable was explicitly declared as 'any'
            if ((existing->primary != FluidType::Any) and (value_type.primary != FluidType::Nil) and
                (value_type.primary != FluidType::Any)) {
               this->fix_local_type(name_ref->identifier.symbol, value_type.primary);
            }
         }
      }
   }

   // Continue with existing analysis

   for (const auto &value : Payload.values) this->analyse_expression(*value);
   for (const auto &target : Payload.targets) this->analyse_expression(*target);
}

void TypeAnalyser::analyse_local_decl(const LocalDeclStmtPayload &Payload)
{
   size_t value_index = 0;
   for (const auto& name : Payload.names) {
      InferredType inferred;

      // Explicit type annotation takes precedence (Unknown = no annotation)
      if (name.type != FluidType::Unknown) {
         inferred.primary = name.type;
         // 'any' type is not fixed - it accepts any value
         inferred.is_fixed = (name.type != FluidType::Any);

         // Check that initial value matches declared type (if present and not 'any')
         if (name.type != FluidType::Any and value_index < Payload.values.size()) {
            InferredType value_type = this->infer_expression_type(*Payload.values[value_index]);

            // Nil is always allowed as initial value for typed variables
            if (value_type.primary != FluidType::Nil and
                value_type.primary != FluidType::Any and
                value_type.primary != name.type) {
               TypeDiagnostic diag;
               diag.location = Payload.values[value_index]->span;
               diag.expected = name.type;
               diag.actual = value_type.primary;
               diag.code = ParserErrorCode::TypeMismatchAssignment;
               diag.message = std::format("cannot assign '{}' to variable of type '{}'",
                  type_name(value_type.primary), type_name(name.type));
               this->diagnostics_.push_back(std::move(diag));
            }
         }
      }
      else if (value_index < Payload.values.size()) {
         // No annotation: infer type from initial value
         inferred = this->infer_expression_type(*Payload.values[value_index]);

         // Non-nil, non-any initial values fix the type
         if (inferred.primary != FluidType::Nil and inferred.primary != FluidType::Any) {
            inferred.is_fixed = true;
         }
      }
      else {
         // No annotation and no initialiser: starts as nil, type not yet determined
         // Use Nil (not Any) so the first non-nil assignment will fix the type
         inferred.primary = FluidType::Nil;
         inferred.is_fixed = false;
      }

      value_index += 1;
      this->current_scope().declare_local(name.symbol, inferred);
   }

   for (const auto& value : Payload.values) {
      this->analyse_expression(*value);
   }
}

void TypeAnalyser::analyse_local_function(const LocalFunctionStmtPayload &Payload)
{
   const FunctionExprPayload* function = Payload.function.get();
   this->current_scope().declare_function(Payload.name.symbol, function);

   if (function) this->analyse_function_payload(*function);
}

void TypeAnalyser::analyse_function_stmt(const FunctionStmtPayload &Payload)
{
   const FunctionExprPayload* function = Payload.function.get();

   if (not Payload.name.segments.empty()) {
      const Identifier& terminal = Payload.name.segments.back();
      this->current_scope().declare_function(terminal.symbol, function);
   }

   if (Payload.name.method) {
      this->current_scope().declare_function(Payload.name.method->symbol, function);
   }

   if (function) this->analyse_function_payload(*function);
}

void TypeAnalyser::analyse_function_payload(const FunctionExprPayload &Function)
{
   this->push_scope();
   for (const auto& param : Function.parameters) {
      this->current_scope().declare_parameter(param.name.symbol, param.type);
   }

   if (Function.body) this->analyse_block(*Function.body);
   this->pop_scope();
}

void TypeAnalyser::analyse_expression(const ExprNode &Expression)
{
   switch (Expression.kind) {
      case AstNodeKind::UnaryExpr: {
         auto* payload = std::get_if<UnaryExprPayload>(&Expression.data);
         if (payload and payload->operand) this->analyse_expression(*payload->operand);
         break;
      }
      case AstNodeKind::UpdateExpr: {
         auto* payload = std::get_if<UpdateExprPayload>(&Expression.data);
         if (payload and payload->target) this->analyse_expression(*payload->target);
         break;
      }
      case AstNodeKind::BinaryExpr: {
         auto* payload = std::get_if<BinaryExprPayload>(&Expression.data);
         if (payload) {
            if (payload->left) this->analyse_expression(*payload->left);
            if (payload->right) this->analyse_expression(*payload->right);
         }
         break;
      }
      case AstNodeKind::TernaryExpr: {
         auto* payload = std::get_if<TernaryExprPayload>(&Expression.data);
         if (payload) {
            if (payload->condition) this->analyse_expression(*payload->condition);
            if (payload->if_true) this->analyse_expression(*payload->if_true);
            if (payload->if_false) this->analyse_expression(*payload->if_false);
         }
         break;
      }
      case AstNodeKind::PresenceExpr: {
         auto* payload = std::get_if<PresenceExprPayload>(&Expression.data);
         if (payload and payload->value) this->analyse_expression(*payload->value);
         break;
      }
      case AstNodeKind::CallExpr: {
         auto* payload = std::get_if<CallExprPayload>(&Expression.data);
         if (payload) this->analyse_call_expr(*payload);
         break;
      }
      case AstNodeKind::MemberExpr: {
         auto* payload = std::get_if<MemberExprPayload>(&Expression.data);
         if (payload and payload->table) this->analyse_expression(*payload->table);
         break;
      }
      case AstNodeKind::IndexExpr: {
         auto* payload = std::get_if<IndexExprPayload>(&Expression.data);
         if (payload) {
            if (payload->table) this->analyse_expression(*payload->table);
            if (payload->index) this->analyse_expression(*payload->index);
         }
         break;
      }
      case AstNodeKind::SafeMemberExpr: {
         auto* payload = std::get_if<SafeMemberExprPayload>(&Expression.data);
         if (payload and payload->table) this->analyse_expression(*payload->table);
         break;
      }
      case AstNodeKind::SafeIndexExpr: {
         auto* payload = std::get_if<SafeIndexExprPayload>(&Expression.data);
         if (payload) {
            if (payload->table) this->analyse_expression(*payload->table);
            if (payload->index) this->analyse_expression(*payload->index);
         }
         break;
      }
      case AstNodeKind::TableExpr: {
         auto* payload = std::get_if<TableExprPayload>(&Expression.data);
         if (payload) {
            for (const auto& field : payload->fields) {
               if (field.key) this->analyse_expression(*field.key);
               if (field.value) this->analyse_expression(*field.value);
            }
         }
         break;
      }
      case AstNodeKind::FunctionExpr: {
         auto* payload = std::get_if<FunctionExprPayload>(&Expression.data);
         if (payload) this->analyse_function_payload(*payload);
         break;
      }
      default:
         break;
   }
}

void TypeAnalyser::analyse_call_expr(const CallExprPayload &Call)
{
   for (const auto& argument : Call.arguments) {
      this->analyse_expression(*argument);
   }

   const FunctionExprPayload* target = this->resolve_call_target(Call.target);
   if (target) this->check_arguments(*target, Call);
}

void TypeAnalyser::check_arguments(const FunctionExprPayload& Function, const CallExprPayload &Call)
{
   size_t param_index = 0;
   for (const auto& param : Function.parameters) {
      if (param_index >= Call.arguments.size()) break;
      this->check_argument_type(*Call.arguments[param_index], param.type, param_index);
      param_index += 1;
   }
}

void TypeAnalyser::check_argument_type(const ExprNode& Argument, FluidType Expected, size_t Index)
{
   if (Expected IS FluidType::Any) return;

   InferredType actual = this->infer_expression_type(Argument);

   if (not actual.matches(Expected)) {
      TypeDiagnostic diag;
      diag.location = Argument.span;
      diag.expected = Expected;
      diag.actual = actual.primary;
      diag.code = ParserErrorCode::TypeMismatchArgument;
      diag.message = std::format("type mismatch: argument {} expects '{}', got '{}'",
         Index + 1, type_name(Expected), type_name(actual.primary));
      this->diagnostics_.push_back(std::move(diag));
   }
}

InferredType TypeAnalyser::infer_expression_type(const ExprNode& Expr) const
{
   InferredType result;

   switch (Expr.kind) {
      case AstNodeKind::LiteralExpr: {
         auto* payload = std::get_if<LiteralValue>(&Expr.data);
         if (payload) return infer_literal_type(*payload);
         break;
      }
      case AstNodeKind::IdentifierExpr: {
         auto* payload = std::get_if<NameRef>(&Expr.data);
         if (payload) {
            auto resolved = this->resolve_identifier(payload->identifier.symbol);
            if (resolved) return *resolved;
         }
         break;
      }
      case AstNodeKind::TableExpr:
         result.primary = FluidType::Table;
         break;
      case AstNodeKind::FunctionExpr:
         result.primary = FluidType::Func;
         break;
      default:
         result.primary = FluidType::Any;
         break;
   }

   return result;
}

std::optional<InferredType> TypeAnalyser::resolve_identifier(GCstr* Name) const
{
   for (auto it = this->scope_stack_.rbegin(); it != this->scope_stack_.rend(); ++it) {
      auto type = it->lookup_local_type(Name);
      if (type) return type;

      auto param = it->lookup_parameter_type(Name);
      if (param) {
         InferredType inferred;
         inferred.primary = *param;
         return inferred;
      }
   }
   return std::nullopt;
}

const FunctionExprPayload* TypeAnalyser::resolve_call_target(const CallTarget& Target) const
{
   if (std::holds_alternative<DirectCallTarget>(Target)) {
      const auto& direct = std::get<DirectCallTarget>(Target);
      if (direct.callable) {
         if (direct.callable->kind IS AstNodeKind::FunctionExpr) {
            auto* payload = std::get_if<FunctionExprPayload>(&direct.callable->data);
            if (payload) return payload;
         }
         if (direct.callable->kind IS AstNodeKind::IdentifierExpr) {
            auto* payload = std::get_if<NameRef>(&direct.callable->data);
            if (payload) return this->resolve_function(payload->identifier.symbol);
         }
      }
   }
   return nullptr;
}

const FunctionExprPayload* TypeAnalyser::resolve_function(GCstr* Name) const
{
   for (auto it = this->scope_stack_.rbegin(); it != this->scope_stack_.rend(); ++it) {
      const FunctionExprPayload* fn = it->lookup_function(Name);
      if (fn) return fn;
   }
   return nullptr;
}

void TypeAnalyser::fix_local_type(GCstr* Name, FluidType Type)
{
   for (auto it = this->scope_stack_.rbegin(); it != this->scope_stack_.rend(); ++it) {
      auto existing = it->lookup_local_type(Name);
      if (existing) {
         it->fix_local_type(Name, Type);
         return;
      }
   }
}

static void publish_type_diagnostics(ParserContext& Context, const std::vector<TypeDiagnostic>& Diagnostics)
{
   for (const auto& diag : Diagnostics) {
      ParserDiagnostic diagnostic;
      diagnostic.severity = Context.config().type_errors_are_fatal
         ? ParserDiagnosticSeverity::Error
         : ParserDiagnosticSeverity::Warning;
      diagnostic.code = diag.code;
      diagnostic.message = diag.message;
      diagnostic.token = Token::from_span(diag.location);
      Context.diagnostics().report(diagnostic);
   }
}

void run_type_analysis(ParserContext& Context, const BlockStmt& Module)
{
   TypeAnalyser analyser(Context);
   analyser.analyse_module(Module);
   publish_type_diagnostics(Context, analyser.diagnostics());
}
