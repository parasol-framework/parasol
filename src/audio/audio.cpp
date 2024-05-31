/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-MODULE-
Audio: Audio support is managed by this module.

The audio module manages the @Audio and @Sound classes.  Functionality for sample mixing is also provided for
modifying playback on the fly.

Audio functionality and performance can differ between platforms.  On Linux, all audio samples are mixed ahead of time
and channeled through a single output.  On Windows this same feature is implemented, but where possible the @Sound
class will assign an independent channel to samples that are played.  This gives a slight edge in reducing lag when
playback is requested.  In general, Linux is considered the baseline implementation and other platforms should meet or
exceed its performance level.

For the general playback of audio samples, we strongly encourage use of the @Sound class.  Use the @Audio class and
its low-level mixer capabilities only if your needs are not met by the @Sound class.

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
#include <parasol/strings.hpp>
#include <sstream>
#include <algorithm>

static ERR MODInit(OBJECTPTR, struct CoreBase *);
static ERR MODExpunge(void);
static ERR MODOpen(OBJECTPTR);

#include "module_def.c"

JUMPTABLE_CORE
static OBJECTPTR glAudioModule = NULL;
static OBJECTPTR clAudio = 0;
static std::unordered_map<OBJECTID, LONG> glSoundChannels;
class extAudio;

ERR add_audio_class(void);
ERR add_sound_class(void);
void free_audio_class(void);
void free_sound_class(void);

extern "C" void end_of_stream(OBJECTPTR, LONG);

static void audio_stopped_event(extAudio &, LONG);
static ERR set_channel_volume(extAudio *, struct AudioChannel *);
static void load_config(extAudio *);
static ERR init_audio(extAudio *);
static ERR audio_timer(extAudio *, LARGE, LARGE);

#include "audio.h"

//********************************************************************************************************************

#ifdef _WIN32
#define USE_WIN32_PLAYBACK TRUE // All Sound objects get an independent DirectSound channel if enabled
#include "windows.h"

extern "C" char * dsInitDevice(int);
extern "C" void dsCloseDevice(void);
#endif

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

static ERR MODInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   pf::Log log;

   CoreBase = argCoreBase;
   glAudioModule = argModule;

#ifdef _WIN32
   {
      CSTRING errstr;
      if ((errstr = dsInitDevice(44100))) {
         log.warning("DirectSound Failed: %s", errstr);
         return ERR::NoSupport;
      }
   }
#elif ALSA_ENABLED
   // Nothing required for ALSA
#else
   log.warning("No audio support available.");
   return ERR::Failed;
#endif

   if (add_audio_class() != ERR::Okay) return ERR::AddClass;
   if (add_sound_class() != ERR::Okay) return ERR::AddClass;
   return ERR::Okay;
}

static ERR MODOpen(OBJECTPTR Module)
{
   Module->set(FID_FunctionList, glFunctions);
   return ERR::Okay;
}

static ERR MODExpunge(void)
{
   for (auto& [id, handle] : glSoundChannels) {
      // NB: Most Audio objects will be disposed of prior to this module being expunged.
      if (handle) {
         pf::ScopedObjectLock<extAudio> audio(id, 3000);
         if (audio.granted()) snd::CloseChannels(*audio, handle);
      }
   }
   glSoundChannels.clear();

   free_audio_class();
   free_sound_class();
   return ERR::Okay;
}

//********************************************************************************************************************

#include "alsa.cpp"
#include "functions.cpp"
#include "mixers.cpp"
#include "commands.cpp"
#include "class_audio.cpp"
#include "class_sound.cpp"

//********************************************************************************************************************

static STRUCTS glStructures = {
   { "AudioLoop", sizeof(AudioLoop) }
};

//********************************************************************************************************************

PARASOL_MOD(MODInit, NULL, MODOpen, MODExpunge, MOD_IDL, &glStructures)
extern "C" struct ModHeader * register_audio_module() { return &ModHeader; }
