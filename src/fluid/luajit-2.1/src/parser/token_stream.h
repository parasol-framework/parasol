// Token stream adapter bridging LexState and typed tokens.

#pragma once

#include "parser/token_types.h"

class TokenStreamAdapter {
public:
   TokenStreamAdapter() = default;
   explicit TokenStreamAdapter(LexState* Lex);

   void bind(LexState* Lex);
   Token current() const;
   Token peek(std::size_t Lookahead) const;
   Token advance();
   void sync_from_lex(LexState& Lex);

private:
   LexState* lex_state = nullptr;
};

