
enum {
   AT_TRANSLATE=1, AT_SCALE, AT_ROTATE, AT_SKEW_X, AT_SKEW_Y
};

#define MAX_VALUES 8

struct svgAnimation {
   struct svgAnimation *Next;
   struct VectorMatrix *Matrix; // Exclusive transform matrix for animation.
   OBJECTID TargetVector;
   CSTRING TargetAttribute; // Name of the target attribute affected by the From and To values.
   CSTRING ID;              // Identifier for the animation
   DOUBLE Duration;         // Measured in seconds, anything < 0 means infinite.
   DOUBLE MinDuration;      // The minimum value of the active duration.  If zero, the active duration is not constrained.
   DOUBLE MaxDuration;      // The maximum value of the active duration.
   DOUBLE RepeatDuration;   // The animation will be allowed to repeat for up to the number of seconds indicated.  The time includes the initial loop.
   CSTRING Values[MAX_VALUES];
   LONG ValueCount;
   DOUBLE End;
   UBYTE  Transform;
   LONG   RepeatCount;    // Repetition count.  Anything < 0 means infinite.
   UBYTE  Freeze:1;
   UBYTE  Replace:1;
   UBYTE  Accumulate:1;
   LARGE  FirstTime;      // Time-stamp of the first iteration
   LARGE  StartTime;      // This is time-stamped once the animation has started (the first begin event is hit)
   LARGE  EndTime;        // This is time-stamped once the animation has finished all of its cycles (including repetitions)
   UBYTE  Restart;
   LONG   RepeatIndex;    // Current index within the repeat cycle.
};

enum {
   RST_ALWAYS=0,
   RST_WHEN_NOT_ACTIVE,
   RST_NEVER
};
