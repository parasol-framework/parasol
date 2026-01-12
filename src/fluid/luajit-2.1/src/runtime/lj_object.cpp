// Native Parasol object handling for LuaJIT.
// Copyright (C) 2026 Paul Manias

#define lj_object_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_object.h"
#include "../../defs.h"

//********************************************************************************************************************
// Create a new GCobject for a Parasol object reference.  The object is allocated via the GC.

GCobject * lj_object_new(lua_State *L, OBJECTID UID, OBJECTPTR Ptr, objMetaClass *ClassPtr, uint8_t Flags)
{
   // lj_mem_newgco allocates and links to GC root list with newwhite()
   auto obj = (GCobject *)lj_mem_newgco(L, sizeof(GCobject));
   obj->gct         = ~LJ_TOBJECT;
   obj->udtype      = 0;
   obj->flags       = Flags;
   obj->accesscount = 0;
   setgcrefnull(obj->metatable);
   setgcrefnull(obj->gclist);
   obj->uid      = UID;
   obj->ptr      = Ptr;
   obj->classptr = ClassPtr;

   return obj;
}

//********************************************************************************************************************
// Finalize a GCobject during GC finalization phase. This is called directly by the GC without metamethod lookup.
// Releases any locks and frees the underlying Parasol object if owned by this script.

void lj_object_finalize(lua_State *L, GCobject *obj)
{
   while (obj->accesscount > 0) release_object(obj);

   if (not obj->is_detached()) {
      // Only free the Parasol object if it's owned by this script.
      // Exception: Recordset objects are always freed as they must be owned by a Database object.
      if (auto ptr = GetObjectPtr(obj->uid)) {
         if ((ptr->Class->BaseClassID IS CLASSID::RECORDSET) or
             (ptr->Owner IS L->script) or
             (ptr->ownerID() IS L->script->TargetID)) {
            pf::Log log("obj.destruct");
            log.trace("Freeing Fluid-owned object #%d.", obj->uid);
            FreeResource(ptr);
         }
      }
   }
}

//********************************************************************************************************************
// Free a GCobject during garbage collection sweep phase.
// NOTE: The underlying Parasol object is NOT freed here. It should have been freed earlier
// during the finalization phase by lj_object_finalize().
// This function only releases any remaining locks and frees the GCobject wrapper itself.

void LJ_FASTCALL lj_object_free(global_State *g, GCobject *obj)
{
   // Release any active locks before freeing the wrapper
   while (obj->accesscount > 0) {
      if (obj->flags & GCOBJ_LOCKED) {
         ReleaseObject((OBJECTPTR)obj->ptr);
         obj->flags &= ~GCOBJ_LOCKED;
         obj->ptr = nullptr;
      }
      obj->accesscount--;
   }

   // Free the GCobject structure (Parasol object should have been freed by __gc finalizer)
   lj_mem_free(g, obj, sizeof(GCobject));
}
