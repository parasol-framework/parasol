// Window class (see include/Platform.h)
//
// The 'id' member of the Window class references the Kotuku Scintilla object.  Windows that Scintilla creates for
// itself (call tips and auto-completion lists) are not supported yet and will have a null id.
//
// The Window class is not the application window, but the View viewport that hosts the Scintilla client area.

inline extScintilla * get_scintilla(Scintilla::Window *Win)
{
   return (extScintilla *)Win->GetID();
}

//********************************************************************************************************************

Scintilla::Window::~Window()
{
   Destroy();
}

//********************************************************************************************************************

void Scintilla::Window::Destroy()
{
   wid = 0; // this object doesn't actually own the Scintilla drawable
}

//********************************************************************************************************************
// The focus state is initialised from the Focus viewport and kept current by feedback_focus().

bool Scintilla::Window::HasFocus()
{
   auto scintilla = get_scintilla(this);
   return (scintilla) and (scintilla->HasFocus);
}

//********************************************************************************************************************
// We're returning the position in absolute coordinates

Scintilla::PRectangle Scintilla::Window::GetPosition()
{
   // Before any size allocated pretend its 1000 wide so not scrolled
   Scintilla::PRectangle rc(0, 0, 1000, 1000);

   auto scintilla = get_scintilla(this);
   if ((scintilla) and (scintilla->View)) {
      int x = 0, y = 0;
      scintilla->View->getAbsX(x);
      scintilla->View->getAbsY(y);
      rc.left   = x;
      rc.top    = y;
      rc.right  = x + scintilla->ViewWidth;
      rc.bottom = y + scintilla->ViewHeight;
   }

   return rc;
}

//********************************************************************************************************************

void Scintilla::Window::SetPosition(Scintilla::PRectangle rc)
{
   // TODO Phase 7: Required for call tip and auto-completion pop-ups.
}

//********************************************************************************************************************

void Scintilla::Window::SetPositionRelative(Scintilla::PRectangle rc, Scintilla::Window relativeTo)
{
   // TODO Phase 7: Required for call tip and auto-completion pop-ups.
}

//********************************************************************************************************************

Scintilla::PRectangle Scintilla::Window::GetClientPosition()
{
   auto scintilla = get_scintilla(this);
   if (scintilla) return Scintilla::PRectangle(0, 0, scintilla->ViewWidth, scintilla->ViewHeight);
   else return Scintilla::PRectangle(0, 0, 0, 0);
}

//********************************************************************************************************************

Scintilla::PRectangle Scintilla::Window::GetMonitorRect(Scintilla::Point)
{
   DisplayInfo *info;
   if (!gfx::GetDisplayInfo(0, &info)) {
      return Scintilla::PRectangle(0, 0, info->Width, info->Height);
   }
   else return 0;
}

//********************************************************************************************************************

void Scintilla::Window::Show(bool show)
{
   // TODO Phase 7: Required for call tip and auto-completion pop-ups.
}

//********************************************************************************************************************

void Scintilla::Window::InvalidateAll()
{
   if (auto scintilla = get_scintilla(this)) {
      invalidate_area(scintilla, 0, 0, scintilla->ViewWidth, scintilla->ViewHeight);
   }
}

//********************************************************************************************************************

void Scintilla::Window::InvalidateRectangle(Scintilla::PRectangle rc)
{
   if (auto scintilla = get_scintilla(this)) invalidate_area(scintilla, rc.left, rc.top, rc.right, rc.bottom);
}

//********************************************************************************************************************

void Scintilla::Window::SetFont(Scintilla::Font &)
{
   // Can not be done generically but only needed for ListBox
}

//********************************************************************************************************************
// Change the cursor for the drawable.

void Scintilla::Window::SetCursor(Cursor curs)
{
   // TODO Phase 5: Set the Cursor field of the View, mapping the Scintilla cursor to PTC values as follows:
   //
   //   cursorText:  PTC::TEXT;  cursorArrow: PTC::DEFAULT;          cursorUp:    PTC::SIZE_TOP;
   //   cursorWait:  PTC::SLEEP; cursorHoriz: PTC::SPLIT_HORIZONTAL; cursorVert:  PTC::SPLIT_VERTICAL;
   //   cursorHand:  PTC::HAND;  default:     PTC::DEFAULT
}

//********************************************************************************************************************
// Report the title string to Scintilla, do not actively attempt to change the title of the nearest window.

void Scintilla::Window::SetTitle(const char *s)
{
   if (auto scintilla = get_scintilla(this)) scintilla->setName(s);
}
