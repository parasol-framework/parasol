#pragma once

// Name:      core.h
// Copyright: Paul Manias 1996-2024
// Generator: idl-c

#include <parasol/main.h>

#include <stdarg.h>
#include <inttypes.h>

#ifdef __cplusplus
#include <list>
#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include <bit>
#include <atomic>
#include <array>
#endif

#if defined(_DEBUG)
 #ifndef _MSC_VER
  #include <signal.h>
 #endif
#endif

#ifndef DEFINE_ENUM_FLAG_OPERATORS
template <size_t S> struct _ENUM_FLAG_INTEGER_FOR_SIZE;
template <> struct _ENUM_FLAG_INTEGER_FOR_SIZE<1> { typedef BYTE type; };
template <> struct _ENUM_FLAG_INTEGER_FOR_SIZE<2> { typedef WORD type; };
template <> struct _ENUM_FLAG_INTEGER_FOR_SIZE<4> { typedef LONG type; };
template <> struct _ENUM_FLAG_INTEGER_FOR_SIZE<8> { typedef LARGE type; };
// used as an approximation of std::underlying_type<T>
template <class T> struct _ENUM_FLAG_SIZED_INTEGER { typedef typename _ENUM_FLAG_INTEGER_FOR_SIZE<sizeof(T)>::type type; };

#define DEFINE_ENUM_FLAG_OPERATORS(ENUMTYPE) \
inline ENUMTYPE operator | (ENUMTYPE a, ENUMTYPE b) { return ENUMTYPE(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a) | ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \
inline ENUMTYPE operator & (ENUMTYPE a, ENUMTYPE b) { return ENUMTYPE(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a) & ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \
inline ENUMTYPE operator ~ (ENUMTYPE a) { return ENUMTYPE(~((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a)); } \
inline ENUMTYPE operator ^ (ENUMTYPE a, ENUMTYPE b) { return ENUMTYPE(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a) ^ ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \
inline ENUMTYPE &operator |= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type &)a) |= ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \
inline ENUMTYPE &operator &= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type &)a) &= ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); }
#endif
class objMetaClass;
class objStorageDevice;
class objFile;
class objConfig;
class objScript;
class objTask;
class objThread;
class objModule;
class objTime;
class objCompression;
class objCompressedStream;

#define NETMSG_START 0
#define NETMSG_END 1

// Clipboard modes

enum class CLIPMODE : ULONG {
   NIL = 0,
   CUT = 0x00000001,
   COPY = 0x00000002,
   PASTE = 0x00000004,
};

DEFINE_ENUM_FLAG_OPERATORS(CLIPMODE)

// Seek positions

enum class SEEK : LONG {
   NIL = 0,
   START = 0,
   CURRENT = 1,
   END = 2,
   RELATIVE = 3,
};

enum class DEVICE : LARGE {
   NIL = 0,
   COMPACT_DISC = 0x00000001,
   HARD_DISK = 0x00000002,
   FLOPPY_DISK = 0x00000004,
   READ = 0x00000008,
   WRITE = 0x00000010,
   REMOVABLE = 0x00000020,
   REMOVEABLE = 0x00000020,
   SOFTWARE = 0x00000040,
   NETWORK = 0x00000080,
   TAPE = 0x00000100,
   PRINTER = 0x00000200,
   SCANNER = 0x00000400,
   TEMPORARY = 0x00000800,
   MEMORY = 0x00001000,
   MODEM = 0x00002000,
   USB = 0x00004000,
   FIXED = 0x00008000,
   PRINTER_3D = 0x00010000,
   SCANNER_3D = 0x00020000,
   BOOKMARK = 0x00040000,
};

DEFINE_ENUM_FLAG_OPERATORS(DEVICE)

// Class categories

enum class CCF : ULONG {
   NIL = 0,
   COMMAND = 0x00000001,
   FILESYSTEM = 0x00000002,
   GRAPHICS = 0x00000004,
   GUI = 0x00000008,
   IO = 0x00000010,
   SYSTEM = 0x00000020,
   TOOL = 0x00000040,
   AUDIO = 0x00000080,
   DATA = 0x00000100,
   MISC = 0x00000200,
   NETWORK = 0x00000400,
   MULTIMEDIA = 0x00000800,
};

DEFINE_ENUM_FLAG_OPERATORS(CCF)

// Action identifiers.

enum class AC : LONG {
   NIL = 0,
   Signal = 1,
   Activate = 2,
   Redimension = 3,
   Clear = 4,
   FreeWarning = 5,
   Enable = 6,
   CopyData = 7,
   DataFeed = 8,
   Deactivate = 9,
   Draw = 10,
   Flush = 11,
   Focus = 12,
   Free = 13,
   SaveSettings = 14,
   GetKey = 15,
   DragDrop = 16,
   Hide = 17,
   Init = 18,
   Lock = 19,
   LostFocus = 20,
   Move = 21,
   MoveToBack = 22,
   MoveToFront = 23,
   NewChild = 24,
   NewOwner = 25,
   NewObject = 26,
   Redo = 27,
   Query = 28,
   Read = 29,
   Rename = 30,
   Reset = 31,
   Resize = 32,
   SaveImage = 33,
   SaveToObject = 34,
   MoveToPoint = 35,
   Seek = 36,
   SetKey = 37,
   Show = 38,
   Undo = 39,
   Unlock = 40,
   Next = 41,
   Prev = 42,
   Write = 43,
   SetField = 44,
   Clipboard = 45,
   Refresh = 46,
   Disable = 47,
   NewPlacement = 48,
   END = 49,
};

// Permission flags

enum class PERMIT : ULONG {
   NIL = 0,
   READ = 0x00000001,
   USER_READ = 0x00000001,
   WRITE = 0x00000002,
   USER_WRITE = 0x00000002,
   EXEC = 0x00000004,
   USER_EXEC = 0x00000004,
   DELETE = 0x00000008,
   USER = 0x0000000f,
   GROUP_READ = 0x00000010,
   GROUP_WRITE = 0x00000020,
   GROUP_EXEC = 0x00000040,
   GROUP_DELETE = 0x00000080,
   GROUP = 0x000000f0,
   OTHERS_READ = 0x00000100,
   EVERYONE_READ = 0x00000111,
   ALL_READ = 0x00000111,
   OTHERS_WRITE = 0x00000200,
   ALL_WRITE = 0x00000222,
   EVERYONE_WRITE = 0x00000222,
   EVERYONE_READWRITE = 0x00000333,
   OTHERS_EXEC = 0x00000400,
   ALL_EXEC = 0x00000444,
   EVERYONE_EXEC = 0x00000444,
   OTHERS_DELETE = 0x00000800,
   EVERYONE_DELETE = 0x00000888,
   ALL_DELETE = 0x00000888,
   OTHERS = 0x00000f00,
   EVERYONE_ACCESS = 0x00000fff,
   HIDDEN = 0x00001000,
   ARCHIVE = 0x00002000,
   PASSWORD = 0x00004000,
   USERID = 0x00008000,
   GROUPID = 0x00010000,
   INHERIT = 0x00020000,
   OFFLINE = 0x00040000,
   NETWORK = 0x00080000,
};

DEFINE_ENUM_FLAG_OPERATORS(PERMIT)

// Special qualifier flags

enum class KQ : ULONG {
   NIL = 0,
   L_SHIFT = 0x00000001,
   R_SHIFT = 0x00000002,
   SHIFT = 0x00000003,
   CAPS_LOCK = 0x00000004,
   L_CONTROL = 0x00000008,
   L_CTRL = 0x00000008,
   R_CTRL = 0x00000010,
   R_CONTROL = 0x00000010,
   CTRL = 0x00000018,
   CONTROL = 0x00000018,
   L_ALT = 0x00000020,
   ALTGR = 0x00000040,
   R_ALT = 0x00000040,
   ALT = 0x00000060,
   INSTRUCTION_KEYS = 0x00000078,
   L_COMMAND = 0x00000080,
   R_COMMAND = 0x00000100,
   COMMAND = 0x00000180,
   QUALIFIERS = 0x000001fb,
   NUM_PAD = 0x00000200,
   REPEAT = 0x00000400,
   RELEASED = 0x00000800,
   PRESSED = 0x00001000,
   NOT_PRINTABLE = 0x00002000,
   INFO = 0x00003c04,
   SCR_LOCK = 0x00004000,
   NUM_LOCK = 0x00008000,
   DEAD_KEY = 0x00010000,
   WIN_CONTROL = 0x00020000,
};

DEFINE_ENUM_FLAG_OPERATORS(KQ)

// Memory types used by AllocMemory().  The lower 16 bits are stored with allocated blocks, the upper 16 bits are function-relative only.

enum class MEM : ULONG {
   NIL = 0,
   DATA = 0x00000000,
   MANAGED = 0x00000001,
   VIDEO = 0x00000002,
   TEXTURE = 0x00000004,
   AUDIO = 0x00000008,
   CODE = 0x00000010,
   NO_POOL = 0x00000020,
   TMP_LOCK = 0x00000040,
   UNTRACKED = 0x00000080,
   STRING = 0x00000100,
   OBJECT = 0x00000200,
   NO_LOCK = 0x00000400,
   EXCLUSIVE = 0x00000800,
   COLLECT = 0x00001000,
   NO_BLOCKING = 0x00002000,
   NO_BLOCK = 0x00002000,
   READ = 0x00010000,
   WRITE = 0x00020000,
   READ_WRITE = 0x00030000,
   NO_CLEAR = 0x00040000,
   HIDDEN = 0x00100000,
   CALLER = 0x00800000,
};

DEFINE_ENUM_FLAG_OPERATORS(MEM)

// Event categories.

enum class EVG : LONG {
   NIL = 0,
   FILESYSTEM = 1,
   NETWORK = 2,
   SYSTEM = 3,
   GUI = 4,
   DISPLAY = 5,
   IO = 6,
   HARDWARE = 7,
   AUDIO = 8,
   USER = 9,
   POWER = 10,
   CLASS = 11,
   APP = 12,
   ANDROID = 13,
   END = 14,
};

// Data codes

enum class DATA : LONG {
   NIL = 0,
   TEXT = 1,
   RAW = 2,
   DEVICE_INPUT = 3,
   XML = 4,
   AUDIO = 5,
   RECORD = 6,
   IMAGE = 7,
   REQUEST = 8,
   RECEIPT = 9,
   FILE = 10,
   CONTENT = 11,
   INPUT_READY = 12,
};

// JTYPE flags are used to categorise input types.

enum class JTYPE : ULONG {
   NIL = 0,
   SECONDARY = 0x00000001,
   ANCHORED = 0x00000002,
   DRAGGED = 0x00000004,
   CROSSING = 0x00000008,
   DIGITAL = 0x00000010,
   ANALOG = 0x00000020,
   EXT_MOVEMENT = 0x00000040,
   BUTTON = 0x00000080,
   MOVEMENT = 0x00000100,
   DBL_CLICK = 0x00000200,
   REPEATED = 0x00000400,
   DRAG_ITEM = 0x00000800,
};

DEFINE_ENUM_FLAG_OPERATORS(JTYPE)

// Gamepad controller buttons.

enum class CON : ULONG {
   NIL = 0,
   GAMEPAD_S = 0x00000001,
   GAMEPAD_E = 0x00000002,
   GAMEPAD_W = 0x00000004,
   GAMEPAD_N = 0x00000008,
   DPAD_UP = 0x00000010,
   DPAD_DOWN = 0x00000020,
   DPAD_LEFT = 0x00000040,
   DPAD_RIGHT = 0x00000080,
   START = 0x00000100,
   SELECT = 0x00000200,
   LEFT_BUMPER_1 = 0x00000400,
   LEFT_BUMPER_2 = 0x00000800,
   RIGHT_BUMPER_1 = 0x00001000,
   RIGHT_BUMPER_2 = 0x00002000,
   LEFT_THUMB = 0x00004000,
   RIGHT_THUMB = 0x00008000,
};

DEFINE_ENUM_FLAG_OPERATORS(CON)

// JET constants are documented in GetInputEvent()

enum class JET : LONG {
   NIL = 0,
   BUTTON_1 = 1,
   LMB = 1,
   BUTTON_2 = 2,
   RMB = 2,
   BUTTON_3 = 3,
   MMB = 3,
   BUTTON_4 = 4,
   BUTTON_5 = 5,
   BUTTON_6 = 6,
   BUTTON_7 = 7,
   BUTTON_8 = 8,
   BUTTON_9 = 9,
   BUTTON_10 = 10,
   WHEEL = 11,
   WHEEL_TILT = 12,
   PEN_TILT_XY = 13,
   ABS_XY = 14,
   CROSSED_IN = 15,
   CROSSED_OUT = 16,
   PRESSURE = 17,
   DEVICE_TILT_XY = 18,
   DEVICE_TILT_Z = 19,
   DISPLAY_EDGE = 20,
   END = 21,
};

// Field descriptors.

#define FD_DOUBLERESULT 0x80000100
#define FD_PTR_DOUBLERESULT 0x88000100
#define FD_VOLATILE 0x00000000
#define FD_VOID 0x00000000
#define FD_OBJECT 0x00000001
#define FD_LOCAL 0x00000002
#define FD_VIRTUAL 0x00000008
#define FD_STRUCT 0x00000010
#define FD_ALLOC 0x00000020
#define FD_FLAGS 0x00000040
#define FD_VARTAGS 0x00000040
#define FD_LOOKUP 0x00000080
#define FD_PTRSIZE 0x00000080
#define FD_BUFSIZE 0x00000080
#define FD_ARRAYSIZE 0x00000080
#define FD_R 0x00000100
#define FD_READ 0x00000100
#define FD_RESULT 0x00000100
#define FD_BUFFER 0x00000200
#define FD_W 0x00000200
#define FD_WRITE 0x00000200
#define FD_RW 0x00000300
#define FD_INIT 0x00000400
#define FD_I 0x00000400
#define FD_TAGS 0x00000400
#define FD_RI 0x00000500
#define FD_ERROR 0x00000800
#define FD_ARRAY 0x00001000
#define FD_RESOURCE 0x00002000
#define FD_CPP 0x00004000
#define FD_CUSTOM 0x00008000
#define FD_SYSTEM 0x00010000
#define FD_PRIVATE 0x00010000
#define FD_SYNONYM 0x00020000
#define FD_UNSIGNED 0x00040000
#define FD_RGB 0x00080000
#define FD_SCALED 0x00200000
#define FD_WORD 0x00400000
#define FD_STRING 0x00800000
#define FD_STR 0x00800000
#define FD_STRRESULT 0x00800100
#define FD_BYTE 0x01000000
#define FD_FUNCTION 0x02000000
#define FD_LARGE 0x04000000
#define FD_LARGERESULT 0x04000100
#define FD_POINTER 0x08000000
#define FD_PTR 0x08000000
#define FD_OBJECTPTR 0x08000001
#define FD_PTRRESULT 0x08000100
#define FD_PTRBUFFER 0x08000200
#define FD_FUNCTIONPTR 0x0a000000
#define FD_PTR_LARGERESULT 0x0c000100
#define FD_FLOAT 0x10000000
#define FD_UNIT 0x20000000
#define FD_LONG 0x40000000
#define FD_OBJECTID 0x40000001
#define FD_LONGRESULT 0x40000100
#define FD_PTR_LONGRESULT 0x48000100
#define FD_DOUBLE 0x80000000

// Predefined cursor styles

enum class PTC : LONG {
   NIL = 0,
   NO_CHANGE = 0,
   DEFAULT = 1,
   SIZE_BOTTOM_LEFT = 2,
   SIZE_BOTTOM_RIGHT = 3,
   SIZE_TOP_LEFT = 4,
   SIZE_TOP_RIGHT = 5,
   SIZE_LEFT = 6,
   SIZE_RIGHT = 7,
   SIZE_TOP = 8,
   SIZE_BOTTOM = 9,
   CROSSHAIR = 10,
   SLEEP = 11,
   SIZING = 12,
   SPLIT_VERTICAL = 13,
   SPLIT_HORIZONTAL = 14,
   MAGNIFIER = 15,
   HAND = 16,
   HAND_LEFT = 17,
   HAND_RIGHT = 18,
   TEXT = 19,
   PAINTBRUSH = 20,
   STOP = 21,
   INVISIBLE = 22,
   CUSTOM = 23,
   DRAGGABLE = 24,
   END = 25,
};

enum class DMF : ULONG {
   NIL = 0,
   SCALED_X = 0x00000001,
   SCALED_Y = 0x00000002,
   FIXED_X = 0x00000004,
   FIXED_Y = 0x00000008,
   SCALED_X_OFFSET = 0x00000010,
   SCALED_Y_OFFSET = 0x00000020,
   FIXED_X_OFFSET = 0x00000040,
   FIXED_Y_OFFSET = 0x00000080,
   FIXED_HEIGHT = 0x00000100,
   FIXED_WIDTH = 0x00000200,
   SCALED_HEIGHT = 0x00000400,
   SCALED_WIDTH = 0x00000800,
   FIXED_DEPTH = 0x00001000,
   SCALED_DEPTH = 0x00002000,
   FIXED_Z = 0x00004000,
   SCALED_Z = 0x00008000,
   SCALED_RADIUS_X = 0x00010000,
   FIXED_RADIUS_X = 0x00020000,
   SCALED_CENTER_X = 0x00040000,
   SCALED_CENTER_Y = 0x00080000,
   FIXED_CENTER_X = 0x00100000,
   FIXED_CENTER_Y = 0x00200000,
   STATUS_CHANGE_H = 0x00400000,
   STATUS_CHANGE_V = 0x00800000,
   SCALED_RADIUS_Y = 0x01000000,
   FIXED_RADIUS_Y = 0x02000000,
};

DEFINE_ENUM_FLAG_OPERATORS(DMF)

// Compass directions.

enum class DRL : LONG {
   NIL = 0,
   NORTH = 0,
   UP = 0,
   SOUTH = 1,
   DOWN = 1,
   EAST = 2,
   RIGHT = 2,
   WEST = 3,
   LEFT = 3,
   NORTH_EAST = 4,
   NORTH_WEST = 5,
   SOUTH_EAST = 6,
   SOUTH_WEST = 7,
};

// Generic flags for controlling movement.

enum class MOVE : ULONG {
   NIL = 0,
   DOWN = 0x00000001,
   UP = 0x00000002,
   LEFT = 0x00000004,
   RIGHT = 0x00000008,
   ALL = 0x0000000f,
};

DEFINE_ENUM_FLAG_OPERATORS(MOVE)

// Edge flags

enum class EDGE : ULONG {
   NIL = 0,
   TOP = 0x00000001,
   LEFT = 0x00000002,
   RIGHT = 0x00000004,
   BOTTOM = 0x00000008,
   TOP_LEFT = 0x00000010,
   TOP_RIGHT = 0x00000020,
   BOTTOM_LEFT = 0x00000040,
   BOTTOM_RIGHT = 0x00000080,
   ALL = 0x000000ff,
};

DEFINE_ENUM_FLAG_OPERATORS(EDGE)

// Universal values for alignment of graphics and text

enum class ALIGN : ULONG {
   NIL = 0,
   LEFT = 0x00000001,
   RIGHT = 0x00000002,
   HORIZONTAL = 0x00000004,
   VERTICAL = 0x00000008,
   MIDDLE = 0x0000000c,
   CENTER = 0x0000000c,
   TOP = 0x00000010,
   BOTTOM = 0x00000020,
};

DEFINE_ENUM_FLAG_OPERATORS(ALIGN)

// Universal values for alignment of graphic layouts in documents.

enum class LAYOUT : ULONG {
   NIL = 0,
   SQUARE = 0x00000000,
   TIGHT = 0x00000001,
   LEFT = 0x00000002,
   RIGHT = 0x00000004,
   WIDE = 0x00000006,
   BACKGROUND = 0x00000008,
   FOREGROUND = 0x00000010,
   EMBEDDED = 0x00000020,
   LOCK = 0x00000040,
   IGNORE_CURSOR = 0x00000080,
   TILE = 0x00000100,
};

DEFINE_ENUM_FLAG_OPERATORS(LAYOUT)

// Script flags

enum class SCF : ULONG {
   NIL = 0,
   EXIT_ON_ERROR = 0x00000001,
   LOG_ALL = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(SCF)

enum class STR : ULONG {
   NIL = 0,
   MATCH_CASE = 0x00000001,
   CASE = 0x00000001,
   MATCH_LEN = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(STR)

// Message flags.

enum class MSF : ULONG {
   NIL = 0,
   WAIT = 0x00000001,
   UPDATE = 0x00000002,
   NO_DUPLICATE = 0x00000004,
   ADD = 0x00000008,
   ADDRESS = 0x00000010,
   MESSAGE_ID = 0x00000020,
};

DEFINE_ENUM_FLAG_OPERATORS(MSF)

// Flags for ProcessMessages

enum class PMF : ULONG {
   NIL = 0,
   SYSTEM_NO_BREAK = 0x00000001,
};

DEFINE_ENUM_FLAG_OPERATORS(PMF)

// Flags for RegisterFD()

enum class RFD : ULONG {
   NIL = 0,
   WRITE = 0x00000001,
   EXCEPT = 0x00000002,
   READ = 0x00000004,
   REMOVE = 0x00000008,
   STOP_RECURSE = 0x00000010,
   ALLOW_RECURSION = 0x00000020,
   SOCKET = 0x00000040,
   RECALL = 0x00000080,
   ALWAYS_CALL = 0x00000100,
};

DEFINE_ENUM_FLAG_OPERATORS(RFD)

// Task flags

enum class TSF : ULONG {
   NIL = 0,
   FOREIGN = 0x00000001,
   WAIT = 0x00000002,
   RESET_PATH = 0x00000004,
   PRIVILEGED = 0x00000008,
   SHELL = 0x00000010,
   LOG_ALL = 0x00000020,
   QUIET = 0x00000040,
   DETACHED = 0x00000080,
   ATTACHED = 0x00000100,
   PIPE = 0x00000200,
};

DEFINE_ENUM_FLAG_OPERATORS(TSF)

// Internal options for requesting function tables from modules.

enum class MHF : ULONG {
   NIL = 0,
   STATIC = 0x00000001,
   STRUCTURE = 0x00000002,
   DEFAULT = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(MHF)

// MoveToPoint flags

enum class MTF : ULONG {
   NIL = 0,
   X = 0x00000001,
   Y = 0x00000002,
   Z = 0x00000004,
   ANIM = 0x00000008,
   RELATIVE = 0x00000010,
};

DEFINE_ENUM_FLAG_OPERATORS(MTF)

// VlogF flags

enum class VLF : ULONG {
   NIL = 0,
   BRANCH = 0x00000001,
   ERROR = 0x00000002,
   WARNING = 0x00000004,
   CRITICAL = 0x00000008,
   INFO = 0x00000010,
   API = 0x00000020,
   DETAIL = 0x00000040,
   TRACE = 0x00000080,
   FUNCTION = 0x00000100,
};

DEFINE_ENUM_FLAG_OPERATORS(VLF)

// Module flags

enum class MOF : ULONG {
   NIL = 0,
   LINK_LIBRARY = 0x00000001,
   STATIC = 0x00000002,
   SYSTEM_PROBE = 0x00000004,
};

DEFINE_ENUM_FLAG_OPERATORS(MOF)

// Thread flags

enum class THF : ULONG {
   NIL = 0,
   AUTO_FREE = 0x00000001,
};

DEFINE_ENUM_FLAG_OPERATORS(THF)

// Flags for the SetDate() file method.

enum class FDT : LONG {
   NIL = 0,
   MODIFIED = 0,
   CREATED = 1,
   ACCESSED = 2,
   ARCHIVED = 3,
};

// Options for SetVolume()

enum class VOLUME : ULONG {
   NIL = 0,
   REPLACE = 0x00000001,
   PRIORITY = 0x00000002,
   HIDDEN = 0x00000004,
   SYSTEM = 0x00000008,
};

DEFINE_ENUM_FLAG_OPERATORS(VOLUME)

// Options for the File Delete() method.

enum class FDL : ULONG {
   NIL = 0,
   FEEDBACK = 0x00000001,
};

DEFINE_ENUM_FLAG_OPERATORS(FDL)

// Compression flags

enum class CMF : ULONG {
   NIL = 0,
   PASSWORD = 0x00000001,
   NEW = 0x00000002,
   CREATE_FILE = 0x00000004,
   READ_ONLY = 0x00000008,
   NO_LINKS = 0x00000010,
   APPLY_SECURITY = 0x00000020,
};

DEFINE_ENUM_FLAG_OPERATORS(CMF)

// Flags for ResolvePath()

enum class RSF : ULONG {
   NIL = 0,
   NO_FILE_CHECK = 0x00000001,
   CHECK_VIRTUAL = 0x00000002,
   APPROXIMATE = 0x00000004,
   NO_DEEP_SCAN = 0x00000008,
   PATH = 0x00000010,
   CASE_SENSITIVE = 0x00000020,
};

DEFINE_ENUM_FLAG_OPERATORS(RSF)

// Flags for the File Watch() method.

enum class MFF : ULONG {
   NIL = 0,
   READ = 0x00000001,
   MODIFY = 0x00000002,
   WRITE = 0x00000002,
   CREATE = 0x00000004,
   DELETE = 0x00000008,
   MOVED = 0x00000010,
   RENAME = 0x00000010,
   ATTRIB = 0x00000020,
   OPENED = 0x00000040,
   CLOSED = 0x00000080,
   UNMOUNT = 0x00000100,
   FOLDER = 0x00000200,
   FILE = 0x00000400,
   SELF = 0x00000800,
   DEEP = 0x00001000,
};

DEFINE_ENUM_FLAG_OPERATORS(MFF)

// Types for StrDatatype().

enum class STT : LONG {
   NIL = 0,
   NUMBER = 1,
   FLOAT = 2,
   HEX = 3,
   STRING = 4,
};

enum class OPF : ULONG {
   NIL = 0,
   OPTIONS = 0x00000001,
   MAX_DEPTH = 0x00000002,
   DETAIL = 0x00000004,
   SHOW_MEMORY = 0x00000008,
   SHOW_IO = 0x00000010,
   SHOW_ERRORS = 0x00000020,
   ARGS = 0x00000040,
   ERROR = 0x00000080,
   PRIVILEGED = 0x00000100,
   SYSTEM_PATH = 0x00000200,
   MODULE_PATH = 0x00000400,
   ROOT_PATH = 0x00000800,
   SCAN_MODULES = 0x00001000,
};

DEFINE_ENUM_FLAG_OPERATORS(OPF)

enum class TOI : LONG {
   NIL = 0,
   LOCAL_CACHE = 0,
   LOCAL_STORAGE = 1,
   ANDROID_ENV = 2,
   ANDROID_CLASS = 3,
   ANDROID_ASSETMGR = 4,
};

// Flags for the OpenDir() function.

enum class RDF : ULONG {
   NIL = 0,
   SIZE = 0x00000001,
   DATE = 0x00000002,
   TIME = 0x00000002,
   PERMISSIONS = 0x00000004,
   FILES = 0x00000008,
   FILE = 0x00000008,
   FOLDERS = 0x00000010,
   FOLDER = 0x00000010,
   READ_ALL = 0x0000001f,
   VOLUME = 0x00000020,
   LINK = 0x00000040,
   TAGS = 0x00000080,
   HIDDEN = 0x00000100,
   QUALIFY = 0x00000200,
   QUALIFIED = 0x00000200,
   VIRTUAL = 0x00000400,
   STREAM = 0x00000800,
   READ_ONLY = 0x00001000,
   ARCHIVE = 0x00002000,
   OPENDIR = 0x00004000,
};

DEFINE_ENUM_FLAG_OPERATORS(RDF)

// File flags

enum class FL : ULONG {
   NIL = 0,
   WRITE = 0x00000001,
   NEW = 0x00000002,
   READ = 0x00000004,
   DIRECTORY = 0x00000008,
   FOLDER = 0x00000008,
   APPROXIMATE = 0x00000010,
   LINK = 0x00000020,
   BUFFER = 0x00000040,
   LOOP = 0x00000080,
   FILE = 0x00000100,
   RESET_DATE = 0x00000200,
   DEVICE = 0x00000400,
   STREAM = 0x00000800,
   EXCLUDE_FILES = 0x00001000,
   EXCLUDE_FOLDERS = 0x00002000,
};

DEFINE_ENUM_FLAG_OPERATORS(FL)

// AnalysePath() values

enum class LOC : LONG {
   NIL = 0,
   DIRECTORY = 1,
   FOLDER = 1,
   VOLUME = 2,
   FILE = 3,
};

// Flags for LoadFile()

enum class LDF : ULONG {
   NIL = 0,
   CHECK_EXISTS = 0x00000001,
};

DEFINE_ENUM_FLAG_OPERATORS(LDF)

// Flags for file feedback.

enum class FBK : LONG {
   NIL = 0,
   MOVE_FILE = 1,
   COPY_FILE = 2,
   DELETE_FILE = 3,
};

// Return codes available to the feedback routine

enum class FFR : LONG {
   NIL = 0,
   OKAY = 0,
   CONTINUE = 0,
   SKIP = 1,
   ABORT = 2,
};

// For use by VirtualVolume()

enum class VAS : LONG {
   NIL = 0,
   DEREGISTER = 1,
   SCAN_DIR = 2,
   DELETE = 3,
   RENAME = 4,
   OPEN_DIR = 5,
   CLOSE_DIR = 6,
   TEST_PATH = 7,
   WATCH_PATH = 8,
   IGNORE_FILE = 9,
   GET_INFO = 10,
   GET_DEVICE_INFO = 11,
   IDENTIFY_FILE = 12,
   MAKE_DIR = 13,
   SAME_FILE = 14,
   CASE_SENSITIVE = 15,
   READ_LINK = 16,
   CREATE_LINK = 17,
   DRIVER_SIZE = 18,
};

// Feedback event indicators.

enum class FDB : LONG {
   NIL = 0,
   DECOMPRESS_FILE = 1,
   COMPRESS_FILE = 2,
   REMOVE_FILE = 3,
   DECOMPRESS_OBJECT = 4,
};

// Compression stream formats

enum class CF : LONG {
   NIL = 0,
   GZIP = 1,
   ZLIB = 2,
   DEFLATE = 3,
};

// Flags that can be passed to FindObject()

enum class FOF : ULONG {
   NIL = 0,
   SMART_NAMES = 0x00000001,
};

DEFINE_ENUM_FLAG_OPERATORS(FOF)

// Flags that can be passed to NewObject().  If a flag needs to be stored with the object, it must be specified in the lower word.

enum class NF : ULONG {
   NIL = 0,
   PRIVATE = 0x00000000,
   UNTRACKED = 0x00000001,
   INITIALISED = 0x00000002,
   LOCAL = 0x00000004,
   FREE_ON_UNLOCK = 0x00000008,
   FREE = 0x00000010,
   TIMER_SUB = 0x00000020,
   SUPPRESS_LOG = 0x00000040,
   COLLECT = 0x00000080,
   RECLASSED = 0x00000100,
   MESSAGE = 0x00000200,
   SIGNALLED = 0x00000400,
   UNIQUE = 0x40000000,
   NAME = 0x80000000,
};

DEFINE_ENUM_FLAG_OPERATORS(NF)

#define MAX_NAME_LEN 31
#define MAX_FILENAME 256

// Reserved message ID's that are handled internally.

#define MSGID_WAIT_FOR_OBJECTS 90
#define MSGID_THREAD_ACTION 91
#define MSGID_THREAD_CALLBACK 92
#define MSGID_VALIDATE_PROCESS 93
#define MSGID_EVENT 94
#define MSGID_DEBUG 95
#define MSGID_FREE 98
#define MSGID_ACTION 99
#define MSGID_BREAK 100
#define MSGID_CORE_END 100
#define MSGID_COMMAND 101
#define MSGID_QUIT 1000

// Types for AllocateID()

enum class IDTYPE : LONG {
   NIL = 0,
   MESSAGE = 1,
   GLOBAL = 2,
   FUNCTION = 3,
};

// Indicates the state of a process.

enum class TSTATE : BYTE {
   NIL = 0,
   RUNNING = 0,
   PAUSED = 1,
   STOPPING = 2,
   TERMINATED = 3,
};

enum class RES : LONG {
   NIL = 0,
   FREE_SWAP = 1,
   CONSOLE_FD = 2,
   KEY_STATE = 3,
   USER_ID = 4,
   DISPLAY_DRIVER = 5,
   PRIVILEGED_USER = 6,
   PRIVILEGED = 7,
   CORE_IDL = 8,
   STATIC_BUILD = 9,
   LOG_LEVEL = 10,
   TOTAL_SHARED_MEMORY = 11,
   MAX_PROCESSES = 12,
   LOG_DEPTH = 13,
   JNI_ENV = 14,
   THREAD_ID = 15,
   OPEN_INFO = 16,
   EXCEPTION_HANDLER = 17,
   NET_PROCESSING = 18,
   PROCESS_STATE = 19,
   TOTAL_MEMORY = 20,
   TOTAL_SWAP = 21,
   CPU_SPEED = 22,
   FREE_MEMORY = 23,
};

// Path types for SetResourcePath()

enum class RP : LONG {
   NIL = 0,
   MODULE_PATH = 1,
   SYSTEM_PATH = 2,
   ROOT_PATH = 3,
};

// Flags for the MetaClass.

enum class CLF : ULONG {
   NIL = 0,
   INHERIT_LOCAL = 0x00000001,
   NO_OWNERSHIP = 0x00000002,
};

DEFINE_ENUM_FLAG_OPERATORS(CLF)

// Flags for the Config class.

enum class CNF : ULONG {
   NIL = 0,
   STRIP_QUOTES = 0x00000001,
   AUTO_SAVE = 0x00000002,
   OPTIONAL_FILES = 0x00000004,
   NEW = 0x00000008,
};

DEFINE_ENUM_FLAG_OPERATORS(CNF)

// Raw key codes

enum class KEY : LONG {
   NIL = 0,
   A = 1,
   B = 2,
   C = 3,
   D = 4,
   E = 5,
   F = 6,
   G = 7,
   H = 8,
   I = 9,
   J = 10,
   K = 11,
   L = 12,
   M = 13,
   N = 14,
   O = 15,
   P = 16,
   Q = 17,
   R = 18,
   S = 19,
   T = 20,
   U = 21,
   V = 22,
   W = 23,
   X = 24,
   Y = 25,
   Z = 26,
   ONE = 27,
   TWO = 28,
   THREE = 29,
   FOUR = 30,
   FIVE = 31,
   SIX = 32,
   SEVEN = 33,
   EIGHT = 34,
   NINE = 35,
   ZERO = 36,
   REVERSE_QUOTE = 37,
   MINUS = 38,
   EQUALS = 39,
   L_SQUARE = 40,
   R_SQUARE = 41,
   SEMI_COLON = 42,
   APOSTROPHE = 43,
   COMMA = 44,
   DOT = 45,
   PERIOD = 45,
   SLASH = 46,
   BACK_SLASH = 47,
   SPACE = 48,
   NP_0 = 49,
   NP_1 = 50,
   NP_2 = 51,
   NP_3 = 52,
   NP_4 = 53,
   NP_5 = 54,
   NP_6 = 55,
   NP_7 = 56,
   NP_8 = 57,
   NP_9 = 58,
   NP_MULTIPLY = 59,
   NP_PLUS = 60,
   NP_BAR = 61,
   NP_SEPARATOR = 61,
   NP_MINUS = 62,
   NP_DECIMAL = 63,
   NP_DOT = 63,
   NP_DIVIDE = 64,
   L_CONTROL = 65,
   R_CONTROL = 66,
   HELP = 67,
   L_SHIFT = 68,
   R_SHIFT = 69,
   CAPS_LOCK = 70,
   PRINT = 71,
   L_ALT = 72,
   R_ALT = 73,
   L_COMMAND = 74,
   R_COMMAND = 75,
   F1 = 76,
   F2 = 77,
   F3 = 78,
   F4 = 79,
   F5 = 80,
   F6 = 81,
   F7 = 82,
   F8 = 83,
   F9 = 84,
   F10 = 85,
   F11 = 86,
   F12 = 87,
   F13 = 88,
   F14 = 89,
   F15 = 90,
   F16 = 91,
   F17 = 92,
   MACRO = 93,
   NP_PLUS_MINUS = 94,
   LESS_GREATER = 95,
   UP = 96,
   DOWN = 97,
   RIGHT = 98,
   LEFT = 99,
   SCR_LOCK = 100,
   PAUSE = 101,
   WAKE = 102,
   SLEEP = 103,
   POWER = 104,
   BACKSPACE = 105,
   TAB = 106,
   ENTER = 107,
   ESCAPE = 108,
   DELETE = 109,
   CLEAR = 110,
   HOME = 111,
   PAGE_UP = 112,
   PAGE_DOWN = 113,
   END = 114,
   SELECT = 115,
   EXECUTE = 116,
   INSERT = 117,
   UNDO = 118,
   REDO = 119,
   MENU = 120,
   FIND = 121,
   CANCEL = 122,
   BREAK = 123,
   NUM_LOCK = 124,
   PRT_SCR = 125,
   NP_ENTER = 126,
   SYSRQ = 127,
   F18 = 128,
   F19 = 129,
   F20 = 130,
   WIN_CONTROL = 131,
   VOLUME_UP = 132,
   VOLUME_DOWN = 133,
   BACK = 134,
   CALL = 135,
   END_CALL = 136,
   CAMERA = 137,
   AT = 138,
   PLUS = 139,
   LENS_FOCUS = 140,
   STOP = 141,
   NEXT = 142,
   PREVIOUS = 143,
   FORWARD = 144,
   REWIND = 145,
   MUTE = 146,
   STAR = 147,
   POUND = 148,
   PLAY = 149,
   LIST_END = 150,
};

struct InputEvent {
   const struct InputEvent * Next;    // Next event in the chain
   DOUBLE   Value;                    // The value associated with the Type
   LARGE    Timestamp;                // PreciseTime() of the recorded input
   OBJECTID RecipientID;              // Surface that the input message is being conveyed to
   OBJECTID OverID;                   // Surface that is directly under the mouse pointer at the time of the event
   DOUBLE   AbsX;                     // Absolute horizontal position of mouse cursor (relative to the top left of the display)
   DOUBLE   AbsY;                     // Absolute vertical position of mouse cursor (relative to the top left of the display)
   DOUBLE   X;                        // Horizontal position relative to the surface that the pointer is over - unless a mouse button is held or pointer is anchored - then the coordinates are relative to the click-held surface
   DOUBLE   Y;                        // Vertical position relative to the surface that the pointer is over - unless a mouse button is held or pointer is anchored - then the coordinates are relative to the click-held surface
   OBJECTID DeviceID;                 // The hardware device that this event originated from
   JET      Type;                     // JET constant that describes the event
   JTYPE    Flags;                    // Broad descriptors for the given Type (see JTYPE flags).  Automatically defined when delivered to the pointer object
   JTYPE    Mask;                     // Mask to use for checking against subscribers
};

struct dcRequest {
   LONG Item;             // Identifier for retrieval from the source
   char Preference[4];    // Data preferences for the returned item(s)
};

struct dcAudio {
   LONG Size;    // Byte size of this structure
   LONG Format;  // Format of the audio data
};

struct dcKeyEntry {
   LONG  Flags;        // Shift/Control/CapsLock...
   LONG  Value;        // ASCII value of the key A/B/C/D...
   LARGE Timestamp;    // ~Core.PreciseTime() at which the keypress was recorded
   LONG  Unicode;      // Unicode value for pre-calculated key translations
};

struct dcDeviceInput {
   DOUBLE   Values[2];  // The value(s) associated with the Type
   LARGE    Timestamp;  // ~Core.PreciseTime() of the recorded input
   OBJECTID DeviceID;   // The hardware device that this event originated from (note: This ID can be to a private/inaccessible object, the point is that the ID is unique)
   JTYPE    Flags;      // Broad descriptors for the given Type.  Automatically defined when delivered to the pointer object
   JET      Type;       // JET constant
};

struct DateTime {
   WORD Year;        // Year
   BYTE Month;       // Month 1 to 12
   BYTE Day;         // Day 1 to 31
   BYTE Hour;        // Hour 0 to 23
   BYTE Minute;      // Minute 0 to 59
   BYTE Second;      // Second 0 to 59
   BYTE TimeZone;    // TimeZone -13 to +13
};

struct HSV {
   DOUBLE Hue;           // Between 0 and 359.999
   DOUBLE Saturation;    // Between 0 and 1.0
   DOUBLE Value;         // Between 0 and 1.0.  Corresponds to Value, Lightness or Brightness
   DOUBLE Alpha;         // Alpha blending value from 0 to 1.0.
};

struct FRGB {
   FLOAT Red;    // Red component value
   FLOAT Green;  // Green component value
   FLOAT Blue;   // Blue component value
   FLOAT Alpha;  // Alpha component value
   FRGB() { };
   FRGB(FLOAT R, FLOAT G, FLOAT B, FLOAT A) : Red(R), Green(G), Blue(B), Alpha(A) { };
};

typedef struct RGB8 {
   UBYTE Red;    // Red component value
   UBYTE Green;  // Green component value
   UBYTE Blue;   // Blue component value
   UBYTE Alpha;  // Alpha component value
} RGB8;

struct RGB16 {
   UWORD Red;    // Red component value
   UWORD Green;  // Green component value
   UWORD Blue;   // Blue component value
   UWORD Alpha;  // Alpha component value
};

struct RGB32 {
   ULONG Red;    // Red component value
   ULONG Green;  // Green component value
   ULONG Blue;   // Blue component value
   ULONG Alpha;  // Alpha component value
};

struct RGBPalette {
   LONG AmtColours;         // Total colours
   struct RGB8 Col[256];    // RGB Palette
};

typedef struct ColourFormat {
   UBYTE RedShift;        // Right shift value for red (15/16 bit formats only)
   UBYTE GreenShift;      // Right shift value for green
   UBYTE BlueShift;       // Right shift value for blue
   UBYTE AlphaShift;      // Right shift value for alpha
   UBYTE RedMask;         // Unshifted mask value for red (ranges from 0x00 to 0xff)
   UBYTE GreenMask;       // Unshifted mask value for green
   UBYTE BlueMask;        // Unshifted mask value for blue
   UBYTE AlphaMask;       // Unshifted mask value for alpha
   UBYTE RedPos;          // Left shift/positional value for red
   UBYTE GreenPos;        // Left shift/positional value for green
   UBYTE BluePos;         // Left shift/positional value for blue
   UBYTE AlphaPos;        // Left shift/positional value for alpha
   UBYTE BitsPerPixel;    // Number of bits per pixel for this format.
} COLOURFORMAT;

struct ClipRectangle {
   LONG Left;    // Left-most coordinate
   LONG Top;     // Top coordinate
   LONG Right;   // Right-most coordinate
   LONG Bottom;  // Bottom coordinate
  ClipRectangle() { }
  ClipRectangle(LONG Value) : Left(Value), Top(Value), Right(Value), Bottom(Value) { }
  ClipRectangle(LONG pLeft, LONG pTop, LONG pRight, LONG pBottom) : Left(pLeft), Top(pTop), Right(pRight), Bottom(pBottom) { }
  int width() const { return Right - Left; }
  int height() const { return Bottom - Top; }
};

struct Edges {
   LONG Left;    // Left-most coordinate
   LONG Top;     // Top coordinate
   LONG Right;   // Right-most coordinate
   LONG Bottom;  // Bottom coordinate
};

#define AHASH_ACTIVATE 0xdbaf4876
#define AHASH_CLEAR 0x0f3b6d8c
#define AHASH_FREEWARNING 0xb903ddbd
#define AHASH_COPYDATA 0x47b0d1fa
#define AHASH_DATAFEED 0x05e6d293
#define AHASH_DEACTIVATE 0x1ee323ff
#define AHASH_DRAW 0x7c95d753
#define AHASH_FLUSH 0x0f71fd67
#define AHASH_FOCUS 0x0f735645
#define AHASH_FREE 0x7c96f087
#define AHASH_GETKEY 0xff87790e
#define AHASH_DRAGDROP 0xf69e8a58
#define AHASH_HIDE 0x7c97e2df
#define AHASH_INIT 0x7c988539
#define AHASH_LOCK 0x7c9a2dce
#define AHASH_LOSTFOCUS 0x319b8e67
#define AHASH_MOVE 0x7c9abc9c
#define AHASH_MOVETOBACK 0xcbdb3170
#define AHASH_MOVETOFRONT 0x479347c8
#define AHASH_NEWCHILD 0x7b86ebf3
#define AHASH_NEWOWNER 0x7c68601a
#define AHASH_NEWOBJECT 0x07f62dc6
#define AHASH_REDO 0x7c9d4daf
#define AHASH_QUERY 0x103db63b
#define AHASH_READ 0x7c9d4d41
#define AHASH_RENAME 0x192cc41d
#define AHASH_RESET 0x10474288
#define AHASH_RESIZE 0x192fa5b7
#define AHASH_SAVEIMAGE 0x398f7c57
#define AHASH_SAVETOOBJECT 0x2878872e
#define AHASH_SEEK 0x7c9dda2d
#define AHASH_SETKEY 0x1b85609a
#define AHASH_SHOW 0x7c9de846
#define AHASH_TIMER 0x106d8b86
#define AHASH_UNLOCK 0x20ce3c11
#define AHASH_NEXT 0x7c9b1ec4
#define AHASH_PREV 0x7c9c6c62
#define AHASH_WRITE 0x10a8b550
#define AHASH_SETFIELD 0x12075f55
#define AHASH_CLIPBOARD 0x4912a9b5
#define AHASH_REFRESH 0x3e3db654
#define AHASH_DISABLE 0x12c4e4b9
#define AHASH_ENABLE 0xfb7573ac
#define AHASH_REDIMENSION 0x08a67fa2
#define AHASH_MOVETOPOINT 0x48467e29
#define AHASH_SORT 0x7c9e066d
#define AHASH_SAVESETTINGS 0x475f7165
#define AHASH_SIGNAL 0x1bc6ade3
#define AHASH_NEWPLACEMENT 0x9b0a0468
#define AHASH_UNDO 0x7c9f191b


typedef AC ACTIONID;

#ifndef __GNUC__
#define __attribute__(a)
#endif

typedef const std::vector<std::pair<std::string, ULONG>> STRUCTS;
typedef std::map<std::string, std::string, std::less<>> KEYVALUE;

#ifndef STRINGIFY
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#endif

#define MOD_IDL NULL

#ifdef PARASOL_STATIC
__export void CloseCore(void);
__export ERR OpenCore(struct OpenInfo *, struct CoreBase **);
#else
__export struct ModHeader ModHeader;
#endif

#ifdef MOD_NAME
#ifdef PARASOL_STATIC
#define PARASOL_MOD(init,close,open,expunge,IDL,Structures) static struct ModHeader ModHeader(init, close, open, expunge, IDL, Structures, TOSTRING(MOD_NAME));
#else
#define PARASOL_MOD(init,close,open,expunge,IDL,Structures) struct ModHeader ModHeader(init, close, open, expunge, IDL, Structures, TOSTRING(MOD_NAME));
#endif
#define MOD_PATH ("modules:" TOSTRING(MOD_NAME))
#else
#define MOD_NAME NULL
#endif

namespace pf {

template <class T> T roundup(T Num, LONG Alignment) {
   return (Num + Alignment) - (Num % Alignment); // Round up to Alignment value, e.g. (14,8) = 16
}

#ifdef PRINTF64I
  #define PF64 "I64d"
#elif PRINTF64_PRID
  #define PF64 PRId64
#else
  #define PF64 "lld"
#endif

// Use DEBUG_BREAK in critical areas where you would want to break in gdb.  This feature will only be compiled
// in to debug builds.

#ifdef _DEBUG
 #ifdef _MSC_VER
  #define DEBUG_BREAK __debugbreak();
 #elif __linux__
  #define DEBUG_BREAK raise(SIGTRAP);
 #else
  #define DEBUG_BREAK
 #endif
#else
 #define DEBUG_BREAK
#endif

// Fast float-2-int conversion, with rounding to the nearest integer (F2I) and truncation (F2T)

#if defined(__GNUC__) && defined(__x86__)

INLINE LONG F2I(DOUBLE val) {
   // This will round if the CPU is kept in its default rounding mode
   LONG ret;
   asm ("fistpl %0" : "=m" (ret) : "t" (val) : "st");
   return(ret);
}

#else

INLINE LONG F2I(DOUBLE val) {
   DOUBLE t = val + 6755399441055744.0;
   return *((int *)(&t));
}

#endif

inline LONG F2T(DOUBLE val) // For numbers no larger than 16 bit, standard (LONG) is faster than F2T().
{
   if ((val > 32767.0) or (val < -32767.0)) return((LONG)val);
   else {
      val = val + (68719476736.0 * 1.5);
      if constexpr (std::endian::native == std::endian::little) {
         return ((LONG *)(APTR)&val)[0]>>16;
      }
      else return ((LONG *)&val)[1]>>16;
   }
}

} // namespace

// Structures to pass to OpenCore()

struct OpenTag {
   TOI Tag;
   union {
      LONG Long;
      LARGE Large;
      APTR Pointer;
      CSTRING String;
   } Value;
};

struct OpenInfo {
   CSTRING Name;            // OPF::NAME
   CSTRING *Args;           // OPF::ARGS
   CSTRING SystemPath;      // OPF::SYSTEM_PATH
   CSTRING ModulePath;      // OPF::MODULE_PATH
   CSTRING RootPath;        // OPF::ROOT_PATH
   OpenTag *Options;        // OPF::OPTIONS Typecast to va_list (defined in stdarg.h)
   OPF     Flags;           // OPF::flags need to be set for fields that have been defined in this structure.
   LONG    MaxDepth;        // OPF::MAX_DEPTH
   LONG    Detail;          // OPF::DETAIL
   LONG    ArgCount;        // OPF::ARGS
   ERR     Error;           // OPF::ERROR
};

// Flags for defining fields, methods, actions and functions.  CLASSDEF's can only be used in field definitions for
// classes.  FUNCDEF's can only be used in argument definitions for methods, actions and functions.

#ifdef _LP64
#define FD_PTR64 FD_POINTER
#else
#define FD_PTR64 0
#endif

// Field flags for classes.  These are intended to simplify field definitions, e.g. using FDF_BYTEARRAY combines
// FD_ARRAY with FD_BYTE.  DO NOT use these for function definitions, they are not intended to be compatible.

// Sizes/Types

#define FT_POINTER  FD_POINTER
#define FT_FLOAT    FD_FLOAT
#define FT_LONG     FD_LONG
#define FT_DOUBLE   FD_DOUBLE
#define FT_LARGE    FD_LARGE
#define FT_STRING   (FD_POINTER|FD_STRING)
#define FT_UNLISTED FD_UNLISTED
#define FT_UNIT     FD_UNIT

// Class field definitions.  See core.h for all FD definitions.

#define FDF_BYTE       FD_BYTE
#define FDF_WORD       FD_WORD     // Field is word sized (16-bit)
#define FDF_LONG       FD_LONG     // Field is long sized (32-bit)
#define FDF_DOUBLE     FD_DOUBLE   // Field is double floating point sized (64-bit)
#define FDF_LARGE      FD_LARGE    // Field is large sized (64-bit)
#define FDF_POINTER    FD_POINTER  // Field is an address pointer (typically 32-bit)
#define FDF_ARRAY      FD_ARRAY    // Field is a pointer to an array
#define FDF_CPP        FD_CPP      // Field is a C++ type variant
#define FDF_PTR        FD_POINTER
#define FDF_UNIT       FD_UNIT
#define FDF_SYNONYM    FD_SYNONYM

#define FDF_UNSIGNED    FD_UNSIGNED
#define FDF_FUNCTION    FD_FUNCTION           // sizeof(FUNCTION) - use FDF_FUNCTIONPTR for sizeof(APTR)
#define FDF_FUNCTIONPTR (FD_FUNCTION|FD_POINTER)
#define FDF_STRUCT      FD_STRUCT
#define FDF_RESOURCE    FD_RESOURCE
#define FDF_OBJECT      (FD_POINTER|FD_OBJECT)   // Field refers to another object
#define FDF_OBJECTID    (FD_LONG|FD_OBJECT)      // Field refers to another object by ID
#define FDF_LOCAL       (FD_POINTER|FD_LOCAL)    // Field refers to a local object
#define FDF_STRING      (FD_POINTER|FD_STRING)   // Field points to a string.  NB: Ideally want to remove the FD_POINTER as it should be redundant
#define FDF_STR         FDF_STRING
#define FDF_SCALED      FD_SCALED
#define FDF_FLAGS       FD_FLAGS                // Field contains flags
#define FDF_ALLOC       FD_ALLOC                // Field is a dynamic allocation - either a memory block or object
#define FDF_LOOKUP      FD_LOOKUP               // Lookup names for values in this field
#define FDF_READ        FD_READ                 // Field is readable
#define FDF_WRITE       FD_WRITE                // Field is writeable
#define FDF_INIT        FD_INIT                 // Field can only be written prior to Init()
#define FDF_SYSTEM      FD_SYSTEM
#define FDF_ERROR       (FD_LONG|FD_ERROR)
#define FDF_RGB         (FD_RGB|FD_BYTE|FD_ARRAY)
#define FDF_R           FD_READ
#define FDF_W           FD_WRITE
#define FDF_RW          (FD_READ|FD_WRITE)
#define FDF_RI          (FD_READ|FD_INIT)
#define FDF_I           FD_INIT
#define FDF_VIRTUAL     FD_VIRTUAL
#define FDF_LONGFLAGS   (FDF_LONG|FDF_FLAGS)
#define FDF_FIELDTYPES  (FD_LONG|FD_DOUBLE|FD_LARGE|FD_POINTER|FD_UNIT|FD_BYTE|FD_ARRAY|FD_FUNCTION)

// These constants have to match the FD* constants << 32

#define TDOUBLE   0x8000000000000000LL
#define TLONG     0x4000000000000000LL
#define TUNIT     0x2000000000000000LL
#define TFLOAT    0x1000000000000000LL // NB: Floats are upscaled to doubles when passed as v-args.
#define TPTR      0x0800000000000000LL
#define TLARGE    0x0400000000000000LL
#define TFUNCTION 0x0200000000000000LL
#define TSTR      0x0080000000000000LL
#define TARRAY    0x0000100000000000LL
#define TSCALE    0x0020000000000000LL
#define TAGEND    0LL
#define TAGDIVERT -1LL
#define TSTRING   TSTR

#define nextutf8(str) if (*(str)) for (++(str); (*(str) & 0xc0) IS 0x80; (str)++);

//********************************************************************************************************************
// FieldValue is used to simplify the initialisation of new objects.

namespace pf {

struct FieldValue {
   ULONG FieldID;
   LONG Type;
   union {
      CSTRING String;
      APTR    Pointer;
      CPTR    CPointer;
      DOUBLE  Double;
      SCALE   Percent;
      LARGE   Large;
      LONG    Long;
   };

   //std::string not included as not compatible with constexpr
   constexpr FieldValue(ULONG pFID, CSTRING pValue)   : FieldID(pFID), Type(FD_STRING), String(pValue) { };
   constexpr FieldValue(ULONG pFID, LONG pValue)      : FieldID(pFID), Type(FD_LONG), Long(pValue) { };
   constexpr FieldValue(ULONG pFID, LARGE pValue)     : FieldID(pFID), Type(FD_LARGE), Large(pValue) { };
   constexpr FieldValue(ULONG pFID, DOUBLE pValue)    : FieldID(pFID), Type(FD_DOUBLE), Double(pValue) { };
   constexpr FieldValue(ULONG pFID, SCALE pValue)     : FieldID(pFID), Type(FD_DOUBLE|FD_SCALED), Percent(pValue) { };
   constexpr FieldValue(ULONG pFID, const FUNCTION &pValue) : FieldID(pFID), Type(FDF_FUNCTIONPTR), CPointer(&pValue) { };
   constexpr FieldValue(ULONG pFID, const FUNCTION *pValue) : FieldID(pFID), Type(FDF_FUNCTIONPTR), CPointer(pValue) { };
   constexpr FieldValue(ULONG pFID, APTR pValue)      : FieldID(pFID), Type(FD_POINTER), Pointer(pValue) { };
   constexpr FieldValue(ULONG pFID, CPTR pValue)      : FieldID(pFID), Type(FD_POINTER), CPointer(pValue) { };
   constexpr FieldValue(ULONG pFID, CPTR pValue, LONG pCustom) : FieldID(pFID), Type(pCustom), CPointer(pValue) { };
};


class FloatRect {
   public:
   DOUBLE X;    // Left-most coordinate
   DOUBLE Y;     // Top coordinate
   DOUBLE Width;   // Right-most coordinate
   DOUBLE Height;  // Bottom coordinate
   FloatRect() { }
   FloatRect(DOUBLE Value) : X(Value), Y(Value), Width(Value), Height(Value) { }
   FloatRect(DOUBLE pX, DOUBLE pY, DOUBLE pWidth, DOUBLE pHeight) : X(pX), Y(pY), Width(pWidth), Height(pHeight) { }
};

}

#include <string.h> // memset()
#include <stdlib.h> // strtol(), strtod()

namespace dmf { // Helper functions for DMF flags
inline bool has(DMF Value, DMF Flags) { return (Value & Flags) != DMF::NIL; }

inline bool hasX(DMF Value) { return (Value & DMF::FIXED_X) != DMF::NIL; }
inline bool hasY(DMF Value) { return (Value & DMF::FIXED_Y) != DMF::NIL; }
inline bool hasWidth(DMF Value) { return (Value & DMF::FIXED_WIDTH) != DMF::NIL; }
inline bool hasHeight(DMF Value) { return (Value & DMF::FIXED_HEIGHT) != DMF::NIL; }
inline bool hasXOffset(DMF Value) { return (Value & DMF::FIXED_X_OFFSET) != DMF::NIL; }
inline bool hasYOffset(DMF Value) { return (Value & DMF::FIXED_Y_OFFSET) != DMF::NIL; }
inline bool hasScaledX(DMF Value) { return (Value & DMF::SCALED_X) != DMF::NIL; }
inline bool hasScaledY(DMF Value) { return (Value & DMF::SCALED_Y) != DMF::NIL; }
inline bool hasScaledWidth(DMF Value) { return (Value & DMF::SCALED_WIDTH) != DMF::NIL; }
inline bool hasScaledHeight(DMF Value) { return (Value & DMF::SCALED_HEIGHT) != DMF::NIL; }
inline bool hasScaledXOffset(DMF Value) { return (Value & DMF::SCALED_X_OFFSET) != DMF::NIL; }
inline bool hasScaledYOffset(DMF Value) { return (Value & DMF::SCALED_Y_OFFSET) != DMF::NIL; }
inline bool hasScaledCenterX(DMF Value) { return (Value & DMF::SCALED_CENTER_X) != DMF::NIL; }
inline bool hasScaledCenterY(DMF Value) { return (Value & DMF::SCALED_CENTER_Y) != DMF::NIL; }
inline bool hasScaledRadiusX(DMF Value) { return (Value & DMF::SCALED_RADIUS_X) != DMF::NIL; }
inline bool hasScaledRadiusY(DMF Value) { return (Value & DMF::SCALED_RADIUS_Y) != DMF::NIL; }

inline bool hasAnyHorizontalPosition(DMF Value) { return (Value & (DMF::FIXED_X|DMF::SCALED_X|DMF::FIXED_X_OFFSET|DMF::SCALED_X_OFFSET)) != DMF::NIL; }
inline bool hasAnyVerticalPosition(DMF Value) { return (Value & (DMF::FIXED_Y|DMF::SCALED_Y|DMF::FIXED_Y_OFFSET|DMF::SCALED_Y_OFFSET)) != DMF::NIL; }
inline bool hasAnyScaledRadius(DMF Value) { return (Value & (DMF::SCALED_RADIUS_X|DMF::SCALED_RADIUS_Y)) != DMF::NIL; }
inline bool hasAnyX(DMF Value) { return (Value & (DMF::SCALED_X|DMF::FIXED_X)) != DMF::NIL; }
inline bool hasAnyY(DMF Value) { return (Value & (DMF::SCALED_Y|DMF::FIXED_Y)) != DMF::NIL; }
inline bool hasAnyWidth(DMF Value) { return (Value & (DMF::SCALED_WIDTH|DMF::FIXED_WIDTH)) != DMF::NIL; }
inline bool hasAnyHeight(DMF Value) { return (Value & (DMF::SCALED_HEIGHT|DMF::FIXED_HEIGHT)) != DMF::NIL; }
inline bool hasAnyXOffset(DMF Value) { return (Value & (DMF::SCALED_X_OFFSET|DMF::FIXED_X_OFFSET)) != DMF::NIL; }
inline bool hasAnyYOffset(DMF Value) { return (Value & (DMF::SCALED_Y_OFFSET|DMF::FIXED_Y_OFFSET)) != DMF::NIL; }
}


struct ObjectSignal {
   OBJECTPTR Object;    // Reference to an object to monitor.
};

struct ResourceManager {
   CSTRING Name;          // The name of the resource.
   ERR (*Free)(APTR);     // A function that will remove the resource's content when terminated.
};

struct FunctionField {
   CSTRING Name;    // Name of the field
   ULONG   Type;    // Type of the field
};

struct Function {
   APTR    Address;                      // Pointer to the function entry point
   CSTRING Name;                         // Name of the function
   const struct FunctionField * Args;    // A list of parameters accepted by the function
};

struct ModHeader {
   MHF     Flags;                                 // Special flags, type of function table wanted from the Core
   CSTRING Definitions;                           // Module definition string, usable by run-time languages such as Fluid
   ERR (*Init)(OBJECTPTR, struct CoreBase *);     // A one-off initialisation routine for when the module is first opened.
   void (*Close)(OBJECTPTR);                      // A function that will be called each time the module is closed.
   ERR (*Open)(OBJECTPTR);                        // A function that will be called each time the module is opened.
   ERR (*Expunge)(void);                          // Reference to an expunge function to terminate the module.
   CSTRING Name;                                  // Name of the module
   STRUCTS *StructDefs;
   class RootModule *Root;
   ModHeader(ERR (*pInit)(OBJECTPTR, struct CoreBase *),
      void  (*pClose)(OBJECTPTR),
      ERR (*pOpen)(OBJECTPTR),
      ERR (*pExpunge)(void),
      CSTRING pDef,
      STRUCTS *pStructs,
      CSTRING pName) {
      Flags         = MHF::DEFAULT;
      Definitions   = pDef;
      StructDefs    = pStructs;
      Init          = pInit;
      Close         = pClose;
      Open          = pOpen;
      Expunge       = pExpunge;
      Name          = pName;
      Root          = NULL;
   }
};

struct FieldArray {
   CSTRING Name;    // The name of the field, e.g. Width
   APTR    GetField; // void GetField(*Object, APTR Result);
   APTR    SetField; // ERR SetField(*Object, APTR Value);
   MAXINT  Arg;     // Can be a pointer or an integer value
   ULONG   Flags;   // Special flags that describe the field
  template <class G = APTR, class S = APTR, class T = MAXINT> FieldArray(CSTRING pName, ULONG pFlags, G pGetField = NULL, S pSetField = NULL, T pArg = 0) :
     Name(pName), GetField((APTR)pGetField), SetField((APTR)pSetField), Arg((MAXINT)pArg), Flags(pFlags)
     { }
};

struct FieldDef {
   CSTRING Name;    // The name of the constant.
   LONG    Value;   // The value of the constant.
   template <class T> FieldDef(CSTRING pName, T pValue) : Name(pName), Value(LONG(pValue)) { }
};

struct SystemState {
   CSTRING Platform;        // String-based field indicating the user's platform.  Currently returns Native, Windows, OSX or Linux.
   HOSTHANDLE ConsoleFD;    // Internal
   LONG    Stage;           // The current operating stage.  -1 = Initialising, 0 indicates normal operating status; 1 means that the program is shutting down; 2 indicates a program restart; 3 is for mode switches.
};

struct Unit {
   DOUBLE Value;    // The unit value.
   ULONG  Type;     // Additional type information
   Unit(double pValue, LONG pType = FD_DOUBLE) : Value(pValue), Type(pType) { }
   Unit() : Value(0), Type(0) { }
   operator double() const { return Value; }
   inline void set(const double pValue) { Value = pValue; }
   inline bool scaled() { return (Type & FD_SCALED) ? true : false; }
};

struct ActionArray {
   APTR Routine;    // Pointer to the function entry point
   AC   ActionCode; // Action identifier
  template <class T> ActionArray(AC pID, T pRoutine) : Routine((APTR)pRoutine), ActionCode(pID) { }
};

struct MethodEntry {
   AC      MethodID;                     // Unique method identifier
   APTR    Routine;                      // The method entry point, defined as ERR (*Routine)(OBJECTPTR, APTR);
   CSTRING Name;                         // Name of the method
   const struct FunctionField * Args;    // List of parameters accepted by the method
   LONG    Size;                         // Total byte-size of all accepted parameters when they are assembled as a C structure.
   MethodEntry() : MethodID(AC::NIL), Routine(NULL), Name(NULL) { }
   MethodEntry(AC pID, APTR pRoutine, CSTRING pName, const struct FunctionField *pArgs, LONG pSize) :
      MethodID(pID), Routine(pRoutine), Name(pName), Args(pArgs), Size(pSize) { }
};

struct ActionTable {
   ULONG   Hash;                         // Hash of the action name.
   LONG    Size;                         // Byte-size of the structure for this action.
   CSTRING Name;                         // Name of the action.
   const struct FunctionField * Args;    // List of fields that are passed to this action.
};

struct ChildEntry {
   OBJECTID ObjectID;    // Object ID
   CLASSID  ClassID;     // The class ID of the referenced object.
};

struct Message {
   LARGE Time;    // A timestamp acquired from ~Core.PreciseTime() when the message was first passed to ~Core.SendMessage().
   LONG  UID;     // A unique identifier automatically created by ~Core.SendMessage().
   LONG  Type;    // A message type identifier as defined by the client.
   LONG  Size;    // The byte-size of the message data, or zero if no data is provided.
};

typedef struct MemInfo {
   APTR     Start;       // The starting address of the memory block (does not apply to shared blocks).
   OBJECTID ObjectID;    // The object that owns the memory block.
   ULONG    Size;        // The size of the memory block.
   MEM      Flags;       // The type of memory.
   MEMORYID MemoryID;    // The unique ID for this block.
   WORD     AccessCount; // Total number of active locks on this block.
} MEMINFO;

struct MsgHandler {
   struct MsgHandler * Prev;    // Previous message handler in the chain
   struct MsgHandler * Next;    // Next message handler in the chain
   APTR     Custom;             // Custom pointer to send to the message handler
   FUNCTION Function;           // Call this function
   LONG     MsgType;            // Type of message being filtered
};

struct CacheFile {
   LARGE   TimeStamp;  // The file's last-modified timestamp.
   LARGE   Size;       // Byte size of the cached data.
   LARGE   LastUse;    // The last time that this file was requested.
   CSTRING Path;       // Pointer to the resolved file path.
   APTR    Data;       // Pointer to the cached data.
};

struct CompressionFeedback {
   FDB     FeedbackID;    // Set to one of the FDB event indicators
   LONG    Index;         // Index of the current file
   CSTRING Path;          // Name of the current file/path in the archive
   CSTRING Dest;          // Destination file/path during decompression
   LARGE   Progress;      // Progress indicator (byte position for the file being de/compressed).
   LARGE   OriginalSize;  // Original size of the file
   LARGE   CompressedSize; // Compressed size of the file
   WORD    Year;          // Year of the original file's datestamp.
   WORD    Month;         // Month of the original file's datestamp.
   WORD    Day;           // Day of the original file's datestamp.
   WORD    Hour;          // Hour of the original file's datestamp.
   WORD    Minute;        // Minute of the original file's datestamp.
   WORD    Second;        // Second of the original file's datestamp.
   CompressionFeedback() : FeedbackID(FDB::NIL), Index(0), Path(NULL), Dest(NULL),
      Progress(0), OriginalSize(0), CompressedSize(0),
      Year(0), Month(0), Day(0), Hour(0), Minute(0), Second(0) { }

   CompressionFeedback(FDB pFeedback, LONG pIndex, CSTRING pPath, CSTRING pDest) :
      FeedbackID(pFeedback), Index(pIndex), Path(pPath), Dest(pDest),
      Progress(0), OriginalSize(0), CompressedSize(0),
      Year(0), Month(0), Day(0), Hour(0), Minute(0), Second(0) { }
};

struct CompressedItem {
   LARGE   OriginalSize;            // Original size of the file
   LARGE   CompressedSize;          // Compressed size of the file
   struct CompressedItem * Next;    // Used only if this is a linked-list.
   CSTRING Path;                    // Path to the file (includes folder prefixes).  Archived folders will include the trailing slash.
   PERMIT  Permissions;             // Original permissions - see PERMIT flags.
   LONG    UserID;                  // Original user ID
   LONG    GroupID;                 // Original group ID
   LONG    OthersID;                // Original others ID
   FL      Flags;                   // FL flags
   struct DateTime Created;         // Date and time of the file's creation.
   struct DateTime Modified;        // Date and time last modified.
    std::unordered_map<std::string, std::string> *Tags;
};

struct FileInfo {
   LARGE  Size;               // The size of the file's content.
   LARGE  TimeStamp;          // 64-bit time stamp - usable only for comparison (e.g. sorting).
   struct FileInfo * Next;    // Next structure in the list, or NULL.
   STRING Name;               // The name of the file.
   RDF    Flags;              // Additional flags to describe the file.
   PERMIT Permissions;        // Standard permission flags.
   LONG   UserID;             // User  ID (Unix systems only).
   LONG   GroupID;            // Group ID (Unix systems only).
   struct DateTime Created;   // The date/time of the file's creation.
   struct DateTime Modified;  // The date/time of the last file modification.
    std::unordered_map<std::string, std::string> *Tags;
};

struct DirInfo {
   struct FileInfo * Info;    // Pointer to a FileInfo structure
   #ifdef PRV_FILE
   APTR   Driver;
   APTR   prvHandle;        // Directory handle.  If virtual, may store a private data address
   STRING prvPath;          // Original folder location string
   STRING prvResolvedPath;  // Resolved folder location
   RDF    prvFlags;         // OpenFolder() RDF flags
   LONG   prvTotal;         // Total number of items in the folder
   ULONG  prvVirtualID;     // Unique ID (name hash) for a virtual device
   union {
      LONG prvIndex;        // Current index within the folder when scanning
      APTR prvIndexPtr;
   };
   WORD   prvResolveLen;    // Byte length of ResolvedPath
   #endif
};

struct FileFeedback {
   LARGE  Size;          // Size of the file
   LARGE  Position;      // Current seek position within the file if moving or copying
   STRING Path;          // Path to the file
   STRING Dest;          // Destination file/path if moving or copying
   FBK    FeedbackID;    // Set to one of the FBK values
   char   Reserved[32];  // Reserved in case of future expansion
  FileFeedback() : Size(0), Position(0), Path(NULL), Dest(NULL), FeedbackID(FBK::NIL) { }
};

struct Field {
   MAXINT  Arg;                                                                // An option to complement the field type.  Can be a pointer or an integer value
   ERR (*GetValue)(APTR, APTR);                                                // A virtual function that will retrieve the value for this field.
   APTR    SetValue;                                                           // A virtual function that will set the value for this field.
   ERR (*WriteValue)(OBJECTPTR, struct Field *, LONG, const void *, LONG);     // An internal function for writing to this field.
   CSTRING Name;                                                               // The English name for the field, e.g. Width
   ULONG   FieldID;                                                            // Provides a fast way of finding fields, e.g. FID_Width
   UWORD   Offset;                                                             // Field offset within the object
   UWORD   Index;                                                              // Field array index
   ULONG   Flags;                                                              // Special flags that describe the field
};

struct ScriptArg { // For use with sc::Exec
   CSTRING Name;
   ULONG Type;
   union {
      APTR   Address;
      LONG   Long;
      LARGE  Large;
      DOUBLE Double;
   };

   ScriptArg(CSTRING pName, OBJECTPTR pValue, ULONG pType = FD_OBJECTPTR) : Name(pName), Type(pType), Address((APTR)pValue) { }
   ScriptArg(CSTRING pName, std::string &pValue, ULONG pType = FD_STRING) : Name(pName), Type(pType), Address((APTR)pValue.data()) { }
   ScriptArg(CSTRING pName, const std::string &pValue, ULONG pType = FD_STRING) : Name(pName), Type(pType), Address((APTR)pValue.data()) { }
   ScriptArg(CSTRING pName, CSTRING pValue, ULONG pType = FD_STRING) : Name(pName), Type(pType), Address((APTR)pValue) { }
   ScriptArg(CSTRING pName, APTR pValue, ULONG pType = FD_PTR) : Name(pName), Type(pType), Address(pValue) { }
   ScriptArg(CSTRING pName, LONG pValue, ULONG pType = FD_LONG) : Name(pName), Type(pType), Long(pValue) { }
   ScriptArg(CSTRING pName, ULONG pValue, ULONG pType = FD_LONG) : Name(pName), Type(pType), Long(pValue) { }
   ScriptArg(CSTRING pName, LARGE pValue, ULONG pType = FD_LARGE) : Name(pName), Type(pType), Large(pValue) { }
   ScriptArg(CSTRING pName, DOUBLE pValue, ULONG pType = FD_DOUBLE) : Name(pName), Type(pType), Double(pValue) { }
};

#ifdef PARASOL_STATIC
#define JUMPTABLE_CORE static struct CoreBase *CoreBase;
#else
#define JUMPTABLE_CORE struct CoreBase *CoreBase;
#endif

struct CoreBase {
#ifndef PARASOL_STATIC
   ERR (*_AccessMemory)(MEMORYID Memory, MEM Flags, LONG MilliSeconds, APTR *Result);
   ERR (*_Action)(AC Action, OBJECTPTR Object, APTR Parameters);
   void (*_ActionList)(struct ActionTable **Actions, LONG *Size);
   ERR (*_DeleteFile)(CSTRING Path, FUNCTION *Callback);
   CSTRING (*_ResolveClassID)(CLASSID ID);
   LONG (*_AllocateID)(IDTYPE Type);
   ERR (*_AllocMemory)(LONG Size, MEM Flags, APTR *Address, MEMORYID *ID);
   ERR (*_AccessObject)(OBJECTID Object, LONG MilliSeconds, OBJECTPTR *Result);
   ERR (*_CheckAction)(OBJECTPTR Object, AC Action);
   ERR (*_CheckMemoryExists)(MEMORYID ID);
   ERR (*_CheckObjectExists)(OBJECTID Object);
   ERR (*_InitObject)(OBJECTPTR Object);
   ERR (*_VirtualVolume)(CSTRING Name, ...);
   OBJECTPTR (*_CurrentContext)(void);
   ERR (*_GetFieldArray)(OBJECTPTR Object, FIELD Field, APTR *Result, LONG *Elements);
   LONG (*_AdjustLogLevel)(LONG Delta);
   ERR (*_ReadFileToBuffer)(CSTRING Path, APTR Buffer, LONG BufferSize, LONG *Result);
   ERR (*_FindObject)(CSTRING Name, CLASSID ClassID, FOF Flags, OBJECTID *ObjectID);
   objMetaClass * (*_FindClass)(CLASSID ClassID);
   ERR (*_AnalysePath)(CSTRING Path, LOC *Type);
   ERR (*_FreeResource)(MEMORYID ID);
   CLASSID (*_GetClassID)(OBJECTID Object);
   OBJECTID (*_GetOwnerID)(OBJECTID Object);
   ERR (*_GetField)(OBJECTPTR Object, FIELD Field, APTR Result);
   ERR (*_GetFieldVariable)(OBJECTPTR Object, CSTRING Field, STRING Buffer, LONG Size);
   ERR (*_CompareFilePaths)(CSTRING PathA, CSTRING PathB);
   const struct SystemState * (*_GetSystemState)(void);
   ERR (*_ListChildren)(OBJECTID Object, pf::vector<ChildEntry> *List);
   ERR (*_RegisterFD)(HOSTHANDLE FD, RFD Flags, void (*Routine)(HOSTHANDLE, APTR) , APTR Data);
   ERR (*_ResolvePath)(const std::string_view & Path, RSF Flags, std::string *Result);
   ERR (*_MemoryIDInfo)(MEMORYID ID, struct MemInfo *MemInfo, LONG Size);
   ERR (*_MemoryPtrInfo)(APTR Address, struct MemInfo *MemInfo, LONG Size);
   ERR (*_NewObject)(CLASSID ClassID, NF Flags, OBJECTPTR *Object);
   void (*_NotifySubscribers)(OBJECTPTR Object, AC Action, APTR Args, ERR Error);
   ERR (*_CopyFile)(CSTRING Source, CSTRING Dest, FUNCTION *Callback);
   ERR (*_ProcessMessages)(PMF Flags, LONG TimeOut);
   ERR (*_IdentifyFile)(CSTRING Path, CLASSID Filter, CLASSID *Class, CLASSID *SubClass);
   ERR (*_ReallocMemory)(APTR Memory, ULONG Size, APTR *Address, MEMORYID *ID);
   ERR (*_GetMessage)(LONG Type, MSF Flags, APTR Buffer, LONG Size);
   ERR (*_ReleaseMemory)(MEMORYID MemoryID);
   CLASSID (*_ResolveClassName)(CSTRING Name);
   ERR (*_SendMessage)(LONG Type, MSF Flags, APTR Data, LONG Size);
   ERR (*_SetOwner)(OBJECTPTR Object, OBJECTPTR Owner);
   OBJECTPTR (*_SetContext)(OBJECTPTR Object);
   ERR (*_SetField)(OBJECTPTR Object, FIELD Field, ...);
   CSTRING (*_FieldName)(ULONG FieldID);
   ERR (*_ScanDir)(struct DirInfo *Info);
   ERR (*_SetName)(OBJECTPTR Object, CSTRING Name);
   void (*_LogReturn)(void);
   ERR (*_SubscribeAction)(OBJECTPTR Object, AC Action, FUNCTION *Callback);
   ERR (*_SubscribeEvent)(LARGE Event, FUNCTION *Callback, APTR *Handle);
   ERR (*_SubscribeTimer)(DOUBLE Interval, FUNCTION *Callback, APTR *Subscription);
   ERR (*_UpdateTimer)(APTR Subscription, DOUBLE Interval);
   ERR (*_UnsubscribeAction)(OBJECTPTR Object, AC Action);
   void (*_UnsubscribeEvent)(APTR Handle);
   ERR (*_BroadcastEvent)(APTR Event, LONG EventSize);
   void (*_WaitTime)(LONG Seconds, LONG MicroSeconds);
   LARGE (*_GetEventID)(EVG Group, CSTRING SubGroup, CSTRING Event);
   ULONG (*_GenCRC32)(ULONG CRC, APTR Data, ULONG Length);
   LARGE (*_GetResource)(RES Resource);
   LARGE (*_SetResource)(RES Resource, LARGE Value);
   ERR (*_ScanMessages)(LONG *Handle, LONG Type, APTR Buffer, LONG Size);
   ERR (*_WaitForObjects)(PMF Flags, LONG TimeOut, struct ObjectSignal *ObjectSignals);
   void (*_UnloadFile)(struct CacheFile *Cache);
   ERR (*_CreateFolder)(CSTRING Path, PERMIT Permissions);
   ERR (*_LoadFile)(CSTRING Path, LDF Flags, struct CacheFile **Cache);
   ERR (*_SetVolume)(CSTRING Name, CSTRING Path, CSTRING Icon, CSTRING Label, CSTRING Device, VOLUME Flags);
   ERR (*_DeleteVolume)(CSTRING Name);
   ERR (*_MoveFile)(CSTRING Source, CSTRING Dest, FUNCTION *Callback);
   ERR (*_UpdateMessage)(LONG Message, LONG Type, APTR Data, LONG Size);
   ERR (*_AddMsgHandler)(APTR Custom, LONG MsgType, FUNCTION *Routine, struct MsgHandler **Handle);
   ERR (*_QueueAction)(AC Action, OBJECTID Object, APTR Args);
   LARGE (*_PreciseTime)(void);
   ERR (*_OpenDir)(CSTRING Path, RDF Flags, struct DirInfo **Info);
   OBJECTPTR (*_GetObjectPtr)(OBJECTID Object);
   struct Field * (*_FindField)(OBJECTPTR Object, ULONG FieldID, OBJECTPTR *Target);
   CSTRING (*_GetErrorMsg)(ERR Error);
   struct Message * (*_GetActionMsg)(void);
   ERR (*_FuncError)(CSTRING Header, ERR Error);
   ERR (*_SetArray)(OBJECTPTR Object, FIELD Field, APTR Array, LONG Elements);
   ERR (*_LockObject)(OBJECTPTR Object, LONG MilliSeconds);
   void (*_ReleaseObject)(OBJECTPTR Object);
   ERR (*_AsyncAction)(AC Action, OBJECTPTR Object, APTR Args, FUNCTION *Callback);
   ERR (*_AddInfoTag)(struct FileInfo *Info, CSTRING Name, CSTRING Value);
   void (*_SetDefaultPermissions)(LONG User, LONG Group, PERMIT Permissions);
   void (*_VLogF)(VLF Flags, const char *Header, const char *Message, va_list Args);
   ERR (*_ReadInfoTag)(struct FileInfo *Info, CSTRING Name, CSTRING *Value);
   ERR (*_SetResourcePath)(RP PathType, CSTRING Path);
   objTask * (*_CurrentTask)(void);
   CSTRING (*_ResolveGroupID)(LONG Group);
   CSTRING (*_ResolveUserID)(LONG User);
   ERR (*_CreateLink)(CSTRING From, CSTRING To);
   OBJECTPTR (*_ParentContext)(void);
#endif // PARASOL_STATIC
};

#ifndef PRV_CORE_MODULE
#ifndef PARASOL_STATIC
extern struct CoreBase *CoreBase;
inline ERR AccessMemory(MEMORYID Memory, MEM Flags, LONG MilliSeconds, APTR *Result) { return CoreBase->_AccessMemory(Memory,Flags,MilliSeconds,Result); }
inline ERR Action(AC Action, OBJECTPTR Object, APTR Parameters) { return CoreBase->_Action(Action,Object,Parameters); }
inline void ActionList(struct ActionTable **Actions, LONG *Size) { return CoreBase->_ActionList(Actions,Size); }
inline ERR DeleteFile(CSTRING Path, FUNCTION *Callback) { return CoreBase->_DeleteFile(Path,Callback); }
inline CSTRING ResolveClassID(CLASSID ID) { return CoreBase->_ResolveClassID(ID); }
inline LONG AllocateID(IDTYPE Type) { return CoreBase->_AllocateID(Type); }
inline ERR AllocMemory(LONG Size, MEM Flags, APTR *Address, MEMORYID *ID) { return CoreBase->_AllocMemory(Size,Flags,Address,ID); }
inline ERR AccessObject(OBJECTID Object, LONG MilliSeconds, OBJECTPTR *Result) { return CoreBase->_AccessObject(Object,MilliSeconds,Result); }
inline ERR CheckAction(OBJECTPTR Object, AC Action) { return CoreBase->_CheckAction(Object,Action); }
inline ERR CheckMemoryExists(MEMORYID ID) { return CoreBase->_CheckMemoryExists(ID); }
inline ERR CheckObjectExists(OBJECTID Object) { return CoreBase->_CheckObjectExists(Object); }
inline ERR InitObject(OBJECTPTR Object) { return CoreBase->_InitObject(Object); }
template<class... Args> ERR VirtualVolume(CSTRING Name, Args... Tags) { return CoreBase->_VirtualVolume(Name,Tags...); }
inline OBJECTPTR CurrentContext(void) { return CoreBase->_CurrentContext(); }
inline ERR GetFieldArray(OBJECTPTR Object, FIELD Field, APTR *Result, LONG *Elements) { return CoreBase->_GetFieldArray(Object,Field,Result,Elements); }
inline LONG AdjustLogLevel(LONG Delta) { return CoreBase->_AdjustLogLevel(Delta); }
inline ERR ReadFileToBuffer(CSTRING Path, APTR Buffer, LONG BufferSize, LONG *Result) { return CoreBase->_ReadFileToBuffer(Path,Buffer,BufferSize,Result); }
inline ERR FindObject(CSTRING Name, CLASSID ClassID, FOF Flags, OBJECTID *ObjectID) { return CoreBase->_FindObject(Name,ClassID,Flags,ObjectID); }
inline objMetaClass * FindClass(CLASSID ClassID) { return CoreBase->_FindClass(ClassID); }
inline ERR AnalysePath(CSTRING Path, LOC *Type) { return CoreBase->_AnalysePath(Path,Type); }
inline ERR FreeResource(MEMORYID ID) { return CoreBase->_FreeResource(ID); }
inline CLASSID GetClassID(OBJECTID Object) { return CoreBase->_GetClassID(Object); }
inline OBJECTID GetOwnerID(OBJECTID Object) { return CoreBase->_GetOwnerID(Object); }
inline ERR GetField(OBJECTPTR Object, FIELD Field, APTR Result) { return CoreBase->_GetField(Object,Field,Result); }
inline ERR GetFieldVariable(OBJECTPTR Object, CSTRING Field, STRING Buffer, LONG Size) { return CoreBase->_GetFieldVariable(Object,Field,Buffer,Size); }
inline ERR CompareFilePaths(CSTRING PathA, CSTRING PathB) { return CoreBase->_CompareFilePaths(PathA,PathB); }
inline const struct SystemState * GetSystemState(void) { return CoreBase->_GetSystemState(); }
inline ERR ListChildren(OBJECTID Object, pf::vector<ChildEntry> *List) { return CoreBase->_ListChildren(Object,List); }
inline ERR RegisterFD(HOSTHANDLE FD, RFD Flags, void (*Routine)(HOSTHANDLE, APTR) , APTR Data) { return CoreBase->_RegisterFD(FD,Flags,Routine,Data); }
inline ERR ResolvePath(const std::string_view & Path, RSF Flags, std::string *Result) { return CoreBase->_ResolvePath(Path,Flags,Result); }
inline ERR MemoryIDInfo(MEMORYID ID, struct MemInfo *MemInfo, LONG Size) { return CoreBase->_MemoryIDInfo(ID,MemInfo,Size); }
inline ERR MemoryPtrInfo(APTR Address, struct MemInfo *MemInfo, LONG Size) { return CoreBase->_MemoryPtrInfo(Address,MemInfo,Size); }
inline ERR NewObject(CLASSID ClassID, NF Flags, OBJECTPTR *Object) { return CoreBase->_NewObject(ClassID,Flags,Object); }
inline void NotifySubscribers(OBJECTPTR Object, AC Action, APTR Args, ERR Error) { return CoreBase->_NotifySubscribers(Object,Action,Args,Error); }
inline ERR CopyFile(CSTRING Source, CSTRING Dest, FUNCTION *Callback) { return CoreBase->_CopyFile(Source,Dest,Callback); }
inline ERR ProcessMessages(PMF Flags, LONG TimeOut) { return CoreBase->_ProcessMessages(Flags,TimeOut); }
inline ERR IdentifyFile(CSTRING Path, CLASSID Filter, CLASSID *Class, CLASSID *SubClass) { return CoreBase->_IdentifyFile(Path,Filter,Class,SubClass); }
inline ERR ReallocMemory(APTR Memory, ULONG Size, APTR *Address, MEMORYID *ID) { return CoreBase->_ReallocMemory(Memory,Size,Address,ID); }
inline ERR GetMessage(LONG Type, MSF Flags, APTR Buffer, LONG Size) { return CoreBase->_GetMessage(Type,Flags,Buffer,Size); }
inline ERR ReleaseMemory(MEMORYID MemoryID) { return CoreBase->_ReleaseMemory(MemoryID); }
inline CLASSID ResolveClassName(CSTRING Name) { return CoreBase->_ResolveClassName(Name); }
inline ERR SendMessage(LONG Type, MSF Flags, APTR Data, LONG Size) { return CoreBase->_SendMessage(Type,Flags,Data,Size); }
inline ERR SetOwner(OBJECTPTR Object, OBJECTPTR Owner) { return CoreBase->_SetOwner(Object,Owner); }
inline OBJECTPTR SetContext(OBJECTPTR Object) { return CoreBase->_SetContext(Object); }
template<class... Args> ERR SetField(OBJECTPTR Object, FIELD Field, Args... Tags) { return CoreBase->_SetField(Object,Field,Tags...); }
inline CSTRING FieldName(ULONG FieldID) { return CoreBase->_FieldName(FieldID); }
inline ERR ScanDir(struct DirInfo *Info) { return CoreBase->_ScanDir(Info); }
inline ERR SetName(OBJECTPTR Object, CSTRING Name) { return CoreBase->_SetName(Object,Name); }
inline void LogReturn(void) { return CoreBase->_LogReturn(); }
inline ERR SubscribeAction(OBJECTPTR Object, AC Action, FUNCTION *Callback) { return CoreBase->_SubscribeAction(Object,Action,Callback); }
inline ERR SubscribeEvent(LARGE Event, FUNCTION *Callback, APTR *Handle) { return CoreBase->_SubscribeEvent(Event,Callback,Handle); }
inline ERR SubscribeTimer(DOUBLE Interval, FUNCTION *Callback, APTR *Subscription) { return CoreBase->_SubscribeTimer(Interval,Callback,Subscription); }
inline ERR UpdateTimer(APTR Subscription, DOUBLE Interval) { return CoreBase->_UpdateTimer(Subscription,Interval); }
inline ERR UnsubscribeAction(OBJECTPTR Object, AC Action) { return CoreBase->_UnsubscribeAction(Object,Action); }
inline void UnsubscribeEvent(APTR Handle) { return CoreBase->_UnsubscribeEvent(Handle); }
inline ERR BroadcastEvent(APTR Event, LONG EventSize) { return CoreBase->_BroadcastEvent(Event,EventSize); }
inline void WaitTime(LONG Seconds, LONG MicroSeconds) { return CoreBase->_WaitTime(Seconds,MicroSeconds); }
inline LARGE GetEventID(EVG Group, CSTRING SubGroup, CSTRING Event) { return CoreBase->_GetEventID(Group,SubGroup,Event); }
inline ULONG GenCRC32(ULONG CRC, APTR Data, ULONG Length) { return CoreBase->_GenCRC32(CRC,Data,Length); }
inline LARGE GetResource(RES Resource) { return CoreBase->_GetResource(Resource); }
inline LARGE SetResource(RES Resource, LARGE Value) { return CoreBase->_SetResource(Resource,Value); }
inline ERR ScanMessages(LONG *Handle, LONG Type, APTR Buffer, LONG Size) { return CoreBase->_ScanMessages(Handle,Type,Buffer,Size); }
inline ERR WaitForObjects(PMF Flags, LONG TimeOut, struct ObjectSignal *ObjectSignals) { return CoreBase->_WaitForObjects(Flags,TimeOut,ObjectSignals); }
inline void UnloadFile(struct CacheFile *Cache) { return CoreBase->_UnloadFile(Cache); }
inline ERR CreateFolder(CSTRING Path, PERMIT Permissions) { return CoreBase->_CreateFolder(Path,Permissions); }
inline ERR LoadFile(CSTRING Path, LDF Flags, struct CacheFile **Cache) { return CoreBase->_LoadFile(Path,Flags,Cache); }
inline ERR SetVolume(CSTRING Name, CSTRING Path, CSTRING Icon, CSTRING Label, CSTRING Device, VOLUME Flags) { return CoreBase->_SetVolume(Name,Path,Icon,Label,Device,Flags); }
inline ERR DeleteVolume(CSTRING Name) { return CoreBase->_DeleteVolume(Name); }
inline ERR MoveFile(CSTRING Source, CSTRING Dest, FUNCTION *Callback) { return CoreBase->_MoveFile(Source,Dest,Callback); }
inline ERR UpdateMessage(LONG Message, LONG Type, APTR Data, LONG Size) { return CoreBase->_UpdateMessage(Message,Type,Data,Size); }
inline ERR AddMsgHandler(APTR Custom, LONG MsgType, FUNCTION *Routine, struct MsgHandler **Handle) { return CoreBase->_AddMsgHandler(Custom,MsgType,Routine,Handle); }
inline ERR QueueAction(AC Action, OBJECTID Object, APTR Args) { return CoreBase->_QueueAction(Action,Object,Args); }
inline LARGE PreciseTime(void) { return CoreBase->_PreciseTime(); }
inline ERR OpenDir(CSTRING Path, RDF Flags, struct DirInfo **Info) { return CoreBase->_OpenDir(Path,Flags,Info); }
inline OBJECTPTR GetObjectPtr(OBJECTID Object) { return CoreBase->_GetObjectPtr(Object); }
inline struct Field * FindField(OBJECTPTR Object, ULONG FieldID, OBJECTPTR *Target) { return CoreBase->_FindField(Object,FieldID,Target); }
inline CSTRING GetErrorMsg(ERR Error) { return CoreBase->_GetErrorMsg(Error); }
inline struct Message * GetActionMsg(void) { return CoreBase->_GetActionMsg(); }
inline ERR FuncError(CSTRING Header, ERR Error) { return CoreBase->_FuncError(Header,Error); }
inline ERR SetArray(OBJECTPTR Object, FIELD Field, APTR Array, LONG Elements) { return CoreBase->_SetArray(Object,Field,Array,Elements); }
inline ERR LockObject(OBJECTPTR Object, LONG MilliSeconds) { return CoreBase->_LockObject(Object,MilliSeconds); }
inline void ReleaseObject(OBJECTPTR Object) { return CoreBase->_ReleaseObject(Object); }
inline ERR AsyncAction(AC Action, OBJECTPTR Object, APTR Args, FUNCTION *Callback) { return CoreBase->_AsyncAction(Action,Object,Args,Callback); }
inline ERR AddInfoTag(struct FileInfo *Info, CSTRING Name, CSTRING Value) { return CoreBase->_AddInfoTag(Info,Name,Value); }
inline void SetDefaultPermissions(LONG User, LONG Group, PERMIT Permissions) { return CoreBase->_SetDefaultPermissions(User,Group,Permissions); }
inline void VLogF(VLF Flags, const char *Header, const char *Message, va_list Args) { return CoreBase->_VLogF(Flags,Header,Message,Args); }
inline ERR ReadInfoTag(struct FileInfo *Info, CSTRING Name, CSTRING *Value) { return CoreBase->_ReadInfoTag(Info,Name,Value); }
inline ERR SetResourcePath(RP PathType, CSTRING Path) { return CoreBase->_SetResourcePath(PathType,Path); }
inline objTask * CurrentTask(void) { return CoreBase->_CurrentTask(); }
inline CSTRING ResolveGroupID(LONG Group) { return CoreBase->_ResolveGroupID(Group); }
inline CSTRING ResolveUserID(LONG User) { return CoreBase->_ResolveUserID(User); }
inline ERR CreateLink(CSTRING From, CSTRING To) { return CoreBase->_CreateLink(From,To); }
inline OBJECTPTR ParentContext(void) { return CoreBase->_ParentContext(); }
#else
extern "C" ERR AccessMemory(MEMORYID Memory, MEM Flags, LONG MilliSeconds, APTR *Result);
extern "C" ERR Action(AC Action, OBJECTPTR Object, APTR Parameters);
extern "C" void ActionList(struct ActionTable **Actions, LONG *Size);
extern "C" ERR DeleteFile(CSTRING Path, FUNCTION *Callback);
extern "C" CSTRING ResolveClassID(CLASSID ID);
extern "C" LONG AllocateID(IDTYPE Type);
extern "C" ERR AllocMemory(LONG Size, MEM Flags, APTR *Address, MEMORYID *ID);
extern "C" ERR AccessObject(OBJECTID Object, LONG MilliSeconds, OBJECTPTR *Result);
extern "C" ERR CheckAction(OBJECTPTR Object, AC Action);
extern "C" ERR CheckMemoryExists(MEMORYID ID);
extern "C" ERR CheckObjectExists(OBJECTID Object);
extern "C" ERR InitObject(OBJECTPTR Object);
extern "C" OBJECTPTR CurrentContext(void);
extern "C" ERR GetFieldArray(OBJECTPTR Object, FIELD Field, APTR *Result, LONG *Elements);
extern "C" LONG AdjustLogLevel(LONG Delta);
extern "C" ERR ReadFileToBuffer(CSTRING Path, APTR Buffer, LONG BufferSize, LONG *Result);
extern "C" ERR FindObject(CSTRING Name, CLASSID ClassID, FOF Flags, OBJECTID *ObjectID);
extern "C" objMetaClass * FindClass(CLASSID ClassID);
extern "C" ERR AnalysePath(CSTRING Path, LOC *Type);
extern "C" ERR FreeResource(MEMORYID ID);
extern "C" CLASSID GetClassID(OBJECTID Object);
extern "C" OBJECTID GetOwnerID(OBJECTID Object);
extern "C" ERR GetField(OBJECTPTR Object, FIELD Field, APTR Result);
extern "C" ERR GetFieldVariable(OBJECTPTR Object, CSTRING Field, STRING Buffer, LONG Size);
extern "C" ERR CompareFilePaths(CSTRING PathA, CSTRING PathB);
extern "C" const struct SystemState * GetSystemState(void);
extern "C" ERR ListChildren(OBJECTID Object, pf::vector<ChildEntry> *List);
extern "C" ERR RegisterFD(HOSTHANDLE FD, RFD Flags, void (*Routine)(HOSTHANDLE, APTR) , APTR Data);
extern "C" ERR ResolvePath(const std::string_view & Path, RSF Flags, std::string *Result);
extern "C" ERR MemoryIDInfo(MEMORYID ID, struct MemInfo *MemInfo, LONG Size);
extern "C" ERR MemoryPtrInfo(APTR Address, struct MemInfo *MemInfo, LONG Size);
extern "C" ERR NewObject(CLASSID ClassID, NF Flags, OBJECTPTR *Object);
extern "C" void NotifySubscribers(OBJECTPTR Object, AC Action, APTR Args, ERR Error);
extern "C" ERR CopyFile(CSTRING Source, CSTRING Dest, FUNCTION *Callback);
extern "C" ERR ProcessMessages(PMF Flags, LONG TimeOut);
extern "C" ERR IdentifyFile(CSTRING Path, CLASSID Filter, CLASSID *Class, CLASSID *SubClass);
extern "C" ERR ReallocMemory(APTR Memory, ULONG Size, APTR *Address, MEMORYID *ID);
extern "C" ERR GetMessage(LONG Type, MSF Flags, APTR Buffer, LONG Size);
extern "C" ERR ReleaseMemory(MEMORYID MemoryID);
extern "C" CLASSID ResolveClassName(CSTRING Name);
extern "C" ERR SendMessage(LONG Type, MSF Flags, APTR Data, LONG Size);
extern "C" ERR SetOwner(OBJECTPTR Object, OBJECTPTR Owner);
extern "C" OBJECTPTR SetContext(OBJECTPTR Object);
extern "C" ERR SetField(OBJECTPTR Object, FIELD Field, ...);
extern "C" CSTRING FieldName(ULONG FieldID);
extern "C" ERR ScanDir(struct DirInfo *Info);
extern "C" ERR SetName(OBJECTPTR Object, CSTRING Name);
extern "C" void LogReturn(void);
extern "C" ERR SubscribeAction(OBJECTPTR Object, AC Action, FUNCTION *Callback);
extern "C" ERR SubscribeEvent(LARGE Event, FUNCTION *Callback, APTR *Handle);
extern "C" ERR SubscribeTimer(DOUBLE Interval, FUNCTION *Callback, APTR *Subscription);
extern "C" ERR UpdateTimer(APTR Subscription, DOUBLE Interval);
extern "C" ERR UnsubscribeAction(OBJECTPTR Object, AC Action);
extern "C" void UnsubscribeEvent(APTR Handle);
extern "C" ERR BroadcastEvent(APTR Event, LONG EventSize);
extern "C" void WaitTime(LONG Seconds, LONG MicroSeconds);
extern "C" LARGE GetEventID(EVG Group, CSTRING SubGroup, CSTRING Event);
extern "C" ULONG GenCRC32(ULONG CRC, APTR Data, ULONG Length);
extern "C" LARGE GetResource(RES Resource);
extern "C" LARGE SetResource(RES Resource, LARGE Value);
extern "C" ERR ScanMessages(LONG *Handle, LONG Type, APTR Buffer, LONG Size);
extern "C" ERR WaitForObjects(PMF Flags, LONG TimeOut, struct ObjectSignal *ObjectSignals);
extern "C" void UnloadFile(struct CacheFile *Cache);
extern "C" ERR CreateFolder(CSTRING Path, PERMIT Permissions);
extern "C" ERR LoadFile(CSTRING Path, LDF Flags, struct CacheFile **Cache);
extern "C" ERR SetVolume(CSTRING Name, CSTRING Path, CSTRING Icon, CSTRING Label, CSTRING Device, VOLUME Flags);
extern "C" ERR DeleteVolume(CSTRING Name);
extern "C" ERR MoveFile(CSTRING Source, CSTRING Dest, FUNCTION *Callback);
extern "C" ERR UpdateMessage(LONG Message, LONG Type, APTR Data, LONG Size);
extern "C" ERR AddMsgHandler(APTR Custom, LONG MsgType, FUNCTION *Routine, struct MsgHandler **Handle);
extern "C" ERR QueueAction(AC Action, OBJECTID Object, APTR Args);
extern "C" LARGE PreciseTime(void);
extern "C" ERR OpenDir(CSTRING Path, RDF Flags, struct DirInfo **Info);
extern "C" OBJECTPTR GetObjectPtr(OBJECTID Object);
extern "C" struct Field * FindField(OBJECTPTR Object, ULONG FieldID, OBJECTPTR *Target);
extern "C" CSTRING GetErrorMsg(ERR Error);
extern "C" struct Message * GetActionMsg(void);
extern "C" ERR FuncError(CSTRING Header, ERR Error);
extern "C" ERR SetArray(OBJECTPTR Object, FIELD Field, APTR Array, LONG Elements);
extern "C" ERR LockObject(OBJECTPTR Object, LONG MilliSeconds);
extern "C" void ReleaseObject(OBJECTPTR Object);
extern "C" ERR AsyncAction(AC Action, OBJECTPTR Object, APTR Args, FUNCTION *Callback);
extern "C" ERR AddInfoTag(struct FileInfo *Info, CSTRING Name, CSTRING Value);
extern "C" void SetDefaultPermissions(LONG User, LONG Group, PERMIT Permissions);
extern "C" void VLogF(VLF Flags, const char *Header, const char *Message, va_list Args);
extern "C" ERR ReadInfoTag(struct FileInfo *Info, CSTRING Name, CSTRING *Value);
extern "C" ERR SetResourcePath(RP PathType, CSTRING Path);
extern "C" objTask * CurrentTask(void);
extern "C" CSTRING ResolveGroupID(LONG Group);
extern "C" CSTRING ResolveUserID(LONG User);
extern "C" ERR CreateLink(CSTRING From, CSTRING To);
extern "C" OBJECTPTR ParentContext(void);
#endif // PARASOL_STATIC
#endif


//********************************************************************************************************************

#define END_FIELD FieldArray(NULL, 0)
#define FDEF static const struct FunctionField

template <class T> inline MEMORYID GetMemoryID(T &&A) {
   return ((MEMORYID *)A)[-2];
}

inline ERR DeregisterFD(HOSTHANDLE Handle) {
   return RegisterFD(Handle, RFD::REMOVE|RFD::READ|RFD::WRITE|RFD::EXCEPT|RFD::ALWAYS_CALL, 0, 0);
}

inline APTR GetResourcePtr(RES ID) { return (APTR)(MAXINT)GetResource(ID); }

inline CSTRING to_cstring(const std::string &A) { return A.c_str(); }
constexpr inline CSTRING to_cstring(CSTRING A) { return A; }
#ifndef PRV_CORE_DATA
// These overloaded functions can't be used in the Core as they will confuse the compiler in key areas.

inline ERR SubscribeAction(OBJECTPTR Object, AC Action, FUNCTION Callback) {
   return SubscribeAction(Object,Action,&Callback);
}

inline ERR SubscribeEvent(LARGE Event, FUNCTION Callback, APTR *Handle) {
   return SubscribeEvent(Event,&Callback,Handle);
}

inline ERR SubscribeTimer(DOUBLE Interval, FUNCTION Callback, APTR *Subscription) {
   return SubscribeTimer(Interval,&Callback,Subscription);
}

inline ERR ReleaseMemory(const void *Address) {
   if (!Address) return ERR::NullArgs;
   return ReleaseMemory(((MEMORYID *)Address)[-2]);
}

inline ERR FreeResource(const void *Address) {
   if (!Address) return ERR::NullArgs;
   return FreeResource(((LONG *)Address)[-2]);
}

inline ERR AllocMemory(LONG Size, MEM Flags, APTR Address) {
   return AllocMemory(Size, Flags, (APTR *)Address, NULL);
}

template<class T> inline ERR NewObject(CLASSID ClassID, T **Result) {
   return NewObject(ClassID, NF::NIL, (OBJECTPTR *)Result);
}

template<class T> inline ERR NewLocalObject(CLASSID ClassID, T **Result) {
   return NewObject(ClassID, NF::LOCAL, (OBJECTPTR *)Result);
}

inline ERR MemoryIDInfo(MEMORYID ID, struct MemInfo * MemInfo) {
   return MemoryIDInfo(ID,MemInfo,sizeof(struct MemInfo));
}

inline ERR MemoryPtrInfo(APTR Address, struct MemInfo * MemInfo) {
   return MemoryPtrInfo(Address,MemInfo,sizeof(struct MemInfo));
}

inline ERR QueueAction(AC Action, OBJECTID ObjectID) {
   return QueueAction(Action, ObjectID, NULL);
}

template <class T> inline ERR SetArray(OBJECTPTR Object, FIELD FieldID, pf::vector<T> &Array)
{
   return SetArray(Object, FieldID, Array.data(), Array.size());
}

template <class T> inline ERR SetArray(OBJECTPTR Object, FIELD FieldID, std::vector<T> &Array)
{
   return SetArray(Object, FieldID, Array.data(), Array.size());
}

template <class T, std::size_t SIZE> inline ERR SetArray(OBJECTPTR Object, FIELD FieldID, std::array<T, SIZE> Array)
{
   return SetArray(Object, FieldID, Array.data(), SIZE);
}
#endif

typedef KEYVALUE ConfigKeys;
typedef std::pair<std::string, ConfigKeys> ConfigGroup;
typedef std::vector<ConfigGroup> ConfigGroups;

namespace pf {

inline void copymem(const void *Src, APTR Dest, std::size_t Length) {
   memmove(Dest, Src, Length);
}

inline void clearmem(APTR Memory, std::size_t Length) {
   if (Memory) memset(Memory, 0, Length);
}

static THREADVAR LONG _tlUniqueThreadID = 0;

[[nodiscard]] inline LONG _get_thread_id(void) {
   if (_tlUniqueThreadID) return _tlUniqueThreadID;
   _tlUniqueThreadID = GetResource(RES::THREAD_ID);
   return _tlUniqueThreadID;
}

// For extremely verbose debug logs, run cmake with -DPARASOL_VLOG=ON

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-zero-length"

class Log { // C++ wrapper for Parasol's log functionality
   private:
      LONG branches;
      CSTRING header;

   public:
      Log() : branches(0), header(NULL) { }
      Log(CSTRING Header) : branches(0), header(Header) { }

      ~Log() {
         while (branches > 0) { branches--; LogReturn(); }
      }

      void branch(CSTRING Message = "", ...) __attribute__((format(printf, 2, 3))) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF::API|VLF::BRANCH, header, Message, arg);
         va_end(arg);
         branches++;
      }

      #ifdef _DEBUG
      void traceBranch(CSTRING Message = "", ...) __attribute__((format(printf, 2, 3))) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF::TRACE|VLF::BRANCH, header, Message, arg);
         va_end(arg);
         branches++;
      }
      #else
      void traceBranch(CSTRING Message = "", ...) __attribute__((format(printf, 2, 3))) { }
      #endif

      inline void debranch() {
         branches--;
         LogReturn();
      }

      void app(CSTRING Message, ...) { // Info level, recommended for applications only
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF::INFO, header, Message, arg);
         va_end(arg);
      }

      void msg(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // Defaults to API level, recommended for modules
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF::API, header, Message, arg);
         va_end(arg);
      }

      void msg(VLF Flags, CSTRING Message, ...) __attribute__((format(printf, 3, 4))) { // Defaults to API level, recommended for modules
         va_list arg;
         va_start(arg, Message);
         VLogF(Flags, header, Message, arg);
         va_end(arg);
         if ((Flags & VLF::BRANCH) != VLF::NIL) branches++;
      }

      void detail(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // Detailed API message
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF::DETAIL, header, Message, arg);
         va_end(arg);
      }

      void pmsg(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // "Parent message", uses the scope of the caller
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF::API, NULL, Message, arg);
         va_end(arg);
      }

      void warning(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF::WARNING, header, Message, arg);
         va_end(arg);
      }

      void error(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // NB: Use for messages intended for the user, not the developer
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF::ERROR, header, Message, arg);
         va_end(arg);
      }

      void function(CSTRING Message = "", ...) __attribute__((format(printf, 2, 3))) { // Equivalent to branch() but without a new branch being created
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF::API|VLF::FUNCTION, header, Message, arg);
         va_end(arg);
      }

      inline ERR error(ERR Code) { // Technically a warning
         FuncError(header, Code);
         return Code;
      }

      inline ERR warning(ERR Code) {
         FuncError(header, Code);
         return Code;
      }

      void trace(CSTRING Message, ...) {
         #ifdef _DEBUG
            va_list arg;
            va_start(arg, Message);
            VLogF(VLF::TRACE, header, Message, arg);
            va_end(arg);
         #endif
      }

      void traceWarning(CSTRING Message, ...) {
         #ifdef _DEBUG
            va_list arg;
            va_start(arg, Message);
            VLogF(VLF::WARNING, header, Message, arg);
            va_end(arg);
         #endif
      }

      inline ERR traceWarning(ERR Code) {
         #ifdef _DEBUG
            FuncError(header, Code);
         #endif
         return Code;
      }
};

#pragma GCC diagnostic pop

class LogLevel {
   private:
      LONG level;
   public:
      LogLevel(LONG Level) : level(Level) {
         AdjustLogLevel(Level);
      }

      ~LogLevel() {
         AdjustLogLevel(-level);
      }
};

} // namespace

//********************************************************************************************************************
// Refer to Object->get() to see what this is about...

template <class T> inline LARGE FIELD_TAG()     { return 0; }
template <> inline LARGE FIELD_TAG<DOUBLE>()    { return TDOUBLE; }
template <> inline LARGE FIELD_TAG<LONG>()      { return TLONG; }
template <> inline LARGE FIELD_TAG<FLOAT>()     { return TFLOAT; }
template <> inline LARGE FIELD_TAG<OBJECTPTR>() { return TPTR; }
template <> inline LARGE FIELD_TAG<APTR>()      { return TPTR; }
template <> inline LARGE FIELD_TAG<LARGE>()     { return TLARGE; }
template <> inline LARGE FIELD_TAG<CSTRING>()   { return TSTRING; }
template <> inline LARGE FIELD_TAG<STRING>()    { return TSTRING; }
template <> inline LARGE FIELD_TAG<SCALE>()     { return TDOUBLE|TSCALE; }

//********************************************************************************************************************
// Header used for all objects.

struct Object { // Must be 64-bit aligned
   union {
      objMetaClass *Class;          // [Public] Class pointer
      class extMetaClass *ExtClass; // [Private] Internal version of the class pointer
   };
   APTR     ChildPrivate;        // Address for the ChildPrivate structure, if allocated
   APTR     CreatorMeta;         // The creator of the object is permitted to store a custom data pointer here.
   struct Object *Owner;         // The owner of this object
   std::atomic_uint64_t NotifyFlags; // Action subscription flags - space for 64 actions max
   std::atomic_uchar ThreadPending; // AsyncAction() increments this.
   std::atomic_char Queue;       // Counter of locks attained by LockObject(); decremented by ReleaseObject()
   std::atomic_char SleepQueue;  // For the use of LockObject() only
   BYTE ActionDepth;             // Incremented each time an action or method is called on the object
   OBJECTID UID;                 // Unique object identifier
   NF       Flags;               // Object flags
   std::atomic_int ThreadID;     // Managed by locking functions.  Atomic due to volatility.
   char Name[MAX_NAME_LEN];      // The name of the object.  NOTE: This value can be adjusted to ensure that the struct is always 8-bit aligned.
   bool PermitTerminate;

   inline bool initialised() { return (Flags & NF::INITIALISED) != NF::NIL; }
   inline bool defined(NF pFlags) { return (Flags & pFlags) != NF::NIL; }
   inline bool isSubClass();
   inline OBJECTID ownerID() { return Owner ? Owner->UID : 0; }
   inline CLASSID classID();
   inline CLASSID baseClassID();
   inline NF flags() { return Flags; }

   CSTRING className();

   inline bool collecting() { // Is object being freed or marked for collection?
      return (Flags & (NF::FREE|NF::COLLECT|NF::FREE_ON_UNLOCK)) != NF::NIL;
   }

   inline bool terminating() { // Is object currently being freed?
      return (Flags & NF::FREE) != NF::NIL;
   }

   // Use lock() to quickly obtain an object lock without a call to LockObject().  Can fail if the object is being collected.

   inline ERR lock(LONG Timeout = -1) {
      if (++Queue IS 1) {
         ThreadID = pf::_get_thread_id();
         return ERR::Okay;
      }
      else {
         if (ThreadID IS pf::_get_thread_id()) return ERR::Okay; // If this is for the same thread then it's a nested lock, so there's no issue.
         --Queue; // Restore the lock count
         return LockObject(this, Timeout); // Can fail if object is marked for collection.
      }
   }

   inline void unlock() {
      // Prefer to use ReleaseObject() if there are threads that need to be woken
      if ((SleepQueue > 0) or defined(NF::FREE_ON_UNLOCK)) ReleaseObject(this);
      else --Queue;
   }

   inline bool hasOwner(OBJECTID ID) { // Return true if ID has ownership.
      auto obj = this->Owner;
      while ((obj) and (obj->UID != ID)) obj = obj->Owner;
      return obj ? true : false;
   }

   template <class T, std::size_t SIZE> ERR set(FIELD FieldID, const std::array<T, SIZE> &Value) {
      return SetArray(this, FieldID|FIELD_TAG<T>(), const_cast<T *>(Value.data()), SIZE);
   }

   template <class T> ERR set(FIELD FieldID, const std::vector<T> &Value) {
      return SetArray(this, FieldID|FIELD_TAG<T>(), const_cast<T *>(Value.data()), std::ssize(Value));
   }

   inline ERR set(FIELD FieldID, const FRGB &Value)     { return SetArray(this, FieldID|TFLOAT, (FLOAT *)&Value, 4); }
   inline ERR set(FIELD FieldID, int Value)             { return SetField(this, FieldID|TLONG, Value); }
   inline ERR set(FIELD FieldID, long Value)            { return SetField(this, FieldID|TLONG, Value); }
   inline ERR set(FIELD FieldID, unsigned int Value)    { return SetField(this, FieldID|TLONG, Value); }
   inline ERR set(FIELD FieldID, LARGE Value)           { return SetField(this, FieldID|TLARGE, Value); }
   inline ERR set(FIELD FieldID, DOUBLE Value)          { return SetField(this, FieldID|TDOUBLE, Value); }
   inline ERR set(FIELD FieldID, const FUNCTION *Value) { return SetField(this, FieldID|TFUNCTION, Value); }
   inline ERR set(FIELD FieldID, const char *Value)     { return SetField(this, FieldID|TSTRING, Value); }
   inline ERR set(FIELD FieldID, const unsigned char *Value) { return SetField(this, FieldID|TSTRING, Value); }
   inline ERR set(FIELD FieldID, const std::string &Value)   { return SetField(this, FieldID|TSTRING, Value.c_str()); }
   inline ERR set(FIELD FieldID, const Unit *Value)          { return SetField(this, FieldID|TUNIT, Value); }
   // Works both for regular data pointers and function pointers if field is defined correctly.
   inline ERR set(FIELD FieldID, const void *Value) { return SetField(this, FieldID|TPTR, Value); }

   inline ERR setScale(FIELD FieldID, DOUBLE Value) { return SetField(this, FieldID|TDOUBLE|TSCALE, Value); }

   // There are two mechanisms for retrieving object values; the first allows the value to be retrieved with an error
   // code and the value itself; the second ignores the error code and returns a value that could potentially be invalid.

   inline ERR get(FIELD FieldID, LONG *Value)     { return GetField(this, FieldID|TLONG, Value); }
   inline ERR get(FIELD FieldID, LARGE *Value)    { return GetField(this, FieldID|TLARGE, Value); }
   inline ERR get(FIELD FieldID, DOUBLE *Value)   { return GetField(this, FieldID|TDOUBLE, Value); }
   inline ERR get(FIELD FieldID, STRING *Value)   { return GetField(this, FieldID|TSTRING, Value); }
   inline ERR get(FIELD FieldID, CSTRING *Value)  { return GetField(this, FieldID|TSTRING, Value); }
   inline ERR get(FIELD FieldID, Unit *Value)     { return GetField(this, FieldID|TUNIT, Value); }
   inline ERR getPtr(FIELD FieldID, APTR Value)   { return GetField(this, FieldID|TPTR, Value); }
   inline ERR getScale(FIELD FieldID, DOUBLE *Value) { return GetField(this, FieldID|TDOUBLE|TSCALE, Value); }

   template <class T> inline T get(FIELD FieldID) { // Validity of the result is not guaranteed
      T val(T(0));
      GetField(this, FieldID|FIELD_TAG<T>(), &val);
      return val;
   };

   template <typename... Args> ERR setFields(Args&&... pFields) {
      pf::Log log("setFields");

      std::initializer_list<pf::FieldValue> Fields = { std::forward<Args>(pFields)... };

      auto ctx = CurrentContext();
      for (auto &f : Fields) {
         OBJECTPTR target;
         if (auto field = FindField(this, f.FieldID, &target)) {
            if ((!(field->Flags & (FD_INIT|FD_WRITE))) and (ctx != target)) {
               log.warning("%s.%s is immutable.", className(), field->Name);
            }
            else if ((field->Flags & FD_INIT) and (target->initialised()) and (ctx != target)) {
               log.warning("%s.%s is init-only.", className(), field->Name);
            }
            else {
               if (target != this) target->lock();

               ERR error;
               if (f.Type & (FD_POINTER|FD_STRING|FD_ARRAY|FD_FUNCTION|FD_UNIT)) {
                  error = field->WriteValue(target, field, f.Type, f.Pointer, 0);
               }
               else if (f.Type & (FD_DOUBLE|FD_FLOAT)) {
                  error = field->WriteValue(target, field, f.Type, &f.Double, 1);
               }
               else if (f.Type & FD_LARGE) {
                  error = field->WriteValue(target, field, f.Type, &f.Large, 1);
               }
               else error = field->WriteValue(target, field, f.Type, &f.Long, 1);

               if (target != this) target->unlock();

               // NB: NoSupport is considered a 'soft' error that does not warrant failure.

               if ((error != ERR::Okay) and (error != ERR::NoSupport)) {
                  log.warning("%s.%s: %s", target->className(), field->Name, GetErrorMsg(error));
                  return error;
               }
            }
         }
         else return log.warning(ERR::UnsupportedField);
      }

      return ERR::Okay;
   }

} __attribute__ ((aligned (8)));

namespace pf {

template<class T = Object>
class Create {
   private:
      T *obj;

   public:
      ERR error;

      // Return an unscoped direct object pointer.  NB: Globals are still tracked to their owner; use untracked() if
      // you don't want this.

      template <typename... Args> static T * global(Args&&... Fields) {
         pf::Create<T> object = { std::forward<Args>(Fields)... };
         if (object.ok()) {
            auto result = *object;
            object.obj = NULL;
            return result;
         }
         else return NULL;
      }

      inline static T * global(const std::initializer_list<FieldValue> Fields) {
         pf::Create<T> object(Fields);
         if (object.ok()) {
            auto result = *object;
            object.obj = NULL;
            return result;
         }
         else return NULL;
      }

      // Return an unscoped local object (suitable for class allocations only).

      template <typename... Args> static T * local(Args&&... Fields) {
         pf::Create<T> object({ std::forward<Args>(Fields)... }, NF::LOCAL);
         if (object.ok()) return *object;
         else return NULL;
      }

      inline static T * local(const std::initializer_list<FieldValue> Fields) {
         pf::Create<T> object(Fields, NF::LOCAL);
         if (object.ok()) return *object;
         else return NULL;
      }

      // Return an unscoped and untracked object pointer.

      template <typename... Args> static T * untracked(Args&&... Fields) {
         pf::Create<T> object({ std::forward<Args>(Fields)... }, NF::UNTRACKED);
         if (object.ok()) return *object;
         else return NULL;
      }

      inline static T * untracked(const std::initializer_list<FieldValue> Fields) {
         pf::Create<T> object(Fields, NF::UNTRACKED);
         if (object.ok()) return *object;
         else return NULL;
      }

      // Create a scoped object that is not initialised.

      Create(NF Flags = NF::NIL) : obj(NULL), error(ERR::NewObject) {
         if (NewObject(T::CLASS_ID, Flags, (Object **)&obj) IS ERR::Okay) {
            error = ERR::Okay;
         }
      }

      // Create a scoped object that is fully initialised.

      Create(const std::initializer_list<FieldValue> Fields, NF Flags = NF::NIL) : obj(NULL), error(ERR::Failed) {
         pf::Log log("CreateObject");
         log.branch(T::CLASS_NAME);

         if (NewObject(T::CLASS_ID, NF::SUPPRESS_LOG|Flags, (Object **)&obj) IS ERR::Okay) {
            for (auto &f : Fields) {
               OBJECTPTR target;
               if (auto field = FindField(obj, f.FieldID, &target)) {
                  if (!(field->Flags & (FD_WRITE|FD_INIT))) {
                     error = log.warning(ERR::NoFieldAccess);
                     return;
                  }
                  else {
                     target->lock();

                     if (f.Type & (FD_POINTER|FD_STRING|FD_ARRAY|FD_FUNCTION|FD_UNIT)) {
                        error = field->WriteValue(target, field, f.Type, f.Pointer, 0);
                     }
                     else if (f.Type & (FD_DOUBLE|FD_FLOAT)) {
                        error = field->WriteValue(target, field, f.Type, &f.Double, 1);
                     }
                     else if (f.Type & FD_LARGE) {
                        error = field->WriteValue(target, field, f.Type, &f.Large, 1);
                     }
                     else error = field->WriteValue(target, field, f.Type, &f.Long, 1);

                     target->unlock();

                     // NB: NoSupport is considered a 'soft' error that does not warrant failure.

                     if ((error != ERR::Okay) and (error != ERR::NoSupport)) return;
                  }
               }
               else {
                  log.warning("%s.%s field not supported.", T::CLASS_NAME, FieldName(f.FieldID));
                  error = log.warning(ERR::UnsupportedField);
                  return;
               }
            }

            if ((error = InitObject(obj)) != ERR::Okay) {
               FreeResource(obj->UID);
               obj = NULL;
            }
         }
         else error = ERR::NewObject;
      }

      ~Create() {
         if (obj) {
            if (obj->initialised()) {
               if ((obj->Object::Flags & (NF::UNTRACKED|NF::LOCAL)) != NF::NIL)  {
                  return; // Detected a successfully created unscoped object
               }
            }
            FreeResource(obj->UID);
            obj = NULL;
         }
      }

      T * operator->() { return obj; }; // Promotes underlying methods and fields
      T * & operator*() { return obj; }; // To allow object pointer referencing when calling functions

      inline bool ok() { return error == ERR::Okay; }
};
}

inline OBJECTID CurrentTaskID() { return ((OBJECTPTR)CurrentTask())->UID; }
inline APTR SetResourcePtr(RES Res, APTR Value) { return (APTR)(MAXINT)(SetResource(Res, (MAXINT)Value)); }

// Action and Notification Structures

struct acClipboard     { static const AC id = AC::Clipboard; CLIPMODE Mode; };
struct acCopyData      { static const AC id = AC::CopyData; OBJECTPTR Dest; };
struct acDataFeed      { static const AC id = AC::DataFeed; OBJECTPTR Object; DATA Datatype; const void *Buffer; LONG Size; };
struct acDragDrop      { static const AC id = AC::DragDrop; OBJECTPTR Source; LONG Item; CSTRING Datatype; };
struct acDraw          { static const AC id = AC::Draw; LONG X; LONG Y; LONG Width; LONG Height; };
struct acGetKey        { static const AC id = AC::GetKey; CSTRING Key; STRING Value; LONG Size; };
struct acMove          { static const AC id = AC::Move; DOUBLE DeltaX; DOUBLE DeltaY; DOUBLE DeltaZ; };
struct acMoveToPoint   { static const AC id = AC::MoveToPoint; DOUBLE X; DOUBLE Y; DOUBLE Z; MTF Flags; };
struct acNewChild      { static const AC id = AC::NewChild; OBJECTPTR Object; };
struct acNewOwner      { static const AC id = AC::NewOwner; OBJECTPTR NewOwner; };
struct acRead          { static const AC id = AC::Read; APTR Buffer; LONG Length; LONG Result; };
struct acRedimension   { static const AC id = AC::Redimension; DOUBLE X; DOUBLE Y; DOUBLE Z; DOUBLE Width; DOUBLE Height; DOUBLE Depth; };
struct acRedo          { static const AC id = AC::Redo; LONG Steps; };
struct acRename        { static const AC id = AC::Rename; CSTRING Name; };
struct acResize        { static const AC id = AC::Resize; DOUBLE Width; DOUBLE Height; DOUBLE Depth; };
struct acSaveImage     { static const AC id = AC::SaveImage; OBJECTPTR Dest; union { CLASSID ClassID; CLASSID Class; }; };
struct acSaveToObject  { static const AC id = AC::SaveToObject; OBJECTPTR Dest; union { CLASSID ClassID; CLASSID Class; }; };
struct acSeek          { static const AC id = AC::Seek; DOUBLE Offset; SEEK Position; };
struct acSetKey        { static const AC id = AC::SetKey; CSTRING Key; CSTRING Value; };
struct acUndo          { static const AC id = AC::Undo; LONG Steps; };
struct acWrite         { static const AC id = AC::Write; CPTR Buffer; LONG Length; LONG Result; };

// Action Macros

inline ERR acActivate(OBJECTPTR Object) { return Action(AC::Activate,Object,NULL); }
inline ERR acClear(OBJECTPTR Object) { return Action(AC::Clear,Object,NULL); }
inline ERR acDeactivate(OBJECTPTR Object) { return Action(AC::Deactivate,Object,NULL); }
inline ERR acDisable(OBJECTPTR Object) { return Action(AC::Disable,Object,NULL); }
inline ERR acDraw(OBJECTPTR Object) { return Action(AC::Draw,Object,NULL); }
inline ERR acEnable(OBJECTPTR Object) { return Action(AC::Enable,Object,NULL); }
inline ERR acFlush(OBJECTPTR Object) { return Action(AC::Flush,Object,NULL); }
inline ERR acFocus(OBJECTPTR Object) { return Action(AC::Focus,Object,NULL); }
inline ERR acHide(OBJECTPTR Object) { return Action(AC::Hide,Object,NULL); }
inline ERR acLock(OBJECTPTR Object) { return Action(AC::Lock,Object,NULL); }
inline ERR acLostFocus(OBJECTPTR Object) { return Action(AC::LostFocus,Object,NULL); }
inline ERR acMoveToBack(OBJECTPTR Object) { return Action(AC::MoveToBack,Object,NULL); }
inline ERR acMoveToFront(OBJECTPTR Object) { return Action(AC::MoveToFront,Object,NULL); }
inline ERR acNext(OBJECTPTR Object) { return Action(AC::Next,Object,NULL); }
inline ERR acPrev(OBJECTPTR Object) { return Action(AC::Prev,Object,NULL); }
inline ERR acQuery(OBJECTPTR Object) { return Action(AC::Query,Object,NULL); }
inline ERR acRefresh(OBJECTPTR Object) { return Action(AC::Refresh, Object, NULL); }
inline ERR acReset(OBJECTPTR Object) { return Action(AC::Reset,Object,NULL); }
inline ERR acSaveSettings(OBJECTPTR Object) { return Action(AC::SaveSettings,Object,NULL); }
inline ERR acShow(OBJECTPTR Object) { return Action(AC::Show,Object,NULL); }
inline ERR acSignal(OBJECTPTR Object) { return Action(AC::Signal,Object,NULL); }
inline ERR acUnlock(OBJECTPTR Object) { return Action(AC::Unlock,Object,NULL); }

inline ERR acClipboard(OBJECTPTR Object, CLIPMODE Mode) {
   struct acClipboard args = { Mode };
   return Action(AC::Clipboard, Object, &args);
}

inline ERR acDragDrop(OBJECTPTR Object, OBJECTPTR Source, LONG Item, CSTRING Datatype) {
   struct acDragDrop args = { Source, Item, Datatype };
   return Action(AC::DragDrop, Object, &args);
}

inline ERR acDrawArea(OBJECTPTR Object, LONG X, LONG Y, LONG Width, LONG Height) {
   struct acDraw args = { X, Y, Width, Height };
   return Action(AC::Draw, Object, &args);
}

inline ERR acDataFeed(OBJECTPTR Object, OBJECTPTR Sender, DATA Datatype, const void *Buffer, LONG Size) {
   struct acDataFeed args = { Sender, Datatype, Buffer, Size };
   return Action(AC::DataFeed, Object, &args);
}

inline ERR acGetKey(OBJECTPTR Object, CSTRING Key, STRING Value, LONG Size) {
   struct acGetKey args = { Key, Value, Size };
   ERR error = Action(AC::GetKey, Object, &args);
   if ((error != ERR::Okay) and (Value)) Value[0] = 0;
   return error;
}

inline ERR acMove(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Z) {
   struct acMove args = { X, Y, Z };
   return Action(AC::Move, Object, &args);
}

inline ERR acRead(OBJECTPTR Object, APTR Buffer, LONG Bytes, LONG *Read) {
   struct acRead read = { (BYTE *)Buffer, Bytes };
   if (auto error = Action(AC::Read, Object, &read); error IS ERR::Okay) {
      if (Read) *Read = read.Result;
      return ERR::Okay;
   }
   else {
      if (Read) *Read = 0;
      return error;
   }
}

inline ERR acRedo(OBJECTPTR Object, LONG Steps = 1) {
   struct acRedo args = { Steps };
   return Action(AC::Redo, Object, &args);
}

inline ERR acRedimension(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Z, DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
   struct acRedimension args = { X, Y, Z, Width, Height, Depth };
   return Action(AC::Redimension, Object, &args);
}

inline ERR acRename(OBJECTPTR Object, CSTRING Name) {
   struct acRename args = { Name };
   return Action(AC::Rename, Object, &args);
}

inline ERR acResize(OBJECTPTR Object, DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
   struct acResize args = { Width, Height, Depth };
   return Action(AC::Resize, Object, &args);
}

inline ERR acMoveToPoint(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Z, MTF Flags) {
   struct acMoveToPoint moveto = { X, Y, Z, Flags };
   return Action(AC::MoveToPoint, Object, &moveto);
}

inline ERR acSaveImage(OBJECTPTR Object, OBJECTPTR Dest, CLASSID ClassID = CLASSID::NIL) {
   struct acSaveImage args = { Dest, { ClassID } };
   return Action(AC::SaveImage, Object, &args);
}

inline ERR acSaveToObject(OBJECTPTR Object, OBJECTPTR Dest, CLASSID ClassID = CLASSID::NIL) {
   struct acSaveToObject args = { Dest, { ClassID } };
   return Action(AC::SaveToObject, Object, &args);
}

inline ERR acSeek(OBJECTPTR Object, DOUBLE Offset, SEEK Position) {
   struct acSeek args = { Offset, Position };
   return Action(AC::Seek, Object, &args);
}

inline ERR acSetKeys(OBJECTPTR Object, CSTRING tags, ...) {
   struct acSetKey args;
   va_list list;

   va_start(list, tags);
   while ((args.Key = va_arg(list, STRING)) != TAGEND) {
      args.Value = va_arg(list, STRING);
      if (Action(AC::SetKey, Object, &args) != ERR::Okay) {
         va_end(list);
         return ERR::Failed;
      }
   }
   va_end(list);
   return ERR::Okay;
}

inline ERR acUndo(OBJECTPTR Object, LONG Steps) {
   struct acUndo args = { Steps };
   return Action(AC::Undo, Object, &args);
}

inline ERR acWrite(OBJECTPTR Object, CPTR Buffer, LONG Bytes, LONG *Result = NULL) {
   struct acWrite write = { (BYTE *)Buffer, Bytes };
   if (auto error = Action(AC::Write, Object, &write); error IS ERR::Okay) {
      if (Result) *Result = write.Result;
      return error;
   }
   else {
      if (Result) *Result = 0;
      return error;
   }
}

inline LONG acWriteResult(OBJECTPTR Object, CPTR Buffer, LONG Bytes) {
   struct acWrite write = { (BYTE *)Buffer, Bytes };
   if (Action(AC::Write, Object, &write) IS ERR::Okay) return write.Result;
   else return 0;
}

#define acSeekStart(a,b)    acSeek((a),(b),SEEK::START)
#define acSeekEnd(a,b)      acSeek((a),(b),SEEK::END)
#define acSeekCurrent(a,b)  acSeek((a),(b),SEEK::CURRENT)

inline ERR acSetKey(OBJECTPTR Object, CSTRING Key, CSTRING Value) {
   struct acSetKey args = { Key, Value };
   return Action(AC::SetKey, Object, &args);
}


// MetaClass class definition

#define VER_METACLASS (1.000000)

// MetaClass methods

namespace mc {
struct FindField { LONG ID; struct Field * Field; objMetaClass * Source; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objMetaClass : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::METACLASS;
   static constexpr CSTRING CLASS_NAME = "MetaClass";

   using create = pf::Create<objMetaClass>;

   DOUBLE  ClassVersion;                // The version number of the class.
   const struct FieldArray * Fields;    // Points to a FieldArray that describes the class' object structure.
   struct Field * Dictionary;           // Returns a field lookup table sorted by field IDs.
   CSTRING ClassName;                   // The name of the represented class.
   CSTRING FileExtension;               // Describes the file extension represented by the class.
   CSTRING FileDescription;             // Describes the file type represented by the class.
   CSTRING FileHeader;                  // Defines a string expression that will allow relevant file data to be matched to the class.
   CSTRING Path;                        // The path to the module binary that represents the class.
   CSTRING Icon;                        // Associates an icon with the file data for this class.
   LONG    Size;                        // The total size of the object structure represented by the MetaClass.
   CLF     Flags;                       // Optional flag settings.
   CLASSID ClassID;                     // Specifies the ID of a class object.
   CLASSID BaseClassID;                 // Specifies the base class ID of a class object.
   LONG    OpenCount;                   // The total number of active objects that are linked back to the MetaClass.
   CCF     Category;                    // The system category that a class belongs to.
   inline ERR findField(LONG ID, struct Field ** Field, objMetaClass ** Source) noexcept {
      struct mc::FindField args = { ID, (struct Field *)0, (objMetaClass *)0 };
      ERR error = Action(AC(-1), this, &args);
      if (Field) *Field = args.Field;
      if (Source) *Source = args.Source;
      return(error);
   }

   // Customised field setting

   inline ERR setClassVersion(const DOUBLE Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->ClassVersion = Value;
      return ERR::Okay;
   }

   inline ERR setFields(const struct FieldArray * Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[24];
      return field->WriteValue(target, field, 0x00001510, Value, Elements);
   }

   template <class T> inline ERR setClassName(T && Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->ClassName = Value;
      return ERR::Okay;
   }

   template <class T> inline ERR setFileExtension(T && Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->FileExtension = Value;
      return ERR::Okay;
   }

   template <class T> inline ERR setFileDescription(T && Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->FileDescription = Value;
      return ERR::Okay;
   }

   template <class T> inline ERR setFileHeader(T && Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->FileHeader = Value;
      return ERR::Okay;
   }

   template <class T> inline ERR setPath(T && Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Path = Value;
      return ERR::Okay;
   }

   inline ERR setSize(const LONG Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Size = Value;
      return ERR::Okay;
   }

   inline ERR setFlags(const CLF Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setClass(const CLASSID Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->ClassID = Value;
      return ERR::Okay;
   }

   inline ERR setBaseClass(const CLASSID Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->BaseClassID = Value;
      return ERR::Okay;
   }

   inline ERR setCategory(const CCF Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Category = Value;
      return ERR::Okay;
   }

   inline ERR setMethods(const APTR Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[17];
      return field->WriteValue(target, field, 0x00001510, Value, Elements);
   }

   inline ERR setActions(APTR Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x08000400, Value, 1);
   }

   template <class T> inline ERR setName(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[11];
      return field->WriteValue(target, field, 0x08810500, to_cstring(Value), 1);
   }

};

inline bool Object::isSubClass() { return Class->ClassID != Class->BaseClassID; }
inline CLASSID Object::classID() { return Class->ClassID; }
inline CLASSID Object::baseClassID() { return Class->BaseClassID; }

// StorageDevice class definition

#define VER_STORAGEDEVICE (1.000000)

class objStorageDevice : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::STORAGEDEVICE;
   static constexpr CSTRING CLASS_NAME = "StorageDevice";

   using create = pf::Create<objStorageDevice>;

   DEVICE DeviceFlags;    // These read-only flags identify the type of device and its features.
   LARGE  DeviceSize;     // The storage size of the device in bytes, without accounting for the file system format.
   LARGE  BytesFree;      // Total amount of storage space that is available, measured in bytes.
   LARGE  BytesUsed;      // Total amount of storage space in use.

   // Customised field setting

   template <class T> inline ERR setVolume(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[4];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

};

// File class definition

#define VER_FILE (1.200000)

// File methods

namespace fl {
struct StartStream { OBJECTID SubscriberID; FL Flags; LONG Length; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct StopStream { static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Delete { FUNCTION * Callback; static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Move { CSTRING Dest; FUNCTION * Callback; static const AC id = AC(-4); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Copy { CSTRING Dest; FUNCTION * Callback; static const AC id = AC(-5); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SetDate { LONG Year; LONG Month; LONG Day; LONG Hour; LONG Minute; LONG Second; FDT Type; static const AC id = AC(-6); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct ReadLine { STRING Result; static const AC id = AC(-7); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct BufferContent { static const AC id = AC(-8); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Next { objFile * File; static const AC id = AC(-9); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Watch { FUNCTION * Callback; LARGE Custom; MFF Flags; static const AC id = AC(-10); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objFile : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::FILE;
   static constexpr CSTRING CLASS_NAME = "File";

   using create = pf::Create<objFile>;

   LARGE    Position; // The current read/write byte position in a file.
   FL       Flags;    // File flags and options.
   LONG     Static;   // Set to true if a file object should be static.
   OBJECTID TargetID; // Specifies a surface ID to target for user feedback and dialog boxes.
   BYTE *   Buffer;   // Points to the internal data buffer if the file content is held in memory.
   public:
   inline CSTRING readLine() {
      struct fl::ReadLine args;
      if (Action(fl::ReadLine::id, this, &args) IS ERR::Okay) return args.Result;
      else return NULL;
   }

   // Action stubs

   inline ERR activate() noexcept { return Action(AC::Activate, this, NULL); }
   inline ERR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, LONG Size) noexcept {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC::DataFeed, this, &args);
   }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR query() noexcept { return Action(AC::Query, this, NULL); }
   template <class T, class U> ERR read(APTR Buffer, T Size, U *Result) noexcept {
      static_assert(std::is_integral<U>::value, "Result value must be an integer type");
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      if (auto error = Action(AC::Read, this, &read); error IS ERR::Okay) {
         *Result = static_cast<U>(read.Result);
         return ERR::Okay;
      }
      else { *Result = 0; return error; }
   }
   template <class T> ERR read(APTR Buffer, T Size) noexcept {
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      return Action(AC::Read, this, &read);
   }
   inline ERR rename(CSTRING Name) noexcept {
      struct acRename args = { Name };
      return Action(AC::Rename, this, &args);
   }
   inline ERR reset() noexcept { return Action(AC::Reset, this, NULL); }
   inline ERR seek(DOUBLE Offset, SEEK Position = SEEK::CURRENT) noexcept {
      struct acSeek args = { Offset, Position };
      return Action(AC::Seek, this, &args);
   }
   inline ERR seekStart(DOUBLE Offset) noexcept { return seek(Offset, SEEK::START); }
   inline ERR seekEnd(DOUBLE Offset) noexcept { return seek(Offset, SEEK::END); }
   inline ERR seekCurrent(DOUBLE Offset) noexcept { return seek(Offset, SEEK::CURRENT); }
   inline ERR write(CPTR Buffer, LONG Size, LONG *Result = NULL) noexcept {
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (auto error = Action(AC::Write, this, &write); error IS ERR::Okay) {
         if (Result) *Result = write.Result;
         return ERR::Okay;
      }
      else {
         if (Result) *Result = 0;
         return error;
      }
   }
   inline ERR write(std::string Buffer, LONG *Result = NULL) noexcept {
      struct acWrite write = { (BYTE *)Buffer.c_str(), LONG(Buffer.size()) };
      if (auto error = Action(AC::Write, this, &write); error IS ERR::Okay) {
         if (Result) *Result = write.Result;
         return ERR::Okay;
      }
      else {
         if (Result) *Result = 0;
         return error;
      }
   }
   inline LONG writeResult(CPTR Buffer, LONG Size) noexcept {
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (Action(AC::Write, this, &write) IS ERR::Okay) return write.Result;
      else return 0;
   }
   inline ERR startStream(OBJECTID SubscriberID, FL Flags, LONG Length) noexcept {
      struct fl::StartStream args = { SubscriberID, Flags, Length };
      return(Action(AC(-1), this, &args));
   }
   inline ERR stopStream() noexcept {
      return(Action(AC(-2), this, NULL));
   }
   inline ERR del(FUNCTION Callback) noexcept {
      struct fl::Delete args = { &Callback };
      return(Action(AC(-3), this, &args));
   }
   inline ERR move(CSTRING Dest, FUNCTION Callback) noexcept {
      struct fl::Move args = { Dest, &Callback };
      return(Action(AC(-4), this, &args));
   }
   inline ERR copy(CSTRING Dest, FUNCTION Callback) noexcept {
      struct fl::Copy args = { Dest, &Callback };
      return(Action(AC(-5), this, &args));
   }
   inline ERR setDate(LONG Year, LONG Month, LONG Day, LONG Hour, LONG Minute, LONG Second, FDT Type) noexcept {
      struct fl::SetDate args = { Year, Month, Day, Hour, Minute, Second, Type };
      return(Action(AC(-6), this, &args));
   }
   inline ERR readLine(STRING * Result) noexcept {
      struct fl::ReadLine args = { (STRING)0 };
      ERR error = Action(AC(-7), this, &args);
      if (Result) *Result = args.Result;
      return(error);
   }
   inline ERR bufferContent() noexcept {
      return(Action(AC(-8), this, NULL));
   }
   inline ERR next(objFile ** File) noexcept {
      struct fl::Next args = { (objFile *)0 };
      ERR error = Action(AC(-9), this, &args);
      if (File) *File = args.File;
      return(error);
   }
   inline ERR watch(FUNCTION Callback, LARGE Custom, MFF Flags) noexcept {
      struct fl::Watch args = { &Callback, Custom, Flags };
      return(Action(AC(-10), this, &args));
   }

   // Customised field setting

   inline ERR setPosition(const LARGE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, FD_LARGE, &Value, 1);
   }

   inline ERR setFlags(const FL Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setStatic(const LONG Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Static = Value;
      return ERR::Okay;
   }

   inline ERR setTarget(OBJECTID Value) noexcept {
      this->TargetID = Value;
      return ERR::Okay;
   }

   inline ERR setDate(APTR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, 0x08000310, Value, 1);
   }

   inline ERR setCreated(APTR Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[20];
      return field->WriteValue(target, field, 0x08000310, Value, 1);
   }

   template <class T> inline ERR setPath(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

   inline ERR setPermissions(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[22];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setSize(const LARGE Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[17];
      return field->WriteValue(target, field, FD_LARGE, &Value, 1);
   }

   template <class T> inline ERR setLink(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERR setUser(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setGroup(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[4];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

};

// Config class definition

#define VER_CONFIG (1.000000)

// Config methods

namespace cfg {
struct ReadValue { CSTRING Group; CSTRING Key; CSTRING Data; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Set { CSTRING Group; CSTRING Key; CSTRING Data; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct WriteValue { CSTRING Group; CSTRING Key; CSTRING Data; static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct DeleteKey { CSTRING Group; CSTRING Key; static const AC id = AC(-4); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct DeleteGroup { CSTRING Group; static const AC id = AC(-5); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct GetGroupFromIndex { LONG Index; CSTRING Group; static const AC id = AC(-6); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SortByKey { CSTRING Key; LONG Descending; static const AC id = AC(-7); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct MergeFile { CSTRING Path; static const AC id = AC(-9); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Merge { OBJECTPTR Source; static const AC id = AC(-10); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objConfig : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::CONFIG;
   static constexpr CSTRING CLASS_NAME = "Config";

   using create = pf::Create<objConfig>;

   STRING Path;         // Set this field to the location of the source configuration file.
   STRING KeyFilter;    // Set this field to enable key filtering.
   STRING GroupFilter;  // Set this field to enable group filtering.
   CNF    Flags;        // Optional flags may be set here.
   public:
   ConfigGroups *Groups;

   // For C++ only, these read variants avoid method calls for speed, but apply identical logic.

   inline ERR read(std::string_view pGroup, std::string_view pKey, DOUBLE &pValue) {
      for (auto& [group, keys] : Groups[0]) {
         if ((!pGroup.empty()) and (group.compare(pGroup))) continue;
         if (pKey.empty()) pValue = strtod(keys.cbegin()->second.c_str(), NULL);
         else if (auto it = keys.find(pKey); it != keys.end()) pValue = strtod(it->second.c_str(), NULL);
         else return ERR::Search;
         return ERR::Okay;
      }
      return ERR::Search;
   }

   inline ERR read(std::string_view pGroup, std::string_view pKey, LONG &pValue) {
      for (auto& [group, keys] : Groups[0]) {
         if ((!pGroup.empty()) and (group.compare(pGroup))) continue;
         if (pKey.empty()) pValue = strtol(keys.cbegin()->second.c_str(), NULL, 0);
         else if (auto it = keys.find(pKey); it != keys.end()) pValue = strtol(it->second.c_str(), NULL, 0);
         else return ERR::Search;
         return ERR::Okay;
      }
      return ERR::Search;
   }

   inline ERR read(std::string_view pGroup, std::string_view pKey, std::string &pValue) {
      for (auto & [group, keys] : Groups[0]) {
         if ((!pGroup.empty()) and (group.compare(pGroup))) continue;
         if (pKey.empty()) pValue = keys.cbegin()->second;
         else if (auto it = keys.find(pKey); it != keys.end()) pValue = it->second;
         else return ERR::Search;
         return ERR::Okay;
      }
      return ERR::Search;
   }

   inline ERR write(std::string_view Group, std::string_view Key, std::string_view Value) {
      ConfigGroups &groups = *Groups;
      for (auto& [group, keys] : groups) {
         if (!group.compare(Group)) {
            if (auto it = keys.find(Key); it != keys.end()) {
               it->second.assign(Value);
            }
            else keys.emplace(Key, Value);
            return ERR::Okay;
         }
      }

      auto &new_group = Groups->emplace_back();
      new_group.first.assign(Group);
      new_group.second.emplace(Key, Value);
      return ERR::Okay;
   }

   // Action stubs

   inline ERR clear() noexcept { return Action(AC::Clear, this, NULL); }
   inline ERR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, LONG Size) noexcept {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC::DataFeed, this, &args);
   }
   inline ERR flush() noexcept { return Action(AC::Flush, this, NULL); }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR saveSettings() noexcept { return Action(AC::SaveSettings, this, NULL); }
   inline ERR saveToObject(OBJECTPTR Dest, CLASSID ClassID = CLASSID::NIL) noexcept {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC::SaveToObject, this, &args);
   }
   inline ERR readValue(CSTRING Group, CSTRING Key, CSTRING * Data) noexcept {
      struct cfg::ReadValue args = { Group, Key, (CSTRING)0 };
      ERR error = Action(AC(-1), this, &args);
      if (Data) *Data = args.Data;
      return(error);
   }
   inline ERR set(CSTRING Group, CSTRING Key, CSTRING Data) noexcept {
      struct cfg::Set args = { Group, Key, Data };
      return(Action(AC(-2), this, &args));
   }
   inline ERR writeValue(CSTRING Group, CSTRING Key, CSTRING Data) noexcept {
      struct cfg::WriteValue args = { Group, Key, Data };
      return(Action(AC(-3), this, &args));
   }
   inline ERR deleteKey(CSTRING Group, CSTRING Key) noexcept {
      struct cfg::DeleteKey args = { Group, Key };
      return(Action(AC(-4), this, &args));
   }
   inline ERR deleteGroup(CSTRING Group) noexcept {
      struct cfg::DeleteGroup args = { Group };
      return(Action(AC(-5), this, &args));
   }
   inline ERR getGroupFromIndex(LONG Index, CSTRING * Group) noexcept {
      struct cfg::GetGroupFromIndex args = { Index, (CSTRING)0 };
      ERR error = Action(AC(-6), this, &args);
      if (Group) *Group = args.Group;
      return(error);
   }
   inline ERR sortByKey(CSTRING Key, LONG Descending) noexcept {
      struct cfg::SortByKey args = { Key, Descending };
      return(Action(AC(-7), this, &args));
   }
   inline ERR mergeFile(CSTRING Path) noexcept {
      struct cfg::MergeFile args = { Path };
      return(Action(AC(-9), this, &args));
   }
   inline ERR merge(OBJECTPTR Source) noexcept {
      struct cfg::Merge args = { Source };
      return(Action(AC(-10), this, &args));
   }

   // Customised field setting

   template <class T> inline ERR setPath(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setKeyFilter(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[3];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setGroupFilter(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[7];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERR setFlags(const CNF Value) noexcept {
      this->Flags = Value;
      return ERR::Okay;
   }

};

// Script class definition

#define VER_SCRIPT (1.000000)

// Script methods

namespace sc {
struct Exec { CSTRING Procedure; const struct ScriptArg * Args; LONG TotalArgs; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct DerefProcedure { FUNCTION * Procedure; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Callback { LARGE ProcedureID; const struct ScriptArg * Args; LONG TotalArgs; ERR Error; static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct GetProcedureID { CSTRING Procedure; LARGE ProcedureID; static const AC id = AC(-4); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objScript : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::SCRIPT;
   static constexpr CSTRING CLASS_NAME = "Script";

   using create = pf::Create<objScript>;

   OBJECTID TargetID;  // Reference to the default container that new script objects will be initialised to.
   SCF      Flags;     // Optional flags.
   ERR      Error;     // If a script fails during execution, an error code may be readable here.
   LONG     CurrentLine; // Indicates the current line being executed when in debug mode.
   LONG     LineOffset; // For debugging purposes, this value is added to any message referencing a line number.

#ifdef PRV_SCRIPT
   LARGE    ProcedureID;          // For callbacks
   KEYVALUE Vars; // Global parameters
   STRING   *Results;
   char     Language[4];          // 3-character language code, null-terminated
   const ScriptArg *ProcArgs;     // Procedure args - applies during Exec
   STRING   Path;                 // File location of the script
   STRING   String;
   STRING   WorkingPath;
   STRING   ErrorString;
   CSTRING  Procedure;
   STRING   CacheFile;
   LONG     ActivationCount;      // Incremented every time the script is activated.
   LONG     ResultsTotal;
   LONG     TotalArgs;            // Total number of ProcArgs
   char     LanguageDir[32];      // Directory to use for language files
   OBJECTID ScriptOwnerID;
#endif

   // Action stubs

   inline ERR activate() noexcept { return Action(AC::Activate, this, NULL); }
   inline ERR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, LONG Size) noexcept {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC::DataFeed, this, &args);
   }
   inline ERR getKey(CSTRING Key, STRING Value, LONG Size) noexcept {
      struct acGetKey args = { Key, Value, Size };
      auto error = Action(AC::GetKey, this, &args);
      if ((error != ERR::Okay) and (Value)) Value[0] = 0;
      return error;
   }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR reset() noexcept { return Action(AC::Reset, this, NULL); }
   inline ERR acSetKey(CSTRING FieldName, CSTRING Value) noexcept {
      struct acSetKey args = { FieldName, Value };
      return Action(AC::SetKey, this, &args);
   }
   inline ERR exec(CSTRING Procedure, const struct ScriptArg * Args, LONG TotalArgs) noexcept {
      struct sc::Exec args = { Procedure, Args, TotalArgs };
      return(Action(AC(-1), this, &args));
   }
   inline ERR derefProcedure(FUNCTION Procedure) noexcept {
      struct sc::DerefProcedure args = { &Procedure };
      return(Action(AC(-2), this, &args));
   }
   inline ERR callback(LARGE ProcedureID, const struct ScriptArg * Args, LONG TotalArgs, ERR * Error) noexcept {
      struct sc::Callback args = { ProcedureID, Args, TotalArgs, (ERR)0 };
      ERR error = Action(AC(-3), this, &args);
      if (Error) *Error = args.Error;
      return(error);
   }
   inline ERR getProcedureID(CSTRING Procedure, LARGE * ProcedureID) noexcept {
      struct sc::GetProcedureID args = { Procedure, (LARGE)0 };
      ERR error = Action(AC(-4), this, &args);
      if (ProcedureID) *ProcedureID = args.ProcedureID;
      return(error);
   }

   // Customised field setting

   inline ERR setTarget(OBJECTID Value) noexcept {
      this->TargetID = Value;
      return ERR::Okay;
   }

   inline ERR setFlags(const SCF Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setLineOffset(const LONG Value) noexcept {
      this->LineOffset = Value;
      return ERR::Okay;
   }

   template <class T> inline ERR setCacheFile(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setErrorString(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setWorkingPath(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[20];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setProcedure(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setName(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, 0x08810300, to_cstring(Value), 1);
   }

   inline ERR setOwner(OBJECTID Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   template <class T> inline ERR setPath(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

   inline ERR setResults(STRING * Value, LONG Elements) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[8];
      return field->WriteValue(target, field, 0x08801300, Value, Elements);
   }

   template <class T> inline ERR setStatement(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

};

namespace sc {
template <std::size_t SIZE> ERR Call(const FUNCTION &Function, const std::array<ScriptArg, SIZE> &Args) noexcept {
   struct Callback args = { Function.ProcedureID, Args.data(), LONG(std::ssize(Args)), ERR::Okay };
   return Action(sc::Callback::id, Function.Context, &args);
}

template <std::size_t SIZE> ERR Call(const FUNCTION &Function, const std::array<ScriptArg, SIZE> &Args, ERR &Result) noexcept {
   struct Callback args = { Function.ProcedureID, Args.data(), LONG(std::ssize(Args)), ERR::Okay };
   ERR error = Action(sc::Callback::id, Function.Context, &args);
   Result = args.Error;
   return(error);
}

inline ERR Call(const FUNCTION &Function) noexcept {
   struct Callback args = { Function.ProcedureID, NULL, 0, ERR::Okay };
   return Action(sc::Callback::id, Function.Context, &args);
}

inline ERR Call(const FUNCTION &Function, ERR &Result) noexcept {
   struct Callback args = { Function.ProcedureID, NULL, 0, ERR::Okay };
   ERR error = Action(sc::Callback::id, Function.Context, &args);
   Result = args.Error;
   return(error);
}
} // namespace
struct ActionEntry {
   ERR (*PerformAction)(OBJECTPTR, APTR);     // Pointer to a custom action hook.
};

// Task class definition

#define VER_TASK (1.000000)

// Task methods

namespace task {
struct Expunge { static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct AddArgument { CSTRING Argument; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Quit { static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct GetEnv { CSTRING Name; CSTRING Value; static const AC id = AC(-4); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct SetEnv { CSTRING Name; CSTRING Value; static const AC id = AC(-5); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objTask : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::TASK;
   static constexpr CSTRING CLASS_NAME = "Task";

   using create = pf::Create<objTask>;

   DOUBLE TimeOut;    // Limits the amount of time to wait for a launched process to return.
   TSF    Flags;      // Optional flags.
   LONG   ReturnCode; // The task's return code can be retrieved following execution.
   LONG   ProcessID;  // Reflects the process ID when an executable is launched.

   // Action stubs

   inline ERR activate() noexcept { return Action(AC::Activate, this, NULL); }
   inline ERR getKey(CSTRING Key, STRING Value, LONG Size) noexcept {
      struct acGetKey args = { Key, Value, Size };
      auto error = Action(AC::GetKey, this, &args);
      if ((error != ERR::Okay) and (Value)) Value[0] = 0;
      return error;
   }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR acSetKey(CSTRING FieldName, CSTRING Value) noexcept {
      struct acSetKey args = { FieldName, Value };
      return Action(AC::SetKey, this, &args);
   }
   inline ERR write(CPTR Buffer, LONG Size, LONG *Result = NULL) noexcept {
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (auto error = Action(AC::Write, this, &write); error IS ERR::Okay) {
         if (Result) *Result = write.Result;
         return ERR::Okay;
      }
      else {
         if (Result) *Result = 0;
         return error;
      }
   }
   inline ERR write(std::string Buffer, LONG *Result = NULL) noexcept {
      struct acWrite write = { (BYTE *)Buffer.c_str(), LONG(Buffer.size()) };
      if (auto error = Action(AC::Write, this, &write); error IS ERR::Okay) {
         if (Result) *Result = write.Result;
         return ERR::Okay;
      }
      else {
         if (Result) *Result = 0;
         return error;
      }
   }
   inline LONG writeResult(CPTR Buffer, LONG Size) noexcept {
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (Action(AC::Write, this, &write) IS ERR::Okay) return write.Result;
      else return 0;
   }
   inline ERR expunge() noexcept {
      return(Action(AC(-1), this, NULL));
   }
   inline ERR addArgument(CSTRING Argument) noexcept {
      struct task::AddArgument args = { Argument };
      return(Action(AC(-2), this, &args));
   }
   inline ERR quit() noexcept {
      return(Action(AC(-3), this, NULL));
   }
   inline ERR getEnv(CSTRING Name, CSTRING * Value) noexcept {
      struct task::GetEnv args = { Name, (CSTRING)0 };
      ERR error = Action(AC(-4), this, &args);
      if (Value) *Value = args.Value;
      return(error);
   }
   inline ERR setEnv(CSTRING Name, CSTRING Value) noexcept {
      struct task::SetEnv args = { Name, Value };
      return(Action(AC(-5), this, &args));
   }

   // Customised field setting

   inline ERR setTimeOut(const DOUBLE Value) noexcept {
      this->TimeOut = Value;
      return ERR::Okay;
   }

   inline ERR setFlags(const TSF Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setReturnCode(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setProcess(const LONG Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->ProcessID = Value;
      return ERR::Okay;
   }

   template <class T> inline ERR setArgs(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[13];
      return field->WriteValue(target, field, 0x08800200, to_cstring(Value), 1);
   }

   inline ERR setParameters(pf::vector<std::string> *Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, 0x08805300, Value, LONG(Value->size()));
   }

   inline ERR setErrorCallback(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERR setExitCallback(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[8];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERR setInputCallback(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[17];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   template <class T> inline ERR setLaunchPath(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setLocation(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERR setName(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERR setOutputCallback(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   template <class T> inline ERR setPath(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERR setPriority(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

};

// Thread class definition

#define VER_THREAD (1.000000)

// Thread methods

namespace th {
struct SetData { APTR Data; LONG Size; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objThread : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::THREAD;
   static constexpr CSTRING CLASS_NAME = "Thread";

   using create = pf::Create<objThread>;

   APTR Data;        // Pointer to initialisation data for the thread.
   LONG DataSize;    // The size of the buffer referenced in the Data field.
   LONG StackSize;   // The stack size to allocate for the thread.
   ERR  Error;       // Reflects the error code returned by the thread routine.
   THF  Flags;       // Optional flags can be defined here.

   // Action stubs

   inline ERR activate() noexcept { return Action(AC::Activate, this, NULL); }
   inline ERR deactivate() noexcept { return Action(AC::Deactivate, this, NULL); }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR setData(APTR Data, LONG Size) noexcept {
      struct th::SetData args = { Data, Size };
      return(Action(AC(-1), this, &args));
   }

   // Customised field setting

   inline ERR setStackSize(const LONG Value) noexcept {
      this->StackSize = Value;
      return ERR::Okay;
   }

   inline ERR setFlags(const THF Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setCallback(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[1];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERR setRoutine(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

};

// Module class definition

#define VER_MODULE (1.000000)

// Module methods

namespace mod {
struct ResolveSymbol { CSTRING Name; APTR Address; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objModule : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::MODULE;
   static constexpr CSTRING CLASS_NAME = "Module";

   using create = pf::Create<objModule>;

   const struct Function * FunctionList;    // Refers to a list of public functions exported by the module.
   APTR ModBase;                            // The Module's function base (jump table) must be read from this field.
   class RootModule * Root;                 // For internal use only.
   struct ModHeader * Header;               // For internal usage only.
   MOF  Flags;                              // Optional flags.
   public:
   static ERR load(std::string Name, OBJECTPTR *Module = NULL, APTR Functions = NULL) {
      if (auto module = objModule::create::global(pf::FieldValue(FID_Name, Name.c_str()))) {
         #ifdef PARASOL_STATIC
            if (Module) *Module = module;
            if (Functions) ((APTR *)Functions)[0] = NULL;
            return ERR::Okay;
         #else
            APTR functionbase;
            if (module->getPtr(FID_ModBase, &functionbase) IS ERR::Okay) {
               if (Module) *Module = module;
               if (Functions) ((APTR *)Functions)[0] = functionbase;
               return ERR::Okay;
            }
            else return ERR::GetField;
         #endif
      }
      else return ERR::CreateObject;
   }

   // Action stubs

   inline ERR init() noexcept { return InitObject(this); }
   inline ERR resolveSymbol(CSTRING Name, APTR * Address) noexcept {
      struct mod::ResolveSymbol args = { Name, (APTR)0 };
      ERR error = Action(AC(-1), this, &args);
      if (Address) *Address = args.Address;
      return(error);
   }

   // Customised field setting

   inline ERR setFunctionList(const struct Function * Value) noexcept {
      this->FunctionList = Value;
      return ERR::Okay;
   }

   inline ERR setHeader(struct ModHeader * Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x08000500, Value, 1);
   }

   inline ERR setFlags(const MOF Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Flags = Value;
      return ERR::Okay;
   }

   template <class T> inline ERR setName(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

};

// Time class definition

#define VER_TIME (1.000000)

// Time methods

namespace pt {
struct SetTime { static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objTime : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::TIME;
   static constexpr CSTRING CLASS_NAME = "Time";

   using create = pf::Create<objTime>;

   LARGE SystemTime;    // Represents the system time when the time object was last queried.
   LONG  Year;          // Year (-ve for BC, +ve for AD).
   LONG  Month;         // Month (1 - 12)
   LONG  Day;           // Day (1 - 31)
   LONG  Hour;          // Hour (0 - 23)
   LONG  Minute;        // Minute (0 - 59)
   LONG  Second;        // Second (0 - 59)
   LONG  TimeZone;      // No information.
   LONG  DayOfWeek;     // Day of week (0 - 6) starting from Sunday.
   LONG  MilliSecond;   // A millisecond is one thousandth of a second (0 - 999)
   LONG  MicroSecond;   // A microsecond is one millionth of a second (0 - 999999)

   // Action stubs

   inline ERR query() noexcept { return Action(AC::Query, this, NULL); }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR setTime() noexcept {
      return(Action(AC(-1), this, NULL));
   }

   // Customised field setting

   inline ERR setSystemTime(const LARGE Value) noexcept {
      this->SystemTime = Value;
      return ERR::Okay;
   }

   inline ERR setYear(const LONG Value) noexcept {
      this->Year = Value;
      return ERR::Okay;
   }

   inline ERR setMonth(const LONG Value) noexcept {
      this->Month = Value;
      return ERR::Okay;
   }

   inline ERR setDay(const LONG Value) noexcept {
      this->Day = Value;
      return ERR::Okay;
   }

   inline ERR setHour(const LONG Value) noexcept {
      this->Hour = Value;
      return ERR::Okay;
   }

   inline ERR setMinute(const LONG Value) noexcept {
      this->Minute = Value;
      return ERR::Okay;
   }

   inline ERR setSecond(const LONG Value) noexcept {
      this->Second = Value;
      return ERR::Okay;
   }

   inline ERR setTimeZone(const LONG Value) noexcept {
      this->TimeZone = Value;
      return ERR::Okay;
   }

   inline ERR setDayOfWeek(const LONG Value) noexcept {
      this->DayOfWeek = Value;
      return ERR::Okay;
   }

   inline ERR setMilliSecond(const LONG Value) noexcept {
      this->MilliSecond = Value;
      return ERR::Okay;
   }

   inline ERR setMicroSecond(const LONG Value) noexcept {
      this->MicroSecond = Value;
      return ERR::Okay;
   }

};

// Compression class definition

#define VER_COMPRESSION (1.000000)

// Compression methods

namespace cmp {
struct CompressBuffer { APTR Input; LONG InputSize; APTR Output; LONG OutputSize; LONG Result; static const AC id = AC(-1); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct CompressFile { CSTRING Location; CSTRING Path; static const AC id = AC(-2); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct DecompressBuffer { APTR Input; APTR Output; LONG OutputSize; LONG Result; static const AC id = AC(-3); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct DecompressFile { CSTRING Path; CSTRING Dest; LONG Flags; static const AC id = AC(-4); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct RemoveFile { CSTRING Path; static const AC id = AC(-5); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct CompressStream { APTR Input; LONG Length; FUNCTION * Callback; APTR Output; LONG OutputSize; static const AC id = AC(-6); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct DecompressStream { APTR Input; LONG Length; FUNCTION * Callback; APTR Output; LONG OutputSize; static const AC id = AC(-7); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct CompressStreamStart { static const AC id = AC(-8); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct CompressStreamEnd { FUNCTION * Callback; APTR Output; LONG OutputSize; static const AC id = AC(-9); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct DecompressStreamEnd { FUNCTION * Callback; static const AC id = AC(-10); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct DecompressStreamStart { static const AC id = AC(-11); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct DecompressObject { CSTRING Path; OBJECTPTR Object; static const AC id = AC(-12); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Scan { CSTRING Folder; CSTRING Filter; FUNCTION * Callback; static const AC id = AC(-13); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };
struct Find { CSTRING Path; LONG CaseSensitive; LONG Wildcard; struct CompressedItem * Item; static const AC id = AC(-14); ERR call(OBJECTPTR Object) { return Action(id, Object, this); } };

} // namespace

class objCompression : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::COMPRESSION;
   static constexpr CSTRING CLASS_NAME = "Compression";

   using create = pf::Create<objCompression>;

   LARGE    TotalOutput;     // The total number of bytes that have been output during the compression or decompression of streamed data.
   OBJECTID OutputID;        // Resulting messages will be sent to the object referred to in this field.
   LONG     CompressionLevel; // The compression level to use when compressing data.
   CMF      Flags;           // Optional flags.
   LONG     SegmentSize;     // Private. Splits the compressed file if it surpasses a set byte limit.
   PERMIT   Permissions;     // Default permissions for decompressed files are defined here.
   LONG     MinOutputSize;   // Indicates the minimum output buffer size that will be needed during de/compression.
   LONG     WindowBits;      // Special option for certain compression formats.

   // Action stubs

   inline ERR flush() noexcept { return Action(AC::Flush, this, NULL); }
   inline ERR init() noexcept { return InitObject(this); }
   inline ERR compressBuffer(APTR Input, LONG InputSize, APTR Output, LONG OutputSize, LONG * Result) noexcept {
      struct cmp::CompressBuffer args = { Input, InputSize, Output, OutputSize, (LONG)0 };
      ERR error = Action(AC(-1), this, &args);
      if (Result) *Result = args.Result;
      return(error);
   }
   inline ERR compressFile(CSTRING Location, CSTRING Path) noexcept {
      struct cmp::CompressFile args = { Location, Path };
      return(Action(AC(-2), this, &args));
   }
   inline ERR decompressBuffer(APTR Input, APTR Output, LONG OutputSize, LONG * Result) noexcept {
      struct cmp::DecompressBuffer args = { Input, Output, OutputSize, (LONG)0 };
      ERR error = Action(AC(-3), this, &args);
      if (Result) *Result = args.Result;
      return(error);
   }
   inline ERR decompressFile(CSTRING Path, CSTRING Dest, LONG Flags) noexcept {
      struct cmp::DecompressFile args = { Path, Dest, Flags };
      return(Action(AC(-4), this, &args));
   }
   inline ERR removeFile(CSTRING Path) noexcept {
      struct cmp::RemoveFile args = { Path };
      return(Action(AC(-5), this, &args));
   }
   inline ERR compressStream(APTR Input, LONG Length, FUNCTION Callback, APTR Output, LONG OutputSize) noexcept {
      struct cmp::CompressStream args = { Input, Length, &Callback, Output, OutputSize };
      return(Action(AC(-6), this, &args));
   }
   inline ERR decompressStream(APTR Input, LONG Length, FUNCTION Callback, APTR Output, LONG OutputSize) noexcept {
      struct cmp::DecompressStream args = { Input, Length, &Callback, Output, OutputSize };
      return(Action(AC(-7), this, &args));
   }
   inline ERR compressStreamStart() noexcept {
      return(Action(AC(-8), this, NULL));
   }
   inline ERR compressStreamEnd(FUNCTION Callback, APTR Output, LONG OutputSize) noexcept {
      struct cmp::CompressStreamEnd args = { &Callback, Output, OutputSize };
      return(Action(AC(-9), this, &args));
   }
   inline ERR decompressStreamEnd(FUNCTION Callback) noexcept {
      struct cmp::DecompressStreamEnd args = { &Callback };
      return(Action(AC(-10), this, &args));
   }
   inline ERR decompressStreamStart() noexcept {
      return(Action(AC(-11), this, NULL));
   }
   inline ERR decompressObject(CSTRING Path, OBJECTPTR Object) noexcept {
      struct cmp::DecompressObject args = { Path, Object };
      return(Action(AC(-12), this, &args));
   }
   inline ERR scan(CSTRING Folder, CSTRING Filter, FUNCTION Callback) noexcept {
      struct cmp::Scan args = { Folder, Filter, &Callback };
      return(Action(AC(-13), this, &args));
   }
   inline ERR find(CSTRING Path, LONG CaseSensitive, LONG Wildcard, struct CompressedItem ** Item) noexcept {
      struct cmp::Find args = { Path, CaseSensitive, Wildcard, (struct CompressedItem *)0 };
      ERR error = Action(AC(-14), this, &args);
      if (Item) *Item = args.Item;
      return(error);
   }

   // Customised field setting

   inline ERR setOutput(OBJECTID Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->OutputID = Value;
      return ERR::Okay;
   }

   inline ERR setCompressionLevel(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERR setFlags(const CMF Value) noexcept {
      this->Flags = Value;
      return ERR::Okay;
   }

   inline ERR setSegmentSize(const LONG Value) noexcept {
      this->SegmentSize = Value;
      return ERR::Okay;
   }

   inline ERR setPermissions(const PERMIT Value) noexcept {
      this->Permissions = Value;
      return ERR::Okay;
   }

   inline ERR setWindowBits(const LONG Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   template <class T> inline ERR setArchiveName(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, 0x08800200, to_cstring(Value), 1);
   }

   template <class T> inline ERR setPath(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERR setFeedback(FUNCTION Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[17];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   template <class T> inline ERR setPassword(T && Value) noexcept {
      auto target = this;
      auto field = &this->Class->Dictionary[7];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

};

// CompressedStream class definition

#define VER_COMPRESSEDSTREAM (1.000000)

class objCompressedStream : public Object {
   public:
   static constexpr CLASSID CLASS_ID = CLASSID::COMPRESSEDSTREAM;
   static constexpr CSTRING CLASS_NAME = "CompressedStream";

   using create = pf::Create<objCompressedStream>;

   LARGE     TotalOutput; // A live counter of total bytes that have been output by the stream.
   OBJECTPTR Input;      // An input object that will supply data for decompression.
   OBJECTPTR Output;     // A target object that will receive data compressed by the stream.
   CF        Format;     // The format of the compressed stream.  The default is GZIP.

   // Customised field setting

   inline ERR setInput(OBJECTPTR Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Input = Value;
      return ERR::Okay;
   }

   inline ERR setOutput(OBJECTPTR Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Output = Value;
      return ERR::Okay;
   }

   inline ERR setFormat(const CF Value) noexcept {
      if (this->initialised()) return ERR::NoFieldAccess;
      this->Format = Value;
      return ERR::Okay;
   }

};

#ifndef PRV_CORE

// Note that the length of the data is only needed when messaging between processes, so we can skip it for these
// direct-access data channel macros.

#define acDataContent(a,b)  acDataFeed((a),0,DATA::CONTENT,(b),0)
#define acDataXML(a,b)      acDataFeed((a),0,DATA::XML,(b),0)
#define acDataText(a,b)     acDataFeed((a),0,DATA::TEXT,(b),0)

#endif // PRV_CORE

#ifdef __unix__
#include <pthread.h>
#endif

namespace pf {

#ifdef __system__
   struct ActionMessage {
      OBJECTID ObjectID;  // The object that is to receive the action
      LONG  Time;
      AC ActionID;        // ID of the action or method to execute
      bool SendArgs;

      // Action arguments follow this structure in a buffer
   };
#endif

// Event support

struct Event {
   EVENTID EventID;
   // Data follows
};

#define EVID_DISPLAY_RESOLUTION_CHANGE  GetEventID(EVG::DISPLAY, "resolution", "change")

#define EVID_GUI_SURFACE_FOCUS          GetEventID(EVG::GUI, "surface", "focus")

#define EVID_FILESYSTEM_VOLUME_CREATED  GetEventID(EVG::FILESYSTEM, "volume", "created")
#define EVID_FILESYSTEM_VOLUME_DELETED  GetEventID(EVG::FILESYSTEM, "volume", "deleted")

#define EVID_SYSTEM_TASK_CREATED        GetEventID(EVG::SYSTEM, "task", "created")
#define EVID_SYSTEM_TASK_REMOVED        GetEventID(EVG::SYSTEM, "task", "removed")

#define EVID_POWER_STATE_SUSPENDING     GetEventID(EVG::POWER, "state", "suspending")
#define EVID_POWER_STATE_RESUMED        GetEventID(EVG::POWER, "state", "resumed")
#define EVID_POWER_DISPLAY_STANDBY      GetEventID(EVG::POWER, "display", "standby")
#define EVID_POWER_BATTERY_LOW          GetEventID(EVG::POWER, "battery", "low")
#define EVID_POWER_BATTERY_CRITICAL     GetEventID(EVG::POWER, "battery", "critical")
#define EVID_POWER_CPUTEMP_HIGH         GetEventID(EVG::POWER, "cputemp", "high")
#define EVID_POWER_CPUTEMP_CRITICAL     GetEventID(EVG::POWER, "cputemp", "critical")
#define EVID_POWER_SCREENSAVER_ON       GetEventID(EVG::POWER, "screensaver", "on")
#define EVID_POWER_SCREENSAVER_OFF      GetEventID(EVG::POWER, "screensaver", "off")

#define EVID_IO_KEYMAP_CHANGE           GetEventID(EVG::IO, "keymap", "change")
#define EVID_IO_KEYBOARD_KEYPRESS       GetEventID(EVG::IO, "keyboard", "keypress")

#define EVID_AUDIO_VOLUME_MASTER        GetEventID(EVG::AUDIO, "volume", "master")
#define EVID_AUDIO_VOLUME_LINEIN        GetEventID(EVG::AUDIO, "volume", "linein")
#define EVID_AUDIO_VOLUME_MIC           GetEventID(EVG::AUDIO, "volume", "mic")
#define EVID_AUDIO_VOLUME_MUTED         GetEventID(EVG::AUDIO, "volume", "muted") // All volumes have been muted
#define EVID_AUDIO_VOLUME_UNMUTED       GetEventID(EVG::AUDIO, "volume", "unmuted") // All volumes have been unmuted

// Event structures.

typedef struct { EVENTID EventID; char Name[1]; } evVolumeCreated;
typedef struct { EVENTID EventID; char Name[1]; } evVolumeDeleted;
typedef struct { EVENTID EventID; OBJECTID TaskID; } evTaskCreated;
typedef struct { EVENTID EventID; OBJECTID TaskID; OBJECTID ProcessID; } evTaskRemoved;
typedef struct { EVENTID EventID; } evPowerSuspending;
typedef struct { EVENTID EventID; } evPowerResumed;
typedef struct { EVENTID EventID; } evUserLogin;
typedef struct { EVENTID EventID; } evKeymapChange;
typedef struct { EVENTID EventID; } evScreensaverOn;
typedef struct { EVENTID EventID; } evScreensaverOff;
typedef struct { EVENTID EventID; DOUBLE Volume; LONG Muted; } evVolume;
typedef struct { EVENTID EventID; KQ Qualifiers; KEY Code; LONG Unicode; } evKey;
typedef struct { EVENTID EventID; WORD TotalWithFocus; WORD TotalLostFocus; OBJECTID FocusList[1]; } evFocus;

// Hotplug event structure.  The hotplug event is sent whenever a new hardware device is inserted by the user.

struct evHotplug {
   EVENTID EventID;
   WORD Type;            // HT ID
   WORD Action;          // HTA_INSERTED, HTA_REMOVED
   LONG VendorID;        // USB vendor ID
   union {
      LONG ProductID;    // USB product or device ID
      LONG DeviceID;
   };
   char  ID[20];         // Typically the PCI bus ID or USB bus ID, serial number or unique identifier
   char  Group[32];      // Group name in the config file
   char  Class[32];      // Class identifier (USB)
   union {
      char Product[40];  // Name of product or the hardware device
      char Device[40];
   };
   char Vendor[40];      // Name of vendor
};

} // namespace

namespace fl {

// Read endian values from files and objects.

template<class T> ERR ReadLE(OBJECTPTR Object, T *Result)
{
   UBYTE data[sizeof(T)];
   struct acRead read = { .Buffer = data, .Length = sizeof(T) };
   if (Action(AC::Read, Object, &read) IS ERR::Okay) {
      if (read.Result IS sizeof(T)) {
         if constexpr (std::endian::native == std::endian::little) {
            *Result = ((T *)data)[0];
         }
         else {
            switch(sizeof(T)) {
               case 2:  *Result = (data[1]<<8) | data[0]; break;
               case 4:  *Result = (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|(data[3]); break;
               case 8:  *Result = ((LARGE)data[0]<<56)|((LARGE)data[1]<<48)|((LARGE)data[2]<<40)|((LARGE)data[3]<<32)|(data[4]<<24)|(data[5]<<16)|(data[6]<<8)|(data[7]); break;
               default: *Result = ((T *)data)[0];
            }
         }
         return ERR::Okay;
      }
      else return ERR::Read;
   }
   else return ERR::Read;
}

template<class T> ERR ReadBE(OBJECTPTR Object, T *Result)
{
   UBYTE data[sizeof(T)];
   struct acRead read = { .Buffer = data, .Length = sizeof(T) };
   if (Action(AC::Read, Object, &read) IS ERR::Okay) {
      if (read.Result IS sizeof(T)) {
         if constexpr (std::endian::native == std::endian::little) {
            switch(sizeof(T)) {
               case 2:  *Result = (data[1]<<8) | data[0]; break;
               case 4:  *Result = (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|(data[3]); break;
               case 8:  *Result = ((LARGE)data[0]<<56)|((LARGE)data[1]<<48)|((LARGE)data[2]<<40)|((LARGE)data[3]<<32)|(data[4]<<24)|(data[5]<<16)|(data[6]<<8)|(data[7]); break;
               default: *Result = ((T *)data)[0];
            }
         }
         else {
            *Result = ((T *)data)[0];
         }
         return ERR::Okay;
      }
      else return ERR::Read;
   }
   else return ERR::Read;
}

} // namespace

// Function construction (refer types.h)

template <class T, class X = APTR> FUNCTION C_FUNCTION(T *pRoutine, X pMeta = 0) {
   auto func    = FUNCTION(CALL::STD_C);
   func.Context = CurrentContext();
   func.Routine = (APTR)pRoutine;
   func.Meta    = reinterpret_cast<void *>(pMeta);
   return func;
};

inline CSTRING Object::className() { return Class->ClassName; }
