/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-MODULE-
Audio: Audio support is managed by this module.

The audio module exports a small number of functions that impact audio on a global basis.  For extensive audio support,
please refer to the @Audio class.

-END-

*********************************************************************************************************************/

#define PRV_AUDIO_MODULE

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
 #ifdef ALSA_ENABLED
  #include <alsa/asoundlib.h> // Requires libasound2-dev
 #endif
#endif

#include <parasol/main.h>
#include <parasol/modules/audio.h>
#include <sstream>
#include <algorithm>

static ERROR CMDInit(OBJECTPTR, struct CoreBase *);
static ERROR CMDExpunge(void);
static ERROR CMDOpen(OBJECTPTR);

#include "module_def.c"

MODULE_COREBASE;
static OBJECTPTR glAudioModule = NULL;
static OBJECTPTR clAudio = 0;
static std::unordered_map<OBJECTID, LONG> glSoundChannels;

#include "audio.h"

//********************************************************************************************************************

static LONG glMaxSoundChannels = 8;

#ifdef _WIN32
#define USE_WIN32_PLAYBACK TRUE // If undefined, only the internal mixer is used.

char * dsInitDevice(int);
void dsCloseDevice(void);
void dsClear(void);
LONG dsMixer(extAudio *);
#endif

LONG mix_data(extAudio *, LONG, void *);
ERROR GetMixAmount(extAudio *, LONG *);
ERROR add_audio_class(void);
ERROR add_sound_class(void);
ERROR process_commands(extAudio *, LONG);
void free_audio_class(void);
void free_sound_class(void);
static ERROR set_channel_volume(extAudio *, struct AudioChannel *);
static void load_config(extAudio *);
static ERROR init_audio(extAudio *);
static ERROR audio_timer(extAudio *Self, LARGE Elapsed, LARGE CurrentTime);

#ifdef ALSA_ENABLED
static void free_alsa(extAudio *);

static const WORD glAlsaConvert[6] = {
   SND_MIXER_SCHN_FRONT_LEFT,   // Conversion table must follow the CHN_ order
   SND_MIXER_SCHN_FRONT_RIGHT,
   SND_MIXER_SCHN_FRONT_CENTER,
   SND_MIXER_SCHN_REAR_LEFT,
   SND_MIXER_SCHN_REAR_RIGHT,
   SND_MIXER_SCHN_WOOFER
};
#endif

//********************************************************************************************************************
// Sample shift - value used for converting total data size down to samples.

inline const LONG sample_shift(const LONG sampleType)
{
   switch (sampleType) {
      case SFM_U8_BIT_STEREO:
      case SFM_S16_BIT_MONO:
         return 1;

      case SFM_S16_BIT_STEREO:
         return 2;
   }
   return 0;
}

//********************************************************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   parasol::Log log;

   CoreBase = argCoreBase;
   glAudioModule = argModule;

#ifdef _WIN32
   {
      CSTRING errstr;
      if ((errstr = dsInitDevice(44100))) {
         log.warning("DirectSound Failed: %s", errstr);
         return ERR_NoSupport;
      }
   }
#elif ALSA_ENABLED
   // Nothing required for ALSA
#else
   log.warning("No audio support available.");
   return ERR_Failed;
#endif

   if (add_audio_class() != ERR_Okay) return ERR_AddClass;
   if (add_sound_class() != ERR_Okay) return ERR_AddClass;
   return ERR_Okay;
}

static ERROR CMDOpen(OBJECTPTR Module)
{
   Module->set(FID_FunctionList, glFunctions);
   return ERR_Okay;
}

static ERROR CMDExpunge(void)
{
   for (auto& [id, handle] : glSoundChannels) {
      // NB: Most Audio objects will be disposed of prior to this module being expunged.
      if (handle) {
         parasol::ScopedObjectLock<extAudio> audio(id, 3000);
         if (audio.granted()) sndCloseChannels(*audio, handle);
      }
   }
   glSoundChannels.clear();

   free_audio_class();
   free_sound_class();
   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
StartDrivers: Starts the audio drivers (platform dependent).

This function will start the audio drivers if they have not already been loaded and initialised.  This feature is
platform specific and is typically used on boot-up only.

-ERRORS-
Okay

*********************************************************************************************************************/

static ERROR sndStartDrivers(void)
{
   return ERR_Okay;
}

/*********************************************************************************************************************

-FUNCTION-
SetChannels: Defines the maximum number of channels available for sound mixing.

The maximum number of sound channels used for software-based sound mixing can be altered by calling this function.
The recommended number of channels is 8, which would indicate that a maximum of 8 sound samples could be played
simultaneously at any given time.

-INPUT-
int Total: The total number of sound channels required by the client.

-RESULT-
int: The previous setting for the maximum number of sound channels is returned.

*********************************************************************************************************************/

static LONG sndSetChannels(LONG Total)
{
   LONG previous = glMaxSoundChannels;
   if (Total > 128) Total = 128;
   else if (Total < 1) Total = 1;
   glMaxSoundChannels = Total;
   return previous;
}

/*********************************************************************************************************************

-FUNCTION-
WaitDrivers: Wait for audio drivers to become initialised on boot-up.

This is an internal function used by the audio server to wait for audio drivers to start.  It does not return until the
drivers have been initialised or the indicated TimeOut has expired.

-INPUT-
int TimeOut: The desired timeout value indicated in 1/1000ths of a second.

-ERRORS-
Okay
-END-

*********************************************************************************************************************/

static ERROR sndWaitDrivers(LONG TimeOut)
{
#ifdef ALSA_ENABLED
   parasol::Log log(__FUNCTION__);
   snd_ctl_t *ctlhandle;
   snd_ctl_card_info_t *info;
   LONG err;
   char name[32];
   STRING cardid;

   log.branch("Waiting for audio drivers to start...");

   snd_ctl_card_info_alloca(&info);

   bool genuine = false;
   LONG card = -1;
   LARGE time = PreciseTime();
   while (PreciseTime() - time < (LARGE)TimeOut * 1000LL) {
      card = -1;
      snd_card_next(&card);
      if (card >= 0) {
         // Sound card detected.  Ignore modems, we are only interested in genuine soundcards.

         genuine = false;
         while (card >= 0) {
            snprintf(name, sizeof(name), "hw:%d", card);

            if ((err = snd_ctl_open(&ctlhandle, name, 0)) >= 0) {
               if ((err = snd_ctl_card_info(ctlhandle, info)) >= 0) {
                  cardid = (STRING)snd_ctl_card_info_get_id(info);
                  log.msg("Detected %s", cardid);

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
      log.msg("No sound drivers were started in the allotted time period.");
      return ERR_Failed;
   }

   return ERR_Okay;
#else
   return ERR_Okay;
#endif
}

//********************************************************************************************************************

#include "alsa.cpp"
#include "functions.cpp"
#include "mixers.cpp"
#include "commands.cpp"
#include "class_audio.cpp"
#include "class_sound.cpp"

//********************************************************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_AUDIO)
