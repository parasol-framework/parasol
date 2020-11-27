
EXTERN(void *) jpeg_get_small JPP((j_common_ptr cinfo, LONG sizeofobject));
EXTERN(void) jpeg_free_small JPP((j_common_ptr cinfo, void * object, LONG sizeofobject));
EXTERN(void FAR *) jpeg_get_large JPP((j_common_ptr cinfo, LONG sizeofobject));
EXTERN(void) jpeg_free_large JPP((j_common_ptr cinfo, void FAR * object, LONG sizeofobject));

#ifndef MAX_ALLOC_CHUNK
#define MAX_ALLOC_CHUNK  1000000000L
#endif

LONG jpeg_mem_available JPP((j_common_ptr cinfo, LONG min_bytes_needed, LONG max_bytes_needed, LONG already_allocated));

#define TEMP_NAME_LENGTH   64	/* max length of a temporary file's name */

typedef struct backing_store_struct * backing_store_ptr;

typedef struct backing_store_struct {
   JMETHOD(void, read_backing_store, (j_common_ptr cinfo, backing_store_ptr info, void FAR * buffer_address, long file_offset, long byte_count));
   JMETHOD(void, write_backing_store, (j_common_ptr cinfo, backing_store_ptr info, void FAR * buffer_address, long file_offset, long byte_count));
   JMETHOD(void, close_backing_store, (j_common_ptr cinfo, backing_store_ptr info));
   OBJECTPTR temp_file;
   char temp_name[TEMP_NAME_LENGTH];
} backing_store_info;

void jpeg_open_backing_store JPP((j_common_ptr cinfo, backing_store_ptr info, long total_bytes_needed));
LONG jpeg_mem_init JPP((j_common_ptr cinfo));
