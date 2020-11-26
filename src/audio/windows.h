
/*****************************************************************************
** Hardware/platform related functions.
*/

int sndCheckActivity(struct PlatformData *);
const char * sndCreateBuffer(struct rkSound *, void *, int, int, struct PlatformData *, int);
void sndFree(struct PlatformData *);
void sndFrequency(struct PlatformData *, int);
LONG sndGetPosition(struct PlatformData *);
const char * sndInitialiseAudio(void);
void sndPan(struct PlatformData *, float);
void sndPlay(struct PlatformData *, int, int);
void sndReleaseAudio(void);
void sndStop(struct PlatformData *);
int sndStreamAudio(struct PlatformData *);
void sndVolume(struct PlatformData *, float);
