
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

   SetStyleColours();
}

/*********************************************************************************************************************
** Apply the BkgdColour and TextColour of the Scintilla object.  Every lexer style shares the document background,
** while the special styles that follow STYLE_DEFAULT (line numbers, braces etc) keep their own.  Style 0 is the
** default style of every lexer and is also used for plain text, so it receives the TextColour.
*/

void ScintillaKTK::SetStyleColours()
{
   auto &bkgd = scintilla->BkgdColour, &text = scintilla->TextColour;
   const long bkgd_colour = SCICOLOUR(bkgd.Red, bkgd.Green, bkgd.Blue);
   const long text_colour = SCICOLOUR(text.Red, text.Green, text.Blue);

   for (int style=0; style <= STYLE_DEFAULT; style++) WndProc(SCI_STYLESETBACK, style, bkgd_colour);

   WndProc(SCI_STYLESETFORE, 0, text_colour);
   WndProc(SCI_STYLESETFORE, STYLE_DEFAULT, text_colour);
}

/*********************************************************************************************************************
** This is the main entry point, we're called from the Init action here.
*/

ScintillaKTK::ScintillaKTK(extScintilla *Scintilla)
:  scintilla(Scintilla)
{
   idle_on        = false;
   ticking_on     = false;
   captured_mouse = false;

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
}

ScintillaKTK::~ScintillaKTK()
{

}

//********************************************************************************************************************

void ScintillaKTK::Finalise()
{
   kt::Log(__FUNCTION__).trace("");

   SetTicking(false);
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

/*********************************************************************************************************************
** Scintilla calls these when topLine or xOffset change.  Scrollbars are external to the editor (see gui.scrollbar), so
** the scroll position is expressed by moving the Page within the View.
*/

void ScintillaKTK::SetVerticalScrollPos()
{
   kt::Log(__FUNCTION__).trace("%d", topLine);
   DwellEnd(true); // Cancel any current mouse hover
   panSyncPage();
}

void ScintillaKTK::SetHorizontalScrollPos()
{
   kt::Log(__FUNCTION__).trace("%d", xOffset);
   DwellEnd(true); // Cancel any current mouse hover
   panSyncPage();
}

/*********************************************************************************************************************
** Scintilla calls this with nMax = MaxScrollPos() + nPage - 1, where nPage is the number of lines that fit in the
** client area.  The Page is sized so that the furthest that it can be moved within the View corresponds to the
** maximum scroll positions of Scintilla, i.e. MaxScrollPos() lines vertically and the scroll width minus the width of
** the text area horizontally.  The result is true if the extent of the Page has changed.
*/

bool ScintillaKTK::ModifyScrollBars(int nMax, int nPage)
{
   kt::Log log(__FUNCTION__);

   int height = (nMax - nPage + 1) * vs.lineHeight + scintilla->ViewHeight;
   int width  = scintilla->ViewWidth;
   if (wrapState IS eWrapNone) width = std::max(width, scrollWidth + vs.fixedColumnWidth + vs.rightMarginWidth);

   if ((width IS scintilla->PageWidth) and (height IS scintilla->PageHeight)) return false;

   log.traceBranch("Lines: %d/%d, Page: %dx%d", nMax, nPage, width, height);

   scintilla->PageWidth  = width;
   scintilla->PageHeight = height;

   if (scintilla->Page) {
      scintilla->Page->setWidth(width);
      scintilla->Page->setHeight(height);
   }

   return true;
}

//********************************************************************************************************************
// Called after SCI_SETWRAPMODE and SCI_SETHSCROLLBAR.  Scrollbars are not managed by the editor, so there is nothing
// to show or hide.

void ScintillaKTK::ReconfigureScrollBars()
{
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

         if (row > 0) {
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
   // TODO Phase 3: Optimise by shifting the bitmap content and invalidating only the exposed band.
   Redraw();
}

/*********************************************************************************************************************
** Scintilla calls SetTicking(true) whenever the caret is shown at a new position.  The caret blink is restarted so that
** the caret remains visible for a full period after it moves.  See UpdateTimers() for the conditions under which the
** tick timer runs.
*/

void ScintillaKTK::SetTicking(bool On)
{
   kt::Log log(__FUNCTION__);
   log.traceBranch("State: %d", On);

   ticking_on = On;
   if (On) {
      timer.ticksToWait = caret.period;
      UpdateTimers();
   }
}

/*********************************************************************************************************************
** Scintilla requests idle processing when it has background work to do, such as wrapping lines.  Returning true
** confirms that Idle() will be called until it reports that the work is complete.
*/

bool ScintillaKTK::SetIdle(bool On)
{
   idle_on = On;
   if (On) UpdateTimers();
   return true;
}

/*********************************************************************************************************************
** Start the tick and idle timers if they are required.  Timers are not stopped here because a timer cannot be removed
** while its own callback is running.  Instead, tick_timer() and idle_timer() end their subscriptions when panTick() or
** panIdle() report that there is nothing left to do.  The timers are subscribed for the Scintilla object, so that
** they are removed with it.
*/

void ScintillaKTK::UpdateTimers()
{
   kt::SwitchContext ctx(scintilla);

   if ((TickRequired()) and (not scintilla->TickTimer)) {
      // Tick() counts down the caret period and other delays in steps of tickSize milliseconds.
      SubscribeTimer(double(timer.tickSize) / 1000.0, C_FUNCTION(tick_timer), &scintilla->TickTimer);
   }

   if ((idle_on) and (not scintilla->IdleTimer)) {
      SubscribeTimer(IDLE_INTERVAL, C_FUNCTION(idle_timer), &scintilla->IdleTimer);
   }
}

/*********************************************************************************************************************
** Ticking is only necessary for blinking the caret while the editor has the focus, and for scrolling while the mouse
** is captured.  Tick() repeats the last mouse movement while the mouse is captured, which scrolls the document if the
** pointer is held outside the client area.
*/

bool ScintillaKTK::TickRequired()
{
   return (HaveMouseCapture()) or ((ticking_on) and (hasFocus) and (caret.period > 0));
}

//********************************************************************************************************************
// Called by tick_timer().  Returns false if the timer is no longer required.

bool ScintillaKTK::panTick()
{
   if (not TickRequired()) return false;
   Tick(); // Goes to -> Editor::Tick()
   return TickRequired();
}

//********************************************************************************************************************
// Called by idle_timer().  Returns false if the timer is no longer required.

bool ScintillaKTK::panIdle()
{
   if (idle_on) idle_on = Idle();
   return idle_on;
}

//********************************************************************************************************************
// Scintilla captures the mouse while the user holds a button and drags (e.g. when selecting text).  The scene already
// directs all pointer input to the Canvas while the LMB is held, so the capture state is only recorded here.  The tick
// timer runs while the mouse is captured, so that holding the pointer outside the client keeps scrolling.

void ScintillaKTK::SetMouseCapture(bool On)
{
   kt::Log log(__FUNCTION__);
   log.traceBranch("State: %d", On);
   captured_mouse = On;
   if (On) UpdateTimers();
}

//********************************************************************************************************************
// Scintilla shows the arrow over the selection to indicate that the text can be dragged.  Text drag and drop is not
// supported yet (see panMousePress()), so the text cursor is kept.  Margins use the reverse arrow and are unaffected.

void ScintillaKTK::DisplayCursor(Scintilla::Window::Cursor Cursor)
{
   if (Cursor IS Scintilla::Window::cursorArrow) Cursor = Scintilla::Window::cursorText; // TODO Phase 7
   ScintillaBase::DisplayCursor(Cursor);
}

//********************************************************************************************************************

bool ScintillaKTK::HaveMouseCapture()
{
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
** Paint an Area of the client into the Scintilla object's Bitmap.  Refer to pan_surface.cxx for the drawing routines.
** If Scintilla abandons the paint because a wider area needs to be redrawn (for instance, styling has spilled over a
** line end) then the entire client is repainted immediately, as the GTK platform does.
*/

void ScintillaKTK::panPaint(Scintilla::PRectangle Area)
{
   kt::Log log(__FUNCTION__);

   if (this->paintState != notPainting) return; // Recursion prevention
   if (not scintilla->Bitmap) return;

   auto rcClient = GetClientRectangle();

   log.traceBranch("Area: %dx%d - %dx%d", Area.left, Area.top, Area.Width(), Area.Height());

   std::unique_ptr<Scintilla::Surface> surface(Scintilla::Surface::Allocate());
   if (not surface) return;
   surface->Init(scintilla->Bitmap, wMain.GetID());

   this->paintState = painting;
   this->rcPaint    = Area;
   this->paintingAllText = rcPaint.Contains(rcClient);
   this->Paint(surface.get(), rcPaint);

   if (this->paintState IS paintAbandoned) {
      this->paintState = painting;
      this->rcPaint    = rcClient;
      this->paintingAllText = true;
      this->Paint(surface.get(), rcPaint);
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

/*********************************************************************************************************************
** Process a key press.  Command keys are translated to Scintilla key codes and passed through the key map, while
** printable characters are inserted as text.  Letters are always passed to the key map in upper case, because that is
** how its shortcuts are defined (e.g. Ctrl+Z).
**
** Ctrl and Alt combinations are treated as shortcuts, with the exception of AltGr.  AltGr is reported as R_ALT on X11
** layouts that map it to Alt_R, and with an additional Ctrl on Windows.  A printable character that is produced with
** AltGr is text.  On X11 layouts that map AltGr to ISO_Level3_Shift, no qualifier is reported for it.
**
** If SCIF::EDIT is not set, the document is read-only for the duration of the key press.  Navigation and selection
** are unaffected, while edits are refused and reported as SEF::FAIL_RO.  The document is otherwise left writable so
** that it can always be modified through the API.
*/

void ScintillaKTK::panKeyDown(KEY Code, KQ Qualifiers, int Unicode)
{
   const bool shift = (Qualifiers & KQ::SHIFT) != KQ::NIL;
   const bool ctrl  = (Qualifiers & KQ::CTRL) != KQ::NIL;
   const bool alt   = (Qualifiers & KQ::ALT) != KQ::NIL;
   const bool altgr = ((Qualifiers & KQ::R_ALT) != KQ::NIL) and ((Qualifiers & KQ::L_ALT) IS KQ::NIL);
   const bool printable = ((Qualifiers & KQ::NOT_PRINTABLE) IS KQ::NIL) and (Unicode >= 0x20) and (Unicode != 0x7f);

   const bool read_only = (scintilla->Flags & SCIF::EDIT) IS SCIF::NIL;
   if (read_only) SendScintilla(SCI_SETREADONLY, 1UL);

   if (int key = command_key(Code, printable)) KeyDown(key, shift, ctrl, alt);
   else if ((printable) and ((altgr) or ((not ctrl) and (not alt)))) {
      char utf8[8];
      int len = UTF8WriteValue(Unicode, std::span<int8_t>((int8_t *)utf8, sizeof(utf8)));
      if (len > 0) AddCharUTF(utf8, len);
   }
   else if (int key = shortcut_key(Code)) KeyDown(key, shift, ctrl, alt);

   if (read_only) SendScintilla(SCI_SETREADONLY, 0UL);
}

/*********************************************************************************************************************
** Process a mouse button press at a client position.  The Time is in milliseconds and allows Scintilla to detect
** double and triple clicks.  Modifier keys are taken from the state that key_event() tracks: Shift extends the
** selection, and Alt starts a rectangular selection.
**
** Scintilla's internal text drag is not supported yet, so a click inside the selection clears it first.  This turns
** the click into an ordinary click, and a drag from that point into a new selection.  Shift clicks are excluded so
** that they extend the selection from its anchor.
*/

void ScintillaKTK::panMousePress(JET Button, double X, double Y, unsigned int Time)
{
   kt::Log log(__FUNCTION__);
   log.traceBranch("%.0fx%.0f", X, Y);

   if (Button IS JET::LMB) {
      Scintilla::Point point(int(std::floor(X)), int(std::floor(Y)));

      if ((not scintilla->KeyShift) and (not SelectionEmpty()) and (PointInSelection(point))) {
         SetEmptySelection(CurrentPosition()); // TODO Phase 7: Text drag and drop
      }

      ButtonDown(point, Time, scintilla->KeyShift, scintilla->KeyCtrl, scintilla->KeyAlt);
   }
   // TODO Phase 7: The RMB will open a context menu.
}

//********************************************************************************************************************

void ScintillaKTK::panMouseMove(double X, double Y)
{
   ButtonMove(Scintilla::Point(int(std::floor(X)), int(std::floor(Y))));
}

//********************************************************************************************************************

void ScintillaKTK::panMouseRelease(JET Button, double X, double Y, unsigned int Time)
{
   kt::Log log(__FUNCTION__);
   log.trace("%.0fx%.0f", X, Y);

   if (Button IS JET::LMB) {
      ButtonUp(Scintilla::Point(int(std::floor(X)), int(std::floor(Y))), Time, scintilla->KeyCtrl);
   }
}

//********************************************************************************************************************
// Abandon a mouse drag, e.g. when the editor is disabled while a button is held.  The selection is left as it is.

void ScintillaKTK::panReleaseMouse()
{
   if (HaveMouseCapture()) SetMouseCapture(false);
}

//********************************************************************************************************************

void ScintillaKTK::panResized()
{
   kt::Log log(__FUNCTION__);
   log.traceBranch();
   ChangeSize();
}

/*********************************************************************************************************************
** Scroll to a point in the document, expressed in pixels from the top-left corner.  Scintilla scrolls vertically by
** whole lines, so Y is rounded to the nearest line.  Both axes are clamped to the extent of the Page.  The Page is
** then snapped to the resulting position, which also corrects a Page that was moved to an unreachable position.
*/

void ScintillaKTK::panScrollTo(double X, double Y)
{
   kt::Log log(__FUNCTION__);
   log.traceBranch("%.2f, %.2f", X, Y);

   if (vs.lineHeight > 0) {
      int max_x = std::max(0, scintilla->PageWidth - scintilla->ViewWidth);

      scintilla->ScrollLocked++;
      ScrollTo(int(std::lround(Y / vs.lineHeight)));
      HorizontalScrollTo(std::clamp(int(std::lround(X)), 0, max_x));
      scintilla->ScrollLocked--;
   }

   panSyncPage();
}

//********************************************************************************************************************
// Scroll relative to the current position by a number of columns (measured in space widths) and lines.

void ScintillaKTK::panScrollBy(int Columns, int Lines)
{
   panScrollTo(xOffset + (Columns * vs.spaceWidth), (topLine + Lines) * vs.lineHeight);
}

//********************************************************************************************************************
// Move the Page and Canvas to reflect Scintilla's current scroll position.

void ScintillaKTK::panSyncPage()
{
   scintilla->ScrollX = xOffset;
   scintilla->ScrollY = topLine * vs.lineHeight;
   sync_page(scintilla);
}

//********************************************************************************************************************
// Showing the caret on receipt of the focus starts the tick timer.  On losing the focus, the timer ends itself on its
// next call.

void ScintillaKTK::panSetFocus(bool Focus)
{
   if (Focus != hasFocus) SetFocusState(Focus);
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
