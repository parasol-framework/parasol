// Lua parser (source code -> bytecode).
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h

#pragma once

#include "lj_obj.h"
#include "lexer.h"

#ifdef ENABLE_UNIT_TESTS
extern void parser_unit_tests(int &Passed, int &Total);
#endif

extern GCproto *lj_parse(LexState *ls);
extern GCstr *lj_parse_keepstr(LexState *ls, const char *str, size_t l);
#if LJ_HASFFI
LJ_FUNC void lj_parse_keepcdata(LexState *ls, TValue *tv, GCcdata *cd);
#endif
