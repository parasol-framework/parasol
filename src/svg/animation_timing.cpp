
//********************************************************************************************************************

void anim_base::activate(extSVG *SVG, bool Reset)
{ 
   // Reset all the variables that control time management and the animation will start from scratch.
   if (Reset) begin_offset = (double(PreciseTime()) / 1000000.0) - SVG->AnimEpoch;
   repeat_index = 0;
   start_time   = SVG->AnimEpoch + begin_offset;
   end_time     = 0;

   // Test: w3-animate-elem-21-t.svg

   if (auto others = SVG->StartOnBegin.find(hash_id()); others != SVG->StartOnBegin.end()) {
      for (auto &other : others->second) {
         other->activate(SVG, true);
         other->start_time = start_time; // Ensure that times match exactly
      }
   }
}

//********************************************************************************************************************
// Return true if the animation has started.  For absolute consistency, animations start 'at the time they should have 
// started', which we can strictly calculate from begin and duration timing values.

bool anim_base::started(extSVG *SVG, double CurrentTime)
{
   if (end_time) return false;
   if (start_time) return true;
   if (repeat_index > 0) return true;
   if (CurrentTime < SVG->AnimEpoch + begin_offset) return false;
   activate(SVG, false);
   return true;
}

//********************************************************************************************************************
// Advance the seek position to represent the next frame.

bool anim_base::next_frame(double CurrentTime)
{
   if (end_time) return false;

   const double elapsed = CurrentTime - start_time;
   
   if (!duration) seek = 0;
   else seek = elapsed / duration; // A value between 0 and 1.0

   if (seek >= 1.0) { // Check if the sequence has ended.
      if ((repeat_count < 0) or (repeat_index+1 < repeat_count)) {
         repeat_index++;
         start_time = CurrentTime;
         seek = 0;
         return false;
      }
      else {
         if (seek > 1.0) seek = 1.0;
         return true;
      }
   }

   // repeat_duration prevents the animation from running past a fixed number of seconds since it started.
   if ((repeat_duration > 0) and (elapsed > repeat_duration)) return true;

   return false;
}

//********************************************************************************************************************

void anim_base::stop(extSVG *SVG, double Time)
{
   if (!begin_series.empty()) {
      // Check if there's a serialised begin offset following the one that's completed.
      LONG i;
      for (i=0; i < std::ssize(begin_series)-1; i++) {
         if (begin_offset IS begin_series[i]) {
            begin_offset = begin_series[i+1];
            start_time = 0;
            return;
         }
      }
   }

   end_time = Time;
   seek = 1.0; // Necessary in case the seek range calculation has overflowed

   // Start animations that are to be triggered from our ending.
   if (auto others = SVG->StartOnEnd.find(hash_id()); others != SVG->StartOnEnd.end()) {
      for (auto &other : others->second) {
         other->activate(SVG, true);
         other->start_time = Time;
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

   for (auto &matrix : SVG->Animatrix) {
      matrix.second.transforms.clear();
   }

   if (!SVG->AnimEpoch) SVG->AnimEpoch = double(CurrentTime) / 1000000.0;
   double current_time = double(CurrentTime) / 1000000.0;

   for (auto &record : SVG->Animations) {
      std::visit([ SVG, current_time ](auto &&anim) {
         if (not anim.started(SVG, current_time)) return;

         bool stop = anim.next_frame(current_time);
         anim.perform(*SVG);
         if (stop) anim.stop(SVG, current_time);
      }, record);
   }

   // Apply transforms

   for (auto &record : SVG->Animatrix) {
      pf::ScopedObjectLock<objVector> vector(record.first, 1000);
      if (record.second.transforms.empty()) continue;

      auto &vt = record.second;

      // SVG rules state that only one transformation matrix is active at any time, irrespective of however many
      // <animateTransform> elements are active for a vector.  Multiple transformations are multiplicative by default.
      // If a transform is in REPLACE mode, all prior transforms are overwritten, INCLUDING the vector's 'transform' 
      // attribute.

      if (not vt.matrix) {
         vecNewMatrix(*vector, &vt.matrix, false);
         vt.matrix->Tag = MTAG_ANIMATE_TRANSFORM;
      }

      // Replace mode is a little tricky if the vector has a transform attribute applied to it.  We want to
      // override the existing transform, but we could cause problems if we were to permanently destroy
      // that information.  The solution we're taking is to create an inversion of the transform declaration
      // in order to undo it.
      //
      // Tested in: w3-animate-elem-(24|81)-t.svg

      VectorMatrix *m = NULL;
      if (vt.transforms.front()->additive IS ADD::REPLACE) {
         for (m = vector->Matrices; (m); m=m->Next) {
            if (m->Tag IS MTAG_SVG_TRANSFORM) {
               double d = 1.0 / (m->ScaleX * m->ScaleY - m->ShearY * m->ShearX);

               double t0 = m->ScaleY * d;
               vt.matrix->ScaleY = m->ScaleX * d;
               vt.matrix->ShearY = -m->ShearY * d;
               vt.matrix->ShearX = -m->ShearX * d;

               double t4 = -m->TranslateX * t0 - m->TranslateY * vt.matrix->ShearX;
               vt.matrix->TranslateY = -m->TranslateX * vt.matrix->ShearY - m->TranslateY * vt.matrix->ScaleY;

               vt.matrix->ScaleX = t0;
               vt.matrix->TranslateX = t4;
               break;
            }
         }
      }

      if (!m) vecResetMatrix(vt.matrix);

      // Apply the transforms in reverse.

      for (auto t = vt.transforms.rbegin(); t != vt.transforms.rend(); t++) {
         vt.matrix[0] *= t[0]->matrix;
         vecFlushMatrix(vt.matrix);
         if (t[0]->additive IS ADD::REPLACE) break;
      }
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
