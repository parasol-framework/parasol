
enum class DU : UBYTE {
   NIL = 0,
   PIXEL,  // px
   SCALED, // %: Scale to fill empty space
};

struct DUNIT {
   DOUBLE value;
   DU type;

   DUNIT() : value(0), type(DU::NIL) { }

   DUNIT(DOUBLE pValue, DU pType = DU::PIXEL) : value(pValue), type(pType) { }

   DUNIT(const std::string &pValue, DOUBLE pMin = std::numeric_limits<DOUBLE>::min()) : 
      DUNIT(pValue.c_str(), pMin) { }

   DUNIT(CSTRING pValue, DOUBLE pMin = std::numeric_limits<DOUBLE>::min());

   constexpr bool empty() { return (type IS DU::NIL) or (!value); }
   constexpr void clear() { value = 0; type = DU::PIXEL; }

   operator double() const{ return value; }
   operator DU() const { return type; }

   LARGE as_field(FIELD FieldID) const {
      return  (type IS DU::SCALED) ? (FieldID|TSCALE) : FieldID;
   }
};

DUNIT::DUNIT(CSTRING pValue, DOUBLE pMin)
{
   bool is_number = true;
   auto v = pValue;
   while ((*v) and (unsigned(*v) <= 0x20)) v++;

   auto str = v;
   if ((*str IS '-') or (*str IS '+')) str++;

   if (((*str >= '0') and (*str <= '9')) or (*str IS '.')) {
      while ((*str >= '0') and (*str <= '9')) str++;

      if (*str IS '.') {
         str++;
         if ((*str >= '0') and (*str <= '9')) {
            while ((*str >= '0') and (*str <= '9')) str++;
         }
         else is_number = false;
      }

      const DOUBLE dpi = 96.0;
      const DOUBLE fv = StrToFloat(v);

      if (str[0] IS '%') { value = fv * 0.01; type = DU::SCALED; }
      else {
         type = DU::PIXEL;

         if ((str[0] IS 'p') and (str[1] IS 'x')) value = fv;
         else if ((str[0] IS 'e') and (str[1] IS 'm')) value = fv * 12.0 * (4.0 / 3.0); // Multiply the current font's pixel height by the provided em value
         else if ((str[0] IS 'e') and (str[1] IS 'x')) value = fv * 6.0 * (4.0 / 3.0); // As for em, but multiple by the pixel height of the 'x' character.  If no x character, revert to 0.5em
         else if ((str[0] IS 'i') and (str[1] IS 'n')) value = fv * dpi; // Inches -> Pixels
         else if ((str[0] IS 'c') and (str[1] IS 'm')) value = fv * (1.0 / 2.56) * dpi; // Centimetres -> Pixels
         else if ((str[0] IS 'm') and (str[1] IS 'm')) value = fv * (1.0 / 20.56) * dpi; // Millimetres -> Pixels
         else if ((str[0] IS 'p') and (str[1] IS 't')) value = fv * (4.0 / 3.0); // Points -> Pixels.  A point is 4/3 of a pixel
         else if ((str[0] IS 'p') and (str[1] IS 'c')) value = fv * (4.0 / 3.0) * 12.0; // Pica -> Pixels.  1 Pica is equal to 12 Points
         else value = fv;

         if (value < pMin) value = pMin;
      }
   }
   else value = 0;
}
