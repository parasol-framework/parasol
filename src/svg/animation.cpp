// Relevant SVG materials:
// https://www.w3.org/TR/SVG11/animate.html#ToAttribute
// https://www.w3.org/TR/2001/REC-smil-animation-20010904

#include "../link/linear_rgb.h"

//********************************************************************************************************************

double anim_motion::get_total_dist()
{
   if (total_dist != 0) return total_dist;

   distances.clear();
   total_dist = 0;
   if (not points.empty()) {
      auto prev = points[0];
      for (auto &pt : points) {
         auto delta = prev - pt;
         total_dist += delta;
         distances.push_back(total_dist);
         prev = pt;
      }
   }
   else if (not values.empty()) {
      POINT<double> prev, pt;
      read_numseq(values[0], { &prev.x, &prev.y });
      distances.push_back(0);
      for (LONG i=1; i < std::ssize(values); i++) {
         read_numseq(values[i], { &pt.x, &pt.y });
         total_dist += prev - pt;
         distances.push_back(total_dist);
         prev = pt;
      }
   }
   else if (not from.empty()) {
      if (not to.empty()) {
         POINT<double> a, b;
         read_numseq(from, { &a.x, &a.y });
         read_numseq(to, { &b.x, &b.y } );
         total_dist = a - b;
      }
      else if (not by.empty()) {
         POINT<double> a = { 0, 0 }, b;
         read_numseq(by, { &b.x, &b.y } );
         total_dist = a - b;
      }
   }
   else return 0;

   return total_dist;
}

//********************************************************************************************************************

double anim_base::get_total_dist()
{
   if (total_dist != 0) return total_dist;

   distances.clear();
   total_dist = 0;
   if (not values.empty()) {
      double prev, val;
      read_numseq(values[0], { &prev });
      distances.push_back(0);
      for (LONG i=1; i < std::ssize(values); i++) {
         read_numseq(values[i], { &val });
         total_dist += std::abs(val - prev);
         distances.push_back(total_dist);
         prev = val;
      }
   }
   else if (not from.empty()) {
      if (not to.empty()) {
         double a, b;
         read_numseq(from, { &a });
         read_numseq(to, { &b } );
         total_dist = std::abs(a - b);
      }
      else if (not by.empty()) {
         read_numseq(by, { &total_dist } );
         total_dist = std::abs(total_dist);
      }
   }
   else return 0;

   return total_dist;
}

//********************************************************************************************************************
// Return an interpolated value based on the values or from/to/by settings.

double anim_base::get_numeric_value()
{
   double from_val, to_val;

   if (not values.empty()) {
      LONG i = F2T((values.size()-1) * seek);
      if (i >= LONG(values.size())-1) i = values.size() - 2;

      read_numseq(values[i], { &from_val });
      read_numseq(values[i+1], { &to_val } );

      // Recompute the seek position to fit between the two values
      const double mod = 1.0 / double(values.size() - 1);
      seek = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;
   }
   else if (not from.empty()) {
      if (not to.empty()) {
         read_numseq(from, { &from_val });
         read_numseq(to, { &to_val } );
      }
      else if (not by.empty()) {
         read_numseq(from, { &from_val });
         read_numseq(by, { &to_val } );
         to_val += from_val;
      }
      else return 0;
   }
   else return 0;

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

double anim_base::get_dimension()
{
   double from_val, to_val;
   double seek_to = seek;

   if (not values.empty()) {
      LONG i;

      if (calc_mode IS CMODE::PACED) {
         const auto dist_pos = seek * get_total_dist();
         for (i=0; (i < std::ssize(distances)-1) and (distances[i+1] < dist_pos); i++);
         seek_to = (dist_pos - distances[i]) / (distances[i+1] - distances[i]);
      }
      else if (calc_mode IS CMODE::SPLINE) {
         i = 0;
         if (timing.empty()) {
            // When no timing is specified, the 'values' are distributed evenly.  This determines
            // what spline-path we are going to use.

            i = std::clamp(F2T(seek * std::ssize(spline_paths)), 0, LONG(std::ssize(spline_paths) - 1));
            auto &sp = spline_paths[i]; // sp = The spline we're going to use

            // Rather than use distance, we're going to use the 'x' position as a lookup on the horizontal axis.
            // The paired y value then gives us the 'real' seek_to value.
            // The spline points are already sorted by the x value to make this easier.
            
            const DOUBLE x = (seek >= 1.0) ? 1.0 : fmod(seek, 1.0 / DOUBLE(std::ssize(spline_paths))) * std::ssize(spline_paths);

            LONG si;
            for (si=0; (si < std::ssize(sp.points)-1) and (sp.points[si+1].point.x < x); si++);

            const double mod_x = x - sp.points[si].point.x;
            const double c = mod_x / sp.points[si].cos_angle;
            seek_to = std::clamp(sp.points[si].point.y + std::sqrt((c * c) - (mod_x * mod_x)), 0.0, 1.0);
         }
         else {

         }
      }
      else {
         i = F2T((values.size()-1) * seek);
         if (i >= LONG(values.size())-1) i = values.size() - 2;
         const double mod = 1.0 / double(values.size() - 1);
         seek_to = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;
      }

      read_numseq(values[i], { &from_val });
      read_numseq(values[i+1], { &to_val });
   }
   else if (not from.empty()) {
      if (not to.empty()) {
         read_numseq(from, { &from_val });
         read_numseq(to, { &to_val } );
      }
      else if (not by.empty()) {
         read_numseq(from, { &from_val });
         read_numseq(by, { &to_val } );
         to_val += from_val;
      }
      else return 0;
   }
   else return 0;

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
      if (seek_to < 0.5) return from_val;
      else return to_val;
   }
   else { // CMODE::LINEAR
      return from_val + ((to_val - from_val) * seek_to);
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

      const double mod = 1.0 / double(values.size() - 1);
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

bool anim_base::started(double CurrentTime)
{
   if (not first_time) first_time = CurrentTime;

   if (start_time) return true;
   if (repeat_index > 0) return true;

   if (begin_offset) {
      // Check if one of the animation's begin triggers has been tripped.
      const double elapsed = CurrentTime - start_time;
      if (elapsed < begin_offset) return false;
   }

   start_time = CurrentTime;
   return true;
}

//********************************************************************************************************************
// Advance the seek position to represent the next frame.

void anim_base::next_frame(double CurrentTime)
{
   const double elapsed = CurrentTime - start_time;
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

static ERR parse_spline(APTR Path, LONG Index, LONG Command, DOUBLE X, DOUBLE Y, anim_base::SPLINE_POINTS &Meta)
{
   Meta.emplace_back(pf::POINT<float> { float(X), float(Y) }, 0);

   if (Meta.size() > 1) {
      Meta[Meta.size()-2].angle = std::atan2(Meta.back().point.y - Meta[Meta.size()-2].point.y, Meta.back().point.x - Meta[Meta.size()-2].point.x);
      Meta[Meta.size()-2].cos_angle = std::cos(Meta[Meta.size()-2].angle);
   }

   return ERR::Okay;
}

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
         while (v < std::ssize(Value)) {
            while ((Value[v]) and (Value[v] <= 0x20)) v++;
            for (s=v; (Value[s]) and (Value[s] != ';'); s++);
            Anim.values.push_back(std::string(Value.substr(v, s-v)));
            v = s;
            if (Value[v] IS ';') v++;
         }
         break;
      }

      // Takes a semicolon-separated list of floating point values between 0 and 1 and indicates how far along
      // the motion path the object shall move at the moment in time specified by corresponding 'keyTimes'
      // value. Distance calculations use the user agent's distance along the path algorithm. Each progress
      // value in the list corresponds to a value in the 'keyTimes' attribute list.

      case SVF_KEYPOINTS: {
         Anim.key_points.clear();
         LONG s, v = 0;
         while (v < std::ssize(Value)) {
            while ((Value[v]) and (Value[v] <= 0x20)) v++;
            for (s=v; (Value[s]) and (Value[s] != ';'); s++);
            std::string_view val = Value.substr(v, s-v);
            double fv;
            auto [ ptr, error ] = std::from_chars(val.data(), val.data() + val.size(), fv);
            fv = std::clamp(fv, 0.0, 1.0);
            Anim.key_points.push_back(fv);
            v = s;
            if (Value[v] IS ';') v++;
         }
         break;
      }

      // A semicolon-separated list of time values used to control the pacing of the animation. Each time in the
      // list corresponds to a value in the 'values' attribute list, and defines when the value is used in the
      // animation function. Each time value in the 'keyTimes' list is specified as a floating point value between
      // 0 and 1 (inclusive), representing a proportional offset into the simple duration of the animation
      // element.
      //
      // For animations specified with a 'values' list, the 'keyTimes' attribute if specified must have exactly as
      // many values as there are in the 'values' attribute. For from/to/by animations, the 'keyTimes' attribute
      // if specified must have two values.
      //
      // Each successive time value must be greater than or equal to the preceding time value.

      case SVF_KEYTIMES: {
         Anim.timing.clear();
         LONG s, v = 0;
         double last_val = 0.0;
         while (v < std::ssize(Value)) {
            while ((Value[v]) and (Value[v] <= 0x20)) v++;
            for (s=v; (Value[s]) and (Value[s] != ';'); s++);
            std::string_view val = Value.substr(v, s-v);
            double fv;
            auto [ ptr, error ] = std::from_chars(val.data(), val.data() + val.size(), fv);
            fv = std::clamp(fv, last_val, 1.0);
            Anim.timing.push_back(fv);
            last_val = fv;
            v = s;
            if (Value[v] IS ';') v++;
         }
         break;
      }

      // A set of Bézier control points associated with the 'keyTimes' list, defining a cubic Bézier function
      // that controls interval pacing. The attribute value is a semicolon-separated list of control point
      // descriptions. Each control point description is a set of four values: x1 y1 x2 y2, describing the
      // Bézier control points for one time segment. Note: SMIL allows these values to be separated either by
      // commas with optional whitespace, or by whitespace alone. The 'keyTimes' values that define the
      // associated segment are the Bézier "anchor points", and the 'keySplines' values are the control points.
      // Thus, there must be one fewer sets of control points than there are 'keyTimes'.
      //
      // The values must all be in the range 0 to 1.
      // This attribute is ignored unless the 'calcMode' is set to 'spline'.
      // Parsing errors must be propagated.

      case SVF_KEYSPLINES: {
         Anim.splines.clear();
         LONG s, v = 0;
         while (v < std::ssize(Value)) {
            while ((Value[v]) and (Value[v] <= 0x20)) v++;
            for (s=v; (Value[s]) and (Value[s] != ';'); s++);
            auto quad = std::string_view(Value.substr(v, s-v));

            POINT<double> a, b;
            read_numseq(quad, { &a.x, &a.y, &b.x, &b.y });
            a.x = std::clamp(a.x, 0.0, 1.0);
            a.y = std::clamp(a.y, 0.0, 1.0);
            b.x = std::clamp(b.x, 0.0, 1.0);
            b.y = std::clamp(b.y, 0.0, 1.0);
            Anim.splines.push_back(std::make_pair(a, b));

            v = s;
            if (Value[v] IS ';') v++;
         }

         if (Anim.splines.size() < 2) Anim.splines.clear();
         else {
            // Convert the splines into bezier paths and generate a point-based path in advance.
            for (auto &sp : Anim.splines) {
               APTR path;
               if (vecGeneratePath(NULL, &path) IS ERR::Okay) {
                  anim_base::SPLINE_POINTS lookup;
                  vecMoveTo(path, 0, 0);
                  vecCurve4(path, sp.first.x, sp.first.y, sp.second.x, sp.second.y, 1.0, 1.0);
                  vecTracePath(path, C_FUNCTION(parse_spline, &lookup), 512.0);
                  Anim.spline_paths.push_back(lookup);
                  FreeResource(path);
               }
            }
         }
         break;
      }

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
// Rotation angles are pre-calculated once.

void anim_motion::precalc_angles()
{
   if (points.empty()) return;

   // Start by calculating all angles from point to point.

   std::vector<double> precalc(points.size());
   POINT prev = points[0];
   precalc[0] = get_angle(points[0], points[1]);
   for (LONG i=1; i < std::ssize(points)-1; i++) {
      precalc[i] = get_angle(prev, points[i]);
      prev = points[i];
   }
   precalc[points.size()-1] = precalc[points.size()-2];

   // Average out the angle for each point so that they have a smoother flow.

   angles.clear();
   angles.reserve(precalc.size());
   angles.push_back(precalc[0]);
   for (LONG i=1; i < std::ssize(precalc)-1; i++) {
      angles.push_back((precalc[i] + precalc[i-1] + precalc[i+1]) / 3);
   }
   angles.push_back(precalc.back());
}

//********************************************************************************************************************

static ERR motion_callback(objVector *Vector, LONG Index, LONG Cmd, double X, double Y, anim_motion &Motion)
{
   Motion.points.push_back(pf::POINT<FLOAT> { FLOAT(X), FLOAT(Y) });
   return ERR::Okay;
};

void anim_motion::perform()
{
   double x1, y1, x2, y2, angle = -1;
   double seek_to = seek;

   pf::ScopedObjectLock<objVector> vector(target_vector, 1000);
   if (!vector.granted()) return;

   // Note that the order of processing here is important, and matches the priorities documented for SVG's
   // animateMotion property.

   if ((mpath) or (not path.empty())) {
      LONG new_timestamp;
      vector->get(FID_PathTimestamp, &new_timestamp);

      if ((points.empty()) or (path_timestamp != new_timestamp)) {
         // Trace the path and store its points.  Transforms are completely ignored when pulling the path from
         // an external source.

         auto call = C_FUNCTION(motion_callback, this);

         points.clear();
         if (mpath) {
            if ((vecTrace(mpath, &call, vector->get<double>(FID_DisplayScale), false) != ERR::Okay) or (points.empty())) return;
         }
         else if ((vecTrace(*path, &call, 1.0, false) != ERR::Okay) or (points.empty())) return;

         vector->get(FID_PathTimestamp, &path_timestamp);

         if ((auto_rotate IS ART::AUTO) or (auto_rotate IS ART::AUTO_REVERSE)) {
            precalc_angles();
         }
      }

      if (calc_mode IS CMODE::PACED) {
         const auto dist = get_total_dist();
         const auto dist_pos = seek * dist;

         // Use the distances array to determine the correct index.

         LONG i;
         for (i=0; (i < std::ssize(distances)-1) and (distances[i+1] < dist_pos); i++);

         x1 = points[i].x;
         y1 = points[i].y;
         x2 = points[i+1].x;
         y2 = points[i+1].y;

         seek_to = (dist_pos - distances[i]) / (distances[i+1] - distances[i]);

         if ((auto_rotate IS ART::AUTO) or (auto_rotate IS ART::AUTO_REVERSE)) {
            angle = (angles[i] * (1.0 - seek_to)) + (angles[i+1] * seek_to);
            if (auto_rotate IS ART::AUTO_REVERSE) angle += 180.0;
         }
      }
      else { // CMODE::LINEAR: Interpolate between the two values
         LONG i = F2T((std::ssize(points)-1) * seek);
         if (i >= std::ssize(points)-1) i = std::ssize(points) - 2;

         x1 = points[i].x;
         y1 = points[i].y;
         x2 = points[i+1].x;
         y2 = points[i+1].y;

         const double mod = 1.0 / double(points.size() - 1);
         seek_to = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;

         if ((auto_rotate IS ART::AUTO) or (auto_rotate IS ART::AUTO_REVERSE)) {
            angle = (angles[i] * (1.0 - seek_to)) + (angles[i+1] * seek_to);
            if (auto_rotate IS ART::AUTO_REVERSE) angle += 180.0;
         }
      }
   }
   else if (not values.empty()) {
      // Values are x,y coordinate pairs.

      LONG i;
      if (calc_mode IS CMODE::PACED) {
         const auto dist_pos = seek * get_total_dist();
         for (i=0; (i < std::ssize(distances)-1) and (distances[i+1] < dist_pos); i++);
         seek_to = (dist_pos - distances[i]) / (distances[i+1] - distances[i]);
      }
      else { // CMODE::LINEAR: Interpolate between the two values
         i = F2T((std::ssize(values)-1) * seek);
         if (i >= std::ssize(values)-1) i = values.size() - 2;
         const double mod = 1.0 / double(values.size() - 1);
         seek_to = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;
      }

      read_numseq(values[i], { &x1, &y1 });
      read_numseq(values[i+1], { &x2, &y2 });
   }
   else if (not from.empty()) {
      if (not to.empty()) {
         read_numseq(from, { &x1, &y1 });
         read_numseq(to, { &x2, &y2 } );
      }
      else if (not by.empty()) {
         read_numseq(from, { &x1, &y1 });
         read_numseq(by, { &x2, &y2 } );
         x2 += x1;
         y2 += y1;
      }
      else return;
   }
   else return;

   if (not matrix) vecNewMatrix(*vector, &matrix);
   vecResetMatrix(matrix);

   if (angle != -1) vecRotate(matrix, angle, 0, 0);
   else if (auto_rotate IS ART::FIXED)  vecRotate(matrix, rotate, 0, 0);

   if (calc_mode IS CMODE::DISCRETE) {
      if (seek_to < 0.5) vecTranslate(matrix, x1, y1);
      else vecTranslate(matrix, x2, y2);
   }
   else { // CMODE::LINEAR
      x1 = x1 + ((x2 - x1) * seek_to);
      y1 = y1 + ((y2 - y1) * seek_to);
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
            double from_angle = 0, from_cx = 0, from_cy = 0, to_angle = 0, to_cx = 0, to_cy = 0;

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

            double mod = 1.0 / (double)(values.size() - 1);
            double ratio;
            if (seek == 1.0) ratio = 1.0;
            else ratio = fmod(seek, mod) / mod;

            double new_angle = from_angle + ((to_angle - from_angle) * ratio);
            double new_cx    = from_cx + ((to_cx - from_cx) * ratio);
            double new_cy    = from_cy + ((to_cy - from_cy) * ratio);

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

         double current_time = double(PreciseTime()) / 1000000.0;

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