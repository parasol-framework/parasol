/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-MODULE-
Surface: Provides UI management functionality.


-END-

*****************************************************************************/

//#define DEBUG
//#define DBG_DRAW_ROUTINES // Use this if you want to debug any external code that is subscribed to surface drawing routines
//#define FASTACCESS
//#define DBG_LAYERS

#define FOCUSMSG(...) //LogF(NULL, __VA_ARGS__)

#ifdef DBG_LAYERS
#include <stdio.h>
#endif

#define __system__
#define PRV_LAYOUT
#define PRV_SURFACE
#define PRV_SURFACE_MODULE

#include <parasol/modules/xml.h>
#include <parasol/modules/window.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/display.h>

#undef NULL
#define NULL 0

#define SIZE_FOCUSLIST   30

static const struct FieldDef clWindowType[];

struct CoreBase *CoreBase;
static struct DisplayBase *DisplayBase;
static struct SharedControl *glSharedControl = NULL;
static TIMER glRefreshPointerTimer = 0;
static objBitmap *glComposite = NULL;
static OBJECTPTR SurfaceClass = NULL, LayoutClass = NULL;
static OBJECTPTR modDisplay = NULL, modSurface = NULL;
static BYTE glDisplayType = DT_NATIVE;
static DOUBLE glpRefreshRate = -1, glpGammaRed = 1, glpGammaGreen = 1, glpGammaBlue = 1;
static LONG glpDisplayWidth = 1024, glpDisplayHeight = 768, glpDisplayX = 0, glpDisplayY = 0;
static LONG glpDisplayDepth = 0; // If zero, the display depth will be based on the hosted desktop's bit depth.
static LONG glpMaximise = FALSE, glpFullScreen = FALSE;
static LONG glpWindowType = SWIN_HOST;
static char glpDPMS[20] = "Standby";
static LONG glClassFlags = CLF_SHARED_ONLY|CLF_PUBLIC_OBJECTS; // Set on CMDInit()
static objXML *glStyle = NULL;
static OBJECTPTR glAppStyle = NULL;
static OBJECTPTR glDesktopStyleScript = NULL;
static OBJECTPTR glDefaultStyleScript = NULL;

// Thread-specific variables.

static THREADVAR APTR glSurfaceMutex = NULL;
static THREADVAR WORD tlNoDrawing = 0, tlNoExpose = 0, tlVolatileIndex = 0;
static THREADVAR UBYTE tlListCount = 0; // For drwAccesslist()
static THREADVAR OBJECTID tlFreeExpose = 0;
static THREADVAR struct SurfaceControl *tlSurfaceList = NULL;

enum {
   STAGE_PRECOPY=1,
   STAGE_AFTERCOPY,
   STAGE_COMPOSITE
};

#include "module_def.c"

//**********************************************************************

#ifdef _WIN32
APTR winGetDC(APTR);
void winReleaseDC(APTR, APTR);
void winSetSurfaceID(APTR, LONG);
#endif

static ERROR access_video(OBJECTID DisplayID, objDisplay **, objBitmap **);
static ERROR apply_style(OBJECTPTR, OBJECTPTR, CSTRING);
static ERROR load_styles(void);
static BYTE  check_surface_list(void);
static UBYTE CheckVisibility(struct SurfaceList *, WORD);
static UBYTE check_volatile(struct SurfaceList *, WORD);
static ERROR create_surface_class(void);
static ERROR create_layout_class(void);
static void expose_buffer(struct SurfaceList *, WORD Total, WORD Index, WORD ScanIndex, LONG Left, LONG Top, LONG Right, LONG Bottom, OBJECTID VideoID, objBitmap *);
static WORD  FindBitmapOwner(struct SurfaceList *, WORD);
static ERROR drwRedrawSurface(OBJECTID, LONG, LONG, LONG, LONG, LONG);
static void  invalidate_overlap(objSurface *, struct SurfaceList *, WORD, LONG, LONG, LONG, LONG, LONG, LONG, objBitmap *);
static void  move_layer(objSurface *, LONG, LONG);
static void  move_layer_pos(struct SurfaceControl *, LONG, LONG);
static LONG  msg_handler(APTR, LONG, LONG, APTR, LONG);
static void  prepare_background(objSurface *, struct SurfaceList *, WORD, WORD, objBitmap *, struct ClipRectangle *, BYTE);
static void  process_surface_callbacks(objSurface *, objBitmap *);
static void  redraw_nonintersect(OBJECTID, struct SurfaceList *, WORD, WORD, struct ClipRectangle *, struct ClipRectangle *, LONG, LONG);
static void  release_video(objDisplay *);
static ERROR track_layer(objSurface *);
static void  untrack_layer(OBJECTID);
static void  check_bmp_buffer_depth(objSurface *, objBitmap *);
static BYTE  restrict_region_to_parents(struct SurfaceList *, LONG, struct ClipRectangle *, BYTE);
static ERROR load_style_values(void);
static ERROR _expose_surface(OBJECTID SurfaceID, struct SurfaceList *list, WORD index, WORD total, LONG X, LONG Y, LONG Width, LONG Height, LONG Flags);
static ERROR _redraw_surface(OBJECTID SurfaceID, struct SurfaceList *list, WORD Index, WORD Total, LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Flags);
static void  _redraw_surface_do(objSurface *, struct SurfaceList *, WORD, WORD, LONG, LONG, LONG, LONG, objBitmap *, LONG);
static void check_styles(STRING Path, OBJECTPTR *Script) __attribute__((unused));

static ERROR resize_layer(objSurface *, LONG, LONG, LONG, LONG, LONG, LONG, LONG, DOUBLE, LONG);

#define UpdateSurfaceList(a) UpdateSurfaceCopy((a), 0)

static ERROR UpdateSurfaceCopy(objSurface *Self, struct SurfaceList *Copy);

#define URF_HATE_CHILDREN     0x00000001

#define UpdateSurfaceField(a,b) { \
   struct SurfaceList *list; struct SurfaceControl *ctl; WORD i; \
   if (Self->Head.Flags & NF_INITIALISED) { \
   if ((ctl = drwAccessList(ARF_UPDATE))) { \
      list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex); \
      for (i=0; i < ctl->Total; i++) { \
         if (list[i].SurfaceID IS (a)->Head.UniqueID) { \
            list[i].b = (a)->b; \
            break; \
         } \
      } \
      drwReleaseList(ARF_UPDATE); \
   } \
   } \
}

#define UpdateSurfaceField2(a,b,c) { \
   struct SurfaceList *list; struct SurfaceControl *ctl; WORD i; \
   if (Self->Head.Flags & NF_INITIALISED) { \
      if ((ctl = drwAccessList(ARF_UPDATE))) { \
         list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex); \
         for (i=0; i < ctl->Total; i++) { \
            if (list[i].SurfaceID IS (a)->Head.UniqueID) { \
               list[i].b = (a)->c; \
               break; \
            } \
         } \
         drwReleaseList(ARF_UPDATE); \
      } \
   } \
}

#ifdef DBG_LAYERS
static void print_layer_list(STRING Function, struct SurfaceControl *Ctl, LONG POI)
{
   struct SurfaceList *list = (struct SurfaceList *)((APTR)Ctl + Ctl->ArrayIndex);
   fprintf(stderr, "LAYER LIST: %d of %d Entries, From %s()\n", Ctl->Total, Ctl->ArraySize, Function);

   LONG i, j;
   for (i=0; i < Ctl->Total; i++) {
      fprintf(stderr, "%.2d: ", i);
      for (j=0; j < list[i].Level; j++) fprintf(stderr, " ");
      fprintf(stderr, "#%d, Parent: %d, Flags: $%.8x", list[i].SurfaceID, list[i].ParentID, list[i].Flags);

      // Highlight any point of interest

      if (i IS POI) fprintf(stderr, " <---- POI");

      // Error checks

      if (!list[i].SurfaceID) fprintf(stderr, " <---- ERROR");
      else if (CheckObjectExists(list[i].SurfaceID, NULL) != ERR_True) fprintf(stderr, " <---- OBJECT MISSING");

      // Does the parent exist in the layer list?

      if (list[i].ParentID) {
         for (j=i-1; j >= 0; j--) {
            if (list[j].SurfaceID IS list[i].ParentID) break;
         }
         if (j < 0) fprintf(stderr, " <---- PARENT MISSING");
      }

      fprintf(stderr, "\n");
   }
}
#endif

//****************************************************************************
// Surface list lookup routines.

static THREADVAR LONG glRecentSurfaceIndex = 0;

#define find_surface_index(a,b) find_surface_list( ((APTR)(a) + (a)->ArrayIndex), (a)->Total, (b))
#define find_own_index(a,b) find_surface_list( ((APTR)(a) + (a)->ArrayIndex), (a)->Total, (b)->Head.UniqueID)

static LONG find_surface_list(struct SurfaceList *list, LONG Total, OBJECTID SurfaceID)
{
   if (glRecentSurfaceIndex < Total) { // Cached lookup
      if (list[glRecentSurfaceIndex].SurfaceID IS SurfaceID) return glRecentSurfaceIndex;
   }

   // Search for the object

   LONG i;
   for (i=0; i < Total; i++) {
      if (list[i].SurfaceID IS SurfaceID) {
         glRecentSurfaceIndex = i;
         return i;
      }
   }

   return -1;
}

#define find_parent_index(a,b) find_parent_list( ((APTR)(a) + (a)->ArrayIndex), (a)->Total, (b))

static LONG find_parent_list(struct SurfaceList *list, WORD Total, objSurface *Self)
{
   if (glRecentSurfaceIndex < Total) { // Cached lookup
      if (list[glRecentSurfaceIndex].SurfaceID IS Self->ParentID) return glRecentSurfaceIndex;
   }

   if ((Self->ListIndex < Total) AND (list[Self->ListIndex].SurfaceID IS Self->Head.UniqueID)) {
      LONG i;
      for (i=Self->ListIndex-1; i >= 0; i--) {
         if (list[i].SurfaceID IS Self->ParentID) {
            glRecentSurfaceIndex = i;
            return i;
         }
      }
   }

   // Search for the object

   LONG i;
   for (i=0; i < Total; i++) {
      if (list[i].SurfaceID IS Self->ParentID) {
         glRecentSurfaceIndex = i;
         return i;
      }
   }

   return -1;
}

//**********************************************************************

static inline void ClipRectangle(struct ClipRectangle *rect, struct ClipRectangle *clip)
{
   if (rect->Left   < clip->Left)   rect->Left   = clip->Left;
   if (rect->Top    < clip->Top)    rect->Top    = clip->Top;
   if (rect->Right  > clip->Right)  rect->Right  = clip->Right;
   if (rect->Bottom > clip->Bottom) rect->Bottom = clip->Bottom;
}

static APTR glExposeHandler = NULL;

//****************************************************************************

ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;
   GetPointer(argModule, FID_Master, &modSurface);

   const struct SystemState *state = GetSystemState();
   if (state->Stage < 0) {
      // An early load indicates that classes are being probed, so just return them.
      create_layout_class();
      return create_surface_class();
   }

   if (GetResource(RES_GLOBAL_INSTANCE)) {
      glClassFlags = CLF_SHARED_ONLY|CLF_PUBLIC_OBJECTS;
   }
   else glClassFlags = 0; // When operating stand-alone, do not share surfaces by default.

   // Add a message handler to the system for responding to interface messages

   FUNCTION call;
   SET_FUNCTION_STDC(call, &msg_handler);
   if (AddMsgHandler(NULL, MSGID_EXPOSE, &call, &glExposeHandler) != ERR_Okay) {
      return FuncError(ERR_Failed);
   }

   // Allocate the FocusList memory block

   MEMORYID mem_id = RPM_FocusList;
   ERROR error = AllocMemory((SIZE_FOCUSLIST) * sizeof(OBJECTID), MEM_UNTRACKED|MEM_RESERVED|MEM_PUBLIC, NULL, &mem_id);
   if ((error != ERR_Okay) AND (error != ERR_ResourceExists)) {
      return PostError(ERR_AllocMemory);
   }

   glSharedControl = GetResourcePtr(RES_SHARED_CONTROL);

   // The SurfaceList mutex controls any attempt to update the glSharedControl->SurfacesMID field.

   if (AllocSharedMutex("SurfaceList", &glSurfaceMutex)) {
      return PostError(ERR_AllocMutex);
   }

   // Allocate the SurfaceList memory block and its associated mutex.  Note that MEM_TMP_LOCK is used as a safety
   // measure to prevent any task from being put to sleep when it has a SurfaceControl lock (due to the potential for
   // deadlocks).

   if (!LockSharedMutex(glSurfaceMutex, 5000)) {
      if (!glSharedControl->SurfacesMID) {
         struct SurfaceControl *ctl;
         const LONG listsize = 200;
         if (!(error = AllocMemory(sizeof(struct SurfaceControl) + (listsize * sizeof(UWORD)) + (listsize * sizeof(struct SurfaceList)),
               MEM_UNTRACKED|MEM_PUBLIC|MEM_NO_CLEAR|MEM_TMP_LOCK, &ctl, &glSharedControl->SurfacesMID))) {
            ctl->ListIndex  = sizeof(struct SurfaceControl);
            ctl->ArrayIndex = sizeof(struct SurfaceControl) + (listsize * sizeof(UWORD));
            ctl->EntrySize  = sizeof(struct SurfaceList);
            ctl->Total      = 0;
            ctl->ArraySize  = listsize;
            ReleaseMemory(ctl);
         }
         else {
            UnlockSharedMutex(glSurfaceMutex);
            return FuncError(ERR_AllocMemory);
         }
      }
      UnlockSharedMutex(glSurfaceMutex);
   }
   else return FuncError(ERR_AccessMemory);

   if (LoadModule("display", MODVERSION_DISPLAY, &modDisplay, &DisplayBase) != ERR_Okay) return ERR_InitModule;

   // Find system objects

   glDisplayType = gfxGetDisplayType();

#ifdef __ANDROID__
      glpFullScreen = TRUE;
      glpDisplayDepth = 16;

      DISPLAYINFO *info;
      if (!gfxGetDisplayInfo(0, &info)) {
         glpDisplayWidth  = info.Width;
         glpDisplayHeight = info.Height;
         glpDisplayDepth  = info.BitsPerPixel;
      }
#else
   OBJECTPTR config;
   if (!CreateObject(ID_CONFIG, NULL, &config,
         FID_Path|TSTRING, "user:config/display.cfg",
         TAGEND)) {

      cfgReadInt(config, "DISPLAY", "Maximise", &glpMaximise);

      if ((glDisplayType IS DT_X11) OR (glDisplayType IS DT_WINDOWS)) {
         LogMsg("Using hosted window dimensions: %dx%d,%dx%d", glpDisplayX, glpDisplayY, glpDisplayWidth, glpDisplayHeight);
         if ((cfgReadInt(config, "DISPLAY", "WindowWidth", &glpDisplayWidth) != ERR_Okay) OR (!glpDisplayWidth)) {
            cfgReadInt(config, "DISPLAY", "Width", &glpDisplayWidth);
         }

         if ((cfgReadInt(config, "DISPLAY", "WindowHeight", &glpDisplayHeight) != ERR_Okay) OR (!glpDisplayHeight)) {
            cfgReadInt(config, "DISPLAY", "Height", &glpDisplayHeight);
         }

         cfgReadInt(config, "DISPLAY", "WindowX", &glpDisplayX);
         cfgReadInt(config, "DISPLAY", "WindowY", &glpDisplayY);
         cfgReadInt(config, "DISPLAY", "FullScreen", &glpFullScreen);
      }
      else {
         cfgReadInt(config, "DISPLAY", "Width", &glpDisplayWidth);
         cfgReadInt(config, "DISPLAY", "Height", &glpDisplayHeight);
         cfgReadInt(config, "DISPLAY", "XCoord", &glpDisplayX);
         cfgReadInt(config, "DISPLAY", "YCoord", &glpDisplayY);
         cfgReadInt(config, "DISPLAY", "Depth", &glpDisplayDepth);
         LogMsg("Using default display dimensions: %dx%d,%dx%d", glpDisplayX, glpDisplayY, glpDisplayWidth, glpDisplayHeight);
      }

      cfgReadFloat(config, "DISPLAY", "RefreshRate", &glpRefreshRate);
      cfgReadFloat(config, "DISPLAY", "GammaRed", &glpGammaRed);
      cfgReadFloat(config, "DISPLAY", "GammaGreen", &glpGammaGreen);
      cfgReadFloat(config, "DISPLAY", "GammaBlue", &glpGammaBlue);
      CSTRING dpms;
      if (!(error = cfgReadValue(config, "DISPLAY", "DPMS", &dpms))) {
         StrCopy(dpms, glpDPMS, sizeof(glpDPMS));
      }
      acFree(config);
   }
#endif

   load_style_values();

   create_layout_class();

   return create_surface_class();
}

//****************************************************************************

static ERROR CMDOpen(OBJECTPTR Module)
{
   SetPointer(Module, FID_FunctionList, glFunctions);
   return ERR_Okay;
}

//****************************************************************************

ERROR CMDExpunge(void)
{
   if (glRefreshPointerTimer) { UpdateTimer(glRefreshPointerTimer, 0); glRefreshPointerTimer = 0; }
   if (glStyle)         { acFree(glStyle); glStyle = NULL; }
   if (glAppStyle)      { acFree(glAppStyle); glAppStyle = NULL; }
   if (glDesktopStyleScript) { acFree(glDesktopStyleScript); glDesktopStyleScript = NULL; }
   if (glDefaultStyleScript) { acFree(glDefaultStyleScript); glDefaultStyleScript = NULL; }
   if (glExposeHandler) { RemoveMsgHandler(glExposeHandler); glExposeHandler = NULL; }
   if (glComposite)     { acFree(glComposite); glComposite = NULL; }
   if (modDisplay)      { acFree(modDisplay); modDisplay = NULL; }
   if (SurfaceClass)    { acFree(SurfaceClass); SurfaceClass = NULL; }
   if (LayoutClass)     { acFree(LayoutClass); LayoutClass = NULL; }

   return ERR_Okay;
}

//****************************************************************************
// Handles incoming interface messages.

static ERROR msg_handler(APTR Custom, LONG UniqueID, LONG Type, APTR Data, LONG Size)
{
   if ((Data) AND (Size >= sizeof(struct ExposeMessage))) {
      struct ExposeMessage *expose;
      expose = Data;
      drwExposeSurface(expose->ObjectID, expose->X, expose->Y, expose->Width, expose->Height, expose->Flags);
   }

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
AccessList: Private. Grants access to the internal SurfaceList array.

-INPUT-
int(ARF) Flags: Specify ARF_WRITE if writing to the list, otherwise ARF_READ must be set.  Use ARF_NO_DELAY if you need immediate access to the surfacelist.

-RESULT-
struct(SurfaceControl): Pointer to the SurfaceControl structure.

*****************************************************************************/

static struct SurfaceControl * drwAccessList(LONG Flags)
{
   if (!tlSurfaceList) {
      ERROR error;

      if (Flags & ARF_NO_DELAY) {
         error = AccessMemory(glSharedControl->SurfacesMID, MEM_READ_WRITE, 20, &tlSurfaceList);
      }
      else error = AccessMemory(glSharedControl->SurfacesMID, MEM_READ_WRITE, 4000, &tlSurfaceList);

      if (!error) {
         tlListCount = 1;
         //FMSG("~AccessList()","");
      }
   }
   else tlListCount++;

   return tlSurfaceList;
}

/****************************************************************************

-FUNCTION-
CopySurface: Copies surface graphics data into any bitmap object

This function will copy the graphics data from any surface object into a @Bitmap of your choosing.  This is
the fastest and most convenient way to get graphics information out of any surface.  As surfaces are buffered, it is
guaranteed that the result will not be obscured by any overlapping surfaces that are on the display.

In the event that the owner of the surface is drawing to the graphics buffer at the time that you call this function,
the results could be out of sync.  If this could be a problem, set the BDF_SYNC option in the Flags parameter.  Keep in
mind that syncing has the negative side effect of having to wait for the other task to complete its draw process, which
can potentially result in time lags.

-INPUT-
oid Surface: The ID of the surface object to copy from.
obj(Bitmap) Bitmap: Must reference a target Bitmap object.
int(BDF) Flags:  Optional flags.
int X:      The horizontal source coordinate.
int Y:      The vertical source coordinate.
int Width:  The width of the graphic that will be copied.
int Height: The height of the graphic that will be copied.
int XDest:  The horizontal target coordinate.
int YDest:  The vertical target coordinate.

-ERRORS-
Okay
Args
Search: The supplied SurfaceID did not refer to a recognised surface object
AccessMemory: Failed to access the internal surfacelist memory structure

****************************************************************************/

static ERROR drwCopySurface(OBJECTID SurfaceID, objBitmap *Bitmap, LONG Flags,
          LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest)
{
   if ((!SurfaceID) OR (!Bitmap)) return FuncError(ERR_NullArgs);

   FMSG("CopySurface()","%dx%d,%dx%d TO %dx%d, Flags $%.8x", X, Y, Width, Height, XDest, YDest, Flags);

   struct SurfaceControl *ctl;
   if ((ctl = drwAccessList(ARF_READ))) {
      WORD i;
      BITMAPSURFACE surface;
      struct SurfaceList *list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex);
      for (i=0; i < ctl->Total; i++) {
         if (list[i].SurfaceID IS SurfaceID) {
            if (X < 0) { XDest -= X; Width  += X; X = 0; }
            if (Y < 0) { YDest -= Y; Height += Y; Y = 0; }
            if (X+Width  > list[i].Width)  Width  = list[i].Width-X;
            if (Y+Height > list[i].Height) Height = list[i].Height-Y;

            // Find the bitmap root

            WORD root = FindBitmapOwner(list, i);

            struct SurfaceList list_i = list[i];
            struct SurfaceList list_root = list[root];
            drwReleaseList(ARF_READ);

            if (Flags & BDF_REDRAW) {
               BYTE state = tlNoDrawing;
               tlNoDrawing = 0;
               drwRedrawSurface(SurfaceID, list_i.Left+X, list_i.Top+Y, list_i.Left+X+Width, list_i.Top+Y+Height, IRF_FORCE_DRAW);
               tlNoDrawing = state;
            }

            if ((Flags & (BDF_SYNC|BDF_DITHER)) OR (!list_root.DataMID)) {
               objBitmap *src;
               if (!AccessObject(list_root.BitmapID, 4000, &src)) {
                  src->XOffset    = list_i.Left - list_root.Left;
                  src->YOffset    = list_i.Top - list_root.Top;
                  src->Clip.Left   = 0;
                  src->Clip.Top    = 0;
                  src->Clip.Right  = list_i.Width;
                  src->Clip.Bottom = list_i.Height;

                  BYTE composite;
                  if (list_i.Flags & RNF_COMPOSITE) composite = TRUE;
                  else composite = FALSE;

                  if (composite) {
                     gfxCopyArea(src, Bitmap, BAF_BLEND|((Flags & BDF_DITHER) ? BAF_DITHER : 0), X, Y, Width, Height, XDest, YDest);
                  }
                  else gfxCopyArea(src, Bitmap, (Flags & BDF_DITHER) ? BAF_DITHER : 0, X, Y, Width, Height, XDest, YDest);

                  ReleaseObject(src);
                  return ERR_Okay;
               }
               else return FuncError(ERR_AccessObject);
            }
            else if (!AccessMemory(list_root.DataMID, MEM_READ, 2000, &surface.Data)) {
               surface.XOffset       = list_i.Left - list_root.Left;
               surface.YOffset       = list_i.Top - list_root.Top;
               surface.LineWidth     = list_root.LineWidth;
               surface.Height        = list_i.Height;
               surface.BitsPerPixel  = list_root.BitsPerPixel;
               surface.BytesPerPixel = list_root.BytesPerPixel;

               BYTE composite;
               if (list_i.Flags & RNF_COMPOSITE) composite = TRUE;
               else composite = FALSE;

               if (composite) gfxCopySurface(&surface, Bitmap, CSRF_DEFAULT_FORMAT|CSRF_OFFSET|CSRF_ALPHA, X, Y, Width, Height, XDest, YDest);
               else gfxCopySurface(&surface, Bitmap, CSRF_DEFAULT_FORMAT|CSRF_OFFSET, X, Y, Width, Height, XDest, YDest);

               ReleaseMemory(surface.Data);
               return ERR_Okay;
            }
            else return FuncError(ERR_AccessMemory);
         }
      }

      drwReleaseList(ARF_READ);
      return ERR_Search;
   }
   else return FuncError(ERR_AccessMemory);

   return ERR_Okay;
}

/****************************************************************************

-FUNCTION-
ExposeSurface: Exposes the content of a surface to the display.

This expose routine will expose all content within a defined surface area, copying it to the display.  This will
include all child surfaces that intersect with the region being exposed if you set the EXF_CHILDREN flag.

-INPUT-
oid Surface: The ID of the surface object that will be exposed.
int X:       The horizontal coordinate of the area to expose.
int Y:       The vertical coordinate of the area to expose.
int Width:   The width of the expose area.
int Height:  The height of the expose area.
int(EXF) Flags: Optional flags - EXF_CHILDREN will expose all intersecting child regions.

-ERRORS-
Okay
NullArgs
Search: The SurfaceID does not refer to an existing surface object
AccessMemory: The internal surfacelist could not be accessed

****************************************************************************/

static ERROR drwExposeSurface(OBJECTID SurfaceID, LONG X, LONG Y, LONG Width, LONG Height, LONG Flags)
{
   if (tlNoDrawing) return ERR_Okay;
   if (!SurfaceID) return ERR_NullArgs;
   if ((Width < 1) OR (Height < 1)) return ERR_Okay;

   struct SurfaceControl *ctl;
   if (!(ctl = drwAccessList(ARF_READ))) return FuncError(ERR_AccessMemory);

   LONG total = ctl->Total;
   struct SurfaceList list[total];
   CopyMemory((APTR)ctl + ctl->ArrayIndex, list, sizeof(list[0]) * ctl->Total);
   drwReleaseList(ARF_READ);

   WORD index;
   if ((index = find_surface_list(list, total, SurfaceID)) IS -1) { // The surface might not be listed if the parent is in the process of being dstroyed.
      FMSG("@ExposeSurface()","Surface %d is not in the surfacelist.", SurfaceID);
      return ERR_Search;
   }

   return _expose_surface(SurfaceID, list, index, total, X, Y, Width, Height, Flags);
}

static ERROR _expose_surface(OBJECTID SurfaceID, struct SurfaceList *list, WORD index, WORD Total, LONG X, LONG Y, LONG Width, LONG Height, LONG Flags)
{
   objBitmap *bitmap;
   struct ClipRectangle abs;
   WORD i, j;
   UBYTE skip;
   OBJECTID parent_id;

   if ((Width < 1) OR (Height < 1)) return ERR_Okay;
   if (!SurfaceID) return PostError(ERR_NullArgs);
   if (index >= Total) return PostError(ERR_OutOfRange);

   if ((!(list[index].Flags & RNF_VISIBLE)) OR ((list[index].Width < 1) OR (list[index].Height < 1))) {
      FMSG("ExposeSurface()","Surface %d invisible or too small to draw.", SurfaceID);
      return ERR_Okay;
   }

   // Calculate the absolute coordinates of the exposed area

   if (Flags & EXF_ABSOLUTE) {
      abs.Left   = X;
      abs.Top    = Y;
      abs.Right  = Width;
      abs.Bottom = Height;
      Flags &= ~EXF_ABSOLUTE;
   }
   else {
      abs.Left   = list[index].Left + X;
      abs.Top    = list[index].Top + Y;
      abs.Right  = abs.Left + Width;
      abs.Bottom = abs.Top  + Height;
   }

   FMSG("~ExposeSurface()","Surface:%d, %dx%d,%dx%d Flags: $%.4x", SurfaceID, abs.Left, abs.Top, abs.Right-abs.Left, abs.Bottom-abs.Top, Flags);

   // If the object is transparent, we need to scan back to a visible parent

   if (list[index].Flags & (RNF_TRANSPARENT|RNF_REGION)) {
      FMSG("ExposeSurface:","Surface is %s; scan to solid starting from index %d.", (list[index].Flags & RNF_REGION) ? "a region" : "invisible", index);

      OBJECTID id = list[index].SurfaceID;
      for (j=index; j > 0; j--) {
         if (list[j].SurfaceID != id) continue;
         if (list[j].Flags & (RNF_TRANSPARENT|RNF_REGION)) id = list[j].ParentID;
         else break;
      }
      Flags |= EXF_CHILDREN;
      index = j;

      FMSG("ExposeSurface:","New index %d.", index);
   }

   // Check if the exposed dimensions are outside of our boundary and/or our parent(s) boundaries.  If so then we must
   // restrict the exposed dimensions.  NOTE: This loop looks strange but is both correct & fast.  Don't touch it!

   for (i=index, parent_id = SurfaceID; ;) {
      if (!(list[i].Flags & RNF_VISIBLE)) goto exit;
      ClipRectangle(&abs, (struct ClipRectangle *)&list[i].Left);
      if (!(parent_id = list[i].ParentID)) break;
      i--;
      while (list[i].SurfaceID != parent_id) i--;
   }

   if ((abs.Left >= abs.Right) OR (abs.Top >= abs.Bottom)) goto exit;

   // Check that the expose area actually overlaps the target surface

   if (abs.Left   >= list[index].Right) goto exit;
   if (abs.Top    >= list[index].Bottom) goto exit;
   if (abs.Right  <= list[index].Left) goto exit;
   if (abs.Bottom <= list[index].Top) goto exit;

   // Cursor split routine.  The purpose of this is to eliminate as much flicker as possible from the cursor when
   // exposing large areas.
   //
   // We scan for the software cursor to see if the bottom of the cursor intersects with our expose region.  If it
   // does, split ExposeSurface() into top and bottom regions.

#ifndef _WIN32
   if (!(Flags & EXF_CURSOR_SPLIT)) {
      WORD cursor;

      for (cursor=index+1; (cursor < Total) AND (!(list[cursor].Flags & RNF_CURSOR)); cursor++);
      if (cursor < Total) {
         if ((list[cursor].SurfaceID) AND (list[cursor].Bottom < abs.Bottom) AND (list[cursor].Bottom > abs.Top) AND
             (list[cursor].Right > abs.Left) AND (list[cursor].Left < abs.Right)) {
            FMSG("~ExposeSurface:","Splitting cursor.");

            _expose_surface(SurfaceID, list, index, Total, abs.Left, abs.Top, abs.Right, list[cursor].Bottom, EXF_CURSOR_SPLIT|EXF_ABSOLUTE|Flags);
            _expose_surface(SurfaceID, list, index, Total, abs.Left, list[cursor].Bottom, abs.Right, abs.Bottom, EXF_CURSOR_SPLIT|EXF_ABSOLUTE|Flags);

            STEP();
            goto exit;
         }
      }
   }
#endif

   // The expose routine starts from the front and works to the back, so if the EXF_CHILDREN flag has been specified,
   // the first thing we do is scan to the final child that is listed in this particular area.

   if (Flags & EXF_CHILDREN) {
      // Change the index to the root bitmap of the exposed object
      index = FindBitmapOwner(list, index);
      for (i=index; (i < Total-1) AND (list[i+1].Level > list[index].Level); i++); // Go all the way to the end of the list
   }
   else i = index;

   for (; i >= index; i--) {
      // Ignore regions and non-visible surfaces

      if (list[i].Flags & (RNF_REGION|RNF_TRANSPARENT)) continue;

      if ((list[i].Flags & RNF_CURSOR) AND (list[i].SurfaceID != SurfaceID)) continue;

      // If this is not a root bitmap object, skip it (i.e. consider it like a region)

      skip = FALSE;
      parent_id = list[i].ParentID;
      for (j=i-1; j >= index; j--) {
         if (list[j].SurfaceID IS parent_id) {
            if (list[j].BitmapID IS list[i].BitmapID) skip = TRUE;
            break;
         }
      }
      if (skip) continue;

      struct ClipRectangle childexpose = abs;

      if (i != index) {
         // Check this child object and its parents to make sure they are visible

         parent_id = list[i].SurfaceID;
         for (j=i; (j >= index) AND (parent_id); j--) {
            if (list[j].SurfaceID IS parent_id) {
               if (!(list[j].Flags & RNF_VISIBLE)) {
                  skip = TRUE;
                  break;
               }

               ClipRectangle(&childexpose, (struct ClipRectangle *)&list[j].Left);

               parent_id = list[j].ParentID;
            }
         }
         if (skip) continue;

         // Skip this surface if there is nothing to be seen (lies outside the expose boundary)

         if ((childexpose.Right <= childexpose.Left) OR (childexpose.Bottom <= childexpose.Top)) continue;
      }

      // Do the expose

      ERROR error;
      if (!(error = AccessObject(list[i].BitmapID, 2000, &bitmap))) {
         expose_buffer(list, Total, i, i, childexpose.Left, childexpose.Top, childexpose.Right, childexpose.Bottom, list[index].DisplayID, bitmap);
         ReleaseObject(bitmap);
      }
      else {
         FMSG("ExposeSurface:","Unable to access internal bitmap, sending delayed expose message.  Error: %s", GetErrorMsg(error));

         struct drwExpose expose = {
            .X      = childexpose.Left   - list[i].Left,
            .Y      = childexpose.Top    - list[i].Top,
            .Width  = childexpose.Right  - childexpose.Left,
            .Height = childexpose.Bottom - childexpose.Top,
            .Flags  = 0
         };
         DelayMsg(MT_DrwExpose, list[i].SurfaceID, &expose);
      }
   }

   // These flags should be set if the surface has had some area of it redrawn prior to the ExposeSurface() call.
   // This can be very important if the application has been writing to the surface directly rather than the more
   // conventional drawing procedures.

   // If the surface bitmap has not been changed, volatile redrawing just wastes CPU time for the user.

   if (Flags & (EXF_REDRAW_VOLATILE|EXF_REDRAW_VOLATILE_OVERLAP)) {
      // Redraw any volatile regions that intersect our expose area (such regions must be updated to reflect the new
      // background graphics).  Note that this routine does a fairly deep scan, due to the selective area copying
      // features in our system (i.e. we cannot just skim over the stuff that is immediately in front of us).
      //
      // EXF_REDRAW_VOLATILE: Redraws every single volatile object that intersects the expose, including internal
      //    volatile children.
      //
      // EXF_REDRAW_VOLATILE_OVERLAP: Only redraws volatile objects that obscure the expose from a position outside of
      //    the surface and its children.  Useful if no redrawing has occurred internally, but the surface object has
      //    been moved to a new position and the parents need to be redrawn.

      WORD level = list[index].Level + 1;

      if ((Flags & EXF_REDRAW_VOLATILE_OVERLAP)) { //OR (Flags & EXF_CHILDREN)) {
         // All children in our area have already been redrawn or do not need redrawing, so skip past them.

         for (i=index+1; (i < Total) AND (list[i].Level > list[index].Level); i++);
         if (list[i-1].Flags & RNF_CURSOR) i--; // Never skip past the cursor
      }
      else {
         i = index;
         if (i < Total) i = i + 1;
         while ((i < Total) AND (list[i].BitmapID IS list[index].BitmapID)) i++;
      }

      FMSG("~ExposeSurface","Redraw volatiles from idx %d, area %dx%d,%dx%d", i, abs.Left, abs.Top, abs.Right - abs.Left, abs.Bottom - abs.Top);

      if (i < tlVolatileIndex) i = tlVolatileIndex; // Volatile index allows the starting point to be specified

      // Redraw and expose volatile overlaps

      for (; (i < Total) AND (list[i].Level > 1); i++) {
         if (list[i].Level < level) level = list[i].Level; // Drop the comparison level down so that we only observe objects in our general drawing space

         if (!(list[i].Flags & RNF_VISIBLE)) {
            j = list[i].Level;
            while ((i+1 < Total) AND (list[i+1].Level > j)) i++;
            continue;
         }

         if (list[i].Flags & (RNF_VOLATILE|RNF_COMPOSITE|RNF_CURSOR)) {
            if (list[i].SurfaceID IS SurfaceID) continue;

            if ((list[i].Right > abs.Left) AND (list[i].Bottom > abs.Top) AND
                (list[i].Left < abs.Right) AND (list[i].Top < abs.Bottom)) {

               if ((list[i].TaskID != CurrentTaskID()) AND (!(list[i].Flags & RNF_COMPOSITE))) {
                  // If the surface belongs to a different task, just post a redraw message to it.
                  // There is no point in performing an expose until it redraws itself.

                  _redraw_surface(list[i].SurfaceID, list, i, Total, abs.Left, abs.Top, abs.Right, abs.Bottom, IRF_IGNORE_CHILDREN); // Redraw the volatile surface, ignore children
               }
               else {
                  if (!(list[i].Flags & RNF_COMPOSITE)) { // Composites never require redrawing because they are not completely volatile, but we will expose them
                     _redraw_surface(list[i].SurfaceID, list, i, Total, abs.Left, abs.Top, abs.Right, abs.Bottom, IRF_IGNORE_CHILDREN); // Redraw the volatile surface, ignore children
                  }

                  _expose_surface(list[i].SurfaceID, list, i, Total, abs.Left, abs.Top, abs.Right, abs.Bottom, EXF_ABSOLUTE); // Redraw the surface, ignore children
               }

               //while (list[i].BitmapID IS list[i+1].BitmapID) i++; This only works if the surfaces being skipped are completely intersecting one another.
            }
         }
      }

      STEP();
   }
   else {
      // Look for a software cursor at the end of the surfacelist and redraw it.  (We have to redraw the cursor as
      // expose_buffer() ignores it for optimisation purposes.)

      i = Total - 1;
      if ((list[i].Flags & RNF_CURSOR) AND (list[i].SurfaceID != SurfaceID)) {
         if ((list[i].Right > abs.Left) AND (list[i].Bottom > abs.Top) AND
             (list[i].Left < abs.Right) AND (list[i].Top < abs.Bottom)) {

            FMSG("~ExposeSurface:","Redrawing/Exposing cursor.");

            if (!(list[i].Flags & RNF_COMPOSITE)) { // Composites never require redrawing because they are not completely volatile
               _redraw_surface(list[i].SurfaceID, list, i, Total, abs.Left, abs.Top, abs.Right, abs.Bottom, NULL);
            }

            _expose_surface(list[i].SurfaceID, list, i, Total, abs.Left, abs.Top, abs.Right, abs.Bottom, EXF_ABSOLUTE);

            STEP();
         }
      }
   }

exit:
   STEP();
   return ERR_Okay;
}

//****************************************************************************

static void expose_buffer(struct SurfaceList *list, WORD Total, WORD Index, WORD ScanIndex, LONG Left, LONG Top,
                         LONG Right, LONG Bottom, OBJECTID DisplayID, objBitmap *Bitmap)
{
   // Scan for overlapping parent/sibling regions and avoid them

   LONG i, j;
   for (i=ScanIndex+1; (i < Total) AND (list[i].Level > 1); i++) {

      // Skip past non-visible areas and their content

      if (!(list[i].Flags & RNF_VISIBLE)) {
         j = list[i].Level;
         while ((i+1 < Total) AND (list[i+1].Level > j)) i++;
         continue;
      }

      if (list[i].Flags & (RNF_REGION|RNF_CURSOR)) goto skip; // Skip regions and the cursor

      struct ClipRectangle listclip = {
         .Left   = list[i].Left,
         .Top    = list[i].Top,
         .Right  = list[i].Right,
         .Bottom = list[i].Bottom
      };

      if (restrict_region_to_parents(list, i, &listclip, FALSE) IS -1) goto skip;

      if ((listclip.Left < Right) AND (listclip.Top < Bottom) AND (listclip.Right > Left) AND (listclip.Bottom > Top)) {
         if (list[i].BitmapID IS list[Index].BitmapID) continue; // Ignore any children that overlap & form part of our bitmap space.  Children that do not overlap are skipped.

         if (listclip.Left <= Left) listclip.Left = Left;
         else expose_buffer(list, Total, Index, ScanIndex, Left, Top, listclip.Left, Bottom, DisplayID, Bitmap); // left

         if (listclip.Right >= Right) listclip.Right = Right;
         else expose_buffer(list, Total, Index, ScanIndex, listclip.Right, Top, Right, Bottom, DisplayID, Bitmap); // right

         if (listclip.Top <= Top) listclip.Top = Top;
         else expose_buffer(list, Total, Index, ScanIndex, listclip.Left, Top, listclip.Right, listclip.Top, DisplayID, Bitmap); // top

         if (listclip.Bottom < Bottom) expose_buffer(list, Total, Index, ScanIndex, listclip.Left, listclip.Bottom, listclip.Right, Bottom, DisplayID, Bitmap); // bottom

         if (list[i].Flags & RNF_TRANSPARENT) {
            // In the case of invisible regions, we will have split the expose process as normal.  However,
            // we also need to look deeper into the invisible region to discover if there is more that
            // we can draw, depending on the content of the invisible region.

            listclip.Left   = list[i].Left;
            listclip.Top    = list[i].Top;
            listclip.Right  = list[i].Right;
            listclip.Bottom = list[i].Bottom;

            if (Left > listclip.Left)     listclip.Left   = Left;
            if (Top > listclip.Top)       listclip.Top    = Top;
            if (Right < listclip.Right)   listclip.Right  = Right;
            if (Bottom < listclip.Bottom) listclip.Bottom = Bottom;

            expose_buffer(list, Total, Index, i, listclip.Left, listclip.Top, listclip.Right, listclip.Bottom, DisplayID, Bitmap);
         }

         return;
      }

      // Skip past any children of the non-overlapping object.  This ensures that we only look at immediate parents and siblings that are in our way.

skip:
      j = i + 1;
      while ((j < Total) AND (list[j].Level > list[i].Level)) j++;
      i = j - 1;
   }

   FMSG("~expose_buffer()","[%d] %dx%d,%dx%d Bmp: %d, Idx: %d/%d", list[Index].SurfaceID, Left, Top, Right - Left, Bottom - Top, list[Index].BitmapID, Index, ScanIndex);

   // The region is not obscured, so perform the redraw

   LONG owner = FindBitmapOwner(list, Index);

   // Turn off offsets and set the clipping to match the source bitmap exactly (i.e. nothing fancy happening here).
   // The real clipping occurs in the display clip.

   Bitmap->XOffset = 0;
   Bitmap->YOffset = 0;

   Bitmap->Clip.Left   = list[Index].Left - list[owner].Left;
   Bitmap->Clip.Top    = list[Index].Top - list[owner].Top;
   Bitmap->Clip.Right  = list[Index].Right - list[owner].Left;
   Bitmap->Clip.Bottom = list[Index].Bottom - list[owner].Top;
   if (Bitmap->Clip.Right  > Bitmap->Width)  Bitmap->Clip.Right  = Bitmap->Width;
   if (Bitmap->Clip.Bottom > Bitmap->Height) Bitmap->Clip.Bottom = Bitmap->Height;

   // Set the clipping so that we are only drawing to the display area that has been exposed

   LONG iscr = Index;
   while ((iscr > 0) AND (list[iscr].ParentID)) iscr--; // Find the top-level display entry

   // If COMPOSITE is in use, this means we have to do compositing on the fly.  This involves copying the background
   // graphics into a temporary buffer, then blitting the composite buffer to the display.

   // Note: On hosted displays in Windows or Linux, compositing is handled by the host's graphics system if the surface
   // is at the root level (no ParentID).

   LONG sx, sy;
   if ((list[Index].Flags & RNF_COMPOSITE) AND
       ((list[Index].ParentID) OR (list[Index].Flags & RNF_CURSOR))) {
      struct ClipRectangle clip;
      if (glComposite) {
         if (glComposite->BitsPerPixel != list[Index].BitsPerPixel) {
            acFree(glComposite);
            glComposite = NULL;
         }
         else {
            if ((glComposite->Width < list[Index].Width) OR (glComposite->Height < list[Index].Height)) {
               acResize(glComposite, (list[Index].Width > glComposite->Width) ? list[Index].Width : glComposite->Width,
                                     (list[Index].Height > glComposite->Height) ? list[Index].Height : glComposite->Height,
                                     0);
            }
         }
      }

      if (!glComposite) {
         if (CreateObject(ID_BITMAP, NF_UNTRACKED, &glComposite,
               FID_Width|TLONG,  list[Index].Width,
               FID_Height|TLONG, list[Index].Height,
               TAGEND) != ERR_Okay) {
            STEP();
            return;
         }

         SetOwner(glComposite, modSurface);
      }

      // Build the background in our buffer

      clip.Left   = Left;
      clip.Top    = Top;
      clip.Right  = Right;
      clip.Bottom = Bottom;
      prepare_background(NULL, list, Total, Index, glComposite, &clip, STAGE_COMPOSITE);

      // Blend the surface's graphics into the composited buffer
      // NOTE: THE FOLLOWING IS NOT OPTIMISED WITH RESPECT TO CLIPPING

      gfxCopyArea(Bitmap, glComposite, BAF_BLEND, 0, 0, list[Index].Width, list[Index].Height, 0, 0);

      Bitmap = glComposite;
      sx = 0;  // Always zero as composites own their bitmap
      sy = 0;
   }
   else {
      sx = list[Index].Left - list[owner].Left;
      sy = list[Index].Top - list[owner].Top;
   }

   objDisplay *display;
   objBitmap *video_bmp;
   if (!access_video(DisplayID, &display, &video_bmp)) {
      video_bmp->XOffset = 0;
      video_bmp->YOffset = 0;

      video_bmp->Clip.Left   = Left - list[iscr].Left; // Ensure that the coords are relative to the display bitmap (important for Windows, X11)
      video_bmp->Clip.Top    = Top - list[iscr].Top;
      video_bmp->Clip.Right  = Right - list[iscr].Left;
      video_bmp->Clip.Bottom = Bottom - list[iscr].Top;
      if (video_bmp->Clip.Left < 0) video_bmp->Clip.Left = 0;
      if (video_bmp->Clip.Top  < 0) video_bmp->Clip.Top  = 0;
      if (video_bmp->Clip.Right  > video_bmp->Width) video_bmp->Clip.Right   = video_bmp->Width;
      if (video_bmp->Clip.Bottom > video_bmp->Height) video_bmp->Clip.Bottom = video_bmp->Height;

      gfxUpdateDisplay(display, Bitmap, sx, sy, // Src X/Y (bitmap relative)
         list[Index].Width, list[Index].Height,
         list[Index].Left - list[iscr].Left, list[Index].Top - list[iscr].Top); // Dest X/Y (absolute display position)

      release_video(display);
   }
   else LogF("@ExposeSurface:","Unable to access display #%d.", DisplayID);

   STEP();
   return;
}

/*****************************************************************************

-FUNCTION-
ForbidDrawing: Temporarily turns off all drawing operations for the calling thread.

The ForbidDrawing() function turns off all attempts to draw or expose rendered surfaces.  Any function or method that
involves drawing will return immediately with an ERR_Okay error code while the forbid is in effect.

Turning off display exposures is often useful when sequential drawing operations would perform best if covered by a
single mass redraw.

This function call must be followed up with a call to PermitDrawing() at some stage if the graphics system is to return
to its normal state of operation.

Calls to ForbidDrawing() will nest.  This function is thread safe and there is no impact on the graphics operations of
other threads.

*****************************************************************************/

static void drwForbidDrawing(void)
{
   tlNoDrawing++;
   tlNoExpose++;
}

/*****************************************************************************

-FUNCTION-
ForbidExpose: Temporarily turns off display exposures.

The ForbidExpose() function turns off all future display exposures attempted by the calling process.  This effectively
means that all calls to ExposeSurface() will return immediately (with ERR_Okay status).

Turning off display exposures is often useful when performing a number of tasks (such as resizing many small surfaces)
that will result in multiple exposures.  This may potentially cause display flickering that could be optimised out of
the system if the exposures were covered manually with a single ExposeSurface() call.  Using ForbidExpose() to turn off
the exposes will allow you to achieve such an optimisation.

This function call must be followed up with a call to PermitExpose() at some stage if the graphics system is to return
to its normal state of operation.

Repeated calls to this function will nest.

*****************************************************************************/

static void drwForbidExpose(void)
{
   tlNoExpose++;
}

/*****************************************************************************

-FUNCTION-
GetSurfaceCoords: Returns the dimensions of a surface.

GetSurfaceCoords() retrieves the dimensions that describe a surface object's area as X, Y, Width and Height.  This is
the fastest way to retrieve surface dimensions when access to the object structure is not already available.

-INPUT-
oid Surface: The surface to query.  If zero, the top-level display is queried.
&int X: The X coordinate of the surface is returned here.
&int Y: The Y coordinate of the surface is returned here.
&int AbsX: The absolute X coordinate of the surface is returned here.
&int AbsY: The absolute Y coordinate of the surface is returned here.
&int Width: The width of the surface is returned here.
&int Height: The height of the surface is returned here.

-ERRORS-
Okay
Search: The supplied SurfaceID did not refer to a recognised surface object.
AccessMemory: Failed to access the internal surfacelist memory structure.

*****************************************************************************/

static ERROR drwGetSurfaceCoords(OBJECTID SurfaceID, LONG *X, LONG *Y, LONG *AbsX, LONG *AbsY, LONG *Width, LONG *Height)
{
   if (!SurfaceID) {
      DISPLAYINFO *display;
      if (!gfxGetDisplayInfo(0, &display)) {
         if (X) *X = 0;
         if (Y) *Y = 0;
         if (AbsX)   *AbsX = 0;
         if (AbsY)   *AbsY = 0;
         if (Width)  *Width  = display->Width;
         if (Height) *Height = display->Height;
         return ERR_Okay;
      }
      else return ERR_Failed;
   }

   struct SurfaceControl *ctl;
   WORD i;

   if ((ctl = drwAccessList(ARF_READ))) {
      struct SurfaceList *list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex);
      if ((i = find_surface_index(ctl, SurfaceID)) IS -1) {
         drwReleaseList(ARF_READ);
         return ERR_Search;
      }

      if (X)      *X = list[i].X;
      if (Y)      *Y = list[i].Y;
      if (Width)  *Width  = list[i].Width;
      if (Height) *Height = list[i].Height;
      if (AbsX)   *AbsX   = list[i].Left;
      if (AbsY)   *AbsY   = list[i].Top;

      drwReleaseList(ARF_READ);
      return ERR_Okay;
   }
   else return FuncError(ERR_AccessMemory);
}

/*****************************************************************************

-FUNCTION-
GetVisibleArea: Returns the visible region of a surface.

The GetVisibleArea() function returns the visible area of a surface, which is based on its position within its parent
surfaces. The resulting coordinates are relative to point 0,0 of the queried surface. If the surface is not obscured,
then the resulting coordinates will be (0,0),(Width,Height).

-INPUT-
oid Surface: The surface to query.  If zero, the top-level display will be queried.
&int X: The X coordinate of the visible area.
&int Y: The Y coordinate of the visible area.
&int AbsX: The absolute X coordinate of the visible area.
&int AbsY: The absolute Y coordinate of the visible area.
&int Width: The visible width of the surface.
&int Height: The visible height of the surface.

-ERRORS-
Okay
Search: The supplied SurfaceID did not refer to a recognised surface object.
AccessMemory: Failed to access the internal surfacelist memory structure.

*****************************************************************************/

static ERROR drwGetVisibleArea(OBJECTID SurfaceID, LONG *X, LONG *Y, LONG *AbsX, LONG *AbsY, LONG *Width, LONG *Height)
{
   if (!SurfaceID) {
      DISPLAYINFO *display;
      if (!gfxGetDisplayInfo(0, &display)) {
         if (X) *X = 0;
         if (Y) *Y = 0;
         if (Width)  *Width = display->Width;
         if (Height) *Height = display->Height;
         if (AbsX)   *AbsX = 0;
         if (AbsY)   *AbsY = 0;
         return ERR_Okay;
      }
      else return ERR_Failed;
   }

   struct SurfaceControl *ctl;
   if ((ctl = drwAccessList(ARF_READ))) {
      struct SurfaceList *list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex);

      WORD i;
      if ((i = find_surface_index(ctl, SurfaceID)) IS -1) {
         drwReleaseList(ARF_READ);
         return ERR_Search;
      }

      struct ClipRectangle clip;
      clip.Left   = list[i].Left;
      clip.Top    = list[i].Top;
      clip.Right  = list[i].Right;
      clip.Bottom = list[i].Bottom;
      restrict_region_to_parents(list, i, &clip, FALSE);

      if (X) *X = clip.Left - list[i].Left;
      if (Y) *Y = clip.Top - list[i].Top;
      if (Width)  *Width  = clip.Right - clip.Left;
      if (Height) *Height = clip.Bottom - clip.Top;
      if (AbsX)   *AbsX   = clip.Left;
      if (AbsY)   *AbsY   = clip.Top;

      drwReleaseList(ARF_READ);
      return ERR_Okay;
   }
   else return FuncError(ERR_AccessMemory);
}

/*****************************************************************************

-FUNCTION-
GetSurfaceFlags: Retrieves the Flags field from a surface.

This function returns the current Flags field from a surface.  It provides the same result as reading the field
directly, however it is considered advantageous in circumstances where the overhead of locking a surface object for a
read operation is undesirable.

For information on the available flags, please refer to the Flags field of the <class>Surface</class> class.

-INPUT-
oid Surface: The surface to query.  If zero, the top-level surface is queried.
&int Flags: The flags value is returned here.

-ERRORS-
Okay
NullArgs
AccessMemory

*****************************************************************************/

static ERROR drwGetSurfaceFlags(OBJECTID SurfaceID, LONG *Flags)
{
   if (Flags) *Flags = 0;
   else return FuncError(ERR_NullArgs);

   if (!SurfaceID) return FuncError(ERR_NullArgs);

   struct SurfaceControl *ctl;
   if ((ctl = drwAccessList(ARF_READ))) {
      struct SurfaceList *list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex);

      LONG i;
      if ((i = find_surface_index(ctl, SurfaceID)) IS -1) {
         drwReleaseList(ARF_READ);
         return ERR_Search;
      }

      *Flags = list[i].Flags;

      drwReleaseList(ARF_READ);
      return ERR_Okay;
   }
   else return FuncError(ERR_AccessMemory);
}

/*****************************************************************************

-FUNCTION-
GetSurfaceInfo: Retrieves display information for any surface object without having to access it directly.

GetSurfaceInfo() is used for quickly retrieving basic information from surfaces, allowing the client to bypass the
AccessObject() function.  The resulting structure values are good only up until the next call to this function,
at which point those values will be overwritten.

-INPUT-
oid Surface: The unique ID of a surface to query.  If zero, the root surface is returned.
&struct(SurfaceInfo) Info: This parameter will receive a SurfaceInfo pointer that describes the Surface object.

-ERRORS-
Okay:
Args:
Search: The supplied SurfaceID did not refer to a recognised surface object.
AccessMemory: Failed to access the internal surfacelist memory structure.

*****************************************************************************/

static ERROR drwGetSurfaceInfo(OBJECTID SurfaceID, SURFACEINFO **Info)
{
   static THREADVAR SURFACEINFO info;

   // Note that a SurfaceID of zero is fine (returns the root surface).

   if (!Info) return FuncError(ERR_NullArgs);

   struct SurfaceControl *ctl;
   if ((ctl = drwAccessList(ARF_READ))) {
      struct SurfaceList *list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex);
      WORD i, root;
      if (!SurfaceID) {
         i = 0;
         root = 0;
      }
      else {
         if ((i = find_surface_index(ctl, SurfaceID)) IS -1) {
            drwReleaseList(ARF_READ);
            return ERR_Search;
         }
         root = FindBitmapOwner(list, i);
      }

      info.ParentID  = list[i].ParentID;
      info.BitmapID  = list[i].BitmapID;
      info.DisplayID = list[i].DisplayID;
      info.DataMID   = list[root].DataMID;
      info.Flags     = list[i].Flags;
      info.X         = list[i].X;
      info.Y         = list[i].Y;
      info.Width     = list[i].Width;
      info.Height    = list[i].Height;
      info.AbsX      = list[i].Left;
      info.AbsY      = list[i].Top;
      info.Level     = list[i].Level;
      info.BytesPerPixel = list[root].BytesPerPixel;
      info.BitsPerPixel  = list[root].BitsPerPixel;
      info.LineWidth     = list[root].LineWidth;
      *Info = &info;

      drwReleaseList(ARF_READ);
      return ERR_Okay;
   }
   else {
      *Info = NULL;
      return FuncError(ERR_AccessMemory);
   }
}

/*****************************************************************************

-FUNCTION-
GetUserFocus: Returns the ID of the surface that currently has the user's focus.

This function returns the unique ID of the surface that has the user's focus.

-RESULT-
oid: Returns the ID of the surface object that has the user focus, or zero on failure.

*****************************************************************************/

static OBJECTID drwGetUserFocus(void)
{
   OBJECTID *focuslist, objectid;

   if (!AccessMemory(RPM_FocusList, MEM_READ, 1000, &focuslist)) {
      objectid = focuslist[0];
      ReleaseMemory(focuslist);
      return objectid;
   }
   else return NULL;
}

/*****************************************************************************

-FUNCTION-
LockBitmap: Returns a bitmap that represents the video area covered by the surface object.

Use the LockBitmap() function to gain direct access to the bitmap information of a surface object.
Because the layering buffer will be inaccessible to the UI whilst you retain the lock, you must keep your access time
to an absolute minimum or desktop performance may suffer.

Repeated calls to this function will nest.  To release a surface bitmap, call the ~UnlockBitmap() function.

-INPUT-
oid Surface:         Object ID of the surface object that you want to lock.
&obj(Bitmap) Bitmap: The resulting bitmap will be returned in this parameter.
&int(LVF) Info:      Special flags may be returned in this parameter.  If LVF_EXPOSE_CHANGES is returned, any changes must be exposed in order for them to be displayed to the user.

-ERRORS-
Okay
Args

*****************************************************************************/

static ERROR drwLockBitmap(OBJECTID SurfaceID, objBitmap **Bitmap, LONG *Info)
{
#if 0
   // This technique that we're using to acquire the bitmap is designed to prevent deadlocking.
   //
   // COMMENTED OUT: May be causing problems with X11?

   LONG i;

   if (Info) *Info = 0;

   if ((!SurfaceID) OR (!Bitmap)) return FuncError(ERR_NullArgs);

   *Bitmap = 0;

   objSurface *surface;
   if (!AccessObject(SurfaceID, 5000, &surface)) {
      objBitmap *bitmap;
      if (AccessObject(surface->BufferID, 5000, &bitmap) != ERR_Okay) {
         ReleaseObject(surface);
         return FuncError(ERR_AccessObject);
      }

      ReleaseObject(surface);

      struct SurfaceControl *ctl;
      if ((ctl = drwAccessList(ARF_READ))) {
         struct SurfaceList *list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex);

         WORD i;
         if ((i = find_surface_index(ctl, SurfaceID)) IS -1) {
            ReleaseObject(bitmap);
            drwReleaseList(ARF_READ);
            return ERR_Search;
         }

         LONG root = FindBitmapOwner(list, i);

         bitmap->XOffset     = list[i].Left - list[root].Left;
         bitmap->YOffset     = list[i].Top - list[root].Top;
         bitmap->Clip.Left   = 0;
         bitmap->Clip.Top    = 0;
         bitmap->Clip.Right  = list[i].Width;
         bitmap->Clip.Bottom = list[i].Height;

         if (Info) {
            // The developer will have to send an expose signal - unless the exposure can be gained for 'free'
            // (possible if the Draw action has been called on the Surface object).

            if (tlFreeExpose IS list[i].BitmapID);
            else *Info |= LVF_EXPOSE_CHANGES;
         }

         drwReleaseList(ARF_READ);

         if (bitmap->Clip.Right + bitmap->XOffset > bitmap->Width){
            bitmap->Clip.Right = bitmap->Width - bitmap->XOffset;
            if (bitmap->Clip.Right < 0) {
               ReleaseObject(bitmap);
               return ERR_Failed;
            }
         }

         if (bitmap->Clip.Bottom + bitmap->YOffset > bitmap->Height) {
            bitmap->Clip.Bottom = bitmap->Height - bitmap->YOffset;
            if (bitmap->ClipBottom < 0) {
               ReleaseObject(bitmap);
               return ERR_Failed;
            }
         }

         *Bitmap = bitmap;
         return ERR_Okay;
      }
      else {
         ReleaseObject(bitmap);
         return FuncError(ERR_AccessMemory);
      }
   }
   else return FuncError(ERR_AccessObject);

#else

   if (Info) *Info = NULL;

   if ((!SurfaceID) OR (!Bitmap)) return FuncError(ERR_NullArgs);

   struct SurfaceControl *ctl;
   if ((ctl = drwAccessList(ARF_READ))) {
      WORD i;
      if ((i = find_surface_index(ctl, SurfaceID)) IS -1) {
         drwReleaseList(ARF_READ);
         return ERR_Search;
      }

      struct SurfaceList *list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex);
      LONG root = FindBitmapOwner(list, i);

      struct SurfaceList list_root = list[root];
      struct SurfaceList list_zero = list[0];
      OBJECTID bitmap_id = list[i].BitmapID;

      struct ClipRectangle expose = {
         .Left   = list_root.Left,
         .Top    = list_root.Top,
         .Right  = list_root.Right,
         .Bottom = list_root.Bottom
      };

      if (restrict_region_to_parents(list, i, &expose, TRUE) IS -1) {
         // The surface is not within a visible area of the available bitmap space
         drwReleaseList(ARF_READ);
         return ERR_OutOfBounds;
      }

      drwReleaseList(ARF_READ);

      if (!list_root.BitmapID) return FuncError(ERR_Failed);

      // Gain access to the bitmap buffer and set the clipping and offsets to the correct values.

      objBitmap *bmp;
      if (!AccessObject(list_root.BitmapID, 5000, &bmp)) {
         bmp->XOffset = expose.Left - list_root.Left; // The offset is the position of the surface within the root bitmap
         bmp->YOffset = expose.Top - list_root.Top;

         expose.Left   -= list_zero.Left; // This adjustment is necessary for displays on hosted platforms (win32, X11)
         expose.Top    -= list_zero.Top;
         expose.Right  -= list_zero.Left;
         expose.Bottom -= list_zero.Top;

         bmp->Clip.Left   = expose.Left   - bmp->XOffset - (list_root.Left - list_zero.Left);
         bmp->Clip.Top    = expose.Top    - bmp->YOffset - (list_root.Top  - list_zero.Top);
         bmp->Clip.Right  = expose.Right  - bmp->XOffset - (list_root.Left - list_zero.Left);
         bmp->Clip.Bottom = expose.Bottom - bmp->YOffset - (list_root.Top  - list_zero.Top);

         if (Info) {
            // The developer will have to send an expose signal - unless the exposure can be gained for 'free'
            // (possible if the Draw action has been called on the Surface object).

            if (tlFreeExpose IS bitmap_id);
            else *Info |= LVF_EXPOSE_CHANGES;
         }

         *Bitmap = bmp;
         return ERR_Okay;
      }
      else return FuncError(ERR_AccessObject);
   }
   else return FuncError(ERR_AccessMemory);

#endif
}

/*****************************************************************************

-FUNCTION-
PermitDrawing: Releases locks imposed by the ForbidDrawing() function.

The PermitDrawing() function reverses the effect of the ~ForbidDrawing() function. Please refer to
~ForbidDrawing() for further information.

*****************************************************************************/

static void drwPermitDrawing(void)
{
   tlNoDrawing--;
   tlNoExpose--;
}

/*****************************************************************************

-FUNCTION-
PermitExpose: Reverses the ForbidExpose() function.

The PermitExpose() function reverses the effects of the ~ForbidExpose() function. Please refer to
~ForbidExpose() for further information.
-END-

*****************************************************************************/

static void drwPermitExpose(void)
{
   tlNoExpose--;
}

/*****************************************************************************

-INTERNAL-
RedrawSurface: Redraws all of the content in a surface object.

Invalidating a surface object will cause everything within a specified area to be redrawn.  This includes child surface
objects that intersect with the area that you have specified.  Overlapping siblings are not redrawn unless they are
marked as volatile.

To quickly redraw an entire surface object's content, call this method directly without supplying an argument structure.
To redraw a surface object and ignore all of its surface children, use the #Draw() action instead of this
function.

To expose the surface area to the display, use the ~ExposeSurface() function.  The ~ExposeSurface() function copies the
graphics buffer to the display only, thus avoiding the speed loss of a complete redraw.

Because RedrawSurface() only redraws internal graphics buffers, this function is typically followed with a call to
ExposeSurface().

Flag options:

&IRF

-INPUT-
oid Surface: The ID of the surface that you want to invalidate.
int Left:    Absolute horizontal coordinate of the region to invalidate.
int Top:     Absolute vertical coordinate of the region to invalidate.
int Right:   Absolute right-hand coordinate of the region to invalidate.
int Bottom:  Absolute bottom coordinate of the region to invalidate.
int(IRF) Flags: Optional flags.

-ERRORS-
Okay:
AccessMemory: Failed to access the internal surface list.

*****************************************************************************/

static ERROR drwRedrawSurface(OBJECTID SurfaceID, LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Flags)
{
   if (tlNoDrawing) {
      FMSG("RedrawSurface()","tlNoDrawing: %d", tlNoDrawing);
      return ERR_Okay;
   }

   struct SurfaceControl *ctl;
   if (!(ctl = drwAccessList(ARF_READ))) {
      LogF("@ExposeSurface()","Unable to access the surfacelist.");
      return ERR_AccessMemory;
   }

   LONG total = ctl->Total;
   struct SurfaceList list[total];
   CopyMemory((APTR)ctl + ctl->ArrayIndex, list, sizeof(list[0]) * ctl->Total);
   drwReleaseList(ARF_READ);

   WORD index;
   if ((index = find_surface_list(list, total, SurfaceID)) IS -1) {
      FMSG("@RedrawSurface:","Unable to find surface #%d in surface list.", SurfaceID);
      STEP();
      return ERR_Search;
   }

   return _redraw_surface(SurfaceID, list, index, total, Left, Top, Right, Bottom, Flags);
}

//****************************************************************************

static ERROR _redraw_surface(OBJECTID SurfaceID, struct SurfaceList *list, WORD index, WORD Total,
   LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Flags)
{
   static THREADVAR BYTE recursive = 0;

   if (list[index].Flags & RNF_TOTAL_REDRAW) {
      // If the TOTALREDRAW flag is set against the surface then the entire surface must be redrawn regardless
      // of the circumstances.  This is often required for algorithmic effects as seen in the Blur class.

      Left   = list[index].Left;
      Top    = list[index].Top;
      Right  = list[index].Right;
      Bottom = list[index].Bottom;
   }
   else if (Flags & IRF_RELATIVE) {
      Left   = list[index].Left + Left;
      Top    = list[index].Top + Top;
      Right  = Left + Right;
      Bottom = Top + Bottom;
      Flags &= ~IRF_RELATIVE;
   }

   FMSG("~RedrawSurface()","[%d] %d/%d Size: %dx%d,%dx%d Expose: %dx%d,%dx%d", SurfaceID, index, Total, list[index].Left, list[index].Top, list[index].Width, list[index].Height, Left, Top, Right-Left, Bottom-Top);

   if ((list[index].Flags & (RNF_REGION|RNF_TRANSPARENT)) AND (!recursive)) {
      FMSG("RedrawSurface","Passing draw request to parent (I am a %s)", (list[index].Flags & RNF_REGION) ? "region" : "invisible");
      WORD parent_index;
      if ((parent_index = find_surface_list(list, Total, list[index].ParentID)) != -1) {
         _redraw_surface(list[parent_index].SurfaceID, list, parent_index, Total, Left, Top, Right, Bottom, Flags & (~IRF_IGNORE_CHILDREN));
      }
      else FMSG("RedrawSurface","Failed to find parent surface #%d", list[index].ParentID); // No big deal, this often happens when freeing a bunch of surfaces due to the parent/child relationships.
      STEP();
      return ERR_Okay;
   }

   // Check if any of the parent surfaces are invisible

   if (!(Flags & IRF_FORCE_DRAW)) {
      if ((!(list[index].Flags & RNF_VISIBLE)) OR (CheckVisibility(list, index) IS FALSE)) {
         FMSG("RedrawSurface:","Surface is not visible.");
         STEP();
         return ERR_Okay;
      }
   }

   // Because we are executing a redraw, we need to ensure that the surface belongs to our process before going any further.

   if (list[index].TaskID != CurrentTaskID()) {
      FMSG("RedrawSurface:","Surface object #%d belongs to task #%d (we are #%d)", SurfaceID, list[index].TaskID, CurrentTaskID());

      LONG x = Left - list[index].Left;
      LONG y = Top - list[index].Top;
      if (Flags & IRF_IGNORE_CHILDREN) {
         acDrawAreaID(list[index].SurfaceID, x, y, Right - Left, Bottom - Top);
      }
      else drwInvalidateRegionID(list[index].SurfaceID, x, y, Right - Left, Bottom - Top);

      STEP();
      return ERR_Okay;
   }

   // Check if the exposed dimensions are outside of our boundary and/or our parent(s) boundaries.  If so then we must restrict the exposed dimensions.

   if (Flags & IRF_FORCE_DRAW) {
      if (Left   < list[index].Left)   Left   = list[index].Left;
      if (Top    < list[index].Top)    Top    = list[index].Top;
      if (Right  > list[index].Right)  Right  = list[index].Right;
      if (Bottom > list[index].Bottom) Bottom = list[index].Bottom;
   }
   else {
      OBJECTID parent_id = SurfaceID;
      WORD i = index;
      while (parent_id) {
         while ((list[i].SurfaceID != parent_id) AND (i > 0)) i--;

         if (list[i].BitmapID != list[index].BitmapID) break; // Stop if we encounter a separate bitmap

         if (Left   < list[i].Left)   Left   = list[i].Left;
         if (Top    < list[i].Top)    Top    = list[i].Top;
         if (Right  > list[i].Right)  Right  = list[i].Right;
         if (Bottom > list[i].Bottom) Bottom = list[i].Bottom;

         parent_id = list[i].ParentID;
      }
   }

   if ((Left >= Right) OR (Top >= Bottom)) {
      STEP();
      return ERR_Okay;
   }

   // Draw the surface graphics into the bitmap buffer

   objSurface *surface;
   ERROR error;
   if (!(error = AccessObject(list[index].SurfaceID, 5000, &surface))) {
      FMSG("RedrawSurface","Area: %dx%d,%dx%d", Left, Top, Right-Left, Bottom-Top);

      objBitmap *bitmap;
      if (!AccessObject(list[index].BitmapID, 5000, &bitmap)) {
         // Check if there has been a change in the video bit depth.  If so, regenerate the bitmap with a matching depth.

         check_bmp_buffer_depth(surface, bitmap);
         _redraw_surface_do(surface, list, Total, index, Left, Top, Right, Bottom, bitmap, (Flags & IRF_FORCE_DRAW) | ((Flags & (IRF_IGNORE_CHILDREN|IRF_IGNORE_NV_CHILDREN)) ? 0 : URF_HATE_CHILDREN));
         ReleaseObject(bitmap);
      }
      else {
         ReleaseObject(surface);
         STEP();
         return FuncError(ERR_AccessObject);
      }

      ReleaseObject(surface);
   }
   else {
      // If the object does not exist then its task has crashed and we need to remove it from the surface list.

      if (error IS ERR_NoMatchingObject) {
         LogF("@RedrawSurface","Removing references to surface object #%d (owner crashed).", list[index].SurfaceID);
         untrack_layer(list[index].SurfaceID);
      }
      else LogF("@RedrawSurface","Unable to access surface object #%d, error %d.", list[index].SurfaceID, error);

      STEP();
      return error;
   }

   // We have done the redraw, so now we can send invalidation messages to any intersecting -child- surfaces for this region.  This process is
   // not recursive (notice the use of IRF_IGNORE_CHILDREN) but all children will be covered due to the way the tree is traversed.

   if (!(Flags & IRF_IGNORE_CHILDREN)) {
      FMSG("RedrawSurface:","Redrawing intersecting child surfaces.");
      WORD level = list[index].Level;
      WORD i;
      for (i=index+1; i < Total; i++) {
         if (list[i].Level <= level) break; // End of list - exit this loop

         if (Flags & IRF_IGNORE_NV_CHILDREN) {
            // Ignore children except for those that are volatile
            if (!(list[i].Flags & RNF_VOLATILE)) continue;
         }
         else {
            if ((Flags & IRF_SINGLE_BITMAP) AND (list[i].BitmapID != list[index].BitmapID)) continue;
         }

         if ((list[i].Flags & (RNF_REGION|RNF_CURSOR)) OR (!(list[i].Flags & RNF_VISIBLE))) {
            continue; // Skip regions and non-visible children
         }

         if ((list[i].Right > Left) AND (list[i].Bottom > Top) AND
             (list[i].Left < Right) AND (list[i].Top < Bottom)) {
            recursive++;
            _redraw_surface(list[i].SurfaceID, list, i, Total, Left, Top, Right, Bottom, Flags|IRF_IGNORE_CHILDREN);
            recursive--;
         }
      }
   }

   STEP();
   return ERR_Okay;
}

//****************************************************************************
// This function fulfils the recursive drawing requirements of _redraw_surface() and is not intended for any other use.

static void _redraw_surface_do(objSurface *Self, struct SurfaceList *list, WORD Total, WORD Index,
                               LONG Left, LONG Top, LONG Right, LONG Bottom, objBitmap *DestBitmap, LONG Flags)
{
   if (Self->Flags & (RNF_REGION|RNF_TRANSPARENT)) return;

   if (Index >= Total) LogF("@redraw_surface","Index %d > %d", Index, Total);

   struct ClipRectangle abs;
   abs.Left   = Left;
   abs.Top    = Top;
   abs.Right  = Right;
   abs.Bottom = Bottom;
   if (abs.Left   < list[Index].Left)   abs.Left   = list[Index].Left;
   if (abs.Top    < list[Index].Top)    abs.Top    = list[Index].Top;
   if (abs.Right  > list[Index].Right)  abs.Right  = list[Index].Right;
   if (abs.Bottom > list[Index].Bottom) abs.Bottom = list[Index].Bottom;

   WORD i;
   if (!(Flags & IRF_FORCE_DRAW)) {
      LONG level = list[Index].Level + 1;   // The +1 is used to include children contained in the surface object

      for (i=Index+1; (i < Total) AND (list[i].Level > 1); i++) {
         if (list[i].Level < level) level = list[i].Level;

         // If the listed object obscures our surface area, analyse the region around it

         if (list[i].Level <= level) {
            // If we have a bitmap buffer and the underlying child region also has its own bitmap,
            // we have to ignore it in order for our graphics buffer to be correct when exposes are made.

            if (list[i].BitmapID != Self->BufferID) continue;
            if (!(list[i].Flags & RNF_VISIBLE)) continue;
            if (list[i].Flags & RNF_REGION) continue; // Regions are completely ignored because it is impossible for them to contain true surface layers

            // Check for an intersection and respond to it

            LONG listx      = list[i].Left;
            LONG listy      = list[i].Top;
            LONG listright  = list[i].Right;
            LONG listbottom = list[i].Bottom;

            if ((listx < Right) AND (listy < Bottom) AND (listright > Left) AND (listbottom > Top)) {
               if (list[i].Flags & RNF_CURSOR) {
                  // Objects like the pointer cursor are ignored completely.  They are redrawn following exposure.

                  return;
               }
               else if (list[i].Flags & RNF_TRANSPARENT) {
                  // If the surface object is see-through then we will ignore its bounds, but legally
                  // it can also contain child surface objects that are solid.  For that reason,
                  // we have to 'go inside' to check for solid children and draw around them.

                  _redraw_surface_do(Self, list, Total, i, Left, Top, Right, Bottom, DestBitmap, Flags);
                  return;
               }

               if ((Flags & URF_HATE_CHILDREN) AND (list[i].Level > list[Index].Level)) {
                  // The HATE_CHILDREN flag is used if the caller intends to redraw all children surfaces.
                  // In this case, we may as well ignore children when they are smaller than 100x100 in size,
                  // because splitting our drawing process into four sectors is probably going to be slower
                  // than just redrawing the entire background in one shot.

                  if (list[i].Width + list[i].Height <= 200) continue;
               }

               if (listx <= Left) listx = Left;
               else _redraw_surface_do(Self, list, Total, Index, Left, Top, listx, Bottom, DestBitmap, Flags); // left

               if (listright >= Right) listright = Right;
               else _redraw_surface_do(Self, list, Total, Index, listright, Top, Right, Bottom, DestBitmap, Flags); // right

               if (listy <= Top) listy = Top;
               else _redraw_surface_do(Self, list, Total, Index, listx, Top, listright, listy, DestBitmap, Flags); // top

               if (listbottom < Bottom) _redraw_surface_do(Self, list, Total, Index, listx, listbottom, listright, Bottom, DestBitmap, Flags); // bottom

               return;
            }
         }
      }
   }

   FMSG("~RedrawSurface:","Index %d, %dx%d,%dx%d", Index, Left, Top, Right-Left, Bottom-Top);

   // If we have been called recursively due to the presence of volatile/invisible regions (see above),
   // our Index field will not match with the surface that is referenced in Self.  We need to ensure
   // correctness before going any further.

   if (list[Index].SurfaceID != Self->Head.UniqueID) {
      Index = find_surface_list(list, Total, Self->Head.UniqueID);
   }

   // Prepare the buffer so that it matches the exposed area

   if (Self->BitmapOwnerID != Self->Head.UniqueID) {
      for (i=Index; (i > 0) AND (list[i].SurfaceID != Self->BitmapOwnerID); i--);
      DestBitmap->XOffset = list[Index].Left - list[i].Left; // Offset is relative to the bitmap owner
      DestBitmap->YOffset = list[Index].Top - list[i].Top;

   }
   else {
      // Set the clipping so that we only draw the area that has been exposed
      DestBitmap->XOffset = 0;
      DestBitmap->YOffset = 0;
   }

   DestBitmap->Clip.Left   = Left - list[Index].Left;
   DestBitmap->Clip.Top    = Top - list[Index].Top;
   DestBitmap->Clip.Right  = Right - list[Index].Left;
   DestBitmap->Clip.Bottom = Bottom - list[Index].Top;

   // THIS SHOULD NOT BE NEEDED - but occasionally it detects surface problems (bugs in other areas of the surface code?)

   if (((DestBitmap->XOffset + DestBitmap->Clip.Left) < 0) OR ((DestBitmap->YOffset + DestBitmap->Clip.Top) < 0) OR
       ((DestBitmap->XOffset + DestBitmap->Clip.Right) > DestBitmap->Width) OR ((DestBitmap->YOffset + DestBitmap->Clip.Bottom) > DestBitmap->Height)) {
      LogF("@UpdateRegion()","Invalid coordinates detected (outside of the surface area).  CODE FIX REQUIRED!");
      if ((DestBitmap->XOffset + DestBitmap->Clip.Left) < 0) DestBitmap->Clip.Left = 0;
      if ((DestBitmap->YOffset + DestBitmap->Clip.Top) < 0)  DestBitmap->Clip.Top = 0;
      DestBitmap->Clip.Right = DestBitmap->Width - DestBitmap->XOffset;
      DestBitmap->Clip.Bottom = DestBitmap->Height - DestBitmap->YOffset;
   }

   // Clear the background

   if ((Self->Flags & RNF_PRECOPY) AND (!(Self->Flags & RNF_COMPOSITE))) {
      struct PrecopyRegion *regions;
      LONG x, y, xoffset, yoffset, width, height;
      WORD j;

      if ((Self->PrecopyMID) AND (!AccessMemory(Self->PrecopyMID, MEM_READ, 2000, &regions))) {
         for (j=0; j < Self->PrecopyTotal; j++) {
            // Convert relative values to their fixed equivalent

            if (regions[j].Dimensions & DMF_RELATIVE_X_OFFSET) xoffset = Self->Width * regions[j].XOffset / 100;
            else xoffset = regions[j].XOffset;

            if (regions[j].Dimensions & DMF_RELATIVE_Y_OFFSET) yoffset = Self->Height * regions[j].YOffset / 100;
            else yoffset = regions[j].YOffset;

            if (regions[j].Dimensions & DMF_RELATIVE_X) x = Self->Width * regions[j].X / 100;
            else x = regions[j].X;

            if (regions[j].Dimensions & DMF_RELATIVE_Y) y = Self->Height * regions[j].Y / 100;
            else y = regions[j].Y;

            // Calculate absolute width

            if (regions[j].Dimensions & DMF_FIXED_WIDTH) width = regions[j].Width;
            else if (regions[j].Dimensions & DMF_RELATIVE_WIDTH) width = Self->Width * regions[j].Width / 100;
            else if ((regions[j].Dimensions & DMF_X_OFFSET) AND
                     (regions[j].Dimensions & DMF_X)) {
               width = Self->Width - x - xoffset;
            }
            else continue;

            // Calculate absolute height

            if (regions[j].Dimensions & DMF_FIXED_HEIGHT) height = regions[j].Height;
            else if (regions[j].Dimensions & DMF_RELATIVE_HEIGHT) height = Self->Height * regions[j].Height / 100;
            else if ((regions[j].Dimensions & DMF_Y_OFFSET) AND
                     (regions[j].Dimensions & DMF_Y)) {
               height = Self->Height - y - yoffset;
            }
            else continue;

            if ((width < 1) OR (height < 1)) continue;

            // X coordinate check

            if ((regions[j].Dimensions & DMF_X_OFFSET) AND (regions[j].Dimensions & DMF_WIDTH)) {
               x = Self->Width - xoffset - width;
            }

            // Y coordinate check

            if ((regions[j].Dimensions & DMF_Y_OFFSET) AND
                (regions[j].Dimensions & DMF_HEIGHT)) {
               y = Self->Height - yoffset - height;
            }

            // Trim coordinates to bitmap clip area

            abs.Left   = x;
            abs.Top    = y;
            abs.Right  = x + width;
            abs.Bottom = y + height;

            if (abs.Left   < DestBitmap->Clip.Left)   abs.Left   = DestBitmap->Clip.Left;
            if (abs.Top    < DestBitmap->Clip.Top)    abs.Top    = DestBitmap->Clip.Top;
            if (abs.Right  > DestBitmap->Clip.Right)  abs.Right  = DestBitmap->Clip.Right;
            if (abs.Bottom > DestBitmap->Clip.Bottom) abs.Bottom = DestBitmap->Clip.Bottom;

            abs.Left   += list[Index].Left;
            abs.Top    += list[Index].Top;
            abs.Right  += list[Index].Left;
            abs.Bottom += list[Index].Top;

            prepare_background(Self, list, Total, Index, DestBitmap, &abs, STAGE_PRECOPY);
         }
         ReleaseMemory(regions);
      }
      else prepare_background(Self, list, Total, Index, DestBitmap, &abs, STAGE_PRECOPY);
   }
   else if (Self->Flags & RNF_COMPOSITE) {
      gfxDrawRectangle(DestBitmap, 0, 0, Self->Width, Self->Height, PackPixelA((objBitmap *)DestBitmap, 0, 0, 0, 0), TRUE);
   }
   else if (Self->Colour.Alpha > 0) {
      gfxDrawRectangle(DestBitmap, 0, 0, Self->Width, Self->Height, PackPixelA((objBitmap *)DestBitmap, Self->Colour.Red, Self->Colour.Green, Self->Colour.Blue, 255), TRUE);
   }

   // Draw graphics to the buffer

   tlFreeExpose = DestBitmap->Head.UniqueID;

      process_surface_callbacks(Self, DestBitmap);

   tlFreeExpose = NULL;

   // After-copy management

   if (!(Self->Flags & RNF_COMPOSITE)) {

      if (Self->Flags & RNF_AFTER_COPY) {
         #ifdef DBG_DRAW_ROUTINES
            FMSG("RedrawSurface:","After-copy graphics drawing.");
         #endif
         prepare_background(Self, list, Total, Index, DestBitmap, &abs, STAGE_AFTERCOPY);
      }
      else if (Self->Type & RT_ROOT) {
         // If the surface object is part of a global background, we have to look for the root layer and check if it has the AFTERCOPY flag set.

         if ((i = find_surface_list(list, Total, Self->RootID)) != -1) {
            if (list[i].Flags & RNF_AFTER_COPY) {
               #ifdef DBG_DRAW_ROUTINES
                  FMSG("RedrawSurface:","After-copy graphics drawing.");
               #endif
               prepare_background(Self, list, Total, Index, DestBitmap, &abs, STAGE_AFTERCOPY);
            }
         }
      }
   }

   STEP();
}

/*****************************************************************************

-FUNCTION-
ReleaseList: Private. Releases access to the internal surfacelist array.

-INPUT-
int(ARF) Flags: Use the same flags as in in the previous call to drwAccessList().

*****************************************************************************/

static void drwReleaseList(LONG Flags)
{
   if (tlListCount > 0) {
      tlListCount--;
      if (!tlListCount) {
         //STEP();
         ReleaseMemory(tlSurfaceList);
         tlSurfaceList = NULL;
      }
   }
   else LogErrorMsg("drwReleaseList() called without an existing lock.");
}

/*****************************************************************************

-FUNCTION-
GetModalSurface: Returns the current modal surface (if defined) for a task.

This function returns the modal surface that is set for a specific task.  If no modal surface has been assigned to the
task, zero is returned.

-INPUT-
oid Task: The task from which to retrieve the modal surface ID.  If zero, the modal surface for the current task is returned.

-RESULT-
oid: The modal surface for the indicated task is returned.

*****************************************************************************/

static OBJECTID drwGetModalSurface(OBJECTID TaskID)
{
   if (!TaskID) TaskID = CurrentTaskID();

   if (!SysLock(PL_PROCESSES, 3000)) {
      OBJECTID result;
      struct TaskList *tasks;
      LONG maxtasks = GetResource(RES_MAX_PROCESSES);
      if ((tasks = GetResourcePtr(RES_TASK_LIST))) {
         LONG i;
         for (i=0; i < maxtasks; i++) {
            if (tasks[i].TaskID IS TaskID) break;
         }

         if (i < maxtasks) {
            result = tasks[i].ModalID;

            // Safety check: Confirm that the object still exists
            if ((result) AND (CheckObjectExists(result, NULL) != ERR_True)) {
               tasks[i].ModalID = 0;
               result = 0;
            }
         }
         else result = 0;
      }
      else result = 0;

      SysUnlock(PL_PROCESSES);
      return result;
   }
   else return 0;
}

/*****************************************************************************

-FUNCTION-
SetModalSurface: Enables a modal surface for the current task.

Any surface that is created by a task can be enabled as a modal surface.  A surface that has been enabled as modal
becomes the central point for all GUI interaction with the task.  All other I/O between the user and surfaces
maintained by the task will be ignored for as long as the target surface remains modal.

A task can switch off the current modal surface by calling this function with a Surface parameter of zero.

If a surface is modal at the time that this function is called, it is not possible to switch to a new modal surface
until the current modal state is dropped.

-INPUT-
oid Surface: The surface to enable as modal.

-RESULT-
oid: The object ID of the previous modal surface is returned (zero if there was no currently modal surface).

*****************************************************************************/

static OBJECTID drwSetModalSurface(OBJECTID SurfaceID)
{
   if (GetClassID(SurfaceID) != ID_SURFACE) return 0;

   LogF("~SetModalSurface()","#%d, CurrentFocus: %d", SurfaceID, drwGetUserFocus());

   OBJECTID old_modal = 0;

   // Check if the surface is invisible, in which case the mode has to be diverted to the modal that was previously
   // targetted or turned off altogether if there was no previously modal surface.

   if (SurfaceID) {
      objSurface *surface;
      OBJECTID divert = 0;
      if (!AccessObject(SurfaceID, 3000, &surface)) {
         if (!(surface->Flags & RNF_VISIBLE)) {
            divert = surface->PrevModalID;
            if (!divert) SurfaceID = 0;
         }
         ReleaseObject(surface);
      }
      if (divert) return drwSetModalSurface(divert);
   }

   if (!SysLock(PL_PROCESSES, 3000)) {
      LONG maxtasks = GetResource(RES_MAX_PROCESSES);
      OBJECTID focus = 0;
      struct TaskList *tasks;
      if ((tasks = GetResourcePtr(RES_TASK_LIST))) {
         LONG i;
         for (i=0; i < maxtasks; i++) {
            if (tasks[i].TaskID IS CurrentTaskID()) break;
         }

         if (i < maxtasks) {
            old_modal = tasks[i].ModalID;
            if (SurfaceID IS -1) { // Return the current modal surface, don't do anything else
            }
            else if (!SurfaceID) { // Turn off modal surface mode for the current task
               tasks[i].ModalID = 0;
            }
            else { // We are the new modal surface
               tasks[i].ModalID = SurfaceID;
               focus = SurfaceID;
            }
         }
      }

      SysUnlock(PL_PROCESSES);

      if (focus) {
         acMoveToFrontID(SurfaceID);

         // Do not change the primary focus if the targetted surface already has it (this ensures that if any children have the focus, they will keep it).

         LONG flags;
         if ((!drwGetSurfaceFlags(SurfaceID, &flags)) AND (!(flags & RNF_HAS_FOCUS))) {
            acFocusID(SurfaceID);
         }
      }
   }

   LogBack();
   return old_modal;
}


/*****************************************************************************

-FUNCTION-
CheckIfChild: Checks if a surface is a child of another particular surface.

This function checks if a surface identified by the Child value is the child of the surface identified by the Parent
value.  ERR_True is returned if the surface is confirmed as being a child of the parent, or if the Child and Parent
values are equal.  All other return codes indicate false or failure.

-INPUT-
oid Parent: The surface that is assumed to be the parent.
oid Child: The child surface to check.

-ERRORS-
True: The Child surface belongs to the Parent.
False: The Child surface is not a child of Parent.
Args: Invalid arguments were specified.
AccessMemory: Failed to access the internal surface list.

*****************************************************************************/

static ERROR drwCheckIfChild(OBJECTID ParentID, OBJECTID ChildID)
{
   FMSG("drwCheckIfChild()","Parent: %d, Child: %d", ParentID, ChildID);

   if ((!ParentID) OR (!ChildID)) return ERR_NullArgs;

   struct SurfaceControl *ctl;
   if ((ctl = drwAccessList(ARF_READ))) {
      // Find the parent surface, then examine its children to find a match for child ID.

      struct SurfaceList *list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex);
      LONG i;
      if ((i = find_surface_index(ctl, ParentID)) != -1) {
         LONG level = list[i].Level;
         for (++i; (i < ctl->Total) AND (list[i].Level > level); i++) {
            if (list[i].SurfaceID IS ChildID) {
               FMSG("drwCheckIfChild:","Child confirmed.");
               drwReleaseList(ARF_READ);
               return ERR_True;
            }
         }
      }

      drwReleaseList(ARF_READ);
      return ERR_False;
   }
   else return FuncError(ERR_AccessMemory);
}

/*****************************************************************************

-FUNCTION-
UnlockBitmap: Unlocks any earlier call to drwLockBitmap().

Call the UnlockBitmap() function to release a surface object from earlier calls to ~LockBitmap().

-INPUT-
oid Surface:        The ID of the surface object that you are releasing.
obj(Bitmap) Bitmap: Pointer to the bitmap structure returned earlier by LockBitmap().

-ERRORS-
Okay: The bitmap has been unlocked successfully.
NullArgs:

*****************************************************************************/

static ERROR drwUnlockBitmap(OBJECTID SurfaceID, objBitmap *Bitmap)
{
   if ((!SurfaceID) OR (!Bitmap)) return FuncError(ERR_NullArgs);
   ReleaseObject(Bitmap);
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
ApplyStyleValues: Applies default values to a GUI object before initialisation.

The ApplyStyleValues() function is reserved for the use of GUI classes that need to pre-initialise their objects with
default values.

Styles are defined in the order of the application's preference, the desktop preference, and then the default if no
preference has been specified or a failure occurred.

An application can define its preferred style by calling ~SetCurrentStyle() with the path of the XML style
file.  This function can be called at any time, allowing the style to be changed on the fly.

A desktop can set its preferred style by storing style information at `environment:config/style.xml`.

To prevent security breaches, users can only set a style preference if the ability to make a choice is exposed by the
desktop.  This is because style files can embed script functions that are executed within each application process
space.

-INPUT-
obj Object: The object that will receive the default values.
cstr Name:  Optional.  Reference to an alternative style to be applied.

-ERRORS-
Okay: Values have been preset successfully.

*****************************************************************************/

static ERROR drwApplyStyleValues(OBJECTPTR Object, CSTRING StyleName)
{
   if (!Object) return PostError(ERR_NullArgs);

   LogF("~ApplyStyleValues()","#%d, Style: %s", Object->UniqueID, StyleName);

   ERROR error;
   if ((error = load_styles())) { LogBack(); return error; }

   if (Object->Flags & NF_INITIALISED) { LogBack(); return PostError(ERR_BadState); }

   if (glDefaultStyleScript) apply_style(Object, glDefaultStyleScript, StyleName);

   if (glAppStyle) {
      //if (!apply_style(Object, glAppStyle, StyleName)) { LogBack(); return ERR_Okay; }
   }

   if (glDesktopStyleScript) apply_style(Object, glDesktopStyleScript, StyleName);

   LogBack();
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
ApplyStyleGraphics: Applies pre-defined graphics to a GUI object.

This is an internal function created for use by classes in the GUI category.  It finds the style definition for the
target Object and executes the procedure with the Surface as the graphics target.

-INPUT-
obj Object:  The object that requires styling.
oid Surface: The surface that will receive the style graphics.
cstr StyleName: Optional.  Reference to a style that is alternative to the default.
cstr StyleType: Optional.  Name of the type of style decoration to be applied.  Use in conjunction with StyleName.

-ERRORS-
Okay:
NullArgs:
BadState: The Object is not initialised.
NothingDone: No style information is defined for the object's class.

*****************************************************************************/

static ERROR drwApplyStyleGraphics(OBJECTPTR Object, OBJECTID SurfaceID, CSTRING StyleName, CSTRING StyleType)
{
   if ((!Object) OR (!SurfaceID)) return PostError(ERR_NullArgs);

   LogF("~ApplyStyleGraphics()","Object: %d, Surface: %d, Style: %s, StyleType: %s", Object->UniqueID, SurfaceID, StyleName, StyleType);

   ERROR error;
   if ((error = load_styles())) { LogBack(); return error; }

   // Try the app's style preference first.
/*
   OBJECTPTR script = glAppStyle;
   if (glAppStyle) {
      if (!xmlFindTag(xml, xpath, NULL, NULL)) {
         SetString(script, FID_Procedure, xpath);
         SetLong(script, FID_Target, SurfaceID);
         if (!acActivate(script)) return ERR_Okay;
      }
   }
*/
   // Now try the desktop preference.

   if (glDesktopStyleScript) {
      const struct ScriptArg args[] = {
         { "Class",   FDF_STRING,   { .Address = StyleName ? (APTR)StyleName : (APTR)Object->Class->ClassName } },
         { "Object",  FDF_OBJECT,   { .Address = Object } },
         { "Surface", FDF_OBJECTID, { .Long = SurfaceID } },
         { "StyleType", FDF_STRING, { .Address = (APTR)StyleType } }
      };

      struct scExec exec = {
         .Procedure = "applyDecoration",
         .Args      = args,
         .TotalArgs = ARRAYSIZE(args)
      };

      Action(MT_ScExec, glDesktopStyleScript, &exec);
      GetLong(glDesktopStyleScript, FID_Error, &error);
      if (!error) { LogBack(); return ERR_Okay; }
   }

   // Still no luck.  Try the default.

   if (glDefaultStyleScript) {
      const struct ScriptArg args[] = {
         { "Class",   FDF_STRING,   { .Address = StyleName ? (APTR)StyleName : (APTR)Object->Class->ClassName } },
         { "Object",  FDF_OBJECT,   { .Address = Object } },
         { "Surface", FDF_OBJECTID, { .Long = SurfaceID } },
         { "StyleType", FDF_STRING, { .Address = (APTR)StyleType } }
      };

      struct scExec exec = {
         .Procedure = "applyDecoration",
         .Args      = args,
         .TotalArgs = ARRAYSIZE(args)
      };

      Action(MT_ScExec, glDefaultStyleScript, &exec);
      GetLong(glDefaultStyleScript, FID_Error, &error);
      if (!error) { LogBack(); return ERR_Okay; }
   }

   LogBack();
   return ERR_NothingDone;
}

/*****************************************************************************

-FUNCTION-
SetCurrentStyle: Sets the current style script for the application.

This function changes the current style script for the application.  A path to the location of the script is required.

The script does not need to provide definitions for all GUI components.  Any component not represented in the script
will receive the default style settings.

The style definition does not affect default style values (i.e. fonts, colours and interface).  Style values can be set
by accessing the glStyle XML object directly and updating the values (do this as early as possible in the startup
process).

-INPUT-
cstr Path: Location of the style script.

-ERRORS-
Okay:
NullArgs:
EmptyString: The Path string is empty.
CreateObject: Failed to load the script.
-END-

*****************************************************************************/

static ERROR drwSetCurrentStyle(CSTRING Path)
{
   if (!Path) return PostError(ERR_NullArgs);
   if (!Path[0]) return PostError(ERR_EmptyString);

   if (glAppStyle) { acFree(glAppStyle); glAppStyle = NULL; }

   OBJECTPTR context = SetContext(modSurface);
   ERROR error = CreateObject(ID_SCRIPT, 0, &glAppStyle,
      FID_Path|TSTR, Path,
      TAGEND);
   SetContext(context);

   if (error) return ERR_CreateObject;

   return ERR_Okay;
}

/*****************************************************************************
** Reloads a style script if the time stamp has changed.  This should be done only when an environment change occurs.
*/

static void check_styles(STRING Path, OBJECTPTR *Script)
{
   objFile *file;
   if (!CreateObject(ID_FILE, NF_INTEGRAL, &file,
         FID_Path|TSTR, Path,
         TAGEND)) {
      LARGE script_timestamp, script_filesize, size, ts;

      GetFields(*Script, FID_TimeStamp|TLARGE, &script_timestamp,
                         FID_FileSize|TLARGE,  &script_filesize,
                         TAGEND);

      GetFields(file, FID_Size|TLARGE, &size,
                      FID_TimeStamp|TLARGE, &ts,
                      TAGEND);

      acFree(file);

      if ((ts != script_timestamp) OR (size != script_filesize)) {
         OBJECTPTR newscript;
         if (!CreateObject(ID_SCRIPT, NF_INTEGRAL, &newscript,
               FID_Path|TSTR, Path,
               TAGEND)) {

            SetOwner(newscript, modSurface);

            acFree(*Script);
            *Script = newscript;
         }
      }
   }
}

//****************************************************************************

static ERROR apply_style(OBJECTPTR Object, OBJECTPTR Script, CSTRING StyleName)
{
   const struct ScriptArg args[] = {
      { "Class",  FDF_STRING, { .Address = StyleName ? (APTR)StyleName : (APTR)Object->Class->ClassName } },
      { "Object", FDF_OBJECT, { .Address = Object } }
   };

   struct scExec exec = {
      .Procedure = "applyStyle",
      .Args      = args,
      .TotalArgs = ARRAYSIZE(args)
   };

   Action(MT_ScExec, Script, &exec);
   return ERR_Okay; // Return Okay only in the event that we did something.
}

//****************************************************************************

static ERROR load_styles(void)
{
   static BYTE desktop_attempted = FALSE;
   static BYTE default_attempted = FALSE;

   if ((!glDefaultStyleScript) AND (!default_attempted)) {
      default_attempted = TRUE;

      LogF("~load_styles()","Loading default style information.");

      OBJECTPTR context = SetContext(modSurface);

         // The app can set a style path that we have to honour if present.  This is typically used for emulating other
         // system styles, like mobile.

         if (!AnalysePath("style:", NULL)) {
            CreateObject(ID_FLUID, 0, &glDefaultStyleScript,
               FID_Path|TSTR, "style:style.fluid",
               TAGEND);
         }

         if (!glDefaultStyleScript) {
            CreateObject(ID_FLUID, 0, &glDefaultStyleScript,
               FID_Path|TSTR, "config:styles/default/style.fluid",
               TAGEND);
         }

      SetContext(context);

      LogBack();

      if (!glDefaultStyleScript) return ERR_CreateObject;
   }

   if ((!glDesktopStyleScript) AND (!desktop_attempted)) {
      desktop_attempted = TRUE;
      if (!AnalysePath("environment:config/style.xml", NULL)) {
         LogF("~load_styles()","Loading desktop style information.");

         OBJECTPTR context = SetContext(modSurface);
            CreateObject(ID_FLUID, 0, &glDesktopStyleScript,
               FID_Path|TSTR, "environment:config/style.fluid",
               TAGEND);
         SetContext(context);

         LogBack();
      }
   }

   // Note that there's no auto-loading for glAppStyle, as that has to be provided by calling SetCurrentStyle()

   return ERR_Okay;
}

//****************************************************************************
// Scans the surfacelist for the 'true owner' of a given bitmap.

static WORD FindBitmapOwner(struct SurfaceList *List, WORD Index)
{
   WORD i;
   WORD owner = Index;
   for (i=Index; i >= 0; i--) {
      if (List[i].SurfaceID IS List[owner].ParentID) {
         if (List[i].BitmapID != List[owner].BitmapID) return owner;
         owner = i;
      }
   }
   return owner;
}

/*****************************************************************************
** This function is responsible for inserting new surface objects into the list of layers for positional/depth management.
**
** Surface levels start at 1, which indicates the top-most level.
*/

static ERROR track_layer(objSurface *Self)
{
   struct SurfaceControl *ctl;
   struct SurfaceList *list;
   WORD i;

   if ((ctl = drwAccessList(ARF_WRITE))) {
      list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex);

      if (ctl->Total >= ctl->ArraySize - 1) {
         // Array is maxed out, we need to expand it

         if ((ctl->Total >= 0xffff) OR (tlListCount > 1)) {
            drwReleaseList(ARF_WRITE);
            return PostError(ERR_ArrayFull);
         }

         LONG blocksize = 200;
         LONG newtotal = ctl->ArraySize + blocksize;
         if (newtotal > 0xffff) newtotal = 0xffff;

         LogMsg("Expanding the size of the surface list to %d entries.", newtotal);

         if (!LockSharedMutex(glSurfaceMutex, 5000)) {
            struct SurfaceControl *nc;
            MEMORYID nc_id;
            if (!AllocMemory(sizeof(struct SurfaceControl) + (newtotal * sizeof(UWORD)) + (newtotal * sizeof(struct SurfaceList)),
                  MEM_UNTRACKED|MEM_PUBLIC|MEM_NO_CLEAR, &nc, &nc_id)) {
               nc->ListIndex  = sizeof(struct SurfaceControl);
               nc->ArrayIndex = sizeof(struct SurfaceControl) + (newtotal * sizeof(UWORD));
               nc->EntrySize  = sizeof(struct SurfaceList);
               nc->Total      = ctl->Total;
               nc->ArraySize  = newtotal;

               CopyMemory((APTR)ctl + ctl->ListIndex,  (APTR)nc + nc->ListIndex, sizeof(UWORD) * ctl->Total);
               CopyMemory((APTR)ctl + ctl->ArrayIndex, (APTR)nc + nc->ArrayIndex, sizeof(struct SurfaceList) * ctl->Total);
               drwReleaseList(ARF_WRITE);

               tlSurfaceList = nc;
               ctl = nc;
               list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex);
               glSharedControl->SurfacesMID = nc_id;
            }
            else {
               UnlockSharedMutex(glSurfaceMutex);
               drwReleaseList(ARF_WRITE);
               return PostError(ERR_AllocMemory);
            }

            UnlockSharedMutex(glSurfaceMutex);
         }
         else {
            drwReleaseList(ARF_WRITE);
            return PostError(ERR_AccessMemory);
         }

         if (ctl->Total >= ctl->ArraySize) {
            drwReleaseList(ARF_WRITE);
            return PostError(ERR_BufferOverflow);
         }
      }

      // Find the position at which the surface object should be inserted

      WORD level;
      WORD absx = 0;
      WORD absy = 0;
      if (!Self->ParentID) {
         // Insert the surface object at the end of the list

         i = ctl->Total;
         level = 1;
         absx = Self->X;
         absy = Self->Y;
      }
      else {
         level = 0;
         if ((i = find_parent_index(ctl, Self)) != -1) {
            level = list[i].Level + 1;
            absx  = list[i].Left + Self->X;
            absy  = list[i].Top + Self->Y;

            // Find the insertion point

            i++;
            while ((i < ctl->Total) AND (list[i].Level >= level)) {
               if (Self->Flags & RNF_STICK_TO_FRONT) {
                  if (list[i].Flags & RNF_POINTER) break;
               }
               else if ((list[i].Flags & RNF_STICK_TO_FRONT) AND (list[i].Level IS level)) break;
               i++;
            }
         }
         else {
            drwReleaseList(ARF_WRITE);
            LogErrorMsg("track_layer() failed to find parent object #%d.", Self->ParentID);
            return ERR_Search;
         }

         // Make space for insertion

         if (i < ctl->Total) {
            CopyMemory(list+i, list+i+1, sizeof(struct SurfaceList) * (ctl->Total-i));
         }
      }

      FMSG("track_layer()","Surface: %d, Index: %d, Level: %d, Parent: %d", Self->Head.UniqueID, i, level, Self->ParentID);

      list[i].ParentID  = Self->ParentID;
      list[i].SurfaceID = Self->Head.UniqueID;
      list[i].BitmapID  = Self->BufferID;
      list[i].DisplayID = Self->DisplayID;
      list[i].TaskID    = Self->Head.TaskID;
      list[i].PopOverID = Self->PopOverID;
      list[i].Flags     = Self->Flags;
      list[i].X         = Self->X;
      list[i].Y         = Self->Y;
      list[i].Left      = absx;
      list[i].Top       = absy;
      list[i].Width     = Self->Width;
      list[i].Height    = Self->Height;
      list[i].Right     = absx + Self->Width;
      list[i].Bottom    = absy + Self->Height;
      list[i].Level     = level;
      list[i].Opacity   = Self->Opacity;
      list[i].BitsPerPixel  = Self->BitsPerPixel;
      list[i].BytesPerPixel = Self->BytesPerPixel;
      list[i].LineWidth     = Self->LineWidth;
      list[i].DataMID       = Self->DataMID;
      list[i].Cursor        = Self->Cursor;
      list[i].RootID        = Self->RootID;

      ctl->Total++;
      list[ctl->Total].SurfaceID = 0; // Backwards compatibility terminators
      list[ctl->Total].Level = 0;

      drwReleaseList(ARF_WRITE);
      return ERR_Okay;
   }
   else {
      LogErrorMsg("track_layer() failed to access the surfacelist.");
      return ERR_LockMutex;
   }
}

//****************************************************************************

static void untrack_layer(OBJECTID ObjectID)
{
   struct SurfaceControl *ctl;
   if ((ctl = drwAccessList(ARF_WRITE))) {
      struct SurfaceList *list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex);

      LONG i, end;
      if ((i = find_surface_index(ctl, ObjectID)) != -1) {

         #ifdef DBG_LAYERS
            LogF("untrack_layer()","%d, Index: %d/%d", ObjectID, i, ctl->Total);
            //print_layer_list("untrack_layer", ctl, i);
         #endif

         // Mark all subsequent layers as invisible

         for (end=i+1; (end < ctl->Total) AND (list[end].Level > list[i].Level); end++) {
            list[end].Flags &= ~RNF_VISIBLE;
         }

         // If this object is at the end of the list, we can simply reduce the total.  Otherwise, shift the objects in front of us down the list.

         if (end >= ctl->Total) {
            // NOTE: All child surfaces are also removed as a result of truncating the list in this way.  This is fast, but can impact
            // on routines that expect the entries to exist irrespective of the destruction process.

            ctl->Total = i;
         }
         else {
            CopyMemory(list+i+1, list+i, sizeof(struct SurfaceList) * (ctl->Total-i));
            ctl->Total--;
         }

         list[ctl->Total].SurfaceID = 0; // This provided for backwards compatibility when the list was terminated with a nil object ID
         list[ctl->Total].Level = 0;

         #ifdef DBG_LAYERS
            print_layer_list("untrack_layer_end", ctl, i);
         #endif
      }

      drwReleaseList(ARF_WRITE);
   }
}

//****************************************************************************

static ERROR UpdateSurfaceCopy(objSurface *Self, struct SurfaceList *Copy)
{
   WORD i, j, level;

   if (!Self) return FuncError(ERR_NullArgs);
   if (!(Self->Head.Flags & NF_INITIALISED)) return ERR_Okay;

   struct SurfaceControl *ctl;
   if ((ctl = drwAccessList(ARF_UPDATE))) {
      struct SurfaceList *list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex);

      // Calculate absolute coordinates by looking for the parent of this object.  Then simply add the parent's
      // absolute X,Y coordinates to our X and Y fields.

      LONG absx, absy;
      if (Self->ParentID) {
         if ((i = find_parent_index(ctl, Self)) != -1) {
            absx = list[i].Left + Self->X;
            absy = list[i].Top + Self->Y;
            i = find_own_index(ctl, Self);
         }
         else {
            absx = 0;
            absy = 0;
         }
      }
      else {
         absx = Self->X;
         absy = Self->Y;
         i = find_own_index(ctl, Self);
      }

      if (i != -1) {
         list[i].ParentID      = Self->ParentID;
         //list[i].SurfaceID    = Self->Head.UniqueID; Never changes
         list[i].BitmapID      = Self->BufferID;
         list[i].DisplayID     = Self->DisplayID;
         //list[i].TaskID      = Self->Head.TaskID; Never changes
         list[i].PopOverID     = Self->PopOverID;
         list[i].X             = Self->X;
         list[i].Y             = Self->Y;
         list[i].Left          = absx;        // Synonym: Left
         list[i].Top           = absy;        // Synonym: Top
         list[i].Width         = Self->Width;
         list[i].Height        = Self->Height;
         list[i].Right         = absx + Self->Width;
         list[i].Bottom        = absy + Self->Height;
         list[i].Flags         = Self->Flags;
         list[i].Opacity       = Self->Opacity;
         list[i].BitsPerPixel  = Self->BitsPerPixel;
         list[i].BytesPerPixel = Self->BytesPerPixel;
         list[i].LineWidth     = Self->LineWidth;
         list[i].DataMID       = Self->DataMID;
         list[i].Cursor        = Self->Cursor;
         list[i].RootID        = Self->RootID;

         if (Copy) CopyMemory(list+i, Copy+i, sizeof(struct SurfaceList));

         // Rebuild absolute coordinates of child objects

         level = list[i].Level;
         WORD c = i+1;
         while ((c < ctl->Total) AND (list[c].Level > level)) {
            for (j=c-1; j >= 0; j--) {
               if (list[j].SurfaceID IS list[c].ParentID) {
                  list[c].Left   = list[j].Left + list[c].X;
                  list[c].Top    = list[j].Top  + list[c].Y;
                  list[c].Right  = list[c].Left + list[c].Width;
                  list[c].Bottom = list[c].Top  + list[c].Height;
                  if (Copy) {
                     Copy[c].Left   = list[c].Left;
                     Copy[c].Top    = list[c].Top;
                     Copy[c].Right  = list[c].Right;
                     Copy[c].Bottom = list[c].Bottom;
                  }
                  break;
               }
            }
            c++;
         }
      }

      drwReleaseList(ARF_UPDATE);
      return ERR_Okay;
   }
   else return PostError(ERR_AccessMemory);
}

//****************************************************************************

// TODO: This function is broken.  It needs to be tested in a separate program to get the bugs out.

static void move_layer_pos(struct SurfaceControl *ctl, LONG SrcIndex, LONG DestIndex)
{
   if (SrcIndex IS DestIndex) return;

   struct SurfaceList *list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex);

   LONG children, target_index;
   for (children=SrcIndex+1; (children < ctl->Total) AND (list[children].Level > list[SrcIndex].Level); children++);
   children -= SrcIndex;

   if ((DestIndex >= SrcIndex) AND (DestIndex <= SrcIndex + children)) return;

   struct SurfaceList tmp[children];
//src = 4
//dest = 8
//children = 3
   // Copy the source entry into a buffer
   CopyMemory(list + SrcIndex, &tmp, sizeof(struct SurfaceList) * children);

   // Shrink the list
   CopyMemory(list + SrcIndex + children, list + SrcIndex, sizeof(struct SurfaceList) * (ctl->Total - (SrcIndex + children)));

   if (DestIndex > SrcIndex) target_index = DestIndex - children;
   else target_index = DestIndex;

//target = 8 - 3 = 5
   // Expand the list at the destination index
//   target_index++;
   CopyMemory(list + target_index, list + target_index + children, sizeof(struct SurfaceList) * (ctl->Total - children - target_index));

   // Insert the saved content
   CopyMemory(&tmp, list + target_index, sizeof(struct SurfaceList) * children);
}
/*
0
1
2
3
 4   x from 4
  5  x
  6  x
 7
 8   z to 8
9
10-NULL

---
0
1
2
3
 7  (4)
 8  (5) <!-- Insert here
9   (6)
10  (7)
---
0
1
2
3
 7
 4
  5
  6
 8
9
10-NULL
*/
/*****************************************************************************
** Internal: check_volatile()
** Used By:  MoveToBack(), move_layer()
**
** This is the best way to figure out if a surface object or its children causes it to be volatile.  Use this function
** if you don't want to do any deep scanning to determine who is volatile or not.
**
** Volatile flags are PRECOPY, AFTER_COPY and CURSOR.
**
** NOTE: Surfaces marked as COMPOSITE or TRANSPARENT are not considered volatile as they do not require redraws.  It's
** up to the caller to make a decision as to whether COMPOSITE's are volatile or not.
*/

static UBYTE check_volatile(struct SurfaceList *list, WORD index)
{
   WORD i, j;

   if (list[index].Flags & RNF_VOLATILE) return TRUE;

   // If there are children with custom root layers or are volatile, that will force volatility

   for (i=index+1; list[i].Level > list[index].Level; i++) {
      if (!(list[i].Flags & RNF_VISIBLE)) {
         j = list[i].Level;
         while (list[i+1].Level > j) i++;
         continue;
      }

      if (list[i].Flags & RNF_VOLATILE) {
         // If a child surface is marked as volatile and is a member of our bitmap space, then effectively all members of the bitmap are volatile.

         if (list[index].BitmapID IS list[i].BitmapID) {
            return TRUE;
         }

         // If this is a custom root layer, check if it refers to a surface that is going to affect our own volatility.

         if (list[i].RootID != list[i].SurfaceID) {
            for (j=i; j > index; j--) {
               if (list[i].RootID IS list[j].SurfaceID) break;
            }

            if (j <= index) {
               return TRUE; // Custom root of a child is outside of bounds - that makes us volatile
            }
         }
      }
   }

   return FALSE;
}

//****************************************************************************
// Checks if an object is visible, according to its visibility and its parents visibility.

static UBYTE CheckVisibility(struct SurfaceList *list, WORD index)
{
   WORD i;

   OBJECTID scan = list[index].SurfaceID;
   for (i=index; i >= 0; i--) {
      if (list[i].SurfaceID IS scan) {
         if (!(list[i].Flags & RNF_VISIBLE)) return FALSE;
         if (!(scan = list[i].ParentID)) return TRUE;
      }
   }

   return TRUE;
}

static void check_bmp_buffer_depth(objSurface *Self, objBitmap *Bitmap)
{
   if (Bitmap->Flags & BMF_FIXED_DEPTH) {
      // Don't change bitmaps marked as fixed-depth
      return;
   }

   DISPLAYINFO *info;
   if (!gfxGetDisplayInfo(Self->DisplayID, &info)) {
      if (info->BitsPerPixel != Bitmap->BitsPerPixel) {
         LogMsg("[%d] Updating buffer Bitmap %dx%dx%d to match new display depth of %dbpp.", Bitmap->Head.UniqueID, Bitmap->Width, Bitmap->Height, Bitmap->BitsPerPixel, info->BitsPerPixel);
         acResize(Bitmap, Bitmap->Width, Bitmap->Height, info->BitsPerPixel);
         Self->LineWidth     = Bitmap->LineWidth;
         Self->BytesPerPixel = Bitmap->BytesPerPixel;
         Self->BitsPerPixel  = Bitmap->BitsPerPixel;
         Self->DataMID       = Bitmap->DataMID;
         UpdateSurfaceList(Self);
      }
   }
}

//****************************************************************************

static ERROR access_video(OBJECTID DisplayID, objDisplay **Display, objBitmap **Bitmap)
{
   if (!AccessObject(DisplayID, 5000, Display)) {
      APTR winhandle;

      if (!GetPointer(Display[0], FID_WindowHandle, &winhandle)) {
         #ifdef _WIN32
            SetPointer(Display[0]->Bitmap, FID_Handle, winGetDC(winhandle));
         #else
            SetPointer(Display[0]->Bitmap, FID_Handle, winhandle);
         #endif
      }

      if (Bitmap) *Bitmap = Display[0]->Bitmap;
      return ERR_Okay;
   }
   else return FuncError(ERR_AccessObject);
}

//****************************************************************************

static void release_video(objDisplay *Display)
{
   #ifdef _WIN32
      APTR surface;
      GetPointer(Display->Bitmap, FID_Handle, &surface);

      APTR winhandle;
      if (!GetPointer(Display, FID_WindowHandle, &winhandle)) {
         winReleaseDC(winhandle, surface);
      }

      SetPointer(Display->Bitmap, FID_Handle, NULL);
   #endif

   acFlush(Display);

   ReleaseObject(Display);
}

//****************************************************************************

static BYTE check_surface_list(void)
{
   FMSG("~check_surfaces()","Validating the surface list...");

   struct SurfaceControl *ctl;
   if ((ctl = drwAccessList(ARF_WRITE))) {
      LONG i;
      struct SurfaceList *list = (struct SurfaceList *)((APTR)ctl + ctl->ArrayIndex);
      BYTE bad = FALSE;
      for (i=0; i < ctl->Total; i++) {
         if ((CheckObjectExists(list[i].SurfaceID, NULL) != ERR_Okay)) {
            FMSG("check_surfaces:","Surface %d, index %d is dead.", list[i].SurfaceID, i);
            untrack_layer(list[i].SurfaceID);
            bad = TRUE;
            i--; // stay at the same index level
         }
      }

      drwReleaseList(ARF_WRITE);
      STEP();
      return bad;
   }
   else {
      STEP();
      return FALSE;
   }
}

//****************************************************************************

static void process_surface_callbacks(objSurface *Self, objBitmap *Bitmap)
{
   #ifdef DBG_DRAW_ROUTINES
      FMSG("~draw_callback()","Bitmap: %d, Count: %d", Bitmap->Head.UniqueID, Self->CallbackCount);
   #endif

   OBJECTPTR context = CurrentContext();

   LONG i;
   for (i=0; i < Self->CallbackCount; i++) {
      Bitmap->Opacity = 255;
      if (Self->Callback[i].Function.Type IS CALL_STDC) {
         void (*routine)(APTR, objSurface *, objBitmap *) = Self->Callback[i].Function.StdC.Routine;

         #ifdef DBG_DRAW_ROUTINES
            LogF("~draw_callback:","%d/%d: Routine: %p, Object: %p, Context: %p", i, Self->CallbackCount, routine, Self->Callback[i].Object, Self->Callback[i].Function.StdC.Context);
         #endif

         if (Self->Callback[i].Function.StdC.Context) {
            SetContext(Self->Callback[i].Function.StdC.Context);
            routine(Self->Callback[i].Function.StdC.Context, Self, Bitmap);
            SetContext(context);
         }
         else routine(Self->Callback[i].Object, Self, Bitmap);

         #ifdef DBG_DRAW_ROUTINES
            LogBack();
         #endif
      }
      else if (Self->Callback[i].Function.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = Self->Callback[i].Function.Script.Script)) {
            const struct ScriptArg args[] = {
               { "Surface", FD_OBJECTPTR, { .Address = Self } },
               { "Bitmap",  FD_OBJECTPTR, { .Address = Bitmap } }
            };
            scCallback(script, Self->Callback[i].Function.Script.ProcedureID, args, ARRAYSIZE(args));
         }
      }
   }

   Bitmap->Opacity = 255;

   #ifdef DBG_DRAW_ROUTINES
      STEP();
   #endif
}

/*****************************************************************************
** This routine will modify a clip region to match the visible area, as governed by parent surfaces within the same
** bitmap space (if MatchBitmap is TRUE).  It also scans the whole parent tree to ensure that all parents are visible,
** returning TRUE or FALSE accordingly.  If the region is completely obscured regardless of visibility settings, -1 is
** returned.
*/

static BYTE restrict_region_to_parents(struct SurfaceList *List, LONG Index, struct ClipRectangle *Clip, BYTE MatchBitmap)
{
   LONG j;

   UBYTE visible = TRUE;
   OBJECTID id = List[Index].SurfaceID;
   for (j=Index; (j >= 0) AND (id); j--) {
      if (List[j].SurfaceID IS id) {
         if (!(List[j].Flags & RNF_VISIBLE)) visible = FALSE;

         id = List[j].ParentID;

         if ((MatchBitmap IS FALSE) OR (List[j].BitmapID IS List[Index].BitmapID)) {
            if (Clip->Left   < List[j].Left)   Clip->Left   = List[j].Left;
            if (Clip->Top    < List[j].Top)    Clip->Top    = List[j].Top;
            if (Clip->Right  > List[j].Right)  Clip->Right  = List[j].Right;
            if (Clip->Bottom > List[j].Bottom) Clip->Bottom = List[j].Bottom;
         }
      }
   }

   if ((Clip->Right <= Clip->Left) OR (Clip->Bottom <= Clip->Top)) {
      Clip->Right = Clip->Left;
      Clip->Bottom = Clip->Top;
      return -1;
   }

   return visible;
}

/*****************************************************************************
** Loads the default style information and then applies the user's preferred values.  This function can be called at
** any time to refresh the style values in memory.
*/

static ERROR load_style_values(void)
{
   objXML *user, *style;
   ERROR error;
   char xpath[80];
   LONG target, a;

   LogF("~load_style_values()","");

   CSTRING style_path;
   style_path = "style:values.xml";
   if (AnalysePath(style_path, NULL) != ERR_Okay) {
      style_path = "environment:config/values.xml";
      if (AnalysePath(style_path, NULL) != ERR_Okay) {
         style_path = "config:styles/default/values.xml";
      }
   }

   if (!(error = CreateObject(ID_XML, 0, &style,
         FID_Name|TSTR, "glStyle",
         FID_Path|TSTR, style_path,
         TAGEND))) {

      // Now check for the user's preferred values.  These are copied over the defaults.

      if (!AnalysePath("user:config/style_values.xml", 0)) {
         if (!CreateObject(ID_XML, 0, &user,
               FID_Path|TSTR, "user:config/style_values.xml",
               TAGEND)) {

            struct XMLTag *tags = user->Tags[0];
            while (tags) {
               if (!StrMatch("fonts", tags->Attrib->Name)) {
                  struct XMLTag *src = tags->Child;
                  CSTRING fontname;
                  if ((fontname = XMLATTRIB(src, "name"))) {
                     StrFormat(xpath, sizeof(xpath), "/fonts/font[@name='%s']", fontname);
                     if (!xmlFindTag(style, xpath, NULL, &target)) {
                        for (a=1; a < src->TotalAttrib; a++) {
                           xmlSetAttrib(style, target, XMS_UPDATE, src->Attrib[a].Name, src->Attrib[a].Value);
                        }
                     }
                  }
               }
               else if (!StrMatch("colours", tags->Attrib->Name)) {
                  if (!xmlFindTag(style, "/colours", NULL, &target)) {
                     for (a=1; a < tags->TotalAttrib; a++) {
                        xmlSetAttrib(style, target, XMS_UPDATE, tags->Attrib[a].Name, tags->Attrib[a].Value);
                     }
                  }
               }
               else if (!StrMatch("interface", tags->Attrib->Name)) {
                  if (!xmlFindTag(style, "/interface", NULL, &target)) {
                     for (a=1; a < tags->TotalAttrib; a++) {
                        xmlSetAttrib(style, target, XMS_UPDATE, tags->Attrib[a].Name, tags->Attrib[a].Value);
                     }
                  }
               }
               tags = tags->Next;
            }
            acFree(user);
         }
      }

      if (glStyle) acFree(glStyle);
      glStyle = style;
   }

   LogBack();
   return error;
}

/*****************************************************************************
** This call is used to refresh the pointer image when at least one layer has been rearranged.  The timer is used to
** delay the refresh - useful if multiple surfaces are being rearranged when we only need to do the refresh once.
** The delay also prevents clashes with read/write access to the surface list.
*/

static ERROR refresh_pointer_timer(OBJECTPTR Task, LARGE Elapsed, LARGE CurrentTime)
{
   objPointer *pointer;
   if ((pointer = gfxAccessPointer())) {
      Action(AC_Refresh, pointer, NULL);
      ReleaseObject(pointer);
   }
   glRefreshPointerTimer = 0;
   return ERR_Terminate; // Timer is only called once
}

static void refresh_pointer(objSurface *Self)
{
   if (!glRefreshPointerTimer) {
      OBJECTPTR context = SetContext(modSurface);

         FUNCTION call;
         SET_FUNCTION_STDC(call, &refresh_pointer_timer);
         SubscribeTimer(0.02, &call, &glRefreshPointerTimer);

      SetContext(context);
   }
}

//**********************************************************************

#include "class_surface/surface_class.c"
#include "class_layout/layout.c"

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_SURFACE)
