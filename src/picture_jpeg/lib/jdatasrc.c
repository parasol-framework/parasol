/*****************************************************************************
** jdatasrc.c
**
** Copyright (C) 1994-1996, Thomas G. Lane.
** This file is part of the Independent JPEG Group's software.
** For conditions of distribution and use, see the accompanying README file.
**
** This file contains decompression data source routines for the case of
** reading JPEG data from a file (or any stdio stream).  While these routines
** are sufficient for most applications, some will want to use a different
** source manager.
** IMPORTANT: we assume that fread() will correctly transcribe an array of
** JOCTETs from 8-bit-wide elements on external storage.  If char is wider
** than 8 bits on your machine, you may need to do some tweaking.
*/

#include "jinclude.h"
#include "jpeglib.h"
#include "jerror.h"

extern struct CoreBase *CoreBase;

typedef struct {
   struct jpeg_source_mgr pub;	/* public fields */

   objFile *File;		/* source stream */
   JOCTET *buffer;		/* start of buffer */
   boolean start_of_file;	/* have we gotten any data yet? */
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

#define INPUT_BUF_SIZE  4096	/* choose an efficiently fread'able size */

/*****************************************************************************
** Initialize source --- called by jpeg_read_header before any data is
** actually read.
*/

METHODDEF(void) init_source(j_decompress_ptr cinfo)
{
   my_src_ptr src = (my_src_ptr)cinfo->src;
   src->start_of_file = TRUE;
}

/*****************************************************************************
** Fill the input buffer --- called whenever buffer is emptied.
*/

METHODDEF(boolean) fill_input_buffer (j_decompress_ptr cinfo)
{
   LONG result;

   my_src_ptr src = (my_src_ptr) cinfo->src;

   acRead(src->File, src->buffer, INPUT_BUF_SIZE, &result);

   if (result <= 0) {
      if (src->start_of_file) ERREXIT(cinfo, JERR_INPUT_EMPTY);
      WARNMS(cinfo, JWRN_JPEG_EOF);
      src->buffer[0] = (JOCTET)0xFF;
      src->buffer[1] = (JOCTET)JPEG_EOI;
      result = 2;
   }

   src->pub.next_input_byte = src->buffer;
   src->pub.bytes_in_buffer = result;
   src->start_of_file = FALSE;
   return TRUE;
}

/*****************************************************************************
** Skip data --- used to skip over a potentially large amount of
** uninteresting data (such as an APPn marker).
*/

void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
   my_src_ptr src = (my_src_ptr) cinfo->src;

   if (num_bytes > 0) {
      while (num_bytes > (long)src->pub.bytes_in_buffer) {
         num_bytes -= (long)src->pub.bytes_in_buffer;
        (void) fill_input_buffer(cinfo);
      }
      src->pub.next_input_byte += (LONG) num_bytes;
      src->pub.bytes_in_buffer -= (LONG) num_bytes;
   }
}

void term_source(j_decompress_ptr cinfo)
{

}

/*****************************************************************************
** Prepare for input from a stdio stream.
** The caller must have already opened the stream, and is responsible
** for closing it after finishing decompression.
*/

void jpeg_stdio_src (j_decompress_ptr cinfo, objFile *File)
{
   my_src_ptr src;

   if (cinfo->src IS NULL) {
      cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT, SIZEOF(my_source_mgr));
      src = (my_src_ptr) cinfo->src;
      src->buffer = (JOCTET *)(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT, INPUT_BUF_SIZE * SIZEOF(JOCTET));
   }

   src = (my_src_ptr) cinfo->src;
   src->pub.init_source       = init_source;
   src->pub.fill_input_buffer = fill_input_buffer;
   src->pub.skip_input_data   = skip_input_data;
   src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
   src->pub.term_source       = term_source;
   src->File                  = File;
   src->pub.bytes_in_buffer   = 0; /* forces fill_input_buffer on first read */
   src->pub.next_input_byte   = NULL; /* until buffer loaded */
}
