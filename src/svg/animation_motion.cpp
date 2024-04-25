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

//********************************************************************************************************************

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
