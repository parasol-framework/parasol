// Lua parser - Core utilities and error handling.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// Major portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h

LJ_NORET LJ_NOINLINE static void err_syntax(LexState *State, ErrMsg em)
{
   lj_lex_error(State, State->tok, em);
}

LJ_NORET LJ_NOINLINE static void err_token(LexState *State, LexToken tok)
{
   lj_lex_error(State, State->tok, LJ_ERR_XTOKEN, State->token2str(tok));
}

LJ_NORET static void err_limit(FuncState* fs, uint32_t limit, const char* what)
{
   if (fs->linedefined == 0)
      lj_lex_error(fs->ls, 0, LJ_ERR_XLIMM, limit, what);
   else
      lj_lex_error(fs->ls, 0, LJ_ERR_XLIMF, fs->linedefined, limit, what);
}

// Lexer support

// Check and consume optional token.

static int lex_opt(LexState *State, LexToken tok)
{
   if (State->tok == tok) {
      State->next();
      return 1;
   }
   return 0;
}

// Check and consume token.

static void lex_check(LexState *State, LexToken tok)
{
   if (State->tok != tok) err_token(State, tok);
   State->next();
}

// Check for matching token.

static void lex_match(LexState *State, LexToken what, LexToken who, BCLine line)
{
   if (!lex_opt(State, what)) {
      if (line == State->linenumber) {
         err_token(State, what);
      }
      else {
         auto swhat = State->token2str(what);
         auto swho = State->token2str(who);
         lj_lex_error(State, State->tok, LJ_ERR_XMATCH, swhat, swho, line);
      }
   }
}

// Check for string token.

static GCstr* lex_str(LexState *State)
{
   GCstr* s;
   if (State->tok != TK_name) err_token(State, TK_name);
   s = strV(&State->tokval);
   State->next();
   return s;
}
