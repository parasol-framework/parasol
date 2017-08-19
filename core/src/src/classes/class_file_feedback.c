
//****************************************************************************

#define PRV_FILE
#define CoreBase LocalCoreBase
#include <parasol/modules/widget.h>

extern struct CoreBase *LocalCoreBase;

LONG feedback_delete(struct FileFeedback *Feedback)
{
   objFile *Self = Feedback->User;

   if (Self->TargetID IS -1) return FFR_OKAY;

   if (Self->ProgressDialog) {
      if (((objDialog *)Self->ProgressDialog)->Response IS RSF_CANCEL) return FFR_ABORT;

      if (((objDialog *)Self->ProgressDialog)->Response IS RSF_CLOSED) {
         // If the window was closed then continue deleting files, don't bother the user with further messages.

         acFree(Self->ProgressDialog);
         Self->ProgressDialog = NULL;
         Self->TargetID = -1;
         return FFR_OKAY;
      }
   }

   // If the copy process exceeds a set time period, popup a progress window

   if ((!Self->ProgressDialog) AND (((PreciseTime()/1000LL) - Self->ProgressTime) > 500)) {
      if (!CreateObject(ID_DIALOG, NF_INTEGRAL, (OBJECTPTR *)&Self->ProgressDialog,
            FID_Target|TLONG, Self->TargetID,
            FID_Title|TSTR,   "File Deletion Progress",
            FID_Image|TSTR,   "icons:tools/eraser(48)",
            FID_Buttons|TSTR, "Cancel",
            FID_String|TSTR,  "Deleting...",
            FID_Static|TLONG, TRUE,
            TAGEND)) {

         acShow(Self->ProgressDialog);
      }
   }

   if (Self->ProgressDialog) {
      LONG i;

      STRING str = Feedback->Path;
      for (i=0; str[i]; i++);
      while ((i > 0) AND (str[i-1] != '/') AND (str[i-1] != '\\') AND (str[i-1] != ':')) i--;

      char buffer[256];
      StrFormat(buffer, sizeof(buffer), "Deleting: %s", str+i);
      SetString(Self->ProgressDialog, FID_String, buffer);
      Self->ProgressTime = (PreciseTime()/1000LL);

      ProcessMessages(0, 0);
   }

   return FFR_OKAY;
}
