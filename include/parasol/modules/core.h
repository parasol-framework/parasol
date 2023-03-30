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
#endif

class objStorageDevice;
class objFile;
class objConfig;
class objScript;
class objMetaClass;
class objTask;
class objThread;
class objModule;
class objTime;
class objCompression;
class objCompressedStream;

#ifdef _WIN32

#define NETMSG_START 0
#define NETMSG_END 1

#endif

// Clipboard modes

#define CLIPMODE_CUT 0x00000001
#define CLIPMODE_COPY 0x00000002
#define CLIPMODE_PASTE 0x00000004

// Seek positions

#define SEEK_START 0
#define SEEK_CURRENT 1
#define SEEK_END 2
#define SEEK_RELATIVE 3

#define DEVICE_COMPACT_DISC 0x00000001
#define DEVICE_HARD_DISK 0x00000002
#define DEVICE_FLOPPY_DISK 0x00000004
#define DEVICE_READ 0x00000008
#define DEVICE_WRITE 0x00000010
#define DEVICE_REMOVEABLE 0x00000020
#define DEVICE_REMOVABLE 0x00000020
#define DEVICE_SOFTWARE 0x00000040
#define DEVICE_NETWORK 0x00000080
#define DEVICE_TAPE 0x00000100
#define DEVICE_PRINTER 0x00000200
#define DEVICE_SCANNER 0x00000400
#define DEVICE_TEMPORARY 0x00000800
#define DEVICE_MEMORY 0x00001000
#define DEVICE_MODEM 0x00002000
#define DEVICE_USB 0x00004000
#define DEVICE_PRINTER_3D 0x00008000
#define DEVICE_SCANNER_3D 0x00010000

// Class categories

#define CCF_COMMAND 0x00000001
#define CCF_DRAWABLE 0x00000002
#define CCF_EFFECT 0x00000004
#define CCF_FILESYSTEM 0x00000008
#define CCF_GRAPHICS 0x00000010
#define CCF_GUI 0x00000020
#define CCF_IO 0x00000040
#define CCF_SYSTEM 0x00000080
#define CCF_TOOL 0x00000100
#define CCF_AUDIO 0x00000200
#define CCF_DATA 0x00000400
#define CCF_MISC 0x00000800
#define CCF_NETWORK 0x00001000
#define CCF_MULTIMEDIA 0x00002000

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

#define PERMIT_READ 0x00000001
#define PERMIT_USER_READ 0x00000001
#define PERMIT_WRITE 0x00000002
#define PERMIT_USER_WRITE 0x00000002
#define PERMIT_EXEC 0x00000004
#define PERMIT_USER_EXEC 0x00000004
#define PERMIT_DELETE 0x00000008
#define PERMIT_USER 0x0000000f
#define PERMIT_GROUP_READ 0x00000010
#define PERMIT_GROUP_WRITE 0x00000020
#define PERMIT_GROUP_EXEC 0x00000040
#define PERMIT_GROUP_DELETE 0x00000080
#define PERMIT_GROUP 0x000000f0
#define PERMIT_OTHERS_READ 0x00000100
#define PERMIT_EVERYONE_READ 0x00000111
#define PERMIT_ALL_READ 0x00000111
#define PERMIT_OTHERS_WRITE 0x00000200
#define PERMIT_ALL_WRITE 0x00000222
#define PERMIT_EVERYONE_WRITE 0x00000222
#define PERMIT_EVERYONE_READWRITE 0x00000333
#define PERMIT_OTHERS_EXEC 0x00000400
#define PERMIT_ALL_EXEC 0x00000444
#define PERMIT_EVERYONE_EXEC 0x00000444
#define PERMIT_OTHERS_DELETE 0x00000800
#define PERMIT_EVERYONE_DELETE 0x00000888
#define PERMIT_ALL_DELETE 0x00000888
#define PERMIT_OTHERS 0x00000f00
#define PERMIT_EVERYONE_ACCESS 0x00000fff
#define PERMIT_HIDDEN 0x00001000
#define PERMIT_ARCHIVE 0x00002000
#define PERMIT_PASSWORD 0x00004000
#define PERMIT_USERID 0x00008000
#define PERMIT_GROUPID 0x00010000
#define PERMIT_INHERIT 0x00020000
#define PERMIT_OFFLINE 0x00040000
#define PERMIT_NETWORK 0x00080000

// Special qualifier flags

#define KQ_L_SHIFT 0x00000001
#define KQ_R_SHIFT 0x00000002
#define KQ_SHIFT 0x00000003
#define KQ_CAPS_LOCK 0x00000004
#define KQ_L_CONTROL 0x00000008
#define KQ_L_CTRL 0x00000008
#define KQ_R_CTRL 0x00000010
#define KQ_R_CONTROL 0x00000010
#define KQ_CTRL 0x00000018
#define KQ_CONTROL 0x00000018
#define KQ_L_ALT 0x00000020
#define KQ_ALTGR 0x00000040
#define KQ_R_ALT 0x00000040
#define KQ_ALT 0x00000060
#define KQ_INSTRUCTION_KEYS 0x00000078
#define KQ_L_COMMAND 0x00000080
#define KQ_R_COMMAND 0x00000100
#define KQ_COMMAND 0x00000180
#define KQ_QUALIFIERS 0x000001fb
#define KQ_NUM_PAD 0x00000200
#define KQ_REPEAT 0x00000400
#define KQ_RELEASED 0x00000800
#define KQ_PRESSED 0x00001000
#define KQ_NOT_PRINTABLE 0x00002000
#define KQ_INFO 0x00003c04
#define KQ_SCR_LOCK 0x00004000
#define KQ_NUM_LOCK 0x00008000
#define KQ_DEAD_KEY 0x00010000
#define KQ_WIN_CONTROL 0x00020000

// Memory types used by AllocMemory().  The lower 16 bits are stored with allocated blocks, the upper 16 bits are function-relative only.

#define MEM_DATA 0x00000000
#define MEM_MANAGED 0x00000001
#define MEM_VIDEO 0x00000002
#define MEM_TEXTURE 0x00000004
#define MEM_AUDIO 0x00000008
#define MEM_CODE 0x00000010
#define MEM_NO_POOL 0x00000020
#define MEM_TMP_LOCK 0x00000040
#define MEM_UNTRACKED 0x00000080
#define MEM_STRING 0x00000100
#define MEM_OBJECT 0x00000200
#define MEM_NO_LOCK 0x00000400
#define MEM_EXCLUSIVE 0x00000800
#define MEM_DELETE 0x00001000
#define MEM_NO_BLOCKING 0x00002000
#define MEM_NO_BLOCK 0x00002000
#define MEM_READ 0x00010000
#define MEM_WRITE 0x00020000
#define MEM_READ_WRITE 0x00030000
#define MEM_NO_CLEAR 0x00040000
#define MEM_HIDDEN 0x00100000
#define MEM_CALLER 0x00800000

// Event categories.

#define EVG_FILESYSTEM 1
#define EVG_NETWORK 2
#define EVG_SYSTEM 3
#define EVG_GUI 4
#define EVG_DISPLAY 5
#define EVG_IO 6
#define EVG_HARDWARE 7
#define EVG_AUDIO 8
#define EVG_USER 9
#define EVG_POWER 10
#define EVG_CLASS 11
#define EVG_APP 12
#define EVG_ANDROID 13
#define EVG_END 14

// Data codes

#define DATA_TEXT 1
#define DATA_RAW 2
#define DATA_DEVICE_INPUT 3
#define DATA_XML 4
#define DATA_AUDIO 5
#define DATA_RECORD 6
#define DATA_IMAGE 7
#define DATA_REQUEST 8
#define DATA_RECEIPT 9
#define DATA_FILE 10
#define DATA_CONTENT 11
#define DATA_INPUT_READY 12

// JTYPE flags are used to categorise input types.

#define JTYPE_SECONDARY 0x0001
#define JTYPE_ANCHORED 0x0002
#define JTYPE_DRAGGED 0x0004
#define JTYPE_FEEDBACK 0x0008
#define JTYPE_DIGITAL 0x0010
#define JTYPE_ANALOG 0x0020
#define JTYPE_EXT_MOVEMENT 0x0040
#define JTYPE_BUTTON 0x0080
#define JTYPE_MOVEMENT 0x0100
#define JTYPE_DBL_CLICK 0x0200
#define JTYPE_REPEATED 0x0400
#define JTYPE_DRAG_ITEM 0x0800

// JET constants are documented in GetInputEvent()

#define JET_DIGITAL_X 1
#define JET_DIGITAL_Y 2
#define JET_BUTTON_1 3
#define JET_LMB 3
#define JET_BUTTON_2 4
#define JET_RMB 4
#define JET_BUTTON_3 5
#define JET_MMB 5
#define JET_BUTTON_4 6
#define JET_BUTTON_5 7
#define JET_BUTTON_6 8
#define JET_BUTTON_7 9
#define JET_BUTTON_8 10
#define JET_BUTTON_9 11
#define JET_BUTTON_10 12
#define JET_TRIGGER_LEFT 13
#define JET_TRIGGER_RIGHT 14
#define JET_BUTTON_START 15
#define JET_BUTTON_SELECT 16
#define JET_LEFT_BUMPER_1 17
#define JET_LEFT_BUMPER_2 18
#define JET_RIGHT_BUMPER_1 19
#define JET_RIGHT_BUMPER_2 20
#define JET_ANALOG_X 21
#define JET_ANALOG_Y 22
#define JET_ANALOG_Z 23
#define JET_ANALOG2_X 24
#define JET_ANALOG2_Y 25
#define JET_ANALOG2_Z 26
#define JET_WHEEL 27
#define JET_WHEEL_TILT 28
#define JET_PEN_TILT_VERTICAL 29
#define JET_PEN_TILT_HORIZONTAL 30
#define JET_ABS_X 31
#define JET_ABS_Y 32
#define JET_ENTERED_SURFACE 33
#define JET_ENTERED 33
#define JET_LEFT_SURFACE 34
#define JET_LEFT 34
#define JET_PRESSURE 35
#define JET_DEVICE_TILT_X 36
#define JET_DEVICE_TILT_Y 37
#define JET_DEVICE_TILT_Z 38
#define JET_DISPLAY_EDGE 39
#define JET_END 40

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

struct InputEvent {
   const struct InputEvent * Next;    // Next event in the chain
   DOUBLE   Value;                    // The value associated with the Type
   LARGE    Timestamp;                // PreciseTime() of the recorded input
   OBJECTID RecipientID;              // Surface that the input message is being conveyed to
   OBJECTID OverID;                   // Surface that is directly under the mouse pointer at the time of the event
   DOUBLE   AbsX;                     // Absolute horizontal position of mouse cursor
   DOUBLE   AbsY;                     // Absolute vertical position of mouse cursor
   DOUBLE   X;                        // Horizontal position relative to the surface that the pointer is over - unless a mouse button is held or pointer is anchored - then the coordinates are relative to the click-held surface
   DOUBLE   Y;                        // Vertical position relative to the surface that the pointer is over - unless a mouse button is held or pointer is anchored - then the coordinates are relative to the click-held surface
   OBJECTID DeviceID;                 // The hardware device that this event originated from
   UWORD    Type;                     // JET constant
   UWORD    Flags;                    // Broad descriptors for the given Type (see JTYPE flags).  Automatically set by the system when sent to the pointer object
   UWORD    Mask;                     // Mask to use for checking against subscribers
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
   LONG     Flags;     // Broad descriptors for the given Type (see JTYPE flags).  Automatically set by the system when sent to the pointer object
   UWORD    Type;      // JET constant
   UWORD    Unused;    // Unused value for 32-bit padding
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

// Predefined cursor styles

#define PTR_NO_CHANGE 0
#define PTR_DEFAULT 1
#define PTR_SIZE_BOTTOM_LEFT 2
#define PTR_SIZE_BOTTOM_RIGHT 3
#define PTR_SIZE_TOP_LEFT 4
#define PTR_SIZE_TOP_RIGHT 5
#define PTR_SIZE_LEFT 6
#define PTR_SIZE_RIGHT 7
#define PTR_SIZE_TOP 8
#define PTR_SIZE_BOTTOM 9
#define PTR_CROSSHAIR 10
#define PTR_SLEEP 11
#define PTR_SIZING 12
#define PTR_SPLIT_VERTICAL 13
#define PTR_SPLIT_HORIZONTAL 14
#define PTR_MAGNIFIER 15
#define PTR_HAND 16
#define PTR_HAND_LEFT 17
#define PTR_HAND_RIGHT 18
#define PTR_TEXT 19
#define PTR_PAINTBRUSH 20
#define PTR_STOP 21
#define PTR_INVISIBLE 22
#define PTR_CUSTOM 23
#define PTR_DRAGGABLE 24
#define PTR_END 25

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

#define DRL_NORTH 0
#define DRL_UP 0
#define DRL_SOUTH 1
#define DRL_DOWN 1
#define DRL_EAST 2
#define DRL_RIGHT 2
#define DRL_WEST 3
#define DRL_LEFT 3
#define DRL_NORTH_EAST 4
#define DRL_NORTH_WEST 5
#define DRL_SOUTH_EAST 6
#define DRL_SOUTH_WEST 7

// Generic flags for controlling movement.

#define MOVE_DOWN 0x00000001
#define MOVE_UP 0x00000002
#define MOVE_LEFT 0x00000004
#define MOVE_RIGHT 0x00000008
#define MOVE_ALL 0x0000000f

// Edge flags

#define EDGE_TOP 0x00000001
#define EDGE_LEFT 0x00000002
#define EDGE_RIGHT 0x00000004
#define EDGE_BOTTOM 0x00000008
#define EDGE_TOP_LEFT 0x00000010
#define EDGE_TOP_RIGHT 0x00000020
#define EDGE_BOTTOM_LEFT 0x00000040
#define EDGE_BOTTOM_RIGHT 0x00000080
#define EDGE_ALL 0x000000ff

// Universal values for alignment of graphics and text

#define ALIGN_LEFT 0x00000001
#define ALIGN_RIGHT 0x00000002
#define ALIGN_HORIZONTAL 0x00000004
#define ALIGN_VERTICAL 0x00000008
#define ALIGN_MIDDLE 0x0000000c
#define ALIGN_CENTER 0x0000000c
#define ALIGN_TOP 0x00000010
#define ALIGN_BOTTOM 0x00000020

// Universal values for alignment of graphic layouts in documents.

#define LAYOUT_SQUARE 0x00000000
#define LAYOUT_TIGHT 0x00000001
#define LAYOUT_LEFT 0x00000002
#define LAYOUT_RIGHT 0x00000004
#define LAYOUT_WIDE 0x00000006
#define LAYOUT_BACKGROUND 0x00000008
#define LAYOUT_FOREGROUND 0x00000010
#define LAYOUT_EMBEDDED 0x00000020
#define LAYOUT_LOCK 0x00000040
#define LAYOUT_IGNORE_CURSOR 0x00000080
#define LAYOUT_TILE 0x00000100

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

#ifndef DEFINE_ENUM_FLAG_OPERATORS
template <size_t S> struct _ENUM_FLAG_INTEGER_FOR_SIZE;
template <> struct _ENUM_FLAG_INTEGER_FOR_SIZE<1> { typedef BYTE type; };
template <> struct _ENUM_FLAG_INTEGER_FOR_SIZE<2> { typedef WORD type; };
template <> struct _ENUM_FLAG_INTEGER_FOR_SIZE<4> { typedef LONG type; };
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
// Script flags

#define SCF_EXIT_ON_ERROR 0x00000001
#define SCF_DEBUG 0x00000002

#define STR_MATCH_CASE 0x0001
#define STR_CASE 0x0001
#define STR_MATCH_LEN 0x0002
#define STR_WILDCARD 0x0004

// Message flags.

#define MSF_WAIT 0x00000001
#define MSF_UPDATE 0x00000002
#define MSF_NO_DUPLICATE 0x00000004
#define MSF_ADD 0x00000008
#define MSF_ADDRESS 0x00000010
#define MSF_MESSAGE_ID 0x00000020

// Flags for ProcessMessages

#define PMF_SYSTEM_NO_BREAK 0x00000001

#define ALF_SHARED 0x0001
#define ALF_RECURSIVE 0x0002

// Flags for semaphores

#define SMF_NO_BLOCKING 0x00000001
#define SMF_NON_BLOCKING 0x00000001
#define SMF_EXISTS 0x00000002

// Flags for RegisterFD()

#define RFD_WRITE 0x0001
#define RFD_EXCEPT 0x0002
#define RFD_READ 0x0004
#define RFD_REMOVE 0x0008
#define RFD_STOP_RECURSE 0x0010
#define RFD_ALLOW_RECURSION 0x0020
#define RFD_SOCKET 0x0040
#define RFD_RECALL 0x0080
#define RFD_ALWAYS_CALL 0x0100

// Flags for StrBuildArray()

#define SBF_NO_DUPLICATES 0x00000001
#define SBF_SORT 0x00000002
#define SBF_CASE 0x00000004
#define SBF_DESC 0x00000008
#define SBF_CSV 0x00000010

// Task flags

#define TSF_FOREIGN 0x00000001
#define TSF_WAIT 0x00000002
#define TSF_RESET_PATH 0x00000004
#define TSF_PRIVILEGED 0x00000008
#define TSF_SHELL 0x00000010
#define TSF_DEBUG 0x00000020
#define TSF_QUIET 0x00000040
#define TSF_DETACHED 0x00000080
#define TSF_ATTACHED 0x00000100
#define TSF_PIPE 0x00000200

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

// Internal style notifications

#define STYLE_ENABLED 1
#define STYLE_DISABLED 2
#define STYLE_FOCUS 3
#define STYLE_LOST_FOCUS 4
#define STYLE_RESIZE 5
#define STYLE_CONTENT 6

// Internal options for requesting function tables from modules.

#define MHF_NULL 0x00000001
#define MHF_DEFAULT 0x00000002
#define MHF_STRUCTURE 0x00000002
#define MHF_STATIC 0x00000004

// Registered CPU ID's

#define CPU_M68K 1
#define CPU_I486 2
#define CPU_I586 3
#define CPU_I686 4
#define CPU_X64 5
#define CPU_ARMEABI 6

// ScrollToPoint flags

#define STP_X 0x00000001
#define STP_Y 0x00000002
#define STP_Z 0x00000004
#define STP_ANIM 0x00000008

// MoveToPoint flags

#define MTF_X 0x00000001
#define MTF_Y 0x00000002
#define MTF_Z 0x00000004
#define MTF_ANIM 0x00000008
#define MTF_RELATIVE 0x00000010

// VlogF flags

#define VLF_BRANCH 0x00000001
#define VLF_ERROR 0x00000002
#define VLF_WARNING 0x00000004
#define VLF_CRITICAL 0x00000008
#define VLF_INFO 0x00000010
#define VLF_API 0x00000020
#define VLF_EXTAPI 0x00000040
#define VLF_DEBUG 0x00000080
#define VLF_TRACE 0x00000100
#define VLF_FUNCTION 0x00000200

// Module flags

#define MOF_LINK_LIBRARY 0x00000001
#define MOF_STATIC 0x00000002
#define MOF_SYSTEM_PROBE 0x00000004

// Thread flags

#define THF_AUTO_FREE 0x00000001

// Flags for the SetDate() file method.

#define FDT_MODIFIED 0
#define FDT_CREATED 1
#define FDT_ACCESSED 2
#define FDT_ARCHIVED 3

// Options for SetVolume()

#define VOLUME_REPLACE 0x00000001
#define VOLUME_PRIORITY 0x00000002
#define VOLUME_HIDDEN 0x00000004
#define VOLUME_SYSTEM 0x00000008

// Options for the File Delete() method.

#define FDL_FEEDBACK 0x00000001

// Compression flags

#define CMF_PASSWORD 0x00000001
#define CMF_NEW 0x00000002
#define CMF_CREATE_FILE 0x00000004
#define CMF_READ_ONLY 0x00000008
#define CMF_NO_LINKS 0x00000010
#define CMF_APPLY_SECURITY 0x00000020

// Flags for ResolvePath()

#define RSF_NO_FILE_CHECK 0x00000001
#define RSF_CHECK_VIRTUAL 0x00000002
#define RSF_APPROXIMATE 0x00000004
#define RSF_NO_DEEP_SCAN 0x00000008
#define RSF_PATH 0x00000010
#define RSF_CASE_SENSITIVE 0x00000020

// Flags for the File Watch() method.

#define MFF_READ 0x00000001
#define MFF_MODIFY 0x00000002
#define MFF_WRITE 0x00000002
#define MFF_CREATE 0x00000004
#define MFF_DELETE 0x00000008
#define MFF_MOVED 0x00000010
#define MFF_RENAME 0x00000010
#define MFF_ATTRIB 0x00000020
#define MFF_OPENED 0x00000040
#define MFF_CLOSED 0x00000080
#define MFF_UNMOUNT 0x00000100
#define MFF_FOLDER 0x00000200
#define MFF_FILE 0x00000400
#define MFF_SELF 0x00000800
#define MFF_DEEP 0x00001000

// Types for StrDatatype().

#define STT_NUMBER 1
#define STT_FLOAT 2
#define STT_HEX 3
#define STT_STRING 4

#define OPF_DEPRECATED 0x00000001
#define OPF_CORE_VERSION 0x00000002
#define OPF_OPTIONS 0x00000004
#define OPF_MAX_DEPTH 0x00000008
#define OPF_DETAIL 0x00000010
#define OPF_SHOW_MEMORY 0x00000020
#define OPF_SHOW_IO 0x00000040
#define OPF_SHOW_ERRORS 0x00000080
#define OPF_ARGS 0x00000100
#define OPF_ERROR 0x00000200
#define OPF_COMPILED_AGAINST 0x00000400
#define OPF_PRIVILEGED 0x00000800
#define OPF_SYSTEM_PATH 0x00001000
#define OPF_MODULE_PATH 0x00002000
#define OPF_ROOT_PATH 0x00004000
#define OPF_SCAN_MODULES 0x00008000

#define TOI_LOCAL_CACHE 0
#define TOI_LOCAL_STORAGE 1
#define TOI_ANDROID_ENV 2
#define TOI_ANDROID_CLASS 3
#define TOI_ANDROID_ASSETMGR 4

// Universal flag values used for searching text

#define STF_CASE 0x00000001
#define STF_MOVE_CURSOR 0x00000002
#define STF_SCAN_SELECTION 0x00000004
#define STF_BACKWARDS 0x00000008
#define STF_EXPRESSION 0x00000010
#define STF_WRAP 0x00000020

// Flags for the OpenDir() function.

#define RDF_SIZE 0x00000001
#define RDF_DATE 0x00000002
#define RDF_TIME 0x00000002
#define RDF_PERMISSIONS 0x00000004
#define RDF_FILES 0x00000008
#define RDF_FILE 0x00000008
#define RDF_FOLDERS 0x00000010
#define RDF_FOLDER 0x00000010
#define RDF_READ_ALL 0x0000001f
#define RDF_VOLUME 0x00000020
#define RDF_LINK 0x00000040
#define RDF_TAGS 0x00000080
#define RDF_HIDDEN 0x00000100
#define RDF_QUALIFY 0x00000200
#define RDF_QUALIFIED 0x00000200
#define RDF_VIRTUAL 0x00000400
#define RDF_STREAM 0x00000800
#define RDF_READ_ONLY 0x00001000
#define RDF_ARCHIVE 0x00002000
#define RDF_OPENDIR 0x00004000

// File flags

#define FL_WRITE 0x00000001
#define FL_NEW 0x00000002
#define FL_READ 0x00000004
#define FL_DIRECTORY 0x00000008
#define FL_FOLDER 0x00000008
#define FL_APPROXIMATE 0x00000010
#define FL_LINK 0x00000020
#define FL_BUFFER 0x00000040
#define FL_LOOP 0x00000080
#define FL_FILE 0x00000100
#define FL_RESET_DATE 0x00000200
#define FL_DEVICE 0x00000400
#define FL_STREAM 0x00000800
#define FL_EXCLUDE_FILES 0x00001000
#define FL_EXCLUDE_FOLDERS 0x00002000

// AnalysePath() values

#define LOC_DIRECTORY 1
#define LOC_FOLDER 1
#define LOC_VOLUME 2
#define LOC_FILE 3

// IdentifyFile() values

#define IDF_SECTION 0x00000001
#define IDF_HOST 0x00000002
#define IDF_IGNORE_HOST 0x00000004

// Flags for LoadFile()

#define LDF_CHECK_EXISTS 0x00000001

// Flags for file feedback.

#define FBK_MOVE_FILE 1
#define FBK_COPY_FILE 2
#define FBK_DELETE_FILE 3

// Return codes available to the feedback routine

#define FFR_OKAY 0
#define FFR_CONTINUE 0
#define FFR_SKIP 1
#define FFR_ABORT 2

// For use by VirtualVolume()

#define VAS_DEREGISTER 1
#define VAS_SCAN_DIR 2
#define VAS_DELETE 3
#define VAS_RENAME 4
#define VAS_OPEN_DIR 5
#define VAS_CLOSE_DIR 6
#define VAS_TEST_PATH 7
#define VAS_WATCH_PATH 8
#define VAS_IGNORE_FILE 9
#define VAS_GET_INFO 10
#define VAS_GET_DEVICE_INFO 11
#define VAS_IDENTIFY_FILE 12
#define VAS_MAKE_DIR 13
#define VAS_SAME_FILE 14
#define VAS_CASE_SENSITIVE 15
#define VAS_READ_LINK 16
#define VAS_CREATE_LINK 17
#define VAS_DRIVER_SIZE 18

// Feedback event indicators.

#define FDB_DECOMPRESS_FILE 1
#define FDB_COMPRESS_FILE 2
#define FDB_REMOVE_FILE 3
#define FDB_DECOMPRESS_OBJECT 4

// Compression stream formats

#define CF_GZIP 1
#define CF_ZLIB 2
#define CF_DEFLATE 3

// Flags that can be passed to FindObject()

#define FOF_SMART_NAMES 0x00000001

// Flags that can be passed to NewObject().  If a flag needs to be stored with the object, it must be specified in the lower word.

enum class NF : ULONG {
   NIL = 0,
   PRIVATE = 0x00000000,
   UNTRACKED = 0x00000001,
   INITIALISED = 0x00000002,
   INTEGRAL = 0x00000004,
   UNLOCK_FREE = 0x00000008,
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

// Module file header constants

#define MODULE_HEADER_V1 1297040385
#define MODULE_HEADER_V2 1297040386
#define MODULE_HEADER_VERSION 1297040386
#define MODULE_HEADER EXPORT struct ModHeader ModHeader

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

#define IDTYPE_MESSAGE 1
#define IDTYPE_GLOBAL 2
#define IDTYPE_FUNCTION 3

#define SEM_GET_VAL 1
#define SEM_GET_COUNTER 2
#define SEM_GET_DATA_PTR 3
#define SEM_GET_DATA_LONG 4
#define SEM_GET_DATA_LARGE 5
#define SEM_GET_DATA_DOUBLE 6
#define SEM_SET_DATA_PTR 7
#define SEM_SET_DATA_LONG 8
#define SEM_SET_DATA_LARGE 9
#define SEM_SET_DATA_DOUBLE 10

// Indicates the state of a process.

#define TSTATE_RUNNING 0
#define TSTATE_PAUSED 1
#define TSTATE_STOPPING 2
#define TSTATE_TERMINATED 3

#define RES_MESSAGE_QUEUE 1
#define RES_CONSOLE_FD 2
#define RES_SHARED_CONTROL 3
#define RES_USER_ID 4
#define RES_DISPLAY_DRIVER 5
#define RES_PRIVILEGED_USER 6
#define RES_PRIVILEGED 7
#define RES_CORE_IDL 8
#define RES_PARENT_CONTEXT 9
#define RES_LOG_LEVEL 10
#define RES_TOTAL_SHARED_MEMORY 11
#define RES_MAX_PROCESSES 12
#define RES_LOG_DEPTH 13
#define RES_JNI_ENV 14
#define RES_THREAD_ID 15
#define RES_CURRENT_MSG 16
#define RES_OPEN_INFO 17
#define RES_EXCEPTION_HANDLER 18
#define RES_NET_PROCESSING 19
#define RES_PROCESS_STATE 20
#define RES_TOTAL_MEMORY 21
#define RES_TOTAL_SWAP 22
#define RES_CPU_SPEED 23
#define RES_FREE_MEMORY 24
#define RES_FREE_SWAP 25
#define RES_KEY_STATE 26

// Path types for SetResourcePath()

#define RP_MODULE_PATH 1
#define RP_SYSTEM_PATH 2
#define RP_ROOT_PATH 3

// Flags for the MetaClass.

#define CLF_PROMOTE_INTEGRAL 0x00000001
#define CLF_NO_OWNERSHIP 0x00000002

// Flags for the Config class.

#define CNF_STRIP_QUOTES 0x00000001
#define CNF_AUTO_SAVE 0x00000002
#define CNF_OPTIONAL_FILES 0x00000004
#define CNF_NEW 0x00000008

// Flags for VarNew()

#define KSF_CASE 0x00000001
#define KSF_THREAD_SAFE 0x00000002
#define KSF_UNTRACKED 0x00000004
#define KSF_AUTO_REMOVE 0x00000008
#define KSF_INTERNAL 0x00000010

// Raw key codes

#define K_A 1
#define K_B 2
#define K_C 3
#define K_D 4
#define K_E 5
#define K_F 6
#define K_G 7
#define K_H 8
#define K_I 9
#define K_J 10
#define K_K 11
#define K_L 12
#define K_M 13
#define K_N 14
#define K_O 15
#define K_P 16
#define K_Q 17
#define K_R 18
#define K_S 19
#define K_T 20
#define K_U 21
#define K_V 22
#define K_W 23
#define K_X 24
#define K_Y 25
#define K_Z 26
#define K_ONE 27
#define K_TWO 28
#define K_THREE 29
#define K_FOUR 30
#define K_FIVE 31
#define K_SIX 32
#define K_SEVEN 33
#define K_EIGHT 34
#define K_NINE 35
#define K_ZERO 36
#define K_REVERSE_QUOTE 37
#define K_MINUS 38
#define K_EQUALS 39
#define K_L_SQUARE 40
#define K_R_SQUARE 41
#define K_SEMI_COLON 42
#define K_APOSTROPHE 43
#define K_COMMA 44
#define K_DOT 45
#define K_PERIOD 45
#define K_SLASH 46
#define K_BACK_SLASH 47
#define K_SPACE 48
#define K_NP_0 49
#define K_NP_1 50
#define K_NP_2 51
#define K_NP_3 52
#define K_NP_4 53
#define K_NP_5 54
#define K_NP_6 55
#define K_NP_7 56
#define K_NP_8 57
#define K_NP_9 58
#define K_NP_MULTIPLY 59
#define K_NP_PLUS 60
#define K_NP_BAR 61
#define K_NP_SEPARATOR 61
#define K_NP_MINUS 62
#define K_NP_DECIMAL 63
#define K_NP_DOT 63
#define K_NP_DIVIDE 64
#define K_L_CONTROL 65
#define K_R_CONTROL 66
#define K_HELP 67
#define K_L_SHIFT 68
#define K_R_SHIFT 69
#define K_CAPS_LOCK 70
#define K_PRINT 71
#define K_L_ALT 72
#define K_R_ALT 73
#define K_L_COMMAND 74
#define K_R_COMMAND 75
#define K_F1 76
#define K_F2 77
#define K_F3 78
#define K_F4 79
#define K_F5 80
#define K_F6 81
#define K_F7 82
#define K_F8 83
#define K_F9 84
#define K_F10 85
#define K_F11 86
#define K_F12 87
#define K_F13 88
#define K_F14 89
#define K_F15 90
#define K_F16 91
#define K_F17 92
#define K_MACRO 93
#define K_NP_PLUS_MINUS 94
#define K_LESS_GREATER 95
#define K_UP 96
#define K_DOWN 97
#define K_RIGHT 98
#define K_LEFT 99
#define K_SCR_LOCK 100
#define K_PAUSE 101
#define K_WAKE 102
#define K_SLEEP 103
#define K_POWER 104
#define K_BACKSPACE 105
#define K_TAB 106
#define K_ENTER 107
#define K_ESCAPE 108
#define K_DELETE 109
#define K_CLEAR 110
#define K_HOME 111
#define K_PAGE_UP 112
#define K_PAGE_DOWN 113
#define K_END 114
#define K_SELECT 115
#define K_EXECUTE 116
#define K_INSERT 117
#define K_UNDO 118
#define K_REDO 119
#define K_MENU 120
#define K_FIND 121
#define K_CANCEL 122
#define K_BREAK 123
#define K_NUM_LOCK 124
#define K_PRT_SCR 125
#define K_NP_ENTER 126
#define K_SYSRQ 127
#define K_F18 128
#define K_F19 129
#define K_F20 130
#define K_WIN_CONTROL 131
#define K_VOLUME_UP 132
#define K_VOLUME_DOWN 133
#define K_BACK 134
#define K_CALL 135
#define K_END_CALL 136
#define K_CAMERA 137
#define K_AT 138
#define K_PLUS 139
#define K_LENS_FOCUS 140
#define K_STOP 141
#define K_NEXT 142
#define K_PREVIOUS 143
#define K_FORWARD 144
#define K_REWIND 145
#define K_MUTE 146
#define K_STAR 147
#define K_POUND 148
#define K_PLAY 149
#define K_LIST_END 150


#ifndef __GNUC__
#define __attribute__(a)
#endif

#define VER_CORE (1.0f)  // Core version + revision
#define REV_CORE (0)     // Core revision as a whole number

#ifdef  __cplusplus
extern "C" {
#endif

#define MODULE_COREBASE struct CoreBase *CoreBase = 0;

#ifndef STRINGIFY
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#endif

typedef const std::vector<std::pair<std::string, ULONG>> STRUCTS;

#define MOD_IDL NULL

#ifdef MOD_NAME
#define PARASOL_MOD(init,close,open,expunge,version,IDL,Structures) EXPORT ModHeader ModHeader(init, close, open, expunge, version, IDL, Structures, TOSTRING(MOD_NAME));
#define MOD_PATH ("modules:" TOSTRING(MOD_NAME))
#else
#define MOD_NAME NULL
#endif

#ifdef DEBUG
 #define MSG(...)  LogF(0,__VA_ARGS__)
 #define FMSG(...) LogF(__VA_ARGS__)
#else
 #define MSG(...)
 #define FMSG(...)
#endif

#ifdef  __cplusplus
}
#endif

#define skipwhitespace(a) while ((*(a) > 0) && (*(a) <= 0x20)) (a)++;

#define ARRAYSIZE(a) ((LONG)(sizeof(a)/sizeof(a[0])))

#define ROUNDUP(a,b) (((a) + (b)) - ((a) % (b))) // ROUNDUP(Number, Alignment) e.g. (14,8) = 16

#define ALIGN64(a) (((a) + 7) & (~7))
#define ALIGN32(a) (((a) + 3) & (~3))
#define ALIGN16(a) (((a) + 1) & (~1))

#define CODE_MEMH 0x4D454D48L
#define CODE_MEMT 0x4D454D54L

#ifdef PRINTF64I
  #define PF64 "I64d"
#elif PRINTF64_PRID
  #define PF64 PRId64
#else
  #define PF64 "lld"
#endif

// Use DEBUG_BREAK in critical areas where you would want to break in gdb.  This feature will only be compiled
// in to debug builds.

#ifdef DEBUG
 #define DEBUG_BREAK asm("int $3");
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

//#define F2T(a) LONG(a)

// Structures to pass to OpenCore()

struct OpenTag {
   LONG Tag;
   union {
      LONG Long;
      LARGE Large;
      APTR Pointer;
      CSTRING String;
   } Value;
};

struct OpenInfo {
   CSTRING Name;            // OPF_NAME
   CSTRING *Args;           // OPF_ARGS
   CSTRING SystemPath;      // OPF_SYSTEM_PATH
   CSTRING ModulePath;      // OPF_MODULE_PATH
   CSTRING RootPath;        // OPF_ROOT_PATH
   struct OpenTag *Options; // OPF_OPTIONS Typecast to va_list (defined in stdarg.h)
   LONG    Flags;           // OPF flags need to be set for fields that have been defined in this structure.
   LONG    MaxDepth;        // OPF_MAX_DEPTH
   LONG    Detail;          // OPF_DETAIL
   LONG    ArgCount;        // OPF_ARGS
   ERROR   Error;           // OPF_ERROR
   FLOAT   CompiledAgainst; // OPF_COMPILED_AGAINST
   FLOAT   CoreVersion;     // OPF_CORE_VERSION
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

template <class T> inline APTR ResolveAddress(T *Pointer, LONG Offset) {
   return APTR(((BYTE *)Pointer) + Offset);
}

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
   LONG    HeaderVersion;                           // The version of this structure.
   LONG    Flags;                                   // Special flags, type of function table wanted from the Core
   FLOAT   ModVersion;                              // Version of this module
   FLOAT   CoreVersion;                             // Core version compiled against
   CSTRING Definitions;                             // Module definition string, usable by run-time languages such as Fluid
   ERROR (*Init)(OBJECTPTR, struct CoreBase *);     // A one-off initialisation routine for when the module is first opened.
   void (*Close)(OBJECTPTR);                        // A function that will be called each time the module is closed.
   ERROR (*Open)(OBJECTPTR);                        // A function that will be called each time the module is opened.
   ERROR (*Expunge)(void);                          // Reference to an expunge function to terminate the module.
   CSTRING Name;                                    // Name of the module
   STRUCTS *StructDefs;
   struct RootModule *Root;
   ModHeader(ERROR (*pInit)(OBJECTPTR, struct CoreBase *),
      void  (*pClose)(OBJECTPTR),
      ERROR (*pOpen)(OBJECTPTR),
      ERROR (*pExpunge)(void),
      FLOAT pVersion,
      CSTRING pDef,
      STRUCTS *pStructs,
      CSTRING pName) {

      HeaderVersion = MODULE_HEADER_VERSION;
      Flags         = MHF_DEFAULT;
      ModVersion    = pVersion;
      CoreVersion   = VER_CORE;
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
   ULONG   Flags;   // Special flags that describe the field
   MAXINT  Arg;     // Can be a pointer or an integer value
   APTR    GetField; // void GetField(*Object, APTR Result);
   APTR    SetField; // ERROR SetField(*Object, APTR Value);
  template <class G = APTR, class S = APTR, class T = MAXINT> FieldArray(CSTRING pName, ULONG pFlags, G pGetField = NULL, S pSetField = NULL, T pArg = 0) :
     Name(pName), Flags(pFlags), Arg((MAXINT)pArg), GetField((APTR)pGetField), SetField((APTR)pSetField)
     { }
};

struct FieldDef {
   CSTRING Name;    // The name of the constant.
   LONG    Value;   // The value of the constant.
};

struct SystemState {
   CSTRING Platform;        // String-based field indicating the user's platform.  Currently returns 'Native', 'Windows', 'OSX' or 'Linux'.
   HOSTHANDLE ConsoleFD;    // Internal
   LONG    CoreVersion;     // Reflects the Core version number.
   LONG    CoreRevision;    // Reflects the Core revision number.
   LONG    Stage;           // The current operating stage.  -1 = Initialising, 0 indicates normal operating status; 1 means that the program is shutting down; 2 indicates a program restart; 3 is for mode switches.
};

struct Variable {
   ULONG  Type;      // Field definition flags
   LONG   Unused;    // Unused 32-bit value for 64-bit alignment
   LARGE  Large;     // The value as a 64-bit integer.
   DOUBLE Double;    // The value as a 64-bit float-point number.
   APTR   Pointer;   // The value as an address pointer.
};

struct ActionArray {
   LONG ActionCode;    // Action identifier
   APTR Routine;       // Pointer to the function entry point
  template <class T> ActionArray(LONG pID, T pRoutine) : ActionCode(pID), Routine((APTR)pRoutine) { }
};

struct MethodArray {
   LONG    MethodID;                     // Unique method identifier
   APTR    Routine;                      // The method entry point, defined as ERROR (*Routine)(OBJECTPTR, APTR);
   CSTRING Name;                         // Name of the method
   const struct FunctionField * Args;    // List of parameters accepted by the method
   LONG    Size;                         // Total byte-size of all accepted parameters when they are assembled as a C structure.
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
   LARGE Time;       // A timestamp acquired from PreciseTime() when the message was first passed to SendMessage().
   LONG  UniqueID;   // A unique identifier automatically created by SendMessage().
   LONG  Type;       // A message type identifier as defined by the client.
   LONG  Size;       // The size of the message data, in bytes.  If there is no data associated with the message, the Size will be set to zero.</>
};

struct ThreadMessage {
   OBJECTID ThreadID;    // Internal
};

struct ThreadActionMessage {
   OBJECTPTR Object;    // Direct pointer to a target object.
   LONG      ActionID;  // The action to execute.
   LONG      Key;       // Internal
   ERROR     Error;     // The error code resulting from the action's execution.
   FUNCTION  Callback;  // Callback function to execute on action completion.
};

typedef struct MemInfo {
   APTR     Start;       // The starting address of the memory block (does not apply to shared blocks).
   OBJECTID ObjectID;    // The object that owns the memory block.
   LONG     Size;        // The size of the memory block.
   WORD     AccessCount; // Total number of active locks on this block.
   WORD     Flags;       // The type of memory.
   MEMORYID MemoryID;    // The unique ID for this block.
} MEMINFO;

struct ActionEntry {
   ERROR (*PerformAction)(OBJECTPTR, APTR);     // Internal
};

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
   LONG    FeedbackID;    // Set to one of the FDB event indicators
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
   LONG    Permissions;             // Original permissions - see PERMIT flags.
   LONG    UserID;                  // Original user ID
   LONG    GroupID;                 // Original group ID
   LONG    OthersID;                // Original others ID
   LONG    Flags;                   // FL flags
   struct DateTime Created;         // Date and time of the file's creation.
   struct DateTime Modified;        // Date and time last modified.
    std::unordered_map<std::string, std::string> *Tags;
};

struct FileInfo {
   LARGE  Size;               // The size of the file's content.
   LARGE  TimeStamp;          // 64-bit time stamp - usable only for comparison (e.g. sorting).
   struct FileInfo * Next;    // Next structure in the list, or NULL.
   STRING Name;               // The name of the file.  This string remains valid until the next call to GetFileInfo().
   LONG   Flags;              // Additional flags to describe the file.
   LONG   Permissions;        // Standard permission flags.
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
   LONG   prvFlags;         // OpenFolder() RDF flags
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
   LONG   FeedbackID;    // Set to one of the FDB integers
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

extern struct CoreBase *CoreBase;
struct CoreBase {
   ERROR (*_AccessMemoryID)(MEMORYID Memory, LONG Flags, LONG MilliSeconds, APTR Result);
   ERROR (*_Action)(LONG Action, OBJECTPTR Object, APTR Parameters);
   void (*_ActionList)(struct ActionTable ** Actions, LONG * Size);
   ERROR (*_ActionMsg)(LONG Action, OBJECTID Object, APTR Args);
   CSTRING (*_ResolveClassID)(CLASSID ID);
   LONG (*_AllocateID)(LONG Type);
   ERROR (*_AllocMemory)(LONG Size, LONG Flags, APTR Address, MEMORYID * ID);
   ERROR (*_AccessObjectID)(OBJECTID Object, LONG MilliSeconds, APTR Result);
   ERROR (*_CheckAction)(OBJECTPTR Object, LONG Action);
   ERROR (*_CheckMemoryExists)(MEMORYID ID);
   ERROR (*_CheckObjectExists)(OBJECTID Object);
   ERROR (*_DeleteFile)(CSTRING Path, FUNCTION * Callback);
   ERROR (*_VirtualVolume)(CSTRING Name, ...);
   OBJECTPTR (*_CurrentContext)(void);
   ERROR (*_GetFieldArray)(OBJECTPTR Object, FIELD Field, APTR Result, LONG * Elements);
   LONG (*_AdjustLogLevel)(LONG Adjust);
   void __attribute__((format(printf, 2, 3))) (*_LogF)(CSTRING Header, CSTRING Message, ...);
   ERROR (*_FindObject)(CSTRING Name, CLASSID ClassID, LONG Flags, OBJECTID * ObjectID);
   objMetaClass * (*_FindClass)(CLASSID ClassID);
   ERROR (*_AnalysePath)(CSTRING Path, LONG * Type);
   LONG (*_UTF8Copy)(CSTRING Src, STRING Dest, LONG Chars, LONG Size);
   ERROR (*_FreeResource)(MEMORYID ID);
   CLASSID (*_GetClassID)(OBJECTID Object);
   OBJECTID (*_GetOwnerID)(OBJECTID Object);
   ERROR (*_GetField)(OBJECTPTR Object, FIELD Field, APTR Result);
   ERROR (*_GetFieldVariable)(OBJECTPTR Object, CSTRING Field, STRING Buffer, LONG Size);
   LONG (*_TotalChildren)(OBJECTID Object);
   CSTRING (*_GetName)(OBJECTPTR Object);
   ERROR (*_ListChildren)(OBJECTID Object, pf::vector<ChildEntry> * List);
   ERROR (*_Base64Decode)(struct pfBase64Decode * State, CSTRING Input, LONG InputSize, APTR Output, LONG * Written);
   ERROR (*_RegisterFD)(HOSTHANDLE FD, LONG Flags, void (*Routine)(HOSTHANDLE, APTR) , APTR Data);
   ERROR (*_ResolvePath)(CSTRING Path, LONG Flags, STRING * Result);
   ERROR (*_MemoryIDInfo)(MEMORYID ID, struct MemInfo * MemInfo, LONG Size);
   ERROR (*_MemoryPtrInfo)(APTR Address, struct MemInfo * MemInfo, LONG Size);
   ERROR (*_NewObject)(LARGE ClassID, NF Flags, APTR Object);
   void (*_NotifySubscribers)(OBJECTPTR Object, LONG Action, APTR Args, ERROR Error);
   ERROR (*_StrReadLocale)(CSTRING Key, CSTRING * Value);
   CSTRING (*_UTF8ValidEncoding)(CSTRING String, CSTRING Encoding);
   ERROR (*_ProcessMessages)(LONG Flags, LONG TimeOut);
   ERROR (*_IdentifyFile)(CSTRING Path, CLASSID * Class, CLASSID * SubClass);
   ERROR (*_ReallocMemory)(APTR Memory, LONG Size, APTR Address, MEMORYID * ID);
   ERROR (*_GetMessage)(MEMORYID Queue, LONG Type, LONG Flags, APTR Buffer, LONG Size);
   MEMORYID (*_ReleaseMemory)(APTR Address);
   CLASSID (*_ResolveClassName)(CSTRING Name);
   ERROR (*_SendMessage)(OBJECTID Task, LONG Type, LONG Flags, APTR Data, LONG Size);
   ERROR (*_SetOwner)(OBJECTPTR Object, OBJECTPTR Owner);
   OBJECTPTR (*_SetContext)(OBJECTPTR Object);
   ERROR (*_SetField)(OBJECTPTR Object, FIELD Field, ...);
   CSTRING (*_FieldName)(ULONG FieldID);
   ERROR (*_ScanDir)(struct DirInfo * Info);
   ERROR (*_SetName)(OBJECTPTR Object, CSTRING Name);
   void (*_LogReturn)(void);
   ERROR (*_StrCompare)(CSTRING String1, CSTRING String2, LONG Length, LONG Flags);
   ERROR (*_SubscribeAction)(OBJECTPTR Object, LONG Action, FUNCTION * Callback);
   ERROR (*_SubscribeEvent)(LARGE Event, FUNCTION * Callback, APTR Custom, APTR Handle);
   ERROR (*_SubscribeTimer)(DOUBLE Interval, FUNCTION * Callback, APTR Subscription);
   ERROR (*_UpdateTimer)(APTR Subscription, DOUBLE Interval);
   ERROR (*_UnsubscribeAction)(OBJECTPTR Object, LONG Action);
   void (*_UnsubscribeEvent)(APTR Event);
   ERROR (*_BroadcastEvent)(APTR Event, LONG EventSize);
   void (*_WaitTime)(LONG Seconds, LONG MicroSeconds);
   LARGE (*_GetEventID)(LONG Group, CSTRING SubGroup, CSTRING Event);
   ULONG (*_GenCRC32)(ULONG CRC, APTR Data, ULONG Length);
   LARGE (*_GetResource)(LONG Resource);
   LARGE (*_SetResource)(LONG Resource, LARGE Value);
   ERROR (*_ScanMessages)(APTR Queue, LONG * Index, LONG Type, APTR Buffer, LONG Size);
   ERROR (*_SysLock)(LONG Index, LONG MilliSeconds);
   ERROR (*_SysUnlock)(LONG Index);
   ERROR (*_CreateFolder)(CSTRING Path, LONG Permissions);
   ERROR (*_LoadFile)(CSTRING Path, LONG Flags, struct CacheFile ** Cache);
   ERROR (*_SetVolume)(CSTRING Name, CSTRING Path, CSTRING Icon, CSTRING Label, CSTRING Device, LONG Flags);
   ERROR (*_DeleteVolume)(CSTRING Name);
   ERROR (*_MoveFile)(CSTRING Source, CSTRING Dest, FUNCTION * Callback);
   ERROR (*_UpdateMessage)(APTR Queue, LONG Message, LONG Type, APTR Data, LONG Size);
   ERROR (*_AddMsgHandler)(APTR Custom, LONG MsgType, FUNCTION * Routine, struct MsgHandler ** Handle);
   ERROR (*_QueueAction)(LONG Action, OBJECTID Object, APTR Args);
   LARGE (*_PreciseTime)(void);
   ERROR (*_OpenDir)(CSTRING Path, LONG Flags, struct DirInfo ** Info);
   OBJECTPTR (*_GetObjectPtr)(OBJECTID Object);
   struct Field * (*_FindField)(OBJECTPTR Object, ULONG FieldID, APTR Target);
   CSTRING (*_GetErrorMsg)(ERROR Error);
   struct Message * (*_GetActionMsg)(void);
   ERROR (*_FuncError)(CSTRING Header, ERROR Error);
   ERROR (*_SetArray)(OBJECTPTR Object, FIELD Field, APTR Array, LONG Elements);
   ERROR (*_ReleaseMemoryID)(MEMORYID MemoryID);
   ERROR (*_LockObject)(OBJECTPTR Object, LONG MilliSeconds);
   void (*_ReleaseObject)(OBJECTPTR Object);
   ERROR (*_AllocMutex)(LONG Flags, APTR Result);
   void (*_FreeMutex)(APTR Mutex);
   ERROR (*_LockMutex)(APTR Mutex, LONG MilliSeconds);
   void (*_UnlockMutex)(APTR Mutex);
   ERROR (*_ActionThread)(LONG Action, OBJECTPTR Object, APTR Args, FUNCTION * Callback, LONG Key);
   ERROR (*_AllocSharedMutex)(CSTRING Name, APTR Mutex);
   void (*_FreeSharedMutex)(APTR Mutex);
   ERROR (*_LockSharedMutex)(APTR Mutex, LONG MilliSeconds);
   void (*_UnlockSharedMutex)(APTR Mutex);
   void (*_VLogF)(int Flags, const char *Header, const char *Message, va_list Args);
   LONG (*_Base64Encode)(struct pfBase64Encode * State, const void * Input, LONG InputSize, STRING Output, LONG OutputSize);
   ERROR (*_ReadInfoTag)(struct FileInfo * Info, CSTRING Name, CSTRING * Value);
   ERROR (*_SetResourcePath)(LONG PathType, CSTRING Path);
   OBJECTPTR (*_CurrentTask)(void);
   CSTRING (*_ResolveGroupID)(LONG Group);
   CSTRING (*_ResolveUserID)(LONG User);
   ERROR (*_CreateLink)(CSTRING From, CSTRING To);
   STRING * (*_StrBuildArray)(STRING List, LONG Size, LONG Total, LONG Flags);
   LONG (*_UTF8CharOffset)(CSTRING String, LONG Offset);
   LONG (*_UTF8Length)(CSTRING String);
   LONG (*_UTF8OffsetToChar)(CSTRING String, LONG Offset);
   LONG (*_UTF8PrevLength)(CSTRING String, LONG Offset);
   LONG (*_UTF8CharLength)(CSTRING String);
   ULONG (*_UTF8ReadValue)(CSTRING String, LONG * Length);
   LONG (*_UTF8WriteValue)(LONG Value, STRING Buffer, LONG Size);
   ERROR (*_CopyFile)(CSTRING Source, CSTRING Dest, FUNCTION * Callback);
   ERROR (*_WaitForObjects)(LONG Flags, LONG TimeOut, struct ObjectSignal * ObjectSignals);
   ERROR (*_ReadFileToBuffer)(CSTRING Path, APTR Buffer, LONG BufferSize, LONG * Result);
   LONG (*_StrDatatype)(CSTRING String);
   void (*_UnloadFile)(struct CacheFile * Cache);
   void (*_SetDefaultPermissions)(LONG User, LONG Group, LONG Permissions);
   ERROR (*_CompareFilePaths)(CSTRING PathA, CSTRING PathB);
   const struct SystemState * (*_GetSystemState)(void);
   ULONG (*_StrHash)(CSTRING String, LONG CaseSensitive);
   ERROR (*_AddInfoTag)(struct FileInfo * Info, CSTRING Name, CSTRING Value);
};

#ifndef PRV_CORE_MODULE
inline ERROR AccessMemoryID(MEMORYID Memory, LONG Flags, LONG MilliSeconds, APTR Result) { return CoreBase->_AccessMemoryID(Memory,Flags,MilliSeconds,Result); }
inline ERROR Action(LONG Action, OBJECTPTR Object, APTR Parameters) { return CoreBase->_Action(Action,Object,Parameters); }
inline void ActionList(struct ActionTable ** Actions, LONG * Size) { return CoreBase->_ActionList(Actions,Size); }
inline ERROR ActionMsg(LONG Action, OBJECTID Object, APTR Args) { return CoreBase->_ActionMsg(Action,Object,Args); }
inline CSTRING ResolveClassID(CLASSID ID) { return CoreBase->_ResolveClassID(ID); }
inline LONG AllocateID(LONG Type) { return CoreBase->_AllocateID(Type); }
inline ERROR AllocMemory(LONG Size, LONG Flags, APTR Address, MEMORYID * ID) { return CoreBase->_AllocMemory(Size,Flags,Address,ID); }
inline ERROR AccessObjectID(OBJECTID Object, LONG MilliSeconds, APTR Result) { return CoreBase->_AccessObjectID(Object,MilliSeconds,Result); }
inline ERROR CheckAction(OBJECTPTR Object, LONG Action) { return CoreBase->_CheckAction(Object,Action); }
inline ERROR CheckMemoryExists(MEMORYID ID) { return CoreBase->_CheckMemoryExists(ID); }
inline ERROR CheckObjectExists(OBJECTID Object) { return CoreBase->_CheckObjectExists(Object); }
inline ERROR DeleteFile(CSTRING Path, FUNCTION * Callback) { return CoreBase->_DeleteFile(Path,Callback); }
template<class... Args> ERROR VirtualVolume(CSTRING Name, Args... Tags) { return CoreBase->_VirtualVolume(Name,Tags...); }
inline OBJECTPTR CurrentContext(void) { return CoreBase->_CurrentContext(); }
inline ERROR GetFieldArray(OBJECTPTR Object, FIELD Field, APTR Result, LONG * Elements) { return CoreBase->_GetFieldArray(Object,Field,Result,Elements); }
inline LONG AdjustLogLevel(LONG Adjust) { return CoreBase->_AdjustLogLevel(Adjust); }
template<class... Args> void LogF(CSTRING Header, CSTRING Message, Args... Tags) { return CoreBase->_LogF(Header,Message,Tags...); }
inline ERROR FindObject(CSTRING Name, CLASSID ClassID, LONG Flags, OBJECTID * ObjectID) { return CoreBase->_FindObject(Name,ClassID,Flags,ObjectID); }
inline objMetaClass * FindClass(CLASSID ClassID) { return CoreBase->_FindClass(ClassID); }
inline ERROR AnalysePath(CSTRING Path, LONG * Type) { return CoreBase->_AnalysePath(Path,Type); }
inline LONG UTF8Copy(CSTRING Src, STRING Dest, LONG Chars, LONG Size) { return CoreBase->_UTF8Copy(Src,Dest,Chars,Size); }
inline ERROR FreeResource(MEMORYID ID) { return CoreBase->_FreeResource(ID); }
inline CLASSID GetClassID(OBJECTID Object) { return CoreBase->_GetClassID(Object); }
inline OBJECTID GetOwnerID(OBJECTID Object) { return CoreBase->_GetOwnerID(Object); }
inline ERROR GetField(OBJECTPTR Object, FIELD Field, APTR Result) { return CoreBase->_GetField(Object,Field,Result); }
inline ERROR GetFieldVariable(OBJECTPTR Object, CSTRING Field, STRING Buffer, LONG Size) { return CoreBase->_GetFieldVariable(Object,Field,Buffer,Size); }
inline LONG TotalChildren(OBJECTID Object) { return CoreBase->_TotalChildren(Object); }
inline CSTRING GetName(OBJECTPTR Object) { return CoreBase->_GetName(Object); }
inline ERROR ListChildren(OBJECTID Object, pf::vector<ChildEntry> * List) { return CoreBase->_ListChildren(Object,List); }
inline ERROR Base64Decode(struct pfBase64Decode * State, CSTRING Input, LONG InputSize, APTR Output, LONG * Written) { return CoreBase->_Base64Decode(State,Input,InputSize,Output,Written); }
inline ERROR RegisterFD(HOSTHANDLE FD, LONG Flags, void (*Routine)(HOSTHANDLE, APTR) , APTR Data) { return CoreBase->_RegisterFD(FD,Flags,Routine,Data); }
inline ERROR ResolvePath(CSTRING Path, LONG Flags, STRING * Result) { return CoreBase->_ResolvePath(Path,Flags,Result); }
inline ERROR MemoryIDInfo(MEMORYID ID, struct MemInfo * MemInfo, LONG Size) { return CoreBase->_MemoryIDInfo(ID,MemInfo,Size); }
inline ERROR MemoryPtrInfo(APTR Address, struct MemInfo * MemInfo, LONG Size) { return CoreBase->_MemoryPtrInfo(Address,MemInfo,Size); }
inline ERROR NewObject(LARGE ClassID, NF Flags, APTR Object) { return CoreBase->_NewObject(ClassID,Flags,Object); }
inline void NotifySubscribers(OBJECTPTR Object, LONG Action, APTR Args, ERROR Error) { return CoreBase->_NotifySubscribers(Object,Action,Args,Error); }
inline ERROR StrReadLocale(CSTRING Key, CSTRING * Value) { return CoreBase->_StrReadLocale(Key,Value); }
inline CSTRING UTF8ValidEncoding(CSTRING String, CSTRING Encoding) { return CoreBase->_UTF8ValidEncoding(String,Encoding); }
inline ERROR ProcessMessages(LONG Flags, LONG TimeOut) { return CoreBase->_ProcessMessages(Flags,TimeOut); }
inline ERROR IdentifyFile(CSTRING Path, CLASSID * Class, CLASSID * SubClass) { return CoreBase->_IdentifyFile(Path,Class,SubClass); }
inline ERROR ReallocMemory(APTR Memory, LONG Size, APTR Address, MEMORYID * ID) { return CoreBase->_ReallocMemory(Memory,Size,Address,ID); }
inline ERROR GetMessage(MEMORYID Queue, LONG Type, LONG Flags, APTR Buffer, LONG Size) { return CoreBase->_GetMessage(Queue,Type,Flags,Buffer,Size); }
inline MEMORYID ReleaseMemory(APTR Address) { return CoreBase->_ReleaseMemory(Address); }
inline CLASSID ResolveClassName(CSTRING Name) { return CoreBase->_ResolveClassName(Name); }
inline ERROR SendMessage(OBJECTID Task, LONG Type, LONG Flags, APTR Data, LONG Size) { return CoreBase->_SendMessage(Task,Type,Flags,Data,Size); }
inline ERROR SetOwner(OBJECTPTR Object, OBJECTPTR Owner) { return CoreBase->_SetOwner(Object,Owner); }
inline OBJECTPTR SetContext(OBJECTPTR Object) { return CoreBase->_SetContext(Object); }
template<class... Args> ERROR SetField(OBJECTPTR Object, FIELD Field, Args... Tags) { return CoreBase->_SetField(Object,Field,Tags...); }
inline CSTRING FieldName(ULONG FieldID) { return CoreBase->_FieldName(FieldID); }
inline ERROR ScanDir(struct DirInfo * Info) { return CoreBase->_ScanDir(Info); }
inline ERROR SetName(OBJECTPTR Object, CSTRING Name) { return CoreBase->_SetName(Object,Name); }
inline void LogReturn(void) { return CoreBase->_LogReturn(); }
inline ERROR StrCompare(CSTRING String1, CSTRING String2, LONG Length, LONG Flags) { return CoreBase->_StrCompare(String1,String2,Length,Flags); }
inline ERROR SubscribeAction(OBJECTPTR Object, LONG Action, FUNCTION * Callback) { return CoreBase->_SubscribeAction(Object,Action,Callback); }
inline ERROR SubscribeEvent(LARGE Event, FUNCTION * Callback, APTR Custom, APTR Handle) { return CoreBase->_SubscribeEvent(Event,Callback,Custom,Handle); }
inline ERROR SubscribeTimer(DOUBLE Interval, FUNCTION * Callback, APTR Subscription) { return CoreBase->_SubscribeTimer(Interval,Callback,Subscription); }
inline ERROR UpdateTimer(APTR Subscription, DOUBLE Interval) { return CoreBase->_UpdateTimer(Subscription,Interval); }
inline ERROR UnsubscribeAction(OBJECTPTR Object, LONG Action) { return CoreBase->_UnsubscribeAction(Object,Action); }
inline void UnsubscribeEvent(APTR Event) { return CoreBase->_UnsubscribeEvent(Event); }
inline ERROR BroadcastEvent(APTR Event, LONG EventSize) { return CoreBase->_BroadcastEvent(Event,EventSize); }
inline void WaitTime(LONG Seconds, LONG MicroSeconds) { return CoreBase->_WaitTime(Seconds,MicroSeconds); }
inline LARGE GetEventID(LONG Group, CSTRING SubGroup, CSTRING Event) { return CoreBase->_GetEventID(Group,SubGroup,Event); }
inline ULONG GenCRC32(ULONG CRC, APTR Data, ULONG Length) { return CoreBase->_GenCRC32(CRC,Data,Length); }
inline LARGE GetResource(LONG Resource) { return CoreBase->_GetResource(Resource); }
inline LARGE SetResource(LONG Resource, LARGE Value) { return CoreBase->_SetResource(Resource,Value); }
inline ERROR ScanMessages(APTR Queue, LONG * Index, LONG Type, APTR Buffer, LONG Size) { return CoreBase->_ScanMessages(Queue,Index,Type,Buffer,Size); }
inline ERROR SysLock(LONG Index, LONG MilliSeconds) { return CoreBase->_SysLock(Index,MilliSeconds); }
inline ERROR SysUnlock(LONG Index) { return CoreBase->_SysUnlock(Index); }
inline ERROR CreateFolder(CSTRING Path, LONG Permissions) { return CoreBase->_CreateFolder(Path,Permissions); }
inline ERROR LoadFile(CSTRING Path, LONG Flags, struct CacheFile ** Cache) { return CoreBase->_LoadFile(Path,Flags,Cache); }
inline ERROR SetVolume(CSTRING Name, CSTRING Path, CSTRING Icon, CSTRING Label, CSTRING Device, LONG Flags) { return CoreBase->_SetVolume(Name,Path,Icon,Label,Device,Flags); }
inline ERROR DeleteVolume(CSTRING Name) { return CoreBase->_DeleteVolume(Name); }
inline ERROR MoveFile(CSTRING Source, CSTRING Dest, FUNCTION * Callback) { return CoreBase->_MoveFile(Source,Dest,Callback); }
inline ERROR UpdateMessage(APTR Queue, LONG Message, LONG Type, APTR Data, LONG Size) { return CoreBase->_UpdateMessage(Queue,Message,Type,Data,Size); }
inline ERROR AddMsgHandler(APTR Custom, LONG MsgType, FUNCTION * Routine, struct MsgHandler ** Handle) { return CoreBase->_AddMsgHandler(Custom,MsgType,Routine,Handle); }
inline ERROR QueueAction(LONG Action, OBJECTID Object, APTR Args) { return CoreBase->_QueueAction(Action,Object,Args); }
inline LARGE PreciseTime(void) { return CoreBase->_PreciseTime(); }
inline ERROR OpenDir(CSTRING Path, LONG Flags, struct DirInfo ** Info) { return CoreBase->_OpenDir(Path,Flags,Info); }
inline OBJECTPTR GetObjectPtr(OBJECTID Object) { return CoreBase->_GetObjectPtr(Object); }
inline struct Field * FindField(OBJECTPTR Object, ULONG FieldID, APTR Target) { return CoreBase->_FindField(Object,FieldID,Target); }
inline CSTRING GetErrorMsg(ERROR Error) { return CoreBase->_GetErrorMsg(Error); }
inline struct Message * GetActionMsg(void) { return CoreBase->_GetActionMsg(); }
inline ERROR FuncError(CSTRING Header, ERROR Error) { return CoreBase->_FuncError(Header,Error); }
inline ERROR SetArray(OBJECTPTR Object, FIELD Field, APTR Array, LONG Elements) { return CoreBase->_SetArray(Object,Field,Array,Elements); }
inline ERROR ReleaseMemoryID(MEMORYID MemoryID) { return CoreBase->_ReleaseMemoryID(MemoryID); }
inline ERROR LockObject(OBJECTPTR Object, LONG MilliSeconds) { return CoreBase->_LockObject(Object,MilliSeconds); }
inline void ReleaseObject(OBJECTPTR Object) { return CoreBase->_ReleaseObject(Object); }
inline ERROR AllocMutex(LONG Flags, APTR Result) { return CoreBase->_AllocMutex(Flags,Result); }
inline void FreeMutex(APTR Mutex) { return CoreBase->_FreeMutex(Mutex); }
inline ERROR LockMutex(APTR Mutex, LONG MilliSeconds) { return CoreBase->_LockMutex(Mutex,MilliSeconds); }
inline void UnlockMutex(APTR Mutex) { return CoreBase->_UnlockMutex(Mutex); }
inline ERROR ActionThread(LONG Action, OBJECTPTR Object, APTR Args, FUNCTION * Callback, LONG Key) { return CoreBase->_ActionThread(Action,Object,Args,Callback,Key); }
inline ERROR AllocSharedMutex(CSTRING Name, APTR Mutex) { return CoreBase->_AllocSharedMutex(Name,Mutex); }
inline void FreeSharedMutex(APTR Mutex) { return CoreBase->_FreeSharedMutex(Mutex); }
inline ERROR LockSharedMutex(APTR Mutex, LONG MilliSeconds) { return CoreBase->_LockSharedMutex(Mutex,MilliSeconds); }
inline void UnlockSharedMutex(APTR Mutex) { return CoreBase->_UnlockSharedMutex(Mutex); }
inline void VLogF(int Flags, const char *Header, const char *Message, va_list Args) { return CoreBase->_VLogF(Flags,Header,Message,Args); }
inline LONG Base64Encode(struct pfBase64Encode * State, const void * Input, LONG InputSize, STRING Output, LONG OutputSize) { return CoreBase->_Base64Encode(State,Input,InputSize,Output,OutputSize); }
inline ERROR ReadInfoTag(struct FileInfo * Info, CSTRING Name, CSTRING * Value) { return CoreBase->_ReadInfoTag(Info,Name,Value); }
inline ERROR SetResourcePath(LONG PathType, CSTRING Path) { return CoreBase->_SetResourcePath(PathType,Path); }
inline OBJECTPTR CurrentTask(void) { return CoreBase->_CurrentTask(); }
inline CSTRING ResolveGroupID(LONG Group) { return CoreBase->_ResolveGroupID(Group); }
inline CSTRING ResolveUserID(LONG User) { return CoreBase->_ResolveUserID(User); }
inline ERROR CreateLink(CSTRING From, CSTRING To) { return CoreBase->_CreateLink(From,To); }
inline STRING * StrBuildArray(STRING List, LONG Size, LONG Total, LONG Flags) { return CoreBase->_StrBuildArray(List,Size,Total,Flags); }
inline LONG UTF8CharOffset(CSTRING String, LONG Offset) { return CoreBase->_UTF8CharOffset(String,Offset); }
inline LONG UTF8Length(CSTRING String) { return CoreBase->_UTF8Length(String); }
inline LONG UTF8OffsetToChar(CSTRING String, LONG Offset) { return CoreBase->_UTF8OffsetToChar(String,Offset); }
inline LONG UTF8PrevLength(CSTRING String, LONG Offset) { return CoreBase->_UTF8PrevLength(String,Offset); }
inline LONG UTF8CharLength(CSTRING String) { return CoreBase->_UTF8CharLength(String); }
inline ULONG UTF8ReadValue(CSTRING String, LONG * Length) { return CoreBase->_UTF8ReadValue(String,Length); }
inline LONG UTF8WriteValue(LONG Value, STRING Buffer, LONG Size) { return CoreBase->_UTF8WriteValue(Value,Buffer,Size); }
inline ERROR CopyFile(CSTRING Source, CSTRING Dest, FUNCTION * Callback) { return CoreBase->_CopyFile(Source,Dest,Callback); }
inline ERROR WaitForObjects(LONG Flags, LONG TimeOut, struct ObjectSignal * ObjectSignals) { return CoreBase->_WaitForObjects(Flags,TimeOut,ObjectSignals); }
inline ERROR ReadFileToBuffer(CSTRING Path, APTR Buffer, LONG BufferSize, LONG * Result) { return CoreBase->_ReadFileToBuffer(Path,Buffer,BufferSize,Result); }
inline LONG StrDatatype(CSTRING String) { return CoreBase->_StrDatatype(String); }
inline void UnloadFile(struct CacheFile * Cache) { return CoreBase->_UnloadFile(Cache); }
inline void SetDefaultPermissions(LONG User, LONG Group, LONG Permissions) { return CoreBase->_SetDefaultPermissions(User,Group,Permissions); }
inline ERROR CompareFilePaths(CSTRING PathA, CSTRING PathB) { return CoreBase->_CompareFilePaths(PathA,PathB); }
inline const struct SystemState * GetSystemState(void) { return CoreBase->_GetSystemState(); }
inline ULONG StrHash(CSTRING String, LONG CaseSensitive) { return CoreBase->_StrHash(String,CaseSensitive); }
inline ERROR AddInfoTag(struct FileInfo * Info, CSTRING Name, CSTRING Value) { return CoreBase->_AddInfoTag(Info,Name,Value); }
#endif


//********************************************************************************************************************

#define PRIME_HASH 2654435761UL
#define END_FIELD FieldArray(NULL, 0)
#define FDEF static const struct FunctionField

template <class T> inline MEMORYID GetMemoryID(T &&A) {
   return ((MEMORYID *)A)[-2];
}

#define DeregisterFD(a) RegisterFD((a), RFD_REMOVE|RFD_READ|RFD_WRITE|RFD_EXCEPT|RFD_ALWAYS_CALL, 0, 0)
#define DeleteMsg(a,b)  UpdateMessage(a,b,(APTR)-1,0,0)

inline OBJECTPTR GetParentContext() { return (OBJECTPTR)(MAXINT)GetResource(RES_PARENT_CONTEXT); }
inline APTR GetResourcePtr(LONG ID) { return (APTR)(MAXINT)GetResource(ID); }

inline CSTRING to_cstring(const std::string &A) { return A.c_str(); }
constexpr inline CSTRING to_cstring(CSTRING A) { return A; }

template <class T, class U> inline ERROR StrMatch(T &&A, U &&B) {
   return StrCompare(to_cstring(A), to_cstring(B), 0, STR_MATCH_LEN);
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

inline ERROR FreeResource(const void *Address) {
   return FreeResource(((LONG *)Address)[-2]);
}

inline ERROR AllocMemory(LONG Size, LONG Flags, APTR Address) {
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

template <class T, class U> inline ERROR StrCompare(T &&A, U &&B, LONG Length, LONG Flags) {
   return StrCompare(to_cstring(A), to_cstring(B), Length, Flags);
}

inline ULONG StrHash(const std::string Value) {
   return CoreBase->_StrHash(Value.c_str(), FALSE);
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

inline LONG StrSearchCase(CSTRING Keyword, CSTRING String)
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

inline LONG StrSearch(CSTRING Keyword, CSTRING String)
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

inline STRING StrClone(CSTRING String)
{
   if (!String) return NULL;

   LONG len = strlen(String);
   STRING newstr;
   if (!AllocMemory(len+1, MEM_STRING, (APTR *)&newstr, NULL)) {
      CopyMemory(String, newstr, len+1);
      return newstr;
   }
   else return NULL;
}

inline LONG StrLength(CSTRING String) {
   if (String) return strlen(String);
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
   auto len = str.copy(String, StringSize-1);
   String[len] = 0;
   return len;
}

inline ERROR ClearMemory(APTR Memory, LONG Length) {
   if (!Memory) return ERR_NullArgs;
   memset(Memory, 0, Length); // memset() is assumed to be optimised by the compiler.
   return ERR_Okay;
}

// If AUTO_OBJECT_LOCK is enabled, objects will be automatically locked to prevent thread-clashes.
// NB: Turning this off will cause issues between threads unless they call the necessary locking functions.

//#define AUTO_OBJECT_LOCK 1

namespace pf {

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
         VLogF(VLF_API|VLF_BRANCH, header, Message, arg);
         va_end(arg);
         branches++;
      }

      #ifdef DEBUG
      void traceBranch(CSTRING Message = "", ...) __attribute__((format(printf, 2, 3))) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_TRACE|VLF_BRANCH, header, Message, arg);
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
         VLogF(VLF_INFO, header, Message, arg);
         va_end(arg);
      }

      void msg(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // Defaults to API level, recommended for modules
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_API, header, Message, arg);
         va_end(arg);
      }

      void msg(LONG Flags, CSTRING Message, ...) __attribute__((format(printf, 3, 4))) { // Defaults to API level, recommended for modules
         va_list arg;
         va_start(arg, Message);
         VLogF(Flags, header, Message, arg);
         va_end(arg);
         if (Flags & VLF_BRANCH) branches++;
      }

      void extmsg(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // Extended API message
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_EXTAPI, header, Message, arg);
         va_end(arg);
      }

      void pmsg(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // "Parent message", uses the scope of the caller
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_API, NULL, Message, arg);
         va_end(arg);
      }

      void warning(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_WARNING, header, Message, arg);
         va_end(arg);
      }

      void error(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // NB: Use for messages intended for the user, not the developer
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_ERROR, header, Message, arg);
         va_end(arg);
      }

      void debug(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) {
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_DEBUG, header, Message, arg);
         va_end(arg);
      }

      void function(CSTRING Message, ...) __attribute__((format(printf, 2, 3))) { // Equivalent to branch() but without a new branch being created
         va_list arg;
         va_start(arg, Message);
         VLogF(VLF_API|VLF_FUNCTION, header, Message, arg);
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
         #ifdef DEBUG
            va_list arg;
            va_start(arg, Message);
            VLogF(VLF_TRACE, header, Message, arg);
            va_end(arg);
         #endif
      }

      void traceWarning(CSTRING Message, ...) {
         #ifdef DEBUG
            va_list arg;
            va_start(arg, Message);
            VLogF(VLF_WARNING, header, Message, arg);
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
   APTR     ChildPrivate;       // Address for the ChildPrivate structure, if allocated
   APTR     CreatorMeta;        // The creator (via NewObject) is permitted to store a custom data pointer here.
   CLASSID  ClassID;            // The object's class ID
   CLASSID  SubID;              // The object's sub-class ID, or zero if irrelevant.
   OBJECTID UID;                // Unique object identifier
   OBJECTID OwnerID;            // The owner of this object
   NF       Flags;              // Object flags
   LONG     NotifyFlags[2];     // Action subscription flags - space for 64 actions max
   volatile LONG  ThreadID;     // Managed by locking functions
   char Name[MAX_NAME_LEN];     // The name of the object (optional)
   UBYTE ThreadPending;         // ActionThread() increments this.
   volatile BYTE Queue;         // Managed by locking functions
   volatile BYTE SleepQueue;    //
   volatile bool Locked;        // Set if locked by AccessObjectID()/LockObject()
   BYTE ActionDepth;            // Incremented each time an action or method is called on the object

   inline bool initialised() { return (Flags & NF::INITIALISED) != NF::NIL; }
   inline bool defined(NF pFlags) { return (Flags & pFlags) != NF::NIL; }
   inline bool isSubClass() { return SubID != 0; }
   inline OBJECTID ownerID() { return OwnerID; }
   inline NF flags() { return Flags; }

   CSTRING className();

   inline bool collecting() { // Is object being freed or marked for collection?
      return (Flags & (NF::FREE|NF::COLLECT)) != NF::NIL;
   }

   inline bool terminating() { // Is object currently being freed?
      return (Flags & NF::FREE) != NF::NIL;
   }

   inline ERROR threadLock() {
      #ifdef AUTO_OBJECT_LOCK
         if (INC_QUEUE(this) IS 1) {
            ThreadID = get_thread_id();
            return ERR_Okay;
         }
         else {
            if (ThreadID IS get_thread_id()) return ERR_Okay; // If this is for the same thread then it's a nested lock, so there's no issue.
            SUB_QUEUE(this); // Put the lock count back to normal before LockObject()
            return LockObject(this, -1); // Can fail if object is marked for deletion.
         }
      #else
         return ERR_Okay;
      #endif
   }

   inline void threadRelease() {
      #ifdef AUTO_OBJECT_LOCK
         if (SleepQueue > 0) ReleaseObject(this);
         else SUB_QUEUE(this);
      #endif
   }

   // These are fast in-line calls for object locking.  They attempt to quickly 'steal' the
   // object lock if the queue value was at zero.

   inline LONG incQueue() {
      return __sync_add_and_fetch(&Queue, 1);
   }

   inline LONG subQueue() {
      return __sync_sub_and_fetch(&Queue, 1);
   }

   inline LONG incSleep() {
      return __sync_add_and_fetch(&SleepQueue, 1);
   }

   inline LONG subSleep() {
      return __sync_sub_and_fetch(&SleepQueue, 1);
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
   inline ERROR set(ULONG FieldID, const std::string &Value) { return SetField(this, (FIELD)FieldID|TSTRING, Value.c_str()); }
   inline ERROR set(ULONG FieldID, const Variable *Value) { return SetField(this, (FIELD)FieldID|TVAR, Value); }
   // Works both for regular data pointers and function pointers if field is defined correctly.
   inline ERROR set(ULONG FieldID, const void *Value) { return SetField(this, (FIELD)FieldID|TPTR, Value); }

   inline ERROR setPercentage(ULONG FieldID, DOUBLE Value) { return SetField(this, (FIELD)FieldID|TDOUBLE|TPERCENT, Value); }

   inline ERROR get(ULONG FieldID, LONG *Value) { return GetField(this, (FIELD)FieldID|TLONG, Value); }
   inline ERROR get(ULONG FieldID, LARGE *Value) { return GetField(this, (FIELD)FieldID|TLARGE, Value); }
   inline ERROR get(ULONG FieldID, DOUBLE *Value) { return GetField(this, (FIELD)FieldID|TDOUBLE, Value); }
   inline ERROR get(ULONG FieldID, STRING *Value) { return GetField(this, (FIELD)FieldID|TSTRING, Value); }
   inline ERROR get(ULONG FieldID, CSTRING *Value) { return GetField(this, (FIELD)FieldID|TSTRING, Value); }
   inline ERROR get(ULONG FieldID, Variable *Value) { return GetField(this, (FIELD)FieldID|TVAR, Value); }
   inline ERROR getPtr(ULONG FieldID, APTR Value) { return GetField(this, (FIELD)FieldID|TPTR, Value); }
   inline ERROR getPercentage(ULONG FieldID, DOUBLE *Value) { return GetField(this, (FIELD)FieldID|TDOUBLE|TPERCENT, Value); }

   template <typename... Args> ERROR setFields(Args&&... pFields) {
      pf::Log log("setFields");

      threadLock();

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
               if (target != this) target->threadLock();

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

               if (target != this) target->threadRelease();

               // NB: NoSupport is considered a 'soft' error that does not warrant failure.

               if ((error) and (error != ERR_NoSupport)) {
                  log.warning("(%s:%d) Failed to set field %s (error #%d).", target->className(), target->UID, field->Name, error);
                  threadRelease();
                  return error;
               }
            }
         }
         else {
            log.warning("Field %s is not supported by class %s.", FieldName(f.FieldID), className());
            threadRelease();
            return ERR_UnsupportedField;
         }
      }

      threadRelease();
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
                     target->threadLock();

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

                     target->threadRelease();

                     // NB: NoSupport is considered a 'soft' error that does not warrant failure.

                     if ((error) and (error != ERR_NoSupport)) return;
                  }
               }
               else { error = log.warning(ERR_UnsupportedField); return; }
            }

            if ((error = acInit(obj))) {
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
inline APTR SetResourcePtr(LONG Res, APTR Value) { return (APTR)(MAXINT)(CoreBase->_SetResource(Res, (MAXINT)Value)); }

// Action and Notification Structures

struct acClipboard     { LONG Mode; };
struct acCopyData      { union { OBJECTID DestID; OBJECTID Dest; }; };
struct acCustom        { LONG Number; CSTRING String; };
struct acDataFeed      { union { OBJECTID ObjectID; OBJECTID Object; }; union { LONG DataType; LONG Datatype; }; const void *Buffer; LONG Size; };
struct acDragDrop      { union { OBJECTID SourceID; OBJECTID Source; }; LONG Item; CSTRING Datatype; };
struct acDraw          { LONG X; LONG Y; LONG Width; LONG Height; };
struct acGetVar        { CSTRING Field; STRING Buffer; LONG Size; };
struct acMove          { DOUBLE DeltaX; DOUBLE DeltaY; DOUBLE DeltaZ; };
struct acMoveToPoint   { DOUBLE X; DOUBLE Y; DOUBLE Z; LONG Flags; };
struct acNewChild      { OBJECTPTR Object; };
struct acNewOwner      { union { OBJECTID NewOwnerID; OBJECTID NewOwner; }; CLASSID ClassID; };
struct acRead          { APTR Buffer; LONG Length; LONG Result; };
struct acRedimension   { DOUBLE X; DOUBLE Y; DOUBLE Z; DOUBLE Width; DOUBLE Height; DOUBLE Depth; };
struct acRedo          { LONG Steps; };
struct acRename        { CSTRING Name; };
struct acResize        { DOUBLE Width; DOUBLE Height; DOUBLE Depth; };
struct acSaveImage     { union { OBJECTID DestID; OBJECTID Dest; }; union { CLASSID ClassID; CLASSID Class; }; };
struct acSaveToObject  { union { OBJECTID DestID; OBJECTID Dest; }; union { CLASSID ClassID; CLASSID Class; }; };
struct acScroll        { DOUBLE DeltaX; DOUBLE DeltaY; DOUBLE DeltaZ; };
struct acScrollToPoint { DOUBLE X; DOUBLE Y; DOUBLE Z; LONG Flags; };
struct acSeek          { DOUBLE Offset; LONG Position; };
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
inline ERROR acInit(OBJECTPTR Object) { return Action(AC_Init,Object,NULL); }
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
inline ERROR acSort(OBJECTPTR Object) { return Action(AC_Sort,Object,NULL); }
inline ERROR acUnlock(OBJECTPTR Object) { return Action(AC_Unlock,Object,NULL); }

inline ERROR acClipboard(OBJECTPTR Object, LONG Mode) {
   struct acClipboard args = { Mode };
   return Action(AC_Clipboard, Object, &args);
}

inline ERROR acDragDrop(OBJECTPTR Object, OBJECTID Source, LONG Item, CSTRING Datatype) {
   struct acDragDrop args = { Source, Item, Datatype };
   return Action(AC_DragDrop, Object, &args);
}

inline ERROR acDrawArea(OBJECTPTR Object, LONG X, LONG Y, LONG Width, LONG Height) {
   struct acDraw args = { X, Y, Width, Height };
   return Action(AC_Draw, Object, &args);
}

inline ERROR acDataFeed(OBJECTPTR Object, OBJECTID ObjectID, LONG Datatype, const void *Buffer, LONG Size) {
   struct acDataFeed args = { { ObjectID }, { Datatype }, Buffer, Size };
   return Action(AC_DataFeed, Object, &args);
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

inline ERROR acRedo(OBJECTPTR Object, LONG Steps) {
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

inline ERROR acScrollToPoint(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Z, LONG Flags) {
   struct acScrollToPoint args = { X, Y, Z, Flags };
   return Action(AC_ScrollToPoint, Object, &args);
}

inline ERROR acUndo(OBJECTPTR Object, LONG Steps) {
   struct acUndo args = { Steps };
   return Action(AC_Undo, Object, &args);
}

inline ERROR acGetVar(OBJECTPTR Object, CSTRING FieldName, STRING Buffer, LONG Size) {
   struct acGetVar args = { FieldName, Buffer, Size };
   ERROR error = Action(AC_GetVar, Object, &args);
   if ((error) and (Buffer)) Buffer[0] = 0;
   return error;
}

inline ERROR acMoveToPoint(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Z, LONG Flags) {
   struct acMoveToPoint moveto = { X, Y, Z, Flags };
   return Action(AC_MoveToPoint, Object, &moveto);
}

inline ERROR acSaveImage(OBJECTPTR Object, OBJECTID DestID, CLASSID ClassID) {
   struct acSaveImage args = { { DestID }, { ClassID } };
   return Action(AC_SaveImage, Object, &args);
}

inline ERROR acSaveToObject(OBJECTPTR Object, OBJECTID DestID, CLASSID ClassID) {
   struct acSaveToObject args = { { DestID }, { ClassID } };
   return Action(AC_SaveToObject, Object, &args);
}

inline ERROR acSeek(OBJECTPTR Object, DOUBLE Offset, LONG Position) {
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

#define acSeekStart(a,b)    acSeek((a),(b),SEEK_START)
#define acSeekEnd(a,b)      acSeek((a),(b),SEEK_END)
#define acSeekCurrent(a,b)  acSeek((a),(b),SEEK_CURRENT)

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

// StorageDevice class definition

#define VER_STORAGEDEVICE (1.000000)

class objStorageDevice : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_STORAGEDEVICE;
   static constexpr CSTRING CLASS_NAME = "StorageDevice";

   using create = pf::Create<objStorageDevice>;

   LARGE DeviceFlags;    // These read-only flags identify the type of device and its features.
   LARGE DeviceSize;     // The storage size of the device in bytes, without accounting for the file system format.
   LARGE BytesFree;      // Total amount of storage space that is available, measured in bytes.
   LARGE BytesUsed;      // Total amount of storage space in use.
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

struct flStartStream { OBJECTID SubscriberID; LONG Flags; LONG Length;  };
struct flDelete { FUNCTION * Callback;  };
struct flMove { CSTRING Dest; FUNCTION * Callback;  };
struct flCopy { CSTRING Dest; FUNCTION * Callback;  };
struct flSetDate { LONG Year; LONG Month; LONG Day; LONG Hour; LONG Minute; LONG Second; LONG Type;  };
struct flReadLine { STRING Result;  };
struct flNext { objFile * File;  };
struct flWatch { FUNCTION * Callback; LARGE Custom; LONG Flags;  };

INLINE ERROR flStartStream(APTR Ob, OBJECTID SubscriberID, LONG Flags, LONG Length) {
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

INLINE ERROR flSetDate(APTR Ob, LONG Year, LONG Month, LONG Day, LONG Hour, LONG Minute, LONG Second, LONG Type) {
   struct flSetDate args = { Year, Month, Day, Hour, Minute, Second, Type };
   return(Action(MT_FlSetDate, (OBJECTPTR)Ob, &args));
}

#define flBufferContent(obj) Action(MT_FlBufferContent,(obj),0)

INLINE ERROR flNext(APTR Ob, objFile ** File) {
   struct flNext args = { 0 };
   ERROR error = Action(MT_FlNext, (OBJECTPTR)Ob, &args);
   if (File) *File = args.File;
   return(error);
}

INLINE ERROR flWatch(APTR Ob, FUNCTION * Callback, LARGE Custom, LONG Flags) {
   struct flWatch args = { Callback, Custom, Flags };
   return(Action(MT_FlWatch, (OBJECTPTR)Ob, &args));
}


class objFile : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_FILE;
   static constexpr CSTRING CLASS_NAME = "File";

   using create = pf::Create<objFile>;

   LARGE    Position; // The current read/write byte position in a file.
   LONG     Flags;    // File flags and options.
   LONG     Static;   // Set to TRUE if a file object should be static.
   OBJECTID TargetID; // Specifies a surface ID to target for user feedback and dialog boxes.
   BYTE *   Buffer;   // Points to the internal data buffer if the file content is held in memory.

   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR dataFeed(OBJECTID ObjectID, LONG Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { { ObjectID }, { Datatype }, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
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
   inline ERROR seek(DOUBLE Offset, LONG Position = SEEK_CURRENT) {
      struct acSeek args = { Offset, Position };
      return Action(AC_Seek, this, &args);
   }
   inline ERROR seekStart(DOUBLE Offset)   { return seek(Offset, SEEK_START); }
   inline ERROR seekEnd(DOUBLE Offset)     { return seek(Offset, SEEK_END); }
   inline ERROR seekCurrent(DOUBLE Offset) { return seek(Offset, SEEK_CURRENT); }
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
   struct cfgReadValue args = { Group, Key, 0 };
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
   struct cfgGetGroupFromIndex args = { Index, 0 };
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
   LONG   Flags;        // Optional flags may be set here.
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
   inline ERROR dataFeed(OBJECTID ObjectID, LONG Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { { ObjectID }, { Datatype }, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERROR flush() { return Action(AC_Flush, this, NULL); }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
   inline ERROR saveSettings() { return Action(AC_SaveSettings, this, NULL); }
   inline ERROR saveToObject(OBJECTID DestID, CLASSID ClassID) {
      struct acSaveToObject args = { { DestID }, { ClassID } };
      return Action(AC_SaveToObject, this, &args);
   }
   inline ERROR sort() { return Action(AC_Sort, this, NULL); }
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
   struct scCallback args = { ProcedureID, Args, TotalArgs, 0 };
   ERROR error = Action(MT_ScCallback, (OBJECTPTR)Ob, &args);
   if (Error) *Error = args.Error;
   return(error);
}

INLINE ERROR scGetProcedureID(APTR Ob, CSTRING Procedure, LARGE * ProcedureID) {
   struct scGetProcedureID args = { Procedure, 0 };
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
   LONG     Flags;     // Optional flags.
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
   inline ERROR dataFeed(OBJECTID ObjectID, LONG Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { { ObjectID }, { Datatype }, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERROR getVar(CSTRING FieldName, STRING Buffer, LONG Size) {
      struct acGetVar args = { FieldName, Buffer, Size };
      ERROR error = Action(AC_GetVar, this, &args);
      if ((error) and (Buffer)) Buffer[0] = 0;
      return error;
   }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
   inline ERROR reset() { return Action(AC_Reset, this, NULL); }
   inline ERROR acSetVar(CSTRING FieldName, CSTRING Value) {
      struct acSetVar args = { FieldName, Value };
      return Action(AC_SetVar, this, &args);
   }
};

// MetaClass class definition

#define VER_METACLASS (1.000000)

// MetaClass methods

#define MT_mcFindField -1

struct mcFindField { LONG ID; struct Field * Field; objMetaClass * Source;  };

INLINE ERROR mcFindField(APTR Ob, LONG ID, struct Field ** Field, objMetaClass ** Source) {
   struct mcFindField args = { ID, 0, 0 };
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
   struct MethodArray * Methods;        // Set this field to define the methods supported by the class.
   const struct FieldArray * Fields;    // Points to a field array that describes the class' object structure.
   CSTRING ClassName;                   // The name of the represented class.
   CSTRING FileExtension;               // Describes the file extension represented by the class.
   CSTRING FileDescription;             // Describes the file type represented by the class.
   CSTRING FileHeader;                  // Defines a string expression that will allow relevant file data to be matched to the class.
   CSTRING Path;                        // The path to the module binary that represents the class.
   LONG    Size;                        // The total size of the object structure represented by the MetaClass.
   LONG    Flags;                       // Optional flag settings.
   CLASSID SubClassID;                  // Specifies the sub-class ID of a class object.
   CLASSID BaseClassID;                 // Specifies the base class ID of a class object.
   LONG    OpenCount;                   // The total number of active objects that are linked back to the MetaClass.
   LONG    TotalMethods;                // The total number of methods supported by a class.
   LONG    TotalFields;                 // The total number of fields defined by a class.
   LONG    Category;                    // The system category that a class belongs to.
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
   struct taskGetEnv args = { Name, 0 };
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
   LONG   Flags;      // Optional flags.
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
   inline ERROR init() { return Action(AC_Init, this, NULL); }
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
};

// Thread class definition

#define VER_THREAD (1.000000)

// Thread methods

#define MT_ThSetData -1
#define MT_ThWait -2

struct thSetData { APTR Data; LONG Size;  };
struct thWait { LONG TimeOut;  };

INLINE ERROR thSetData(APTR Ob, APTR Data, LONG Size) {
   struct thSetData args = { Data, Size };
   return(Action(MT_ThSetData, (OBJECTPTR)Ob, &args));
}

INLINE ERROR thWait(APTR Ob, LONG TimeOut) {
   struct thWait args = { TimeOut };
   return(Action(MT_ThWait, (OBJECTPTR)Ob, &args));
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
   LONG  Flags;      // Optional flags can be defined here.

   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR deactivate() { return Action(AC_Deactivate, this, NULL); }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
};

// Module class definition

#define VER_MODULE (1.000000)

// Module methods

#define MT_ModResolveSymbol -1

struct modResolveSymbol { CSTRING Name; APTR Address;  };

INLINE ERROR modResolveSymbol(APTR Ob, CSTRING Name, APTR * Address) {
   struct modResolveSymbol args = { Name, 0 };
   ERROR error = Action(MT_ModResolveSymbol, (OBJECTPTR)Ob, &args);
   if (Address) *Address = args.Address;
   return(error);
}


class objModule : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_MODULE;
   static constexpr CSTRING CLASS_NAME = "Module";

   using create = pf::Create<objModule>;

   DOUBLE Version;                          // Minimum required version number.
   const struct Function * FunctionList;    // Refers to a list of public functions exported by the module.
   APTR   ModBase;                          // The Module's function base (jump table) must be read from this field.
   struct RootModule * Root;                // For internal use only.
   struct ModHeader * Header;               // For internal usage only.
   LONG   Flags;                            // Optional flags.
   public:
   static ERROR load(std::string Name, DOUBLE Version, OBJECTPTR *Module = NULL, APTR Functions = NULL) {
      if (auto module = objModule::create::global(pf::FieldValue(FID_Name, Name.c_str()), pf::FieldValue(FID_Version, Version))) {
         APTR functionbase;
         if (!module->getPtr(FID_ModBase, &functionbase)) {
            if (Module) *Module = module;
            if (Functions) ((APTR *)Functions)[0] = functionbase;
            return ERR_Okay;
         }
         else return ERR_GetField;
      }
      else return ERR_CreateObject;
   }

   // Action stubs

   inline ERROR init() { return Action(AC_Init, this, NULL); }
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
   inline ERROR init() { return Action(AC_Init, this, NULL); }
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
struct cmpFind { CSTRING Path; LONG Flags; struct CompressedItem * Item;  };

INLINE ERROR cmpCompressBuffer(APTR Ob, APTR Input, LONG InputSize, APTR Output, LONG OutputSize, LONG * Result) {
   struct cmpCompressBuffer args = { Input, InputSize, Output, OutputSize, 0 };
   ERROR error = Action(MT_CmpCompressBuffer, (OBJECTPTR)Ob, &args);
   if (Result) *Result = args.Result;
   return(error);
}

INLINE ERROR cmpCompressFile(APTR Ob, CSTRING Location, CSTRING Path) {
   struct cmpCompressFile args = { Location, Path };
   return(Action(MT_CmpCompressFile, (OBJECTPTR)Ob, &args));
}

INLINE ERROR cmpDecompressBuffer(APTR Ob, APTR Input, APTR Output, LONG OutputSize, LONG * Result) {
   struct cmpDecompressBuffer args = { Input, Output, OutputSize, 0 };
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

INLINE ERROR cmpFind(APTR Ob, CSTRING Path, LONG Flags, struct CompressedItem ** Item) {
   struct cmpFind args = { Path, Flags, 0 };
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
   LONG     Flags;           // Optional flags.
   LONG     SegmentSize;     // Private. Splits the compressed file if it surpasses a set byte limit.
   LONG     Permissions;     // Default permissions for decompressed files are defined here.
   LONG     MinOutputSize;   // Indicates the minimum output buffer size that will be needed during de/compression.
   LONG     WindowBits;      // Special option for certain compression formats.

   // Action stubs

   inline ERROR flush() { return Action(AC_Flush, this, NULL); }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
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
   LONG      Format;     // The format of the compressed stream.  The default is GZIP.
};

#ifndef PRV_CORE

// Note that the length of the data is only needed when messaging between processes, so we can skip it for these
// direct-access data channel macros.

#define acDataContent(a,b)  acDataFeed((a),0,DATA_CONTENT,(b),0)
#define acDataXML(a,b)      acDataFeed((a),0,DATA_XML,(b),0)
#define acDataText(a,b)     acDataFeed((a),0,DATA_TEXT,(b),0)

inline ERROR acCustom(OBJECTID ObjectID, LONG Number, CSTRING String) {
   struct acCustom args = { Number, String };
   return ActionMsg(AC_Custom, ObjectID, &args);
}

inline ERROR acDataFeed(OBJECTID ObjectID, OBJECTID SenderID, LONG Datatype, const APTR Data, LONG Size) {
   struct acDataFeed channel = { { SenderID }, { Datatype }, Data, Size };
   return ActionMsg(AC_DataFeed, ObjectID, &channel);
}

inline ERROR acDragDrop(OBJECTID ObjectID, OBJECTID Source, LONG Item, CSTRING Datatype) {
   struct acDragDrop args = { { Source }, Item, Datatype };
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

inline ERROR acMoveToPoint(OBJECTID ObjectID, DOUBLE X, DOUBLE Y, DOUBLE Z = 0, LONG Flags = MTF_X|MTF_Y) {
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

inline ERROR acScrollToPoint(OBJECTID ObjectID, DOUBLE X, DOUBLE Y, DOUBLE Z = 0, LONG Flags = STP_X|STP_Y) {
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
inline ERROR acInit(OBJECTID ObjectID) { return ActionMsg(AC_Init, ObjectID, NULL); }
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

// Semaphore management structure.

#define MAX_SEMAPHORES  40  // Maximum number of semaphores that can be allocated in the system

struct SemaphoreEntry {   // The index of each semaphore in the array indicates their IDs
   ULONG NameID;          // Hashed name of the semaphore
   LONG  BlockingProcess; // Process ID of the blocker
   LONG  BlockingThread;  // Global thread ID of the blocker
   LARGE Data;            // User configurable 64-bit data
   WORD  Flags;           // Status flags
   WORD  BlockingValue;   // Value used for blocking access
   WORD  MaxValue;        // Semaphore maximum value
   WORD  Counter;         // When the counter reaches zero, the semaphore is blocked
   struct SemProcess {
      LONG ProcessID;
      UBYTE AllocCount;      // Number of times that this process has allocated the semaphore with AllocSemaphore()
      UBYTE BlockCount;      // Count of blocking locks currently recorded
      UBYTE AccessCount;     // Count of access locks currently recorded
      UBYTE BufferCount;     // Buffered accesses (this value increases instead of AccessCount when the BlockCount is set)
   } Processes[20];
   //LONG     FIFO[10];       // List of processes currently queued for access
};

struct ActionMessage {
   OBJECTID ObjectID;        // The object that is to receive the action
   LONG  Time;
   ACTIONID ActionID;        // ID of the action or method to execute
   bool SendArgs;            //

   // Action arguments follow this structure in a buffer
};

#endif

enum { // For SysLock()
   PL_WAITLOCKS=1,
   PL_FORBID,
   PL_SEMAPHORES,
   #ifdef _WIN32
      CN_SEMAPHORES,
   #endif
   PL_END
};

struct SharedControl {
   volatile LONG ValidateProcess;
   WORD SystemState;
   volatile WORD WLIndex;           // Current insertion point for the wait-lock array.
   LONG MagicKey;                   // This magic key is set to the semaphore key (used only as an indicator for initialisation)
   LONG SemaphoreOffset;            // Offset to the semaphore control array
   LONG TaskOffset;                 // Offset to the task control array
   LONG WLOffset;                   // Offset to the wait-lock array
   #ifdef __unix__
      struct {
         pthread_mutex_t Mutex;
         pthread_cond_t Cond;
         LONG PID;               // Resource tracking: Process that has the current lock.
         WORD Count;             // Resource tracking: Count of all locks (nesting)
      } PublicLocks[PL_END];
   #endif
};

// Event support

struct rkEvent {
   EVENTID EventID;
   // Data follows
};

#define EVID_DISPLAY_RESOLUTION_CHANGE  GetEventID(EVG_DISPLAY, "resolution", "change")

#define EVID_GUI_SURFACE_FOCUS          GetEventID(EVG_GUI, "surface", "focus")

#define EVID_FILESYSTEM_VOLUME_CREATED  GetEventID(EVG_FILESYSTEM, "volume", "created")
#define EVID_FILESYSTEM_VOLUME_DELETED  GetEventID(EVG_FILESYSTEM, "volume", "deleted")

#define EVID_SYSTEM_TASK_CREATED        GetEventID(EVG_SYSTEM, "task", "created")
#define EVID_SYSTEM_TASK_REMOVED        GetEventID(EVG_SYSTEM, "task", "removed")

#define EVID_POWER_STATE_SUSPENDING     GetEventID(EVG_POWER, "state", "suspending")
#define EVID_POWER_STATE_RESUMED        GetEventID(EVG_POWER, "state", "resumed")
#define EVID_POWER_DISPLAY_STANDBY      GetEventID(EVG_POWER, "display", "standby")
#define EVID_POWER_BATTERY_LOW          GetEventID(EVG_POWER, "battery", "low")
#define EVID_POWER_BATTERY_CRITICAL     GetEventID(EVG_POWER, "battery", "critical")
#define EVID_POWER_CPUTEMP_HIGH         GetEventID(EVG_POWER, "cputemp", "high")
#define EVID_POWER_CPUTEMP_CRITICAL     GetEventID(EVG_POWER, "cputemp", "critical")
#define EVID_POWER_SCREENSAVER_ON       GetEventID(EVG_POWER, "screensaver", "on")
#define EVID_POWER_SCREENSAVER_OFF      GetEventID(EVG_POWER, "screensaver", "off")

#define EVID_IO_KEYMAP_CHANGE           GetEventID(EVG_IO, "keymap", "change")
#define EVID_IO_KEYBOARD_KEYPRESS       GetEventID(EVG_IO, "keyboard", "keypress")

#define EVID_AUDIO_VOLUME_MASTER        GetEventID(EVG_AUDIO, "volume", "master")
#define EVID_AUDIO_VOLUME_LINEIN        GetEventID(EVG_AUDIO, "volume", "linein")
#define EVID_AUDIO_VOLUME_MIC           GetEventID(EVG_AUDIO, "volume", "mic")
#define EVID_AUDIO_VOLUME_MUTED         GetEventID(EVG_AUDIO, "volume", "muted") // All volumes have been muted
#define EVID_AUDIO_VOLUME_UNMUTED       GetEventID(EVG_AUDIO, "volume", "unmuted") // All volumes have been unmuted

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
typedef struct { EVENTID EventID; LONG Qualifiers; LONG Code; LONG Unicode; } evKey;
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
