// Lexical analyser.
// Copyright (C) 2025 Paul Manias

#pragma once

#include <parasol/main.h>
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
#include "../debug/filesource.h"

#ifdef INCLUDE_TIPS
#include <memory>
class TipEmitter;  // Forward declaration
#endif

struct SourceSpan {
   BCLine line = 0;
   BCLine column = 0;
   size_t offset = 0;
};

//********************************************************************************************************************

struct TokenDefinition {
   std::string_view name;    // Token identifier (e.g., "and", "if_empty")
   std::string_view symbol;  // Display symbol (e.g., "and", "??")
   bool reserved;            // True for reserved words that cannot be used as identifiers

   [[nodiscard]] constexpr bool is_reserved() const noexcept { return reserved; }
};

// Define all tokens once using X-macro pattern
// Format: TOKEN_DEF(name, symbol, reserved)
#define TOKEN_DEF_LIST \
   TOKEN_DEF(and,          "and",      true) \
   TOKEN_DEF(as,           "as",       true) \
   TOKEN_DEF(break,        "break",    true) \
   TOKEN_DEF(choose,       "choose",   true) \
   TOKEN_DEF(continue,     "continue", true) \
   TOKEN_DEF(defer,        "defer",    true) \
   TOKEN_DEF(do,           "do",       true) \
   TOKEN_DEF(else,         "else",     true) \
   TOKEN_DEF(elseif,       "elseif",   true) \
   TOKEN_DEF(end,          "end",      true) \
   TOKEN_DEF(false,        "false",    true) \
   TOKEN_DEF(for,          "for",      true) \
   TOKEN_DEF(from,         "from",     true) \
   TOKEN_DEF(function,     "function", true) \
   TOKEN_DEF(global,       "global",   true) \
   TOKEN_DEF(if,           "if",       true) \
   TOKEN_DEF(import,       "import",   true) \
   TOKEN_DEF(in,           "in",       true) \
   TOKEN_DEF(is,           "is",       true) \
   TOKEN_DEF(local,        "local",    true) \
   TOKEN_DEF(namespace,    "namespace", true) \
   TOKEN_DEF(nil,          "nil",      true) \
   TOKEN_DEF(not,          "not",      true) \
   TOKEN_DEF(or,           "or",       true) \
   TOKEN_DEF(repeat,       "repeat",   true) \
   TOKEN_DEF(return,       "return",   true) \
   TOKEN_DEF(then,         "then",     true) \
   TOKEN_DEF(thunk,        "thunk",    true) \
   TOKEN_DEF(true,         "true",     true) \
   TOKEN_DEF(try,          "try",      true) \
   TOKEN_DEF(except,       "except",   true) \
   TOKEN_DEF(until,        "until",    true) \
   TOKEN_DEF(when,         "when",     true) \
   TOKEN_DEF(success,      "success",  true) \
   TOKEN_DEF(raise,        "raise",    true) \
   TOKEN_DEF(check,        "check",    true) \
   TOKEN_DEF(while,        "while",    true) \
   TOKEN_DEF(case_arrow,   "->",       false) \
   TOKEN_DEF(if_empty,     "??",       false) \
   TOKEN_DEF(safe_field,   "?.",       false) \
   TOKEN_DEF(safe_index,   "?[",       false) \
   TOKEN_DEF(safe_method,  "?:",       false) \
   TOKEN_DEF(arrow,        "=>",       false) \
   TOKEN_DEF(concat,       "..",       false) \
   TOKEN_DEF(dots,         "...",      false) \
   TOKEN_DEF(eq,           "==",       false) \
   TOKEN_DEF(ge,           ">=",       false) \
   TOKEN_DEF(le,           "<=",       false) \
   TOKEN_DEF(ne,           "~=",       false) \
   TOKEN_DEF(shl,          "<<",       false) \
   TOKEN_DEF(shr,          ">>",       false) \
   TOKEN_DEF(ternary_sep,  ":>",       false) \
   TOKEN_DEF(number,       "<number>", false) \
   TOKEN_DEF(name,         "<name>",   false) \
   TOKEN_DEF(string,       "<string>", false) \
   TOKEN_DEF(cadd,         "+=",       false) \
   TOKEN_DEF(csub,         "-=",       false) \
   TOKEN_DEF(cmul,         "*=",       false) \
   TOKEN_DEF(cdiv,         "/=",       false) \
   TOKEN_DEF(cconcat,      "..=",      false) \
   TOKEN_DEF(cmod,         "%=",       false) \
   TOKEN_DEF(cif_empty,    "?\?=",     false) \
   TOKEN_DEF(cif_nil,      "?=",       false) \
   TOKEN_DEF(plusplus,     "++",       false) \
   TOKEN_DEF(pipe,         "|>",       false) \
   TOKEN_DEF(defer_open,   "<{",       false) \
   TOKEN_DEF(defer_typed,  "<type{",   false) \
   TOKEN_DEF(defer_close,  "}>",       false) \
   TOKEN_DEF(array_typed,  "array<type>", false) \
   TOKEN_DEF(annotate,     "@",        false) \
   TOKEN_DEF(compif,       "@if",      false) \
   TOKEN_DEF(compend,      "@end",     false) \
   TOKEN_DEF(eof,          "<eof>",    false)

// Generate TOKEN_DEFINITIONS array from TOKEN_DEF_LIST
// This array provides compile-time token metadata
#define TOKEN_DEF(name, symbol, reserved) TokenDefinition{#name, symbol, reserved},
inline constexpr std::array TOKEN_DEFINITIONS = {
   TOKEN_DEF_LIST
};
#undef TOKEN_DEF

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

// Generate enum values from TOKEN_DEF_LIST
// SINGLE SOURCE OF TRUTH: All token definitions come from TOKEN_DEF_LIST above

#define TOKEN_DEF(name, symbol, reserved) TK_##name,
enum {
   TK_OFS = 256,
   TOKEN_DEF_LIST
   TK_RESERVED = TK_while - TK_OFS
};
#undef TOKEN_DEF

// Static assertions to verify enum and TOKEN_DEFINITIONS stay in sync.
// Token values start at TK_OFS + 1 (e.g., TK_and = 257).

static_assert(TK_eof - TK_OFS == TOKEN_DEFINITIONS.size(), "TOKEN_DEFINITIONS array size must match enum token count");
static_assert(TK_RESERVED == generate_reserved_count(), "Reserved word count mismatch between enum and TOKEN_DEFINITIONS");

typedef int LexChar;    //  Lexical character. Unsigned ext. from char.
typedef int LexToken;   //  Lexical token.

// Combined bytecode ins/line. Only used during bytecode generation.

typedef struct BCInsLine {
   BCIns ins;        //  Bytecode instruction.
   BCLine line;      //  Line number for this bytecode.
} BCInsLine;

// Info for local variables. Only used during bytecode generation.

enum class VarInfoFlag : uint8_t;
enum class FluidType : uint8_t;  // Forward declaration (defined in ast_nodes.h)

typedef struct VarInfo {
   GCRef name;        //  Local variable name.
   std::array<FluidType, MAX_RETURN_TYPES> result_types{};  // Return types if this variable holds a function
   BCPOS startpc;     //  First point where the local variable is active.
   BCPOS endpc;       //  First point where the local variable is dead.
   uint8_t slot;      //  Variable slot.
   VarInfoFlag info;  //  Variable info flags.
   FluidType fixed_type;  // Type once established (Unknown = not yet fixed)
   CLASSID object_class_id = CLASSID::NIL;  // CLASSID for Object types (0 = unknown class)
   BCLine line = 0;    // Line number where the variable was declared (for diagnostics)
   BCLine column = 0;  // Column number where the variable was declared (for diagnostics)
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
   std::string_view source; // Complete source text (immutable).
   size_t     pos = 0;      // Current position in source.
   LexChar    c;            // Current character (cached).
   LexToken   tok;          // Current token.
   LexToken   lookahead;    // Lookahead token.
   SBuf       sb;           // String buffer for tokens.

   // Bytecode reader compatibility fields (used only by lj_bcread.cpp)
   const char *p = nullptr;  // Current position in bytecode buffer.
   const char *pe = nullptr; // End of bytecode buffer.
   lua_Reader rfunc = nullptr; // Reader callback for bytecode streaming.
   void *rdata = nullptr;    // Reader callback data.
   int endmark = 0;          // Trust bytecode end marker.

   BCLine     linenumber;   // Input line counter.
   BCLine     lastline;     // Line of last token.
   uint8_t    current_file_index = 0;  // File index for FileSource tracking (0 = main file)

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
   int        is_bytecode;   // Set to 1 if input is bytecode, 0 if source text.
   int64_t    array_typed_size = -1;  // Size parameter for array<type, size> (-1 = no size specified)

   size_t   current_offset = 0;
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
   bool diagnose_mode = false;  // When true, lexer errors are collected instead of thrown
   bool had_lex_error = false;  // Set when a recoverable lexer error occurred

#ifdef INCLUDE_TIPS
   // Tip system: 0 = off, 1 = best (critical), 2 = most (medium), 3 = all
   uint8_t tip_level = 0;
   std::unique_ptr<TipEmitter> tip_emitter;
#endif

   LexState() = default;  // Default constructor for bytecode reader usage
   LexState(lua_State* L, std::string_view Source, std::string_view Chunkarg, std::optional<std::string_view> Mode = std::nullopt);
   LexState(lua_State* L, lua_Reader Rfunc, void* Rdata, std::string_view Chunkarg, std::optional<std::string_view> Mode);  // Bytecode streaming constructor
   LexState(lua_State* L, const char* BytecodePtr, GCstr* ChunkName);  // Direct bytecode reading (for embedded bytecode)
   ~LexState();

   // Character stream operations
   [[nodiscard]] LexChar peek(size_t Offset = 0) const noexcept;
   [[nodiscard]] LexChar peek_next() const noexcept;

   void next();
   LexToken lookahead_token();
   const char* token2str(LexToken Tok);
   LJ_NOINLINE void err_syntax(ErrMsg Message);
   LJ_NOINLINE void err_token(LexToken Token);
   int lex_opt(LexToken Token);
   void lex_check(LexToken Token);
   void lex_match(LexToken What, LexToken Who, BCLine Line);
   GCstr* lex_str();

   // Variable management
   void var_new(BCREG Reg, GCstr* Name, BCLine Line = 0, BCLine Column = 0);
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

   // Returns an encoded BCLine with file index in upper 8 bits and line number in lower 24 bits.
   // This allows error reporting to identify both the source file and line number.
   // Uses lastline (the line of the most recently consumed token) for bytecode emission.

   [[nodiscard]] inline BCLine effective_line() const noexcept {
      BCLine line = this->lastline;
      if (line < 1) line = 1;
      return BCLine::encode(this->current_file_index, line);
   }

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

void lj_lex_error(LexState *, LexToken, ErrMsg, ...);

//********************************************************************************************************************
// RAII guard for import LexState instances.
//
// Windows SEH (used by lj_err_throw) doesn't call C++ destructors during stack unwinding.  This guard handles the
// normal path (destructor called on scope exit), while lua_load cleans up any orphaned lexers left in
// pending_import_lexers after catching SEH exceptions.
//
// Usage:
//    ImportLexerGuard guard(L, source, chunkname);
//    guard->next();  // Access via operator->
//    // ... parsing code ...
//    // Destructor automatically cleans up on normal exit

class ImportLexerGuard {
public:
   ImportLexerGuard(lua_State *L, std::string_view Source, std::string ChunkName)
      : lua(L)
      , lexer(new LexState(L, Source, std::move(ChunkName)))
   {
      lua->pending_import_lexers.push_back(lexer);
   }

   ~ImportLexerGuard() {
      if (lexer) {
         // Remove from tracking vector (find and erase to handle nested imports correctly)
         auto &vec = lua->pending_import_lexers;
         auto it = std::find(vec.begin(), vec.end(), lexer);
         if (it != vec.end()) vec.erase(it);
         delete lexer;
      }
   }

   // Non-copyable, non-movable (prevents accidental double-delete)
   ImportLexerGuard(const ImportLexerGuard&) = delete;
   ImportLexerGuard& operator=(const ImportLexerGuard&) = delete;
   ImportLexerGuard(ImportLexerGuard&&) = delete;
   ImportLexerGuard& operator=(ImportLexerGuard&&) = delete;

   [[nodiscard]] LexState* get() const noexcept { return lexer; }
   [[nodiscard]] LexState* operator->() const noexcept { return lexer; }
   [[nodiscard]] LexState& operator*() const noexcept { return *lexer; }

private:
   lua_State *lua;
   LexState *lexer;
};
