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

double anim_base::get_numeric_value(objVector &Vector, FIELD Field)
{
   double from_val, to_val;
   double seek_to = seek;

   if (not values.empty()) {
      LONG i;
      if (timing.size() IS values.size()) {
         seek *= timing.back(); // In discrete mode the last time doesn't have to be 1.0
         for (i=0; (i < std::ssize(timing)-1) and (timing[i+1] < seek); i++);
         const double delta = timing[i+1] - timing[i];
         seek_to = (seek - timing[i]) / delta;
      }
      else {
         i = std::clamp<LONG>(F2T((values.size()-1) * seek), 0, values.size() - 2);
         // Recompute the seek position to fit between the two values
         const double mod = 1.0 / double(values.size() - 1);
         seek_to = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;
      }

      read_numseq(values[i], { &from_val });
      read_numseq(values[i+1], { &to_val } );
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
   else if (not to.empty()) {
      from_val = Vector.get<double>(Field);
      from = std::to_string(from_val);
      read_numseq(to, { &to_val } );
   }
   else if (not by.empty()) {
      from_val = Vector.get<double>(Field);
      from = std::to_string(from_val);
      read_numseq(by, { &to_val } );
      to_val += from_val;
   }
   else return 0;

   const auto offset = to_val;

   if ((accumulate) and (repeat_count)) {
      // Cumulative animation is not permitted for:
      // * The 'to animation' where 'from' is undefined.

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
// Suitable for <set> instructions only.  Very straight-forward as there is no interpolation.

std::string anim_base::get_string()
{
   if (not to.empty()) return to;
   else return "";
}

//********************************************************************************************************************
// Return an interpolated value based on the values or from/to/by settings.

double anim_base::get_dimension(objVector &Vector, FIELD Field)
{
   double from_val, to_val;
   double seek_to = seek;

   if (not values.empty()) {
      LONG i;

      if (calc_mode IS CMODE::PACED) {
         const auto dist_pos = seek * get_total_dist();
         for (i=0; (i < std::ssize(distances)-1) and (distances[i+1] < dist_pos); i++);
         seek_to = (dist_pos - distances[i]) / (distances[i+1] - distances[i]);
         // keyTiming is not permitted in PACED mode.
      }
      else if (calc_mode IS CMODE::SPLINE) {
         i = 0;
         if (timing.size() IS spline_paths.size()) {
            for (i=0; (i < std::ssize(timing)-1) and (timing[i+1] < seek); i++);
            i = std::clamp<LONG>(i, 0, timing.size() - 1);
         }
         else {
            // When no timing is specified, the 'values' are distributed evenly.  This determines
            // what spline-path we are going to use.

            i = std::clamp<LONG>(F2T(seek * std::ssize(spline_paths)), 0, std::ssize(spline_paths) - 1);
         }

         auto &sp = spline_paths[i]; // sp = The spline we're going to use

         // Rather than use distance, we're going to use the 'x' position as a lookup on the horizontal axis.
         // The paired y value then gives us the 'real' seek_to value.
         // The spline points are already sorted by the x value to make this easier.

         const double x = (seek >= 1.0) ? 1.0 : fmod(seek, 1.0 / double(std::ssize(spline_paths))) * std::ssize(spline_paths);

         LONG si;
         for (si=0; (si < std::ssize(sp.points)-1) and (sp.points[si+1].point.x < x); si++);

         const double mod_x = x - sp.points[si].point.x;
         const double c = mod_x / sp.points[si].cos_angle;
         seek_to = std::clamp(sp.points[si].point.y + std::sqrt((c * c) - (mod_x * mod_x)), 0.0, 1.0);
      }
      else { // CMODE::LINEAR
         if (timing.size() IS values.size()) {
            seek *= timing.back(); // In discrete mode the last time doesn't have to be 1.0

            for (i=0; (i < std::ssize(timing)-1) and (timing[i+1] < seek); i++);
            i = std::clamp<LONG>(i, 0, timing.size() - 2);
            const double delta = timing[i+1] - timing[i];
            seek_to = (seek - timing[i]) / delta;
         }
         else {
            i = std::clamp<LONG>(F2T((values.size()-1) * seek), 0, values.size() - 2);
            const double mod = 1.0 / double(values.size() - 1);
            seek_to = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;
         }
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
   else if (not to.empty()) {
      from_val = Vector.get<double>(Field);
      from = std::to_string(from_val);
      read_numseq(to, { &to_val } );
   }
   else if (not by.empty()) {
      from_val = Vector.get<double>(Field);
      from = std::to_string(from_val);
      read_numseq(by, { &to_val } );
      to_val += from_val;
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

FRGB anim_base::get_colour_value(objVector &Vector, FIELD Field)
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
         float(from_col.Colour.Red   + ((to_col.Colour.Red   - from_col.Colour.Red) * seek)),
         float(from_col.Colour.Green + ((to_col.Colour.Green - from_col.Colour.Green) * seek)),
         float(from_col.Colour.Blue  + ((to_col.Colour.Blue  - from_col.Colour.Blue) * seek)),
         float(from_col.Colour.Alpha + ((to_col.Colour.Alpha - from_col.Colour.Alpha) * seek))
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

   // Start/Reset linked animations
   for (auto &other : start_on_begin) {
      other->activate();
      other->start_time = CurrentTime;
   }

   start_time = CurrentTime;
   return true;
}

//********************************************************************************************************************
// Advance the seek position to represent the next frame.

void anim_base::next_frame(double CurrentTime)
{
   if (end_time) return;

   const double elapsed = CurrentTime - start_time;
   seek = elapsed / duration; // A value between 0 and 1.0

   if (seek >= 1.0) { // Check if the sequence has ended.
      if ((repeat_count < 0) or (repeat_index+1 < repeat_count)) {
         repeat_index++;
         start_time = CurrentTime;
         seek = 0;
         return;
      }
      else stop(CurrentTime); // Setting the end-time will prevent further animation after the completion of this frame.
   }

   // repeat_duration prevents the animation from running past a fixed number of seconds since it started.
   if ((repeat_duration > 0) and (elapsed > repeat_duration)) stop(CurrentTime);
}

//********************************************************************************************************************
// Set common animation properties

static ERR parse_spline(APTR Path, LONG Index, LONG Command, double X, double Y, anim_base::SPLINE_POINTS &Meta)
{
   Meta.emplace_back(pf::POINT<float> { float(X), float(Y) }, 0);

   if (Meta.size() > 1) {
      Meta[Meta.size()-2].angle = std::atan2(Meta.back().point.y - Meta[Meta.size()-2].point.y, Meta.back().point.x - Meta[Meta.size()-2].point.x);
      Meta[Meta.size()-2].cos_angle = std::cos(Meta[Meta.size()-2].angle);
   }

   return ERR::Okay;
}

static ERR set_anim_property(extSVG *Self, anim_base &Anim, XMLTag &Tag, ULONG Hash, const std::string_view Value)
{
   switch (Hash) {
      case SVF_ID:
         Anim.id = Value;
         add_id(Self, Tag, Value);
         break;

      case SVF_HREF:
      case SVF_XLINK_HREF: {
         OBJECTPTR ref_vector;
         if (scFindDef(Self->Scene, Value.data(), &ref_vector) IS ERR::Okay) {
            Anim.target_vector = ref_vector->UID;
         }
         break;
      }

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

         if ("indefinite" IS Value) {
            Anim.begin_offset = std::numeric_limits<double>::max();
            break;
         }

         if (Value.ends_with(".begin")) {
            Anim.begin_offset = std::numeric_limits<double>::max();
            auto ref = Value.substr(0, Value.size()-6);
            for (auto &scan : Self->Animations) {
               std::visit([ &Anim, &ref ](auto &&scan) {
                  if (scan.id IS ref) scan.start_on_begin.emplace_back(&Anim);
               }, scan);
            }
            break;
         }

         if (Value.ends_with(".end")) {
            Anim.begin_offset = std::numeric_limits<double>::max();
            auto ref = Value.substr(0, Value.size()-4);
            for (auto &scan : Self->Animations) {
               std::visit([ &Anim, &ref ](auto &&scan) {
                  if (scan.id IS ref) scan.start_on_end.emplace_back(&Anim);
               }, scan);
            }
            break;
         }

         if ("access-key" IS Value) { // Start the animation when the user presses a key.
            Anim.begin_offset = std::numeric_limits<double>::max();
            break;
         }

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
   Motion.points.push_back(pf::POINT<float> { float(X), float(Y) });
   return ERR::Okay;
};

void anim_motion::perform(extSVG &SVG)
{
   POINT<float> a, b;
   double angle = -1;
   double seek_to = seek;

   if ((end_time) and (!freeze)) return;

   pf::ScopedObjectLock<objVector> vector(target_vector, 1000);
   if (!vector.granted()) return;

   // Note that the order of processing here is important, and matches the priorities documented for SVG's
   // animateMotion property.

   if ((mpath) or (not path.empty())) {
      auto new_timestamp = vector->get<LONG>(FID_PathTimestamp);

      if ((points.empty()) or (path_timestamp != new_timestamp)) {
         // Trace the path and store its points.  Transforms are completely ignored when pulling the path from
         // an external source.

         auto call = C_FUNCTION(motion_callback, this);

         points.clear();
         if (mpath) {
            if ((vecTrace(mpath, &call, vector->get<double>(FID_DisplayScale), false) != ERR::Okay) or (points.empty())) return;
         }
         else if ((vecTrace(*path, &call, 1.0, false) != ERR::Okay) or (points.empty())) return;

         path_timestamp = vector->get<LONG>(FID_PathTimestamp);

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

         a = points[i];
         b = points[i+1];

         seek_to = (dist_pos - distances[i]) / (distances[i+1] - distances[i]);

         if ((auto_rotate IS ART::AUTO) or (auto_rotate IS ART::AUTO_REVERSE)) {
            angle = (angles[i] * (1.0 - seek_to)) + (angles[i+1] * seek_to);
            if (auto_rotate IS ART::AUTO_REVERSE) angle += 180.0;
         }
      }
      else { // CMODE::LINEAR: Interpolate between the two values
         LONG i = F2T((std::ssize(points)-1) * seek);
         if (i >= std::ssize(points)-1) i = std::ssize(points) - 2;

         a = points[i];
         b = points[i+1];

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

      read_numseq(values[i], { &a.x, &a.y });
      read_numseq(values[i+1], { &b.x, &b.y });
   }
   else if (not from.empty()) {
      if (not to.empty()) {
         read_numseq(from, { &a.x, &a.y });
         read_numseq(to, { &b.x, &b.y } );
      }
      else if (not by.empty()) {
         read_numseq(from, { &a.x, &a.y });
         read_numseq(by, { &b.x, &b.y } );
         b.x += a.x;
         b.y += a.y;
      }
      else return;
   }
   else return;

   // Note how the matrix is assigned to the end of the transform list so that it is executed last.  This is a
   // requirement of the SVG standard.  It is important that the matrix is managed independently and not
   // intermixed with other transforms.

   if (not matrix) {
      vecNewMatrix(*vector, &matrix, true);
      matrix->Tag = MTAG_ANIMATE_MOTION;
   }
   vecResetMatrix(matrix);

   if (angle != -1) vecRotate(matrix, angle, 0, 0);
   else if (auto_rotate IS ART::FIXED) vecRotate(matrix, rotate, 0, 0);

   if (calc_mode IS CMODE::DISCRETE) {
      if (seek_to < 0.5) vecTranslate(matrix, a.x, a.y);
      else vecTranslate(matrix, b.x, b.y);
   }
   else { // CMODE::LINEAR
      pf::POINT<double> final { a.x + ((b.x - a.x) * seek_to), a.y + ((b.y - a.y) * seek_to) };
      vecTranslate(matrix, final.x, final.y);
   }
}

//********************************************************************************************************************
// Note: SVG rules state that only one transformation matrix is active at any time, irrespective of however many
// <animateTransform> elements are active for a vector.  Multiple transformations are multiplicative by default.
// If a transform is in REPLACE mode, all prior transforms are overwritten, INCLUDING the vector's 'transform' attribute.

void anim_transform::perform(extSVG &SVG)
{
   double seek_to = seek;

   if ((end_time) and (!freeze)) return;

   pf::ScopedObjectLock<objVector> vector(target_vector, 1000);
   if (vector.granted()) {
      if (not matrix) {
         if (auto im = SVG.Animatrix.find(target_vector); im != SVG.Animatrix.end()) {
            matrix = im->second;
         }
      }

      if (not matrix) {
         vecNewMatrix(*vector, &matrix, false);
         matrix->Tag = MTAG_ANIMATE_TRANSFORM;
         SVG.Animatrix[target_vector] = matrix;
      }

      if (additive IS ADD::REPLACE) {
         // Replace mode is a little tricky if the vector has a transform attribute applied to it.  We want to
         // override the existing transform, but we could cause problems if we were to permanently destroy
         // that information.  The solution we're taking is to create an inversion of the transform declaration
         // in order to undo it.

         VectorMatrix *m = NULL;
         for (m = vector->Matrices; (m); m=m->Next) {
            if (m->Tag IS MTAG_SVG_TRANSFORM) {
               double d = 1.0 / (m->ScaleX * m->ScaleY - m->ShearY * m->ShearX);

               double t0 = m->ScaleY * d;
               matrix->ScaleY = m->ScaleX * d;
               matrix->ShearY = -m->ShearY * d;
               matrix->ShearX = -m->ShearX * d;

               double t4 = -m->TranslateX * t0 - m->TranslateY * matrix->ShearX;
               matrix->TranslateY = -m->TranslateX * matrix->ShearY - m->TranslateY * matrix->ScaleY;

               matrix->ScaleX = t0;
               matrix->TranslateX = t4;
               break;
            }
         }

         if (!m) vecResetMatrix(matrix);
      }
      else {} // In the case of ADD::SUM, we are layering this transform on top of any previously declared animateTransforms

      switch(type) {
         case AT::TRANSLATE: {
            POINT<double> t_from = { 0, 0 }, t_to = { 0, 0 };

            if (not values.empty()) {
               LONG vi = F2T((values.size()-1) * seek);
               if (vi >= std::ssize(values)-1) vi = std::ssize(values) - 2;

               read_numseq(values[vi], { &t_from.x, &t_from.y });
               read_numseq(values[vi+1], { &t_to.x, &t_to.y } );

               const double mod = 1.0 / double(values.size() - 1);
               seek_to = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;
            }
            else if (not from.empty()) {
               read_numseq(from, { &t_from.x, &t_from.y });

               if (not to.empty()) {
                  read_numseq(to, { &t_to.x, &t_to.y } );
               }
               else if (not by.empty()) {
                  read_numseq(by, { &t_to.x, &t_to.y } );
                  t_to.x += t_from.x;
                  t_to.y += t_from.y;
               }
               else break;
            }
            else if (not to.empty()) break; // SVG prohibits the use of a single 'to' value for transforms.
            else if (not by.empty()) { // Placeholder; not correctly implemented
               read_numseq(by, { &t_to.x, &t_to.y } );
               t_from = t_to;
            }
            else break;

            const POINT<double> t_offset = t_to;

            if ((accumulate) and (repeat_count)) {
               const POINT<double> acc = t_offset * repeat_index;
               t_from += acc;
               t_to   += acc;
            }

            const double x = t_from.x + ((t_to.x - t_from.x) * seek_to);
            double y = t_from.y + ((t_to.y - t_from.y) * seek_to);
            vecTranslate(matrix, x, y);
            break;
         }

         case AT::SCALE: {
            POINT<double> t_from = { 0, 0 }, t_to = { 0, 0 };

            if (not values.empty()) {
               LONG vi = F2T((values.size()-1) * seek);
               if (vi >= std::ssize(values)-1) vi = std::ssize(values) - 2;

               read_numseq(values[vi], { &t_from.x, &t_from.y });
               read_numseq(values[vi+1], { &t_to.x, &t_to.y } );

               if (!t_from.y) t_from.y = t_from.x;

               const double mod = 1.0 / double(values.size() - 1);
               seek_to = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;
            }
            else if (not from.empty()) {
               read_numseq(from, { &t_from.x, &t_from.y });
               if (!t_from.y) t_from.y = t_from.x;

               if (not to.empty()) {
                  read_numseq(to, { &t_to.x, &t_to.y } );
               }
               else if (not by.empty()) {
                  read_numseq(by, { &t_to.x, &t_to.y } );
                  t_to.x += t_from.x;
                  t_to.y += t_from.y;
               }
               else break;
            }
            else if (not to.empty()) break; // SVG prohibits the use of a single 'to' value for transforms.
            else if (not by.empty()) { // Placeholder; not correctly implemented
               read_numseq(by, { &t_to.x, &t_to.y } );
               t_from = t_to;
            }
            else break;

            if (!t_to.y) t_to.y = t_to.x;

            const POINT<double> t_offset = t_to;

            if ((accumulate) and (repeat_count)) {
               const POINT<double> acc = t_offset * repeat_index;
               t_from += acc;
               t_to   += acc;
            }

            const double x = t_from.x + ((t_to.x - t_from.x) * seek_to);
            double y = t_from.y + ((t_to.y - t_from.y) * seek_to);
            if (!y) y = x;
            vecScale(matrix, x, y);
            break;
         }

         case AT::ROTATE: {
            ROTATE r_from, r_to;

            if (not values.empty()) {
               LONG vi = F2T((values.size()-1) * seek);
               if (vi >= std::ssize(values)-1) vi = std::ssize(values) - 2;

               read_numseq(values[vi], { &r_from.angle, &r_from.cx, &r_from.cy });
               read_numseq(values[vi+1], { &r_to.angle, &r_to.cx, &r_to.cy } );

               const double mod = 1.0 / double(values.size() - 1);
               seek_to = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;
            }
            else if (not from.empty()) {
               read_numseq(from, { &r_from.angle, &r_from.cx, &r_from.cy });
               if (not to.empty()) {
                  read_numseq(to, { &r_to.angle, &r_to.cx, &r_to.cy } );
               }
               else if (not by.empty()) {
                  read_numseq(by, { &r_to.angle, &r_to.cx, &r_to.cy } );
                  r_to.angle += r_from.angle;
                  r_to.cx += r_from.cx;
                  r_to.cy += r_from.cy;
               }
               else break;
            }
            else if (not to.empty()) break; // SVG prohibits the use of a single 'to' value for transforms.
            else if (not by.empty()) { // Placeholder; not correctly implemented
               read_numseq(by, { &r_to.angle, &r_to.cx, &r_to.cy } );
               r_from = r_to;
            }
            else break;

            const auto r_offset = r_to;

            if ((accumulate) and (repeat_count)) {
               r_from += r_offset * repeat_index;
               r_to   += r_offset * repeat_index;
            }

            const ROTATE r_new = {
               r_from.angle + ((r_to.angle - r_from.angle) * seek_to),
               r_from.cx + ((r_to.cx - r_from.cx) * seek_to),
               r_from.cy + ((r_to.cy - r_from.cy) * seek_to)
            };

            vecRotate(matrix, r_new.angle, r_new.cx, r_new.cy);
            break;
         }

         case AT::SKEW_X: {
            double t_from = 0, t_to = 0;

            if (not values.empty()) {
               LONG vi = F2T((values.size()-1) * seek);
               if (vi >= std::ssize(values)-1) vi = std::ssize(values) - 2;

               read_numseq(values[vi], { &t_from });
               read_numseq(values[vi+1], { &t_to } );

               const double mod = 1.0 / double(values.size() - 1);
               seek_to = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;
            }
            else if (not from.empty()) {
               read_numseq(from, { &t_from });

               if (not to.empty()) {
                  read_numseq(to, { &t_to } );
               }
               else if (not by.empty()) {
                  read_numseq(by, { &t_to } );
                  t_to += t_from;
               }
               else break;
            }
            else if (not to.empty()) break; // SVG prohibits the use of a single 'to' value for transforms.
            else if (not by.empty()) { // Placeholder; not correctly implemented
               read_numseq(by, { &t_to } );
               t_from = t_to;
            }
            else break;

            const double t_offset = t_to;

            if ((accumulate) and (repeat_count)) {
               const double acc = t_offset * repeat_index;
               t_from += acc;
               t_to   += acc;
            }

            const double x = t_from + ((t_to - t_from) * seek_to);
            vecSkew(matrix, x, 0);
            break;
         }

         case AT::SKEW_Y: {
            double t_from = 0, t_to = 0;

            if (not values.empty()) {
               LONG vi = F2T((values.size()-1) * seek);
               if (vi >= std::ssize(values)-1) vi = std::ssize(values) - 2;

               read_numseq(values[vi], { &t_from });
               read_numseq(values[vi+1], { &t_to } );

               const double mod = 1.0 / double(values.size() - 1);
               seek_to = (seek >= 1.0) ? 1.0 : fmod(seek, mod) / mod;
            }
            else if (not from.empty()) {
               read_numseq(from, { &t_from });

               if (not to.empty()) {
                  read_numseq(to, { &t_to } );
               }
               else if (not by.empty()) {
                  read_numseq(by, { &t_to } );
                  t_to += t_from;
               }
               else break;
            }
            else if (not to.empty()) break; // SVG prohibits the use of a single 'to' value for transforms.
            else if (not by.empty()) { // Placeholder; not correctly implemented
               read_numseq(by, { &t_to } );
               t_from = t_to;
            }
            else break;

            const double t_offset = t_to;

            if ((accumulate) and (repeat_count)) {
               const double acc = t_offset * repeat_index;
               t_from += acc;
               t_to   += acc;
            }

            const double y = t_from + ((t_to - t_from) * seek_to);
            vecSkew(matrix, 0, y);
            break;
         }

         default: break;
      }
   }
}

//********************************************************************************************************************
// <rect><animate attributeType="CSS" attributeName="opacity" from="1" to="0" dur="5s" repeatCount="indefinite"/></rect>
// <animate attributeName="font-size" attributeType="CSS" begin="0s" dur="6s" fill="freeze" from="40" to="80"/>
// <animate attributeName="fill" attributeType="CSS" begin="0s" dur="6s" fill="freeze" from="#00f" to="#070"/>

void anim_value::perform(extSVG &SVG)
{
   if ((end_time) and (!freeze)) return;

   pf::ScopedObjectLock<objVector> vector(target_vector, 1000);
   if (vector.granted()) {
      // Determine the type of the attribute that we're targeting, then interpolate the value and set it.

      switch(StrHash(target_attrib)) {
         case SVF_FONT_SIZE: {
            auto val = get_numeric_value(**vector, FID_FontSize);
            vector->set(FID_FontSize, val);
            break;
         }

         case SVF_FILL: {
            auto val = get_colour_value(**vector, FID_FillColour);
            vector->setArray(FID_FillColour, (float *)&val, 4);
            break;
         }

         case SVF_FILL_OPACITY: {
            auto val = get_numeric_value(**vector, FID_FillOpacity);
            vector->set(FID_FillOpacity, val);
            break;
         }

         case SVF_STROKE: {
            auto val = get_colour_value(**vector, FID_StrokeColour);
            vector->setArray(FID_StrokeColour, (float *)&val, 4);
            break;
         }

         case SVF_STROKE_WIDTH:
            vector->set(FID_StrokeWidth, get_numeric_value(**vector, FID_StrokeWidth));
            break;

         case SVF_OPACITY:
            vector->set(FID_Opacity, get_numeric_value(**vector, FID_Opacity));
            break;

         case SVF_CX:
            vector->set(FID_CX, get_dimension(**vector, FID_CX));
            break;

         case SVF_CY:
            vector->set(FID_CY, get_dimension(**vector, FID_CY));
            break;
                    
         case SVF_X1:
            vector->set(FID_X1, get_dimension(**vector, FID_X1));
            break;

         case SVF_Y1:
            vector->set(FID_Y1, get_dimension(**vector, FID_Y1));
            break;

         case SVF_X2:
            vector->set(FID_X2, get_dimension(**vector, FID_X2));
            break;

         case SVF_Y2:
            vector->set(FID_Y2, get_dimension(**vector, FID_Y2));
            break;

         case SVF_X: {
            if (vector->Class->ClassID IS ID_VECTORGROUP) {
               // Special case: SVG groups don't have an (x,y) position, but can declare one in the form of a
               // transform.  Refer to xtag_use() for a working example as to why.

               VectorMatrix *m;
               for (m=vector->Matrices; (m) and (m->Tag != MTAG_SVG_TRANSFORM); m=m->Next);

               if (!m) {
                  vecNewMatrix(*vector, &m, false);
                  m->Tag = MTAG_SVG_TRANSFORM;
               }

               if (m) {
                  m->TranslateX = get_dimension(**vector, FID_X);
                  vecFlushMatrix(m);
               }
            }
            else vector->set(FID_X, get_dimension(**vector, FID_X));
            break;
         }

         case SVF_Y: {
            if (vector->Class->ClassID IS ID_VECTORGROUP) {
               VectorMatrix *m;
               for (m=vector->Matrices; (m) and (m->Tag != MTAG_SVG_TRANSFORM); m=m->Next);

               if (!m) {
                  vecNewMatrix(*vector, &m, false);
                  m->Tag = MTAG_SVG_TRANSFORM;
               }

               if (m) {
                  m->TranslateY = get_dimension(**vector, FID_Y);
                  vecFlushMatrix(m);
               }
            }
            else vector->set(FID_Y, get_dimension(**vector, FID_Y));
            break;
         }

         case SVF_WIDTH:
            vector->set(FID_Width, get_dimension(**vector, FID_Width));
            break;

         case SVF_HEIGHT:
            vector->set(FID_Height, get_dimension(**vector, FID_Height));
            break;

         case SVF_VISIBILITY:
            vector->set(FID_Visibility, get_string());
            break;
      }
   }
}

//********************************************************************************************************************

static ERR animation_timer(extSVG *SVG, LARGE TimeElapsed, LARGE CurrentTime)
{
   pf::Log log(__FUNCTION__);

   if (SVG->Animations.empty()) {
      log.msg("All animations processed, timer suspended.");
      return ERR::Terminate;
   }

   // All transform matrices need to be reset on each cycle.

   for (auto &matrix : SVG->Animatrix) {
      vecResetMatrix(matrix.second);
   }

   for (auto &record : SVG->Animations) {
      std::visit([SVG](auto &&anim) {
         double current_time = double(PreciseTime()) / 1000000.0;

         if (not anim.started(current_time)) return;
         anim.next_frame(current_time);
         anim.perform(*SVG);
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