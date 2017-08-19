/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Scroll: This class aids the creation of scrollbars and sliders.

The Scroll class provides a facility for creating scrollbars that allow the user to move surface objects within their
containers.  It can also be used to scroll contents or 'pages' of information (useful for Text Editors for instance).
In either case, it is most useful in situations  where the available graphics space is not sufficient for the amount
of information that needs to be shown.  The Scroll class in itself is only designed to provide scrolling functionality,
i.e. it does not create any gadgetry or graphics for the scrollbar.  For that reason, complete scrollbars are usually
created using scripts or helper classes, such as the @ScrollBar class.

The Scroll class is designed to provide scrolling in 3 different directions - along the X, Y, or Z axis.  You can
combine the different axis, so a diagonal scrolling gadget could be created for example.  It is also possible to create
buttons that are attached to the scroll object.  Refer to the Left, Right, Up, Down, In and Out fields for more
information.

You can use the Scroll class to create fixed or proportional scrollbars depending on what the situation dictates.
Where possible you should use proportional scrolling as it is the easier of the two to set up.  All you need to do is
specify the page size (which represents the width or height of the graphical content) and the view size (the 'window'
into the information).  To create a fixed scrollbar, set the #SliderSize manually.

Some objects are supportive of scrolling - for example, the @Text class supports horizontal and vertical
scrolling through its HScroll and VScroll fields.  Objects that support scrollbars expect to be connected directly to
an appropriate scroll object.  Once that connection is established, they will take over the scroll object so that the
page size and positioning is always managed correctly.  In such cases it is not necessary for you to pre-calculate the
scale or page and view sizes.

-END-

*****************************************************************************/

//#define DEBUG
#define PRV_SCROLL
#define __system__
#include <parasol/main.h>
#include <parasol/modules/widget.h>
#include <parasol/modules/display.h>
#include <parasol/modules/surface.h>
#include "defs.h"

#define ScrollMsg ActionMsg

static OBJECTPTR clScroll = NULL;
static const struct FieldArray clFields[];
static const struct ActionArray clScrollActions[];
static const struct MethodArray clScrollMethods[];

static ERROR process_click(objScroll *, OBJECTID, LONG, LONG);
static void update_scroll(objScroll *, LONG, LONG, DOUBLE, LONG);
static void send_feedback(objScroll *, DOUBLE, DOUBLE, DOUBLE);

//****************************************************************************

INLINE DOUBLE check_position(objScroll *Self, DOUBLE Position)
{
   DOUBLE result = Position;

   if (Position < 0) result = 0;
   else if (Self->PageSize <= Self->ViewSize) result = 0;
   else if (Self->Flags & SCF_SLIDER) {
      if (Position > Self->PageSize) result = Self->PageSize;
   }
   else if (Position > Self->PageSize - (Self->ViewSize - Self->ObscuredView)) {
      result = Self->PageSize - (Self->ViewSize - Self->ObscuredView);
   }

   if (result != Position) FMSG("check_position()","Requested %.2f, allowing %.2f.  (Page: %d, View: %d, Obscured: %d", Position, result, Self->PageSize, Self->ViewSize, Self->ObscuredView);

   return result;
}

//****************************************************************************

static void set_position(objScroll *Self, DOUBLE Position)
{
   if (Position IS Self->Position) return;

   FMSG("~set_position()","%.2f, Current: %.2f", Position, Self->Position);

   SetDouble(Self, FID_Position, Position); //Self->Position = Position;

   if (Self->Flags & SCF_AUTO_ACTIVATE) acActivate(Self);

   // Inform the object if it wants a field update

   if ((Self->Field[0]) AND (Self->ObjectID)) {
      OBJECTPTR object;
      if (!AccessObject(Self->ObjectID, 5000, &object)) {
         UBYTE buffer[32];
         IntToStr(Position, buffer, sizeof(buffer));
         SetFieldEval(object, Self->Field, buffer);
         ReleaseObject(object);
      }
   }

   STEP();
}

//****************************************************************************

static ERROR SCROLL_ActionNotify(objScroll *Self, struct acActionNotify *Notify)
{
   if (Notify->Error != ERR_Okay) return ERR_Okay;

   if (Notify->ActionID IS AC_Redimension) {
      struct acRedimension *resize = (struct acRedimension *)Notify->Args;

      FMSG("~","Redimension notification received by Scroll object.  Size: %.0fx%.0f,%.0fx%.0f", resize->X, resize->Y, resize->Width, resize->Height);

      if (Notify->ObjectID IS Self->SliderID) { // The slider has moved
         if (Self->RecursionBlock) {
            MSG("Recursive block protection.");
            STEP();
            return ERR_Okay;
         }

         // MoveToPoint messages originate from the slider

         LONG slidepos;
         if (Self->Flags & SCF_HORIZONTAL) slidepos = resize->X;
         else slidepos = resize->Y;

         DOUBLE position;
         if (Self->SliderSize >= Self->BarSize) position = 0;
         else if (Self->Flags & SCF_REVERSE) {
            if (Self->Flags & SCF_SLIDER) position = Self->PageSize * ((DOUBLE)(slidepos - Self->StartMargin) / (DOUBLE)(Self->BarSize - Self->SliderSize));
            else position = (Self->PageSize - (Self->ViewSize - Self->ObscuredView)) * ((DOUBLE)(slidepos - Self->StartMargin) / (DOUBLE)(Self->BarSize - Self->SliderSize));
            position = Self->PageSize - position;
         }
         else if (Self->Flags & SCF_SLIDER) {
            position = Self->PageSize * ((DOUBLE)(slidepos - Self->StartMargin) / (DOUBLE)(Self->BarSize - Self->SliderSize));
            MSG("Move detected in slider (slide mode).  %.2f = (slidepos %d - startmargin %d) / (barsize %d - slidersize %d)", position, slidepos, Self->StartMargin, Self->BarSize, Self->SliderSize);
         }
         else {
            DOUBLE pct;
            pct = ((DOUBLE)(slidepos - Self->StartMargin) / (DOUBLE)(Self->BarSize - Self->SliderSize));
            position = ((DOUBLE)(Self->PageSize - (Self->ViewSize - Self->ObscuredView))) * pct;
            MSG("Percentage: %.2f = (%d slidepos - %d startmargin) / (%d barsize - %d slidersize)", pct, slidepos, Self->StartMargin, Self->BarSize, Self->SliderSize);
            MSG("Move detected in scroll slider.  %.2f = (pagesize %d - (viewsize %d - obscured %d) * %.2f%%", position, Self->PageSize, Self->ViewSize, Self->ObscuredView, pct);
         }

         position = check_position(Self, position);

         if (position IS Self->Position) { STEP(); return ERR_Okay; }

         // NB: Delays are used because drawing whilst inside of Redimension notifications is disabled by the Surface class.

         Self->RecursionBlock++;

         struct acScroll scroll;
         if (Self->ObjectID) {
            if (Self->Flags & SCF_RELATIVE) {
               if (Self->Axis IS AXIS_X) scroll.XChange = (slidepos - Self->PrevCoord);
               else scroll.XChange = 0;

               if (Self->Axis IS AXIS_Y) scroll.YChange = (slidepos - Self->PrevCoord);
               else scroll.YChange = 0;

               if (Self->Axis IS AXIS_Z) scroll.ZChange = (slidepos - Self->PrevCoord);
               else scroll.ZChange = 0;

               scroll.XChange = scroll.XChange * (Self->PageSize / (Self->ViewSize - Self->ObscuredView));
               scroll.YChange = scroll.YChange * (Self->PageSize / (Self->ViewSize - Self->ObscuredView));
               scroll.ZChange = scroll.ZChange * (Self->PageSize / (Self->ViewSize - Self->ObscuredView));
               ScrollMsg(AC_Scroll, Self->ObjectID, &scroll);
            }
            else if (Self->Axis IS AXIS_X) {
               struct acScrollToPoint scrollto = { position, 0, 0, STP_X };
               ScrollMsg(AC_ScrollToPoint, Self->ObjectID, &scrollto);
            }
            else if (Self->Axis IS AXIS_Y) {
               struct acScrollToPoint scrollto = { 0, position, 0, STP_Y };
               ScrollMsg(AC_ScrollToPoint, Self->ObjectID, &scrollto);
            }
            else if (Self->Axis IS AXIS_Z) {
               struct acScrollToPoint scrollto = { 0, 0, position, STP_Z };
               ScrollMsg(AC_ScrollToPoint, Self->ObjectID, &scrollto);
            }
            else LogErrorMsg("Invalid Axis setting of %d.", Self->Axis);
         }

         if (Self->Feedback.Type) {
            if (Self->Flags & SCF_RELATIVE) {
               DOUBLE x, y, z;
               if (Self->Axis IS AXIS_X) x = slidepos - Self->PrevCoord;
               else x = 0;

               if (Self->Axis IS AXIS_Y) y = slidepos - Self->PrevCoord;
               else y = 0;

               if (Self->Axis IS AXIS_Z) z = slidepos - Self->PrevCoord;
               else z = 0;

               x = x * (Self->PageSize / (Self->ViewSize - Self->ObscuredView));
               y = y * (Self->PageSize / (Self->ViewSize - Self->ObscuredView));
               z = z * (Self->PageSize / (Self->ViewSize - Self->ObscuredView));

               send_feedback(Self, scroll.XChange, scroll.YChange, scroll.ZChange);
            }
            else if (Self->Axis IS AXIS_X) {
               send_feedback(Self, position, -1, -1);
            }
            else if (Self->Axis IS AXIS_Y) {
               send_feedback(Self, -1, position, -1);
            }
            else if (Self->Axis IS AXIS_Z) {
               send_feedback(Self, -1, -1, position);
            }
         }

         set_position(Self, position);
         Self->RecursionBlock--;
         Self->PrevCoord = slidepos;
      }

      if (Notify->ObjectID IS Self->ScrollbarID) {
         if (Self->Flags & SCF_VERTICAL) Self->BarSize = resize->Height - (Self->StartMargin + Self->EndMargin);
         else Self->BarSize = resize->Width - (Self->StartMargin + Self->EndMargin);
      }

      if ((Notify->ObjectID IS Self->ViewID) OR ((Notify->ObjectID IS Self->ScrollbarID) AND (!Self->ViewID))) {
         LONG viewlength, viewsize;

         // The size of the view has changed

         if (Self->PageSize <= 0) { STEP(); return ERR_Okay; }

         if (Self->Flags & SCF_VERTICAL) viewlength = resize->Height;
         else viewlength = resize->Width;

         if (Self->Flags & SCF_SLIDER) { // For sliders, the viewsize is preset to a fixed value
            viewsize = Self->ViewSize;
         }
         else viewsize = viewlength;

         FMSG("~","Size of the view has changed to %d, obscured: %d, pos %.2f, barsize: %d (%d+%d margins)", viewlength, Self->ObscuredView, Self->Position, Self->BarSize, Self->StartMargin, Self->EndMargin);

            DOUBLE pos = Self->Position;

            if ((Self->PageSize <= (viewsize - Self->ObscuredView)) AND (Self->Position > 0) AND (!(Self->Flags & SCF_SLIDER))) {
               // If the page is smaller than the view area, reset the object to position zero.

               if (Self->Flags & SCF_RELATIVE) {

               }
               else {
                  struct acScrollToPoint scrollto = { 0, 0, 0, 0 };

                  if (Self->Axis IS AXIS_X) scrollto.Flags = STP_X;
                  else scrollto.Flags = STP_Y;

                  if (Self->ObjectID) {
                     DelayMsg(AC_ScrollToPoint, Self->ObjectID, &scrollto); // Use a delay to give good redraw results
                  }

                  if (Self->Feedback.Type) {
                     send_feedback(Self, (Self->Axis IS AXIS_X) ? 0 : -1, (Self->Axis IS AXIS_Y) ? 0 : -1, (Self->Axis IS AXIS_Z) ? 0 : -1);
                  }
               }
               pos = 0;
            }

            update_scroll(Self, -1, viewsize, pos, Self->Unit);

         STEP();
      }

      STEP();
   }
   else if (Notify->ActionID IS AC_Free) {
      if ((Self->Feedback.Type IS CALL_SCRIPT) AND (Self->Feedback.Script.Script->UniqueID IS Notify->ObjectID)) {
         Self->Feedback.Type = CALL_NONE;
      }
   }
   else if (Notify->ActionID IS AC_Hide) {
      OBJECTPTR bar, intersect;

      // The Hide action is received when an -intersecting- scrollbar is hidden.  This code will adjust our position
      // to deal with the intersection point.

      FMSG("~","Intersecting scrollbar hidden.");

      if (Self->PostIntersect) { // Recompute the viewable area
         Self->ObscuredView = 0;
         update_scroll(Self, -1, Self->BarSize + Self->StartMargin + Self->EndMargin, Self->Position, Self->Unit);
      }
      else if (!AccessObject(Self->ScrollbarID, 5000, &bar)) {
         if (!AccessObject(Self->IntersectSurface, 5000, &intersect)) {
            LONG offset;
            if (Self->Flags & SCF_HORIZONTAL) {
               GetLong(intersect, FID_XOffset, &offset);
               SetLong(bar, FID_XOffset, offset);
            }
            else if (Self->Flags & SCF_VERTICAL) {
               GetLong(intersect, FID_YOffset, &offset);
               SetLong(bar, FID_YOffset, offset);
            }
            ReleaseObject(intersect);
         }
         ReleaseObject(bar);
      }

      STEP();
   }
   else if (Notify->ActionID IS AC_Scroll) {
      if (Self->RecursionBlock) return ERR_Okay;

      struct acScroll *scroll = (struct acScroll *)Notify->Args;

      if (Notify->ObjectID IS Self->ObjectID) {
         // If the message came from the object maintained by the scrollbar, we need to adjust our slider rather than
         // send another scroll signal.

         MSG("Scroll action received from #%d - moving the slider.", Notify->ObjectID);

         Self->RecursionBlock++;

         struct acMove move;
         move.XChange = ((DOUBLE)(scroll->XChange * Self->BarSize) / (DOUBLE)(Self->PageSize - (Self->ViewSize - Self->ObscuredView)));
         move.YChange = ((DOUBLE)(scroll->YChange * Self->BarSize) / (DOUBLE)(Self->PageSize - (Self->ViewSize - Self->ObscuredView)));
         move.ZChange = scroll->ZChange;
         ActionMsg(AC_Move, Self->SliderID, &move);

         Self->RecursionBlock--;
      }
      else if (Notify->ObjectID IS Self->MonitorID) {
         DOUBLE position;

         FMSG("~","Scroll action received from monitored #%d - sending scroll signal.", Notify->ObjectID);

         // A scroll request has come from the monitored object.  We have to send a scroll message to the object that
         // our scrollbar is controlling, then update our slider so that it reflects the new position.

         if (Self->Axis IS AXIS_X) {
            position = check_position(Self, Self->Position + scroll->XChange);

            if (Self->ObjectID) {
               if (Self->Flags & SCF_RELATIVE) {
                  ActionMsg(AC_Scroll, Self->ObjectID, scroll); // Pass the original scroll message straight to the subscribed object
               }
               else {
                  struct acScrollToPoint scrollto = { position, 0, 0, STP_X };
                  ActionMsg(AC_ScrollToPoint, Self->ObjectID, &scrollto);
               }
            }

            if (Self->Feedback.Type) {
               if (Self->Flags & SCF_RELATIVE) {
                  send_feedback(Self, scroll->XChange, scroll->YChange, scroll->ZChange);
               }
               else send_feedback(Self, position, -1, -1);
            }
         }
         else if (Self->Axis IS AXIS_Y) {
            position = check_position(Self, Self->Position + scroll->YChange);

            if (Self->ObjectID) {
               struct acScrollToPoint scrollto = { 0, position, 0, STP_Y };
               ActionMsg(AC_ScrollToPoint, Self->ObjectID, &scrollto);
            }

            if (Self->Feedback.Type) send_feedback(Self, -1, position, -1);
         }
         else if (Self->Axis IS AXIS_Z) {
            position = check_position(Self, Self->Position + scroll->YChange);

            if (Self->ObjectID) {
               struct acScrollToPoint scrollto = { 0, 0, position, STP_Z };
               ActionMsg(AC_ScrollToPoint, Self->ObjectID, &scrollto);
            }

            if (Self->Feedback.Type) send_feedback(Self, -1, -1, position);
         }
         else {
            LogErrorMsg("Invalid Axis setting of %d.", Self->Axis);
            return ERR_Okay;
         }

         MSG("Updating slider position.");

         update_scroll(Self, -1, -1, position, Self->Unit);

         STEP();
      }
   }
   else if (Notify->ActionID IS AC_Show) {
      OBJECTPTR bar;

      // The Show action is received when an intersecting scrollbar is shown.  This code will adjust our position to
      // deal with the intersection point.

      FMSG("~","Intersecting scrollbar has been shown.  PostIntersect: %d", Self->PostIntersect);

      if (Self->PostIntersect) {
         // Recompute the viewable area.  The vertical bar is usually 'post intersect' because
         // it overlaps the horizontal bar.

         Self->ObscuredView = 0;

         LONG ix, iy, iwidth, iheight;
         LONG vx, vy, vwidth, vheight;
         if (!drwGetSurfaceCoords(Self->IntersectSurface, NULL, NULL, &ix, &iy, &iwidth, &iheight)) {
            if (!drwGetSurfaceCoords(Self->ViewID ? Self->ViewID : Self->ScrollbarID, NULL, NULL, &vx, &vy, &vwidth, &vheight)) {
               if (Self->Flags & SCF_HORIZONTAL) {
                  Self->ObscuredView = vx + vwidth - ix;
                  if (Self->ObscuredView < 0) Self->ObscuredView = 0;
               }
               else if (Self->Flags & SCF_VERTICAL) {
                  Self->ObscuredView = vy + vheight - iy;
                  if (Self->ObscuredView < 0) Self->ObscuredView = 0;
               }
            }
            else {
               if (Self->Flags & SCF_HORIZONTAL) Self->ObscuredView = iwidth;
               else if (Self->Flags & SCF_VERTICAL) Self->ObscuredView = iheight;
            }

            if (Self->ViewSize > 0) update_scroll(Self, -1, -1, Self->Position, Self->Unit);
            else MSG("ViewSize undefined.");
         }
      }
      else if (!AccessObject(Self->ScrollbarID, 5000, &bar)) {
         // This is usually the horizontal bar

         objSurface *intersect;
         LONG offset, size;

         if (!AccessObject(Self->IntersectSurface, 5000, &intersect)) {
            if (Self->Flags & SCF_HORIZONTAL) {
               GetFields(intersect, FID_XOffset|TLONG, &offset, FID_Width|TLONG, &size, TAGEND);
               SetLong(bar, FID_XOffset, offset + size);
            }
            else if (Self->Flags & SCF_VERTICAL) {
               GetFields(intersect, FID_YOffset|TLONG, &offset, FID_Height|TLONG, &size, TAGEND);
               SetLong(bar, FID_YOffset, offset + size);
            }

            ReleaseObject(intersect);
         }
         ReleaseObject(bar);
      }

      STEP();
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Activate: Calls the Activate action on all children of the scroll object.
-END-
*****************************************************************************/

static ERROR SCROLL_Activate(objScroll *Self, APTR Void)
{
   struct ChildEntry list[16];
   LONG count = ARRAYSIZE(list);
   if (!ListChildren(GetUniqueID(Self), list, &count)) {
      WORD i;
      if (Self->Flags & SCF_MESSAGE) {
         for (i=0; i < count; i++) DelayMsg(AC_Activate, list[i].ObjectID, NULL);
      }
      else for (i=0; i < count; i++) acActivateID(list[i].ObjectID);
   }
   return ERR_Okay;
}

//****************************************************************************

#define PAGESCROLL 0.80

enum {
   DIR_NONE=0,
   DIR_NEGATIVE,
   DIR_POSITIVE
};

static ERROR process_click(objScroll *Self, OBJECTID NotifyID, LONG X, LONG Y)
{
   SURFACEINFO *slider;

   if (drwGetSurfaceInfo(Self->SliderID, &slider) != ERR_Okay) return ERR_Failed;

   FMSG("~process_click()","Surface: %d, XY: %dx%d, Slider: %dx%d,%dx%d, Margins: %d,%d, Unit: %d", NotifyID, X, Y, slider->X, slider->Y, slider->Width, slider->Height, Self->StartMargin, Self->EndMargin, Self->Unit);

   struct acMove move = { 0, 0, 0 };

   if (NotifyID IS Self->ScrollbarID) {
      // The empty area surrounding the slider was clicked.  Scroll a single page in the correct direction.

      if (Self->Flags & SCF_HORIZONTAL) {
         move.XChange = slider->Width;

         if ((Y >= slider->Y) AND (Y <= slider->Y + slider->Height)) {
            if (X < slider->X) {
               // Slide left
               if (X >= Self->StartMargin) {
                  move.XChange = -move.XChange;
                  ActionMsg(AC_Move, Self->SliderID, &move);
               }
            }
            else if (X > slider->X + slider->Width) {
               if (X <= (Self->StartMargin + Self->BarSize)) {
                  ActionMsg(AC_Move, Self->SliderID, &move);
               }
            }
         }
      }
      else if (Self->Flags & SCF_VERTICAL) {
         if ((X >= slider->X) AND (X <= slider->X + slider->Width)) {
            DOUBLE pos = -1;

            BYTE dir = DIR_NONE;
            if ((Y >= Self->StartMargin) AND (Y < slider->Y)) {
               dir = DIR_NEGATIVE;
            }
            else if ((Y > slider->Y + slider->Height) AND (Y <= (Self->StartMargin + Self->BarSize))) {
               dir = DIR_POSITIVE;
            }

            if (Self->Flags & SCF_REVERSE) {
               if (dir IS DIR_NEGATIVE) dir = DIR_POSITIVE;
               else if (dir IS DIR_POSITIVE) dir = DIR_NEGATIVE;
            }

            if (dir IS DIR_NEGATIVE) {
               pos = check_position(Self, Self->Position - ((Self->ViewSize - Self->ObscuredView) * PAGESCROLL));

               if (Self->Flags & SCF_RELATIVE) {
                  struct acScroll scroll;
                  scroll.XChange = 0;
                  scroll.YChange = -ABS(F2T(Self->Position - pos));
                  scroll.ZChange = 0;
                  ActionMsg(AC_Scroll, Self->ObjectID, &scroll);

                  send_feedback(Self, 0, scroll.YChange, 0);
               }
               else {
                  struct acScrollToPoint scrollto = { .X = 0, .Y = F2T(pos), .Z = 0, .Flags = STP_Y };
                  ActionMsg(AC_ScrollToPoint, Self->ObjectID, &scrollto);

                  send_feedback(Self, -1, scrollto.Y, -1);
               }
            }
            else if (dir IS DIR_POSITIVE) {
               pos = check_position(Self, Self->Position + ((Self->ViewSize - Self->ObscuredView) * PAGESCROLL));

               if (Self->Flags & SCF_RELATIVE) {
                  struct acScroll scroll;
                  scroll.XChange = 0;
                  scroll.YChange = ABS(F2T(Self->Position - pos));
                  scroll.ZChange = 0;
                  ActionMsg(AC_Scroll, Self->ObjectID, &scroll);

                  send_feedback(Self, 0, scroll.YChange, 0);
               }
               else {
                  struct acScrollToPoint scrollto = { .X = 0, .Y = F2T(pos), .Z = 0, .Flags = STP_Y };
                  ActionMsg(AC_ScrollToPoint, Self->ObjectID, &scrollto);

                  send_feedback(Self, -1, scrollto.Y, -1);
               }
            }

            if (pos != -1) update_scroll(Self, -1, -1, pos, Self->Unit);
         }
      }
   }
   else {
      LONG change, i;

      for (i=0; i < ARRAYSIZE(Self->Buttons); i++) {
         if (Self->Buttons[i].ButtonID != NotifyID) continue;

         if (Self->Unit < 1) {
            if (Self->Flags & SCF_HORIZONTAL) {
               change = slider->Width;
               if (Self->Buttons[i].Direction IS SD_NEGATIVE) change = -change;
            }
            else {
               change = slider->Height;
               if (Self->Buttons[i].Direction IS SD_NEGATIVE) change = -change;
            }

            if (Self->Axis IS AXIS_X) move.XChange = change;
            else move.XChange = 0;
            if (Self->Axis IS AXIS_Y) move.YChange = change;
            else move.YChange = 0;
            if (Self->Axis IS AXIS_Z) move.ZChange = change;
            else move.ZChange = 0;

            ActionMsg(AC_Move, Self->SliderID, &move);
         }
         else {
            DOUBLE pos;

            if (Self->Buttons[i].Direction IS SD_NEGATIVE) {
               pos = check_position(Self, Self->Position - Self->Unit);
            }
            else pos = check_position(Self, Self->Position + Self->Unit);

            FMSG("process_click:","Position change to %.2f from %.2f", pos, Self->Position);

            if (F2T(pos) != Self->Position) {
               update_scroll(Self, Self->PageSize, Self->ViewSize, pos, Self->Unit);

               if (Self->Flags & SCF_RELATIVE) {
                  if (Self->Buttons[i].Direction IS SD_NEGATIVE) change = -Self->Unit;
                  else change = Self->Unit;

                  struct acScroll scroll;
                  if (Self->Axis IS AXIS_X) scroll.XChange = change;
                  else scroll.XChange = 0;
                  if (Self->Axis IS AXIS_Y) scroll.YChange = change;
                  else scroll.YChange = 0;
                  if (Self->Axis IS AXIS_Z) scroll.ZChange = change;
                  else scroll.ZChange = 0;
                  ActionMsg(AC_Scroll, Self->ObjectID, &scroll);

                  send_feedback(Self, scroll.XChange, scroll.YChange, scroll.ZChange);
               }
               else {
                  struct acScrollToPoint scrollto;
                  if (Self->Axis IS AXIS_X) { scrollto.X = Self->Position; scrollto.Flags = STP_X; }
                  else if (Self->Axis IS AXIS_Y) { scrollto.Y = Self->Position; scrollto.Flags = STP_Y; }
                  else if (Self->Axis IS AXIS_Z) { scrollto.Z = Self->Position; scrollto.Flags = STP_Z; }
                  ActionMsg(AC_ScrollToPoint, Self->ObjectID, &scrollto);

                  send_feedback(Self, (Self->Axis IS AXIS_X) ? Self->Position : -1, (Self->Axis IS AXIS_Y) ? Self->Position : -1, (Self->Axis IS AXIS_Z) ? Self->Position : -1);
               }
            }
         }
      }
   }

   STEP();
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
AddScrollButton: Registers a new button with the scroll object.

The AddScrollButton method is used to aid in the creation of scrollbars that feature buttons.

Buttons are normally created from the @Button or @Surface classes, but it is possible to use any class that allocates a
suitable surface for passing to this method.  The button must have a nominated direction when activated, which can be
expressed with either the SD_NEGATIVE or SD_POSITIVE values.

-INPUT-
oid Surface: Must refer to a @Surface object.
int(SD) Direction: The direction that the button represents - SD_NEGATIVE or SD_POSITIVE.

-ERRORS-
Okay
NullArgs
-END-

*****************************************************************************/

static ERROR SCROLL_AddScrollButton(objScroll *Self, struct scAddScrollButton *Args)
{
   if ((!Args) OR (!Args->SurfaceID) OR (!Args->Direction)) return PostError(ERR_NullArgs);

   LogBranch("%d", Args->SurfaceID);

   WORD i;
   for (i=0; i < ARRAYSIZE(Self->Buttons); i++) {
      if (Self->Buttons[i].Direction IS Args->Direction) break;
      if (!Self->Buttons[i].ButtonID) break;
   }

   if (i >= ARRAYSIZE(Self->Buttons)) return PostError(ERR_ArrayFull);

   if (!gfxSubscribeInput(Args->SurfaceID, JTYPE_BUTTON|JTYPE_REPEATED, 0)) {
      if (Self->Buttons[i].ButtonID) gfxUnsubscribeInput(Self->Buttons[i].ButtonID);

      Self->Buttons[i].ButtonID = Args->SurfaceID;

      if (Args->Direction IS 3) Self->Buttons[i].Direction = SD_NEGATIVE; // Backwards compatible with SD_LEFT
      else if (Args->Direction IS 4) Self->Buttons[i].Direction = SD_POSITIVE; // Backwards compatible with SD_RIGHT
      else Self->Buttons[i].Direction = Args->Direction;
   }
   else {
      LogBack();
      return PostError(ERR_Failed);
   }

   LogBack();
   return ERR_Okay;
}

//****************************************************************************

static ERROR SCROLL_DataFeed(objScroll *Self, struct acDataFeed *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->DataType IS DATA_INPUT_READY) {
      struct InputMsg *input;

      while (!gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &input)) {
         if (input->Flags & JTYPE_BUTTON) {
            if (input->Value > 0) {
               process_click(Self, input->RecipientID, input->X, input->Y);
            }
         }
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR SCROLL_Free(objScroll *Self, APTR Void)
{
   OBJECTPTR object;

   if ((Self->SliderID) AND (!AccessObject(Self->SliderID, 5000, &object))) {
      UnsubscribeAction(object, 0);
      ReleaseObject(object);
   }

   if ((Self->ScrollbarID) AND (!AccessObject(Self->ScrollbarID, 5000, &object))) {
      UnsubscribeAction(object, 0);
      ReleaseObject(object);
   }

   if ((Self->ViewID) AND (!AccessObject(Self->ViewID, 5000, &object))) {
      UnsubscribeAction(object, 0);
      ReleaseObject(object);
   }

   if ((Self->MonitorID) AND (!AccessObject(Self->MonitorID, 5000, &object))) {
      UnsubscribeAction(object, 0);
      ReleaseObject(object);
   }

   if ((Self->ObjectID) AND (!AccessObject(Self->ObjectID, 5000, &object))) {
      UnsubscribeAction(object, 0);
      ReleaseObject(object);
   }

   if ((Self->IntersectSurface) AND (!AccessObject(Self->IntersectSurface, 5000, &object))) {
      UnsubscribeAction(object, 0);
      ReleaseObject(object);
   }

   gfxUnsubscribeInput(0);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Hide: Hides the scrollbar.
-END-
*****************************************************************************/

static ERROR SCROLL_Hide(objScroll *Self, APTR Void)
{
   FMSG("~","Passing to surface %d", Self->ScrollbarID);

   LONG flags;
   if (!drwGetSurfaceFlags(Self->ScrollbarID, &flags)) {
      if (flags & RNF_VISIBLE) acHideID(Self->ScrollbarID);
   }

   STEP();
   return ERR_Okay;
}

//****************************************************************************

static ERROR SCROLL_Init(objScroll *Self, APTR Void)
{
   if (!(Self->Flags & (SCF_HORIZONTAL|SCF_VERTICAL))) { // Is the scrollbar horizontal or vertical?
      Self->Flags |= SCF_VERTICAL;
   }

   if (!Self->Axis) { // Defines the axis that is signalled when the slider is moved.
      if (Self->Flags & SCF_HORIZONTAL) Self->Axis = AXIS_X;
      else Self->Axis = AXIS_Y;
   }

   if (!Self->SliderID) { // Find the surface object that we are associated with
      OBJECTID owner_id = GetOwner(Self);
      while ((owner_id) AND (GetClassID(owner_id) != ID_SURFACE)) {
         owner_id = GetOwnerID(owner_id);
      }
      if (!owner_id) return PostError(ERR_UnsupportedOwner);
      else Self->SliderID = owner_id;
   }

   if (Self->PageID) { // Derive the object from the parent of the elected page
      SURFACEINFO *info;
      if (!drwGetSurfaceInfo(Self->PageID, &info)) {
         Self->ObjectID = info->ParentID;
         Self->ViewID   = info->ParentID;
         if (Self->Flags & SCF_VERTICAL) Self->PageSize = info->Height;
         else Self->PageSize = info->Width;
      }
   }

   if (!Self->ObjectID) {
      if (!Self->ObjectID) LogMsg("Warning: The Object field is not set."); // Minor warning, do not abort
   }

   // Monitor the scroll container for movement, and the Slider's surface container for Resize actions.

   objSurface *surface;
   LONG offset, size, visible;
   if (!AccessObject(Self->SliderID, 5000, &surface)) {
      // NOTE: The Scrollbar is a reference to a surface and not a member of the Scrollbar class.

      Self->ScrollbarID = GetOwner(surface);

      SubscribeAction(surface, AC_Redimension); // This is to listen for movement by the slider

      Self->SliderX = surface->X;
      Self->SliderY = surface->Y;

      // Calculate the inside bar height/width and subscribe to the Scrollbar's Redimension action.

      OBJECTPTR bar;
      if (!AccessObject(Self->ScrollbarID, 5000, &bar)) {
         gfxSubscribeInput(Self->ScrollbarID, JTYPE_BUTTON|JTYPE_REPEATED, 0);

         // In the case of intersecting scrollbars, it may be better that the size of the view is actually determined
         // from the length of the scrollbar.

         OBJECTID bar_parent;
         GetLong(bar, FID_Parent, &bar_parent);
         if (Self->ViewID IS bar_parent) Self->ViewID = 0;

         SubscribeAction(bar, AC_Redimension);

         if (Self->Flags & SCF_VERTICAL) {
            GetLong(bar, FID_Height, &Self->BarSize);

            if (Self->StartMargin IS -1) GetLong(surface, FID_TopLimit, &Self->StartMargin);
            if (Self->EndMargin IS -1) GetLong(surface, FID_BottomLimit, &Self->EndMargin);
         }
         else if (Self->Flags & SCF_HORIZONTAL) {
            GetLong(bar, FID_Width, &Self->BarSize);

            if (Self->StartMargin IS -1) GetLong(surface, FID_LeftLimit, &Self->StartMargin);
            if (Self->EndMargin IS -1) GetLong(surface, FID_RightLimit, &Self->EndMargin);
         }

         if (!Self->ViewID) Self->ViewSize = Self->BarSize;

         Self->BarSize -= (Self->StartMargin + Self->EndMargin);

         ReleaseObject(bar);
      }

      ReleaseObject(surface);
   }

   if (Self->ViewID) {
      objSurface *view;
      if (!AccessObject(Self->ViewID, 5000, &view)) {
         SubscribeAction(view, AC_Redimension);

         if (Self->Flags & SCF_HORIZONTAL) Self->ViewSize = view->Width;
         else Self->ViewSize = view->Height;

         ReleaseObject(view);
      }
   }

   // Subscribe to the Scroll action of the target object.  This allows us to adjust the sliders in the event that
   // somebody scrolls the target object without informing us directly.

   if (Self->ObjectID) {
      OBJECTPTR object;
      if (!AccessObject(Self->ObjectID, 5000, &object)) {
         SubscribeActionTags(object, AC_Scroll, TAGEND);
         ReleaseObject(object);
      }
   }

   // If a surface is to be monitored for scroll commands (e.g. from the mouse wheel) we will subscribe to it here.

   //if (Self->MonitorID IS Self->ObjectID) Self->MonitorID = 0;

   if ((Self->MonitorID) AND (Self->MonitorID != Self->ObjectID)) {
      OBJECTPTR object;
      if (!AccessObject(Self->MonitorID, 5000, &object)) {
         SubscribeActionTags(object, AC_Scroll, TAGEND);
         ReleaseObject(object);
      }
      else Self->MonitorID = 0;
   }

   // If an intersecting scrollbar has been specified, subscribe to its surface's Hide and Show actions.

   if ((Self->IntersectID) AND (Self->IntersectSurface)) {
      objScroll *intersect;
      if (!AccessObject(Self->IntersectID, 5000, &intersect)) {
         if (!AccessObject(Self->IntersectSurface, 5000, &surface)) {
            SubscribeActionTags(surface, AC_Hide, AC_Show, TAGEND);

            SetLong(intersect, FID_Intersect, Self->Head.UniqueID);

            // Position ourselves according to whether or not the intersecting scrollbar is visible.

            GetLong(surface, FID_Visible, &visible);

            OBJECTPTR bar;
            if (!AccessObject(Self->ScrollbarID, 5000, &bar)) {
               if (visible) {
                  if (Self->Flags & SCF_HORIZONTAL) {
                     GetFields(surface, FID_XOffset|TLONG, &offset, FID_Width|TLONG, &size, TAGEND);
                     SetLong(bar, FID_XOffset, offset + size);
                  }
                  else if (Self->Flags & SCF_VERTICAL) {
                     GetFields(surface, FID_YOffset|TLONG, &offset, FID_Height|TLONG, &size, TAGEND);
                     SetLong(bar, FID_YOffset, offset + size);
                  }

                  MSG("Intersection bar is visible, shrunk to offset %d.", offset - size);
               }
               else {
                  if (Self->Flags & SCF_HORIZONTAL) {
                     GetLong(surface, FID_XOffset, &offset);
                     SetLong(bar, FID_XOffset, offset);
                  }
                  else if (Self->Flags & SCF_VERTICAL) {
                     GetLong(surface, FID_YOffset, &offset);
                     SetLong(bar, FID_YOffset, offset);
                  }
                  MSG("Intersection bar is invisible, expanded to offset %d.", offset);
               }
               ReleaseObject(bar);
            }
            ReleaseObject(surface);
         }
         else Self->IntersectID = 0;

         ReleaseObject(intersect);
      }
      else Self->IntersectID = 0;
   }

   // If both the PageSize and ViewSize values have been specified, set up the scrolling area to reflect the settings.

   if ((Self->PageSize) AND (Self->ViewSize)) {
      LogMsg("Preset PageSize %d, ViewSize %d and Position %.2f", Self->PageSize, Self->ViewSize, Self->Position);

      update_scroll(Self, -1, -1, Self->Position, Self->Unit);
   }

   LogMsg("Object: %d, Slider: %d, Scrollbar: %d", Self->ObjectID, Self->SliderID, Self->ScrollbarID);

   return ERR_Okay;
}

//****************************************************************************

static ERROR SCROLL_NewObject(objScroll *Self, APTR Void)
{
   Self->Unit        = 1;
   Self->PrevCoord   = -1;
   Self->StartMargin = -1;
   Self->EndMargin   = -1;
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Shows: Shows the #Scrollbar
-END-
*****************************************************************************/

static ERROR SCROLL_Show(objScroll *Self, APTR Args)
{
   // This code decides whether it is necessary to see the scrollbar or not, according to its values.  In auto-hide
   // mode, it may actually hide the scrollbar if it shouldn't be visible.

   if (Self->Flags & SCF_SLIDER) {
      // Do nothing in slider mode as there is no definitive scrollbar
   }
   else if (!(Self->Flags & SCF_INVISIBLE)) {
      LONG flags;
      if (!drwGetSurfaceFlags(Self->ScrollbarID, &flags)) {
         if (Self->Flags & SCF_AUTO_HIDE) {
            MSG("Checking autohide, pagesize: %d/%d, offset: %d, Slider: %d, Bar: %d", Self->PageSize, Self->ViewSize, Self->Offset, Self->SliderSize, Self->BarSize);
            if ((Self->PageSize <= 1) OR (Self->ViewSize < 1)) {
               if (flags & RNF_VISIBLE) acHideID(Self->ScrollbarID);
            }
            else if ((Self->Offset IS 0) AND ((Self->PageSize <= (Self->ViewSize - Self->ObscuredView)) OR (Self->SliderSize >= Self->BarSize))) {
               if (flags & RNF_VISIBLE) acHideID(Self->ScrollbarID);
            }
            else if (!(flags & RNF_VISIBLE)) acShowID(Self->ScrollbarID);
         }
         else acShowID(Self->ScrollbarID);
      }
   }
   else MSG("Surface marked as invisible.");

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
UpdateScroll: Updates the dimensions of a scroll object's slider.

Post-initialisation, the correct way to make changes to the #PageSize, #Position or #ViewSize is to use the
UpdateScroll() method.  It validates and updates the size and position information so that the slider is in the correct
state.

-INPUT-
int PageSize: The size of the page.  Set to zero for no change.
int ViewSize: The size of the view of the page.  Set to zero for no change.
int Position: The current position within the page.
int Unit:     The unit size to use for micro-scrolling.  Set to zero for no change, or -1 to enable jump scrolling.

-ERRORS-
Okay
NullArgs
-END-

*****************************************************************************/

static ERROR SCROLL_UpdateScroll(objScroll *Self, struct scUpdateScroll *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   update_scroll(Self, Args->PageSize, Args->ViewSize, Args->Position, Args->Unit);
   return ERR_Okay;
}

//****************************************************************************

static void update_scroll(objScroll *Self, LONG PageSize, LONG ViewSize, DOUBLE Position, LONG Unit)
{
   Self->Position = Position;

   if (PageSize > 0) Self->PageSize = PageSize;
   if (ViewSize > 0) Self->ViewSize = ViewSize;
   if (Unit) Self->Unit = Unit;

   FMSG("~","Pos: %.2f, Page: %d, View: %d (Req: %d), Obscured: %d, Unit: %d, %s [Start]", Self->Position, Self->PageSize, Self->ViewSize, ViewSize, Self->ObscuredView, Self->Unit, (Self->Flags & SCF_HORIZONTAL) ? "Horizontal" : "Vertical");

   if ((Self->PageSize < 0) OR (Self->ViewSize <= 0)) {
      LogErrorMsg("Illegal pagesize (%d) and/or viewsize (%d)", Self->PageSize, Self->ViewSize);
      STEP();
      return;
   }

   if (!Self->PageSize) {
      Self->Position = 0;
      Self->PageSize = 1; // Set to 1 in order to prevent division by zero errors
   }

   Self->RecursionBlock++;

   Self->Position = check_position(Self, Self->Position);

   LONG minsize, view_size;
   if (Self->Flags & SCF_SLIDER) view_size = Self->ViewSize;
   else view_size = Self->ViewSize - Self->ObscuredView;

   DOUBLE pos = Self->Position;
   if (Self->Flags & SCF_REVERSE) pos = ((DOUBLE)Self->PageSize - pos);
   if (pos < 0) pos = 0;

   if (!(Self->Flags & SCF_FIXED)) { // Proportional slider
      Self->SliderSize = (Self->BarSize * view_size) / Self->PageSize;
   }

   if ((Self->Flags & (SCF_SLIDER|SCF_FIXED))) minsize = 11;
   else minsize = 20;

   LONG offset;
   if (Self->SliderSize < minsize) { // This routine is for a fixed slider
      Self->SliderSize = minsize;

      DOUBLE scale = (DOUBLE)(Self->PageSize - view_size) / (DOUBLE)(Self->BarSize - minsize);
      offset = F2I(pos / scale);

      // Do not allow the slider size to exceed the maximum amount of movement space available to the slider.

      if ((Self->SliderSize + offset) > Self->BarSize) {
         offset = Self->BarSize - Self->SliderSize;
         if (offset < 0) offset = 0;
         if ((Self->SliderSize + offset) > Self->BarSize) {
            Self->SliderSize = Self->BarSize;
         }
      }
   }
   else {
      if (Self->Flags & SCF_SLIDER) { // This routine is for a proportional slider
         offset = pos * (Self->SliderSize - Self->BarSize) / Self->PageSize;
      }
      else { // This routine is for a proportional scrollbar
         if ((pos + view_size) IS Self->PageSize) offset = Self->BarSize - Self->SliderSize;
         else {
            offset = F2I((pos * Self->BarSize) / Self->PageSize);
         }
      }

      // Do not allow the slider size to exceed the maximum amount of movement space available to the slider.

      if ((Self->SliderSize + offset) > Self->BarSize) {
         Self->SliderSize = Self->BarSize - offset;
      }
   }

   // Set the values

   if (offset < 0) {
      #ifdef DEBUG
         LogErrorMsg("Calculated illegal slider offset of %d.", offset);
      #endif
      offset = -offset;
   }

   Self->Offset = offset;

   if (Self->Flags & SCF_VERTICAL) {
      acRedimensionID(Self->SliderID, Self->SliderX, Self->StartMargin + offset, 0, 0, Self->SliderSize, 0);
      if (Self->PrevCoord IS -1) Self->PrevCoord = Self->StartMargin + offset;
   }
   else if (Self->Flags & SCF_HORIZONTAL) {
      acRedimensionID(Self->SliderID, Self->StartMargin + offset, Self->SliderY, 0, Self->SliderSize, 0, 0);
      if (Self->PrevCoord IS -1) Self->PrevCoord = Self->StartMargin + offset;
   }

   SCROLL_Show(Self, NULL); // Run the autohide decision code

   // If an object field is linked to us, we must always ensure that it is told of the current position.

   if ((Self->Field[0]) AND (Self->ObjectID)) {
      OBJECTPTR object;
      if (!AccessObject(Self->ObjectID, 5000, &object)) {
         UBYTE buffer[32];
         IntToStr(F2T(Self->Position), buffer, sizeof(buffer));
         SetFieldEval(object, Self->Field, buffer);
         ReleaseObject(object);
      }
   }

   MSG("Final Pos: %.2f, Page: %d, View: %d (%d), SliderSize: %d, BarSize: %d [End]", Self->Position, Self->PageSize, Self->ViewSize, view_size, Self->SliderSize, Self->BarSize);

   set_position(Self, Self->Position);

   Self->RecursionBlock--;
   STEP();
}

/*****************************************************************************

-FIELD-
Axis: The axis that the scroll object represents can be defined here.

When a scroll slider is moved, scroll messages are sent for one axis only - either X, Y, or Z.  You need to define the
axis here using one of the constants AXIS_X, AXIS_Y or AXIS_Z.  The axis does not necessarily have to match the
orientation of the scrollbar.  For instance, a horizontal scrollbar can send vertical scroll messages if you use AXIS_Y.

*****************************************************************************/

static ERROR SET_Axis(objScroll *Self, LONG Value)
{
   if (Value IS AXIS_X) Self->Axis = AXIS_X;
   else if (Value IS AXIS_Y) Self->Axis = AXIS_Y;
   else if (Value IS AXIS_Z) {
      Self->Axis = AXIS_Z;
      Self->Flags |= SCF_SLIDER;
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
EndMargin: Private. Prevents the slider from moving beyond the bounds of its parent surface.

-FIELD-
Feedback: Provides instant feedback for the user's scrolling.

Set the Feedback field with a callback function in order to receive instant feedback when scrolling occurs.  The
function prototype is `routine(*Scroll, DOUBLE X, DOUBLE Y, DOUBLE Z)`

If the RELATIVE flag is set in the #Flags field, then the X, Y and Z values will be expressed in terms of the
distance travelled to complete the scroll operation.  Otherwise, the values are expressed in absolute coordinates.
Any parameter that is set to -1 indicates that the axis is ignored.

*****************************************************************************/

static ERROR GET_Feedback(objScroll *Self, FUNCTION **Value)
{
   if (Self->Feedback.Type != CALL_NONE) {
      *Value = &Self->Feedback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Feedback(objScroll *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->Feedback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->Feedback.Script.Script, AC_Free);
      Self->Feedback = *Value;
      if (Self->Feedback.Type IS CALL_SCRIPT) SubscribeAction(Self->Feedback.Script.Script, AC_Free);
   }
   else Self->Feedback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Field: Reference to an object field that should be updated with the scroll value.

If you would like to write the position of a scroll object to a field belonging to another object (refer to the Object
field), you can make reference to the field name here.  By doing this, whenever the scroll object updates its internal
position value, it will also write that value to the referenced field name.

*****************************************************************************/

static ERROR SET_Field(objScroll *Self, CSTRING Value)
{
   if (Value) StrCopy(Value, Self->Field, sizeof(Self->Field));
   else Self->Field[0] = 0;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Flags: Optional scroll flags.

-FIELD-
Intersect: This field is used for managing intersecting scrollbars.

When two scroll objects are used to create opposing scrollbars (e.g. horizontal and vertical bars) then you should set
the Intersect field if there is an overlap between the two.  The purpose of this is to keep the scrollbars neatly
arranged if one of them disappears (which will occur if the viewable area is larger than the size of the scrollable
page).

The Intersect field must be set to a valid scroll object that represents the opposing scrollbar.  The opposing scroll
object does not need to have its Intersect field set.

If you use the ScrollBar class, intersections are managed automatically.

*****************************************************************************/

static ERROR SET_Intersect(objScroll *Self, OBJECTID ObjectID)
{
   if ((Self->IntersectID = ObjectID)) {
      if (GetClassID(Self->IntersectID) != ID_SCROLL) {
         Self->IntersectID = 0;
         LogErrorMsg("The Intersect field can only be set with valid Scroll objects.");
         return ERR_Failed;
      }

      objScroll *intersect;
      if (!AccessObject(Self->IntersectID, 5000, &intersect)) {
         Self->IntersectSurface = intersect->ScrollbarID;
         ReleaseObject(intersect);
      }
      else {
         Self->IntersectID = 0;
         return ERR_AccessObject;
      }
   }

   // If we have been initialised already, then this is a post-intersection setting.  In this mode we are not required
   // to adjust our scrollbar position, but we do need to make adjustments to the available viewing area in the event
   // that the intersecting scrollbar is obscuring the scrollable page.

   if (Self->Head.Flags & NF_INITIALISED) {
      Self->PostIntersect = TRUE;
      OBJECTPTR surface;
      if (!AccessObject(Self->IntersectSurface, 5000, &surface)) {
         SubscribeActionTags(surface, AC_Hide, AC_Show, TAGEND);
         ReleaseObject(surface);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Monitor: Objects can be monitored for scroll requests by setting this field.

To monitor an object for scroll requests, set this field to a valid object ID.  This feature is often used to support
the wheel mouse, for instances where the mouse is positioned over a surface area and the wheel is used.  The use of the
wheel will cause Scroll messages to be sent from the mouse to the underlying surface.  By setting this field to the
surface area that is being scrolled, the scrollbar can receive and respond to the scroll messages.

*****************************************************************************/

static ERROR SET_Monitor(objScroll *Self, OBJECTID Value)
{
   if (Self->MonitorID IS Value) return ERR_Okay;

   if (Self->Head.Flags & NF_INITIALISED) {
      if (Self->MonitorID IS Self->ObjectID) {
         // Do nothing because we will already have subscribed to the Scroll action
      }
      else {
         OBJECTPTR object;
         if (Self->MonitorID) {
            if (!AccessObject(Self->MonitorID, 5000, &object)) {
               UnsubscribeAction(object, AC_Scroll);
               Self->MonitorID = 0;
               ReleaseObject(object);
            }
         }

         if (!Value) Self->MonitorID = 0;
         else if (!AccessObject(Value, 5000, &object)) {
            SubscribeActionTags(object, AC_Scroll, TAGEND);
            Self->MonitorID = Value;
            ReleaseObject(object);
         }
         else return PostError(ERR_AccessObject);
      }
   }
   else Self->MonitorID = Value;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Object: Refers to the object that will be targeted with the scroll action.

The Object field must refer to a foreign object that will receive Scroll notification calls whenever the scroll object
is moved.

-FIELD-
Page: Refers to a surface that acts as a scrollable page (optional).

The Page field can be set prior to initialisation to refer to a surface that acts as a scrollable page.  Doing so
will have the following effects:

<ol>
<li>The #Object and #View fields will be forcibly set to the parent surface of the page object.</li>
<li>The #PageSize field will be set to the width or height of the page, depending on orientation settings.</li>
</ol>

Following initialisation the Page field will serve no further purpose.

-FIELD-
PageSize: Defines the size of the page that is to be scrolled.

The page size of the area that is being scrolled is declared through this field.  The page size should almost always be
larger than the view size, because the page lies 'under' the view.  If the page is smaller than the view, the scroll
object will serve no purpose until the circumstances are changed.

*****************************************************************************/

static ERROR SET_PageSize(objScroll *Self, LONG Value)
{
   Self->PageSize = Value;
   if (Self->Head.Flags & NF_INITIALISED) update_scroll(Self, Self->PageSize, -1, Self->Position, Self->Unit);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Position: Reflects the current position of the page within the view.

The Position indicates the offset of the page within its view.  Prior to initialisation, it may be necessary to predefine
the Position value if the page is already offset within the view.  Otherwise, leave this field at the default position
of 0.

When a page moves within its view, the Position field will be updated to reflect the current offset.

*****************************************************************************/

static ERROR SET_Position(objScroll *Self, DOUBLE Value)
{
   if (Value IS Self->Position) return ERR_Okay;
   Self->Position = Value;
   if (Self->Position > Self->PageSize) Self->Position = Self->PageSize;
   if (Self->Position < 0) Self->Position = 0;
   if (Self->Head.Flags & NF_INITIALISED) update_scroll(Self, -1, -1, Self->Position, Self->Unit);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Slider: Refers to the surface that represents the slider widget.

The Slider is a compulsory field value that refers to the surface that acts as the slider widget.  If it
is not set prior to initialisation, the scroll object will attempt to find the closest viable surface by following
its ownership chain and sets the Slider field automatically.

To work effectively, the slider must be placed within another surface that is suitable for acting as the scrollbar
region.

-FIELD-
SliderSize: The size of the slider, measured in pixels.

The SliderSize field indicates the size of the slider that represents the scroll object.  This field can be set
prior to initialisation if a fixed-size slider is required (note that this results in a non-proportional scrollbar).

*****************************************************************************/

static ERROR SET_SliderSize(objScroll *Self, LONG Value)
{
   if (!(Self->Head.Flags & NF_INITIALISED)) {
      Self->SliderSize = Value;
      Self->Flags |= SCF_FIXED;
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
StartMargin: Prevents the slider from moving beyond the bounds of its parent surface.

The StartMargin and EndMargin fields are used to prevent the slider from moving beyond the bounds of its parent
surface.  This feature is typically used to prevent the slider from entering the space allocated to buttons at either
end of the scrollbar.

For instance, if buttons with a size of 16x16 are placed in the scrollbar then the StartMargin and EndMargin
would both be set to 16.

-FIELD-
Unit: Defines the amount of movement used when a scroll button is pressed.

If buttons are linked to a scroll object through the In, Out, Left, Right, Up or Down fields then consider
specifying the amount of units to use when a related button is pressed.  Example:  If the unit is set to 15 and the
scroll position is currently set to 60, pressing a negative button would change the position to 45.

The Unit value should normally be positive, but if set to 0 or less then the Unit will be dynamically calculated
to match the size of the slider.

-FIELD-
View: Refers to a surface that contains a scrollable page (optional).

For page-based scroll handling, set the View field to the surface object that is acting as the view for the page.
Doing so enables pro-active monitoring of the view surface for events such as resizing.  The scroll object will
automatically respond by checking the new dimensions of the view and recalculating the page coordinates.  Any changes
will be reflected in the scroll object's field values and reported via the #Feedback mechanism.

-FIELD-
ViewSize: Defines the size of the view that contains the page.

The ViewSize defines the width or height of the area that contains the page, depending on the orientation of the
slider (if horizontal, then ViewSize would reflect the width).
-END-

*****************************************************************************/

static ERROR SET_ViewSize(objScroll *Self, LONG Value)
{
   Self->ViewSize = Value;
   if (Self->Head.Flags & NF_INITIALISED) update_scroll(Self, -1, Self->ViewSize, Self->Position, Self->Unit);
   return ERR_Okay;
}

//****************************************************************************

static void send_feedback(objScroll *Self, DOUBLE X, DOUBLE Y, DOUBLE Z)
{
   if (Self->Feedback.Type IS CALL_STDC) {
      void (*routine)(OBJECTPTR Context, objScroll *, DOUBLE X, DOUBLE Y, DOUBLE Z);

      routine = Self->Feedback.StdC.Routine;

      if (Self->Feedback.StdC.Context) {
         OBJECTPTR context = SetContext(Self->Feedback.StdC.Context);
         routine(Self->Feedback.StdC.Context, Self, X, Y, Z);
         SetContext(context);
      }
      else routine(Self->Feedback.StdC.Context, Self, X, Y, Z);
   }
   else if (Self->Feedback.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Self->Feedback.Script.Script)) {
         const struct ScriptArg args[] = {
            { "Scroll", FD_OBJECTPTR, { .Address = Self } },
            { "X",      FD_DOUBLE,    { .Double = X } },
            { "Y",      FD_DOUBLE,    { .Double = Y } },
            { "Z",      FD_DOUBLE,    { .Double = Z } }
         };
         scCallback(script, Self->Feedback.Script.ProcedureID, args, ARRAYSIZE(args));
      }
   }
}

//****************************************************************************

#include "class_scroll_def.c"

static const struct FieldArray clFields[] = {
   { "Position",    FDF_DOUBLE|FDF_RW,    0, NULL, SET_Position },
   { "Object",      FDF_OBJECTID|FDF_RW,  0, NULL, NULL },
   { "Slider",      FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, NULL },
   { "Intersect",   FDF_OBJECTID|FDF_RW,  ID_SCROLL, NULL, SET_Intersect },
   { "Monitor",     FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, SET_Monitor },
   { "View",        FDF_OBJECTID|FDF_RI,  ID_SURFACE, NULL, NULL },
   { "Page",        FDF_OBJECTID|FDF_RI,  ID_SURFACE, NULL, NULL },
   { "Unit",        FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Flags",       FDF_LONGFLAGS|FDF_RW, (MAXINT)&clScrollFlags, NULL, NULL },
   { "PageSize",    FDF_LONG|FDF_RW,      0, NULL, SET_PageSize },
   { "ViewSize",    FDF_LONG|FDF_RW,      0, NULL, SET_ViewSize },
   { "StartMargin", FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "EndMargin",   FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "SliderSize",  FDF_LONG|FDF_RI,      0, NULL, SET_SliderSize },
   { "Axis",        FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clScrollAxis, NULL, SET_Axis },
   // Virtual Fields
   { "Field",       FDF_STRING|FDF_W,     0, NULL, SET_Field },
   { "Feedback",    FDF_FUNCTIONPTR|FDF_RW, 0, GET_Feedback, SET_Feedback },
   END_FIELD
};

//****************************************************************************

ERROR init_scroll(void)
{
   return(CreateObject(ID_METACLASS, 0, &clScroll,
      FID_ClassVersion|TFLOAT, VER_SCROLL,
      FID_Name|TSTRING,   "Scroll",
      FID_Category|TLONG, CCF_GUI,
      FID_Actions|TPTR,   clScrollActions,
      FID_Methods|TARRAY, clScrollMethods,
      FID_Fields|TARRAY,  clFields,
      FID_Size|TLONG,     sizeof(objScroll),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND));
}

void free_scroll(void)
{
   if (clScroll) { acFree(clScroll); clScroll = NULL; }
}
