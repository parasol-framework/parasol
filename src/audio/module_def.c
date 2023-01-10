// Auto-generated by idl-c.fluid

#ifdef  __cplusplus
extern "C" {
#endif

static ERROR sndStartDrivers();
static ERROR sndWaitDrivers(LONG TimeOut);
static LONG sndSetChannels(LONG Total);

#ifdef  __cplusplus
}
#endif
#ifndef FDEF
#define FDEF static const struct FunctionField
#endif

FDEF argsSetChannels[] = { { "Result", FD_LONG }, { "Total", FD_LONG }, { 0, 0 } };
FDEF argsStartDrivers[] = { { "Error", FD_LONG|FD_ERROR }, { 0, 0 } };
FDEF argsWaitDrivers[] = { { "Error", FD_LONG|FD_ERROR }, { "TimeOut", FD_LONG }, { 0, 0 } };

const struct Function glFunctions[] = {
   { (APTR)sndStartDrivers, "StartDrivers", argsStartDrivers },
   { (APTR)sndWaitDrivers, "WaitDrivers", argsWaitDrivers },
   { (APTR)sndSetChannels, "SetChannels", argsSetChannels },
   { NULL, NULL, NULL }
};

#undef MOD_IDL
#define MOD_IDL "s.AudioSample:pData,lStreamID,lSampleLength,lLoop1Start,lLoop1End,lLoop2Start,lLoop2End,lSeekStart,lStreamLength,lBufferLength,lStreamPos,ucSampleType,cLoopMode,cLoop1Type,cLoop2Type,cFree,cUsed\ns.AudioChannel:eSample:AudioSample,dLVolume,dRVolume,dLVolumeTarget,dRVolumeTarget,lSoundID,lSampleHandle,lFlags,ulPosition,ulFrequency,uwPositionLow,wVolume,cPriority,cState[CHS],cLoopIndex,cPan\ns.AudioCommand:lCommandID,lHandle,lData\ns.ChannelSet:pChannel:AudioChannel,pCommands:AudioCommand,lUpdateRate,lMixLeft,wTotal,wActual,wTotalCommands,wPosition\ns.VolumeCtl:wSize,cName[32],lFlags,fChannels[1]\ns.AudioLoop:wLoopMode,cLoop1Type[LTYPE],cLoop2Type[LTYPE],lLoop1Start,lLoop1End,lLoop2Start,lLoop2End\nc.ADF:AUTO_SAVE=0x20,FILTER_HIGH=0x4,FILTER_LOW=0x2,OVER_SAMPLING=0x1,SERVICE_MODE=0x80,STEREO=0x8,SYSTEM_WIDE=0x40,VOL_RAMPING=0x10\nc.CHF:BACKWARD=0x2,CHANGED=0x8,MUTE=0x1,VOL_RAMP=0x4\nc.CHS:FADE_OUT=0x4,FINISHED=0x1,PLAYING=0x2,RELEASED=0x3,STOPPED=0x0\nc.CMD:CONTINUE=0x10,END_SEQUENCE=0x2,FADE_IN=0xc,FADE_OUT=0xd,MUTE=0xe,PLAY=0xb,SET_FREQUENCY=0x6,SET_LENGTH=0xf,SET_PAN=0x5,SET_POSITION=0xa,SET_RATE=0x7,SET_SAMPLE=0x3,SET_VOLUME=0x4,START_SEQUENCE=0x1,STOP=0x8,STOP_LOOPING=0x9\nc.LOOP:AMIGA=0x5,AMIGA_NONE=0x4,DOUBLE=0x3,SINGLE=0x1,SINGLE_RELEASE=0x2\nc.LTYPE:BIDIRECTIONAL=0x2,UNIDIRECTIONAL=0x1\nc.NOTE:A=0x9,AS=0xa,B=0xb,C=0x0,CS=0x1,D=0x2,DS=0x3,E=0x4,F=0x5,FS=0x6,G=0x7,GS=0x8,OCTAVE=0xc\nc.SDF:LOOP=0x1,NEW=0x2,NOTE=0x80000000,QUERY=0x4,RESTRICT_PLAY=0x20,STEREO=0x8,STREAM=0x40000000,TERMINATE=0x10\nc.SFM:BIG_ENDIAN=0x80000000,END=0x5,S16_BIT_MONO=0x2,S16_BIT_STEREO=0x4,U8_BIT_MONO=0x1,U8_BIT_STEREO=0x3\nc.STREAM:ALWAYS=0x3,NEVER=0x1,SMART=0x2\nc.SVF:CAPTURE=0x10000,MUTE=0x100,SYNC=0x100000,UNMUTE=0x1000,UNSYNC=0x1000000\nc.VCF:CAPTURE=0x10,JOINED=0x100,MONO=0x1000,MUTE=0x10000,PLAYBACK=0x1,SYNC=0x100000\n"
