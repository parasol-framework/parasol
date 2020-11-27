
struct dcDisplayInputReady { // This is an internal structure used by the display module to replace dcInputReady
   LARGE NextIndex;    // Next message index for the subscriber to look at
   LONG  SubIndex;     // Index into the InputSubscription list
};

#define MT_PtrSetWinCursor -1
#define MT_PtrGrabX11Pointer -2
#define MT_PtrUngrabX11Pointer -3

struct ptrSetWinCursor { LONG Cursor;  };
struct ptrGrabX11Pointer { OBJECTID SurfaceID;  };

INLINE ERROR ptrSetWinCursor(APTR Ob, LONG Cursor) {
   struct ptrSetWinCursor args = { Cursor };
   return Action(MT_PtrSetWinCursor, Ob, &args);
}

#define ptrUngrabX11Pointer(obj) Action(MT_PtrUngrabX11Pointer,(obj),0)

INLINE ERROR ptrGrabX11Pointer(APTR Ob, OBJECTID SurfaceID) {
   struct ptrGrabX11Pointer args = { SurfaceID };
   return Action(MT_PtrGrabX11Pointer, Ob, &args);
}
