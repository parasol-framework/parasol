// Lexical analyser.
// Copyright (C) 2025 Paul Manias

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <deque>
#include <optional>
#include <span>
#include <stdarg.h>
#include <string_view>
#include <cstdint>

#include "lj_obj.h"
#include "lj_err.h"

struct SourceSpan {
   BCLine line = 0;
   BCLine column = 0;
   size_t offset = 0;
};

//********************************************************************************************************************
// Token definitions using C++20 constexpr structures.
// Replaces the legacy X-Macro pattern for improved debuggability and IDE support.

struct TokenDefinition {
   std::string_view name;    // Token identifier (e.g., "and", "if_empty")
   std::string_view symbol;  // Display symbol (e.g., "and", "??")
   bool reserved;            // True for reserved words that cannot be used as identifiers

   [[nodiscard]] constexpr bool is_reserved() const noexcept { return reserved; }
};

// Complete token definitions array. Order defines enum values starting from TK_OFS (256).
// Reserved words must appear first, ending with "while", to correctly compute TK_RESERVED.
inline constexpr std::array TOKEN_DEFINITIONS = {
   // Reserved words (name matches symbol) - these must come first
   TokenDefinition{"and",      "and",      true},
   TokenDefinition{"break",    "break",    true},
   TokenDefinition{"continue", "continue", true},
   TokenDefinition{"defer",    "defer",    true},
   TokenDefinition{"do",       "do",       true},
   TokenDefinition{"else",     "else",     true},
   TokenDefinition{"elseif",   "elseif",   true},
   TokenDefinition{"end",      "end",      true},
   TokenDefinition{"false",    "false",    true},
   TokenDefinition{"for",      "for",      true},
   TokenDefinition{"function", "function", true},
   TokenDefinition{"if",       "if",       true},
   TokenDefinition{"in",       "in",       true},
   TokenDefinition{"is",       "is",       true},
   TokenDefinition{"local",    "local",    true},
   TokenDefinition{"nil",      "nil",      true},
   TokenDefinition{"not",      "not",      true},
   TokenDefinition{"or",       "or",       true},
   TokenDefinition{"repeat",   "repeat",   true},
   TokenDefinition{"return",   "return",   true},
   TokenDefinition{"then",     "then",     true},
   TokenDefinition{"true",     "true",     true},
   TokenDefinition{"until",    "until",    true},
   TokenDefinition{"while",    "while",    true},  // Last reserved word

   // Non-reserved tokens with explicit symbols
   TokenDefinition{"if_empty",    "??",       false},
   TokenDefinition{"safe_field",  "?.",       false},
   TokenDefinition{"safe_index",  "?[",       false},
   TokenDefinition{"safe_method", "?:",       false},
   TokenDefinition{"concat",      "..",       false},
   TokenDefinition{"dots",        "...",      false},
   TokenDefinition{"eq",          "==",       false},
   TokenDefinition{"ge",          ">=",       false},
   TokenDefinition{"le",          "<=",       false},
   TokenDefinition{"ne",          "~=",       false},
   TokenDefinition{"shl",         "<<",       false},
   TokenDefinition{"shr",         ">>",       false},
   TokenDefinition{"ternary_sep", ":>",       false},
   TokenDefinition{"number",      "<number>", false},
   TokenDefinition{"name",        "<name>",   false},
   TokenDefinition{"string",      "<string>", false},
   TokenDefinition{"cadd",        "+=",       false},
   TokenDefinition{"csub",        "-=",       false},
   TokenDefinition{"cmul",        "*=",       false},
   TokenDefinition{"cdiv",        "/=",       false},
   TokenDefinition{"cconcat",     "..=",      false},
   TokenDefinition{"cmod",        "%=",       false},
   TokenDefinition{"cif_empty",   "?=",       false},
   TokenDefinition{"plusplus",    "++",       false},
   TokenDefinition{"eof",         "<eof>",    false},
};

// Compile-time count of reserved words
inline constexpr size_t generate_reserved_count() noexcept {
   size_t count = 0;
   for (const auto& def : TOKEN_DEFINITIONS) {
      if (def.is_reserved()) ++count;
   }
   return count;
}

// Compile-time token symbol lookup by index
[[nodiscard]] inline constexpr std::string_view token_symbol(size_t Index) noexcept {
   if (Index < TOKEN_DEFINITIONS.size()) {
      return TOKEN_DEFINITIONS[Index].symbol;
   }
   return "<invalid>";
}

// Compile-time token name lookup by index
[[nodiscard]] inline constexpr std::string_view token_name(size_t Index) noexcept {
   if (Index < TOKEN_DEFINITIONS.size()) {
      return TOKEN_DEFINITIONS[Index].name;
   }
   return "<invalid>";
}

// Token enum values. Order must match TOKEN_DEFINITIONS array.
// Note: TK_OFS is the offset base, and token values start at TK_OFS + 1.
enum {
   TK_OFS = 256,
   TK_and,          // = 257
   TK_break,
   TK_continue,
   TK_defer,
   TK_do,
   TK_else,
   TK_elseif,
   TK_end,
   TK_false,
   TK_for,
   TK_function,
   TK_if,
   TK_in,
   TK_is,
   TK_local,
   TK_nil,
   TK_not,
   TK_or,
   TK_repeat,
   TK_return,
   TK_then,
   TK_true,
   TK_until,
   TK_while,        // Last reserved word
   TK_if_empty,
   TK_safe_field,
   TK_safe_index,
   TK_safe_method,
   TK_concat,
   TK_dots,
   TK_eq,
   TK_ge,
   TK_le,
   TK_ne,
   TK_shl,
   TK_shr,
   TK_ternary_sep,
   TK_number,
   TK_name,
   TK_string,
   TK_cadd,
   TK_csub,
   TK_cmul,
   TK_cdiv,
   TK_cconcat,
   TK_cmod,
   TK_cif_empty,
   TK_plusplus,
   TK_eof,
   TK_RESERVED = TK_while - TK_OFS
};

// Static assertions to verify enum and TOKEN_DEFINITIONS stay in sync.
// Token values start at TK_OFS + 1 (e.g., TK_and = 257).
static_assert(TK_eof - TK_OFS == TOKEN_DEFINITIONS.size(),
   "TOKEN_DEFINITIONS array size must match enum token count");
static_assert(TK_RESERVED == generate_reserved_count(),
   "Reserved word count mismatch between enum and TOKEN_DEFINITIONS");

typedef int LexChar;   //  Lexical character. Unsigned ext. from char.
typedef int LexToken;   //  Lexical token.

// Combined bytecode ins/line. Only used during bytecode generation.

typedef struct BCInsLine {
   BCIns ins;        //  Bytecode instruction.
   BCLine line;      //  Line number for this bytecode.
} BCInsLine;

// Info for local variables. Only used during bytecode generation.

enum class VarInfoFlag : uint8_t;

typedef struct VarInfo {
   GCRef name;        //  Local variable name.
   BCPOS startpc;     //  First point where the local variable is active.
   BCPOS endpc;       //  First point where the local variable is dead.
   uint8_t slot;      //  Variable slot.
   VarInfoFlag info;  //  Variable info flags.
} VarInfo;

// Forward declarations for parser scope helpers.

struct FuncScope;
struct ExpDesc;
enum class BinOpr : int8_t;

void lj_reserve_words(lua_State *);

class ParserContext;
template<typename T>
class ParserResult;
enum class TokenKind : uint16_t;

struct LocalDeclResult {
   BCREG declared = 0;
   BCREG initialised = 0;
};

// Lua lexer state.

class LexState {
public:
   struct BufferedToken {
      LexToken token = 0;
      TValue value;
      BCLine line = 0;
      BCLine column = 0;
      size_t offset = 0;
   };

   struct FuncState *fs;    // Current FuncState. Defined in lj_parse.c.
   lua_State *L;            // Lua state.
   TValue     tokval;       // Current token value.
   TValue     lookaheadval; // Lookahead token value.
   const char *p;           // Current position in input buffer.
   const char *pe;          // End of input buffer.
   LexChar    c;            // Current character.
   LexToken   tok;          // Current token.
   LexToken   lookahead;    // Lookahead token.
   SBuf       sb;           // String buffer for tokens.
   lua_Reader rfunc;        // Reader callback.
   void *     rdata;        // Reader callback data.
   BCLine     linenumber;   // Input line counter.
   BCLine     lastline;     // Line of last token.
   GCstr *    chunkname;    // Current chunk name (interned string).
   const char *chunkarg;    // Chunk name argument.
   const char *mode;        // Allow loading bytecode (b) and/or source text (t).
   GCstr *    empty_string_constant; // Cached empty string reference.
   VarInfo *  vstack;        // Stack for names and extents of local variables.
   MSize      sizevstack;    // Size of variable stack.
   MSize      vtop;          // Top of variable stack.
   BCInsLine* bcstack;       // Stack for bytecode instructions/line numbers.
   MSize      sizebcstack;   // Size of bytecode stack.
   uint32_t   level;         // Syntactical nesting level.
   uint32_t   ternary_depth; // Number of pending ternary operators.
   uint8_t    pending_if_empty_colon; // Tracks ?: misuse after ??.
   int        endmark;       // Trust bytecode end marker, even if not at EOF.
   int        is_bytecode;   // Set to 1 if input is bytecode, 0 if source text.

   size_t   current_offset = 0;
   size_t   next_offset = 0;
   size_t   line_start_offset = 0;

   BCLine   current_token_line = 1;
   BCLine   current_token_column = 1;
   size_t   current_token_offset = 0;

   BCLine   lookahead_line = 1;
   BCLine   lookahead_column = 1;
   size_t   lookahead_offset = 0;

   BCLine   pending_token_line = 1;
   BCLine   pending_token_column = 1;
   size_t   pending_token_offset = 0;

   ParserContext* active_context = nullptr;
   std::deque<BufferedToken> buffered_tokens;

   LexState() = default;  // Default constructor for bytecode reader usage
   LexState(lua_State* L, lua_Reader Rfunc, void* Rdata, std::string_view Chunkarg, std::optional<std::string_view> Mode);
   LexState(lua_State* L, const char* BytecodePtr, GCstr* ChunkName);  // Constructor for direct bytecode reading
   ~LexState();

   void next();
   LexToken lookahead_token();
   const char* token2str(LexToken Tok);
   LJ_NORET LJ_NOINLINE void err_syntax(ErrMsg Message);
   LJ_NORET LJ_NOINLINE void err_token(LexToken Token);
   int lex_opt(LexToken Token);
   void lex_check(LexToken Token);
   void lex_match(LexToken What, LexToken Who, BCLine Line);
   GCstr* lex_str();

   // Variable management
   void var_new(BCREG Reg, GCstr* Name);
   void var_new_lit(BCREG Reg, std::string_view Value);
   void var_new_fixed(BCREG Reg, uintptr_t Name);
   void var_add(BCREG VariableCount);
   void var_remove(BCREG TargetLevel);
   MSize var_lookup(ExpDesc *);
   MSize var_lookup_symbol(GCstr* Name, ExpDesc *);

   // Break and continue management
   MSize gola_new(int JumpType, VarInfoFlag Info, BCPOS Position);
   void gola_patch(VarInfo* GotoInfo, VarInfo* LabelInfo);
   void gola_close(VarInfo* GotoInfo);
   void gola_resolve(FuncScope* Scope, MSize Index);
   void gola_fixup(FuncScope* Scope);

   // Function state lifecycle
   size_t fs_prep_var(FuncState* FunctionState, size_t* OffsetVar);
   void fs_fixup_var(GCproto* Prototype, uint8_t* Buffer, size_t OffsetVar);
   GCproto* fs_finish(BCLine Line);
   void fs_init(FuncState* FunctionState);

   [[maybe_unused]] void assign_adjust(BCREG VariableCount, BCREG, ExpDesc *);
   [[nodiscard]] bool should_emit_presence();

   // Public parser helpers
   GCstr * keepstr(std::string_view Value);
   [[nodiscard]] GCstr* intern_empty_string();
   void ensure_lookahead(size_t count);
   [[nodiscard]] size_t available_lookahead() const;
   [[nodiscard]] const BufferedToken* buffered_token(size_t index) const;
   [[nodiscard]] SourceSpan current_token_span() const;
   [[nodiscard]] SourceSpan lookahead_token_span() const;
   void mark_token_start();
   void apply_buffered_token(const BufferedToken& token);
   BufferedToken scan_buffered_token();

#if LJ_HASFFI
   void keepcdata(TValue* Value, GCcdata* Cdata);
#endif

#ifdef LUA_USE_ASSERT
   template<typename... Args>
   void assert_condition(bool Condition, const char* Format, Args... Arguments) {
      lj_assertG_(G(this->L), Condition, Format, Arguments...);
   }
#else
   template<typename... Args>
   void assert_condition(bool Condition, const char* Format, Args... Arguments) {
      (void)this; (void)Condition; (void)Format;
   }
#endif
};

LJ_FUNC_NORET void lj_lex_error(LexState *, LexToken, ErrMsg, ...);
