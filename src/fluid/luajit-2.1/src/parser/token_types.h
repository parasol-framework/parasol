// Parser token types and helpers.
// Copyright (C) 2005-2022 Mike Pall.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

#include "lj_obj.h"
#include "lj_lex.h"

enum class TokenKind : uint16_t {
   Unknown = 0,
   LeftParen = '(',
   RightParen = ')',
   LeftBracket = '[',
   RightBracket = ']',
   LeftBrace = '{',
   RightBrace = '}',
   Dot = '.',
   Colon = ':',
   Comma = ',',
   Semicolon = ';',
   Plus = '+',
   Minus = '-',
   Star = '*',
   Slash = '/',
   Percent = '%',
   Caret = '^',
   Hash = '#',
   Equal = '=',
   Less = '<',
   Greater = '>',
   Tilde = '~',
   Question = '?',
   Quote = '\'',
   DoubleQuote = '"',
   Backslash = '\\',
   Name = TK_name,
   Number = TK_number,
   String = TK_string,
   ReservedAnd = TK_and,
   ReservedBreak = TK_break,
   ReservedContinue = TK_continue,
   ReservedDefer = TK_defer,
   ReservedDo = TK_do,
   ReservedElse = TK_else,
   ReservedElseif = TK_elseif,
   ReservedEnd = TK_end,
   ReservedFalse = TK_false,
   ReservedFor = TK_for,
   ReservedFunction = TK_function,
   ReservedIf = TK_if,
   ReservedIn = TK_in,
   ReservedIs = TK_is,
   ReservedLocal = TK_local,
   ReservedNil = TK_nil,
   ReservedNot = TK_not,
   ReservedOr = TK_or,
   ReservedRepeat = TK_repeat,
   ReservedReturn = TK_return,
   ReservedThen = TK_then,
   ReservedTrue = TK_true,
   ReservedUntil = TK_until,
   ReservedWhile = TK_while,
   IfEmpty = TK_if_empty,
   Concat = TK_concat,
   Dots = TK_dots,
   Eq = TK_eq,
   Ge = TK_ge,
   Le = TK_le,
   Ne = TK_ne,
   Shl = TK_shl,
   Shr = TK_shr,
   TernarySeparator = TK_ternary_sep,
   CompoundAdd = TK_cadd,
   CompoundSub = TK_csub,
   CompoundMul = TK_cmul,
   CompoundDiv = TK_cdiv,
   CompoundConcat = TK_cconcat,
   CompoundMod = TK_cmod,
   CompoundIfEmpty = TK_cif_empty,
   PlusPlus = TK_plusplus,
   Eof = TK_eof
};

struct TokenValueStorage {
   TValue value;
};

struct TokenPayload {
   using Variant = std::variant<std::monostate, double, GCstr*, TokenValueStorage>;

   TokenPayload();
   explicit TokenPayload(double NumberValue);
   explicit TokenPayload(GCstr* StringValue);
   explicit TokenPayload(const TValue& Value);

   [[nodiscard]] bool has_number() const;
   [[nodiscard]] bool has_string() const;
   [[nodiscard]] bool has_value() const;
   [[nodiscard]] double number_value(double Fallback = 0.0) const;
   [[nodiscard]] GCstr* string_value() const;
   [[nodiscard]] const TValue* tvalue() const;

   Variant data;
};

struct Token {
   TokenKind kind = TokenKind::Unknown;
   TokenPayload payload;
   LexToken raw = 0;
   BCLine line = 0;
   BCLine last_line = 0;
   std::size_t column = 0;
   std::size_t offset = 0;

   [[nodiscard]] bool is_identifier() const;
   [[nodiscard]] bool is_literal() const;
   [[nodiscard]] bool is(TokenKind Kind) const;
   [[nodiscard]] GCstr* identifier() const;
   [[nodiscard]] double number_value(double Fallback = 0.0) const;
};

TokenKind token_kind_from_lex(LexToken Token);
std::string describe_token_kind(TokenKind Kind);
Token make_token_from_lex(LexState& Lex, LexToken TokenValue, const TValue& Value);

