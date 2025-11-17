#include "parser/token_stream.h"

TokenStreamAdapter::TokenStreamAdapter(LexState* Lex)
   : lex_state(Lex)
{
}

void TokenStreamAdapter::bind(LexState* Lex)
{
   this->lex_state = Lex;
}

Token TokenStreamAdapter::current() const
{
   lj_assertX(this->lex_state != nullptr, "token stream not bound");
   return make_token_from_lex(*this->lex_state, this->lex_state->tok, this->lex_state->tokval);
}

Token TokenStreamAdapter::peek(std::size_t Lookahead) const
{
   lj_assertX(this->lex_state != nullptr, "token stream not bound");
   if (Lookahead IS 0) return this->current();
   if (Lookahead IS 1) {
      LexToken Next = (this->lex_state->lookahead != TK_eof) ? this->lex_state->lookahead : this->lex_state->lookahead_token();
      TValue Value = this->lex_state->lookaheadval;
      return make_token_from_lex(*this->lex_state, Next, Value);
   }
   Token Unknown;
   Unknown.kind = TokenKind::Unknown;
   Unknown.line = this->lex_state->linenumber;
   Unknown.last_line = this->lex_state->lastline;
   return Unknown;
}

Token TokenStreamAdapter::advance()
{
   lj_assertX(this->lex_state != nullptr, "token stream not bound");
   this->lex_state->next();
   return this->current();
}

void TokenStreamAdapter::sync_from_lex(LexState& Lex)
{
   this->lex_state = &Lex;
}

