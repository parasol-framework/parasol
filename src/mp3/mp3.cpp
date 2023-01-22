/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

MP3: Sound class extension

*********************************************************************************************************************/

#include <parasol/main.h>
#include <parasol/modules/audio.h>
#include <parasol/strings.hpp>

extern "C" {
//#define MINIMP3_NO_SIMD
//#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_IMPLEMENTATION
#include "minimp3/minimp3.h"
}

#include <array>

#define VER_MP3 1.0

MODULE_COREBASE;

static OBJECTPTR modAudio = NULL;
struct AudioBase *AudioBase;

static OBJECTPTR clMP3 = NULL;

const LONG COMMENT_TRACK = 29;

#define MPF_MPEG1     (1<<19)
#define MPF_PAD       (1<<9)
#define MPF_COPYRIGHT (1<<3)
#define MPF_ORIGINAL  (1<<2)

struct prvMP3 {
   UBYTE  Input[16384];        // For incoming MP3 data.  Also needs to be big enough to accomodate the ID3v2 header.
   std::array<UBYTE, 64 * 1024> Buffer; // Decoded audio data
   std::array<UBYTE, 100> TOC; // Xing Table of Contents
   mp3dec_t mp3d;              // Decoder information
   mp3dec_frame_info_t info;   // Retains info on the most recently decoded frame.
   // frame_bytes, frame_offset, channels, hz, layer, bitrate_kbps
   LONG   SamplesPerFrame;     // Last known frame size, measured in samples: 384, 576 or 1152
   LONG   SeekOffset;          // Offset to apply when performing seek operations.
   LONG   WriteOffset;         // Current stream offset in bytes, relative to Sound.Length.
   LONG   ReadOffset;          // Current seek position for the Read() action.  Max value is Sound.Length
   LONG   CompressedOffset;    // Next byte position for reading compressed input
   LONG   FramesProcessed;     // Count of frames processed by the decoder.
   LONG   TotalFrames;         // Total frames for the entire stream (known if CBR data, or VBR header is present).
   LONG   TotalSamples;        // Total samples for the entire stream.  Adjusted for null padding at either end of the stream.
   LONG   PaddingStart;        // Total samples at the start of the decoded stream that can be skipped.
   LONG   PaddingEnd;          // Total samples at the end of the decoded stream that can be ignored.
   LONG   StreamSize;          // Compressed stream length, if defined by VBR header.
   bool   EndOfFile;           // True if all incoming data has been read.
   bool   VBR;                 // True if VBR detected, otherwise CBR
   bool   XingChecked;         // True if Xing header has been checked
   bool   TOCLoaded;           // True if the Table of Contents has been defined.

   void reset() { // Reset the decoder.  Necessary for seeking.
      CompressedOffset = 0;
      ReadOffset       = 0;
      WriteOffset      = 0;
      FramesProcessed  = 0;
      SamplesPerFrame  = 1152;
      EndOfFile        = false;
   }
};

struct id3tag {
   char tag[3];
   char title[30];
   char artist[30];
   char album[30];
   char year[4];
   char comment[30]; // Byte 30 may contain a track number instead of a null terminator
   UBYTE genre;
};

static ERROR decode_mp3(objSound *, bool ProcessHeaders = false);
static LONG find_frame(objSound *, UBYTE *, LONG);

//********************************************************************************************************************

static const std::vector<CSTRING> genre_table = {
   "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
   "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B",
   "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska",
   "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop",
   "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental", "Acid",
   "House", "Game", "Sound Clip", "Gospel", "Noise", "AlternRock", "Bass",
   "Soul", "Punk", "Space", "Meditative", "Instrumental Pop", "Instrumental Rock",
   "Ethnic", "Gothic", "Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk",
   "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult", "Gangsta", "Top 40",
   "Christian Rap", "Pop/Funk", "Jungle", "Native American", "Cabaret", "New Wave",
   "Psychadelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk",
   "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock", "Folk",
   "Folk/Rock", "National folk", "Swing", "Fast-fusion", "Bebob", "Latin",
   "Revival", "Celtic", "Bluegrass", "Avantgarde", "Gothic Rock", "Progressive Rock",
   "Psychedelic Rock", "Symphonic Rock", "Slow Rock", "Big Band", "Chorus",
   "Easy Listening", "Acoustic", "Humour", "Speech", "Chanson", "Opera", "Chamber Music",
   "Sonata", "Symphony", "Booty Bass", "Primus", "Porn Groove", "Satire",
   "Slow Jam", "Club", "Tango", "Samba", "Folklore", "Ballad", "Powder Ballad",
   "Rhythmic Soul", "Freestyle", "Duet", "Punk Rock", "Drum Solo", "A Capella",
   "Euro-House", "Dance Hall", "Goa", "Drum & Bass", "Club House", "Hardcore",
   "Terror", "Indie", "BritPop", "NegerPunk", "Polsk Punk", "Beat",
   "Christian Gangsta", "Heavy Metal", "Black Metal", "Crossover", "Contemporary C",
   "Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop",
   "SynthPop"
};

// Determine the decoded byte length of the entire MP3 sample

static const LONG bitrate_table[5][15] = {
   // MPEG-1
   { 0, 32000,  64000,  96000, 128000, 160000, 192000, 224000, 256000, 288000, 320000, 352000, 384000, 416000, 448000 }, // Layer I
   { 0, 32000,  48000,  56000,  64000,  80000,  96000, 112000, 128000, 160000, 192000, 224000, 256000, 320000, 384000 }, // Layer II
   { 0, 32000,  40000,  48000,  56000,  64000,  80000,  96000, 112000, 128000, 160000, 192000, 224000, 256000, 320000 }, // Layer III
   // MPEG-2 LSF
   { 0, 32000,  48000,  56000,  64000,  80000,  96000, 112000, 128000, 144000, 160000, 176000, 192000, 224000, 256000 }, // Layer I
   { 0,  8000,  16000,  24000,  32000,  40000,  48000,  56000, 64000,  80000,  96000,  112000, 128000, 144000, 160000 }  // Layers II & III
};

static const LONG samplerate_table[3] = { 44100, 48000, 32000 };

static LONG calc_length(objSound *, LONG);

//********************************************************************************************************************
// The ID3v1 tag can be located at the end of the MP3 file.
// There may also be an 'Enhanced TAG' just prior to the ID3v1 header - this code does not support it yet.

static bool parse_id3v1(objSound *Self)
{
   bool processed = false;
   parasol::Log log(__FUNCTION__);

   id3tag id3;
   Self->File->seekEnd(sizeof(id3));

   LONG result;
   if ((!Self->File->read(&id3, sizeof(id3), &result)) and (result IS sizeof(id3))) {
      if (!StrCompare("TAG", (STRING)&id3, 3, STR_CASE)) {
         char buffer[sizeof(id3)];

         log.extmsg("ID3v1 tag found.");

         std::string title(id3.title);
         parasol::ltrim(title, " ");
         SetVar(Self, "Title", title.c_str());

         std::string artist(id3.artist);
         parasol::ltrim(artist, " ");
         SetVar(Self, "Author", artist.c_str());

         std::string album(id3.album);
         parasol::ltrim(album, " ");
         SetVar(Self, "Album", album.c_str());

         std::string comment(id3.comment);
         parasol::ltrim(comment, " ");
         SetVar(Self, "Description", comment.c_str());

         if (id3.genre <= genre_table.size()) {
            SetVar(Self, "Genre", genre_table[id3.genre]);
         }
         else SetVar(Self, "Genre", "Unknown");

         if (id3.comment[COMMENT_TRACK] > 0) {
            IntToStr(id3.comment[COMMENT_TRACK], buffer, sizeof(buffer));
            SetVar(Self, "Track", buffer);
         }

         processed = true;
      }
   }

   Self->File->seekStart(0);

   return processed;
}

//********************************************************************************************************************

static LONG detect_id3v2(const char *Buffer)
{
   if (!StrCompare(Buffer, "ID3", 3, STR_CASE)) {
      if (!((Buffer[5] & 15) or (Buffer[6] & 0x80) or (Buffer[7] & 0x80) or (Buffer[8] & 0x80) or (Buffer[9] & 0x80))) {
         LONG id3v2size = (((Buffer[6] & 0x7f) << 21) | ((Buffer[7] & 0x7f) << 14) | ((Buffer[8] & 0x7f) << 7) | (Buffer[9] & 0x7f)) + 10;
         if ((Buffer[5] & 16)) id3v2size += 10; // footer
         return id3v2size;
      }
   }

   return 0;
}

//********************************************************************************************************************
// Check for the Xing/Info tag.  Ideally this is always present for VBR files, and is also useful for CBR files.

const LONG XING_FRAMES       = 1; // Total number of frames is defined.
const LONG XING_STREAM_SIZE  = 2; // The compressed audio stream size in bytes is defined.  Excludes ID3vX, Xing etc.
const LONG XING_TOC          = 4; // TOC entries are defined.
const LONG XING_SCALE        = 8; // VBR quality is indicated from 0 (best) to 100 (worst).

static int check_xing(objSound *Self, const UBYTE *Frame)
{
   parasol::Log log;

   auto prv = (prvMP3 *)Self->ChildPrivate;

   if (prv->XingChecked) return 1;
   else prv->XingChecked = true;

   bs_t bs[1];
   L3_gr_info_t gr_info[4];
   bs_init(bs, Frame + HDR_SIZE, prv->info.frame_bytes - HDR_SIZE);
   if (HDR_IS_CRC(Frame)) get_bits(bs, 16);
   if (L3_read_side_info(bs, gr_info, Frame) < 0) return 0; // side info corrupted

   const UBYTE *tag = Frame + HDR_SIZE + bs->pos / 8;
   if ((!StrCompare("Xing", (CSTRING)tag, 4, STR_CASE)) and (!StrCompare("Info", (CSTRING)tag, 4, STR_CASE))) return 0;
   const LONG flags = tag[7];
   if (!(flags & XING_FRAMES)) return -1;
   tag += 8;

   prv->TotalFrames = LONG((tag[0] << 24) | (tag[1] << 16) | (tag[2] << 8) | tag[3]);
   //prv->TotalFrames--; // The VBR frame doesn't count as audio data.
   tag += 4;

   if (flags & XING_STREAM_SIZE) {
      // This value is used for TOC seek calculations.
      prv->StreamSize = LONG((tag[0] << 24) | (tag[1] << 16) | (tag[2] << 8) | tag[3]);
      tag += 4;
   }

   if (flags & XING_TOC) {
      CopyMemory(tag, prv->TOC.data(), 100);
      tag += 100;
      prv->TOCLoaded = true;
   }

   if (flags & XING_SCALE) {
      LONG quality = LONG((tag[0] << 24) | (tag[1] << 16) | (tag[2] << 8) | tag[3]);
      acSetVar(Self, "Quality", std::to_string(quality).c_str());
      tag += 4;
   }

   LONG delay = 0; // Typically the first 528 samples are padding set to zero and can be skipped.
   LONG padding = 0; // Padding tells you the number of samples at the end of the file that are empty.

   if (*tag) { // Optional extension, LAME, Lavc, etc. Should be the same structure.
       tag += 21;
       if (tag - Frame + 14 >= prv->info.frame_bytes);
       else {
          delay   = ((tag[0] << 4) | (tag[1] >> 4)) + (528 + 1);
          padding = (((tag[1] & 0xF) << 8) | tag[2]) - (528 + 1);
       }
   }

   prv->PaddingEnd   = padding;
   prv->PaddingStart = delay;

   // Calculate the total no of samples for the entire streaam.
   LARGE detected_samples = prv->info.samples * (LARGE)prv->TotalFrames;
   if (detected_samples >= prv->PaddingStart) detected_samples -= prv->PaddingStart;
   if (detected_samples >= prv->PaddingEnd) detected_samples -= prv->PaddingEnd;

   prv->TotalSamples = detected_samples;

   const DOUBLE seconds_len = DOUBLE(detected_samples) / DOUBLE(prv->info.hz);

   LONG len = prv->TotalFrames * prv->SamplesPerFrame * prv->info.channels * sizeof(WORD);
   len -= prv->PaddingEnd * prv->info.channels * sizeof(WORD);
   Self->set(FID_Length, len);

   log.msg("Info header detected.  Total Frames: %d, Samples: %d, Track Time: %.2fs, Byte Length: %d, Padding: %d/%d", prv->TotalFrames, prv->TotalSamples, seconds_len, len, prv->PaddingStart, prv->PaddingEnd);

   return 1;
}

//********************************************************************************************************************
// ID3v2 is located at the start of the file.  This can be followed by a Xing VBR header.

static void parse_id3v2(objSound *Self)
{

}

//********************************************************************************************************************

static ERROR MP3_Free(objSound *Self, APTR Void)
{
   prvMP3 *prv;
   if (!(prv = (prvMP3 *)Self->ChildPrivate)) return ERR_Okay;

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR MP3_Init(objSound *Self, APTR Void)
{
   parasol::Log log;

   STRING location;
   Self->get(FID_Path, &location);

   if ((!location) or (Self->Flags & SDF_NEW)) {
      // If no location has been specified, assume that the sound is being
      // created from scratch (e.g. to record an mp3 file to disk).

      return ERR_Okay;
   }

   // Open the source file if the Sound class has not done so already

   if (!Self->File) {
      if (!(Self->File = objFile::create::integral(fl::Path(location), fl::Flags(FL_READ|FL_APPROXIMATE)))) {
         return log.warning(ERR_CreateObject);
      }
   }
   else Self->File->seekStart(0);

   // Read the ID3v1 file header, if there is one.

   LONG reduce = 0;
   if (parse_id3v1(Self)) reduce += sizeof(struct id3tag);

   prvMP3 *prv;
   if (!AllocMemory(sizeof(prvMP3), MEM_DATA, &Self->ChildPrivate)) {
      prv = (prvMP3 *)Self->ChildPrivate;
      new (prv) prvMP3;
   }
   else return ERR_AllocMemory;

   mp3dec_init(&prv->mp3d);
   prv->reset();

   // Fill the buffer with audio information and parse any MP3 header that is present.  This will also prove whether
   // or not this is really an mp3 file.

   Self->BufferLength = prv->Buffer.size(); // User-defined buffer sizes are not supported.
   if (decode_mp3(Self, true)) {
      FreeResource(Self->ChildPrivate); Self->ChildPrivate = NULL;
      return ERR_NoSupport;
   }

   if (prv->info.channels IS 2) Self->Flags |= SDF_STEREO;

   if (Self->Flags & SDF_STEREO) Self->BytesPerSecond = prv->info.hz * 4;
   else Self->BytesPerSecond = prv->info.hz * 2;

   Self->Flags         |= SDF_STREAM;
   Self->BitsPerSample  = 16;
   Self->Frequency      = prv->info.hz;
   Self->Playback       = Self->Frequency;
   Self->Length         = calc_length(Self, reduce);

   log.msg("File is MP3.  Stereo: %c, BytesPerSecond: %d, Freq: %d, Byte Length: %d, Buffer Size: %d", Self->Flags & SDF_STEREO ? 'Y' : 'N', Self->BytesPerSecond, Self->Frequency, Self->Length, Self->BufferLength);

   // If all we are doing is querying the source file, return immediately

   if (Self->Flags & SDF_QUERY) return ERR_Okay;

   // Add our mp3 audio stream to the SystemAudio object

   struct sndAddStream addstream;
   AudioLoop loop;

   if (Self->Flags & SDF_LOOP) {
      ClearMemory(&loop, sizeof(loop));
      loop.LoopMode   = LOOP::SINGLE;
      loop.Loop1Type  = LTYPE::UNIDIRECTIONAL;
      loop.Loop1Start = Self->LoopStart;
      if (Self->LoopEnd) loop.Loop1End = Self->LoopEnd;
      else loop.Loop1End = Self->Length;

      addstream.Loop     = &loop;
      addstream.LoopSize = sizeof(struct AudioLoop);
   }
   else {
      addstream.Loop     = 0;
      addstream.LoopSize = 0;
   }

   addstream.Path      = 0;
   addstream.ObjectID  = Self->UID;
   addstream.SeekStart = 0;

   if (Self->Flags & SDF_STEREO) addstream.SampleFormat = SFM_S16_BIT_STEREO;
   else addstream.SampleFormat = SFM_S16_BIT_MONO;

   addstream.SampleLength = (Self->Length > 0) ? Self->Length : -1;
   addstream.BufferLength = Self->BufferLength;

   if (!ActionMsg(MT_SndAddStream, Self->AudioID, &addstream)) {
      Self->Handle = addstream.Result;
      return ERR_Okay;
   }
   else {
      log.warning("Failed to add sample to the Audio device.");
      return ERR_Failed;
   }
}

//********************************************************************************************************************

static ERROR MP3_Read(objSound *Self, struct acRead *Args)
{
   parasol::Log log;
   auto prv = (prvMP3 *)Self->ChildPrivate;

   if (!Args) return log.warning(ERR_NullArgs);

   decode_mp3(Self); // Generate as much decoded output as possible.

   LONG read_max = Args->Length;
   auto write_to = (UBYTE *)Args->Buffer;

   if (prv->ReadOffset + read_max > prv->WriteOffset) {
      // We can't read more than what is already decoded.
      read_max = prv->WriteOffset - prv->ReadOffset;
   }

   const LONG read_offset = prv->ReadOffset % Self->BufferLength;

   if (read_offset + read_max <= (LONG)Self->BufferLength) {
      CopyMemory(prv->Buffer.data() + read_offset, write_to, read_max);
   }
   else {
      const LONG read_tail = Self->BufferLength - read_offset;
      CopyMemory(prv->Buffer.data() + read_offset, write_to, read_tail);
      CopyMemory(prv->Buffer.data(), write_to + read_tail, read_max - read_tail);
   }

   prv->ReadOffset += read_max;
   Args->Result = read_max;
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR MP3_SaveToObject(objSound *Self, struct acSaveToObject *Args)
{
   return ERR_NoSupport;
}

//********************************************************************************************************************

static ERROR MP3_Seek(objSound *Self, struct acSeek *Args)
{
   parasol::Log log;
   auto prv = (prvMP3 *)Self->ChildPrivate;

   prv->reset();
   mp3dec_init(&prv->mp3d);

   if ((Args->Offset IS 0) and (Args->Position IS SEEK_START)) {
      log.traceBranch("Resetting play position to zero.");
      Self->File->seekStart(prv->SeekOffset);
      decode_mp3(Self);
      return ERR_Okay;
   }
   else {
      // Seeking via byte position, where the position is relative to the decoded length.

      if (Self->Length <= 0) {
         log.warning("MP3 stream length unknown, cannot seek.");
         return ERR_Failed;
      }

      DOUBLE pct;
      if (Args->Position IS SEEK_START) pct = Args->Offset / DOUBLE(Self->Length);
      else if (Args->Position IS SEEK_END) pct = (Self->Length - Args->Offset) / DOUBLE(Self->Length);
      else if (Args->Position IS SEEK_CURRENT) pct = (prv->WriteOffset + Args->Offset) / DOUBLE(Self->Length);
      else if (Args->Position IS SEEK_RELATIVE) pct = Args->Offset;
      else return ERR_Args;

      if (pct < 0) pct = 0;
      else if (pct > 1.0) pct = 1.0;

      if (prv->TOCLoaded) {
         // The TOC gives us an approx. frame number for a given location in the compressed stream
         // (relative to TotalFrames).  By knowing the frame number, we can make more accurate calculations
         // as to time and length remaining.

         LONG idx = F2T(pct * prv->TOC.size());
         if (idx < 0) idx = 0;
         else if (idx >= (LONG)prv->TOC.size()) idx = prv->TOC.size() - 1;

         LONG offset = prv->SeekOffset + ((prv->TOC[idx] * prv->StreamSize) / 256);
         LONG frame = prv->TOC[idx] * prv->TotalFrames / 256;
         Self->File->seekStart(offset);

         log.extmsg("Seeking to byte offset %d, frame %d of %d", offset, frame, prv->TotalFrames);

         prv->WriteOffset = frame * prv->SamplesPerFrame * prv->info.channels * sizeof(WORD);
         prv->ReadOffset = prv->WriteOffset;
         prv->FramesProcessed = frame;

         decode_mp3(Self);
         return ERR_Okay;
      }
      else {
         // Seeking without a TOC has two approaches: 1) Scan from frame 1 until you reach the frame
         // you're looking for; 2) Use the average frame size to make a jump and rely on frame syncing to
         // find the nearest viable frame.  The accuracy of this is largely dependent on the calc_length()
         // computations.

         if (!prv->StreamSize) {
            LARGE size;
            Self->File->get(FID_Size, &size);
            prv->StreamSize = size - prv->SeekOffset;
         }

         LONG frame = prv->TotalFrames * pct;
         LONG offset = prv->StreamSize * pct;
         Self->File->seekStart(prv->SeekOffset + offset);

         log.extmsg("Seeking to byte offset %d, frame %d of %d", offset, frame, prv->TotalFrames);

         prv->WriteOffset = frame * prv->SamplesPerFrame * prv->info.channels * sizeof(WORD);
         prv->ReadOffset = prv->WriteOffset;
         prv->FramesProcessed = frame;

         decode_mp3(Self);
         return ERR_Okay;
      }
   }


   return ERR_NoSupport;
}

//********************************************************************************************************************
// Write as much data as possible to the internal decoding buffer, reading from the compressed MP3 data.

static ERROR decode_mp3(objSound *Self, bool ProcessHeaders)
{
   parasol::Log log(__FUNCTION__);

   auto prv = (prvMP3 *)Self->ChildPrivate;

   if (prv->EndOfFile) return ERR_Okay;

   LARGE output_limit = prv->ReadOffset + Self->BufferLength - (8 * 1024); // The limit is determined by the client's read position.

   // Keep decoding until we exhaust space in the output buffer.

   WORD pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];

   bool no_more_input = false;
   while ((!prv->EndOfFile) and (prv->WriteOffset < output_limit)) {
      // Read as much input as possible.

      log.trace("Write %" PF64 " max bytes to %d, Avail. Compressed: %d bytes",  output_limit - prv->WriteOffset, prv->WriteOffset, prv->CompressedOffset);

      if ((prv->CompressedOffset < (LONG)sizeof(prv->Input)) and (!prv->EndOfFile)) {
         LONG result;
         ERROR error = Self->File->read(prv->Input + prv->CompressedOffset, sizeof(prv->Input) - prv->CompressedOffset, &result);

         if (error != ERR_Okay) {
            log.warning("File read error: %s", GetErrorMsg(error));
            prv->EndOfFile = true;
            break;
         }
         else if (!result) {
            log.extmsg("Reached end of input file.");
            no_more_input = true;
            // Don't change the EOF - let the output code do that.
         }

         prv->CompressedOffset += result;
      }

      LONG in = 0; // Always start from zero

      if (ProcessHeaders) {
         // Process ID3V2 and Xing VBR headers if present.

         if (auto id3size = detect_id3v2((const char *)prv->Input)) {
            log.msg("Detected ID3v2 header of %d bytes.", id3size);
            if (id3size < prv->CompressedOffset) {
               in += id3size;
            }
            else { // Header too large for our buffer.
               Self->File->seekStart(id3size);
               LONG result;
               if (!Self->File->read(prv->Input, sizeof(prv->Input), &result)) {
                  prv->CompressedOffset = result;
               }
               else {
                  prv->EndOfFile = true;
                  break;
               }
            }
            prv->SeekOffset = id3size;
         }
         else {
            log.msg("No ID3v2 header found.");
            prv->SeekOffset = 0;
         }

         if (find_frame(Self, prv->Input + in, prv->CompressedOffset - in) != -1) {
            if (check_xing(Self, prv->Input + in)) {
               in += prv->info.frame_bytes;
               prv->SeekOffset += prv->info.frame_bytes;
            }
            else log.extmsg("No VBR header found.");
         }

         ProcessHeaders = false;
      }

      while (prv->WriteOffset < output_limit) {
         auto decoded_samples = mp3dec_decode_frame(&prv->mp3d, prv->Input + in, prv->CompressedOffset - in, pcm, &prv->info);

         // Decoder results:
         // 0: No MP3 data found; 384: Layer 1; 576: Layer 3; 1152: All others

         if (!decoded_samples) {
            if (prv->info.frame_bytes > 0) {
               // The decoder skipped ID3 or invalid data - do not play this frame
               log.msg("Skipping MP3 frame at offset %d, size %d.", in, prv->CompressedOffset - in);
               in += prv->info.frame_bytes;
            }
            else if (!prv->info.frame_bytes) {
               // Insufficient data (read more to obtain a frame) OR end of file
               if ((!in) or (no_more_input)) prv->EndOfFile = true;
               break;
            }
         }
         else {
            prv->FramesProcessed++;
            prv->SamplesPerFrame = decoded_samples;

            LONG decoded_bytes = decoded_samples * sizeof(WORD) * prv->info.channels;

            in += prv->info.frame_bytes;

            const LONG target = prv->WriteOffset % Self->BufferLength;

            if (target + decoded_bytes < Self->BufferLength) {
               CopyMemory(pcm, prv->Buffer.data() + target, decoded_bytes);
            }
            else {
               LONG tail_size = Self->BufferLength - target;
               CopyMemory(pcm, prv->Buffer.data() + target, tail_size);
               CopyMemory((UBYTE *)pcm + tail_size, prv->Buffer.data(), decoded_bytes - tail_size);
            }

            prv->WriteOffset += decoded_bytes;
         }
      }

      // Shift any remaining data that couldn't be decoded.  This will help maintain the minimum 16k of
      // data in the buffer as recommended by minimp3.

      if (in < prv->CompressedOffset) {
         CopyMemory((UBYTE *)prv->Input + in, prv->Input, prv->CompressedOffset - in);
      }

      prv->CompressedOffset -= in;
   }

   if (prv->EndOfFile) {
      // We now know the exact length of the decoded audio stream, so we pass that information to the Audio
      // object.  This will cause the Audio server to stop the stream at the correct position.  The Sound
      // class will remove the stream from the system once the sound is stopped.

      LONG pad_end = prv->PaddingEnd * prv->info.channels * sizeof(WORD);
      if (Self->Length != prv->WriteOffset - pad_end) {
         log.msg("Changing sample length from %d to %d bytes.  Decoded %d frames.", Self->Length, prv->WriteOffset - pad_end, prv->FramesProcessed);
         Self->set(FID_Length, prv->WriteOffset - pad_end);
      }
      else log.msg("Stream ending, decoded %d frames.", prv->FramesProcessed);
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// Calculate the approximate decoded length of an MP3 audio stream.  This will normally be unnecessary if the stream
// has defined a Xing header.

#define SIZE_BUFFER     256000  // Load up to this many bytes to determine if the file is in variable bit-rate
#define SIZE_CBR_BUFFER 51200   // Load at least this many bytes to determine if the file is in constant bit-rate

static LONG calc_length(objSound *Self, LONG ReduceEnd)
{
   LONG buffer_size;
   std::array<UWORD, 16> avg;  // Used to compute the interquartile mean
   std::vector<UWORD> fsizes;  // List of all compressed frame sizes

   if (Self->Length > 0) return Self->Length;

   parasol::Log log(__FUNCTION__);
   log.branch();

   auto prv = (prvMP3 *)Self->ChildPrivate;

   avg.fill(0);

   LONG frame_start     = 0;
   LONG current_bitrate = 0;
   LONG frame_size      = 0;
   LONG channels        = 1;
   LONG frame_samples   = 1152;
   BYTE layer = 0;
   prv->VBR = false;

   LONG filesize;
   Self->File->get(FID_Size, &filesize);

   UBYTE *buffer;
   if (!AllocMemory(SIZE_BUFFER, MEM_DATA|MEM_NO_CLEAR, &buffer, NULL)) {
      // Load MP3 data from the file

      Self->File->seekStart(prv->SeekOffset);
      Self->File->read(buffer, SIZE_CBR_BUFFER, &buffer_size);

      // Find the start of the frame data

      frame_start = find_frame(Self, buffer, buffer_size);

      if (frame_start IS -1) {
         log.warning("Failed to find the first mp3 frame.");
         FreeResource(buffer);
         return -1;
      }

      LONG pos = frame_start;
      while (pos < buffer_size - 8) {
         // MP3 frame information consists of a single 32-bit header
         LONG frame = (buffer[pos]<<24) | (buffer[pos+1]<<16) | (buffer[pos+2]<<8) | buffer[pos+3];

         bool invalid;
         if ((frame & 0xffe00000) IS 0xffe00000) {
            layer = 4 - ((frame & ((1<<17)|(1<<18)))>>17);
            LONG samplerate = samplerate_table[(frame & 0x0c00)>>10];
            if ((layer > 3) or (!samplerate)) {
               pos++;
               continue;
            }

            LONG index = (frame & 0x0000f000)>>12;
            frame_samples = hdr_frame_samples(buffer);

            LONG bitrate;
            if (frame & MPF_MPEG1) bitrate = bitrate_table[layer - 1][index]; // MPEG-1
            else bitrate = bitrate_table[3 + (layer >> 1)][index]; // MPEG-2

            if (!current_bitrate) current_bitrate = bitrate;
            else if (current_bitrate != bitrate) prv->VBR = true;

            BYTE pad = (frame & MPF_PAD) ? 1 : 0;
            channels = HDR_IS_MONO(buffer) ? 1 : 2;

            if (layer IS 1) frame_size = ((12 * bitrate / samplerate) + pad) * 4;
            else frame_size = (144 * bitrate / samplerate) + pad;

            if (frame_size <= 0) { // In case file lacks integrity.  Frame must be at least 1 byte
               pos++;
               continue;
            }

            fsizes.push_back(frame_size);
            pos += frame_size;
            avg[index] = avg[index] + 1;
            invalid = false;
         }
         else invalid = true;

         // Check if we need to load more data into our buffer for VBR analysis

         if (pos >= buffer_size - 8) {
            if ((prv->VBR) and (buffer_size IS SIZE_CBR_BUFFER)) {
               // Read more file data so that we can calculate the vbr more accurately
               LONG result;
               Self->File->read(buffer + buffer_size, SIZE_BUFFER - buffer_size, &result);
               buffer_size += result;
            }
            else break; // File is CBR, no need to scan more data
         }

         // Check that the frame is valid

         if (invalid) {
            LONG index = find_frame(Self, buffer + pos, buffer_size - pos);
            if ((index IS -1) or (!index)) {
               log.msg("Failed to find the next frame at position %d.", pos);
               break;
            }
            else pos += index;
         }
      }

      FreeResource(buffer);
   }
   else return -1;

   if (fsizes.empty()) return -1;

   // Calculate average frame length using interquartile mean

   sort(fsizes.begin(), fsizes.end(), std::greater<UWORD>());
   const LONG first = fsizes.size() / 4;
   const LONG last  = F2T(fsizes.size() * 0.75);
   DOUBLE avg_frame_len = 0;
   for (LONG i=first; i < last; i++) avg_frame_len += fsizes[i];
   avg_frame_len /= (last - first);

   log.extmsg("File Size: %d, %d frames, Average frame length: %.2f bytes, VBR: %c", filesize, (LONG)fsizes.size(), avg_frame_len, prv->VBR ? 'Y' : 'N');

   if (filesize > buffer_size) {
      if (prv->VBR) {
         prv->TotalFrames = F2T((filesize - prv->SeekOffset - frame_start - ReduceEnd) / avg_frame_len);
         return prv->TotalFrames * frame_samples * channels * sizeof(WORD);
      }
      else {
         // For CBR we guess the total frames from the file size.
         Self->File->get(FID_Size, &filesize);
         LONG total_frames = F2T((filesize - prv->SeekOffset - frame_start - ReduceEnd) / avg_frame_len);
         DOUBLE seconds = (total_frames * (DOUBLE)avg_frame_len) / (DOUBLE(current_bitrate) / 1000.0 * 125.0);
         prv->TotalFrames = total_frames;
         return seconds * Self->BytesPerSecond;
      }
   }
   else if (fsizes.size() > 0) { // The entire file was loaded into the buffer, so we know the exact length.
      return fsizes.size() * frame_size * channels * sizeof(WORD);
   }
   else { // File has no detectable MP3 audio content
      return -1;
   }
}

//********************************************************************************************************************

static LONG find_frame(objSound *Self, UBYTE *Buffer, LONG BufferSize)
{
   parasol::Log log(__FUNCTION__);
   LONG bitrate, frame_size;
   auto prv = (prvMP3 *)Self->ChildPrivate;

   log.traceBranch("Buffer Size: %d", BufferSize);

   for (LONG pos=0; (pos < BufferSize-8); pos++) {
      if (Buffer[pos] IS 0xff) {
         LONG frame = (Buffer[pos]<<24) | (Buffer[pos+1]<<16) | (Buffer[pos+2]<<8) | Buffer[pos+3];
         if ((frame & 0xffe00000) IS 0xffe00000) {
            // Frame sync found.  Check its validity by looking for a following frame.

            LONG layer = 4 - ((frame & ((1<<17)|(1<<18)))>>17);
            LONG index = (frame & 0x0c00)>>10;
            if (index >= ARRAYSIZE(samplerate_table)) continue;
            LONG samplerate = samplerate_table[index];
            if ((layer < 0) or (layer > 3) or (!samplerate)) continue;

            index = (frame & 0x0000f000)>>12;

            if (frame & MPF_MPEG1) bitrate = bitrate_table[layer - 1][index]; // MPEG-1
            else bitrate = bitrate_table[3 + (layer >> 1)][index]; // MPEG-2

            BYTE pad = (frame & MPF_PAD) ? 1 : 0;

            if (layer IS 1) frame_size = ((12 * bitrate / samplerate) + pad) * 4;
            else frame_size = (144 * bitrate / samplerate) + pad;

            LONG next = pos + frame_size;
            if (next + 4 < BufferSize) {
               frame = (Buffer[next]<<24) | (Buffer[next+1]<<16) | (Buffer[next+2]<<8) | Buffer[next+3];

               if ((frame & 0xffe00000) != 0xffe00000) continue;

               prv->info.channels    = HDR_IS_MONO(Buffer) ? 1 : 2;
               prv->info.hz          = samplerate;
               prv->info.frame_bytes = frame_size;
               prv->info.samples     = hdr_frame_samples(Buffer);

               log.extmsg("Frame found at %d, size %d, channels %d, %d samples, %dhz.", pos, prv->info.frame_bytes, prv->info.channels, prv->info.samples, prv->info.hz);

               return pos;
            }
         }
      }
   }

   log.extmsg("Failed to find a valid frame.");

   return -1;
}

//********************************************************************************************************************

static const struct ActionArray clActions[] = {
   { AC_Free,         (APTR)MP3_Free },
   { AC_Init,         (APTR)MP3_Init },
   { AC_Read,         (APTR)MP3_Read },
   { AC_SaveToObject, (APTR)MP3_SaveToObject },
   { AC_Seek,         (APTR)MP3_Seek },
   { 0, NULL }
};

//********************************************************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   if (objModule::load("audio", MODVERSION_AUDIO, &modAudio, &AudioBase) != ERR_Okay) return ERR_InitModule;

   clMP3 = objMetaClass::create::global(
      fl::BaseClassID(ID_SOUND),
      fl::SubClassID(ID_MP3),
      fl::ClassVersion(VER_MP3),
      fl::FileExtension("*.mp3"),
      fl::FileDescription("MP3 Audio Stream"),
      fl::Name("MP3"),
      fl::Category(CCF_AUDIO),
      fl::Actions(clActions),
      fl::Path(MOD_PATH));

   return clMP3 ? ERR_Okay : ERR_AddClass;
}

static ERROR CMDExpunge(void)
{
   if (clMP3) { acFree(clMP3); clMP3 = NULL; }
   return ERR_Okay;
}

//********************************************************************************************************************

PARASOL_MOD(CMDInit, NULL, NULL, CMDExpunge, VER_MP3)
