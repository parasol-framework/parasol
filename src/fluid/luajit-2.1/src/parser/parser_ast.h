// Parser AST definitions and builders for the LuaJIT parser.

#pragma once

#include <cstdint>
#include <vector>

#include "parser/parser_context.h"

struct AstIdentifier {
   Token token;
};

enum class AstPrimaryPrefixKind : uint8_t {
   Identifier
};

struct AstPrimaryPrefix {
   AstPrimaryPrefixKind kind = AstPrimaryPrefixKind::Identifier;
   Token token;
};

enum class AstPrimarySuffixKind : uint8_t {
   Field,
   Index,
   MethodCall,
   Call,
   PresenceCheck,
   PostfixIncrement
};

struct AstPrimarySuffix {
   AstPrimarySuffixKind kind = AstPrimarySuffixKind::Field;
   Token token;
};

struct AstPrimaryExpression {
   AstPrimaryPrefix prefix;
   std::vector<AstPrimarySuffix> suffixes;
};

struct AstLocalBinding {
   Token name;
};

struct AstLocalStatement {
   Token local_token;
   std::vector<AstLocalBinding> bindings;
   bool has_initializer = false;
};

class AstBuilder {
public:
   explicit AstBuilder(ParserContext& Context);

   ParserResult<AstPrimaryExpression> parse_primary_expression();
   ParserResult<AstLocalStatement> parse_local_statement();

private:
   ParserContext* context = nullptr;
};

