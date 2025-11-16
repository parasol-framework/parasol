// Lexical analyzer.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// Major portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h

#define lj_lex_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_buf.h"
#include "lj_str.h"

#if LJ_HASFFI
#include "lj_tab.h"
#include "lj_ctype.h"
#include "lj_cdata.h"
#include "lualib.h"
#endif

#include "lj_state.h"
#include "lj_lex.h"
#include "lj_parse.h"
#include "lj_char.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"
#include "../bytecode/lj_bcdump.h"

// Lua lexer token names.
static const char* const token_names[] = {
#define TKSTR1(name)      #name,
#define TKSTR2(name, sym) #sym,
TKDEF(TKSTR1, TKSTR2)
#undef TKSTR1
#undef TKSTR2
  nullptr
};

//********************************************************************************************************************

#define LEX_EOF       (-1)
#define lex_iseol(State) (State->c IS '\n' or State->c IS '\r')

// Get more input from reader.
static LJ_NOINLINE LexChar lex_more(LexState *State)
{
   size_t sz;
   auto p = State->rfunc(State->L, State->rdata, &sz);
   if (not p or not sz) return LEX_EOF;
   if (sz >= LJ_MAX_BUF) {
      if (sz != ~(size_t)0) lj_err_mem(State->L);
      sz = ~(uintptr_t)0 - (uintptr_t)p;
      if (sz >= LJ_MAX_BUF) sz = LJ_MAX_BUF - 1;
      State->endmark = 1;
   }
   State->pe = p + sz;
   State->p = p + 1;
   return (LexChar)(uint8_t)p[0];
}

// Get next character.

static LJ_AINLINE LexChar lex_next(LexState *State)
{
   return (State->c = State->p < State->pe ? (LexChar)(uint8_t)*State->p++ : lex_more(State));
}

// Save character.

static LJ_AINLINE void lex_save(LexState *State, LexChar c)
{
   lj_buf_putb(&State->sb, c);
}

// Save previous character and get next character.

static LJ_AINLINE LexChar lex_savenext(LexState *State)
{
   lex_save(State, State->c);
   return lex_next(State);
}

//********************************************************************************************************************
// Skip line break. Handles "\n", "\r", "\r\n" or "\n\r".

static void lex_newline(LexState *State)
{
   LexChar old = State->c;
   State->assert_condition(lex_iseol(State), "bad usage");
   lex_next(State);  //  Skip "\n" or "\r".
   if (lex_iseol(State) and State->c != old) lex_next(State);  //  Skip "\n\r" or "\r\n".
   if (uint32_t(++State->linenumber) >= LJ_MAX_LINE)
      lj_lex_error(State, State->tok, LJ_ERR_XLINES);
}

//********************************************************************************************************************
// Scanner for terminals

// Parse a number literal.

static void lex_number(LexState *State, TValue* tv)
{
   StrScanFmt fmt;
   LexChar c, xp = 'e';
   State->assert_condition(lj_char_isdigit(State->c), "bad usage");
   if ((c = State->c) IS '0' and (lex_savenext(State) | 0x20) IS 'x') xp = 'p';
   while (lj_char_isident(State->c) or State->c IS '.' or ((State->c IS '-' or State->c IS '+') and (c | 0x20) IS xp)) {
      c = State->c;
      lex_savenext(State);
   }

   lex_save(State, '\0');
   fmt = lj_strscan_scan((const uint8_t*)State->sb.b, sbuflen(&State->sb) - 1, tv,
      (LJ_DUALNUM ? STRSCAN_OPT_TOINT : STRSCAN_OPT_TONUM) |
      (LJ_HASFFI ? (STRSCAN_OPT_LL | STRSCAN_OPT_IMAG) : 0));

   if (LJ_DUALNUM and fmt IS STRSCAN_INT) {
      setitype(tv, LJ_TISNUM);
   }
   else if (fmt IS STRSCAN_NUM) {
      // Already in correct format.
#if LJ_HASFFI
   }
   else if (fmt != STRSCAN_ERROR) {
      lua_State* L = State->L;
      GCcdata* cd;
      State->assert_condition(fmt IS STRSCAN_I64 or fmt IS STRSCAN_U64 or fmt IS STRSCAN_IMAG,
         "unexpected number format %d", fmt);
      ctype_loadffi(L);
      if (fmt IS STRSCAN_IMAG) {
         cd = lj_cdata_new_(L, CTID_COMPLEX_DOUBLE, 2 * sizeof(double));
         ((double*)cdataptr(cd))[0] = 0;
         ((double*)cdataptr(cd))[1] = numV(tv);
      }
      else {
         cd = lj_cdata_new_(L, fmt IS STRSCAN_I64 ? CTID_INT64 : CTID_UINT64, 8);
         *(uint64_t*)cdataptr(cd) = tv->u64;
      }
      lj_parse_keepcdata(State, tv, cd);
#endif
   }
   else {
      State->assert_condition(fmt IS STRSCAN_ERROR,
         "unexpected number format %d", fmt);
      lj_lex_error(State, TK_number, LJ_ERR_XNUMBER);
   }
}

//********************************************************************************************************************
// Skip equal signs for "[=...=[" and "]=...=]" and return their count.

static int lex_skipeq(LexState *State)
{
   int count = 0;
   LexChar s = State->c;
   State->assert_condition(s IS '[' or s IS ']', "bad usage");
   while (lex_savenext(State) IS '=' and count < 0x20000000) count++;
   return (State->c IS s) ? count : (-count) - 1;
}

//********************************************************************************************************************
// Parse a long string or long comment (tv set to nullptr).

static void lex_longstring(LexState *State, TValue* tv, int sep)
{
   lex_savenext(State);  //  Skip second '['.
   if (lex_iseol(State))  //  Skip initial newline.
      lex_newline(State);
   for (;;) {
      switch (State->c) {
      case LEX_EOF:
         lj_lex_error(State, TK_eof, tv ? LJ_ERR_XLSTR : LJ_ERR_XLCOM);
         break;
      case ']':
         if (lex_skipeq(State) IS sep) {
            lex_savenext(State);  //  Skip second ']'.
            goto endloop;
         }
         break;
      case '\n':
      case '\r':
         lex_save(State, '\n');
         lex_newline(State);
         if (!tv) lj_buf_reset(&State->sb);  //  Don't waste space for comments.
         break;
      default:
         lex_savenext(State);
         break;
      }
   } endloop:
   if (tv) {
      GCstr* str = State->keepstr(std::string_view(State->sb.b + (2 + (MSize)sep),
         sbuflen(&State->sb) - 2 * (2 + (MSize)sep)));
      setstrV(State->L, tv, str);
   }
}

//********************************************************************************************************************
// Parse a string.

static void lex_string(LexState *State, TValue* tv)
{
   LexChar delim = State->c;  //  Delimiter is '\'' or '"'.
   lex_savenext(State);
   while (State->c != delim) {
      switch (State->c) {

      case LEX_EOF:
         lj_lex_error(State, TK_eof, LJ_ERR_XSTR);
         continue;

      case '\n':
      case '\r':
         lj_lex_error(State, TK_string, LJ_ERR_XSTR);
         continue;

      case '\\': {
         LexChar c = lex_next(State);  //  Skip the '\\'.
         switch (c) {
         case 'a': c = '\a'; break;
         case 'b': c = '\b'; break;
         case 'f': c = '\f'; break;
         case 'n': c = '\n'; break;
         case 'r': c = '\r'; break;
         case 't': c = '\t'; break;
         case 'v': c = '\v'; break;

         case 'x':  //  Hexadecimal escape '\xXX'.
            c = (lex_next(State) & 15u) << 4;
            if (!lj_char_isdigit(State->c)) {
               if (!lj_char_isxdigit(State->c)) goto err_xesc;
               c += 9 << 4;
            }
            c += (lex_next(State) & 15u);
            if (!lj_char_isdigit(State->c)) {
               if (!lj_char_isxdigit(State->c)) goto err_xesc;
               c += 9;
            }
            break;

         case 'u':  //  Unicode escape '\u{XX...}'.
            if (lex_next(State) != '{') goto err_xesc;
            lex_next(State);
            c = 0;
            do {
               c = (c << 4) | (State->c & 15u);
               if (!lj_char_isdigit(State->c)) {
                  if (!lj_char_isxdigit(State->c)) goto err_xesc;
                  c += 9;
               }
               if (c >= 0x110000) goto err_xesc;  //  Out of Unicode range.
            } while (lex_next(State) != '}');

            if (c < 0x800) {
               if (c < 0x80) break;
               lex_save(State, 0xc0 | (c >> 6));
            }
            else {
               if (c >= 0x10000) {
                  lex_save(State, 0xf0 | (c >> 18));
                  lex_save(State, 0x80 | ((c >> 12) & 0x3f));
               }
               else {
                  if (c >= 0xd800 and c < 0xe000) goto err_xesc;  //  No surrogates.
                  lex_save(State, 0xe0 | (c >> 12));
               }
               lex_save(State, 0x80 | ((c >> 6) & 0x3f));
            }
            c = 0x80 | (c & 0x3f);
            break;

         case 'z':  //  Skip whitespace.
            lex_next(State);
            while (lj_char_isspace(State->c))
               if (lex_iseol(State)) lex_newline(State); else lex_next(State);
            continue;

         case '\n': case '\r': lex_save(State, '\n'); lex_newline(State); continue;

         case '\\': case '\"': case '\'': break;

         case LEX_EOF: continue;

         default:
            if (!lj_char_isdigit(c)) goto err_xesc;
            c -= '0';  //  Decimal escape '\ddd'.
            if (lj_char_isdigit(lex_next(State))) {
               c = c * 10 + (State->c - '0');
               if (lj_char_isdigit(lex_next(State))) {
                  c = c * 10 + (State->c - '0');
                  if (c > 255) {
                  err_xesc:
                     lj_lex_error(State, TK_string, LJ_ERR_XESC);
                  }
                  lex_next(State);
               }
            }
            lex_save(State, c);
            continue;
         }
         lex_save(State, c);
         lex_next(State);
         continue;
      }

      default:
         lex_savenext(State);
         break;
      }
   }

   lex_savenext(State);  //  Skip trailing delimiter.
   setstrV(State->L, tv, State->keepstr(std::string_view(State->sb.b + 1, sbuflen(&State->sb) - 2)));
}

//********************************************************************************************************************
// Get next lexical token.

static LexToken lex_scan(LexState *State, TValue *tv)
{
   lj_buf_reset(&State->sb);

   while (true) {
      if (lj_char_isident(State->c)) {
         GCstr *s;
         if (lj_char_isdigit(State->c)) {  // Numeric literal.
            lex_number(State, tv);
            return TK_number;
         }

         // Identifier or reserved word.
         do {
            lex_savenext(State);
         } while (lj_char_isident(State->c));

         s = State->keepstr(std::string_view(State->sb.b, sbuflen(&State->sb)));
         setstrV(State->L, tv, s);
         if (s->reserved > 0) {  // Reserved word?
            LexToken tok = TK_OFS + s->reserved;
            return tok;
         }
         return TK_name;
      }

      switch (State->c) {
      case '\n':
      case '\r':
         lex_newline(State);
         continue;

      case ' ':
      case '\t':
      case '\v':
      case '\f':
         lex_next(State);
         continue;

      case '-':
         lex_next(State);
         if (State->c IS '=') { lex_next(State); return TK_csub; }
         if (State->c != '-') return '-';
         lex_next(State);
         if (State->c IS '[') {  // Long comment "--[=*[...]=*]".
            int sep = lex_skipeq(State);
            lj_buf_reset(&State->sb);  //  `lex_skipeq' may dirty the buffer
            if (sep >= 0) {
               lex_longstring(State, nullptr, sep);
               lj_buf_reset(&State->sb);
               continue;
            }
         }

         // Short comment "--.*\n".

         while (!lex_iseol(State) and State->c != LEX_EOF) lex_next(State);
         continue;

      case '[': {
         int sep = lex_skipeq(State);
         if (sep >= 0) {
            lex_longstring(State, tv, sep);
            return TK_string;
         }
         else if (sep IS -1) {
            return '[';
         }
         else {
            lj_lex_error(State, TK_string, LJ_ERR_XLDELIM);
            continue;
         }
      }
      case '+':
         lex_next(State);
         if (State->c IS '=') { lex_next(State); return TK_cadd; }
         if (State->c IS '+') { lex_next(State); return TK_plusplus; }
         return '+';

      case '*':
         lex_next(State);
         if (State->c IS '=') { lex_next(State); return TK_cmul; }
         return '*';

      case '/':
         lex_next(State);
         if (State->c IS '=') { lex_next(State); return TK_cdiv; }
         if (State->c IS '/') {
            while (State->c != '\n' and State->c != LEX_EOF) lex_next(State);
            continue;
         }
         else return '/';

      case '%':
         lex_next(State);
         if (State->c IS '=') { lex_next(State); return TK_cmod; }
         return '%';

      case '!': { // Support for !=
         lex_next(State);
         if (State->c != '=') return '!';
         else { lex_next(State); return TK_ne; }
      }

      case '=':
         lex_next(State);
         if (State->c != '=') return '='; else { lex_next(State); return TK_eq; }

      case '<':
         lex_next(State);
         if (State->c IS '=') { lex_next(State); return TK_le; }
         else if (State->c IS '<') { lex_next(State); return TK_shl; }  // PARASOL PATCHED IN: Support for '<<' operator
         else return '<';

      case '>':
         lex_next(State);
         if (State->c IS '=') { lex_next(State); return TK_ge; }
         else if (State->c IS '>') { lex_next(State); return TK_shr; }  // PARASOL PATCHED IN: Support for '>>' operator
         else return '>';

      case '~': // To be deprecated: ~=
         lex_next(State);
         if (State->c != '=') return '~'; else { lex_next(State); return TK_ne; }

      case ':': // Support for ':>' ternary operator
         lex_next(State);
         if (State->c IS '>') { lex_next(State); return TK_ternary_sep; }
         else return ':';

      case '?':
         lex_next(State);
         if (State->c IS '=') { lex_next(State); return TK_cif_empty; }
         if (State->c IS '?') { lex_next(State); return TK_if_empty; }
         return '?';

      case '"':
      case '\'':
         lex_string(State, tv);
         return TK_string;

      case '.':
         if (lex_savenext(State) IS '.') {
            lex_next(State);
            if (State->c IS '.') {
               lex_next(State);
               return TK_dots;   //  ...
            }
            if (State->c IS '=') { lex_next(State); return TK_cconcat; }
            return TK_concat;   //  ..
         }
         else if (!lj_char_isdigit(State->c)) {
            return '.';
         }
         else {
            lex_number(State, tv);
            return TK_number;
         }

      case LEX_EOF:
         return TK_eof;

      default: {
         LexChar c = State->c;
         lex_next(State);
         return c;  //  Single-char tokens (+ - / ...).
      }
      }
   }
}

//********************************************************************************************************************
// LexState constructor

LexState::LexState(lua_State* L, lua_Reader Rfunc, void* Rdata, std::string_view Chunkarg, std::optional<std::string_view> Mode)
{
   int header = 0;
   this->L = L;
   this->fs = nullptr;
   this->pe = this->p = nullptr;
   this->vstack = nullptr;
   this->sizevstack = 0;
   this->vtop = 0;
   this->bcstack = nullptr;
   this->sizebcstack = 0;
   this->tok = 0;
   this->lookahead = TK_eof;  //  No look-ahead token.
   this->linenumber = 1;
   this->lastline = 1;
   this->level = 0;
   this->ternary_depth = 0;
   this->pending_if_empty_colon = 0;
   this->endmark = 0;
   this->is_bytecode = 0;
   this->rfunc = Rfunc;
   this->rdata = Rdata;
   this->chunkarg = Chunkarg.data();
   this->mode = Mode.has_value() ? Mode->data() : nullptr;
   this->empty_string_constant = nullptr;

   // Initialize string buffer
   lj_buf_init(L, &this->sb);

   lex_next(this);  //  Read-ahead first char.
   if (this->c IS 0xef and this->p + 2 <= this->pe and (uint8_t)this->p[0] IS 0xbb and
      (uint8_t)this->p[1] IS 0xbf) {  // Skip UTF-8 BOM (if buffered).
      this->p += 2;
      lex_next(this);
      header = 1;
   }

   if (this->c IS '#') {  // Skip POSIX #! header line.
      do {
         lex_next(this);
         if (this->c IS LEX_EOF) return;
      } while (!lex_iseol(this));
      lex_newline(this);
      header = 1;
   }

   if (this->c IS LUA_SIGNATURE[0]) {  // Bytecode dump.
      if (header) {

         // Loading bytecode with an extra header is disabled for security
         // reasons. This may circumvent the usual check for bytecode vs.
         // Lua code by looking at the first char. Since this is a potential
         // security violation no attempt is made to echo the chunkname either.

         setstrV(L, L->top++, lj_err_str(L, LJ_ERR_BCBAD));
         lj_err_throw(L, LUA_ERRSYNTAX);
      }
      this->is_bytecode = 1;
   }
}

//********************************************************************************************************************
// LexState constructor for direct bytecode reading (used by library initialization)

LexState::LexState(lua_State* L, const char* BytecodePtr, GCstr* ChunkName)
{
   this->L = L;
   this->fs = nullptr;
   this->p = BytecodePtr;
   this->pe = (const char*)~(uintptr_t)0;
   this->vstack = nullptr;
   this->sizevstack = 0;
   this->vtop = 0;
   this->bcstack = nullptr;
   this->sizebcstack = 0;
   this->tok = 0;
   this->lookahead = TK_eof;
   this->linenumber = 1;
   this->lastline = 1;
   this->level = (BCDUMP_F_STRIP | (LJ_BE * BCDUMP_F_BE));
   this->ternary_depth = 0;
   this->pending_if_empty_colon = 0;
   this->endmark = 0;
   this->is_bytecode = 1;
   this->c = -1;
   this->chunkname = ChunkName;
   this->chunkarg = nullptr;
   this->mode = nullptr;
   this->rfunc = nullptr;
   this->rdata = nullptr;
   this->empty_string_constant = nullptr;

   // Initialize string buffer
   lj_buf_init(L, &this->sb);
}

//********************************************************************************************************************
// LexState destructor

LexState::~LexState()
{
   // Only cleanup if L is set (indicates proper initialization)
   if (this->L) {
      global_State* g = G(this->L);
      if (this->bcstack) lj_mem_freevec(g, this->bcstack, this->sizebcstack, BCInsLine);
      if (this->vstack) lj_mem_freevec(g, this->vstack, this->sizevstack, VarInfo);
      lj_buf_free(g, &this->sb);
   }
}

//********************************************************************************************************************
// Return next lexical token.

void LexState::next()
{
   this->lastline = this->linenumber;
   if (LJ_LIKELY(this->lookahead IS TK_eof)) {  // No lookahead token?
      this->tok = lex_scan(this, &this->tokval);  //  Get next token.
   }
   else {  // Otherwise return lookahead token.
      this->tok = this->lookahead;
      this->lookahead = TK_eof;
      this->tokval = this->lookaheadval;
   }
}

//********************************************************************************************************************
// Look ahead for the next token.

LexToken LexState::lookahead_token()
{
   this->assert_condition(this->lookahead IS TK_eof, "double lookahead");
   this->lookahead = lex_scan(this, &this->lookaheadval);
   return this->lookahead;
}

//********************************************************************************************************************
// Convert token to string.

const char* LexState::token2str(LexToken Tok)
{
   if (Tok > TK_OFS)
      return token_names[Tok - TK_OFS - 1];
   else if (!lj_char_iscntrl(Tok))
      return lj_strfmt_pushf(this->L, "%c", Tok);
   else
      return lj_strfmt_pushf(this->L, "char(%d)", Tok);
}

//********************************************************************************************************************
// Lexer error. Wrapper for LexState::error()

void lj_lex_error(LexState *State, LexToken tok, ErrMsg em, ...)
{
   va_list argp;
   va_start(argp, em);
   const char* tokstr;

   if (not tok) tokstr = nullptr;
   else if (tok IS TK_name or tok IS TK_string or tok IS TK_number) {
      lex_save(State, '\0');
      tokstr = State->sb.b;
   }
   else tokstr = State->token2str(tok);

   lj_err_lex(State->L, State->chunkname, tokstr, State->linenumber, em, argp);
   va_end(argp);
}
//********************************************************************************************************************
// Initialize strings for reserved words inside the Lua state.

void lj_reserve_words(lua_State *Lua)
{
   uint32_t i;
   for (i = 0; i < TK_RESERVED; i++) {
      GCstr *s = lj_str_newz(Lua, token_names[i]);
      fixstring(s);  //  Reserved words are never collected.
      s->reserved = (uint8_t)(i + 1);
   }
}
