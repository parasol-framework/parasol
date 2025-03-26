//********************************************************************************************************************
// Display Unit class.  Reads CSS metric values during parsing and returns them as pixel values during the layout
// process.

enum class DU : UBYTE {
   NIL = 0,
   PIXEL,             // px in 72DPI
   SCALED,            // %: Scale to fill empty space
   FONT_SIZE,         // em
   CHAR,              // ch: The advance (width) of the '0' character
   LINE_HEIGHT,       // lh:  Current line height
   TRUE_LINE_HEIGHT,  // lh:  Current line height
   ROOT_FONT_SIZE,    // rem: Font size of the root element
   ROOT_LINE_HEIGHT,  // rlh: Line height of the root element
   VP_WIDTH,          // vw:  1% of the viewport's width
   VP_HEIGHT,         // vh:  1% of the viewport's height
   VP_MIN,            // vmin: 1% of the viewport's smallest axis
   VP_MAX             // vmax: 1% of the viewport's largest axis
};

struct DUNIT {
   DOUBLE value;
   DU type;

   DUNIT() : value(0), type(DU::NIL) { }

   DUNIT(DOUBLE pValue, DU pType = DU::PIXEL) : value(pValue), type(pType) { }

   DUNIT(const std::string_view pValue, DU pDefaultType = DU::PIXEL, DOUBLE pMin = std::numeric_limits<DOUBLE>::min());

   DOUBLE px(class layout &Layout);
   
   constexpr bool empty() { return (type IS DU::NIL) or (!value); }
   constexpr void clear() { value = 0; type = DU::PIXEL; }

   bool operator!=(DUNIT const &rhs) const {return !(*this == rhs);}

   bool operator==(DUNIT const &rhs) const {
      return (value == rhs.value) and (type == rhs.type);
   }
};
