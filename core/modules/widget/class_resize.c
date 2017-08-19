/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Resize: The Resize class controls the resizing of rendered areas.

The Resize class is used for creating user-interactive resizing areas.  In most cases it is applied to the edges of
Surface objects so that the user can make simple adjustments to display areas.  When creating a new Resize object, you
can choose what edges of the surface border should be monitored for resizing, or alternatively you may pin-point the
resizing area through standard dimension specifications.  The following example demonstrates the use of both methods:

<pre>
local surface = obj.new('surface', { x=50, y=70, width=250, height=300 })
surface.new('resize', { border="left|right|top|bottom", bordersize=10 })
surface.new('resize', { xoffset=10, yoffset=10, width=20, height=20, direction="all" })
</pre>

The first Resize object monitors all four sides of the surface area, to a region not exceeding 10 units on either edge.
To do this, we simply specified the borders that are to be monitored through the Border field.  The second Resize
object monitors an area that is 20x20 units in size at an offset of 10 units from the bottom right edge.  The Direction
field has been set to a value of 'all', which means that the user can resize the surface area in any direction by
interacting with the Resize object.

When using Resize objects to manage the dimensions of surfaces, it is recommended that the MinWidth, MinHeight,
MaxWidth and MaxHeight fields are used to prevent excessive shrinkage or expansion.  These values must be set in the
Surface object that the resize functionality is being applied to.

-END-

*****************************************************************************/

#define PRV_RESIZE
#include <parasol/modules/display.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/widget.h>
#include "defs.h"

#define CLICK_HELD     1
#define CLICK_RELEASED 0

static OBJECTPTR clResize = NULL;

static ERROR SET_BorderSize(objResize *, LONG);

static LONG within_area(objResize *, LONG, LONG);

//****************************************************************************

static LONG get_cursor_type(objResize *Self)
{
   switch (Self->Direction) {
      case (MOVE_UP|MOVE_LEFT):    return PTR_SIZE_TOP_LEFT;
      case (MOVE_UP|MOVE_RIGHT):   return PTR_SIZE_TOP_RIGHT;
      case (MOVE_DOWN|MOVE_LEFT):  return PTR_SIZE_BOTTOM_LEFT;
      case (MOVE_DOWN|MOVE_RIGHT): return PTR_SIZE_BOTTOM_RIGHT;
      case MOVE_LEFT:              return PTR_SIZE_LEFT;
      case MOVE_RIGHT:             return PTR_SIZE_RIGHT;
      case MOVE_UP:                return PTR_SIZE_TOP;
      case MOVE_DOWN:              return PTR_SIZE_BOTTOM;
      default:                     return PTR_SIZING;
   }
}

//****************************************************************************
// Checks if the given coordinates fall within the given area.

static LONG within_area(objResize *Self, LONG AreaX, LONG AreaY)
{
   if (Self->Border) {
      SURFACEINFO *info;
      if (drwGetSurfaceInfo(Self->Layout->SurfaceID, &info) != ERR_Okay) return 0;

      LONG width = info->Width;
      LONG height = info->Height;

      if (Self->Border & EDGE_TOP_LEFT) {
         if ((AreaX >= 0) AND (AreaY >= 0) AND (AreaX < Self->BorderSize * 2) AND (AreaY < Self->BorderSize * 2)) {
            Self->Direction = MOVE_UP|MOVE_LEFT;
            return EDGE_TOP_LEFT;
         }
      }

      if (Self->Border & EDGE_TOP_RIGHT) {
         if ((AreaX >= width - (Self->BorderSize * 2)) AND (AreaY >= 0) AND (AreaX < width) AND (AreaY < Self->BorderSize * 2)) {
            Self->Direction = MOVE_UP|MOVE_RIGHT;
            return EDGE_TOP_RIGHT;
         }
      }

      if (Self->Border & EDGE_BOTTOM_LEFT) {
         if ((AreaX >= 0) AND (AreaY >= height - (Self->BorderSize*2)) AND (AreaX < Self->BorderSize*2) AND (AreaY < height)) {
            Self->Direction = MOVE_DOWN|MOVE_LEFT;
            return EDGE_BOTTOM_LEFT;
         }
      }

      if (Self->Border & EDGE_BOTTOM_RIGHT) {
         if ((AreaX >= width - (Self->BorderSize*2)) AND (AreaY >= height - (Self->BorderSize*2)) AND
             (AreaX < width) AND (AreaY < height)) {
            Self->Direction = MOVE_DOWN|MOVE_RIGHT;
            return EDGE_BOTTOM_RIGHT;
         }
      }

      if (Self->Border & EDGE_TOP) {
         if ((AreaX >= 0) AND (AreaY >= 0) AND (AreaX < width) AND (AreaY < Self->BorderSize)) {
            Self->Direction = MOVE_UP;
            return EDGE_TOP;
         }
      }

      if (Self->Border & EDGE_BOTTOM) {
         if ((AreaX >= 0) AND (AreaY >= height - Self->BorderSize) AND (AreaX < width) AND (AreaY < height)) {
            Self->Direction = MOVE_DOWN;
            return EDGE_BOTTOM;
         }
      }

      if (Self->Border & EDGE_LEFT) {
         if ((AreaX >= 0) AND (AreaY >= 0) AND (AreaX < Self->BorderSize) AND (AreaY < height)) {
            Self->Direction = MOVE_LEFT;
            return EDGE_LEFT;
         }
      }

      if (Self->Border & EDGE_RIGHT) {
         if ((AreaX >= width - Self->BorderSize) AND (AreaY >= 0) AND (AreaX < width) AND (AreaY < height)) {
            Self->Direction = MOVE_RIGHT;
            return EDGE_RIGHT;
         }
      }

      return 0;
   }
   else {
      if (AreaX < Self->Layout->BoundX) return 0;
      else if (AreaY < Self->Layout->BoundY) return 0;
      else if (AreaX >= Self->Layout->BoundX + Self->Layout->BoundWidth) return 0;
      else if (AreaY >= Self->Layout->BoundY + Self->Layout->BoundHeight) return 0;
      else return -1;
   }
}

//****************************************************************************

static ERROR RESIZE_DataFeed(objResize *Self, struct acDataFeed *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   // The display will send us DATA_INPUT_READY messages when our target surface has input messages in its log.  We
   // consume these messages using GetInputMsg().

   if (Args->DataType IS DATA_INPUT_READY) {
      struct InputMsg *input, *scan;
      ERROR inputerror;

      while (!gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &input)) {
         if (Self->State IS CLICK_HELD) {
            if (input->Flags & (JTYPE_ANCHORED|JTYPE_MOVEMENT)) {
               struct acRedimension redim;
               objSurface *object;
               ERROR error;
               LONG over_x, over_y;

               if (input->Flags & JTYPE_ANCHORED) {
                  // Note: Anchoring is typically not available in hosted environments, so this feature goes unused.

                  // Consume all anchor events up to the latest one.  This is important as X and Y movement can often
                  // be split into two separate messages (JET_ABS_X and JET_ABS_Y).

                  over_x = input->X; // NB: Misnomer - over_x/y will actually reflect the change in position and not a coordinate.
                  over_y = input->Y;
                  while (!(inputerror = gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &scan))) {
                     if (scan->Flags & JTYPE_ANCHORED) {
                        input = scan;
                        if (input->Type IS JET_ABS_X) over_x += input->X;
                        else if (input->Type IS JET_ABS_Y) over_y += input->Y;
                     }
                     else break;
                  }
               }
               else {
                  // Consume all movement events up to the latest one (just skip to the most recent message)
                  while (!(inputerror = gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &scan))) {
                     if (scan->Flags & JTYPE_MOVEMENT) input = scan;
                     else break;
                  }

                  over_x = input->AbsX - Self->OriginalAbsX; // NB: Using input->X won't work because X is relative to the window surface (which matters when resizing from the left side)
                  over_y = input->AbsY - Self->OriginalAbsY;
               }

               if (!(error = AccessObject(Self->ObjectID, 4000, &object))) {
                  // Send the Redimension message to the target object

                  LONG maxwidth  = object->MaxWidth  + object->LeftMargin + object->RightMargin;
                  LONG maxheight = object->MaxHeight + object->TopMargin + object->BottomMargin;
                  LONG minwidth  = object->MinWidth  + object->LeftMargin + object->RightMargin;
                  LONG minheight = object->MinHeight + object->TopMargin + object->BottomMargin;

                  if (Self->Direction & MOVE_RIGHT) {
                     // Resizing the right edge of the surface (width is adjusted)

                     redim.X = Self->OriginalX;
                     if (Self->prvAnchored) redim.Width = object->Width + over_x;
                     else redim.Width = over_x + (Self->OriginalWidth - Self->prvAnchorX);

                     // Restrict the width to the size of the parent's width

                     LONG px, pwidth;
                     if (!drwGetVisibleArea(object->ParentID, &px, NULL, NULL, NULL, &pwidth, NULL)) {
                        if (object->X + redim.Width >= px + pwidth) redim.Width = px + pwidth - object->X;
                     }
                  }
                  else if (Self->Direction & MOVE_LEFT) {
                     // Movement comes from the left edge of the surface

                     //if (absx >= Self->OriginalX + Self->OriginalWidth) continue;

                     if (Self->prvAnchored) {
                        redim.X = object->X + over_x;
                        redim.Width  = object->Width - over_x;
                     }
                     else {
                        redim.X  = Self->OriginalX + over_x;
                        redim.Width = Self->OriginalWidth - over_x;
                     }

                     // Restrict the left edge to the parent's visible left edge

                     LONG px;
                     if (!drwGetVisibleArea(object->ParentID, &px, NULL, NULL, NULL, NULL, NULL)) {
                        if (redim.X < px) {
                           redim.Width -= px - redim.X;
                           redim.X = px;
                        }
                     }

                     // Check minwidth/maxwidth settings due to 'reverse resizing'

                     if (redim.Width > maxwidth) {
                        redim.X = Self->OriginalX + Self->OriginalWidth - maxwidth;
                        redim.Width = maxwidth;
                     }
                     else if (redim.Width < minwidth) {
                        redim.X = Self->OriginalX + Self->OriginalWidth - minwidth;
                        redim.Width  = minwidth;
                     }
                  }
                  else {
                     redim.X = Self->OriginalX;
                     redim.Width  = 0;
                  }

                  if (Self->Direction & MOVE_DOWN) {
                     redim.Y = Self->OriginalY;
                     if (Self->prvAnchored) redim.Height = object->Height + over_y;
                     else redim.Height = input->AbsY - Self->OriginalAbsY;

                     // Restrict the height to the size of the parent's height

                     LONG py, pheight;
                     if (!drwGetVisibleArea(object->ParentID, NULL, &py, NULL, NULL, NULL, &pheight)) {
                        if (object->Y + redim.Height >= py + pheight) redim.Height = py + pheight - object->Y;
                     }
                  }
                  else if (Self->Direction & MOVE_UP) {
                     if (Self->prvAnchored) {
                        redim.Y = object->Y + over_y;
                        redim.Height = object->Height - over_y;
                     }
                     else {
                        redim.Y = Self->OriginalY + over_y;
                        redim.Height = Self->OriginalHeight - over_y;
                     }

                     // Restrict the top edge to the parent's visible top edge

                     LONG py;
                     if (!drwGetVisibleArea(object->ParentID, NULL, &py, NULL, NULL, NULL, NULL)) {
                        if (redim.Y < py) {
                           redim.Height  -= py - redim.Y;
                           redim.Y = py;
                        }
                     }

                     // Check minheight/maxheight settings due to 'reverse resizing'

                     if (redim.Height > maxheight) {
                        redim.Y = Self->OriginalY + Self->OriginalHeight - maxheight;
                        redim.Height = maxheight;
                     }
                     else if (redim.Height < minheight) {
                        redim.Y = Self->OriginalY + Self->OriginalHeight - minheight;
                        redim.Height = minheight;
                     }
                  }
                  else {
                     redim.Y = Self->OriginalY;
                     redim.Height = 0;
                  }

                  if (redim.Width < 0)  redim.Width  = 0;
                  if (redim.Height < 0) redim.Height = 0;
                  redim.Z = 0;
                  redim.Depth  = 0;

                  Action(AC_Redimension, object, &redim);
                  //DelayMsg(AC_Redimension, object->UniqueID, &redim); //<- Only works if anchoring is disabled.

                  // If we have anchored the pointer, we need to tell the pointer to move or else it will stay locked
                  // at its current position.

                  redim.Width  = object->Width;
                  redim.Height = object->Height;
                  ReleaseObject(object);

                  if (Self->prvAnchored) {
                     LONG absx, absy;

                     if (!drwGetSurfaceCoords(Self->Layout->SurfaceID, NULL, NULL, &absx, &absy, NULL, NULL)) {
                        if (Self->Direction & MOVE_RIGHT) {
                           absx = ((absx + redim.Width) - (Self->OriginalWidth - Self->prvAnchorX));
                        }
                        else absx = (absx + Self->prvAnchorX);

                        if (Self->Direction & MOVE_DOWN) {
                           absy = ((absy + redim.Height) - (Self->OriginalHeight - Self->prvAnchorY));
                        }
                        else absy = (absy + Self->prvAnchorY);

                        gfxSetCursorPos(absx, absy);
                     }
                  }
               }
               else if (error IS ERR_NoMatchingObject) {
                  Self->ObjectID = 0;
                  acFree(Self); // Commit suicide
               }

               if (inputerror) break;
               else input = scan;
            }
         }

         // Note that this code has to 'drop through' due to the movement consolidation loop earlier in this subroutine.

         if (input->Flags & JTYPE_MOVEMENT) {
            while (!(inputerror = gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &scan))) {
               if (scan->Flags & JTYPE_MOVEMENT) {
                  input = scan;
               }
               else break;
            }

            // If the user is moving the mouse pointer over the resizing area and the mouse button is not currently
            // held, check if we can change the pointer image to something else.  This provides effective visual
            // notification to the user.

            if (input->OverID IS Self->Layout->SurfaceID) {
               LONG cursor;

               LONG x = input->X;
               LONG y = input->Y;
               gfxGetRelativeCursorPos(Self->Layout->SurfaceID, &x, &y);

               if (within_area(Self, x, y)) {
                  cursor = get_cursor_type(Self);  // Determine what cursor we should be using
                  if (cursor != Self->CursorSet) { // If the cursor is to change, use gfxSetCursor() to do it
                     if (!gfxSetCursor(0, CRF_BUFFER|CRF_NO_BUTTONS, cursor, 0, Self->Head.UniqueID)) {
                        Self->CursorSet = cursor;
                     }
                  }
               }
               else if (Self->CursorSet) {
                  gfxRestoreCursor(PTR_DEFAULT, Self->Head.UniqueID);
                  Self->CursorSet = 0;
               }
            }
            else if (Self->CursorSet) {
               gfxRestoreCursor(PTR_DEFAULT, Self->Head.UniqueID);
               Self->CursorSet = 0;
            }

            if (inputerror) break;
            else input = scan;
         }

         // Note that this code has to 'drop through' due to the movement consolidation loop earlier in this subroutine.

         if (input->Type IS Self->Button) {
            if (input->Value > 0) {
               // Check the region to make sure that the button click has fallen in the correct place.

               if (within_area(Self, input->X, input->Y)) {
                  if (!drwGetSurfaceCoords(Self->ObjectID, &Self->OriginalX, &Self->OriginalY, &Self->OriginalAbsX, &Self->OriginalAbsY, &Self->OriginalWidth, &Self->OriginalHeight)) {
                     // Attempt to anchor the pointer (failure is likely on hosted displays)

                     if (!gfxLockCursor(Self->Layout->SurfaceID)) {
                        Self->prvAnchored = TRUE;
                     }

                     Self->prvAnchorX  = input->X; // Remember the original pointer position irrespective of whether or not we got the anchor.
                     Self->prvAnchorY  = input->Y;

                     Self->State = CLICK_HELD;
                  }
               }
            }
            else if (Self->State IS CLICK_HELD) {
               if (Self->prvAnchored) {
                  Self->prvAnchored = FALSE;
                  gfxUnlockCursor(Self->Layout->SurfaceID);
               }

               LONG x, y;
               if ((!gfxGetRelativeCursorPos(Self->Layout->SurfaceID, &x, &y)) AND (within_area(Self, x, y))) {
               }
               else {
                  // Release the pointer image

                  if (Self->CursorSet) {
                     gfxRestoreCursor(PTR_DEFAULT, Self->Head.UniqueID);
                     Self->CursorSet = 0;
                  }
               }

               Self->State = CLICK_RELEASED;
            }
         }
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR RESIZE_Free(objResize *Self, APTR Void)
{
   if (Self->Layout) { acFree(Self->Layout); Self->Layout = NULL; }

   if (Self->prvAnchored) {
      Self->prvAnchored = FALSE;
      gfxUnlockCursor(Self->Layout->SurfaceID);
   }

   if (Self->CursorSet) {
      gfxRestoreCursor(PTR_DEFAULT, Self->Head.UniqueID);
      Self->CursorSet = 0;
   }

   gfxUnsubscribeInput(0);

   return ERR_Okay;
}

//****************************************************************************

static ERROR RESIZE_Init(objResize *Self, APTR Void)
{
   LONG minwidth, minheight, maxwidth, maxheight;

   if (acInit(Self->Layout) != ERR_Okay) {
      return ERR_Init;
   }

   OBJECTPTR surface;
   if (!AccessObject(Self->Layout->SurfaceID, 2000, &surface)) {
      // If the surface has matching dimension restrictions, there is no point in initialising the resize object.

      if (!GetFields(surface, FID_MinWidth|TLONG,  &minwidth,
                              FID_MinHeight|TLONG, &minheight,
                              FID_MaxWidth|TLONG,  &maxwidth,
                              FID_MaxHeight|TLONG, &maxheight,
                              TAGEND)) {
         if ((minwidth IS maxwidth) AND (minheight IS maxheight)) {
            ReleaseObject(surface);
            return ERR_LimitedSuccess;
         }
      }

      ReleaseObject(surface);
   }

   // Subscribing to display input will allow us to receive DATA_INPUT_READY messages in DataFeed().

   gfxSubscribeInput(Self->Layout->SurfaceID, JTYPE_MOVEMENT|JTYPE_BUTTON, 0);

   // If no object was specified for resizing, default to the container

   if (!Self->ObjectID) Self->ObjectID = GetOwner(Self);

   if (GetClassID(Self->ObjectID) != ID_SURFACE) {
      return ERR_Failed;
   }

   if (Self->Border) {
      if (Self->Border & EDGE_TOP_LEFT)  Self->Direction = MOVE_UP|MOVE_LEFT;
      else if (Self->Border & EDGE_TOP)  Self->Direction = MOVE_UP;
      else if (Self->Border & EDGE_LEFT) Self->Direction = MOVE_LEFT;

      if (Self->Border & EDGE_TOP_RIGHT)   Self->Direction = MOVE_UP|MOVE_RIGHT;
      else if (Self->Border & EDGE_RIGHT)  Self->Direction = MOVE_RIGHT;
      else if (Self->Border & EDGE_BOTTOM) Self->Direction = MOVE_DOWN;
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR RESIZE_NewObject(objResize *Self, APTR Void)
{
   Self->Button     = JET_LMB;
   Self->State      = CLICK_RELEASED;
   Self->Direction  = MOVE_DOWN|MOVE_RIGHT;
   Self->BorderSize = 6.0;

   if (!NewObject(ID_LAYOUT, NF_INTEGRAL, &Self->Layout)) {
      return ERR_Okay;
   }
   else return ERR_NewObject;
}

/*****************************************************************************

-FIELD-
Border: Set this field to define the borders that should be monitored.

If your Resize object needs to monitor the borders of the surface area that it is being applied to, use the Border
field to specify which borders should be monitored.  If you do not supply any border flags then the Resize object will
expect you to provide the dimensions for an area to monitor.

The size of the borders that are to be monitored must be set through the BorderSize field.

-FIELD-
BorderSize: Determines the size of the monitored regions when borders are used.

If you have used the Border field to determine what borders should be monitored, it is recommended that you set the
BorderSize field to define the size of the border areas.  If you do not set the BorderSize field then a default value
will be used for determining the border size.

*****************************************************************************/

static ERROR SET_BorderSize(objResize *Self, LONG Value)
{
   if ((Value > 0) AND (Value < 100)) {
      Self->BorderSize = Value;
      return ERR_Okay;
   }
   else return PostError(ERR_OutOfRange);
}

/*****************************************************************************

-FIELD-
Button: Defines the user button that starts the resize process.

By default, the user can interact with a resize area by moving the mouse over it and pressing/holding the left mouse
button or its nearest equivalent.  If you would like to change the button that the Resize object reacts to, you can set
the Button field to a different value.  Valid settings are:

<pre>
1 = Left Mouse Button
2 = Right Mouse Button
3 = Middle Mouse Button
</pre>

-FIELD-
Direction: Limits the directions in which the user can apply resizing.

If you are using a Resize object to monitor a specific region rather than using the border functionality, you will need
to tell the object what directions the user is allowed to apply the resize.  Valid directions are UP, DOWN, LEFT and
RIGHT.  Setting a direction such as UP|LEFT would allow the user to resize towards the top left corner of the display,
but not the bottom right corner.

If you have set the Border field then there is no need to set the Direction as it will be ignored.

-FIELD-
Object: Defines the object that is to be the recipient of the Resize() action.

This field determines the object that receives resize messages when the user interacts with the Resize object.  By
default the Resize object's container will receive the messages, but setting this field directly allows you to change
who the recipient is.
-END-

*****************************************************************************/

//****************************************************************************

#include "class_resize_def.c"

static const struct FieldDef DirectionFlags[] = {
   { "Down",  MOVE_DOWN }, { "Up",    MOVE_UP    },
   { "Left",  MOVE_LEFT }, { "Right", MOVE_RIGHT },
   { "All",   MOVE_ALL  },
   { 0, 0 }
};

static const struct FieldDef Border[] = {
   { "Top",         EDGE_TOP },
   { "Left",        EDGE_LEFT },
   { "Right",       EDGE_RIGHT },
   { "Bottom",      EDGE_BOTTOM },
   { "TopLeft",     EDGE_TOP_LEFT },
   { "TopRight",    EDGE_TOP_RIGHT },
   { "BottomLeft",  EDGE_BOTTOM_LEFT },
   { "BottomRight", EDGE_BOTTOM_RIGHT },
   { "All",         EDGE_ALL },
   { 0, 0 }
};

static const struct FieldDef clButton[] = {
   { "LMB", JET_LMB },
   { "RMB", JET_RMB },
   { "MMB", JET_MMB },
   { 0, 0 }
};

static const struct FieldArray clFields[] = {
   { "Layout",     FDF_INTEGRAL|FDF_SYSTEM|FDF_R, 0, NULL, NULL },
   { "Object",     FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, NULL },
   { "Button",     FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clButton, NULL, NULL },
   { "Direction",  FDF_LONGFLAGS|FDF_RW, (MAXINT)&DirectionFlags, NULL, NULL },
   { "Border",     FDF_LONGFLAGS|FDF_RW, (MAXINT)&Border, NULL, NULL },
   { "BorderSize", FDF_LONG|FDF_RW,      0, NULL, SET_BorderSize },
   END_FIELD
};

//****************************************************************************

ERROR init_resize(void)
{
   return(CreateObject(ID_METACLASS, 0, &clResize,
      FID_Name|TSTRING,        "Resize",
      FID_ClassVersion|TFLOAT, VER_RESIZE,
      FID_Category|TLONG, CCF_GUI,
      FID_Flags|TLONG,    CLF_PROMOTE_INTEGRAL,
      FID_Actions|TPTR,   clResizeActions,
      FID_Fields|TARRAY,  clFields,
      FID_Size|TLONG,     sizeof(objResize),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND));
}

void free_resize(void)
{
   if (clResize) { acFree(clResize); clResize   = NULL; }
}
