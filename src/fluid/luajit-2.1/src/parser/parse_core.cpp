// Lua parser - Core utilities and error handling.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// Major portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h

LJ_NORET LJ_NOINLINE static void err_syntax(LexState* ls, ErrMsg em)
{
   lj_lex_error(ls, ls->tok, em);
}

LJ_NORET LJ_NOINLINE static void err_token(LexState* ls, LexToken tok)
{
   lj_lex_error(ls, ls->tok, LJ_ERR_XTOKEN, lj_lex_token2str(ls, tok));
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

static int lex_opt(LexState* ls, LexToken tok)
{
   if (ls->tok == tok) {
      lj_lex_next(ls);
      return 1;
   }
   return 0;
}

// Check and consume token.

static void lex_check(LexState* ls, LexToken tok)
{
   if (ls->tok != tok)
      err_token(ls, tok);
   lj_lex_next(ls);
}

// Check for matching token.

static void lex_match(LexState* ls, LexToken what, LexToken who, BCLine line)
{
   if (!lex_opt(ls, what)) {
      if (line == ls->linenumber) {
         err_token(ls, what);
      }
      else {
         const char* swhat = lj_lex_token2str(ls, what);
         const char* swho = lj_lex_token2str(ls, who);
         lj_lex_error(ls, ls->tok, LJ_ERR_XMATCH, swhat, swho, line);
      }
   }
}

// Check for string token.

static GCstr* lex_str(LexState* ls)
{
   GCstr* s;
   if (ls->tok != TK_name) err_token(ls, TK_name);
   s = strV(&ls->tokval);
   lj_lex_next(ls);
   return s;
}
