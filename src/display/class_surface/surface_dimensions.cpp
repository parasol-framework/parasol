
/*********************************************************************************************************************

-FIELD-
AbsX: The absolute horizontal position of a surface object.

This field returns the absolute horizontal position of a surface object. The absolute value is calculated based on the
surface object's position relative to the top most surface object in the local hierarchy.

It is possible to set this field, but only after initialisation of the surface object has occurred.

*********************************************************************************************************************/

static ERR GET_AbsX(extSurface *Self, LONG *Value)
{
   const std::lock_guard<std::recursive_mutex> lock(glSurfaceLock);

   LONG i;
   if ((i = find_surface_list(Self)) != -1) {
      *Value = glSurfaces[i].Left;
      return ERR::Okay;
   }
   else return ERR::Search;
}

static ERR SET_AbsX(extSurface *Self, LONG Value)
{
   pf::Log log;

   if (Self->initialised()) {
      const std::lock_guard<std::recursive_mutex> lock(glSurfaceLock);

      LONG parent, x;
      if ((parent = find_parent_list(glSurfaces, Self)) != -1) {
         x = Value - glSurfaces[parent].Left;
         move_layer(Self, x, Self->Y);
         return ERR::Okay;
      }
      else return log.warning(ERR::Search);
   }
   else return log.warning(ERR::NotInitialised);
}

/*********************************************************************************************************************

-FIELD-
AbsY: The absolute vertical position of a surface object.

This field returns the absolute vertical position of a surface object. The absolute value is calculated based on the
surface object's position relative to the top most surface object in the local hierarchy.

It is possible to set this field, but only after initialisation of the surface object has occurred.

*********************************************************************************************************************/

static ERR GET_AbsY(extSurface *Self, LONG *Value)
{
   const std::lock_guard<std::recursive_mutex> lock(glSurfaceLock);

   LONG i;
   if ((i = find_surface_list(Self)) != -1) {
      *Value = glSurfaces[i].Top;
      return ERR::Okay;
   }
   else return ERR::Search;
}

static ERR SET_AbsY(extSurface *Self, LONG Value)
{
   pf::Log log;

   if (Self->initialised()) {
      const std::lock_guard<std::recursive_mutex> lock(glSurfaceLock);

      LONG parent, y;
      if ((parent = find_parent_list(glSurfaces, Self)) != -1) {
         y = Value - glSurfaces[parent].Top;
         move_layer(Self, Self->X, y);
         return ERR::Okay;
      }

      return log.warning(ERR::Search);
   }
   else return log.warning(ERR::NotInitialised);
}

/*********************************************************************************************************************

-FIELD-
Align: This field allows you to align a surface area within its owner.

If you would like to set an abstract position for a surface area, you can give it an alignment.  This feature is most
commonly used for horizontal and vertical centring, as aligning to the the edges of a surface area is already handled
by existing dimension fields.  Note that setting the alignment overrides any settings in related coordinate fields.
Valid alignment flags are `BOTTOM`, `CENTER/MIDDLE`, `LEFT`, `HORIZONTAL`, `RIGHT`, `TOP`, `VERTICAL`.

-FIELD-
Bottom: Returns the bottom-most coordinate of a surface object (Y + Height).

*********************************************************************************************************************/

static ERR GET_Bottom(extSurface *Self, LONG *Bottom)
{
   *Bottom = Self->Y + Self->Height;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
BottomLimit: Prevents a surface object from moving beyond a given point at the bottom of its container.

A client can prevent a surface object from moving beyond a given point at the bottom of its container by setting this field.
If for example you were to set the BottomLimit to 5, then any attempt to move the surface object into or beyond the 5
units at the bottom of its container would fail.

Limits only apply to movement, as induced through the Move() action.  This means that limits can be over-ridden by
setting the coordinate fields directly (which can be useful in certain cases).

*********************************************************************************************************************/

static ERR SET_BottomLimit(extSurface *Self, LONG Value)
{
   Self->BottomLimit = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
BottomMargin: Manipulates the bottom margin of a surface object.

The Surface class supports margin settings, which are similar to the concept of margins on printed paper.  Margin
values have no significant meaning or effect on a surface object itself, but they are often used by other objects and
can be helpful in interface construction.  For instance, the Window template uses margins to indicate the space
available for placing graphics and other surface objects inside of it.

By default, all margins are set to zero when a new surface object is created.

*********************************************************************************************************************/

static ERR SET_BottomMargin(extSurface *Self, LONG Value)
{
   if (Value < 0) Self->BottomMargin = -Value;
   else Self->BottomMargin = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Dimensions: Indicates currently active dimension settings.
Lookup: DMF

The dimension settings of a surface object can be read from this field.  The flags indicate the dimension fields that
are in use, and whether the values are fixed or relative.

It is strongly recommended that this field is never set manually, because the flags are automatically managed for the
client when setting fields such as #X and #Width.  If circumstances require manual configuration, take care to ensure
that the flags do not conflict.  For instance, `FIXED_X` and `SCALED_X` cannot be paired, nor could `FIXED_X`,
`FIXED_XOFFSET` and `FIXED_WIDTH` simultaneously.

*********************************************************************************************************************/

static ERR SET_Dimensions(extSurface *Self, LONG Value)
{
   SURFACEINFO *parent;

   if (gfxGetSurfaceInfo(Self->ParentID, &parent) IS ERR::Okay) {
      if (Value & DMF_Y) {
         if ((Value & DMF_HEIGHT) or (Value & DMF_Y_OFFSET)) {
            Self->Dimensions &= ~DMF_VERTICAL_FLAGS;
            Self->Dimensions |= Value & DMF_VERTICAL_FLAGS;
         }
      }
      else if ((Value & DMF_HEIGHT) and (Value & DMF_Y_OFFSET)) {
         Self->Dimensions &= ~DMF_VERTICAL_FLAGS;
         Self->Dimensions |= Value & DMF_VERTICAL_FLAGS;
      }

      if (Value & DMF_X) {
         if ((Value & DMF_WIDTH) or (Value & DMF_X_OFFSET)) {
            Self->Dimensions &= ~DMF_HORIZONTAL_FLAGS;
            Self->Dimensions |= Value & DMF_HORIZONTAL_FLAGS;
         }
      }
      else if ((Value & DMF_WIDTH) and (Value & DMF_X_OFFSET)) {
         Self->Dimensions &= ~DMF_HORIZONTAL_FLAGS;
         Self->Dimensions |= Value & DMF_HORIZONTAL_FLAGS;
      }

      struct acRedimension resize;
      if (Self->Dimensions & DMF_FIXED_X) resize.X = Self->X;
      else if (Self->Dimensions & DMF_SCALED_X) resize.X = (parent->Width * F2I(Self->XPercent));
      else if (Self->Dimensions & DMF_FIXED_X_OFFSET) resize.X = parent->Width - Self->XOffset;
      else if (Self->Dimensions & DMF_SCALED_X_OFFSET) resize.X = parent->Width - ((parent->Width * F2I(Self->XOffsetPercent)));
      else resize.X = 0;

      if (Self->Dimensions & DMF_FIXED_Y) resize.Y = Self->Y;
      else if (Self->Dimensions & DMF_SCALED_Y) resize.Y = (parent->Height * F2I(Self->YPercent));
      else if (Self->Dimensions & DMF_FIXED_Y_OFFSET) resize.Y = parent->Height - Self->YOffset;
      else if (Self->Dimensions & DMF_SCALED_Y_OFFSET) resize.Y = parent->Height - ((parent->Height * F2I(Self->YOffsetPercent)));
      else resize.Y = 0;

      if (Self->Dimensions & DMF_FIXED_WIDTH) resize.Width = Self->Width;
      else if (Self->Dimensions & DMF_SCALED_WIDTH) resize.Width = (parent->Width * F2I(Self->WidthPercent));
      else {
         if (Self->Dimensions & DMF_SCALED_X_OFFSET) resize.Width = parent->Width - (parent->Width * F2I(Self->XOffsetPercent));
         else resize.Width = parent->Width - Self->XOffset;

         if (Self->Dimensions & DMF_SCALED_X) resize.Width = resize.Width - ((parent->Width * F2I(Self->XPercent)));
         else resize.Width = resize.Width - Self->X;
      }

      if (Self->Dimensions & DMF_FIXED_HEIGHT) resize.Height = Self->Height;
      else if (Self->Dimensions & DMF_SCALED_HEIGHT) resize.Height = (parent->Height * F2I(Self->HeightPercent));
      else {
         if (Self->Dimensions & DMF_SCALED_Y_OFFSET) resize.Height = parent->Height - (parent->Height * F2I(Self->YOffsetPercent));
         else resize.Height = parent->Height - Self->YOffset;

         if (Self->Dimensions & DMF_SCALED_Y) resize.Height = resize.Height - ((parent->Height * F2I(Self->YPercent)));
         else resize.Height = resize.Height - Self->Y;
      }

      resize.Z = 0;
      resize.Depth  = 0;
      Action(AC_Redimension, Self, &resize);

      return ERR::Okay;
   }
   else return ERR::Search;
}

/*********************************************************************************************************************

-FIELD-
Height: Defines the height of a surface object.

The height of a surface object is manipulated through this field.  Alternatively, use the Resize() action to adjust the
Width and Height at the same time.  A client can set the Height as a fixed value by default, or as a scaled value in
conjunction with the `FD_SCALED` flag.  Scaled values are multiplied by the height of their parent container.

Setting the Height while a surface object is on display causes an immediate graphical update to reflect the change.
Any objects that are within the surface area will be re-drawn and resized as necessary.

If a value less than zero is passed to an initialised surface, the height will be 'turned off' - this is convenient
for pairing the Y and YOffset fields together for dynamic height adjustment.

*********************************************************************************************************************/

static ERR GET_Height(extSurface *Self, Variable *Value)
{
   if (Value->Type & FD_SCALED) {
      if (Self->Dimensions & DMF_SCALED_HEIGHT) {
         Value->Double = Self->HeightPercent;
         Value->Large  = F2I(Self->HeightPercent);
         return ERR::Okay;
      }
      else return ERR::Failed;
   }
   else {
      Value->Double  = Self->Height;
      Value->Large   = Self->Height;
      return ERR::Okay;
   }
}

static ERR SET_Height(extSurface *Self, Variable *Value)
{
   pf::Log log;
   DOUBLE value;

   if (Value->Type & FD_DOUBLE)      value = Value->Double;
   else if (Value->Type & FD_LARGE)  value = Value->Large;
   else if (Value->Type & FD_STRING) value = StrToFloat((CSTRING)Value->Pointer);
   else return log.warning(ERR::SetValueNotNumeric);

   if (value <= 0) {
      if (Self->initialised()) return ERR::InvalidDimension;
      else {
         Self->Dimensions &= ~(DMF_HEIGHT);
         return ERR::Okay;
      }
   }
   if (value > 0x7fffffff) value = 0x7fffffff;

   if (Value->Type & FD_SCALED) {
      if (Self->ParentID) {
         extSurface *parent;
         if (AccessObject(Self->ParentID, 500, &parent) IS ERR::Okay) {
            Self->HeightPercent = value;
            Self->Dimensions = (Self->Dimensions & ~DMF_FIXED_HEIGHT) | DMF_SCALED_HEIGHT;
            resize_layer(Self, Self->X, Self->Y, 0, parent->Height * value, 0, 0, 0, 0, 0);
            ReleaseObject(parent);
         }
         else return log.warning(ERR::AccessObject);
      }
      else {
         Self->HeightPercent = value;
         Self->Dimensions    = (Self->Dimensions & ~DMF_FIXED_HEIGHT) | DMF_SCALED_HEIGHT;
      }
   }
   else {
      if (value != Self->Height) resize_layer(Self, Self->X, Self->Y, 0, value, 0, 0, 0, 0, 0);

      Self->Dimensions = (Self->Dimensions & ~DMF_SCALED_HEIGHT) | DMF_FIXED_HEIGHT;

      // If the offset flags are used, adjust the vertical position

      if (Self->Dimensions & DMF_SCALED_Y_OFFSET) {
         Variable var;
         var.Type   = FD_DOUBLE|FD_SCALED;
         var.Double = Self->YOffsetPercent;
         SET_YOffset(Self, &var);
      }
      else if (Self->Dimensions & DMF_FIXED_Y_OFFSET) {
         Variable var(Self->YOffset);
         SET_YOffset(Self, &var);
      }
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
InsideHeight: Defines the amount of space between the vertical margins.

A client can determine the internal height of a surface object by reading the InsideHeight field.  The returned value is the
result of calculating this formula: `Height - TopMargin - BottomMargin`.

If the TopMargin and BottomMargin fields are not set, the returned value will be equal to the surface object's
height.

*********************************************************************************************************************/

static ERR GET_InsideHeight(extSurface *Self, LONG *Value)
{
   *Value = Self->Height - Self->TopMargin - Self->BottomMargin;
   return ERR::Okay;
}

static ERR SET_InsideHeight(extSurface *Self, LONG Value)
{
   LONG height = Value + Self->TopMargin + Self->BottomMargin;
   if (height < Self->MinHeight) height = Self->MinHeight;
   Self->setHeight(height);
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
InsideWidth: Defines the amount of space between the horizontal margins.

A client can determine the internal width of a surface object by reading the InsideWidth field.  The returned value is the
result of calculating this formula: `Width - LeftMargin - RightMargin`.

If the LeftMargin and RightMargin fields are not set, the returned value will be equal to the surface object's width.

*********************************************************************************************************************/

static ERR GET_InsideWidth(extSurface *Self, LONG *Value)
{
   *Value = Self->Width - Self->LeftMargin - Self->RightMargin;
   return ERR::Okay;
}

static ERR SET_InsideWidth(extSurface *Self, LONG Value)
{
   LONG width = Value + Self->LeftMargin + Self->RightMargin;
   if (width < Self->MinWidth) width = Self->MinWidth;
   Self->setWidth(width);
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
LeftLimit: Prevents a surface object from moving beyond a given point on the left-hand side.

A client can prevent a surface object from moving beyond a given point at the left-hand side of its container by setting
this field.  If for example you were to set the LeftLimit to 3, then any attempt to move the surface object into or
beyond the 3 units at the left of its container would fail.

Limits only apply to movement, as induced through the #Move() action.  This means it is possible to override limits by
setting the coordinate fields directly.

*********************************************************************************************************************/

static ERR SET_LeftLimit(extSurface *Self, LONG Value)
{
   Self->LeftLimit = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
LeftMargin: Manipulates the left margin of a surface object.

The Surface class supports margin settings, which are similar to the concept of margins on printed paper.  Margin
values have no significant meaning or effect on a surface object itself, but they are often used by other objects and
can be helpful in interface construction.  For instance, the Window template uses margins to indicate the space
available for placing graphics and other surface objects inside of it.

By default, all margins are set to zero when a new surface object is created.

-FIELD-
MaxHeight: Prevents the height of a surface object from exceeding a certain value.

A client can limit the maximum height of a surface object by setting this field.  Limiting the height affects resizing,
making it impossible to use the Resize() action to extend beyond the height you specify.

It is possible to circumvent the MaxHeight by setting the Height field directly.  Note that the MaxHeight value refers
to the inside-height of the surface area, thus the overall maximum height will include both the #TopMargin and
#BottomMargin values.

*********************************************************************************************************************/

static ERR SET_MaxHeight(extSurface *Self, LONG Value)
{
   Self->MaxHeight = Value;

   if ((!Self->ParentID) and (Self->DisplayID)) {
      struct gfxSizeHints hints = {
         .MinWidth  = -1,
         .MinHeight = -1,
         .MaxWidth  = Self->MaxWidth + Self->LeftMargin + Self->RightMargin,
         .MaxHeight = Self->MaxHeight + Self->TopMargin + Self->BottomMargin,
         .EnforceAspect = (Self->Flags & RNF::ASPECT_RATIO) != RNF::NIL
      };
      ActionMsg(MT_GfxSizeHints, Self->DisplayID, &hints);
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
MaxWidth: Prevents the width of a surface object from exceeding a certain value.

A client can limit the maximum width of a surface object by setting this field.  Limiting the width affects resizing, making
it impossible to use the Resize() action to extend beyond the width you specify.

It is possible to circumvent the MaxWidth by setting the Width field directly.  Note that the MaxWidth value refers to
the inside-width of the surface area, thus the overall maximum width will include both the LeftMargin and RightMargin
values.

*********************************************************************************************************************/

static ERR SET_MaxWidth(extSurface *Self, LONG Value)
{
   Self->MaxWidth = Value;

   if ((!Self->ParentID) and (Self->DisplayID)) {
      struct gfxSizeHints hints = {
         .MinWidth  = -1,
         .MinHeight = -1,
         .MaxWidth  = Self->MaxWidth + Self->LeftMargin + Self->RightMargin,
         .MaxHeight = Self->MaxHeight + Self->TopMargin + Self->BottomMargin,
         .EnforceAspect = (Self->Flags & RNF::ASPECT_RATIO) != RNF::NIL
      };
      ActionMsg(MT_GfxSizeHints, Self->DisplayID, &hints);
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
MinHeight: Prevents the height of a surface object from shrinking beyond a certain value.

A client can prevent the height of a surface object from shrinking too far by setting this field.  This feature specifically
affects resizing, making it impossible to use the Resize() action to shrink the height of a surface object to a value
less than the one you specify.

It is possible to circumvent the MinHeight by setting the #Height field directly.

*********************************************************************************************************************/

static ERR SET_MinHeight(extSurface *Self, LONG Value)
{
   Self->MinHeight = Value;
   if (Self->MinHeight < 1) Self->MinHeight = 1;

   if ((!Self->ParentID) and (Self->DisplayID)) {
      struct gfxSizeHints hints = {
         .MinWidth  = Self->MinWidth + Self->LeftMargin + Self->RightMargin,
         .MinHeight = Self->MinHeight + Self->TopMargin + Self->BottomMargin,
         .MaxWidth  = -1,
         .MaxHeight = -1,
         .EnforceAspect = (Self->Flags & RNF::ASPECT_RATIO) != RNF::NIL
      };
      ActionMsg(MT_GfxSizeHints, Self->DisplayID, &hints);
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
MinWidth: Prevents the width of a surface object from shrinking beyond a certain value.

A client can prevent the width of a surface object from shrinking too far by setting this field.  This feature specifically
affects resizing, making it impossible to use the Resize() action to shrink the width of a surface object to a value
less than the one you specify.

It is possible to circumvent the MinWidth by setting the Width field directly.

*********************************************************************************************************************/

static ERR SET_MinWidth(extSurface *Self, LONG Value)
{
   Self->MinWidth = Value;
   if (Self->MinWidth < 1) Self->MinWidth = 1;

   if ((!Self->ParentID) and (Self->DisplayID)) {
      struct gfxSizeHints hints = {
         .MinWidth  = Self->MinWidth + Self->LeftMargin + Self->RightMargin,
         .MinHeight = Self->MinHeight + Self->TopMargin + Self->BottomMargin,
         .MaxWidth  = -1,
         .MaxHeight = -1,
         .EnforceAspect = (Self->Flags & RNF::ASPECT_RATIO) != RNF::NIL
      };
      ActionMsg(MT_GfxSizeHints, Self->DisplayID, &hints);
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Right: Returns the right-most coordinate of a surface object (X + Width).

*********************************************************************************************************************/

static ERR GET_Right(extSurface *Self, LONG *Value)
{
   *Value = Self->X + Self->Width;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
RightLimit: Prevents a surface object from moving beyond a given point on the right-hand side.

A client can prevent a surface object from moving beyond a given point at the right-hand side of its container by setting
this field.  If for example you were to set the RightLimit to 8, then any attempt to move the surface object into or
beyond the 8 units at the right-hand side of its container would fail.

Limits only apply to movement, as induced through the #Move() action.  This means that limits can be over-ridden by
setting the coordinate fields directly (which can be useful in certain cases).

*********************************************************************************************************************/

static ERR SET_RightLimit(extSurface *Self, LONG Value)
{
   Self->RightLimit = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
RightMargin: Manipulates the right margin of a surface object.

The Surface class supports margin settings, which are similar to the concept of margins on printed paper.  Margin
values have no significant meaning or effect on a surface object itself, but they are often used by other objects and
can be helpful in interface construction.  For instance, the Window template uses margins to indicate the space
available for placing graphics and other surface objects inside of it.

By default, all margins are set to zero when a new surface object is created.

*********************************************************************************************************************/

static ERR SET_RightMargin(extSurface *Self, LONG Value)
{
   if (Value < 0) Self->RightMargin = -Value;
   else Self->RightMargin = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
TopLimit: Prevents a surface object from moving beyond a given point at the top of its container.

A client can prevent a surface object from moving beyond a given point at the top of its container by setting this field.
If for example you were to set the TopLimit to 10, then any attempt to move the surface object into or beyond the 10
units at the top of its container would fail.

Limits only apply to movement, as induced through the Move() action.  This means that limits can be over-ridden by
setting the coordinate fields directly (which can be useful in certain cases).

*********************************************************************************************************************/

static ERR SET_TopLimit(extSurface *Self, LONG Value)
{
   Self->TopLimit = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
TopMargin: Manipulates the top margin of a surface object.

The Surface class supports margin settings, which are similar to the concept of margins on printed paper.  Margin
values have no significant meaning or effect on a surface object itself, but they are often used by other objects and
can be helpful in interface construction.  For instance, the Window template uses margins to indicate the space
available for placing graphics and other surface objects inside of it.

By default, all margins are set to zero when a new surface object is created.

-FIELD-
VisibleHeight: The visible height of the surface area, relative to its parents.

To determine the visible area of a surface, read the VisibleX, VisibleY, VisibleWidth and VisibleHeight fields.

The 'visible area' is determined by the position of the surface relative to its parents.  For example, if the surface
is 100 pixels across and smallest parent is 50 pixels across, the number of pixels visible to the user must be 50
pixels or less, depending on the position of the surface.

If none of the surface area is visible then zero is returned.  The result is never negative.

*********************************************************************************************************************/

static ERR GET_VisibleHeight(extSurface *Self, LONG *Value)
{
   if (!Self->ParentID) {
      *Value = Self->Height;
      return ERR::Okay;
   }
   else {
      const std::lock_guard<std::recursive_mutex> lock(glSurfaceLock);

      WORD i;
      if ((i = find_surface_list(Self)) IS -1) return ERR::Search;

      auto clip = glSurfaces[i].area();
      restrict_region_to_parents(glSurfaces, i, clip, false);
      *Value = clip.height();
      return ERR::Okay;
   }
}

/*********************************************************************************************************************

-FIELD-
VisibleWidth: The visible width of the surface area, relative to its parents.

To determine the visible area of a surface, read the VisibleX, VisibleY, VisibleWidth and VisibleHeight fields.

The 'visible area' is determined by the position of the surface relative to its parents.  For example, if the surface
is 100 pixels across and smallest parent is 50 pixels across, the number of pixels visible to the user must be 50
pixels or less, depending on the position of the surface.

If none of the surface area is visible then zero is returned.  The result is never negative.

*********************************************************************************************************************/

static ERR GET_VisibleWidth(extSurface *Self, LONG *Value)
{
   if (!Self->ParentID) {
      *Value = Self->Height;
      return ERR::Okay;
   }
   else {
      const std::lock_guard<std::recursive_mutex> lock(glSurfaceLock);

      WORD i;
      if ((i = find_surface_list(Self)) IS -1) return ERR::Search;

      auto clip = glSurfaces[i].area();
      restrict_region_to_parents(glSurfaces, i, clip, false);
      *Value = clip.width();
      return ERR::Okay;
   }
}

/*********************************************************************************************************************

-FIELD-
VisibleX: The first visible X coordinate of the surface area, relative to its parents.

To determine the visible area of a surface, read the VisibleX, VisibleY, VisibleWidth and VisibleHeight fields.

The 'visible area' is determined by the position of the surface relative to its parents.  For example, if the surface
is 100 pixels across and smallest parent is 50 pixels across, the number of pixels visible to the user must be 50
pixels or less, depending on the position of the surface.

If none of the surface area is visible then zero is returned.  The result is never negative.

*********************************************************************************************************************/

static ERR GET_VisibleX(extSurface *Self, LONG *Value)
{
   if (!Self->ParentID) {
      *Value = Self->Height;
      return ERR::Okay;
   }
   else {
      const std::lock_guard<std::recursive_mutex> lock(glSurfaceLock);

      WORD i;
      if ((i = find_surface_list(Self)) IS -1) return ERR::Search;

      auto clip = glSurfaces[i].area();
      restrict_region_to_parents(glSurfaces, i, clip, false);

      *Value = clip.Left - glSurfaces[i].Left;
      return ERR::Okay;
   }
}

/*********************************************************************************************************************

-FIELD-
VisibleY: The first visible Y coordinate of the surface area, relative to its parents.

To determine the visible area of a surface, read the VisibleX, VisibleY, VisibleWidth and VisibleHeight fields.

The 'visible area' is determined by the position of the surface relative to its parents.  For example, if the surface
is 100 pixels across and smallest parent is 50 pixels across, the number of pixels visible to the user must be 50
pixels or less, depending on the position of the surface.

If none of the surface area is visible then zero is returned.  The result is never negative.

*********************************************************************************************************************/

static ERR GET_VisibleY(extSurface *Self, LONG *Value)
{
   if (!Self->ParentID) {
      *Value = Self->Height;
      return ERR::Okay;
   }
   else {
      const std::lock_guard<std::recursive_mutex> lock(glSurfaceLock);

      WORD i;
      if ((i = find_surface_list(Self)) IS -1) return ERR::Search;

      auto clip = glSurfaces[i].area();
      restrict_region_to_parents(glSurfaces, i, clip, false);
      *Value = clip.Top - glSurfaces[i].Top;
      return ERR::Okay;
   }
}

/*********************************************************************************************************************

-FIELD-
Width: Defines the width of a surface object.

The width of a surface object is manipulated through this field.  Alternatively, use the Resize() action to adjust the
Width and Height at the same time.  A client can set the Width as a fixed value by default, or as a scaled value in
conjunction with the `FD_SCALED` flag.  Scaled values are multiplied by the width of their parent container.

Setting the Width while a surface object is on display causes an immediate graphical update to reflect the change.  Any
objects that are within the surface area will be re-drawn and resized as necessary.

Width values of 0 or less are illegal, and will result in an `ERR::OutOfRange` error-code.

*********************************************************************************************************************/

static ERR GET_Width(extSurface *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) {
      if (Value->Type & FD_SCALED) {
         if (Self->Dimensions & DMF_SCALED_WIDTH) {
            Value->Double = Self->WidthPercent;
         }
         else return ERR::Failed;
      }
      else Value->Double = Self->Width;
   }
   else Value->Large = Self->Width;
   return ERR::Okay;
}

static ERR SET_Width(extSurface *Self, Variable *Value)
{
   pf::Log log;
   Variable var;
   extSurface *parent;
   DOUBLE value;

   if (Value->Type & FD_DOUBLE)      value = Value->Double;
   else if (Value->Type & FD_LARGE)  value = Value->Large;
   else if (Value->Type & FD_STRING) value = StrToFloat((CSTRING)Value->Pointer);
   else return log.warning(ERR::SetValueNotNumeric);

   if (value <= 0) {
      if (Self->initialised()) return ERR::InvalidDimension;
      else {
         Self->Dimensions &= ~(DMF_WIDTH);
         return ERR::Okay;
      }
   }
   if (value > 0x7fffffff) value = 0x7fffffff;

   if (Value->Type & FD_SCALED) {
      if (Self->ParentID) {
         if (AccessObject(Self->ParentID, 500, &parent) IS ERR::Okay) {
            Self->WidthPercent = value;
            Self->Dimensions   = (Self->Dimensions & ~DMF_FIXED_WIDTH) | DMF_SCALED_WIDTH;
            resize_layer(Self, Self->X, Self->Y, parent->Width * value, 0, 0, 0, 0, 0, 0);
            ReleaseObject(parent);
         }
         else return ERR::AccessObject;
      }
      else {
         Self->WidthPercent = value;
         Self->Dimensions   = (Self->Dimensions & ~DMF_FIXED_WIDTH) | DMF_SCALED_WIDTH;
      }
   }
   else {
      if (value != Self->Width) resize_layer(Self, Self->X, Self->Y, value, 0, 0, 0, 0, 0, 0);

      Self->Dimensions = (Self->Dimensions & ~DMF_SCALED_WIDTH) | DMF_FIXED_WIDTH;

      // If the offset flags are used, adjust the horizontal position
      if (Self->Dimensions & DMF_SCALED_X_OFFSET) {
         var.Type = FD_DOUBLE|FD_SCALED;
         var.Double = Self->XOffsetPercent;
         SET_XOffset(Self, &var);
      }
      else if (Self->Dimensions & DMF_FIXED_X_OFFSET) {
         var.Type = FD_DOUBLE;
         var.Double = Self->XOffset;
         SET_XOffset(Self, &var);
      }
   }
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
X: Determines the horizontal position of a surface object.

The horizontal position of a surface object can be set through this field.  You have the choice of setting a fixed
coordinate (the default) or a scaled coordinate if you use the `FD_SCALED` flag.

If you set the X while the surface object is on display, the position of the surface area will be updated
immediately.

*********************************************************************************************************************/

static ERR GET_XCoord(extSurface *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) {
      if (Value->Type & FD_SCALED) Value->Double = Self->XPercent;
      else Value->Double = Self->X;
   }
   else if (Value->Type & FD_LARGE) {
      if (Value->Type & FD_SCALED) Value->Large = F2I(Self->XPercent);
      else Value->Large = Self->X;
   }
   else {
      pf::Log log;
      return log.warning(ERR::FieldTypeMismatch);
   }
   return ERR::Okay;
}

static ERR SET_XCoord(extSurface *Self, Variable *Value)
{
   pf::Log log;
   extSurface *parent;
   DOUBLE value;

   if (Value->Type & FD_DOUBLE)      value = Value->Double;
   else if (Value->Type & FD_LARGE)  value = Value->Large;
   else if (Value->Type & FD_STRING) value = StrToFloat((CSTRING)Value->Pointer);
   else return log.warning(ERR::SetValueNotNumeric);

   if (Value->Type & FD_SCALED) {
      Self->Dimensions = (Self->Dimensions & ~DMF_FIXED_X) | DMF_SCALED_X;
      Self->XPercent   = value;
      if (Self->ParentID) {
         if (AccessObject(Self->ParentID, 500, &parent) IS ERR::Okay) {
            move_layer(Self, parent->Width * value, Self->Y);
            ReleaseObject(parent);
         }
         else return log.warning(ERR::AccessObject);
      }
   }
   else {
      Self->Dimensions = (Self->Dimensions & ~DMF_SCALED_X) | DMF_FIXED_X;
      move_layer(Self, value, Self->Y);

      // If our right-hand side is relative, we need to resize our surface to counteract the movement.

      if ((Self->ParentID) and (Self->Dimensions & (DMF_SCALED_X_OFFSET|DMF_FIXED_X_OFFSET))) {
         if (AccessObject(Self->ParentID, 1000, &parent) IS ERR::Okay) {
            resize_layer(Self, Self->X, Self->Y, parent->Width - Self->X - Self->XOffset, 0, 0, 0, 0, 0, 0);
            ReleaseObject(parent);
         }
         else return log.warning(ERR::AccessObject);
      }
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
XOffset: Determines the horizontal offset of a surface object.

The XOffset has a dual purpose depending on whether or not it is set in conjunction with the X or Width fields.

If set in conjunction with the X field, the width of the surface object will be from that X coordinate up to the width
of the container, minus the value given in the XOffset.  This means that the width of the surface object is dynamically
calculated in relation to the width of its container.

If the XOffset field is set in conjunction with a fixed or scaled width then the surface object will be positioned at
an X coordinate calculated from the formula `X = ContainerWidth - SurfaceWidth - XOffset`.
-END-

*********************************************************************************************************************/

static ERR GET_XOffset(extSurface *Self, Variable *Value)
{
   pf::Log log;
   Variable xoffset;
   extSurface *parent;
   DOUBLE value;

   if (Value->Type & FD_SCALED) {
      xoffset.Type = FD_DOUBLE;
      xoffset.Double = 0;
      if (GET_XOffset(Self, &xoffset) IS ERR::Okay) {
         value = xoffset.Double / Self->Width;
      }
      else value = 0;
   }
   else {
      if (Self->Dimensions & DMF_X_OFFSET) {
         value = Self->XOffset;
      }
      else if ((Self->Dimensions & DMF_WIDTH) and
               (Self->Dimensions & DMF_X) and
               (Self->ParentID)) {
         if (AccessObject(Self->ParentID, 1000, &parent) IS ERR::Okay) {
            value = parent->Width - Self->X - Self->Width;
            ReleaseObject(parent);
         }
         else return log.warning(ERR::AccessObject);
      }
      else value = 0;
   }

   if (Value->Type & FD_DOUBLE)     Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large  = value;
   else return log.warning(ERR::FieldTypeMismatch);

   return ERR::Okay;
}

static ERR SET_XOffset(extSurface *Self, Variable *Value)
{
   pf::Log log;
   extSurface *parent;
   DOUBLE value;

   if (Value->Type & FD_DOUBLE)      value = Value->Double;
   else if (Value->Type & FD_LARGE)  value = Value->Large;
   else if (Value->Type & FD_STRING) value = StrToFloat((CSTRING)Value->Pointer);
   else return log.warning(ERR::SetValueNotNumeric);

   if (value < 0) value = -value;

   if (Value->Type & FD_SCALED) {
      Self->Dimensions = (Self->Dimensions & ~DMF_FIXED_X_OFFSET) | DMF_SCALED_X_OFFSET;
      Self->XOffsetPercent = value;

      if (Self->ParentID) {
         if (AccessObject(Self->ParentID, 500, &parent) IS ERR::Okay) {
            Self->XOffset = parent->Width * F2I(Self->XOffsetPercent);
            if (!(Self->Dimensions & DMF_X)) Self->X = parent->Width - Self->XOffset - Self->Width;
            if (!(Self->Dimensions & DMF_WIDTH)) {
               resize_layer(Self, Self->X, Self->Y, parent->Width - Self->X - Self->XOffset, 0, 0, 0, 0, 0, 0);
            }
            ReleaseObject(parent);
         }
         else return log.warning(ERR::AccessObject);
      }
   }
   else {
      Self->Dimensions = (Self->Dimensions & ~DMF_SCALED_X_OFFSET) | DMF_FIXED_X_OFFSET;
      Self->XOffset = value;

      if ((Self->Dimensions & DMF_WIDTH) and (Self->ParentID)) {
         if (AccessObject(Self->ParentID, 1000, &parent) IS ERR::Okay) {
            move_layer(Self, parent->Width - Self->XOffset - Self->Width, Self->Y);
            ReleaseObject(parent);
         }
         else return log.warning(ERR::AccessObject);
      }
      else if ((Self->Dimensions & DMF_X) and (Self->ParentID)) {
         if (AccessObject(Self->ParentID, 1000, &parent) IS ERR::Okay) {
            resize_layer(Self, Self->X, Self->Y, parent->Width - Self->X - Self->XOffset, 0, 0, 0, 0, 0, 0);
            ReleaseObject(parent);
         }
         else return log.warning(ERR::AccessObject);
      }
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Y: Determines the vertical position of a surface object.

The vertical position of a surface object can be set through this field.  You have the choice of setting a fixed
coordinate (the default) or a scaled coordinate if you use the `FD_SCALED` flag.

If the value is changed while the surface is on display, its position will be updated immediately.

*********************************************************************************************************************/

static ERR GET_YCoord(extSurface *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) {
      if (Value->Type & FD_SCALED) Value->Double = Self->YPercent;
      else Value->Double = Self->Y;
   }
   else if (Value->Type & FD_LARGE) {
      if (Value->Type & FD_SCALED) Value->Large = F2I(Self->YPercent);
      else Value->Large = Self->Y;
   }
   else {
      pf::Log log;
      return log.warning(ERR::FieldTypeMismatch);
   }
   return ERR::Okay;
}

static ERR SET_YCoord(extSurface *Self, Variable *Value)
{
   pf::Log log;
   extSurface *parent;
   DOUBLE value;

   if (Value->Type & FD_DOUBLE)     value = Value->Double;
   else if (Value->Type & FD_LARGE) value = Value->Large;
   else if (Value->Type & FD_STRING) value = StrToFloat((CSTRING)Value->Pointer);
   else return log.warning(ERR::SetValueNotNumeric);

   if (Value->Type & FD_SCALED) {
      Self->Dimensions = (Self->Dimensions & ~DMF_FIXED_Y) | DMF_SCALED_Y;
      Self->YPercent = value;
      if (Self->ParentID) {
         if (AccessObject(Self->ParentID, 500, &parent) IS ERR::Okay) {
            move_layer(Self, Self->X, parent->Height * value);
            ReleaseObject(parent);
         }
         else return log.warning(ERR::AccessObject);
      }
   }
   else {
      Self->Dimensions = (Self->Dimensions & ~DMF_SCALED_Y) | DMF_FIXED_Y;
      move_layer(Self, Self->X, value);
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
YOffset: Determines the vertical offset of a surface object.

The YOffset has a dual purpose depending on whether or not it is set in conjunction with the Y or Height fields.

If set in conjunction with the Y field, the height of the surface object will be from that Y coordinate up to the
height of the container, minus the value given in the YOffset.  This means that the height of the surface object is
dynamically calculated in relation to the height of its container.

If the YOffset field is set in conjunction with a fixed or scaled height then the surface object will be positioned
at a Y coordinate calculated from the formula "Y = ContainerHeight - SurfaceHeight - YOffset".
-END-

*********************************************************************************************************************/

static ERR GET_YOffset(extSurface *Self, Variable *Value)
{
   pf::Log log;
   Variable yoffset;
   extSurface *parent;
   DOUBLE value;

   if (Value->Type & FD_SCALED) {
      yoffset.Type = FD_DOUBLE;
      yoffset.Double = 0;
      if (GET_YOffset(Self, &yoffset) IS ERR::Okay) {
         value = yoffset.Double / Self->Height;
      }
      else value = 0;
   }
   else {
      if (Self->Dimensions & DMF_Y_OFFSET) {
         value = Self->YOffset;
      }
      else if ((Self->Dimensions & DMF_HEIGHT) and (Self->Dimensions & DMF_Y) and (Self->ParentID)) {
         if (AccessObject(Self->ParentID, 1000, &parent) IS ERR::Okay) {
            value = parent->Height - Self->Y - Self->Height;
            ReleaseObject(parent);
         }
         else return log.warning(ERR::AccessObject);
      }
      else value = 0;
   }

   if (Value->Type & FD_DOUBLE)     Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large  = value;
   else return log.warning(ERR::FieldTypeMismatch);

   return ERR::Okay;
}

static ERR SET_YOffset(extSurface *Self, Variable *Value)
{
   pf::Log log;
   extSurface *parent;
   DOUBLE value;

   if (Value->Type & FD_DOUBLE)      value = Value->Double;
   else if (Value->Type & FD_LARGE)  value = Value->Large;
   else if (Value->Type & FD_STRING) value = StrToInt((CSTRING)Value->Pointer);
   else return log.warning(ERR::SetValueNotNumeric);

   if (value < 0) value = -value;

   if (Value->Type & FD_SCALED) {
      Self->Dimensions = (Self->Dimensions & ~DMF_FIXED_Y_OFFSET) | DMF_SCALED_Y_OFFSET;
      Self->YOffsetPercent = value;

      if (Self->ParentID) {
         if (AccessObject(Self->ParentID, 500, &parent) IS ERR::Okay) {
            Self->YOffset = parent->Height * F2I(Self->YOffsetPercent);
            if (!(Self->Dimensions & DMF_Y))Self->Y = parent->Height - Self->YOffset - Self->Height;
            if (!(Self->Dimensions & DMF_HEIGHT)) {
               resize_layer(Self, Self->X, Self->Y, 0, parent->Height - Self->Y - Self->YOffset, 0, 0, 0, 0, 0);
            }
            else move_layer(Self, Self->X, parent->Height - Self->YOffset - Self->Height);
            ReleaseObject(parent);
         }
         else return log.warning(ERR::AccessObject);
      }
   }
   else {
      Self->Dimensions = (Self->Dimensions & ~DMF_SCALED_Y_OFFSET) | DMF_FIXED_Y_OFFSET;
      Self->YOffset = value;

      if ((Self->Dimensions & DMF_HEIGHT) and (Self->ParentID)) {
         if (AccessObject(Self->ParentID, 1000, &parent) IS ERR::Okay) {
            if (!(Self->Dimensions & DMF_HEIGHT)) {
               resize_layer(Self, Self->X, Self->Y, 0, parent->Height - Self->Y - Self->YOffset, 0, 0, 0, 0, 0);
            }
            else move_layer(Self, Self->X, parent->Height - Self->YOffset - Self->Height);
            ReleaseObject(parent);
         }
         else return log.warning(ERR::AccessObject);
      }
      else if ((Self->Dimensions & DMF_Y) and (Self->ParentID)) {
         if (AccessObject(Self->ParentID, 1000, &parent) IS ERR::Okay) {
            resize_layer(Self, Self->X, Self->Y, 0, parent->Height - Self->Y - Self->YOffset, 0, 0, 0, 0, 0);
            ReleaseObject(parent);
         }
         else return log.warning(ERR::AccessObject);
      }
   }
   return ERR::Okay;
}

