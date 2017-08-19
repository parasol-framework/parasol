/*
** NOTE: The code in this source file will be executing in a separate thread to the original.
** This can be a major issue, especially with graphics management and drawing.  Where possible,
** please try to offload calls to the Parasol Core via messaging instead of acting on them
** immediately.
*/

static void android_init_window(LONG MsgID)
{
   LogF("~android_init_window()","Display: %d", glActiveDisplayID); // glActiveDisplayID is typically the SystemDisplay (Display class)

   // We want EGL to be initialised in the Parasol Core thread, so we set glEGLState and let
   // lock_graphics() take care of the initialisation.

   glDisplayInfo->DisplayID = 0xffffffff; // Inform that a refresh of the cache is required.
   glEGLState = EGL_REQUIRES_INIT;

   if (glActiveDisplayID) {
      // Send a Show and Draw action to the top-most graphics object of the target display.

      OBJECTID show_id = glActiveDisplayID;
      OBJECTID owner_id = GetOwnerID(show_id);
      if (GetClassID(owner_id) IS ID_SURFACE) {
         show_id = owner_id;
         owner_id = GetOwnerID(owner_id);
         if (GetClassID(owner_id) IS ID_WINDOW) {
            show_id = owner_id;
         }
      }

      DelayMsg(AC_Show, show_id, NULL);
      DelayMsg(AC_Draw, show_id, NULL); // Notify the display that a redraw is required (the host, e.g. Surface, has to be hooked in to this to act on it).
   }

   LogF("android_init_window","Process complete.");
   LogBack();
}

static void android_term_window(LONG MsgID)
{
   LogF("~android_term_window()","");
   free_egl(); // It is OK to terminate EGL in this thread.  Note that this function will do the lock_graphics() for us.
   LogBack();
}

