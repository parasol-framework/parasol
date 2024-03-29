// Auto-generated by idl-c.fluid

extern "C" {
objPointer * gfxAccessPointer();
ERROR gfxCheckIfChild(OBJECTID Parent, OBJECTID Child);
ERROR gfxCopyArea(extBitmap * Bitmap, extBitmap * Dest, BAF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
ERROR gfxCopyRawBitmap(struct BitmapSurfaceV2 * Surface, extBitmap * Bitmap, CSRF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
ERROR gfxCopySurface(OBJECTID Surface, extBitmap * Bitmap, BDF Flags, LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest);
void gfxDrawPixel(extBitmap * Bitmap, LONG X, LONG Y, ULONG Colour);
void gfxDrawRGBPixel(extBitmap * Bitmap, LONG X, LONG Y, struct RGB8 * RGB);
void gfxDrawRectangle(extBitmap * Bitmap, LONG X, LONG Y, LONG Width, LONG Height, ULONG Colour, BAF Flags);
ERROR gfxExposeSurface(OBJECTID Surface, LONG X, LONG Y, LONG Width, LONG Height, EXF Flags);
void gfxFlipBitmap(extBitmap * Bitmap, FLIP Orientation);
void gfxGetColourFormat(struct ColourFormat * Format, LONG BitsPerPixel, LONG RedMask, LONG GreenMask, LONG BlueMask, LONG AlphaMask);
ERROR gfxGetCursorInfo(struct CursorInfo * Info, LONG Size);
ERROR gfxGetCursorPos(DOUBLE * X, DOUBLE * Y);
ERROR gfxGetDisplayInfo(OBJECTID Display, struct DisplayInfoV3 ** Info);
DT gfxGetDisplayType();
CSTRING gfxGetInputTypeName(JET Type);
OBJECTID gfxGetModalSurface();
ERROR gfxGetRelativeCursorPos(OBJECTID Surface, DOUBLE * X, DOUBLE * Y);
ERROR gfxGetSurfaceCoords(OBJECTID Surface, LONG * X, LONG * Y, LONG * AbsX, LONG * AbsY, LONG * Width, LONG * Height);
ERROR gfxGetSurfaceFlags(OBJECTID Surface, RNF * Flags);
ERROR gfxGetSurfaceInfo(OBJECTID Surface, struct SurfaceInfoV2 ** Info);
OBJECTID gfxGetUserFocus();
ERROR gfxGetVisibleArea(OBJECTID Surface, LONG * X, LONG * Y, LONG * AbsX, LONG * AbsY, LONG * Width, LONG * Height);
ERROR gfxLockBitmap(OBJECTID Surface, objBitmap ** Bitmap, LVF * Info);
ERROR gfxLockCursor(OBJECTID Surface);
ULONG gfxReadPixel(extBitmap * Bitmap, LONG X, LONG Y);
void gfxReadRGBPixel(extBitmap * Bitmap, LONG X, LONG Y, struct RGB8 ** RGB);
ERROR gfxResample(extBitmap * Bitmap, struct ColourFormat * ColourFormat);
ERROR gfxRestoreCursor(PTC Cursor, OBJECTID Owner);
DOUBLE gfxScaleToDPI(DOUBLE Value);
ERROR gfxScanDisplayModes(CSTRING Filter, struct DisplayInfoV3 * Info, LONG Size);
void gfxSetClipRegion(extBitmap * Bitmap, LONG Number, LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Terminate);
ERROR gfxSetCursor(OBJECTID Surface, CRF Flags, PTC Cursor, CSTRING Name, OBJECTID Owner);
ERROR gfxSetCursorPos(DOUBLE X, DOUBLE Y);
ERROR gfxSetCustomCursor(OBJECTID Surface, CRF Flags, objBitmap * Bitmap, LONG HotX, LONG HotY, OBJECTID Owner);
ERROR gfxSetHostOption(HOST Option, LARGE Value);
OBJECTID gfxSetModalSurface(OBJECTID Surface);
ERROR gfxStartCursorDrag(OBJECTID Source, LONG Item, CSTRING Datatypes, OBJECTID Surface);
ERROR gfxSubscribeInput(FUNCTION * Callback, OBJECTID SurfaceFilter, JTYPE Mask, OBJECTID DeviceFilter, LONG * Handle);
void gfxSync(extBitmap * Bitmap);
ERROR gfxUnlockBitmap(OBJECTID Surface, extBitmap * Bitmap);
ERROR gfxUnlockCursor(OBJECTID Surface);
ERROR gfxUnsubscribeInput(LONG Handle);
ERROR gfxWindowHook(OBJECTID SurfaceID, WH Event, FUNCTION * Callback);

} // extern c
