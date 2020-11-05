
#define PRV_PICTURE

#define PRV_PICTURE_FIELDS \
   STRING   prvPath; \
   char     prvAuthor[60]; \
   char     prvCopyright[80]; \
   char     prvTitle[50]; \
   char     prvSoftware[30]; \
   STRING   prvDescription; \
   STRING   prvDisclaimer; \
   BYTE     prvHeader[256]; \
   struct rkFile *prvFile; \
   UBYTE    Cached:1; \
   UBYTE    Queried:1;
