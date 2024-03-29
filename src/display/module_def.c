// Auto-generated by idl-c.fluid

#ifndef FDEF
#define FDEF static const struct FunctionField
#endif

FDEF argsAccessPointer[] = { { "Object", FD_OBJECTPTR }, { 0, 0 } };
FDEF argsCheckIfChild[] = { { "Error", FD_LONG|FD_ERROR }, { "Parent", FD_OBJECTID }, { "Child", FD_OBJECTID }, { 0, 0 } };
FDEF argsCopyArea[] = { { "Error", FD_LONG|FD_ERROR }, { "Bitmap", FD_OBJECTPTR }, { "Dest", FD_OBJECTPTR }, { "Flags", FD_LONG }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { "XDest", FD_LONG }, { "YDest", FD_LONG }, { 0, 0 } };
FDEF argsCopyRawBitmap[] = { { "Error", FD_LONG|FD_ERROR }, { "BitmapSurface:Surface", FD_PTR|FD_STRUCT }, { "Bitmap", FD_OBJECTPTR }, { "Flags", FD_LONG }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { "XDest", FD_LONG }, { "YDest", FD_LONG }, { 0, 0 } };
FDEF argsCopySurface[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "Bitmap", FD_OBJECTPTR }, { "Flags", FD_LONG }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { "XDest", FD_LONG }, { "YDest", FD_LONG }, { 0, 0 } };
FDEF argsDrawPixel[] = { { "Void", FD_VOID }, { "Bitmap", FD_OBJECTPTR }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Colour", FD_LONG|FD_UNSIGNED }, { 0, 0 } };
FDEF argsDrawRGBPixel[] = { { "Void", FD_VOID }, { "Bitmap", FD_OBJECTPTR }, { "X", FD_LONG }, { "Y", FD_LONG }, { "RGB8:RGB", FD_PTR|FD_STRUCT }, { 0, 0 } };
FDEF argsDrawRectangle[] = { { "Void", FD_VOID }, { "Bitmap", FD_OBJECTPTR }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { "Colour", FD_LONG|FD_UNSIGNED }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF argsExposeSurface[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF argsFlipBitmap[] = { { "Void", FD_VOID }, { "Bitmap", FD_OBJECTPTR }, { "Orientation", FD_LONG }, { 0, 0 } };
FDEF argsGetColourFormat[] = { { "Void", FD_VOID }, { "ColourFormat:Format", FD_PTR|FD_STRUCT }, { "BitsPerPixel", FD_LONG }, { "RedMask", FD_LONG }, { "GreenMask", FD_LONG }, { "BlueMask", FD_LONG }, { "AlphaMask", FD_LONG }, { 0, 0 } };
FDEF argsGetCursorInfo[] = { { "Error", FD_LONG|FD_ERROR }, { "CursorInfo:Info", FD_PTR|FD_STRUCT }, { "Size", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF argsGetCursorPos[] = { { "Error", FD_LONG|FD_ERROR }, { "X", FD_DOUBLE|FD_RESULT }, { "Y", FD_DOUBLE|FD_RESULT }, { 0, 0 } };
FDEF argsGetDisplayInfo[] = { { "Error", FD_LONG|FD_ERROR }, { "Display", FD_OBJECTID }, { "DisplayInfo:Info", FD_PTR|FD_STRUCT|FD_RESULT }, { 0, 0 } };
FDEF argsGetDisplayType[] = { { "Result", FD_LONG }, { 0, 0 } };
FDEF argsGetInputTypeName[] = { { "Result", FD_STR }, { "Type", FD_LONG }, { 0, 0 } };
FDEF argsGetModalSurface[] = { { "Result", FD_OBJECTID }, { 0, 0 } };
FDEF argsGetRelativeCursorPos[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "X", FD_DOUBLE|FD_RESULT }, { "Y", FD_DOUBLE|FD_RESULT }, { 0, 0 } };
FDEF argsGetSurfaceCoords[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "X", FD_LONG|FD_RESULT }, { "Y", FD_LONG|FD_RESULT }, { "AbsX", FD_LONG|FD_RESULT }, { "AbsY", FD_LONG|FD_RESULT }, { "Width", FD_LONG|FD_RESULT }, { "Height", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsGetSurfaceFlags[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "Flags", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsGetSurfaceInfo[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "SurfaceInfo:Info", FD_PTR|FD_STRUCT|FD_RESULT }, { 0, 0 } };
FDEF argsGetUserFocus[] = { { "Result", FD_OBJECTID }, { 0, 0 } };
FDEF argsGetVisibleArea[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "X", FD_LONG|FD_RESULT }, { "Y", FD_LONG|FD_RESULT }, { "AbsX", FD_LONG|FD_RESULT }, { "AbsY", FD_LONG|FD_RESULT }, { "Width", FD_LONG|FD_RESULT }, { "Height", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsLockBitmap[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "Bitmap", FD_OBJECTPTR|FD_RESULT }, { "Info", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsLockCursor[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { 0, 0 } };
FDEF argsReadPixel[] = { { "Result", FD_LONG|FD_UNSIGNED }, { "Bitmap", FD_OBJECTPTR }, { "X", FD_LONG }, { "Y", FD_LONG }, { 0, 0 } };
FDEF argsReadRGBPixel[] = { { "Void", FD_VOID }, { "Bitmap", FD_OBJECTPTR }, { "X", FD_LONG }, { "Y", FD_LONG }, { "RGB8:RGB", FD_PTR|FD_STRUCT|FD_RESULT }, { 0, 0 } };
FDEF argsResample[] = { { "Error", FD_LONG|FD_ERROR }, { "Bitmap", FD_OBJECTPTR }, { "ColourFormat:ColourFormat", FD_PTR|FD_STRUCT }, { 0, 0 } };
FDEF argsRestoreCursor[] = { { "Error", FD_LONG|FD_ERROR }, { "Cursor", FD_LONG }, { "Owner", FD_OBJECTID }, { 0, 0 } };
FDEF argsScaleToDPI[] = { { "Result", FD_DOUBLE }, { "Value", FD_DOUBLE }, { 0, 0 } };
FDEF argsScanDisplayModes[] = { { "Error", FD_LONG|FD_ERROR }, { "Filter", FD_STR }, { "DisplayInfo:Info", FD_PTR|FD_STRUCT }, { "Size", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF argsSetClipRegion[] = { { "Void", FD_VOID }, { "Bitmap", FD_OBJECTPTR }, { "Number", FD_LONG }, { "Left", FD_LONG }, { "Top", FD_LONG }, { "Right", FD_LONG }, { "Bottom", FD_LONG }, { "Terminate", FD_LONG }, { 0, 0 } };
FDEF argsSetCursor[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "Flags", FD_LONG }, { "Cursor", FD_LONG }, { "Name", FD_STR }, { "Owner", FD_OBJECTID }, { 0, 0 } };
FDEF argsSetCursorPos[] = { { "Error", FD_LONG|FD_ERROR }, { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { 0, 0 } };
FDEF argsSetCustomCursor[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "Flags", FD_LONG }, { "Bitmap", FD_OBJECTPTR }, { "HotX", FD_LONG }, { "HotY", FD_LONG }, { "Owner", FD_OBJECTID }, { 0, 0 } };
FDEF argsSetHostOption[] = { { "Error", FD_LONG|FD_ERROR }, { "Option", FD_LONG }, { "Value", FD_LARGE }, { 0, 0 } };
FDEF argsSetModalSurface[] = { { "Result", FD_OBJECTID }, { "Surface", FD_OBJECTID }, { 0, 0 } };
FDEF argsStartCursorDrag[] = { { "Error", FD_LONG|FD_ERROR }, { "Source", FD_OBJECTID }, { "Item", FD_LONG }, { "Datatypes", FD_STR }, { "Surface", FD_OBJECTID }, { 0, 0 } };
FDEF argsSubscribeInput[] = { { "Error", FD_LONG|FD_ERROR }, { "Callback", FD_FUNCTIONPTR }, { "SurfaceFilter", FD_OBJECTID }, { "Mask", FD_LONG }, { "DeviceFilter", FD_OBJECTID }, { "Handle", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsSync[] = { { "Void", FD_VOID }, { "Bitmap", FD_OBJECTPTR }, { 0, 0 } };
FDEF argsUnlockBitmap[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { "Bitmap", FD_OBJECTPTR }, { 0, 0 } };
FDEF argsUnlockCursor[] = { { "Error", FD_LONG|FD_ERROR }, { "Surface", FD_OBJECTID }, { 0, 0 } };
FDEF argsUnsubscribeInput[] = { { "Error", FD_LONG|FD_ERROR }, { "Handle", FD_LONG }, { 0, 0 } };
FDEF argsWindowHook[] = { { "Error", FD_LONG|FD_ERROR }, { "SurfaceID", FD_OBJECTID }, { "Event", FD_LONG }, { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };

const struct Function glFunctions[] = {
   { (APTR)gfxAccessPointer, "AccessPointer", argsAccessPointer },
   { (APTR)gfxCheckIfChild, "CheckIfChild", argsCheckIfChild },
   { (APTR)gfxCopyArea, "CopyArea", argsCopyArea },
   { (APTR)gfxCopyRawBitmap, "CopyRawBitmap", argsCopyRawBitmap },
   { (APTR)gfxCopySurface, "CopySurface", argsCopySurface },
   { (APTR)gfxDrawPixel, "DrawPixel", argsDrawPixel },
   { (APTR)gfxDrawRGBPixel, "DrawRGBPixel", argsDrawRGBPixel },
   { (APTR)gfxDrawRectangle, "DrawRectangle", argsDrawRectangle },
   { (APTR)gfxExposeSurface, "ExposeSurface", argsExposeSurface },
   { (APTR)gfxFlipBitmap, "FlipBitmap", argsFlipBitmap },
   { (APTR)gfxGetColourFormat, "GetColourFormat", argsGetColourFormat },
   { (APTR)gfxGetCursorInfo, "GetCursorInfo", argsGetCursorInfo },
   { (APTR)gfxGetCursorPos, "GetCursorPos", argsGetCursorPos },
   { (APTR)gfxGetDisplayInfo, "GetDisplayInfo", argsGetDisplayInfo },
   { (APTR)gfxGetDisplayType, "GetDisplayType", argsGetDisplayType },
   { (APTR)gfxGetInputTypeName, "GetInputTypeName", argsGetInputTypeName },
   { (APTR)gfxGetModalSurface, "GetModalSurface", argsGetModalSurface },
   { (APTR)gfxGetRelativeCursorPos, "GetRelativeCursorPos", argsGetRelativeCursorPos },
   { (APTR)gfxGetSurfaceCoords, "GetSurfaceCoords", argsGetSurfaceCoords },
   { (APTR)gfxGetSurfaceFlags, "GetSurfaceFlags", argsGetSurfaceFlags },
   { (APTR)gfxGetSurfaceInfo, "GetSurfaceInfo", argsGetSurfaceInfo },
   { (APTR)gfxGetUserFocus, "GetUserFocus", argsGetUserFocus },
   { (APTR)gfxGetVisibleArea, "GetVisibleArea", argsGetVisibleArea },
   { (APTR)gfxLockBitmap, "LockBitmap", argsLockBitmap },
   { (APTR)gfxLockCursor, "LockCursor", argsLockCursor },
   { (APTR)gfxReadPixel, "ReadPixel", argsReadPixel },
   { (APTR)gfxReadRGBPixel, "ReadRGBPixel", argsReadRGBPixel },
   { (APTR)gfxResample, "Resample", argsResample },
   { (APTR)gfxRestoreCursor, "RestoreCursor", argsRestoreCursor },
   { (APTR)gfxScaleToDPI, "ScaleToDPI", argsScaleToDPI },
   { (APTR)gfxScanDisplayModes, "ScanDisplayModes", argsScanDisplayModes },
   { (APTR)gfxSetClipRegion, "SetClipRegion", argsSetClipRegion },
   { (APTR)gfxSetCursor, "SetCursor", argsSetCursor },
   { (APTR)gfxSetCursorPos, "SetCursorPos", argsSetCursorPos },
   { (APTR)gfxSetCustomCursor, "SetCustomCursor", argsSetCustomCursor },
   { (APTR)gfxSetHostOption, "SetHostOption", argsSetHostOption },
   { (APTR)gfxSetModalSurface, "SetModalSurface", argsSetModalSurface },
   { (APTR)gfxStartCursorDrag, "StartCursorDrag", argsStartCursorDrag },
   { (APTR)gfxSubscribeInput, "SubscribeInput", argsSubscribeInput },
   { (APTR)gfxSync, "Sync", argsSync },
   { (APTR)gfxUnlockBitmap, "UnlockBitmap", argsUnlockBitmap },
   { (APTR)gfxUnlockCursor, "UnlockCursor", argsUnlockCursor },
   { (APTR)gfxUnsubscribeInput, "UnsubscribeInput", argsUnsubscribeInput },
   { (APTR)gfxWindowHook, "WindowHook", argsWindowHook },
   { NULL, NULL, NULL }
};

