/*********************************************************************************************************************

-FIELD-
BitsPerPixel: Defines the number of bits per pixel for a surface.

The BitsPerPixel field may be set prior to initialisation in order to force a particular bits-per-pixel setting that
may not match the display.  This will result in the graphics system converting each pixel when drawing the surface to
the display and as such is not recommended.

*********************************************************************************************************************/

static ERROR GET_BitsPerPixel(extSurface *Self, LONG *Value)
{
   SURFACEINFO *info;
   if (!gfxGetSurfaceInfo(Self->UID, &info)) {
      *Value = info->BitsPerPixel;
   }
   else *Value = 0;
   return ERR_Okay;
}

static ERROR SET_BitsPerPixel(extSurface *Self, LONG Value)
{
   Self->BitsPerPixel = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Buffer: The ID of the bitmap that manages the surface's graphics.

Each surface is assigned a bitmap buffer that is referred to in this field. In many cases the bitmap will be shared
between multiple surfaces.  A client should avoid interacting with the buffer unless circumstances are such that
there are no other means to get access to internal graphics information.

Please note that the bitmap object represents an off-screen, temporary buffer.  Drawing to the bitmap directly will
have no impact on the display.

-FIELD-
Colour: String-based field for setting the background colour.

If the surface object should have a plain background colour, set this field to the colour value that you want to use.
The colour must be specified in the standard format of '#RRGGBB' for hexadecimal or 'Red,Green,Blue' for a decimal
colour.

Surface objects that do not have a colour will not be cleared when being drawn.  The background will thus consist of
'junk' graphics and the background will need to be drawn using another method.  This gives your the power to choose the
fastest drawing model to suit your needs.

If you set the Colour and later want to turn the background colour off, write a NULL value to the Colour field or set
the Alpha component to zero.  Changing the Colour field does not cause a graphics redraw.

-FIELD-
Cursor: A default cursor image can be set here for changing the mouse pointer.

The Cursor field provides a convenient way of setting the pointer's cursor image in a single operation.  The mouse
pointer will automatically switch to the specified cursor image when it enters the surface area.

The available cursor image settings are listed in the @Pointer.CursorID documentation.

The Cursor field may be written with valid cursor names or their ID's, as you prefer.

*********************************************************************************************************************/

static ERROR SET_Cursor(extSurface *Self, LONG Value)
{
   Self->Cursor = Value;
   if (Self->initialised()) {
      UpdateSurfaceField(Self, &SurfaceRecord::Cursor, (BYTE)Self->Cursor);
      OBJECTID pointer_id;
      if (!FindObject("SystemPointer", ID_POINTER, FOF::NIL, &pointer_id)) {
         acRefresh(pointer_id);
      }
   }
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Display: Refers to the @Display object that is managing the surface's graphics.

All surfaces belong to a @Display object that manages drawing to the user's video display.  This field refers
to the Display object of which the surface is a member.

-FIELD-
Drag: This object-based field is used to control the dragging of objects around the display.

Click-dragging of surfaces is enabled by utilising the Drag field.

To use, write this field with reference to a Surface that is to be dragged when the user starts a click-drag operation.
For instance, if you create a window with a title-bar at the top, you would set the Drag field of the title-bar to
point to the object ID of the window. If necessary, you can set the Drag field to point back to your surface object
(this can be useful for creating icons and other small widgets).

To turn off dragging, set the field to zero.

*********************************************************************************************************************/

static ERROR SET_Drag(extSurface *Self, OBJECTID Value)
{
   if (Value) {
      auto callback = make_function_stdc(consume_input_events);
      if (!gfxSubscribeInput(&callback, Self->UID, JTYPE_MOVEMENT|JTYPE_BUTTON, 0, &Self->InputHandle)) {
         Self->DragID = Value;
         return ERR_Okay;
      }
      else return ERR_Failed;
   }
   else {
      if (Self->InputHandle) { gfxUnsubscribeInput(Self->InputHandle); Self->InputHandle = 0; }
      Self->DragID = 0;
      return ERR_Okay;
   }
}

/*********************************************************************************************************************

-FIELD-
DragStatus: Indicates the draggable state when dragging is enabled.

If the surface is draggable, the DragStatus indicates the current state of the surface with respect to it being
dragged.

-FIELD-
Flags: Optional flags.

The Flags field allows special options to be set for a surface object.  Use a logical-OR operation when setting this
field so that existing flags are not overwritten.  To not do so can produce unexpected behaviour.

*********************************************************************************************************************/

static ERROR SET_Flags(extSurface *Self, LONG Value)
{
   LONG flags = (Self->Flags & RNF_READ_ONLY) | (Value & (~RNF_READ_ONLY));

   if (Self->initialised()) flags = flags & (~RNF_INIT_ONLY);

   if (flags != Self->Flags) {
      Self->Flags = flags;
      UpdateSurfaceField(Self, &SurfaceRecord::Flags, flags);
   }

   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Modal: Sets the surface as modal (prevents user interaction with other surfaces).

If TRUE, the surface will become the modal surface for the program when it is shown.  This prevents interaction with
other surfaces until the modal surface is either hidden or destroyed.  Children of the modal surface may be interacted
with normally.

*********************************************************************************************************************/

static ERROR SET_Modal(extSurface *Self, LONG Value)
{
   if ((!Value) and (Self->Modal)) {
      if (Self->PrevModalID) {
         gfxSetModalSurface(Self->PrevModalID);
         Self->PrevModalID = 0;
      }
      else if (gfxGetModalSurface() IS Self->UID) gfxSetModalSurface(0);
   }

   Self->Modal = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Movement: Limits the movement of a surface object to vertical or horizontal shifts.
Lookup: MOVE

The directions in which a surface object can move can be limited by setting the Movement field.  By default, a surface
object is capable of moving both horizontally and vertically.

This field is only effective in relation to the Move action, and it is possible to circumvent the restrictions by
setting the coordinate fields directly.

*********************************************************************************************************************/

static ERROR SET_Movement(extSurface *Self, LONG Flags)
{
   if (Flags IS MOVE_HORIZONTAL) Self->Flags = (Self->Flags & RNF_NO_HORIZONTAL) | RNF_NO_VERTICAL;
   else if (Flags IS MOVE_VERTICAL) Self->Flags = (Self->Flags & RNF_NO_VERTICAL) | RNF_NO_HORIZONTAL;
   else if (Flags IS (MOVE_HORIZONTAL|MOVE_VERTICAL)) Self->Flags &= ~(RNF_NO_VERTICAL | RNF_NO_HORIZONTAL);
   else Self->Flags |= RNF_NO_HORIZONTAL|RNF_NO_VERTICAL;

   UpdateSurfaceField(Self, &SurfaceRecord::Flags, Self->Flags);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Opacity: Affects the level of translucency applied to a surface object.

This field determines the translucency level of a surface area.  The default setting is 100%, which means that the
surface will be solid.  Any other value that you set here will alter the impact of a surface over its destination area.
High values will retain the boldness of the graphics, while low values can surface it close to invisible.

Note: The translucent drawing routine works by drawing the surface content to its internal buffer first, then copying
the graphics that are immediately in the background straight over the top with an alpha-blending routine.  This is not
always ideal and better results might be obtainable with the pre-copy feature.

Please note that the use of translucency is realised at a significant cost to CPU usage.

*********************************************************************************************************************/

static ERROR GET_Opacity(extSurface *Self, DOUBLE *Value)
{
   *Value = Self->Opacity * 100 / 255;
   return ERR_Okay;
}

static ERROR SET_Opacity(extSurface *Self, DOUBLE Value)
{
   LONG opacity;

   // NB: It is OK to set the opacity on a surface object when it does not own its own bitmap, as the aftercopy
   // routines will refer the copy so that it starts from the bitmap owner.

   if (Value >= 100) {
      opacity = 255;
      if (opacity IS Self->Opacity) return ERR_Okay;
      Self->Flags &= ~RNF_AFTER_COPY;
   }
   else {
      if (Value < 0) opacity = 0;
      else opacity = (Value * 255) / 100;
      if (opacity IS Self->Opacity) return ERR_Okay;
      Self->Flags |= RNF_AFTER_COPY; // See PrepareBackground() to see what these flags are for

      // NB: Currently the combination of PRECOPY and AFTERCOPY at the same time is permissible,
      // e.g. icons need this feature so that they can fade in and out of the desktop.
   }

   Self->Opacity = opacity;
   UpdateSurfaceRecord(Self); // Update Opacity, Flags

   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Parent: The parent for a surface is defined here.

The parent for child surfaces is defined here.  Top level surfaces will have no parent.  If the Parent field is not set
prior to initialisation, the surface class will attempt to discover a valid parent by checking its ownership chain for
a surface object.  This behaviour can be switched off by setting a Parent of zero prior to initialisation.

*********************************************************************************************************************/

static ERROR SET_Parent(extSurface *Self, LONG Value)
{
   // To change the parent post-initialisation, we have to re-track the surface so that it is correctly repositioned
   // within the surface lists.

   if (Self->initialised()) {
      if (!Self->ParentID) return ERR_Failed; // Top level surfaces cannot be re-parented
      if (Self->ParentID IS Value) return ERR_Okay;

      acHide(Self);

      Self->ParentID = Value;
      Self->ParentDefined = true;

      const std::lock_guard<std::recursive_mutex> lock(glSurfaceLock);

      LONG index, parent;
      if ((index = find_surface_list(Self)) != -1) {
         if (!Value) parent = 0;
         else for (parent=0; (glSurfaces[parent].SurfaceID) and (glSurfaces[parent].SurfaceID != Self->ParentID); parent++);

         if (glSurfaces[parent].SurfaceID) move_layer_pos(glSurfaces, index, parent + 1);

         // Reset bitmap and buffer information in the list


      }

      acShow(Self);
   }
   else {
      Self->ParentID = Value;
      Self->ParentDefined = true;
   }

   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
PopOver: Keeps a surface in front of another surface in the Z order.

Setting the PopOver field to a sibling surface ID will keep the surface in front of its sibling at all times.  For
dialog windows, it is recommended that the popover and modal options be combined together to prevent interaction with
other surfaces created by the current program.

Setting the PopOver field to zero will return the surface to its normal state.

If an object that does not belong to the Surface class is detected, an attempt will be made to read that object's
Surface field, if available.  If this does not yield a valid surface then ERR_InvalidObject is returned.

*********************************************************************************************************************/

static ERROR SET_PopOver(extSurface *Self, OBJECTID Value)
{
   pf::Log log;

   if (Value IS Self->UID) return ERR_Okay;

   if (Self->initialised()) return log.warning(ERR_Immutable);

   if (Value) {
      CLASSID class_id = GetClassID(Value);
      if (class_id != ID_SURFACE) {
         OBJECTPTR obj;
         if (!AccessObject(Value, 3000, &obj)) {
            obj->get(FID_Surface, &Value);
            ReleaseObject(obj);
         }
         else return ERR_AccessObject;

         if (class_id != ID_SURFACE) return log.warning(ERR_InvalidObject);
      }
   }

   Self->PopOverID = Value;
   return ERR_Okay;
}

static ERROR SET_RevertFocus(extSurface *Self, OBJECTID Value)
{
   Self->RevertFocusID = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
RootLayer: Private

*********************************************************************************************************************/

static ERROR SET_RootLayer(extSurface *Self, OBJECTID Value)
{
   Self->RootID = Value;
   UpdateSurfaceField(Self, &SurfaceRecord::RootID, Value); // Update RootLayer
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
UserFocus: Refers to the surface object that has the current focus.

Returns the surface object that has the primary user focus.  Returns NULL if no object has the focus.

*********************************************************************************************************************/

static ERROR GET_UserFocus(extSurface *Self, OBJECTID *Value)
{
   const std::lock_guard<std::mutex> lock(glFocusLock);
   *Value = glFocusList[0];
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Visible: Indicates the visibility of a surface object.

If you need to know if a surface object is visible or hidden, you can read this field to find out either way.  A TRUE
value is returned if the object is visible and FALSE is returned if the object is invisible.  Note that visibility is
subject to the properties of the container that the surface object resides in.  For example, if a surface object is
visible but is contained within a surface object that is invisible, the end result is that both objects are actually
invisible.

Visibility is directly affected by the Hide and Show actions if you wish to change the visibility of a surface object.

*********************************************************************************************************************/

static ERROR GET_Visible(extSurface *Self, LONG *Value)
{
   if (Self->Flags & RNF_VISIBLE) *Value = TRUE;
   else *Value = FALSE;
   return ERR_Okay;
}

static ERROR SET_Visible(extSurface *Self, LONG Value)
{
   if (Value) acShow(Self);
   else acHide(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
WindowType: Indicator for surfaces that represent themselves as a desktop window.
Lookup: SWIN

This field affects a surface's status on hosted desktops such as Windows and X11. It only affects top-level surfaces
that have no parent - child surfaces ignore this field.  Surfaces created in the desktop area will also ignore
this field, as the desktop is treated as a parent.

It is the responsibility of the developer to provide window gadgets such as titlebars and set the resize borders for
custom surfaces.

*********************************************************************************************************************/

static ERROR GET_WindowType(extSurface *Self, LONG *Value)
{
   *Value = Self->WindowType;
   return ERR_Okay;
}

static ERROR SET_WindowType(extSurface *Self, LONG Value)
{
   if (Self->initialised()) {
      pf::Log log;
      bool border;
      LONG flags;
      objDisplay *display;

      if (Self->WindowType IS Value) {
         log.trace("WindowType == %d", Value);
         return ERR_Okay;
      }

      if (Self->DisplayID) {
         if (!AccessObject(Self->DisplayID, 2000, &display)) {
            log.trace("Changing window type to %d.", Value);

            switch(Value) {
               case SWIN_TASKBAR:
               case SWIN_ICON_TRAY:
               case SWIN_NONE:
                  border = false;
                  break;
               default:
                  border = true;
                  break;
            }

            if (border) {
               if (display->Flags & SCR_BORDERLESS) {
                  flags = display->Flags & (~SCR_BORDERLESS);
                  display->setFlags(flags);
               }
            }
            else if (!(display->Flags & SCR_BORDERLESS)) {
               flags = display->Flags | SCR_BORDERLESS;
               display->setFlags(flags);
            }

            Self->WindowType = Value;
            ReleaseObject(display);
         }
         else return ERR_AccessObject;
      }
      else return log.warning(ERR_NoSupport);
   }
   else Self->WindowType = Value;

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
WindowHandle: Refers to a surface object's window handle, if relevant.

This field refers to the window handle of a surface object, but only if such a thing is relevant to the platform that
the system is running on.  Currently, this field is only usable when creating a primary surface object within an X11
window manager or Microsoft Windows.

It is possible to set the WindowHandle field prior to initialisation if you want a surface object to be based on a
window that already exists.
-END-

*********************************************************************************************************************/

static ERROR GET_WindowHandle(extSurface *Self, APTR *Value)
{
   *Value = (APTR)Self->DisplayWindow;
   return ERR_Okay;
}

static ERROR SET_WindowHandle(extSurface *Self, APTR Value)
{
   if (Self->initialised()) return ERR_Failed;
   if (Value) Self->DisplayWindow = Value;
   return ERR_Okay;
}

