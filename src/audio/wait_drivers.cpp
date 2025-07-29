/*********************************************************************************************************************

This is an internal function used by the audio server to wait for audio drivers to start.  It does not return until the
drivers have been initialised or the indicated TimeOut has expired.

*********************************************************************************************************************/

static ERR sndWaitDrivers(LONG TimeOut)
{
#ifdef ALSA_ENABLED
   pf::Log log(__FUNCTION__);

   log.branch("Waiting for audio drivers to start...");

   snd_ctl_card_info_alloca(&info);

   bool genuine = false;
   LONG card = -1;
   LARGE time = PreciseTime();
   while (PreciseTime() - time < LARGE(TimeOut) * 1000LL) {
      card = -1;
      snd_card_next(&card);
      if (card >= 0) {
         // Sound card detected.  Ignore modems, we are only interested in genuine soundcards.

         genuine = false;
         while (card >= 0) {
            auto name = "hw:" + std::to_string(card);

            snd_ctl_t *ctlhandle;
            snd_ctl_card_info_t *info;
            if (auto err = snd_ctl_open(&ctlhandle, name.c_str(), 0); err >= 0) {
               if ((err = snd_ctl_card_info(ctlhandle, info)) >= 0) {
                  auto cardid = std::string_view(snd_ctl_card_info_get_id(info));
                  log.msg("Detected %s", cardid.data());

                  if (!iequals("modem", cardid)) genuine = true;
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
      return ERR::Failed;
   }

   return ERR::Okay;
#else
   return ERR::Okay;
#endif
}
