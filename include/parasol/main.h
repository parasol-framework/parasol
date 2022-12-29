#pragma once
#define PARASOL_MAIN_H TRUE

#ifndef PLATFORM_CONFIG_H
#include <parasol/config.h> // Generated by cmake
#endif

#ifdef _MSC_VER
#pragma warning (disable : 4244 4311 4312) // Disable annoying VC++ typecast warnings
#endif

#include <parasol/system/types.h>
#include <parasol/system/registry.h>
#include <parasol/system/errors.h>
#include <parasol/system/fields.h>
#include <parasol/modules/core.h>

#include <memory>
#include <optional>

namespace parasol {

template <class T>
class ScopedAccessMemory { // C++ wrapper for automatically releasing shared memory
   public:
      LONG id;
      T *ptr;
      ERROR error;

      ScopedAccessMemory(LONG ID, LONG Flags, LONG Milliseconds = 5000) {
         id = ID;
         error = AccessMemory(ID, Flags, Milliseconds, (APTR *)&ptr);
      }

      ~ScopedAccessMemory() { if (!error) ReleaseMemory(ptr); }

      bool granted() { return error == ERR_Okay; }

      void release() {
         if (!error) {
            ReleaseMemory(ptr);
            error = ERR_NotLocked;
         }
      }
};

//********************************************************************************************************************

template <class T = BaseClass>
class ScopedObject { // C++ wrapper for automatically freeing an object
   public:
      T *obj;

      ScopedObject(T *Object) { obj = Object; }
      ScopedObject() { obj = NULL; }
      ~ScopedObject() { if (obj) acFree(obj); }
};

//********************************************************************************************************************
// Scoped object locker.  Use granted() to confirm that the lock has been granted.

template <class T = BaseClass>
class ScopedObjectLock { // C++ wrapper for automatically releasing an object
   public:
      ERROR error;
      T *obj;

      ScopedObjectLock(OBJECTID ObjectID, LONG Milliseconds = 3000) {
         error = AccessObject(ObjectID, Milliseconds, &obj);
      }

      ScopedObjectLock(OBJECTPTR Object, LONG Milliseconds = 3000) {
         error = AccessPrivateObject(Object, Milliseconds);
         obj = (T *)Object;
      }

      ScopedObjectLock() { obj = NULL; error = ERR_NotLocked; }
      ~ScopedObjectLock() { if (!error) ReleaseObject((OBJECTPTR)obj); }
      bool granted() { return error == ERR_Okay; }
};

//********************************************************************************************************************

class ScopedSysLock { // C++ wrapper for terminating a system lock when scope is lost
   private:
      LONG index;

   public:
      ERROR error; // ERR_Okay is used to indicate that the lock is acquired

      ScopedSysLock(LONG Index, LONG Milliseconds) {
         error = SysLock(Index, Milliseconds);
         index = Index;
      }

      ~ScopedSysLock() { if (!error) SysUnlock(index); }

      bool granted() { return error == ERR_Okay; }

      void release() {
         if (!error) {
            SysUnlock(index);
            error = ERR_NotLocked;
         }
      }

      ERROR acquire(LONG Milliseconds) {
         if (error) error = SysLock(index, Milliseconds);
         return error;
      }
};

//********************************************************************************************************************
// Resource guard for any allocation that can be freed with FreeResource()
//
// Usage: parasol::GuardedResource resource(thing)

template <class T>
class GuardedResource { // C++ wrapper for terminating resources when scope is lost
   private:
      APTR resource;
   public:
      GuardedResource(T Resource) { resource = Resource; }
      ~GuardedResource() { FreeResource(resource); }
};

//********************************************************************************************************************
// Resource guard for temporarily switching context and back when out of scope.
//
// Usage: parasol::SwitchContext context(YourObject)

template <class T>
class SwitchContext { // C++ wrapper for changing the current context with a resource guard in place
   private:
      OBJECTPTR old_context;
   public:
      SwitchContext(T NewContext) {
         if (NewContext) old_context = SetContext((OBJECTPTR)NewContext);
         else old_context = NULL;
      }
      ~SwitchContext() { if (old_context) SetContext(old_context); }
};

} // namespace

//********************************************************************************************************************
// These field name and type declarations help to ensure that fields are paired with the correct type during create().

namespace fl {
   using namespace parasol;

constexpr FieldValue Path(CSTRING Value) { return FieldValue(FID_Path, Value); }
inline FieldValue Path(std::string Value) { return FieldValue(FID_Path, Value.c_str()); }
constexpr FieldValue Volume(CSTRING Value) { return FieldValue(FID_Volume, Value); }
inline FieldValue Volume(std::string Value) { return FieldValue(FID_Volume, Value.c_str()); }
constexpr FieldValue Flags(LONG Value) { return FieldValue(FID_Flags, Value); }
constexpr FieldValue Permissions(LONG Value) { return FieldValue(FID_Permissions, Value); }
constexpr FieldValue Routine(CPTR Value) { return FieldValue(FID_Routine, Value); }
constexpr FieldValue BaseClassID(LONG Value) { return FieldValue(FID_BaseClassID, Value); }
constexpr FieldValue SubClassID(LONG Value) { return FieldValue(FID_SubClassID, Value); }
constexpr FieldValue ClassVersion(DOUBLE Value) { return FieldValue(FID_ClassVersion, Value); }
constexpr FieldValue Version(DOUBLE Value) { return FieldValue(FID_Version, Value); }
constexpr FieldValue Name(CSTRING Value) { return FieldValue(FID_Name, Value); }
inline FieldValue Name(std::string Value) { return FieldValue(FID_Name, Value.c_str()); }
constexpr FieldValue Category(LONG Value) { return FieldValue(FID_Category, Value); }
constexpr FieldValue FileExtension(CSTRING Value) { return FieldValue(FID_FileExtension, Value); }
inline FieldValue FileExtension(std::string Value) { return FieldValue(FID_FileExtension, Value.c_str()); }
constexpr FieldValue FileDescription(CSTRING Value) { return FieldValue(FID_FileDescription, Value); }
inline FieldValue FileDescription(std::string Value) { return FieldValue(FID_FileDescription, Value.c_str()); }
constexpr FieldValue FileHeader(CSTRING Value) { return FieldValue(FID_FileHeader, Value); }
inline FieldValue FileHeader(std::string Value) { return FieldValue(FID_FileHeader, Value.c_str()); }
constexpr FieldValue Actions(CPTR Value) { return FieldValue(FID_Actions, Value); }
constexpr FieldValue Size(LONG Value) { return FieldValue(FID_Size, Value); }
constexpr FieldValue Methods(const MethodArray *Value) { return FieldValue(FID_Methods, Value, FD_ARRAY); }
constexpr FieldValue Fields(const FieldArray *Value) { return FieldValue(FID_Fields, Value, FD_ARRAY); }
constexpr FieldValue ArchiveName(CSTRING Value) { return FieldValue(FID_ArchiveName, Value); }
inline FieldValue ArchiveName(std::string Value) { return FieldValue(FID_ArchiveName, Value.c_str()); }

}

//********************************************************************************************************************

inline ERROR LoadModule(CSTRING Name, DOUBLE Version, OBJECTPTR *Module, APTR Functions) {
   if (auto module = objModule::create::global(fl::Name(Name), fl::Version(Version))) {
      APTR functionbase;
      if (!module->getPtr(FID_ModBase, &functionbase)) {
         if (Module) *Module = module;
         if (Functions) ((APTR *)Functions)[0] = functionbase;
         return ERR_Okay;
      }
      else return ERR_GetField;
   }
   else return ERR_CreateObject;
}
