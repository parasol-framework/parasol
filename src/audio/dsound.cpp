
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <cstdio>
#include <math.h>

#define MSG(...) fprintf(stderr, __VA_ARGS__)

#define IS  ==

#define FILL_FIRST  2
#define FILL_SECOND 3

class BaseClass;
class extAudio;

struct PlatformData {
   LPDIRECTSOUNDBUFFER SoundBuffer;
   DWORD  BufferLength;
   DWORD  Position;      // Total number of bytes that have so far been loaded from the audio data source
   DWORD  SampleLength;  // Total length of the original sample
   DWORD  BufferPos;
   DWORD  SampleEnd;
   DWORD  Cycles;        // For streaming only, indicates the number of times the playback buffer has cycled.
   char   Fill;
   char   Stream;
   char   Loop;
   char   Stop;
   struct BaseClass *File;
};

int ReadData(struct BaseClass *, void *, int);
void SeekData(struct BaseClass *, int);

#include "windows.h"

static LPDIRECTSOUND glDirectSound;     // the DirectSound object
static HMODULE dsModule = NULL;         // dsound.dll module handle
static HWND glWindow;                   // HWND for DirectSound
static const int BUFFER_LENGTH = 500;   // buffer length in milliseconds

static HRESULT (WINAPI *dsDirectSoundCreate)(const GUID *, LPDIRECTSOUND *, IUnknown FAR *) = NULL;

//********************************************************************************************************************
// DirectSound uses logarithmic values for volume.  If there's a need to optimise this, generate a lookup table.

inline int linear2ds(float Volume)
{
   if (Volume <= 0.01) return DSBVOLUME_MIN;
   else return (int)floorf(2000.0 * log10f(Volume) + 0.5);
}

//********************************************************************************************************************

const char * dsInitDevice(int mixRate)
{
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

   if (dsDirectSoundCreate(NULL, &glDirectSound, NULL) != DS_OK) return "Failed in call to DirectSoundCreate().";

   if (glDirectSound->SetCooperativeLevel(glWindow, DSSCL_PRIORITY) != DS_OK)
      return "Failed in call to SetCooperativeLevel().";

   return NULL;
}

//********************************************************************************************************************

void dsCloseDevice(void)
{
   if (!glDirectSound) return;

   glDirectSound->Release();
   glDirectSound = 0;
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

const char * sndCreateBuffer(struct BaseClass *File, void *Wave, int BufferLength, int SampleLength, PlatformData *Sound, int Stream)
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
   dsbdesc.dwSize  = sizeof(DSBUFFERDESC);
   dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2
                     |DSBCAPS_GLOBALFOCUS
                     |DSBCAPS_CTRLVOLUME
                     |DSBCAPS_CTRLPAN
                     |DSBCAPS_CTRLFREQUENCY
                     |DSBCAPS_CTRLPOSITIONNOTIFY;
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
   if (!glDirectSound) return 0;

   if (Sound->SoundBuffer) {
      DWORD current_read, current_write;
      IDirectSoundBuffer_GetCurrentPosition(Sound->SoundBuffer, &current_read, &current_write);
      return (Sound->Cycles * Sound->BufferLength) + current_read;
   }
   else return 0;
}
