
/*****************************************************************************
-ACTION-
Redimension: Moves and resizes a surface object in a single action call.
-END-
*****************************************************************************/

static ERROR SURFACE_Redimension(extSurface *Self, struct acRedimension *Args)
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
         log.trace("Ignoring superseded redimension message (%" PF64 " < %" PF64 ").", msg->Time, Self->LastRedimension);
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
            if ((action->ActionID IS AC_Redimension) and (action->ObjectID IS Self->UID)) {
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

static ERROR SURFACE_Resize(extSurface *Self, struct acResize *Args)
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

static ERROR SURFACE_SetDisplay(extSurface *Self, struct gfxSetDisplay *Args)
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
