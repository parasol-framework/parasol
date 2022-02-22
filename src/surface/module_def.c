// Auto-generated by idl-c.fluid

#ifdef  __cplusplus
extern "C" {
#endif

static ERROR drwGetSurfaceInfo(OBJECTID Surface, struct SurfaceInfoV2 ** Info);
static ERROR drwLockBitmap(OBJECTID Surface, struct rkBitmap ** Bitmap, LONG * Info);
static ERROR drwUnlockBitmap(OBJECTID Surface, struct rkBitmap * Bitmap);
static ERROR drwExposeSurface(OBJECTID Surface, LONG X, LONG Y, LONG Width, LONG Height, LONG Flags);
static ERROR drwCopySurface(OBJECTID Surface, struct rkBitmap * Bitmap, LONG Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
static struct SurfaceControl * drwAccessList(LONG Flags);
static void drwReleaseList(LONG Flags);
static OBJECTID drwSetModalSurface(OBJECTID Surface);
static OBJECTID drwGetUserFocus();
static void drwForbidExpose();
static void drwPermitExpose();
static void drwForbidDrawing();
static void drwPermitDrawing();
static ERROR drwGetSurfaceCoords(OBJECTID Surface, LONG * X, LONG * Y, LONG * AbsX, LONG * AbsY, LONG * Width, LONG * Height);
static OBJECTID drwGetModalSurface(OBJECTID Task);
static ERROR drwGetSurfaceFlags(OBJECTID Surface, LONG * Flags);
static ERROR drwGetVisibleArea(OBJECTID Surface, LONG * X, LONG * Y, LONG * AbsX, LONG * AbsY, LONG * Width, LONG * Height);
static ERROR drwCheckIfChild(OBJECTID Parent, OBJECTID Child);
static ERROR drwApplyStyleValues(OBJECTPTR Object, CSTRING Name);
static ERROR drwApplyStyleGraphics(OBJECTPTR Object, OBJECTID Target, CSTRING StyleName, CSTRING StyleType);
static ERROR drwSetCurrentStyle(CSTRING Path);

#ifdef  __cplusplus
}
#endif
#ifndef FDEF
#define FDEF static const struct FunctionField
#endif

FDEF argsAccessList[] = { { "SurfaceControl", FD_PTR|FD_STRUCT }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF argsApplyStyleGraphics[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Target", FD_OBJECTID }, { "StyleName", FD_STR }, { "StyleType", FD_STR }, { 0, 0 } };
FDEF argsApplyStyleValues[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Name", FD_STR }, { 0, 0 } };
FDEF argsCheckIfChild[] = { { "Error", FD_LONG|FD_ERROR }, { "Parent", FD_OBJECTID }, { "Child", FD_OBJECTID }, { 0, 0 } };
FDEF argsCopySurface[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "Bitmap", FD_OBJECTPTR }, { "Flags", FD_LONG }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { "XDest", FD_LONG }, { "YDest", FD_LONG }, { 0, 0 } };
FDEF argsExposeSurface[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF argsForbidDrawing[] = { { "Void", FD_VOID }, { 0, 0 } };
FDEF argsForbidExpose[] = { { "Void", FD_VOID }, { 0, 0 } };
FDEF argsGetModalSurface[] = { { "Result", FD_OBJECTID }, { "Task", FD_OBJECTID }, { 0, 0 } };
FDEF argsGetSurfaceCoords[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "X", FD_LONG|FD_RESULT }, { "Y", FD_LONG|FD_RESULT }, { "AbsX", FD_LONG|FD_RESULT }, { "AbsY", FD_LONG|FD_RESULT }, { "Width", FD_LONG|FD_RESULT }, { "Height", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsGetSurfaceFlags[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "Flags", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsGetSurfaceInfo[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "SurfaceInfo:Info", FD_PTR|FD_STRUCT|FD_RESULT }, { 0, 0 } };
FDEF argsGetUserFocus[] = { { "Result", FD_OBJECTID }, { 0, 0 } };
FDEF argsGetVisibleArea[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "X", FD_LONG|FD_RESULT }, { "Y", FD_LONG|FD_RESULT }, { "AbsX", FD_LONG|FD_RESULT }, { "AbsY", FD_LONG|FD_RESULT }, { "Width", FD_LONG|FD_RESULT }, { "Height", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsLockBitmap[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "Bitmap", FD_OBJECTPTR|FD_RESULT }, { "Info", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsPermitDrawing[] = { { "Void", FD_VOID }, { 0, 0 } };
FDEF argsPermitExpose[] = { { "Void", FD_VOID }, { 0, 0 } };
FDEF argsReleaseList[] = { { "Void", FD_VOID }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF argsSetCurrentStyle[] = { { "Error", FD_LONG|FD_ERROR }, { "Path", FD_STR }, { 0, 0 } };
FDEF argsSetModalSurface[] = { { "Result", FD_OBJECTID }, { "Surface", FD_OBJECTID }, { 0, 0 } };
FDEF argsUnlockBitmap[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "Bitmap", FD_OBJECTPTR }, { 0, 0 } };

const struct Function glFunctions[] = {
   { (APTR)drwGetSurfaceInfo, "GetSurfaceInfo", argsGetSurfaceInfo },
   { (APTR)drwLockBitmap, "LockBitmap", argsLockBitmap },
   { (APTR)drwUnlockBitmap, "UnlockBitmap", argsUnlockBitmap },
   { (APTR)drwExposeSurface, "ExposeSurface", argsExposeSurface },
   { (APTR)drwCopySurface, "CopySurface", argsCopySurface },
   { (APTR)drwAccessList, "AccessList", argsAccessList },
   { (APTR)drwReleaseList, "ReleaseList", argsReleaseList },
   { (APTR)drwSetModalSurface, "SetModalSurface", argsSetModalSurface },
   { (APTR)drwGetUserFocus, "GetUserFocus", argsGetUserFocus },
   { (APTR)drwForbidExpose, "ForbidExpose", argsForbidExpose },
   { (APTR)drwPermitExpose, "PermitExpose", argsPermitExpose },
   { (APTR)drwForbidDrawing, "ForbidDrawing", argsForbidDrawing },
   { (APTR)drwPermitDrawing, "PermitDrawing", argsPermitDrawing },
   { (APTR)drwGetSurfaceCoords, "GetSurfaceCoords", argsGetSurfaceCoords },
   { (APTR)drwGetModalSurface, "GetModalSurface", argsGetModalSurface },
   { (APTR)drwGetSurfaceFlags, "GetSurfaceFlags", argsGetSurfaceFlags },
   { (APTR)drwGetVisibleArea, "GetVisibleArea", argsGetVisibleArea },
   { (APTR)drwCheckIfChild, "CheckIfChild", argsCheckIfChild },
   { (APTR)drwApplyStyleValues, "ApplyStyleValues", argsApplyStyleValues },
   { (APTR)drwApplyStyleGraphics, "ApplyStyleGraphics", argsApplyStyleGraphics },
   { (APTR)drwSetCurrentStyle, "SetCurrentStyle", argsSetCurrentStyle },
   { NULL, NULL, NULL }
};

#undef MOD_IDL
#define MOD_IDL "s.SurfaceControl:lListIndex,lArrayIndex,lEntrySize,lTotal,lArraySize\ns.SurfaceInfo:lParentID,lBitmapID,lDataMID,lDisplayID,lFlags,lX,lY,lWidth,lHeight,lAbsX,lAbsY,wLevel,cBitsPerPixel,cBytesPerPixel,lLineWidth\ns.SurfaceList:lParentID,lSurfaceID,lBitmapID,lDisplayID,lDataMID,lTaskID,lRootID,lPopOverID,lFlags,lX,lY,lWidth,lHeight,lLeft,lRight,lBottom,lTop,wLevel,wLineWidth,cBytesPerPixel,cBitsPerPixel,cCursor,ucOpacity\ns.SurfaceCoords:lX,lY,lWidth,lHeight,lAbsX,lAbsY\nc.ARF:UPDATE=0x4,NO_DELAY=0x8,READ=0x1,WRITE=0x2\nc.RT:ROOT=0x1\nc.BDF:REDRAW=0x2,DITHER=0x4,SYNC=0x1\nc.DRAG:ANCHOR=0x1,NORMAL=0x2,NONE=0x0\nc.IRF:RELATIVE=0x8,IGNORE_NV_CHILDREN=0x1,IGNORE_CHILDREN=0x2,FORCE_DRAW=0x10,SINGLE_BITMAP=0x4\nc.DSF:NO_EXPOSE=0x2,NO_DRAW=0x1\nc.RNF:STICKY=0x10,GRAB_FOCUS=0x20,HAS_FOCUS=0x40,FAST_RESIZE=0x80,DISABLED=0x100,REGION=0x200,AUTO_QUIT=0x400,PRECOPY=0x1000,VIDEO=0x2000,WRITE_ONLY=0x2000,NO_HORIZONTAL=0x4000,NO_VERTICAL=0x8000,CURSOR=0x10000,POINTER=0x10000,SCROLL_CONTENT=0x20000,AFTER_COPY=0x40000,FIXED_BUFFER=0x80000,PERVASIVE_COPY=0x100000,VOLATILE=0x51000,NO_FOCUS=0x200000,READ_ONLY=0x50240,FIXED_DEPTH=0x400000,INIT_ONLY=0x6583981,TOTAL_REDRAW=0x800000,COMPOSITE=0x1000000,NO_PRECOMPOSITE=0x1000000,POST_COMPOSITE=0x1000000,FULL_SCREEN=0x2000000,IGNORE_FOCUS=0x4000000,ASPECT_RATIO=0x8000000,HOST=0x800,TRANSPARENT=0x1,STICK_TO_BACK=0x2,STICK_TO_FRONT=0x4,VISIBLE=0x8\nc.EXF:ABSOLUTE=0x8,REDRAW_VOLATILE=0x2,REDRAW_VOLATILE_OVERLAP=0x4,CURSOR_SPLIT=0x10,CHILDREN=0x1,ABSOLUTE_COORDS=0x8\nc.SWIN:NONE=0x3,HOST=0x0,TASKBAR=0x1,ICON_TRAY=0x2\nc.LVF:EXPOSE_CHANGES=0x1\n"
