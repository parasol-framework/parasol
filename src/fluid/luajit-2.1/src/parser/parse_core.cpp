// Lua parser - Error handling.
// Copyright (C) 2025 Paul Manias

#include <parasol/main.h>
#include "parser/parser_context.h"

[[noreturn]] LJ_NOINLINE void LexState::err_syntax(ErrMsg Message)
{
   if (this->active_context) this->active_context->err_syntax(Message);
   lj_lex_error(this, this->tok, Message);
}

[[noreturn]] LJ_NOINLINE void LexState::err_token(LexToken Token)
{
   if (this->active_context) this->active_context->err_token(Token);
   lj_lex_error(this, this->tok, ErrMsg::XTOKEN, this->token2str(Token));
}

[[noreturn]] static void err_limit(FuncState *fs, uint32_t limit, CSTRING what)
{
   if (fs->ls->active_context) fs->ls->active_context->report_limit_error(*fs, limit, what);

   if (fs->linedefined == 0) lj_lex_error(fs->ls, 0, ErrMsg::XLIMM, limit, what);
   else lj_lex_error(fs->ls, 0, ErrMsg::XLIMF, fs->linedefined, limit, what);
}

// Check and consume optional token.

int LexState::lex_opt(LexToken Token)
{
   if (this->active_context) return this->active_context->lex_opt(Token);

   if (this->tok == Token) {
      this->next();
      return 1;
   }
   return 0;
}

// Check and consume token.

void LexState::lex_check(LexToken Token)
{
   if (this->active_context) {
      this->active_context->lex_check(Token);
      return;
   }
   if (this->tok != Token) this->err_token(Token);
   this->next();
}

// Check for matching token.

void LexState::lex_match(LexToken What, LexToken Who, BCLine Line)
{
   if (this->active_context) this->active_context->lex_match(What, Who, Line);
   else if (not this->lex_opt(What)) {
      if (Line == this->linenumber) this->err_token(What);
      else {
         auto swhat = this->token2str(What);
         auto swho = this->token2str(Who);
         lj_lex_error(this, this->tok, ErrMsg::XMATCH, swhat, swho, Line);
      }
   }
}

// Check for string token.

[[nodiscard]] GCstr * LexState::lex_str()
{
   if (this->active_context) return this->active_context->lex_str();

   if (this->tok != TK_name) this->err_token(TK_name);
   GCstr *s = strV(&this->tokval);
   this->next();
   return s;
}
