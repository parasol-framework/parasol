#include "parser/type_checker.h"

#include <format>

#include <parasol/main.h>
#include "parser/parser_context.h"

namespace {

[[nodiscard]] InferredType infer_literal_type(const LiteralValue &Literal)
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

// Helper to check if type tracing is enabled
[[nodiscard]] inline bool should_trace_types(lua_State* L)
{
   auto prv = (prvFluid *)L->script->ChildPrivate;
   return (prv->JitOptions & JOF::TRACE_TYPES) != JOF::NIL;
}

} // namespace

class TypeAnalyser {
public:
   explicit TypeAnalyser(ParserContext &Context) : ctx_(Context) {}
   void analyse_module(const BlockStmt &);
   [[nodiscard]] const std::vector<TypeDiagnostic> & diagnostics() const { return this->diagnostics_; }

private:
   void push_scope();
   void pop_scope();
   [[nodiscard]] TypeCheckScope & current_scope();
   [[nodiscard]] const TypeCheckScope & current_scope() const;

   // Function context stack management for return type validation
   void enter_function(const FunctionExprPayload &, GCstr *Name = nullptr);
   void leave_function();
   [[nodiscard]] FunctionContext* current_function();
   [[nodiscard]] const FunctionContext* current_function() const;

   void analyse_block(const BlockStmt &);
   void analyse_statement(const StmtNode &);
   void analyse_assignment(const AssignmentStmtPayload &);
   void analyse_local_decl(const LocalDeclStmtPayload &);
   void analyse_local_function(const LocalFunctionStmtPayload &);
   void analyse_function_stmt(const FunctionStmtPayload &);
   void analyse_function_payload(const FunctionExprPayload &, GCstr *Name = nullptr);
   void analyse_expression(const ExprNode &);
   void analyse_call_expr(const CallExprPayload &);
   void check_arguments(const FunctionExprPayload &, const CallExprPayload &);
   void check_argument_type(const ExprNode &, FluidType, size_t);

   [[nodiscard]] InferredType infer_expression_type(const ExprNode &) const;
   [[nodiscard]] InferredType infer_call_return_type(const ExprNode &, size_t Position) const;
   [[nodiscard]] std::optional<InferredType> resolve_identifier(GCstr *) const;
   [[nodiscard]] const FunctionExprPayload * resolve_call_target(const CallTarget &) const;
   [[nodiscard]] const FunctionExprPayload * resolve_function(GCstr *) const;
   void fix_local_type(GCstr *, FluidType);

   // Return type validation

   void validate_return_types(const ReturnStmtPayload &, SourceSpan);
   [[nodiscard]] bool is_recursive_function(const FunctionExprPayload &, GCstr *) const;
   [[nodiscard]] bool function_has_return_values(const FunctionExprPayload &) const;
   [[nodiscard]] bool body_has_return_values(const BlockStmt &) const;
   [[nodiscard]] bool body_contains_call_to(const BlockStmt &, GCstr *) const;
   [[nodiscard]] bool statement_contains_call_to(const StmtNode &, GCstr *) const;
   [[nodiscard]] bool expression_contains_call_to(const ExprNode &, GCstr *) const;

   // Tracing helper
   [[nodiscard]] bool trace_enabled() const { return should_trace_types(&this->ctx_.lua()); }
   void trace_infer(BCLine Line, std::string_view Context, FluidType Type) const;
   void trace_fix(BCLine Line, GCstr* Name, FluidType Type) const;
   void trace_decl(BCLine Line, GCstr* Name, FluidType Type, bool IsFixed) const;

   ParserContext &ctx_;
   std::vector<TypeCheckScope> scope_stack_{};
   std::vector<FunctionContext> function_stack_{};  // Stack of function contexts for return type tracking
   std::vector<TypeDiagnostic> diagnostics_{};
};

//********************************************************************************************************************
// Tracing implementations

void TypeAnalyser::trace_infer(BCLine Line, std::string_view Context, FluidType Type) const
{
   if (not this->trace_enabled()) return;
   auto type_str = type_name(Type);
   pf::Log("TypeCheck").msg("[%d] infer %.*s -> %.*s", Line, int(Context.size()), Context.data(),
      int(type_str.size()), type_str.data());
}

void TypeAnalyser::trace_fix(BCLine Line, GCstr* Name, FluidType Type) const
{
   if (not this->trace_enabled()) return;
   std::string_view name_view = Name ? std::string_view(strdata(Name), Name->len) : std::string_view("<unknown>");
   auto type_str = type_name(Type);
   pf::Log("TypeCheck").msg("[%d] fix '%.*s' -> %.*s", Line, int(name_view.size()), name_view.data(),
      int(type_str.size()), type_str.data());
}

void TypeAnalyser::trace_decl(BCLine Line, GCstr* Name, FluidType Type, bool IsFixed) const
{
   if (not this->trace_enabled()) return;
   std::string_view name_view = Name ? std::string_view(strdata(Name), Name->len) : std::string_view("<unknown>");
   auto type_str = type_name(Type);
   pf::Log("TypeCheck").msg("[%d] decl '%.*s': %.*s%s", Line, int(name_view.size()), name_view.data(),
      int(type_str.size()), type_str.data(), IsFixed ? " (fixed)" : "");
}

//********************************************************************************************************************

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

void TypeAnalyser::enter_function(const FunctionExprPayload &Function, GCstr *Name)
{
   FunctionContext ctx;
   ctx.function = &Function;
   ctx.function_name = Name;

   // If function has explicit return types, use them
   if (Function.return_types.is_explicit) {
      ctx.expected_returns = Function.return_types;
      ctx.return_type_inferred = true;  // Explicit types are considered "inferred" for validation purposes
   }

   this->function_stack_.push_back(ctx);
}

void TypeAnalyser::leave_function()
{
   if (not this->function_stack_.empty()) {
      this->function_stack_.pop_back();
   }
}

FunctionContext * TypeAnalyser::current_function()
{
   if (this->function_stack_.empty()) return nullptr;
   return &this->function_stack_.back();
}

const FunctionContext* TypeAnalyser::current_function() const
{
   if (this->function_stack_.empty()) return nullptr;
   return &this->function_stack_.back();
}

void TypeAnalyser::analyse_module(const BlockStmt &Module)
{
   this->push_scope();
   this->analyse_block(Module);
   this->pop_scope();
}

void TypeAnalyser::analyse_block(const BlockStmt &Block)
{
   for (const auto &statement : Block.view()) {
      this->analyse_statement(statement);
   }
}

void TypeAnalyser::analyse_statement(const StmtNode &Statement)
{
   switch (Statement.kind) {
      case AstNodeKind::AssignmentStmt: {
         auto *payload = std::get_if<AssignmentStmtPayload>(&Statement.data);
         if (payload) this->analyse_assignment(*payload);
         break;
      }
      case AstNodeKind::LocalDeclStmt: {
         auto *payload = std::get_if<LocalDeclStmtPayload>(&Statement.data);
         if (payload) this->analyse_local_decl(*payload);
         break;
      }
      case AstNodeKind::LocalFunctionStmt: {
         auto *payload = std::get_if<LocalFunctionStmtPayload>(&Statement.data);
         if (payload) this->analyse_local_function(*payload);
         break;
      }
      case AstNodeKind::FunctionStmt: {
         auto *payload = std::get_if<FunctionStmtPayload>(&Statement.data);
         if (payload) this->analyse_function_stmt(*payload);
         break;
      }
      case AstNodeKind::IfStmt: {
         auto *payload = std::get_if<IfStmtPayload>(&Statement.data);
         if (payload) {
            for (const auto &clause : payload->clauses) {
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
         auto *payload = std::get_if<LoopStmtPayload>(&Statement.data);
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
         auto *payload = std::get_if<NumericForStmtPayload>(&Statement.data);
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
         auto *payload = std::get_if<GenericForStmtPayload>(&Statement.data);
         if (payload) {
            for (const auto &iterator : payload->iterators) {
               this->analyse_expression(*iterator);
            }
            if (payload->body) {
               this->push_scope();
               // Declare loop variables in the for loop's scope
               for (const auto &name : payload->names) {
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
         auto *payload = std::get_if<ReturnStmtPayload>(&Statement.data);
         if (payload) {
            for (const auto &value : payload->values) {
               this->analyse_expression(*value);
            }
            // Validate return types against function declaration
            this->validate_return_types(*payload, Statement.span);
         }
         break;
      }
      case AstNodeKind::DeferStmt: {
         auto *payload = std::get_if<DeferStmtPayload>(&Statement.data);
         if (payload) {
            if (payload->callable) this->analyse_function_payload(*payload->callable);
            for (const auto &argument : payload->arguments) {
               this->analyse_expression(*argument);
            }
         }
         break;
      }
      case AstNodeKind::DoStmt: {
         auto *payload = std::get_if<DoStmtPayload>(&Statement.data);
         if (payload and payload->block) {
            this->push_scope();
            this->analyse_block(*payload->block);
            this->pop_scope();
         }
         break;
      }
      case AstNodeKind::ExpressionStmt: {
         auto *payload = std::get_if<ExpressionStmtPayload>(&Statement.data);
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
      const auto &target_ptr = Payload.targets[i];
      if (not target_ptr) continue;
      const auto &target = *target_ptr;

      // Only check local variable assignments
      if (target.kind IS AstNodeKind::IdentifierExpr) {
         auto *name_ref = std::get_if<NameRef>(&target.data);
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
   // Track which position we're at for multi-value returns from function calls
   // When a function call is the last (or only) value, it may provide multiple return values
   size_t value_index = 0;
   size_t call_return_index = 0;  // Position within a multi-return call
   const ExprNode* multi_return_call = nullptr;  // The function call providing multi-returns

   for (size_t name_index = 0; name_index < Payload.names.size(); ++name_index) {
      const auto& name = Payload.names[name_index];
      InferredType inferred;
      InferredType value_type;
      bool have_value_type = false;

      // Determine the value type for this variable
      if (value_index < Payload.values.size()) {
         // We have an explicit value at this position
         const ExprNode& value_expr = *Payload.values[value_index];
         value_type = this->infer_expression_type(value_expr);
         have_value_type = true;

         // If this is the last value and it's a call expression, it may provide multiple returns
         if (value_index == Payload.values.size() - 1 and value_expr.kind IS AstNodeKind::CallExpr) {
            multi_return_call = &value_expr;
            call_return_index = 0;
         }

         value_index += 1;
      }
      else if (multi_return_call) {
         // No more explicit values, but we have a trailing function call
         // Use the next return value position from the multi-return call
         call_return_index += 1;
         value_type = this->infer_call_return_type(*multi_return_call, call_return_index);
         have_value_type = (value_type.primary != FluidType::Any);
      }

      // Explicit type annotation takes precedence (Unknown = no annotation)
      if (name.type != FluidType::Unknown) {
         inferred.primary = name.type;
         // 'any' type is not fixed - it accepts any value
         inferred.is_fixed = (name.type != FluidType::Any);

         // Check that initial value matches declared type (if present and not 'any')
         if (name.type != FluidType::Any and have_value_type) {
            // Nil is always allowed as initial value for typed variables
            if (value_type.primary != FluidType::Nil and
                value_type.primary != FluidType::Any and
                value_type.primary != name.type) {
               TypeDiagnostic diag;
               if (value_index > 0 and value_index - 1 < Payload.values.size()) {
                  diag.location = Payload.values[value_index - 1]->span;
               }
               diag.expected = name.type;
               diag.actual = value_type.primary;
               diag.code = ParserErrorCode::TypeMismatchAssignment;
               diag.message = std::format("cannot assign '{}' to variable of type '{}'",
                  type_name(value_type.primary), type_name(name.type));
               this->diagnostics_.push_back(std::move(diag));
            }
         }
      }
      else if (have_value_type) {
         // No annotation: infer type from initial value
         inferred = value_type;

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

      this->current_scope().declare_local(name.symbol, inferred);
      this->trace_decl(this->ctx_.lex().linenumber, name.symbol, inferred.primary, inferred.is_fixed);
   }

   for (const auto &value : Payload.values) {
      this->analyse_expression(*value);
   }
}

void TypeAnalyser::analyse_local_function(const LocalFunctionStmtPayload &Payload)
{
   const FunctionExprPayload* function = Payload.function.get();
   this->current_scope().declare_function(Payload.name.symbol, function);

   if (function) this->analyse_function_payload(*function, Payload.name.symbol);
}

void TypeAnalyser::analyse_function_stmt(const FunctionStmtPayload &Payload)
{
   const FunctionExprPayload* function = Payload.function.get();
   GCstr *function_name = nullptr;

   if (not Payload.name.segments.empty()) {
      const Identifier& terminal = Payload.name.segments.back();
      this->current_scope().declare_function(terminal.symbol, function);
      function_name = terminal.symbol;
   }

   if (Payload.name.method) {
      this->current_scope().declare_function(Payload.name.method->symbol, function);
      function_name = Payload.name.method->symbol;
   }

   if (function) this->analyse_function_payload(*function, function_name);
}

void TypeAnalyser::analyse_function_payload(const FunctionExprPayload &Function, GCstr *Name)
{
   this->push_scope();
   this->enter_function(Function, Name);

   for (const auto &param : Function.parameters) {
      this->current_scope().declare_parameter(param.name.symbol, param.type);
   }

   // Check for recursive functions without explicit return types
   // Recursive functions must have explicit return type declarations because their
   // return type cannot be inferred without executing the recursion.
   // Exception: void functions (no return values) are exempt since there's nothing to infer.

   if ((not Function.return_types.is_explicit) and (Name) and (this->is_recursive_function(Function, Name)) and
       this->function_has_return_values(Function)) {
      TypeDiagnostic diag;
      diag.location = Function.body ? Function.body->span : SourceSpan{};
      diag.code = ParserErrorCode::RecursiveFunctionNeedsType;
      diag.message = std::format("recursive function '{}' must have explicit return type declaration",
         Name ? std::string_view(strdata(Name), Name->len) : "<anonymous>");
      this->diagnostics_.push_back(std::move(diag));
   }

   if (Function.body) this->analyse_block(*Function.body);

   this->leave_function();
   this->pop_scope();
}

void TypeAnalyser::analyse_expression(const ExprNode &Expression)
{
   switch (Expression.kind) {
      case AstNodeKind::UnaryExpr: {
         auto *payload = std::get_if<UnaryExprPayload>(&Expression.data);
         if (payload and payload->operand) this->analyse_expression(*payload->operand);
         break;
      }
      case AstNodeKind::UpdateExpr: {
         auto *payload = std::get_if<UpdateExprPayload>(&Expression.data);
         if (payload and payload->target) this->analyse_expression(*payload->target);
         break;
      }
      case AstNodeKind::BinaryExpr: {
         auto *payload = std::get_if<BinaryExprPayload>(&Expression.data);
         if (payload) {
            if (payload->left) this->analyse_expression(*payload->left);
            if (payload->right) this->analyse_expression(*payload->right);
         }
         break;
      }
      case AstNodeKind::TernaryExpr: {
         auto *payload = std::get_if<TernaryExprPayload>(&Expression.data);
         if (payload) {
            if (payload->condition) this->analyse_expression(*payload->condition);
            if (payload->if_true) this->analyse_expression(*payload->if_true);
            if (payload->if_false) this->analyse_expression(*payload->if_false);
         }
         break;
      }
      case AstNodeKind::PresenceExpr: {
         auto *payload = std::get_if<PresenceExprPayload>(&Expression.data);
         if (payload and payload->value) this->analyse_expression(*payload->value);
         break;
      }
      case AstNodeKind::CallExpr: {
         auto *payload = std::get_if<CallExprPayload>(&Expression.data);
         if (payload) this->analyse_call_expr(*payload);
         break;
      }
      case AstNodeKind::MemberExpr: {
         auto *payload = std::get_if<MemberExprPayload>(&Expression.data);
         if (payload and payload->table) this->analyse_expression(*payload->table);
         break;
      }
      case AstNodeKind::IndexExpr: {
         auto *payload = std::get_if<IndexExprPayload>(&Expression.data);
         if (payload) {
            if (payload->table) this->analyse_expression(*payload->table);
            if (payload->index) this->analyse_expression(*payload->index);
         }
         break;
      }
      case AstNodeKind::SafeMemberExpr: {
         auto *payload = std::get_if<SafeMemberExprPayload>(&Expression.data);
         if (payload and payload->table) this->analyse_expression(*payload->table);
         break;
      }
      case AstNodeKind::SafeIndexExpr: {
         auto *payload = std::get_if<SafeIndexExprPayload>(&Expression.data);
         if (payload) {
            if (payload->table) this->analyse_expression(*payload->table);
            if (payload->index) this->analyse_expression(*payload->index);
         }
         break;
      }
      case AstNodeKind::TableExpr: {
         auto *payload = std::get_if<TableExprPayload>(&Expression.data);
         if (payload) {
            for (const auto &field : payload->fields) {
               if (field.key) this->analyse_expression(*field.key);
               if (field.value) this->analyse_expression(*field.value);
            }
         }
         break;
      }
      case AstNodeKind::FunctionExpr: {
         auto *payload = std::get_if<FunctionExprPayload>(&Expression.data);
         if (payload) this->analyse_function_payload(*payload);
         break;
      }
      default:
         break;
   }
}

void TypeAnalyser::analyse_call_expr(const CallExprPayload &Call)
{
   for (const auto &argument : Call.arguments) {
      this->analyse_expression(*argument);
   }

   const FunctionExprPayload* target = this->resolve_call_target(Call.target);
   if (target) this->check_arguments(*target, Call);
}

void TypeAnalyser::check_arguments(const FunctionExprPayload &Function, const CallExprPayload &Call)
{
   size_t param_index = 0;
   for (const auto &param : Function.parameters) {
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
         auto *payload = std::get_if<LiteralValue>(&Expr.data);
         if (payload) return infer_literal_type(*payload);
         break;
      }
      case AstNodeKind::IdentifierExpr: {
         auto *payload = std::get_if<NameRef>(&Expr.data);
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
      case AstNodeKind::CallExpr: {
         // For call expressions, try to infer from the function's declared return type
         auto *payload = std::get_if<CallExprPayload>(&Expr.data);
         if (payload) {
            const FunctionExprPayload* target = this->resolve_call_target(payload->target);
            if (target and target->return_types.is_explicit and target->return_types.count > 0) {
               result.primary = target->return_types.types[0];
               return result;
            }
         }
         result.primary = FluidType::Any;
         break;
      }
      case AstNodeKind::BinaryExpr: {
         // Infer type from binary expression operands and operator
         auto *payload = std::get_if<BinaryExprPayload>(&Expr.data);
         if (payload) {
            switch (payload->op) {
               // Comparison operators always return boolean
               case AstBinaryOperator::Equal:
               case AstBinaryOperator::NotEqual:
               case AstBinaryOperator::LessThan:
               case AstBinaryOperator::LessEqual:
               case AstBinaryOperator::GreaterThan:
               case AstBinaryOperator::GreaterEqual:
                  result.primary = FluidType::Bool;
                  return result;
               // Logical operators in Lua/Fluid return one of their operands.
               // Try to infer from operands, if both have the same type, use that.
               case AstBinaryOperator::LogicalAnd:
               case AstBinaryOperator::LogicalOr: {
                  InferredType left_type, right_type;
                  if (payload->left) left_type = this->infer_expression_type(*payload->left);
                  if (payload->right) right_type = this->infer_expression_type(*payload->right);

                  // If both operands have the same concrete type, return that

                  if ((left_type.primary IS right_type.primary) and (left_type.primary != FluidType::Any) and
                      (left_type.primary != FluidType::Unknown)) {
                     return left_type;
                  }

                  // For `or`, the right operand is the fallback, so prefer its type if known

                  if (payload->op IS AstBinaryOperator::LogicalOr) {
                     if ((right_type.primary != FluidType::Any) and (right_type.primary != FluidType::Unknown)) {
                        return right_type;
                     }
                     if ((left_type.primary != FluidType::Any) and (left_type.primary != FluidType::Unknown)) {
                        return left_type;
                     }
                  }
                  else { // For `and`, the left operand short-circuits, so prefer left type if known
                     if ((left_type.primary != FluidType::Any) and (left_type.primary != FluidType::Unknown)) {
                        return left_type;
                     }
                     if ((right_type.primary != FluidType::Any) and (right_type.primary != FluidType::Unknown)) {
                        return right_type;
                     }
                  }

                  result.primary = FluidType::Any;
                  return result;
               }
               // Concatenation returns string
               case AstBinaryOperator::Concat:
                  result.primary = FluidType::Str;
                  return result;
               // Arithmetic operators return number
               case AstBinaryOperator::Add:
               case AstBinaryOperator::Subtract:
               case AstBinaryOperator::Multiply:
               case AstBinaryOperator::Divide:
               case AstBinaryOperator::Modulo:
               case AstBinaryOperator::Power:
               case AstBinaryOperator::BitAnd:
               case AstBinaryOperator::BitOr:
               case AstBinaryOperator::BitXor:
               case AstBinaryOperator::ShiftLeft:
               case AstBinaryOperator::ShiftRight:
                  result.primary = FluidType::Num;
                  return result;
               // IfEmpty returns type of the operands
               case AstBinaryOperator::IfEmpty:
                  if (payload->left) {
                     result = this->infer_expression_type(*payload->left);
                     if (result.primary != FluidType::Any and result.primary != FluidType::Unknown) {
                        return result;
                     }
                  }
                  if (payload->right) {
                     return this->infer_expression_type(*payload->right);
                  }
                  break;
            }
         }
         result.primary = FluidType::Any;
         break;
      }
      case AstNodeKind::UnaryExpr: {
         auto *payload = std::get_if<UnaryExprPayload>(&Expr.data);
         if (payload) {
            switch (payload->op) {
               case AstUnaryOperator::Not:
                  result.primary = FluidType::Bool;
                  return result;
               case AstUnaryOperator::Negate:
               case AstUnaryOperator::BitNot:
                  result.primary = FluidType::Num;
                  return result;
               case AstUnaryOperator::Length:
                  result.primary = FluidType::Num;
                  return result;
            }
         }
         result.primary = FluidType::Any;
         break;
      }
      case AstNodeKind::TernaryExpr: {
         // Ternary returns type of true branch (or false branch if true is unknown)
         auto *payload = std::get_if<TernaryExprPayload>(&Expr.data);
         if (payload) {
            if (payload->if_true) {
               result = this->infer_expression_type(*payload->if_true);
               if (result.primary != FluidType::Any and result.primary != FluidType::Unknown) return result;
            }

            if (payload->if_false) return this->infer_expression_type(*payload->if_false);
         }
         result.primary = FluidType::Any;
         break;
      }
      default:
         result.primary = FluidType::Any;
         break;
   }

   return result;
}

// Infer the return type at a specific position from a function call expression
// This is used for multi-value assignments like: local a, b = func()
[[nodiscard]] InferredType TypeAnalyser::infer_call_return_type(const ExprNode& Expr, size_t Position) const
{
   InferredType result;
   result.primary = FluidType::Any;

   if (Expr.kind != AstNodeKind::CallExpr) return result;

   auto *payload = std::get_if<CallExprPayload>(&Expr.data);
   if (not payload) return result;

   const FunctionExprPayload* target = this->resolve_call_target(payload->target);
   if (not target) return result;

   if (not target->return_types.is_explicit) return result;

   // Get the type at the requested position
   FluidType type = target->return_types.type_at(Position);
   if (type != FluidType::Unknown) result.primary = type;

   return result;
}

std::optional<InferredType> TypeAnalyser::resolve_identifier(GCstr *Name) const
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
      const auto &direct = std::get<DirectCallTarget>(Target);
      if (direct.callable) {
         if (direct.callable->kind IS AstNodeKind::FunctionExpr) {
            auto *payload = std::get_if<FunctionExprPayload>(&direct.callable->data);
            if (payload) return payload;
         }
         if (direct.callable->kind IS AstNodeKind::IdentifierExpr) {
            auto *payload = std::get_if<NameRef>(&direct.callable->data);
            if (payload) return this->resolve_function(payload->identifier.symbol);
         }
      }
   }
   return nullptr;
}

const FunctionExprPayload* TypeAnalyser::resolve_function(GCstr *Name) const
{
   for (auto it = this->scope_stack_.rbegin(); it != this->scope_stack_.rend(); ++it) {
      const FunctionExprPayload* fn = it->lookup_function(Name);
      if (fn) return fn;
   }
   return nullptr;
}

void TypeAnalyser::fix_local_type(GCstr *Name, FluidType Type)
{
   for (auto it = this->scope_stack_.rbegin(); it != this->scope_stack_.rend(); ++it) {
      auto existing = it->lookup_local_type(Name);
      if (existing) {
         it->fix_local_type(Name, Type);
         this->trace_fix(this->ctx_.lex().linenumber, Name, Type);
         return;
      }
   }
}

//********************************************************************************************************************
// Return type validation
//
// This method validates return statements against the function's declared or inferred return types.
// It implements:
// - Type mismatch detection between returned values and declared types
// - Return count validation (too many values returned)
// - First-wins inference rule for functions without explicit return type declarations
// - Nil is always allowed as a valid return value for any type slot

void TypeAnalyser::validate_return_types(const ReturnStmtPayload &Return, SourceSpan Location)
{
   FunctionContext* ctx = this->current_function();
   if (not ctx) return;  // Not inside a function (shouldn't happen in valid code)

   size_t return_count = Return.values.size();

   if (ctx->expected_returns.is_explicit) {
      // Explicit declaration: validate against declared types

      // Check for too many return values (unless variadic)
      if (not ctx->expected_returns.is_variadic and return_count > ctx->expected_returns.count) {
         TypeDiagnostic diag;
         diag.location = Location;
         diag.code = ParserErrorCode::ReturnCountMismatch;
         diag.message = std::format("too many return values: function declares {} but {} returned",
            ctx->expected_returns.count, return_count);
         this->diagnostics_.push_back(std::move(diag));
      }

      // Validate type of each returned value
      for (size_t i = 0; i < return_count and i < MAX_RETURN_TYPES; ++i) {
         FluidType expected = ctx->expected_returns.type_at(i);
         if (expected IS FluidType::Any or expected IS FluidType::Unknown) continue;

         InferredType actual = this->infer_expression_type(*Return.values[i]);

         // Nil is always allowed as a "clear" or "no value" return
         if (actual.primary IS FluidType::Nil) continue;
         // Any can be assigned to any type
         if (actual.primary IS FluidType::Any) continue;

         if (actual.primary != expected) {
            TypeDiagnostic diag;
            diag.location = Return.values[i]->span;
            diag.expected = expected;
            diag.actual = actual.primary;
            diag.code = ParserErrorCode::ReturnTypeMismatch;
            diag.message = std::format("return type mismatch at position {}: expected '{}', got '{}'",
               i + 1, type_name(expected), type_name(actual.primary));
            this->diagnostics_.push_back(std::move(diag));
         }
      }
   }
   else {
      // Inference mode: first non-nil return statement fixes types (first-wins rule)
      // Nil returns don't establish a type - they're compatible with any future type
      if (not ctx->return_type_inferred and return_count > 0) {
         // First return: infer types from returned values
         bool has_non_nil = false;
         for (size_t i = 0; i < std::min(return_count, MAX_RETURN_TYPES); ++i) {
            InferredType inferred = this->infer_expression_type(*Return.values[i]);
            ctx->expected_returns.types[i] = inferred.primary;
            if (inferred.primary != FluidType::Nil and inferred.primary != FluidType::Any) {
               has_non_nil = true;
            }
         }
         ctx->expected_returns.count = uint8_t(std::min(return_count, MAX_RETURN_TYPES));
         // Only mark as inferred if we have at least one concrete (non-nil) type
         // This allows a later return with concrete types to establish the actual types
         ctx->return_type_inferred = has_non_nil;
      }
      else if (return_count > 0) {
         // Subsequent return: check consistency with inferred types
         size_t check_count = std::min(return_count, size_t(ctx->expected_returns.count));

         for (size_t i = 0; i < check_count; ++i) {
            FluidType expected = ctx->expected_returns.types[i];
            InferredType actual = this->infer_expression_type(*Return.values[i]);

            // If expected is nil/any/unknown, and actual is concrete, upgrade the expected type
            if ((expected IS FluidType::Nil or expected IS FluidType::Any or expected IS FluidType::Unknown) and
                actual.primary != FluidType::Nil and actual.primary != FluidType::Any and
                actual.primary != FluidType::Unknown) {
               ctx->expected_returns.types[i] = actual.primary;
               ctx->return_type_inferred = true;
               continue;
            }

            if (expected IS FluidType::Any or expected IS FluidType::Unknown) continue;

            // Nil is always allowed as a "clear" or "no value" return
            if (actual.primary IS FluidType::Nil) continue;
            // Any can match any type
            if (actual.primary IS FluidType::Any) continue;

            if (actual.primary != expected) {
               TypeDiagnostic diag;
               diag.location = Return.values[i]->span;
               diag.expected = expected;
               diag.actual = actual.primary;
               diag.code = ParserErrorCode::ReturnTypeMismatch;
               diag.message = std::format("inconsistent return type at position {}: first return established '{}', but this returns '{}'",
                  i + 1, type_name(expected), type_name(actual.primary));
               this->diagnostics_.push_back(std::move(diag));
            }
         }
      }
   }
}

//********************************************************************************************************************
// Recursive function detection
//
// Recursive functions must have explicit return type declarations because their return type
// cannot be inferred without executing the recursion. This detects direct recursion (function
// calls itself) and flags an error if no explicit return type is declared.

bool TypeAnalyser::is_recursive_function(const FunctionExprPayload &Function, GCstr *Name) const
{
   if (not Name) return false;
   if (not Function.body) return false;
   return this->body_contains_call_to(*Function.body, Name);
}

// Check if a function has any return statements with values (non-void returns)
bool TypeAnalyser::function_has_return_values(const FunctionExprPayload &Function) const
{
   if (not Function.body) return false;
   return this->body_has_return_values(*Function.body);
}

// Recursively check if a block contains any return statements with values
bool TypeAnalyser::body_has_return_values(const BlockStmt& Block) const
{
   for (const auto &stmt : Block.statements) {
      if (not stmt) continue;

      switch (stmt->kind) {
         case AstNodeKind::ReturnStmt: {
            auto *payload = std::get_if<ReturnStmtPayload>(&stmt->data);
            if (payload and not payload->values.empty()) {
               return true;  // Found a return with values
            }
            break;
         }
         case AstNodeKind::IfStmt: {
            auto *payload = std::get_if<IfStmtPayload>(&stmt->data);
            if (payload) {
               for (const auto &clause : payload->clauses) {
                  if (clause.block and this->body_has_return_values(*clause.block)) return true;
               }
            }
            break;
         }
         case AstNodeKind::WhileStmt:
         case AstNodeKind::RepeatStmt: {
            auto *payload = std::get_if<LoopStmtPayload>(&stmt->data);
            if (payload and payload->body and this->body_has_return_values(*payload->body)) return true;
            break;
         }
         case AstNodeKind::NumericForStmt: {
            auto *payload = std::get_if<NumericForStmtPayload>(&stmt->data);
            if (payload and payload->body and this->body_has_return_values(*payload->body)) return true;
            break;
         }
         case AstNodeKind::GenericForStmt: {
            auto *payload = std::get_if<GenericForStmtPayload>(&stmt->data);
            if (payload and payload->body and this->body_has_return_values(*payload->body)) return true;
            break;
         }
         case AstNodeKind::DoStmt: {
            auto *payload = std::get_if<DoStmtPayload>(&stmt->data);
            if (payload and payload->block and this->body_has_return_values(*payload->block)) return true;
            break;
         }
         default:
            break;
      }
   }
   return false;
}

bool TypeAnalyser::body_contains_call_to(const BlockStmt& Block, GCstr *Name) const
{
   for (const auto &stmt : Block.statements) {
      if (stmt and this->statement_contains_call_to(*stmt, Name)) {
         return true;
      }
   }
   return false;
}

bool TypeAnalyser::statement_contains_call_to(const StmtNode& Stmt, GCstr *Name) const
{
   switch (Stmt.kind) {
      case AstNodeKind::ExpressionStmt: {
         auto *payload = std::get_if<ExpressionStmtPayload>(&Stmt.data);
         if (payload and payload->expression) {
            return this->expression_contains_call_to(*payload->expression, Name);
         }
         break;
      }
      case AstNodeKind::AssignmentStmt: {
         auto *payload = std::get_if<AssignmentStmtPayload>(&Stmt.data);
         if (payload) {
            for (const auto &value : payload->values) {
               if (value and this->expression_contains_call_to(*value, Name)) return true;
            }
         }
         break;
      }
      case AstNodeKind::LocalDeclStmt: {
         auto *payload = std::get_if<LocalDeclStmtPayload>(&Stmt.data);
         if (payload) {
            for (const auto &value : payload->values) {
               if (value and this->expression_contains_call_to(*value, Name)) return true;
            }
         }
         break;
      }
      case AstNodeKind::ReturnStmt: {
         auto *payload = std::get_if<ReturnStmtPayload>(&Stmt.data);
         if (payload) {
            for (const auto &value : payload->values) {
               if (value and this->expression_contains_call_to(*value, Name)) return true;
            }
         }
         break;
      }
      case AstNodeKind::IfStmt: {
         auto *payload = std::get_if<IfStmtPayload>(&Stmt.data);
         if (payload) {
            for (const auto &clause : payload->clauses) {
               if (clause.condition and this->expression_contains_call_to(*clause.condition, Name)) return true;
               if (clause.block and this->body_contains_call_to(*clause.block, Name)) return true;
            }
         }
         break;
      }
      case AstNodeKind::WhileStmt:
      case AstNodeKind::RepeatStmt: {
         auto *payload = std::get_if<LoopStmtPayload>(&Stmt.data);
         if (payload) {
            if (payload->condition and this->expression_contains_call_to(*payload->condition, Name)) return true;
            if (payload->body and this->body_contains_call_to(*payload->body, Name)) return true;
         }
         break;
      }
      case AstNodeKind::NumericForStmt: {
         auto *payload = std::get_if<NumericForStmtPayload>(&Stmt.data);
         if (payload) {
            if (payload->start and this->expression_contains_call_to(*payload->start, Name)) return true;
            if (payload->stop and this->expression_contains_call_to(*payload->stop, Name)) return true;
            if (payload->step and this->expression_contains_call_to(*payload->step, Name)) return true;
            if (payload->body and this->body_contains_call_to(*payload->body, Name)) return true;
         }
         break;
      }
      case AstNodeKind::GenericForStmt: {
         auto *payload = std::get_if<GenericForStmtPayload>(&Stmt.data);
         if (payload) {
            for (const auto &iter : payload->iterators) {
               if (iter and this->expression_contains_call_to(*iter, Name)) return true;
            }
            if (payload->body and this->body_contains_call_to(*payload->body, Name)) return true;
         }
         break;
      }
      case AstNodeKind::DoStmt: {
         auto *payload = std::get_if<DoStmtPayload>(&Stmt.data);
         if (payload and payload->block) {
            return this->body_contains_call_to(*payload->block, Name);
         }
         break;
      }
      default:
         break;
   }
   return false;
}

bool TypeAnalyser::expression_contains_call_to(const ExprNode& Expr, GCstr *Name) const
{
   switch (Expr.kind) {
      case AstNodeKind::CallExpr: {
         auto *payload = std::get_if<CallExprPayload>(&Expr.data);
         if (payload) {
            // Check if this is a direct call to the function name
            if (std::holds_alternative<DirectCallTarget>(payload->target)) {
               const auto &direct = std::get<DirectCallTarget>(payload->target);
               if (direct.callable and direct.callable->kind IS AstNodeKind::IdentifierExpr) {
                  auto *name_ref = std::get_if<NameRef>(&direct.callable->data);
                  if (name_ref and name_ref->identifier.symbol IS Name) {
                     return true;  // Direct recursive call found
                  }
               }
               // Also check inside the callable expression
               if (direct.callable and this->expression_contains_call_to(*direct.callable, Name)) {
                  return true;
               }
            }
            // Check arguments for recursive calls
            for (const auto &arg : payload->arguments) {
               if (arg and this->expression_contains_call_to(*arg, Name)) return true;
            }
         }
         break;
      }
      case AstNodeKind::BinaryExpr: {
         auto *payload = std::get_if<BinaryExprPayload>(&Expr.data);
         if (payload) {
            if (payload->left and this->expression_contains_call_to(*payload->left, Name)) return true;
            if (payload->right and this->expression_contains_call_to(*payload->right, Name)) return true;
         }
         break;
      }
      case AstNodeKind::UnaryExpr: {
         auto *payload = std::get_if<UnaryExprPayload>(&Expr.data);
         if (payload and payload->operand) {
            return this->expression_contains_call_to(*payload->operand, Name);
         }
         break;
      }
      case AstNodeKind::TernaryExpr: {
         auto *payload = std::get_if<TernaryExprPayload>(&Expr.data);
         if (payload) {
            if (payload->condition and this->expression_contains_call_to(*payload->condition, Name)) return true;
            if (payload->if_true and this->expression_contains_call_to(*payload->if_true, Name)) return true;
            if (payload->if_false and this->expression_contains_call_to(*payload->if_false, Name)) return true;
         }
         break;
      }
      case AstNodeKind::MemberExpr: {
         auto *payload = std::get_if<MemberExprPayload>(&Expr.data);
         if (payload and payload->table) {
            return this->expression_contains_call_to(*payload->table, Name);
         }
         break;
      }
      case AstNodeKind::IndexExpr: {
         auto *payload = std::get_if<IndexExprPayload>(&Expr.data);
         if (payload) {
            if (payload->table and this->expression_contains_call_to(*payload->table, Name)) return true;
            if (payload->index and this->expression_contains_call_to(*payload->index, Name)) return true;
         }
         break;
      }
      case AstNodeKind::TableExpr: {
         auto *payload = std::get_if<TableExprPayload>(&Expr.data);
         if (payload) {
            for (const auto &field : payload->fields) {
               if (field.key and this->expression_contains_call_to(*field.key, Name)) return true;
               if (field.value and this->expression_contains_call_to(*field.value, Name)) return true;
            }
         }
         break;
      }
      default:
         break;
   }
   return false;
}

static void publish_type_diagnostics(ParserContext& Context, const std::vector<TypeDiagnostic>& Diagnostics)
{
   for (const auto &diag : Diagnostics) {
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
