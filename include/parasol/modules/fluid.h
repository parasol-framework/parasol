#pragma once

// Name:      fluid.h
// Copyright: Paul Manias © 2006-2025
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_FLUID (1)

// JIT behaviour options

enum class JOF : uint32_t {
   NIL = 0,
   DIAGNOSE = 0x00000001,
   LEGACY = 0x00000002,
   DUMP_BYTECODE = 0x00000004,
   PROFILE = 0x00000008,
   TRACE_TOKENS = 0x00000010,
   TRACE_EXPECT = 0x00000020,
   TRACE_BOUNDARY = 0x00000040,
   TRACE_OPERATORS = 0x00000080,
   TRACE_REGISTERS = 0x00000100,
   TRACE_CFG = 0x00000200,
   TRACE_ASSIGNMENTS = 0x00000400,
   TRACE_VALUE_CATEGORY = 0x00000800,
   TRACE = 0x00000ff0,
};

DEFINE_ENUM_FLAG_OPERATORS(JOF)

#ifdef PARASOL_STATIC
#define JUMPTABLE_FLUID [[maybe_unused]] static struct FluidBase *FluidBase = nullptr;
#else
#define JUMPTABLE_FLUID struct FluidBase *FluidBase = nullptr;
#endif

struct FluidBase {
#ifndef PARASOL_STATIC
   ERR (*_SetVariable)(objScript *Script, CSTRING Name, int Type, ...);
#endif // PARASOL_STATIC
};

#if !defined(PARASOL_STATIC) and !defined(PRV_FLUID_MODULE)
extern struct FluidBase *FluidBase;
namespace fl {
template<class... Args> ERR SetVariable(objScript *Script, CSTRING Name, int Type, Args... Tags) { return FluidBase->_SetVariable(Script,Name,Type,Tags...); }
} // namespace
#else
namespace fl {
extern ERR SetVariable(objScript *Script, CSTRING Name, int Type, ...);
} // namespace
#endif // PARASOL_STATIC

