// Copyright (C) 2025 Paul Manias

#include "parser/token_types.h"

#include <cmath>

#include "lj_obj.h"

TokenPayload::TokenPayload()
{
   this->has_payload = false;
   this->owner = nullptr;
}

void TokenPayload::assign(lua_State *state, const TValue &value)
{
   this->owner = state;
   copyTV(state, &this->payload, &value);
   this->has_payload = true;
}

GCstr * TokenPayload::as_string() const
{
   if (not this->has_payload) return nullptr;
   if (not tvisstr(&this->payload)) return nullptr;
   return strV(&this->payload);
}

double TokenPayload::as_number() const
{
   if (not this->has_payload) return 0.0;
   if (tvisnum(&this->payload)) return numV(&this->payload);
   return 0.0;
}

Token Token::from_current(LexState &state)
{
   Token token;
   token.token_kind = to_token_kind(state.tok);
   token.raw_token = state.tok;
   token.source = state.current_token_span();
   token.data.assign(state.L, state.tokval);
   return token;
}

Token Token::from_lookahead(LexState &state)
{
   Token token;
   LexToken lookahead = (state.lookahead != TK_eof) ? state.lookahead : state.lookahead_token();
   token.token_kind = to_token_kind(lookahead);
   token.raw_token = lookahead;
   token.source = state.lookahead_token_span();
   token.data.assign(state.L, state.lookaheadval);
   return token;
}

Token Token::from_buffered(LexState& state, const LexState::BufferedToken& buffered)
{
   Token token;
   token.token_kind = to_token_kind(buffered.token);
   token.raw_token = buffered.token;
   token.source.line = buffered.line;
   token.source.column = buffered.column;
   token.source.offset = buffered.offset;
   token.data.assign(state.L, buffered.value);
   return token;
}

bool Token::is_identifier() const
{
   return this->token_kind IS TokenKind::Identifier;
}

bool Token::is_literal() const
{
   switch (this->token_kind) {
      case TokenKind::Number:
      case TokenKind::String:
      case TokenKind::Nil:
      case TokenKind::TrueToken:
      case TokenKind::FalseToken:
         return true;
      default:
         return false;
   }
}

bool Token::is_eof() const
{
   return this->token_kind IS TokenKind::EndOfFile;
}

GCstr * Token::identifier() const
{
   return this->data.as_string();
}

TokenKind to_token_kind(LexToken token)
{
   return (TokenKind)token;
}

const char * token_kind_name(TokenKind kind, LexState& lex)
{
   return lex.token2str((LexToken)kind);
}
