// Relevant SVG materials:
// https://www.w3.org/TR/SVG11/animate.html#ToAttribute
// https://www.w3.org/TR/2001/REC-smil-animation-20010904

#include "../link/linear_rgb.h"

//********************************************************************************************************************
// Return an interpolated value based on the values or from/to/by settings.

DOUBLE anim_base::get_numeric_value()
{
   DOUBLE from_val, to_val;

   if (not values.empty()) {
      LONG vi = F2T((values.size()-1) * seek);
      if (vi >= LONG(values.size())-1) vi = values.size() - 2;

      read_numseq(values[vi], { &from_val });
      read_numseq(values[vi+1], { &to_val } );

      // Recompute the seek position to fit between the two values
      const DOUBLE mod = 1.0 / DOUBLE(values.size() - 1);
      seek = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;
   }
   else if (not from.empty()) {
      if (not to.empty()) {
         read_numseq(from, { &from_val });
         read_numseq(to, { &to_val } );
      }
      else if (not by.empty()) {
         return 0;
      }
   }

   const auto offset = to_val;

   if ((accumulate) and (repeat_count)) {
      // Cumulative animation is not permitted for:
      // * The 'to animation' where 'from' is undefined.
      // * Animations that do not repeat

      from_val += offset * repeat_index;
      to_val += offset * repeat_index;
   }

   if (additive IS ADD::SUM) {
      from_val += offset;
      to_val   += offset;
   }

   if (calc_mode IS CMODE::DISCRETE) {
      if (seek < 0.5) return from_val;
      else return to_val;
   }
   else { // CMODE::LINEAR
      return from_val + ((to_val - from_val) * seek);
   }
}

//********************************************************************************************************************
// Return an interpolated value based on the values or from/to/by settings.

DOUBLE anim_base::get_dimension()
{
   DOUBLE from_val, to_val;

   if (not values.empty()) {
      LONG vi = F2T((values.size()-1) * seek);
      if (vi >= LONG(values.size())-1) vi = values.size() - 2;

      read_numseq(values[vi], { &from_val });
      read_numseq(values[vi+1], { &to_val } );

      // Recompute the seek position to fit between the two values
      const DOUBLE mod = 1.0 / DOUBLE(values.size() - 1);
      seek = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;
   }
   else if (not from.empty()) {
      if (not to.empty()) {
         read_numseq(from, { &from_val });
         read_numseq(to, { &to_val } );
      }
      else if (not by.empty()) {
         return 0;
      }
   }

   const auto offset = to_val;

   if ((accumulate) and (repeat_count)) {
      // Cumulative animation is not permitted for:
      // * The 'to animation' where 'from' is undefined.
      // * Animations that do not repeat

      from_val += offset * repeat_index;
      to_val += offset * repeat_index;
   }

   if (additive IS ADD::SUM) {
      from_val += offset;
      to_val   += offset;
   }

   if (calc_mode IS CMODE::DISCRETE) {
      if (seek < 0.5) return from_val;
      else return to_val;
   }
   else { // CMODE::LINEAR
      return from_val + ((to_val - from_val) * seek);
   }
}

//********************************************************************************************************************

FRGB anim_base::get_colour_value()
{
   VectorPainter from_col, to_col;

   if (not values.empty()) {
      LONG vi = F2T((values.size()-1) * seek);
      if (vi >= LONG(values.size())-1) vi = values.size() - 2;
      vecReadPainter(NULL, values[vi].c_str(), &from_col, NULL);
      vecReadPainter(NULL, values[vi+1].c_str(), &to_col, NULL);

      const DOUBLE mod = 1.0 / DOUBLE(values.size() - 1);
      seek = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;
   }
   else if (not from.empty()) {
      if (not to.empty()) {
         vecReadPainter(NULL, from.c_str(), &from_col, NULL);
         vecReadPainter(NULL, to.c_str(), &to_col, NULL);
      }
      else if (not by.empty()) {
         return { 0, 0, 0, 0 };
      }
   }
   else return { 0, 0, 0, 0 };

   if (calc_mode IS CMODE::DISCRETE) {
      if (seek < 0.5) return from_col.Colour;
      else return to_col.Colour;
   }
   else { // CMODE::LINEAR
      // Linear RGB interpolation is superior to operating on raw RGB values.

      glLinearRGB.convert(from_col.Colour);
      glLinearRGB.convert(to_col.Colour);

      auto result = FRGB {
         FLOAT(from_col.Colour.Red   + ((to_col.Colour.Red   - from_col.Colour.Red) * seek)),
         FLOAT(from_col.Colour.Green + ((to_col.Colour.Green - from_col.Colour.Green) * seek)),
         FLOAT(from_col.Colour.Blue  + ((to_col.Colour.Blue  - from_col.Colour.Blue) * seek)),
         FLOAT(from_col.Colour.Alpha + ((to_col.Colour.Alpha - from_col.Colour.Alpha) * seek))
      };

      glLinearRGB.invert(result);
      return result;
   }
}

//********************************************************************************************************************
// Return true if the animation has started

bool anim_base::started(DOUBLE CurrentTime)
{
   if (not first_time) first_time = CurrentTime;

   if (start_time) return true;
   if (repeat_index > 0) return true;

   if (begin_offset) {
      // Check if one of the animation's begin triggers has been tripped.
      const DOUBLE elapsed = CurrentTime - start_time;
      if (elapsed < begin_offset) return false;
   }

   start_time = CurrentTime;
   return true;
}

//********************************************************************************************************************
// Advance the seek position to represent the next frame.

void anim_base::next_frame(DOUBLE CurrentTime)
{
   const DOUBLE elapsed = CurrentTime - start_time;
   seek = elapsed / duration; // A value between 0 and 1.0

   if (seek >= 1.0) { // Check if the sequence has ended.
      if ((repeat_count < 0) or (repeat_index+1 < repeat_count)) {
         repeat_index++;
         start_time = CurrentTime;
         seek = 0;
         return;
      }
      else {
         end_time = CurrentTime; // Setting the end-time will prevent further animation after the completion of this frame.
         seek = 1.0; // Necessary in case the seek range calculation has overflowed
      }
   }

   // repeat_duration prevents the animation from running past a fixed number of seconds since it started.
   if ((repeat_duration > 0) and (elapsed > repeat_duration)) {
      end_time = CurrentTime; // End the animation.
      seek = 1.0;
   }
}

//********************************************************************************************************************
// Set common animation properties

static ERR set_anim_property(extSVG *Self, anim_base &Anim, objVector *Vector, XMLTag &Tag, ULONG Hash, const std::string_view Value)
{
   switch (Hash) {
      case SVF_ID:
         Anim.id = Value;
         add_id(Self, Tag, Value);
         break;

      case SVF_ATTRIBUTENAME: // Name of the target attribute affected by the From and To values.
         Anim.target_attrib = Value;
         break;

      case SVF_ATTRIBUTETYPE: // Namespace of the target attribute: XML, CSS, auto
         if (iequals("XML", Value)) Anim.attrib_type = ATT::XML;
         else if (iequals("CSS", Value)) Anim.attrib_type = ATT::CSS;
         else if (iequals("auto", Value)) Anim.attrib_type = ATT::AUTO;
         break;

      case SVF_FILL: // freeze, remove
         if (iequals("freeze", Value)) Anim.freeze = true; // Freeze the effect value at the last value of the duration (i.e. keep the last frame).
         else if (iequals("remove", Value)) Anim.freeze = true; // The default.  The effect is stopped when the duration is over.
         break;

      case SVF_ADDITIVE: // replace, sum
         if (iequals("replace", Value)) Anim.additive = ADD::REPLACE; // The animation values replace the underlying values of the target vector's attributes.
         else if (iequals("sum", Value)) Anim.additive = ADD::SUM; // The animation adds to the underlying values of the target vector.
         break;

      case SVF_ACCUMULATE:
         if (iequals("none", Value)) Anim.accumulate = false; // Repeat iterations are not cumulative.  This is the default.
         else if (iequals("sum", Value)) Anim.accumulate = true; // Each repeated iteration builds on the last value of the previous iteration.
         break;

      case SVF_FROM: // The starting value of the animation.
         Anim.from = Value;
         break;

      // It is not legal to specify both 'by' and 'to' attributes - if both are specified, only the to attribute will
      // be used (the by will be ignored).

      case SVF_TO: // Specifies the ending value of the animation.
         Anim.to = Value;
         break;

      case SVF_BY: // Specifies a relative offset value for the animation.
         Anim.by = Value;
         break;

      case SVF_BEGIN:
         // Defines when the element should become active.  Specified as a semi-colon list.
         //   offset: A clock-value that is offset from the moment the animation is activated.
         //   id.end/begin: Reference to another animation's begin or end to determine when the animation starts.
         //   event: An event reference like 'focusin' determines that the animation starts when the event is triggered.
         //   id.repeat(value): Reference to another animation, repeat when the given value is reached.
         //   access-key: The animation starts when a keyboard key is pressed.
         //   clock: A real-world clock time (not supported)
         Anim.begin_offset = read_time(Value);
         break;

      case SVF_END:
         // The animation ends when one of the triggers is reached.  A semi-colon list of multiple values is permitted
         // and documented as the 'end-value-list'.  End is paired with 'begin' and should be parsed in the same way.
         break;

      case SVF_DUR: // 4s, 02:33, 12:10:53, 45min, 4ms, 12.93, 1h, 'media', 'indefinite'
         if (iequals("media", Value)) Anim.duration = 0; // Does not apply to animation
         else if (iequals("indefinite", Value)) Anim.duration = -1;
         else Anim.duration = read_time(Value);
         break;

      case SVF_MIN: // Specifies the minimum value of the active duration.
         if (iequals("media", Value)) Anim.min_duration = 0; // Does not apply to animation
         else Anim.min_duration = read_time(Value);
         break;

      case SVF_MAX: // Specifies the maximum value of the active duration.
         if (iequals("media", Value)) Anim.max_duration = 0; // Does not apply to animation
         else Anim.max_duration = read_time(Value);
         break;

      case SVF_CALCMODE: // Specifies the interpolation mode for the animation.
         if (iequals("discrete", Value))    Anim.calc_mode = CMODE::DISCRETE;
         else if (iequals("linear", Value)) Anim.calc_mode = CMODE::LINEAR;
         else if (iequals("paced", Value))  Anim.calc_mode = CMODE::PACED;
         else if (iequals("spline", Value)) Anim.calc_mode = CMODE::SPLINE;
         break;

      case SVF_RESTART: // always, whenNotActive, never
         if (iequals("always", Value)) Anim.restart = RST::ALWAYS;
         else if (iequals("whenNotActive", Value)) Anim.restart = RST::WHEN_NOT_ACTIVE;
         else if (iequals("never", Value)) Anim.restart = RST::NEVER;
         break;

      case SVF_REPEATDUR: // Specifies the total duration for repeat.
         if (iequals("indefinite", Value)) Anim.repeat_duration = -1;
         else Anim.repeat_duration = read_time(Value);
         break;

      case SVF_REPEATCOUNT: // Specifies the number of iterations of the animation function.  Integer, 'indefinite'
         if (iequals("indefinite", Value)) Anim.repeat_count = -1;
         else Anim.repeat_count = read_time(Value);
         break;

      // Similar to 'from' and 'to', this is a series of values that are interpolated over the time line.
      // If a list of values is specified, any from, to and by attribute values are ignored.
      case SVF_VALUES: {
         Anim.values.clear();
         LONG s, v = 0;
         while ((v < std::ssize(Value)) and (std::ssize(Anim.values) < MAX_VALUES)) {
            while ((Value[v]) and (Value[v] <= 0x20)) v++;
            for (s=v; (Value[s]) and (Value[s] != ';'); s++);
            Anim.values.push_back(std::string(Value.substr(v, s-v)));
            v = s;
            if (Value[v] IS ';') v++;
         }
         break;
      }

      case SVF_KEYPOINTS:
         // Takes a semicolon-separated list of floating point values between 0 and 1 and indicates how far along
         // the motion path the object shall move at the moment in time specified by corresponding ‘keyTimes’
         // value. Distance calculations use the user agent's distance along the path algorithm. Each progress
         // value in the list corresponds to a value in the ‘keyTimes’ attribute list.
         break;

      case SVF_KEYTIMES:
         break;

      case SVF_KEYSPLINES:
         break;

      case SVF_EXTERNALRESOURCESREQUIRED:
         // Deprecated
         break;
   }

   return ERR::Okay;
}

//********************************************************************************************************************

void anim_colour::perform()
{

}

//********************************************************************************************************************
// The specified values for 'from', 'by', 'to' and 'values' consists of x, y coordinate pairs, with a single comma
// and/or white space separating the x coordinate from the y coordinate. For example, from="33,15" specifies an x
// coordinate value of 33 and a y coordinate value of 15.

static ERR motion_callback(objVector *Vector, LONG Index, LONG Cmd, DOUBLE X, DOUBLE Y, anim_motion &Motion)
{
   Motion.points.push_back(anim_motion::POINT { FLOAT(X), FLOAT(Y) });
   return ERR::Okay;
};

void anim_motion::perform()
{
   DOUBLE x1, y1, x2, y2;

   pf::ScopedObjectLock<objVector> vector(target_vector, 1000);
   if (vector.granted()) {
      if (not path.empty()) {
         LONG new_timestamp;
         vector->get(FID_PathTimestamp, &new_timestamp);

         if ((points.empty()) or (path_timestamp != new_timestamp)) {
            // Trace the path and store its points
            auto call = C_FUNCTION(motion_callback);
            call.Meta = this;
            points.clear();
            if ((vecTracePath(*path, &call) != ERR::Okay) or (points.empty())) return;
            vector->get(FID_PathTimestamp, &path_timestamp);
         }

         LONG vi = F2T((std::ssize(points)-1) * seek);
         if (vi >= std::ssize(points)-1) vi = std::ssize(points) - 2;

         x1 = points[vi].x;
         y1 = points[vi].y;
         x2 = points[vi+1].x;
         y2 = points[vi+1].y;

         // Recompute the seek position to fit between the two values
         const DOUBLE mod = 1.0 / DOUBLE(points.size() - 1);
         seek = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;
      }
      else if (not values.empty()) {
         LONG vi = F2T((values.size()-1) * seek);
         if (vi >= LONG(values.size())-1) vi = values.size() - 2;

         read_numseq(values[vi], { &x1, &y1 });
         read_numseq(values[vi+1], { &x2, &y2 } );

         // Recompute the seek position to fit between the two values
         const DOUBLE mod = 1.0 / DOUBLE(values.size() - 1);
         seek = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;
      }
      else if (not from.empty()) {
         if (not to.empty()) {
            read_numseq(from, { &x1, &y1 });
            read_numseq(to, { &x2, &y2 } );
         }
         else if (not by.empty()) {
            return;
         }
      }
   }

   if (not matrix) vecNewMatrix(*vector, &matrix);
   vecResetMatrix(matrix);

   if (calc_mode IS CMODE::DISCRETE) {
      if (seek < 0.5) vecTranslate(matrix, x1, y1);
      else vecTranslate(matrix, x2, y2);
   }
   else { // CMODE::LINEAR
      x1 = x1 + ((x2 - x1) * seek);
      y1 = y1 + ((y2 - y1) * seek);
      vecTranslate(matrix, x1, y1);
   }
}

//********************************************************************************************************************

void anim_transform::perform()
{
   pf::ScopedObjectLock<objVector *> vector(target_vector, 1000);
   if (vector.granted()) {
      if (not matrix) vecNewMatrix(*vector, &matrix);

      switch(type) {
         case AT::TRANSLATE: break;
         case AT::SCALE: break;
         case AT::ROTATE: {
            DOUBLE from_angle = 0, from_cx = 0, from_cy = 0, to_angle = 0, to_cx = 0, to_cy = 0;

            if (not values.empty()) {
               LONG vi = F2T((values.size()-1) * seek);
               if (vi >= LONG(values.size())-1) vi = values.size() - 2;

               read_numseq(values[vi], { &from_angle, &from_cx, &from_cy });
               read_numseq(values[vi+1], { &to_angle, &to_cx, &to_cy } );
            }
            else if (not from.empty()) {
               read_numseq(from, { &from_angle, &from_cx, &from_cy });
               if (not to.empty()) {
                  read_numseq(to, { &to_angle, &to_cx, &to_cy } );
               }
               else if (not by.empty()) {
                  read_numseq(by, { &to_angle, &to_cx, &to_cy } );
                  to_angle += from_angle;
                  to_cx += from_cx;
                  to_cy += from_cy;
               }
               else break;
            }
            else break;

            DOUBLE mod = 1.0 / (DOUBLE)(values.size() - 1);
            DOUBLE ratio;
            if (seek == 1.0) ratio = 1.0;
            else ratio = fmod(seek, mod) / mod;

            DOUBLE new_angle = from_angle + ((to_angle - from_angle) * ratio);
            DOUBLE new_cx    = from_cx + ((to_cx - from_cx) * ratio);
            DOUBLE new_cy    = from_cy + ((to_cy - from_cy) * ratio);

            vecResetMatrix(matrix);
            vecRotate(matrix, new_angle, new_cx, new_cy);
            break;
         }
         case AT::SKEW_X: break;
         case AT::SKEW_Y: break;
         default: break;
      }
   }
}

//********************************************************************************************************************
// <rect><animate attributeType="CSS" attributeName="opacity" from="1" to="0" dur="5s" repeatCount="indefinite"/></rect>
// <animate attributeName="font-size" attributeType="CSS" begin="0s" dur="6s" fill="freeze" from="40" to="80"/>
// <animate attributeName="fill" attributeType="CSS" begin="0s" dur="6s" fill="freeze" from="#00f" to="#070"/>

void anim_value::perform()
{
   pf::Log log(__FUNCTION__);

   pf::ScopedObjectLock<objVector> vector(target_vector, 1000);
   if (vector.granted()) {
      // Determine the type of the attribute that we're targeting, then interpolate the value and set it.

      switch(StrHash(target_attrib)) {
         case SVF_FONT_SIZE: {
            auto val = get_numeric_value();
            vector->set(FID_FontSize, val);
            break;
         }

         case SVF_FILL: {
            auto val = get_colour_value();
            vector->setArray(FID_FillColour, (FLOAT *)&val, 4);
            break;
         }

         case SVF_OPACITY: {
            auto val = get_numeric_value();
            vector->set(FID_Opacity, val);
            break;
         }

         case SVF_X: {
            auto val = get_dimension();
            vector->set(FID_X, val);
            break;
         }

         case SVF_Y: {
            auto val = get_dimension();
            vector->set(FID_Y, val);
            break;
         }

         case SVF_WIDTH: {
            auto val = get_dimension();
            vector->set(FID_Width, val);
            break;
         }

         case SVF_HEIGHT: {
            auto val = get_dimension();
            vector->set(FID_Height, val);
            break;
         }
      }
   }
}

//********************************************************************************************************************

static ERR animation_timer(extSVG *SVG, LARGE TimeElapsed, LARGE CurrentTime)
{
   if (SVG->Animations.empty()) {
      pf::Log log(__FUNCTION__);
      log.msg("All animations processed, timer suspended.");
      return ERR::Terminate;
   }

   for (auto &record : SVG->Animations) {
      std::visit([ &record ](auto &&anim) {
         if (anim.end_time) return;

         DOUBLE current_time = DOUBLE(PreciseTime()) / 1000000.0;

         if (not anim.started(current_time)) return;
         anim.next_frame(current_time);
         anim.perform();
      }, record);
   }

   SVG->Scene->Viewport->draw();

   if (SVG->FrameCallback.defined()) {
      if (SVG->FrameCallback.isC()) {
         pf::SwitchContext context(SVG->FrameCallback.Context);
         auto routine = (void (*)(extSVG *, APTR))SVG->FrameCallback.Routine;
         routine(SVG, SVG->FrameCallback.Meta);
      }
      else if (SVG->FrameCallback.isScript()) {
         scCall(SVG->FrameCallback, std::to_array<ScriptArg>({ { "SVG", SVG, FD_OBJECTPTR } }));
      }
   }

   return ERR::Okay;
}