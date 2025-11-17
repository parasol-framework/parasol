#include "parser/token_types.h"

TokenPayload::TokenPayload()
   : data(std::monostate{})
{
}

TokenPayload::TokenPayload(double NumberValue)
   : data(NumberValue)
{
}

TokenPayload::TokenPayload(GCstr* StringValue)
   : data(StringValue)
{
}

TokenPayload::TokenPayload(const TValue& Value)
   : data(TokenValueStorage{Value})
{
}

bool TokenPayload::has_number() const
{
   return std::holds_alternative<double>(this->data);
}

bool TokenPayload::has_string() const
{
   return std::holds_alternative<GCstr*>(this->data);
}

bool TokenPayload::has_value() const
{
   return !std::holds_alternative<std::monostate>(this->data);
}

double TokenPayload::number_value(double Fallback) const
{
   if (std::holds_alternative<double>(this->data))
      return std::get<double>(this->data);
   if (std::holds_alternative<TokenValueStorage>(this->data)) {
      const TValue& Value = std::get<TokenValueStorage>(this->data).value;
#if LJ_DUALNUM
      if (tvisint(&Value)) return double(intV(&Value));
#endif
      if (tvisnum(&Value)) return numV(&Value);
   }
   return Fallback;
}

GCstr* TokenPayload::string_value() const
{
   if (std::holds_alternative<GCstr*>(this->data))
      return std::get<GCstr*>(this->data);
   if (std::holds_alternative<TokenValueStorage>(this->data)) {
      const TValue& Value = std::get<TokenValueStorage>(this->data).value;
      if (tvisstr(&Value)) return strV(&Value);
   }
   return nullptr;
}

const TValue* TokenPayload::tvalue() const
{
   if (std::holds_alternative<TokenValueStorage>(this->data))
      return &std::get<TokenValueStorage>(this->data).value;
   return nullptr;
}

bool Token::is_identifier() const
{
   return this->kind IS TokenKind::Name;
}

bool Token::is_literal() const
{
   return this->kind IS TokenKind::Number or this->kind IS TokenKind::String or this->kind IS TokenKind::ReservedTrue or this->kind IS TokenKind::ReservedFalse or this->kind IS TokenKind::ReservedNil;
}

bool Token::is(TokenKind Kind) const
{
   return this->kind IS Kind;
}

GCstr* Token::identifier() const
{
   return this->payload.string_value();
}

double Token::number_value(double Fallback) const
{
   return this->payload.number_value(Fallback);
}

static std::string describe_symbol_token(TokenKind Kind)
{
   switch (Kind) {
   case TokenKind::LeftParen: return "'('";
   case TokenKind::RightParen: return "')'";
   case TokenKind::LeftBracket: return "'['";
   case TokenKind::RightBracket: return "']'";
   case TokenKind::LeftBrace: return "'{'";
   case TokenKind::RightBrace: return "'}'";
   case TokenKind::Dot: return "'.'";
   case TokenKind::Colon: return "':'";
   case TokenKind::Comma: return "','";
   case TokenKind::Semicolon: return "';'";
   case TokenKind::Plus: return "'+'";
   case TokenKind::Minus: return "'-'";
   case TokenKind::Star: return "'*'";
   case TokenKind::Slash: return "'/'";
   case TokenKind::Percent: return "'%'";
   case TokenKind::Caret: return "'^'";
   case TokenKind::Hash: return "'#'";
   case TokenKind::Equal: return "'='";
   case TokenKind::Less: return "'<'";
   case TokenKind::Greater: return "'>'";
   case TokenKind::Tilde: return "'~'";
   case TokenKind::Question: return "'?'";
   default:
      break;
   }
   return std::string("token");
}

static std::string describe_reserved_token(TokenKind Kind)
{
   switch (Kind) {
   case TokenKind::ReservedAnd: return "'and'";
   case TokenKind::ReservedBreak: return "'break'";
   case TokenKind::ReservedContinue: return "'continue'";
   case TokenKind::ReservedDefer: return "'defer'";
   case TokenKind::ReservedDo: return "'do'";
   case TokenKind::ReservedElse: return "'else'";
   case TokenKind::ReservedElseif: return "'elseif'";
   case TokenKind::ReservedEnd: return "'end'";
   case TokenKind::ReservedFalse: return "'false'";
   case TokenKind::ReservedFor: return "'for'";
   case TokenKind::ReservedFunction: return "'function'";
   case TokenKind::ReservedIf: return "'if'";
   case TokenKind::ReservedIn: return "'in'";
   case TokenKind::ReservedIs: return "'is'";
   case TokenKind::ReservedLocal: return "'local'";
   case TokenKind::ReservedNil: return "'nil'";
   case TokenKind::ReservedNot: return "'not'";
   case TokenKind::ReservedOr: return "'or'";
   case TokenKind::ReservedRepeat: return "'repeat'";
   case TokenKind::ReservedReturn: return "'return'";
   case TokenKind::ReservedThen: return "'then'";
   case TokenKind::ReservedTrue: return "'true'";
   case TokenKind::ReservedUntil: return "'until'";
   case TokenKind::ReservedWhile: return "'while'";
   default:
      break;
   }
   return std::string("reserved token");
}

TokenKind token_kind_from_lex(LexToken Token)
{
   if (Token < TK_OFS) {
      switch (Token) {
      case '(': return TokenKind::LeftParen;
      case ')': return TokenKind::RightParen;
      case '[': return TokenKind::LeftBracket;
      case ']': return TokenKind::RightBracket;
      case '{': return TokenKind::LeftBrace;
      case '}': return TokenKind::RightBrace;
      case '.': return TokenKind::Dot;
      case ':': return TokenKind::Colon;
      case ',': return TokenKind::Comma;
      case ';': return TokenKind::Semicolon;
      case '+': return TokenKind::Plus;
      case '-': return TokenKind::Minus;
      case '*': return TokenKind::Star;
      case '/': return TokenKind::Slash;
      case '%': return TokenKind::Percent;
      case '^': return TokenKind::Caret;
      case '#': return TokenKind::Hash;
      case '=': return TokenKind::Equal;
      case '<': return TokenKind::Less;
      case '>': return TokenKind::Greater;
      case '~': return TokenKind::Tilde;
      case '?': return TokenKind::Question;
      default:
         return TokenKind::Unknown;
      }
   }

   switch (Token) {
   case TK_name: return TokenKind::Name;
   case TK_number: return TokenKind::Number;
   case TK_string: return TokenKind::String;
   case TK_and: return TokenKind::ReservedAnd;
   case TK_break: return TokenKind::ReservedBreak;
   case TK_continue: return TokenKind::ReservedContinue;
   case TK_defer: return TokenKind::ReservedDefer;
   case TK_do: return TokenKind::ReservedDo;
   case TK_else: return TokenKind::ReservedElse;
   case TK_elseif: return TokenKind::ReservedElseif;
   case TK_end: return TokenKind::ReservedEnd;
   case TK_false: return TokenKind::ReservedFalse;
   case TK_for: return TokenKind::ReservedFor;
   case TK_function: return TokenKind::ReservedFunction;
   case TK_if: return TokenKind::ReservedIf;
   case TK_in: return TokenKind::ReservedIn;
   case TK_is: return TokenKind::ReservedIs;
   case TK_local: return TokenKind::ReservedLocal;
   case TK_nil: return TokenKind::ReservedNil;
   case TK_not: return TokenKind::ReservedNot;
   case TK_or: return TokenKind::ReservedOr;
   case TK_repeat: return TokenKind::ReservedRepeat;
   case TK_return: return TokenKind::ReservedReturn;
   case TK_then: return TokenKind::ReservedThen;
   case TK_true: return TokenKind::ReservedTrue;
   case TK_until: return TokenKind::ReservedUntil;
   case TK_while: return TokenKind::ReservedWhile;
   case TK_if_empty: return TokenKind::IfEmpty;
   case TK_concat: return TokenKind::Concat;
   case TK_dots: return TokenKind::Dots;
   case TK_eq: return TokenKind::Eq;
   case TK_ge: return TokenKind::Ge;
   case TK_le: return TokenKind::Le;
   case TK_ne: return TokenKind::Ne;
   case TK_shl: return TokenKind::Shl;
   case TK_shr: return TokenKind::Shr;
   case TK_ternary_sep: return TokenKind::TernarySeparator;
   case TK_cadd: return TokenKind::CompoundAdd;
   case TK_csub: return TokenKind::CompoundSub;
   case TK_cmul: return TokenKind::CompoundMul;
   case TK_cdiv: return TokenKind::CompoundDiv;
   case TK_cconcat: return TokenKind::CompoundConcat;
   case TK_cmod: return TokenKind::CompoundMod;
   case TK_cif_empty: return TokenKind::CompoundIfEmpty;
   case TK_plusplus: return TokenKind::PlusPlus;
   case TK_eof: return TokenKind::Eof;
   default:
      return TokenKind::Unknown;
   }
}

std::string describe_token_kind(TokenKind Kind)
{
   if (Kind < TokenKind::Name) {
      return describe_symbol_token(Kind);
   }
   if (Kind IS TokenKind::Name) return std::string("identifier");
   if (Kind IS TokenKind::Number) return std::string("number");
   if (Kind IS TokenKind::String) return std::string("string");
   if (Kind IS TokenKind::Dots) return std::string("'...'" );
   if (Kind IS TokenKind::Concat) return std::string("'..'" );
   if (Kind IS TokenKind::IfEmpty) return std::string("'??'");
   if (Kind IS TokenKind::PlusPlus) return std::string("'++'");
   if (Kind IS TokenKind::CompoundIfEmpty) return std::string("'?='");
   if (Kind IS TokenKind::Eof) return std::string("<eof>");
   return describe_reserved_token(Kind);
}

static TokenPayload payload_from_lex(LexState& Lex, TokenKind Kind)
{
   if (Kind IS TokenKind::Number) {
#if LJ_DUALNUM
      if (tvisint(&Lex.tokval)) return TokenPayload(double(intV(&Lex.tokval)));
#endif
      if (tvisnum(&Lex.tokval)) return TokenPayload(numV(&Lex.tokval));
#if LJ_HASFFI
      if (tviscdata(&Lex.tokval)) return TokenPayload(Lex.tokval);
#endif
   }
   else if (Kind IS TokenKind::Name or Kind IS TokenKind::String) {
      return TokenPayload(strV(&Lex.tokval));
   }
   return TokenPayload();
}

Token make_token_from_lex(LexState& Lex, LexToken TokenValue, const TValue& Value)
{
   Token TokenInfo;
   TokenInfo.kind = token_kind_from_lex(TokenValue);
   TokenInfo.raw = TokenValue;
   TokenInfo.line = Lex.linenumber;
   TokenInfo.last_line = Lex.lastline;
   TokenInfo.column = 0;
   TokenInfo.offset = 0;
   TokenInfo.payload = payload_from_lex(Lex, TokenInfo.kind);
   if (TokenInfo.payload.has_value()) {
      if (std::holds_alternative<TokenValueStorage>(TokenInfo.payload.data))
         TokenInfo.payload = TokenPayload(Value);
   }
   return TokenInfo;
}

