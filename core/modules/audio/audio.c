/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-MODULE-
Audio: Audio support is managed by this module.

The audio module exports a small number of functions that impact audio on a global basis.  For extensive audio support,
please refer to the @Audio class.

-END-

*****************************************************************************/

#define PRV_AUDIO
#define PRV_AUDIO_MODULE
#define PRV_SOUND

#ifdef __linux__
 #include <sys/ioctl.h>
 #include <fcntl.h>
 #ifdef __ANDROID__
  #include <linux/soundcard.h>
  #include "kd.h"
 #else
  #include <sys/soundcard.h>
  #include <sys/kd.h>
 #endif
 #include <unistd.h>
 #include <alsa/asoundlib.h> // Requires libasound2-dev
#endif

#include <parasol/main.h>
#include <parasol/modules/audio.h>

static ERROR CMDInit(OBJECTPTR, struct CoreBase *);
static ERROR CMDExpunge(void);
static ERROR CMDOpen(OBJECTPTR);

#include "module_def.c"

MODULE_COREBASE;
static OBJECTPTR clAudio = 0;
static OBJECTID glAudioID = 0;
static struct globalaudio *glAudio = NULL;
static DOUBLE glTaskVolume = 1.0;

#include "audio.h"

//****************************************************************************

static FLOAT MixLeftVolFloat, MixRightVolFloat;
static LONG  MixSrcPos, MixStep;
static UBYTE *glMixDest = NULL;
static UBYTE *MixSample = NULL;
static FLOAT *ByteFloatTable = NULL;
static APTR MixBuffer;          // DSM mixing buffer. Play() writes the mixed data here. Post-processing is usually necessary.
static MixRoutineSet MixMonoFloat;
static MixRoutineSet MixStereoFloat;
static MixRoutineSet MixMonoFloatInterp;
static MixRoutineSet MixStereoFloatInterp;
static LONG glMaxSoundChannels = 8;

#ifdef _WIN32
unsigned char * dsInitDevice(int);
void dsCloseDevice(void);
void dsClear(void);
LONG dsPlay(objAudio *);
void dsSetVolume(float);
#endif

LONG MixData(objAudio *, ULONG, void *);
ERROR GetMixAmount(objAudio *, LONG *);
static LONG SampleShift(LONG);
static ERROR RelocateMixingRoutine(MixRoutine *);
static LONG HandleSampleEnd(objAudio *, struct AudioChannel *);
ERROR add_audio_class(void);
ERROR add_sound_class(void);
void free_audio_class(void);
void free_sound_class(void);
static ERROR SetInternalVolume(objAudio *, struct AudioChannel *);
static void load_config(objAudio *);
static ERROR init_audio(objAudio *);

#ifdef __linux__
static void free_alsa(objAudio *);
static ERROR DropMixAmount(objAudio *, LONG);

static WORD glAlsaConvert[6] = {
   SND_MIXER_SCHN_FRONT_LEFT,   // Conversion table must follow the CHN_ order
   SND_MIXER_SCHN_FRONT_RIGHT,
   SND_MIXER_SCHN_FRONT_CENTER,
   SND_MIXER_SCHN_REAR_LEFT,
   SND_MIXER_SCHN_REAR_RIGHT,
   SND_MIXER_SCHN_WOOFER
};
#else
ERROR DropMixAmount(objAudio *, LONG);
#endif

#define MixLeft(a) (((100 * (LARGE)Self->OutputRate) / ((a) * 40)) + 1) & 0xfffffffe;

struct BufferCommand {
   WORD CommandID;
   ERROR (*Routine)(objAudio *Self, APTR);
};

static ERROR COMMAND_Continue(objAudio *, LONG);
static ERROR COMMAND_FadeIn(objAudio *, LONG);
static ERROR COMMAND_FadeOut(objAudio *, LONG);
static ERROR COMMAND_Play(objAudio *, LONG, LONG);
static ERROR COMMAND_SetFrequency(objAudio *, LONG, ULONG);
static ERROR COMMAND_Mute(objAudio *, LONG, LONG);
static ERROR COMMAND_SetLength(objAudio *, LONG, LONG);
static ERROR COMMAND_SetPan(objAudio *, LONG, LONG);
static ERROR COMMAND_SetPosition(objAudio *, LONG, LONG);
static ERROR COMMAND_SetRate(objAudio *, LONG, LONG);
static ERROR COMMAND_SetSample(objAudio *, LONG, LONG);
static ERROR COMMAND_SetVolume(objAudio *, LONG, LONG);
static ERROR COMMAND_Stop(objAudio *, LONG);
static ERROR COMMAND_StopLooping(objAudio *, LONG);

const struct BufferCommand glCommands[] = {
   { CMD_END_SEQUENCE,   NULL },
   { CMD_CONTINUE,       (APTR)COMMAND_Continue },
   { CMD_FADE_IN,        (APTR)COMMAND_FadeIn },
   { CMD_FADE_OUT,       (APTR)COMMAND_FadeOut },
   { CMD_PLAY,           (APTR)COMMAND_Play },
   { CMD_SET_FREQUENCY,  (APTR)COMMAND_SetFrequency },
   { CMD_MUTE,           (APTR)COMMAND_Mute },
   { CMD_SET_LENGTH,     (APTR)COMMAND_SetLength },
   { CMD_SET_PAN,        (APTR)COMMAND_SetPan },
   { CMD_SET_POSITION,   (APTR)COMMAND_SetPosition },
   { CMD_SET_RATE,       (APTR)COMMAND_SetRate },
   { CMD_SET_SAMPLE,     (APTR)COMMAND_SetSample },
   { CMD_SET_VOLUME,     (APTR)COMMAND_SetVolume },
   { CMD_START_SEQUENCE, NULL },
   { CMD_STOP,           (APTR)COMMAND_Stop },
   { CMD_STOP_LOOPING,   (APTR)COMMAND_StopLooping },
   { 0, NULL }
};

//****************************************************************************

#ifdef _WIN32 // Functions for use by dsound.c
int ReadData(objSound *Self, void *Buffer, int Length) {
   struct acRead read = { Buffer, Length };
   if (!Action(AC_Read, Self->File, &read)) return read.Result;
   return 0;
}

void SeekData(objSound *Self, int Offset) {
   struct acSeek seek = { Offset, SEEK_START };
   Action(AC_Seek, Self->File, &seek);
}

void SeekZero(objSound *Self) {
   struct acSeek seek = { Self->prvDataOffset, SEEK_START };
   Action(AC_Seek, Self->File, &seek);
}
#endif

//****************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

#ifdef _WIN32
   {
      STRING errstr;
      if ((errstr = dsInitDevice(44100))) {
         LogErrorMsg("DirectSound Failed: %s", errstr);
         return ERR_NoSupport;
      }
   }
#endif

   // Allocate public audio variables

   MEMORYID memid = RPM_Audio;
   ERROR error = AllocMemory(sizeof(struct globalaudio), MEM_UNTRACKED|MEM_RESERVED|MEM_PUBLIC|MEM_NO_BLOCKING, &glAudio, &memid);
   if (error IS ERR_ResourceExists) {
      if (AccessMemory(RPM_Audio, MEM_READ_WRITE, 2000, &glAudio) != ERR_Okay) {
         return ERR_AccessMemory;
      }
   }
   else if (error != ERR_Okay) return ERR_AllocMemory;
   else glAudio->Volume = 80;

   if (add_audio_class() != ERR_Okay) return ERR_AddClass;
   if (add_sound_class() != ERR_Okay) return ERR_AddClass;
   return ERR_Okay;
}

static ERROR CMDOpen(OBJECTPTR Module)
{
   SetPointer(Module, FID_FunctionList, glFunctions);
   return ERR_Okay;
}

static ERROR CMDExpunge(void)
{
   if (glAudio) { ReleaseMemory(glAudio); glAudio = NULL; }
   free_audio_class();
   free_sound_class();
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
StartDrivers: Starts the audio drivers (platform dependent).

This function will start the audio drivers if they have not already been loaded and initialised.  This feature is
platform specific and is typically used on boot-up only.

-ERRORS-
Okay

*****************************************************************************/

static ERROR sndStartDrivers(void)
{
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
SetChannels: Defines the maximum number of channels available for sound mixing.

The maximum number of sound channels used for software-based sound mixing can be altered by calling this function.  The
recommended number of channels is 8, which would indicate that a maximum of 8 sound samples could be played
simultaneously at any given time.

-INPUT-
int Total: The total number of sound channels required by the client.

-RESULT-
int: The previous setting for the maximum number of sound channels is returned.

*****************************************************************************/

static LONG sndSetChannels(LONG Total)
{
   LONG previous = glMaxSoundChannels;
   if (Total > 128) Total = 128;
   else if (Total < 1) Total = 1;
   glMaxSoundChannels = Total;
   return previous;
}

/*****************************************************************************

-FUNCTION-
SetTaskVolume: Set the default volume for the task (not global)

The default volume for the current task can be adjusted by calling this function.  The volume is expressed as a
percentage between 0 and 100 - if it is set to a value outside of this range then no adjustment is made to the current
volume.

-INPUT-
double Volume: Desired volume, between 0 and 100.

-RESULT-
double: The previous volume setting is returned by this function, regardless of whether the volume setting is successful or not.

*****************************************************************************/

static DOUBLE sndSetTaskVolume(DOUBLE Volume)
{
   if ((Volume < 0) OR (Volume > 100)) {
      return glTaskVolume * 100.0;
   }
   else {
      DOUBLE old = glTaskVolume;
      glTaskVolume = Volume * (1.0/100.0);

      #ifdef _WIN32
         dsSetVolume(Volume);
      #endif

      if (!glAudioID) {
         FastFindObject("SystemAudio", ID_AUDIO, &glAudioID, 1, NULL);
      }

      if (glAudioID) {
         objAudio *audio;
         if (!AccessObject(glAudioID, 3000, &audio)) {
            OBJECTID taskid = CurrentTaskID();
            LONG i;
            for (i=0; i < ARRAYSIZE(audio->Channels); i++) {
               if (audio->Channels[i].TaskID IS taskid) {
                  audio->Channels[i].TaskVolume = glTaskVolume;
               }
            }
            ReleaseObject(audio);
         }
      }

      return old;
   }
}

/*****************************************************************************

-FUNCTION-
WaitDrivers: Wait for audio drivers to become initialised on boot-up.

This is an internal function used by the audio server to wait for audio drivers to start.  It does not return until the
drivers have been initialised or the indicated TimeOut has expired.

-INPUT-
int TimeOut: The desired timeout value indicated in 1/1000ths of a second.

-ERRORS-
Okay
-END-

*****************************************************************************/

static ERROR sndWaitDrivers(LONG TimeOut)
{
#ifdef __linux__
   snd_ctl_t *ctlhandle;
   snd_ctl_card_info_t *info;
   LONG err;
   UBYTE name[32];
   STRING cardid;

   LogBranch("Waiting for audio drivers to start...");

   snd_ctl_card_info_alloca(&info);

   BYTE genuine = FALSE;
   LONG card = -1;
   LARGE time = PreciseTime();
   while (PreciseTime() - time < (LARGE)TimeOut * 1000LL) {
      card = -1;
      snd_card_next(&card);
      if (card >= 0) {
         // Sound card detected.  Ignore modems, we are only interested in genuine soundcards.

         genuine = FALSE;
         while (card >= 0) {
            StrFormat(name, sizeof(name), "hw:%d", card);

            if ((err = snd_ctl_open(&ctlhandle, name, 0)) >= 0) {
               if ((err = snd_ctl_card_info(ctlhandle, info)) >= 0) {
                  cardid = (STRING)snd_ctl_card_info_get_id(info);
                  LogMsg("Detected %s", cardid);

                  if (StrMatch("modem", cardid) != ERR_Okay) genuine = TRUE;
               }
               snd_ctl_close(ctlhandle);
            }
            if (snd_card_next(&card) < 0) card = -1;
         }

         if (genuine) break;
      }

      WaitTime(0, -100000);
   }

   if (!genuine) {
      LogMsg("No sound drivers were started in the allotted time period.");
      LogReturn();
      return ERR_Failed;
   }

   LogReturn();
   return ERR_Okay;
#else
   return ERR_Okay;
#endif
}

//****************************************************************************

#include "class_audio.c"
#include "commands.c"
#include "functions.c"
#include "class_sound.c"

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_AUDIO)
