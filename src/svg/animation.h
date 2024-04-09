
enum class AT : char { // Transform Type
   TRANSLATE = 1, SCALE, ROTATE, SKEW_X, SKEW_Y
};

enum class ADD : char { // Additive
   SUM = 0, REPLACE
};

enum class ATT : char { // Attribute Type
   AUTO = 0, CSS, XML
};

enum class ART: char {
   NIL = 0,
   AUTO,
   AUTO_REVERSE,
   FIXED
};

enum class CMODE : char { // Specifies the interpolation mode for the animation.
   LINEAR = 0, // Simple linear interpolation between values is used to calculate the animation function.
   DISCRETE,   // The animation function will jump from one value to the next without any interpolation.
   PACED,      // Defines interpolation to produce an even pace of change across the animation. This is only supported for values that define a linear numeric range, and for which some notion of "distance" between points can be calculated (e.g. position, width, height, etc.).  Any keyTimes or keySplines will be ignored.
   SPLINE      // Interpolates from one value in the values list to the next according to a time function defined by a cubic Bezier spline. The points of the spline are defined in the keyTimes attribute, and the control points for each interval are defined in the keySplines attribute.
};

constexpr LONG MAX_VALUES = 8;

enum class RST : char { // Restart
   ALWAYS = 0,
   WHEN_NOT_ACTIVE,
   NEVER
};

class anim_base {
public:
   std::vector<std::string> values; // Set of discrete values that override 'from', 'to', 'by'
   std::string from;             // Start from this value. Ignored if 'values' is defined.
   std::string to, by;           // Note that 'to' and 'by' are mutually exclusive, with 'to' as the preference.
   std::string target_attrib;    // Name of the target attribute affected by the From and To values.
   std::string id;               // Identifier for the animation
   struct VectorMatrix *matrix = NULL; // Exclusive transform matrix for animation.
   DOUBLE begin_offset = 0;    // Start animating after this much time (in seconds) has elapsed.
   DOUBLE repeat_duration = 0; // The animation will be allowed to repeat for up to the number of seconds indicated.  The time includes the initial loop.
   DOUBLE min_duration = 0;    // The minimum value of the active duration.  If zero, the active duration is not constrained.
   DOUBLE max_duration = 0;    // The maximum value of the active duration.
   DOUBLE duration   = 0;      // Measured in seconds, anything < 0 means infinite.
   DOUBLE first_time = 0;      // Time-stamp of the first iteration
   DOUBLE start_time = 0;      // This is time-stamped once the animation has started (the first begin event is hit)
   DOUBLE end_time   = 0;      // This is time-stamped once the animation has finished all of its cycles (including repetitions)
   DOUBLE end  = 0;            
   DOUBLE seek = 0;            // Current seek position, between 0 - 1.0
   OBJECTID target_vector = 0;
   LONG   repeat_count = 0; // Repetition count.  Anything < 0 means infinite.
   LONG   repeat_index = 0; // Current index within the repeat cycle.
   CMODE  calc_mode   = CMODE::LINEAR;
   RST    restart     = RST::ALWAYS;
   ATT    attrib_type = ATT::AUTO;
   ADD    additive    = ADD::SUM;
   bool   freeze      = false;
   bool   accumulate  = false;

   anim_base(OBJECTID pTarget) : target_vector(pTarget) { }

   DOUBLE get_dimension();
   DOUBLE get_numeric_value();
   FRGB get_colour_value();
   bool started(DOUBLE);
   void next_frame(DOUBLE);

   virtual void perform() = 0;
   virtual bool is_valid() {
      if (!values.empty()) return true;
      if ((!to.empty()) or (!by.empty())) return true;
      return false;
   }
};

class anim_transform : public anim_base {
public:
   AT type;
   anim_transform(OBJECTID pTarget) : anim_base(pTarget) { }
   void perform();
};

class anim_motion : public anim_base {
public:
   typedef struct { FLOAT x, y; } POINT;
   ART auto_rotate = ART::NIL; // 0 = None; 1 = Auto Rotate by path tangent; -1 = Auto rotate by inverse of path tangent
   DOUBLE rotate = 0;
   pf::GuardedObject<objVector> path;
   std::vector<POINT> points;
   LONG path_timestamp;

   anim_motion(OBJECTID pTarget) : anim_base(pTarget) { }
   void perform();

   bool is_valid() {
      if (!values.empty()) return true;
      if (path.id) return true;
      if ((!to.empty()) or (!by.empty())) return true;
      return false;
   }
};

class anim_colour : public anim_base {
public:
   anim_colour(OBJECTID pTarget) : anim_base(pTarget) { }
   void perform();
};

class anim_value : public anim_base {
public:
   anim_value(OBJECTID pTarget) : anim_base(pTarget) { }
   void perform();
};

