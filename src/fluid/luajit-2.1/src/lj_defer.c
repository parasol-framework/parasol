/*
** Fluid defer runtime support.
*/

#define lj_defer_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_state.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_vm.h"
#include "lj_defer.h"

#define REG_STEP       1

static DeferFrame *defer_frame_new(lua_State *L, uint16_t base)
{
  DeferFrame *frame;
  if (L->defer_free) {
    frame = L->defer_free;
    L->defer_free = frame->prev;
  } else {
    frame = (DeferFrame *)lj_mem_new(L, sizeof(DeferFrame));
  }
  frame->prev = L->defer_frame;
  frame->top = NULL;
  frame->base = base;
  frame->count = 0;
  L->defer_frame = frame;
  return frame;
}

static void defer_entry_free(lua_State *L, DeferRecord *entry)
{
  global_State *g = G(L);
  lj_mem_free(g, entry, sizeof(DeferRecord) + (size_t)entry->nargs * sizeof(TValue));
}

static DeferRecord *defer_entry_new(lua_State *L, uint32_t index, uint32_t nargs)
{
  size_t sz = sizeof(DeferRecord) + (size_t)nargs * sizeof(TValue);
  DeferRecord *entry = (DeferRecord *)lj_mem_new(L, (GCSize)sz);
  entry->index = (uint16_t)index;
  entry->nargs = (uint16_t)nargs;
  entry->prev = NULL;
  return entry;
}

void lj_defer_state_init(lua_State *L)
{
  L->defer_frame = NULL;
  L->defer_free = NULL;
  setnilV(&L->defer_error);
  L->defer_pending = 0;
}

static void defer_frames_clear(lua_State *L)
{
  DeferFrame *frame = L->defer_frame;
  while (frame) {
    DeferFrame *prev = frame->prev;
    DeferRecord *entry = frame->top;
    while (entry) {
      DeferRecord *p = entry->prev;
      defer_entry_free(L, entry);
      entry = p;
    }
    lj_mem_free(G(L), frame, sizeof(DeferFrame));
    frame = prev;
  }
  L->defer_frame = NULL;
}

void lj_defer_state_close(lua_State *L)
{
  defer_frames_clear(L);
  while (L->defer_free) {
    DeferFrame *frame = L->defer_free;
    L->defer_free = frame->prev;
    lj_mem_free(G(L), frame, sizeof(DeferFrame));
  }
  L->defer_free = NULL;
  setnilV(&L->defer_error);
  L->defer_pending = 0;
}

DeferFrame *lj_defer_frame_acquire(lua_State *L, uint16_t base)
{
  DeferFrame *frame = L->defer_frame;
  if (!frame || base < frame->base) {
    frame = defer_frame_new(L, base);
  }
  return frame;
}

LJ_FUNCA void lj_defer_register(lua_State *L, TValue *slot, uint32_t nargs,
                                uint32_t index)
{
  TValue *src;
  uint32_t i;
  DeferFrame *frame = lj_defer_frame_acquire(L, (uint16_t)index);
  DeferRecord *entry = defer_entry_new(L, index, nargs);

  src = slot;
  copyTV(L, &entry->slot[0], src);
  src += REG_STEP;
  for (i = 0; i < nargs; i++) {
    copyTV(L, &entry->slot[1+i], src);
    src += REG_STEP;
  }

  /* Clear handler register and arguments. */
  src = slot;
  setnilV(src);
  src += REG_STEP;
  for (i = 0; i < nargs; i++) {
    setnilV(src);
    src += REG_STEP;
  }

  entry->prev = frame->top;
  frame->top = entry;
  frame->count++;
}

static void defer_frame_pop(lua_State *L)
{
  DeferFrame *frame = L->defer_frame;
  if (frame) {
    L->defer_frame = frame->prev;
    frame->prev = L->defer_free;
    frame->top = NULL;
    frame->base = 0;
    frame->count = 0;
    L->defer_free = frame;
  }
}

static TValue *defer_call_slot(lua_State *L, DeferRecord *entry)
{
  TValue *dst = L->top;
  TValue *src = entry->slot;
  uint32_t i;
  MSize need = (MSize)((entry->nargs + 1) * (1+LJ_FR2));

  if (dst + need > tvref(L->maxstack))
    lj_state_growstack(L, need);

  copyTV(L, dst, src++);
  dst++;
  if (LJ_FR2) setnilV(dst++);

  for (i = 0; i < entry->nargs; i++) {
    copyTV(L, dst, src++);
    dst++;
    if (LJ_FR2) setnilV(dst++);
  }
  L->top = dst;
  return dst - (entry->nargs + 1) * REG_STEP;
}

LJ_FUNCA void lj_defer_unwind(lua_State *L, uint32_t count, uint32_t base)
{
  DeferFrame *frame = L->defer_frame;
  TValue errsave;
  int has_err = 0;

  while (frame && count > 0) {
    while (frame->top && frame->top->index >= base && count > 0) {
      DeferRecord *entry = frame->top;
      TValue *callbase;
      int status;

      frame->top = entry->prev;
      frame->count--;
      callbase = defer_call_slot(L, entry);
      status = lj_vm_pcall(L, callbase, 0+1, 0);
      if (status) {
        L->top--;
        if (!has_err && !L->defer_pending) {
          copyTV(L, &errsave, L->top);
          has_err = 1;
        }
      } else {
        L->top = callbase;
      }
      defer_entry_free(L, entry);
      count--;
    }
    if (!frame->top) {
      DeferFrame *prev = frame->prev;
      defer_frame_pop(L);
      frame = prev;
    } else {
      break;
    }
  }

  if (!frame && count > 0)
    count = 0;

  if (has_err) {
    copyTV(L, &L->defer_error, &errsave);
    L->defer_pending = 1;
  }

  if (L->defer_pending)
    lj_defer_raise_pending(L);
}

LJ_FUNCA void lj_defer_raise_pending(lua_State *L)
{
  if (L->defer_pending) {
    copyTV(L, L->top, &L->defer_error);
    incr_top(L);
    L->defer_pending = 0;
    setnilV(&L->defer_error);
    lj_err_run(L);
  }
}
