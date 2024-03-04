#pragma once
#define PARASOL_MAIN_H TRUE

#ifndef PLATFORM_CONFIG_H
#include <parasol/config.h> // Generated by cmake
#endif

#ifdef _MSC_VER
#pragma warning (disable : 4244 4311 4312 4267 4244 4068) // Disable annoying VC++ typecast warnings

#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

#ifdef __GNUC__
#define PACK(D) D __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK(D) __pragma(pack(push, 1)) D __pragma(pack(pop))
#endif

#include <parasol/system/types.h>
#include <parasol/vector.hpp>
#include <parasol/system/registry.h>
#include <parasol/system/errors.h>
#include <parasol/system/fields.h>
#include <parasol/modules/core.h>

#include <type_traits>
#include <memory>
#include <optional>

namespace pf {

template <class T>
class ScopedAccessMemory { // C++ wrapper for automatically releasing shared memory
   public:
      LONG id;
      T *ptr;
      ERROR error;

      ScopedAccessMemory(LONG ID, MEM Flags, LONG Milliseconds = 5000) {
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
// Defer() function for calling lambdas at end-of-scope

template <typename FUNC> struct deferred_call {
   deferred_call(const deferred_call& that) = delete;
   deferred_call& operator=(const deferred_call& that) = delete;
   deferred_call(deferred_call&& that) = delete;
   deferred_call(FUNC&& f) : func(std::forward<FUNC>(f)) { }

   ~deferred_call() { func(); }

private:
   FUNC func;
};

template <typename F> deferred_call<F> Defer(F&& f) {
   return deferred_call<F>(std::forward<F>(f));
}

//********************************************************************************************************************
// Deleter for use with std::unique_ptr to free objects correctly on destruction.  Note that these always assume that
// the object pointer remains safe (cannot be deleted by external factors).
//
// E.g. std::unique_ptr<objVectorViewport, DeleteObject<objVectorViewport>> viewport;

template <class T = BaseClass> struct DeleteObject {
  void operator()(T *Object) const { if (Object) FreeResource(Object->UID); }
};

// Simplify the creation of unique pointers with the destructor

template <class T = BaseClass> std::unique_ptr<T> make_unique_object(T *Object) {
   return std::unique_ptr<T>(Object, DeleteObject{});
}

// Variant for std::shared_ptr

template <class T = BaseClass> std::shared_ptr<T> make_shared_object(T *Object) {
   return std::shared_ptr<T>(Object, DeleteObject{});
}

//********************************************************************************************************************
// Scoped object locker.  Use granted() to confirm that the lock has been granted.

template <class T = BaseClass>
class ScopedObjectLock { // C++ wrapper for automatically releasing an object
   public:
      ERROR error;
      T *obj;

      ScopedObjectLock(OBJECTID ObjectID, LONG Milliseconds = 3000) {
         error = AccessObject(ObjectID, Milliseconds, (OBJECTPTR *)&obj);
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
// Resource guard for any allocation that can be freed with FreeResource().  Retains the resource ID rather than the
// pointer to ensure that termination is safe even if the original resource gets terminated elsewhere.
//
// Usage: pf::GuardedResource resource(thing)

template <class T>
class GuardedResource {
   private:
      MEMORYID id;
   public:
      GuardedResource(T Resource) {
         static_assert(std::is_pointer<T>::value, "The resource value must be a pointer");
         id = ((LONG *)Resource)[-2];
      }
      ~GuardedResource() { FreeResource(id); }
};

//********************************************************************************************************************
// The object equivalent of GuardedResource.  Also guarantees safety for object termination.

template <class T = BaseClass, class C = std::atomic_int>
class GuardedObject {
   private:
      C * count;  // Count of GuardedObjects accessing the same resource.  Can be LONG (non-threaded) or std::atomic_int
      T * object;

   public:
      OBJECTID id; // Object UID

      // Constructors

      GuardedObject() : count(new C(1)), object(NULL), id(0) { }

      GuardedObject(T *Object) : count(new C(1)), object(Object), id(Object->UID) {
         static_assert(std::is_base_of_v<BaseClass, T>, "The resource value must belong to BaseClass");
      }

      GuardedObject(const GuardedObject &other) { // Copy constructor
         if (other.object) {
            object = other.object;
            count  = other.count;
            count[0]++;
         }
         else { // If the other object is undefined then use a default state
            object = NULL;
            id     = 0;
            count  = new C(1);
         }
      }

      GuardedObject(GuardedObject &&other) { // Move constructor
         id     = other.id;
         object = other.object;
         count  = other.count;
         other.count = NULL;
      }

      // Destructor

      ~GuardedObject() {
         if (!count) return; // The count can be empty if this GuardedObject was moved

         if (!--count[0]) {
            if (id) FreeResource(id);
            delete count;
         }
      }

      // Assignments

      GuardedObject & operator = (const GuardedObject &other) { // Copy assignment
         if (this == &other) return *this;
         if (!--count[0]) delete count;
         if (other.object) {
            object = other.object;
            count  = other.count;
            count[0]++;
         }
         else { // If the other object is undefined then we reset our state with no count inheritance.
            object   = NULL;
            id       = 0;
            count[0] = 1;
         }
         return *this;
      }

      GuardedObject & operator = (GuardedObject &&other) { // Move assignment
         if (this == &other) return *this;
         if (!--count[0]) delete count;
         id     = other.id;
         object = other.object;
         count  = other.count;
         other.count = NULL;
         return *this;
      }

      // Public methods

      inline void set(T *Object) { // set() requires caution as the object reference is modified without adjusting the counter
         if (!Object) return;
         else if (count[0] IS 1) {
            object = Object;
            id     = Object->UID;
         }
         else { pf::Log log(__FUNCTION__); log.warning(ERR_InUse); }
      }

      constexpr bool empty() { return !object; }

      T * operator->() { return object; }; // Promotes underlying methods and fields
      T * & operator*() { return object; }; // To allow object pointer referencing when calling functions
};

//********************************************************************************************************************
// Resource guard for temporarily switching context and back when out of scope.
//
// Usage: pf::SwitchContext context(YourObject)

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
   using namespace pf;

constexpr FieldValue Path(CSTRING Value) { return FieldValue(FID_Path, Value); }
inline FieldValue Path(const std::string &Value) { return FieldValue(FID_Path, Value.c_str()); }

constexpr FieldValue Location(CSTRING Value) { return FieldValue(FID_Location, Value); }
inline FieldValue Location(const std::string &Value) { return FieldValue(FID_Location, Value.c_str()); }

constexpr FieldValue Args(CSTRING Value) { return FieldValue(FID_Args, Value); }
inline FieldValue Args(const std::string &Value) { return FieldValue(FID_Args, Value.c_str()); }

constexpr FieldValue Fill(CSTRING Value) { return FieldValue(FID_Fill, Value); }
inline FieldValue Fill(const std::string &Value) { return FieldValue(FID_Fill, Value.c_str()); }

constexpr FieldValue Statement(CSTRING Value) { return FieldValue(FID_Statement, Value); }
inline FieldValue Statement(const std::string &Value) { return FieldValue(FID_Statement, Value.c_str()); }

constexpr FieldValue Stroke(CSTRING Value) { return FieldValue(FID_Stroke, Value); }
inline FieldValue Stroke(const std::string &Value) { return FieldValue(FID_Stroke, Value.c_str()); }

constexpr FieldValue String(CSTRING Value) { return FieldValue(FID_String, Value); }
inline FieldValue String(const std::string &Value) { return FieldValue(FID_String, Value.c_str()); }

constexpr FieldValue Name(CSTRING Value) { return FieldValue(FID_Name, Value); }
inline FieldValue Name(const std::string &Value) { return FieldValue(FID_Name, Value.c_str()); }

constexpr FieldValue Allow(CSTRING Value) { return FieldValue(FID_Allow, Value); }
inline FieldValue Allow(const std::string &Value) { return FieldValue(FID_Allow, Value.c_str()); }

constexpr FieldValue Style(CSTRING Value) { return FieldValue(FID_Style, Value); }
inline FieldValue Style(const std::string &Value) { return FieldValue(FID_Style, Value.c_str()); }

constexpr FieldValue Face(CSTRING Value) { return FieldValue(FID_Face, Value); }
inline FieldValue Face(const std::string &Value) { return FieldValue(FID_Face, Value.c_str()); }

constexpr FieldValue FileExtension(CSTRING Value) { return FieldValue(FID_FileExtension, Value); }
inline FieldValue FileExtension(const std::string &Value) { return FieldValue(FID_FileExtension, Value.c_str()); }

constexpr FieldValue FileDescription(CSTRING Value) { return FieldValue(FID_FileDescription, Value); }
inline FieldValue FileDescription(const std::string &Value) { return FieldValue(FID_FileDescription, Value.c_str()); }

constexpr FieldValue FileHeader(CSTRING Value) { return FieldValue(FID_FileHeader, Value); }
inline FieldValue FileHeader(const std::string &Value) { return FieldValue(FID_FileHeader, Value.c_str()); }

constexpr FieldValue FontSize(DOUBLE Value) { return FieldValue(FID_FontSize, Value); }
constexpr FieldValue FontSize(LONG Value) { return FieldValue(FID_FontSize, Value); }
constexpr FieldValue FontSize(CSTRING Value) { return FieldValue(FID_FontSize, Value); }
inline FieldValue FontSize(const std::string &Value) { return FieldValue(FID_FontSize, Value.c_str()); }

constexpr FieldValue ArchiveName(CSTRING Value) { return FieldValue(FID_ArchiveName, Value); }
inline FieldValue ArchiveName(const std::string &Value) { return FieldValue(FID_ArchiveName, Value.c_str()); }

constexpr FieldValue Volume(CSTRING Value) { return FieldValue(FID_Volume, Value); }
inline FieldValue Volume(const std::string &Value) { return FieldValue(FID_Volume, Value.c_str()); }

constexpr FieldValue DPMS(CSTRING Value) { return FieldValue(FID_DPMS, Value); }
inline FieldValue DPMS(const std::string &Value) { return FieldValue(FID_DPMS, Value.c_str()); }

constexpr FieldValue Procedure(CSTRING Value) { return FieldValue(FID_Procedure, Value); }
inline FieldValue Procedure(const std::string &Value) { return FieldValue(FID_Procedure, Value.c_str()); }

constexpr FieldValue ReadOnly(LONG Value) { return FieldValue(FID_ReadOnly, Value); }
constexpr FieldValue ReadOnly(bool Value) { return FieldValue(FID_ReadOnly, (Value ? 1 : 0)); }

constexpr FieldValue ButtonOrder(CSTRING Value) { return FieldValue(FID_ButtonOrder, Value); }
inline FieldValue ButtonOrder(const std::string &Value) { return FieldValue(FID_ButtonOrder, Value.c_str()); }

constexpr FieldValue Point(DOUBLE Value) { return FieldValue(FID_Point, Value); }
constexpr FieldValue Point(LONG Value) { return FieldValue(FID_Point, Value); }
constexpr FieldValue Point(CSTRING Value) { return FieldValue(FID_Point, Value); }
inline FieldValue Point(const std::string &Value) { return FieldValue(FID_Point, Value.c_str()); }

constexpr FieldValue Points(CSTRING Value) { return FieldValue(FID_Points, Value); }
inline FieldValue Points(const std::string &Value) { return FieldValue(FID_Points, Value.c_str()); }

constexpr FieldValue Pretext(CSTRING Value) { return FieldValue(FID_Pretext, Value); }
inline FieldValue Pretext(const std::string &Value) { return FieldValue(FID_Pretext, Value.c_str()); }

constexpr FieldValue Acceleration(DOUBLE Value) { return FieldValue(FID_Acceleration, Value); }
constexpr FieldValue Actions(CPTR Value) { return FieldValue(FID_Actions, Value); }
constexpr FieldValue AmtColours(LONG Value) { return FieldValue(FID_AmtColours, Value); }
constexpr FieldValue BaseClassID(LONG Value) { return FieldValue(FID_BaseClassID, Value); }
constexpr FieldValue Bitmap(objBitmap *Value) { return FieldValue(FID_Bitmap, Value); }
constexpr FieldValue BitsPerPixel(LONG Value) { return FieldValue(FID_BitsPerPixel, Value); }
constexpr FieldValue BytesPerPixel(LONG Value) { return FieldValue(FID_BytesPerPixel, Value); }
constexpr FieldValue Category(CCF Value) { return FieldValue(FID_Category, LONG(Value)); }
constexpr FieldValue ClassID(LONG Value) { return FieldValue(FID_ClassID, Value); }
constexpr FieldValue ClassVersion(DOUBLE Value) { return FieldValue(FID_ClassVersion, Value); }
constexpr FieldValue Closed(bool Value) { return FieldValue(FID_Closed, (Value ? 1 : 0)); }
constexpr FieldValue Cursor(PTC Value) { return FieldValue(FID_Cursor, LONG(Value)); }
constexpr FieldValue DataFlags(MEM Value) { return FieldValue(FID_DataFlags, LONG(Value)); }
constexpr FieldValue DoubleClick(DOUBLE Value) { return FieldValue(FID_DoubleClick, Value); }
constexpr FieldValue Feedback(CPTR Value) { return FieldValue(FID_Feedback, Value); }
constexpr FieldValue Fields(const FieldArray *Value) { return FieldValue(FID_Fields, Value, FD_ARRAY); }
constexpr FieldValue Flags(LONG Value) { return FieldValue(FID_Flags, Value); }
constexpr FieldValue Font(OBJECTPTR Value) { return FieldValue(FID_Font, Value); }
constexpr FieldValue HostScene(OBJECTPTR Value) { return FieldValue(FID_HostScene, Value); }
constexpr FieldValue Incoming(CPTR Value) { return FieldValue(FID_Incoming, Value); }
constexpr FieldValue Input(CPTR Value) { return FieldValue(FID_Input, Value); }
constexpr FieldValue LineLimit(LONG Value) { return FieldValue(FID_LineLimit, Value); }
constexpr FieldValue Listener(LONG Value) { return FieldValue(FID_Listener, Value); }
constexpr FieldValue MatrixColumns(LONG Value) { return FieldValue(FID_MatrixColumns, Value); }
constexpr FieldValue MatrixRows(LONG Value) { return FieldValue(FID_MatrixRows, Value); }
constexpr FieldValue MaxHeight(LONG Value) { return FieldValue(FID_MaxHeight, Value); }
constexpr FieldValue MaxSpeed(DOUBLE Value) { return FieldValue(FID_MaxSpeed, Value); }
constexpr FieldValue MaxWidth(LONG Value) { return FieldValue(FID_MaxWidth, Value); }
constexpr FieldValue Methods(const MethodEntry *Value) { return FieldValue(FID_Methods, Value, FD_ARRAY); }
constexpr FieldValue Opacity(DOUBLE Value) { return FieldValue(FID_Opacity, Value); }
constexpr FieldValue Owner(OBJECTID Value) { return FieldValue(FID_Owner, Value); }
constexpr FieldValue Parent(OBJECTID Value) { return FieldValue(FID_Parent, Value); }
constexpr FieldValue Permissions(PERMIT Value) { return FieldValue(FID_Permissions, LONG(Value)); }
constexpr FieldValue Picture(OBJECTPTR Value) { return FieldValue(FID_Picture, Value); }
constexpr FieldValue PopOver(OBJECTID Value) { return FieldValue(FID_PopOver, Value); }
constexpr FieldValue RefreshRate(DOUBLE Value) { return FieldValue(FID_RefreshRate, Value); }
constexpr FieldValue Routine(CPTR Value) { return FieldValue(FID_Routine, Value); }
constexpr FieldValue Size(LONG Value) { return FieldValue(FID_Size, Value); }
constexpr FieldValue Speed(DOUBLE Value) { return FieldValue(FID_Speed, Value); }
constexpr FieldValue StrokeWidth(DOUBLE Value) { return FieldValue(FID_StrokeWidth, Value); }
constexpr FieldValue Surface(OBJECTID Value) { return FieldValue(FID_Surface, Value); }
constexpr FieldValue Target(OBJECTID Value) { return FieldValue(FID_Target, Value); }
constexpr FieldValue Target(OBJECTPTR Value) { return FieldValue(FID_Target, Value); }
constexpr FieldValue UserData(CPTR Value) { return FieldValue(FID_UserData, Value); }
constexpr FieldValue Version(DOUBLE Value) { return FieldValue(FID_Version, Value); }
constexpr FieldValue Viewport(OBJECTID Value) { return FieldValue(FID_Viewport, Value); }
constexpr FieldValue Viewport(OBJECTPTR Value) { return FieldValue(FID_Viewport, Value); }
constexpr FieldValue WheelSpeed(DOUBLE Value) { return FieldValue(FID_WheelSpeed, Value); }
constexpr FieldValue WindowHandle(APTR Value) { return FieldValue(FID_WindowHandle, Value); }
constexpr FieldValue WindowHandle(LONG Value) { return FieldValue(FID_WindowHandle, Value); }

// Template-based Flags are required for strongly typed enums

template <class T> FieldValue Type(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_enum<T>::value, "Type value must be numeric");
   return FieldValue(FID_Type, LONG(Value));
}

template <class T> FieldValue AspectRatio(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_enum<T>::value, "AspectRatio value must be numeric");
   return FieldValue(FID_AspectRatio, LONG(Value));
}

template <class T> FieldValue ColourSpace(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_enum<T>::value, "ColourSpace value must be numeric");
   return FieldValue(FID_ColourSpace, LONG(Value));
}

template <class T> FieldValue Flags(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_enum<T>::value, "Flags value must be numeric");
   return FieldValue(FID_Flags, LONG(Value));
}

template <class T> FieldValue Units(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_enum<T>::value, "Units value must be numeric");
   return FieldValue(FID_Units, LONG(Value));
}

template <class T> FieldValue SpreadMethod(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_enum<T>::value, "SpreadMethod value must be numeric");
   return FieldValue(FID_SpreadMethod, LONG(Value));
}

template <class T> FieldValue Visibility(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_enum<T>::value, "Visibility value must be numeric");
   return FieldValue(FID_Visibility, LONG(Value));
}

template <class T> FieldValue PageWidth(T Value) {
   static_assert(std::is_arithmetic<T>::value, "PageWidth value must be numeric");
   return FieldValue(FID_PageWidth, Value);
}

template <class T> FieldValue PageHeight(T Value) {
   static_assert(std::is_arithmetic<T>::value, "PageHeight value must be numeric");
   return FieldValue(FID_PageHeight, Value);
}

template <class T> FieldValue Radius(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "Radius value must be numeric");
   return FieldValue(FID_Radius, Value);
}

template <class T> FieldValue CenterX(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "CenterX value must be numeric");
   return FieldValue(FID_CenterX, Value);
}

template <class T> FieldValue CenterY(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "CenterY value must be numeric");
   return FieldValue(FID_CenterY, Value);
}

template <class T> FieldValue FX(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "FX value must be numeric");
   return FieldValue(FID_FX, Value);
}

template <class T> FieldValue FY(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "FY value must be numeric");
   return FieldValue(FID_FY, Value);
}

template <class T> FieldValue ResX(T Value) {
   static_assert(std::is_arithmetic<T>::value, "ResX value must be numeric");
   return FieldValue(FID_ResX, Value);
}

template <class T> FieldValue ResY(T Value) {
   static_assert(std::is_arithmetic<T>::value, "ResY value must be numeric");
   return FieldValue(FID_ResY, Value);
}

template <class T> FieldValue ViewX(T Value) {
   static_assert(std::is_arithmetic<T>::value, "ViewX value must be numeric");
   return FieldValue(FID_ViewX, Value);
}

template <class T> FieldValue ViewY(T Value) {
   static_assert(std::is_arithmetic<T>::value, "ViewY value must be numeric");
   return FieldValue(FID_ViewY, Value);
}

template <class T> FieldValue ViewWidth(T Value) {
   static_assert(std::is_arithmetic<T>::value, "ViewWidth value must be numeric");
   return FieldValue(FID_ViewWidth, Value);
}

template <class T> FieldValue ViewHeight(T Value) {
   static_assert(std::is_arithmetic<T>::value, "ViewHeight value must be numeric");
   return FieldValue(FID_ViewHeight, Value);
}

template <class T> FieldValue Width(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "Width value must be numeric");
   return FieldValue(FID_Width, Value);
}

template <class T> FieldValue Height(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "Height value must be numeric");
   return FieldValue(FID_Height, Value);
}

template <class T> FieldValue X(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "X value must be numeric");
   return FieldValue(FID_X, Value);
}

template <class T> FieldValue XOffset(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "XOffset value must be numeric");
   return FieldValue(FID_XOffset, Value);
}

template <class T> FieldValue Y(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "Y value must be numeric");
   return FieldValue(FID_Y, Value);
}

template <class T> FieldValue YOffset(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "YOffset value must be numeric");
   return FieldValue(FID_YOffset, Value);
}

template <class T> FieldValue X1(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "X1 value must be numeric");
   return FieldValue(FID_X1, Value);
}

template <class T> FieldValue Y1(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "Y1 value must be numeric");
   return FieldValue(FID_Y1, Value);
}

template <class T> FieldValue X2(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "X2 value must be numeric");
   return FieldValue(FID_X2, Value);
}

template <class T> FieldValue Y2(T Value) {
   static_assert(std::is_arithmetic<T>::value || std::is_base_of_v<SCALE, T>, "Y2 value must be numeric");
   return FieldValue(FID_Y2, Value);
}

}
