// Window class (see include/Platform.h)
//
// The 'id' member of the Window class references the Parasol Scintilla object.
//
// The Window class is not the application window, but the target surface
// for scintilla draw operations.

inline OBJECTID getSurfaceID(Scintilla::Window* win)
{
   extScintilla *scintilla = (extScintilla *)win->GetID();
   return scintilla->SurfaceID;
}

//********************************************************************************************************************

Scintilla::Window::~Window()
{
   pf::Log log(__FUNCTION__);
   log.branch();
   Destroy();
}

//********************************************************************************************************************

void Scintilla::Window::Destroy()
{
   pf::Log log(__FUNCTION__);
   log.branch();

   wid = 0; // this object doesn't actually own the Scintilla drawable
}

//********************************************************************************************************************

bool Scintilla::Window::HasFocus()
{
   pf::Log log(__FUNCTION__);
   SURFACEINFO *info;

   log.branch();

   if (!gfxGetSurfaceInfo(getSurfaceID(this), &info)) {
      if (info->Flags & RNF_HAS_FOCUS) return 1;
   }

   return 0;
}

//********************************************************************************************************************
// We're returning the position in absolute screen coordinates

Scintilla::PRectangle Scintilla::Window::GetPosition()
{
   pf::Log log(__FUNCTION__);
   SURFACEINFO *info;

   // Before any size allocated pretend its 1000 wide so not scrolled
   Scintilla::PRectangle rc(0, 0, 1000, 1000);

   if (!gfxGetSurfaceInfo(getSurfaceID(this), &info)) {
      rc.left   = info->AbsX;
      rc.top    = info->AbsY;
      rc.right  = info->AbsX + info->Width;
      rc.bottom = info->AbsY + info->Height;
   }

   log.msg("%dx%d,%dx%d", rc.left, rc.top, rc.right, rc.bottom);

   return rc;
}

//********************************************************************************************************************

void Scintilla::Window::SetPosition(Scintilla::PRectangle rc)
{
   pf::Log log(__FUNCTION__);
   log.branch();

   // Surface class supports the redimension action
   acRedimension(getSurfaceID(this), rc.left, rc.top, 0, rc.Width(), rc.Height(), 0);
}

//********************************************************************************************************************

void Scintilla::Window::SetPositionRelative(Scintilla::PRectangle rc, Scintilla::Window relativeTo)
{
   pf::Log log(__FUNCTION__);
   SURFACEINFO *info;

   log.branch();

   if (!relativeTo.wid) return;
   if (!wid) return;

   // Get the position of the other window

   if (!gfxGetSurfaceInfo(getSurfaceID(&relativeTo), &info)) {
      rc.left -= info->X;
      rc.top  -= info->Y;
   }

   SetPosition(rc);
}

//********************************************************************************************************************

Scintilla::PRectangle Scintilla::Window::GetClientPosition()
{
   extScintilla *scintilla = (extScintilla *)this->GetID();

   //FMSG("Window::GetClientPosition()","%dx%d", scintilla->Surface.Width, scintilla->Surface.Height);
   return Scintilla::PRectangle(0, 0, scintilla->Surface.Width, scintilla->Surface.Height);
}

//********************************************************************************************************************

Scintilla::PRectangle Scintilla::Window::GetMonitorRect(Scintilla::Point)
{
   DISPLAYINFO *info;
   if (!gfxGetDisplayInfo(0, &info)) {
      return Scintilla::PRectangle(0, 0, info->Width, info->Height);
   }
   else return 0;
}

//********************************************************************************************************************

void Scintilla::Window::Show(bool show)
{
   pf::Log log(__FUNCTION__);
   log.branch();

   if (show) acShow(getSurfaceID(this));
   else acHide(getSurfaceID(this));
}

//********************************************************************************************************************

void Scintilla::Window::InvalidateAll()
{
   pf::Log log(__FUNCTION__);

   auto scintilla = (extScintilla *)this->GetID();

   // Scintilla expects the invalidation to be buffered, so a delayed message is appropriate.

   if (scintilla->Visible IS FALSE) return;

   log.traceBranch();
   QueueAction(AC_Draw, getSurfaceID(this));
}

//********************************************************************************************************************

void Scintilla::Window::InvalidateRectangle(Scintilla::PRectangle rc)
{
   pf::Log log(__FUNCTION__);

   auto scintilla = (extScintilla *)this->GetID();

   if (!scintilla->Visible) return;

   log.traceBranch("%dx%d,%dx%d", rc.left, rc.top, rc.Width(), rc.Height());

   // Scintilla expects the invalidation to be buffered, so a delayed message is appropriate.

   struct acDraw draw = { rc.left, rc.top, rc.Width(), rc.Height() };
   QueueAction(AC_Draw, getSurfaceID(this), &draw);
}

//********************************************************************************************************************

void Scintilla::Window::SetFont(Scintilla::Font &)
{
   pf::Log log(__FUNCTION__);
   log.branch("[UNSUPPORTED]");
   // Can not be done generically but only needed for ListBox
}

//********************************************************************************************************************
// Change the cursor for the drawable.

void Scintilla::Window::SetCursor(Cursor curs)
{
   objSurface *surface;
   LONG cursorid;

   if (curs IS cursorLast) return;

   switch (curs) {
      case cursorText:  cursorid = PTR_TEXT; break;
      case cursorArrow: cursorid = PTR_DEFAULT; break;
      case cursorUp:    cursorid = PTR_SIZE_TOP; break;
      case cursorWait:  cursorid = PTR_SLEEP; break;
      case cursorHoriz: cursorid = PTR_SPLIT_HORIZONTAL; break;
      case cursorVert:  cursorid = PTR_SPLIT_VERTICAL; break;
      case cursorHand:  cursorid = PTR_HAND; break;
      default:          cursorid = PTR_DEFAULT; break;
   }

   if (wid) {
      if (!AccessObject(getSurfaceID(this), 500, &surface)) {
         surface->set(FID_Cursor, cursorid);
         cursorLast = curs;
         ReleaseObject(surface);
      }
   }
}

//********************************************************************************************************************
// Report the title string to Scintilla, do not actively attempt to change the title of the nearest window.

void Scintilla::Window::SetTitle(const char *s)
{
   extScintilla *scintilla = (extScintilla *)this->GetID();
   scintilla->set(FID_Title, (STRING)s);
}
