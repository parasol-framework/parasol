
enum {
   AT_TRANSLATE=1, AT_SCALE, AT_ROTATE, AT_SKEW_X, AT_SKEW_Y
};

constexpr LONG MAX_VALUES = 8;

struct svgAnimation {
   std::vector<std::string> Values;
   struct VectorMatrix *Matrix = NULL; // Exclusive transform matrix for animation.
   OBJECTID TargetVector = 0;
   std::string TargetAttribute; // Name of the target attribute affected by the From and To values.
   std::string ID;              // Identifier for the animation
   DOUBLE Duration = 0;         // Measured in seconds, anything < 0 means infinite.
   DOUBLE MinDuration = 0;      // The minimum value of the active duration.  If zero, the active duration is not constrained.
   DOUBLE MaxDuration = 0;      // The maximum value of the active duration.
   DOUBLE RepeatDuration = 0;   // The animation will be allowed to repeat for up to the number of seconds indicated.  The time includes the initial loop.
   DOUBLE End = 0;
   LARGE  FirstTime = 0;  // Time-stamp of the first iteration
   LARGE  StartTime = 0;  // This is time-stamped once the animation has started (the first begin event is hit)
   LARGE  EndTime = 0;    // This is time-stamped once the animation has finished all of its cycles (including repetitions)
   LONG   RepeatCount;    // Repetition count.  Anything < 0 means infinite.
   LONG   RepeatIndex;    // Current index within the repeat cycle.
   UBYTE  Transform = 0;
   UBYTE  Restart = 0;
   bool   Freeze = false;
   bool   Replace = false;
   bool   Accumulate = false;
};

enum {
   RST_ALWAYS=0,
   RST_WHEN_NOT_ACTIVE,
   RST_NEVER
};
