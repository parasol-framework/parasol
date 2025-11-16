// Lexical analyzer.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h

#pragma once

#include <stdarg.h>

#include "lj_obj.h"
#include "lj_err.h"

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

typedef struct VarInfo {
   GCRef name;        //  Local variable name.
   BCPos startpc;     //  First point where the local variable is active.
   BCPos endpc;       //  First point where the local variable is dead.
   uint8_t slot;      //  Variable slot.
   uint8_t info;      //  Variable info.
} VarInfo;

// Lua lexer state.

class LexState {
public:
   struct FuncState* fs; // Current FuncState. Defined in lj_parse.c.
   struct lua_State* L;  // Lua state.
   TValue tokval;        // Current token value.
   TValue lookaheadval;  // Lookahead token value.
   const char* p;        // Current position in input buffer.
   const char* pe;       // End of input buffer.
   LexChar c;            // Current character.
   LexToken tok;         // Current token.
   LexToken lookahead;   // Lookahead token.
   SBuf sb;              // String buffer for tokens.
   lua_Reader rfunc;     // Reader callback.
   void* rdata;          // Reader callback data.
   BCLine linenumber;    // Input line counter.
   BCLine lastline;      // Line of last token.
   GCstr* chunkname;     // Current chunk name (interned string).
   const char* chunkarg; // Chunk name argument.
   const char* mode;     // Allow loading bytecode (b) and/or source text (t).
   VarInfo* vstack;      // Stack for names and extents of local variables.
   MSize sizevstack;     // Size of variable stack.
   MSize vtop;           // Top of variable stack.
   BCInsLine* bcstack;   // Stack for bytecode instructions/line numbers.
   MSize sizebcstack;    // Size of bytecode stack.
   uint32_t level;       // Syntactical nesting level.
   uint32_t ternary_depth; // Number of pending ternary operators.
   uint8_t pending_if_empty_colon; // Tracks ?: misuse after ??.
   int endmark;          // Trust bytecode end marker, even if not at EOF.
   int is_bytecode;      // Set to 1 if input is bytecode, 0 if source text.

   LexState() = default;  // Default constructor for bytecode reader usage
   LexState(lua_State* L, lua_Reader Rfunc, void* Rdata, const char* Chunkarg, const char* Mode);
   LexState(lua_State* L, const char* BytecodePtr, GCstr* ChunkName);  // Constructor for direct bytecode reading
   ~LexState();

   void next();
   LexToken lookahead_token();
   const char* token2str(LexToken Tok);
   [[noreturn]] void error(LexToken Tok, ErrMsg Em, ...);

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

// Deprecated standalone functions - kept for compatibility during transition

LJ_FUNC_NORET void lj_lex_error(LexState* ls, LexToken tok, ErrMsg em, ...);  // Deprecated: use ls->error()
LJ_FUNC void lj_lex_init(lua_State* L);
