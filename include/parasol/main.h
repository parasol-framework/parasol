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

#include <type_traits>
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
         error = AccessMemoryID(ID, Flags, Milliseconds, (APTR *)&ptr);
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
         error = AccessObjectID(ObjectID, Milliseconds, &obj);
      }

      ScopedObjectLock(OBJECTPTR Object, LONG Milliseconds = 3000) {
         error = LockObject(Object, Milliseconds);
         obj = (T *)Object;
      }

      ScopedObjectLock() { obj = NULL; error = ERR_NotLocked; }
      ~ScopedObjectLock() { if (!error) ReleaseObject((OBJECTPTR)obj); }
      bool granted() { return error == ERR_Okay; }

      T * operator->() { return obj; }; // Promotes underlying methods and fields
      T * & operator*() { return obj; }; // To allow object pointer referencing when calling functions
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

class objBitmap;

namespace fl {
   using namespace parasol;

constexpr FieldValue Path(CSTRING Value) { return FieldValue(FID_Path, Value); }
inline FieldValue Path(std::string Value) { return FieldValue(FID_Path, Value.c_str()); }

constexpr FieldValue Location(CSTRING Value) { return FieldValue(FID_Location, Value); }
inline FieldValue Location(std::string Value) { return FieldValue(FID_Location, Value.c_str()); }

constexpr FieldValue Args(CSTRING Value) { return FieldValue(FID_Args, Value); }
inline FieldValue Args(std::string Value) { return FieldValue(FID_Args, Value.c_str()); }

constexpr FieldValue Statement(CSTRING Value) { return FieldValue(FID_Statement, Value); }
inline FieldValue Statement(std::string Value) { return FieldValue(FID_Statement, Value.c_str()); }

constexpr FieldValue Stroke(CSTRING Value) { return FieldValue(FID_Stroke, Value); }
inline FieldValue Stroke(std::string Value) { return FieldValue(FID_Stroke, Value.c_str()); }

constexpr FieldValue String(CSTRING Value) { return FieldValue(FID_String, Value); }
inline FieldValue String(std::string Value) { return FieldValue(FID_String, Value.c_str()); }

constexpr FieldValue Name(CSTRING Value) { return FieldValue(FID_Name, Value); }
inline FieldValue Name(std::string Value) { return FieldValue(FID_Name, Value.c_str()); }

constexpr FieldValue Allow(CSTRING Value) { return FieldValue(FID_Allow, Value); }
inline FieldValue Allow(std::string Value) { return FieldValue(FID_Allow, Value.c_str()); }

constexpr FieldValue Style(CSTRING Value) { return FieldValue(FID_Style, Value); }
inline FieldValue Style(std::string Value) { return FieldValue(FID_Style, Value.c_str()); }

constexpr FieldValue Face(CSTRING Value) { return FieldValue(FID_Face, Value); }
inline FieldValue Face(std::string Value) { return FieldValue(FID_Face, Value.c_str()); }

constexpr FieldValue FileExtension(CSTRING Value) { return FieldValue(FID_FileExtension, Value); }
inline FieldValue FileExtension(std::string Value) { return FieldValue(FID_FileExtension, Value.c_str()); }

constexpr FieldValue FileDescription(CSTRING Value) { return FieldValue(FID_FileDescription, Value); }
inline FieldValue FileDescription(std::string Value) { return FieldValue(FID_FileDescription, Value.c_str()); }

constexpr FieldValue FileHeader(CSTRING Value) { return FieldValue(FID_FileHeader, Value); }
inline FieldValue FileHeader(std::string Value) { return FieldValue(FID_FileHeader, Value.c_str()); }

constexpr FieldValue ArchiveName(CSTRING Value) { return FieldValue(FID_ArchiveName, Value); }
inline FieldValue ArchiveName(std::string Value) { return FieldValue(FID_ArchiveName, Value.c_str()); }

constexpr FieldValue Volume(CSTRING Value) { return FieldValue(FID_Volume, Value); }
inline FieldValue Volume(std::string Value) { return FieldValue(FID_Volume, Value.c_str()); }

constexpr FieldValue DPMS(CSTRING Value) { return FieldValue(FID_DPMS, Value); }
inline FieldValue DPMS(std::string Value) { return FieldValue(FID_DPMS, Value.c_str()); }

constexpr FieldValue ReadOnly(LONG Value) { return FieldValue(FID_ReadOnly, Value); }
constexpr FieldValue ReadOnly(bool Value) { return FieldValue(FID_ReadOnly, (Value ? 1 : 0)); }

constexpr FieldValue Point(DOUBLE Value) { return FieldValue(FID_Point, Value); }
constexpr FieldValue Point(LONG Value) { return FieldValue(FID_Point, Value); }
constexpr FieldValue Point(CSTRING Value) { return FieldValue(FID_Point, Value); }
inline FieldValue Point(std::string Value) { return FieldValue(FID_Point, Value.c_str()); }

constexpr FieldValue Owner(OBJECTID Value) { return FieldValue(FID_Owner, Value); }
constexpr FieldValue Target(OBJECTID Value) { return FieldValue(FID_Target, Value); }
constexpr FieldValue Flags(LONG Value) { return FieldValue(FID_Flags, Value); }
constexpr FieldValue Listener(LONG Value) { return FieldValue(FID_Listener, Value); }
constexpr FieldValue Permissions(LONG Value) { return FieldValue(FID_Permissions, Value); }
constexpr FieldValue UserData(CPTR Value) { return FieldValue(FID_UserData, Value); }
constexpr FieldValue Routine(CPTR Value) { return FieldValue(FID_Routine, Value); }
constexpr FieldValue Feedback(CPTR Value) { return FieldValue(FID_Feedback, Value); }
constexpr FieldValue Incoming(CPTR Value) { return FieldValue(FID_Incoming, Value); }
constexpr FieldValue BaseClassID(LONG Value) { return FieldValue(FID_BaseClassID, Value); }
constexpr FieldValue SubClassID(LONG Value) { return FieldValue(FID_SubClassID, Value); }
constexpr FieldValue AmtColours(LONG Value) { return FieldValue(FID_AmtColours, Value); }
constexpr FieldValue ClassVersion(DOUBLE Value) { return FieldValue(FID_ClassVersion, Value); }
constexpr FieldValue Version(DOUBLE Value) { return FieldValue(FID_Version, Value); }
constexpr FieldValue Category(LONG Value) { return FieldValue(FID_Category, Value); }
constexpr FieldValue Actions(CPTR Value) { return FieldValue(FID_Actions, Value); }
constexpr FieldValue Size(LONG Value) { return FieldValue(FID_Size, Value); }
constexpr FieldValue Methods(const MethodArray *Value) { return FieldValue(FID_Methods, Value, FD_ARRAY); }
constexpr FieldValue Fields(const FieldArray *Value) { return FieldValue(FID_Fields, Value, FD_ARRAY); }
constexpr FieldValue Bitmap(objBitmap *Value) { return FieldValue(FID_Bitmap, Value); }
constexpr FieldValue SpreadMethod(LONG Value) { return FieldValue(FID_SpreadMethod, Value); }
constexpr FieldValue Units(LONG Value) { return FieldValue(FID_Units, Value); }
constexpr FieldValue AspectRatio(LONG Value) { return FieldValue(FID_AspectRatio, Value); }
constexpr FieldValue ColourSpace(LONG Value) { return FieldValue(FID_ColourSpace, Value); }
constexpr FieldValue WindowHandle(LONG Value) { return FieldValue(FID_WindowHandle, Value); }
constexpr FieldValue WindowHandle(APTR Value) { return FieldValue(FID_WindowHandle, Value); }
constexpr FieldValue StrokeWidth(DOUBLE Value) { return FieldValue(FID_StrokeWidth, Value); }
constexpr FieldValue Closed(bool Value) { return FieldValue(FID_Closed, (Value ? 1 : 0)); }
constexpr FieldValue Visibility(LONG Value) { return FieldValue(FID_Visibility, Value); }
constexpr FieldValue Input(CPTR Value) { return FieldValue(FID_Input, Value); }
constexpr FieldValue Picture(OBJECTPTR Value) { return FieldValue(FID_Picture, Value); }
constexpr FieldValue BitsPerPixel(LONG Value) { return FieldValue(FID_BitsPerPixel, Value); }
constexpr FieldValue BytesPerPixel(LONG Value) { return FieldValue(FID_BytesPerPixel, Value); }
constexpr FieldValue DataFlags(LONG Value) { return FieldValue(FID_DataFlags, Value); }
constexpr FieldValue RefreshRate(DOUBLE Value) { return FieldValue(FID_RefreshRate, Value); }
constexpr FieldValue Opacity(DOUBLE Value) { return FieldValue(FID_Opacity, Value); }
constexpr FieldValue PopOver(OBJECTID Value) { return FieldValue(FID_PopOver, Value); }
constexpr FieldValue Parent(OBJECTID Value) { return FieldValue(FID_Parent, Value); }
constexpr FieldValue MaxWidth(LONG Value) { return FieldValue(FID_MaxWidth, Value); }
constexpr FieldValue MaxHeight(LONG Value) { return FieldValue(FID_MaxHeight, Value); }

template <class T> FieldValue PageWidth(T Value) {
   static_assert(std::is_arithmetic<T>::value, "PageWidth value must be numeric");
   return FieldValue(FID_PageWidth, Value);
}

template <class T> FieldValue PageHeight(T Value) {
   static_assert(std::is_arithmetic<T>::value, "PageHeight value must be numeric");
   return FieldValue(FID_PageHeight, Value);
}

template <class T> FieldValue Width(T Value) {
   static_assert(std::is_arithmetic<T>::value, "Width value must be numeric");
   return FieldValue(FID_Width, Value);
}

template <class T> FieldValue Height(T Value) {
   static_assert(std::is_arithmetic<T>::value, "Height value must be numeric");
   return FieldValue(FID_Height, Value);
}

template <class T> FieldValue X(T Value) {
   static_assert(std::is_arithmetic<T>::value, "X value must be numeric");
   return FieldValue(FID_X, Value);
}

template <class T> FieldValue Y(T Value) {
   static_assert(std::is_arithmetic<T>::value, "Y value must be numeric");
   return FieldValue(FID_Y, Value);
}

template <class T> FieldValue X1(T Value) {
   static_assert(std::is_arithmetic<T>::value, "X1 value must be numeric");
   return FieldValue(FID_X1, Value);
}

template <class T> FieldValue Y1(T Value) {
   static_assert(std::is_arithmetic<T>::value, "Y1 value must be numeric");
   return FieldValue(FID_Y1, Value);
}

template <class T> FieldValue X2(T Value) {
   static_assert(std::is_arithmetic<T>::value, "X2 value must be numeric");
   return FieldValue(FID_X2, Value);
}

template <class T> FieldValue Y2(T Value) {
   static_assert(std::is_arithmetic<T>::value, "Y2 value must be numeric");
   return FieldValue(FID_Y2, Value);
}

}
