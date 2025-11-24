// Copyright (C) 2025 Paul Manias

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <parasol/main.h>

#include "lexer.h"

// Strongly typed representation of lexer tokens.
enum class TokenKind : uint16_t {
   Unknown = 0,
#define TOKEN_KIND_ENUM(name) name = TK_##name,
#define TOKEN_KIND_ENUM_SYM(name, sym) name = TK_##name,
   Identifier = TK_name,
   Number = TK_number,
   String = TK_string,
   Nil = TK_nil,
   TrueToken = TK_true,
   FalseToken = TK_false,
   Function = TK_function,
   Local = TK_local,
   EndToken = TK_end,
   ReturnToken = TK_return,
   If = TK_if,
   Else = TK_else,
   ElseIf = TK_elseif,
   For = TK_for,
   WhileToken = TK_while,
   Repeat = TK_repeat,
   Until = TK_until,
   DoToken = TK_do,
   ThenToken = TK_then,
   InToken = TK_in,
   BreakToken = TK_break,
   ContinueToken = TK_continue,
   DeferToken = TK_defer,
   AndToken = TK_and,
   OrToken = TK_or,
   NotToken = TK_not,
   IsToken = TK_is,
   TernarySep = TK_ternary_sep,
   Dots = TK_dots,
   Cat = TK_concat,
   Equal = TK_eq,
   NotEqual = TK_ne,
   LessEqual = TK_le,
   GreaterEqual = TK_ge,
   ShiftLeft = TK_shl,
   ShiftRight = TK_shr,
   CompoundAdd = TK_cadd,
   CompoundSub = TK_csub,
   CompoundMul = TK_cmul,
   CompoundDiv = TK_cdiv,
   CompoundMod = TK_cmod,
   CompoundConcat = TK_cconcat,
   CompoundIfEmpty = TK_cif_empty,
   Presence = TK_if_empty, // NOTE: This single token covers use of both `if present?? then` (postfix) and `(variable ?? default_value)` (if empty).
   PlusPlus = TK_plusplus,
   EndOfFile = TK_eof,
#undef TOKEN_KIND_ENUM
#undef TOKEN_KIND_ENUM_SYM
   LeftParen = '(',
   RightParen = ')',
   LeftBrace = '{',
   RightBrace = '}',
   LeftBracket = '[',
   RightBracket = ']',
   Dot = '.',
   Colon = ':',
   Comma = ',',
   Semicolon = ';',
   Equals = '=',
   Plus = '+',
   Minus = '-',
   Multiply = '*',
   Divide = '/',
   Modulo = '%',
   Question = '?'
};

[[nodiscard]] CSTRING token_kind_name(TokenKind kind, LexState& lex);

struct TokenPayload {
   TokenPayload();
   void assign(lua_State* state, const TValue& value);

   [[nodiscard]] bool has_value() const { return this->has_payload; }
   [[nodiscard]] const TValue& value() const { return this->payload; }
   [[nodiscard]] GCstr* as_string() const;
   [[nodiscard]] double as_number() const;

private:
   bool has_payload = false;
   TValue payload;
   lua_State* owner = nullptr;
};

class Token {
public:
   Token() = default;

   [[nodiscard]] static Token from_current(LexState& state);
   [[nodiscard]] static Token from_lookahead(LexState& state);
   [[nodiscard]] static Token from_buffered(LexState& state, const LexState::BufferedToken& buffered);

   [[nodiscard]] TokenKind kind() const { return this->token_kind; }
   [[nodiscard]] LexToken raw() const { return this->raw_token; }
   [[nodiscard]] SourceSpan span() const { return this->source; }
   [[nodiscard]] bool is(TokenKind kind) const { return this->token_kind IS kind; }
   [[nodiscard]] bool is_identifier() const;
   [[nodiscard]] bool is_literal() const;
   [[nodiscard]] bool is_eof() const;
   [[nodiscard]] const TokenPayload& payload() const { return this->data; }
   [[nodiscard]] GCstr* identifier() const;

private:
   TokenKind token_kind = TokenKind::Unknown;
   LexToken raw_token = 0;
   SourceSpan source;
   TokenPayload data;
};

TokenKind to_token_kind(LexToken token);

