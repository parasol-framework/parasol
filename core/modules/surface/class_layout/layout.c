/*****************************************************************************

-CLASS-
Layout: Manages the layout of objects that support graphics.

The Layout class is provided as an extension for a generic, standardised system of graphics management for all objects.
It extends the features of other existing classes only - i.e. it is not provided for high level, independent use.

The Layout class supports a large number of features and it is not expected that a class will make use of all them.
Certain fields and features exist for exotic and rare occasions only.  As a developer, do not not feel pressured to
support all of the extensions provided by the Layout class.

-END-

Each task is allocated one layout object.  The layout object acts as a layout management interface for all objects that
want to use the Parasol Surface GUI.

Other Layout classes can be used as a drop-in replacement for this default class
- e.g. the DocLayout class.

*****************************************************************************/

/*
#undef MSG
#undef FMSG
#undef STEP
#define MSG(...)  LogF(0,__VA_ARGS__)
#define FMSG(...) LogF(__VA_ARGS__)
#define STEP()    LogBack()
*/

static ERROR GET_Layout_X(objLayout *, struct Variable *);
static ERROR GET_Layout_Y(objLayout *, struct Variable *);
static ERROR GET_Layout_Width(objLayout *, struct Variable *);
static ERROR GET_Layout_Height(objLayout *, struct Variable *);

static ERROR SET_Layout_X(objLayout *, struct Variable *);
static ERROR SET_Layout_Y(objLayout *, struct Variable *);
static ERROR SET_Layout_XOffset(objLayout *, struct Variable *);
static ERROR SET_Layout_YOffset(objLayout *, struct Variable *);
static ERROR SET_Layout_Width(objLayout *, struct Variable *);
static ERROR SET_Layout_Height(objLayout *, struct Variable *);

static ERROR SET_Layout_Surface(objLayout *, OBJECTID);

static ERROR LAYOUT_Focus(objLayout *Self, APTR);
static ERROR LAYOUT_Free(objLayout *Self, APTR);
static ERROR LAYOUT_Hide(objLayout *Self, APTR);
static ERROR LAYOUT_Init(objLayout *Self, APTR);
static ERROR LAYOUT_LostFocus(objLayout *Self, APTR);
static ERROR LAYOUT_Move(objLayout *Self, struct acMove *);
static ERROR LAYOUT_MoveToBack(objLayout *Self, APTR);
static ERROR LAYOUT_MoveToFront(objLayout *Self, APTR);
static ERROR LAYOUT_MoveToPoint(objLayout *Self, struct acMoveToPoint *);
static ERROR LAYOUT_NewObject(objLayout *Self, APTR);
static ERROR LAYOUT_Redimension(objLayout *Self, struct acRedimension *);
static ERROR LAYOUT_Resize(objLayout *Self, struct acResize *);
static ERROR LAYOUT_Show(objLayout *Self, APTR);

static const struct FieldArray clLayoutFields[];
static const struct ActionArray clLayoutActions[];

//****************************************************************************
// Class methods.

//static ERROR LAYOUT_SetOpacity(objLayout *Self, struct mtSetSurfaceOpacity *);

//static const struct FunctionField argsSetOpacity[] = { { "Value", FD_DOUBLE }, { "Adjustment", FD_DOUBLE }, { NULL, NULL } };

static const struct MethodArray clLayoutMethods[] = {
//   { MT_DrwSetOpacity, LAYOUT_SetOpacity, "SetOpacity", argsSetOpacity, sizeof(struct mtSetLayoutOpacity) },
   { 0, NULL, NULL, NULL }
};

//****************************************************************************

static ERROR init_surface(objLayout *Self, OBJECTID SurfaceID)
{
   objSurface *surface;

   if ((Self->SurfaceID) AND (SurfaceID != Self->SurfaceID)) {
      LogErrorMsg("Attempt to change surface from #%d to #%d - switching surfaces is not allowed.", Self->SurfaceID, SurfaceID);
      return ERR_Failed;
   }

   LogF("~init_surface()","Surface: %d", SurfaceID);

   if (!AccessObject(SurfaceID, 3000, &surface)) {
      // In the case of documents, the bounds need to be taken from the parent and not the containing surface, as the
      // dimensions are typically huge and not actually reflective of the width and height of the document page.

      if (!StrMatch("rgnDocPage", GetName(surface))) {
         objSurface *view;
         Self->PageID = surface->ParentID;
         Self->Document = GetObjectPtr(GetOwnerID(Self->PageID));

         if ((!Self->Document) OR (Self->Document->ClassID != ID_DOCUMENT)) {
            LogErrorMsg("Expected a Document object to control this surface.");
            LogBack();
            return ERR_Failed;
         }

         if ((Self->PageID) AND (!AccessObject(Self->PageID, 3000, &view))) {
            SubscribeActionTags(view, AC_Redimension, TAGEND);

            Self->ParentSurface.X = view->X;
            Self->ParentSurface.Y = view->Y;
            Self->ParentSurface.Width  = view->Width;
            Self->ParentSurface.Height = view->Height;
            ReleaseObject(view);
         }
         else return ERR_AccessObject;
      }
      else {
         SubscribeActionTags(surface, AC_Redimension, TAGEND);
         Self->PageID = SurfaceID;

         Self->ParentSurface.X = surface->X;
         Self->ParentSurface.Y = surface->Y;
         Self->ParentSurface.Width  = surface->Width;
         Self->ParentSurface.Height = surface->Height;

      }

      if ((!Self->Document) AND (Self->DrawCallback.Type)) {
         struct drwAddCallback args = { &Self->DrawCallback };
         Action(MT_DrwAddCallback, surface, &args);
      }

      ReleaseObject(surface);

      LogBack();
      return ERR_Okay;
   }
   else return LogBackError(0, ERR_AccessObject);
}

/*****************************************************************************
** Internal: create_layout_class()
*/

static ERROR create_layout_class(void)
{
   return(CreateObject(ID_METACLASS, 0, &LayoutClass,
      FID_Name|TSTR,           "Layout",
      FID_ClassVersion|TFLOAT, 1.0,
      FID_Category|TLONG, CCF_GUI,
      FID_Actions|TPTR,   clLayoutActions,
      FID_Methods|TARRAY, clLayoutMethods,
      FID_Fields|TARRAY,  clLayoutFields,
      FID_Size|TLONG,     sizeof(objLayout),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND));
}

//****************************************************************************

static ERROR LAYOUT_ActionNotify(objLayout *Self, struct acActionNotify *Args)
{
   if (Args->ActionID IS AC_Free) {
      if ((Self->ResizeCallback.Type IS CALL_SCRIPT) AND (Self->ResizeCallback.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->ResizeCallback.Type = CALL_NONE;
      }
   }
   else if (Args->ActionID IS AC_Redimension) {
      struct acRedimension *resize = (struct acRedimension *)Args->Args;

      // PLEASE NOTE: If the layout is part of a document, then the page surface is monitored as that
      // contains the true width/height of the page as opposed to the containing surface.

      if (resize->Depth) Self->BitsPerPixel = resize->Depth;

      if ((resize->Width IS Self->ParentSurface.Width) AND (resize->Height IS Self->ParentSurface.Height)) return ERR_Okay;

      Self->ParentSurface.X = resize->X;
      Self->ParentSurface.Y = resize->Y;
      Self->ParentSurface.Width  = resize->Width;
      Self->ParentSurface.Height = resize->Height;

      struct Variable var;
      var.Type = FD_LARGE;
      GET_Layout_X(Self, &var); Self->BoundX = var.Large;
      GET_Layout_Y(Self, &var); Self->BoundY = var.Large;
      GET_Layout_Width(Self, &var);  Self->BoundWidth = var.Large;
      GET_Layout_Height(Self, &var); Self->BoundHeight = var.Large;

      if (Self->ResizeCallback.Type) {
          if (Self->ResizeCallback.Type IS CALL_STDC) {
            void (*routine)(OBJECTPTR);
            OBJECTPTR context = SetContext(Self->ResizeCallback.StdC.Context);
               routine = Self->ResizeCallback.StdC.Routine;
               routine(Self->Owner);
            SetContext(context);
         }
         else if (Self->ResizeCallback.Type IS CALL_SCRIPT) {
            OBJECTPTR script;
            if ((script = Self->ResizeCallback.Script.Script)) {
               const struct ScriptArg args[] = {
                  { "Owner", FD_OBJECTID, { .Long = Self->Owner ? Self->Owner->UniqueID : 0 } },
               };
               scCallback(script, Self->ResizeCallback.Script.ProcedureID, args, ARRAYSIZE(args));
            }
         }
      }
   }

   return ERR_Okay;
}


//****************************************************************************

static ERROR LAYOUT_Focus(objLayout *Self, APTR Void)
{
   return ERR_Okay;
}

//****************************************************************************

static ERROR LAYOUT_Free(objLayout *Self, APTR Void)
{
   objSurface *surface;

   if (Self->SurfaceID) {
      if (!AccessObject(Self->SurfaceID, 5000, &surface)) {
         UnsubscribeAction(surface, 0);
         //UnsubscribeFeed(surface, Self->Owner);
         if (Self->DrawCallback.Type IS CALL_STDC) {
            OBJECTPTR context = SetContext(Self->DrawCallback.StdC.Context);
            drwRemoveCallback(surface, NULL);
            SetContext(context);
         }
         else if (Self->DrawCallback.Type) {
            OBJECTPTR context = SetContext(Self->Owner);
            drwRemoveCallback(surface, NULL);
            SetContext(context);
         }
         ReleaseObject(surface);
      }
   }

   if ((Self->PageID) AND (Self->PageID != Self->SurfaceID)) {
      if (!AccessObject(Self->PageID, 5000, &surface)) {
         UnsubscribeAction(surface, 0);
         ReleaseObject(surface);
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR LAYOUT_Hide(objLayout *Self, APTR Void)
{
   if (Self->Visible IS TRUE) {
      Self->Visible = FALSE;
      if (Self->Head.Flags & NF_INITIALISED) {
         struct acDraw draw = { Self->BoundX, Self->BoundY, Self->BoundWidth, Self->BoundHeight };
         ActionMsg(AC_Draw, Self->SurfaceID, &draw);
      }
   }
   return ERR_Okay;
}

//****************************************************************************

static ERROR LAYOUT_Init(objLayout *Self, APTR Void)
{
   struct Variable var;
   objSurface *surface;

   Self->Owner = GetObjectPtr(GetOwner(Self));
   if (!Self->Owner) {
      LogErrorMsg("Failed to get owner address.");
      return ERR_Failed;
   }

   // Find the surface object that we are associated with

   if (!Self->SurfaceID) {
      OBJECTID owner_id = GetOwner(Self);
      while ((owner_id) AND (GetClassID(owner_id) != ID_SURFACE)) {
         owner_id = GetOwnerID(owner_id);
      }

      if (!owner_id) return PostError(ERR_UnsupportedOwner);
      else {
         init_surface(Self, owner_id);
         Self->SurfaceID = owner_id;
      }
   }
   else init_surface(Self, Self->SurfaceID);

   if ((Self->Dimensions & 0xffff)) {
      if ((Self->Dimensions & DMF_X) AND (Self->Dimensions & (DMF_FIXED_WIDTH|DMF_RELATIVE_WIDTH|DMF_FIXED_X_OFFSET|DMF_RELATIVE_X_OFFSET))) {
         Self->PresetX = TRUE;
         Self->PresetWidth = TRUE;
      }
      else if ((Self->Dimensions & DMF_X_OFFSET) AND (Self->Dimensions & (DMF_FIXED_WIDTH|DMF_RELATIVE_WIDTH|DMF_FIXED_X|DMF_RELATIVE_X))) {
         Self->PresetX = TRUE;
         Self->PresetWidth = TRUE;
      }
      else if (Self->Dimensions & DMF_WIDTH) {
         Self->PresetWidth = TRUE;
      }

      if ((Self->Dimensions & DMF_Y) AND (Self->Dimensions & (DMF_FIXED_HEIGHT|DMF_RELATIVE_HEIGHT|DMF_FIXED_Y_OFFSET|DMF_RELATIVE_Y_OFFSET))) {
         Self->PresetY = TRUE;
         Self->PresetHeight = TRUE;
      }
      else if ((Self->Dimensions & DMF_Y_OFFSET) AND (Self->Dimensions & (DMF_FIXED_HEIGHT|DMF_RELATIVE_HEIGHT|DMF_FIXED_Y|DMF_RELATIVE_Y))) {
         Self->PresetY = TRUE;
         Self->PresetHeight = TRUE;
      }
      else if (Self->Dimensions & DMF_HEIGHT) {
         Self->PresetHeight = TRUE;
      }
   }

   // If dimension settings are missing (e.g. if it is impossible to determine width, height or a coordinate), then we
   // set the missing fields to maximum possible values.

   var.Type = FD_DOUBLE;
   var.Double = 0;

   if (!(Self->Dimensions & (DMF_FIXED_X|DMF_RELATIVE_X|DMF_FIXED_X_OFFSET|DMF_RELATIVE_X_OFFSET))) {
      SET_Layout_X(Self, &var);
   }

   if (!(Self->Dimensions & (DMF_FIXED_Y|DMF_RELATIVE_Y|DMF_FIXED_Y_OFFSET|DMF_RELATIVE_Y_OFFSET))) {
      SET_Layout_Y(Self, &var);
   }

   if (!(Self->Dimensions & (DMF_FIXED_WIDTH|DMF_RELATIVE_WIDTH|DMF_FIXED_X_OFFSET|DMF_RELATIVE_X_OFFSET))) {
      SET_Layout_XOffset(Self, &var);
   }

   if (!(Self->Dimensions & (DMF_FIXED_HEIGHT|DMF_RELATIVE_HEIGHT|DMF_FIXED_Y_OFFSET|DMF_RELATIVE_Y_OFFSET))) {
      SET_Layout_YOffset(Self, &var);
   }

   if ((Self->PresetX) AND (Self->PresetY)) {
      // If the user has set fixed values on *both* axis, he can enable fixed placement mode, which means that the
      // cursor is completely ignored and the existing Bound* fields will be used without alteration.
      //
      // This also means that the left, right, top and bottom margins are all ignored.  Text will still be wrapped
      // around the boundaries.

      Self->Layout |= LAYOUT_IGNORE_CURSOR;
   }

   if (Self->Layout & LAYOUT_BACKGROUND) Self->Layout &= ~LAYOUT_EMBEDDED;
   else if ((Self->PresetX) AND (Self->PresetY)) Self->Layout &= ~LAYOUT_EMBEDDED;
   else if (Self->Align) Self->Layout &= ~LAYOUT_EMBEDDED;
   else Self->Layout |= LAYOUT_EMBEDDED;

   var.Type = FD_LARGE;
   GET_Layout_X(Self, &var); Self->BoundX = var.Large;
   GET_Layout_Y(Self, &var); Self->BoundY = var.Large;
   GET_Layout_Width(Self, &var); Self->BoundWidth = var.Large;
   GET_Layout_Height(Self, &var); Self->BoundHeight = var.Large;

   if ((!Self->Document) AND (Self->DrawCallback.Type)) {
      if ((Self->SurfaceID) AND (!AccessObject(Self->SurfaceID, 5000, &surface))) {
         struct drwAddCallback args = { &Self->DrawCallback };
         Action(MT_DrwAddCallback, surface, &args);
         ReleaseObject(surface);
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR LAYOUT_LostFocus(objLayout *Self, APTR Void)
{
   return ERR_Okay;
}

//****************************************************************************

static ERROR LAYOUT_Move(objLayout *Self, struct acMove *Void)
{
   return ERR_Okay;
}

//****************************************************************************

static ERROR LAYOUT_MoveToBack(objLayout *Self, APTR Void)
{
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToFront: Brings the image graphics to the front of the surface.
-END-
*****************************************************************************/

static ERROR LAYOUT_MoveToFront(objLayout *Self, APTR Void)
{
   objSurface *surface;

   if (Self->Document) return ERR_NoSupport;

   if (Self->DrawCallback.Type) {
      if ((Self->SurfaceID) AND (!AccessObject(Self->SurfaceID, 3000, &surface))) {
         struct drwAddCallback args = { &Self->DrawCallback };
         Action(MT_DrwAddCallback, surface, &args);
         ReleaseObject(surface);
         return ERR_Okay;
      }
      else return ERR_AccessObject;
   }
   else return ERR_FieldNotSet;
}

//****************************************************************************

static ERROR LAYOUT_MoveToPoint(objLayout *Self, struct acMoveToPoint *Args)
{
   return ERR_Okay;
}

//****************************************************************************

static ERROR LAYOUT_NewObject(objLayout *Self, APTR Void)
{
   Self->ParentSurface.Width = 1;
   Self->ParentSurface.Height = 1;
   Self->Visible = TRUE;
   return ERR_Okay;
}

//****************************************************************************

static ERROR LAYOUT_Redimension(objLayout *Self, struct acRedimension *Args)
{
   return ERR_Okay;
}

//****************************************************************************

static ERROR LAYOUT_Resize(objLayout *Self, struct acResize *Args)
{
   return ERR_Okay;
}

//****************************************************************************

static ERROR LAYOUT_Show(objLayout *Self, APTR Void)
{
   if (Self->Visible IS FALSE) {
      Self->Visible = TRUE;
      if (Self->Head.Flags & NF_INITIALISED) {
         struct acDraw draw = { Self->BoundX, Self->BoundY, Self->BoundWidth, Self->BoundHeight };
         ActionMsg(AC_Draw, Self->SurfaceID, &draw);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
AbsX: The absolute horizontal position of a graphic.

This field returns the absolute horizontal position of a graphic, relative to the display.

It is possible to set this field, but only after initialisation of the surface object has occurred.

*****************************************************************************/

static ERROR GET_Layout_AbsX(objLayout *Self, LONG *Value)
{
   LONG absx = 0;
   if (!drwGetSurfaceCoords(Self->SurfaceID, NULL, NULL, &absx, NULL, NULL, NULL)) {
      *Value = absx + Self->X;
      return ERR_Okay;
   }
   else return PostError(ERR_Failed);
}

static ERROR SET_Layout_AbsX(objLayout *Self, LONG Value)
{
   LONG absx = 0;
   if (!drwGetSurfaceCoords(Self->SurfaceID, NULL, NULL, &absx, NULL, NULL, NULL)) {
      Self->X = Value - absx;
      return ERR_Okay;
   }
   else return PostError(ERR_Failed);
}

/*****************************************************************************

-FIELD-
AbsY: The absolute vertical position of a graphic.

This field returns the absolute vertical position of a graphic, relative to the display.

It is possible to set this field, but only after initialisation of the surface object has occurred.

*****************************************************************************/

static ERROR GET_Layout_AbsY(objLayout *Self, LONG *Value)
{
   LONG absy = 0;
   if (!drwGetSurfaceCoords(Self->SurfaceID, NULL, NULL, NULL, &absy, NULL, NULL)) {
      *Value = absy + Self->Y;
      return ERR_Okay;
   }
   else return PostError(ERR_Failed);
}

static ERROR SET_Layout_AbsY(objLayout *Self, LONG Value)
{
   LONG absy = 0;
   if (!drwGetSurfaceCoords(Self->SurfaceID, NULL, NULL, NULL, &absy, NULL, NULL)) {
      Self->Y = Value - absy;
      return ERR_Okay;
   }
   else return PostError(ERR_Failed);
}

/*****************************************************************************

-FIELD-
Align: Defines the alignment of the graphic in relation to boundaries.

The position of a Layout object can be abstractly defined with alignment instructions by setting this field.  The
alignment feature takes precedence over values in coordinate fields such as #X and #Y.

*****************************************************************************/

static ERROR GET_Layout_Align(objLayout *Self, LONG *Value)
{
   *Value = Self->Align;
   return ERR_Okay;
}

static ERROR SET_Layout_Align(objLayout *Self, LONG Value)
{
   Self->Align = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Bottom: The bottom coordinate of the layout object (#BoundY + #BoundHeight).

*****************************************************************************/

static ERROR GET_Layout_Bottom(objLayout *Self, LONG *Value)
{
   *Value = Self->BoundY + Self->BoundHeight;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
BottomLimit: Prevents a graphic from moving beyond a fixed point at the bottom of its container.

You can prevent a surface object from moving beyond a given point at the bottom of its container by setting this field.
If for example you were to set the BottomLimit to 5, then any attempt to move the surface object into or beyond the 5
units at the bottom of its container would fail.

Limits only apply to movement, as induced through the Move() action.  This means that limits can be over-ridden by
setting the coordinate fields directly (which can be useful in certain cases).

*****************************************************************************/

static ERROR GET_Layout_BottomLimit(objLayout *Self, LONG *Value)
{
   *Value = Self->BottomLimit;
   return ERR_Okay;
}

static ERROR SET_Layout_BottomLimit(objLayout *Self, LONG Value)
{
   Self->BottomLimit = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
BottomMargin: Defines a white-space value for the bottom of the graphics page.

Margins declare an area of white-space to which no graphics should be drawn.  Margin values have no significant meaning
to the target object, but may be used for the management of graphics placed within its area. For instance, the Window
template uses margins to indicate the space available for placing graphics and other surface objects inside of it.

By default, all margins are initially set to zero.

*****************************************************************************/

static ERROR GET_Layout_BottomMargin(objLayout *Self, LONG *Value)
{
   *Value = Self->BottomMargin;
   return ERR_Okay;
}

static ERROR SET_Layout_BottomMargin(objLayout *Self, LONG Value)
{
   Self->BottomMargin = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Cursor: Defines the preferred cursor to use when the mouse pointer is positioned over the layout object.

This field defines the cursor image to use when the mouse pointer is positioned over the layout object.

For a list of valid values, please refer to the @Pointer.CursorID field.

*****************************************************************************/

static ERROR GET_Layout_Cursor(objLayout *Self, LONG *Value)
{
   *Value = Self->Cursor;
   return ERR_Okay;
}

static ERROR SET_Layout_Cursor(objLayout *Self, LONG Value)
{
   Self->Cursor = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Dimensions: Indicates the fields that are being used to manage the layout coordinates.

The dimension settings of a layout object can be read from this field.  The flags indicate the dimension fields that
are in use, and whether the values are fixed or relative.

It is strongly recommended that this field is never set manually, because the flags are automatically managed for the
client when setting fields such as #X and #Width.  If circumstances require manual configuration,
take care to ensure that the flags do not conflict.  For instance, FIXED_X and RELATIVE_X cannot be paired, nor could
FIXED_X, FIXED_X_OFFSET and FIXED_WIDTH simultaneously.

<types lookup="DMF"/>

*****************************************************************************/

static ERROR GET_Layout_Dimensions(objLayout *Self, LONG *Value)
{
   *Value = Self->Dimensions;
   return ERR_Okay;
}

static ERROR SET_Layout_Dimensions(objLayout *Self, LONG Value)
{
   Self->Dimensions = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
DisableDrawing: If TRUE, automatic redraws will be disabled.

Automated redrawing can be disabled by setting this field to TRUE.  Automated redrawing occurs when performing
real-time layout changes, such as moving the graphic and altering visibility.  If many changes are being made to the
layout, then this may have a negative effect on CPU performance and it will be desirable to temporarily switch off
automated redraws.

This feature is provided for the purpose of temporary graphics optimisation only, and the setting should be quickly
reversed once the layout changes are complete.

*****************************************************************************/

static ERROR GET_Layout_DisableDrawing(objLayout *Self, LONG *Value)
{
   *Value = Self->DisableDrawing;
   return ERR_Okay;
}

static ERROR SET_Layout_DisableDrawing(objLayout *Self, LONG Value)
{
   if (Value) Self->DisableDrawing = TRUE;
   else Self->DisableDrawing = FALSE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
DrawCallback: Allows direct drawing to the surface bitmap via a function callback.

To draw to the bitmap of a layout surface, set the DrawCallback field with a function reference.  The function
will be routed to the ~Surface module's ~Surface.AddCallback() function - please refer to
the documentation for this function for more details on the required function format and calling procedure.

*****************************************************************************/

static ERROR GET_Layout_DrawCallback(objLayout *Self, FUNCTION **Value)
{
   if (Self->DrawCallback.Type != CALL_NONE) {
      *Value = &Self->DrawCallback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Layout_DrawCallback(objLayout *Self, FUNCTION *Value)
{
   if (Self->Head.Flags & NF_INITIALISED) return PostError(ERR_Immutable);

   if (Value) Self->DrawCallback = *Value;
   else Self->DrawCallback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
EastGap: Defines a white-space gap to the right of the layout area when it is used in a document.

This field can be used when the layout object is targeting a @Document.  It allows for white-space (defined in
pixels) to be maintained on the right-side of the layout area.  The document will ensure that the white-space area is
kept free of content when positioning elements on the page.

-FIELD-
Gap: Defines a gap for all 4 sides surrounding the layout area if it is used in a document.

This field can be used when the layout object is targeting a @Document.  It allows for white-space (defined in
pixels) to surround the layout area.  The document will ensure that the white-space area is kept free of content when
positioning the elements on the page.

*****************************************************************************/

static ERROR GET_Layout_Gap(objLayout *Self, LONG *Value)
{
   *Value = (Self->TopMargin + Self->BottomMargin + Self->RightMargin + Self->LeftMargin)/4;
   return ERR_Okay;
}

static ERROR SET_Layout_Gap(objLayout *Self, LONG Value)
{
   if (Value >= 0) {
      Self->TopMargin = Self->BottomMargin = Self->LeftMargin = Self->RightMargin = Value;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*****************************************************************************

-FIELD-
GraphicHeight: Defines a graphic's height in pixels.

The height of a graphic can be read and manipulated through this field.  If you set the height to a specific value then
the picture will be resized to match the requested height.  The height is taken as a fixed value by default, but a
relative height may be defined by passing the value as a percentage.

Reading this field will always return a fixed height value.

*****************************************************************************/

static ERROR GET_Layout_GraphicHeight(objLayout *Self, struct Variable *Value)
{
   Value->Double = Self->GraphicHeight;
   Value->Large = Self->GraphicHeight;
   return ERR_Okay;
}

static ERROR SET_Layout_GraphicHeight(objLayout *Self, struct Variable *Value)
{
   if (Value->Type & FD_DOUBLE) {
      if (Value->Double <= 0) {
         LogErrorMsg("A GraphicHeight of %.2f is illegal.", Value->Double);
         return ERR_OutOfRange;
      }
      if (Value->Type & FD_PERCENTAGE) Self->GraphicRelHeight = Value->Double;
      else {
         Self->GraphicHeight = Value->Double;
         Self->GraphicRelHeight = 0;
      }
   }
   else if (Value->Type & FD_LARGE) {
      if (Value->Large <= 0) {
         LogErrorMsg("A GraphicHeight of " PF64() " is illegal.", Value->Large);
         return ERR_OutOfRange;
      }

      if (Value->Type & FD_PERCENTAGE) Self->GraphicRelHeight = Value->Large;
      else {
         Self->GraphicHeight = Value->Large;
         Self->GraphicRelHeight = 0;
      }
   }
   else return PostError(ERR_FieldTypeMismatch);

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
GraphicWidth: Defines a graphic's width in pixels.

The width of a graphic can be read and manipulated through this field.  If you set the width to a specific value then
the picture will be resized to match the requested width.  The width is taken as a fixed value by default, but a
relative width may be defined by passing the value as a percentage.

Reading this field will always return a fixed width value.

*****************************************************************************/

static ERROR GET_Layout_GraphicWidth(objLayout *Self, struct Variable *Value)
{
   Value->Double = Self->GraphicWidth;
   Value->Large = Self->GraphicWidth;
   return ERR_Okay;
}

static ERROR SET_Layout_GraphicWidth(objLayout *Self, struct Variable *Value)
{
   if (Value->Type & FD_DOUBLE) {
      if (Value->Double <= 0) {
         LogErrorMsg("A GraphicWidth of %.2f is illegal.", Value->Double);
         return ERR_OutOfRange;
      }
      if (Value->Type & FD_PERCENTAGE) Self->GraphicRelWidth = Value->Double / 100.0;
      else {
         Self->GraphicWidth = F2T(Value->Double);
         Self->GraphicRelWidth = 0;
      }
   }
   else if (Value->Type & FD_LARGE) {
      if (Value->Large <= 0) {
         LogErrorMsg("A GraphicWidth of " PF64() " is illegal.", Value->Large);
         return ERR_OutOfRange;
      }
      if (Value->Type & FD_PERCENTAGE) Self->GraphicRelWidth = (DOUBLE)Value->Large / 100.0;
      else {
         Self->GraphicWidth = Value->Large;
         Self->GraphicRelWidth = 0;
      }
   }
   else return PostError(ERR_FieldTypeMismatch);

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
GraphicX: Defines a fixed horizontal position for the graphic, relative to the assigned target area.

By default a graphic will be positioned at `(0,0)` relative to the target area defined by the #X and
#Y values.  You can move the horizontal position of the graphic within the target area by changing the
GraphicX value.  Any parts of the graphic that fall outside the boundaries of the target area will be clipped.

*****************************************************************************/

static ERROR GET_Layout_GraphicX(objLayout *Self, struct Variable *Value)
{
   Value->Double = Self->GraphicX;
   Value->Large = Self->GraphicX;
   return ERR_Okay;
}

static ERROR SET_Layout_GraphicX(objLayout *Self, struct Variable *Value)
{
   if (Value->Type & FD_DOUBLE) {
      if (Value->Type & FD_PERCENTAGE) {
         Self->GraphicRelX = Value->Double / 100.0;
      }
      else {
         Self->GraphicX = F2T(Value->Double);
         Self->GraphicRelX = 0;
      }
   }
   else if (Value->Type & FD_LARGE) {
      if (Value->Type & FD_PERCENTAGE) {
         Self->GraphicRelX = (DOUBLE)Value->Large / 100.0;
      }
      else {
         Self->GraphicX = Value->Large;
         Self->GraphicRelX = 0;
      }
   }
   else return PostError(ERR_FieldTypeMismatch);

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
GraphicY: Defines a fixed vertical position for the graphic, relative to the assigned target area.

By default a graphic will be positioned at `(0,0)` relative to the target area defined by the #X and #Y values.  You can
move the vertical position of the graphic within the target area by changing the GraphicY value.  Any parts of the
graphic that fall outside the boundaries of the target area will be clipped.

*****************************************************************************/

static ERROR GET_Layout_GraphicY(objLayout *Self, struct Variable *Value)
{
   Value->Double = Self->GraphicY;
   Value->Large = Self->GraphicY;
   return ERR_Okay;
}

static ERROR SET_Layout_GraphicY(objLayout *Self, struct Variable *Value)
{
   if (Value->Type & FD_DOUBLE) {
      if (Value->Type & FD_PERCENTAGE) Self->GraphicRelY = Value->Double / 100.0;
      else {
         Self->GraphicY = F2T(Value->Double);
         Self->GraphicRelY = 0;
      }
   }
   else if (Value->Type & FD_LARGE) {
      if (Value->Type & FD_PERCENTAGE) Self->GraphicRelY = (DOUBLE)Value->Large / 100.0;
      else {
         Self->GraphicY = Value->Large;
         Self->GraphicRelY = 0;
      }
   }
   else return PostError(ERR_FieldTypeMismatch);

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Height: The height of an image is specified here.

If an image graphic is to be limited to a specific surface area, set this field to limit the clipping height.  A
percentage may be specified if the FD_PERCENT flag is used when setting the field.

*****************************************************************************/

static ERROR GET_Layout_Height(objLayout *Self, struct Variable *Value)
{
   DOUBLE ycoord, value;

   if (Self->Dimensions & DMF_FIXED_HEIGHT) value = Self->Height;
   else if (Self->Dimensions & DMF_RELATIVE_HEIGHT) {
      value = (DOUBLE)Self->Height * (DOUBLE)Self->ParentSurface.Height * 0.01;
   }
   else if ((Self->Dimensions & DMF_Y) AND
            (Self->Dimensions & DMF_Y_OFFSET)) {
      if (Self->Dimensions & DMF_FIXED_Y) ycoord = Self->Y;
      else ycoord = (DOUBLE)Self->ParentSurface.Height * (DOUBLE)Self->Y * 0.01;
      if (Self->Dimensions & DMF_FIXED_Y_OFFSET) value = Self->ParentSurface.Height - ycoord - Self->YOffset;
      else value = (DOUBLE)Self->ParentSurface.Height - (DOUBLE)ycoord - ((DOUBLE)Self->ParentSurface.Height * (DOUBLE)Self->YOffset * 0.01);
   }
   else value = 0;

   if (Value->Type & FD_PERCENTAGE) value = ((DOUBLE)value * 100.0) / (DOUBLE)Self->ParentSurface.Height;

   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else return PostError(ERR_FieldTypeMismatch);

   return ERR_Okay;
}

static ERROR SET_Layout_Height(objLayout *Self, struct Variable *Value)
{
   if (Value->Type & FD_DOUBLE) {
      if (Value->Double < 0) {
         LogErrorMsg("A height of %.2f is illegal.", Value->Double);
         return ERR_OutOfRange;
      }
      Self->Height = Value->Double;
   }
   else if (Value->Type & FD_LARGE) {
      if (Value->Large < 0) {
         LogErrorMsg("A height of " PF64() " is illegal.", Value->Large);
         return ERR_OutOfRange;
      }
      Self->Height = Value->Large;
   }
   else return PostError(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) Self->Dimensions = (Self->Dimensions & ~DMF_FIXED_HEIGHT) | DMF_RELATIVE_HEIGHT;
   else Self->Dimensions = (Self->Dimensions & ~DMF_RELATIVE_HEIGHT) | DMF_FIXED_HEIGHT;

   if ((Self->Dimensions & (DMF_RELATIVE_Y|DMF_FIXED_Y)) AND (Self->Dimensions & (DMF_RELATIVE_Y_OFFSET|DMF_RELATIVE_Y))) Self->Dimensions &= ~(DMF_RELATIVE_Y_OFFSET|DMF_FIXED_Y_OFFSET);

   if (Self->Head.Flags & NF_INITIALISED) {
      struct Variable var;
      var.Type = FD_LARGE;
      GET_Layout_Y(Self, &var);
      Self->BoundY = var.Large;
      GET_Layout_Height(Self, &var);
      Self->BoundHeight = var.Large;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Hide: Hides the layout graphics if set to TRUE.

Set this field to TRUE to hide the layout graphics, or set to FALSE to ensure that it is displayed.
Post-initialisation, it is recommended that the #Show() and #Hide() actions are used to manage visibility.

*****************************************************************************/

static ERROR GET_Layout_Hide(objLayout *Self, LONG *Value)
{
   if (Self->Visible) *Value = FALSE;
   else *Value = TRUE;
   return ERR_Okay;
}

static ERROR SET_Layout_Hide(objLayout *Self, LONG Value)
{
   if (Value) {
      if (Self->Head.Flags & NF_INITIALISED) return acHide(Self);
      else Self->Visible = FALSE;
   }
   else {
      if (Self->Head.Flags & NF_INITIALISED) return acShow(Self);
      else Self->Visible = TRUE;
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
InsideHeight: Reflects the amount of space between the vertical margins.

The InsideHeight field determines the amount of space available for graphics containment. The returned value is the
result of applying the formula `Height - TopMargin - BottomMargin`.

If you have not set the #TopMargin and/or #BottomMargin fields, then the returned value will be equal to the current
#Height.

*****************************************************************************/

static ERROR GET_Layout_InsideHeight(objLayout *Self, LONG *Value)
{
   *Value = Self->Height - Self->TopMargin - Self->BottomMargin;
   return ERR_Okay;
}

static ERROR SET_Layout_InsideHeight(objLayout *Self, LONG Value)
{
   LONG height = Value + Self->TopMargin + Self->BottomMargin;
   if (height < Self->MinHeight) height = Self->MinHeight;
   SetLong(Self, FID_Height, height);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
InsideWidth: Reflects the amount of space between the horizontal margins.

The InsideWidth field determines the amount of space available for graphics containment. The returned value is the
result of applying the formula `Width - LeftMargin - RightMargin`

If you have not set the #LeftMargin and/or #RightMargin fields, then the returned value will be equal to the current
#Width.

*****************************************************************************/

static ERROR GET_Layout_InsideWidth(objLayout *Self, LONG *Value)
{
   *Value = Self->Width - Self->LeftMargin - Self->RightMargin;
   return ERR_Okay;
}

static ERROR SET_Layout_InsideWidth(objLayout *Self, LONG Value)
{
   LONG width = Value + Self->LeftMargin + Self->RightMargin;
   if (width < Self->MinWidth) width = Self->MinWidth;
   SetLong(Self, FID_Width, width);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Layout: Manages the layout of an image when used in a document.

If an image is used in a document, the LAYOUT flags can be used to manage the arrangement of text around the image.

*****************************************************************************/

static ERROR GET_Layout_Layout(objLayout *Self, LONG *Value)
{
   *Value = Self->Layout;
   return ERR_Okay;
}

static ERROR SET_Layout_Layout(objLayout *Self, LONG Value)
{
   Self->Layout = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
LeftLimit: Prevents a graphic from moving beyond a fixed point on the left-hand side.

You can prevent a graphic from moving beyond a given point at the left-hand side of its container by setting this
field.  If for example you were to set the LeftLimit to 3, then any attempt to move the surface object into or beyond
the 3 units at the left of its container would fail.

Limits only apply to movement, as induced through the #Move() action.  This means that you can override limits by
setting the coordinate fields directly (which can be useful in certain cases).

*****************************************************************************/

static ERROR GET_Layout_LeftLimit(objLayout *Self, LONG *Value)
{
   *Value = Self->LeftLimit;
   return ERR_Okay;
}

static ERROR SET_Layout_LeftLimit(objLayout *Self, LONG Value)
{
   Self->LeftLimit = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
LeftMargin: Defines a white-space value for the left side of the graphics page.

Margins declare an area of white-space to which no graphics should be drawn.  Margin values have no significant meaning
to the target object, but may be used for the management of graphics placed within its area. For instance, the Window
template uses margins to indicate the space available for placing graphics and other surface objects inside of it.

By default, all margins are initially set to zero.

*****************************************************************************/

static ERROR GET_Layout_LeftMargin(objLayout *Self, LONG *Value)
{
   *Value = Self->LeftMargin;
   return ERR_Okay;
}

static ERROR SET_Layout_LeftMargin(objLayout *Self, LONG Value)
{
   Self->LeftMargin = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
MaxHeight: Hints at the maximum allowable height for the layout.

The MaxHeight value is a hint that prevents the layout from being expanded beyond the maximum height indicated.
It specifically affects resizing, making it impossible to use the #Resize() or #Redimension() actions to extend beyond
any imposed limits.

If the MaxHeight value is less than the #MinHeight value, the results when resizing are undefined.

It is possible to circumvent the MaxHeight by setting the #Height field directly.

*****************************************************************************/

static ERROR GET_Layout_MaxHeight(objLayout *Self, LONG *Value)
{
   *Value = Self->MaxHeight;
   return ERR_Okay;
}

static ERROR SET_Layout_MaxHeight(objLayout *Self, LONG Value)
{
   if (Self->MaxHeight >= 0) {
      Self->MaxHeight = Value;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*****************************************************************************

-FIELD-
MaxWidth: Hints at the maximum allowable width for the layout.

The MaxWidth value is a hint that prevents the layout from being expanded beyond the maximum width indicated.
It specifically affects resizing, making it impossible to use the #Resize() or #Redimension() actions to extend beyond
any imposed limits.

If the MaxWidth value is less than the #MinWidth value, the results when resizing are undefined.

It is possible to circumvent the MaxWidth by setting the #Width field directly.

*****************************************************************************/

static ERROR GET_Layout_MaxWidth(objLayout *Self, LONG *Value)
{
   *Value = Self->MaxWidth;
   return ERR_Okay;
}

static ERROR SET_Layout_MaxWidth(objLayout *Self, LONG Value)
{
   if (Value >= 0) {
      Self->MaxWidth = Value;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*****************************************************************************

-FIELD-
MinHeight: Hints at the minimum allowable height for the layout.

The MinHeight is a hint that defines the minimum allowable height for the layout.  The minimum height will typically be
honoured at all times except when circumstances prevent this (such as the container not being large enough to contain
the layout).

If the MinHeight value is greater than the #MaxHeight value, the results when resizing are undefined.

It is possible to circumvent the MinHeight by setting the #Height field directly.

*****************************************************************************/

static ERROR GET_Layout_MinHeight(objLayout *Self, LONG *Value)
{
   *Value = Self->MinHeight;
   return ERR_Okay;
}

static ERROR SET_Layout_MinHeight(objLayout *Self, LONG Value)
{
   if (Value > 0) {
      Self->MinHeight = Value;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*****************************************************************************

-FIELD-
MinWidth: Hints at the minimum allowable width for the layout.

The MinWidth is a hint that defines the minimum allowable width for the layout.  The minimum width will typically be
honoured at all times except when circumstances prevent this (such as the container not being large enough to contain
the layout).

If the MinWidth value is greater than the #MaxWidth value, the results when resizing are undefined.

It is possible to circumvent the MinWidth by setting the #Width field directly.

*****************************************************************************/

static ERROR GET_Layout_MinWidth(objLayout *Self, LONG *Value)
{
   *Value = Self->MinWidth;
   return ERR_Okay;
}

static ERROR SET_Layout_MinWidth(objLayout *Self, LONG Value)
{
   if (Value > 0) {
      Self->MinWidth = Value;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*****************************************************************************

-FIELD-
NorthGap: Defines a white-space gap at the top of the layout area when it is used in a document.

This field can be used when the layout object is targeting a @Document.  It allows for white-space (defined in
pixels) to be maintained at the top of the layout area.  The document will ensure that the white-space area is
kept free of content when positioning elements on the page.

-FIELD-
ResizeCallback: Define a function reference here to receive callbacks when the layout is resized.

To receive notifications when a layout area is resized, set the ResizeCallback field with a function reference.  The
function must be in the format `ResizeCallback(OBJECTPTR Object)`

The Object parameter will be identical to the value in the Owner field.

Your function can read the new size of the layout area from the #BoundX, #BoundY, #BoundWidth and #BoundHeight fields.

*****************************************************************************/

static ERROR GET_Layout_ResizeCallback(objLayout *Self, FUNCTION **Value)
{
   if (Self->ResizeCallback.Type != CALL_NONE) {
      *Value = &Self->ResizeCallback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Layout_ResizeCallback(objLayout *Self, FUNCTION *Value)
{
   if (Value) Self->ResizeCallback = *Value;
   else Self->ResizeCallback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Right: Returns the right-most coordinate of the restricted drawing area.

This field indicates the right-most coordinate of the graphic's restricted drawing space. This is essentially the
opposite of the X field, and is calculated by adding the X and Width fields together.

*****************************************************************************/

static ERROR GET_Layout_Right(objLayout *Self, LONG *Value)
{
   *Value = Self->BoundX + Self->BoundWidth;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
RightLimit: Prevents the graphic from moving beyond a fixed point on the right-hand side.

You can prevent a graphic from moving beyond a given point at the right-hand side of its container by setting this
field.  If for example you were to set the RightLimit to 8, then any attempt to move the surface object into or beyond
the 8 units at the right-hand side of its container would fail.

Limits only apply to movement, as induced through the #Move() action.  This means that limits can be over-ridden by
setting the coordinate fields directly (which can be useful in certain cases).

*****************************************************************************/

static ERROR GET_Layout_RightLimit(objLayout *Self, LONG *Value)
{
   *Value = Self->RightLimit;
   return ERR_Okay;
}

static ERROR SET_Layout_RightLimit(objLayout *Self, LONG Value)
{
   Self->RightLimit = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
RightMargin: Defines a whitespace value for the right side of the graphics page.

Margins declare an area of whitespace to which no graphics should be drawn.  Margin values have no significant meaning
to the target object, but may be used for the management of graphics placed within its area. For instance, the Window
template uses margins to indicate the space available for placing graphics and other surface objects inside of it.

By default, all margins are initially set to zero.

*****************************************************************************/

static ERROR GET_Layout_RightMargin(objLayout *Self, LONG *Value)
{
   *Value = Self->RightMargin;
   return ERR_Okay;
}

static ERROR SET_Layout_RightMargin(objLayout *Self, LONG Value)
{
   Self->RightMargin = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
SouthGap: Defines a white-space gap at the bottom of the layout area when it is used in a document.

This field can be used when the layout object is targeting a @Document.  It allows for white-space (defined in
pixels) to be maintained at the bottom of the layout area.  The document will ensure that the white-space area is
kept free of content when positioning elements on the page.

-FIELD-
Surface: Defines the surface area for the image graphic.

When creating a new graphics object, it will need to be contained by a @Surface object.  Normally a graphics
object will detect the nearest surface by analysing its parents and automatically set the Surface field to the correct
object ID.  However in some cases it may be necessary to initialise the graphics object to a non-graphical container,
in which case the Surface field must be manually set to a valid @Surface object.

*****************************************************************************/

static ERROR GET_Layout_Surface(objLayout *Self, OBJECTID *Value)
{
   *Value = Self->SurfaceID;
   return ERR_Okay;
}

static ERROR SET_Layout_Surface(objLayout *Self, OBJECTID Value)
{
   if (Value IS Self->SurfaceID) return ERR_Okay;

   if (Self->Head.Flags & NF_INITIALISED) {
      LogErrorMsg("The target surface cannot be changed post-initialisation.");
      return ERR_Failed;
   }
   else Self->SurfaceID = Value;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TopLimit: Prevents a graphic from moving beyond a fixed point at the top of its container.

You can prevent a graphic from moving beyond a given point at the top of its container by setting this field.  If for
example you were to set the TopLimit to 10, then any attempt to move the surface object into or beyond the 10 units at
the top of its container would fail.

Limits only apply to movement, as induced through the Move() action.  This means that limits can be over-ridden by
setting the coordinate fields directly (which can be useful in certain cases).

*****************************************************************************/

static ERROR GET_Layout_TopLimit(objLayout *Self, LONG *Value)
{
   *Value = Self->TopLimit;
   return ERR_Okay;
}

static ERROR SET_Layout_TopLimit(objLayout *Self, LONG Value)
{
   Self->TopLimit = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TopMargin: Defines a whitespace value for the top of the graphics page.

Margins declare an area of whitespace to which no graphics should be drawn.  Margin values have no significant meaning
to the target object, but may be used for the management of graphics placed within its area. For instance, the Window
template uses margins to indicate the space available for placing graphics and other surface objects inside of it.

By default, all margins are initially set to zero.

*****************************************************************************/

static ERROR GET_Layout_TopMargin(objLayout *Self, LONG *Value)
{
   *Value = Self->TopMargin;
   return ERR_Okay;
}

static ERROR SET_Layout_TopMargin(objLayout *Self, LONG Value)
{
   Self->TopMargin = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Visible: If TRUE, the graphic is visible.

To know or change the visibility of a graphic, use this field. A TRUE value is returned if the object is visible and
FALSE is returned if the object is invisible.  Note that visibility is subject to the properties of the container that
the surface object resides in.  For example, if a surface object is visible but is contained within a surface object
that is invisible, the end result is that both objects are actually invisible.

Visibility is directly affected by the Hide and Show actions if you wish to change the visibility of a surface object.

*****************************************************************************/

static ERROR GET_Layout_Visible(objLayout *Self, LONG *Value)
{
   *Value = Self->Visible;
   return ERR_Okay;
}

static ERROR SET_Layout_Visible(objLayout *Self, LONG Value)
{
   if (Value IS FALSE) {
      if (Self->Head.Flags & NF_INITIALISED) return acHide(Self);
      else Self->Visible = FALSE;
   }
   else {
      if (Self->Head.Flags & NF_INITIALISED) return acShow(Self);
      else Self->Visible = TRUE;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
VisibleHeight: The visible height of the graphic, relative to its restricted drawing area.

To determine the visible area of a graphic, read the VisibleX, VisibleY, VisibleWidth and VisibleHeight fields.

The 'visible area' is determined by the position of the graphic relative to its restricted drawing area.  For example,
if the graphic is 100 pixels across but is restricted to an area 50 pixels across, the number of pixels visible to the
user must be 50 pixels or less, depending on the position of the graphic.

If none of the graphic is visible, then zero is returned.  The result is never negative.

*****************************************************************************/

static ERROR GET_Layout_VisibleHeight(objLayout *Self, LONG *Value)
{
   if (Self->Head.Flags & NF_INITIALISED) {
      if (Self->Y + Self->Height > Self->BoundY + Self->BoundHeight) *Value = Self->BoundHeight - Self->Y;
      else *Value = Self->Y + Self->Height;
      return ERR_Okay;
   }
   else return ERR_NotInitialised;
}

/*****************************************************************************

-FIELD-
VisibleWidth: The visible width of the graphic, relative to its restricted drawing area.

To determine the visible area of a graphic, read the VisibleX, VisibleY, VisibleWidth and VisibleHeight fields.

The 'visible area' is determined by the position of the graphic relative to its restricted drawing area.  For example,
if the graphic is 100 pixels across but is restricted to an area 50 pixels across, the number of pixels visible to the
user must be 50 pixels or less, depending on the position of the graphic.

If none of the graphic is visible, then zero is returned.  The result is never negative.

*****************************************************************************/

static ERROR GET_Layout_VisibleWidth(objLayout *Self, LONG *Value)
{
   if (Self->Head.Flags & NF_INITIALISED) {
      if (Self->X + Self->Width > Self->BoundX + Self->BoundWidth) *Value = Self->BoundWidth - Self->X;
      else *Value = Self->X + Self->Width;
      return ERR_Okay;
   }
   else return ERR_NotInitialised;
}

/*****************************************************************************

-FIELD-
VisibleX: The first visible X coordinate of the graphic, relative to its restricted drawing area.

To determine the visible area of a graphic, read the VisibleX, VisibleY, VisibleWidth and VisibleHeight fields.

The 'visible area' is determined by the position of the graphic relative to its restricted drawing area.  For example,
if the graphic is 100 pixels across but is restricted to an area 50 pixels across, the number of pixels visible to the
user must be 50 pixels or less, depending on the position of the graphic.

If none of the graphic is visible, then zero is returned.  The result is never negative.

*****************************************************************************/

static ERROR GET_Layout_VisibleX(objLayout *Self, LONG *Value)
{
   if (Self->Head.Flags & NF_INITIALISED) {
      if (Self->X < Self->BoundX) *Value = Self->BoundX;
      else *Value = Self->X;
      return ERR_Okay;
   }
   else return ERR_NotInitialised;
}

/*****************************************************************************

-FIELD-
VisibleY: The first visible Y coordinate of the graphic, relative to its restricted drawing area.

To determine the visible area of a graphic, read the VisibleX, VisibleY, VisibleWidth and VisibleHeight fields.

The 'visible area' is determined by the position of the graphic relative to its restricted drawing area.  For example,
if the graphic is 100 pixels across but is restricted to an area 50 pixels across, the number of pixels visible to the
user must be 50 pixels or less, depending on the position of the graphic.

If none of the graphic is visible, then zero is returned.  The result is never negative.

*****************************************************************************/

static ERROR GET_Layout_VisibleY(objLayout *Self, LONG *Value)
{
   if (Self->Head.Flags & NF_INITIALISED) {
      if (Self->Y < Self->BoundY) *Value = Self->BoundY;
      else *Value = Self->Y;
      return ERR_Okay;
   }
   else return ERR_NotInitialised;
}

/*****************************************************************************

-FIELD-
WestGap: Defines a white-space gap to the left of the layout area when it is used in a document.

This field can be used when the layout object is targeting a @Document.  It allows for white-space (defined in
pixels) to be maintained on the left-side of the layout area.  The document will ensure that the white-space area is
kept free of content when positioning elements on the page.

-FIELD-
Width: The width of an image's surface area is specified here.

If an image graphic is to be limited to a specific surface area, set this field to limit the clipping width.  A
percentage may be specified if the FD_PERCENT flag is used when setting the field.

*****************************************************************************/

static ERROR GET_Layout_Width(objLayout *Self, struct Variable *Value)
{
   DOUBLE xcoord, value;

   if (Self->Dimensions & DMF_FIXED_WIDTH) value = Self->Width;
   else if (Self->Dimensions & DMF_RELATIVE_WIDTH) {
      value = (DOUBLE)Self->Width * (DOUBLE)Self->ParentSurface.Width * 0.01;
   }
   else if ((Self->Dimensions & DMF_X) AND (Self->Dimensions & DMF_X_OFFSET)) {
      if (Self->Dimensions & DMF_FIXED_X) xcoord = Self->X;
      else xcoord = (DOUBLE)Self->ParentSurface.Width * (DOUBLE)Self->X * 0.01;

      if (Self->Dimensions & DMF_FIXED_X_OFFSET) value = Self->ParentSurface.Width - xcoord - Self->XOffset;
      else value = (DOUBLE)Self->ParentSurface.Width - (DOUBLE)xcoord - ((DOUBLE)Self->ParentSurface.Width * (DOUBLE)Self->XOffset * 0.01);
   }
   else value = 0;

   if (Value->Type & FD_PERCENTAGE) value = ((DOUBLE)value * 100.0) / (DOUBLE)Self->ParentSurface.Width;

   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else return PostError(ERR_FieldTypeMismatch);

   return ERR_Okay;
}

static ERROR SET_Layout_Width(objLayout *Self, struct Variable *Value)
{
   if (Value->Type & FD_DOUBLE) {
      if (Value->Double < 0) {
         LogErrorMsg("A width of %.2f is illegal.", Value->Double);
         return ERR_OutOfRange;
      }
      Self->Width = Value->Double;
   }
   else if (Value->Type & FD_LARGE) {
      if (Value->Large < 0) {
         LogErrorMsg("A width of " PF64() " is illegal.", Value->Large);
         return ERR_OutOfRange;
      }
      Self->Width = Value->Large;
   }
   else return PostError(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) Self->Dimensions = (Self->Dimensions & ~DMF_FIXED_WIDTH) | DMF_RELATIVE_WIDTH;
   else Self->Dimensions = (Self->Dimensions & ~DMF_RELATIVE_WIDTH) | DMF_FIXED_WIDTH;

   if ((Self->Dimensions & (DMF_RELATIVE_X|DMF_FIXED_X)) AND (Self->Dimensions & (DMF_RELATIVE_X_OFFSET|DMF_RELATIVE_X))) Self->Dimensions &= ~(DMF_RELATIVE_X_OFFSET|DMF_FIXED_X_OFFSET);

   if (Self->Head.Flags & NF_INITIALISED) {
      struct Variable var;
      FMSG("~","Resetting BoundX and BoundWidth");

      var.Type = FD_LARGE;
      GET_Layout_X(Self, &var);
      Self->BoundX = var.Large;

      GET_Layout_Width(Self, &var);
      Self->BoundWidth = var.Large;

      STEP();
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
X: Defines the horizontal position of the layout area.

The horizontal position of the layout area can be set to an absolute or relative coordinate by writing a value to the X
field.  To set a relative/percentage based value, use the FD_PERCENT flag or the value will be interpreted as
fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_Layout_X(objLayout *Self, struct Variable *Value)
{
   DOUBLE width, value;

   if (Self->Dimensions & DMF_FIXED_X) value = Self->X;
   else if (Self->Dimensions & DMF_RELATIVE_X) {
      value = (DOUBLE)Self->X * (DOUBLE)Self->ParentSurface.Width * 0.01;
   }
   else if ((Self->Dimensions & DMF_WIDTH) AND
            (Self->Dimensions & DMF_X_OFFSET)) {
      if (Self->Dimensions & DMF_FIXED_WIDTH) width = Self->Width;
      else width = (DOUBLE)Self->ParentSurface.Width * (DOUBLE)Self->Width * 0.01;
      if (Self->Dimensions & DMF_FIXED_X_OFFSET) value = Self->ParentSurface.Width - width - Self->XOffset;
      else value = (DOUBLE)Self->ParentSurface.Width - (DOUBLE)width - ((DOUBLE)Self->ParentSurface.Width * (DOUBLE)Self->XOffset * 0.01);
   }
   else value = 0;

   if (Value->Type & FD_PERCENTAGE) value = ((DOUBLE)value * 100.0) / (DOUBLE)Self->ParentSurface.Width;

   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else return PostError(ERR_FieldTypeMismatch);

   return ERR_Okay;
}

static ERROR SET_Layout_X(objLayout *Self, struct Variable *Value)
{
   if (Value->Type & FD_DOUBLE) Self->X = Value->Double;
   else if (Value->Type & FD_LARGE) Self->X = Value->Large;
   else return PostError(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) Self->Dimensions = (Self->Dimensions & ~DMF_FIXED_X) | DMF_RELATIVE_X;
   else Self->Dimensions = (Self->Dimensions & ~DMF_RELATIVE_X) | DMF_FIXED_X;

   if (Self->Head.Flags & NF_INITIALISED) {
      struct Variable var;

      FMSG("~","Resetting BoundX and BoundWidth");

      var.Type = FD_LARGE;
      GET_Layout_X(Self, &var);
      Self->BoundX = var.Large;

      GET_Layout_Width(Self, &var);
      Self->BoundWidth = var.Large;

      STEP();
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
XOffset: Defines the horizontal offset of an image.

The XOffset has a dual purpose depending on whether or not it is set in conjunction with an X coordinate or a Width
based field.

If set in conjunction with an X coordinate then the image will be drawn from that X coordinate up to the width of the
container, minus the value given in the XOffset.  This means that the width of the image is dynamically calculated in
relation to the width of the container.

If the XOffset field is set in conjunction with a fixed or relative width then the image will be drawn at an X
coordinate calculated from the formula `X = ContainerWidth - ImageWidth - XOffset`.

*****************************************************************************/

static ERROR GET_Layout_XOffset(objLayout *Self, struct Variable *Value)
{
   DOUBLE width, value;

   if (Self->Dimensions & DMF_FIXED_X_OFFSET) value = Self->XOffset;
   else if (Self->Dimensions & DMF_RELATIVE_X_OFFSET) {
      value = (DOUBLE)Self->XOffset * (DOUBLE)Self->ParentSurface.Width * 0.01;
   }
   else if ((Self->Dimensions & DMF_X) AND
            (Self->Dimensions & DMF_WIDTH)) {

      if (Self->Dimensions & DMF_FIXED_WIDTH) width = Self->Width;
      else width = (DOUBLE)Self->ParentSurface.Width * (DOUBLE)Self->Width * 0.01;

      if (Self->Dimensions & DMF_FIXED_X) value = Self->ParentSurface.Width - (Self->X + width);
      else value = Self->ParentSurface.Width - (((DOUBLE)Self->X * (DOUBLE)Self->ParentSurface.Width * 0.01) + width);
   }
   else value = 0;

   if (Value->Type & FD_PERCENTAGE) value = (value * 100.0) / (DOUBLE)Self->ParentSurface.Width;

   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else return PostError(ERR_FieldTypeMismatch);

   return ERR_Okay;
}

static ERROR SET_Layout_XOffset(objLayout *Self, struct Variable *Value)
{
   if (Value->Type & FD_DOUBLE) Self->XOffset = Value->Double;
   else if (Value->Type & FD_LARGE) Self->XOffset = Value->Large;
   else return PostError(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) Self->Dimensions = (Self->Dimensions & ~DMF_FIXED_X_OFFSET) | DMF_RELATIVE_X_OFFSET;
   else Self->Dimensions = (Self->Dimensions & ~DMF_RELATIVE_X_OFFSET) | DMF_FIXED_X_OFFSET;

   if (Self->Head.Flags & NF_INITIALISED) {
      struct Variable var;

      FMSG("~","Resetting BoundX and BoundWidth");

      var.Type = FD_LARGE;
      GET_Layout_X(Self, &var);
      Self->BoundX = var.Large;

      GET_Layout_Width(Self, &var);
      Self->BoundWidth = var.Large;

      STEP();
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Y: Defines the vertical position of the layout area.

The vertical position of the layout area can be set to an absolute or relative coordinate by writing a value to the Y
field.  To set a relative/percentage based value, use the FD_PERCENT flag or the value will be interpreted as
fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_Layout_Y(objLayout *Self, struct Variable *Value)
{
   DOUBLE value, height;

   if (Self->Dimensions & DMF_FIXED_Y) value = Self->Y;
   else if (Self->Dimensions & DMF_RELATIVE_Y) {
      value = (DOUBLE)Self->Y * (DOUBLE)Self->ParentSurface.Height * 0.01;
   }
   else if ((Self->Dimensions & DMF_HEIGHT) AND
            (Self->Dimensions & DMF_Y_OFFSET)) {
      if (Self->Dimensions & DMF_FIXED_HEIGHT) height = Self->Height;
      else height = (DOUBLE)Self->ParentSurface.Height * (DOUBLE)Self->Height * 0.01;

      if (Self->Dimensions & DMF_FIXED_Y_OFFSET) value = Self->ParentSurface.Height - height - Self->YOffset;
      else value = (DOUBLE)Self->ParentSurface.Height - height - ((DOUBLE)Self->ParentSurface.Height * (DOUBLE)Self->YOffset * 0.01);
   }
   else value = 0;

   if (Value->Type & FD_PERCENTAGE) value = (value * 100.0) / (DOUBLE)Self->ParentSurface.Height;
   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else return PostError(ERR_FieldTypeMismatch);

   return ERR_Okay;
}

static ERROR SET_Layout_Y(objLayout *Self, struct Variable *Value)
{
   if (Value->Type & FD_DOUBLE) Self->Y = Value->Double;
   else if (Value->Type & FD_LARGE) Self->Y = Value->Large;
   else return PostError(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) Self->Dimensions = (Self->Dimensions & ~DMF_FIXED_Y) | DMF_RELATIVE_Y;
   else Self->Dimensions = (Self->Dimensions & ~DMF_RELATIVE_Y) | DMF_FIXED_Y;

   if (Self->Head.Flags & NF_INITIALISED) {
      struct Variable var;
      var.Type = FD_LARGE;
      GET_Layout_Y(Self, &var);
      Self->BoundY = var.Large;
      GET_Layout_Height(Self, &var);
      Self->BoundHeight = var.Large;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
YOffset: Defines the vertical offset of an image.

The YOffset has a dual purpose depending on whether or not it is set in conjunction with a Y coordinate or a Height
based field.

If set in conjunction with a Y coordinate then the image will be drawn from that Y coordinate up to the height of the
container, minus the value given in the YOffset.  This means that the height of the Image is dynamically calculated in
relation to the height of the container.

If the YOffset field is set in conjunction with a fixed or relative height then the image will be drawn at a Y
coordinate calculated from the formula `Y = ContainerHeight - ImageHeight - YOffset`.
-END-

*****************************************************************************/

static ERROR GET_Layout_YOffset(objLayout *Self, struct Variable *Value)
{
   DOUBLE height;
   DOUBLE value = 0;
   if (Self->Dimensions & DMF_FIXED_Y_OFFSET) value = Self->YOffset;
   else if (Self->Dimensions & DMF_RELATIVE_Y_OFFSET) {
      value = (DOUBLE)Self->YOffset * (DOUBLE)Self->ParentSurface.Height * 0.01;
   }
   else if ((Self->Dimensions & DMF_Y) AND (Self->Dimensions & DMF_HEIGHT)) {
      if (Self->Dimensions & DMF_FIXED_HEIGHT) height = Self->Height;
      else height = (DOUBLE)Self->ParentSurface.Height * (DOUBLE)Self->Height * 0.01;

      if (Self->Dimensions & DMF_FIXED_Y) value = Self->ParentSurface.Height - (Self->Y + height);
      else value = (DOUBLE)Self->ParentSurface.Height - (((DOUBLE)Self->Y * (DOUBLE)Self->ParentSurface.Height * 0.01) + (DOUBLE)height);
   }
   else value = 0;

   if (Value->Type & FD_PERCENTAGE) value = ((DOUBLE)value * 100.0) / (DOUBLE)Self->ParentSurface.Height;

   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else return PostError(ERR_FieldTypeMismatch);

   return ERR_Okay;
}

static ERROR SET_Layout_YOffset(objLayout *Self, struct Variable *Value)
{
   if (Value->Type & FD_DOUBLE) Self->YOffset = Value->Double;
   else if (Value->Type & FD_LARGE) Self->YOffset = Value->Large;
   else return PostError(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) Self->Dimensions = (Self->Dimensions & ~DMF_FIXED_Y_OFFSET) | DMF_RELATIVE_Y_OFFSET;
   else Self->Dimensions = (Self->Dimensions & ~DMF_RELATIVE_Y_OFFSET) | DMF_FIXED_Y_OFFSET;

   if (Self->Head.Flags & NF_INITIALISED) {
      struct Variable var;
      var.Type = FD_LARGE;
      GET_Layout_Y(Self, &var);
      Self->BoundY = var.Large;
      GET_Layout_Height(Self, &var);
      Self->BoundHeight = var.Large;
   }

   return ERR_Okay;
}

//****************************************************************************

static const struct FieldDef clLayoutFlags[] = {
   { "Square",       LAYOUT_SQUARE },
   { "Wide",         LAYOUT_WIDE },
   { "Right",        LAYOUT_RIGHT },
   { "Left",         LAYOUT_LEFT },
   { "Background",   LAYOUT_BACKGROUND },
   { "Foreground",   LAYOUT_FOREGROUND },
   { "Tile",         LAYOUT_TILE },
   { "IgnoreCursor", LAYOUT_IGNORE_CURSOR },
   { "Lock",         LAYOUT_LOCK },
   { "Embedded",     LAYOUT_EMBEDDED },
   { "Tight",        LAYOUT_TIGHT },
   { NULL, 0 }
};

static const struct ActionArray clLayoutActions[] = {
   { AC_ActionNotify, LAYOUT_ActionNotify },
   { AC_Focus,       LAYOUT_Focus },
   { AC_Free,        LAYOUT_Free },
   { AC_Hide,        LAYOUT_Hide },
   { AC_Init,        LAYOUT_Init },
   { AC_LostFocus,   LAYOUT_LostFocus },
   { AC_Move,        LAYOUT_Move },
   { AC_MoveToBack,  LAYOUT_MoveToBack },
   { AC_MoveToFront, LAYOUT_MoveToFront },
   { AC_MoveToPoint, LAYOUT_MoveToPoint },
   { AC_NewObject,   LAYOUT_NewObject },
   { AC_Redimension, LAYOUT_Redimension },
   { AC_Resize,      LAYOUT_Resize },
   { AC_Show,        LAYOUT_Show },
   { 0, NULL }
};

// NOTE: All Layout fields are backed by virtual functions, so the order of the field descriptions is irrelevant
// for the class blueprint.

static const struct FieldArray clLayoutFields[] = {
#if 0
   { "BoundHeight",  FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Layout_BoundHeight,  SET_Layout_BoundHeight },
   { "BoundWidth",   FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Layout_BoundWidth,   SET_Layout_BoundWidth },
   { "BoundX",       FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Layout_BoundX,       SET_Layout_BoundX },
   { "BoundXOffset", FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Layout_BoundXOffset, SET_Layout_BoundXOffset },
   { "BoundY",       FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Layout_BoundY,       SET_Layout_BoundY },
   { "BoundYOffset", FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Layout_BoundYOffset, SET_Layout_BoundYOffset },
#endif
   { "AbsX",          FDF_LONG|FDF_RW,      0, GET_Layout_AbsX,            SET_Layout_AbsX },
   { "AbsY",          FDF_LONG|FDF_RW,      0, GET_Layout_AbsY,            SET_Layout_AbsY },
   { "Align",         FDF_LONGFLAGS|FDF_RW, (MAXINT)&clSurfaceAlign,       GET_Layout_Align, SET_Layout_Align },
   { "Bottom",        FDF_LONG|FDF_R,       0, GET_Layout_Bottom,          NULL },
   { "BottomLimit",   FDF_LONG|FDF_RW,      0, GET_Layout_BottomLimit,     SET_Layout_BottomLimit },
   { "BottomMargin",  FDF_LONG|FDF_RW,      0, GET_Layout_BottomMargin,    SET_Layout_BottomMargin },
   { "Cursor",        FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clSurfaceCursor, GET_Layout_Cursor,     SET_Layout_Cursor },
   { "Dimensions",    FDF_LONGFLAGS|FDF_RW, (MAXINT)&clSurfaceDimensions,  GET_Layout_Dimensions, SET_Layout_Dimensions },
   { "DisableDrawing",FDF_LONG|FDF_RW,      0, GET_Layout_DisableDrawing,  SET_Layout_DisableDrawing },
   { "DrawCallback",  FDF_FUNCTIONPTR|FDF_RI, 0, GET_Layout_DrawCallback,  SET_Layout_DrawCallback },
   { "EastGap",       FDF_LONG|FDF_RW,      0, GET_Layout_RightMargin,     SET_Layout_RightMargin },
   { "Layout",        FDF_LONGFLAGS|FDF_RW, (MAXINT)&clLayoutFlags,        GET_Layout_Layout,  SET_Layout_Layout },
   { "Gap",           FDF_LONG|FDF_RW,      0, GET_Layout_Gap,             SET_Layout_Gap },
   { "GraphicX",      FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0,    GET_Layout_GraphicX, SET_Layout_GraphicX },
   { "GraphicY",      FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0,    GET_Layout_GraphicY, SET_Layout_GraphicY },
   { "GraphicWidth",  FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0,    GET_Layout_GraphicWidth, SET_Layout_GraphicWidth },
   { "GraphicHeight", FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0,    GET_Layout_GraphicHeight, SET_Layout_GraphicHeight },
   { "Hide",          FDF_LONG|FDF_RI,      0, GET_Layout_Hide,            SET_Layout_Hide },
   { "InsideHeight",  FDF_LONG|FDF_RW,      0, GET_Layout_InsideHeight,    SET_Layout_InsideHeight },
   { "InsideWidth",   FDF_LONG|FDF_RW,      0, GET_Layout_InsideWidth,     SET_Layout_InsideWidth },
   { "LeftLimit",     FDF_LONG|FDF_RW,      0, GET_Layout_LeftLimit,      SET_Layout_LeftLimit },
   { "LeftMargin",    FDF_LONG|FDF_RW,      0, GET_Layout_LeftMargin,     SET_Layout_LeftMargin },
   { "MaxHeight",     FDF_LONG|FDF_RW,      0, GET_Layout_MaxHeight,      SET_Layout_MaxHeight },
   { "MaxWidth",      FDF_LONG|FDF_RW,      0, GET_Layout_MaxWidth,       SET_Layout_MaxWidth },
   { "MinHeight",     FDF_LONG|FDF_RW,      0, GET_Layout_MinHeight,      SET_Layout_MinHeight },
   { "MinWidth",      FDF_LONG|FDF_RW,      0, GET_Layout_MinWidth,       SET_Layout_MinWidth },
   { "NorthGap",      FDF_LONG|FDF_RW,      0, GET_Layout_TopMargin,      SET_Layout_TopMargin },
   { "ResizeCallback",FDF_FUNCTIONPTR|FDF_RI,   0, GET_Layout_ResizeCallback, SET_Layout_ResizeCallback },
   { "Right",         FDF_LONG|FDF_R,       0, GET_Layout_Right,          NULL },
   { "RightLimit",    FDF_LONG|FDF_RW,      0, GET_Layout_RightLimit,     SET_Layout_RightLimit },
   { "RightMargin",   FDF_LONG|FDF_RW,      0, GET_Layout_RightMargin,    SET_Layout_RightMargin },
   { "SouthGap",      FDF_LONG|FDF_RW,      0, GET_Layout_BottomMargin,   SET_Layout_BottomMargin },
   { "Surface",       FDF_OBJECTID|FDF_RI,  0, GET_Layout_Surface,        SET_Layout_Surface },
   { "TopMargin",     FDF_LONG|FDF_RW,      0, GET_Layout_TopMargin,      SET_Layout_TopMargin },
   { "TopLimit",      FDF_LONG|FDF_RW,      0, GET_Layout_TopLimit,       SET_Layout_TopLimit },
   { "Visible",       FDF_LONG|FDF_RW,      0, GET_Layout_Visible,        SET_Layout_Visible },
   { "VisibleHeight", FDF_LONG|FDF_R,       0, GET_Layout_VisibleHeight,  NULL },
   { "VisibleWidth",  FDF_LONG|FDF_R,       0, GET_Layout_VisibleWidth,   NULL },
   { "VisibleX",      FDF_LONG|FDF_R,       0, GET_Layout_VisibleX,       NULL },
   { "VisibleY",      FDF_LONG|FDF_R,       0, GET_Layout_VisibleY,       NULL },
   { "WestGap",       FDF_LONG|FDF_RW,      0, GET_Layout_LeftMargin,     SET_Layout_LeftMargin },
   { "Width",         FD_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, GET_Layout_Width, SET_Layout_Width },
   { "Height",        FD_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, GET_Layout_Height, SET_Layout_Height },
   { "X",             FD_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, GET_Layout_X,  SET_Layout_X },
   { "XOffset",       FD_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, GET_Layout_XOffset, SET_Layout_XOffset },
   { "Y",             FD_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, GET_Layout_Y,  SET_Layout_Y },
   { "YOffset",       FD_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, GET_Layout_YOffset, SET_Layout_YOffset },
   END_FIELD
};
