
enum {
   AT_TRANSLATE=1, AT_SCALE, AT_ROTATE, AT_SKEW_X, AT_SKEW_Y
};

#define MAX_VALUES 8

struct svgAnimation {
   std::vector<std::string> Values;
   struct VectorMatrix *Matrix; // Exclusive transform matrix for animation.
   OBJECTID TargetVector;
   std::string TargetAttribute; // Name of the target attribute affected by the From and To values.
   std::string ID;              // Identifier for the animation
   DOUBLE Duration;         // Measured in seconds, anything < 0 means infinite.
   DOUBLE MinDuration;      // The minimum value of the active duration.  If zero, the active duration is not constrained.
   DOUBLE MaxDuration;      // The maximum value of the active duration.
   DOUBLE RepeatDuration;   // The animation will be allowed to repeat for up to the number of seconds indicated.  The time includes the initial loop.
   DOUBLE End;
   LARGE  FirstTime;      // Time-stamp of the first iteration
   LARGE  StartTime;      // This is time-stamped once the animation has started (the first begin event is hit)
   LARGE  EndTime;        // This is time-stamped once the animation has finished all of its cycles (including repetitions)
   LONG   RepeatCount;    // Repetition count.  Anything < 0 means infinite.
   LONG   RepeatIndex;    // Current index within the repeat cycle.
   UBYTE  Transform;
   UBYTE  Restart;
   bool   Freeze;
   bool   Replace;
   bool   Accumulate;
};

enum {
   RST_ALWAYS=0,
   RST_WHEN_NOT_ACTIVE,
   RST_NEVER
};
