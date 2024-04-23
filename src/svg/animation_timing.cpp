
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

   for (auto &record : SVG->Animations) {
      std::visit([SVG](auto &&anim) {
         double current_time = double(PreciseTime()) / 1000000.0;

         if (not anim.started(current_time)) return;

         if (anim.next_frame(current_time)) {
            anim.perform(*SVG);
            anim.stop(current_time);
         }
         else anim.perform(*SVG);
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
      // Tested in: w3-animate-elem-24-t.svg

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

      std::for_each(vt.transforms.rbegin(), vt.transforms.rend(), [&](auto t) {
         // In the case of ADD::SUM, we are layering this transform on top of any previously declared animateTransforms
         vt.matrix[0] *= t->matrix;
         //if (t->additive IS ADD::SUM) vt.matrix[0] *= t->matrix;
         //else vt.matrix[0] = t->matrix;
         vecFlushMatrix(vt.matrix);
      });
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
