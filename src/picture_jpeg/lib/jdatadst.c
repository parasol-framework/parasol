
#include <parasol/main.h>
#include "jinclude.h"
#include "jpeglib.h"
#include "jerror.h"

extern struct CoreBase *CoreBase;

typedef struct {
   struct jpeg_destination_mgr pub; /* public fields */
   objFile *outfile;		/* target stream */
   JOCTET * buffer;		/* start of buffer */
} my_destination_mgr;

typedef my_destination_mgr * my_dest_ptr;

#define OUTPUT_BUF_SIZE  4096

/*********************************************************************************************************************
** Initialize destination --- called by jpeg_start_compress before any data
** is actually written.
*/

METHODDEF(void) init_destination(j_compress_ptr cinfo)
{
   my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
   dest->buffer = (JOCTET *)(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE, OUTPUT_BUF_SIZE * SIZEOF(JOCTET));
   dest->pub.next_output_byte = dest->buffer;
   dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

/*********************************************************************************************************************
** Empty the output buffer --- called whenever buffer fills up.
**
** In typical applications, this should write the entire output buffer
** (ignoring the current state of next_output_byte & free_in_buffer),
** reset the pointer & count to the start of the buffer, and return TRUE
** indicating that the buffer has been dumped.
**
** In applications that need to be able to suspend compression due to output
** overrun, a FALSE return indicates that the buffer cannot be emptied now.
** In this situation, the compressor will return to its caller (possibly with
** an indication that it has not accepted all the supplied scanlines).  The
** application should resume compression after it has made more room in the
** output buffer.  Note that there are substantial restrictions on the use of
** suspension --- see the documentation.
**
** When suspending, the compressor will back up to a convenient restart point
** (typically the start of the current MCU). next_output_byte & free_in_buffer
** indicate where the restart point will be if the current call returns FALSE.
** Data beyond this point will be regenerated after resumption, so do not
** write it out when emptying the buffer externally.
*/

METHODDEF(boolean) empty_output_buffer (j_compress_ptr cinfo)
{
   my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

   if (acWrite(dest->outfile, dest->buffer, OUTPUT_BUF_SIZE, NULL) != ERR::Okay) ERREXIT(cinfo, JERR_FILE_WRITE);

   dest->pub.next_output_byte = dest->buffer;
   dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
   return TRUE;
}

/*********************************************************************************************************************
** Terminate destination --- called by jpeg_finish_compress after all data has
** been written.  Usually needs to flush buffer.
*/

METHODDEF(void) term_destination(j_compress_ptr cinfo)
{
   struct acWrite write;
   my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
   LONG datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

   if (datacount > 0) {
      write.Buffer = dest->buffer;
      write.Length = datacount;
      if (Action(AC::Write, dest->outfile, &write) != ERR::Okay) ERREXIT(cinfo, JERR_FILE_WRITE);
   }
}

/*********************************************************************************************************************
** Prepare for output to a stdio stream.
** The caller must have already opened the stream, and is responsible
** for closing it after finishing compression.
*/

GLOBAL(void) jpeg_stdio_dest(j_compress_ptr cinfo, objFile *outfile)
{
   my_dest_ptr dest;

   /* The destination object is made permanent so that multiple JPEG images
   ** can be written to the same file without re-executing jpeg_stdio_dest.
   ** This makes it dangerous to use this manager and a different destination
   ** manager serially with the same JPEG object, because their private object
   ** sizes may be different.  Caveat programmer.
   */

   if (cinfo->dest IS NULL) {
      cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT, SIZEOF(my_destination_mgr));
   }

   dest = (my_dest_ptr) cinfo->dest;
   dest->pub.init_destination    = init_destination;
   dest->pub.empty_output_buffer = empty_output_buffer;
   dest->pub.term_destination    = term_destination;
   dest->outfile                 = outfile;
}
