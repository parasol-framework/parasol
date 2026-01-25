// Native Parasol object handling for LuaJIT.
// Copyright (C) 2026 Paul Manias

#define lj_object_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_ir.h"
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
   obj->uid         = UID;
   obj->ptr         = Ptr;
   obj->classptr    = ClassPtr;
   obj->read_table  = nullptr;  // Lazily populated on first __index access
   obj->write_table = nullptr;  // Lazily populated on first __newindex access

   return obj;
}

//********************************************************************************************************************
// Finalize a GCobject during GC finalization phase. This is called directly by the GC without metamethod lookup.
// Releases any locks and frees the underlying Parasol object if owned by this script.

void lj_object_finalize(lua_State *L, GCobject *obj)
{
   while (obj->accesscount > 0) release_object(obj); // Critical for recovering from exceptions

   if (not obj->is_detached()) {
      // Only free the Parasol object if it's owned by this script.
      // Exception: Recordset objects are always freed as they must be owned by a Database object.
      if (auto ptr = GetObjectPtr(obj->uid)) {
         if ((ptr->Class->BaseClassID IS CLASSID::RECORDSET) or
             (ptr->Owner IS L->script) or
             (ptr->ownerID() IS L->script->TargetID)) {
            pf::Log log("obj.destruct");
            log.traceBranch("Freeing Fluid-owned object #%d.", obj->uid);
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

//********************************************************************************************************************
// pairs() iterator for objects - returns field name and flags for each iteration.

static int object_next_pair(lua_State *L)
{
   auto fields = (Field *)lua_touserdata(L, lua_upvalueindex(1));
   int field_total = lua_tointeger(L, lua_upvalueindex(2));
   int field_index = lua_tointeger(L, lua_upvalueindex(3));

   if ((field_index >= 0) and (field_index < field_total)) {
      lua_pushinteger(L, field_index + 1);
      lua_replace(L, lua_upvalueindex(3)); // Update the field counter

      lua_pushstring(L, fields[field_index].Name);
      lua_pushinteger(L, fields[field_index].Flags);
      return 2;
   }
   else return 0; // Terminates the iteration
}

int lj_object_pairs(lua_State *L)
{
   auto def = objectV(L->base);

   Field *fields;
   int total;
   if (def->classptr->get(FID_Dictionary, fields, total) IS ERR::Okay) {
      // Create the iterator closure with upvalues
      lua_pushlightuserdata(L, fields);
      lua_pushinteger(L, total);
      lua_pushinteger(L, 0);
      lua_pushcclosure(L, object_next_pair, 3);

      // FFH return values are placed at specific stack positions:
      // L->base - 2: Iterator function
      // L->base - 1: State (unused)
      // L->base:     Initial key (nil for pairs)
      TValue *o = L->base;
      copyTV(L, o - 2, L->top - 1);  // Copy closure to return position
      setnilV(o - 1);  // State (unused, closure uses upvalues)
      setnilV(o);      // Initial control variable
      L->top--;        // Pop the closure from top (now at FFH return position)
      return 3;
   }
   else luaL_error(L, ERR::FieldSearch, "Object class defines no fields.");
   return 0;
}

//********************************************************************************************************************
// ipairs() iterator for objects - returns field index and name for each iteration.

static int object_next_ipair(lua_State *L)
{
   auto fields = (Field *)lua_touserdata(L, lua_upvalueindex(1));
   int field_total = lua_tointeger(L, lua_upvalueindex(2));
   int field_index = lua_tointeger(L, 2); // Arg 2 is the previous index. It's nil if this is the first iteration.

   if ((field_index >= 0) and (field_index < field_total)) {
      lua_pushinteger(L, field_index + 1);
      lua_pushstring(L, fields[field_index].Name);
      return 2;
   }
   else return 0; // Terminates the iteration
}

int lj_object_ipairs(lua_State *L)
{
   auto def = objectV(L->base);

   Field *fields;
   int total;
   if (def->classptr->get(FID_Dictionary, fields, total) IS ERR::Okay) {
      // Create the iterator closure with upvalues
      lua_pushlightuserdata(L, fields);
      lua_pushinteger(L, total);
      lua_pushcclosure(L, object_next_ipair, 2);

      // FFH return values are placed at specific stack positions:
      // L->base - 2: Iterator function
      // L->base - 1: State (unused)
      // L->base:     Initial key (0 for ipairs)
      TValue *o = L->base;
      copyTV(L, o - 2, L->top - 1);  // Copy closure to return position
      setnilV(o - 1);  // State (unused, closure uses upvalues)
      setintV(o, 0);   // Initial control variable (field index starts at 0)
      L->top--;        // Pop the closure from top (now at FFH return position)
      return 3;
   }
   else luaL_error(L, ERR::FieldSearch, "Object class defines no fields.");
   return 0;
}

//********************************************************************************************************************
// Fast object field get - called from BC_OBGETF bytecode handler. Writes result directly to Dest, or throws an error
// if the field doesn't exist or the object has been freed.

extern "C" void lj_object_gets(lua_State *L, GCobject *Obj, GCstr *Key, TValue *Dest)
{
   // Ensure L->top is past the value register before any error can be thrown.
   // luaL_error pushes the error string to L->top, which would corrupt active registers if too low.
   const auto saved_top = L->top;
   const auto stack_base = tvref(L->stack);
   const auto stack_end = stack_base + L->stacksize;
   if (Dest >= stack_base and Dest < stack_end and L->top <= Dest) {
      L->top = Dest + 1;  // Ensure handler pushes after destination slot
   }

   if (not Obj->uid) luaL_error(L, ERR::DoesNotExist, "Object dereferenced, unable to read field.");

   // Use cached read_table or lazily populate it
   auto read_table = (READ_TABLE *)Obj->read_table;
   if (not read_table) {
      read_table = get_read_table(L, Obj->classptr);
      Obj->read_table = (void *)read_table;
   }

   // Look up the field handler using the string's precomputed hash

   auto hash_key = obj_read(Key->hash);
   auto func = read_table->find(hash_key);
   if (func IS read_table->end()) {
      luaL_error(L, ERR::NoFieldAccess, "Field does not exist: %s.%s", Obj->classptr ? Obj->classptr->ClassName: "?", strdata(Key));
   }

   // Call the field handler - it pushes result onto the Lua stack
   if (func->Call(L, *func, Obj) > 0) copyTV(L, Dest, L->top - 1);
   else setnilV(Dest);
   L->top = saved_top;
}

//********************************************************************************************************************
// Fast object field set - called from BC_OBSETF bytecode handler. Writes Val to the object field, or throws an error.

extern "C" void lj_object_sets(lua_State *L, GCobject *Obj, GCstr *Key, TValue *Val)
{
   // Ensure L->top is past the value register before any error can be thrown.
   // luaL_error pushes the error string to L->top, which would corrupt active registers if too low.

   const auto saved_top  = L->top;
   const auto stack_base = tvref(L->stack);
   const auto stack_end  = stack_base + L->stacksize;
   auto val_ptr    = Val;
   if ((Val < stack_base) or (Val >= stack_end)) {
      copyTV(L, L->top, Val);
      val_ptr = L->top;
      L->top++;
   }
   else if (L->top <= Val) L->top = Val + 1;

   if (not Obj->uid) luaL_error(L, ERR::DoesNotExist, "Object dereferenced, unable to write field.");

   // Use cached write_table or lazily populate it
   auto write_table = (WRITE_TABLE *)Obj->write_table;
   if (not write_table) {
      write_table = get_write_table(Obj->classptr);
      Obj->write_table = (void *)write_table;
   }

   // Look up the field handler using the string's precomputed hash
   auto hash_key = obj_write(Key->hash);
   auto func = write_table->find(hash_key);
   if (func IS write_table->end()) {
      luaL_error(L, ERR::UndefinedField, "Field does not exist: %s.%s", Obj->classptr ? Obj->classptr->ClassName: "?", strdata(Key));
   }

   if (auto pobj = access_object(Obj)) {
      auto stack_idx = int(val_ptr - L->base) + 1;
      ERR error = func->Call(L, pobj, func->Field, stack_idx);
      L->top = saved_top;
      release_object(Obj);

      if (error >= ERR::ExceptionThreshold) luaL_error(L, error);
   }
   else luaL_error(L, ERR::AccessObject);
}

//********************************************************************************************************************
// JIT field type lookup - returns the IR type for a field.  Returns -1 if field not found or unknown type.  This
// function must have no side effects as it is called during JIT recording.

extern "C" int ir_object_field_type(GCobject *Obj, GCstr *Key)
{
   if (not Obj->uid or not Obj->classptr) return -1;

   auto *mc = Obj->classptr;
   objMetaClass *src_class;
   Field *field;
   if (mc->findField(Key->hash, &field, &src_class) IS ERR::Okay) {
      auto flags = field->Flags;
      if (not (flags & FDF_R)) return -1;  // Not readable

      // NB: Order is important
      if (flags & FD_ARRAY) return IRT_ARRAY;
      if (flags & FD_STRING) return IRT_STR;
      if (flags & (FD_DOUBLE|FD_INT64)) return IRT_NUM;
      if (flags & (FD_OBJECT|FD_LOCAL)) return IRT_OBJECT;
      if (flags & FD_INT) {
         // FD_UNSIGNED always uses lua_pushnumber, even in DUALNUM builds
         if (flags & FD_UNSIGNED) return IRT_NUM;
         return LJ_DUALNUM ? IRT_INT : IRT_NUM;
      }
      if (flags & FD_STRUCT) {
         if (flags & FD_RESOURCE) return IRT_LIGHTUD;
         else return IRT_TAB;
      }
      if (flags & FD_POINTER) return IRT_LIGHTUD;
      return -1;  // Unknown type
   }

   return -1;  // Field not found in dictionary
}
