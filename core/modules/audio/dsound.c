
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

typedef unsigned char UBYTE;

#define DEBUG_CODE(x)

#define IS  ==
#define AND &&
#define OR  ||

static LPDIRECTSOUND       glDirectSound = 0;
static LPDIRECTSOUNDBUFFER glPrimaryBuffer = 0;

#define FILL_NONE   1
#define FILL_FIRST  2
#define FILL_SECOND 3

struct PlatformData {
   LPDIRECTSOUNDBUFFER SoundBuffer;
   DWORD BufferLength;
   DWORD Position;      /* Total number of bytes that have so far been loaded from the audio data source */
   DWORD PlayPosition;  /* Reflects the total number of bytes that have been played via streaming audio */
   DWORD SampleLength;  /* Total length of the original sample */
   DWORD BufferPos;
   DWORD SampleEnd;
   char Fill;
   char Stream;
   char Loop;
   char Stop;
   void *File;
};

struct rkSound {};
struct rkAudio;

int ReadData(struct rkSound *, void *, int);
void SeekData(struct rkSound *, int);
void SeekZero(struct rkSound *);
int GetMixAmount(struct rkAudio *, int *);
int DropMixAmount(struct rkAudio *, int);

#include "windows.h"

extern unsigned outputMode;
unsigned int mixElemSize;
static unsigned bufferLen;
static unsigned bufferLeaveSpace;
static unsigned writePos;               /* write position */
static LPDIRECTSOUND glDirectSound;     /* the DirectSound object */
static HMODULE dsModule = NULL;         /* dsound.dll module handle */
static void *glWindow;                         /* HWND for DirectSound */
static unsigned int targetBufferLen;
static int mBufferLength = 500;         /* buffer length in milliseconds */

static HRESULT (WINAPI *dsDirectSoundCreate)(const GUID *, LPDIRECTSOUND *, IUnknown FAR *) = NULL;

#define OK 0

/*****************************************************************************
** Function: dsError()
*/

static unsigned char * dserror(HRESULT error)
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

/****************************************************************************\
** Function: dsInitDevice()
*/

unsigned char * dsInitDevice(int mixRate)
{
   HRESULT     result;
   DSBUFFERDESC bufferDesc;
   WAVEFORMATEX format;
   DSCAPS      dscaps;

   glWindow = GetDesktopWindow();
   mixElemSize = sizeof(WORD) * 2;

   if (!glWindow) return "Failed to get desktop window.";

   /* If the DirectSound DLL hasn't been loaded yet, load it and get the
   ** address for DirectSoundCreate.
   */

   if ((dsModule IS NULL) OR (dsDirectSoundCreate IS NULL) ) {
      if ((dsModule = LoadLibrary("dsound.dll")) IS NULL) {
         return "Couldn't load dsound.dll";
      }

      if ((dsDirectSoundCreate = (void *)GetProcAddress(dsModule, "DirectSoundCreate")) IS NULL) {
         return "Couldn't get DirectSoundCreate address";
      }
   }

   /*** Create the DirectSound object ***/

   if ((result = dsDirectSoundCreate(NULL, &glDirectSound, NULL)) != DS_OK) return "Failed in call to DirectSoundCreate().";

   if ((result = glDirectSound->lpVtbl->SetCooperativeLevel(glDirectSound, glWindow, DSSCL_PRIORITY)) != DS_OK)
      return "Failed in call to SetCooperativeLevel().";

   /*** Create primary output buffer ***/

   memset(&bufferDesc, 0, sizeof(DSBUFFERDESC));
   bufferDesc.dwSize  = sizeof(DSBUFFERDESC);
   bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
   if ((result = glDirectSound->lpVtbl->CreateSoundBuffer(glDirectSound, &bufferDesc, &glPrimaryBuffer, NULL)) != DS_OK)
      return dserror(result);

   /*** Set the primary buffer format: ***/

   memset(&format, 0, sizeof(WAVEFORMATEX));

   format.nChannels       = 2;   /* Stereo */
   format.wBitsPerSample  = 16;  /* 16 bit */
   format.wFormatTag      = WAVE_FORMAT_PCM;
   format.nSamplesPerSec  = mixRate;
   format.nBlockAlign     = format.wBitsPerSample / 8 * format.nChannels;
   format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

   if ((result = glPrimaryBuffer->lpVtbl->SetFormat(glPrimaryBuffer, &format)) != DS_OK) return dserror(result);

   /* Figure out our preferred buffer length: (use mDSoundBufferLength
   ** only if not in emulation mode)
   */

   dscaps.dwSize = sizeof(DSCAPS);
   if ((result = glDirectSound->lpVtbl->GetCaps(glDirectSound, &dscaps)) != DS_OK) return dserror(result);

   if (dscaps.dwFlags & DSCAPS_EMULDRIVER) targetBufferLen = (mixRate * mixElemSize * mBufferLength) / 100;
   else targetBufferLen = (mixRate * mixElemSize * 100) / 100;

   targetBufferLen = (targetBufferLen + 15) & (~15);

   writePos = 0;

   /*** Now create the primary/mix playback buffer ***/

   bufferLen = targetBufferLen;
   bufferLeaveSpace = 16;

   memset(&format, 0, sizeof(WAVEFORMATEX));
   format.nChannels       = 2;
   format.wBitsPerSample  = 16;
   format.wFormatTag      = WAVE_FORMAT_PCM;
   format.nSamplesPerSec  = mixRate;
   format.nBlockAlign     = format.wBitsPerSample / 8 * format.nChannels;
   format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

   memset(&bufferDesc, 0, sizeof(DSBUFFERDESC));
   bufferDesc.dwSize  = sizeof(DSBUFFERDESC);
   bufferDesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2
                        |DSBCAPS_CTRLVOLUME
                        |DSBCAPS_CTRLPAN
                        |DSBCAPS_CTRLFREQUENCY
                        |DSBCAPS_GLOBALFOCUS         /* Allows background playing */
                        |DSBCAPS_CTRLPOSITIONNOTIFY; /* Needed for notification */
   bufferDesc.dwBufferBytes = bufferLen;
   bufferDesc.lpwfxFormat   = &format;

   if ((result = glDirectSound->lpVtbl->CreateSoundBuffer(glDirectSound, &bufferDesc, &glPrimaryBuffer, NULL)) != DS_OK) return dserror(result);

   /*** Play it ***/

   if ((result = glPrimaryBuffer->lpVtbl->Play(glPrimaryBuffer, 0, 0, DSBPLAY_LOOPING)) != DS_OK) return dserror(result);

   return NULL;
}

/*****************************************************************************
** Function: dsCloseDevice()
*/

void dsCloseDevice(void)
{
   if (!glDirectSound) return;

   /*** Free primary sound buffer ***/

   if (glPrimaryBuffer) {
      glPrimaryBuffer->lpVtbl->Stop(glPrimaryBuffer);
      glPrimaryBuffer->lpVtbl->Release(glPrimaryBuffer);
      glPrimaryBuffer = NULL;
   }

   glDirectSound->lpVtbl->Release(glDirectSound);
   glDirectSound = 0;
}

/*****************************************************************************
** Function: dsClear()
*/

void dsClear(void)
{
   unsigned char *write1, *write2;
   DWORD length1, length2, i;

   if (glPrimaryBuffer) {
      glPrimaryBuffer->lpVtbl->Lock(glPrimaryBuffer, 0, targetBufferLen, (void *)&write1, &length1, (void *)&write2, &length2, 0);
      if (write1) for (i=0; i < length1; i++) write1[i] = 0;
      if (write2) for (i=0; i < length2; i++) write2[i] = 0;
      glPrimaryBuffer->lpVtbl->Unlock(glPrimaryBuffer, write1, length1, write2, length2);
   }
}

/*****************************************************************************
** Function: dsResume()
** Short:    Resumes sound playback after winSuspend()
*/

int dsResume(void)
{
   unsigned char *write1, *write2;
   DWORD length1, length2, i;

   if (!glDirectSound) return OK;

   if (glPrimaryBuffer) {
      /*** Clear the playback buffer of any old sampling data ***/
      glPrimaryBuffer->lpVtbl->Lock(glPrimaryBuffer, 0, targetBufferLen, (void *)&write1, &length1, (void *)&write2, &length2, 0);
      if (write1) for (i=0; i < length1; i++) write1[i] = 0;
      if (write2) for (i=0; i < length2; i++) write2[i] = 0;
      glPrimaryBuffer->lpVtbl->Unlock(glPrimaryBuffer, write1, length1, write2, length2);

      /*** Set the current position back to zero, then play the buffer ***/
      glPrimaryBuffer->lpVtbl->SetCurrentPosition(glPrimaryBuffer, 0);
      glPrimaryBuffer->lpVtbl->Play(glPrimaryBuffer, 0, 0, DSBPLAY_LOOPING);
   }

   return OK;
}

/*****************************************************************************
** Function: dsPlay()
*/

int MixData(void *, unsigned, void *);

int dsPlay(void *Self)
{
   HRESULT result;
   UBYTE *write1, *write2;
   DWORD len1, len2, elements, spaceleft, cursor, writeCursor;
   int mixleft;

   if (!glDirectSound) return 0;

   glPrimaryBuffer->lpVtbl->GetCurrentPosition(glPrimaryBuffer, &cursor, &writeCursor);

   /*** Calculate the amount of space left in the buffer: ***/

   if (writePos <= cursor) spaceleft = cursor - writePos;
   else spaceleft = bufferLen - writePos + cursor;

   if (spaceleft > 16) spaceleft -= 16;
   else spaceleft = 0;

   spaceleft = spaceleft / mixElemSize; /* Convert to number of elements */

   while (spaceleft) {
      /*** Scan channels to check if an update rate is going to be met ***/

      GetMixAmount(Self, &mixleft);

      if (mixleft < spaceleft) elements = mixleft;
      else elements = spaceleft;

      if ((result = glPrimaryBuffer->lpVtbl->Lock(glPrimaryBuffer, writePos, mixElemSize * elements, (void **)&write1, &len1, (void **)&write2, &len2, 0)) != DS_OK) {
         if (result IS DSERR_BUFFERLOST) {
            if (glPrimaryBuffer->lpVtbl->Restore(glPrimaryBuffer) != DS_OK) return 1;
            if (glPrimaryBuffer->lpVtbl->Play(glPrimaryBuffer, 0, 0, DSBPLAY_LOOPING) != DS_OK) return 1;
         }
         else return 1;
      }

      if (len1) MixData(Self, len1 / mixElemSize, write1);
      if (len2) MixData(Self, len2 / mixElemSize, write2);

      writePos += len1 + len2;
      if (writePos >= bufferLen) writePos -= bufferLen;

      glPrimaryBuffer->lpVtbl->Unlock(glPrimaryBuffer, (void **)write1, len1, (void **)write2, len2);

      /*** Drop the mix amount.  This may also update buffered channels for the next round ***/

      DropMixAmount(Self, elements);

      spaceleft -= elements;
   }

   return OK;
}

/*****************************************************************************
** Internal: dsSetVolume()
*/

void dsSetVolume(float Volume)
{
   if (glPrimaryBuffer) {
      if (Volume <= 1) IDirectSoundBuffer_SetVolume(glPrimaryBuffer, -10000); /* zero volume */
      else IDirectSoundBuffer_SetVolume(glPrimaryBuffer, (DWORD)(Volume * 50)-5000);
   }
}

/*****************************************************************************
** Internal: sndCheckActivity()
*/

int sndCheckActivity(struct PlatformData *Sound)
{
   DWORD status, pos;

   if (!glDirectSound) return -1;

   if (!Sound->SoundBuffer) return -1; /* Error */

   if (IDirectSoundBuffer_GetStatus(Sound->SoundBuffer, &status) IS DS_OK) {
      if (status & DSBSTATUS_PLAYING) {
         /* If this is a stream, check if the playback position has passed the end of the sample */

         if (Sound->Stream) {
            pos = sndGetPosition(Sound);
            if (pos >= Sound->SampleLength) {
               IDirectSoundBuffer_Stop(Sound->SoundBuffer); /* Ensure that the sample is terminated */
               return 0;
            }
         }

         return 1;
      }
      else return 0;
   }
   else return -1; /* Error */
}

/*****************************************************************************
** Internal: sndCreateBuffer()
*/

unsigned char * sndCreateBuffer(struct rkSound *File, void *Wave, int BufferLength, int SampleLength, struct PlatformData *Sound, int Stream)
{
   DSBUFFERDESC dsbdesc;
   int length1, i;
   unsigned char *write1;

   if (!glDirectSound) return 0;

   Sound->File         = File;
   Sound->SampleLength = SampleLength;
   Sound->BufferLength = BufferLength;
   Sound->Position     = 0;
   Sound->PlayPosition = 0;
   Sound->Stream       = Stream;
   Sound->Fill         = FILL_FIRST; /* First half waiting to be filled */

   ZeroMemory(&dsbdesc, sizeof(DSBUFFERDESC));
   dsbdesc.dwSize        = sizeof(DSBUFFERDESC);
   dsbdesc.dwFlags       = DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_GLOBALFOCUS|DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLPAN|DSBCAPS_CTRLFREQUENCY;
   dsbdesc.dwBufferBytes = BufferLength;
   dsbdesc.lpwfxFormat   = Wave;
   if ((IDirectSound_CreateSoundBuffer(glDirectSound, &dsbdesc, &Sound->SoundBuffer, NULL) != DS_OK) OR (!Sound->SoundBuffer)) {
      sndFree(Sound);
      return "CreateSoundBuffer() failed to create WAVE audio buffer.";
   }

   /* Fill the buffer with audio content (unless it is streamed, in
   ** which case it will be filled when play occurs).
   */

   if (IDirectSoundBuffer_Lock(Sound->SoundBuffer, 0, BufferLength, (void *)&write1, (DWORD *)&length1, NULL, NULL, 0) IS DS_OK) {
      if (Sound->Stream) {
         for (i=0; i < length1; i++) write1[i] = 0;
      }
      else Sound->Position = ReadData(File, write1, length1);
      IDirectSoundBuffer_Unlock(Sound->SoundBuffer, write1, length1, 0, 0);
   }

   return NULL;
}

/*****************************************************************************
** Internal: sndFree()
*/

void sndFree(struct PlatformData *Info)
{
   if (!glDirectSound) return;

   if (Info->SoundBuffer) {
      /*** Stop the playback buffer ***/
      IDirectSoundBuffer_Stop(Info->SoundBuffer);

      /*** Free the playback buffer ***/
      IDirectSoundBuffer_Release(Info->SoundBuffer);
      Info->SoundBuffer = NULL;
   }
}

/*****************************************************************************
** Internal: sndFrequency()
*/

void sndFrequency(struct PlatformData *Sound, int Frequency)
{
   if (!glDirectSound) return;
   if (Sound->SoundBuffer) IDirectSoundBuffer_SetFrequency(Sound->SoundBuffer, Frequency);
}

/*****************************************************************************
** Internal: sndPan()
*/

void sndPan(struct PlatformData *Sound, float Pan)
{
   if (!glDirectSound) return;

   if (Sound->SoundBuffer) IDirectSoundBuffer_SetPan(Sound->SoundBuffer, (DWORD)(Pan * 100));
}

/*****************************************************************************
** Internal: sndStop()
*/

void sndStop(struct PlatformData *Sound)
{
   if (!glDirectSound) return;

   if (Sound->SoundBuffer) IDirectSoundBuffer_Stop(Sound->SoundBuffer);
}

/*****************************************************************************
** Internal: sndPlay()
*/

void sndPlay(struct PlatformData *Sound, int Loop, int Offset)
{
   int length1;
   unsigned char *write1;

   if ((!Sound) OR (!Sound->SoundBuffer)) return;

   if (Offset < 0) Offset = 0;
   if (Offset >= Sound->SampleLength) return;

   Sound->Loop = Loop;
   Sound->SampleEnd = 0;

   if (Sound->Stream) {
      /*** Streamed samples require that we reload sound data from scratch ***/

      IDirectSoundBuffer_Stop(Sound->SoundBuffer);

      SeekZero(Sound->File);
      Sound->Fill = FILL_FIRST;
      Sound->Stop = 0;
      Sound->Position = 0;

      if (IDirectSoundBuffer_Lock(Sound->SoundBuffer, 0, Sound->BufferLength, (void *)&write1, (DWORD *)&length1, 0, 0, 0) IS DS_OK) {
         ZeroMemory(write1, length1); /* Clear the buffer */
         SeekData(Sound->File, Offset);
         Sound->Position = Offset + ReadData(Sound->File, write1, length1);
         Sound->PlayPosition = Offset;
         IDirectSoundBuffer_Unlock(Sound->SoundBuffer, write1, length1, 0, 0);
      }

      /*** Reset the play position within the buffer to zero ***/

      IDirectSoundBuffer_SetCurrentPosition(Sound->SoundBuffer, 0);
   }
   else {
      /*** For non-streamed samples, start the play position from the proposed offset ***/
      IDirectSoundBuffer_Stop(Sound->SoundBuffer);
      IDirectSoundBuffer_SetCurrentPosition(Sound->SoundBuffer, Offset);
   }

   /*** Play the sound ***/

   if ((Sound->Loop) OR (Sound->Stream)) {
      IDirectSoundBuffer_Play(Sound->SoundBuffer, 0, 0, DSBPLAY_LOOPING);
   }
   else {
      IDirectSoundBuffer_Play(Sound->SoundBuffer, 0, 0, 0);
   }
}

/*****************************************************************************
** Internal: sndStreamAudio()
*/

int sndStreamAudio(struct PlatformData *Sound)
{
   int length, length2, bytesread, i, lockstart, locklength;
   unsigned char *write, *write2;

   if (!glDirectSound) return 0;

   if (!Sound->SoundBuffer) return 1;

   if (Sound->Fill IS FILL_NONE) return 1;

   /*** Get the current play position ***/

   if (IDirectSoundBuffer_GetCurrentPosition(Sound->SoundBuffer, &Sound->BufferPos, NULL) IS DS_OK) {
      lockstart = -1;
      locklength = 0;
      if ((Sound->Fill IS FILL_FIRST) AND (Sound->BufferPos >= Sound->BufferLength>>1)) {
         Sound->Fill = FILL_SECOND;
         lockstart  = 0;
         locklength = Sound->BufferLength>>1;
      }
      else if ((Sound->Fill IS FILL_SECOND) AND (Sound->BufferPos < Sound->BufferLength>>1)) {
         Sound->Fill = FILL_FIRST;
         lockstart  = Sound->BufferLength>>1;
         locklength = Sound->BufferLength - (Sound->BufferLength>>1);
      }

      if (lockstart != -1) {

         /* Set the SampleEnd to zero if the sample does not terminate
         ** in this current buffer segment.
         */

         if ((Sound->Stop > 1) AND (Sound->SampleEnd > 0)) Sound->SampleEnd = 0;

         /*** Load more data if we have entered the next audio buffer section ***/

         if (IDirectSoundBuffer_Lock(Sound->SoundBuffer, lockstart, locklength,(void *)&write, (DWORD *)&length, (void *)&write2, (DWORD *)&length2, 0) IS DS_OK) {

            if (Sound->Fill IS FILL_FIRST) Sound->PlayPosition += Sound->BufferLength; /* Every time the audio position restarts, increase the play position by the buffer length */

            bytesread = ReadData(Sound->File, write, length);
            Sound->Position += bytesread;

            /*** Check if all bytes have been read from the audio source ***/

            if (Sound->Position >= Sound->SampleLength) {
               /* All of the bytes have been read from the sample.  We will need
               ** to allow this buffer segment to play through, then we can terminate
               ** the playback.
               */

               if (Sound->Loop) {
                  SeekZero(Sound->File);
                  bytesread = ReadData(Sound->File, write + bytesread, length - bytesread);
                  Sound->Position = bytesread;
               }
               else {
                  if (!Sound->Stop) {
                     /* Set the SampleEnd to indicate where the sample will end
                     ** within the buffer.
                     */
                     Sound->SampleEnd = lockstart + bytesread;
                  }

                  Sound->Stop++;

                  /*** Clear trailing data for a clean exit ***/

                  for (i=bytesread; i < length; i++) write[i] = 0;
               }
            }

            IDirectSoundBuffer_Unlock(Sound->SoundBuffer, write, length, write2, length2);
         }
      }

      /*** Send a stop signal if we have reached the end of the sample data ***/

      if ((!Sound->Loop) AND (Sound->Stop > 1) AND (Sound->BufferPos >= Sound->SampleEnd)) {
         IDirectSoundBuffer_Stop(Sound->SoundBuffer);
         return 1;
      }
      else return 0;
   }
   else return 1;
}

/*****************************************************************************
** Internal: sndVolume()
*/

void sndVolume(struct PlatformData *Sound, float Volume)
{
   if (!glDirectSound) return;

   if (Sound->SoundBuffer) {
      if (Volume <= 1) IDirectSoundBuffer_SetVolume(Sound->SoundBuffer, -10000); /* zero volume */
      else IDirectSoundBuffer_SetVolume(Sound->SoundBuffer, (DWORD)(Volume * 50)-5000);
   }
}

/*****************************************************************************
** Internal: sndGetPosition()
*/

LONG sndGetPosition(struct PlatformData *Sound)
{
   DWORD position;

   if (!glDirectSound) return 0;

   if (Sound->SoundBuffer) {
      IDirectSoundBuffer_GetCurrentPosition(Sound->SoundBuffer, &position, NULL);
      return Sound->PlayPosition + position;
   }
   else return 0;
}
