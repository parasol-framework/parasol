
class extPicture : public objPicture {
   public:
   std::string prvPath;
   std::string prvAuthor;
   std::string prvCopyright;
   std::string prvTitle;
   std::string prvSoftware;
   std::string prvDescription;
   std::string prvDisclaimer;
   BYTE     prvHeader[256];
   objFile  *prvFile;
   UBYTE    Cached:1;
   UBYTE    Queried:1;
};
