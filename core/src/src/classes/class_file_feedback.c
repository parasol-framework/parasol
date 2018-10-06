
//****************************************************************************

#define PRV_FILE
#define CoreBase LocalCoreBase
#include <parasol/modules/widget.h>

extern struct CoreBase *LocalCoreBase;

const char feedback_script[] = "\
   require 'gui/dialog'\n\
\n\
   local dlg = gui.dialog.message({\n\
      image   = 'icons:tools/eraser',\n\
      title   = 'File Deletion Progress',\n\
      message = 'Deleting...',\n\
      options = { id=1, text='Cancel', icon='items/cancel' },\n\
      feedback = function(Dialog, Response, State)\n\
         if Response then\n\
            if Response.id == 1 then\n\
               obj.find('self')._status = '1'\n\
            end\n\
         else\n\
            obj.find('self')._status = '2'\n\
         end\n\
      end\n\
   })\n\
\n\
function update_msg(Message)\n\
   dlg.message(Message)\n\
end\n";

LONG feedback_delete(struct FileFeedback *Feedback)
{
   objFile *Self = Feedback->User;

   if (Self->TargetID IS -1) return FFR_OKAY;

   if (Self->ProgressDialog) {
      char response[20];
      if (!acGetVar(Self->ProgressDialog, "response", response, sizeof(response))) {
         if (response[0] IS '1') return FFR_ABORT;
         else if (response[0] IS '2') {
            // If the window was closed then continue deleting files, don't bother the user with further messages.

            acFree(Self->ProgressDialog);
            Self->ProgressDialog = NULL;
            Self->TargetID = -1;
            return FFR_OKAY;
         }
      }
   }

   // If the copy process exceeds a set time period, popup a progress window

   if ((!Self->ProgressDialog) AND ((PreciseTime() - Self->ProgressTime) > 500000LL)) {
      if (!CreateObject(ID_SCRIPT, NF_INTEGRAL, (OBJECTPTR *)&Self->ProgressDialog,
            FID_Target|TLONG,   Self->TargetID,
            FID_Statement|TSTR, feedback_script,
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
      struct ScriptArg args[] = { { "Message", FD_STRING, { .Address = buffer } } };
      scExec(Self->ProgressDialog, "update_msg", args, 1);
      Self->ProgressTime = PreciseTime();

      ProcessMessages(0, 0);
   }

   return FFR_OKAY;
}
