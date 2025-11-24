// Load and dump code.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h

#include <parasol/main.h>
#include <errno.h>
#include <stdio.h>

#define lj_load_c
#define LUA_CORE

#include "lua.h"
#include "lauxlib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_buf.h"
#include "lj_func.h"
#include "lj_frame.h"
#include "lj_vm.h"
#include "../parser/lexer.h"
#include "lj_bcdump.h"
#include "../parser/parser.h"

//********************************************************************************************************************
// Load Lua source code and bytecode

static TValue* cpparser(lua_State *L, lua_CFunction dummy, APTR ud)
{
   LexState* ls = (LexState*)ud;
   GCproto* pt;
   GCfunc* fn;
   int bc;
   UNUSED(dummy);
   cframe_errfunc(L->cframe) = -1;  //  Inherit error function.
   bc = ls->is_bytecode;
   if (ls->mode and !strchr(ls->mode, bc ? 'b' : 't')) {
      setstrV(L, L->top++, lj_err_str(L, ErrMsg::XMODE));
      lj_err_throw(L, LUA_ERRSYNTAX);
   }
   pt = bc ? lj_bcread(ls) : lj_parse(ls);
   fn = lj_func_newL_empty(L, pt, tabref(L->env));
   // Don't combine above/below into one statement.
   setfuncV(L, L->top++, fn);
   return nullptr;
}

//********************************************************************************************************************

extern int lua_loadx(lua_State* L, lua_Reader reader, APTR data, CSTRING chunkname, CSTRING mode)
{
   LexState ls(L, reader, data, chunkname ? chunkname : "?",
      mode ? std::optional<std::string_view>(mode) : std::nullopt);
   int status;
   status = lj_vm_cpcall(L, nullptr, &ls, cpparser);
   // Destructor will be called automatically when ls goes out of scope
   lj_gc_check(L);
   return status;
}

extern int lua_load(lua_State* L, lua_Reader reader, APTR data, CSTRING chunkname)
{
   return lua_loadx(L, reader, data, chunkname, nullptr);
}

typedef struct FileReaderCtx {
   FILE* fp;
   char buf[LUAL_BUFFERSIZE];
} FileReaderCtx;

static CSTRING reader_file(lua_State* L, APTR ud, size_t* size)
{
   FileReaderCtx* ctx = (FileReaderCtx*)ud;
   UNUSED(L);
   if (feof(ctx->fp)) return nullptr;
   *size = fread(ctx->buf, 1, sizeof(ctx->buf), ctx->fp);
   return *size > 0 ? ctx->buf : nullptr;
}

//********************************************************************************************************************
// Load a file as a Lua chunk.

extern int luaL_loadfilex(lua_State* L, CSTRING filename, CSTRING mode)
{
   FileReaderCtx ctx;
   int status;
   CSTRING chunkname;
   if (filename) {
      ctx.fp = fopen(filename, "rb");
      if (ctx.fp == nullptr) {
         lua_pushfstring(L, "cannot open %s: %s", filename, strerror(errno));
         return LUA_ERRFILE;
      }
      chunkname = lua_pushfstring(L, "@%s", filename);
   }
   else {
      ctx.fp = stdin;
      chunkname = "=stdin";
   }
   status = lua_loadx(L, reader_file, &ctx, chunkname, mode);
   if (ferror(ctx.fp)) {
      L->top -= filename ? 2 : 1;
      lua_pushfstring(L, "cannot read %s: %s", chunkname + 1, strerror(errno));
      if (filename)
         fclose(ctx.fp);
      return LUA_ERRFILE;
   }
   if (filename) {
      L->top--;
      copyTV(L, L->top - 1, L->top);
      fclose(ctx.fp);
   }
   return status;
}

//********************************************************************************************************************

extern int luaL_loadfile(lua_State* L, CSTRING filename)
{
   return luaL_loadfilex(L, filename, nullptr);
}

//********************************************************************************************************************

typedef struct StringReaderCtx {
   CSTRING str;
   size_t size;
} StringReaderCtx;

static CSTRING reader_string(lua_State* L, APTR ud, size_t* size)
{
   StringReaderCtx* ctx = (StringReaderCtx*)ud;
   UNUSED(L);
   if (ctx->size == 0) return nullptr;
   *size = ctx->size;
   ctx->size = 0;
   return ctx->str;
}

extern int luaL_loadbufferx(lua_State* L, CSTRING buf, size_t size, CSTRING name, CSTRING mode)
{
   StringReaderCtx ctx;
   ctx.str = buf;
   ctx.size = size;
   return lua_loadx(L, reader_string, &ctx, name, mode);
}

extern int luaL_loadbuffer(lua_State* L, CSTRING buf, size_t size, CSTRING name)
{
   return luaL_loadbufferx(L, buf, size, name, nullptr);
}

//********************************************************************************************************************
// Dump bytecode

extern int lua_dump(lua_State* L, lua_Writer writer, APTR data)
{
   cTValue* o = L->top - 1;
   lj_checkapi(L->top > L->base, "top slot empty");
   if (tvisfunc(o) and isluafunc(funcV(o))) return lj_bcwrite(L, funcproto(funcV(o)), writer, data, 0);
   else return 1;
}

