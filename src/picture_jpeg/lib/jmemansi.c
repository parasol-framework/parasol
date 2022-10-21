
#define JPEG_INTERNALS
#include <parasol/main.h>
#include "jinclude.h"
#include "jpeglib.h"
#include "jmemsys.h"		/* import the system-dependent declarations */

extern struct CoreBase *CoreBase;

/*****************************************************************************
** Memory allocation and freeing are controlled by the regular library
** routines malloc() and free().
*/

APTR jpeg_get_small(j_common_ptr cinfo, LONG sizeofobject)
{
   APTR result;
   if (AllocMemory(sizeofobject, MEM_DATA, &result, NULL) IS ERR_Okay) return result;
   else return NULL;
}

void jpeg_free_small(j_common_ptr cinfo, void * object, LONG sizeofobject)
{
   FreeResource(object);
}

APTR jpeg_get_large(j_common_ptr cinfo, LONG sizeofobject)
{
   APTR result;
   if (AllocMemory(sizeofobject, MEM_DATA, &result, NULL) IS ERR_Okay) return result;
   else return NULL;
}

void jpeg_free_large (j_common_ptr cinfo, void FAR * object, LONG sizeofobject)
{
   FreeResource(object);
}

/*****************************************************************************
** This routine computes the total memory space available for allocation.
** It's impossible to do this in a portable way; our current solution is
** to make the user tell us (with a default value set at compile time).
** If you can actually get the available space, it's a good idea to subtract
** a slop factor of 5% or so.
*/

LONG jpeg_mem_available(j_common_ptr cinfo, LONG min_bytes_needed, LONG max_bytes_needed, LONG already_allocated)
{
  return cinfo->mem->max_memory_to_use - already_allocated;
}

/*****************************************************************************
** Backing store (temporary file) management.
** Backing store objects are only used when the value returned by
** jpeg_mem_available is less than the total space needed.  You can dispense
** with these routines if you have plenty of virtual memory; see jmemnobs.c.
*/

void read_backing_store(j_common_ptr cinfo, backing_store_ptr info, void FAR * buffer_address, long file_offset, long byte_count)
{
   struct acRead read;
   struct acSeek seek;

   seek.Offset   = file_offset;
   seek.Position = SEEK_START;
   if (Action(AC_Seek, info->temp_file, &seek) != ERR_Okay) ERREXIT(cinfo, JERR_TFILE_SEEK);

   read.Buffer = buffer_address;
   read.Length = byte_count;
   if (Action(AC_Read, info->temp_file, &read) != ERR_Okay) ERREXIT(cinfo, JERR_TFILE_READ);
}

void write_backing_store(j_common_ptr cinfo, backing_store_ptr info, void FAR * buffer_address, long file_offset, long byte_count)
{
   struct acSeek seek = { .Offset = (DOUBLE)file_offset, .Position = SEEK_START };
   if (Action(AC_Seek, info->temp_file, &seek) != ERR_Okay) ERREXIT(cinfo, JERR_TFILE_SEEK);

   struct acWrite write = { .Buffer = buffer_address, .Length = byte_count };
   if (Action(AC_Write, info->temp_file, &write) != ERR_Okay) ERREXIT(cinfo, JERR_TFILE_WRITE);
}

void close_backing_store(j_common_ptr cinfo, backing_store_ptr info)
{
   Action(AC_Free, info->temp_file, NULL);
   info->temp_file = NULL;
}

/*****************************************************************************
** Initial opening of a backing-store object.
*/

void jpeg_open_backing_store (j_common_ptr cinfo, backing_store_ptr info, long total_bytes_needed)
{
   ERREXIT(cinfo, JERR_TFILE_READ);

   //if ((info->temp_file = tmpfile()) == NULL) ERREXITS(cinfo, JERR_TFILE_CREATE, "");
   //info->read_backing_store = read_backing_store;
   //info->write_backing_store = write_backing_store;
   //info->close_backing_store = close_backing_store;
}

LONG jpeg_mem_init (j_common_ptr cinfo)
{
   return 16777216; /* Return the maximum amount of available memory for libjpeg (16MB) */
}
