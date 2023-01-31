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

#ifdef _WIN32
#define USE_WIN32_PLAYBACK TRUE // All Sound objects get an independent DirectSound channel if enabled
#include "windows.h"

char * dsInitDevice(int);
void dsCloseDevice(void);
#endif

ERROR add_audio_class(void);
ERROR add_sound_class(void);
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

//********************************************************************************************************************

#include "alsa.cpp"
#include "functions.cpp"
#include "mixers.cpp"
#include "commands.cpp"
#include "class_audio.cpp"
#include "class_sound.cpp"

//********************************************************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MODVERSION_AUDIO)
