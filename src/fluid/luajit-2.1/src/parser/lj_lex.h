// Lexical analyzer.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h

#pragma once

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

// Lua lexer tokens.
#define TKDEF(_, __) \
  _(and) _(break) _(continue) _(defer) _(do) _(else) _(elseif) _(end) _(false) \
  _(for) _(function) _(if) _(in) _(is) _(local) _(nil) _(not) _(or) \
  _(repeat) _(return) _(then) _(true) _(until) _(while) \
  __(if_empty, ?\?) \
  __(concat, ..) __(dots, ...) __(eq, ==) __(ge, >=) __(le, <=) __(ne, ~=) \
  __(shl, <<) __(shr, >>) __(ternary_sep, :>) \
  __(number, <number>) __(name, <name>) __(string, <string>) \
  __(cadd, +=) __(csub, -=) __(cmul, *=) __(cdiv, /=) __(cconcat, ..=) __(cmod, %=) \
  __(cif_empty, ?=) \
  __(plusplus, ++) \
  __(eof, <eof>)

enum {
   TK_OFS = 256,
#define TKENUM1(name)      TK_##name,
#define TKENUM2(name, sym)   TK_##name,
   TKDEF(TKENUM1, TKENUM2)
#undef TKENUM1
#undef TKENUM2
   TK_RESERVED = TK_while - TK_OFS
};

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
   BCPos startpc;     //  First point where the local variable is active.
   BCPos endpc;       //  First point where the local variable is dead.
   uint8_t slot;      //  Variable slot.
   VarInfoFlag info;  //  Variable info flags.
} VarInfo;

// Forward declarations for parser scope helpers.

struct FuncScope;
struct ExpDesc;
enum BinOpr : int;

void lj_reserve_words(lua_State *);

class ParserContext;
template<typename T>
class ParserResult;
enum class TokenKind : uint16_t;

struct LocalDeclResult {
   BCReg declared = 0;
   BCReg initialised = 0;
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
   BCLine     lastline;    // Line of last token.
   GCstr *    chunkname;  // Current chunk name (interned string).
   const char *chunkarg; // Chunk name argument.
   const char *mode;     // Allow loading bytecode (b) and/or source text (t).
   GCstr *    empty_string_constant; // Cached empty string reference.
   VarInfo * vstack;     // Stack for names and extents of local variables.
   MSize     sizevstack;  // Size of variable stack.
   MSize     vtop;        // Top of variable stack.
   BCInsLine* bcstack;   // Stack for bytecode instructions/line numbers.
   MSize    sizebcstack; // Size of bytecode stack.
   uint32_t level;       // Syntactical nesting level.
   uint32_t ternary_depth; // Number of pending ternary operators.
   uint8_t  pending_if_empty_colon; // Tracks ?: misuse after ??.
   int      endmark;          // Trust bytecode end marker, even if not at EOF.
   int      is_bytecode;      // Set to 1 if input is bytecode, 0 if source text.

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
   void var_new(BCReg Reg, GCstr* Name);
   void var_new_lit(BCReg Reg, std::string_view Value);
   void var_new_fixed(BCReg Reg, uintptr_t Name);
   void var_add(BCReg VariableCount);
   void var_remove(BCReg TargetLevel);
   MSize var_lookup(ExpDesc* Expression);
   MSize var_lookup_symbol(GCstr* Name, ExpDesc* Expression);

   // Break and continue management
   MSize gola_new(int JumpType, VarInfoFlag Info, BCPos Position);
   void gola_patch(VarInfo* GotoInfo, VarInfo* LabelInfo);
   void gola_close(VarInfo* GotoInfo);
   void gola_resolve(FuncScope* Scope, MSize Index);
   void gola_fixup(FuncScope* Scope);

   // Function state lifecycle
   size_t fs_prep_var(FuncState* FunctionState, size_t* OffsetVar);
   void fs_fixup_var(GCproto* Prototype, uint8_t* Buffer, size_t OffsetVar);
   GCproto* fs_finish(BCLine Line);
   void fs_init(FuncState* FunctionState);

   // Expression parsing
   ParserResult<ExpDesc> expr(ExpDesc* Expression);
   void expr_str(ExpDesc* Expression);
   void expr_field(ExpDesc* Expression);
   void expr_bracket(ExpDesc* Expression);
   void expr_table(ExpDesc* Expression);
   BCReg parse_params(int NeedSelf);
   void parse_body_impl(ExpDesc* Expression, int NeedSelf, BCLine Line, int OptionalParams);
   void parse_body(ExpDesc* Expression, int NeedSelf, BCLine Line);
   void parse_body_defer(ExpDesc* Expression, BCLine Line);
   ParserResult<BCReg> expr_list(ExpDesc* Expression);
   void parse_args(ExpDesc* Expression);
   void inc_dec_op(BinOpr Operator, ExpDesc* Expression, int IsPost);
   ParserResult<ExpDesc> expr_primary(ExpDesc* Expression);
   ParserResult<ExpDesc> expr_simple(ExpDesc* Expression);
   void synlevel_begin();
   void synlevel_end();
   ParserResult<BinOpr> expr_binop(ExpDesc* Expression, uint32_t Limit);
   ParserResult<BinOpr> expr_shift_chain(ExpDesc* LeftHandSide, BinOpr Operator);
   ParserResult<ExpDesc> expr_unop(ExpDesc* Expression);
   ParserResult<ExpDesc> expr_next();
   ParserResult<BCPos> expr_cond();
   bool should_emit_presence();

   // Statement parsing
   void assign_hazard(std::span<ExpDesc> Left, const ExpDesc& Var);
   void assign_adjust(BCReg VariableCount, BCReg ExpressionCount, ExpDesc* Expression);
   int assign_if_empty(ParserContext& Context, ExpDesc* Variables);
   int assign_compound(ParserContext& Context, ExpDesc* Variables, TokenKind OperatorType);
   void parse_assignment(ParserContext& Context, ExpDesc* FirstVariable);
   void parse_call_assign(ParserContext& Context);
   ParserResult<LocalDeclResult> parse_local(ParserContext& Context);
   void parse_defer();
   void parse_func(BCLine Line);
   void parse_return(ParserContext& Context);
   void parse_continue();
   void parse_break();
   void parse_block(ParserContext& Context);
   void parse_while(ParserContext& Context, BCLine Line);
   void parse_repeat(ParserContext& Context, BCLine Line);
   void parse_for_num(ParserContext& Context, GCstr* VariableName, BCLine Line);
   void parse_for_iter(ParserContext& Context, GCstr* IndexName);
   void parse_for(ParserContext& Context, BCLine Line);
   BCPos parse_then(ParserContext& Context);
   void parse_if(ParserContext& Context, BCLine Line);
   bool parse_stmt(ParserContext& Context);
   void parse_chunk(ParserContext& Context);

   // Public parser helpers
   GCstr* keepstr(std::string_view Value);
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
