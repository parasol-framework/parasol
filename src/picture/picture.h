
class extPicture : public objPicture {
   public:
   STRING   prvPath;
   char     prvAuthor[60];
   char     prvCopyright[80];
   char     prvTitle[50];
   char     prvSoftware[30];
   STRING   prvDescription;
   STRING   prvDisclaimer;
   BYTE     prvHeader[256];
   objFile  *prvFile;
   UBYTE    Cached:1;
   UBYTE    Queried:1;
};
