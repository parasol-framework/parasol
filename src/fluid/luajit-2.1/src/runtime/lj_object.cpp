// Native Parasol object handling for LuaJIT.
// Copyright (C) 2026 Paul Manias

#define lj_object_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_object.h"

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
// Free a GCobject during garbage collection.
// If the object owns the Parasol object (not detached), it will be freed.

void LJ_FASTCALL lj_object_free(global_State *g, GCobject *obj)
{
   // Release any active locks before freeing
   while (obj->accesscount > 0) {
      if (obj->flags & GCOBJ_LOCKED) {
         ReleaseObject((OBJECTPTR)obj->ptr);
         obj->flags &= ~GCOBJ_LOCKED;
         obj->ptr = nullptr;
      }
      obj->accesscount--;
   }

   // If we own the object (not detached), free it
   if (!(obj->flags & GCOBJ_DETACHED) and obj->uid) {
      if (auto target = GetObjectPtr(obj->uid)) {
         // Only free if we're still the owner
         auto *L = mainthread(g);
         if (L and L->script) {
            if ((target->Class->BaseClassID IS CLASSID::RECORDSET) or
                (target->Owner IS L->script) or
                (target->ownerID() IS L->script->TargetID)) {
               FreeResource(target);
            }
         }
      }
   }

   // Free the GCobject structure
   lj_mem_free(g, obj, sizeof(GCobject));
}
