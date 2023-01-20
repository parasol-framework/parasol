/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

MP3: Sound class extension

*********************************************************************************************************************/

#include <parasol/main.h>
#include <parasol/modules/audio.h>
#include <parasol/modules/music.h>
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
const LONG MAX_FRAME_SIZE = 1152;

// Minimum buffer size for the outgoing audio stream.  Needs to be big enough to circumvent any lag issues.

const LONG MIN_DECODE_BUFFER = 65536;

#define MPF_MPEG1     (1<<19)
#define MPF_PAD       (1<<9)
#define MPF_COPYRIGHT (1<<3)
#define MPF_ORIGINAL  (1<<2)

struct prvMP3 {
   UBYTE  Input[16384];      // For incoming MP3 data.  Also needs to be big enough to accomodate the ID3v2 header.
   mp3dec_t mp3d;            // Decoder information
   mp3dec_frame_info_t info; // Retains info on the most recently decoded frame.
   // frame_bytes, frame_offset, channels, hz, layer, bitrate_kbps
   LONG   WriteOffset;       // Current stream offset in bytes, relative to Sound.Length.
   LONG   InputPos;          // Next byte position for reading input
   LONG   FramesProcessed;   // Count of frames processed by the decoder.
   LONG   TotalFrames;       // Total frames for the entire stream (known if CBR data, or VBR header is present).
   LONG   TotalSamples;      // Total samples for the entire stream.  Adjusted for null padding at either end of the stream.
   LONG   PaddingStart;      // Total samples at the start of the decoded stream that can be skipped.
   LONG   PaddingEnd;        // Total samples at the end of the decoded stream that can be ignored.
   LONG   StreamSize;        // Compressed stream length, if defined by VBR header.
   bool   EndOfFile;         // True if all incoming data has been read.
   bool   VBR;               // True if VBR detected, otherwise CBR
   bool   VBRChecked;        // True if VBR header has been checked
   bool   TOCLoaded;         // True if the Table of Contents has been defined.
   std::array<UBYTE, 100> TOC; // VBR Table of Contents

   void reset() { // Reset the decoder.  Necessary for seeking.
      InputPos        = 0;
      WriteOffset     = 0;
      FramesProcessed = 0;
      EndOfFile       = false;
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

static ERROR decode_mp3(objSound *);
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

static LONG skip_id3v2(const char *Buffer)
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

#define VBR_FRAMES       1 // Total number of frames is defined.
#define VBR_STREAM_SIZE  2 // The compressed audio stream size in bytes is defined.  Excludes ID3vX, Xing etc.
#define VBR_TOC          4 // TOC entries are defined.
#define VBR_SCALE        8 // VBR quality is indicated from 0 (best) to 100 (worst).

static int check_vbrtag(objSound *Self, const UBYTE *frame)
{
   parasol::Log log;

   auto prv = (prvMP3 *)Self->ChildPrivate;

   if (prv->VBRChecked) return 1;
   else prv->VBRChecked = true;

   bs_t bs[1];
   L3_gr_info_t gr_info[4];
   bs_init(bs, frame + HDR_SIZE, prv->info.frame_bytes - HDR_SIZE);
   if (HDR_IS_CRC(frame)) get_bits(bs, 16);
   if (L3_read_side_info(bs, gr_info, frame) < 0) return 0; // side info corrupted

   const UBYTE *tag = frame + HDR_SIZE + bs->pos / 8;
   if ((!StrCompare("Xing", (CSTRING)tag, 4, STR_CASE)) and (!StrCompare("Info", (CSTRING)tag, 4, STR_CASE))) return 0;
   int flags = tag[7];
   if (!(flags & VBR_FRAMES)) return -1;
   tag += 8;

   prv->TotalFrames = LONG((tag[0] << 24) | (tag[1] << 16) | (tag[2] << 8) | tag[3]);
   prv->TotalFrames--; // The VBR frame doesn't count as audio data.
   tag += 4;

   if (flags & VBR_STREAM_SIZE) {
      // This value is used for TOC seek calculations.
      prv->StreamSize = LONG((tag[0] << 24) | (tag[1] << 16) | (tag[2] << 8) | tag[3]);
      tag += 4;
   }

   if (flags & VBR_TOC) {
      CopyMemory(tag, prv->TOC.data(), 100);
      tag += 100;
      prv->TOCLoaded = true;
   }

   if (flags & VBR_SCALE) {
      LONG quality = LONG((tag[0] << 24) | (tag[1] << 16) | (tag[2] << 8) | tag[3]);
      acSetVar(Self, "VBRQuality", std::to_string(quality).c_str());
      tag += 4;
   }

   LONG delay = 0; // Typically the first 528 samples are padding set to zero and can be skipped.
   LONG padding = 0; // Padding tells you the number of samples at the end of the file that are empty.

   if (*tag) { // extension, LAME, Lavc, etc. Should be the same structure.
       tag += 21;
       if (tag - frame + 14 >= prv->info.frame_bytes) return 0;
       delay   = ((tag[0] << 4) | (tag[1] >> 4)) + (528 + 1);
       padding = (((tag[1] & 0xF) << 8) | tag[2]) - (528 + 1);
   }

   prv->PaddingEnd   = padding * prv->info.channels;
   prv->PaddingStart = delay * prv->info.channels;

   // Calculate the total no of samples for the entire streaam.
   LARGE detected_samples = prv->info.samples * (LARGE)prv->TotalFrames;
   if (detected_samples >= prv->PaddingStart) detected_samples -= prv->PaddingStart;
   if (detected_samples >= prv->PaddingEnd) detected_samples -= prv->PaddingEnd;

   prv->TotalSamples = detected_samples;
   prv->VBR = true;

   const DOUBLE seconds_len = DOUBLE(detected_samples) / DOUBLE(prv->info.hz);

   LONG len = prv->TotalFrames * 1152 * prv->info.channels * sizeof(WORD);
   len -= prv->PaddingEnd;
   Self->set(FID_Length, len);

   log.msg("VBR header detected.  Total Frames: %d, Samples: %d, Track Time: %.2fs, Byte Length: %d", prv->TotalFrames, prv->TotalSamples, seconds_len, len);

   return 1;
}

//********************************************************************************************************************
// ID3v2 is located at the start of the file.  This can be followed by a Xing VBR header.

static void parse_id3v2(objSound *Self)
{

}

//********************************************************************************************************************

static ERROR MP3_ActionNotify(objSound *Self, struct acActionNotify *Args)
{
   parasol::Log log;
   auto prv = (prvMP3 *)Self->ChildPrivate;

   if (Args->ActionID IS AC_Read) {
      decode_mp3(Self);
   }
   else if (Args->ActionID IS AC_Seek) {
      struct acSeek *seek;

      if (!(seek = (struct acSeek *)Args->Args)) return ERR_NullArgs;

      log.branch("Seeking to byte position %d.", seek->Position);

      prv->reset();
      mp3dec_init(&prv->mp3d);

      if ((seek->Position IS 0) and (seek->Offset IS SEEK_START)) {
         Self->File->seekStart(0);
         decode_mp3(Self);
         return ERR_Okay;
      }
      else {
         // Seeking via byte position, where the position is relative to the decoded length.

         if (Self->Length >= 0) {
            log.warning("MP3 stream length unknown, cannot seek.");
            return ERR_Failed;
         }

         LARGE seek_to;
         if (seek->Offset IS SEEK_START) seek_to = seek->Position;
         else if (seek->Offset IS SEEK_END) seek_to = Self->Length - seek->Position;
         else seek_to = prv->WriteOffset + seek->Position;

         if (seek_to < 0) seek_to = 0;
         else if (seek_to > Self->Length) seek_to = Self->Length;

         DOUBLE pct = DOUBLE(seek_to) / DOUBLE(Self->Length);

         if (prv->TOCLoaded) {
            LONG idx = F2T(pct * prv->TOC.size());
            if (idx < 0) idx = 0;
            else if (idx >= (LONG)prv->TOC.size()) idx = prv->TOC.size() - 1;

            //unsigned syncframe = syncentry * xing->i_frames / prv->TOC.size();
            //*pi_time = vlc_tick_from_samples(syncframe * mpgah->i_samples_per_frame, mpgah->i_sample_rate ) + VLC_TICK_0;
            LONG start = 0;
            LONG offset = (prv->TOC[idx] * prv->StreamSize) / 256;
            Self->File->seekStart(start + offset);

            // NB: The WriteOffset isn't going to be in perfect sync with the frame that we're seeking to.
            prv->WriteOffset = pct * Self->Length;

         }
         else {


         }

         decode_mp3(Self);
      }
   }
   else log.extmsg("Unrecognised action #%d.", Args->ActionID);

   return ERR_Okay;
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

   LONG bufferlength;
   if ((bufferlength = Self->BufferLength) < MIN_DECODE_BUFFER) bufferlength = MIN_DECODE_BUFFER;

   // Create a public file object for storing our decoded audio stream

   objFile *stream;
   if ((stream = objFile::create::global(fl::Flags(FL_BUFFER|FL_LOOP), fl::Size(bufferlength)))) {
      // Subscribe to the Read action of the stream object so that we can
      // feed it with more data each time the Audio object reads from it.

      SubscribeActionTags(stream, AC_Read, AC_Seek, TAGEND);
      Self->StreamFileID = stream->UID;
   }
   else return log.warning(ERR_CreateObject);

   // Open the file if the Sound class has not done so already

   if (!Self->File) {
      if (!(Self->File = objFile::create::integral(fl::Path(location), fl::Flags(FL_READ|FL_APPROXIMATE)))) {
         acFree(Self->StreamFileID);
         Self->StreamFileID = 0;
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
   }
   else {
      acFree(Self->StreamFileID);
      Self->StreamFileID = 0;
      return ERR_AllocMemory;
   }

   mp3dec_init(&prv->mp3d);
   prv->reset();

   // Fill the buffer with audio information and parse any MP3 header that is present.  This will also prove whether
   // or not this is really an mp3 file.

   if (decode_mp3(Self)) {
      FreeResource(Self->ChildPrivate); Self->ChildPrivate = NULL;
      acFree(Self->StreamFileID); Self->StreamFileID = 0;
      return ERR_NoSupport;
   }

   if (prv->info.channels IS 2) Self->Flags |= SDF_STEREO;

   if (Self->Flags & SDF_STEREO) Self->BytesPerSecond = prv->info.hz * 4;
   else Self->BytesPerSecond = prv->info.hz * 2;

   Self->Flags         |= SDF_STREAM;
   Self->BitsPerSample  = 16;
   Self->Frequency      = prv->info.hz;
   Self->Playback       = Self->Frequency;
   Self->BufferLength   = bufferlength;
   Self->Length         = calc_length(Self, reduce);

   log.msg("File is MP3.  Stereo: %c, BytesPerSecond: %d, Freq: %d, Byte Length: %d", Self->Flags & SDF_STEREO ? 'Y' : 'N', Self->BytesPerSecond, Self->Frequency, Self->Length);

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
   addstream.ObjectID  = Self->StreamFileID;
   addstream.SeekStart = 0;

   if (Self->Flags & SDF_STEREO) addstream.SampleFormat = SFM_S16_BIT_STEREO;
   else addstream.SampleFormat = SFM_S16_BIT_MONO;

   addstream.SampleLength = (Self->Length > 0) ? Self->Length : -1;
   addstream.BufferLength = bufferlength;

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

static ERROR MP3_SaveToObject(objSound *Self, struct acSaveToObject *Args)
{
   return ERR_NoSupport;
}

//********************************************************************************************************************

static ERROR decode_mp3(objSound *Self)
{
   parasol::Log log(__FUNCTION__);

   auto prv = (prvMP3 *)Self->ChildPrivate;

   if (prv->EndOfFile) return ERR_Okay;

   // Every time the system audio object reads from our audio file, we need to update it with a fresh set of information.

   parasol::ScopedObjectLock<objFile> outfile(Self->StreamFileID, 5000);
   if (outfile.granted()) {
      UBYTE *buffer;
      LONG write_pos, buffer_size;
      outfile->getPtr(FID_Buffer, &buffer);
      outfile->get(FID_Position,  &write_pos);
      outfile->get(FID_Size,      &buffer_size);

      LONG output_limit = write_pos + buffer_size - 8192;

      //log.msg("Stream out to %d of %d (diff %d), Buffer: %d, EOF: %d; InputPos: %d/%d", write_pos, prv->WriteOffset, prv->WriteOffset - write_pos, buffer_size, prv->EndOfFile, prv->InputPos, (LONG)sizeof(prv->Input));

      // Keep decoding until we exhaust space in the output buffer.

      WORD pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];

      // Force the seek position because the client may not have read the entire available amount on the last cycle.

      outfile->Position = prv->WriteOffset;

      bool no_more_input = false;
      while ((!prv->EndOfFile) and (prv->WriteOffset < output_limit)) {
         // Read as much input as possible.

         if (prv->InputPos < (LONG)sizeof(prv->Input)) {
            LONG result;
            ERROR error = Self->File->read(prv->Input + prv->InputPos, sizeof(prv->Input) - prv->InputPos, &result);

            if (error != ERR_Okay) {
               log.warning("File read error: %s", GetErrorMsg(error));
               prv->EndOfFile = true;
            }
            else if (!result) {
               log.extmsg("Reached end of input file.");
               no_more_input = true;
               // Don't change the EOF - let the output code do that.
            }

            prv->InputPos += result;
         }

         LONG in = 0; // Always start from zero

         if (!prv->FramesProcessed) {
            // Process ID3V2 and Xing VBR headers if present.

            LONG id3size = skip_id3v2((const char *)prv->Input);
            if (id3size) {
               log.msg("Detected ID3v2 header of %d bytes.", id3size);
               if (id3size < prv->InputPos) in = id3size;
            }
            else log.msg("No ID3v2 header found.");

            if (find_frame(Self, prv->Input + in, prv->InputPos - in) != -1) {
               if (check_vbrtag(Self, prv->Input + in)) {
                  in += prv->info.frame_bytes;
               }
               else log.msg("No VBR header found.");
            }
         }

         while (prv->WriteOffset < output_limit) {
            auto decoded_samples = mp3dec_decode_frame(&prv->mp3d, prv->Input + in, prv->InputPos - in, pcm, &prv->info);

            // Decoder results:
            // 0: No MP3 data found; 384: Layer 1; 576: Layer 3; 1152: All others

            if (!decoded_samples) {
               if (prv->info.frame_bytes > 0) {
                  // The decoder skipped ID3 or invalid data - do not play this frame
                  log.msg("Skipping MP3 frame at offset %d, size %d.", in, prv->InputPos - in);
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

               auto decoded_bytes = (prv->info.channels IS 2) ? (decoded_samples * sizeof(WORD) * 2) : (decoded_samples * sizeof(WORD));

               in += prv->info.frame_bytes;

               if (!outfile->write(pcm, decoded_bytes, NULL)) {
                  prv->WriteOffset += decoded_bytes;
               }
               else {
                  log.warning("Write failed, MP3 decode aborted.");
                  prv->EndOfFile = true;
               }
            }
         }

         // Shift any remaining data that couldn't be decoded.

         if (in < prv->InputPos) {
            CopyMemory((UBYTE *)prv->Input + in, prv->Input, prv->InputPos - in);
         }
         prv->InputPos -= in;
      }

      if (prv->EndOfFile) {
         // We now know the exact length of the decoded audio stream, so we pass that information to the Audio
         // object.  This will cause the Audio server to stop the stream at the correct position.  The Sound
         // class will remove the stream from the system once the sound is stopped.

         if (Self->Length != prv->WriteOffset - prv->PaddingEnd) {
            log.msg("Changing sample length from %d to %d bytes.", Self->Length, prv->WriteOffset - prv->PaddingEnd);
            Self->set(FID_Length, prv->WriteOffset - prv->PaddingEnd);
         }
      }

      outfile->Position = write_pos; // Reset the position for the client to read from.

      return ERR_Okay;
   }
   else return log.warning(ERR_AccessObject);
}

//********************************************************************************************************************
// Calculate the approximate decoded length of an MP3 audio stream.

#define SIZE_BUFFER     256000  // Load up to this many bytes to determine if the file is in variable bit-rate
#define SIZE_CBR_BUFFER 51200   // Load at least this many bytes to determine if the file is in constant bit-rate

static LONG calc_length(objSound *Self, LONG Reduce)
{
   LONG buffer_size;
   std::array<UWORD, 16> avg;  // This table is used to compute the interquartile mean

   if (Self->Length > 0) return Self->Length;

   parasol::Log log(__FUNCTION__);
   log.branch();

   auto prv = (prvMP3 *)Self->ChildPrivate;

   avg.fill(0);

   DOUBLE seconds       = 0;
   LONG frame_count     = 0;
   LONG avg_frame_len   = 0;
   LONG frame_start     = 0;
   LONG current_bitrate = 0;
   BYTE layer = 0;
   prv->VBR = false;

   LONG filesize;
   Self->File->get(FID_Size, &filesize);

   UBYTE *buffer;
   if (!AllocMemory(SIZE_BUFFER, MEM_DATA|MEM_NO_CLEAR, &buffer, NULL)) {
      // Load MP3 data from the file

      Self->File->seekStart(0.0);
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

            LONG bitrate;
            if (frame & MPF_MPEG1) bitrate = bitrate_table[layer - 1][index]; // MPEG-1
            else bitrate = bitrate_table[3 + (layer >> 1)][index]; // MPEG-2

            if (!current_bitrate) current_bitrate = bitrate;
            else if (current_bitrate != bitrate) prv->VBR = true;

            BYTE pad = (frame & MPF_PAD) ? 1 : 0;

            LONG frame_size;
            if (layer IS 1) frame_size = ((12 * bitrate / samplerate) + pad) * 4;
            else frame_size = (144 * bitrate / samplerate) + pad;

            if (!frame_size) { // In case file lacks integrity, frame must be at least 1 byte
               pos++;
               continue;
            }

            avg_frame_len += frame_size;

            seconds += DOUBLE(frame_size) / (DOUBLE(bitrate) / 1000.0 * 125.0);

            pos += frame_size;

            avg[index] = avg[index] + 1;
            frame_count++;
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

   avg_frame_len /= frame_count;

   log.extmsg("Average frame length: %d bytes", avg_frame_len);

   if (filesize > buffer_size) {
      if (prv->VBR) {
         // VBR detected and file size exceeds buffer - approximate the MP3 file length.
         //
         // Computing the average bit rate accurately when VBR is in use is crucial.  We use
         // the interquartile mean computation to do this.  This involves clearing a set
         // percentage of the top and bottom sampled bit rates, then calculating the average
         // mean from the values that are left.

         // Clear the lower quartile

         LONG clear = frame_count * 0.05;
         for (auto i=0; (i < (LONG)avg.size()) and (clear > 0); i++) {
            if (avg[i]) {
               if (avg[i] > clear) {
                  avg[i] -= clear;
                  clear = 0;
               }
               else {
                  clear -= avg[i];
                  avg[i] = 0;
               }
            }
         }

         // Clear the upper quartile

         clear = frame_count * 0.05;
         for (auto i=(LONG)avg.size()-1; (i >= 0) and (clear > 0); i--) {
            if (avg[i]) {
               if (avg[i] > clear) {
                  avg[i] -= clear;
                  clear = 0;
               }
               else {
                  clear -= avg[i];
                  avg[i] = 0;
               }
            }
         }

         // Compute the average kilo-bit sample rate

         DOUBLE avgbitrate = 0;
         DOUBLE total = 0;
         for (auto i=0; i < (LONG)avg.size(); i++) {
            if (avg[i]) {
               avgbitrate += avg[i] * (bitrate_table[layer - 1][i] / 1000);
               total += avg[i];
            }
         }

         avgbitrate /= total;

         // Guess the total frame count
         DOUBLE total_frames = (filesize - frame_start - Reduce) / avg_frame_len;
         seconds = (total_frames * DOUBLE(avg_frame_len)) / (avgbitrate * 125.0);
         return seconds * Self->BytesPerSecond;
      }
      else {
         Self->File->get(FID_Size, &filesize);
         DOUBLE total_frames = (filesize - frame_start - Reduce) / avg_frame_len;
         seconds = (total_frames * (DOUBLE)avg_frame_len) / (DOUBLE(current_bitrate) / 1000.0 * 125.0);
         return seconds * Self->BytesPerSecond;
      }
   }
   else if (frame_count > 0) {
      return seconds * Self->BytesPerSecond;
   }
   else {
      return -1; // Infinite stream length
   }
}

//********************************************************************************************************************

static LONG find_frame(objSound *Self, UBYTE *Buffer, LONG BufferSize)
{
   parasol::Log log(__FUNCTION__);
   LONG bitrate, frame_size;
   BYTE pad;
   auto prv = (prvMP3 *)Self->ChildPrivate;

   log.extmsg("find_frame(BufferSize: %d)", BufferSize);

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

            if (frame & MPF_PAD) pad = 1;
            else pad = 0;

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
   { AC_ActionNotify, (APTR)MP3_ActionNotify },
   { AC_Free,         (APTR)MP3_Free },
   { AC_Init,         (APTR)MP3_Init },
   { AC_SaveToObject, (APTR)MP3_SaveToObject },
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
