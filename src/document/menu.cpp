//********************************************************************************************************************
// Default template for building the menu's layout.

static const char * glSVGHeader = R"LONGSTRING(
<svg placement="background">
  <defs>
    <pattern id="Background" x="0" y="0" width="40" height="20" patternUnits="userSpaceOnUse">
      <rect width="40" height="20" fill="#ffffff"/>
      <rect width="20" height="20" fill="#f9f9f9"/>
    </pattern>

    <clipPath id="PageClip">
      <rect x="3" y="4" xOffset="3" yOffset="3"/>
    </clipPath>

)LONGSTRING";

static const char * glSVGTail = R"LONGSTRING(
  </defs>

  <rect rx="3%" ry="3%" x="1" y="1" xOffset="2" yOffset="2" fill="rgb(245,245,245)" stroke="rgb(0,0,0,40)" stroke-width="0.5%"/>
  <rect rx="3%" ry="3%" x="3" y="3" xOffset="1" yOffset="1" fill="none" stroke="rgb(0,0,0,90)" stroke-width="0.5%"/>
</svg>
)LONGSTRING";

//********************************************************************************************************************

static void menu_lost_focus(OBJECTPTR Surface, ACTIONID ActionID, ERROR Error, APTR Args, doc_menu *Menu)
{
   if (Error) return;
   acHide(Surface);
}

//********************************************************************************************************************

static void menu_hidden(OBJECTPTR Surface, ACTIONID ActionID, ERROR Error, APTR Args, doc_menu *Menu)
{
   if (Error) return;
   Menu->m_hide_time = PreciseTime();
}

//********************************************************************************************************************
// Intercept hyperlink interaction with menu items

static ERROR menu_doc_events(objDocument *DocMenu, DEF Event, KEYVALUE *EventData)
{
   pf::Log log(__FUNCTION__);

   if ((Event & DEF::LINK_ACTIVATED) != DEF::NIL) {
      auto menu = (doc_menu *)DocMenu->CreatorMeta;

      acHide(*menu->m_surface);

      if (!menu->m_callback) return ERR_Skip;

      auto kv_item = EventData->find("id");
      if ((kv_item != EventData->end()) and (!kv_item->second.empty())) {
         for (auto &item : menu->m_items) {
            if (item.id IS kv_item->second) {
               menu->m_callback(*menu, item);
               return ERR_Skip;
            }
         }
      }

      kv_item = EventData->find("value");
      if ((kv_item != EventData->end()) and (!kv_item->second.empty())) {
         for (auto &item : menu->m_items) {
            if (item.value IS kv_item->second) {
               menu->m_callback(*menu, item);
               return ERR_Skip;
            }
         }
      }

      log.warning("No id or value defined for selected menu item.");
   }

   return ERR_Skip;
}

//********************************************************************************************************************

void doc_menu::define_font(objFont *Font)
{
   m_font_face  = Font->Face;
   m_font_size  = Font->Point;
   m_font_style = Font->Style;
}

//********************************************************************************************************************

objSurface * doc_menu::create(DOUBLE Width)
{
   pf::Log log(__FUNCTION__);

   log.branch();

   if (m_surface.empty()) {
      DOUBLE height = m_items.size() * 20;
      if (height < 20) height = 20;

      m_surface.set(objSurface::create::global({
         fl::Name("menu"),
         fl::Parent(0),
         fl::Flags(RNF::STICK_TO_FRONT|RNF::COMPOSITE),
         fl::WindowType(SWIN::NONE),
         fl::X(0), fl::Y(0), fl::Width(Width), fl::Height(height)
      }));

      m_scene = objVectorScene::create::global({
         fl::Name("menu_scene"),
         fl::Flags(VPF::RESIZE),
         fl::Surface(m_surface->UID)
      });

      m_view = objVectorViewport::create::global({
         fl::Owner(m_scene->UID),
         fl::X(0), fl::Y(0), fl::Width(SCALE(1.0)), fl::Height(SCALE(1.0))
      });

      m_doc = objDocument::create::global({
         fl::Owner(m_surface->UID),
         fl::Viewport(m_view),
         fl::EventMask(DEF::LINK_ACTIVATED),
         fl::EventCallback(APTR(menu_doc_events))
      });

      m_doc->CreatorMeta = this;

      SubscribeAction(*m_surface, AC_LostFocus, FUNCTION(menu_lost_focus, this));
      SubscribeAction(*m_surface, AC_Hide, FUNCTION(menu_hidden, this));

      refresh();
   }

   return *m_surface;
}

//********************************************************************************************************************

void doc_menu::refresh()
{
   pf::Log log(__FUNCTION__);

   const DOUBLE VGAP = m_font_size * 0.5;
   const DOUBLE HGAP = m_font_size * 1.0;
   const DOUBLE GAP = VGAP;
   const DOUBLE LEADING = 0.2;
   LONG total_icons = 0;

   std::ostringstream buf;
   buf << "<body margins=\"" << HGAP << " " << VGAP << " " << HGAP << " " << VGAP << "\" link=\"rgb(0,0,0)\" v-link=\"rgb(0,0,0)\" " <<
      "font-face=\"" << m_font_face << "\" font-size=\"" << m_font_size << "\"/>\n";

   if (!m_style.empty()) {
      buf << m_style;
   }
   else {
      buf << glSVGHeader;

      for (auto &item : m_items) {
         if (!item.icon.empty()) {
            buf << "    <image id=\"" << item.icon << "\" xlink:href=\"" << item.icon << "\" " <<
               "width=\"" << m_font_size * 2 << "\" height=\"" << m_font_size * 2 << "\"/>\n";
            total_icons++;
         }
      }

      buf << glSVGTail;
   }

   buf << "<page name=\"Index\">";

   for (auto &item : m_items) {
      buf << "<p no-wrap leading=\"" << LEADING << "\">";

      if (!item.id.empty()) buf << "<a @id=\"" << item.id << "\">";
      else if (!item.value.empty()) buf << "<a @value=\"" << item.value << "\">";
      else buf << "<a>";

      if (item.icon.empty()) {
         if (total_icons) buf << "<advance x=\"[=" << GAP << "+[%line-height]]\"/>";
      }
      else buf << "<image src=\"url(#" << item.icon << ")\"/><advance x=\"" << GAP << "\"/>";

      if (!item.content.empty()) buf << item.content;
      else buf << item.value;

      buf << "</a></p>\n";
   }

   buf << "</page>";

   acClear(m_doc);
   acDataXML(m_doc, buf.str().c_str());
   
   #ifdef DBG_LAYOUT
      log.warning("%s", buf.str().c_str());
   #endif

   // Resize the menu to match the new content.  If the height of the menu is excessive (relative to the height
   // of the display), we reduce it and utilise a scrollbar to see all menu items.

   auto doc_width  = m_doc->get<DOUBLE>(FID_PageWidth);
   auto doc_height = m_doc->get<DOUBLE>(FID_PageHeight);

   DOUBLE view_width  = doc_width;
   DOUBLE view_height = doc_height;

   DISPLAYINFO *display;
   if (!gfxGetDisplayInfo(0, &display)) {
      if (view_height > display->Height * 0.25) view_height = display->Height * 0.25;
   }

   if (view_width > m_surface->Width) {
      acResize(*m_surface, view_width, view_height, 0);
   }
   else m_surface->setHeight(view_height);

   if (doc_height > view_height) {
      m_view->setFields(fl::Height(view_height));

      objVectorViewport *doc_page, *doc_view;
      if (!m_doc->getPtr(FID_Page, &doc_page)) {
         if (!m_doc->getPtr(FID_View, &doc_view)) {
            m_scroll.init((extDocument *)CurrentContext(), doc_page, doc_view);
            m_scroll.m_auto_adjust_view_size = false;

            OBJECTPTR clip;
            if (!scFindDef(m_scene, "PageClip", &clip)) {
               doc_page->set(FID_Mask, clip);
            }
         }
      }
   }
}

//********************************************************************************************************************

void doc_menu::reposition(objVectorViewport *RelativeViewport)
{
   DISPLAYINFO *display;
   gfxGetDisplayInfo(0, &display);

   pf::ScopedObjectLock<objSurface> lk_surface(RelativeViewport->Scene->SurfaceID); // Window surface
   if (lk_surface.granted()) {
      auto w_absx = lk_surface->get<DOUBLE>(FID_AbsX);
      auto w_absy = lk_surface->get<DOUBLE>(FID_AbsY);

      auto vp_absx = RelativeViewport->get<DOUBLE>(FID_AbsX);
      auto vp_absy = RelativeViewport->get<DOUBLE>(FID_AbsY);
      auto vp_height = RelativeViewport->get<DOUBLE>(FID_Height);

      // Invert the menu position if it will drop off the display

      DOUBLE y = w_absy + vp_absy + vp_height;
      if (y + m_surface->Height > display->Height) {
         y -= m_surface->Height + vp_height;
      }

      acMoveToPoint(*m_surface, w_absx + vp_absx, y, 0, MTF::X|MTF::Y);
   }
}

//********************************************************************************************************************

void doc_menu::toggle(objVectorViewport *Relative)
{
   pf::Log log(__FUNCTION__);
   log.branch();

   const DOUBLE time_lapse = 20000; // Amount of time that must elapse to trigger the toggle.

   auto current_time = PreciseTime();
   if (m_show_time > m_hide_time) { // Hide the menu
      if (current_time - m_show_time >= time_lapse) {
         acHide(*m_surface);
      }
   }
   else if (current_time - m_hide_time >= time_lapse) {
      reposition(Relative);
      acShow(*m_surface);
      m_show_time = current_time;
   }
}