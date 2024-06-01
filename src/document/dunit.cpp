
DUNIT::DUNIT(const std::string_view pValue, DU pDefaultType, DOUBLE pMin)
{
   const DOUBLE dpi = 96.0; // TODO: Needs to be derived from the display
   std::size_t i = 0;
   while ((i < pValue.size()) and (unsigned(pValue[i]) <= 0x20)) i++;

   DOUBLE fv;
   auto [ ptr, error ] = std::from_chars(pValue.data() + i, pValue.data() + pValue.size(), fv);

   if (error IS std::errc()) {
      if (ptr IS pValue.data() + pValue.size()) { value = fv; type = pDefaultType; }
      else if (ptr[0] IS '%') { value = fv * 0.01; type = DU::SCALED; }
      else if (ptr+2 <= pValue.data() + pValue.size()) {
         if ((ptr[0] IS 'p') and (ptr[1] IS 'x')) { value = fv; type = DU::PIXEL; }
         else if ((ptr[0] IS 'e') and (ptr[1] IS 'm')) { value = fv; type = DU::FONT_SIZE; }
         else if ((ptr[0] IS 'e') and (ptr[1] IS 'x')) { value = fv * 2.0; type = DU::FONT_SIZE; }
         else if ((ptr[0] IS 'i') and (ptr[1] IS 'n')) { value = fv * dpi; type = DU::PIXEL; } // Inches -> Pixels
         else if ((ptr[0] IS 'c') and (ptr[1] IS 'm')) { value = fv * (1.0 / 2.56) * dpi; type = DU::PIXEL; } // Centimetres -> Pixels
         else if ((ptr[0] IS 'm') and (ptr[1] IS 'm')) { value = fv * (1.0 / 20.56) * dpi; type = DU::PIXEL; } // Millimetres -> Pixels
         else if ((ptr[0] IS 'p') and (ptr[1] IS 't')) { value = fv * (4.0 / 3.0); type = DU::PIXEL; } // Points -> Pixels.  A point is 4/3 of a pixel
         else if ((ptr[0] IS 'p') and (ptr[1] IS 'c')) { value = fv * (4.0 / 3.0) * 12.0; type = DU::PIXEL; } // Pica -> Pixels.  1 Pica is equal to 12 Points
         else { value = fv; type = pDefaultType; }
      }
      else { value = fv; type = pDefaultType; }
   }
   else value = 0;

   if (value < pMin) value = pMin;
}

//********************************************************************************************************************
// Return values are truncated because true floating point values can lead to subtle computational bugs that aren't 
// worth the time of investigation.

DOUBLE DUNIT::px(class layout &Layout) {
   switch (type) {
      case DU::PIXEL:            return value;
      // Using the true font-size in the Height value gives a more consistent result vs the client's
      // requested 'font-size' (which guarantees nothing as to what is returned by Freetype).
      case DU::FONT_SIZE:        return std::trunc(value * Layout.m_font->metrics.Height); //Layout.m_font->font_size * 72.0 / 96.0);
      case DU::TRUE_LINE_HEIGHT: return std::trunc(value * Layout.m_line.height);
      case DU::LINE_HEIGHT:      return std::trunc(value * Layout.m_font->metrics.LineSpacing);
      case DU::CHAR:             return std::trunc(value * DOUBLE(vec::CharWidth(Layout.m_font->handle, '0', 0, NULL))); // Equivalent to CSS
      case DU::VP_WIDTH:         return std::trunc(value * Layout.m_viewport->Parent->get<DOUBLE>(FID_Width) * 0.01);
      case DU::VP_HEIGHT:        return std::trunc(value * Layout.m_viewport->Parent->get<DOUBLE>(FID_Height) * 0.01);
      case DU::ROOT_FONT_SIZE:   return std::trunc(value * Layout.Self->FontSize); // Measured in 72DPI pixels
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
