/*
** Fluid defer runtime support.
*/

#ifndef _LJ_DEFER_H
#define _LJ_DEFER_H

#include "lj_obj.h"

struct DeferRecord;
typedef struct DeferRecord DeferRecord;

typedef struct DeferFrame {
  struct DeferFrame *prev;
  DeferRecord *top;
  uint16_t base;
  uint16_t count;
} DeferFrame;

struct DeferRecord {
  DeferRecord *prev;
  uint16_t index;
  uint16_t nargs;
  TValue slot[1];
};

LJ_FUNC void lj_defer_state_init(lua_State *L);
LJ_FUNC void lj_defer_state_close(lua_State *L);
LJ_FUNC DeferFrame *lj_defer_frame_acquire(lua_State *L, uint16_t base);
LJ_FUNCA void lj_defer_register(lua_State *L, TValue *slot, uint32_t nargs,
                                uint32_t index);
LJ_FUNCA void lj_defer_unwind(lua_State *L, uint32_t count, uint32_t base);
LJ_FUNCA void lj_defer_raise_pending(lua_State *L);

#endif
