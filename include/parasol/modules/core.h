#pragma once

// Name:      core.h
// Copyright: Paul Manias 1996-2023
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
   REMOVEABLE = 0x00000020,
   REMOVABLE = 0x00000020,
   SOFTWARE = 0x00000040,
   NETWORK = 0x00000080,
   TAPE = 0x00000100,
   PRINTER = 0x00000200,
   SCANNER = 0x00000400,
   TEMPORARY = 0x00000800,
   MEMORY = 0x00001000,
   MODEM = 0x00002000,
   USB = 0x00004000,
   PRINTER_3D = 0x00008000,
   SCANNER_3D = 0x00010000,
};

DEFINE_ENUM_FLAG_OPERATORS(DEVICE)

// Class categories

enum class CCF : ULONG {
   NIL = 0,
   COMMAND = 0x00000001,
   DRAWABLE = 0x00000002,
   EFFECT = 0x00000004,
   FILESYSTEM = 0x00000008,
   GRAPHICS = 0x00000010,
   GUI = 0x00000020,
   IO = 0x00000040,
   SYSTEM = 0x00000080,
   TOOL = 0x00000100,
   AUDIO = 0x00000200,
   DATA = 0x00000400,
   MISC = 0x00000800,
   NETWORK = 0x00001000,
   MULTIMEDIA = 0x00002000,
};

DEFINE_ENUM_FLAG_OPERATORS(CCF)

// Action identifiers.

#define AC_Signal 1
#define AC_Activate 2
#define AC_SelectArea 3
#define AC_Clear 4
#define AC_FreeWarning 5
#define AC_Sort 6
#define AC_CopyData 7
#define AC_DataFeed 8
#define AC_Deactivate 9
#define AC_Draw 10
#define AC_Flush 11
#define AC_Focus 12
#define AC_Free 13
#define AC_SaveSettings 14
#define AC_GetVar 15
#define AC_DragDrop 16
#define AC_Hide 17
#define AC_Init 18
#define AC_Lock 19
#define AC_LostFocus 20
#define AC_Move 21
#define AC_MoveToBack 22
#define AC_MoveToFront 23
#define AC_NewChild 24
#define AC_NewOwner 25
#define AC_NewObject 26
#define AC_Redo 27
#define AC_Query 28
#define AC_Read 29
#define AC_Rename 30
#define AC_Reset 31
#define AC_Resize 32
#define AC_SaveImage 33
#define AC_SaveToObject 34
#define AC_Scroll 35
#define AC_Seek 36
#define AC_SetVar 37
#define AC_Show 38
#define AC_Undo 39
#define AC_Unlock 40
#define AC_Next 41
#define AC_Prev 42
#define AC_Write 43
#define AC_SetField 44
#define AC_Clipboard 45
#define AC_Refresh 46
#define AC_Disable 47
#define AC_Enable 48
#define AC_Redimension 49
#define AC_MoveToPoint 50
#define AC_ScrollToPoint 51
#define AC_Custom 52
#define AC_END 53

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
   DELETE = 0x00001000,
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
   FEEDBACK = 0x00000008,
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

// JET constants are documented in GetInputEvent()

enum class JET : LONG {
   NIL = 0,
   DIGITAL_X = 1,
   DIGITAL_Y = 2,
   BUTTON_1 = 3,
   LMB = 3,
   BUTTON_2 = 4,
   RMB = 4,
   BUTTON_3 = 5,
   MMB = 5,
   BUTTON_4 = 6,
   BUTTON_5 = 7,
   BUTTON_6 = 8,
   BUTTON_7 = 9,
   BUTTON_8 = 10,
   BUTTON_9 = 11,
   BUTTON_10 = 12,
   TRIGGER_LEFT = 13,
   TRIGGER_RIGHT = 14,
   BUTTON_START = 15,
   BUTTON_SELECT = 16,
   LEFT_BUMPER_1 = 17,
   LEFT_BUMPER_2 = 18,
   RIGHT_BUMPER_1 = 19,
   RIGHT_BUMPER_2 = 20,
   ANALOG_X = 21,
   ANALOG_Y = 22,
   ANALOG_Z = 23,
   ANALOG2_X = 24,
   ANALOG2_Y = 25,
   ANALOG2_Z = 26,
   WHEEL = 27,
   WHEEL_TILT = 28,
   PEN_TILT_VERTICAL = 29,
   PEN_TILT_HORIZONTAL = 30,
   ABS_X = 31,
   ABS_Y = 32,
   ENTERED_SURFACE = 33,
   ENTERED = 33,
   LEFT_SURFACE = 34,
   LEFT = 34,
   PRESSURE = 35,
   DEVICE_TILT_X = 36,
   DEVICE_TILT_Y = 37,
   DEVICE_TILT_Z = 38,
   DISPLAY_EDGE = 39,
   END = 40,
};

// Field descriptors.

#define FD_DOUBLERESULT 0x80000100
#define FD_PTR_DOUBLERESULT 0x88000100
#define FD_VOID 0x00000000
#define FD_VOLATILE 0x00000000
#define FD_OBJECT 0x00000001
#define FD_INTEGRAL 0x00000002
#define FD_REQUIRED 0x00000004
#define FD_VIRTUAL 0x00000008
#define FD_STRUCT 0x00000010
#define FD_ALLOC 0x00000020
#define FD_FLAGS 0x00000040
#define FD_VARTAGS 0x00000040
#define FD_BUFSIZE 0x00000080
#define FD_LOOKUP 0x00000080
#define FD_ARRAYSIZE 0x00000080
#define FD_PTRSIZE 0x00000080
#define FD_R 0x00000100
#define FD_READ 0x00000100
#define FD_RESULT 0x00000100
#define FD_W 0x00000200
#define FD_WRITE 0x00000200
#define FD_BUFFER 0x00000200
#define FD_RW 0x00000300
#define FD_I 0x00000400
#define FD_TAGS 0x00000400
#define FD_INIT 0x00000400
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
#define FD_PERCENTAGE 0x00200000
#define FD_WORD 0x00400000
#define FD_STR 0x00800000
#define FD_STRING 0x00800000
#define FD_STRRESULT 0x00800100
#define FD_BYTE 0x01000000
#define FD_FUNCTION 0x02000000
#define FD_LARGE 0x04000000
#define FD_LARGERESULT 0x04000100
#define FD_PTR 0x08000000
#define FD_POINTER 0x08000000
#define FD_OBJECTPTR 0x08000001
#define FD_PTRRESULT 0x08000100
#define FD_PTRBUFFER 0x08000200
#define FD_FUNCTIONPTR 0x0a000000
#define FD_PTR_LARGERESULT 0x0c000100
#define FD_FLOAT 0x10000000
#define FD_VARIABLE 0x20000000
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

#define DMF_RELATIVE_X 0x00000001
#define DMF_RELATIVE_Y 0x00000002
#define DMF_FIXED_X 0x00000004
#define DMF_X 0x00000005
#define DMF_FIXED_Y 0x00000008
#define DMF_Y 0x0000000a
#define DMF_RELATIVE_X_OFFSET 0x00000010
#define DMF_RELATIVE_Y_OFFSET 0x00000020
#define DMF_FIXED_X_OFFSET 0x00000040
#define DMF_X_OFFSET 0x00000050
#define DMF_FIXED_Y_OFFSET 0x00000080
#define DMF_Y_OFFSET 0x000000a0
#define DMF_FIXED_HEIGHT 0x00000100
#define DMF_FIXED_WIDTH 0x00000200
#define DMF_RELATIVE_HEIGHT 0x00000400
#define DMF_HEIGHT 0x00000500
#define DMF_HEIGHT_FLAGS 0x000005a0
#define DMF_VERTICAL_FLAGS 0x000005aa
#define DMF_RELATIVE_WIDTH 0x00000800
#define DMF_WIDTH 0x00000a00
#define DMF_WIDTH_FLAGS 0x00000a50
#define DMF_HORIZONTAL_FLAGS 0x00000a55
#define DMF_FIXED_DEPTH 0x00001000
#define DMF_RELATIVE_DEPTH 0x00002000
#define DMF_FIXED_Z 0x00004000
#define DMF_RELATIVE_Z 0x00008000
#define DMF_RELATIVE_RADIUS_X 0x00010000
#define DMF_FIXED_RADIUS_X 0x00020000
#define DMF_RELATIVE_CENTER_X 0x00040000
#define DMF_RELATIVE_CENTER_Y 0x00080000
#define DMF_FIXED_CENTER_X 0x00100000
#define DMF_FIXED_CENTER_Y 0x00200000
#define DMF_STATUS_CHANGE_H 0x00400000
#define DMF_STATUS_CHANGE_V 0x00800000
#define DMF_STATUS_CHANGE 0x00c00000
#define DMF_RELATIVE_RADIUS_Y 0x01000000
#define DMF_RELATIVE_RADIUS 0x01010000
#define DMF_FIXED_RADIUS_Y 0x02000000
#define DMF_FIXED_RADIUS 0x02020000

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
   WILDCARD = 0x00000004,
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

// ScrollToPoint flags

enum class STP : ULONG {
   NIL = 0,
   X = 0x00000001,
   Y = 0x00000002,
   Z = 0x00000004,
   ANIM = 0x00000008,
};

DEFINE_ENUM_FLAG_OPERATORS(STP)

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
   EXTAPI = 0x00000040,
   DEBUG = 0x00000080,
   TRACE = 0x00000100,
   FUNCTION = 0x00000200,
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
   INTEGRAL = 0x00000004,
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

#define MAX_FILENAME 256

#define MAX_NAME_LEN 32

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
   PARENT_CONTEXT = 9,
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
   STATIC_BUILD = 24,
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
   PROMOTE_INTEGRAL = 0x00000001,
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
   LARGE Timestamp;    // PreciseTime() at which the keypress was recorded
   LONG  Unicode;      // Unicode value for pre-calculated key translations
};

struct dcDeviceInput {
   DOUBLE   Value;     // The value associated with the Type
   LARGE    Timestamp; // PreciseTime() of the recorded input
   OBJECTID DeviceID;  // The hardware device that this event originated from (note: This ID can be to a private/inaccessible object, the point is that the ID is unique)
   JTYPE    Flags;     // Broad descriptors for the given Type.  Automatically defined when delivered to the pointer object
   JET      Type;      // JET constant
};

struct DateTime {
   LONG Year;        // Year
   LONG Month;       // Month 1 to 12
   LONG Day;         // Day 1 to 31
   LONG Hour;        // Hour 0 to 23
   LONG Minute;      // Minute 0 to 59
   LONG Second;      // Second 0 to 59
   LONG TimeZone;    // TimeZone -13 to +13
};

struct HSV {
   DOUBLE Hue;           // Between 0 and 359.999
   DOUBLE Saturation;    // Between 0 and 1.0
   DOUBLE Value;         // Between 0 and 1.0.  Corresponds to Value, Lightness or Brightness
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
   LONG AmtColours;         // Amount of Colours
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
#define AHASH_ACCESSOBJECT 0xbcf3b98e
#define AHASH_CLEAR 0x0f3b6d8c
#define AHASH_FREEWARNING 0xb903ddbd
#define AHASH_COPYDATA 0x47b0d1fa
#define AHASH_DATAFEED 0x05e6d293
#define AHASH_DEACTIVATE 0x1ee323ff
#define AHASH_DRAW 0x7c95d753
#define AHASH_FLUSH 0x0f71fd67
#define AHASH_FOCUS 0x0f735645
#define AHASH_FREE 0x7c96f087
#define AHASH_RELEASEOBJECT 0x9e22661d
#define AHASH_GETVAR 0xff87a74e
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
#define AHASH_SCROLL 0x1b6028b4
#define AHASH_SEEK 0x7c9dda2d
#define AHASH_SETVAR 0x1b858eda
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
#define AHASH_SCROLLTOPOINT 0xe3665f41
#define AHASH_CUSTOM 0xf753f9c0
#define AHASH_SORT 0x7c9e066d
#define AHASH_SAVESETTINGS 0x475f7165
#define AHASH_SELECTAREA 0xf55e615e
#define AHASH_SIGNAL 0x1bc6ade3
#define AHASH_UNDO 0x7c9f191b


#ifndef __GNUC__
#define __attribute__(a)
#endif

typedef const std::vector<std::pair<std::string, ULONG>> STRUCTS;

#ifndef STRINGIFY
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#endif

#define MOD_IDL NULL

extern "C" {

#ifdef PARASOL_STATIC
extern "C" __export void CloseCore(void);
extern "C" __export ERROR OpenCore(struct OpenInfo *, struct CoreBase **);
#endif

#ifdef MOD_NAME
#ifdef PARASOL_STATIC
#define PARASOL_MOD(init,close,open,expunge,IDL,Structures) static ModHeader ModHeader(init, close, open, expunge, IDL, Structures, TOSTRING(MOD_NAME));
#else
#define PARASOL_MOD(init,close,open,expunge,IDL,Structures) __export ModHeader ModHeader(init, close, open, expunge, IDL, Structures, TOSTRING(MOD_NAME));
#endif
#define MOD_PATH ("modules:" TOSTRING(MOD_NAME))
#else
#define MOD_NAME NULL
#endif

inline void FMSG(CSTRING, CSTRING, ...) __attribute__((format(printf, 2, 3)));
inline void MSG(CSTRING, ...) __attribute__((format(printf, 1, 2)));

inline void FMSG(CSTRING Function, CSTRING Message, ...) {
#ifdef DEBUG
   va_list arg;
   va_start(arg, Message);
   VLogF(VLF::API, header, Message, arg);
   va_end(arg);
#endif
}

inline void MSG(CSTRING Message, ...) {
#ifdef DEBUG
   va_list arg;
   va_start(arg, Message);
   VLogF(VLF::API, header, Message, arg);
   va_end(arg);
#endif
}

}

#define ARRAYSIZE(a) (LONG(sizeof(a)/sizeof(a[0])))

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
 #else
  #define DEBUG_BREAK raise(SIGTRAP);
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
   struct OpenTag *Options; // OPF::OPTIONS Typecast to va_list (defined in stdarg.h)
   OPF     Flags;           // OPF::flags need to be set for fields that have been defined in this structure.
   LONG    MaxDepth;        // OPF::MAX_DEPTH
   LONG    Detail;          // OPF::DETAIL
   LONG    ArgCount;        // OPF::ARGS
   ERROR   Error;           // OPF::ERROR
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
#define FT_VARIABLE FD_VARIABLE

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
#define FDF_VARIABLE   FD_VARIABLE
#define FDF_SYNONYM    FD_SYNONYM

#define FDF_UNSIGNED    (FD_UNSIGNED)
#define FDF_FUNCTION    (FD_FUNCTION)           // sizeof(struct rkFunction) - use FDF_FUNCTIONPTR for sizeof(APTR)
#define FDF_FUNCTIONPTR (FD_FUNCTION|FD_POINTER)
#define FDF_STRUCT      (FD_STRUCT)
#define FDF_RESOURCE    (FD_RESOURCE)
#define FDF_OBJECT      (FD_POINTER|FD_OBJECT)   // Field refers to another object
#define FDF_OBJECTID    (FD_LONG|FD_OBJECT)      // Field refers to another object by ID
#define FDF_INTEGRAL    (FD_POINTER|FD_INTEGRAL) // Field refers to an integral object
#define FDF_STRING      (FD_POINTER|FD_STRING)   // Field points to a string.  NB: Ideally want to remove the FD_POINTER as it should be redundant
#define FDF_STR         (FDF_STRING)
#define FDF_PERCENTAGE  FD_PERCENTAGE
#define FDF_FLAGS       FD_FLAGS                // Field contains flags
#define FDF_ALLOC       FD_ALLOC                // Field is a dynamic allocation - either a memory block or object
#define FDF_LOOKUP      FD_LOOKUP               // Lookup names for values in this field
#define FDF_READ        FD_READ                 // Field is readable
#define FDF_WRITE       FD_WRITE                // Field is writeable
#define FDF_INIT        FD_INIT                 // Field can only be written prior to Init()
#define FDF_SYSTEM      FD_SYSTEM
#define FDF_ERROR       (FD_LONG|FD_ERROR)
#define FDF_REQUIRED    FD_REQUIRED
#define FDF_RGB         (FD_RGB|FD_BYTE|FD_ARRAY)
#define FDF_R           (FD_READ)
#define FDF_W           (FD_WRITE)
#define FDF_RW          (FD_READ|FD_WRITE)
#define FDF_RI          (FD_READ|FD_INIT)
#define FDF_I           (FD_INIT)
#define FDF_VIRTUAL     FD_VIRTUAL
#define FDF_LONGFLAGS   (FDF_LONG|FDF_FLAGS)
#define FDF_FIELDTYPES  (FD_LONG|FD_DOUBLE|FD_LARGE|FD_POINTER|FD_VARIABLE|FD_BYTE|FD_ARRAY|FD_FUNCTION)

// These constants have to match the FD* constants << 32

#define TDOUBLE   0x8000000000000000LL
#define TLONG     0x4000000000000000LL
#define TVAR      0x2000000000000000LL
#define TFLOAT    0x1000000000000000LL // NB: Floats are upscaled to doubles when passed as v-args.
#define TPTR      0x0800000000000000LL
#define TLARGE    0x0400000000000000LL
#define TFUNCTION 0x0200000000000000LL
#define TSTR      0x0080000000000000LL
#define TRELATIVE 0x0020000000000000LL
#define TARRAY    0x0000100000000000LL
#define TPERCENT  TRELATIVE
#define TAGEND    0LL
#define TAGDIVERT -1LL
#define TSTRING   TSTR
#define TREL      TRELATIVE

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
      PERCENT Percent;
      LARGE   Large;
      LONG    Long;
   };

   //std::string not included as not compatible with constexpr
   constexpr FieldValue(ULONG pFID, CSTRING pValue) : FieldID(pFID), Type(FD_STRING), String(pValue) { };
   constexpr FieldValue(ULONG pFID, LONG pValue)    : FieldID(pFID), Type(FD_LONG), Long(pValue) { };
   constexpr FieldValue(ULONG pFID, LARGE pValue)   : FieldID(pFID), Type(FD_LARGE), Large(pValue) { };
   constexpr FieldValue(ULONG pFID, DOUBLE pValue)  : FieldID(pFID), Type(FD_DOUBLE), Double(pValue) { };
   constexpr FieldValue(ULONG pFID, PERCENT pValue) : FieldID(pFID), Type(FD_DOUBLE|FD_PERCENTAGE), Percent(pValue) { };
   constexpr FieldValue(ULONG pFID, APTR pValue)    : FieldID(pFID), Type(FD_POINTER), Pointer(pValue) { };
   constexpr FieldValue(ULONG pFID, CPTR pValue)    : FieldID(pFID), Type(FD_POINTER), CPointer(pValue) { };
   constexpr FieldValue(ULONG pFID, CPTR pValue, LONG pCustom) : FieldID(pFID), Type(pCustom), CPointer(pValue) { };
};

}

#include <string.h> // memset()
#include <stdlib.h> // strtol(), strtod()

struct ObjectSignal {
   OBJECTPTR Object;
};

struct ResourceManager {
   CSTRING Name;            // The name of the resource.
   ERROR (*Free)(APTR);     // A function that will remove the resource's content when terminated.
};

typedef struct pfBase64Decode {
   UBYTE Step;             // Internal
   UBYTE PlainChar;        // Internal
   UBYTE Initialised:1;    // Internal
  pfBase64Decode() : Step(0), PlainChar(0), Initialised(0) { };
} BASE64DECODE;

typedef struct pfBase64Encode {
   UBYTE Step;        // Internal
   UBYTE Result;      // Internal
   LONG  StepCount;   // Internal
  pfBase64Encode() : Step(0), Result(0), StepCount(0) { };
} BASE64ENCODE;

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
   MHF     Flags;                                   // Special flags, type of function table wanted from the Core
   CSTRING Definitions;                             // Module definition string, usable by run-time languages such as Fluid
   ERROR (*Init)(OBJECTPTR, struct CoreBase *);     // A one-off initialisation routine for when the module is first opened.
   void (*Close)(OBJECTPTR);                        // A function that will be called each time the module is closed.
   ERROR (*Open)(OBJECTPTR);                        // A function that will be called each time the module is opened.
   ERROR (*Expunge)(void);                          // Reference to an expunge function to terminate the module.
   CSTRING Name;                                    // Name of the module
   STRUCTS *StructDefs;
   class RootModule *Root;
   ModHeader(ERROR (*pInit)(OBJECTPTR, struct CoreBase *),
      void  (*pClose)(OBJECTPTR),
      ERROR (*pOpen)(OBJECTPTR),
      ERROR (*pExpunge)(void),
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
   CSTRING Name;    // The name of the field, e.g. "Width"
   APTR    GetField; // void GetField(*Object, APTR Result);
   APTR    SetField; // ERROR SetField(*Object, APTR Value);
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
   CSTRING Platform;        // String-based field indicating the user's platform.  Currently returns 'Native', 'Windows', 'OSX' or 'Linux'.
   HOSTHANDLE ConsoleFD;    // Internal
   LONG    Stage;           // The current operating stage.  -1 = Initialising, 0 indicates normal operating status; 1 means that the program is shutting down; 2 indicates a program restart; 3 is for mode switches.
};

struct Variable {
   ULONG  Type;      // Field definition flags
   LONG   Unused;    // Unused 32-bit value for 64-bit alignment
   LARGE  Large;     // The value as a 64-bit integer.
   DOUBLE Double;    // The value as a 64-bit float-point number.
   APTR   Pointer;   // The value as an address pointer.
   Variable(LONG Value) : Type(FD_LARGE), Large(Value) { }
   Variable(LARGE Value) : Type(FD_LARGE), Large(Value) { }
   Variable(DOUBLE Value) : Type(FD_DOUBLE), Double(Value) { }
   Variable(APTR Value) : Type(FD_POINTER), Pointer(Value) { }
   Variable() { }
};

struct ActionArray {
   APTR Routine;       // Pointer to the function entry point
   LONG ActionCode;    // Action identifier
  template <class T> ActionArray(LONG pID, T pRoutine) : Routine((APTR)pRoutine), ActionCode(pID) { }
};

struct MethodEntry {
   LONG    MethodID;                     // Unique method identifier
   APTR    Routine;                      // The method entry point, defined as ERROR (*Routine)(OBJECTPTR, APTR);
   CSTRING Name;                         // Name of the method
   const struct FunctionField * Args;    // List of parameters accepted by the method
   LONG    Size;                         // Total byte-size of all accepted parameters when they are assembled as a C structure.
   MethodEntry() : MethodID(0), Routine(NULL), Name(NULL) { }
   MethodEntry(LONG pID, APTR pRoutine, CSTRING pName, const struct FunctionField *pArgs, LONG pSize) :
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
   LARGE Time;    // A timestamp acquired from PreciseTime() when the message was first passed to SendMessage().
   LONG  UID;     // A unique identifier automatically created by SendMessage().
   LONG  Type;    // A message type identifier as defined by the client.
   LONG  Size;    // The size of the message data, in bytes.  If there is no data associated with the message, the Size will be set to zero.</>
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
   STRING Name;               // The name of the file.  This string remains valid until the next call to GetFileInfo().
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
};

struct Field {
   MAXINT  Arg;                                                                  // An option to complement the field type.  Can be a pointer or an integer value
   ERROR (*GetValue)(APTR, APTR);                                                // A virtual function that will retrieve the value for this field.
   APTR    SetValue;                                                             // A virtual function that will set the value for this field.
   ERROR (*WriteValue)(OBJECTPTR, struct Field *, LONG, const void *, LONG);     // An internal function for writing to this field.
   CSTRING Name;                                                                 // The English name for the field, e.g. "Width"
   ULONG   FieldID;                                                              // Provides a fast way of finding fields, e.g. FID_WIDTH
   UWORD   Offset;                                                               // Field offset within the object
   UWORD   Index;                                                                // Field array index
   ULONG   Flags;                                                                // Special flags that describe the field
};

struct ScriptArg { // For use with scExec
   CSTRING Name;
   ULONG Type;
   union {
      APTR   Address;
      LONG   Long;
      LARGE  Large;
      DOUBLE Double;
   };
};

#ifdef PARASOL_STATIC
#define JUMPTABLE_CORE static struct CoreBase *CoreBase;
#else
#define JUMPTABLE_CORE struct CoreBase *CoreBase;
#endif

struct CoreBase {
#ifndef PARASOL_STATIC
   ERROR (*_AccessMemory)(MEMORYID Memory, MEM Flags, LONG MilliSeconds, APTR Result);
   ERROR (*_Action)(LONG Action, OBJECTPTR Object, APTR Parameters);
   void (*_ActionList)(struct ActionTable ** Actions, LONG * Size);
   ERROR (*_ActionMsg)(LONG Action, OBJECTID Object, APTR Args);
   CSTRING (*_ResolveClassID)(CLASSID ID);
   LONG (*_AllocateID)(IDTYPE Type);
   ERROR (*_AllocMemory)(LONG Size, MEM Flags, APTR Address, MEMORYID * ID);
   ERROR (*_AccessObject)(OBJECTID Object, LONG MilliSeconds, APTR Result);
   ERROR (*_CheckAction)(OBJECTPTR Object, LONG Action);
   ERROR (*_CheckMemoryExists)(MEMORYID ID);
   ERROR (*_CheckObjectExists)(OBJECTID Object);
   ERROR (*_InitObject)(OBJECTPTR Object);
   ERROR (*_VirtualVolume)(CSTRING Name, ...);
   OBJECTPTR (*_CurrentContext)(void);
   ERROR (*_GetFieldArray)(OBJECTPTR Object, FIELD Field, APTR Result, LONG * Elements);
   LONG (*_AdjustLogLevel)(LONG Adjust);
   ERROR (*_ReadFileToBuffer)(CSTRING Path, APTR Buffer, LONG BufferSize, LONG * Result);
   ERROR (*_FindObject)(CSTRING Name, CLASSID ClassID, FOF Flags, OBJECTID * ObjectID);
   objMetaClass * (*_FindClass)(CLASSID ClassID);
   ERROR (*_AnalysePath)(CSTRING Path, LOC * Type);
   LONG (*_UTF8Copy)(CSTRING Src, STRING Dest, LONG Chars, LONG Size);
   ERROR (*_FreeResource)(MEMORYID ID);
   CLASSID (*_GetClassID)(OBJECTID Object);
   OBJECTID (*_GetOwnerID)(OBJECTID Object);
   ERROR (*_GetField)(OBJECTPTR Object, FIELD Field, APTR Result);
   ERROR (*_GetFieldVariable)(OBJECTPTR Object, CSTRING Field, STRING Buffer, LONG Size);
   ERROR (*_CompareFilePaths)(CSTRING PathA, CSTRING PathB);
   const struct SystemState * (*_GetSystemState)(void);
   ERROR (*_ListChildren)(OBJECTID Object, pf::vector<ChildEntry> * List);
   ERROR (*_Base64Decode)(struct pfBase64Decode * State, CSTRING Input, LONG InputSize, APTR Output, LONG * Written);
   ERROR (*_RegisterFD)(HOSTHANDLE FD, RFD Flags, void (*Routine)(HOSTHANDLE, APTR) , APTR Data);
   ERROR (*_ResolvePath)(CSTRING Path, RSF Flags, STRING * Result);
   ERROR (*_MemoryIDInfo)(MEMORYID ID, struct MemInfo * MemInfo, LONG Size);
   ERROR (*_MemoryPtrInfo)(APTR Address, struct MemInfo * MemInfo, LONG Size);
   ERROR (*_NewObject)(LARGE ClassID, NF Flags, APTR Object);
   void (*_NotifySubscribers)(OBJECTPTR Object, LONG Action, APTR Args, ERROR Error);
   ERROR (*_StrReadLocale)(CSTRING Key, CSTRING * Value);
   CSTRING (*_UTF8ValidEncoding)(CSTRING String, CSTRING Encoding);
   ERROR (*_ProcessMessages)(PMF Flags, LONG TimeOut);
   ERROR (*_IdentifyFile)(CSTRING Path, CLASSID * Class, CLASSID * SubClass);
   ERROR (*_ReallocMemory)(APTR Memory, ULONG Size, APTR Address, MEMORYID * ID);
   ERROR (*_GetMessage)(LONG Type, MSF Flags, APTR Buffer, LONG Size);
   ERROR (*_ReleaseMemory)(MEMORYID MemoryID);
   CLASSID (*_ResolveClassName)(CSTRING Name);
   ERROR (*_SendMessage)(LONG Type, MSF Flags, APTR Data, LONG Size);
   ERROR (*_SetOwner)(OBJECTPTR Object, OBJECTPTR Owner);
   OBJECTPTR (*_SetContext)(OBJECTPTR Object);
   ERROR (*_SetField)(OBJECTPTR Object, FIELD Field, ...);
   CSTRING (*_FieldName)(ULONG FieldID);
   ERROR (*_ScanDir)(struct DirInfo * Info);
   ERROR (*_SetName)(OBJECTPTR Object, CSTRING Name);
   void (*_LogReturn)(void);
   ERROR (*_StrCompare)(CSTRING String1, CSTRING String2, LONG Length, STR Flags);
   ERROR (*_SubscribeAction)(OBJECTPTR Object, LONG Action, FUNCTION * Callback);
   ERROR (*_SubscribeEvent)(LARGE Event, FUNCTION * Callback, APTR Custom, APTR Handle);
   ERROR (*_SubscribeTimer)(DOUBLE Interval, FUNCTION * Callback, APTR Subscription);
   ERROR (*_UpdateTimer)(APTR Subscription, DOUBLE Interval);
   ERROR (*_UnsubscribeAction)(OBJECTPTR Object, LONG Action);
   void (*_UnsubscribeEvent)(APTR Handle);
   ERROR (*_BroadcastEvent)(APTR Event, LONG EventSize);
   void (*_WaitTime)(LONG Seconds, LONG MicroSeconds);
   LARGE (*_GetEventID)(EVG Group, CSTRING SubGroup, CSTRING Event);
   ULONG (*_GenCRC32)(ULONG CRC, APTR Data, ULONG Length);
   LARGE (*_GetResource)(RES Resource);
   LARGE (*_SetResource)(RES Resource, LARGE Value);
   ERROR (*_ScanMessages)(LONG * Handle, LONG Type, APTR Buffer, LONG Size);
   STT (*_StrDatatype)(CSTRING String);
   void (*_UnloadFile)(struct CacheFile * Cache);
   ERROR (*_CreateFolder)(CSTRING Path, PERMIT Permissions);
   ERROR (*_LoadFile)(CSTRING Path, LDF Flags, struct CacheFile ** Cache);
   ERROR (*_SetVolume)(CSTRING Name, CSTRING Path, CSTRING Icon, CSTRING Label, CSTRING Device, VOLUME Flags);
   ERROR (*_DeleteVolume)(CSTRING Name);
   ERROR (*_MoveFile)(CSTRING Source, CSTRING Dest, FUNCTION * Callback);
   ERROR (*_UpdateMessage)(LONG Message, LONG Type, APTR Data, LONG Size);
   ERROR (*_AddMsgHandler)(APTR Custom, LONG MsgType, FUNCTION * Routine, struct MsgHandler ** Handle);
   ERROR (*_QueueAction)(LONG Action, OBJECTID Object, APTR Args);
   LARGE (*_PreciseTime)(void);
   ERROR (*_OpenDir)(CSTRING Path, RDF Flags, struct DirInfo ** Info);
   OBJECTPTR (*_GetObjectPtr)(OBJECTID Object);
   struct Field * (*_FindField)(OBJECTPTR Object, ULONG FieldID, APTR Target);
   CSTRING (*_GetErrorMsg)(ERROR Error);
   struct Message * (*_GetActionMsg)(void);
   ERROR (*_FuncError)(CSTRING Header, ERROR Error);
   ERROR (*_SetArray)(OBJECTPTR Object, FIELD Field, APTR Array, LONG Elements);
   ULONG (*_StrHash)(CSTRING String, LONG CaseSensitive);
   ERROR (*_LockObject)(OBJECTPTR Object, LONG MilliSeconds);
   void (*_ReleaseObject)(OBJECTPTR Object);
   ERROR (*_ActionThread)(LONG Action, OBJECTPTR Object, APTR Args, FUNCTION * Callback, LONG Key);
   ERROR (*_AddInfoTag)(struct FileInfo * Info, CSTRING Name, CSTRING Value);
   void (*_SetDefaultPermissions)(LONG User, LONG Group, PERMIT Permissions);
   void (*_VLogF)(VLF Flags, const char *Header, const char *Message, va_list Args);
   LONG (*_Base64Encode)(struct pfBase64Encode * State, const void * Input, LONG InputSize, STRING Output, LONG OutputSize);
   ERROR (*_ReadInfoTag)(struct FileInfo * Info, CSTRING Name, CSTRING * Value);
   ERROR (*_SetResourcePath)(RP PathType, CSTRING Path);
   objTask * (*_CurrentTask)(void);
   CSTRING (*_ResolveGroupID)(LONG Group);
   CSTRING (*_ResolveUserID)(LONG User);
   ERROR (*_CreateLink)(CSTRING From, CSTRING To);
   ERROR (*_DeleteFile)(CSTRING Path, FUNCTION * Callback);
   LONG (*_UTF8CharOffset)(CSTRING String, LONG Offset);
   LONG (*_UTF8Length)(CSTRING String);
   LONG (*_UTF8OffsetToChar)(CSTRING String, LONG Offset);
   LONG (*_UTF8PrevLength)(CSTRING String, LONG Offset);
   LONG (*_UTF8CharLength)(CSTRING String);
   ULONG (*_UTF8ReadValue)(CSTRING String, LONG * Length);
   LONG (*_UTF8WriteValue)(LONG Value, STRING Buffer, LONG Size);
   ERROR (*_CopyFile)(CSTRING Source, CSTRING Dest, FUNCTION * Callback);
   ERROR (*_WaitForObjects)(PMF Flags, LONG TimeOut, struct ObjectSignal * ObjectSignals);
#endif // PARASOL_STATIC
};

#ifndef PRV_CORE_MODULE
#ifndef PARASOL_STATIC
extern struct CoreBase *CoreBase;
inline ERROR AccessMemory(MEMORYID Memory, MEM Flags, LONG MilliSeconds, APTR Result) { return CoreBase->_AccessMemory(Memory,Flags,MilliSeconds,Result); }
inline ERROR Action(LONG Action, OBJECTPTR Object, APTR Parameters) { return CoreBase->_Action(Action,Object,Parameters); }
inline void ActionList(struct ActionTable ** Actions, LONG * Size) { return CoreBase->_ActionList(Actions,Size); }
inline ERROR ActionMsg(LONG Action, OBJECTID Object, APTR Args) { return CoreBase->_ActionMsg(Action,Object,Args); }
inline CSTRING ResolveClassID(CLASSID ID) { return CoreBase->_ResolveClassID(ID); }
inline LONG AllocateID(IDTYPE Type) { return CoreBase->_AllocateID(Type); }
inline ERROR AllocMemory(LONG Size, MEM Flags, APTR Address, MEMORYID * ID) { return CoreBase->_AllocMemory(Size,Flags,Address,ID); }
inline ERROR AccessObject(OBJECTID Object, LONG MilliSeconds, APTR Result) { return CoreBase->_AccessObject(Object,MilliSeconds,Result); }
inline ERROR CheckAction(OBJECTPTR Object, LONG Action) { return CoreBase->_CheckAction(Object,Action); }
inline ERROR CheckMemoryExists(MEMORYID ID) { return CoreBase->_CheckMemoryExists(ID); }
inline ERROR CheckObjectExists(OBJECTID Object) { return CoreBase->_CheckObjectExists(Object); }
inline ERROR InitObject(OBJECTPTR Object) { return CoreBase->_InitObject(Object); }
template<class... Args> ERROR VirtualVolume(CSTRING Name, Args... Tags) { return CoreBase->_VirtualVolume(Name,Tags...); }
inline OBJECTPTR CurrentContext(void) { return CoreBase->_CurrentContext(); }
inline ERROR GetFieldArray(OBJECTPTR Object, FIELD Field, APTR Result, LONG * Elements) { return CoreBase->_GetFieldArray(Object,Field,Result,Elements); }
inline LONG AdjustLogLevel(LONG Adjust) { return CoreBase->_AdjustLogLevel(Adjust); }
inline ERROR ReadFileToBuffer(CSTRING Path, APTR Buffer, LONG BufferSize, LONG * Result) { return CoreBase->_ReadFileToBuffer(Path,Buffer,BufferSize,Result); }
inline ERROR FindObject(CSTRING Name, CLASSID ClassID, FOF Flags, OBJECTID * ObjectID) { return CoreBase->_FindObject(Name,ClassID,Flags,ObjectID); }
inline objMetaClass * FindClass(CLASSID ClassID) { return CoreBase->_FindClass(ClassID); }
inline ERROR AnalysePath(CSTRING Path, LOC * Type) { return CoreBase->_AnalysePath(Path,Type); }
inline LONG UTF8Copy(CSTRING Src, STRING Dest, LONG Chars, LONG Size) { return CoreBase->_UTF8Copy(Src,Dest,Chars,Size); }
inline ERROR FreeResource(MEMORYID ID) { return CoreBase->_FreeResource(ID); }
inline CLASSID GetClassID(OBJECTID Object) { return CoreBase->_GetClassID(Object); }
inline OBJECTID GetOwnerID(OBJECTID Object) { return CoreBase->_GetOwnerID(Object); }
inline ERROR GetField(OBJECTPTR Object, FIELD Field, APTR Result) { return CoreBase->_GetField(Object,Field,Result); }
inline ERROR GetFieldVariable(OBJECTPTR Object, CSTRING Field, STRING Buffer, LONG Size) { return CoreBase->_GetFieldVariable(Object,Field,Buffer,Size); }
inline ERROR CompareFilePaths(CSTRING PathA, CSTRING PathB) { return CoreBase->_CompareFilePaths(PathA,PathB); }
inline const struct SystemState * GetSystemState(void) { return CoreBase->_GetSystemState(); }
inline ERROR ListChildren(OBJECTID Object, pf::vector<ChildEntry> * List) { return CoreBase->_ListChildren(Object,List); }
inline ERROR Base64Decode(struct pfBase64Decode * State, CSTRING Input, LONG InputSize, APTR Output, LONG * Written) { return CoreBase->_Base64Decode(State,Input,InputSize,Output,Written); }
inline ERROR RegisterFD(HOSTHANDLE FD, RFD Flags, void (*Routine)(HOSTHANDLE, APTR) , APTR Data) { return CoreBase->_RegisterFD(FD,Flags,Routine,Data); }
inline ERROR ResolvePath(CSTRING Path, RSF Flags, STRING * Result) { return CoreBase->_ResolvePath(Path,Flags,Result); }
inline ERROR MemoryIDInfo(MEMORYID ID, struct MemInfo * MemInfo, LONG Size) { return CoreBase->_MemoryIDInfo(ID,MemInfo,Size); }
inline ERROR MemoryPtrInfo(APTR Address, struct MemInfo * MemInfo, LONG Size) { return CoreBase->_MemoryPtrInfo(Address,MemInfo,Size); }
inline ERROR NewObject(LARGE ClassID, NF Flags, APTR Object) { return CoreBase->_NewObject(ClassID,Flags,Object); }
inline void NotifySubscribers(OBJECTPTR Object, LONG Action, APTR Args, ERROR Error) { return CoreBase->_NotifySubscribers(Object,Action,Args,Error); }
inline ERROR StrReadLocale(CSTRING Key, CSTRING * Value) { return CoreBase->_StrReadLocale(Key,Value); }
inline CSTRING UTF8ValidEncoding(CSTRING String, CSTRING Encoding) { return CoreBase->_UTF8ValidEncoding(String,Encoding); }
inline ERROR ProcessMessages(PMF Flags, LONG TimeOut) { return CoreBase->_ProcessMessages(Flags,TimeOut); }
inline ERROR IdentifyFile(CSTRING Path, CLASSID * Class, CLASSID * SubClass) { return CoreBase->_IdentifyFile(Path,Class,SubClass); }
inline ERROR ReallocMemory(APTR Memory, ULONG Size, APTR Address, MEMORYID * ID) { return CoreBase->_ReallocMemory(Memory,Size,Address,ID); }
inline ERROR GetMessage(LONG Type, MSF Flags, APTR Buffer, LONG Size) { return CoreBase->_GetMessage(Type,Flags,Buffer,Size); }
inline ERROR ReleaseMemory(MEMORYID MemoryID) { return CoreBase->_ReleaseMemory(MemoryID); }
inline CLASSID ResolveClassName(CSTRING Name) { return CoreBase->_ResolveClassName(Name); }
inline ERROR SendMessage(LONG Type, MSF Flags, APTR Data, LONG Size) { return CoreBase->_SendMessage(Type,Flags,Data,Size); }
inline ERROR SetOwner(OBJECTPTR Object, OBJECTPTR Owner) { return CoreBase->_SetOwner(Object,Owner); }
inline OBJECTPTR SetContext(OBJECTPTR Object) { return CoreBase->_SetContext(Object); }
template<class... Args> ERROR SetField(OBJECTPTR Object, FIELD Field, Args... Tags) { return CoreBase->_SetField(Object,Field,Tags...); }
inline CSTRING FieldName(ULONG FieldID) { return CoreBase->_FieldName(FieldID); }
inline ERROR ScanDir(struct DirInfo * Info) { return CoreBase->_ScanDir(Info); }
inline ERROR SetName(OBJECTPTR Object, CSTRING Name) { return CoreBase->_SetName(Object,Name); }
inline void LogReturn(void) { return CoreBase->_LogReturn(); }
inline ERROR StrCompare(CSTRING String1, CSTRING String2, LONG Length, STR Flags) { return CoreBase->_StrCompare(String1,String2,Length,Flags); }
inline ERROR SubscribeAction(OBJECTPTR Object, LONG Action, FUNCTION * Callback) { return CoreBase->_SubscribeAction(Object,Action,Callback); }
inline ERROR SubscribeEvent(LARGE Event, FUNCTION * Callback, APTR Custom, APTR Handle) { return CoreBase->_SubscribeEvent(Event,Callback,Custom,Handle); }
inline ERROR SubscribeTimer(DOUBLE Interval, FUNCTION * Callback, APTR Subscription) { return CoreBase->_SubscribeTimer(Interval,Callback,Subscription); }
inline ERROR UpdateTimer(APTR Subscription, DOUBLE Interval) { return CoreBase->_UpdateTimer(Subscription,Interval); }
inline ERROR UnsubscribeAction(OBJECTPTR Object, LONG Action) { return CoreBase->_UnsubscribeAction(Object,Action); }
inline void UnsubscribeEvent(APTR Handle) { return CoreBase->_UnsubscribeEvent(Handle); }
inline ERROR BroadcastEvent(APTR Event, LONG EventSize) { return CoreBase->_BroadcastEvent(Event,EventSize); }
inline void WaitTime(LONG Seconds, LONG MicroSeconds) { return CoreBase->_WaitTime(Seconds,MicroSeconds); }
inline LARGE GetEventID(EVG Group, CSTRING SubGroup, CSTRING Event) { return CoreBase->_GetEventID(Group,SubGroup,Event); }
inline ULONG GenCRC32(ULONG CRC, APTR Data, ULONG Length) { return CoreBase->_GenCRC32(CRC,Data,Length); }
inline LARGE GetResource(RES Resource) { return CoreBase->_GetResource(Resource); }
inline LARGE SetResource(RES Resource, LARGE Value) { return CoreBase->_SetResource(Resource,Value); }
inline ERROR ScanMessages(LONG * Handle, LONG Type, APTR Buffer, LONG Size) { return CoreBase->_ScanMessages(Handle,Type,Buffer,Size); }
inline STT StrDatatype(CSTRING String) { return CoreBase->_StrDatatype(String); }
inline void UnloadFile(struct CacheFile * Cache) { return CoreBase->_UnloadFile(Cache); }
inline ERROR CreateFolder(CSTRING Path, PERMIT Permissions) { return CoreBase->_CreateFolder(Path,Permissions); }
inline ERROR LoadFile(CSTRING Path, LDF Flags, struct CacheFile ** Cache) { return CoreBase->_LoadFile(Path,Flags,Cache); }
inline ERROR SetVolume(CSTRING Name, CSTRING Path, CSTRING Icon, CSTRING Label, CSTRING Device, VOLUME Flags) { return CoreBase->_SetVolume(Name,Path,Icon,Label,Device,Flags); }
inline ERROR DeleteVolume(CSTRING Name) { return CoreBase->_DeleteVolume(Name); }
inline ERROR MoveFile(CSTRING Source, CSTRING Dest, FUNCTION * Callback) { return CoreBase->_MoveFile(Source,Dest,Callback); }
inline ERROR UpdateMessage(LONG Message, LONG Type, APTR Data, LONG Size) { return CoreBase->_UpdateMessage(Message,Type,Data,Size); }
inline ERROR AddMsgHandler(APTR Custom, LONG MsgType, FUNCTION * Routine, struct MsgHandler ** Handle) { return CoreBase->_AddMsgHandler(Custom,MsgType,Routine,Handle); }
inline ERROR QueueAction(LONG Action, OBJECTID Object, APTR Args) { return CoreBase->_QueueAction(Action,Object,Args); }
inline LARGE PreciseTime(void) { return CoreBase->_PreciseTime(); }
inline ERROR OpenDir(CSTRING Path, RDF Flags, struct DirInfo ** Info) { return CoreBase->_OpenDir(Path,Flags,Info); }
inline OBJECTPTR GetObjectPtr(OBJECTID Object) { return CoreBase->_GetObjectPtr(Object); }
inline struct Field * FindField(OBJECTPTR Object, ULONG FieldID, APTR Target) { return CoreBase->_FindField(Object,FieldID,Target); }
inline CSTRING GetErrorMsg(ERROR Error) { return CoreBase->_GetErrorMsg(Error); }
inline struct Message * GetActionMsg(void) { return CoreBase->_GetActionMsg(); }
inline ERROR FuncError(CSTRING Header, ERROR Error) { return CoreBase->_FuncError(Header,Error); }
inline ERROR SetArray(OBJECTPTR Object, FIELD Field, APTR Array, LONG Elements) { return CoreBase->_SetArray(Object,Field,Array,Elements); }
inline ULONG StrHash(CSTRING String, LONG CaseSensitive) { return CoreBase->_StrHash(String,CaseSensitive); }
inline ERROR LockObject(OBJECTPTR Object, LONG MilliSeconds) { return CoreBase->_LockObject(Object,MilliSeconds); }
inline void ReleaseObject(OBJECTPTR Object) { return CoreBase->_ReleaseObject(Object); }
inline ERROR ActionThread(LONG Action, OBJECTPTR Object, APTR Args, FUNCTION * Callback, LONG Key) { return CoreBase->_ActionThread(Action,Object,Args,Callback,Key); }
inline ERROR AddInfoTag(struct FileInfo * Info, CSTRING Name, CSTRING Value) { return CoreBase->_AddInfoTag(Info,Name,Value); }
inline void SetDefaultPermissions(LONG User, LONG Group, PERMIT Permissions) { return CoreBase->_SetDefaultPermissions(User,Group,Permissions); }
inline void VLogF(VLF Flags, const char *Header, const char *Message, va_list Args) { return CoreBase->_VLogF(Flags,Header,Message,Args); }
inline LONG Base64Encode(struct pfBase64Encode * State, const void * Input, LONG InputSize, STRING Output, LONG OutputSize) { return CoreBase->_Base64Encode(State,Input,InputSize,Output,OutputSize); }
inline ERROR ReadInfoTag(struct FileInfo * Info, CSTRING Name, CSTRING * Value) { return CoreBase->_ReadInfoTag(Info,Name,Value); }
inline ERROR SetResourcePath(RP PathType, CSTRING Path) { return CoreBase->_SetResourcePath(PathType,Path); }
inline objTask * CurrentTask(void) { return CoreBase->_CurrentTask(); }
inline CSTRING ResolveGroupID(LONG Group) { return CoreBase->_ResolveGroupID(Group); }
inline CSTRING ResolveUserID(LONG User) { return CoreBase->_ResolveUserID(User); }
inline ERROR CreateLink(CSTRING From, CSTRING To) { return CoreBase->_CreateLink(From,To); }
inline ERROR DeleteFile(CSTRING Path, FUNCTION * Callback) { return CoreBase->_DeleteFile(Path,Callback); }
inline LONG UTF8CharOffset(CSTRING String, LONG Offset) { return CoreBase->_UTF8CharOffset(String,Offset); }
inline LONG UTF8Length(CSTRING String) { return CoreBase->_UTF8Length(String); }
inline LONG UTF8OffsetToChar(CSTRING String, LONG Offset) { return CoreBase->_UTF8OffsetToChar(String,Offset); }
inline LONG UTF8PrevLength(CSTRING String, LONG Offset) { return CoreBase->_UTF8PrevLength(String,Offset); }
inline LONG UTF8CharLength(CSTRING String) { return CoreBase->_UTF8CharLength(String); }
inline ULONG UTF8ReadValue(CSTRING String, LONG * Length) { return CoreBase->_UTF8ReadValue(String,Length); }
inline LONG UTF8WriteValue(LONG Value, STRING Buffer, LONG Size) { return CoreBase->_UTF8WriteValue(Value,Buffer,Size); }
inline ERROR CopyFile(CSTRING Source, CSTRING Dest, FUNCTION * Callback) { return CoreBase->_CopyFile(Source,Dest,Callback); }
inline ERROR WaitForObjects(PMF Flags, LONG TimeOut, struct ObjectSignal * ObjectSignals) { return CoreBase->_WaitForObjects(Flags,TimeOut,ObjectSignals); }
#else
extern "C" {
extern ERROR AccessMemory(MEMORYID Memory, MEM Flags, LONG MilliSeconds, APTR Result);
extern ERROR Action(LONG Action, OBJECTPTR Object, APTR Parameters);
extern void ActionList(struct ActionTable ** Actions, LONG * Size);
extern ERROR ActionMsg(LONG Action, OBJECTID Object, APTR Args);
extern CSTRING ResolveClassID(CLASSID ID);
extern LONG AllocateID(IDTYPE Type);
extern ERROR AllocMemory(LONG Size, MEM Flags, APTR Address, MEMORYID * ID);
extern ERROR AccessObject(OBJECTID Object, LONG MilliSeconds, APTR Result);
extern ERROR CheckAction(OBJECTPTR Object, LONG Action);
extern ERROR CheckMemoryExists(MEMORYID ID);
extern ERROR CheckObjectExists(OBJECTID Object);
extern ERROR InitObject(OBJECTPTR Object);
extern OBJECTPTR CurrentContext(void);
extern ERROR GetFieldArray(OBJECTPTR Object, FIELD Field, APTR Result, LONG * Elements);
extern LONG AdjustLogLevel(LONG Adjust);
extern ERROR ReadFileToBuffer(CSTRING Path, APTR Buffer, LONG BufferSize, LONG * Result);
extern ERROR FindObject(CSTRING Name, CLASSID ClassID, FOF Flags, OBJECTID * ObjectID);
extern objMetaClass * FindClass(CLASSID ClassID);
extern ERROR AnalysePath(CSTRING Path, LOC * Type);
extern LONG UTF8Copy(CSTRING Src, STRING Dest, LONG Chars, LONG Size);
extern ERROR FreeResource(MEMORYID ID);
extern CLASSID GetClassID(OBJECTID Object);
extern OBJECTID GetOwnerID(OBJECTID Object);
extern ERROR GetField(OBJECTPTR Object, FIELD Field, APTR Result);
extern ERROR GetFieldVariable(OBJECTPTR Object, CSTRING Field, STRING Buffer, LONG Size);
extern ERROR CompareFilePaths(CSTRING PathA, CSTRING PathB);
extern const struct SystemState * GetSystemState(void);
extern ERROR ListChildren(OBJECTID Object, pf::vector<ChildEntry> * List);
extern ERROR Base64Decode(struct pfBase64Decode * State, CSTRING Input, LONG InputSize, APTR Output, LONG * Written);
extern ERROR RegisterFD(HOSTHANDLE FD, RFD Flags, void (*Routine)(HOSTHANDLE, APTR) , APTR Data);
extern ERROR ResolvePath(CSTRING Path, RSF Flags, STRING * Result);
extern ERROR MemoryIDInfo(MEMORYID ID, struct MemInfo * MemInfo, LONG Size);
extern ERROR MemoryPtrInfo(APTR Address, struct MemInfo * MemInfo, LONG Size);
extern ERROR NewObject(LARGE ClassID, NF Flags, APTR Object);
extern void NotifySubscribers(OBJECTPTR Object, LONG Action, APTR Args, ERROR Error);
extern ERROR StrReadLocale(CSTRING Key, CSTRING * Value);
extern CSTRING UTF8ValidEncoding(CSTRING String, CSTRING Encoding);
extern ERROR ProcessMessages(PMF Flags, LONG TimeOut);
extern ERROR IdentifyFile(CSTRING Path, CLASSID * Class, CLASSID * SubClass);
extern ERROR ReallocMemory(APTR Memory, ULONG Size, APTR Address, MEMORYID * ID);
extern ERROR GetMessage(LONG Type, MSF Flags, APTR Buffer, LONG Size);
extern ERROR ReleaseMemory(MEMORYID MemoryID);
extern CLASSID ResolveClassName(CSTRING Name);
extern ERROR SendMessage(LONG Type, MSF Flags, APTR Data, LONG Size);
extern ERROR SetOwner(OBJECTPTR Object, OBJECTPTR Owner);
extern OBJECTPTR SetContext(OBJECTPTR Object);
extern ERROR SetField(OBJECTPTR Object, FIELD Field, ...);
extern CSTRING FieldName(ULONG FieldID);
extern ERROR ScanDir(struct DirInfo * Info);
extern ERROR SetName(OBJECTPTR Object, CSTRING Name);
extern void LogReturn(void);
extern ERROR StrCompare(CSTRING String1, CSTRING String2, LONG Length, STR Flags);
extern ERROR SubscribeAction(OBJECTPTR Object, LONG Action, FUNCTION * Callback);
extern ERROR SubscribeEvent(LARGE Event, FUNCTION * Callback, APTR Custom, APTR Handle);
extern ERROR SubscribeTimer(DOUBLE Interval, FUNCTION * Callback, APTR Subscription);
extern ERROR UpdateTimer(APTR Subscription, DOUBLE Interval);
extern ERROR UnsubscribeAction(OBJECTPTR Object, LONG Action);
extern void UnsubscribeEvent(APTR Handle);
extern ERROR BroadcastEvent(APTR Event, LONG EventSize);
extern void WaitTime(LONG Seconds, LONG MicroSeconds);
extern LARGE GetEventID(EVG Group, CSTRING SubGroup, CSTRING Event);
extern ULONG GenCRC32(ULONG CRC, APTR Data, ULONG Length);
extern LARGE GetResource(RES Resource);
extern LARGE SetResource(RES Resource, LARGE Value);
extern ERROR ScanMessages(LONG * Handle, LONG Type, APTR Buffer, LONG Size);
extern STT StrDatatype(CSTRING String);
extern void UnloadFile(struct CacheFile * Cache);
extern ERROR CreateFolder(CSTRING Path, PERMIT Permissions);
extern ERROR LoadFile(CSTRING Path, LDF Flags, struct CacheFile ** Cache);
extern ERROR SetVolume(CSTRING Name, CSTRING Path, CSTRING Icon, CSTRING Label, CSTRING Device, VOLUME Flags);
extern ERROR DeleteVolume(CSTRING Name);
extern ERROR MoveFile(CSTRING Source, CSTRING Dest, FUNCTION * Callback);
extern ERROR UpdateMessage(LONG Message, LONG Type, APTR Data, LONG Size);
extern ERROR AddMsgHandler(APTR Custom, LONG MsgType, FUNCTION * Routine, struct MsgHandler ** Handle);
extern ERROR QueueAction(LONG Action, OBJECTID Object, APTR Args);
extern LARGE PreciseTime(void);
extern ERROR OpenDir(CSTRING Path, RDF Flags, struct DirInfo ** Info);
extern OBJECTPTR GetObjectPtr(OBJECTID Object);
extern struct Field * FindField(OBJECTPTR Object, ULONG FieldID, APTR Target);
extern CSTRING GetErrorMsg(ERROR Error);
extern struct Message * GetActionMsg(void);
extern ERROR FuncError(CSTRING Header, ERROR Error);
extern ERROR SetArray(OBJECTPTR Object, FIELD Field, APTR Array, LONG Elements);
extern ULONG StrHash(CSTRING String, LONG CaseSensitive);
extern ERROR LockObject(OBJECTPTR Object, LONG MilliSeconds);
extern void ReleaseObject(OBJECTPTR Object);
extern ERROR ActionThread(LONG Action, OBJECTPTR Object, APTR Args, FUNCTION * Callback, LONG Key);
extern ERROR AddInfoTag(struct FileInfo * Info, CSTRING Name, CSTRING Value);
extern void SetDefaultPermissions(LONG User, LONG Group, PERMIT Permissions);
extern void VLogF(VLF Flags, const char *Header, const char *Message, va_list Args);
extern LONG Base64Encode(struct pfBase64Encode * State, const void * Input, LONG InputSize, STRING Output, LONG OutputSize);
extern ERROR ReadInfoTag(struct FileInfo * Info, CSTRING Name, CSTRING * Value);
extern ERROR SetResourcePath(RP PathType, CSTRING Path);
extern objTask * CurrentTask(void);
extern CSTRING ResolveGroupID(LONG Group);
extern CSTRING ResolveUserID(LONG User);
extern ERROR CreateLink(CSTRING From, CSTRING To);
extern ERROR DeleteFile(CSTRING Path, FUNCTION * Callback);
extern LONG UTF8CharOffset(CSTRING String, LONG Offset);
extern LONG UTF8Length(CSTRING String);
extern LONG UTF8OffsetToChar(CSTRING String, LONG Offset);
extern LONG UTF8PrevLength(CSTRING String, LONG Offset);
extern LONG UTF8CharLength(CSTRING String);
extern ULONG UTF8ReadValue(CSTRING String, LONG * Length);
extern LONG UTF8WriteValue(LONG Value, STRING Buffer, LONG Size);
extern ERROR CopyFile(CSTRING Source, CSTRING Dest, FUNCTION * Callback);
extern ERROR WaitForObjects(PMF Flags, LONG TimeOut, struct ObjectSignal * ObjectSignals);
}
#endif // PARASOL_STATIC
#endif


//********************************************************************************************************************

#define PRIME_HASH 2654435761UL
#define END_FIELD FieldArray(NULL, 0)
#define FDEF static const struct FunctionField

template <class T> inline MEMORYID GetMemoryID(T &&A) {
   return ((MEMORYID *)A)[-2];
}

inline ERROR DeregisterFD(HOSTHANDLE Handle) {
   return RegisterFD(Handle, RFD::REMOVE|RFD::READ|RFD::WRITE|RFD::EXCEPT|RFD::ALWAYS_CALL, 0, 0);
}

#define DeleteMsg(a,b)  UpdateMessage(a,b,(APTR)-1,0,0)

inline OBJECTPTR GetParentContext() { return (OBJECTPTR)(MAXINT)GetResource(RES::PARENT_CONTEXT); }
inline APTR GetResourcePtr(RES ID) { return (APTR)(MAXINT)GetResource(ID); }

inline CSTRING to_cstring(const std::string &A) { return A.c_str(); }
constexpr inline CSTRING to_cstring(CSTRING A) { return A; }

template <class T, class U> inline ERROR StrMatch(T &&A, U &&B) {
   return StrCompare(to_cstring(A), to_cstring(B), 0, STR::MATCH_LEN);
}

template <class T> inline LONG StrCopy(T &&Source, STRING Dest, LONG Length = 0x7fffffff)
{
   auto src = to_cstring(Source);
   if ((Length > 0) and (src) and (Dest)) {
      LONG i = 0;
      while (*src) {
         if (i IS Length) {
            Dest[i-1] = 0;
            return i;
         }
         Dest[i++] = *src++;
      }

      Dest[i] = 0;
      return i;
   }
   else return 0;
}

#ifndef PRV_CORE_DATA

inline ERROR ReleaseMemory(const void *Address) {
   if (!Address) return ERR_NullArgs;
   return ReleaseMemory(((MEMORYID *)Address)[-2]);
}

inline ERROR FreeResource(const void *Address) {
   if (!Address) return ERR_NullArgs;
   return FreeResource(((LONG *)Address)[-2]);
}

inline ERROR AllocMemory(LONG Size, MEM Flags, APTR Address) {
   return AllocMemory(Size, Flags, (APTR *)Address, NULL);
}

template<class T> inline ERROR NewObject(LARGE ClassID, T **Result) {
   return NewObject(ClassID, NF::NIL, Result);
}

inline ERROR MemoryIDInfo(MEMORYID ID, struct MemInfo * MemInfo) {
   return MemoryIDInfo(ID,MemInfo,sizeof(struct MemInfo));
}

inline ERROR MemoryPtrInfo(APTR Address, struct MemInfo * MemInfo) {
   return MemoryPtrInfo(Address,MemInfo,sizeof(struct MemInfo));
}

inline ERROR QueueAction(LONG Action, OBJECTID ObjectID) {
   return QueueAction(Action, ObjectID, NULL);
}

template <class T, class U> inline ERROR StrCompare(T &&A, U &&B, LONG Length = 0, STR Flags = STR::NIL) {
   return StrCompare(to_cstring(A), to_cstring(B), Length, Flags);
}

inline ULONG StrHash(const std::string Value) {
   return StrHash(Value.c_str(), FALSE);
}

template <class T> inline ERROR SetArray(OBJECTPTR Object, FIELD FieldID, pf::vector<T> Array)
{
   return SetArray(Object, FieldID, Array.data(), Array.size());
}

template <class T> inline ERROR SetArray(OBJECTPTR Object, FIELD FieldID, std::vector<T> Array)
{
   return SetArray(Object, FieldID, Array.data(), Array.size());
}
#endif

typedef std::map<std::string, std::string> ConfigKeys;
typedef std::pair<std::string, ConfigKeys> ConfigGroup;
typedef std::vector<ConfigGroup> ConfigGroups;

inline void CopyMemory(const void *Src, APTR Dest, LONG Length)
{
   memmove(Dest, Src, Length);
}

[[nodiscard]] inline LONG StrSearchCase(CSTRING Keyword, CSTRING String)
{
   LONG i;
   LONG pos = 0;
   while (String[pos]) {
      for (i=0; Keyword[i]; i++) if (String[pos+i] != Keyword[i]) break;
      if (!Keyword[i]) return pos;
      for (++pos; (String[pos] & 0xc0) IS 0x80; pos++);
   }

   return -1;
}

[[nodiscard]] inline LONG StrSearch(CSTRING Keyword, CSTRING String)
{
   LONG i;
   LONG pos = 0;
   while (String[pos]) {
      for (i=0; Keyword[i]; i++) if (std::toupper(String[pos+i]) != std::toupper(Keyword[i])) break;
      if (!Keyword[i]) return pos;
      for (++pos; (String[pos] & 0xc0) IS 0x80; pos++);
   }

   return -1;
}

[[nodiscard]] inline STRING StrClone(CSTRING String)
{
   if (!String) return NULL;

   auto len = LONG(strlen(String));
   STRING newstr;
   if (!AllocMemory(len+1, MEM::STRING, (APTR *)&newstr, NULL)) {
      CopyMemory(String, newstr, len+1);
      return newstr;
   }
   else return NULL;
}

[[nodiscard]] inline LONG StrLength(CSTRING String) {
   if (String) return LONG(strlen(String));
   else return 0;
}

template <class T> inline LARGE StrToInt(T &&String) {
   CSTRING str = to_cstring(String);
   if (!str) return 0;

   while ((*str < '0') or (*str > '9')) { // Ignore any leading characters
      if (!str[0]) return 0;
      else if (*str IS '-') break;
      else if (*str IS '+') break;
      else str++;
   }

   return strtoll(str, NULL, 0);
}

template <class T> inline DOUBLE StrToFloat(T &&String) {
   CSTRING str = to_cstring(String);
   if (!str) return 0;

   while ((*str != '-') and (*str != '.') and ((*str < '0') or (*str > '9'))) {
      if (!*str) return 0;
      str++;
   }

   return strtod(str, NULL);
}

// NB: Prefer std::to_string(value) where viable to get the std::string of a number.

inline LONG IntToStr(LARGE Integer, STRING String, LONG StringSize) {
   auto str = std::to_string(Integer);
   auto len = LONG(str.copy(String, StringSize-1));
   String[len] = 0;
   return len;
}

inline ERROR ClearMemory(APTR Memory, LONG Length) {
   if (!Memory) return ERR_NullArgs;
   memset(Memory, 0, Length); // memset() is assumed to be optimised by the compiler.
   return ERR_Okay;
}

namespace pf {

static THREADVAR LONG _tlUniqueThreadID = 0;

[[nodiscard]] inline LONG _get_thread_id(void) {
   if (_tlUniqueThreadID) return _tlUniqueThreadID;
   _tlUniqueThreadID = GetResource(RES::THREAD_ID);
   return _tlUniqueThreadID;
}

// For extremely verbose debug logs, run cmake with -DPARASOL_VLOG=ON

class Log { // C++ wrapper for Parasol's log functionality
   private:
      LONG branches = 0;

   public:
      CSTRING header;

      Log() {
         header = NULL;
      }

      Log(CSTRING Header) {
         header = Header;
      }

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

      void debranch() {
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

      void extmsg(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // Extended API message
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF::EXTAPI, header, Message, arg);
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

      void debug(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF::DEBUG, header, Message, arg);
         va_end(arg);
      }

      void function(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // Equivalent to branch() but without a new branch being created
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF::API|VLF::FUNCTION, header, Message, arg);
         va_end(arg);
      }

      ERROR error(ERROR Code) { // Technically a warning
         FuncError(header, Code);
         return Code;
      }

      ERROR warning(ERROR Code) {
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
};

} // namespace

//********************************************************************************************************************
// Header used for all objects.

struct BaseClass { // Must be 64-bit aligned
   union {
      objMetaClass *Class;          // [Public] Class pointer
      class extMetaClass *ExtClass; // [Private] Internal version of the class pointer
   };
   APTR     ChildPrivate;        // Address for the ChildPrivate structure, if allocated
   APTR     CreatorMeta;         // The creator (via NewObject) is permitted to store a custom data pointer here.
   std::atomic_uint64_t NotifyFlags; // Action subscription flags - space for 64 actions max
   OBJECTID UID;                 // Unique object identifier
   OBJECTID OwnerID;             // The owner of this object
   NF       Flags;               // Object flags
   volatile LONG  ThreadID;      // Managed by locking functions
   char Name[MAX_NAME_LEN];      // The name of the object (optional)
   std::atomic_uchar ThreadPending; // ActionThread() increments this.
   std::atomic_char Queue;       // Counter of locks gained by incQueue()
   std::atomic_char SleepQueue;  // For the use of LockObject() only
   std::atomic_bool Locked;      // Set if locked by AccessObject()/LockObject()
   BYTE ActionDepth;             // Incremented each time an action or method is called on the object

   inline bool initialised() { return (Flags & NF::INITIALISED) != NF::NIL; }
   inline bool defined(NF pFlags) { return (Flags & pFlags) != NF::NIL; }
   inline bool isSubClass();
   inline OBJECTID ownerID() { return OwnerID; }
   inline NF flags() { return Flags; }

   CSTRING className();

   inline bool collecting() { // Is object being freed or marked for collection?
      return (Flags & (NF::FREE|NF::COLLECT)) != NF::NIL;
   }

   inline bool terminating() { // Is object currently being freed?
      return (Flags & NF::FREE) != NF::NIL;
   }

   // Use lock() to quickly obtain an object lock without a call to LockObject()

   inline ERROR lock() {
      if (++Queue IS 1) {
         ThreadID = pf::_get_thread_id();
         return ERR_Okay;
      }
      else {
         if (ThreadID IS pf::_get_thread_id()) return ERR_Okay; // If this is for the same thread then it's a nested lock, so there's no issue.
         --Queue; // Restore the lock count
         return LockObject(this, -1); // Can fail if object is marked for deletion.
      }
   }

   inline void unlock() {
      // Prefer to use ReleaseObject() if there are threads that need to be woken
      if (SleepQueue.load() > 0) ReleaseObject(this);
      else --Queue;
   }

   inline bool hasOwner(OBJECTID ID) { // Return true if ID has ownership.
      auto oid = this->OwnerID;
      while ((oid) and (oid != ID)) oid = GetOwnerID(oid);
      return oid ? true : false;
   }

   inline ERROR set(ULONG FieldID, int Value)             { return SetField(this, (FIELD)FieldID|TLONG, Value); }
   inline ERROR set(ULONG FieldID, unsigned int Value)    { return SetField(this, (FIELD)FieldID|TLONG, Value); }
   inline ERROR set(ULONG FieldID, LARGE Value)           { return SetField(this, (FIELD)FieldID|TLARGE, Value); }
   inline ERROR set(ULONG FieldID, DOUBLE Value)          { return SetField(this, (FIELD)FieldID|TDOUBLE, Value); }
   inline ERROR set(ULONG FieldID, const FUNCTION *Value) { return SetField(this, (FIELD)FieldID|TFUNCTION, Value); }
   inline ERROR set(ULONG FieldID, const char *Value)     { return SetField(this, (FIELD)FieldID|TSTRING, Value); }
   inline ERROR set(ULONG FieldID, const unsigned char *Value) { return SetField(this, (FIELD)FieldID|TSTRING, Value); }
   inline ERROR set(ULONG FieldID, const std::string &Value)   { return SetField(this, (FIELD)FieldID|TSTRING, Value.c_str()); }
   inline ERROR set(ULONG FieldID, const Variable *Value)      { return SetField(this, (FIELD)FieldID|TVAR, Value); }
   // Works both for regular data pointers and function pointers if field is defined correctly.
   inline ERROR set(ULONG FieldID, const void *Value) { return SetField(this, (FIELD)FieldID|TPTR, Value); }

   inline ERROR setPercentage(ULONG FieldID, DOUBLE Value) { return SetField(this, (FIELD)FieldID|TDOUBLE|TPERCENT, Value); }

   inline ERROR get(ULONG FieldID, LONG *Value)     { return GetField(this, (FIELD)FieldID|TLONG, Value); }
   inline ERROR get(ULONG FieldID, LARGE *Value)    { return GetField(this, (FIELD)FieldID|TLARGE, Value); }
   inline ERROR get(ULONG FieldID, DOUBLE *Value)   { return GetField(this, (FIELD)FieldID|TDOUBLE, Value); }
   inline ERROR get(ULONG FieldID, STRING *Value)   { return GetField(this, (FIELD)FieldID|TSTRING, Value); }
   inline ERROR get(ULONG FieldID, CSTRING *Value)  { return GetField(this, (FIELD)FieldID|TSTRING, Value); }
   inline ERROR get(ULONG FieldID, Variable *Value) { return GetField(this, (FIELD)FieldID|TVAR, Value); }
   inline ERROR getPtr(ULONG FieldID, APTR Value)   { return GetField(this, (FIELD)FieldID|TPTR, Value); }
   inline ERROR getPercentage(ULONG FieldID, DOUBLE *Value) { return GetField(this, (FIELD)FieldID|TDOUBLE|TPERCENT, Value); }

   template <typename... Args> ERROR setFields(Args&&... pFields) {
      pf::Log log("setFields");

      lock();

      std::initializer_list<pf::FieldValue> Fields = { std::forward<Args>(pFields)... };

      for (auto &f : Fields) {
         OBJECTPTR target;
         if (auto field = FindField(this, f.FieldID, &target)) {
            if ((!(field->Flags & (FD_INIT|FD_WRITE))) and (CurrentContext() != target)) {
               log.warning("Field \"%s\" of class %s is not writeable.", field->Name, className());
            }
            else if ((field->Flags & FD_INIT) and (target->initialised()) and (CurrentContext() != target)) {
               log.warning("Field \"%s\" of class %s is init-only.", field->Name, className());
            }
            else {
               if (target != this) target->lock();

               ERROR error;
               if (f.Type & (FD_POINTER|FD_STRING|FD_ARRAY|FD_FUNCTION|FD_VARIABLE)) {
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

               if ((error) and (error != ERR_NoSupport)) {
                  log.warning("(%s:%d) Failed to set field %s (error #%d).", target->className(), target->UID, field->Name, error);
                  unlock();
                  return error;
               }
            }
         }
         else {
            log.warning("Field %s is not supported by class %s.", FieldName(f.FieldID), className());
            unlock();
            return ERR_UnsupportedField;
         }
      }

      unlock();
      return ERR_Okay;
   }

} __attribute__ ((aligned (8)));

namespace pf {

template<class T = BaseClass>
class Create {
   private:
      T *obj;

   public:
      ERROR error;

      // Return an unscoped direct object pointer.  NB: Globals are still tracked

      template <typename... Args> static T * global(Args&&... Fields) {
         pf::Create<T> object = { std::forward<Args>(Fields)... };
         if (object.ok()) {
            auto result = *object;
            object.obj = NULL;
            return result;
         }
         else return NULL;
      }

      // Return an unscoped integral object (suitable for class allocations only).

      template <typename... Args> static T * integral(Args&&... Fields) {
         pf::Create<T> object({ std::forward<Args>(Fields)... }, NF::INTEGRAL);
         if (object.ok()) return *object;
         else return NULL;
      }

      // Return an unscoped and untracked object pointer.

      template <typename... Args> static T * untracked(Args&&... Fields) {
         pf::Create<T> object({ std::forward<Args>(Fields)... }, NF::UNTRACKED);
         if (object.ok()) return *object;
         else return NULL;
      }

      // Create a scoped object that is not initialised.

      Create(NF Flags = NF::NIL) : obj(NULL), error(ERR_NewObject) {
         if (!NewObject(T::CLASS_ID, Flags, (BaseClass **)&obj)) {
            error = ERR_Okay;
         }
      }

      // Create a scoped object that is fully initialised.

      Create(std::initializer_list<FieldValue> Fields, NF Flags = NF::NIL) : obj(NULL), error(ERR_Failed) {
         pf::Log log("CreateObject");
         log.branch(T::CLASS_NAME);

         if (!NewObject(T::CLASS_ID, NF::SUPPRESS_LOG|Flags, (BaseClass **)&obj)) {
            for (auto &f : Fields) {
               OBJECTPTR target;
               if (auto field = FindField(obj, f.FieldID, &target)) {
                  if ((!(field->Flags & (FD_INIT|FD_WRITE))) and (CurrentContext() != target)) {
                     error = log.warning(ERR_NoFieldAccess);
                     return;
                  }
                  else if ((field->Flags & FD_INIT) and (target->initialised()) and (CurrentContext() != target)) {
                     error = log.warning(ERR_NoFieldAccess);
                     return;
                  }
                  else {
                     target->lock();

                     if (f.Type & (FD_POINTER|FD_STRING|FD_ARRAY|FD_FUNCTION|FD_VARIABLE)) {
                        error = field->WriteValue(target, field, f.Type, f.Pointer, 0);
                     }
                     else if (f.Type & (FD_DOUBLE|FD_FLOAT)) {
                        error = field->WriteValue(target, field, f.Type, &f.Double, 1);
                     }
                     else if (f.Type & FD_LARGE) {
                        error = field->WriteValue(target, field, f.Type, &f.Large, 1);
                     }
                     else {
                        error = field->WriteValue(target, field, f.Type, &f.Long, 1);
                     }

                     target->unlock();

                     // NB: NoSupport is considered a 'soft' error that does not warrant failure.

                     if ((error) and (error != ERR_NoSupport)) return;
                  }
               }
               else {
                  log.warning("Field %s is not supported by class %s.", FieldName(f.FieldID), T::CLASS_NAME);
                  error = log.warning(ERR_UnsupportedField);
                  return;
               }
            }

            if ((error = InitObject(obj))) {
               FreeResource(obj->UID);
               obj = NULL;
            }
         }
         else error = ERR_NewObject;
      }

      ~Create() {
         if (obj) {
            if (obj->initialised()) {
               if ((obj->BaseClass::Flags & (NF::UNTRACKED|NF::INTEGRAL)) != NF::NIL)  {
                  return; // Detected a successfully created unscoped object
               }
            }
            FreeResource(obj->UID);
            obj = NULL;
         }
      }

      T * operator->() { return obj; }; // Promotes underlying methods and fields
      T * & operator*() { return obj; }; // To allow object pointer referencing when calling functions

      inline bool ok() { return error == ERR_Okay; }
};
}

inline OBJECTID CurrentTaskID() { return ((OBJECTPTR)CurrentTask())->UID; }
inline APTR SetResourcePtr(RES Res, APTR Value) { return (APTR)(MAXINT)(SetResource(Res, (MAXINT)Value)); }

// Action and Notification Structures

struct acClipboard     { CLIPMODE Mode; };
struct acCopyData      { OBJECTPTR Dest; };
struct acCustom        { LONG Number; CSTRING String; };
struct acDataFeed      { OBJECTPTR Object; DATA Datatype; const void *Buffer; LONG Size; };
struct acDragDrop      { OBJECTPTR Source; LONG Item; CSTRING Datatype; };
struct acDraw          { LONG X; LONG Y; LONG Width; LONG Height; };
struct acGetVar        { CSTRING Field; STRING Buffer; LONG Size; };
struct acMove          { DOUBLE DeltaX; DOUBLE DeltaY; DOUBLE DeltaZ; };
struct acMoveToPoint   { DOUBLE X; DOUBLE Y; DOUBLE Z; MTF Flags; };
struct acNewChild      { OBJECTPTR Object; };
struct acNewOwner      { OBJECTPTR NewOwner; };
struct acRead          { APTR Buffer; LONG Length; LONG Result; };
struct acRedimension   { DOUBLE X; DOUBLE Y; DOUBLE Z; DOUBLE Width; DOUBLE Height; DOUBLE Depth; };
struct acRedo          { LONG Steps; };
struct acRename        { CSTRING Name; };
struct acResize        { DOUBLE Width; DOUBLE Height; DOUBLE Depth; };
struct acSaveImage     { OBJECTPTR Dest; union { CLASSID ClassID; CLASSID Class; }; };
struct acSaveToObject  { OBJECTPTR Dest; union { CLASSID ClassID; CLASSID Class; }; };
struct acScroll        { DOUBLE DeltaX; DOUBLE DeltaY; DOUBLE DeltaZ; };
struct acScrollToPoint { DOUBLE X; DOUBLE Y; DOUBLE Z; STP Flags; };
struct acSeek          { DOUBLE Offset; SEEK Position; };
struct acSelectArea    { DOUBLE X; DOUBLE Y; DOUBLE Width; DOUBLE Height; };
struct acSetVar        { CSTRING Field; CSTRING Value; };
struct acUndo          { LONG Steps; };
struct acWrite         { CPTR Buffer; LONG Length; LONG Result; };

// Action Macros

inline ERROR acActivate(OBJECTPTR Object) { return Action(AC_Activate,Object,NULL); }
inline ERROR acClear(OBJECTPTR Object) { return Action(AC_Clear,Object,NULL); }
inline ERROR acDeactivate(OBJECTPTR Object) { return Action(AC_Deactivate,Object,NULL); }
inline ERROR acDisable(OBJECTPTR Object) { return Action(AC_Disable,Object,NULL); }
inline ERROR acDraw(OBJECTPTR Object) { return Action(AC_Draw,Object,NULL); }
inline ERROR acEnable(OBJECTPTR Object) { return Action(AC_Enable,Object,NULL); }
inline ERROR acFlush(OBJECTPTR Object) { return Action(AC_Flush,Object,NULL); }
inline ERROR acFocus(OBJECTPTR Object) { return Action(AC_Focus,Object,NULL); }
inline ERROR acHide(OBJECTPTR Object) { return Action(AC_Hide,Object,NULL); }
inline ERROR acLock(OBJECTPTR Object) { return Action(AC_Lock,Object,NULL); }
inline ERROR acLostFocus(OBJECTPTR Object) { return Action(AC_LostFocus,Object,NULL); }
inline ERROR acMoveToBack(OBJECTPTR Object) { return Action(AC_MoveToBack,Object,NULL); }
inline ERROR acMoveToFront(OBJECTPTR Object) { return Action(AC_MoveToFront,Object,NULL); }
inline ERROR acNext(OBJECTPTR Object) { return Action(AC_Next,Object,NULL); }
inline ERROR acPrev(OBJECTPTR Object) { return Action(AC_Prev,Object,NULL); }
inline ERROR acQuery(OBJECTPTR Object) { return Action(AC_Query,Object,NULL); }
inline ERROR acRefresh(OBJECTPTR Object) { return Action(AC_Refresh, Object, NULL); }
inline ERROR acReset(OBJECTPTR Object) { return Action(AC_Reset,Object,NULL); }
inline ERROR acSaveSettings(OBJECTPTR Object) { return Action(AC_SaveSettings,Object,NULL); }
inline ERROR acShow(OBJECTPTR Object) { return Action(AC_Show,Object,NULL); }
inline ERROR acSignal(OBJECTPTR Object) { return Action(AC_Signal,Object,NULL); }
inline ERROR acSort(OBJECTPTR Object) { return Action(AC_Sort,Object,NULL); }
inline ERROR acUnlock(OBJECTPTR Object) { return Action(AC_Unlock,Object,NULL); }

inline ERROR acClipboard(OBJECTPTR Object, CLIPMODE Mode) {
   struct acClipboard args = { Mode };
   return Action(AC_Clipboard, Object, &args);
}

inline ERROR acDragDrop(OBJECTPTR Object, OBJECTPTR Source, LONG Item, CSTRING Datatype) {
   struct acDragDrop args = { Source, Item, Datatype };
   return Action(AC_DragDrop, Object, &args);
}

inline ERROR acDrawArea(OBJECTPTR Object, LONG X, LONG Y, LONG Width, LONG Height) {
   struct acDraw args = { X, Y, Width, Height };
   return Action(AC_Draw, Object, &args);
}

inline ERROR acDataFeed(OBJECTPTR Object, OBJECTPTR Sender, DATA Datatype, const void *Buffer, LONG Size) {
   struct acDataFeed args = { Sender, Datatype, Buffer, Size };
   return Action(AC_DataFeed, Object, &args);
}

inline ERROR acGetVar(OBJECTPTR Object, CSTRING FieldName, STRING Buffer, LONG Size) {
   struct acGetVar args = { FieldName, Buffer, Size };
   ERROR error = Action(AC_GetVar, Object, &args);
   if ((error) and (Buffer)) Buffer[0] = 0;
   return error;
}

inline ERROR acMove(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Z) {
   struct acMove args = { X, Y, Z };
   return Action(AC_Move, Object, &args);
}

inline ERROR acRead(OBJECTPTR Object, APTR Buffer, LONG Bytes, LONG *Read) {
   ERROR error;
   struct acRead read = { (BYTE *)Buffer, Bytes };
   if (!(error = Action(AC_Read, Object, &read))) {
      if (Read) *Read = read.Result;
      return ERR_Okay;
   }
   else {
      if (Read) *Read = 0;
      return error;
   }
}

inline ERROR acRedo(OBJECTPTR Object, LONG Steps = 1) {
   struct acRedo args = { Steps };
   return Action(AC_Redo, Object, &args);
}

inline ERROR acRedimension(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Z, DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
   struct acRedimension args = { X, Y, Z, Width, Height, Depth };
   return Action(AC_Redimension, Object, &args);
}

inline ERROR acRename(OBJECTPTR Object, CSTRING Name) {
   struct acRename args = { Name };
   return Action(AC_Rename, Object, &args);
}

inline ERROR acResize(OBJECTPTR Object, DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
   struct acResize args = { Width, Height, Depth };
   return Action(AC_Resize, Object, &args);
}

inline ERROR acScroll(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Z) {
   struct acScroll args = { X, Y, Z };
   return Action(AC_Scroll, Object, &args);
}

inline ERROR acScrollToPoint(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Z, STP Flags) {
   struct acScrollToPoint args = { X, Y, Z, Flags };
   return Action(AC_ScrollToPoint, Object, &args);
}

inline ERROR acMoveToPoint(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Z, MTF Flags) {
   struct acMoveToPoint moveto = { X, Y, Z, Flags };
   return Action(AC_MoveToPoint, Object, &moveto);
}

inline ERROR acSaveImage(OBJECTPTR Object, OBJECTPTR Dest, CLASSID ClassID = 0) {
   struct acSaveImage args = { Dest, { ClassID } };
   return Action(AC_SaveImage, Object, &args);
}

inline ERROR acSaveToObject(OBJECTPTR Object, OBJECTPTR Dest, CLASSID ClassID = 0) {
   struct acSaveToObject args = { Dest, { ClassID } };
   return Action(AC_SaveToObject, Object, &args);
}

inline ERROR acSeek(OBJECTPTR Object, DOUBLE Offset, SEEK Position) {
   struct acSeek args = { Offset, Position };
   return Action(AC_Seek, Object, &args);
}

inline ERROR acSetVars(OBJECTPTR Object, CSTRING tags, ...) {
   struct acSetVar args;
   va_list list;

   va_start(list, tags);
   while ((args.Field = va_arg(list, STRING)) != TAGEND) {
      args.Value = va_arg(list, STRING);
      if (Action(AC_SetVar, Object, &args) != ERR_Okay) {
         va_end(list);
         return ERR_Failed;
      }
   }
   va_end(list);
   return ERR_Okay;
}

inline ERROR acUndo(OBJECTPTR Object, LONG Steps) {
   struct acUndo args = { Steps };
   return Action(AC_Undo, Object, &args);
}

inline ERROR acWrite(OBJECTPTR Object, CPTR Buffer, LONG Bytes, LONG *Result) {
   ERROR error;
   struct acWrite write = { (BYTE *)Buffer, Bytes };
   if (!(error = Action(AC_Write, Object, &write))) {
      if (Result) *Result = write.Result;
   }
   else if (Result) *Result = 0;
   return error;
}

inline LONG acWriteResult(OBJECTPTR Object, CPTR Buffer, LONG Bytes) {
   struct acWrite write = { (BYTE *)Buffer, Bytes };
   if (!Action(AC_Write, Object, &write)) return write.Result;
   else return 0;
}

#define acSeekStart(a,b)    acSeek((a),(b),SEEK::START)
#define acSeekEnd(a,b)      acSeek((a),(b),SEEK::END)
#define acSeekCurrent(a,b)  acSeek((a),(b),SEEK::CURRENT)

inline ERROR acSelectArea(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height) {
   struct acSelectArea area = { X, Y, Width, Height };
   return Action(AC_SelectArea, Object, &area);
}

inline ERROR acSetVar(OBJECTPTR Object, CSTRING FieldName, CSTRING Value) {
   struct acSetVar args = { FieldName, Value };
   return Action(AC_SetVar, Object, &args);
}

#define GetVar(a,b,c,d)  acGetVar(a,b,c,d)
#define SetVar(a,b,c)    acSetVar(a,b,c)

// MetaClass class definition

#define VER_METACLASS (1.000000)

// MetaClass methods

#define MT_mcFindField -1

struct mcFindField { LONG ID; struct Field * Field; objMetaClass * Source;  };

INLINE ERROR mcFindField(APTR Ob, LONG ID, struct Field ** Field, objMetaClass ** Source) {
   struct mcFindField args = { ID, (struct Field *)0, (objMetaClass *)0 };
   ERROR error = Action(MT_mcFindField, (OBJECTPTR)Ob, &args);
   if (Field) *Field = args.Field;
   if (Source) *Source = args.Source;
   return(error);
}


class objMetaClass : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_METACLASS;
   static constexpr CSTRING CLASS_NAME = "MetaClass";

   using create = pf::Create<objMetaClass>;

   DOUBLE  ClassVersion;                // The version number of the class.
   const struct FieldArray * Fields;    // Points to a field array that describes the class' object structure.
   struct Field * Dictionary;           // Returns a field lookup table sorted by field IDs.
   CSTRING ClassName;                   // The name of the represented class.
   CSTRING FileExtension;               // Describes the file extension represented by the class.
   CSTRING FileDescription;             // Describes the file type represented by the class.
   CSTRING FileHeader;                  // Defines a string expression that will allow relevant file data to be matched to the class.
   CSTRING Path;                        // The path to the module binary that represents the class.
   LONG    Size;                        // The total size of the object structure represented by the MetaClass.
   CLF     Flags;                       // Optional flag settings.
   CLASSID ClassID;                     // Specifies the ID of a class object.
   CLASSID BaseClassID;                 // Specifies the base class ID of a class object.
   LONG    OpenCount;                   // The total number of active objects that are linked back to the MetaClass.
   CCF     Category;                    // The system category that a class belongs to.

   // Customised field setting

   inline ERROR setClassVersion(const DOUBLE Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->ClassVersion = Value;
      return ERR_Okay;
   }

   inline ERROR setFields(const struct FieldArray * Value, LONG Elements) {
      auto target = this;
      auto field = &this->Class->Dictionary[23];
      return field->WriteValue(target, field, 0x00001510, Value, Elements);
   }

   template <class T> inline ERROR setClassName(T && Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->ClassName = Value;
      return ERR_Okay;
   }

   template <class T> inline ERROR setFileExtension(T && Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->FileExtension = Value;
      return ERR_Okay;
   }

   template <class T> inline ERROR setFileDescription(T && Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->FileDescription = Value;
      return ERR_Okay;
   }

   template <class T> inline ERROR setFileHeader(T && Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->FileHeader = Value;
      return ERR_Okay;
   }

   template <class T> inline ERROR setPath(T && Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Path = Value;
      return ERR_Okay;
   }

   inline ERROR setSize(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Size = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const CLF Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setClass(const CLASSID Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->ClassID = Value;
      return ERR_Okay;
   }

   inline ERROR setBaseClass(const CLASSID Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->BaseClassID = Value;
      return ERR_Okay;
   }

   inline ERROR setCategory(const CCF Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Category = Value;
      return ERR_Okay;
   }

   inline ERROR setMethods(const APTR Value, LONG Elements) {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, 0x00001510, Value, Elements);
   }

   inline ERROR setActions(APTR Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x08000400, Value, 1);
   }

   template <class T> inline ERROR setName(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, 0x08810500, to_cstring(Value), 1);
   }

};

inline bool BaseClass::isSubClass() { return Class->ClassID != Class->BaseClassID; }

// StorageDevice class definition

#define VER_STORAGEDEVICE (1.000000)

class objStorageDevice : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_STORAGEDEVICE;
   static constexpr CSTRING CLASS_NAME = "StorageDevice";

   using create = pf::Create<objStorageDevice>;

   DEVICE DeviceFlags;    // These read-only flags identify the type of device and its features.
   LARGE  DeviceSize;     // The storage size of the device in bytes, without accounting for the file system format.
   LARGE  BytesFree;      // Total amount of storage space that is available, measured in bytes.
   LARGE  BytesUsed;      // Total amount of storage space in use.

   // Customised field setting

   template <class T> inline ERROR setVolume(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[4];
      return field->WriteValue(target, field, 0x08800504, to_cstring(Value), 1);
   }

};

// File class definition

#define VER_FILE (1.200000)

// File methods

#define MT_FlStartStream -1
#define MT_FlStopStream -2
#define MT_FlDelete -3
#define MT_FlMove -4
#define MT_FlCopy -5
#define MT_FlSetDate -6
#define MT_FlReadLine -7
#define MT_FlBufferContent -8
#define MT_FlNext -9
#define MT_FlWatch -10

struct flStartStream { OBJECTID SubscriberID; FL Flags; LONG Length;  };
struct flDelete { FUNCTION * Callback;  };
struct flMove { CSTRING Dest; FUNCTION * Callback;  };
struct flCopy { CSTRING Dest; FUNCTION * Callback;  };
struct flSetDate { LONG Year; LONG Month; LONG Day; LONG Hour; LONG Minute; LONG Second; FDT Type;  };
struct flReadLine { STRING Result;  };
struct flNext { objFile * File;  };
struct flWatch { FUNCTION * Callback; LARGE Custom; MFF Flags;  };

INLINE ERROR flStartStream(APTR Ob, OBJECTID SubscriberID, FL Flags, LONG Length) {
   struct flStartStream args = { SubscriberID, Flags, Length };
   return(Action(MT_FlStartStream, (OBJECTPTR)Ob, &args));
}

#define flStopStream(obj) Action(MT_FlStopStream,(obj),0)

INLINE ERROR flDelete(APTR Ob, FUNCTION * Callback) {
   struct flDelete args = { Callback };
   return(Action(MT_FlDelete, (OBJECTPTR)Ob, &args));
}

INLINE ERROR flMove(APTR Ob, CSTRING Dest, FUNCTION * Callback) {
   struct flMove args = { Dest, Callback };
   return(Action(MT_FlMove, (OBJECTPTR)Ob, &args));
}

INLINE ERROR flCopy(APTR Ob, CSTRING Dest, FUNCTION * Callback) {
   struct flCopy args = { Dest, Callback };
   return(Action(MT_FlCopy, (OBJECTPTR)Ob, &args));
}

INLINE ERROR flSetDate(APTR Ob, LONG Year, LONG Month, LONG Day, LONG Hour, LONG Minute, LONG Second, FDT Type) {
   struct flSetDate args = { Year, Month, Day, Hour, Minute, Second, Type };
   return(Action(MT_FlSetDate, (OBJECTPTR)Ob, &args));
}

#define flBufferContent(obj) Action(MT_FlBufferContent,(obj),0)

INLINE ERROR flNext(APTR Ob, objFile ** File) {
   struct flNext args = { (objFile *)0 };
   ERROR error = Action(MT_FlNext, (OBJECTPTR)Ob, &args);
   if (File) *File = args.File;
   return(error);
}

INLINE ERROR flWatch(APTR Ob, FUNCTION * Callback, LARGE Custom, MFF Flags) {
   struct flWatch args = { Callback, Custom, Flags };
   return(Action(MT_FlWatch, (OBJECTPTR)Ob, &args));
}


class objFile : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_FILE;
   static constexpr CSTRING CLASS_NAME = "File";

   using create = pf::Create<objFile>;

   LARGE    Position; // The current read/write byte position in a file.
   FL       Flags;    // File flags and options.
   LONG     Static;   // Set to TRUE if a file object should be static.
   OBJECTID TargetID; // Specifies a surface ID to target for user feedback and dialog boxes.
   BYTE *   Buffer;   // Points to the internal data buffer if the file content is held in memory.

   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERROR init() { return InitObject(this); }
   inline ERROR query() { return Action(AC_Query, this, NULL); }
   template <class T, class U> ERROR read(APTR Buffer, T Size, U *Result) {
      static_assert(std::is_integral<U>::value, "Result value must be an integer type");
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      ERROR error;
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      if (!(error = Action(AC_Read, this, &read))) *Result = static_cast<U>(read.Result);
      else *Result = 0;
      return error;
   }
   template <class T> ERROR read(APTR Buffer, T Size) {
      static_assert(std::is_integral<T>::value, "Size value must be an integer type");
      const LONG bytes = (Size > 0x7fffffff) ? 0x7fffffff : Size;
      struct acRead read = { (BYTE *)Buffer, bytes };
      return Action(AC_Read, this, &read);
   }
   inline ERROR rename(CSTRING Name) {
      struct acRename args = { Name };
      return Action(AC_Rename, this, &args);
   }
   inline ERROR reset() { return Action(AC_Reset, this, NULL); }
   inline ERROR seek(DOUBLE Offset, SEEK Position = SEEK::CURRENT) {
      struct acSeek args = { Offset, Position };
      return Action(AC_Seek, this, &args);
   }
   inline ERROR seekStart(DOUBLE Offset)   { return seek(Offset, SEEK::START); }
   inline ERROR seekEnd(DOUBLE Offset)     { return seek(Offset, SEEK::END); }
   inline ERROR seekCurrent(DOUBLE Offset) { return seek(Offset, SEEK::CURRENT); }
   inline ERROR write(CPTR Buffer, LONG Size, LONG *Result = NULL) {
      ERROR error;
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (!(error = Action(AC_Write, this, &write))) {
         if (Result) *Result = write.Result;
      }
      else if (Result) *Result = 0;
      return error;
   }
   inline ERROR write(std::string Buffer, LONG *Result = NULL) {
      ERROR error;
      struct acWrite write = { (BYTE *)Buffer.c_str(), LONG(Buffer.size()) };
      if (!(error = Action(AC_Write, this, &write))) {
         if (Result) *Result = write.Result;
      }
      else if (Result) *Result = 0;
      return error;
   }
   inline LONG writeResult(CPTR Buffer, LONG Size) {
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (!Action(AC_Write, this, &write)) return write.Result;
      else return 0;
   }

   // Customised field setting

   inline ERROR setPosition(const LARGE Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, FD_LARGE, &Value, 1);
   }

   inline ERROR setFlags(const FL Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setStatic(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Static = Value;
      return ERR_Okay;
   }

   inline ERROR setTarget(const OBJECTID Value) {
      this->TargetID = Value;
      return ERR_Okay;
   }

   inline ERROR setDate(APTR Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, 0x08000310, Value, 1);
   }

   inline ERROR setCreated(APTR Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[20];
      return field->WriteValue(target, field, 0x08000310, Value, 1);
   }

   template <class T> inline ERROR setPath(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

   inline ERROR setPermissions(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[22];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setSize(const LARGE Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[17];
      return field->WriteValue(target, field, FD_LARGE, &Value, 1);
   }

   template <class T> inline ERROR setLink(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setUser(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setGroup(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[4];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

};

// Config class definition

#define VER_CONFIG (1.000000)

// Config methods

#define MT_CfgReadValue -1
#define MT_CfgSet -2
#define MT_CfgWriteValue -3
#define MT_CfgDeleteKey -4
#define MT_CfgDeleteGroup -5
#define MT_CfgGetGroupFromIndex -6
#define MT_CfgSortByKey -7
#define MT_CfgMergeFile -9
#define MT_CfgMerge -10

struct cfgReadValue { CSTRING Group; CSTRING Key; CSTRING Data;  };
struct cfgSet { CSTRING Group; CSTRING Key; CSTRING Data;  };
struct cfgWriteValue { CSTRING Group; CSTRING Key; CSTRING Data;  };
struct cfgDeleteKey { CSTRING Group; CSTRING Key;  };
struct cfgDeleteGroup { CSTRING Group;  };
struct cfgGetGroupFromIndex { LONG Index; CSTRING Group;  };
struct cfgSortByKey { CSTRING Key; LONG Descending;  };
struct cfgMergeFile { CSTRING Path;  };
struct cfgMerge { OBJECTPTR Source;  };

INLINE ERROR cfgReadValue(APTR Ob, CSTRING Group, CSTRING Key, CSTRING * Data) {
   struct cfgReadValue args = { Group, Key, (CSTRING)0 };
   ERROR error = Action(MT_CfgReadValue, (OBJECTPTR)Ob, &args);
   if (Data) *Data = args.Data;
   return(error);
}

INLINE ERROR cfgSet(APTR Ob, CSTRING Group, CSTRING Key, CSTRING Data) {
   struct cfgSet args = { Group, Key, Data };
   return(Action(MT_CfgSet, (OBJECTPTR)Ob, &args));
}

INLINE ERROR cfgWriteValue(APTR Ob, CSTRING Group, CSTRING Key, CSTRING Data) {
   struct cfgWriteValue args = { Group, Key, Data };
   return(Action(MT_CfgWriteValue, (OBJECTPTR)Ob, &args));
}

INLINE ERROR cfgDeleteKey(APTR Ob, CSTRING Group, CSTRING Key) {
   struct cfgDeleteKey args = { Group, Key };
   return(Action(MT_CfgDeleteKey, (OBJECTPTR)Ob, &args));
}

INLINE ERROR cfgDeleteGroup(APTR Ob, CSTRING Group) {
   struct cfgDeleteGroup args = { Group };
   return(Action(MT_CfgDeleteGroup, (OBJECTPTR)Ob, &args));
}

INLINE ERROR cfgGetGroupFromIndex(APTR Ob, LONG Index, CSTRING * Group) {
   struct cfgGetGroupFromIndex args = { Index, (CSTRING)0 };
   ERROR error = Action(MT_CfgGetGroupFromIndex, (OBJECTPTR)Ob, &args);
   if (Group) *Group = args.Group;
   return(error);
}

INLINE ERROR cfgSortByKey(APTR Ob, CSTRING Key, LONG Descending) {
   struct cfgSortByKey args = { Key, Descending };
   return(Action(MT_CfgSortByKey, (OBJECTPTR)Ob, &args));
}

INLINE ERROR cfgMergeFile(APTR Ob, CSTRING Path) {
   struct cfgMergeFile args = { Path };
   return(Action(MT_CfgMergeFile, (OBJECTPTR)Ob, &args));
}

INLINE ERROR cfgMerge(APTR Ob, OBJECTPTR Source) {
   struct cfgMerge args = { Source };
   return(Action(MT_CfgMerge, (OBJECTPTR)Ob, &args));
}


class objConfig : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_CONFIG;
   static constexpr CSTRING CLASS_NAME = "Config";

   using create = pf::Create<objConfig>;

   STRING Path;         // Set this field to the location of the source configuration file.
   STRING KeyFilter;    // Set this field to enable key filtering.
   STRING GroupFilter;  // Set this field to enable group filtering.
   CNF    Flags;        // Optional flags may be set here.
   public:
   ConfigGroups *Groups;

   // For C++ only, these read variants avoid method calls for speed, but apply identical logic.

   inline ERROR read(CSTRING pGroup, CSTRING pKey, DOUBLE *pValue) {
      for (auto& [group, keys] : Groups[0]) {
         if ((pGroup) and (group.compare(pGroup))) continue;
         if (!pKey) {
            *pValue = strtod(keys.cbegin()->second.c_str(), NULL);
            return ERR_Okay;
         }
         else if (keys.contains(pKey)) {
            *pValue = strtod(keys[pKey].c_str(), NULL);
            return ERR_Okay;
         }
      }
      return ERR_Search;
   }

   inline ERROR read(CSTRING pGroup, CSTRING pKey, LONG *pValue) {
      for (auto& [group, keys] : Groups[0]) {
         if ((pGroup) and (group.compare(pGroup))) continue;
         if (!pKey) {
            *pValue = strtol(keys.cbegin()->second.c_str(), NULL, 0);
            return ERR_Okay;
         }
         else if (keys.contains(pKey)) {
            *pValue = strtol(keys[pKey].c_str(), NULL, 0);
            return ERR_Okay;
         }
      }
      return ERR_Search;
   }

   inline ERROR read(CSTRING pGroup, CSTRING pKey, std::string &pValue) {
      for (auto& [group, keys] : Groups[0]) {
         if ((pGroup) and (group.compare(pGroup))) continue;
         if (!pKey) {
            pValue = keys.cbegin()->second;
            return ERR_Okay;
         }
         else if (keys.contains(pKey)) {
            pValue = keys[pKey];
            return ERR_Okay;
         }
      }
      return ERR_Search;
   }

   inline ERROR write(CSTRING Group, CSTRING Key, CSTRING Value) {
      struct cfgWriteValue write = { Group, Key, Value };
      return Action(MT_CfgWriteValue, this, &write);
   }
   inline ERROR write(CSTRING Group, CSTRING Key, STRING Value) {
      struct cfgWriteValue write = { Group, Key, Value };
      return Action(MT_CfgWriteValue, this, &write);
   }
   inline ERROR write(CSTRING Group, CSTRING Key, std::string Value) {
      struct cfgWriteValue write = { Group, Key, Value.c_str() };
      return Action(MT_CfgWriteValue, this, &write);
   }
   template <class T> inline ERROR write(CSTRING Group, CSTRING Key, T Value) {
      auto str = std::to_string(Value);
      struct cfgWriteValue write = { Group, Key, str.c_str() };
      return Action(MT_CfgWriteValue, this, &write);
   }

   // Action stubs

   inline ERROR clear() { return Action(AC_Clear, this, NULL); }
   inline ERROR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERROR flush() { return Action(AC_Flush, this, NULL); }
   inline ERROR init() { return InitObject(this); }
   inline ERROR saveSettings() { return Action(AC_SaveSettings, this, NULL); }
   inline ERROR saveToObject(OBJECTPTR Dest, CLASSID ClassID = 0) {
      struct acSaveToObject args = { Dest, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }
   inline ERROR sort() { return Action(AC_Sort, this, NULL); }

   // Customised field setting

   template <class T> inline ERROR setPath(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setKeyFilter(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[3];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setGroupFilter(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[7];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setFlags(const CNF Value) {
      this->Flags = Value;
      return ERR_Okay;
   }

};

inline ERROR cfgRead(OBJECTPTR Self, CSTRING Group, CSTRING Key, DOUBLE *Value)
{
   ERROR error;
   struct cfgReadValue read = { .Group = Group, .Key = Key };
   if (!(error = Action(MT_CfgReadValue, Self, &read))) {
      *Value = strtod(read.Data, NULL);
      return ERR_Okay;
   }
   else { *Value = 0; return error; }
}

inline ERROR cfgRead(OBJECTPTR Self, CSTRING Group, CSTRING Key, LONG *Value)
{
   ERROR error;
   struct cfgReadValue read = { .Group = Group, .Key = Key };
   if (!(error = Action(MT_CfgReadValue, Self, &read))) {
      *Value = strtol(read.Data, NULL, 0);
      return ERR_Okay;
   }
   else { *Value = 0; return error; }
}
// Script class definition

#define VER_SCRIPT (1.000000)

// Script methods

#define MT_ScExec -1
#define MT_ScDerefProcedure -2
#define MT_ScCallback -3
#define MT_ScGetProcedureID -4

struct scExec { CSTRING Procedure; const struct ScriptArg * Args; LONG TotalArgs;  };
struct scDerefProcedure { FUNCTION * Procedure;  };
struct scCallback { LARGE ProcedureID; const struct ScriptArg * Args; LONG TotalArgs; LONG Error;  };
struct scGetProcedureID { CSTRING Procedure; LARGE ProcedureID;  };

INLINE ERROR scExec(APTR Ob, CSTRING Procedure, const struct ScriptArg * Args, LONG TotalArgs) {
   struct scExec args = { Procedure, Args, TotalArgs };
   return(Action(MT_ScExec, (OBJECTPTR)Ob, &args));
}

INLINE ERROR scDerefProcedure(APTR Ob, FUNCTION * Procedure) {
   struct scDerefProcedure args = { Procedure };
   return(Action(MT_ScDerefProcedure, (OBJECTPTR)Ob, &args));
}

INLINE ERROR scCallback(APTR Ob, LARGE ProcedureID, const struct ScriptArg * Args, LONG TotalArgs, LONG * Error) {
   struct scCallback args = { ProcedureID, Args, TotalArgs, (LONG)0 };
   ERROR error = Action(MT_ScCallback, (OBJECTPTR)Ob, &args);
   if (Error) *Error = args.Error;
   return(error);
}

INLINE ERROR scGetProcedureID(APTR Ob, CSTRING Procedure, LARGE * ProcedureID) {
   struct scGetProcedureID args = { Procedure, (LARGE)0 };
   ERROR error = Action(MT_ScGetProcedureID, (OBJECTPTR)Ob, &args);
   if (ProcedureID) *ProcedureID = args.ProcedureID;
   return(error);
}


class objScript : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_SCRIPT;
   static constexpr CSTRING CLASS_NAME = "Script";

   using create = pf::Create<objScript>;

   OBJECTID TargetID;  // Reference to the default container that new script objects will be initialised to.
   SCF      Flags;     // Optional flags.
   ERROR    Error;     // If a script fails during execution, an error code may be readable here.
   LONG     CurrentLine; // Indicates the current line being executed when in debug mode.
   LONG     LineOffset; // For debugging purposes, this value is added to any message referencing a line number.

#ifdef PRV_SCRIPT
   LARGE    ProcedureID;          // For callbacks
   std::map<std::string, std::string> Vars; // Global parameters
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

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR dataFeed(OBJECTPTR Object, DATA Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { Object, Datatype, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERROR getVar(CSTRING FieldName, STRING Buffer, LONG Size) {
      struct acGetVar args = { FieldName, Buffer, Size };
      ERROR error = Action(AC_GetVar, this, &args);
      if ((error) and (Buffer)) Buffer[0] = 0;
      return error;
   }
   inline ERROR init() { return InitObject(this); }
   inline ERROR reset() { return Action(AC_Reset, this, NULL); }
   inline ERROR acSetVar(CSTRING FieldName, CSTRING Value) {
      struct acSetVar args = { FieldName, Value };
      return Action(AC_SetVar, this, &args);
   }

   // Customised field setting

   inline ERROR setTarget(const OBJECTID Value) {
      this->TargetID = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const SCF Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setLineOffset(const LONG Value) {
      this->LineOffset = Value;
      return ERR_Okay;
   }

   template <class T> inline ERROR setCacheFile(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setErrorString(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setWorkingPath(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[20];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setProcedure(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setName(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, 0x08810300, to_cstring(Value), 1);
   }

   inline ERROR setOwner(const OBJECTID Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   template <class T> inline ERROR setPath(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

   inline ERROR setResults(STRING * Value, LONG Elements) {
      auto target = this;
      auto field = &this->Class->Dictionary[8];
      return field->WriteValue(target, field, 0x08801300, Value, Elements);
   }

   template <class T> inline ERROR setStatement(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

};

// Task class definition

#define VER_TASK (1.000000)

// Task methods

#define MT_TaskExpunge -1
#define MT_TaskAddArgument -2
#define MT_TaskQuit -3
#define MT_TaskGetEnv -4
#define MT_TaskSetEnv -5

struct taskAddArgument { CSTRING Argument;  };
struct taskGetEnv { CSTRING Name; CSTRING Value;  };
struct taskSetEnv { CSTRING Name; CSTRING Value;  };

#define taskExpunge(obj) Action(MT_TaskExpunge,(obj),0)

INLINE ERROR taskAddArgument(APTR Ob, CSTRING Argument) {
   struct taskAddArgument args = { Argument };
   return(Action(MT_TaskAddArgument, (OBJECTPTR)Ob, &args));
}

#define taskQuit(obj) Action(MT_TaskQuit,(obj),0)

INLINE ERROR taskGetEnv(APTR Ob, CSTRING Name, CSTRING * Value) {
   struct taskGetEnv args = { Name, (CSTRING)0 };
   ERROR error = Action(MT_TaskGetEnv, (OBJECTPTR)Ob, &args);
   if (Value) *Value = args.Value;
   return(error);
}

INLINE ERROR taskSetEnv(APTR Ob, CSTRING Name, CSTRING Value) {
   struct taskSetEnv args = { Name, Value };
   return(Action(MT_TaskSetEnv, (OBJECTPTR)Ob, &args));
}


class objTask : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_TASK;
   static constexpr CSTRING CLASS_NAME = "Task";

   using create = pf::Create<objTask>;

   DOUBLE TimeOut;    // Limits the amount of time to wait for a launched process to return.
   TSF    Flags;      // Optional flags.
   LONG   ReturnCode; // The task's return code can be retrieved following execution.
   LONG   ProcessID;  // Reflects the process ID when an executable is launched.

   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR getVar(CSTRING FieldName, STRING Buffer, LONG Size) {
      struct acGetVar args = { FieldName, Buffer, Size };
      ERROR error = Action(AC_GetVar, this, &args);
      if ((error) and (Buffer)) Buffer[0] = 0;
      return error;
   }
   inline ERROR init() { return InitObject(this); }
   inline ERROR acSetVar(CSTRING FieldName, CSTRING Value) {
      struct acSetVar args = { FieldName, Value };
      return Action(AC_SetVar, this, &args);
   }
   inline ERROR write(CPTR Buffer, LONG Size, LONG *Result = NULL) {
      ERROR error;
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (!(error = Action(AC_Write, this, &write))) {
         if (Result) *Result = write.Result;
      }
      else if (Result) *Result = 0;
      return error;
   }
   inline ERROR write(std::string Buffer, LONG *Result = NULL) {
      ERROR error;
      struct acWrite write = { (BYTE *)Buffer.c_str(), LONG(Buffer.size()) };
      if (!(error = Action(AC_Write, this, &write))) {
         if (Result) *Result = write.Result;
      }
      else if (Result) *Result = 0;
      return error;
   }
   inline LONG writeResult(CPTR Buffer, LONG Size) {
      struct acWrite write = { (BYTE *)Buffer, Size };
      if (!Action(AC_Write, this, &write)) return write.Result;
      else return 0;
   }

   // Customised field setting

   inline ERROR setTimeOut(const DOUBLE Value) {
      this->TimeOut = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const TSF Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setReturnCode(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[9];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setProcess(const LONG Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->ProcessID = Value;
      return ERR_Okay;
   }

   template <class T> inline ERROR setArgs(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[13];
      return field->WriteValue(target, field, 0x08800200, to_cstring(Value), 1);
   }

   inline ERROR setParameters(pf::vector<std::string> *Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[16];
      return field->WriteValue(target, field, 0x08805300, Value, LONG(Value->size()));
   }

   inline ERROR setErrorCallback(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERROR setExitCallback(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[8];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERROR setInputCallback(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[17];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   template <class T> inline ERROR setLaunchPath(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setLocation(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setName(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setOutputCallback(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   template <class T> inline ERROR setPath(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[15];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setPriority(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

};

// Thread class definition

#define VER_THREAD (1.000000)

// Thread methods

#define MT_ThSetData -1

struct thSetData { APTR Data; LONG Size;  };

INLINE ERROR thSetData(APTR Ob, APTR Data, LONG Size) {
   struct thSetData args = { Data, Size };
   return(Action(MT_ThSetData, (OBJECTPTR)Ob, &args));
}


class objThread : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_THREAD;
   static constexpr CSTRING CLASS_NAME = "Thread";

   using create = pf::Create<objThread>;

   APTR  Data;       // Pointer to initialisation data for the thread.
   LONG  DataSize;   // The size of the buffer referenced in the Data field.
   LONG  StackSize;  // The stack size to allocate for the thread.
   ERROR Error;      // Reflects the error code returned by the thread routine.
   THF   Flags;      // Optional flags can be defined here.

   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR deactivate() { return Action(AC_Deactivate, this, NULL); }
   inline ERROR init() { return InitObject(this); }

   // Customised field setting

   inline ERROR setStackSize(const LONG Value) {
      this->StackSize = Value;
      return ERR_Okay;
   }

   inline ERROR setFlags(const THF Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setCallback(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[1];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   inline ERROR setRoutine(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

};

// Module class definition

#define VER_MODULE (1.000000)

// Module methods

#define MT_ModResolveSymbol -1

struct modResolveSymbol { CSTRING Name; APTR Address;  };

INLINE ERROR modResolveSymbol(APTR Ob, CSTRING Name, APTR * Address) {
   struct modResolveSymbol args = { Name, (APTR)0 };
   ERROR error = Action(MT_ModResolveSymbol, (OBJECTPTR)Ob, &args);
   if (Address) *Address = args.Address;
   return(error);
}


class objModule : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_MODULE;
   static constexpr CSTRING CLASS_NAME = "Module";

   using create = pf::Create<objModule>;

   const struct Function * FunctionList;    // Refers to a list of public functions exported by the module.
   APTR ModBase;                            // The Module's function base (jump table) must be read from this field.
   class RootModule * Root;                 // For internal use only.
   struct ModHeader * Header;               // For internal usage only.
   MOF  Flags;                              // Optional flags.
   public:
   static ERROR load(std::string Name, OBJECTPTR *Module = NULL, APTR Functions = NULL) {
      if (auto module = objModule::create::global(pf::FieldValue(FID_Name, Name.c_str()))) {
         #ifdef PARASOL_STATIC
            if (Module) *Module = module;
            if (Functions) ((APTR *)Functions)[0] = NULL;
            return ERR_Okay;
         #else
            APTR functionbase;
            if (!module->getPtr(FID_ModBase, &functionbase)) {
               if (Module) *Module = module;
               if (Functions) ((APTR *)Functions)[0] = functionbase;
               return ERR_Okay;
            }
            else return ERR_GetField;
         #endif
      }
      else return ERR_CreateObject;
   }

   // Action stubs

   inline ERROR init() { return InitObject(this); }

   // Customised field setting

   inline ERROR setFunctionList(const struct Function * Value) {
      this->FunctionList = Value;
      return ERR_Okay;
   }

   inline ERROR setHeader(struct ModHeader * Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[0];
      return field->WriteValue(target, field, 0x08000500, Value, 1);
   }

   inline ERROR setFlags(const MOF Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Flags = Value;
      return ERR_Okay;
   }

   template <class T> inline ERROR setName(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[5];
      return field->WriteValue(target, field, 0x08800500, to_cstring(Value), 1);
   }

};

// Time class definition

#define VER_TIME (1.000000)

// Time methods

#define MT_TmSetTime -1

#define tmSetTime(obj) Action(MT_TmSetTime,(obj),0)


class objTime : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_TIME;
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
   LONG  MilliSecond;   // Millisecond (0 - 999)
   LONG  MicroSecond;   // Microsecond (0 - 999999)

   // Action stubs

   inline ERROR query() { return Action(AC_Query, this, NULL); }
   inline ERROR init() { return InitObject(this); }

   // Customised field setting

   inline ERROR setSystemTime(const LARGE Value) {
      this->SystemTime = Value;
      return ERR_Okay;
   }

   inline ERROR setYear(const LONG Value) {
      this->Year = Value;
      return ERR_Okay;
   }

   inline ERROR setMonth(const LONG Value) {
      this->Month = Value;
      return ERR_Okay;
   }

   inline ERROR setDay(const LONG Value) {
      this->Day = Value;
      return ERR_Okay;
   }

   inline ERROR setHour(const LONG Value) {
      this->Hour = Value;
      return ERR_Okay;
   }

   inline ERROR setMinute(const LONG Value) {
      this->Minute = Value;
      return ERR_Okay;
   }

   inline ERROR setSecond(const LONG Value) {
      this->Second = Value;
      return ERR_Okay;
   }

   inline ERROR setTimeZone(const LONG Value) {
      this->TimeZone = Value;
      return ERR_Okay;
   }

   inline ERROR setDayOfWeek(const LONG Value) {
      this->DayOfWeek = Value;
      return ERR_Okay;
   }

   inline ERROR setMilliSecond(const LONG Value) {
      this->MilliSecond = Value;
      return ERR_Okay;
   }

   inline ERROR setMicroSecond(const LONG Value) {
      this->MicroSecond = Value;
      return ERR_Okay;
   }

};

// Compression class definition

#define VER_COMPRESSION (1.000000)

// Compression methods

#define MT_CmpCompressBuffer -1
#define MT_CmpCompressFile -2
#define MT_CmpDecompressBuffer -3
#define MT_CmpDecompressFile -4
#define MT_CmpRemoveFile -5
#define MT_CmpCompressStream -6
#define MT_CmpDecompressStream -7
#define MT_CmpCompressStreamStart -8
#define MT_CmpCompressStreamEnd -9
#define MT_CmpDecompressStreamEnd -10
#define MT_CmpDecompressStreamStart -11
#define MT_CmpDecompressObject -12
#define MT_CmpScan -13
#define MT_CmpFind -14

struct cmpCompressBuffer { APTR Input; LONG InputSize; APTR Output; LONG OutputSize; LONG Result;  };
struct cmpCompressFile { CSTRING Location; CSTRING Path;  };
struct cmpDecompressBuffer { APTR Input; APTR Output; LONG OutputSize; LONG Result;  };
struct cmpDecompressFile { CSTRING Path; CSTRING Dest; LONG Flags;  };
struct cmpRemoveFile { CSTRING Path;  };
struct cmpCompressStream { APTR Input; LONG Length; FUNCTION * Callback; APTR Output; LONG OutputSize;  };
struct cmpDecompressStream { APTR Input; LONG Length; FUNCTION * Callback; APTR Output; LONG OutputSize;  };
struct cmpCompressStreamEnd { FUNCTION * Callback; APTR Output; LONG OutputSize;  };
struct cmpDecompressStreamEnd { FUNCTION * Callback;  };
struct cmpDecompressObject { CSTRING Path; OBJECTPTR Object;  };
struct cmpScan { CSTRING Folder; CSTRING Filter; FUNCTION * Callback;  };
struct cmpFind { CSTRING Path; STR Flags; struct CompressedItem * Item;  };

INLINE ERROR cmpCompressBuffer(APTR Ob, APTR Input, LONG InputSize, APTR Output, LONG OutputSize, LONG * Result) {
   struct cmpCompressBuffer args = { Input, InputSize, Output, OutputSize, (LONG)0 };
   ERROR error = Action(MT_CmpCompressBuffer, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR cmpCompressFile(APTR Ob, CSTRING Location, CSTRING Path) {
   struct cmpCompressFile args = { Location, Path };
   return(Action(MT_CmpCompressFile, (OBJECTPTR)Ob, &args));
}

INLINE ERROR cmpDecompressBuffer(APTR Ob, APTR Input, APTR Output, LONG OutputSize, LONG * Result) {
   struct cmpDecompressBuffer args = { Input, Output, OutputSize, (LONG)0 };
   ERROR error = Action(MT_CmpDecompressBuffer, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR cmpDecompressFile(APTR Ob, CSTRING Path, CSTRING Dest, LONG Flags) {
   struct cmpDecompressFile args = { Path, Dest, Flags };
   return(Action(MT_CmpDecompressFile, (OBJECTPTR)Ob, &args));
}

INLINE ERROR cmpRemoveFile(APTR Ob, CSTRING Path) {
   struct cmpRemoveFile args = { Path };
   return(Action(MT_CmpRemoveFile, (OBJECTPTR)Ob, &args));
}

INLINE ERROR cmpCompressStream(APTR Ob, APTR Input, LONG Length, FUNCTION * Callback, APTR Output, LONG OutputSize) {
   struct cmpCompressStream args = { Input, Length, Callback, Output, OutputSize };
   return(Action(MT_CmpCompressStream, (OBJECTPTR)Ob, &args));
}

INLINE ERROR cmpDecompressStream(APTR Ob, APTR Input, LONG Length, FUNCTION * Callback, APTR Output, LONG OutputSize) {
   struct cmpDecompressStream args = { Input, Length, Callback, Output, OutputSize };
   return(Action(MT_CmpDecompressStream, (OBJECTPTR)Ob, &args));
}

#define cmpCompressStreamStart(obj) Action(MT_CmpCompressStreamStart,(obj),0)

INLINE ERROR cmpCompressStreamEnd(APTR Ob, FUNCTION * Callback, APTR Output, LONG OutputSize) {
   struct cmpCompressStreamEnd args = { Callback, Output, OutputSize };
   return(Action(MT_CmpCompressStreamEnd, (OBJECTPTR)Ob, &args));
}

INLINE ERROR cmpDecompressStreamEnd(APTR Ob, FUNCTION * Callback) {
   struct cmpDecompressStreamEnd args = { Callback };
   return(Action(MT_CmpDecompressStreamEnd, (OBJECTPTR)Ob, &args));
}

#define cmpDecompressStreamStart(obj) Action(MT_CmpDecompressStreamStart,(obj),0)

INLINE ERROR cmpDecompressObject(APTR Ob, CSTRING Path, OBJECTPTR Object) {
   struct cmpDecompressObject args = { Path, Object };
   return(Action(MT_CmpDecompressObject, (OBJECTPTR)Ob, &args));
}

INLINE ERROR cmpScan(APTR Ob, CSTRING Folder, CSTRING Filter, FUNCTION * Callback) {
   struct cmpScan args = { Folder, Filter, Callback };
   return(Action(MT_CmpScan, (OBJECTPTR)Ob, &args));
}

INLINE ERROR cmpFind(APTR Ob, CSTRING Path, STR Flags, struct CompressedItem ** Item) {
   struct cmpFind args = { Path, Flags, (struct CompressedItem *)0 };
   ERROR error = Action(MT_CmpFind, (OBJECTPTR)Ob, &args);
   if (Item) *Item = args.Item;
   return(error);
}


class objCompression : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_COMPRESSION;
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

   inline ERROR flush() { return Action(AC_Flush, this, NULL); }
   inline ERROR init() { return InitObject(this); }

   // Customised field setting

   inline ERROR setOutput(const OBJECTID Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->OutputID = Value;
      return ERR_Okay;
   }

   inline ERROR setCompressionLevel(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[6];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   inline ERROR setFlags(const CMF Value) {
      this->Flags = Value;
      return ERR_Okay;
   }

   inline ERROR setSegmentSize(const LONG Value) {
      this->SegmentSize = Value;
      return ERR_Okay;
   }

   inline ERROR setPermissions(const PERMIT Value) {
      this->Permissions = Value;
      return ERR_Okay;
   }

   inline ERROR setWindowBits(const LONG Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[14];
      return field->WriteValue(target, field, FD_LONG, &Value, 1);
   }

   template <class T> inline ERROR setArchiveName(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[18];
      return field->WriteValue(target, field, 0x08800200, to_cstring(Value), 1);
   }

   template <class T> inline ERROR setPath(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[12];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

   inline ERROR setFeedback(FUNCTION Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[17];
      return field->WriteValue(target, field, FD_FUNCTION, &Value, 1);
   }

   template <class T> inline ERROR setPassword(T && Value) {
      auto target = this;
      auto field = &this->Class->Dictionary[7];
      return field->WriteValue(target, field, 0x08800300, to_cstring(Value), 1);
   }

};

// CompressedStream class definition

#define VER_COMPRESSEDSTREAM (1.000000)

class objCompressedStream : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_COMPRESSEDSTREAM;
   static constexpr CSTRING CLASS_NAME = "CompressedStream";

   using create = pf::Create<objCompressedStream>;

   LARGE     TotalOutput; // A live counter of total bytes that have been output by the stream.
   OBJECTPTR Input;      // An input object that will supply data for decompression.
   OBJECTPTR Output;     // A target object that will receive data compressed by the stream.
   CF        Format;     // The format of the compressed stream.  The default is GZIP.

   // Customised field setting

   inline ERROR setInput(OBJECTPTR Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Input = Value;
      return ERR_Okay;
   }

   inline ERROR setOutput(OBJECTPTR Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Output = Value;
      return ERR_Okay;
   }

   inline ERROR setFormat(const CF Value) {
      if (this->initialised()) return ERR_NoFieldAccess;
      this->Format = Value;
      return ERR_Okay;
   }

};

#ifndef PRV_CORE

// Note that the length of the data is only needed when messaging between processes, so we can skip it for these
// direct-access data channel macros.

#define acDataContent(a,b)  acDataFeed((a),0,DATA::CONTENT,(b),0)
#define acDataXML(a,b)      acDataFeed((a),0,DATA::XML,(b),0)
#define acDataText(a,b)     acDataFeed((a),0,DATA::TEXT,(b),0)

inline ERROR acCustom(OBJECTID ObjectID, LONG Number, CSTRING String) {
   struct acCustom args = { Number, String };
   return ActionMsg(AC_Custom, ObjectID, &args);
}

inline ERROR acDataFeed(OBJECTID ObjectID, OBJECTPTR Sender, DATA Datatype, const APTR Data, LONG Size) {
   struct acDataFeed channel = { Sender, Datatype, Data, Size };
   return ActionMsg(AC_DataFeed, ObjectID, &channel);
}

inline ERROR acDragDrop(OBJECTID ObjectID, OBJECTPTR Source, LONG Item, CSTRING Datatype) {
   struct acDragDrop args = { Source, Item, Datatype };
   return ActionMsg(AC_DragDrop, ObjectID, &args);
}

inline ERROR acDrawArea(OBJECTID ObjectID, LONG X, LONG Y, LONG Width, LONG Height) {
   struct acDraw draw = { X, Y, Width, Height };
   return ActionMsg(AC_Draw, ObjectID, &draw);
}

inline ERROR acMove(OBJECTID ObjectID, DOUBLE X, DOUBLE Y, DOUBLE Z = 0) {
   struct acMove move = { X, Y, Z };
   return ActionMsg(AC_Move, ObjectID, &move);
}

inline ERROR acMoveToPoint(OBJECTID ObjectID, DOUBLE X, DOUBLE Y, DOUBLE Z = 0, MTF Flags = MTF::X|MTF::Y) {
   struct acMoveToPoint moveto = { X, Y, Z, Flags };
   return ActionMsg(AC_MoveToPoint, ObjectID, &moveto);
}

inline ERROR acRedimension(OBJECTID ObjectID, DOUBLE X, DOUBLE Y, DOUBLE Z, DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
   struct acRedimension resize = { X, Y, Z, Width, Height, Depth };
   return ActionMsg(AC_Redimension, ObjectID, &resize);
}

inline ERROR acResize(OBJECTID ObjectID, DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
   struct acResize resize = { Width, Height, Depth };
   return ActionMsg(AC_Resize, ObjectID, &resize);
}

inline ERROR acScrollToPoint(OBJECTID ObjectID, DOUBLE X, DOUBLE Y, DOUBLE Z = 0, STP Flags = STP::X|STP::Y) {
   struct acScrollToPoint scroll = { X, Y, Z, Flags };
   return ActionMsg(AC_ScrollToPoint, ObjectID, &scroll);
}

inline ERROR acScroll(OBJECTID ObjectID, DOUBLE X, DOUBLE Y, DOUBLE Z = 0) {
   struct acScroll scroll = { X, Y, Z };
   return ActionMsg(AC_Scroll, ObjectID, &scroll);
}

inline ERROR acSelectArea(OBJECTID ObjectID, DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height) {
   struct acSelectArea area = { X, Y, Width, Height };
   return ActionMsg(AC_SelectArea, ObjectID, &area);
}

inline ERROR acActivate(OBJECTID ObjectID) { return ActionMsg(AC_Activate, ObjectID, NULL); }
inline ERROR acClear(OBJECTID ObjectID) { return ActionMsg(AC_Clear, ObjectID, NULL); }
inline ERROR acDisable(OBJECTID ObjectID) { return ActionMsg(AC_Disable, ObjectID, NULL); }
inline ERROR acDraw(OBJECTID ObjectID) { return ActionMsg(AC_Draw, ObjectID, NULL); }
inline ERROR acEnable(OBJECTID ObjectID) { return ActionMsg(AC_Enable, ObjectID, NULL); }
inline ERROR acFlush(OBJECTID ObjectID) { return ActionMsg(AC_Flush, ObjectID, NULL); }
inline ERROR acFocus(OBJECTID ObjectID) { return ActionMsg(AC_Focus, ObjectID, NULL); }
inline ERROR acHide(OBJECTID ObjectID) { return ActionMsg(AC_Hide, ObjectID, NULL); }
inline ERROR acLostFocus(OBJECTID ObjectID) { return ActionMsg(AC_LostFocus, ObjectID, NULL); }
inline ERROR acMoveToBack(OBJECTID ObjectID) { return ActionMsg(AC_MoveToBack, ObjectID, NULL); }
inline ERROR acMoveToFront(OBJECTID ObjectID) { return ActionMsg(AC_MoveToFront, ObjectID, NULL); }
inline ERROR acQuery(OBJECTID ObjectID) { return ActionMsg(AC_Query, ObjectID, NULL); }
inline ERROR acRefresh(OBJECTID ObjectID) { return ActionMsg(AC_Refresh, ObjectID, NULL); }
inline ERROR acSaveSettings(OBJECTID ObjectID) { return ActionMsg(AC_SaveSettings, ObjectID, NULL); }
inline ERROR acShow(OBJECTID ObjectID) { return ActionMsg(AC_Show, ObjectID, NULL); }

inline ERROR acWrite(OBJECTID ObjectID, CPTR Buffer, LONG Bytes) {
   struct acWrite write = { (BYTE *)Buffer, Bytes };
   return ActionMsg(AC_Write, ObjectID, &write);
}

inline FIELD ResolveField(CSTRING Field) {
   return StrHash(Field, FALSE);
}

#endif // PRV_CORE

#ifdef __unix__
#include <pthread.h>
#endif

#ifdef __system__

struct ActionMessage {
   OBJECTID ObjectID;        // The object that is to receive the action
   LONG  Time;
   ACTIONID ActionID;        // ID of the action or method to execute
   bool SendArgs;            //

   // Action arguments follow this structure in a buffer
};

#endif

// Event support

struct rkEvent {
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
   char  Group[32];    // Group name in the config file
   char  Class[32];      // Class identifier (USB)
   union {
      char Product[40]; // Name of product or the hardware device
      char Device[40];
   };
   char Vendor[40];     // Name of vendor
};

// File Methods.

inline CSTRING flReadLine(OBJECTPTR Object) {
   struct flReadLine args;
   if (!Action(MT_FlReadLine, Object, &args)) return args.Result;
   else return NULL;
}

// Read endian values from files and objects.

template<class T> ERROR flReadLE(OBJECTPTR Object, T *Result)
{
   UBYTE data[sizeof(T)];
   struct acRead read = { .Buffer = data, .Length = sizeof(T) };
   if (!Action(AC_Read, Object, &read)) {
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
         return ERR_Okay;
      }
      else return ERR_Read;
   }
   else return ERR_Read;
}

template<class T> ERROR flReadBE(OBJECTPTR Object, T *Result)
{
   UBYTE data[sizeof(T)];
   struct acRead read = { .Buffer = data, .Length = sizeof(T) };
   if (!Action(AC_Read, Object, &read)) {
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
         return ERR_Okay;
      }
      else return ERR_Read;
   }
   else return ERR_Read;
}

template <class R>
constexpr FUNCTION make_function_stdc(R Routine, OBJECTPTR Context = CurrentContext()) {
   FUNCTION func = { .Type = CALL_STDC, .StdC = { .Context = Context, .Routine = (APTR)Routine } };
   return func;
}

inline FUNCTION make_function_script(OBJECTPTR Script, LARGE Procedure) {
   FUNCTION func = { .Type = CALL_SCRIPT, .Script = { .Script = (OBJECTPTR)Script, .ProcedureID = Procedure } };
   return func;
}

inline CSTRING BaseClass::className() { return Class->ClassName; }
