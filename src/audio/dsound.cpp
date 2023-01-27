
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <cstdio>
#include <math.h>

typedef unsigned char UBYTE;

#define MSG(...) fprintf(stderr, __VA_ARGS__)

#define IS  ==

static LPDIRECTSOUNDBUFFER glPrimaryBuffer = 0;

#define FILL_FIRST  2
#define FILL_SECOND 3

class extSound;
class extAudio;

struct PlatformData {
   LPDIRECTSOUNDBUFFER SoundBuffer;
   DWORD BufferLength;
   DWORD Position;      // Total number of bytes that have so far been loaded from the audio data source
   DWORD SampleLength;  // Total length of the original sample
   DWORD BufferPos;
   DWORD SampleEnd;
   DWORD Cycles;        // For streaming only, indicates the number of times the playback buffer has cycled.
   char Fill;
   char Stream;
   char Loop;
   char Stop;
   extSound *File;
};

int ReadData(extSound *, void *, int);
void SeekData(extSound *, int);
int GetMixAmount(extAudio *, int *);
int process_commands(extAudio *, int);
int process_commands(extAudio *, int);

#include "windows.h"

static const unsigned int SAMPLE_SIZE = sizeof(WORD) * 2; // Sample Size * Channels
static unsigned glMixerPos = 0;         // write position
static LPDIRECTSOUND glDirectSound;     // the DirectSound object
static HMODULE dsModule = NULL;         // dsound.dll module handle
static HWND glWindow;                   // HWND for DirectSound
static unsigned int glTargetBufferLen;
static const int BUFFER_LENGTH = 500;   // buffer length in milliseconds

static HRESULT (WINAPI *dsDirectSoundCreate)(const GUID *, LPDIRECTSOUND *, IUnknown FAR *) = NULL;

#define OK 0

//********************************************************************************************************************
// DirectSound uses logarithmic values for volume.  If there's a need to optimise this, generate a lookup table.

inline int linear2ds(float Volume)
{
   if (Volume <= 0.01) return DSBVOLUME_MIN;
   else return (int)floorf(2000.0 * log10f(Volume) + 0.5);
}

//********************************************************************************************************************

static const char * dserror(HRESULT error)
{
   switch (error) {
      case DS_OK:                    return "DS_OK";
      case DSERR_ALLOCATED:          return "DSERR_ALLOCATED";
      case DSERR_ALREADYINITIALIZED: return "DSERR_ALREADYINITIALIZED";
      case DSERR_BADFORMAT:          return "DSERR_BADFORMAT";
      case DSERR_BUFFERLOST:         return "DSERR_BUFFERLOST";
      case DSERR_CONTROLUNAVAIL:     return "DSERR_CONTROLUNAVAIL";
      case DSERR_GENERIC:            return "DSERR_GENERIC";
      case DSERR_INVALIDCALL:        return "DSERR_INVALIDCALL";
      case DSERR_INVALIDPARAM:       return "DSERR_INVALIDPARAM";
      case DSERR_NOAGGREGATION:      return "DSERR_NOAGGREGATION";
      case DSERR_NODRIVER:           return "DSERR_NODRIVER";
      case DSERR_OTHERAPPHASPRIO:    return "DSERR_OTHERAPPHASPRIO";
      case DSERR_OUTOFMEMORY:        return "DSERR_OUTOFMEMORY";
      case DSERR_PRIOLEVELNEEDED:    return "DSERR_PRIOLEVELNEEDED";
      case DSERR_UNINITIALIZED:      return "DSERR_UNINITIALIZED";
      case DSERR_UNSUPPORTED:        return "DSERR_UNSUPPORTED";
   }

   return "DirectSound undefined error";
}

//********************************************************************************************************************

const char * dsInitDevice(int mixRate)
{
   HRESULT     result;
   DSBUFFERDESC bufferDesc;
   WAVEFORMATEX format;
   DSCAPS      dscaps;

   glWindow = GetDesktopWindow();

   if (!glWindow) return "Failed to get desktop window.";

   // If the DirectSound DLL hasn't been loaded yet, load it and get the address for DirectSoundCreate.

   if ((dsModule IS NULL) or (dsDirectSoundCreate IS NULL) ) {
      if ((dsModule = LoadLibrary("dsound.dll")) IS NULL) {
         return "Couldn't load dsound.dll";
      }

      if ((dsDirectSoundCreate = (HRESULT (*)(const GUID*, IDirectSound**, IUnknown*))GetProcAddress(dsModule, "DirectSoundCreate")) IS NULL) {
         return "Couldn't get DirectSoundCreate address";
      }
   }

   // Create the DirectSound object

   if ((result = dsDirectSoundCreate(NULL, &glDirectSound, NULL)) != DS_OK) return "Failed in call to DirectSoundCreate().";

   if ((result = glDirectSound->SetCooperativeLevel(glWindow, DSSCL_PRIORITY)) != DS_OK)
      return "Failed in call to SetCooperativeLevel().";

   // Create primary output buffer

   ZeroMemory(&bufferDesc, sizeof(DSBUFFERDESC));
   bufferDesc.dwSize  = sizeof(DSBUFFERDESC);
   bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
   if ((result = glDirectSound->CreateSoundBuffer(&bufferDesc, &glPrimaryBuffer, NULL)) != DS_OK)
      return dserror(result);

   // Set the primary buffer format

   ZeroMemory(&format, sizeof(WAVEFORMATEX));

   format.nChannels       = 2;   // Stereo
   format.wBitsPerSample  = 16;  // 16 bit
   format.wFormatTag      = WAVE_FORMAT_PCM;
   format.nSamplesPerSec  = mixRate;
   format.nBlockAlign     = (format.wBitsPerSample / 8) * format.nChannels;
   format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

   if ((result = glPrimaryBuffer->SetFormat(&format)) != DS_OK) return dserror(result);

   // Figure out our preferred buffer length: (use mDSoundBufferLength only if not in emulation mode)

   dscaps.dwSize = sizeof(DSCAPS);
   if ((result = glDirectSound->GetCaps(&dscaps)) != DS_OK) return dserror(result);

   if (dscaps.dwFlags & DSCAPS_EMULDRIVER) glTargetBufferLen = (mixRate * SAMPLE_SIZE * BUFFER_LENGTH) / 100;
   else glTargetBufferLen = (mixRate * SAMPLE_SIZE * 100) / 100;

   glTargetBufferLen = (glTargetBufferLen + 15) & (~15);

   // Now create the primary/mix playback buffer

   ZeroMemory(&format, sizeof(WAVEFORMATEX));
   format.nChannels       = 2;
   format.wBitsPerSample  = 16;
   format.wFormatTag      = WAVE_FORMAT_PCM;
   format.nSamplesPerSec  = mixRate;
   format.nBlockAlign     = (format.wBitsPerSample / 8) * format.nChannels;
   format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

   ZeroMemory(&bufferDesc, sizeof(DSBUFFERDESC));
   bufferDesc.dwSize  = sizeof(DSBUFFERDESC);
   bufferDesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2
                        |DSBCAPS_CTRLVOLUME
                        |DSBCAPS_CTRLPAN
                        |DSBCAPS_CTRLFREQUENCY
                        |DSBCAPS_GLOBALFOCUS         // Allows background playing
                        |DSBCAPS_CTRLPOSITIONNOTIFY; // Needed for notification
   bufferDesc.dwBufferBytes = glTargetBufferLen;
   bufferDesc.lpwfxFormat   = &format;

   if ((result = glDirectSound->CreateSoundBuffer(&bufferDesc, &glPrimaryBuffer, NULL)) != DS_OK) return dserror(result);

   // Play it

   if ((result = glPrimaryBuffer->Play(0, 0, DSBPLAY_LOOPING)) != DS_OK) return dserror(result);

   return NULL;
}

//********************************************************************************************************************

void dsCloseDevice(void)
{
   if (!glDirectSound) return;

   if (glPrimaryBuffer) {
      glPrimaryBuffer->Stop();
      glPrimaryBuffer->Release();
      glPrimaryBuffer = NULL;
   }

   glDirectSound->Release();
   glDirectSound = 0;
}

//********************************************************************************************************************

void dsClear(void)
{
   unsigned char *write1, *write2;
   DWORD length1, length2;

   if (glPrimaryBuffer) {
      glPrimaryBuffer->Lock(0, glTargetBufferLen, (void **)&write1, &length1, (void **)&write2, &length2, 0);
      if (write1) for (DWORD i=0; i < length1; i++) write1[i] = 0;
      if (write2) for (DWORD i=0; i < length2; i++) write2[i] = 0;
      glPrimaryBuffer->Unlock(write1, length1, write2, length2);
   }
}

//********************************************************************************************************************
// Resumes sound playback after winSuspend()

int dsResume(void)
{
   unsigned char *write1, *write2;
   DWORD length1, length2, i;

   if (!glDirectSound) return OK;

   if (glPrimaryBuffer) {
      // Clear the playback buffer of any old sampling data
      glPrimaryBuffer->Lock(0, glTargetBufferLen, (void **)&write1, &length1, (void **)&write2, &length2, 0);
      if (write1) for (i=0; i < length1; i++) write1[i] = 0;
      if (write2) for (i=0; i < length2; i++) write2[i] = 0;
      glPrimaryBuffer->Unlock(write1, length1, write2, length2);

      // Set the current position back to zero, then play the buffer
      glPrimaryBuffer->SetCurrentPosition(0);
      glPrimaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
   }

   return OK;
}

//********************************************************************************************************************

int mix_data(extAudio *, int, void *);

int dsMixer(extAudio *Self)
{
   if (!glDirectSound) return 0;

   // The write_cursor always leads the play cursor, typically by about 15 milliseconds.
   // It is guaranteed to be safe to write behind the play_cursor.

   DWORD space_left, play_cursor, write_cursor;
   glPrimaryBuffer->GetCurrentPosition(&play_cursor, &write_cursor);

   // Calculate the amount of space left in the buffer:

   if (glMixerPos <= play_cursor) space_left = play_cursor - glMixerPos;
   else space_left = glTargetBufferLen - glMixerPos + play_cursor;

   if (space_left > 16) space_left -= 16;
   else space_left = 0;

   space_left = space_left / SAMPLE_SIZE; // Convert to number of samples

   while (space_left) { // Scan channels to check if an update rate is going to be met
      DWORD len1, len2, elements;
      int mix_left;
      GetMixAmount((extAudio *)Self, &mix_left);

      if ((DWORD)mix_left < space_left) elements = mix_left;
      else elements = space_left;

      UBYTE *write1, *write2;
      HRESULT result;
      if ((result = glPrimaryBuffer->Lock(glMixerPos, SAMPLE_SIZE * elements, (void **)&write1, &len1, (void **)&write2, &len2, 0)) != DS_OK) {
         if (result IS DSERR_BUFFERLOST) {
            if (glPrimaryBuffer->Restore() != DS_OK) return 1;
            if (glPrimaryBuffer->Play(0, 0, DSBPLAY_LOOPING) != DS_OK) return 1;
         }
         else return 1;
      }

      if (len1) { if (mix_data(Self, len1 / SAMPLE_SIZE, write1)) break; }
      if (len2) { if (mix_data(Self, len2 / SAMPLE_SIZE, write2)) break; }

      glMixerPos += len1 + len2;
      if (glMixerPos >= glTargetBufferLen) glMixerPos -= glTargetBufferLen;

      glPrimaryBuffer->Unlock((void **)write1, len1, (void **)write2, len2);

      // Drop the mix amount.  This may also update buffered channels for the next round

      process_commands((extAudio *)Self, elements);

      space_left -= elements;
   }

   return OK;
}

//********************************************************************************************************************

int sndCheckActivity(PlatformData *Sound)
{
   DWORD status;

   if (!glDirectSound) return -1;
   if (!Sound->SoundBuffer) return -1; // Error

   if (IDirectSoundBuffer_GetStatus(Sound->SoundBuffer, &status) IS DS_OK) {
      if (status & DSBSTATUS_PLAYING) {
         return 1;
      }
      else return 0;
   }
   else return -1; // Error
}

//********************************************************************************************************************
// SampleLength: The byte length of the raw audio data, excludes all file headers.

const char * sndCreateBuffer(extSound *File, void *Wave, int BufferLength, int SampleLength, PlatformData *Sound, int Stream)
{
   if (!glDirectSound) return 0;

   Sound->File         = File;
   Sound->SampleLength = SampleLength;
   Sound->BufferLength = BufferLength;
   Sound->Position     = 0;
   Sound->Cycles       = 0;
   Sound->Stream       = Stream;
   Sound->Fill         = FILL_FIRST; // First half waiting to be filled

   DSBUFFERDESC dsbdesc;
   ZeroMemory(&dsbdesc, sizeof(DSBUFFERDESC));
   dsbdesc.dwSize        = sizeof(DSBUFFERDESC);
   dsbdesc.dwFlags       = DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_GLOBALFOCUS|DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLPAN|DSBCAPS_CTRLFREQUENCY;
   dsbdesc.dwBufferBytes = BufferLength;
   dsbdesc.lpwfxFormat   = (LPWAVEFORMATEX)Wave;
   if ((IDirectSound_CreateSoundBuffer(glDirectSound, &dsbdesc, &Sound->SoundBuffer, NULL) != DS_OK) or (!Sound->SoundBuffer)) {
      return "CreateSoundBuffer() failed to create WAVE audio buffer.";
   }

   // Fill the buffer with audio content (unless it is streamed, in which case it will be filled when play occurs).

   if (Stream) return NULL;

   void *bufA, *bufB;
   int lenA, lenB;
   if (IDirectSoundBuffer_Lock(Sound->SoundBuffer, 0, BufferLength, (void **)&bufA, (DWORD *)&lenA, (void **)&bufB, (DWORD *)&lenB, 0) IS DS_OK) {
      if (lenA) lenA = ReadData(File, bufA, lenA);
      if (lenB) lenB = ReadData(File, bufB, lenB);
      Sound->Position += lenA + lenB;
      IDirectSoundBuffer_Unlock(Sound->SoundBuffer, bufA, lenA, bufB, lenB); // lenA/B inform DSound as to how many bytes were written.
   }

   return NULL;
}

//********************************************************************************************************************

void sndFree(PlatformData *Info)
{
   if (!glDirectSound) return;

   if (Info->SoundBuffer) {
      IDirectSoundBuffer_Stop(Info->SoundBuffer);     // Stop the playback buffer
      IDirectSoundBuffer_Release(Info->SoundBuffer);  // Free the playback buffer
      Info->SoundBuffer = NULL;
   }
}

//********************************************************************************************************************

void sndFrequency(PlatformData *Sound, int Frequency)
{
   if (!glDirectSound) return;
   if (Sound->SoundBuffer) IDirectSoundBuffer_SetFrequency(Sound->SoundBuffer, Frequency);
}

//********************************************************************************************************************

void sndPan(PlatformData *Sound, float Pan)
{
   if (!glDirectSound) return;

   // Range -10,000 to 10,000 (DSBPAN_LEFT to DSBPAN_RIGHT)
   LONG pan = LONG(Pan * DSBPAN_RIGHT);
   if (pan < DSBPAN_LEFT) pan = DSBPAN_LEFT;
   else if (pan > DSBPAN_RIGHT) pan = DSBPAN_RIGHT;
   if (Sound->SoundBuffer) IDirectSoundBuffer_SetPan(Sound->SoundBuffer, pan);
}

//********************************************************************************************************************

void sndStop(PlatformData *Sound)
{
   if (!glDirectSound) return;
   if (Sound->SoundBuffer) IDirectSoundBuffer_Stop(Sound->SoundBuffer);
}

//********************************************************************************************************************
// Used by the Sound class to play WAV or raw audio samples that are independent of our custom mixer.

int sndPlay(PlatformData *Sound, int Loop, int Offset)
{
   if ((!Sound) or (!Sound->SoundBuffer)) return -1;

   if (Offset < 0) Offset = 0;
   else if (Offset >= (int)Sound->SampleLength) return -1;

   Sound->Loop      = Loop;
   Sound->SampleEnd = 0;
   Sound->Cycles    = 0;

   if (Sound->Stream) {
      // Streamed samples require that we reload sound data from scratch.  This call initiates the streaming playback,
      // after which sndStreamAudio() needs to be used to keep filling the buffer.

      IDirectSoundBuffer_Stop(Sound->SoundBuffer);

      Sound->Fill = FILL_FIRST;
      Sound->Stop = 0;

      unsigned char *bufA, *bufB;
      int lenA, lenB;
      if (IDirectSoundBuffer_Lock(Sound->SoundBuffer, 0, Sound->BufferLength, (void **)&bufA, (DWORD *)&lenA, (void **)&bufB, (DWORD *)&lenB, 0) IS DS_OK) {
         SeekData(Sound->File, Offset);
         if (lenA > 0) lenA = ReadData(Sound->File, bufA, lenA);
         if (lenB > 0) lenB = ReadData(Sound->File, bufB, lenB);
         Sound->Position = Offset + lenA + lenB;
         IDirectSoundBuffer_Unlock(Sound->SoundBuffer, bufA, lenA, bufB, lenB);
      }
      else return -1;
   }
   else { // For non-streamed samples, start the play position from the proposed offset
      IDirectSoundBuffer_Stop(Sound->SoundBuffer);
      IDirectSoundBuffer_SetCurrentPosition(Sound->SoundBuffer, Offset);
   }

   // Play the sound

   if ((Sound->Loop) or (Sound->Stream)) {
      IDirectSoundBuffer_Play(Sound->SoundBuffer, 0, 0, DSBPLAY_LOOPING);
   }
   else IDirectSoundBuffer_Play(Sound->SoundBuffer, 0, 0, 0);

   return 0;
}

//********************************************************************************************************************
// Streaming audio process for WAV or raw audio samples played via the Sound class.  This is regularly called by the
// Sound class' timer.

int sndStreamAudio(PlatformData *Sound)
{
   unsigned char *write, *write2;

   if (!glDirectSound) return -1;
   if (!Sound->SoundBuffer) return -1;

   // Get the current play position.  NB: The BufferPos will cycle.

   if (IDirectSoundBuffer_GetCurrentPosition(Sound->SoundBuffer, &Sound->BufferPos, NULL) IS DS_OK) {
      // If the playback marker is in the buffer's first half, we fill the second half, and vice versa.

      int lock_start = -1;
      int lock_length = 0;
      if ((Sound->Fill IS FILL_FIRST) and (Sound->BufferPos >= Sound->BufferLength>>1)) {
         Sound->Fill = FILL_SECOND;
         lock_start  = 0;
         lock_length = Sound->BufferLength>>1;
      }
      else if ((Sound->Fill IS FILL_SECOND) and (Sound->BufferPos < Sound->BufferLength>>1)) {
         Sound->Fill = FILL_FIRST;
         lock_start  = Sound->BufferLength>>1;
         lock_length = Sound->BufferLength - (Sound->BufferLength>>1);
      }

      if (lock_start != -1) {
         // Set the SampleEnd to zero if the sample does not terminate in this current buffer segment.

         if ((Sound->Stop > 1) and (Sound->SampleEnd > 0)) Sound->SampleEnd = 0;

         // Load more data if we have entered the next audio buffer section

         int length, length2, bytes_out;
         if (IDirectSoundBuffer_Lock(Sound->SoundBuffer, lock_start, lock_length, (void **)&write, (DWORD *)&length, (void **)&write2, (DWORD *)&length2, 0) IS DS_OK) {
            auto len = length;
            if (Sound->Position + len > Sound->SampleLength) len = Sound->SampleLength - Sound->Position;

            if (len > 0) {
               bytes_out = ReadData(Sound->File, write, len);
               Sound->Position += bytes_out;
            }
            else bytes_out = 0;

            if (Sound->Position >= Sound->SampleLength) {
               // All of the bytes have been read from the sample.

               if (Sound->Loop) {
                  SeekData(Sound->File, 0);
                  bytes_out = ReadData(Sound->File, write + bytes_out, length - bytes_out);
                  Sound->Position = bytes_out;
               }
               else {
                  if (!Sound->Stop) { // Set the SampleEnd to indicate where the sample will end within the buffer.
                     Sound->SampleEnd = lock_start + bytes_out;
                  }

                  Sound->Stop++;
                  if (length - bytes_out > 0) ZeroMemory(write+bytes_out, length-bytes_out); // Clear trailing data for a clean exit
                  if (length2 > 0) ZeroMemory(write2, length2);
               }
            }

            IDirectSoundBuffer_Unlock(Sound->SoundBuffer, write, bytes_out, write2, 0);
         }
      }

      // Send a stop signal if playback reached the end of the sample data

      if ((!Sound->Loop) and (Sound->Stop > 1) and (Sound->BufferPos >= Sound->SampleEnd)) {
         IDirectSoundBuffer_Stop(Sound->SoundBuffer);
         return 1;
      }
      else return 0;
   }
   else return -1;
}

//********************************************************************************************************************
// Adjusting the length is supported for streaming samples only.

void sndLength(PlatformData *Sound, int Length)
{
   if (!glDirectSound) return;

   Sound->SampleLength = Length;
}

//********************************************************************************************************************

void sndVolume(PlatformData *Sound, float Volume)
{
   if (!glDirectSound) return;

   if (Sound->SoundBuffer) {
      IDirectSoundBuffer_SetVolume(Sound->SoundBuffer, linear2ds(Volume));
   }
}

//********************************************************************************************************************

LONG sndGetPosition(PlatformData *Sound)
{
   DWORD current_read, current_write;

   if (!glDirectSound) return 0;

   if (Sound->SoundBuffer) {
      IDirectSoundBuffer_GetCurrentPosition(Sound->SoundBuffer, &current_read, &current_write);
      return (Sound->Cycles * Sound->BufferLength) + current_read;
   }
   else return 0;
}
