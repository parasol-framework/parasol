
#include "defs.h"

/******************************************************************************
** Called when windows has an item to be dropped on our display area.
*/

#ifdef _WIN32
void winDragDropFromHost_Drop(int SurfaceID, char *Datatypes)
{
#ifdef WIN_DRAGDROP
   parasol::Log log(__FUNCTION__);
   objPointer *pointer;
   OBJECTID modal_id;
   extern OBJECTID glOverTaskID;

   log.branch("Surface: %d", SurfaceID);

   if ((pointer = gfxAccessPointer())) {
      // Pass AC_DragDrop to the surface underneath the mouse cursor.  If a surface subscriber accepts the data, it
      // will send a DATA_REQUEST to the relevant display object.  See DISPLAY_DataFeed() and winGetData().

      modal_id = gfxGetModalSurface(glOverTaskID);
      if (modal_id IS SurfaceID) modal_id = 0;

      if (!modal_id) {
         SURFACEINFO *info;
         if (!gfxGetSurfaceInfo(pointer->OverObjectID, &info)) {
            acDragDropID(pointer->OverObjectID, info->DisplayID, -1, Datatypes);
         }
         else log.warning(ERR_GetSurfaceInfo);
      }
      else log.msg("Program is modal - drag/drop cancelled.");

      gfxReleasePointer(pointer);
   }
#endif
}
#endif

/*****************************************************************************
** Surface locking routines.  These should only be called on occasions where you need to use the CPU to access graphics
** memory.  These functions are internal, if the user wants to lock a bitmap surface then the Lock() action must be
** called on the bitmap.
**
** Please note: Regarding SURFACE_READ, using this flag will cause the video content to be copied to the bitmap buffer.
** If you do not need this overhead because the bitmap content is going to be refreshed, then specify SURFACE_WRITE
** only.  You will still be able to read the bitmap content with the CPU, it just avoids the copy overhead.
*/

#ifdef _WIN32

ERROR lock_surface(extBitmap *Bitmap, WORD Access)
{
   if (!Bitmap->Data) {
      parasol::Log log(__FUNCTION__);
      log.warning("[Bitmap:%d] Bitmap is missing the Data field.", Bitmap->UID);
      return ERR_FieldNotSet;
   }

   return ERR_Okay;
}

ERROR unlock_surface(extBitmap *Bitmap)
{
   return ERR_Okay;
}

#elif __xwindows__

ERROR lock_surface(extBitmap *Bitmap, WORD Access)
{
   LONG size;
   WORD alignment;

   if ((Bitmap->Flags & BMF_X11_DGA) and (glDGAAvailable)) {
      return ERR_Okay;
   }
   else if ((Bitmap->x11.drawable) and (Access & SURFACE_READ)) {
      // If there is an existing readable area, try to reuse it if possible
      if (Bitmap->x11.readable) {
         if ((Bitmap->x11.readable->width >= Bitmap->Width) and (Bitmap->x11.readable->height >= Bitmap->Height)) {
            if (Access & SURFACE_READ) {
               XGetSubImage(XDisplay, Bitmap->x11.drawable, Bitmap->XOffset + Bitmap->Clip.Left,
                  Bitmap->YOffset + Bitmap->Clip.Top, Bitmap->Clip.Right - Bitmap->Clip.Left,
                  Bitmap->Clip.Bottom - Bitmap->Clip.Top, 0xffffffff, ZPixmap, Bitmap->x11.readable,
                  Bitmap->XOffset + Bitmap->Clip.Left, Bitmap->YOffset + Bitmap->Clip.Top);
            }
            return ERR_Okay;
         }
         else XDestroyImage(Bitmap->x11.readable);
      }

      // Generate a fresh XImage from the current drawable

      if (Bitmap->LineWidth & 0x0001) alignment = 8;
      else if (Bitmap->LineWidth & 0x0002) alignment = 16;
      else alignment = 32;

      if (Bitmap->Type IS BMP_PLANAR) {
         size = Bitmap->LineWidth * Bitmap->Height * Bitmap->BitsPerPixel;
      }
      else size = Bitmap->LineWidth * Bitmap->Height;

      Bitmap->Data = (UBYTE *)malloc(size);

      if ((Bitmap->x11.readable = XCreateImage(XDisplay, CopyFromParent, Bitmap->BitsPerPixel,
           ZPixmap, 0, (char *)Bitmap->Data, Bitmap->Width, Bitmap->Height, alignment, Bitmap->LineWidth))) {
         if (Access & SURFACE_READ) {
            XGetSubImage(XDisplay, Bitmap->x11.drawable, Bitmap->XOffset + Bitmap->Clip.Left,
               Bitmap->YOffset + Bitmap->Clip.Top, Bitmap->Clip.Right - Bitmap->Clip.Left,
               Bitmap->Clip.Bottom - Bitmap->Clip.Top, 0xffffffff, ZPixmap, Bitmap->x11.readable,
               Bitmap->XOffset + Bitmap->Clip.Left, Bitmap->YOffset + Bitmap->Clip.Top);
         }
         return ERR_Okay;
      }
      else return ERR_Failed;
   }
   return ERR_Okay;
}

ERROR unlock_surface(extBitmap *Bitmap)
{
   return ERR_Okay;
}

#elif _GLES_

ERROR lock_surface(extBitmap *Bitmap, WORD Access)
{
   parasol::Log log(__FUNCTION__);

   if (Bitmap->DataFlags & MEM_VIDEO) {
      // MEM_VIDEO represents the video display in OpenGL.  Read/write CPU access is not available to this area but
      // we can use glReadPixels() to get a copy of the framebuffer and then write changes back.  Because this is
      // extremely bad practice (slow), a debug message is printed to warn the developer to use a different code path.
      //
      // Practically the only reason why we allow this is for unusual measures like taking screenshots, grabbing the display for debugging, development testing etc.

      log.warning("Warning: Locking of OpenGL video surfaces for CPU access is bad practice (bitmap: #%d, mem: $%.8x)", Bitmap->UID, Bitmap->DataFlags);

      if (!Bitmap->Data) {
         if (AllocMemory(Bitmap->Size, MEM_NO_BLOCKING|MEM_NO_POOL|MEM_NO_CLEAR|Bitmap->memflags()|Bitmap->DataFlags, &Bitmap->Data, &Bitmap->DataMID) != ERR_Okay) {
            return log.warning(ERR_AllocMemory);
         }
         Bitmap->prvAFlags |= BF_DATA;
      }

      if (!lock_graphics_active(__func__)) {
         if (Access & SURFACE_READ) {
            //glPixelStorei(GL_PACK_ALIGNMENT, 1); Might be required if width is not 32-bit aligned (i.e. 16 bit uneven width?)
            glReadPixels(0, 0, Bitmap->Width, Bitmap->Height, Bitmap->prvGLPixel, Bitmap->prvGLFormat, Bitmap->Data);
         }

         if (Access & SURFACE_WRITE) Bitmap->prvWriteBackBuffer = TRUE;
         else Bitmap->prvWriteBackBuffer = FALSE;

         unlock_graphics();
      }

      return ERR_Okay;
   }
   else if (Bitmap->DataFlags & MEM_TEXTURE) {
      // Using the CPU on BLIT bitmaps is banned - it is considered to be poor programming.  Instead,
      // MEM_DATA bitmaps should be used when R/W CPU access is desired to a bitmap.

      return log.warning(ERR_NoSupport);
   }

   if (!Bitmap->Data) {
      log.warning("[Bitmap:%d] Bitmap is missing the Data field.  Memory flags: $%.8x", Bitmap->UID, Bitmap->DataFlags);
      return ERR_FieldNotSet;
   }

   return ERR_Okay;
}

ERROR unlock_surface(extBitmap *Bitmap)
{
   if ((Bitmap->DataFlags & MEM_VIDEO) and (Bitmap->prvWriteBackBuffer)) {
      if (!lock_graphics_active(__func__)) {
         #ifdef GL_DRAW_PIXELS
            glDrawPixels(Bitmap->Width, Bitmap->Height, pixel_type, format, Bitmap->Data);
         #else
            GLenum glerror;
            GLuint texture_id;
            if ((glerror = alloc_texture(Bitmap->Width, Bitmap->Height, &texture_id)) IS GL_NO_ERROR) { // Create a new texture space and bind it.
               //(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
               glTexImage2D(GL_TEXTURE_2D, 0, Bitmap->prvGLPixel, Bitmap->Width, Bitmap->Height, 0, Bitmap->prvGLPixel, Bitmap->prvGLFormat, Bitmap->Data); // Copy the bitmap content to the texture. (Target, Level, Bitmap, Border)
               if ((glerror = glGetError()) IS GL_NO_ERROR) {
                  // Copy graphics to the frame buffer.

                  glClearColor(0, 0, 0, 1.0);
                  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);    // Ensure colour is reset.
                  glDrawTexiOES(0, 0, 1, Bitmap->Width, Bitmap->Height);
                  glBindTexture(GL_TEXTURE_2D, 0);
                  eglSwapBuffers(glEGLDisplay, glEGLSurface);
               }
               else log.warning(ERR_OpenGL);

               glDeleteTextures(1, &texture_id);
            }
            else log.warning(ERR_OpenGL);
         #endif

         unlock_graphics();
      }

      Bitmap->prvWriteBackBuffer = FALSE;
   }

   return ERR_Okay;
}

#else

#error Platform not supported.

#define lock_surface(a,b)
#define unlock_surface(a)

#endif

//****************************************************************************

ERROR get_surface_abs(OBJECTID SurfaceID, LONG *AbsX, LONG *AbsY, LONG *Width, LONG *Height)
{
   SurfaceControl *ctl;

   if (!AccessMemory(glSharedControl->SurfacesMID, MEM_READ, 500, &ctl)) {
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
      LONG i;
      for (i=0; (list[i].SurfaceID) and (list[i].SurfaceID != SurfaceID); i++);

      if (!list[i].SurfaceID) {
         ReleaseMemory(ctl);
         return ERR_Search;
      }
      if (AbsX) *AbsX = list[i].Left;
      if (AbsY) *AbsY = list[i].Top;
      if (Width)  *Width  = list[i].Width;
      if (Height) *Height = list[i].Height;
      ReleaseMemory(ctl);
      return ERR_Okay;
   }
   else return ERR_AccessMemory;
}

/*****************************************************************************
** Redraw everything in RegionB that does not intersect with RegionA.
*/

void redraw_nonintersect(OBJECTID SurfaceID, SurfaceList *List, WORD Index, WORD Total,
   ClipRectangle *Region, ClipRectangle *RegionB, LONG RedrawFlags, LONG ExposeFlags)
{
   parasol::Log log(__FUNCTION__);

   if (!SurfaceID) { // Implemented this check because an invalid SurfaceID has happened before.
      log.warning("SurfaceID == 0");
      return;
   }

   log.traceBranch("redraw_nonintersect: (A) %dx%d,%dx%d Vs (B) %dx%d,%dx%d", Region->Left, Region->Top, Region->Right, Region->Bottom, RegionB->Left, RegionB->Top, RegionB->Right, RegionB->Bottom);

   ExposeFlags |= EXF_ABSOLUTE;

   struct { LONG left, top, right, bottom; } rect = { RegionB->Left, RegionB->Top, RegionB->Right, RegionB->Bottom };

   if (rect.right > Region->Right) { // Right
      log.trace("redraw_nonrect: Right exposure");

      if (RedrawFlags != -1) _redraw_surface(SurfaceID, List, Index, Total, (rect.left > Region->Right) ? rect.left : Region->Right, rect.top, rect.right, rect.bottom, RedrawFlags);
      if (ExposeFlags != -1) _expose_surface(SurfaceID, List, Index, Total, (rect.left > Region->Right) ? rect.left : Region->Right, rect.top, rect.right, rect.bottom, ExposeFlags);
      rect.right = Region->Right;
      if (rect.left >= rect.right) return;
   }

   if (rect.bottom > Region->Bottom) { // Bottom
      log.trace("redraw_nonrect: Bottom exposure");
      if (RedrawFlags != -1) _redraw_surface(SurfaceID, List, Index, Total, rect.left, (rect.top > Region->Bottom) ? rect.top : Region->Bottom, rect.right, rect.bottom, RedrawFlags);
      if (ExposeFlags != -1) _expose_surface(SurfaceID, List, Index, Total, rect.left, (rect.top > Region->Bottom) ? rect.top : Region->Bottom, rect.right, rect.bottom, ExposeFlags);
      rect.bottom = Region->Bottom;
      if (rect.top >= rect.bottom) return;
   }

   if (rect.top < Region->Top) { // Top
      log.trace("redraw_nonrect: Top exposure");
      if (RedrawFlags != -1) _redraw_surface(SurfaceID, List, Index, Total, rect.left, rect.top, rect.right, (rect.bottom < Region->Top) ? rect.bottom : Region->Top, RedrawFlags);
      if (ExposeFlags != -1) _expose_surface(SurfaceID, List, Index, Total, rect.left, rect.top, rect.right, (rect.bottom < Region->Top) ? rect.bottom : Region->Top, ExposeFlags);
      rect.top = Region->Top;
   }

   if (rect.left < Region->Left) { // Left
      log.trace("redraw_nonrect: Left exposure");
      if (RedrawFlags != -1) _redraw_surface(SurfaceID, List, Index, Total, rect.left, rect.top, (rect.right < Region->Left) ? rect.right : Region->Left, rect.bottom, RedrawFlags);
      if (ExposeFlags != -1) _expose_surface(SurfaceID, List, Index, Total, rect.left, rect.top, (rect.right < Region->Left) ? rect.right : Region->Left, rect.bottom, ExposeFlags);
   }
}

//****************************************************************************
// Scans the surfacelist for the 'true owner' of a given bitmap.

WORD find_bitmap_owner(SurfaceList *List, WORD Index)
{
   WORD owner = Index;
   for (LONG i=Index; i >= 0; i--) {
      if (List[i].SurfaceID IS List[owner].ParentID) {
         if (List[i].BitmapID != List[owner].BitmapID) return owner;
         owner = i;
      }
   }
   return owner;
}

//****************************************************************************
// This function is responsible for inserting new surface objects into the list of layers for positional/depth management.
//
// Surface levels start at 1, which indicates the top-most level.

ERROR track_layer(extSurface *Self)
{
   parasol::Log log(__FUNCTION__);
   SurfaceControl *ctl;
   WORD i;

   if ((ctl = gfxAccessList(ARF_WRITE))) {
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);

      if (ctl->Total >= ctl->ArraySize - 1) { // Array is maxed out, we need to expand it
         if ((ctl->Total >= 0xffff) or (tlListCount > 1)) {
            gfxReleaseList(ARF_WRITE);
            return log.warning(ERR_ArrayFull);
         }

         LONG blocksize = 200;
         LONG newtotal = ctl->ArraySize + blocksize;
         if (newtotal > 0xffff) newtotal = 0xffff;

         log.msg("Expanding the size of the surface list to %d entries.", newtotal);

         if (!LockSharedMutex(glSurfaceMutex, 5000)) {
            SurfaceControl *nc;
            MEMORYID nc_id;
            if (!AllocMemory(sizeof(SurfaceControl) + (newtotal * sizeof(UWORD)) + (newtotal * sizeof(SurfaceList)),
                  MEM_UNTRACKED|MEM_PUBLIC|MEM_NO_CLEAR, &nc, &nc_id)) {
               nc->ListIndex  = sizeof(SurfaceControl);
               nc->ArrayIndex = sizeof(SurfaceControl) + (newtotal * sizeof(UWORD));
               nc->EntrySize  = sizeof(SurfaceList);
               nc->Total      = ctl->Total;
               nc->ArraySize  = newtotal;

               CopyMemory((BYTE *)ctl + ctl->ListIndex,  (BYTE *)nc + nc->ListIndex, sizeof(UWORD) * ctl->Total);
               CopyMemory((BYTE *)ctl + ctl->ArrayIndex, (BYTE *)nc + nc->ArrayIndex, sizeof(SurfaceList) * ctl->Total);
               gfxReleaseList(ARF_WRITE);

               tlSurfaceList = nc;
               ctl = nc;
               list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
               glSharedControl->SurfacesMID = nc_id;
            }
            else {
               UnlockSharedMutex(glSurfaceMutex);
               gfxReleaseList(ARF_WRITE);
               return log.warning(ERR_AllocMemory);
            }

            UnlockSharedMutex(glSurfaceMutex);
         }
         else {
            gfxReleaseList(ARF_WRITE);
            return log.warning(ERR_AccessMemory);
         }

         if (ctl->Total >= ctl->ArraySize) {
            gfxReleaseList(ARF_WRITE);
            return log.warning(ERR_BufferOverflow);
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
            while ((i < ctl->Total) and (list[i].Level >= level)) {
               if (Self->Flags & RNF_STICK_TO_FRONT) {
                  if (list[i].Flags & RNF_POINTER) break;
               }
               else if ((list[i].Flags & RNF_STICK_TO_FRONT) and (list[i].Level IS level)) break;
               i++;
            }
         }
         else {
            gfxReleaseList(ARF_WRITE);
            log.warning("track_layer() failed to find parent object #%d.", Self->ParentID);
            return ERR_Search;
         }

         // Make space for insertion

         if (i < ctl->Total) CopyMemory(list+i, list+i+1, sizeof(SurfaceList) * (ctl->Total-i));
      }

      log.trace("Surface: %d, Index: %d, Level: %d, Parent: %d", Self->UID, i, level, Self->ParentID);

      list[i].ParentID  = Self->ParentID;
      list[i].SurfaceID = Self->UID;
      list[i].BitmapID  = Self->BufferID;
      list[i].DisplayID = Self->DisplayID;
      list[i].TaskID    = Self->ownerTask();
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

      gfxReleaseList(ARF_WRITE);
      return ERR_Okay;
   }
   else {
      log.warning("track_layer() failed to access the surfacelist.");
      return ERR_LockMutex;
   }
}

//****************************************************************************

void untrack_layer(OBJECTID ObjectID)
{
   parasol::Log log(__FUNCTION__);
   SurfaceControl *ctl;
   if ((ctl = gfxAccessList(ARF_WRITE))) {
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);

      LONG i, end;
      if ((i = find_surface_index(ctl, ObjectID)) != -1) {
         #ifdef DBG_LAYERS
            log.msg("%d, Index: %d/%d", ObjectID, i, ctl->Total);
            //print_layer_list("untrack_layer", ctl, i);
         #endif

         // Mark all subsequent layers as invisible

         for (end=i+1; (end < ctl->Total) and (list[end].Level > list[i].Level); end++) {
            list[end].Flags &= ~RNF_VISIBLE;
         }

         // If this object is at the end of the list, we can simply reduce the total.  Otherwise, shift the objects in front of us down the list.

         if (end >= ctl->Total) {
            // NOTE: All child surfaces are also removed as a result of truncating the list in this way.  This is fast, but can impact
            // on routines that expect the entries to exist irrespective of the destruction process.

            ctl->Total = i;
         }
         else {
            CopyMemory(list+i+1, list+i, sizeof(SurfaceList) * (ctl->Total-i));
            ctl->Total--;
         }

         list[ctl->Total].SurfaceID = 0; // This provided for backwards compatibility when the list was terminated with a nil object ID
         list[ctl->Total].Level = 0;

         #ifdef DBG_LAYERS
            print_layer_list("untrack_layer_end", ctl, i);
         #endif
      }

      gfxReleaseList(ARF_WRITE);
   }
}

//****************************************************************************

ERROR update_surface_copy(extSurface *Self, SurfaceList *Copy)
{
   parasol::Log log(__FUNCTION__);
   WORD i, j, level;

   if (!Self) return log.warning(ERR_NullArgs);
   if (!Self->initialised()) return ERR_Okay;

   SurfaceControl *ctl;
   if ((ctl = gfxAccessList(ARF_UPDATE))) {
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);

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
         //list[i].SurfaceID    = Self->UID; Never changes
         list[i].BitmapID      = Self->BufferID;
         list[i].DisplayID     = Self->DisplayID;
         //list[i].TaskID      = Self->ownerTask(); Never changes
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

         if (Copy) CopyMemory(list+i, Copy+i, sizeof(SurfaceList));

         // Rebuild absolute coordinates of child objects

         level = list[i].Level;
         WORD c = i+1;
         while ((c < ctl->Total) and (list[c].Level > level)) {
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

      gfxReleaseList(ARF_UPDATE);
      return ERR_Okay;
   }
   else return log.warning(ERR_AccessMemory);
}

//****************************************************************************

// TODO: This function is broken.  It needs to be tested in a dedicated test app to get the bugs out.

void move_layer_pos(SurfaceControl *ctl, LONG SrcIndex, LONG DestIndex)
{
   if (SrcIndex IS DestIndex) return;

   auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);

   LONG children, target_index;
   for (children=SrcIndex+1; (children < ctl->Total) and (list[children].Level > list[SrcIndex].Level); children++);
   children -= SrcIndex;

   if ((DestIndex >= SrcIndex) and (DestIndex <= SrcIndex + children)) return;

   SurfaceList tmp[children];
//src = 4
//dest = 8
//children = 3
   // Copy the source entry into a buffer
   CopyMemory(list + SrcIndex, &tmp, sizeof(SurfaceList) * children);

   // Shrink the list
   CopyMemory(list + SrcIndex + children, list + SrcIndex, sizeof(SurfaceList) * (ctl->Total - (SrcIndex + children)));

   if (DestIndex > SrcIndex) target_index = DestIndex - children;
   else target_index = DestIndex;

//target = 8 - 3 = 5
   // Expand the list at the destination index
//   target_index++;
   CopyMemory(list + target_index, list + target_index + children, sizeof(SurfaceList) * (ctl->Total - children - target_index));

   // Insert the saved content
   CopyMemory(&tmp, list + target_index, sizeof(SurfaceList) * children);
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

//****************************************************************************
// This function is responsible for managing the resizing of top-most surface objects and is also used by some of the
// field management functions for Width/Height adjustments.
//
// This function is also useful for skipping the dimension limits normally imposed when resizing.

ERROR resize_layer(extSurface *Self, LONG X, LONG Y, LONG Width, LONG Height, LONG InsideWidth,
   LONG InsideHeight, LONG BPP, DOUBLE RefreshRate, LONG DeviceFlags)
{
   if (!Width)  Width = Self->Width;
   if (!Height) Height = Self->Height;

   if (!Self->initialised()) {
      Self->X = X;
      Self->Y = Y;
      Self->Width  = Width;
      Self->Height = Height;
      return ERR_Okay;
   }

   if ((Self->X IS X) and (Self->Y IS Y) and (Self->Width IS Width) and (Self->Height IS Height) and
       (Self->ParentID)) {
      return ERR_Okay;
   }

   parasol::Log log;

   log.traceBranch("resize_layer() %dx%d,%dx%d TO %dx%d,%dx%dx%d", Self->X, Self->Y, Self->Width, Self->Height, X, Y, Width, Height, BPP);

   if (Self->BitmapOwnerID IS Self->UID) {
      objBitmap *bitmap;
      if (!AccessObject(Self->BufferID, 5000, &bitmap)) {
         if (!acResize(bitmap, Width, Height, BPP)) {
            Self->LineWidth     = bitmap->LineWidth;
            Self->BytesPerPixel = bitmap->BytesPerPixel;
            Self->BitsPerPixel  = bitmap->BitsPerPixel;
            Self->DataMID       = bitmap->DataMID;
            UpdateSurfaceList(Self);
         }
         else {
            ReleaseObject(bitmap);
            return log.warning(ERR_Resize);
         }

         ReleaseObject(bitmap);
      }
      else return log.warning(ERR_AccessObject);
   }

   if (!Self->ParentID) {
      if (Width  > Self->MaxWidth  + Self->LeftMargin + Self->RightMargin)  Width  = Self->MaxWidth  + Self->LeftMargin + Self->RightMargin;
      if (Height > Self->MaxHeight + Self->TopMargin  + Self->BottomMargin) Height = Self->MaxHeight + Self->TopMargin  + Self->BottomMargin;
      if (InsideWidth < Width) InsideWidth = Width;
      if (InsideHeight < Height) InsideHeight = Height;

      OBJECTPTR display;
      if (!AccessObject(Self->DisplayID, 5000, &display)) { // NB: SetDisplay() always processes coordinates relative to the client area in order to resolve issues when in hosted mode.
         if (gfxSetDisplay(display, X, Y, Width, Height, InsideWidth, InsideHeight, BPP, RefreshRate, DeviceFlags)) {
            ReleaseObject(display);
            return log.warning(ERR_Redimension);
         }

         GetFields(display, FID_Width|TLONG, &Width, FID_Height|TLONG, &Height, TAGEND);
         ReleaseObject(display);
      }
      else return log.warning(ERR_AccessObject);
   }

   LONG oldx = Self->X;
   LONG oldy = Self->Y;
   LONG oldw = Self->Width;
   LONG oldh = Self->Height;

   Self->X = X;
   Self->Y = Y;
   Self->Width  = Width;
   Self->Height = Height;
   UpdateSurfaceList(Self);

   if (!Self->initialised()) return ERR_Okay;

   // Send a Resize notification to our subscribers.  Basically, this informs our surface children to resize themselves
   // to the new dimensions.  Surface objects are not permitted to redraw themselves when they receive the Redimension
   // notification - we will send a delayed draw message later in this routine.

   forbidDrawing();

   struct acRedimension redimension = { (DOUBLE)X, (DOUBLE)Y, 0, (DOUBLE)Width, (DOUBLE)Height, (DOUBLE)BPP };
   NotifySubscribers(Self, AC_Redimension, &redimension, NULL, ERR_Okay);

   permitDrawing();

   if (!(Self->Flags & RNF_VISIBLE)) return ERR_Okay;

   if (!tlNoDrawing) {
      // Post the drawing update.  This method is the only reliable way to generate updates when our surface may
      // contain children that belong to foreign tasks.

      SurfaceControl *ctl;
      if (!(ctl = gfxAccessList(ARF_READ))) return ERR_AccessMemory;

      LONG total = ctl->Total;
      SurfaceList cplist[total];
      CopyMemory((BYTE *)ctl + ctl->ArrayIndex, cplist, sizeof(cplist[0]) * total);
      gfxReleaseList(ARF_READ);

      WORD index;
      if ((index = find_surface_list(cplist, total, Self->UID)) IS -1) { // The surface might not be listed if the parent is in the process of being dstroyed.
         return ERR_Search;
      }

      parasol::Log log;
      log.traceBranch("Redrawing the resized surface.");

      _redraw_surface(Self->UID, cplist, index, total, cplist[index].Left, cplist[index].Top, cplist[index].Right, cplist[index].Bottom, 0);
      _expose_surface(Self->UID, cplist, index, total, 0, 0, Self->Width, Self->Height, EXF_CHILDREN|EXF_REDRAW_VOLATILE_OVERLAP);

      if (Self->ParentID) {
         // Update external regions on all four sides that have been exposed by the resize, for example due to a decrease in area or a coordinate shift.
         //
         // Note: tlVolatileIndex determines the point at which volatile exposes will start.  We want volatile exposes to start just after our target surface, and not
         // anything that sits behind us in the containing parent.

         WORD vindex;
         for (vindex=index+1; (vindex < total) and (cplist[vindex].Level > cplist[index].Level); vindex++);
         tlVolatileIndex = vindex;

         LONG parent_index;
         for (parent_index=index-1; parent_index >= 0; parent_index--) {
            if (cplist[parent_index].SurfaceID IS Self->ParentID) break;
         }

         ClipRectangle region_b = {
            .Left   = cplist[parent_index].Left + oldx,
            .Top    = cplist[parent_index].Top + oldy,
            .Right  = (cplist[parent_index].Left + oldx) + oldw,
            .Bottom = (cplist[parent_index].Top + oldy) + oldh
         };

         ClipRectangle region_a = {
            .Left   = cplist[index].Left,
            .Top    = cplist[index].Top,
            .Right  = cplist[index].Right,
            .Bottom = cplist[index].Bottom
         };

         if (Self->BitmapOwnerID IS Self->UID) {
            redraw_nonintersect(Self->ParentID, cplist, parent_index, total, &region_a, &region_b, -1, EXF_CHILDREN|EXF_REDRAW_VOLATILE);
         }
         else redraw_nonintersect(Self->ParentID, cplist, parent_index, total, &region_a, &region_b, 0, EXF_CHILDREN|EXF_REDRAW_VOLATILE);

         tlVolatileIndex = 0;
      }
   }

   refresh_pointer(Self);
   return ERR_Okay;
}

//****************************************************************************
// Checks if an object is visible, according to its visibility and its parents visibility.

static UBYTE check_visibility(SurfaceList *list, WORD index)
{
   OBJECTID scan = list[index].SurfaceID;
   for (WORD i=index; i >= 0; i--) {
      if (list[i].SurfaceID IS scan) {
         if (!(list[i].Flags & RNF_VISIBLE)) return FALSE;
         if (!(scan = list[i].ParentID)) return TRUE;
      }
   }

   return TRUE;
}

static void check_bmp_buffer_depth(extSurface *Self, objBitmap *Bitmap)
{
   parasol::Log log(__FUNCTION__);

   if (Bitmap->Flags & BMF_FIXED_DEPTH) return;  // Don't change bitmaps marked as fixed-depth

   DISPLAYINFO *info;
   if (!gfxGetDisplayInfo(Self->DisplayID, &info)) {
      if (info->BitsPerPixel != Bitmap->BitsPerPixel) {
         log.msg("[%d] Updating buffer Bitmap %dx%dx%d to match new display depth of %dbpp.", Bitmap->UID, Bitmap->Width, Bitmap->Height, Bitmap->BitsPerPixel, info->BitsPerPixel);
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

void process_surface_callbacks(extSurface *Self, extBitmap *Bitmap)
{
   parasol::Log log(__FUNCTION__);

   #ifdef DBG_DRAW_ROUTINES
      log.traceBranch("Bitmap: %d, Count: %d", Bitmap->UID, Self->CallbackCount);
   #endif

   for (LONG i=0; i < Self->CallbackCount; i++) {
      Bitmap->Opacity = 255;
      if (Self->Callback[i].Function.Type IS CALL_STDC) {
         auto routine = (void (*)(APTR, extSurface *, objBitmap *))Self->Callback[i].Function.StdC.Routine;

         #ifdef DBG_DRAW_ROUTINES
            parasol::Log log(__FUNCTION__);
            log.branch("%d/%d: Routine: %p, Object: %p, Context: %p", i, Self->CallbackCount, routine, Self->Callback[i].Object, Self->Callback[i].Function.StdC.Context);
         #endif

         if (Self->Callback[i].Function.StdC.Context) {
            parasol::SwitchContext context(Self->Callback[i].Function.StdC.Context);
            routine(Self->Callback[i].Function.StdC.Context, Self, Bitmap);
         }
         else routine(Self->Callback[i].Object, Self, Bitmap);
      }
      else if (Self->Callback[i].Function.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = Self->Callback[i].Function.Script.Script)) {
            const ScriptArg args[] = {
               { "Surface", FD_OBJECTPTR, { .Address = Self } },
               { "Bitmap",  FD_OBJECTPTR, { .Address = Bitmap } }
            };
            scCallback(script, Self->Callback[i].Function.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
         }
      }
   }

   Bitmap->Opacity = 255;
}

/*****************************************************************************
** This routine will modify a clip region to match the visible area, as governed by parent surfaces within the same
** bitmap space (if MatchBitmap is TRUE).  It also scans the whole parent tree to ensure that all parents are visible,
** returning TRUE or FALSE accordingly.  If the region is completely obscured regardless of visibility settings, -1 is
** returned.
*/

BYTE restrict_region_to_parents(SurfaceList *List, LONG Index, ClipRectangle *Clip, BYTE MatchBitmap)
{
   UBYTE visible = TRUE;
   OBJECTID id = List[Index].SurfaceID;
   for (LONG j=Index; (j >= 0) and (id); j--) {
      if (List[j].SurfaceID IS id) {
         if (!(List[j].Flags & RNF_VISIBLE)) visible = FALSE;

         id = List[j].ParentID;

         if ((MatchBitmap IS FALSE) or (List[j].BitmapID IS List[Index].BitmapID)) {
            if (Clip->Left   < List[j].Left)   Clip->Left   = List[j].Left;
            if (Clip->Top    < List[j].Top)    Clip->Top    = List[j].Top;
            if (Clip->Right  > List[j].Right)  Clip->Right  = List[j].Right;
            if (Clip->Bottom > List[j].Bottom) Clip->Bottom = List[j].Bottom;
         }
      }
   }

   if ((Clip->Right <= Clip->Left) or (Clip->Bottom <= Clip->Top)) {
      Clip->Right = Clip->Left;
      Clip->Bottom = Clip->Top;
      return -1;
   }

   return visible;
}

//****************************************************************************

void forbidDrawing(void)
{
   tlNoDrawing++;
   tlNoExpose++;
}

void forbidExpose(void)
{
   tlNoExpose++;
}

void permitDrawing(void)
{
   tlNoDrawing--;
   tlNoExpose--;
}

void permitExpose(void)
{
   tlNoExpose--;
}

#ifdef DBG_LAYERS
void print_layer_list(STRING Function, SurfaceControl *Ctl, LONG POI)
{
   SurfaceList *list = (SurfaceList *)((BYTE *)Ctl + Ctl->ArrayIndex);
   fprintf(stderr, "LAYER LIST: %d of %d Entries, From %s()\n", Ctl->Total, Ctl->ArraySize, Function);

   LONG j;
   for (LONG i=0; i < Ctl->Total; i++) {
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

LONG find_surface_list(SurfaceList *list, LONG Total, OBJECTID SurfaceID)
{
   if (glRecentSurfaceIndex < Total) { // Cached lookup
      if (list[glRecentSurfaceIndex].SurfaceID IS SurfaceID) return glRecentSurfaceIndex;
   }

   // Search for the object

   for (LONG i=0; i < Total; i++) {
      if (list[i].SurfaceID IS SurfaceID) {
         glRecentSurfaceIndex = i;
         return i;
      }
   }

   return -1;
}

LONG find_parent_list(SurfaceList *list, WORD Total, extSurface *Self)
{
   if (glRecentSurfaceIndex < Total) { // Cached lookup
      if (list[glRecentSurfaceIndex].SurfaceID IS Self->ParentID) return glRecentSurfaceIndex;
   }

   if ((Self->ListIndex < Total) and (list[Self->ListIndex].SurfaceID IS Self->UID)) {
      for (LONG i=Self->ListIndex-1; i >= 0; i--) {
         if (list[i].SurfaceID IS Self->ParentID) {
            glRecentSurfaceIndex = i;
            return i;
         }
      }
   }

   // Search for the object

   for (LONG i=0; i < Total; i++) {
      if (list[i].SurfaceID IS Self->ParentID) {
         glRecentSurfaceIndex = i;
         return i;
      }
   }

   return -1;
}

/*****************************************************************************

-FUNCTION-
AccessList: Private. Grants access to the internal SurfaceList array.

-INPUT-
int(ARF) Flags: Specify ARF_WRITE if writing to the list, otherwise ARF_READ must be set.  Use ARF_NO_DELAY if you need immediate access to the surfacelist.

-RESULT-
struct(SurfaceControl): Pointer to the SurfaceControl structure.

*****************************************************************************/

SurfaceControl * gfxAccessList(LONG Flags)
{
   if (!tlSurfaceList) {
      ERROR error;

      if (Flags & ARF_NO_DELAY) {
         error = AccessMemory(glSharedControl->SurfacesMID, MEM_READ_WRITE, 20, &tlSurfaceList);
      }
      else error = AccessMemory(glSharedControl->SurfacesMID, MEM_READ_WRITE, 4000, &tlSurfaceList);

      if (!error) tlListCount = 1;
   }
   else tlListCount++;

   return tlSurfaceList;
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

ERROR gfxCheckIfChild(OBJECTID ParentID, OBJECTID ChildID)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("Parent: %d, Child: %d", ParentID, ChildID);

   if ((!ParentID) or (!ChildID)) return ERR_NullArgs;

   SurfaceControl *ctl;
   if ((ctl = gfxAccessList(ARF_READ))) {
      // Find the parent surface, then examine its children to find a match for child ID.

      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
      LONG i;
      if ((i = find_surface_index(ctl, ParentID)) != -1) {
         LONG level = list[i].Level;
         for (++i; (i < ctl->Total) and (list[i].Level > level); i++) {
            if (list[i].SurfaceID IS ChildID) {
               log.trace("Child confirmed.");
               gfxReleaseList(ARF_READ);
               return ERR_True;
            }
         }
      }

      gfxReleaseList(ARF_READ);
      return ERR_False;
   }
   else return log.warning(ERR_AccessMemory);
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
ext(Bitmap) Bitmap: Must reference a target Bitmap object.
int(BDF) Flags:  Optional flags.
int X:      The horizontal source coordinate.
int Y:      The vertical source coordinate.
int Width:  The width of the graphic that will be copied.
int Height: The height of the graphic that will be copied.
int XDest:  The horizontal target coordinate.
int YDest:  The vertical target coordinate.

-ERRORS-
Okay
NullArgs
Search: The supplied SurfaceID did not refer to a recognised surface object
AccessMemory: Failed to access the internal surfacelist memory structure

****************************************************************************/

ERROR gfxCopySurface(OBJECTID SurfaceID, extBitmap *Bitmap, LONG Flags,
          LONG X, LONG Y, LONG Width, LONG Height, LONG XDest, LONG YDest)
{
   parasol::Log log(__FUNCTION__);

   if ((!SurfaceID) or (!Bitmap)) return log.warning(ERR_NullArgs);

   log.traceBranch("%dx%d,%dx%d TO %dx%d, Flags $%.8x", X, Y, Width, Height, XDest, YDest, Flags);

   SurfaceControl *ctl;
   if ((ctl = gfxAccessList(ARF_READ))) {
      BITMAPSURFACE surface;
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
      for (WORD i=0; i < ctl->Total; i++) {
         if (list[i].SurfaceID IS SurfaceID) {
            if (X < 0) { XDest -= X; Width  += X; X = 0; }
            if (Y < 0) { YDest -= Y; Height += Y; Y = 0; }
            if (X+Width  > list[i].Width)  Width  = list[i].Width-X;
            if (Y+Height > list[i].Height) Height = list[i].Height-Y;

            // Find the bitmap root

            WORD root = find_bitmap_owner(list, i);

            SurfaceList list_i = list[i];
            SurfaceList list_root = list[root];
            gfxReleaseList(ARF_READ);

            if (Flags & BDF_REDRAW) {
               BYTE state = tlNoDrawing;
               tlNoDrawing = 0;
               gfxRedrawSurface(SurfaceID, list_i.Left+X, list_i.Top+Y, list_i.Left+X+Width, list_i.Top+Y+Height, IRF_FORCE_DRAW);
               tlNoDrawing = state;
            }

            if ((Flags & (BDF_SYNC|BDF_DITHER)) or (!list_root.DataMID)) {
               extBitmap *src;
               if (!AccessObject(list_root.BitmapID, 4000, &src)) {
                  src->XOffset    = list_i.Left - list_root.Left;
                  src->YOffset    = list_i.Top - list_root.Top;
                  src->Clip.Left   = 0;
                  src->Clip.Top    = 0;
                  src->Clip.Right  = list_i.Width;
                  src->Clip.Bottom = list_i.Height;

                  bool composite;
                  if (list_i.Flags & RNF_COMPOSITE) composite = true;
                  else composite = false;

                  if (composite) {
                     gfxCopyArea(src, Bitmap, BAF_BLEND|((Flags & BDF_DITHER) ? BAF_DITHER : 0), X, Y, Width, Height, XDest, YDest);
                  }
                  else gfxCopyArea(src, Bitmap, (Flags & BDF_DITHER) ? BAF_DITHER : 0, X, Y, Width, Height, XDest, YDest);

                  ReleaseObject(src);
                  return ERR_Okay;
               }
               else return log.warning(ERR_AccessObject);
            }
            else if (!AccessMemory(list_root.DataMID, MEM_READ, 2000, &surface.Data)) {
               surface.XOffset       = list_i.Left - list_root.Left;
               surface.YOffset       = list_i.Top - list_root.Top;
               surface.LineWidth     = list_root.LineWidth;
               surface.Height        = list_i.Height;
               surface.BitsPerPixel  = list_root.BitsPerPixel;
               surface.BytesPerPixel = list_root.BytesPerPixel;

               bool composite;
               if (list_i.Flags & RNF_COMPOSITE) composite = true;
               else composite = false;

               if (composite) gfxCopyRawBitmap(&surface, Bitmap, CSRF_DEFAULT_FORMAT|CSRF_OFFSET|CSRF_ALPHA, X, Y, Width, Height, XDest, YDest);
               else gfxCopyRawBitmap(&surface, Bitmap, CSRF_DEFAULT_FORMAT|CSRF_OFFSET, X, Y, Width, Height, XDest, YDest);

               ReleaseMemory(surface.Data);
               return ERR_Okay;
            }
            else return log.warning(ERR_AccessMemory);
         }
      }

      gfxReleaseList(ARF_READ);
      return ERR_Search;
   }
   else return log.warning(ERR_AccessMemory);
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

ERROR gfxExposeSurface(OBJECTID SurfaceID, LONG X, LONG Y, LONG Width, LONG Height, LONG Flags)
{
   parasol::Log log(__FUNCTION__);

   if (tlNoDrawing) return ERR_Okay;
   if (!SurfaceID) return ERR_NullArgs;
   if ((Width < 1) or (Height < 1)) return ERR_Okay;

   SurfaceControl *ctl;
   if (!(ctl = gfxAccessList(ARF_READ))) return log.warning(ERR_AccessMemory);

   LONG total = ctl->Total;
   SurfaceList list[total];
   CopyMemory((BYTE *)ctl + ctl->ArrayIndex, list, sizeof(list[0]) * ctl->Total);
   gfxReleaseList(ARF_READ);

   WORD index;
   if ((index = find_surface_list(list, total, SurfaceID)) IS -1) { // The surface might not be listed if the parent is in the process of being dstroyed.
      log.traceWarning("Surface %d is not in the surfacelist.", SurfaceID);
      return ERR_Search;
   }

   return _expose_surface(SurfaceID, list, index, total, X, Y, Width, Height, Flags);
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

ERROR gfxGetSurfaceCoords(OBJECTID SurfaceID, LONG *X, LONG *Y, LONG *AbsX, LONG *AbsY, LONG *Width, LONG *Height)
{
   parasol::Log log(__FUNCTION__);

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

   SurfaceControl *ctl;
   WORD i;

   if ((ctl = gfxAccessList(ARF_READ))) {
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
      if ((i = find_surface_index(ctl, SurfaceID)) IS -1) {
         gfxReleaseList(ARF_READ);
         return ERR_Search;
      }

      if (X)      *X = list[i].X;
      if (Y)      *Y = list[i].Y;
      if (Width)  *Width  = list[i].Width;
      if (Height) *Height = list[i].Height;
      if (AbsX)   *AbsX   = list[i].Left;
      if (AbsY)   *AbsY   = list[i].Top;

     gfxReleaseList(ARF_READ);
      return ERR_Okay;
   }
   else return log.warning(ERR_AccessMemory);
}

/*****************************************************************************

-FUNCTION-
GetSurfaceFlags: Retrieves the Flags field from a surface.

This function returns the current Flags field from a surface.  It provides the same result as reading the field
directly, however it is considered advantageous in circumstances where the overhead of locking a surface object for a
read operation is undesirable.

For information on the available flags, please refer to the Flags field of the @Surface class.

-INPUT-
oid Surface: The surface to query.  If zero, the top-level surface is queried.
&int Flags: The flags value is returned here.

-ERRORS-
Okay
NullArgs
AccessMemory

*****************************************************************************/

ERROR gfxGetSurfaceFlags(OBJECTID SurfaceID, LONG *Flags)
{
   parasol::Log log(__FUNCTION__);

   if (Flags) *Flags = 0;
   else return log.warning(ERR_NullArgs);

   if (!SurfaceID) return log.warning(ERR_NullArgs);

   SurfaceControl *ctl;
   if ((ctl = gfxAccessList(ARF_READ))) {
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);

      LONG i;
      if ((i = find_surface_index(ctl, SurfaceID)) IS -1) {
         gfxReleaseList(ARF_READ);
         return ERR_Search;
      }

      *Flags = list[i].Flags;

      gfxReleaseList(ARF_READ);
      return ERR_Okay;
   }
   else return log.warning(ERR_AccessMemory);
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

ERROR gfxGetSurfaceInfo(OBJECTID SurfaceID, SURFACEINFO **Info)
{
   parasol::Log log(__FUNCTION__);
   static THREADVAR SURFACEINFO info;

   // Note that a SurfaceID of zero is fine (returns the root surface).

   if (!Info) return log.warning(ERR_NullArgs);

   SurfaceControl *ctl;
   if ((ctl = gfxAccessList(ARF_READ))) {
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
      WORD i, root;
      if (!SurfaceID) {
         i = 0;
         root = 0;
      }
      else {
         if ((i = find_surface_index(ctl, SurfaceID)) IS -1) {
            gfxReleaseList(ARF_READ);
            return ERR_Search;
         }
         root = find_bitmap_owner(list, i);
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

      gfxReleaseList(ARF_READ);
      return ERR_Okay;
   }
   else {
      *Info = NULL;
      return log.warning(ERR_AccessMemory);
   }
}

/*****************************************************************************

-FUNCTION-
GetUserFocus: Returns the ID of the surface that currently has the user's focus.

This function returns the unique ID of the surface that has the user's focus.

-RESULT-
oid: Returns the ID of the surface object that has the user focus, or zero on failure.

*****************************************************************************/

OBJECTID gfxGetUserFocus(void)
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

ERROR gfxGetVisibleArea(OBJECTID SurfaceID, LONG *X, LONG *Y, LONG *AbsX, LONG *AbsY, LONG *Width, LONG *Height)
{
   parasol::Log log(__FUNCTION__);

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

   SurfaceControl *ctl;
   if ((ctl = gfxAccessList(ARF_READ))) {
      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);

      WORD i;
      if ((i = find_surface_index(ctl, SurfaceID)) IS -1) {
         gfxReleaseList(ARF_READ);
         return ERR_Search;
      }

      ClipRectangle clip = {
         .Left   = list[i].Left,
         .Top    = list[i].Top,
         .Right  = list[i].Right,
         .Bottom = list[i].Bottom
      };
      restrict_region_to_parents(list, i, &clip, FALSE);

      if (X)      *X      = clip.Left - list[i].Left;
      if (Y)      *Y      = clip.Top - list[i].Top;
      if (Width)  *Width  = clip.Right - clip.Left;
      if (Height) *Height = clip.Bottom - clip.Top;
      if (AbsX)   *AbsX   = clip.Left;
      if (AbsY)   *AbsY   = clip.Top;

      gfxReleaseList(ARF_READ);
      return ERR_Okay;
   }
   else return log.warning(ERR_AccessMemory);
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

ERROR gfxRedrawSurface(OBJECTID SurfaceID, LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Flags)
{
   parasol::Log log(__FUNCTION__);

   if (tlNoDrawing) {
      log.trace("tlNoDrawing: %d", tlNoDrawing);
      return ERR_Okay;
   }

   SurfaceControl *ctl;
   if (!(ctl = gfxAccessList(ARF_READ))) {
      log.warning("Unable to access the surfacelist.");
      return ERR_AccessMemory;
   }

   LONG total = ctl->Total;
   SurfaceList list[total];
   CopyMemory((BYTE *)ctl + ctl->ArrayIndex, list, sizeof(list[0]) * ctl->Total);
   gfxReleaseList(ARF_READ);

   WORD index;
   if ((index = find_surface_list(list, total, SurfaceID)) IS -1) {
      log.traceWarning("Unable to find surface #%d in surface list.", SurfaceID);
      return ERR_Search;
   }

   return _redraw_surface(SurfaceID, list, index, total, Left, Top, Right, Bottom, Flags);
}

//****************************************************************************

ERROR _redraw_surface(OBJECTID SurfaceID, SurfaceList *list, WORD index, WORD Total,
   LONG Left, LONG Top, LONG Right, LONG Bottom, LONG Flags)
{
   parasol::Log log("redraw_surface");
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

   log.traceBranch("[%d] %d/%d Size: %dx%d,%dx%d Expose: %dx%d,%dx%d", SurfaceID, index, Total, list[index].Left, list[index].Top, list[index].Width, list[index].Height, Left, Top, Right-Left, Bottom-Top);

   if ((list[index].Flags & (RNF_REGION|RNF_TRANSPARENT)) and (!recursive)) {
      log.trace("Passing draw request to parent (I am a %s)", (list[index].Flags & RNF_REGION) ? "region" : "invisible");
      WORD parent_index;
      if ((parent_index = find_surface_list(list, Total, list[index].ParentID)) != -1) {
         _redraw_surface(list[parent_index].SurfaceID, list, parent_index, Total, Left, Top, Right, Bottom, Flags & (~IRF_IGNORE_CHILDREN));
      }
      else log.trace("Failed to find parent surface #%d", list[index].ParentID); // No big deal, this often happens when freeing a bunch of surfaces due to the parent/child relationships.
      return ERR_Okay;
   }

   // Check if any of the parent surfaces are invisible

   if (!(Flags & IRF_FORCE_DRAW)) {
      if ((!(list[index].Flags & RNF_VISIBLE)) or (check_visibility(list, index) IS FALSE)) {
         log.trace("Surface is not visible.");
         return ERR_Okay;
      }
   }

   // Because we are executing a redraw, we need to ensure that the surface belongs to our process before going any further.

   if (list[index].TaskID != CurrentTaskID()) {
      log.trace("Surface object #%d belongs to task #%d (we are #%d)", SurfaceID, list[index].TaskID, CurrentTaskID());

      LONG x = Left - list[index].Left;
      LONG y = Top - list[index].Top;
      if (Flags & IRF_IGNORE_CHILDREN) {
         acDrawAreaID(list[index].SurfaceID, x, y, Right - Left, Bottom - Top);
      }
      else drwInvalidateRegionID(list[index].SurfaceID, x, y, Right - Left, Bottom - Top);
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
         while ((list[i].SurfaceID != parent_id) and (i > 0)) i--;

         if (list[i].BitmapID != list[index].BitmapID) break; // Stop if we encounter a separate bitmap

         if (Left   < list[i].Left)   Left   = list[i].Left;
         if (Top    < list[i].Top)    Top    = list[i].Top;
         if (Right  > list[i].Right)  Right  = list[i].Right;
         if (Bottom > list[i].Bottom) Bottom = list[i].Bottom;

         parent_id = list[i].ParentID;
      }
   }

   if ((Left >= Right) or (Top >= Bottom)) return ERR_Okay;

   // Draw the surface graphics into the bitmap buffer

   extSurface *surface;
   ERROR error;
   if (!(error = AccessObject(list[index].SurfaceID, 5000, &surface))) {
      log.trace("Area: %dx%d,%dx%d", Left, Top, Right-Left, Bottom-Top);

      extBitmap *bitmap;
      if (!AccessObject(list[index].BitmapID, 5000, &bitmap)) {
         // Check if there has been a change in the video bit depth.  If so, regenerate the bitmap with a matching depth.

         check_bmp_buffer_depth(surface, bitmap);
         _redraw_surface_do(surface, list, Total, index, Left, Top, Right, Bottom, bitmap, (Flags & IRF_FORCE_DRAW) | ((Flags & (IRF_IGNORE_CHILDREN|IRF_IGNORE_NV_CHILDREN)) ? 0 : URF_REDRAWS_CHILDREN));
         ReleaseObject(bitmap);
      }
      else {
         ReleaseObject(surface);
         return log.warning(ERR_AccessObject);
      }

      ReleaseObject(surface);
   }
   else {
      // If the object does not exist then its task has crashed and we need to remove it from the surface list.

      if (error IS ERR_NoMatchingObject) {
         log.warning("Removing references to surface object #%d (owner crashed).", list[index].SurfaceID);
         untrack_layer(list[index].SurfaceID);
      }
      else log.warning("Unable to access surface object #%d, error %d.", list[index].SurfaceID, error);
      return error;
   }

   // We have done the redraw, so now we can send invalidation messages to any intersecting -child- surfaces for this region.  This process is
   // not recursive (notice the use of IRF_IGNORE_CHILDREN) but all children will be covered due to the way the tree is traversed.

   if (!(Flags & IRF_IGNORE_CHILDREN)) {
      log.trace("Redrawing intersecting child surfaces.");
      WORD level = list[index].Level;
      for (WORD i=index+1; i < Total; i++) {
         if (list[i].Level <= level) break; // End of list - exit this loop

         if (Flags & IRF_IGNORE_NV_CHILDREN) {
            // Ignore children except for those that are volatile
            if (!(list[i].Flags & RNF_VOLATILE)) continue;
         }
         else {
            if ((Flags & IRF_SINGLE_BITMAP) and (list[i].BitmapID != list[index].BitmapID)) continue;
         }

         if ((list[i].Flags & (RNF_REGION|RNF_CURSOR)) or (!(list[i].Flags & RNF_VISIBLE))) {
            continue; // Skip regions and non-visible children
         }

         if ((list[i].Right > Left) and (list[i].Bottom > Top) and
             (list[i].Left < Right) and (list[i].Top < Bottom)) {
            recursive++;
            _redraw_surface(list[i].SurfaceID, list, i, Total, Left, Top, Right, Bottom, Flags|IRF_IGNORE_CHILDREN);
            recursive--;
         }
      }
   }

   return ERR_Okay;
}

//****************************************************************************
// This function fulfils the recursive drawing requirements of _redraw_surface() and is not intended for any other use.

void _redraw_surface_do(extSurface *Self, SurfaceList *list, WORD Total, WORD Index,
                               LONG Left, LONG Top, LONG Right, LONG Bottom, extBitmap *DestBitmap, LONG Flags)
{
   parasol::Log log("redraw_surface");

   if (Self->Flags & (RNF_REGION|RNF_TRANSPARENT)) return;

   if (Index >= Total) log.warning("Index %d > %d", Index, Total);

   ClipRectangle abs;
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

      for (i=Index+1; (i < Total) and (list[i].Level > 1); i++) {
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

            if ((listx < Right) and (listy < Bottom) and (listright > Left) and (listbottom > Top)) {
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

               if ((Flags & URF_REDRAWS_CHILDREN) and (list[i].Level > list[Index].Level)) {
                  // The REDRAWS_CHILDREN flag is used if the caller intends to redraw all children surfaces.
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

   log.traceBranch("Index %d, %dx%d,%dx%d", Index, Left, Top, Right-Left, Bottom-Top);

   // If we have been called recursively due to the presence of volatile/invisible regions (see above),
   // our Index field will not match with the surface that is referenced in Self.  We need to ensure
   // correctness before going any further.

   if (list[Index].SurfaceID != Self->UID) {
      Index = find_surface_list(list, Total, Self->UID);
   }

   // Prepare the buffer so that it matches the exposed area

   if (Self->BitmapOwnerID != Self->UID) {
      for (i=Index; (i > 0) and (list[i].SurfaceID != Self->BitmapOwnerID); i--);
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

   if (((DestBitmap->XOffset + DestBitmap->Clip.Left) < 0) or ((DestBitmap->YOffset + DestBitmap->Clip.Top) < 0) OR
       ((DestBitmap->XOffset + DestBitmap->Clip.Right) > DestBitmap->Width) or ((DestBitmap->YOffset + DestBitmap->Clip.Bottom) > DestBitmap->Height)) {
      log.warning("Invalid coordinates detected (outside of the surface area).  CODE FIX REQUIRED!");
      if ((DestBitmap->XOffset + DestBitmap->Clip.Left) < 0) DestBitmap->Clip.Left = 0;
      if ((DestBitmap->YOffset + DestBitmap->Clip.Top) < 0)  DestBitmap->Clip.Top = 0;
      DestBitmap->Clip.Right = DestBitmap->Width - DestBitmap->XOffset;
      DestBitmap->Clip.Bottom = DestBitmap->Height - DestBitmap->YOffset;
   }

   // Clear the background

   if ((Self->Flags & RNF_PRECOPY) and (!(Self->Flags & RNF_COMPOSITE))) {
      PrecopyRegion *regions;
      LONG x, y, xoffset, yoffset, width, height;
      WORD j;

      if ((Self->PrecopyMID) and (!AccessMemory(Self->PrecopyMID, MEM_READ, 2000, &regions))) {
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
            else if ((regions[j].Dimensions & DMF_X_OFFSET) and
                     (regions[j].Dimensions & DMF_X)) {
               width = Self->Width - x - xoffset;
            }
            else continue;

            // Calculate absolute height

            if (regions[j].Dimensions & DMF_FIXED_HEIGHT) height = regions[j].Height;
            else if (regions[j].Dimensions & DMF_RELATIVE_HEIGHT) height = Self->Height * regions[j].Height / 100;
            else if ((regions[j].Dimensions & DMF_Y_OFFSET) and
                     (regions[j].Dimensions & DMF_Y)) {
               height = Self->Height - y - yoffset;
            }
            else continue;

            if ((width < 1) or (height < 1)) continue;

            // X coordinate check

            if ((regions[j].Dimensions & DMF_X_OFFSET) and (regions[j].Dimensions & DMF_WIDTH)) {
               x = Self->Width - xoffset - width;
            }

            // Y coordinate check

            if ((regions[j].Dimensions & DMF_Y_OFFSET) and
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
      gfxDrawRectangle(DestBitmap, 0, 0, Self->Width, Self->Height, DestBitmap->packPixel(0, 0, 0, 0), TRUE);
   }
   else if (Self->Colour.Alpha > 0) {
      gfxDrawRectangle(DestBitmap, 0, 0, Self->Width, Self->Height, DestBitmap->packPixel(Self->Colour.Red, Self->Colour.Green, Self->Colour.Blue), TRUE);
   }

   // Draw graphics to the buffer

   tlFreeExpose = DestBitmap->UID;

      process_surface_callbacks(Self, DestBitmap);

   tlFreeExpose = NULL;

   // After-copy management

   if (!(Self->Flags & RNF_COMPOSITE)) {
      if (Self->Flags & RNF_AFTER_COPY) {
         #ifdef DBG_DRAW_ROUTINES
            log.trace("After-copy graphics drawing.");
         #endif
         prepare_background(Self, list, Total, Index, DestBitmap, &abs, STAGE_AFTERCOPY);
      }
      else if (Self->Type & RT_ROOT) {
         // If the surface object is part of a global background, we have to look for the root layer and check if it has the AFTERCOPY flag set.

         if ((i = find_surface_list(list, Total, Self->RootID)) != -1) {
            if (list[i].Flags & RNF_AFTER_COPY) {
               #ifdef DBG_DRAW_ROUTINES
                  log.trace("After-copy graphics drawing.");
               #endif
               prepare_background(Self, list, Total, Index, DestBitmap, &abs, STAGE_AFTERCOPY);
            }
         }
      }
   }
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

OBJECTID gfxSetModalSurface(OBJECTID SurfaceID)
{
   parasol::Log log(__FUNCTION__);

   if (GetClassID(SurfaceID) != ID_SURFACE) return 0;

   log.branch("#%d, CurrentFocus: %d", SurfaceID, gfxGetUserFocus());

   OBJECTID old_modal = 0;

   // Check if the surface is invisible, in which case the mode has to be diverted to the modal that was previously
   // targetted or turned off altogether if there was no previously modal surface.

   if (SurfaceID) {
      extSurface *surface;
      OBJECTID divert = 0;
      if (!AccessObject(SurfaceID, 3000, &surface)) {
         if (!(surface->Flags & RNF_VISIBLE)) {
            divert = surface->PrevModalID;
            if (!divert) SurfaceID = 0;
         }
         ReleaseObject(surface);
      }
      if (divert) return gfxSetModalSurface(divert);
   }

   if (!SysLock(PL_PROCESSES, 3000)) {
      LONG maxtasks = GetResource(RES_MAX_PROCESSES);
      OBJECTID focus = 0;
      TaskList *tasks;
      if ((tasks = (TaskList *)GetResourcePtr(RES_TASK_LIST))) {
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
         if ((!gfxGetSurfaceFlags(SurfaceID, &flags)) and (!(flags & RNF_HAS_FOCUS))) {
            acFocusID(SurfaceID);
         }
      }
   }

   return old_modal;
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

ERROR gfxLockBitmap(OBJECTID SurfaceID, objBitmap **Bitmap, LONG *Info)
{
   parasol::Log log(__FUNCTION__);
#if 0
   // This technique that we're using to acquire the bitmap is designed to prevent deadlocking.
   //
   // COMMENTED OUT: May be causing problems with X11?

   LONG i;

   if (Info) *Info = 0;

   if ((!SurfaceID) or (!Bitmap)) return log.warning(ERR_NullArgs);

   *Bitmap = 0;

   extSurface *surface;
   if (!AccessObject(SurfaceID, 5000, &surface)) {
      extBitmap *bitmap;
      if (AccessObject(surface->BufferID, 5000, &bitmap) != ERR_Okay) {
         ReleaseObject(surface);
         return log.warning(ERR_AccessObject);
      }

      ReleaseObject(surface);

      SurfaceControl *ctl;
      if ((ctl = gfxAccessList(ARF_READ))) {
         auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);

         WORD i;
         if ((i = find_surface_index(ctl, SurfaceID)) IS -1) {
            ReleaseObject(bitmap);
            gfxReleaseList(ARF_READ);
            return ERR_Search;
         }

         LONG root = find_bitmap_owner(list, i);

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

         gfxReleaseList(ARF_READ);

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
         return log.warning(ERR_AccessMemory);
      }
   }
   else return log.warning(ERR_AccessObject);

#else

   if (Info) *Info = NULL;

   if ((!SurfaceID) or (!Bitmap)) return log.warning(ERR_NullArgs);

   SurfaceControl *ctl;
   if ((ctl = gfxAccessList(ARF_READ))) {
      WORD i;
      if ((i = find_surface_index(ctl, SurfaceID)) IS -1) {
         gfxReleaseList(ARF_READ);
         return ERR_Search;
      }

      auto list = (SurfaceList *)((BYTE *)ctl + ctl->ArrayIndex);
      LONG root = find_bitmap_owner(list, i);

      SurfaceList list_root = list[root];
      SurfaceList list_zero = list[0];
      OBJECTID bitmap_id = list[i].BitmapID;

      ClipRectangle expose = {
         .Left   = list_root.Left,
         .Top    = list_root.Top,
         .Right  = list_root.Right,
         .Bottom = list_root.Bottom
      };

      if (restrict_region_to_parents(list, i, &expose, TRUE) IS -1) {
         // The surface is not within a visible area of the available bitmap space
         gfxReleaseList(ARF_READ);
         return ERR_OutOfBounds;
      }

      gfxReleaseList(ARF_READ);

      if (!list_root.BitmapID) return log.warning(ERR_Failed);

      // Gain access to the bitmap buffer and set the clipping and offsets to the correct values.

      extBitmap *bmp;
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
      else return log.warning(ERR_AccessObject);
   }
   else return log.warning(ERR_AccessMemory);

#endif
}

/*****************************************************************************

-FUNCTION-
ReleaseList: Private. Releases access to the internal surfacelist array.

-INPUT-
int(ARF) Flags: Use the same flags as in in the previous call to gfxAccessList().

*****************************************************************************/

void gfxReleaseList(LONG Flags)
{
   if (tlListCount > 0) {
      tlListCount--;
      if (!tlListCount) {
         ReleaseMemory(tlSurfaceList);
         tlSurfaceList = NULL;
      }
   }
   else {
      parasol::Log log(__FUNCTION__);
      log.warning("drwReleaseList() called without an existing lock.");
   }
}

/*****************************************************************************

-FUNCTION-
UnlockBitmap: Unlocks any earlier call to gfxLockBitmap().

Call the UnlockBitmap() function to release a surface object from earlier calls to ~LockBitmap().

-INPUT-
oid Surface:        The ID of the surface object that you are releasing.
ext(Bitmap) Bitmap: Pointer to the bitmap structure returned earlier by LockBitmap().

-ERRORS-
Okay: The bitmap has been unlocked successfully.
NullArgs:

*****************************************************************************/

ERROR gfxUnlockBitmap(OBJECTID SurfaceID, extBitmap *Bitmap)
{
   if ((!SurfaceID) or (!Bitmap)) return ERR_NullArgs;
   ReleaseObject(Bitmap);
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
WindowHook: Adds a function hook for receiving window messages from a host desktop.

Adds a function hook for receiving window events from a host desktop.

-INPUT-
oid SurfaceID: A hosted surface to be monitored.
int(WH) Event: A window hook event.
ptr(func) Callback: A function to callback when the event is triggered.

-ERRORS-
Okay
NullArgs

-END-

*****************************************************************************/

ERROR gfxWindowHook(OBJECTID SurfaceID, LONG Event, FUNCTION *Callback)
{
   if ((!SurfaceID) or (!Event) or (!Callback)) return ERR_NullArgs;

   const WindowHook hook(SurfaceID, Event);
   glWindowHooks[hook] = *Callback;
   return ERR_Okay;
}

