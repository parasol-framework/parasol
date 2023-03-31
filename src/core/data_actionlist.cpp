
#include <parasol/main.h>

#define FDEF static const struct FunctionField

FDEF argsClipboard[]     = { { "Mode", FD_LONG }, { 0, 0 } };
FDEF argsCopyData[]      = { { "Dest", FD_OBJECTPTR  }, { 0, 0 } };
FDEF argsCustom[]        = { { "Number", FD_LONG }, { "String", FD_STR }, { 0, 0 } };
FDEF argsDataFeed[]      = { { "Object", FD_OBJECTPTR }, { "Datatype", FD_LONG }, { "Buffer", FD_PTR }, { "Size", FD_LONG|FD_PTRSIZE }, { 0, 0 } };
FDEF argsDragDrop[]      = { { "Source", FD_OBJECTPTR }, { "Item", FD_LONG }, { "Datatype", FD_STR }, { 0, 0 } };
FDEF argsDraw[]          = { { "X", FD_LONG }, { "Y", FD_LONG }, { "Width", FD_LONG }, { "Height", FD_LONG }, { 0, 0 } };
FDEF argsGetVar[]        = { { "Field", FD_STR }, { "Buffer",  FD_PTRBUFFER }, { "Size", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF argsMove[]          = { { "DeltaX", FD_DOUBLE }, { "DeltaY", FD_DOUBLE }, { "DeltaZ", FD_DOUBLE }, { 0, 0 } };
FDEF argsMoveToPoint[]   = { { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { "Z", FD_DOUBLE }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF argsNewChild[]      = { { "NewChild", FD_OBJECTPTR }, { 0, 0 } };
FDEF argsNewOwner[]      = { { "NewOwner", FD_OBJECTID }, { "Class", FD_LONG }, { 0, 0 } };
FDEF argsRead[]          = { { "Buffer", FD_PTRBUFFER }, { "Length", FD_LONG|FD_BUFSIZE }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsRedimension[]   = { { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { "Z", FD_DOUBLE }, { "Width", FD_DOUBLE }, { "Height", FD_DOUBLE }, { "Depth", FD_DOUBLE }, { 0, 0 } };
FDEF argsRedo[]          = { { "Steps", FD_LONG }, { 0, 0 } };
FDEF argsRename[]        = { { "Name", FD_STR }, { 0, 0 } };
FDEF argsResize[]        = { { "Width", FD_DOUBLE }, { "Height", FD_DOUBLE }, { "Depth", FD_DOUBLE }, { 0, 0 } };
FDEF argsSaveImage[]     = { { "Dest", FD_OBJECTPTR }, { "Class", FD_LONG }, { 0, 0 } };
FDEF argsSaveToObject[]  = { { "Dest", FD_OBJECTPTR }, { "Class", FD_LONG }, { 0, 0 } };
FDEF argsScroll[]        = { { "DeltaX", FD_DOUBLE }, { "DeltaY", FD_DOUBLE }, { "DeltaZ", FD_DOUBLE }, { 0, 0 } };
FDEF argsScrollToPoint[] = { { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { "Z", FD_DOUBLE }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF argsSeek[]          = { { "Offset", FD_DOUBLE }, { "Position", FD_LONG }, { 0, 0 } };
FDEF argsSetVar[]        = { { "Field", FD_STR }, { "Value", FD_STR }, { 0, 0 } };
FDEF argsUndo[]          = { { "Steps", FD_LONG }, { 0, 0 } };
FDEF argsWrite[]         = { { "Buffer", FD_PTR|FD_BUFFER }, { "Length", FD_LONG|FD_BUFSIZE }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsSelectArea[]    = { { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { "Width", FD_DOUBLE }, { "Height", FD_DOUBLE }, { 0, 0 } };

extern "C" const struct ActionTable ActionTable[] = { // Sorted by action ID.
   { 0, 0, 0, 0 },
   { AHASH_SIGNAL,         0, "Signal", 0 },
   { AHASH_ACTIVATE,       0, "Activate", 0 },
   { AHASH_SELECTAREA,     sizeof(struct acSelectArea), "SelectArea", argsSelectArea },
   { AHASH_CLEAR,          0, "Clear", 0 },
   { AHASH_FREEWARNING,    0, "FreeWarning", 0 },
   { AHASH_SORT,           0, "Sort", 0 },
   { AHASH_COPYDATA,       sizeof(struct acCopyData), "CopyData", argsCopyData },
   { AHASH_DATAFEED,       sizeof(struct acDataFeed), "DataFeed", argsDataFeed },
   { AHASH_DEACTIVATE,     0, "Deactivate", 0 },
   { AHASH_DRAW,           sizeof(struct acDraw), "Draw", argsDraw },
   { AHASH_FLUSH,          0, "Flush", 0 },
   { AHASH_FOCUS,          0, "Focus", 0 },
   { AHASH_FREE,           0, "Free", 0 },
   { AHASH_SAVESETTINGS,   0, "SaveSettings", 0 },
   { AHASH_GETVAR,         sizeof(struct acGetVar), "GetVar", argsGetVar },
   { AHASH_DRAGDROP,       sizeof(struct acDragDrop), "DragDrop", argsDragDrop },
   { AHASH_HIDE,           0, "Hide", 0 },
   { AHASH_INIT,           0, "Init", 0 },
   { AHASH_LOCK,           0, "Lock", 0 },
   { AHASH_LOSTFOCUS,      0, "LostFocus", 0 },
   { AHASH_MOVE,           sizeof(struct acMove), "Move", argsMove },
   { AHASH_MOVETOBACK,     0, "MoveToBack", 0 },
   { AHASH_MOVETOFRONT,    0, "MoveToFront", 0 },
   { AHASH_NEWCHILD,       sizeof(struct acNewChild), "NewChild", argsNewChild },
   { AHASH_NEWOWNER,       sizeof(struct acNewOwner), "NewOwner", argsNewOwner },
   { AHASH_NEWOBJECT,      0, "NewObject", 0 },
   { AHASH_REDO,           sizeof(struct acRedo), "Redo", argsRedo },
   { AHASH_QUERY,          0, "Query", 0 },
   { AHASH_READ,           sizeof(struct acRead), "Read", argsRead },
   { AHASH_RENAME,         sizeof(struct acRename), "Rename", argsRename },
   { AHASH_RESET,          0, "Reset", 0 },
   { AHASH_RESIZE,         sizeof(struct acResize), "Resize", argsResize },
   { AHASH_SAVEIMAGE,      sizeof(struct acSaveImage), "SaveImage", argsSaveImage },
   { AHASH_SAVETOOBJECT,   sizeof(struct acSaveToObject), "SaveToObject", argsSaveToObject },
   { AHASH_SCROLL,         sizeof(struct acScroll), "Scroll", argsScroll },
   { AHASH_SEEK,           sizeof(struct acSeek), "Seek", argsSeek },
   { AHASH_SETVAR,         sizeof(struct acSetVar), "SetVar", argsSetVar },
   { AHASH_SHOW,           0, "Show", 0 },
   { AHASH_UNDO,           sizeof(struct acUndo), "Undo", argsUndo },
   { AHASH_UNLOCK,         0, "Unlock", 0 },
   { AHASH_NEXT,           0, "Next", 0 },
   { AHASH_PREV,           0, "Prev", 0 },
   { AHASH_WRITE,          sizeof(struct acWrite), "Write", argsWrite },
   { AHASH_SETFIELD,       0, "SetField", 0 }, // Used for logging SetField() calls
   { AHASH_CLIPBOARD,      sizeof(struct acClipboard), "Clipboard", argsClipboard },
   { AHASH_REFRESH,        0, "Refresh", 0 },
   { AHASH_DISABLE,        0, "Disable", 0 },
   { AHASH_ENABLE,         0, "Enable", 0 },
   { AHASH_REDIMENSION,    sizeof(struct acRedimension), "Redimension", argsRedimension },
   { AHASH_MOVETOPOINT,    sizeof(struct acMoveToPoint), "MoveToPoint", argsMoveToPoint },
   { AHASH_SCROLLTOPOINT,  sizeof(struct acScrollToPoint), "ScrollToPoint", argsScrollToPoint },
   { AHASH_CUSTOM,         sizeof(struct acCustom), "Custom", argsCustom },
   { 0, 0, 0, 0 }
};
