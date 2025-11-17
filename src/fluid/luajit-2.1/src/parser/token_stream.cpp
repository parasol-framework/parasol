#include "parser/token_stream.h"

#include "lj_lex.h"

TokenStreamAdapter::TokenStreamAdapter(LexState& state)
   : lex_state(&state)
{
}

Token TokenStreamAdapter::current() const
{
   return Token::from_current(*this->lex_state);
}

Token TokenStreamAdapter::peek(size_t lookahead) const
{
   if (lookahead IS 0) return this->current();
   if (lookahead IS 1) return Token::from_lookahead(*this->lex_state);
   return this->current();
}

Token TokenStreamAdapter::advance()
{
   this->lex_state->next();
   return this->current();
}

void TokenStreamAdapter::sync_from_lex(LexState& state)
{
   this->lex_state = &state;
}

