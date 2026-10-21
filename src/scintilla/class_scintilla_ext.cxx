
// Style codes for each lexer are defined in SciLexer.h

#include <format>

#define COL_BLACK          0x000000
#define COL_DARKSLATEGREY  0x2F4F4F
#define COL_LIGHTSLATEGREY 0x576889
#define COL_GREY           0x808080
#define COL_LIGHTGREY      0xd3d3d3
#define COL_MIDNIGHTBLUE   0x191970
#define COL_NAVYBLUE       0x000080
#define COL_BLUE           0x0000ff
#define COL_CYAN           0x00ffff
#define COL_TURQUOISE      0x00ced1
#define COL_CADETBLUE      0x5f9ea0
#define COL_OLIVE          0x6b8e23
#define COL_KHAKI          0xBDB76B
#define COL_GOLD           0xFFD700
#define COL_INDIANRED      0xCD5C5C
#define COL_SADDLEBROWN    0x8B4513
#define COL_FIREBRICK      0xB22222
#define COL_BROWN          0xA52A2A
#define COL_FORESTGREEN    0x228B22
#define COL_BRIGHTRED      0xFF0000
#define COL_BRIGHTGREEN    0xFF0000
#define COL_DARKVIOLET     0x9400D3
#define COL_RED            0xb00000

static const struct styledef std_styles[] = {
   { STYLE_DEFAULT,    COL_BLACK, FTF::NIL },
   { STYLE_LINENUMBER, COL_BLACK, FTF::NIL },
   { STYLE_BRACELIGHT, COL_BRIGHTRED, FTF::BOLD },
   { STYLE_BRACEBAD,   COL_BRIGHTRED, FTF::BOLD|FTF::ITALIC }  // Somebody set us up the bomb
};

static const struct styledef c_styles[] = {
   { SCE_C_DEFAULT,                COL_BLACK, FTF::NIL },          // What you say?
   { SCE_C_COMMENT,                COL_GREY, FTF::NIL },           // Standard C comment
   { SCE_C_COMMENTLINE,            COL_GREY, FTF::NIL },           // // style comment
   { SCE_C_COMMENTDOC,             COL_LIGHTSLATEGREY, FTF::NIL }, // Double-star comments
   { SCE_C_NUMBER,                 COL_BLUE, FTF::NIL },           // Any number or float
   { SCE_C_WORD,                   COL_FIREBRICK, FTF::NIL },
   { SCE_C_STRING,                 COL_RED, FTF::NIL },          // Strings "..."
   { SCE_C_CHARACTER,              COL_RED, FTF::NIL },          // Characters ' '
   { SCE_C_UUID,                   COL_BRIGHTRED, FTF::NIL },    // \n, \r
   { SCE_C_PREPROCESSOR,           COL_FORESTGREEN, FTF::NIL },  // #include, #define etc
   { SCE_C_OPERATOR,               COL_BLACK, FTF::NIL },        // + - *
   { SCE_C_IDENTIFIER,             COL_BLACK, FTF::NIL },        // The default colour
   { SCE_C_STRINGEOL,              COL_BRIGHTRED, FTF::NIL },
   { SCE_C_VERBATIM,               COL_BRIGHTRED, FTF::NIL },
   { SCE_C_REGEX,                  COL_BLUE, FTF::NIL },
   { SCE_C_COMMENTLINEDOC,         COL_GREY, FTF::NIL }, // // style comment
   { SCE_C_WORD2,                  COL_BRIGHTRED, FTF::NIL },
   { SCE_C_COMMENTDOCKEYWORD,      COL_GREY, FTF::NIL },
   { SCE_C_COMMENTDOCKEYWORDERROR, COL_GREY, FTF::NIL },
   { SCE_C_GLOBALCLASS,            COL_RED, FTF::NIL }
};

void ScintillaKTK::SetStyles(const struct styledef *Def, int Total)
{
   kt::Log log("SetStyles");
   int i, index;

   log.branch("%d", Total);

   for (i=0; i < Total; i++) {
      index = Def[i].Index;
      WndProc(SCI_STYLESETFONT, index, (MAXINT)"Clean"); // Font name, must be set to something
      WndProc(SCI_STYLESETSIZE, index, 10); // Default Size
      WndProc(SCI_STYLESETFORE, index, SCICOLOUR((uint8_t)(Def[i].Colour>>16), (uint8_t)(Def[i].Colour>>8), Def[i].Colour));

      if ((index IS STYLE_BRACELIGHT) or (index IS STYLE_BRACEBAD)) {
         WndProc(SCI_STYLESETBACK, index, SCICOLOUR(255, 255, 200));
      }

      if ((Def[i].FontStyle & FTF::BOLD) != FTF::NIL) WndProc(SCI_STYLESETBOLD, index, 1);
      if ((Def[i].FontStyle & FTF::ITALIC) != FTF::NIL) WndProc(SCI_STYLESETITALIC, index, 1);
   }

   WndProc(SCI_STYLESETBACK, STYLE_DEFAULT, (long int)SCICOLOUR(scintilla->BkgdColour.Red, scintilla->BkgdColour.Green, scintilla->BkgdColour.Blue));
   WndProc(SCI_STYLESETFORE, STYLE_DEFAULT, (long int)SCICOLOUR(scintilla->TextColour.Red, scintilla->TextColour.Green, scintilla->TextColour.Blue));
}

/*********************************************************************************************************************
** This is the main entry point, we're called from the Init action here.
*/

ScintillaKTK::ScintillaKTK(extScintilla *Scintilla)
:  scintilla(Scintilla)
{
   lastkeytrans[0] = 0;
   idle_timer_on   = false;
   lastticktime    = 0;
   captured_mouse  = false;

   // The platform Window is identified by the Kotuku Scintilla object (see pan_window.cxx)

   wMain = Scintilla;

   SetStyles(c_styles, std::ssize(c_styles));
   SetStyles(std_styles, std::ssize(std_styles));

   SendScintilla(SCI_SETMODEVENTMASK, SC_MOD_INSERTTEXT, SC_MOD_DELETETEXT |
      SC_PERFORMED_USER | SC_PERFORMED_UNDO | SC_PERFORMED_REDO |
      SC_MULTISTEPUNDOREDO | SC_LASTSTEPINUNDOREDO | SC_MOD_BEFOREINSERT |
      SC_MOD_BEFOREDELETE | SC_MULTILINEUNDOREDO);

   // We're always UTF-8

   SendScintilla(SCI_SETCODEPAGE, SC_CP_UTF8);

   SetTicking(true);
}

ScintillaKTK::~ScintillaKTK()
{

}

//********************************************************************************************************************

void ScintillaKTK::Finalise()
{
   kt::Log(__FUNCTION__).trace("");

   SetTicking(true);
   ScintillaBase::Finalise();
}

//********************************************************************************************************************

void ScintillaKTK::CreateCallTipWindow(Scintilla::PRectangle rc)
{
   kt::Log(__FUNCTION__).trace("");
}

//********************************************************************************************************************

void ScintillaKTK::AddToPopUp(const char *label, int cmD, bool enabled)
{
   kt::Log(__FUNCTION__).trace("%s", label);

   // The one and only Menu object is a member of ScintillaBase: Menu popup;

   if (auto menu = OBJECTPTR(popup.GetID()); menu) {
      auto buffer = std::format("<item text=\"{}\"></item>", label);
      acDataFeed(menu, nullptr, DATA::XML,
         std::span<const int8_t>((const int8_t *)buffer.data(), buffer.size()));
   }
}

//********************************************************************************************************************

void ScintillaKTK::SetVerticalScrollPos()
{
   kt::Log log(__FUNCTION__);

   log.traceBranch("%d", topLine);

   DwellEnd(true); // Cancel any current mouse hover
/*
   if (!scintilla->ScrollLocked) {
      scroll.ViewSize = -1;
      scroll.PageSize = -1;
      scroll.Position = topLine * vs.lineHeight;
      scroll.Unit     = vs.lineHeight;
      if (glBitmap) QueueAction(sc::UpdateScroll::id, scintilla->VScrollID, &scroll);
      else ActionMsg(sc::UpdateScroll::id, scintilla->VScrollID, &scroll);
   }
*/
}

//********************************************************************************************************************

void ScintillaKTK::SetHorizontalScrollPos()
{
   kt::Log log(__FUNCTION__);

   log.traceBranch("%d", xOffset);

   DwellEnd(true); // Cancel any current mouse hover
/*
   scroll.ViewSize = -1;
   scroll.PageSize = -1;
   scroll.Position = xOffset;
   scroll.Unit     = vs.lineHeight;
   if (glBitmap) QueueAction(sc::UpdateScroll::id, scintilla->HScrollID, &scroll);
   else ActionMsg(sc::UpdateScroll::id, scintilla->HScrollID, &scroll);
*/
}

/*********************************************************************************************************************
** nMax: Total number of lines in document
** nPage: Number of lines per view.
*/

bool ScintillaKTK::ModifyScrollBars(int nMax, int nPage)
{
   kt::Log log(__FUNCTION__);

   if (scintilla->ScrollLocked) return FALSE;
#if 0
   // Note: Sometimes Scintilla will attempt to change the scrollbars in the middle of surface redrawing.  This can
   // cause problems, so message delays are used in those cases.

   // Horizontal scrollbar

   scroll.ViewSize = -1;
   scroll.PageSize = SendScintilla(SCI_GETSCROLLWIDTH);
   scroll.Position = xOffset;
   scroll.Unit     = vs.lineHeight;

   log.traceBranch("Lines: %d, PageWidth: %d/%d, Delay: %c", nMax, scroll.PageSize, scroll.ViewSize, (glBitmap ? 'Y' : 'N'));

   if (glBitmap) QueueAction(sc::UpdateScroll::id, scintilla->HScrollID, &scroll);
   else ActionMsg(sc::UpdateScroll::id, scintilla->HScrollID, &scroll);

   // Vertical scrollbar

   lines = SendScintilla(SCI_GETLINECOUNT);

   if ((scintilla->Flags & SCIF::EXT_PAGE) != SCIF::NIL) {
      // Scintilla's nMax variable caters for all the lines, plus the height of the viewing area.

      scroll.ViewSize = nPage * vs.lineHeight;
      scroll.PageSize = nMax * vs.lineHeight;
   }
   else {
      scroll.ViewSize = -1;
      scroll.PageSize = (nMax+1) * vs.lineHeight;
      if ((scroll.PageSize > scintilla->Surface.Height) and (vs.lineHeight > 0)) {
         scroll.PageSize -= (scintilla->Surface.Height / vs.lineHeight);
      }
   }

   scroll.Position = topLine * vs.lineHeight;
   scroll.Unit     = vs.lineHeight;

   log.trace("PageLength: %d/%d (lines: %d/%d), Pos: %d", scroll.PageSize, scroll.ViewSize, lines, nMax, scroll.Position);

   if (glBitmap) QueueAction(sc::UpdateScroll::id, scintilla->VScrollID, &scroll);
   else ActionMsg(sc::UpdateScroll::id, scintilla->VScrollID, &scroll);
#endif
   return TRUE;
}

//********************************************************************************************************************
// Called after SCI_SETWRAPMODE and SCI_SETHSCROLLBAR.

void ScintillaKTK::ReconfigureScrollBars()
{
   kt::Log log(__FUNCTION__);
   log.traceBranch();

/*
	 if (horizontalScrollBarVisible) acShowID(scintilla->HScroll);
	 else acHideID(scintilla->HScroll);

	 if (verticalScrollBarVisible) acShowID(scintilla->VScroll);
	 else acHideID(scintilla->VScroll);
*/
}

/****************************************************************************
** Copies the selected text section to the Pandora clipboard.
*/

void ScintillaKTK::CopyToClipboard(const Scintilla::SelectionText &selectedText)
{
   kt::Log log(__FUNCTION__);
   log.traceBranch();

   auto clipboard = objClipboard::create { };
   if ((clipboard.ok()) and (selectedText.s) and (selectedText.len > 0)) {
      if (!clipboard->addText(std::string_view(selectedText.s, size_t(selectedText.len - 1)))) {

      }
   }
}

/*********************************************************************************************************************
** Cut the selected text to the clipboard.
*/

void ScintillaKTK::Cut()
{
   kt::Log log(__FUNCTION__);
   log.traceBranch();

   if (SendScintilla(SCI_GETSELECTIONSTART) != SendScintilla(SCI_GETSELECTIONEND)) {
      Scintilla::SelectionText text;
      CopySelectionRange(&text);
      CopyToClipboard(text);
      ClearSelection();
   }
}

/*********************************************************************************************************************
** Copy the selected text to the clipboard.
*/

void ScintillaKTK::Copy()
{
   kt::Log log(__FUNCTION__);
   log.traceBranch();

   if (SendScintilla(SCI_GETSELECTIONSTART) != SendScintilla(SCI_GETSELECTIONEND)) {
      Scintilla::SelectionText text;
      CopySelectionRange(&text);
      CopyToClipboard(text);
   }
}

//********************************************************************************************************************

void ScintillaKTK::Paste()
{
   kt::Log log(__FUNCTION__);

   log.traceBranch();

   objClipboard::create clipboard = { };
   if (clipboard.ok()) {
      kt::vector<std::string> files;
      if (!clipboard->getFiles(CLIPTYPE::TEXT, 0, nullptr, files, nullptr)) {
         objFile::create file = { fl::Path(files[0]), fl::Flags(FL::READ) };
         if (file.ok()) {
            int64_t len, size;
            if ((!file->getSize(size)) and (size > 0)) {
               std::vector<char> buffer(size);
               if (!file->read(std::span<int8_t>((int8_t *)buffer.data(), size_t(size)), &len)) {
                  pdoc->BeginUndoAction();

                     ClearSelection();
                     pdoc->InsertString(CurrentPosition(), (char *)buffer.data(), len);
                     SetEmptySelection(CurrentPosition() + len);

                  pdoc->EndUndoAction();

                  NotifyChange();
                  Redraw();

                  calc_longest_line(scintilla);
               }
               else error_dialog("Paste Error", "Failed to read data from the clipboard file.", ERR::Okay);
            }
         }
         else error_dialog("Paste Error", std::format("Failed to load clipboard file \"{}\"", files[0]), ERR::Okay);
      }
   }
}

//********************************************************************************************************************
// This is used for the drag and drop of selected text.

void ScintillaKTK::ClaimSelection()
{
   kt::Log log(__FUNCTION__);
   log.traceBranch();
   if (!SelectionEmpty()) primarySelection = true;
   else primarySelection = false;
}

//********************************************************************************************************************

void ScintillaKTK::NotifyChange()
{
   // This method is useless because Scintilla immediately follows this
   // up with the SCN_MODIFIED message, which carries a lot more detail.
}

//********************************************************************************************************************
// Sometimes Scintilla will report events that have occurred in the text editor.

void ScintillaKTK::NotifyParent(Scintilla::SCNotification scn)
{
   kt::Log log("SciMsg");
   int code;

   if (!(code = scn.nmhdr.code)) return;

   if (code IS SCN_UPDATEUI) {
      // Either the text or styling of the document has changed or the selection range has changed. Now would be a good
      // time to update any container UI elements that depend on document or view state.  This was previously called
      // SCN_CHECKBRACE because a common use is to check whether the caret is next to a brace and set highlights on
      // this brace and its corresponding matching brace.

      log.traceBranch("[UPDATEUI] $%x", scn.updated);

      // Update the cursor-row and cursor-column fields if they've changed.

      int pos = SendScintilla(SCI_GETCURRENTPOS);
      if (pos != oldpos) {
         oldpos = pos;
         scintilla->CursorRow = SendScintilla(SCI_LINEFROMPOSITION, pos);
         scintilla->CursorCol = SendScintilla(SCI_GETCOLUMN, pos);
         if (SendScintilla(SCI_GETLEXER) IS SCLEX_CPP) braceMatch();

         // Event report has to be delayed, as we otherwise get interference in the drawing process.
         scintilla->ReportEventFlags |= SEF::CURSOR_POS;
         QueueAction(sci::ReportEvent::id, scintilla->UID);
      }
   }
   else if (code IS SCN_STYLENEEDED) {
      // If you used SCLEX_CONTAINER to make the container act as the lexer, you will receive this notification when
      // Scintilla is about to display or print text that requires styling. You are required to style the text from the
      // line that contains the position returned by SCI_GETENDSTYLED up to the position passed in
      // SCNotification.position.

      log.trace("[STYLENEEDED]");

      //MyStyleRoutine(start, scn.position);

   }
   else if (code IS SCN_DOUBLECLICK) {
      // Mouse buttons have been interpreted as a double click

      log.trace("[DOUBLECLICK]");
   }
   else if (code IS SCN_MODIFYATTEMPTRO) {
      // An attempt has been made to modify the document when in read-only mode

      log.trace("[MODIFYATTEMPTRO]");

      scintilla->ReportEventFlags |= SEF::FAIL_RO;
      QueueAction(sci::ReportEvent::id, scintilla->UID);
   }
   else if (code IS SCN_CHARADDED) {
      // This is sent when the user types an ordinary text character (as opposed to a command character) that is
      // entered into the text. The container can use this to decide to display a call tip or an auto completion list.
      // The character is in SCNotification.ch.

      log.traceBranch("[CHARADDED]");

      int pos = SendScintilla(SCI_GETSELECTIONSTART);
      if (pos != SendScintilla(SCI_GETSELECTIONEND)) return;

      // Auto-indent management for the enter key

      if ((scintilla->AutoIndent) and ((scn.ch IS '\r') or (scn.ch IS '\n'))) {
         int row, indent, col;

         pos = SendScintilla(SCI_GETCURRENTPOS);
         row = SendScintilla(SCI_LINEFROMPOSITION, pos);
         col = SendScintilla(SCI_GETCOLUMN, pos);

         if (row > 1) {
            indent = SendScintilla(SCI_GETLINEINDENTATION, row - 1); // indent = The indentation of the previous line

            if (indent <= col);
            else {
               // Indent the current line
               SendScintilla(SCI_SETLINEINDENTATION, row, indent);

               // Move the cursor to the start of the line, after following the indentation whitespace
               pos = SendScintilla(SCI_GETLINEINDENTPOSITION, row);

               SendScintilla(SCI_SETSEL, (uint32_t)-1, pos);
            }
         }
      }

      scintilla->ReportEventFlags |= SEF::NEW_CHAR;
      QueueAction(sci::ReportEvent::id, scintilla->UID);
   }
   else if (code IS SCN_SAVEPOINTREACHED) {
      // The document is unmodified (recently saved)

      log.trace("[SAVEPOINTREACHED]");
   }
   else if (code IS SCN_SAVEPOINTLEFT) {
      // The document has just been modified

      log.trace("[SAVEPOINTLEFT]");

      if (!scintilla->HoldModify) {
         scintilla->setModified(TRUE);
      }
      else {
         // 'Hold Modifications' means that we have to tell Scintilla that the document is unmodified.

         SendScintilla(SCI_SETSAVEPOINT);
      }
   }
   else if (code IS SCN_KEY) {
      // Reports all keys pressed but not consumed by Scintilla

      log.trace("[KEY]");
   }
   else if (code IS SCN_MODIFIED) {
      // This notification is sent when the text or styling of the document changes or is about to change. You can set
      // a mask for the notifications that are sent to the container with SCI_SETMODEVENTMASK. The notification
      // structure contains information about what changed, how the change occurred and whether this changed the number
      // of lines in the document. No modifications may be performed while in a SCN_MODIFIED event.
      //
      // See HTML documentation for more information.

      log.trace("[MODIFIED] Type: %d, Length: %d, LinesAdded: %d, Line: %d", scn.modificationType, scn.length, scn.linesAdded, scn.line);
   }
   else if (code IS SCEN_SETFOCUS) {
      log.trace("[SETFOCUS]");
   }
   else if (code IS SCEN_KILLFOCUS) {
      log.trace("[KILLFOCUS]");
   }
   else if (code IS SCN_MACRORECORD) {
      // The SCI_STARTRECORD and SCI_STOPRECORD messages enable and disable macro recording. When enabled, each time a
      // recordable change occurs, the SCN_MACRORECORD notification is sent to the container. It is up to the container
      // to record the action. To see the complete list of SCI_* messages that are recordable, search the Scintilla
      // source Editor.cxx for Editor::NotifyMacroRecord. The fields of SCNotification set in this notification are:
      // Field    Usage
      // message  The SCI_* message that caused the notification.
      // wParam   The value of wParam in the SCI_* message.
      // lParam   The value of lParam in the SCI_* message.

      log.trace("[MACRORECORD]");
   }
   else if (code IS SCN_MARGINCLICK) {
      // This notification tells the container that the mouse was clicked inside a margin that was marked as sensitive
      // (see SCI_SETMARGINSENSITIVEN). This can be used to perform folding or to place breakpoints. The following
      // SCNotification fields are used:
      //
      // Field   Usage
      // modifiers The appropriate combination of SCI_SHIFT, SCI_CTRL and SCI_ALT to indicate the keys that were held down at the time of the margin click.
      // position  The position of the start of the line in the document that corresponds to the margin click.
      // margin    The margin number that was clicked.

      log.trace("[MARGINCLICK]");
   }
   else if (code IS SCN_NEEDSHOWN) {
      // Scintilla has determined that a range of lines that is currently invisible should be made visible. An example of where this may be
      // needed is if the end of line of a contracted fold point is deleted. This message is sent to the container in case it wants to make the
      // line visible in some unusual way such as making the whole document visible. Most containers will just ensure each line in the range is
      // visible by calling SCI_ENSUREVISIBLE. The position and length fields of SCNotification indicate the range of the document that should be
      // made visible.

      int i, first, last;

      first = SendScintilla(SCI_LINEFROMPOSITION, scn.position);
      last = SendScintilla(SCI_LINEFROMPOSITION, scn.position + scn.length - 1);

      log.trace("[NEEDSHOWN] First: %d, Last: %d", first, last);

      for (i=first; i < last; ++i) {
         SendScintilla(SCI_ENSUREVISIBLE, i);
      }
   }
   else if (code IS SCN_PAINTED) {
      // Painting has just been done. Useful when you want to update some other widgets based on a change in Scintilla,
      // but want to have the paint occur first to appear more responsive. There is no other information in SCNotification.

      log.trace("[PAINTED]");
   }
   else if (code IS SCN_USERLISTSELECTION) {
      // The user has selected an item in a user list. The SCNotification fields used are:
      // Field   Usage
      // wParam  This is set to the listType parameter from the SCI_USERLISTSHOW message that initiated the list.
      // text    The text of the selection.

      log.trace("[USERLISTSELECTION]");
   }
   else if (code IS SCN_DWELLSTART) {
      // Generated when the user keeps the mouse in one position for the dwell period (see SCI_SETMOUSEDWELLTIME).
      //
      // position: This is the nearest position in the document to the position where the mouse pointer was lingering.
      // x, y: Where the pointer lingered. The position field is set to SCI_POSITIONFROMPOINTCLOSE(x, y).

      log.trace("[DWELLSTART]");
   }
   else if (code IS SCN_DWELLEND) {
      // Generated after a SCN_DWELLSTART and the mouse is moved or other activity such as key press indicates the dwell is over.

      log.trace("[DWELLEND]");
   }
   else if (code IS SCN_ZOOM) {
      // Unsupported/Redundant Scintilla feature
   }
   else if (code IS SCN_HOTSPOTCLICK) {
      log.trace("[HOTSPOTCLICK]");

   }
   else if (code IS SCN_HOTSPOTDOUBLECLICK) {
      // Generated when the user clicks or double clicks on text that is in a style with the hotspot attribute set. This notification can be
      // used to link to variable definitions or web pages. The position field is set the text position of the click or double click and
      // the modifiers field set to the key modifiers held down in a similar manner to SCN_KEY.

      log.trace("[HOTSPOTDOUBLECLICK]");
   }
   else if (code IS SCN_CALLTIPCLICK) {
      // Generated when the user clicks on a calltip. This notification can be used to display the next function prototype when a function name
      // is overloaded with different arguments. The position field is set to 1 if the click is in an up arrow, 2 if in a down arrow, and 0 if
      // elsewhere.

      log.trace("[CALLTIPCLICK]");
   }
   else if (code IS SCN_AUTOCSELECTION) {
      // The user has selected an item in an autocompletion list. The notification is sent before the selection is inserted. Automatic
      // insertion can be cancelled by sending a SCI_AUTOCCANCEL message before returning from the notification. The SCNotification fields
      // used are:
      //
      // lParam: The start position of the word being completed.
      // text: The text of the selection.

      log.trace("[AUTOCSELECTION]");
   }
   else if (code IS 2012) {
      // Deprecated
   }
   else log.traceWarning("Notification code %d unsupported.", code);
}

//********************************************************************************************************************

void ScintillaKTK::ScrollText(int linesToMove)
{
   // TODO Phase 3: Start with a full repaint of the client area, then optimise by shifting the bitmap content.
}

//********************************************************************************************************************

void ScintillaKTK::SetTicking(bool On)
{
   kt::Log log(__FUNCTION__);
   log.traceBranch("State: %d", On);

   if (!On) ticking_on = false;
   else if (!ticking_on) {
      ticking_on = true;
      lastticktime = (PreciseTime() / 1000LL);
   }
}

//********************************************************************************************************************
// Grab or release the mouse and keyboard.  This is usually called when the user clicks a mouse button and holds it
// while dragging the mouse (e.g. when highlighting text).

void ScintillaKTK::SetMouseCapture(bool On)
{
   kt::Log log(__FUNCTION__);
   log.traceBranch("State: %d", On);
   captured_mouse = On;
}

/*********************************************************************************************************************
** Simply returns the capture state.
*/

bool ScintillaKTK::HaveMouseCapture()
{
   //log.trace("HaveMouseCapture()");
   return captured_mouse;
}

//********************************************************************************************************************

sptr_t ScintillaKTK::WndProc(unsigned int iMessage, uptr_t wParam, sptr_t lParam)
{
   switch(iMessage) {

   case SCI_GETDIRECTFUNCTION:
      return reinterpret_cast<sptr_t>(DirectFunction);

   case SCI_GETDIRECTPOINTER:
      return reinterpret_cast<sptr_t>(this);

   default:
      return ScintillaBase::WndProc(iMessage, wParam, lParam);
   }
   return 0;
}

//********************************************************************************************************************

sptr_t ScintillaKTK::DirectFunction(ScintillaKTK *sci, unsigned int iMessage, uptr_t wParam, sptr_t lParam)
{
   return sci->WndProc(iMessage, wParam, lParam);
}

/*********************************************************************************************************************
** Do nothing; this is a Win32 support function.
*/

sptr_t ScintillaKTK::DefWndProc(unsigned int iMessage, uptr_t wParam, sptr_t lParam)
{
   return 0;
}

/*********************************************************************************************************************
** Refer to the pan_surface.cxx file for the drawing routines that are used when panDraw() is active.
**
** TODO Phase 2: Nothing calls panDraw() until the deferred repaint of the Canvas bitmap is implemented.
*/

void ScintillaKTK::panDraw(objBitmap *Bitmap)
{
   kt::Log log(__FUNCTION__);
   Scintilla::PRectangle rcClient;
   Scintilla::Surface *surface;

   if (this->paintState != notPainting) return; // Recursion prevention

   Scintilla::PRectangle paintrect(Bitmap->Clip.Left, Bitmap->Clip.Top, Bitmap->Clip.Right, Bitmap->Clip.Bottom);

   this->paintState = painting;
   this->rcPaint    = paintrect;
   rcClient         = GetClientRectangle();
   this->paintingAllText = rcPaint.Contains(rcClient);

   log.traceBranch("Area: %dx%d - %dx%d", rcClient.left, rcClient.top, rcClient.Width(), rcClient.Height());

   // Create a new surface object (SurfacePan)

   if ((surface = Scintilla::Surface::Allocate())) {
      surface->Init(Bitmap, wMain.GetID());
      this->Paint(surface, paintrect);      // Get Scintilla to do the painting.  See Editor::paint()
      surface->Release();
      delete surface;
   }

   if (this->paintState IS paintAbandoned) {
      // Painting area was insufficient to cover new styling or brace highlight positions, word wrapping etc.  This
      // means that the clipping area needs to be extended.

      // TODO Phase 2: Widen the repaint to the full client area and reschedule it.
   }

   this->paintState = notPainting;
}

//********************************************************************************************************************
// Invalidate the entire client area.

void ScintillaKTK::panRedraw()
{
   Redraw();
}

/*********************************************************************************************************************
** Called on initialisation and from the SetFont() method whenever the user opts to change the font.  The font objects
** are read from the Scintilla object by SurfacePan::GetFont().
*/

void ScintillaKTK::panFontChanged()
{
   kt::Log log(__FUNCTION__);
   log.traceBranch();

   this->InvalidateStyleRedraw();
   this->RefreshStyleData();
}

//********************************************************************************************************************

void ScintillaKTK::panWordwrap(int Value)
{
   kt::Log log(__FUNCTION__);
   log.traceBranch("%d", Value);

//   int bytepos = SendScintilla(SCI_POSITIONFROMLINE, topLine);

   SendScintilla(SCI_SETWRAPMODE, Value);

   // Scintilla likes to process wordwrapping in its timer, but this causes
   // issues with the scrollbar (and potentially other problems) so this
   // loop will force the wordwrap to be processed immediately.

   scintilla->ScrollLocked++;
   while (Idle());
   scintilla->ScrollLocked--;

   calc_longest_line(scintilla);
/*
   if (!Value) {
      topLine = SendScintilla(SCI_LINEFROMPOSITION, bytepos);
      SetVerticalScrollPos();
   }
*/
   //SendScintilla(SCI_ENSUREVISIBLE, SendScintilla(SCI_LINEFROMPOSITION, SendScintilla(SCI_GETCURRENTPOS)));
}

//********************************************************************************************************************

void ScintillaKTK::panIdleEvent()
{
   if (idle_timer_on) {
      bool keepidling = Idle();

      if (!keepidling) {
         //SetIdle(false);
         idle_timer_on = FALSE;
      }
   }

   if (ticking_on) {
#ifdef _DEBUG
      if ((int)((PreciseTime()/1000LL) - lastticktime) >= caret.period) {
#else
      if ((int)((PreciseTime()/1000LL) - lastticktime) >= caret.period / 5) {
#endif
    	 Tick();  // Goes to -> Editor::Tick()
         lastticktime = (PreciseTime()/1000LL);
      }
   }
}

//********************************************************************************************************************

void ScintillaKTK::panKeyDown(int Key, KQ Flags)
{
   bool consumed;

   // After we call KeyDown(), Scintilla will call KeyDefault()

   KeyDown(Key, (Flags & KQ::SHIFT) != KQ::NIL, (Flags & KQ::CTRL) != KQ::NIL, (Flags & KQ::ALT) != KQ::NIL, &consumed);
}

//********************************************************************************************************************

int ScintillaKTK::KeyDefault(int key, int modifiers)
{
   //log.msg("%d, $%.8x", key, modifiers);
   AddCharUTF(lastkeytrans, strlen(lastkeytrans), false);
   return 1;
}

//********************************************************************************************************************

void ScintillaKTK::panMousePress(JET Button, double x, double y)
{
   kt::Log log(__FUNCTION__);

   log.traceBranch("%.0fx%.0f", x, y);

   if (Button IS JET::LMB) {
      // This disables the current selection (effectively eliminating the potential for drag and drop).

      SetEmptySelection(CurrentPosition());
      Scintilla::Point point((int)x, (int)y);
      ButtonDown(point, (PreciseTime()/1000LL), scintilla->KeyShift, scintilla->KeyCtrl, scintilla->KeyAlt);
   }
   else if (Button IS JET::RMB) {




   }
}

//********************************************************************************************************************

void ScintillaKTK::panMouseMove(double x, double y)
{
   Scintilla::Point point((int)x, (int)y);
   ButtonMove(point);
}

//********************************************************************************************************************

void ScintillaKTK::panMouseRelease(JET Button, double x, double y)
{
   kt::Log log(__FUNCTION__);
   Scintilla::Point point((int)x, (int)y);

   log.trace("%.0fx%.0f", x, y);

   if (Button IS JET::LMB) {
      ButtonUp(point, (PreciseTime()/1000LL), scintilla->KeyCtrl);
   }
}

//********************************************************************************************************************

void ScintillaKTK::panResized()
{
   kt::Log log(__FUNCTION__);
   log.traceBranch();
   ChangeSize();
}

//********************************************************************************************************************

void ScintillaKTK::panScrollToX(double x)
{
   kt::Log log(__FUNCTION__);
   log.traceBranch("%.2f", x);
   HorizontalScrollTo((int)(x));
}

//********************************************************************************************************************

void ScintillaKTK::panScrollToY(double y)
{
   kt::Log log(__FUNCTION__);
   log.traceBranch("%.2f", y);
   ScrollTo((int)(y / vs.lineHeight));
}

//********************************************************************************************************************

void ScintillaKTK::panGotFocus()
{
   SetFocusState(TRUE);
}

//********************************************************************************************************************

void ScintillaKTK::panLostFocus()
{
   SetFocusState(FALSE);
}

/*********************************************************************************************************************
** Get the cursor position.
*/

void ScintillaKTK::panGetCursorPosition(int *line, int *index)
{
   int pos, lin, linpos;

   pos = SendScintilla(SCI_GETCURRENTPOS);
   lin = SendScintilla(SCI_LINEFROMPOSITION,pos);
   linpos = SendScintilla(SCI_POSITIONFROMLINE,lin);

   *line = lin;
   *index = pos - linpos;
}

/*********************************************************************************************************************
** Set the cursor position.
*/

void ScintillaKTK::panSetCursorPosition(int line, int index)
{
   kt::Log log(__FUNCTION__);
   int pos, eol;

   log.trace("Line: %d, Index: %d", line, index);

   pos = SendScintilla(SCI_POSITIONFROMLINE,line) + index;
   eol = SendScintilla(SCI_GETLINEENDPOSITION,line);

   if (pos > eol) pos = eol;

   SendScintilla(SCI_GOTOPOS, pos);
}

/*********************************************************************************************************************
** Ensure a line is visible (expands folded zones).
*/

void ScintillaKTK::panEnsureLineVisible(int line)
{
   SendScintilla(SCI_ENSUREVISIBLEENFORCEPOLICY,line);
}

/*********************************************************************************************************************
** Set the lexer.  To turn off the lexer, use SCLEX_NULL.
*/

void ScintillaKTK::SetLexer(uptr_t LexID)
{
   kt::Log log(__FUNCTION__);
   log.branch("Using lexer %d", (int)LexID);

//   SendScintilla(SCI_STYLERESETDEFAULT);
   SendScintilla(SCI_SETLEXER, LexID);
   //SendScintilla(SCI_CLEARDOCUMENTSTYLE);

   SendScintilla(SCI_STARTSTYLING, 0, 0x1f);
   Redraw();
}

void ScintillaKTK::SetLexerLanguage(const char *languageName)
{
   SendScintilla(SCI_SETLEXERLANGUAGE, 0UL, languageName);
}

/*********************************************************************************************************************
** Handle brace matching.
*/

void ScintillaKTK::braceMatch()
{
   long braceAtCaret, braceOpposite;

   findMatchingBrace(braceAtCaret, braceOpposite, braceMode);

   if (braceAtCaret >= 0 and braceOpposite < 0) {
      SendScintilla(SCI_BRACEBADLIGHT, braceAtCaret);
      SendScintilla(SCI_SETHIGHLIGHTGUIDE,0UL);
   }
   else {
      char chBrace = SendScintilla(SCI_GETCHARAT,braceAtCaret);

      SendScintilla(SCI_BRACEHIGHLIGHT,braceAtCaret,braceOpposite);

      long columnAtCaret = SendScintilla(SCI_GETCOLUMN,braceAtCaret);
      long columnOpposite = SendScintilla(SCI_GETCOLUMN,braceOpposite);

      if (chBrace IS ':') {
         long lineStart = SendScintilla(SCI_LINEFROMPOSITION,braceAtCaret);
         long indentPos = SendScintilla(SCI_GETLINEINDENTPOSITION,lineStart);
         long indentPosNext = SendScintilla(SCI_GETLINEINDENTPOSITION,lineStart + 1);
         columnAtCaret = SendScintilla(SCI_GETCOLUMN,indentPos);
         long columnAtCaretNext = SendScintilla(SCI_GETCOLUMN,indentPosNext);
         long indentSize = SendScintilla(SCI_GETINDENT);

         if (columnAtCaretNext - indentSize > 1) columnAtCaret = columnAtCaretNext - indentSize;
         if (columnOpposite IS 0) columnOpposite = columnAtCaret;
      }

      long column = columnAtCaret;

      if (column > columnOpposite) column = columnOpposite;

      SendScintilla(SCI_SETHIGHLIGHTGUIDE,column);
   }
}

/*********************************************************************************************************************
** Check if the character at a position is a brace.
*/

long ScintillaKTK::checkBrace(long pos,int brace_style)
{
   long brace_pos = -1;
   char ch = SendScintilla(SCI_GETCHARAT,pos);

   if ((ch IS '{') or (ch IS '}')) {
      if (brace_style < 0) brace_pos = pos;
      else {
         int style = SendScintilla(SCI_GETSTYLEAT,pos) & 0x1f;

         if (style IS brace_style) brace_pos = pos;
      }
   }

   return brace_pos;
}

/*********************************************************************************************************************
** Find a brace and it's match.  Return TRUE if the current position is inside a pair of braces.
*/

bool ScintillaKTK::findMatchingBrace(long &brace,long &other,long mode)
{
   int brace_style;

/*   if (lexLanguage IS SCLEX_CPP) brace_style = '{';
   else*/ brace_style = -1;

   brace = -1;
   other = -1;

   long caretPos = SendScintilla(SCI_GETCURRENTPOS);

   if (caretPos > 0) brace = checkBrace(caretPos - 1,brace_style);

   bool isInside = FALSE;

   if (brace < 0 and mode) {
      brace = checkBrace(caretPos,brace_style);
      if (brace >= 0) isInside = TRUE;
   }

   if (brace >= 0) {
      other = SendScintilla(SCI_BRACEMATCH,brace);
      if (other > brace) isInside = !isInside;
   }

   return isInside;
}

// Move to the matching brace.
void ScintillaKTK::moveToMatchingBrace()
{
   gotoMatchingBrace(FALSE);
}

// Select to the matching brace.
void ScintillaKTK::selectToMatchingBrace()
{
   gotoMatchingBrace(TRUE);
}

// Move to the matching brace and optionally select the text.
void ScintillaKTK::gotoMatchingBrace(bool select)
{
   long braceAtCaret;
   long braceOpposite;

   bool isInside = findMatchingBrace(braceAtCaret,braceOpposite,TRUE);

   if (braceOpposite >= 0) {
      // Convert the character positions into caret positions based
      // on whether the caret position was inside or outside the
      // braces.
      if (isInside) {
         if (braceOpposite > braceAtCaret) braceAtCaret++;
         else braceOpposite++;
      }
      else {
         if (braceOpposite > braceAtCaret) braceOpposite++;
         else braceAtCaret++;
      }

      //ensureLineVisible(SendScintilla(SCI_LINEFROMPOSITION,braceOpposite));

      if (select) SendScintilla(SCI_SETSEL,braceAtCaret,braceOpposite);
      else SendScintilla(SCI_SETSEL,braceOpposite,braceOpposite);
   }
}
