// Lua parser - Core utilities and error handling.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// Major portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h

LJ_NORET LJ_NOINLINE void LexState::err_syntax(ErrMsg Message)
{
   if (this->parser_context)
      this->parser_context->emit_legacy_error(Message, this->parser_context->tokens().current());
   lj_lex_error(this, this->tok, Message);
}

LJ_NORET LJ_NOINLINE void LexState::err_token(LexToken Token)
{
   if (this->parser_context)
      this->parser_context->emit_legacy_error(LJ_ERR_XTOKEN, this->parser_context->tokens().current());
   lj_lex_error(this, this->tok, LJ_ERR_XTOKEN, this->token2str(Token));
}

LJ_NORET static void err_limit(FuncState* fs, uint32_t limit, const char* what)
{
   if (fs->ls and fs->ls->parser_context) {
      auto& Context = *fs->ls->parser_context;
      Context.emit_error(ParserErrorCode::InternalError, what, Context.tokens().current());
   }
   if (fs->linedefined == 0) lj_lex_error(fs->ls, 0, LJ_ERR_XLIMM, limit, what);
   else lj_lex_error(fs->ls, 0, LJ_ERR_XLIMF, fs->linedefined, limit, what);
}

// Check and consume optional token.

int LexState::lex_opt(LexToken Token)
{
   if (this->tok == Token) {
      this->next();
      return 1;
   }
   return 0;
}

// Check and consume token.

void LexState::lex_check(LexToken Token)
{
   if (this->tok != Token) this->err_token(Token);
   this->next();
}

// Check for matching token.

void LexState::lex_match(LexToken What, LexToken Who, BCLine Line)
{
   if (!this->lex_opt(What)) {
      if (Line == this->linenumber) {
         this->err_token(What);
      }
      else {
         auto swhat = this->token2str(What);
         auto swho = this->token2str(Who);
         lj_lex_error(this, this->tok, LJ_ERR_XMATCH, swhat, swho, Line);
      }
   }
}

// Check for string token.

GCstr* LexState::lex_str()
{
   GCstr* s;
   if (this->tok != TK_name) this->err_token(TK_name);
   s = strV(&this->tokval);
   this->next();
   return s;
}
