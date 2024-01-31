
DUNIT::DUNIT(CSTRING pValue, DU pDefaultType, DOUBLE pMin)
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
      else if ((str[0] IS 'p') and (str[1] IS 'x')) { value = fv; type = DU::PIXEL; }
      else if ((str[0] IS 'e') and (str[1] IS 'm')) { value = fv; type = DU::FONT_SIZE; }
      else if ((str[0] IS 'e') and (str[1] IS 'x')) { value = fv * 2.0; type = DU::FONT_SIZE; }
      else if ((str[0] IS 'i') and (str[1] IS 'n')) { value = fv * dpi; type = DU::PIXEL; } // Inches -> Pixels
      else if ((str[0] IS 'c') and (str[1] IS 'm')) { value = fv * (1.0 / 2.56) * dpi; type = DU::PIXEL; } // Centimetres -> Pixels
      else if ((str[0] IS 'm') and (str[1] IS 'm')) { value = fv * (1.0 / 20.56) * dpi; type = DU::PIXEL; } // Millimetres -> Pixels
      else if ((str[0] IS 'p') and (str[1] IS 't')) { value = fv * (4.0 / 3.0); type = DU::PIXEL; } // Points -> Pixels.  A point is 4/3 of a pixel
      else if ((str[0] IS 'p') and (str[1] IS 'c')) { value = fv * (4.0 / 3.0) * 12.0; type = DU::PIXEL; } // Pica -> Pixels.  1 Pica is equal to 12 Points
      else { value = fv; type = pDefaultType; }

      if (value < pMin) value = pMin;
   }
   else value = 0;
}

//********************************************************************************************************************
// Return values are truncated because true floating point values can lead to subtle computational bugs that aren't 
// worth the time of investigation.

DOUBLE DUNIT::px(class layout &Layout) {
   switch (type) {
      case DU::PIXEL:            return value;
      case DU::FONT_SIZE:        return std::trunc(value * Layout.m_font->Ascent);
      case DU::TRUE_LINE_HEIGHT: return std::trunc(value * Layout.m_line.height);
      case DU::LINE_HEIGHT:      return std::trunc(value * Layout.m_font->LineSpacing);
      case DU::CHAR:             return std::trunc(value * DOUBLE(fntCharWidth(Layout.m_font, '0', 0, NULL))); // Equivalent to CSS
      case DU::VP_WIDTH:         return std::trunc(value * Layout.m_viewport->Parent->get<DOUBLE>(FID_Width) * 0.01);
      case DU::VP_HEIGHT:        return std::trunc(value * Layout.m_viewport->Parent->get<DOUBLE>(FID_Height) * 0.01);
      case DU::ROOT_FONT_SIZE:   return std::trunc(value * Layout.Self->FontSize);
      case DU::ROOT_LINE_HEIGHT: return std::trunc(value * (Layout.Self->FontSize * 1.3)); // Guesstimate
         
      case DU::VP_MIN: {
         auto w = std::trunc(value * Layout.m_viewport->Parent->get<DOUBLE>(FID_Width) * 0.01);
         auto h = std::trunc(value * Layout.m_viewport->Parent->get<DOUBLE>(FID_Height) * 0.01);
         return std::min(w, h);
      }

      case DU::VP_MAX: {
         auto w = std::trunc(value * Layout.m_viewport->Parent->get<DOUBLE>(FID_Width) * 0.01);
         auto h = std::trunc(value * Layout.m_viewport->Parent->get<DOUBLE>(FID_Height) * 0.01);
         return std::max(w, h);
      }

      case DU::SCALED: // wrap_edge equates to m_page_width - m_margins.right;
         return std::trunc(value * 0.01 * (Layout.wrap_edge() - Layout.m_cursor_x));
         //return value * 0.01 * (Layout.wrap_edge() - m_margins.left);

      default: return 0;
   }

   return 0;
}
