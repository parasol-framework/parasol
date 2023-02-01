
/*****************************************************************************
** Hardware/platform related functions.
*/

int sndCheckActivity(struct PlatformData *);
const char * sndCreateBuffer(struct BaseClass *, void *, int, int, struct PlatformData *, int);
void sndFree(struct PlatformData *);
void sndFrequency(struct PlatformData *, int);
void sndSetPosition(struct PlatformData *, int);
const char * sndInitialiseAudio(void);
void sndPan(struct PlatformData *, float);
int sndPlay(struct PlatformData *, bool, int);
void sndReleaseAudio(void);
void sndStop(struct PlatformData *);
int sndStreamAudio(struct PlatformData *);
void sndVolume(struct PlatformData *, float);
void sndLength(struct PlatformData *, int);
