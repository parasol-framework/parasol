
/******************************************************************************
** Win32 font structures
*/

struct winFont {
   LONG Offset, Size, Point;
};

struct winmz_header_fields {
   UWORD magic;
   UBYTE data[29 * 2];
   ULONG lfanew;
};

struct winne_header_fields {
   UWORD magic;
   UBYTE data[34];
   UWORD resource_tab_offset;
   UWORD rname_tab_offset;
};

struct winfnt_header_fields {
   UWORD version;
   ULONG file_size;
   char copyright[60];
   UWORD file_type;
   UWORD nominal_point_size;     // Point size
   UWORD vertical_resolution;
   UWORD horizontal_resolution;
   UWORD ascent;                 // The amount of pixels above the base-line
   UWORD internal_leading;       // top leading pixels
   UWORD external_leading;       // gutter
   BYTE  italic;                 // TRUE if font is italic
   BYTE  underline;              // TRUE if font is underlined
   BYTE  strike_out;             // TRUE if font is striked-out
   UWORD weight;                 // Indicates font boldness
   BYTE  charset;
   UWORD pixel_width;
   UWORD pixel_height;
   BYTE  pitch_and_family;
   UWORD avg_width;
   UWORD max_width;
   UBYTE first_char;
   UBYTE last_char;
   UBYTE default_char;
   UBYTE break_char;
   UWORD bytes_per_row;
   ULONG device_offset;
   ULONG face_name_offset;
   ULONG bits_pointer;
   ULONG bits_offset;
   BYTE  reserved;
   ULONG flags;
   UWORD A_space;
   UWORD B_space;
   UWORD C_space;
   UWORD color_table_offset;
   BYTE  reservedend[4];
} __attribute__((packed));

#define ID_WINMZ  0x5A4D
#define ID_WINNE  0x454E

//*****************************************************************************
// Structure definition for cached bitmap fonts.

class BitmapCache {
private:
   UBYTE *mOutline;

public:
   UBYTE *mData;
   winfnt_header_fields Header;
   FontCharacter Chars[256];
   std::string Path;
   WORD OpenCount;
   FTF StyleFlags;
   ERROR Result;

   BitmapCache(winfnt_header_fields &pFace, CSTRING pStyle, CSTRING pPath, objFile *pFile, winFont &pWinFont) {
      pf::Log log(__FUNCTION__);

      log.branch("Caching font %s : %d : %s", pPath, pFace.nominal_point_size, pStyle);

      mData     = NULL;
      mOutline  = NULL;
      OpenCount = 0;
      Result    = ERR_Okay;
      Header    = pFace;

      if (!StrMatch("Bold", pStyle)) StyleFlags = FTF::BOLD;
      else if (!StrMatch("Italic", pStyle)) StyleFlags = FTF::ITALIC;
      else if (!StrMatch("Bold Italic", pStyle)) StyleFlags = FTF::BOLD|FTF::ITALIC;
      else StyleFlags = FTF::NIL;

      Path = pPath;

      // Read character information from the file

      pFile->seek(pWinFont.Offset + 118, SEEK::START);

      ClearMemory(Chars, sizeof(Chars));
      if (pFace.version IS 0x300) {
         LONG j = pFace.first_char;
         for (LONG i=0; i < pFace.last_char - pFace.first_char + 1; i++) {
            UWORD width;
            ULONG offset;

            if (flReadLE(pFile, &width)) break;
            if (flReadLE(pFile, &offset)) break;

            Chars[j].Width   = width;
            Chars[j].Advance = Chars[j].Width;
            Chars[j].Offset  = offset - pFace.bits_offset;
            j++;
         }
      }
      else {
         LONG j = pFace.first_char;
         for (LONG i=0; i < pFace.last_char - pFace.first_char + 1; i++) {
            UWORD width, offset;
            if (flReadLE(pFile, &width)) break;
            if (flReadLE(pFile, &offset)) break;
            Chars[j].Width   = width;
            Chars[j].Advance = Chars[j].Width;
            Chars[j].Offset  = offset - pFace.bits_offset;
            j++;
         }
      }

      LONG size = pFace.file_size - pFace.bits_offset;

      if (!AllocMemory(size, MEM::UNTRACKED, &mData)) {
         LONG result;
         pFile->seek(pWinFont.Offset + pFace.bits_offset, SEEK::START);

         if ((!pFile->read(mData, size, &result)) and (result IS size)) {
            // Convert the graphics format for wide characters from column-first format to row-first format.

            for (WORD i=0; i < 256; i++) {
               if (!Chars[i].Width) continue;

               LONG sz = ((Chars[i].Width+7)>>3) * pFace.pixel_height;
               if (Chars[i].Width > 8) {
                  auto buffer = std::make_unique<UBYTE[]>(sz);
                  ClearMemory(buffer.get(), sz);

                  UBYTE *gfx = mData + Chars[i].Offset;
                  LONG bytewidth = (Chars[i].Width + 7)>>3;
                  LONG pos = 0;
                  for (LONG k=0; k < pFace.pixel_height; k++) {
                     for (LONG j=0; j < bytewidth; j++) {
                        buffer[pos++] = gfx[k + (j * pFace.pixel_height)];
                     }
                  }

                  CopyMemory(buffer.get(), gfx, pos);
               }
            }
         }
         else Result = log.warning(ERR_Read);
      }
      else Result = log.warning(ERR_AllocMemory);

      if (((StyleFlags & FTF::BOLD) != FTF::NIL) and (Header.weight < 600)) {
         log.msg("Converting base font graphics data to bold.");

         LONG size = 0;
         for (LONG i=0; i < 256; i++) {
            if (Chars[i].Width) size += Header.pixel_height * ((Chars[i].Width+8)>>3);
         }

         UBYTE *buffer;
         if (!AllocMemory(size, MEM::UNTRACKED, &buffer)) {
            LONG pos = 0;
            for (LONG i=0; i < 256; i++) {
               if (Chars[i].Width) {
                  UBYTE *gfx = mData + Chars[i].Offset;
                  Chars[i].Offset = pos;

                  // Copy character graphic to the buffer and embolden it

                  LONG oldwidth = (Chars[i].Width+7)>>3;
                  LONG newwidth = (Chars[i].Width+8)>>3;
                  for (LONG y=0; y < Header.pixel_height; y++) {
                     for (LONG xb=0; xb < oldwidth; xb++) {
                        buffer[pos+xb] |= gfx[xb]|(gfx[xb]>>1);
                        if ((xb < newwidth) and (gfx[xb] & 0x01)) buffer[pos+xb+1] |= 0x80;
                     }

                     pos += newwidth;
                     gfx += oldwidth;
                  }

                  Chars[i].Width++;
                  Chars[i].Advance++;
               }
            }

            FreeResource(mData);
            mData = buffer;
         }
         else Result = log.warning(ERR_AllocMemory);
      }

      if (((StyleFlags & FTF::ITALIC) != FTF::NIL) and (!Header.italic)) {
         log.msg("Converting base font graphics data to italic.");

         LONG size = 0;
         LONG extra = Header.pixel_height>>2;

         for (LONG i=0; i < 256; i++) {
            if (Chars[i].Width) size += Header.pixel_height * ((Chars[i].Width+7+extra)>>3);
         }

         UBYTE *buffer;
         if (!AllocMemory(size, MEM::UNTRACKED, &buffer)) {
            LONG pos = 0;
            for (LONG i=0; i < 256; i++) {
               if (Chars[i].Width) {
                  UBYTE *gfx = mData + Chars[i].Offset;
                  Chars[i].Offset = pos;

                  LONG oldwidth = (Chars[i].Width+7)>>3;
                  LONG newwidth = (Chars[i].Width+7+extra)>>3;
                  LONG italic = Header.pixel_height;
                  UBYTE *dest = buffer + pos;
                  for (LONG y=0; y < Header.pixel_height; y++) {
                     LONG dx = italic>>2;
                     for (LONG sx=0; sx < Chars[i].Width; sx++) {
                        if (gfx[sx>>3] & (0x80>>(sx & 0x07))) {
                           dest[dx>>3] |= (0x80>>(dx & 0x07));
                        }
                        dx++;
                     }

                     pos  += newwidth;
                     dest += newwidth;
                     gfx  += oldwidth;
                     italic--;
                  }

                  Chars[i].Width += extra;
               }
            }

            FreeResource(mData);
            mData = buffer;
         }
         else Result = log.warning(ERR_AllocMemory);
      }
   }

   UBYTE * get_outline()
   {
      if (mOutline) return mOutline;

      LONG size = 0;
      for (WORD i=0; i < 256; i++) {
         if (Chars[i].Width) size += (Header.pixel_height+2) * ((Chars[i].Width+9)>>3);
      }

      UBYTE *buffer;
      if (AllocMemory(size, MEM::UNTRACKED, &buffer) != ERR_Okay) return NULL;

      LONG pos = 0;
      for (WORD i=0; i < 256; i++) {
         if (Chars[i].Width) {
            auto gfx = mData + Chars[i].Offset;
            Chars[i].OutlineOffset = pos;

            LONG oldwidth = (Chars[i].Width+7)>>3;
            LONG newwidth = (Chars[i].Width+9)>>3;

            auto dest = buffer + pos;

            dest += newwidth; // Start ahead of line 0
            for (LONG sy=0; sy < Header.pixel_height; sy++) {
               LONG dx = 1;
               for (LONG sx=0; sx < Chars[i].Width; sx++) {
                  if (gfx[sx>>3] & (0x80>>(sx & 0x07))) {
                     if ((sx >= Chars[i].Width-1) or (!(gfx[(sx+1)>>3] & (0x80>>((sx+1) & 0x07))))) dest[(dx+1)>>3] |= (0x80>>((dx+1) & 0x07));
                     if ((sx IS 0) or (!(gfx[(sx-1)>>3] & (0x80>>((sx-1) & 0x07))))) dest[(dx-1)>>3] |= (0x80>>((dx-1) & 0x07));
                     if ((sy < 1) or (!(gfx[(sx>>3)-oldwidth] & (0x80>>(sx & 0x07))))) dest[(dx>>3)-newwidth] |= (0x80>>(dx & 0x07));
                     if ((sy >= Header.pixel_height-1) or (!(gfx[(sx>>3)+oldwidth] & (0x80>>(sx & 0x07))))) dest[(dx>>3)+newwidth] |= (0x80>>(dx & 0x07));
                  }
                  dx++;
               }

               pos  += newwidth;
               dest += newwidth;
               gfx  += oldwidth;
            }
            pos += newwidth * 2;
         }
      }

      mOutline = buffer;
      return mOutline;
   }

   ~BitmapCache() {
      if (OpenCount) {
         pf::Log log(__FUNCTION__);
         log.warning("Removing \"%s : %d : $%.8x\" with an open count of %d", Path.c_str(), Header.nominal_point_size, LONG(StyleFlags), OpenCount);
      }
      if (mData) { FreeResource(mData); mData = NULL; }
      if (mOutline) { FreeResource(mOutline); mOutline = NULL; }
   }
};

static std::list<BitmapCache> glBitmapCache;
static APTR glCacheTimer = NULL;

//********************************************************************************************************************
// Assumes a cache lock is held on being called.

static BitmapCache * check_bitmap_cache(extFont *Self, FTF Style)
{
   pf::Log log(__FUNCTION__);

   for (auto & cache : glBitmapCache) {
      if (cache.Result != ERR_Okay) continue;

      if (!StrMatch(cache.Path.c_str(), Self->Path)) {
         if (cache.StyleFlags IS Style) {
            if (Self->Point IS cache.Header.nominal_point_size) {
               log.trace("Exists in cache (count %d) %s : %s", cache.OpenCount, cache.Path.c_str(), Self->prvStyle);
               return &cache;
            }
            else log.trace("Failed point check %.2f / %d", Self->Point, cache.Header.nominal_point_size);
         }
         else log.trace("Failed style check $%.8x != $%.8x", Style, cache.StyleFlags);
      }
   }

   return NULL;
}

//********************************************************************************************************************

ERROR bitmap_cache_cleaner(OBJECTPTR Subscriber, LARGE Elapsed, LARGE CurrentTime)
{
   pf::Log log(__FUNCTION__);

   log.msg("Checking bitmap font cache for unused fonts...");

   CACHE_LOCK lock(glCacheMutex);
   for (auto it=glBitmapCache.begin(); it != glBitmapCache.end(); ) {
      if (!it->OpenCount) it = glBitmapCache.erase(it);
      else it++;
   }
   glCacheTimer = NULL;
   return ERR_Terminate;
}
