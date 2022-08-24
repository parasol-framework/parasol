
/*****************************************************************************
-ACTION-
Redimension: Moves and resizes a surface object in a single action call.
-END-
*****************************************************************************/

static ERROR SURFACE_Redimension(objSurface *Self, struct acRedimension *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs)|ERF_Notified;

   if ((Args->Width < 0) or (Args->Height < 0)) {
      log.trace("Bad width/height: %.0fx%.0f", Args->Width, Args->Height);
      return ERR_Args|ERF_Notified;
   }

   Message *msg;
   if ((msg = GetActionMsg())) { // If this action was called as a message, then it could have been delayed and thus superseded by a more recent call.
      if (msg->Time < Self->LastRedimension) {
         log.trace("Ignoring superseded redimension message (" PF64() " < " PF64() ").", msg->Time, Self->LastRedimension);
         return ERR_Okay|ERF_Notified;
      }
   }

   // 2013-12: For a long time this sub-routine was commented out.  Have brought it back to try and keep the queue
   // clear of redundant redimension messages.  Seems fine...

   if (Self->Flags & RNF_VISIBLE) { // Visibility check because this sub-routine doesn't play nice with hidden surfaces.
      APTR queue;
      if (!AccessMemory(GetResource(RES_MESSAGE_QUEUE), MEM_READ_WRITE, 3000, &queue)) {
         UBYTE msgbuffer[sizeof(Message) + sizeof(ActionMessage)];
         LONG index = 0;
         while (!ScanMessages(queue, &index, MSGID_ACTION, msgbuffer, sizeof(msgbuffer))) {
            auto action = (ActionMessage *)(msgbuffer + sizeof(Message));
            if ((action->ActionID IS AC_Redimension) and (action->ObjectID IS Self->Head.UID)) {
               ReleaseMemory(queue);
               return ERR_Okay|ERF_Notified;
            }
         }
         ReleaseMemory(queue);
      }
   }

   Self->LastRedimension = PreciseTime();

   LONG oldx      = Self->X;
   LONG oldy      = Self->Y;
   LONG oldwidth  = Self->Width;
   LONG oldheight = Self->Height;

   // Extract the new dimensions from the arguments

   LONG newwidth, newheight;
   LONG newx = F2T(Args->X);
   LONG newy = F2T(Args->Y);
   if (!Args->Width) newwidth = Self->Width;
   else newwidth = F2T(Args->Width);
   if (!Args->Height) newheight = Self->Height;
   else newheight = F2T(Args->Height);

   // Ensure that the requested width does not exceed minimum and maximum values

   if ((Self->MinWidth > 0) and (newwidth < Self->MinWidth + Self->LeftMargin + Self->RightMargin)) {
      if (oldwidth > newwidth) {
         if (oldwidth > Self->MinWidth + Self->LeftMargin + Self->RightMargin) newwidth = Self->MinWidth + Self->LeftMargin + Self->RightMargin;
         else newwidth = oldwidth; // Maintain the current width because it is < MinWidth
      }
   }

   if ((newwidth > Self->MaxWidth + Self->LeftMargin + Self->RightMargin)) newwidth = Self->MaxWidth + Self->LeftMargin + Self->RightMargin;

   if (newwidth < 2) newwidth = 2;

   // Check requested height against minimum and maximum height values

   if ((Self->MinHeight > 0) and (newheight < Self->MinHeight + Self->TopMargin + Self->BottomMargin)) {
      if (oldheight > newheight) {
         if (oldheight > Self->MinHeight + Self->TopMargin + Self->BottomMargin) newheight = Self->MinHeight + Self->TopMargin + Self->BottomMargin;
         else newheight = oldheight;
      }
   }

   if ((Self->MaxHeight > 0) and (newheight > Self->MaxHeight + Self->TopMargin + Self->BottomMargin)) {
      newheight = Self->MaxHeight + Self->TopMargin + Self->BottomMargin;
   }

   if (newheight < 2) newheight = 2;

   // Check for changes

   if ((newx IS oldx) and (newy IS oldy) and (newwidth IS oldwidth) and (newheight IS oldheight)) {
      return ERR_Okay|ERF_Notified;
   }

   log.traceBranch("%dx%d %dx%d (req. %dx%d, %dx%d) Depth: %.0f $%.8x", newx, newy, newwidth, newheight, F2T(Args->X), F2T(Args->Y), F2T(Args->Width), F2T(Args->Height), Args->Depth, Self->Flags);

   ERROR error = resize_layer(Self, newx, newy, newwidth, newheight, newwidth, newheight, F2T(Args->Depth), 0.0, NULL);
   return error|ERF_Notified;
}

/*****************************************************************************
-ACTION-
Resize: Alters the dimensions of a surface object.
-END-
*****************************************************************************/

static ERROR SURFACE_Resize(objSurface *Self, struct acResize *Args)
{
   if (!Args) return ERR_NullArgs|ERF_Notified;

   if (((!Args->Width) or (Args->Width IS Self->Width)) and
       ((!Args->Height) or (Args->Height IS Self->Height))) return ERR_Okay|ERF_Notified;

   struct acRedimension redimension = { (DOUBLE)Self->X, (DOUBLE)Self->Y, 0, Args->Width, Args->Height, Args->Depth };
   return Action(AC_Redimension, Self, &redimension)|ERF_Notified;
}

/*****************************************************************************

-METHOD-
SetDisplay: Changes the screen resolution (applies to top-level surface objects only).

The SetDisplay method is used to change the screen resolution of the top-level surface object (which represents the
screen display).  It allows you to set the size of the display and you may also change the bitmap depth and the
monitor's refresh rate.  If successful, the change is immediate.

This method exercises some intelligence in adjusting the display to your requested settings.  For instance, if the
requested width and/or height is not available, the closest display setting will be chosen.

This method does not work on anything other than top-level surface objects.  The current top-level surface object is
usually named "SystemSurface" by default and can be searched for by that name.

-INPUT-
int X: The horizontal coordinate/offset for the display.
int Y: The vertical coordinate/offset for the display.
int Width: The width of the display.
int Height: The height of the display.
int InsideWidth: The page width of the display must be the same as Width or greater.
int InsideHeight: The page height of the display must be the same as Height or greater.
int BitsPerPixel: Bits per pixel - 15, 16, 24 or 32.
double RefreshRate: Refresh rate.
int Flags: Optional flags.

-ERRORS-
Okay
Args
Failed
-END-

*****************************************************************************/

static ERROR SURFACE_SetDisplay(objSurface *Self, struct gfxSetDisplay *Args)
{
   parasol::Log log;

   if ((!Args) or (Args->Width < 0) or (Args->Height < 0)) return log.warning(ERR_Args);
   if (Self->ParentID) return log.warning(ERR_Failed);

   LONG newx = Args->X;
   LONG newy = Args->Y;

   LONG newwidth, newheight;

   if (!Args->Width) newwidth = Self->Width;
   else newwidth = Args->Width;
   if (!Args->Height) newheight = Self->Height;
   else newheight = Args->Height;

   //if ((newx IS Self->X) and (newy IS Self->Y) and (newwidth IS Self->Width) and (newheight IS Self->Height)) return ERR_Okay;

   log.branch("%dx%d,%dx%d, BPP %d", newx, newy, newwidth, newheight, Args->BitsPerPixel);

   ERROR error = resize_layer(Self, newx, newy, newwidth, newheight,
      Args->InsideWidth, Args->InsideHeight, Args->BitsPerPixel,
      Args->RefreshRate, Args->Flags);

   return error;
}

//****************************************************************************
// This function is responsible for managing the resizing of top-most surface objects and is also used by some of the
// field management functions for Width/Height adjustments.
//
// This function is also useful for skipping the dimension limits normally imposed when resizing.

ERROR resize_layer(objSurface *Self, LONG X, LONG Y, LONG Width, LONG Height, LONG InsideWidth,
   LONG InsideHeight, LONG BPP, DOUBLE RefreshRate, LONG DeviceFlags)
{
   if (!Width)  Width = Self->Width;
   if (!Height) Height = Self->Height;

   if (!(Self->Head.Flags & NF_INITIALISED)) {
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

   if (Self->BitmapOwnerID IS Self->Head.UID) {
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

   if (!(Self->Head.Flags & NF_INITIALISED)) return ERR_Okay;

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
      if ((index = find_surface_list(cplist, total, Self->Head.UID)) IS -1) { // The surface might not be listed if the parent is in the process of being dstroyed.
         return ERR_Search;
      }

      parasol::Log log;
      log.traceBranch("Redrawing the resized surface.");

      _redraw_surface(Self->Head.UID, cplist, index, total, cplist[index].Left, cplist[index].Top, cplist[index].Right, cplist[index].Bottom, 0);
      _expose_surface(Self->Head.UID, cplist, index, total, 0, 0, Self->Width, Self->Height, EXF_CHILDREN|EXF_REDRAW_VOLATILE_OVERLAP);

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

         struct ClipRectangle region_b = {
            .Left   = cplist[parent_index].Left + oldx,
            .Right  = (cplist[parent_index].Left + oldx) + oldw,
            .Bottom = (cplist[parent_index].Top + oldy) + oldh,
            .Top    = cplist[parent_index].Top + oldy
         };

         struct ClipRectangle region_a = {
            .Left   = cplist[index].Left,
            .Right  = cplist[index].Right,
            .Bottom = cplist[index].Bottom,
            .Top    = cplist[index].Top
         };

         if (Self->BitmapOwnerID IS Self->Head.UID) {
            redraw_nonintersect(Self->ParentID, cplist, parent_index, total, &region_a, &region_b, -1, EXF_CHILDREN|EXF_REDRAW_VOLATILE);
         }
         else redraw_nonintersect(Self->ParentID, cplist, parent_index, total, &region_a, &region_b, 0, EXF_CHILDREN|EXF_REDRAW_VOLATILE);

         tlVolatileIndex = 0;
      }
   }

   refresh_pointer(Self);
   return ERR_Okay;
}
