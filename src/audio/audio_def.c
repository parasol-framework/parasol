// Auto-generated by idl-c.fluid

static const struct FieldDef clAudioFlags[] = {
   { "OverSampling", 0x00000001 },
   { "FilterLow", 0x00000002 },
   { "FilterHigh", 0x00000004 },
   { "Stereo", 0x00000008 },
   { "VolRamping", 0x00000010 },
   { "AutoSave", 0x00000020 },
   { "SystemWide", 0x00000040 },
   { NULL, 0 }
};

FDEF maOpenChannels[] = { { "Total", FD_LONG }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF maCloseChannels[] = { { "Handle", FD_LONG }, { 0, 0 } };
FDEF maAddSample[] = { { "OnStop", FD_FUNCTION }, { "SampleFormat", FD_LONG }, { "Data", FD_BUFFER|FD_PTR }, { "DataSize", FD_LONG|FD_BUFSIZE }, { "AudioLoop:Loop", FD_PTR|FD_STRUCT }, { "LoopSize", FD_LONG|FD_BUFSIZE }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF maRemoveSample[] = { { "Handle", FD_LONG }, { 0, 0 } };
FDEF maSetSampleLength[] = { { "Sample", FD_LONG }, { "Length", FD_LARGE }, { 0, 0 } };
FDEF maAddStream[] = { { "Callback", FD_FUNCTION }, { "OnStop", FD_FUNCTION }, { "SampleFormat", FD_LONG }, { "SampleLength", FD_LONG }, { "PlayOffset", FD_LONG }, { "AudioLoop:Loop", FD_PTR|FD_STRUCT }, { "LoopSize", FD_LONG|FD_BUFSIZE }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF maBeep[] = { { "Pitch", FD_LONG }, { "Duration", FD_LONG }, { "Volume", FD_LONG }, { 0, 0 } };
FDEF maSetVolume[] = { { "Index", FD_LONG }, { "Name", FD_STR }, { "Flags", FD_LONG }, { "Channel", FD_LONG }, { "Volume", FD_DOUBLE }, { 0, 0 } };

static const struct MethodEntry clAudioMethods[] = {
   { -1, (APTR)AUDIO_OpenChannels, "OpenChannels", maOpenChannels, sizeof(struct sndOpenChannels) },
   { -2, (APTR)AUDIO_CloseChannels, "CloseChannels", maCloseChannels, sizeof(struct sndCloseChannels) },
   { -3, (APTR)AUDIO_AddSample, "AddSample", maAddSample, sizeof(struct sndAddSample) },
   { -4, (APTR)AUDIO_RemoveSample, "RemoveSample", maRemoveSample, sizeof(struct sndRemoveSample) },
   { -5, (APTR)AUDIO_SetSampleLength, "SetSampleLength", maSetSampleLength, sizeof(struct sndSetSampleLength) },
   { -6, (APTR)AUDIO_AddStream, "AddStream", maAddStream, sizeof(struct sndAddStream) },
   { -7, (APTR)AUDIO_Beep, "Beep", maBeep, sizeof(struct sndBeep) },
   { -8, (APTR)AUDIO_SetVolume, "SetVolume", maSetVolume, sizeof(struct sndSetVolume) },
   { 0, 0, 0, 0, 0 }
};

static const struct ActionArray clAudioActions[] = {
   { AC_Activate, AUDIO_Activate },
   { AC_Deactivate, AUDIO_Deactivate },
   { AC_Free, AUDIO_Free },
   { AC_Init, AUDIO_Init },
   { AC_NewObject, AUDIO_NewObject },
   { AC_SaveSettings, AUDIO_SaveSettings },
   { AC_SaveToObject, AUDIO_SaveToObject },
   { 0, NULL }
};

