#ifdef ALSA_ENABLED

//********************************************************************************************************************

static void free_alsa(extAudio *Self)
{
   if (Self->sndlog) { snd_output_close(Self->sndlog); Self->sndlog = NULL; }
   if (Self->Handle) { snd_pcm_close(Self->Handle); Self->Handle = NULL; }
   if (Self->MixHandle) { snd_mixer_close(Self->MixHandle); Self->MixHandle = NULL; }
   if (Self->AudioBuffer) { FreeResource(Self->AudioBuffer); Self->AudioBuffer = NULL; }
}

//********************************************************************************************************************

static ERROR init_audio(extAudio *Self)
{
   parasol::Log log(__FUNCTION__);
   struct sndSetVolume setvol;
   snd_pcm_hw_params_t *hwparams;
   snd_pcm_stream_t stream;
   snd_ctl_t *ctlhandle;
   snd_pcm_t *pcmhandle;
   snd_mixer_elem_t *elem;
   snd_mixer_selem_id_t *sid;
   snd_pcm_uframes_t periodsize, buffersize;
   snd_ctl_card_info_t *info;
   LONG err, index;
   WORD channel;
   long pmin, pmax;
   int dir;
   WORD voltotal;
   std::string pcm_name;

   if (Self->Handle) {
      log.msg("Audio system is already active.");
      return ERR_Okay;
   }

   snd_ctl_card_info_alloca(&info);

   log.msg("Initialising sound card device.");

   // If 'plughw:0,0' is used, we get ALSA's software mixer, which allows us to set any kind of output options.
   // If 'hw:0,0' is used, we get precise hardware information.  Otherwise stick to 'default'.

   if (!Self->Device.empty()) pcm_name = Self->Device;
   else pcm_name = "default";

   // Convert english pcm_name to the device number

   if (StrMatch("default", pcm_name) != ERR_Okay) {
      STRING cardid, cardname;
      LONG card;

      card = -1;
      if ((snd_card_next(&card) < 0) or (card < 0)) {
         log.warning("There are no sound cards supported by audio drivers.");
         return ERR_NoSupport;
      }

      while (card >= 0) {
         std::string name = "hw:" + std::to_string(card);

         if ((err = snd_ctl_open(&ctlhandle, name.c_str(), 0)) >= 0) {
            if ((err = snd_ctl_card_info(ctlhandle, info)) >= 0) {
               cardid = (STRING)snd_ctl_card_info_get_id(info);
               cardname = (STRING)snd_ctl_card_info_get_name(info);

               if (!StrMatch(cardid, pcm_name)) {
                  pcm_name = name;
                  snd_ctl_close(ctlhandle);
                  break;
               }
            }
            snd_ctl_close(ctlhandle);
         }
         if (snd_card_next(&card) < 0) card = -1;
      }
   }

   // Check if the default ALSA device is a real sound card.  We don't want to use it if it's a modem or other
   // unexpected device.

   if (!StrMatch("default", pcm_name)) {
      snd_mixer_t *mixhandle;
      STRING cardid, cardname;
      WORD volmax;
      LONG card;

      // If there are no sound devices in the system, abort

      card = -1;
      if ((snd_card_next(&card) < 0) or (card < 0)) {
         log.warning("There are no sound cards supported by audio drivers.");
         return ERR_NoSupport;
      }

      // Check the number of mixer controls for all cards that support output.  We'll choose the card that has the most
      // mixer controls as the default.

      volmax = 0;
      while (card >= 0) {
         std::string name = "hw:" + std::to_string(card);
         log.msg("Opening card %s", name.c_str());

         if ((err = snd_ctl_open(&ctlhandle, name.c_str(), 0)) >= 0) {
            if ((err = snd_ctl_card_info(ctlhandle, info)) >= 0) {

               cardid = (STRING)snd_ctl_card_info_get_id(info);
               cardname = (STRING)snd_ctl_card_info_get_name(info);

               log.msg("Identified card %s, name %s", cardid, cardname);

               if (!StrMatch("modem", cardid)) goto next_card;

               if ((err = snd_mixer_open(&mixhandle, 0)) >= 0) {
                  if ((err = snd_mixer_attach(mixhandle, name.c_str())) >= 0) {
                     if ((err = snd_mixer_selem_register(mixhandle, NULL, NULL)) >= 0) {
                        if ((err = snd_mixer_load(mixhandle)) >= 0) {
                           // Build a list of all available volume controls

                           snd_mixer_selem_id_alloca(&sid);
                           voltotal = 0;
                           for (elem=snd_mixer_first_elem(mixhandle); elem; elem=snd_mixer_elem_next(elem)) voltotal++;

                           log.msg("Card %s has %d mixer controls.", cardid, voltotal);

                           if (voltotal > volmax) {
                              volmax = voltotal;
                              StrCopy(cardid, Self->Device, sizeof(Self->Device));
                              pcm_name = name;
                           }
                        }
                        else log.warning("snd_mixer_load() %s", snd_strerror(err));
                     }
                     else log.warning("snd_mixer_selem_register() %s", snd_strerror(err));
                  }
                  else log.warning("snd_mixer_attach() %s", snd_strerror(err));
                  snd_mixer_close(mixhandle);
               }
               else log.warning("snd_mixer_open() %s", snd_strerror(err));
            }
next_card:
            snd_ctl_close(ctlhandle);
         }
         if (snd_card_next(&card) < 0) card = -1;
      }
   }

   snd_output_stdio_attach(&Self->sndlog, stderr, 0);

   // If a mix handle is open from a previous Activate() attempt, close it

   if (Self->MixHandle) {
      snd_mixer_close(Self->MixHandle);
      Self->MixHandle = NULL;
   }

   // Mixer initialisation, for controlling volume

   if ((err = snd_mixer_open(&Self->MixHandle, 0)) < 0) {
      log.warning("snd_mixer_open() %s", snd_strerror(err));
      return ERR_Failed;
   }

   if ((err = snd_mixer_attach(Self->MixHandle, pcm_name)) < 0) {
      log.warning("snd_mixer_attach() %s", snd_strerror(err));
      return ERR_Failed;
   }

   if ((err = snd_mixer_selem_register(Self->MixHandle, NULL, NULL)) < 0) {
      log.warning("snd_mixer_selem_register() %s", snd_strerror(err));
      return ERR_Failed;
   }

   if ((err = snd_mixer_load(Self->MixHandle)) < 0) {
      log.warning("snd_mixer_load() %s", snd_strerror(err));
      return ERR_Failed;
   }

   // Build a list of all available volume controls

   snd_mixer_selem_id_alloca(&sid);
   voltotal = 0;
   for (elem=snd_mixer_first_elem(Self->MixHandle); elem; elem=snd_mixer_elem_next(elem)) voltotal++;

   log.msg("%d mixer controls have been reported by alsa.", voltotal);

   if (voltotal < 1) {
      log.warning("Aborting due to lack of mixers for the sound device.");
      return ERR_NoSupport;
   }

   std::vector<VolumeCtl> volctl;
   volctl.reserve(32);

   index = 0;
   for (elem=snd_mixer_first_elem(Self->MixHandle); elem; elem=snd_mixer_elem_next(elem)) {
      volctl.resize(volctl.size() + 1);

      snd_mixer_selem_get_id(elem, sid);
      if (!snd_mixer_selem_is_active(elem)) continue;

      if ((volctl[index].Flags & VCF::CAPTURE) != VCF::NIL) {
         snd_mixer_selem_get_capture_volume_range(elem, &pmin, &pmax);
      }
      else snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);

      if (pmin >= pmax) continue; // Ignore mixers with no range

      log.trace("Mixer Control '%s',%i", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));

      volctl[index].Name = snd_mixer_selem_id_get_name(sid);

      for (channel=0; channel < (LONG)volctl[index].Channels.size(); channel++) volctl[index].Channels[channel] = -1;

      VCF flags = VCF::NIL;
      if (snd_mixer_selem_has_playback_volume(elem))        flags |= VCF::PLAYBACK;
      if (snd_mixer_selem_has_capture_volume(elem))         flags |= VCF::CAPTURE;
      if (snd_mixer_selem_has_capture_volume_joined(elem))  flags |= VCF::JOINED;
      if (snd_mixer_selem_has_playback_volume_joined(elem)) flags |= VCF::JOINED;
      if (snd_mixer_selem_is_capture_mono(elem))            flags |= VCF::MONO;
      if (snd_mixer_selem_is_playback_mono(elem))           flags |= VCF::MONO;

      // Get the current channel volumes

      if ((flags & VCF::MONO) IS VCF::NIL) {
         for (channel=0; channel < ARRAYSIZE(glAlsaConvert); channel++) {
            if (snd_mixer_selem_has_playback_channel(elem, (snd_mixer_selem_channel_id_t)glAlsaConvert[channel]))   {
               long vol;
               snd_mixer_selem_get_playback_volume(elem, (snd_mixer_selem_channel_id_t)glAlsaConvert[channel], &vol);
               volctl[index].Channels[channel] = vol;
            }
         }
      }
      else volctl[index].Channels[0] = 0;

      // By default, input channels need to be muted.  This is because some rare PC's have been noted to cause high
      // pitched feedback, e.g. when the microphone channel is on.  All playback channels are enabled by default.

      if ((snd_mixer_selem_has_capture_switch(elem)) and (!snd_mixer_selem_has_playback_switch(elem))) {
         for (channel=0; channel < ARRAYSIZE(glAlsaConvert); channel++) {
            flags |= VCF::MUTE;
            snd_mixer_selem_set_capture_switch(elem, (snd_mixer_selem_channel_id_t)channel, 0);
         }
      }
      else if (snd_mixer_selem_has_playback_switch(elem)) {
         for (channel=0; channel < ARRAYSIZE(glAlsaConvert); channel++) {
            snd_mixer_selem_set_capture_switch(elem, (snd_mixer_selem_channel_id_t)channel, 1);
         }
      }

      volctl[index].Flags = flags;

      index++;
   }

   log.msg("Configured %d mixer controls.", index);

   snd_pcm_hw_params_alloca(&hwparams); // Stack allocation, no need to free it

   stream = SND_PCM_STREAM_PLAYBACK;
   if ((err = snd_pcm_open(&pcmhandle, pcm_name, stream, 0)) < 0) {
      log.warning("snd_pcm_open(%s) %s", pcm_name, snd_strerror(err));
      return ERR_Failed;
   }

   // Set access type, either SND_PCM_ACCESS_RW_INTERLEAVED or SND_PCM_ACCESS_RW_NONINTERLEAVED.

   if ((err = snd_pcm_hw_params_any(pcmhandle, hwparams)) < 0) {
      log.warning("Broken configuration for this PCM: no configurations available");
      return ERR_Failed;
   }

   if ((err = snd_pcm_hw_params_set_access(pcmhandle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
      log.warning("set_access() %d %s", err, snd_strerror(err));
      return ERR_Failed;
   }

   // Set the preferred audio bit format

   if (Self->BitDepth IS 16) {
      if ((err = snd_pcm_hw_params_set_format(pcmhandle, hwparams, SND_PCM_FORMAT_S16_LE)) < 0) {
         log.warning("set_format(16) %s", snd_strerror(err));
         return ERR_Failed;
      }
   }
   else if ((err = snd_pcm_hw_params_set_format(pcmhandle, hwparams, SND_PCM_FORMAT_U8)) < 0) {
      log.warning("set_format(8) %s", snd_strerror(err));
      return ERR_Failed;
   }

   // Retrieve the bit rate from alsa

   snd_pcm_format_t bitformat;
   snd_pcm_hw_params_get_format(hwparams, &bitformat);

   switch (bitformat) {
      case SND_PCM_FORMAT_S16_LE:
      case SND_PCM_FORMAT_S16_BE:
      case SND_PCM_FORMAT_U16_LE:
      case SND_PCM_FORMAT_U16_BE:
         Self->BitDepth = 16;
         break;

      case SND_PCM_FORMAT_S8:
      case SND_PCM_FORMAT_U8:
         Self->BitDepth = 8;
         break;

      default:
         log.warning("Hardware uses an unsupported audio format.");
         return ERR_Failed;
   }

   log.msg("ALSA bit rate: %d", Self->BitDepth);

   // Set the output rate to the rate that we are using internally.  ALSA will use the nearest possible rate allowed
   // by the hardware.

   dir = 0;
   if ((err = snd_pcm_hw_params_set_rate_near(pcmhandle, hwparams, (ULONG *)&Self->OutputRate, &dir)) < 0) {
      log.warning("set_rate_near() %s", snd_strerror(err));
      return ERR_Failed;
   }

   // Set number of channels

   ULONG channels = (Self->Flags & ADF_STEREO) ? 2 : 1;
   if ((err = snd_pcm_hw_params_set_channels_near(pcmhandle, hwparams, &channels)) < 0) {
      log.warning("set_channels_near(%d) %s", channels, snd_strerror(err));
      return ERR_Failed;
   }

   if (channels IS 2) Self->Stereo = true;
   else Self->Stereo = false;

   snd_pcm_uframes_t periodsize_min, periodsize_max;
   snd_pcm_uframes_t buffersize_min, buffersize_max;

   snd_pcm_hw_params_get_buffer_size_min(hwparams, &buffersize_min);
   snd_pcm_hw_params_get_buffer_size_max(hwparams, &buffersize_max);

   dir = 0;
   snd_pcm_hw_params_get_period_size_min(hwparams, &periodsize_min, &dir);

   dir = 0;
   snd_pcm_hw_params_get_period_size_max(hwparams, &periodsize_max, &dir);

   // NOTE: Audio buffersize is measured in samples, not bytes

   if (!Self->AudioBufferSize) buffersize = DEFAULT_BUFFER_SIZE;
   else buffersize = Self->AudioBufferSize;

   if (buffersize > buffersize_max) buffersize = buffersize_max;
   else if (buffersize < buffersize_min) buffersize = buffersize_min;

   periodsize = buffersize / 4;
   if (periodsize < periodsize_min) periodsize = periodsize_min;
   else if (periodsize > periodsize_max) periodsize = periodsize_max;
   buffersize = periodsize * 4;

   // Set buffer sizes.  Note that we will retrieve the period and buffer sizes AFTER telling ALSA what the audio
   // parameters are.

   log.msg("Using period frame size of %d, buffer size of %d", (LONG)periodsize, (LONG)buffersize);

   if ((err = snd_pcm_hw_params_set_period_size_near(pcmhandle, hwparams, &periodsize, 0)) < 0) {
      log.warning("Period size failure: %s", snd_strerror(err));
      return ERR_Failed;
   }

   if ((err = snd_pcm_hw_params_set_buffer_size_near(pcmhandle, hwparams, &buffersize)) < 0) {
      log.warning("Buffer size failure: %s", snd_strerror(err));
      return ERR_Failed;
   }

   // ALSA device initialisation

   if ((err = snd_pcm_hw_params(pcmhandle, hwparams)) < 0) {
      log.warning("snd_pcm_hw_params() %s", snd_strerror(err));
      return ERR_Failed;
   }

   if ((err = snd_pcm_prepare(pcmhandle)) < 0) {
      log.warning("snd_pcm_prepare() %s", snd_strerror(err));
      return ERR_Failed;
   }

   // Retrieve ALSA buffer sizes

   err = snd_pcm_hw_params_get_periods(hwparams, (ULONG *)&Self->Periods, &dir);

   snd_pcm_hw_params_get_period_size(hwparams, &periodsize, 0);
   Self->PeriodSize = periodsize;

   // Note that ALSA reports the audio buffer size in samples, not bytes

   snd_pcm_hw_params_get_buffer_size(hwparams, &buffersize);
   Self->AudioBufferSize = buffersize;

   if (Self->Stereo) Self->AudioBufferSize = Self->AudioBufferSize<<1;
   if (Self->BitDepth IS 16) Self->AudioBufferSize = Self->AudioBufferSize<<1;

   log.msg("Total Periods: %d, Period Size: %d, Buffer Size: %d (bytes)", Self->Periods, Self->PeriodSize, Self->AudioBufferSize);

   // Allocate a buffer that we will use for audio output

   if (Self->AudioBuffer) { FreeResource(Self->AudioBuffer); Self->AudioBuffer = NULL; }

   if (!AllocMemory(Self->AudioBufferSize, MEM_DATA, &Self->AudioBuffer)) {
      #ifdef DEBUG
         snd_pcm_hw_params_dump(hwparams, log);
      #endif

      if (Self->Flags & ADF_SYSTEM_WIDE) {
         log.msg("Applying user configured volumes.");

         auto oldctl = Self->Volumes;
         Self->Volumes = volctl;

         for (LONG i=0; i < (LONG)volctl.size(); i++) {
            LONG j;
            for (j=0; j < (LONG)oldctl.size(); j++) {
               if (volctl[i].Name == oldctl[j].Name) {
                  setvol.Index   = i;
                  setvol.Name    = NULL;
                  setvol.Flags   = 0;
                  setvol.Volume  = oldctl[j].Channels[0];
                  if ((oldctl[j].Flags & VCF::MUTE) != VCF::NIL) setvol.Flags |= SVF_MUTE;
                  else setvol.Flags |= SVF_UNMUTE;
                  Action(MT_SndSetVolume, Self, &setvol);
                  break;
               }
            }

            // If the user has no volume defined for a mixer, set our own.

            if (j IS (LONG)oldctl.size()) {
               setvol.Index   = i;
               setvol.Name    = NULL;
               setvol.Flags   = 0;
               setvol.Volume  = 0.8;
               Action(MT_SndSetVolume, Self, &setvol);
            }
         }
      }
      else {
         log.msg("Skipping preset volumes.");
         Self->Volumes = volctl;
      }

      // Free existing volume measurements and apply the information that we read from alsa.

      Self->Handle = pcmhandle;
   }
   else {
      return log.warning(ERR_AllocMemory);
   }

   return ERR_Okay;
}

#endif
