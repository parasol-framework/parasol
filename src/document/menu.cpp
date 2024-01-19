//********************************************************************************************************************
// Template for building the menu's layout.
// Additional SVG defs can be inserted after the header

static const char * glMenuHeader = R"LONGSTRING(
<svg>
  <defs>
    <linearGradient id="Gradient" gradientUnits="objectBoundingBox">
      <stop offset="0" stop-color="rgb(255,255,255,246)"/>
      <stop offset="1" stop-color="rgb(255,255,255,190)"/>
    </linearGradient>
)LONGSTRING";

// Actual content can be inserted after the page

static const char * glMenuPage = R"LONGSTRING(
  </defs>

  <rect rx="3%" ry="3%" x="1" y="1" xOffset="1" yOffset="1" fill="url(#Gradient)" stroke="rgb(0,0,0,80)" stroke-width="1"/>
</svg>

<page name="Index">
)LONGSTRING";

// Remaining tail-end of the page

static const char * glMenuFooter = R"LONGSTRING(
</page>
)LONGSTRING";

//********************************************************************************************************************

static void menu_lost_focus(OBJECTPTR Surface, ACTIONID ActionID, ERROR Error, APTR Args)
{
   if (Error) return;

   acHide(Surface);
}

//********************************************************************************************************************
// Intercept hyperlink interaction with menu items

static ERROR menu_doc_events(objDocument *DocMenu, DEF Event, KEYVALUE *EventData)
{
   pf::Log log(__FUNCTION__);

   if ((Event & DEF::LINK_ACTIVATED) != DEF::NIL) {
      auto menu = (doc_menu *)DocMenu->CreatorMeta;

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
      
      kv_item = EventData->find("name");
      if ((kv_item != EventData->end()) and (!kv_item->second.empty())) {
         for (auto &item : menu->m_items) {
            if (item.name IS kv_item->second) {
               menu->m_callback(*menu, item);
               return ERR_Skip;
            }
         }
      }

      log.warning("No id or name defined for selected menu item.");
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
   
   auto Self = (extDocument *)CurrentContext();

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

      auto scene = objVectorScene::create::global({
         fl::Name("menu_scene"),
         fl::Flags(VPF::RESIZE),
         fl::Surface(m_surface->UID) 
      });

      auto vp = objVectorViewport::create::global({
         fl::Owner(scene->UID),
         fl::X(0), fl::Y(0), fl::XOffset(0), fl::YOffset(0)
      });

      m_doc.set((extDocument *)(objDocument::create::global({
         fl::Owner(Self->UID),
         fl::Viewport(vp),
         fl::EventMask(DEF::LINK_ACTIVATED),
         fl::EventCallback(menu_doc_events)
      })));
      
      m_doc->CreatorMeta = this;

      auto call = make_function_stdc(menu_lost_focus);
      SubscribeAction(*m_surface, AC_LostFocus, &call);

      refresh();
   }

   return *m_surface;
}

//********************************************************************************************************************

void doc_menu::refresh()
{
   pf::Log log(__FUNCTION__);

   acClear(*m_doc);

   const DOUBLE GAP = m_font_size * 0.5;
   const DOUBLE LEADING = 0.2;
   std::ostringstream buf;
   buf << "<body margins=\"" << GAP << "\" link=\"rgb(0,0,0)\" v-link=\"rgb(0,0,0)\" " << 
      "font-face=\"" << m_font_face << "\" font-size=\"" << m_font_size << "\"/>\n";
   buf << glMenuHeader;
   
   LONG total_icons = 0;
   for (auto &item : m_items) {
      if (!item.icon.empty()) {
         buf << "    <image id=\"" << item.icon << "\" xlink:href=\"" << item.icon << "\" " << 
            "width=\"" << m_font_size * 2 << "\" height=\"" << m_font_size * 2 << "\"/>\n";
         total_icons++;
      }
   }

   buf << glMenuPage;
   
   for (auto &item : m_items) {
      if (item.icon.empty()) {
         if (total_icons) {
            buf << "<p no-wrap leading=\"" << LEADING << "\"><a @id=\"" << item.id << "\" @name=\"" << item.name << "\">" << 
               "<advance x=\"[=" << GAP << "+[%line-height]]\"/>" << item.name << "</a></p>\n";
         }
         else buf << "<p no-wrap leading=\"" << LEADING << "\"><a @id=\"" << item.id << "\" @name=\"" << item.name << "\">" << item.name << "</a></p>\n";
      }
      else {
         buf << "<p no-wrap leading=\"" << LEADING << "\"><a @id=\"" << item.id << "\" @name=\"" << item.name << "\"><image src=\"url(#" << item.icon <<
            ")\"/><advance x=\"" << GAP << "\"/>" << item.name << "</a></p>\n";
      }
   }

   buf << glMenuFooter;
   acDataXML(*m_doc, buf.str().c_str());

   DOUBLE page_height;
   m_doc->get(FID_PageHeight, &page_height);
   m_surface->setHeight(page_height);

   acRefresh(*m_doc);
}

//********************************************************************************************************************

void doc_menu::reposition(objVectorViewport *RelativeViewport)
{
   pf::ScopedObjectLock lk_surface(RelativeViewport->Scene->SurfaceID);
   if (lk_surface.granted()) {
      DOUBLE w_absx, w_absy, vp_absx, vp_absy, vp_height;

      lk_surface->get(FID_AbsX, &w_absx);
      lk_surface->get(FID_AbsY, &w_absy);

      RelativeViewport->get(FID_AbsX, &vp_absx);
      RelativeViewport->get(FID_AbsY, &vp_absy);
      RelativeViewport->get(FID_Height, &vp_height);
      
      acMoveToPoint(*m_surface, w_absx + vp_absx, w_absy + vp_absy + vp_height, 0, MTF::X|MTF::Y);
   }
}

//********************************************************************************************************************

bool doc_menu::toggle(objVectorViewport *Relative)
{
   pf::Log log(__FUNCTION__);

   log.branch();

   const DOUBLE time_lapse = 20000;

   auto current_time = PreciseTime();
   if (m_show_time > m_hide_time) { // Hide the menu
      if (current_time - m_show_time >= time_lapse) {
         acHide(*m_surface);
         m_hide_time = current_time;
      }
      return false;
   }
   else {
      if (current_time - m_hide_time >= time_lapse) {
         reposition(Relative);
         acShow(*m_surface);
         m_show_time = current_time;
      }
      return true;
   }
}
