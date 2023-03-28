
#include "jinclude.h"
#include "jpeglib.h"
#include "jversion.h"
#include "jerror.h"
#include <stdlib.h>

#ifdef USE_WINDOWS_MESSAGEBOX
#include <windows.h>
#endif

#ifndef EXIT_FAILURE		/* define exit() codes if not provided */
#define EXIT_FAILURE  1
#endif

#ifdef NEED_SHORT_EXTERNAL_NAMES
#define jpeg_std_message_table	jMsgTable
#endif

#define JMESSAGE(code,string)	string ,

const char * const jpeg_std_message_table[] = {
#include "jerror.h"
  NULL
};

void error_exit (j_common_ptr cinfo)
{
   (*cinfo->err->output_message) (cinfo);
   jpeg_destroy(cinfo);
   exit(EXIT_FAILURE);
}

/*
 * Actual output of an error or trace message.
 * Applications may override this method to send JPEG messages somewhere
 * other than stderr.
 *
 * On Windows, printing to stderr is generally completely useless,
 * so we provide optional code to produce an error-dialog popup.
 * Most Windows applications will still prefer to override this routine,
 * but if they don't, it'll do something at least marginally useful.
 *
 * NOTE: to use the library in an environment that doesn't support the
 * C stdio library, you may have to delete the call to fprintf() entirely,
 * not just not use this routine.
 */

void output_message (j_common_ptr cinfo)
{

}

/*
 * Decide whether to emit a trace or warning message.
 * msg_level is one of:
 *   -1: recoverable corrupt-data warning, may want to abort.
 *    0: important advisory messages (always display to user).
 *    1: first level of tracing detail.
 *    2,3,...: successively more detailed tracing messages.
 * An application might override this method if it wanted to abort on warnings
 * or change the policy about which messages to display.
 */

void emit_message (j_common_ptr cinfo, int msg_level)
{
   struct jpeg_error_mgr * err = cinfo->err;

   if (msg_level < 0) {
      if (err->num_warnings == 0 || err->trace_level >= 3) (*err->output_message) (cinfo);
      err->num_warnings++;
   }
   else if (err->trace_level >= msg_level) (*err->output_message) (cinfo);
}

void reset_error_mgr (j_common_ptr cinfo)
{
   cinfo->err->num_warnings = 0;
   cinfo->err->msg_code = 0;
}

struct jpeg_error_mgr * jpeg_std_error (struct jpeg_error_mgr * err)
{
  err->error_exit      = error_exit;
  err->emit_message    = emit_message;
  err->output_message  = output_message;
  err->format_message  = NULL;
  err->reset_error_mgr = reset_error_mgr;
  err->trace_level  = 0;      /* default = no tracing */
  err->num_warnings = 0;      /* no warnings emitted yet */
  err->msg_code     = 0;      /* may be useful as a flag for "no error" */
  err->jpeg_message_table  = jpeg_std_message_table;
  err->last_jpeg_message   = (int)JMSG_LASTMSGCODE - 1;
  err->addon_message_table = NULL;
  err->first_addon_message = 0;	/* for safety */
  err->last_addon_message  = 0;
  return err;
}
