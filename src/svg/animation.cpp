
//****************************************************************************

static ERROR animation_timer(objSVG *SVG, LARGE TimeElapsed, LARGE CurrentTime)
{
   for (auto anim=SVG->Animations; anim; anim=anim->Next) {
      if (anim->ValueCount < 2) continue; // Skip animation if no From and To list is specified.
      if (anim->EndTime) continue;
restart:
      {
         LARGE current_time = PreciseTime() / 1000LL;

         if (!anim->StartTime) {
            // Check if one of the animation's begin triggers has been tripped.  If there are no triggers then the
            // animation can start immediately.

            anim->StartTime = current_time;
            if (!anim->FirstTime) anim->FirstTime = anim->StartTime;
         }

         DOUBLE elapsed = (current_time - anim->StartTime);
         DOUBLE frame = elapsed / (anim->Duration * 1000.0); // A value between 0 and 1.0

         if (frame >= 1.0) { // Check if the sequence has ended.
            anim->RepeatIndex++;
            if ((anim->RepeatCount < 0) OR (anim->RepeatIndex <= anim->RepeatCount)) {
               anim->StartTime = 0;
               goto restart;
            }
            else {
               anim->EndTime = current_time; // Setting the end-time will prevent further animation after the completion of this frame.
               frame = 1.0; // Necessary in case the frame range calculation has overflowed
            }
         }

         // RepeatDuration prevents the animation from running past a fixed number of seconds since it started.
         if ((anim->RepeatDuration > 0) AND ((DOUBLE)(current_time - anim->StartTime) / 1000.0 > anim->RepeatDuration)) {
            anim->EndTime = current_time; // End the animation.
            frame = 1.0;
         }

         LONG vi = F2T((anim->ValueCount-1) * frame);
         if (vi >= anim->ValueCount-1) vi = anim->ValueCount - 2;

         if (anim->Transform) { // Animated transform
            objVector *vector;
            if (!AccessObject(anim->TargetVector, 1000, &vector)) {
               switch(anim->Transform) {
                  case AT_TRANSLATE: break;
                  case AT_SCALE: break;
                  case AT_ROTATE: {
                     DOUBLE from_angle, from_cx, from_cy, to_angle, to_cx, to_cy;
                     read_numseq(anim->Values[vi], &from_angle, &from_cx, &from_cy, TAGEND);
                     read_numseq(anim->Values[vi+1], &to_angle, &to_cx, &to_cy, TAGEND);

                     DOUBLE mod = 1.0 / (DOUBLE)(anim->ValueCount - 1);
                     DOUBLE ratio;
                     if (frame == 1.0) ratio = 1.0;
                     else ratio = fmod(frame, mod) / mod;

                     DOUBLE new_angle = from_angle + ((to_angle - from_angle) * ratio);
                     DOUBLE new_cx    = from_cx + ((to_cx - from_cx) * ratio);
                     DOUBLE new_cy    = from_cy + ((to_cy - from_cy) * ratio);

                     vecRotate(vector, new_angle, new_cx, new_cy);
                     break;
                  }
                  case AT_SKEW_X: break;
                  case AT_SKEW_Y: break;
                  default: break;
               }

               ReleaseObject(vector);
            }
         }
         else { // Animated motion

         }
      }
   }

   if (SVG->FrameCallback.Type != CALL_NONE) {
      if (SVG->FrameCallback.Type IS CALL_STDC) {
         parasol::SwitchContext context(SVG->FrameCallback.StdC.Context);
         auto routine = (void (*)(rkSVG *))SVG->FrameCallback.StdC.Routine;
         routine(SVG);
      }
      else if (SVG->FrameCallback.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = SVG->FrameCallback.Script.Script)) {
            const ScriptArg args[] = {
               { "SVG", FD_OBJECTPTR, { .Address = SVG } }
            };
            scCallback(script, SVG->FrameCallback.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
         }
      }
   }

   return ERR_Okay;
}