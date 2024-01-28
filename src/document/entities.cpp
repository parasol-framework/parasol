
void bc_cell::set_fill(const std::string Fill)
{
   if (rect_fill.empty()) {
      rect_fill.set(objVectorRectangle::create::global({
         fl::Name("cell_rect"),
         fl::Owner(viewport->UID),
         fl::X(0), fl::Y(0), fl::Width(SCALE(1.0)), fl::Height(SCALE(1.0)),
         fl::Fill(Fill)
      }));

      rect_fill->moveToBack();
   }
   else rect_fill->set(FID_Fill, Fill);
}
