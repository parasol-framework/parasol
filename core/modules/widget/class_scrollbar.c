/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
ScrollBar: The ScrollBar class creates scrollbars for the user interface.

The ScrollBar class simplifies the creation and management of scrollbars as part of the user interface.

The ScrollBar class is closely related to the @Scroll class.  To configure the size of the scrollable page and
the viewable area, you need to communicate that information to the @Scroll class.  More information is
available in the @Scroll class documentation.

To create a new scrollbar, the client must specify the scrollbar's scrolling direction at a minimum.  The position of
the scrollbar will be calculated automatically based on this information.  To link a scrollbar to an object, such as a
text viewing area, you will need to extract the scroll object from the scrollbar and pass it to the scrollable object.
Here is an example:

<pre>
local vsb = obj.new("scrollbar", { direction="vertical" })
obj.new("text", { vscroll=vsb.scroll, ... })
</pre>

If a new scrollbar is created without being hooked into another object, it will send scroll messages to its parent
surface.

-END-

*****************************************************************************/

//#define DEBUG
#define PRV_SCROLLBAR
#include <parasol/modules/xml.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/widget.h>
#include "defs.h"

static OBJECTPTR clScrollbar = NULL;

//****************************************************************************

static ERROR SCROLLBAR_ActionNotify(objScrollbar *Self, struct acActionNotify *Args)
{
   return ERR_Okay;
}

//****************************************************************************

static ERROR SCROLLBAR_Free(objScrollbar *Self, APTR Void)
{
   if (Self->Scroll)   { acFree(Self->Scroll); Self->Scroll = NULL; }
   if (Self->RegionID) { acFreeID(Self->RegionID); Self->RegionID = 0; }
   if (Self->Script)   { acFree(Self->Script); Self->Script = NULL; }
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Hide: Removes the scrollbar from the display.
-END-
*****************************************************************************/

static ERROR SCROLLBAR_Hide(objScrollbar *Self, APTR Void)
{
   acHideID(Self->RegionID);
   return ERR_Okay;
}

//****************************************************************************

static ERROR SCROLLBAR_Init(objScrollbar *Self, APTR Void)
{
   objScrollbar *intersect;
   OBJECTID monitorid, objectid;
   LONG i;

   if (!Self->SurfaceID) { // Find parent surface
      OBJECTID owner_id = GetOwner(Self);
      while ((owner_id) AND (GetClassID(owner_id) != ID_SURFACE)) {
         owner_id = GetOwnerID(owner_id);
      }
      if (owner_id) Self->SurfaceID = owner_id;
      else return PostError(ERR_UnsupportedOwner);
   }

   if (!Self->Scroll->ViewID) Self->Scroll->ViewID = Self->SurfaceID;

   // Initialise the main scrollbar region

   objSurface *region;
   if (!AccessObject(Self->RegionID, 5000, &region)) {
      SetName(region, "rgnScrollbar");
      SetFields(region, FID_Parent|TLONG,    Self->SurfaceID,
                        FID_Opacity|TDOUBLE, Self->Opacity,
                        FID_Flags|TLONG,     region->Flags | RNF_STICKY | RNF_IGNORE_FOCUS,
                        TAGEND);

      if (!Self->Direction) {
         // Try to determine the scrollbar direction based on the dimensions that have already been set.

         BYTE count = 0;
         if (region->Dimensions & DMF_Y) count++;
         if (region->Dimensions & DMF_Y_OFFSET) count++;
         if (region->Dimensions & DMF_HEIGHT) count++;

         if (count > 1) {
            Self->Direction = SO_VERTICAL;
         }
         else {
            count = 0;
            if (region->Dimensions & DMF_X) count++;
            if (region->Dimensions & DMF_X_OFFSET) count++;
            if (region->Dimensions & DMF_WIDTH) count++;

            if (count > 1) Self->Direction = SO_HORIZONTAL;
         }
      }

      if (Self->Direction IS SO_HORIZONTAL) {
         if (!(region->Dimensions & DMF_X)) SetLong(region, FID_X, 0);

         if ((!(region->Dimensions & DMF_WIDTH)) AND (!(region->Dimensions & DMF_X_OFFSET))) {
            SetLong(region, FID_XOffset, 0);
         }

         if ((!(region->Dimensions & DMF_Y)) AND (!(region->Dimensions & DMF_Y_OFFSET))) {
            SetLong(region, FID_YOffset, 0);
         }

         if (!(region->Dimensions & DMF_HEIGHT)) SetLong(region, FID_Height, Self->Breadth);
      }

      if (Self->Direction IS SO_VERTICAL) {
         if (!(region->Dimensions & DMF_Y)) SetLong(region, FID_Y, 0);

         if ((!(region->Dimensions & DMF_HEIGHT)) AND (!(region->Dimensions & DMF_Y_OFFSET))) {
            SetLong(region, FID_YOffset, 0);
         }

         if ((!(region->Dimensions & DMF_X)) AND (!(region->Dimensions & DMF_X_OFFSET))) {
            SetLong(region, FID_XOffset, 0);
         }

         if (!(region->Dimensions & DMF_WIDTH)) SetLong(region, FID_Width, Self->Breadth);
      }

      if (!acInit(region)) {
         if (!Self->Direction) {
            if (region->Width > region->Height) Self->Direction = SO_HORIZONTAL;
            else Self->Direction = SO_VERTICAL;
         }

         if (Self->Direction IS SO_HORIZONTAL) drwApplyStyleGraphics(Self, Self->RegionID, "hscroll", "bar");
         else drwApplyStyleGraphics(Self, Self->RegionID, "vscroll", "bar");

         // Initialise the slider region

         objSurface *slider;
         if (!AccessObject(Self->SliderID, 5000, &slider)) {
            SetOwner(slider, region);

            CSTRING movement;
            if (Self->Direction IS SO_HORIZONTAL) movement = "horizontal";
            else movement = "vertical";

            SetFields(slider, FID_X|TLONG,           region->LeftMargin,
                              FID_Y|TLONG,           region->TopMargin,
                              FID_Width|TLONG,       region->Width - region->LeftMargin - region->RightMargin,
                              FID_Height|TLONG,      region->Height - region->TopMargin - region->BottomMargin,
                              FID_Drag|TLONG,        Self->SliderID,
                              FID_TopLimit|TLONG,    region->TopMargin,
                              FID_LeftLimit|TLONG,   region->LeftMargin,
                              FID_RightLimit|TLONG,  region->RightMargin,
                              FID_BottomLimit|TLONG, region->BottomMargin,
                              FID_Movement|TSTRING,  movement,
                              FID_Region|TLONG,      TRUE,
                              TAGEND);

            slider->Flags |= RNF_IGNORE_FOCUS;

            if (!acInit(slider)) {
               if (Self->Direction IS SO_HORIZONTAL) drwApplyStyleGraphics(Self, Self->SliderID, "hscroll", "slider");
               else drwApplyStyleGraphics(Self, Self->SliderID, "vscroll", "slider");
            }

            acShow(slider);

            ReleaseObject(slider);
         }
         else return ERR_AccessObject;
      }
      else {
         ReleaseObject(region);
         return ERR_Init;
      }

      ReleaseObject(region);
   }
   else return ERR_AccessObject;

   // If no intersecting scrollbar has been specified, check our parent surface to see if an opposed scrollbar exists.

   if ((!Self->IntersectID) AND (!(Self->Flags & SBF_NO_INTERSECT))) {
      MSG("Looking for an intersecting scrollbar in surface %d...", Self->SurfaceID);
      struct ChildEntry list[16];
      LONG count = ARRAYSIZE(list);
      if (!ListChildren(Self->SurfaceID, list, &count)) {
         for (i=0; i < count; i++) {
            if ((list[i].ClassID IS ID_SCROLLBAR) AND (list[i].ObjectID != Self->Head.UniqueID)) {
               if (!AccessObject(list[i].ObjectID, 5000, &intersect)) {
                  MSG("Found scrollbar #%d.", list[i].ObjectID);
                  if ((intersect->Direction IS SO_HORIZONTAL) AND (Self->Direction IS SO_VERTICAL)) Self->IntersectID = list[i].ObjectID;
                  else if ((intersect->Direction IS SO_VERTICAL) AND (Self->Direction IS SO_HORIZONTAL)) Self->IntersectID = list[i].ObjectID;
                  ReleaseObject(intersect);
               }
               break;
            }
         }
      }
      if (!Self->IntersectID) MSG("Unable to find an intersecting scrollbar.");
   }

   // Initialise the scroll management object

   LONG flags = 0;
   if (!(Self->Flags & SBF_CONSTANT)) flags |= SCF_AUTO_HIDE;
   if (Self->Flags & SBF_RELATIVE) flags |= SCF_RELATIVE;
   if (Self->Flags & SBF_SLIDER) flags |= SCF_SLIDER;
   if (Self->Direction IS SO_HORIZONTAL) flags |= SCF_HORIZONTAL;
   else flags |= SCF_VERTICAL;

   if (Self->IntersectID) {
      if (GetClassID(Self->IntersectID) IS ID_SCROLLBAR) {
         if (!AccessObject(Self->IntersectID, 5000, &intersect)) {
            Self->IntersectID = intersect->Scroll->Head.UniqueID;
            ReleaseObject(intersect);
         }
      }
   }

   // If the Scroll.Object field has not been set, set it to our parent surface

   if ((GetLong(Self->Scroll, FID_Object, &objectid) != ERR_Okay) OR (!objectid)) {
      SetLong(Self->Scroll, FID_Object, Self->SurfaceID);
      objectid = Self->SurfaceID;
   }

   // If the Scroll.Monitor field is not set, set it to the parent surface

   if ((GetLong(Self->Scroll, FID_Monitor, &monitorid) != ERR_Okay) OR (!monitorid)) {
      if (GetClassID(objectid) IS ID_SURFACE) SetLong(Self->Scroll, FID_Monitor, objectid);
      else SetLong(Self->Scroll, FID_Monitor, Self->SurfaceID);
   }

   if (!SetFields(Self->Scroll, FID_Slider|TLONG,    Self->SliderID,
                                FID_Flags|TLONG,     flags,
                                FID_Intersect|TLONG, Self->IntersectID,
                                TAGEND)) {
      char name[40];
      StrFormat(name, sizeof(name), "%s_scroll", GetName(Self));
      SetName(Self->Scroll, name);
      if (!acInit(Self->Scroll)) {

      }
   }

   // Create the buttons for the scrollbar

   if (Self->Direction IS SO_HORIZONTAL) drwApplyStyleGraphics(Self, Self->RegionID, "hscroll", "buttons");
   else drwApplyStyleGraphics(Self, Self->RegionID, "vscroll", "buttons");

   if ((Self->Flags & SBF_CONSTANT) AND (!(Self->Scroll->Flags & SCF_INVISIBLE))) acShow(Self);

   return ERR_Okay;
}

//****************************************************************************

static ERROR SCROLLBAR_NewObject(objScrollbar *Self, APTR Void)
{
   if (!NewLockedObject(ID_SURFACE, NF_INTEGRAL, NULL, &Self->RegionID)) {
      OBJECTPTR slider;
      if (!NewLockedObject(ID_SURFACE, 0, &slider, &Self->SliderID)) {
         SetName(slider, "_sbslider");
         if (!NewObject(ID_SCROLL, Self->Head.Flags & (~NF_INTEGRAL), &Self->Scroll)) {
            SetOwner(Self->Scroll, slider);
            ReleaseObject(slider);

            Self->Breadth = 16;
            Self->Opacity = 100;

            drwApplyStyleValues(Self, NULL);

            return ERR_Okay;
         }
         else {
            ReleaseObject(slider);
            return ERR_NewObject;
         }
      }
      else return ERR_NewObject;
   }
   else return ERR_NewObject;
}

/*****************************************************************************
-ACTION-
Redimension: Changes the size and position of the scrollbar.
-END-
*****************************************************************************/

static ERROR SCROLLBAR_Redimension(objScrollbar *Self, struct acRedimension *Args)
{
   return ActionMsg(AC_Redimension, Self->RegionID, Args);
}

/*****************************************************************************
-ACTION-
Resize: Alters the size of the scrollbar.
-END-
*****************************************************************************/

static ERROR SCROLLBAR_Resize(objScrollbar *Self, struct acResize *Args)
{
   return ActionMsg(AC_Resize, Self->RegionID, Args);
}

/*****************************************************************************
-ACTION-
Show: Puts the scrollbar on display.
-END-
*****************************************************************************/

static ERROR SCROLLBAR_Show(objScrollbar *Self, APTR Void)
{
   return acShow(Self->Scroll);
}

/*****************************************************************************

-FIELD-
Bottom: The bottom coordinate of the scrollbar.

The bottom coordinate of the scrollbar (calculated as Y + Height) is readable from this field.

*****************************************************************************/

static ERROR GET_Bottom(objScrollbar *Self, LONG *Value)
{
   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      GetLong(surface, FID_Bottom, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
Breadth: The breadth of the scrollbar is defined here.

The breadth of the scrollbar can be altered in this field.  The breadth of the scrollbar will either be its width (if
vertical) or height (if horizontal). The breadth of the scrollbar is user-definable and should not be changed without
good reason.

-FIELD-
Direction: The orientation of the scrollbar can be defined here.

Set the direction of the scrollbar to HORIZONTAL or VERTICAL depending on the orientation that you would like.  If the
direction is not defined on initialisation, the scrollbar will attempt to guess its orientation from its position in
the GUI.

-FIELD-
Flags: Optional flags may be defined here.

-FIELD-
Height: Defines the height of a scrollbar.

A scrollbar can be given a fixed or relative height by setting this field to the desired value.  To set a relative
height, use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Height(objScrollbar *Self, struct Variable *Value)
{
   LONG height;
   if (!drwGetSurfaceCoords(Self->RegionID, NULL, NULL, NULL, NULL, NULL, &height)) {
      if (Value->Type & FD_DOUBLE) Value->Double = height;
      else if (Value->Type & FD_LARGE) Value->Large = height;
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

static ERROR SET_Height(objScrollbar *Self, struct Variable *Value)
{
   if (((Value->Type & FD_DOUBLE) AND (!Value->Double)) OR
       ((Value->Type & FD_LARGE) AND (!Value->Large))) {
      return ERR_Okay;
   }

   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      SetVariable(surface, FID_Height, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
Hide: Hides the scrollbar when set to TRUE.

*****************************************************************************/

static ERROR SET_Hide(objScrollbar *Self, LONG Value)
{
   if (Value IS TRUE) {
      FMSG("~","Scrollbar invisible.");
      Self->Scroll->Flags |= SCF_INVISIBLE;
      if (Self->Head.Flags & NF_INITIALISED) acHide(Self);
      STEP();
   }
   else {
      MSG("Scrollbar now visible.");
      Self->Scroll->Flags &= ~SCF_INVISIBLE;
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Intersect: Refers to an intersecting scrollbar, if present.

An intersecting scrollbar can be set in this field (if not defined, the scrollbar will automatically look for an
intersecting scrollbar on initialisation).  The intersecting scrollbars must be at right angles to each other (one will
have a horizontal alignment, the other vertical).

-FIELD-
Opacity: The translucency level applied to the scrollbar.

The level of translucency applied to the scrollbar graphic is set here.  The value is expressed as a percentage, where
a setting of 100% will result in an opaque scrollbar.  The opacity setting is managed in the user's preferences - for
this reason we recommend that you avoid setting this field unless the scrollbar needs to be opaque.

-FIELD-
Region: If TRUE, the scrollbar is created as a region.

-FIELD-
Right: The right coordinate of the scrollbar (X + Width)

*****************************************************************************/

static ERROR GET_Right(objScrollbar *Self, LONG *Value)
{
   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      GetLong(surface, FID_Right, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
Scroll: Refers to the @Scroll object that provides the scrolling functionality.

The scrollbar's scrolling functionality is handed off to a @Scroll object which is referenced in this field.
Interaction with the Scroll object is permitted.

-FIELD-
Slider: Refers to the @Surface object that represents the scrollbar's slider.

The draggable slider inside the scrollbar is referenced via this field.  The slider is represented as a @Surface
object.  Interacting with the slider is not recommended as it is managed internally.

-FIELD-
Surface: The surface that will contain the scrollbar graphic.

The surface that will contain the scrollbar graphic is set here.  If this field is not set prior to initialisation, the
scrollbar will attempt to scan for the correct surface by analysing its parents until it finds a suitable candidate.

-FIELD-
Width: Defines the width of a scrollbar.

A scrollbar can be given a fixed or relative width by setting this field to the desired value.  To set a relative
width, use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Width(objScrollbar *Self, struct Variable *Value)
{
   LONG width;
   if (!drwGetSurfaceCoords(Self->RegionID, NULL, NULL, NULL, NULL, &width, NULL)) {
      if (Value->Type & FD_DOUBLE) Value->Double = width;
      else if (Value->Type & FD_LARGE) Value->Large = width;
      return ERR_Okay;
   }
   else return ERR_Failed;
}

static ERROR SET_Width(objScrollbar *Self, struct Variable *Value)
{
   if (((Value->Type & FD_DOUBLE) AND (!Value->Double)) OR ((Value->Type & FD_LARGE) AND (!Value->Large))) {
      return ERR_Okay;
   }

   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      SetVariable(surface, FID_Width, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
X: The horizontal position of a scrollbar.

The horizontal position of a scrollbar can be set to an absolute or relative coordinate by writing a value to the
X field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be
interpreted as fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_X(objScrollbar *Self, struct Variable *Value)
{
   LONG x;
   if (!drwGetSurfaceCoords(Self->RegionID, &x, NULL, NULL, NULL, NULL, NULL)) {
      if (Value->Type & FD_DOUBLE) Value->Double = x;
      else if (Value->Type & FD_LARGE) Value->Large = x;
      return ERR_Okay;
   }
   else return ERR_Failed;
}

static ERROR SET_X(objScrollbar *Self, struct Variable *Value)
{
   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      SetVariable(surface, FID_X, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
XOffset: The horizontal offset of a scrollbar.

The XOffset has a dual purpose depending on whether or not it is set in conjunction with an X coordinate or a Width
based field.

If set in conjunction with an X coordinate then the scrollbar will be drawn from that X coordinate up to the width of
the container, minus the value given in the XOffset.  This means that the width of the ScrollBar is dynamically
calculated in relation to the width of the container.

If the XOffset field is set in conjunction with a fixed or relative width then the scrollbar will be drawn at an X
coordinate calculated from the formula `X = ContainerWidth - ScrollBarWidth - XOffset`.

*****************************************************************************/

static ERROR GET_XOffset(objScrollbar *Self, struct Variable *Value)
{
   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      DOUBLE value;
      GetDouble(surface, FID_XOffset, &value);
      ReleaseObject(surface);

      if (Value->Type & FD_DOUBLE) Value->Double = value;
      else if (Value->Type & FD_LARGE) Value->Large = value;
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

static ERROR SET_XOffset(objScrollbar *Self, struct Variable *Value)
{
   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      SetVariable(surface, FID_XOffset, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
Y: The vertical position of a scrollbar.

The vertical position of a ScrollBar can be set to an absolute or relative coordinate by writing a value to the Y
field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be interpreted as
fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_Y(objScrollbar *Self, struct Variable *Value)
{
   LONG y;
   if (!drwGetSurfaceCoords(Self->RegionID, NULL, &y, NULL, NULL, NULL, NULL)) {
      if (Value->Type & FD_DOUBLE) Value->Double = y;
      else if (Value->Type & FD_LARGE) Value->Large = y;
      return ERR_Okay;
   }
   else return ERR_AccessObject;

}

static ERROR SET_Y(objScrollbar *Self, struct Variable *Value)
{
   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      SetVariable(surface, FID_Y, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
YOffset: The vertical offset of a scrollbar.

The YOffset has a dual purpose depending on whether or not it is set in conjunction with a Y coordinate or a Height
based field.

If set in conjunction with a Y coordinate then the scrollbar will be drawn from that Y coordinate up to the height of
the container, minus the value given in the YOffset.  This means that the height of the scrollbar is dynamically
calculated in relation to the height of the container.

If the YOffset field is set in conjunction with a fixed or relative height then the scrollbar will be drawn at a Y
coordinate calculated from the formula `Y = ContainerHeight - ScrollBarHeight - YOffset`.
-END-

*****************************************************************************/

static ERROR GET_YOffset(objScrollbar *Self, struct Variable *Value)
{
   OBJECTPTR surface;

   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      DOUBLE value;
      GetDouble(surface, FID_YOffset, &value);
      ReleaseObject(surface);

      if (Value->Type & FD_DOUBLE) Value->Double = value;
      else if (Value->Type & FD_LARGE) Value->Large = value;
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

static ERROR SET_YOffset(objScrollbar *Self, struct Variable *Value)
{
   OBJECTPTR surface;

   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      SetVariable(surface, FID_YOffset, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

//****************************************************************************

#include "class_scrollbar_def.c"

static const struct FieldArray clFields[] = {
   { "Opacity",   FDF_DOUBLE|FDF_RI,     0, NULL, NULL },
   { "Region",    FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, NULL },
   { "Surface",   FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, NULL },
   { "Slider",    FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, NULL },
   { "Flags",     FDF_LONGFLAGS|FDF_RW, (MAXINT)&clScrollbarFlags, NULL, NULL },
   { "Scroll",    FDF_INTEGRAL|FDF_R,   ID_SCROLL, NULL, NULL },
   { "Direction", FDF_LONG|FDF_LOOKUP|FDF_RI, (MAXINT)&clScrollbarDirection, NULL, NULL },
   { "Breadth",   FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "Intersect", FDF_OBJECTID|FDF_RI,  ID_SCROLLBAR, NULL, NULL },
   // Virtual fields
   { "Bottom",   FDF_VIRTUAL|FDF_LONG|FDF_R, 0, GET_Bottom,   NULL },
   { "Right",    FDF_VIRTUAL|FDF_LONG|FDF_R, 0, GET_Right,    NULL },
   { "Hide",     FDF_VIRTUAL|FDF_LONG|FDF_W, 0, NULL, SET_Hide },
   // Variable Fields
   { "Height",   FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Height,  SET_Height },
   { "Width",    FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Width,   SET_Width },
   { "X",        FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_X,       SET_X },
   { "XOffset",  FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_XOffset, SET_XOffset },
   { "Y",        FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Y,       SET_Y },
   { "YOffset",  FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_YOffset, SET_YOffset },
   END_FIELD
};

//****************************************************************************

ERROR init_scrollbar(void)
{
   return(CreateObject(ID_METACLASS, 0, &clScrollbar,
      FID_ClassVersion|TDOUBLE, VER_SCROLLBAR,
      FID_Name|TSTRING,   "ScrollBar",
      FID_Category|TLONG, CCF_GUI,
      FID_Flags|TLONG,    CLF_PROMOTE_INTEGRAL|CLF_PRIVATE_ONLY,
      FID_Actions|TPTR,   clScrollbarActions,
      FID_Fields|TARRAY,  clFields,
      FID_Size|TLONG,     sizeof(objScrollbar),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND));
}

void free_scrollbar(void)
{
   if (clScrollbar) { acFree(clScrollbar); clScrollbar = NULL; }
}