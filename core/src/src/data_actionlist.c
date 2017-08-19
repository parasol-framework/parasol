
#include <parasol/main.h>

#define FDEF static const struct FunctionField

FDEF argsActionNotify[]  = { { "Action", FD_LONG }, { "Object", FD_OBJECTID }, { "Args", FD_PTR }, { "Size", FD_LONG|FD_PTRSIZE }, { "Error", FD_LONG }, { "Time", FD_LONG }, { 0, 0 } };
FDEF argsClipboard[]     = { { "Mode", FD_LONG }, { 0, 0 } };
FDEF argsCopyData[]      = { { "Dest", FD_OBJECTID  }, { 0, 0 } };
FDEF argsCustom[]        = { { "Number", FD_LONG }, { "String", FD_STR }, { 0, 0 } };
FDEF argsDataFeed[]      = { { "Object", FD_OBJECTID }, { "Datatype", FD_LONG }, { "Buffer", FD_PTR }, { "Size", FD_LONG|FD_PTRSIZE }, { 0, 0 } };
FDEF argsDragDrop[]      = { { "Source", FD_OBJECTID }, { "Item", FD_LONG }, { "Datatype", FD_STR }, { 0, 0 } };
FDEF argsDraw[]          = { { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { 0, 0 } };
FDEF argsGetVar[]        = { { "Field", FD_STR }, { "Buffer",  FD_PTRBUFFER }, { "Size", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF argsMove[]          = { { "XChange", FD_DOUBLE }, { "YChange", FD_DOUBLE }, { "ZChange", FD_DOUBLE }, { 0, 0 } };
FDEF argsMoveToPoint[]   = { { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { "Z", FD_DOUBLE }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF argsNewChild[]      = { { "NewChild", FD_OBJECTID }, { 0, 0 } };
FDEF argsNewOwner[]      = { { "NewOwner", FD_OBJECTID }, { "Class", FD_LONG }, { 0, 0 } };
FDEF argsRead[]          = { { "Buffer", FD_PTRBUFFER }, { "Length", FD_LONG|FD_BUFSIZE }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsRedimension[]   = { { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { "Z", FD_DOUBLE }, { "Width", FD_DOUBLE }, { "Height", FD_DOUBLE }, { "Depth", FD_DOUBLE }, { 0, 0 } };
FDEF argsRedo[]          = { { "Steps", FD_LONG }, { 0, 0 } };
FDEF argsRename[]        = { { "Name", FD_STR }, { 0, 0 } };
FDEF argsResize[]        = { { "Width", FD_DOUBLE }, { "Height", FD_DOUBLE }, { "Depth", FD_DOUBLE }, { 0, 0 } };
FDEF argsSaveImage[]     = { { "Dest", FD_OBJECTID }, { "Class", FD_LONG }, { 0, 0 } };
FDEF argsSaveToObject[]  = { { "Dest", FD_OBJECTID }, { "Class", FD_LONG }, { 0, 0 } };
FDEF argsScroll[]        = { { "XChange", FD_DOUBLE }, { "YChange", FD_DOUBLE }, { "ZChange", FD_DOUBLE }, { 0, 0 } };
FDEF argsScrollToPoint[] = { { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { "Z", FD_DOUBLE }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF argsSeek[]          = { { "Offset", FD_DOUBLE }, { "Position", FD_LONG }, { 0, 0 } };
FDEF argsSetVar[]        = { { "Field", FD_STR }, { "Value", FD_STR }, { 0, 0 } };
FDEF argsUndo[]          = { { "Steps", FD_LONG }, { 0, 0 } };
FDEF argsWrite[]         = { { "Buffer", FD_PTR|FD_BUFFER }, { "Length", FD_LONG|FD_BUFSIZE }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsSelectArea[]    = { { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { "Width", FD_DOUBLE }, { "Height", FD_DOUBLE }, { 0, 0 } };

const struct ActionTable ActionTable[] = { // Sorted by action ID.
   { 0, 0, 0, 0 },
   { AHASH_ACTIONNOTIFY,    "ActionNotify",       argsActionNotify, sizeof(struct acActionNotify) },
   { AHASH_ACTIVATE,        "Activate",           0, 0 },
   { AHASH_ACCESSOBJECT,    "AccessObject",       0, 0 },
   { AHASH_CLEAR,           "Clear",              0, 0 },
   { AHASH_FREEWARNING,     "FreeWarning",        0, 0 },
   { AHASH_OWNERDESTROYED,  "OwnerDestroyed",         0, 0 },
   { AHASH_COPYDATA,        "CopyData",           argsCopyData, sizeof(struct acCopyData) },
   { AHASH_DATAFEED,        "DataFeed",           argsDataFeed, sizeof(struct acDataFeed) },
   { AHASH_DEACTIVATE,      "Deactivate",         0, 0 },
   { AHASH_DRAW,            "Draw",               argsDraw, sizeof(struct acDraw) },
   { AHASH_FLUSH,           "Flush",              0, 0 },
   { AHASH_FOCUS,           "Focus",              0, 0 },
   { AHASH_FREE,            "Free",               0, 0 },
   { AHASH_RELEASEOBJECT,   "ReleaseObject",      0, 0 },
   { AHASH_GETVAR,          "GetVar",             argsGetVar, sizeof(struct acGetVar) },
   { AHASH_DRAGDROP,        "DragDrop",           argsDragDrop, sizeof(struct acDragDrop) },
   { AHASH_HIDE,            "Hide",               0, 0 },
   { AHASH_INIT,            "Init",               0, 0 },
   { AHASH_LOCK,            "Lock",               0, 0 },
   { AHASH_LOSTFOCUS,       "LostFocus",          0, 0 },
   { AHASH_MOVE,            "Move",               argsMove, sizeof(struct acMove) },
   { AHASH_MOVETOBACK,      "MoveToBack",         0, 0 },
   { AHASH_MOVETOFRONT,     "MoveToFront",        0, 0 },
   { AHASH_NEWCHILD,        "NewChild",           argsNewChild, sizeof(struct acNewChild) },
   { AHASH_NEWOWNER,        "NewOwner",           argsNewOwner, sizeof(struct acNewOwner) },
   { AHASH_NEWOBJECT,       "NewObject",          0, 0 },
   { AHASH_REDO,            "Redo",               argsRedo, sizeof(struct acRedo) },
   { AHASH_QUERY,           "Query",              0, 0 },
   { AHASH_READ,            "Read",               argsRead, sizeof(struct acRead) },
   { AHASH_RENAME,          "Rename",             argsRename, sizeof(struct acRename) },
   { AHASH_RESET,           "Reset",              0, 0 },
   { AHASH_RESIZE,          "Resize",             argsResize, sizeof(struct acResize) },
   { AHASH_SAVEIMAGE,       "SaveImage",          argsSaveImage, sizeof(struct acSaveImage) },
   { AHASH_SAVETOOBJECT,    "SaveToObject",       argsSaveToObject, sizeof(struct acSaveToObject) },
   { AHASH_SCROLL,          "Scroll",             argsScroll, sizeof(struct acScroll) },
   { AHASH_SEEK,            "Seek",               argsSeek, sizeof(struct acSeek) },
   { AHASH_SETVAR,          "SetVar",             argsSetVar, sizeof(struct acSetVar) },
   { AHASH_SHOW,            "Show",               0, 0 },
   { AHASH_UNDO,            "Undo",               argsUndo, sizeof(struct acUndo) },
   { AHASH_UNLOCK,          "Unlock",             0, 0 },
   { AHASH_NEXT,            "Next",               0, 0 },
   { AHASH_PREV,            "Prev",               0, 0 },
   { AHASH_WRITE,           "Write",              argsWrite, sizeof(struct acWrite) },
   { AHASH_SETFIELD,        "SetField",           0, 0 }, // Used for logging SetField() calls
   { AHASH_CLIPBOARD,       "Clipboard",          argsClipboard, sizeof(struct acClipboard) },
   { AHASH_REFRESH,         "Refresh",            0, 0 },
   { AHASH_DISABLE,         "Disable",            0, 0 },
   { AHASH_ENABLE,          "Enable",             0, 0 },
   { AHASH_REDIMENSION,     "Redimension",        argsRedimension, sizeof(struct acRedimension) },
   { AHASH_MOVETOPOINT,     "MoveToPoint",        argsMoveToPoint, sizeof(struct acMoveToPoint) },
   { AHASH_SCROLLTOPOINT,   "ScrollToPoint",      argsScrollToPoint, sizeof(struct acScrollToPoint) },
   { AHASH_CUSTOM,          "Custom",             argsCustom, sizeof(struct acCustom) },
   { AHASH_SORT,            "Sort",               0, 0 },
   { AHASH_SAVESETTINGS,    "SaveSettings",       0, 0 },
   { AHASH_SELECTAREA,      "SelectArea",         argsSelectArea, sizeof(struct acSelectArea) },
   { 0, 0, 0, 0 }
};
