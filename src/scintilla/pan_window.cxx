/*
** Window class (see include/Platform.h)
**
** The 'id' member of the Window class references the Parasol Scintilla object.
**
** The Window class is not the application window, but the target surface
** for scintilla draw operations.
*/

inline OBJECTID getSurfaceID(Scintilla::Window* win)
{
   extScintilla *scintilla = (extScintilla *)win->GetID();
   return scintilla->SurfaceID;
}

/****************************************************************************/

Scintilla::Window::~Window()
{
   LogF("Window::~Window()","");
   Destroy();
}

/****************************************************************************/

void Scintilla::Window::Destroy()
{
   LogF("Window::Destroy()","");
   wid = 0; /* this object doesn't actually own the Scintilla drawable */
}

/****************************************************************************/

bool Scintilla::Window::HasFocus()
{
   SURFACEINFO *info;

   LogF("Window::HasFocus()","");

   if (!gfxGetSurfaceInfo(getSurfaceID(this), &info)) {
      if (info->Flags & RNF_HAS_FOCUS) return 1;
   }

   return 0;
}

/*****************************************************************************
** We're returning the position in absolute screen coordinates
*/

Scintilla::PRectangle Scintilla::Window::GetPosition()
{
   SURFACEINFO *info;

   // Before any size allocated pretend its 1000 wide so not scrolled
   Scintilla::PRectangle rc(0, 0, 1000, 1000);

   if (!gfxGetSurfaceInfo(getSurfaceID(this), &info)) {
      rc.left   = info->AbsX;
      rc.top    = info->AbsY;
      rc.right  = info->AbsX + info->Width;
      rc.bottom = info->AbsY + info->Height;
   }

   LogF("Window::GetPosition()","%dx%d,%dx%d", rc.left, rc.top, rc.right, rc.bottom);

   return rc;
}

/****************************************************************************/

void Scintilla::Window::SetPosition(Scintilla::PRectangle rc)
{
   LogF("Window::SetPosition()","");

   // Surface class supports the redimension action
   acRedimensionID(getSurfaceID(this), rc.left, rc.top, 0, rc.Width(), rc.Height(), 0);
}

/****************************************************************************/

void Scintilla::Window::SetPositionRelative(Scintilla::PRectangle rc, Scintilla::Window relativeTo)
{
   SURFACEINFO *info;

   LogF("Window::SetPositionRelative()","");

   if (!relativeTo.wid) return;
   if (!wid) return;

   // Get the position of the other window

   if (!gfxGetSurfaceInfo(getSurfaceID(&relativeTo), &info)) {
      rc.left -= info->X;
      rc.top  -= info->Y;
   }

   SetPosition(rc);
}

/****************************************************************************/

Scintilla::PRectangle Scintilla::Window::GetClientPosition()
{
   extScintilla *scintilla = (extScintilla *)this->GetID();

   //FMSG("Window::GetClientPosition()","%dx%d", scintilla->Surface.Width, scintilla->Surface.Height);
   return Scintilla::PRectangle(0, 0, scintilla->Surface.Width, scintilla->Surface.Height);
}

/****************************************************************************/

Scintilla::PRectangle Scintilla::Window::GetMonitorRect(Scintilla::Point)
{
   DISPLAYINFO *info;
   if (!gfxGetDisplayInfo(0, &info)) {
      return Scintilla::PRectangle(0, 0, info->Width, info->Height);
   }
   else return 0;
}

/****************************************************************************/

void Scintilla::Window::Show(bool show)
{
   LogF("Window::Show()","");

   if (show) acShowID(getSurfaceID(this));
   else acHideID(getSurfaceID(this));
}

/****************************************************************************/

void Scintilla::Window::InvalidateAll()
{
   auto scintilla = (extScintilla *)this->GetID();

   // Scintilla expects the invalidation to be buffered, so a delayed message is appropriate.

   if (scintilla->Visible IS FALSE) return;

   FMSG("~Window::InvalidateAll()","");
   DelayMsg(AC_Draw, getSurfaceID(this), NULL);
   LOGRETURN();
}

/****************************************************************************/

void Scintilla::Window::InvalidateRectangle(Scintilla::PRectangle rc)
{
   auto scintilla = (extScintilla *)this->GetID();

   if (scintilla->Visible IS FALSE) return;

   FMSG("~Window::InvalidateRectangle()","%dx%d,%dx%d", rc.left, rc.top, rc.Width(), rc.Height());

   // Scintilla expects the invalidation to be buffered, so a delayed message is appropriate.

   struct acDraw draw = { rc.left, rc.top, rc.Width(), rc.Height() };
   DelayMsg(AC_Draw, getSurfaceID(this), &draw);

   LOGRETURN();
}

/****************************************************************************/

void Scintilla::Window::SetFont(Scintilla::Font &)
{
   LogF("Window::SetFont()","[UNSUPPORTED]");
   // Can not be done generically but only needed for ListBox
}

/*****************************************************************************
** Change the cursor for the drawable.
*/

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
         SetLong(surface, FID_Cursor, cursorid);
         cursorLast = curs;
         ReleaseObject(surface);
      }
   }
}

/*****************************************************************************
** Report the title string to Scintilla, do not actively attempt to change the title of the nearest window.
*/

void Scintilla::Window::SetTitle(const char *s)
{
   extScintilla *scintilla = (extScintilla *)this->GetID();
   SetString(scintilla, FID_Title, (STRING)s);
}
