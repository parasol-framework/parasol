// Error handling.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// LuaJIT can either use internal or external frame unwinding:
//
// - Internal frame unwinding (INT) is free-standing and doesn't require
//   any OS or library support.
//
// - External frame unwinding (EXT) uses the system-provided unwind handler.
//
// Pros and Cons:
//
// - EXT requires unwind tables for *all* functions on the C stack between
//   the pcall/catch and the error/throw. C modules used by Lua code can
//   throw errors, so these need to have unwind tables, too. Transitively
//   this applies to all system libraries used by C modules -- at least
//   when they have callbacks which may throw an error.
//
// - INT is faster when actually throwing errors, but this happens rarely.
//   Setting up error handlers is zero-cost in any case.
//
// - INT needs to save *all* callee-saved registers when entering the
//   interpreter. EXT only needs to save those actually used inside the
//   interpreter. JIT-compiled code may need to save some more.
//
// - EXT provides full interoperability with C++ exceptions. You can throw
//   Lua errors or C++ exceptions through a mix of Lua frames and C++ frames.
//   C++ destructors are called as needed. C++ exceptions caught by pcall
//   are converted to the string "C++ exception". Lua errors can be caught
//   with catch (...) in C++.
//
// - INT has only limited support for automatically catching C++ exceptions
//   on POSIX systems using DWARF2 stack unwinding. Other systems may use
//   the wrapper function feature. Lua errors thrown through C++ frames
//   cannot be caught by C++ code and C++ destructors are not run.
//
// - EXT can handle errors from internal helper functions that are called
//   from JIT-compiled code (except for Windows/x86 and 32 bit ARM).
//   INT has no choice but to call the panic handler, if this happens.
//   Note: this is mainly relevant for out-of-memory errors.
//
// EXT is the default on all systems where the toolchain produces unwind
// tables by default (*). This is hard-coded and/or detected in src/Makefile.
// You can thwart the detection with: TARGET_XCFLAGS=-DLUAJIT_UNWIND_INTERNAL
//
// INT is the default on all other systems.
//
// EXT can be manually enabled for toolchains that are able to produce
// conforming unwind tables:
//   "TARGET_XCFLAGS=-funwind-tables -DLUAJIT_UNWIND_EXTERNAL"
// As explained above, *all* C code used directly or indirectly by LuaJIT
// must be compiled with -funwind-tables (or -fexceptions). C++ code must
// *not* be compiled with -fno-exceptions.
//
// If you're unsure whether error handling inside the VM works correctly,
// try running this and check whether it prints "OK":
//
//   luajit -e "print(select(2, load('OK')):match('OK'))"
//
// (*) Originally, toolchains only generated unwind tables for C++ code. For
// interoperability reasons, this can be manually enabled for plain C code,
// too (with -funwind-tables). With the introduction of the x64 architecture,
// the corresponding POSIX and Windows ABIs mandated unwind tables for all
// code. Over the following years most desktop and server platforms have
// enabled unwind tables by default on all architectures. OTOH mobile and
// embedded platforms do not consistently mandate unwind tables.

#define lj_err_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_str.h"
#include "lj_func.h"
#include "lj_state.h"
#include "lj_frame.h"
#include "lj_ff.h"
#include "lj_trace.h"
#include "lj_vm.h"
#include "lj_strfmt.h"
#include "lj_meta.h"
#include "lj_tab.h"
#include "lj_gc.h"

// For prvFluid access in try-except handling
#include "../../defs.h"

// Forward declarations for internal try-except functions that use Parasol's ERR type.
// These are defined in fluid_functions.cpp.

extern "C" bool lj_try_find_handler(lua_State *, const TryFrame *, ERR, const BCIns **, BCREG *);
extern "C" void lj_try_build_exception_table(lua_State *, ERR, CSTRING, int, BCREG);

// Error message strings.
LJ_DATADEF CSTRING lj_err_allmsg =
#define ERRDEF(name, msg)  msg "\0"
#include "lj_errmsg.h"
;

//********************************************************************************************************************
// Call __close handlers for to-be-closed locals during error unwinding.
// Sets _G.__close_err so bytecode-based close handlers can access the error.
// Returns the error object to propagate (may be updated if a __close handler throws).
// Per Lua 5.4: if a __close handler throws, that error replaces the original,
// but all other pending __close handlers are still called.

static TValue* unwind_close_handlers(lua_State *L, TValue* frame, TValue* errobj)
{
   // Get the function from this frame
   GCfunc *fn = frame_func(frame);

   // Only process Lua functions (they have closeslots in their prototype)
   if (!isluafunc(fn)) return errobj;

   GCproto *pt = funcproto(fn);
   uint64_t closeslots = pt->closeslots;
   if (closeslots IS 0) return errobj;

   // Set _G.__close_err for bytecode-based handlers that might run later

   GCtab *env = tabref(L->env);
   if (env) {
      GCstr *key = lj_str_newlit(L, "__close_err");
      TValue *slot = lj_tab_setstr(L, env, key);
      if (errobj) copyTV(L, slot, errobj);
      else setnilV(slot);
      lj_gc_anybarriert(L, env);
   }

   // Also set L->close_err for direct access
   if (errobj) copyTV(L, &L->close_err, errobj);
   else setnilV(&L->close_err);

   // Call lj_meta_close for each slot with <close> attribute in LIFO order
   // Iterate from highest slot to lowest to match Lua 5.4 semantics
   TValue *base = frame + 1;
   TValue *current_err = errobj;
   for (int slot = 63; slot >= 0; slot--) {
      if (closeslots & (1ULL << slot)) {
         TValue *o = base + slot;
         // Verify slot is within valid frame range: must be >= base and < L->top
         if (o >= base and o < L->top and !tvisnil(o) and !tvisfalse(o)) {
            int errcode = lj_meta_close(L, o, current_err);
            if (errcode != 0) {
               // Per Lua 5.4: error in __close replaces the original error.
               // The new error is at L->top - 1 after the failed pcall.
               // Continue calling other __close handlers with the new error.
               current_err = L->top - 1;
               // Update _G.__close_err with the new error
               if (env) {
                  GCstr *key = lj_str_newlit(L, "__close_err");
                  TValue *slot = lj_tab_setstr(L, env, key);
                  copyTV(L, slot, current_err);
                  lj_gc_anybarriert(L, env);
               }
               copyTV(L, &L->close_err, current_err);
            }
         }
      }
   }
   return current_err;
}

//********************************************************************************************************************
// Call __close handlers for all frames from 'from' down to 'to'.
// This must be called BEFORE L->base is modified during unwinding.
// If a __close handler throws, the new error replaces the original at L->top - 1.

static void unwind_close_all(lua_State *L, TValue *from, TValue *to)
{
   TValue *errobj = (L->top > to) ? L->top - 1 : nullptr;
   TValue *frame = from;
   int count = 0;
   // Use LUAI_MAXCSTACK as the safety limit - this matches the maximum call depth
   // that LuaJIT enforces, so any valid frame chain should terminate well before this.
   // The limit guards against stack corruption causing infinite loops.
   while (frame >= to and count < LUAI_MAXCSTACK) {
      count++;
      // unwind_close_handlers may return a different error if a __close threw
      TValue* new_err = unwind_close_handlers(L, frame, errobj);
      if (new_err != errobj and new_err != nullptr and errobj != nullptr) {
         // A __close handler threw - update the error at the original location
         copyTV(L, errobj, new_err);
      }
      errobj = new_err;  // Use the (possibly updated) error for subsequent handlers
      // Move to previous frame based on type
      int ftype = frame_type(frame);
      if (ftype IS FRAME_LUA or ftype IS FRAME_LUAP) frame = frame_prevl(frame);
      else frame = frame_prevd(frame);
   }
   // If we hit the limit, the frame chain is likely corrupt. Log an assertion
   // in debug builds to help diagnose the issue.
   lj_assertL(count < LUAI_MAXCSTACK, "frame chain exceeded LUAI_MAXCSTACK during __close unwinding");

   // Clear __close_err after all handlers run

   if (GCtab *env = tabref(L->env)) {
      GCstr *key = lj_str_newlit(L, "__close_err");
      TValue *slot = lj_tab_setstr(L, env, key);
      setnilV(slot);
   }
   setnilV(&L->close_err);
}

//********************************************************************************************************************
// Unwind Lua stack and move error message to new top.

LJ_NOINLINE static void unwindstack(lua_State *L, TValue *top)
{
   lj_func_closeuv(L, top);
   if (top < L->top - 1) {
      copyTV(L, top, L->top - 1);
      L->top = top + 1;
   }
   lj_state_relimitstack(L);
}

//********************************************************************************************************************
// Sentinel value returned by err_unwind when a try-except handler is found.
// The caller should re-enter the VM at L->try_handler_pc.

#define ERR_TRYHANDLER ((void*)(intptr_t)-2)

//********************************************************************************************************************
// Check if a try frame should handle this error.
// Returns true if handler found, with L->try_handler_pc set.

// Check if a try handler exists for the current error.
// If found, returns true but does NOT modify L->base, L->top, or the try stack.
// The actual state modification is done by setup_try_handler().

static bool check_try_handler(lua_State *L, int errcode)
{
   // Note: JIT state check is done in err_unwind before calling this function

   if (L->try_stack.depth IS 0) return false;

   // Don't intercept errors from JIT-compiled code
   //if (tvref(G(L)->jit_base)) return false; // Disabled - PROTO_NOJIT flag provides coverage

   // Validate try stack depth is within bounds
   lj_assertL(L->try_stack.depth <= LJ_MAX_TRY_DEPTH,
      "check_try_handler: try_stack depth %u exceeds LJ_MAX_TRY_DEPTH", L->try_stack.depth);

   TryFrame *try_frame = &L->try_stack.frames[L->try_stack.depth - 1];

   lj_assertL(try_frame->func != nullptr, "check_try_handler: try_frame->func is null");

   // Check if there's a protected call frame (FRAME_CP, FRAME_PCALL, FRAME_PCALLH) between
   // the current error and the try block. If so, let the protected call handle the error first.
   // This ensures that lua_pcall() inside functions like exec() works correctly.
   //
   // We walk the Lua frame chain looking for protected frames that are "above" the try block
   // (i.e., started after the try block).

   {
      TValue *pf = L->base - 1;
      TValue *try_base = restorestack(L, try_frame->frame_base);

      while (pf > tvref(L->stack) + LJ_FR2) {
         int pf_type = frame_typep(pf);

         // Check if this is a protected frame (C protected or Lua pcall)
         if (pf_type IS FRAME_CP or pf_type IS FRAME_PCALL or pf_type IS FRAME_PCALLH) {
            // This protected frame is above the try block's base - it should handle the error first
            if (pf >= try_base) return false;
         }

         // If we've reached the try block's function, stop searching
         GCfunc *func = frame_func(pf);
         if (func IS try_frame->func) {
            break;  // Reached the try frame's function
         }

         // Move to previous frame based on frame type
         if (pf_type IS FRAME_LUA or pf_type IS FRAME_LUAP) pf = frame_prevl(pf);
         else pf = frame_prevd(pf);
      }
   }

   // Verify try frame is in current call chain by walking up the frame chain.
   // The error may have been raised from a C function (like error()) so we need
   // to check if the try block's function is anywhere in the call chain.

   TValue *frame = L->base - 1;
   bool found_try_func = false;

   // Validate initial frame pointer is within stack bounds
   lj_assertL(frame >= tvref(L->stack), "check_try_handler: initial frame below stack start");

   while (frame > tvref(L->stack) + LJ_FR2) {
      GCfunc *func = frame_func(frame);
      if (func IS try_frame->func) {
         found_try_func = true;
         break;
      }
      frame = frame_prev(frame);
   }

   if (not found_try_func) return false;

   // Extract error code from prvFluid if available
   ERR err_code = ERR::Exception;  // Default for Lua errors
   if (L->script) {
      auto prv = (prvFluid *)L->script->ChildPrivate;
      if (prv and prv->CaughtError >= ERR::ExceptionThreshold) err_code = prv->CaughtError;
   }

   const BCIns *handler_pc = nullptr;
   BCREG exception_reg = 0xFF;

   if (lj_try_find_handler(L, try_frame, err_code, &handler_pc, &exception_reg)) {
      // Validate handler PC was set
      lj_assertL(handler_pc != nullptr, "check_try_handler: handler found but handler_pc is null");

      // Just record that a handler exists - don't modify state yet
      L->try_handler_pc = handler_pc;
      return true;
   }

   return false;
}

//********************************************************************************************************************
// Called to actually set up the try handler state before resuming execution.
// This should be called right before jumping to the handler, NOT during search phase.

extern "C" void setup_try_handler(lua_State *L)
{
   if (L->try_stack.depth IS 0) return;

   lj_assertL(L->try_stack.depth <= LJ_MAX_TRY_DEPTH, "setup_try_handler: try_stack depth %u exceeds LJ_MAX_TRY_DEPTH", L->try_stack.depth);

   TryFrame *try_frame = &L->try_stack.frames[L->try_stack.depth - 1];

   lj_assertL(try_frame->func != nullptr, "setup_try_handler: try_frame->func is null");

   // Extract error code from prvFluid if available
   ERR err_code = ERR::Exception;
   if (L->script) {
      auto prv = (prvFluid *)L->script->ChildPrivate;
      if (prv and prv->CaughtError >= ERR::ExceptionThreshold) {
         err_code = prv->CaughtError;
      }
   }

   const BCIns *handler_pc = nullptr;
   BCREG exception_reg = 0xFF;

   if (not lj_try_find_handler(L, try_frame, err_code, &handler_pc, &exception_reg)) {
      // This should not happen if check_try_handler returned true - assert in debug builds
      lj_assertL(false, "setup_try_handler: no handler found but check_try_handler returned true");
      return;
   }

   // Validate handler PC
   lj_assertL(handler_pc != nullptr, "setup_try_handler: handler found but handler_pc is null");

   // Get error message before restoring stack
   CSTRING error_msg = nullptr;
   if (L->top > L->base and tvisstr(L->top - 1)) {
      error_msg = strVdata(L->top - 1);
   }

   // Extract line number from error message (format: "filename:line: message")
   int line = 0;
   if (error_msg) {
      CSTRING colon1 = strchr(error_msg, ':');
      if (colon1) {
         // Check if next character starts a number (line number)
         CSTRING num_start = colon1 + 1;
         if (*num_start >= '0' and *num_start <= '9') {
            line = int(strtol(num_start, nullptr, 10));
         }
      }
   }

   // Convert offsets back to pointers using restorestack()
   TValue *saved_base = restorestack(L, try_frame->frame_base);
   TValue *saved_top = restorestack(L, try_frame->saved_top);

   // Validate restored pointers are within stack bounds
   lj_assertL(saved_base >= tvref(L->stack), "setup_try_handler: saved_base below stack start");
   lj_assertL(saved_base <= tvref(L->maxstack), "setup_try_handler: saved_base above maxstack");
   lj_assertL(saved_top >= tvref(L->stack), "setup_try_handler: saved_top below stack start");
   lj_assertL(saved_top <= tvref(L->maxstack), "setup_try_handler: saved_top above maxstack");
   lj_assertL(saved_top >= saved_base, "setup_try_handler: saved_top below saved_base");

   lj_func_closeuv(L, saved_top); // Close upvalues and restore stack state
   L->base = saved_base;
   L->top = saved_top;

   L->try_stack.depth--; // Pop try frame

   // Build exception table and place in handler's register
   lj_try_build_exception_table(L, err_code, error_msg, line, exception_reg);

   // Reset CaughtError so it doesn't leak to subsequent exceptions

   auto prv = (prvFluid *)L->script->ChildPrivate;
   prv->CaughtError = ERR::Okay;

   L->try_handler_pc = handler_pc; // Stash handler PC for VM re-entry (already set, but confirm)
}

//********************************************************************************************************************
// Unwind until stop frame. Optionally cleanup frames.

extern void * err_unwind(lua_State *L, void *stopcf, int errcode)
{
   // Check for try-except handlers first.
   // On Windows, errcode is 0 during search phase and non-zero during unwind phase.
   // We need to check for try handlers even during search phase (errcode=0).
   // Use LUA_ERRRUN as default for search phase.

   int try_errcode = errcode ? errcode : LUA_ERRRUN;
   if (check_try_handler(L, try_errcode)) return ERR_TRYHANDLER;

   TValue *frame = L->base - 1;
   void *cf = L->cframe;
   while (cf) {
      int32_t nres = cframe_nres(cframe_raw(cf));
      if (nres < 0) {  // C frame without Lua frame?
         TValue *top = restorestack(L, -nres);
         if (frame < top) {  // Frame reached?
            if (errcode) {
               unwind_close_all(L, L->base - 1, top);
               L->base = frame + 1;
               L->cframe = cframe_prev(cf);
               unwindstack(L, top);
            }
            return cf;
         }
      }

      if (frame <= tvref(L->stack) + LJ_FR2) break;

      switch (frame_typep(frame)) {
         case FRAME_LUA:  //  Lua frame.
         case FRAME_LUAP:
            frame = frame_prevl(frame);
            break;
         case FRAME_C:  //  C frame.
         unwind_c:
   #if LJ_UNWIND_EXT
            if (errcode) {
               TValue* target = frame - LJ_FR2;
               unwind_close_all(L, L->base - 1, target);
               L->base = frame_prevd(frame) + 1;
               L->cframe = cframe_prev(cf);
               unwindstack(L, target);
            }
            else if (cf != stopcf) {
               cf = cframe_prev(cf);
               frame = frame_prevd(frame);
               break;
            }
            return nullptr;  //  Continue unwinding.
   #else
            cf = cframe_prev(cf);
            frame = frame_prevd(frame);
            break;
   #endif
         case FRAME_CP:  //  Protected C frame.
            if (cframe_canyield(cf)) {  // Resume?
               if (errcode) {
                  hook_leave(G(L));  //  Assumes nobody uses coroutines inside hooks.
                  L->cframe = nullptr;
                  L->status = (uint8_t)errcode;
               }
               return cf;
            }
            if (errcode) {
               L->base = frame_prevd(frame) + 1;
               L->cframe = cframe_prev(cf);
               unwindstack(L, frame - LJ_FR2);
            }
            return cf;
         case FRAME_CONT:  //  Continuation frame.
            if (frame_iscont_fficb(frame))
               goto unwind_c;
            // fallthrough
         case FRAME_VARG:  //  Vararg frame.
            frame = frame_prevd(frame);
            break;
         case FRAME_PCALL:  //  FF pcall() frame.
         case FRAME_PCALLH:  //  FF pcall() frame inside hook.
            if (errcode) {
               if (errcode == LUA_YIELD) {
                  frame = frame_prevd(frame);
                  break;
               }
               if (frame_typep(frame) IS FRAME_PCALL)
                  hook_leave(G(L));
               // Call __close handlers BEFORE modifying L->base
               TValue* target = frame_prevd(frame) + 1;
               unwind_close_all(L, L->base - 1, target);
               L->base = target;
               L->cframe = cf;
               unwindstack(L, L->base);
            }
            return (void*)((intptr_t)cf | CFRAME_UNWIND_FF);
      }
   }

   // No C frame.

   if (errcode) {
      TValue* target = tvref(L->stack) + 1 + LJ_FR2;
      unwind_close_all(L, L->base - 1, target);
      L->base = target;
      L->cframe = nullptr;
      unwindstack(L, L->base);
      if (G(L)->panic) G(L)->panic(L);
      exit(EXIT_FAILURE);
   }
   return L;  //  Anything non-nullptr will do.
}

//********************************************************************************************************************
// External frame unwinding

#if LJ_ABI_WIN

extern "C" void err_unwind_win_jit(global_State* g, int errcode);
extern "C" void err_raise_ext(global_State *g, int errcode);

#elif !LJ_NO_UNWIND and (defined(__GNUC__) or defined(__clang__))

// We have to use our own definitions instead of the mandatory (!) unwind.h,
// since various OS, distros and compilers mess up the header installation.

typedef struct _Unwind_Context _Unwind_Context;

#define _URC_OK         0
#define _URC_FATAL_PHASE2_ERROR  2
#define _URC_FATAL_PHASE1_ERROR  3
#define _URC_HANDLER_FOUND 6
#define _URC_INSTALL_CONTEXT  7
#define _URC_CONTINUE_UNWIND  8
#define _URC_FAILURE    9

#define LJ_UEXCLASS     0x4c55414a49543200ULL   //  LUAJIT2\0
#define LJ_UEXCLASS_MAKE(c)   (LJ_UEXCLASS | (uint64_t)(c))
#define LJ_UEXCLASS_CHECK(cl) (((cl) ^ LJ_UEXCLASS) <= 0xff)
#define LJ_UEXCLASS_ERRCODE(cl)  ((int)((cl) & 0xff))

#if !LJ_TARGET_ARM

typedef struct _Unwind_Exception
{
   uint64_t exclass;
   void (*excleanup)(int, struct _Unwind_Exception*);
   uintptr_t p1, p2;
} __attribute__((__aligned__)) _Unwind_Exception;
#define UNWIND_EXCEPTION_TYPE _Unwind_Exception

extern "C" uintptr_t _Unwind_GetCFA(_Unwind_Context*);
extern "C" void _Unwind_SetGR(_Unwind_Context*, int, uintptr_t);
extern "C" uintptr_t _Unwind_GetIP(_Unwind_Context*);
extern "C" void _Unwind_SetIP(_Unwind_Context*, uintptr_t);
extern "C" void _Unwind_DeleteException(_Unwind_Exception*);
extern "C" int _Unwind_RaiseException(_Unwind_Exception*);

#define _UA_SEARCH_PHASE   1
#define _UA_CLEANUP_PHASE  2
#define _UA_HANDLER_FRAME  4
#define _UA_FORCE_UNWIND   8

// DWARF2 personality handler referenced from interpreter .eh_frame.
LJ_FUNCA int lj_err_unwind_dwarf(int version, int actions, uint64_t uexclass, _Unwind_Exception* uex, _Unwind_Context* ctx)
{
   void* cf;
   lua_State *L;
   if (version != 1)
      return _URC_FATAL_PHASE1_ERROR;
   cf = (void*)_Unwind_GetCFA(ctx);
   L = cframe_L(cf);
   if ((actions & _UA_SEARCH_PHASE)) {
#if LJ_UNWIND_EXT
      if (err_unwind(L, cf, 0) == nullptr) return _URC_CONTINUE_UNWIND;
#endif
      if (!LJ_UEXCLASS_CHECK(uexclass)) {
         setstrV(L, L->top++, lj_err_str(L, ErrMsg::ERRCPP));
      }
      return _URC_HANDLER_FOUND;
   }
   if ((actions & _UA_CLEANUP_PHASE)) {
      int errcode;
      if (LJ_UEXCLASS_CHECK(uexclass)) {
         errcode = LJ_UEXCLASS_ERRCODE(uexclass);
      }
      else {
         if ((actions & _UA_HANDLER_FRAME))
            _Unwind_DeleteException(uex);
         errcode = LUA_ERRRUN;
      }
#if LJ_UNWIND_EXT
      cf = err_unwind(L, cf, errcode);
      if ((actions & _UA_FORCE_UNWIND)) {
         return _URC_CONTINUE_UNWIND;
      }
      else if (cf) {
         _Unwind_SetGR(ctx, LJ_TARGET_EHRETREG, errcode);
         _Unwind_SetIP(ctx, (uintptr_t)(cframe_unwind_ff(cf) ? lj_vm_unwind_ff_eh : lj_vm_unwind_c_eh));
         return _URC_INSTALL_CONTEXT;
      }
#if LJ_TARGET_X86ORX64
      else if ((actions & _UA_HANDLER_FRAME)) {
         /* Workaround for ancient libgcc bug. Still present in RHEL 5.5. :-/
         ** Real fix: http://gcc.gnu.org/viewcvs/trunk/gcc/unwind-dw2.c?r1=121165&r2=124837&pathrev=153877&diff_format=h
         */
         _Unwind_SetGR(ctx, LJ_TARGET_EHRETREG, errcode);
         _Unwind_SetIP(ctx, (uintptr_t)lj_vm_unwind_rethrow);
         return _URC_INSTALL_CONTEXT;
      }
#endif
#else
      /* This is not the proper way to escape from the unwinder. We get away with
      ** it on non-x64 because the interpreter restores all callee-saved regs.
      */
      lj_err_throw(L, errcode);
#if LJ_TARGET_X64
#error "Broken build system -- only use the provided Makefiles!"
#endif
#endif
   }
   return _URC_CONTINUE_UNWIND;
}

#if LJ_UNWIND_EXT && defined(LUA_USE_ASSERT)
struct dwarf_eh_bases { void* tbase, * dbase, * func; };
extern "C" const void* _Unwind_Find_FDE(void* pc, struct dwarf_eh_bases* bases);

// Verify that external error handling actually has a chance to work.
void lj_err_verify(void)
{
#if !LJ_TARGET_OSX
   // Check disabled on MacOS due to brilliant software engineering at Apple.
   struct dwarf_eh_bases ehb;
   lj_assertX(_Unwind_Find_FDE((void*)lj_err_throw, &ehb), "broken build: external frame unwinding enabled, but missing -funwind-tables");
#endif
   /* Check disabled, because of broken Fedora/ARM64. See #722.
   lj_assertX(_Unwind_Find_FDE((void *)_Unwind_RaiseException, &ehb), "broken build: external frame unwinding enabled, but system libraries have no unwind tables");
   */
}
#endif

#if LJ_UNWIND_JIT
// DWARF2 personality handler for JIT-compiled code.
static int err_unwind_jit(int version, int actions,
   uint64_t uexclass, _Unwind_Exception* uex, _Unwind_Context* ctx)
{
   // NYI: FFI C++ exception interoperability.
   if (version != 1 or !LJ_UEXCLASS_CHECK(uexclass))
      return _URC_FATAL_PHASE1_ERROR;
   if ((actions & _UA_SEARCH_PHASE)) {
      return _URC_HANDLER_FOUND;
   }
   if ((actions & _UA_CLEANUP_PHASE)) {
      global_State* g = *(global_State**)(uex + 1);
      ExitNo exitno;
      uintptr_t addr = _Unwind_GetIP(ctx);  //  Return address _after_ call.
      uintptr_t stub = lj_trace_unwind(G2J(g), addr - sizeof(MCode), &exitno);
      lj_assertG(tvref(g->jit_base), "unexpected throw across mcode frame");
      if (stub) {  // Jump to side exit to unwind the trace.
         G2J(g)->exitcode = LJ_UEXCLASS_ERRCODE(uexclass);
         _Unwind_SetIP(ctx, stub);
         return _URC_INSTALL_CONTEXT;
      }
      return _URC_FATAL_PHASE2_ERROR;
   }
   return _URC_FATAL_PHASE1_ERROR;
}

/* DWARF2 template frame info for JIT-compiled code.
**
** After copying the template to the start of the mcode segment,
** the frame handler function and the code size is patched.
** The frame handler always installs a new context to jump to the exit,
** so don't bother to add any unwind opcodes.
*/

static const uint8_t err_frame_jit_template[] = {
#if LJ_BE
  0,0,0,
#endif
  0x1c,  //  CIE length.
#if LJ_LE
  0,0,0,
#endif
  0,0,0,0, 1, 'z','P','R',0,  //  CIE mark, CIE version, augmentation.
  1, 0x78, LJ_TARGET_EHRAREG,  //  Code/data align, RA.
  10, 0, 0,0,0,0,0,0,0,0, 0x1b,  //  Aug. data ABS handler, PCREL|SDATA4 code.
  0,0,0,0,0,  //  Alignment.
#if LJ_BE
  0,0,0,
#endif
  0x14,  //  FDE length.
  0,0,0,
  0x24,  //  CIE offset.
  0,0,0,
  0x14,  //  Code offset. After Final FDE.
#if LJ_LE
  0,0,0,
#endif
  0,0,0,0, 0, 0,0,0, //  Code size, augmentation length, alignment.
  0,0,0,0,  //  Alignment.
  0,0,0,0  //  Final FDE.
};

#define ERR_FRAME_JIT_OFS_HANDLER   0x12
#define ERR_FRAME_JIT_OFS_FDE       0x20
#define ERR_FRAME_JIT_OFS_CODE_SIZE 0x2c
#if LJ_TARGET_OSX
#define ERR_FRAME_JIT_OFS_REGISTER  ERR_FRAME_JIT_OFS_FDE
#else
#define ERR_FRAME_JIT_OFS_REGISTER  0
#endif

extern "C" void __register_frame(const void*);
extern "C" void __deregister_frame(const void*);

uint8_t* lj_err_register_mcode(void* base, size_t sz, uint8_t* info)
{
   void* handler;
   memcpy(info, err_frame_jit_template, sizeof(err_frame_jit_template));
   handler = (void*)err_unwind_jit;
   memcpy(info + ERR_FRAME_JIT_OFS_HANDLER, &handler, sizeof(handler));
   *(uint32_t*)(info + ERR_FRAME_JIT_OFS_CODE_SIZE) =
      (uint32_t)(sz - sizeof(err_frame_jit_template) - (info - (uint8_t*)base));
   __register_frame(info + ERR_FRAME_JIT_OFS_REGISTER);
#ifdef LUA_USE_ASSERT
   {
      struct dwarf_eh_bases ehb;
      lj_assertX(_Unwind_Find_FDE(info + sizeof(err_frame_jit_template) + 1, &ehb),
         "bad JIT unwind table registration");
   }
#endif
   return info + sizeof(err_frame_jit_template);
}

void lj_err_deregister_mcode(void* base, size_t sz, uint8_t* info)
{
   __deregister_frame(info + ERR_FRAME_JIT_OFS_REGISTER);
}
#endif

#else //  LJ_TARGET_ARM

#define _US_VIRTUAL_UNWIND_FRAME 0
#define _US_UNWIND_FRAME_STARTING   1
#define _US_ACTION_MASK       3
#define _US_FORCE_UNWIND      8

typedef struct _Unwind_Control_Block _Unwind_Control_Block;
#define UNWIND_EXCEPTION_TYPE _Unwind_Control_Block

struct _Unwind_Control_Block {
   uint64_t exclass;
   uint32_t misc[20];
};

extern "C" int _Unwind_RaiseException(_Unwind_Control_Block*);
extern "C" int __gnu_unwind_frame(_Unwind_Control_Block*, _Unwind_Context*);
extern "C" int _Unwind_VRS_Set(_Unwind_Context*, int, uint32_t, int, void*);
extern "C" int _Unwind_VRS_Get(_Unwind_Context*, int, uint32_t, int, void*);

static inline uint32_t _Unwind_GetGR(_Unwind_Context* ctx, int r)
{
   uint32_t v;
   _Unwind_VRS_Get(ctx, 0, r, 0, &v);
   return v;
}

static inline void _Unwind_SetGR(_Unwind_Context* ctx, int r, uint32_t v)
{
   _Unwind_VRS_Set(ctx, 0, r, 0, &v);
}

extern "C" void lj_vm_unwind_ext(void);

// ARM unwinder personality handler referenced from interpreter .ARM.extab.
LJ_FUNCA int lj_err_unwind_arm(int state, _Unwind_Control_Block* ucb,
   _Unwind_Context* ctx)
{
   void* cf = (void*)_Unwind_GetGR(ctx, 13);
   lua_State *L = cframe_L(cf);
   int errcode;

   switch ((state & _US_ACTION_MASK)) {
   case _US_VIRTUAL_UNWIND_FRAME:
      if ((state & _US_FORCE_UNWIND)) break;
      return _URC_HANDLER_FOUND;
   case _US_UNWIND_FRAME_STARTING:
      if (LJ_UEXCLASS_CHECK(ucb->exclass)) {
         errcode = LJ_UEXCLASS_ERRCODE(ucb->exclass);
      }
      else {
         errcode = LUA_ERRRUN;
         setstrV(L, L->top++, lj_err_str(L, ErrMsg::ERRCPP));
      }
      cf = err_unwind(L, cf, errcode);
      if ((state & _US_FORCE_UNWIND) or cf == nullptr) break;
      _Unwind_SetGR(ctx, 15, (uint32_t)lj_vm_unwind_ext);
      _Unwind_SetGR(ctx, 0, (uint32_t)ucb);
      _Unwind_SetGR(ctx, 1, (uint32_t)errcode);
      _Unwind_SetGR(ctx, 2, cframe_unwind_ff(cf) ?
         (uint32_t)lj_vm_unwind_ff_eh :
      (uint32_t)lj_vm_unwind_c_eh);
      return _URC_INSTALL_CONTEXT;
   default:
      return _URC_FAILURE;
   }
   if (__gnu_unwind_frame(ucb, ctx) != _URC_OK)
      return _URC_FAILURE;
#ifdef LUA_USE_ASSERT
   // We should never get here unless this is a forced unwind aka backtrace.
   if (_Unwind_GetGR(ctx, 0) == 0xff33aa77) {
      _Unwind_SetGR(ctx, 0, 0xff33aa88);
   }
#endif
   return _URC_CONTINUE_UNWIND;
}

#if LJ_UNWIND_EXT && defined(LUA_USE_ASSERT)
typedef int (*_Unwind_Trace_Fn)(_Unwind_Context*, void*);
extern "C" int _Unwind_Backtrace(_Unwind_Trace_Fn, void*);

static int err_verify_bt(_Unwind_Context* ctx, int* got)
{
   if (_Unwind_GetGR(ctx, 0) == 0xff33aa88) { *got = 2; }
   else if (*got == 0) { *got = 1; _Unwind_SetGR(ctx, 0, 0xff33aa77); }
   return _URC_OK;
}

// Verify that external error handling actually has a chance to work.
void lj_err_verify(void)
{
   int got = 0;
   _Unwind_Backtrace((_Unwind_Trace_Fn)err_verify_bt, &got);
   lj_assertX(got == 2, "broken build: external frame unwinding enabled, but missing -funwind-tables");
}
#endif

/*
** Note: LJ_UNWIND_JIT is not implemented for 32 bit ARM.
**
** The quirky ARM unwind API doesn't have __register_frame().
** A potential workaround might involve _Unwind_Backtrace.
** But most 32 bit ARM targets don't qualify for LJ_UNWIND_EXT, anyway,
** since they are built without unwind tables by default.
*/

#endif //  LJ_TARGET_ARM


#if LJ_UNWIND_EXT
static __thread struct {
   UNWIND_EXCEPTION_TYPE ex;
   global_State* g;
} static_uex;

// Raise external exception.
static void err_raise_ext(global_State* g, int errcode)
{
   memset(&static_uex, 0, sizeof(static_uex));
   static_uex.ex.exclass = LJ_UEXCLASS_MAKE(errcode);
   static_uex.g = g;
   _Unwind_RaiseException(&static_uex.ex);
}

#endif

#endif

// Throw error. Find catch frame, unwind stack and continue.

LJ_NOINLINE void LJ_FASTCALL lj_err_throw(lua_State *L, int errcode)
{
   global_State* g = G(L);
   lj_trace_abort(g);
   L->status = LUA_OK;

#if LJ_UNWIND_EXT
   err_raise_ext(g, errcode);

   // A return from this function signals a corrupt C stack that cannot be
   // unwound. We have no choice but to call the panic function and exit.
   //
   // Usually this is caused by a C function without unwind information.
   // This may happen if you've manually enabled LUAJIT_UNWIND_EXTERNAL
   // and forgot to recompile *every* non-C++ file with -funwind-tables.

   if (G(L)->panic) G(L)->panic(L);
#else
   setmref(g->jit_base, nullptr);

   {
      void* cf = err_unwind(L, nullptr, errcode);
      if (cf IS ERR_TRYHANDLER) {
         // A try-except handler was found. check_try_handler() only recorded
         // the handler PC. Now set up the actual state before resuming:
         // - Restore L->base and L->top to try block entry state
         // - Close upvalues above the restored top
         // - Pop the try frame
         // - Build exception table and place in handler's register
         setup_try_handler(L);

         // Resume execution at the handler PC using the VM entry point.
         lj_vm_resume_try(cframe_raw(L->cframe));
      }
      else if (cframe_unwind_ff(cf))
         lj_vm_unwind_ff(cframe_raw(cf));
      else
         lj_vm_unwind_c(cframe_raw(cf), errcode);
   }
#endif
   exit(EXIT_FAILURE);
}

// Return string object for error message.
LJ_NOINLINE GCstr* lj_err_str(lua_State *L, ErrMsg em)
{
   return lj_str_newz(L, err2msg(em));
}

//********************************************************************************************************************
// Out-of-memory error.

LJ_NOINLINE void lj_err_mem(lua_State *L)
{
   if (L->status == LUA_ERRERR + 1)  //  Don't touch the stack during lua_open.
      lj_vm_unwind_c(L->cframe, LUA_ERRMEM);
   setstrV(L, L->top++, lj_err_str(L, ErrMsg::ERRMEM));
   lj_err_throw(L, LUA_ERRMEM);
}

//********************************************************************************************************************
// Find error function for runtime errors. Requires an extra stack traversal.
static ptrdiff_t finderrfunc(lua_State *L)
{
   cTValue* frame = L->base - 1, * bot = tvref(L->stack) + LJ_FR2;
   void* cf = L->cframe;
   while (frame > bot and cf) {
      while (cframe_nres(cframe_raw(cf)) < 0) {  // cframe without frame?
         if (frame >= restorestack(L, -cframe_nres(cf)))
            break;
         if (cframe_errfunc(cf) >= 0)  //  Error handler not inherited (-1)?
            return cframe_errfunc(cf);
         cf = cframe_prev(cf);  //  Else unwind cframe and continue searching.
         if (cf == nullptr)
            return 0;
      }
      switch (frame_typep(frame)) {
      case FRAME_LUA:
      case FRAME_LUAP:
         frame = frame_prevl(frame);
         break;
      case FRAME_C:
         cf = cframe_prev(cf);
         // fallthrough
      case FRAME_VARG:
         frame = frame_prevd(frame);
         break;
      case FRAME_CONT:
         if (frame_iscont_fficb(frame))
            cf = cframe_prev(cf);
         frame = frame_prevd(frame);
         break;
      case FRAME_CP:
         if (cframe_canyield(cf)) return 0;
         if (cframe_errfunc(cf) >= 0)
            return cframe_errfunc(cf);
         cf = cframe_prev(cf);
         frame = frame_prevd(frame);
         break;
      case FRAME_PCALL:
      case FRAME_PCALLH:
         if (frame_func(frame_prevd(frame))->c.ffid == FF_xpcall)
            return savestack(L, frame_prevd(frame) + 1);  //  xpcall's errorfunc.
         return 0;
      default:
         lj_assertL(0, "bad frame type");
         return 0;
      }
   }
   return 0;
}

//********************************************************************************************************************
// Runtime error.

LJ_NOINLINE void LJ_FASTCALL lj_err_run(lua_State *L)
{
   ptrdiff_t ef = tvref(G(L)->jit_base) ? 0 : finderrfunc(L);
   if (ef) {
      TValue* errfunc = restorestack(L, ef);
      TValue* top = L->top;
      lj_trace_abort(G(L));
      if (!tvisfunc(errfunc) or L->status == LUA_ERRERR) {
         setstrV(L, top - 1, lj_err_str(L, ErrMsg::ERRERR));
         lj_err_throw(L, LUA_ERRERR);
      }
      L->status = LUA_ERRERR;
      copyTV(L, top + LJ_FR2, top - 1);
      copyTV(L, top - 1, errfunc);
      if (LJ_FR2) setnilV(top++);
      L->top = top + 1;
      lj_vm_call(L, top, 1 + 1);  //  Stack: |errfunc|msg| -> |msg|
   }
   lj_err_throw(L, LUA_ERRRUN);
}

LJ_NOINLINE void LJ_FASTCALL lj_err_trace(lua_State *L, int errcode)
{
   if (errcode == LUA_ERRRUN) lj_err_run(L);
   else lj_err_throw(L, errcode);
}

//********************************************************************************************************************
// Formatted runtime error message.

LJ_NORET LJ_NOINLINE static void err_msgv(lua_State *L, ErrMsg em, ...)
{
   CSTRING msg;
   va_list argp;
   va_start(argp, em);
   if (curr_funcisL(L)) L->top = curr_topL(L);
   msg = lj_strfmt_pushvf(L, err2msg(em), argp);
   va_end(argp);
   lj_debug_addloc(L, msg, L->base - 1, nullptr);
   lj_err_run(L);
}

// Non-vararg variant for better calling conventions.

LJ_NOINLINE void lj_err_msg(lua_State *L, ErrMsg em)
{
   err_msgv(L, em);
}

// Vararg variant for formatted messages. Use this for errors raised from VM helper functions called from assembler
// (e.g. lj_arr_set, lj_meta_tset). These functions are called while executing bytecode and need L->top adjusted
// for proper unwinding.

LJ_NOINLINE void lj_err_msgv(lua_State *L, ErrMsg em, ...)
{
   va_list argp;
   va_start(argp, em);
   if (curr_funcisL(L)) L->top = curr_topL(L);
   auto msg = lj_strfmt_pushvf(L, err2msg(em), argp);
   va_end(argp);
   lj_debug_addloc(L, msg, L->base - 1, nullptr);
   lj_err_run(L);
}

//********************************************************************************************************************
// Lexer error.

LJ_NOINLINE void lj_err_lex(lua_State *L, GCstr* src, CSTRING tok, BCLine line, ErrMsg em, va_list argp)
{
   char buff[LUA_IDSIZE];
   lj_debug_shortname(buff, src, line);
   auto msg = lj_strfmt_pushvf(L, err2msg(em), argp);
   msg = lj_strfmt_pushf(L, "%s:%d: %s", buff, line, msg);
   if (tok) lj_strfmt_pushf(L, err2msg(ErrMsg::XNEAR), msg, tok);
   lj_err_throw(L, LUA_ERRSYNTAX);
}

//********************************************************************************************************************
// Typecheck error for operands.

LJ_NOINLINE void lj_err_optype(lua_State *L, cTValue* o, ErrMsg opm)
{
   auto tname = lj_typename(o);
   auto opname = err2msg(opm);
   if (curr_funcisL(L)) {
      GCproto *pt = curr_proto(L);
      const BCIns *pc = cframe_Lpc(L) - 1;
      CSTRING oname = nullptr;
      auto kind = lj_debug_slotname(pt, pc, (BCREG)(o - L->base), &oname);
      if (kind) err_msgv(L, ErrMsg::BADOPRT, opname, kind, oname, tname);
   }
   err_msgv(L, ErrMsg::BADOPRV, opname, tname);
}

//********************************************************************************************************************
// Typecheck error for ordered comparisons.

LJ_NOINLINE void lj_err_comp(lua_State *L, cTValue* o1, cTValue* o2)
{
   auto t1 = lj_typename(o1);
   auto t2 = lj_typename(o2);
   err_msgv(L, t1 == t2 ? ErrMsg::BADCMPV : ErrMsg::BADCMPT, t1, t2);
   // This assumes the two "boolean" entries are commoned by the C compiler.
}

//********************************************************************************************************************
// Typecheck error for __call.

LJ_NOINLINE void lj_err_optype_call(lua_State *L, TValue* o)
{
   // Gross hack if lua_[p]call or pcall/xpcall fail for a non-callable object:
   // L->base still points to the caller. So add a dummy frame with L instead
   // of a function. See lua_getstack().

   const BCIns* pc = cframe_Lpc(L);
   if (((ptrdiff_t)pc & FRAME_TYPE) != FRAME_LUA) {
      CSTRING tname = lj_typename(o);
      setframe_gc(o, obj2gco(L), LJ_TTHREAD);
      if (LJ_FR2) o++;
      setframe_pc(o, pc);
      L->top = L->base = o + 1;
      err_msgv(L, ErrMsg::BADCALL, tname);
   }
   lj_err_optype(L, o, ErrMsg::OPCALL);
}

//********************************************************************************************************************
// Error in context of caller.

LJ_NOINLINE void lj_err_callermsg(lua_State *L, CSTRING msg)
{
   TValue* frame = nullptr, * pframe = nullptr;
   if (not tvref(G(L)->jit_base)) {
      frame = L->base - 1;
      if (frame_islua(frame)) pframe = frame_prevl(frame);
      else if (frame_iscont(frame)) {
         if (frame_iscont_fficb(frame)) {
            pframe = frame;
            frame = nullptr;
         }
         else {
            pframe = frame_prevd(frame);
#if LJ_HASFFI
            // Remove frame for FFI metamethods.
            if (frame_func(frame)->c.ffid >= FF_ffi_meta___index &&
               frame_func(frame)->c.ffid <= FF_ffi_meta___tostring) {
               L->base = pframe + 1;
               L->top = frame;
               setcframe_pc(cframe_raw(L->cframe), frame_contpc(frame));
            }
#endif
         }
      }
   }
   lj_debug_addloc(L, msg, pframe, frame);
   lj_err_run(L);
}

//********************************************************************************************************************
// Formatted error in context of caller. Use this for errors raised from C library functions (lua_* API, lib_*.cpp).
// Do NOT use for VM helper functions called from assembler - use lj_err_msgv() instead, which adjusts L->top for
// proper unwinding.

LJ_NOINLINE void lj_err_callerv(lua_State *L, ErrMsg em, ...)
{
   CSTRING msg;
   va_list argp;
   va_start(argp, em);
   msg = lj_strfmt_pushvf(L, err2msg(em), argp);
   va_end(argp);
   lj_err_callermsg(L, msg);
}

//********************************************************************************************************************
// Error in context of caller.
// Do NOT use for VM helper functions called from assembler - use lj_err_msgv() instead, which adjusts L->top for
// proper unwinding.

LJ_NOINLINE void lj_err_caller(lua_State *L, ErrMsg em)
{
   lj_err_callermsg(L, err2msg(em));
}

//********************************************************************************************************************
// Argument error message.

LJ_NORET LJ_NOINLINE static void err_argmsg(lua_State *L, int narg, CSTRING msg)
{
   CSTRING fname = "?";
   CSTRING ftype = lj_debug_funcname(L, L->base - 1, &fname);
   if (narg < 0 and narg > LUA_REGISTRYINDEX) narg = (int)(L->top - L->base) + narg + 1;
   if (ftype and ftype[3] == 'h' and --narg == 0)  //  Check for "method".
      msg = lj_strfmt_pushf(L, err2msg(ErrMsg::BADSELF), fname, msg);
   else msg = lj_strfmt_pushf(L, err2msg(ErrMsg::BADARG), narg, fname, msg);
   lj_err_callermsg(L, msg);
}

//********************************************************************************************************************
// Formatted argument error.

LJ_NOINLINE void lj_err_argv(lua_State *L, int narg, ErrMsg em, ...)
{
   CSTRING msg;
   va_list argp;
   va_start(argp, em);
   msg = lj_strfmt_pushvf(L, err2msg(em), argp);
   va_end(argp);
   err_argmsg(L, narg, msg);
}

//********************************************************************************************************************
// Argument error.

LJ_NOINLINE void lj_err_arg(lua_State *L, int narg, ErrMsg em)
{
   err_argmsg(L, narg, err2msg(em));
}

//********************************************************************************************************************
// Typecheck error for arguments.

LJ_NOINLINE void lj_err_argtype(lua_State *L, int narg, CSTRING xname)
{
   CSTRING tname, msg;
   if (narg <= LUA_REGISTRYINDEX) {
      if (narg >= LUA_GLOBALSINDEX) {
         tname = lj_obj_itypename[~LJ_TTAB];
      }
      else {
         GCfunc* fn = curr_func(L);
         int idx = LUA_GLOBALSINDEX - narg;
         if (idx <= fn->c.nupvalues) tname = lj_typename(&fn->c.upvalue[idx - 1]);
         else tname = lj_obj_typename[0];
      }
   }
   else {
      TValue *o = narg < 0 ? L->top + narg : L->base + narg - 1;
      tname = o < L->top ? lj_typename(o) : lj_obj_typename[0];
   }
   msg = lj_strfmt_pushf(L, err2msg(ErrMsg::BADTYPE), xname, tname);
   err_argmsg(L, narg, msg);
}

//********************************************************************************************************************
// Typecheck error for arguments.

LJ_NOINLINE void lj_err_argt(lua_State *L, int narg, int tt)
{
   lj_err_argtype(L, narg, lj_obj_typename[tt + 1]);
}

//********************************************************************************************************************
// Type assignment error - used when assigning wrong type to a typed variable.

LJ_NOINLINE void lj_err_assigntype(lua_State *L, int slot, CSTRING expected_type)
{
   TValue* o = L->base + slot;
   CSTRING actual_type = o < L->top ? lj_typename(o) : lj_obj_typename[0];
   CSTRING msg = lj_strfmt_pushf(L, err2msg(ErrMsg::BADASSIGN), actual_type, expected_type);
   lj_err_callermsg(L, msg);
}

//********************************************************************************************************************
// Public error handling API

extern lua_CFunction lua_atpanic(lua_State *L, lua_CFunction panicf)
{
   lua_CFunction old = G(L)->panic;
   G(L)->panic = panicf;
   return old;
}

// Forwarders for the public API (C calling convention and no LJ_NORET).
extern int lua_error(lua_State *L)
{
   lj_err_run(L);
   return 0;  //  unreachable
}

extern int luaL_argerror(lua_State *L, int narg, CSTRING msg)
{
   err_argmsg(L, narg, msg);
   return 0;  //  unreachable
}

extern int luaL_typerror(lua_State *L, int narg, CSTRING xname)
{
   lj_err_argtype(L, narg, xname);
   return 0;  //  unreachable
}

extern void luaL_where(lua_State *L, int level)
{
   int size;
   cTValue* frame = lj_debug_frame(L, level, &size);
   lj_debug_addloc(L, "", frame, size ? frame + size : nullptr);
}

[[noreturn]] extern void luaL_error(lua_State *L, CSTRING fmt, ...)
{
   CSTRING msg;
   va_list argp;
   va_start(argp, fmt);
   msg = lj_strfmt_pushvf(L, fmt, argp);
   va_end(argp);
   lj_err_callermsg(L, msg);
}

//********************************************************************************************************************
// Internal assertion failure handler for LUA_USE_ASSERT and LUA_USE_APICHECK.

#if defined(LUA_USE_ASSERT) || defined(LUA_USE_APICHECK)

#include <cstdio>
#include <cstdarg>

LJ_NOINLINE void lj_assert_fail(global_State* g, CSTRING file, int line,
   CSTRING func, CSTRING fmt, ...)
{
   va_list argp;
   va_start(argp, fmt);
   fprintf(stderr, "LuaJIT ASSERT FAILED: %s:%d: %s: ", file, line, func);
   vfprintf(stderr, fmt, argp);
   fprintf(stderr, "\n");
   va_end(argp);
   fflush(stderr);
   abort();
}

#endif
