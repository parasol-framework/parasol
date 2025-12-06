// Load and dump code.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h

#include <parasol/main.h>
#include <errno.h>
#include <stdio.h>
#include <string>

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

static TValue * cpparser(lua_State *L, lua_CFunction dummy, APTR ud)
{
   auto ls = (LexState*)ud;
   GCproto* pt;
   GCfunc* fn;
   int bc;

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

extern int lua_load(lua_State *Lua, class objFile *File, CSTRING SourceName)
{
   std::string buffer;
   auto filesize = File->get<int>(FID_Size);
   buffer.resize(filesize);
   File->read(buffer.data(), filesize);

   LexState ls(Lua, buffer, SourceName, std::nullopt);
   auto status = lj_vm_cpcall(Lua, nullptr, &ls, cpparser);
   lj_gc_check(Lua);
   return status;
}

extern int lua_load(lua_State *Lua, std::string_view Source, CSTRING SourceName)
{
   LexState ls(Lua, Source, SourceName, std::nullopt);
   auto status = lj_vm_cpcall(Lua, nullptr, &ls, cpparser);
   lj_gc_check(Lua);
   return status;
}

//********************************************************************************************************************
// Load from buffer (deprecated)

extern int luaL_loadbuffer(lua_State *Lua, CSTRING Buffer, size_t Size, CSTRING SourceName)
{
   LexState ls(Lua, std::string_view(Buffer, Size), SourceName, std::nullopt);
   int status = lj_vm_cpcall(Lua, nullptr, &ls, cpparser);
   lj_gc_check(Lua);
   return status;
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

