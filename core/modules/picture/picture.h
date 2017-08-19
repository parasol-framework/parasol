
#define PRV_PICTURE

#define PRV_PICTURE_FIELDS \
   STRING   prvPath; \
   UBYTE    prvAuthor[60]; \
   UBYTE    prvCopyright[80]; \
   UBYTE    prvTitle[50]; \
   UBYTE    prvSoftware[30]; \
   STRING   prvDescription; \
   STRING   prvDisclaimer; \
   BYTE     prvHeader[256]; \
   struct rkFile *prvFile; \
   UBYTE    Cached:1; \
   UBYTE    Queried:1;
