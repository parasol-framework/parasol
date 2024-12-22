// Field Unit.  Makes it user to define field values that could be fixed or scaled.

#include <cfloat>

enum class DU : UBYTE {
   NIL = 0,
   PIXEL,  // px
   SCALED, // %: Scale to fill empty space
};

struct FUNIT {
   FIELD field_id;
   DOUBLE value;
   DU type;

   constexpr FUNIT() noexcept : field_id(0), value(0), type(DU::NIL) { }

   // With field
   explicit FUNIT(FIELD pField, DOUBLE pValue, DU pType = DU::NIL) noexcept : field_id(pField), value(pValue), type(pType) { }
   
   FUNIT(FIELD pField, const std::string &pValue, DU pType = DU::NIL, DOUBLE pMin = -DBL_MAX) noexcept : 
      FUNIT(pValue.c_str(), pType, pMin) { field_id = pField; }

   FUNIT(FIELD pField, CSTRING pValue, DU pType = DU::NIL, DOUBLE pMin = -DBL_MAX) noexcept :
      FUNIT(pValue, pType, pMin) { field_id = pField; }

   // Without field
   explicit FUNIT(DOUBLE pValue, DU pType = DU::PIXEL) noexcept : value(pValue), type(pType) { }

   FUNIT(const std::string &pValue, DU pType = DU::NIL, DOUBLE pMin = -DBL_MAX) noexcept : 
      FUNIT(pValue.c_str(), pType, pMin) { }

   FUNIT(CSTRING pValue, DU pType = DU::NIL, DOUBLE pMin = -DBL_MAX) noexcept;

   constexpr bool empty() const noexcept { return (type == DU::NIL) || (!value); }
   constexpr void clear() noexcept { value = 0; type = DU::PIXEL; }

   operator double() const noexcept { return value; }
   operator DU() const noexcept { return type; }

   inline LARGE field() const noexcept {
      return (type == DU::SCALED) ? (field_id | TDOUBLE | TSCALE) : field_id | TDOUBLE;
   }

   inline bool valid_size() const noexcept { // Return true if this is a valid width/height
      return (value >= 0.001);
   }

   inline ERR set(OBJECTPTR Object) const noexcept { return SetField(Object, field(), value); }
};

FUNIT::FUNIT(CSTRING pValue, DU pType, DOUBLE pMin) noexcept
{
   char *str = (char *)pValue;
   while ((*str) and (unsigned(*str) <= 0x20)) str++;

   const DOUBLE dpi = 96.0;
   const DOUBLE fv = strtod(str, &str);

   if (str[0] IS '%') { value = fv * 0.01; type = pType != DU::NIL ? pType : DU::SCALED; }
   else {
      type = pType != DU::NIL ? pType : DU::PIXEL;

      if (str[0]) {
         if ((str[0] IS 'e') and (str[1] IS 'm')) value = fv * 12.0 * (4.0 / 3.0); // Multiply the current font's pixel height by the provided em value
         else if ((str[0] IS 'e') and (str[1] IS 'x')) value = fv * 6.0 * (4.0 / 3.0); // As for em, but multiple by the pixel height of the 'x' character.  If no x character, revert to 0.5em
         else if ((str[0] IS 'i') and (str[1] IS 'n')) value = fv * dpi; // Inches -> Pixels
         else if ((str[0] IS 'c') and (str[1] IS 'm')) value = fv * (1.0 / 2.56) * dpi; // Centimetres -> Pixels
         else if ((str[0] IS 'm') and (str[1] IS 'm')) value = fv * (1.0 / 20.56) * dpi; // Millimetres -> Pixels
         else if ((str[0] IS 'p') and (str[1] IS 't')) value = fv * (4.0 / 3.0); // Points -> Pixels.  A point is 4/3 of a pixel
         else if ((str[0] IS 'p') and (str[1] IS 'c')) value = fv * (4.0 / 3.0) * 12.0; // Pica -> Pixels.  1 Pica is equal to 12 Points
         else value = fv;
      }
      else value = fv;

      if (value < pMin) value = pMin;
   }
}
