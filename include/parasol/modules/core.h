#pragma once

// Name:      core.h
// Copyright: Paul Manias 1996-2022
// Generator: idl-c

#include <parasol/main.h>

#include <stdarg.h>

#ifdef __cplusplus
#include <list>
#include <map>
#include <string>
#include <vector>
#include <unordered_map>
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

#define AC_ActionNotify 1
#define AC_Activate 2
#define AC_AccessObject 3
#define AC_Clear 4
#define AC_FreeWarning 5
#define AC_OwnerDestroyed 6
#define AC_CopyData 7
#define AC_DataFeed 8
#define AC_Deactivate 9
#define AC_Draw 10
#define AC_Flush 11
#define AC_Focus 12
#define AC_Free 13
#define AC_ReleaseObject 14
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
#define AC_Sort 53
#define AC_SaveSettings 54
#define AC_SelectArea 55
#define AC_Signal 56
#define AC_END 57

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
#define MEM_SHARED 0x00000001
#define MEM_PUBLIC 0x00000001
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
#define MEM_FIXED 0x00004000
#define MEM_MANAGED 0x00008000
#define MEM_READ 0x00010000
#define MEM_WRITE 0x00020000
#define MEM_READ_WRITE 0x00030000
#define MEM_NO_CLEAR 0x00040000
#define MEM_RESERVED 0x00080000
#define MEM_HIDDEN 0x00100000
#define MEM_TASK 0x00200000
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
#define FD_LOOKUP 0x00000080
#define FD_ARRAYSIZE 0x00000080
#define FD_PTRSIZE 0x00000080
#define FD_BUFSIZE 0x00000080
#define FD_R 0x00000100
#define FD_RESULT 0x00000100
#define FD_READ 0x00000100
#define FD_WRITE 0x00000200
#define FD_W 0x00000200
#define FD_BUFFER 0x00000200
#define FD_RW 0x00000300
#define FD_INIT 0x00000400
#define FD_I 0x00000400
#define FD_TAGS 0x00000400
#define FD_RI 0x00000500
#define FD_ERROR 0x00000800
#define FD_ARRAY 0x00001000
#define FD_RESOURCE 0x00002000
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
#define FD_POINTER 0x08000000
#define FD_PTR 0x08000000
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
   UWORD    Unused;
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
};

struct Edges {
   LONG Left;    // Left-most coordinate
   LONG Top;     // Top coordinate
   LONG Right;   // Right-most coordinate
   LONG Bottom;  // Bottom coordinate
};

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

// Flags for NotifySubscribers

#define NSF_EXCLUSIVE 0x00000001
#define NSF_LOCAL 0x00000002
#define NSF_LOCAL_TASK 0x00000002
#define NSF_OTHER_TASKS 0x00000004
#define NSF_DELAY 0x00000008
#define NSF_FORCE_DELAY 0x00000008

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
#define TSF_DUMMY 0x00000002
#define TSF_WAIT 0x00000004
#define TSF_RESET_PATH 0x00000008
#define TSF_PRIVILEGED 0x00000010
#define TSF_SHELL 0x00000020
#define TSF_DEBUG 0x00000040
#define TSF_QUIET 0x00000080
#define TSF_DETACHED 0x00000100
#define TSF_ATTACHED 0x00000200
#define TSF_PIPE 0x00000400

#define AHASH_ACTIONNOTIFY 0xa04a409c
#define AHASH_ACTIVATE 0xdbaf4876
#define AHASH_ACCESSOBJECT 0xbcf3b98e
#define AHASH_CLEAR 0x0f3b6d8c
#define AHASH_FREEWARNING 0xb903ddbd
#define AHASH_OWNERDESTROYED 0x77295103
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
#define THF_MSG_HANDLER 0x00000002

// Flags for the SetDate() file method.

#define FDT_MODIFIED 0
#define FDT_CREATED 1
#define FDT_ACCESSED 2
#define FDT_ARCHIVED 3

#define VOLUME_REPLACE 0x00000001
#define VOLUME_PRIORITY 0x00000002
#define VOLUME_HIDDEN 0x00000004
#define VOLUME_SAVE 0x00000008
#define VOLUME_SYSTEM 0x00000010

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

#define OPF_NAME 0x00000001
#define OPF_COPYRIGHT 0x00000002
#define OPF_DATE 0x00000004
#define OPF_AUTHOR 0x00000008
#define OPF_CORE_VERSION 0x00000010
#define OPF_JUMPTABLE 0x00000020
#define OPF_MAX_DEPTH 0x00000040
#define OPF_DETAIL 0x00000080
#define OPF_SHOW_MEMORY 0x00000100
#define OPF_SHOW_IO 0x00000200
#define OPF_SHOW_ERRORS 0x00000400
#define OPF_ARGS 0x00000800
#define OPF_ERROR 0x00001000
#define OPF_COMPILED_AGAINST 0x00002000
#define OPF_PRIVILEGED 0x00004000
#define OPF_SYSTEM_PATH 0x00008000
#define OPF_MODULE_PATH 0x00010000
#define OPF_ROOT_PATH 0x00020000
#define OPF_SCAN_MODULES 0x00040000
#define OPF_GLOBAL_INSTANCE 0x00080000
#define OPF_OPTIONS 0x00100000
#define OPF_SHOW_PUBLIC_MEM 0x00200000

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

// Tags for SetVolume()

#define AST_Path 1LL
#define AST_PATH 1LL
#define AST_Name 2LL
#define AST_NAME 2LL
#define AST_Flags 3LL
#define AST_FLAGS 3LL
#define AST_Icon 4LL
#define AST_ICON 4LL
#define AST_Comment 5LL
#define AST_COMMENT 5LL
#define AST_Label 6LL
#define AST_LABEL 6LL
#define AST_Device 7LL
#define AST_DEVICE 7LL
#define AST_DevicePath 8LL
#define AST_DEVICE_PATH 8LL
#define AST_ID 9LL

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
#define FOF_INCLUDE_SHARED 0x00000002

// Flags that can be passed to NewObject().  If a flag needs to be stored with the object, it must be specified in the lower word.

#define NF_PRIVATE 0x00000000
#define NF_UNTRACKED 0x00000001
#define NF_NO_TRACK 0x00000001
#define NF_SHARED 0x00000002
#define NF_PUBLIC 0x00000002
#define NF_FOREIGN_OWNER 0x00000004
#define NF_INITIALISED 0x00000008
#define NF_INTEGRAL 0x00000010
#define NF_UNLOCK_FREE 0x00000020
#define NF_FREE 0x00000040
#define NF_TIMER_SUB 0x00000080
#define NF_CREATE_OBJECT 0x00000100
#define NF_COLLECT 0x00000200
#define NF_NEW_OBJECT 0x00000400
#define NF_RECLASSED 0x00000800
#define NF_MESSAGE 0x00001000
#define NF_SIGNALLED 0x00002000
#define NF_HAS_SHARED_RESOURCES 0x00004000
#define NF_UNIQUE 0x40000000
#define NF_NAME 0x80000000

// Reserved Public Memory identifiers.

#define RPM_SharedObjects -1000
#define RPM_Audio -1001
#define RPM_Clipboard -1002
#define RPM_X11 -1003
#define RPM_AlphaBlend -1004
#define RPM_XWindowLookup -1005
#define RPM_FocusList -1006
#define RPM_InputEvents -1007
#define RPM_DisplayInfo -1008

#define MAX_FILENAME 256

// Flags for StrCopy()

#define COPY_ALL 0x7fffffff

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
#define MSGID_ACTION_RESULT 96
#define MSGID_GET_FIELD 97
#define MSGID_SET_FIELD 98
#define MSGID_ACTION 99
#define MSGID_CORE_END 100
#define MSGID_EXPOSE 100
#define MSGID_COMMAND 101
#define MSGID_BREAK 102
#define MSGID_QUIT 1000

// Flags for ListTasks()

#define LTF_CURRENT_PROCESS 1

// Special tags for SubscribeActionTags()

#define SUB_FAIL_EXISTS 0x7ffffffe
#define SUB_WARN_EXISTS 0x7fffffff

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
#define RES_GLOBAL_INSTANCE 3
#define RES_SHARED_CONTROL 4
#define RES_USER_ID 5
#define RES_DISPLAY_DRIVER 6
#define RES_PRIVILEGED_USER 7
#define RES_PRIVILEGED 8
#define RES_RANDOM_SEED 9
#define RES_PARENT_CONTEXT 10
#define RES_LOG_LEVEL 11
#define RES_TOTAL_SHARED_MEMORY 12
#define RES_TASK_CONTROL 13
#define RES_TASK_LIST 14
#define RES_MAX_PROCESSES 15
#define RES_LOG_DEPTH 16
#define RES_JNI_ENV 17
#define RES_THREAD_ID 18
#define RES_CURRENT_MSG 19
#define RES_OPEN_INFO 20
#define RES_EXCEPTION_HANDLER 21
#define RES_NET_PROCESSING 22
#define RES_PROCESS_STATE 23
#define RES_TOTAL_MEMORY 24
#define RES_TOTAL_SWAP 25
#define RES_CPU_SPEED 26
#define RES_SHARED_BLOCKS 27
#define RES_FREE_MEMORY 28
#define RES_FREE_SWAP 29
#define RES_KEY_STATE 30
#define RES_CORE_IDL 31

// Path types for SetResourcePath()

#define RP_MODULE_PATH 1
#define RP_SYSTEM_PATH 2
#define RP_ROOT_PATH 3

// Flags for the MetaClass.

#define CLF_SHARED_ONLY 0x00000001
#define CLF_SHARED_OBJECTS 0x00000001
#define CLF_PRIVATE_ONLY 0x00000002
#define CLF_PROMOTE_INTEGRAL 0x00000004
#define CLF_PUBLIC_OBJECTS 0x00000008
#define CLF_XML_CONTENT 0x00000010
#define CLF_NO_OWNERSHIP 0x00000020

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

struct ObjectSignal {
   OBJECTPTR Object;
};

struct ResourceManager {
   CSTRING Name;           // The name of the resource.
   void (*Free)(APTR);     // A function that will remove the resource's content when terminated.
};

typedef struct rkBase64Decode {
   UBYTE Step;             // Internal
   UBYTE PlainChar;        // Internal
   UBYTE Initialised:1;    // Internal
} BASE64DECODE;

typedef struct rkBase64Encode {
   UBYTE Step;        // Internal
   UBYTE Result;      // Internal
   LONG  StepCount;   // Internal
} BASE64ENCODE;

struct FeedSubscription {
   OBJECTID SubscriberID;    // Subscriber ID
   MEMORYID MessagePortMID;  // Message port for the subscriber
   CLASSID  ClassID;         // Class of the subscriber
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
   LONG    HeaderVersion;                           // The version of this structure.
   LONG    Flags;                                   // Special flags, type of function table wanted from the Core
   FLOAT   ModVersion;                              // Version of this module
   FLOAT   CoreVersion;                             // Core version compiled against
   CSTRING Definitions;                             // Module definition string, usable by run-time languages such as Fluid
   ERROR (*Init)(OBJECTPTR, struct CoreBase *);     // A one-off initialisation routine for when the module is first opened.
   void (*Close)(OBJECTPTR);                        // A function that will be called each time the module is closed.
   ERROR (*Open)(OBJECTPTR);                        // A function that will be called each time the module is opened.
   ERROR (*Expunge)(void);                          // Reference to an expunge function to terminate the module.
   const struct Function * DefaultList;             // Pointer to default function list
   CSTRING Name;                                    // Name of the module
   struct ModuleMaster * Master;                    // Private, must be set to zero
};

struct FieldArray {
   CSTRING Name;    // The name of the field, e.g. "Width"
   ULONG   Flags;   // Special flags that describe the field
   MAXINT  Arg;     // Can be a pointer or an integer value
   APTR    GetField; // void GetField(*Object, APTR Result);
   APTR    SetField; // ERROR SetField(*Object, APTR Value);
};

struct FieldDef {
   CSTRING Name;    // The name of the constant.
   LONG    Value;   // The value of the constant.
};

struct SystemState {
   CSTRING * ErrorMessages;    // A sorted array of all error codes, translated into human readable strings.
   CSTRING   RootPath;         // The current root path, which defaults to the location of the installation folder.
   CSTRING   SystemPath;       // The current path of the 'system:' volume.
   CSTRING   ModulePath;       // The current path to the system modules, normally 'system:modules/'
   CSTRING   Platform;         // String-based field indicating the user's platform.  Currently returns 'Native', 'Windows', 'OSX' or 'Linux'.
   HOSTHANDLE ConsoleFD;       // Internal
   LONG      CoreVersion;      // Reflects the Core version number.
   LONG      CoreRevision;     // Reflects the Core revision number.
   LONG      InstanceID;       // This is the ID of the instance that the calling process resides in.
   LONG      TotalErrorMessages; // The total number of error codes listed in the ErrorMessages array.
   LONG      Stage;            // The current operating stage.  -1 = Initialising, 0 indicates normal operating status; 1 means that the program is shutting down; 2 indicates a program restart; 3 is for mode switches.
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
};

struct MethodArray {
   LONG    MethodID;                     // Unique method identifier
   APTR    Routine;                      // The method entry point, defined as ERROR (*Routine)(OBJECTPTR, APTR);
   CSTRING Name;                         // Name of the method
   const struct FunctionField * Args;    // List of parameters accepted by the method
   LONG    Size;                         // Total byte-size of all accepted parameters when they are assembled as a C structure.
};

struct MemoryLocks {
   MEMORYID MemoryID;    // Reference to a memory ID.
   WORD     Locks;       // The total number of locks on the memory block.
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

struct ListTasks {
   LONG     ProcessID;                  // Core process ID
   OBJECTID TaskID;                     // Task ID for this array entry
   LONG     WaitingProcessID;           // If the task is waiting, this field reflects the other task's ID
   MEMORYID WaitingMemoryID;            // If the task is waiting, this field reflects the memory ID
   LONG     WaitingTime;                // If the task is waiting, the time at which the sleep started (msec)
   MEMORYID MessageID;                  // Message queue ID
   OBJECTID OutputID;                   // The object that the task should output information to
   HOSTHANDLE Semaphore;                // Semaphore for IPC
   LONG     InstanceID;                 // Instance that the task belongs to
   LONG     TotalMemoryLocks;           // Total number of held memory locks
   OBJECTID ModalID;                    // Refers to any surface that currently holds a modal lock.
   struct MemoryLocks * MemoryLocks;    // An array of memory locks currently held by the process.
};

struct FDTable {
   HOSTHANDLE FD;                         // The file descriptor that is managed by this record.
   void (*Routine)(HOSTHANDLE, APTR);     // The routine that will process read/write messages for the FD.
   APTR Data;                             // A user specific data pointer.
   LONG Flags;                            // Set to RFD_READ, RFD_WRITE or RFD_EXCEPT.
};

struct Message {
   LARGE Time;       // A timestamp acquired from PreciseTime() when the message was first passed to SendMessage().
   LONG  UniqueID;   // A unique identifier automatically created by SendMessage().
   LONG  Type;       // A message type identifier as defined by the client.
   LONG  Size;       // The size of the message data, in bytes.  If there is no data associated with the message, the Size will be set to zero.</>
};

struct ExposeMessage {
   OBJECTID ObjectID;    // Target surface
   LONG     X;           // Horizontal starting coordinate
   LONG     Y;           // Vertical starting coordinate
   LONG     Width;       // Width of exposed area
   LONG     Height;      // Height of exposed area
   LONG     Flags;       // Optional flags
};

struct DebugMessage {
   LONG DebugID;    // Internal
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
   OBJECTID LockID;      // Reference to the task that currently has a lock on the block.
   OBJECTID TaskID;      // The Task that owns the memory block
   LONG     Handle;      // Native system handle (e.g. the shmid in Linux)
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

struct KeyStore {
   APTR Mutex;                 // Internal mutex for managing thread-safety.
   struct KeyPair * * Data;    // Key-pairs are stored here.
   LONG TableSize;             // The size of the available storage area.
   LONG Total;                 // Total number of currently stored key-pairs, including dead keys.
   LONG Flags;                 // Optional flags used for VarNew()
};

struct CacheFile {
   LARGE  TimeStamp;   // The file's last-modified timestamp.
   LARGE  Size;        // Byte size of the cached data.
   LARGE  LastUse;     // The last time that this file was requested.
   STRING Path;        // Pointer to the resolved file path.
   APTR   Data;        // Pointer to the cached data.
   WORD   Locks;       // Internal count of active locks for this element.
   WORD   PathLength;  // Length of the Path string, including trailing zero.
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
   struct KeyStore * Tags;          // Any archive specific information is expressed here as key value pairs.
   LONG    Permissions;             // Original permissions - see PERMIT flags.
   LONG    UserID;                  // Original user ID
   LONG    GroupID;                 // Original group ID
   LONG    OthersID;                // Original others ID
   LONG    Flags;                   // FL flags
   struct DateTime Created;         // Date and time of the file's creation.
   struct DateTime Modified;        // Date and time last modified.
};

struct FileInfo {
   LARGE  Size;               // The size of the file's content.
   LARGE  TimeStamp;          // 64-bit time stamp - usable only for comparison (e.g. sorting).
   struct FileInfo * Next;    // Next structure in the list, or NULL.
   STRING Name;               // The name of the file.  This string remains valid until the next call to GetFileInfo().
   struct KeyStore * Tags;    // A store of special tag strings that are file-specific.
   LONG   Flags;              // Additional flags to describe the file.
   LONG   Permissions;        // Standard permission flags.
   LONG   UserID;             // User  ID (Unix systems only).
   LONG   GroupID;            // Group ID (Unix systems only).
   struct DateTime Created;   // The date/time of the file's creation.
   struct DateTime Modified;  // The date/time of the last file modification.
};

struct DirInfo {
   struct FileInfo * Info;    // Pointer to a FileInfo structure
   #ifdef PRV_FILE
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

struct CoreBase {
   ERROR (*_AccessMemory)(MEMORYID, LONG, LONG, APTR);
   ERROR (*_Action)(LONG, OBJECTPTR, APTR);
   void (*_ActionList)(struct ActionTable **, LONG *);
   ERROR (*_ActionMsg)(LONG, OBJECTID, APTR, MEMORYID, CLASSID);
   ERROR (*_ActionTags)(LONG, OBJECTPTR, ...);
   CSTRING (*_ResolveClassID)(CLASSID);
   LONG (*_AllocateID)(LONG);
   ERROR (*_AllocMemory)(LONG, LONG, APTR, MEMORYID *);
   ERROR (*_AccessObject)(OBJECTID, LONG, APTR);
   ERROR (*_ListTasks)(LONG, struct ListTasks **);
   ERROR (*_CheckAction)(OBJECTPTR, LONG);
   ERROR (*_CheckMemoryExists)(MEMORYID);
   ERROR (*_CheckObjectExists)(OBJECTID);
   ERROR (*_CloneMemory)(APTR, LONG, APTR, MEMORYID *);
   ERROR (*_CreateObject)(LARGE, LONG, APTR, ...);
   OBJECTPTR (*_CurrentContext)(void);
   ERROR (*_GetFieldArray)(OBJECTPTR, FIELD, APTR, LONG *);
   LONG (*_AdjustLogLevel)(LONG);
   void (*_LogF)(const void *, const char *, ...) __attribute__((format(printf, 2, 3)));
   ERROR (*_FindObject)(CSTRING, CLASSID, LONG, OBJECTID *, LONG *);
   objMetaClass * (*_FindClass)(CLASSID);
   ERROR (*_ReleaseObject)(OBJECTPTR);
   ERROR (*_FreeResource)(const void *);
   ERROR (*_FreeResourceID)(MEMORYID);
   CLASSID (*_GetClassID)(OBJECTID);
   OBJECTID (*_GetOwnerID)(OBJECTID);
   ERROR (*_GetField)(OBJECTPTR, FIELD, APTR);
   ERROR (*_GetFieldVariable)(OBJECTPTR, CSTRING, STRING, LONG);
   ERROR (*_GetFields)(OBJECTPTR, ...);
   CSTRING (*_GetName)(OBJECTPTR);
   ERROR (*_ListChildren)(OBJECTID, LONG, struct ChildEntry *, LONG *);
   ERROR (*_Base64Decode)(struct rkBase64Decode *, CSTRING, LONG, APTR, LONG *);
   ERROR (*_RegisterFD)(HOSTHANDLE, LONG, void (*Routine)(HOSTHANDLE, APTR), APTR);
   ERROR (*_ManageAction)(LONG, APTR);
   ERROR (*_MemoryIDInfo)(MEMORYID, struct MemInfo *, LONG);
   ERROR (*_MemoryPtrInfo)(APTR, struct MemInfo *, LONG);
   ERROR (*_NewObject)(LARGE, LONG, APTR);
   LONG (*_NotifySubscribers)(OBJECTPTR, LONG, APTR, LONG, ERROR);
   ERROR (*_StrReadLocale)(CSTRING, CSTRING *);
   APTR (*_GetMemAddress)(MEMORYID);
   ERROR (*_ProcessMessages)(LONG, LONG);
   LONG (*_RandomNumber)(LONG);
   ERROR (*_ReallocMemory)(APTR, LONG, APTR, MEMORYID *);
   ERROR (*_GetMessage)(MEMORYID, LONG, LONG, APTR, LONG);
   MEMORYID (*_ReleaseMemory)(APTR);
   CLASSID (*_ResolveClassName)(CSTRING);
   ERROR (*_KeySet)(struct KeyStore *, ULONG, const void *, LONG);
   ERROR (*_SendMessage)(MEMORYID, LONG, LONG, APTR, LONG);
   ERROR (*_SetOwner)(OBJECTPTR, OBJECTPTR);
   OBJECTPTR (*_SetContext)(OBJECTPTR);
   ERROR (*_SetField)(OBJECTPTR, FIELD, ...);
   ERROR (*_SetFields)(OBJECTPTR, ...);
   ERROR (*_SetFieldEval)(OBJECTPTR, CSTRING, CSTRING);
   ERROR (*_SetName)(OBJECTPTR, CSTRING);
   void (*_LogReturn)(void);
   ERROR (*_StrCompare)(CSTRING, CSTRING, LONG, LONG);
   ERROR (*_SubscribeAction)(OBJECTPTR, LONG);
   ERROR (*_VarGet)(struct KeyStore *, CSTRING, APTR, LONG *);
   ERROR (*_SubscribeEvent)(LARGE, FUNCTION *, APTR, APTR);
   ERROR (*_SubscribeTimer)(DOUBLE, FUNCTION *, APTR);
   ERROR (*_UpdateTimer)(APTR, DOUBLE);
   ERROR (*_UnsubscribeAction)(OBJECTPTR, LONG);
   APTR (*_VarSet)(struct KeyStore *, CSTRING, APTR, LONG);
   void (*_UnsubscribeEvent)(APTR);
   ERROR (*_BroadcastEvent)(APTR, LONG);
   void (*_WaitTime)(LONG, LONG);
   LARGE (*_GetEventID)(LONG, CSTRING, CSTRING);
   ULONG (*_GenCRC32)(ULONG, APTR, ULONG);
   LARGE (*_GetResource)(LONG);
   LARGE (*_SetResource)(LONG, LARGE);
   ERROR (*_ScanMessages)(APTR, LONG *, LONG, APTR, LONG);
   ERROR (*_SysLock)(LONG, LONG);
   ERROR (*_SysUnlock)(LONG);
   ERROR (*_CopyMemory)(const void *, APTR, LONG);
   ERROR (*_LoadFile)(CSTRING, LONG, struct CacheFile **);
   ERROR (*_SubscribeActionTags)(OBJECTPTR, ...);
   void (*_PrintDiagnosis)(LONG, LONG);
   ERROR (*_NewLockedObject)(LARGE, LONG, APTR, OBJECTID *, CSTRING);
   ERROR (*_UpdateMessage)(APTR, LONG, LONG, APTR, LONG);
   ERROR (*_AddMsgHandler)(APTR, LONG, FUNCTION *, struct MsgHandler **);
   ERROR (*_FindPrivateObject)(CSTRING, APTR);
   LARGE (*_PreciseTime)(void);
   ERROR (*_SetFieldsID)(OBJECTID, ...);
   OBJECTPTR (*_GetObjectPtr)(OBJECTID);
   struct Field * (*_FindField)(OBJECTPTR, ULONG, APTR);
   LONG (*_GetMsgPort)(OBJECTID);
   CSTRING (*_GetErrorMsg)(ERROR);
   struct Message * (*_GetActionMsg)(void);
   ERROR (*_FuncError)(CSTRING, ERROR);
   ERROR (*_SetArray)(OBJECTPTR, FIELD, APTR, LONG);
   ERROR (*_ReleaseMemoryID)(MEMORYID);
   ERROR (*_AccessPrivateObject)(OBJECTPTR, LONG);
   void (*_ReleasePrivateObject)(OBJECTPTR);
   ERROR (*_AllocMutex)(LONG, APTR);
   void (*_FreeMutex)(APTR);
   ERROR (*_LockMutex)(APTR, LONG);
   void (*_UnlockMutex)(APTR);
   ERROR (*_ActionThread)(LONG, OBJECTPTR, APTR, FUNCTION *, LONG);
   struct KeyStore * (*_VarNew)(LONG, LONG);
   ERROR (*_AllocSharedMutex)(CSTRING, APTR);
   void (*_FreeSharedMutex)(APTR);
   ERROR (*_LockSharedMutex)(APTR, LONG);
   void (*_UnlockSharedMutex)(APTR);
   void (*_VLogF)(int, const char *, const char *, va_list);
   LONG (*_StrSearch)(CSTRING, CSTRING, LONG);
   ERROR (*_VarSetSized)(struct KeyStore *, CSTRING, LONG, APTR, LONG *);
   ERROR (*_VarLock)(struct KeyStore *, LONG);
   ERROR (*_WakeProcess)(LONG);
   ERROR (*_SetResourcePath)(LONG, CSTRING);
   OBJECTPTR (*_CurrentTask)(void);
   ERROR (*_KeyIterate)(struct KeyStore *, ULONG, ULONG *, APTR, LONG *);
   CSTRING (*_ResolveGroupID)(LONG);
   LONG (*_StrCopy)(CSTRING, STRING, LONG);
   STRING (*_StrClone)(CSTRING);
   void (*_VarUnlock)(struct KeyStore *);
   CSTRING (*_ResolveUserID)(LONG);
   ERROR (*_StrSort)(CSTRING *, LONG);
   STRING * (*_StrBuildArray)(STRING, LONG, LONG, LONG);
   LONG (*_UTF8CharOffset)(CSTRING, LONG);
   LONG (*_UTF8Length)(CSTRING);
   LONG (*_UTF8OffsetToChar)(CSTRING, LONG);
   LONG (*_UTF8PrevLength)(CSTRING, LONG);
   LONG (*_UTF8CharLength)(CSTRING);
   ULONG (*_UTF8ReadValue)(CSTRING, LONG *);
   LONG (*_UTF8WriteValue)(LONG, STRING, LONG);
   LONG (*_StrFormat)(const void *, LONG, const char *, ...) __attribute__((format(printf, 3, 4)));
   ERROR (*_SaveImageToFile)(OBJECTPTR, CSTRING, CLASSID, LONG);
   ERROR (*_ReadFileToBuffer)(CSTRING, APTR, LONG, LONG *);
   LONG (*_StrDatatype)(CSTRING);
   void (*_UnloadFile)(struct CacheFile *);
   void (*_SetDefaultPermissions)(LONG, LONG, LONG);
   ERROR (*_CompareFilePaths)(CSTRING, CSTRING);
   const struct SystemState * (*_GetSystemState)(void);
   LONG (*_StrSortCompare)(CSTRING, CSTRING);
   ERROR (*_AddInfoTag)(struct FileInfo *, CSTRING, CSTRING);
   LONG (*_UTF8Copy)(CSTRING, STRING, LONG, LONG);
   LONG (*_Base64Encode)(const void *, LONG, STRING, LONG);
   ERROR (*_VarSetString)(struct KeyStore *, CSTRING, CSTRING);
   CSTRING (*_VarGetString)(struct KeyStore *, CSTRING);
   ERROR (*_VarCopy)(struct KeyStore *, struct KeyStore *);
   ULONG (*_StrHash)(CSTRING, LONG);
   CSTRING (*_UTF8ValidEncoding)(CSTRING, CSTRING);
   ERROR (*_AnalysePath)(CSTRING, LONG *);
   ERROR (*_CreateFolder)(CSTRING, LONG);
   ERROR (*_MoveFile)(CSTRING, CSTRING, FUNCTION *);
   ERROR (*_ResolvePath)(CSTRING, LONG, STRING *);
   ERROR (*_SetVolume)(LARGE,...);
   ERROR (*_DeleteVolume)(CSTRING);
   ERROR (*_VirtualVolume)(CSTRING, ...);
   ERROR (*_CopyFile)(CSTRING, CSTRING, FUNCTION *);
   ERROR (*_KeyGet)(struct KeyStore *, ULONG, APTR, LONG *);
   ERROR (*_VarIterate)(struct KeyStore *, CSTRING, CSTRING *, APTR, LONG *);
   ERROR (*_DeleteFile)(CSTRING, FUNCTION *);
   ERROR (*_WaitForObjects)(LONG, LONG, struct ObjectSignal *);
   ERROR (*_SaveObjectToFile)(OBJECTPTR, CSTRING, LONG);
   ERROR (*_OpenDir)(CSTRING, LONG, struct DirInfo **);
   ERROR (*_ScanDir)(struct DirInfo *);
   ERROR (*_IdentifyFile)(CSTRING, CSTRING, LONG, CLASSID *, CLASSID *, STRING *);
   ERROR (*_TranslateCmdRef)(CSTRING, STRING *);
   ERROR (*_CreateLink)(CSTRING, CSTRING);
};

#ifndef PRV_CORE_MODULE
#define AccessMemory(...) (CoreBase->_AccessMemory)(__VA_ARGS__)
#define Action(...) (CoreBase->_Action)(__VA_ARGS__)
#define ActionList(...) (CoreBase->_ActionList)(__VA_ARGS__)
#define ActionMsg(...) (CoreBase->_ActionMsg)(__VA_ARGS__)
#define ActionTags(...) (CoreBase->_ActionTags)(__VA_ARGS__)
#define ResolveClassID(...) (CoreBase->_ResolveClassID)(__VA_ARGS__)
#define AllocateID(...) (CoreBase->_AllocateID)(__VA_ARGS__)
#define AllocMemory(...) (CoreBase->_AllocMemory)(__VA_ARGS__)
#define AccessObject(...) (CoreBase->_AccessObject)(__VA_ARGS__)
#define ListTasks(...) (CoreBase->_ListTasks)(__VA_ARGS__)
#define CheckAction(...) (CoreBase->_CheckAction)(__VA_ARGS__)
#define CheckMemoryExists(...) (CoreBase->_CheckMemoryExists)(__VA_ARGS__)
#define CheckObjectExists(...) (CoreBase->_CheckObjectExists)(__VA_ARGS__)
#define CloneMemory(...) (CoreBase->_CloneMemory)(__VA_ARGS__)
#define CreateObject(...) (CoreBase->_CreateObject)(__VA_ARGS__)
#define CurrentContext(...) (CoreBase->_CurrentContext)(__VA_ARGS__)
#define GetFieldArray(...) (CoreBase->_GetFieldArray)(__VA_ARGS__)
#define AdjustLogLevel(...) (CoreBase->_AdjustLogLevel)(__VA_ARGS__)
#define LogF(...) (CoreBase->_LogF)(__VA_ARGS__)
#define FindObject(...) (CoreBase->_FindObject)(__VA_ARGS__)
#define FindClass(...) (CoreBase->_FindClass)(__VA_ARGS__)
#define ReleaseObject(...) (CoreBase->_ReleaseObject)(__VA_ARGS__)
#define FreeResource(...) (CoreBase->_FreeResource)(__VA_ARGS__)
#define FreeResourceID(...) (CoreBase->_FreeResourceID)(__VA_ARGS__)
#define GetClassID(...) (CoreBase->_GetClassID)(__VA_ARGS__)
#define GetOwnerID(...) (CoreBase->_GetOwnerID)(__VA_ARGS__)
#define GetField(...) (CoreBase->_GetField)(__VA_ARGS__)
#define GetFieldVariable(...) (CoreBase->_GetFieldVariable)(__VA_ARGS__)
#define GetFields(...) (CoreBase->_GetFields)(__VA_ARGS__)
#define GetName(...) (CoreBase->_GetName)(__VA_ARGS__)
#define ListChildren(...) (CoreBase->_ListChildren)(__VA_ARGS__)
#define Base64Decode(...) (CoreBase->_Base64Decode)(__VA_ARGS__)
#define RegisterFD(...) (CoreBase->_RegisterFD)(__VA_ARGS__)
#define ManageAction(...) (CoreBase->_ManageAction)(__VA_ARGS__)
#define MemoryIDInfo(a,b) (CoreBase->_MemoryIDInfo)(a,b,sizeof(*b))
#define MemoryPtrInfo(a,b) (CoreBase->_MemoryPtrInfo)(a,b,sizeof(*b))
#define NewObject(...) (CoreBase->_NewObject)(__VA_ARGS__)
#define NotifySubscribers(...) (CoreBase->_NotifySubscribers)(__VA_ARGS__)
#define StrReadLocale(...) (CoreBase->_StrReadLocale)(__VA_ARGS__)
#define GetMemAddress(...) (CoreBase->_GetMemAddress)(__VA_ARGS__)
#define ProcessMessages(...) (CoreBase->_ProcessMessages)(__VA_ARGS__)
#define RandomNumber(...) (CoreBase->_RandomNumber)(__VA_ARGS__)
#define ReallocMemory(...) (CoreBase->_ReallocMemory)(__VA_ARGS__)
#define GetMessage(...) (CoreBase->_GetMessage)(__VA_ARGS__)
#define ReleaseMemory(...) (CoreBase->_ReleaseMemory)(__VA_ARGS__)
#define ResolveClassName(...) (CoreBase->_ResolveClassName)(__VA_ARGS__)
#define KeySet(...) (CoreBase->_KeySet)(__VA_ARGS__)
#define SendMessage(...) (CoreBase->_SendMessage)(__VA_ARGS__)
#define SetOwner(...) (CoreBase->_SetOwner)(__VA_ARGS__)
#define SetContext(...) (CoreBase->_SetContext)(__VA_ARGS__)
#define SetField(...) (CoreBase->_SetField)(__VA_ARGS__)
#define SetFields(...) (CoreBase->_SetFields)(__VA_ARGS__)
#define SetFieldEval(...) (CoreBase->_SetFieldEval)(__VA_ARGS__)
#define SetName(...) (CoreBase->_SetName)(__VA_ARGS__)
#define LogReturn(...) (CoreBase->_LogReturn)(__VA_ARGS__)
#define StrCompare(...) (CoreBase->_StrCompare)(__VA_ARGS__)
#define SubscribeAction(...) (CoreBase->_SubscribeAction)(__VA_ARGS__)
#define VarGet(...) (CoreBase->_VarGet)(__VA_ARGS__)
#define SubscribeEvent(...) (CoreBase->_SubscribeEvent)(__VA_ARGS__)
#define SubscribeTimer(...) (CoreBase->_SubscribeTimer)(__VA_ARGS__)
#define UpdateTimer(...) (CoreBase->_UpdateTimer)(__VA_ARGS__)
#define UnsubscribeAction(...) (CoreBase->_UnsubscribeAction)(__VA_ARGS__)
#define VarSet(...) (CoreBase->_VarSet)(__VA_ARGS__)
#define UnsubscribeEvent(...) (CoreBase->_UnsubscribeEvent)(__VA_ARGS__)
#define BroadcastEvent(...) (CoreBase->_BroadcastEvent)(__VA_ARGS__)
#define WaitTime(...) (CoreBase->_WaitTime)(__VA_ARGS__)
#define GetEventID(...) (CoreBase->_GetEventID)(__VA_ARGS__)
#define GenCRC32(...) (CoreBase->_GenCRC32)(__VA_ARGS__)
#define GetResource(...) (CoreBase->_GetResource)(__VA_ARGS__)
#define SetResource(...) (CoreBase->_SetResource)(__VA_ARGS__)
#define ScanMessages(...) (CoreBase->_ScanMessages)(__VA_ARGS__)
#define SysLock(...) (CoreBase->_SysLock)(__VA_ARGS__)
#define SysUnlock(...) (CoreBase->_SysUnlock)(__VA_ARGS__)
#define CopyMemory(...) (CoreBase->_CopyMemory)(__VA_ARGS__)
#define LoadFile(...) (CoreBase->_LoadFile)(__VA_ARGS__)
#define SubscribeActionTags(...) (CoreBase->_SubscribeActionTags)(__VA_ARGS__)
#define PrintDiagnosis(...) (CoreBase->_PrintDiagnosis)(__VA_ARGS__)
#define NewLockedObject(...) (CoreBase->_NewLockedObject)(__VA_ARGS__)
#define UpdateMessage(...) (CoreBase->_UpdateMessage)(__VA_ARGS__)
#define AddMsgHandler(...) (CoreBase->_AddMsgHandler)(__VA_ARGS__)
#define FindPrivateObject(...) (CoreBase->_FindPrivateObject)(__VA_ARGS__)
#define PreciseTime(...) (CoreBase->_PreciseTime)(__VA_ARGS__)
#define SetFieldsID(...) (CoreBase->_SetFieldsID)(__VA_ARGS__)
#define GetObjectPtr(...) (CoreBase->_GetObjectPtr)(__VA_ARGS__)
#define FindField(...) (CoreBase->_FindField)(__VA_ARGS__)
#define GetMsgPort(...) (CoreBase->_GetMsgPort)(__VA_ARGS__)
#define GetErrorMsg(...) (CoreBase->_GetErrorMsg)(__VA_ARGS__)
#define GetActionMsg(...) (CoreBase->_GetActionMsg)(__VA_ARGS__)
#define FuncError(...) (CoreBase->_FuncError)(__VA_ARGS__)
#define SetArray(...) (CoreBase->_SetArray)(__VA_ARGS__)
#define ReleaseMemoryID(...) (CoreBase->_ReleaseMemoryID)(__VA_ARGS__)
#define AccessPrivateObject(...) (CoreBase->_AccessPrivateObject)(__VA_ARGS__)
#define ReleasePrivateObject(...) (CoreBase->_ReleasePrivateObject)(__VA_ARGS__)
#define AllocMutex(...) (CoreBase->_AllocMutex)(__VA_ARGS__)
#define FreeMutex(...) (CoreBase->_FreeMutex)(__VA_ARGS__)
#define LockMutex(...) (CoreBase->_LockMutex)(__VA_ARGS__)
#define UnlockMutex(...) (CoreBase->_UnlockMutex)(__VA_ARGS__)
#define ActionThread(...) (CoreBase->_ActionThread)(__VA_ARGS__)
#define VarNew(...) (CoreBase->_VarNew)(__VA_ARGS__)
#define AllocSharedMutex(...) (CoreBase->_AllocSharedMutex)(__VA_ARGS__)
#define FreeSharedMutex(...) (CoreBase->_FreeSharedMutex)(__VA_ARGS__)
#define LockSharedMutex(...) (CoreBase->_LockSharedMutex)(__VA_ARGS__)
#define UnlockSharedMutex(...) (CoreBase->_UnlockSharedMutex)(__VA_ARGS__)
#define VLogF(...) (CoreBase->_VLogF)(__VA_ARGS__)
#define StrSearch(...) (CoreBase->_StrSearch)(__VA_ARGS__)
#define VarSetSized(...) (CoreBase->_VarSetSized)(__VA_ARGS__)
#define VarLock(...) (CoreBase->_VarLock)(__VA_ARGS__)
#define WakeProcess(...) (CoreBase->_WakeProcess)(__VA_ARGS__)
#define SetResourcePath(...) (CoreBase->_SetResourcePath)(__VA_ARGS__)
#define CurrentTask(...) (CoreBase->_CurrentTask)(__VA_ARGS__)
#define KeyIterate(...) (CoreBase->_KeyIterate)(__VA_ARGS__)
#define ResolveGroupID(...) (CoreBase->_ResolveGroupID)(__VA_ARGS__)
#define StrCopy(...) (CoreBase->_StrCopy)(__VA_ARGS__)
#define StrClone(...) (CoreBase->_StrClone)(__VA_ARGS__)
#define VarUnlock(...) (CoreBase->_VarUnlock)(__VA_ARGS__)
#define ResolveUserID(...) (CoreBase->_ResolveUserID)(__VA_ARGS__)
#define StrSort(...) (CoreBase->_StrSort)(__VA_ARGS__)
#define StrBuildArray(...) (CoreBase->_StrBuildArray)(__VA_ARGS__)
#define UTF8CharOffset(...) (CoreBase->_UTF8CharOffset)(__VA_ARGS__)
#define UTF8Length(...) (CoreBase->_UTF8Length)(__VA_ARGS__)
#define UTF8OffsetToChar(...) (CoreBase->_UTF8OffsetToChar)(__VA_ARGS__)
#define UTF8PrevLength(...) (CoreBase->_UTF8PrevLength)(__VA_ARGS__)
#define UTF8CharLength(...) (CoreBase->_UTF8CharLength)(__VA_ARGS__)
#define UTF8ReadValue(...) (CoreBase->_UTF8ReadValue)(__VA_ARGS__)
#define UTF8WriteValue(...) (CoreBase->_UTF8WriteValue)(__VA_ARGS__)
#define StrFormat(...) (CoreBase->_StrFormat)(__VA_ARGS__)
#define SaveImageToFile(...) (CoreBase->_SaveImageToFile)(__VA_ARGS__)
#define ReadFileToBuffer(...) (CoreBase->_ReadFileToBuffer)(__VA_ARGS__)
#define StrDatatype(...) (CoreBase->_StrDatatype)(__VA_ARGS__)
#define UnloadFile(...) (CoreBase->_UnloadFile)(__VA_ARGS__)
#define SetDefaultPermissions(...) (CoreBase->_SetDefaultPermissions)(__VA_ARGS__)
#define CompareFilePaths(...) (CoreBase->_CompareFilePaths)(__VA_ARGS__)
#define GetSystemState(...) (CoreBase->_GetSystemState)(__VA_ARGS__)
#define StrSortCompare(...) (CoreBase->_StrSortCompare)(__VA_ARGS__)
#define AddInfoTag(...) (CoreBase->_AddInfoTag)(__VA_ARGS__)
#define UTF8Copy(...) (CoreBase->_UTF8Copy)(__VA_ARGS__)
#define Base64Encode(...) (CoreBase->_Base64Encode)(__VA_ARGS__)
#define VarSetString(...) (CoreBase->_VarSetString)(__VA_ARGS__)
#define VarGetString(...) (CoreBase->_VarGetString)(__VA_ARGS__)
#define VarCopy(...) (CoreBase->_VarCopy)(__VA_ARGS__)
#define StrHash(...) (CoreBase->_StrHash)(__VA_ARGS__)
#define UTF8ValidEncoding(...) (CoreBase->_UTF8ValidEncoding)(__VA_ARGS__)
#define AnalysePath(...) (CoreBase->_AnalysePath)(__VA_ARGS__)
#define CreateFolder(...) (CoreBase->_CreateFolder)(__VA_ARGS__)
#define MoveFile(...) (CoreBase->_MoveFile)(__VA_ARGS__)
#define ResolvePath(...) (CoreBase->_ResolvePath)(__VA_ARGS__)
#define SetVolume(...) (CoreBase->_SetVolume)(__VA_ARGS__)
#define DeleteVolume(...) (CoreBase->_DeleteVolume)(__VA_ARGS__)
#define VirtualVolume(...) (CoreBase->_VirtualVolume)(__VA_ARGS__)
#define CopyFile(...) (CoreBase->_CopyFile)(__VA_ARGS__)
#define KeyGet(...) (CoreBase->_KeyGet)(__VA_ARGS__)
#define VarIterate(...) (CoreBase->_VarIterate)(__VA_ARGS__)
#define DeleteFile(...) (CoreBase->_DeleteFile)(__VA_ARGS__)
#define WaitForObjects(...) (CoreBase->_WaitForObjects)(__VA_ARGS__)
#define SaveObjectToFile(...) (CoreBase->_SaveObjectToFile)(__VA_ARGS__)
#define OpenDir(...) (CoreBase->_OpenDir)(__VA_ARGS__)
#define ScanDir(...) (CoreBase->_ScanDir)(__VA_ARGS__)
#define IdentifyFile(...) (CoreBase->_IdentifyFile)(__VA_ARGS__)
#define TranslateCmdRef(...) (CoreBase->_TranslateCmdRef)(__VA_ARGS__)
#define CreateLink(...) (CoreBase->_CreateLink)(__VA_ARGS__)
#endif


#define PRIME_HASH 2654435761UL
#define END_FIELD { NULL, 0, 0, NULL, NULL }
#define FDEF static const struct FunctionField

//****************************************************************************

#ifndef PRV_CORE_MODULE
#undef ActionMsg
#define ActionMsg(a,b,c)          (CoreBase->_ActionMsg(a,b,c,0,0))

#undef Action
#define Action(a,b,c) (CoreBase->_Action(a,b,c))

#define ActionMsgPort(a,b,c,d,e)  (CoreBase->_ActionMsg(a,b,c,d,e))
#define DeregisterFD(a)           (CoreBase->_RegisterFD((a), RFD_REMOVE|RFD_READ|RFD_WRITE|RFD_EXCEPT|RFD_ALWAYS_CALL, 0, 0))
#define DelayAction(a,b,c)        (CoreBase->_ActionMsg(a,b,c,0,(CLASSID)-1))
#define DelayMsg(a,b,c)           (CoreBase->_ActionMsg(a,b,c,0,(CLASSID)-1))
#define DeleteMsg(a,b)            (CoreBase->_UpdateMessage(a,b,(APTR)-1,0,0))
#define GetObjectAddress          (CoreBase->_GetMemAddress)

#undef NewLockedObject
#define NewLockedObject(a,b,c,d)  (CoreBase->_NewLockedObject(a,b,c,d,0))

#undef PrintDiagnosis
#define PrintDiagnosis()          (CoreBase->_PrintDiagnosis(NULL,NULL))

#define SendAction(a,b,c,d)       (CoreBase->_ActionMsg(a,b,c,d,0))

#define WaitMsg(a,b,c)            (CoreBase->_ActionMsg(a,b,c,0,-2))
#endif // PRV_CORE_MODULE

// Macros

#define GetParentContext()        ((OBJECTPTR)(MAXINT)GetResource(RES_PARENT_CONTEXT))
#define GetResourcePtr(a)         ((APTR)(MAXINT)GetResource((a)))
#define AllocPublicMemory(a,b,c)  (AllocMemory((a),(b)|MEM_PUBLIC,0,(c)))
#define AllocPrivateMemory(a,b,c) (AllocMemory((a),(b),(c),0))
#define NewPrivateObject(a,b,c)   (NewObject(a,b,c))
#define NewPublicObject(a,b,c)    (NewLockedObject(a,(b)|NF_PUBLIC,0,c))
#define NewNamedObject(a,b,c,d,e) (CoreBase->_NewLockedObject(a,(b)|NF_NAME,c,d,e))
#define StrMatch(a,b)             (StrCompare((a),(b),0,STR_MATCH_LEN))

extern struct CoreBase *CoreBase;

typedef std::map<std::string, std::string> ConfigKeys;
typedef std::pair<std::string, ConfigKeys> ConfigGroup;
typedef std::vector<ConfigGroup> ConfigGroups;

// If AUTO_OBJECT_LOCK is enabled, objects will be automatically locked to prevent thread-clashes.
// NB: Turning this off will cause issues between threads unless they call the necessary locking functions.

//#define AUTO_OBJECT_LOCK 1

//********************************************************************************************************************
// Header used for all objects.

struct BaseClass { // Must be 64-bit aligned
   union {
      objMetaClass *Class;          // Class pointer, resolved on AccessObject()
      class extMetaClass *ExtClass; // Internal class reference
   };
   struct Stats *Stats;         // Stats pointer, resolved on AccessObject() [Private]
   APTR     ChildPrivate;       // Address for the ChildPrivate structure, if allocated
   APTR     CreatorMeta;        // The creator (via NewObject) is permitted to store a custom data pointer here.
   CLASSID  ClassID;            // Reference to the object's class, used to resolve the Class pointer
   CLASSID  SubID;              // Reference to the object's sub-class, used to resolve the Class pointer
   OBJECTID UID;                // Unique object identifier
   OBJECTID OwnerID;            // Refers to the owner of this object
   WORD     Flags;              // Object flags
   WORD     MemFlags;           // Recommended memory allocation flags
   OBJECTID TaskID;             // The process that this object belongs to
   volatile LONG  ThreadID;     // Managed by locking functions
   #ifdef _WIN32
      WINHANDLE ThreadMsg;      // Pipe for sending messages to the owner thread.
   #else
      LONG ThreadMsg;
   #endif
   UBYTE ThreadPending;         // ActionThread() increments this.
   volatile BYTE Queue;         // Managed by locking functions
   volatile BYTE SleepQueue;    //
   volatile bool Locked;        // Set if locked by AccessObject()/AccessPrivateObject()
   BYTE ActionDepth;            // Incremented each time an action or method is called on the object

   inline bool initialised() { return Flags & NF_INITIALISED; }
   inline bool isPublic() { return Flags & NF_PUBLIC; }
   inline OBJECTID ownerTask() { return TaskID; }
   inline OBJECTID ownerID() { return OwnerID; }
   inline LONG memflags() { return MemFlags; }
   inline LONG flags() { return Flags; }

   CSTRING className();

   inline bool collecting() { // Is object being freed or marked for collection?
      return Flags & (NF_FREE|NF_COLLECT);
   }

   inline bool terminating() { // Is object currently being freed?
      return Flags & NF_FREE;
   }

   inline ERROR threadLock() {
      #ifdef AUTO_OBJECT_LOCK
         if (INC_QUEUE(this) IS 1) {
            ThreadID = get_thread_id();
            return ERR_Okay;
         }
         else {
            if (ThreadID IS get_thread_id()) return ERR_Okay; // If this is for the same thread then it's a nested lock, so there's no issue.
            SUB_QUEUE(this); // Put the lock count back to normal before AccessPrivateObject()
            return AccessPrivateObject(this, -1); // Can fail if object is marked for deletion.
         }
      #else
         return ERR_Okay;
      #endif
   }

   inline void threadRelease() {
      #ifdef AUTO_OBJECT_LOCK
         if (SleepQueue > 0) ReleasePrivateObject(this);
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

} __attribute__ ((aligned (8)));

#define ClassName(a) ((a)->Class->Name)

INLINE OBJECTID CurrentTaskID() { return ((OBJECTPTR)CurrentTask())->UID; }
INLINE APTR SetResourcePtr(LONG Res, APTR Value) { return (APTR)(MAXINT)(CoreBase->_SetResource(Res, (MAXINT)Value)); }
#define CONV_TIME_DATETIME(a) ((struct DateTime *)(&(a)->Year))

INLINE BYTE CMP_DATETIME(struct DateTime *one, struct DateTime *two)
{
   if (one->Year < two->Year) return -1;
   if (one->Year > two->Year) return 1;
   if (one->Month < two->Month) return -1;
   if (one->Month > two->Month) return 1;
   if (one->Day < two->Day) return -1;
   if (one->Day > two->Day) return 1;
   if (one->Minute < two->Minute) return -1;
   if (one->Minute > two->Minute) return 1;
   if (one->Hour < two->Hour) return -1;
   if (one->Hour > two->Hour) return 1;
   if (one->Second < two->Second) return -1;
   if (one->Second > two->Second) return 1;
   return 0;
}

// Macro based actions.

#define SetRead(a,b,c)  a.Buffer=(b);   a.Length=(c);
#define SetSeek(a,b,c)  a.Position=(b); a.Offset=(c);
#define SetWrite(a,b,c) a.Buffer=(b);   a.Length=(c);

// Action and Notification Structures

struct acActionNotify  { union { ACTIONID ActionID; ACTIONID Action; }; union { OBJECTID ObjectID; OBJECTID Object; }; APTR Args; LONG Size; ERROR Error; LONG Time; };
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

#define acActivate(a)         (Action(AC_Activate,(a),NULL))
#define acClear(a)            (Action(AC_Clear,(a),NULL))
#define acDeactivate(a)       (Action(AC_Deactivate,(a),NULL))
#define acDisable(a)          (Action(AC_Disable,(a),NULL))
#define acDragDrop(obj,b,c,d) (Action(AC_DragDrop,(obj),(b),(c),(d))
#define acDraw(a)             (Action(AC_Draw,(a),NULL))
#define acEnable(a)           (Action(AC_Enable,(a),NULL))
#define acFlush(a)            (Action(AC_Flush,(a),NULL))
#define acFocus(a)            (Action(AC_Focus,(a),NULL))
#define acFree(a)             (Action(AC_Free,(a),NULL))
#define acHide(a)             (Action(AC_Hide,(a),NULL))
#define acInit(a)             (Action(AC_Init,(a),NULL))
#define acLock(a)             (Action(AC_Lock,(a),NULL))
#define acLostFocus(a)        (Action(AC_LostFocus,(a),NULL))
#define acMoveToBack(a)       (Action(AC_MoveToBack,(a),NULL))
#define acMoveToFront(a)      (Action(AC_MoveToFront,(a),NULL))
#define acNext(a)             (Action(AC_Next,(a),NULL)
#define acPrev(a)             (Action(AC_Prev,(a),NULL)
#define acQuery(a)            (Action(AC_Query,(a),NULL))
#define acRefresh(a)          (Action(AC_Refresh, (a), NULL))
#define acReset(a)            (Action(AC_Reset,(a),NULL))
#define acSaveSettings(a)     (Action(AC_SaveSettings,(a),NULL))
#define acShow(a)             (Action(AC_Show,(a),NULL))
#define acSort(a)             (Action(AC_Sort,(a),NULL))
#define acUnlock(a)           (Action(AC_Unlock,(a),NULL))

#define acActivateID(a)       (ActionMsg(AC_Activate,(a),NULL))
#define acClearID(a)          (ActionMsg(AC_Clear,(a),NULL))
#define acDisableID(a)        (ActionMsg(AC_Disable,(a),NULL))
#define acDrawID(a)           (ActionMsg(AC_Draw,(a),NULL))
#define acEnableID(a)         (ActionMsg(AC_Enable,(a),NULL))
#define acFlushID(a)          (ActionMsg(AC_Flush,(a),NULL))
#define acFocusID(a)          (ActionMsg(AC_Focus,(a),NULL))
#define acFreeID(a)           (ActionMsg(AC_Free,(a),NULL))
#define acHideID(a)           (ActionMsg(AC_Hide,(a),NULL))
#define acInitID(a)           (ActionMsg(AC_Init,(a),NULL))
#define acLostFocusID(a)      (ActionMsg(AC_LostFocus,(a),NULL))
#define acMoveToBackID(a)     (ActionMsg(AC_MoveToBack,(a),NULL))
#define acMoveToFrontID(a)    (ActionMsg(AC_MoveToFront,(a),NULL))
#define acQueryID(a)          (ActionMsg(AC_Query,(a),NULL))
#define acRefreshID(a)        (ActionMsg(AC_Refresh,(a),NULL))
#define acSaveSettingsID(a)   (ActionMsg(AC_SaveSettings,(a),NULL))
#define acShowID(a)           (ActionMsg(AC_Show,(a),NULL))

INLINE ERROR acClipboard(OBJECTPTR Object, LONG Mode) {
   struct acClipboard args = { Mode };
   return Action(AC_Clipboard, Object, &args);
}

INLINE ERROR acDrawArea(OBJECTPTR Object, LONG X, LONG Y, LONG Width, LONG Height) {
   struct acDraw args = { X, Y, Width, Height };
   return Action(AC_Draw, Object, &args);
}

INLINE ERROR acDataFeed(OBJECTPTR Object, OBJECTID ObjectID, LONG Datatype, const void *Buffer, LONG Size) {
   struct acDataFeed args = { { ObjectID }, { Datatype }, Buffer, Size };
   return Action(AC_DataFeed, Object, &args);
}

INLINE ERROR acMove(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Z) {
   struct acMove args = { X, Y, Z };
   return Action(AC_Move, Object, &args);
}

INLINE ERROR acRead(OBJECTPTR Object, APTR Buffer, LONG Bytes, LONG *Read) {
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

INLINE ERROR acRedo(OBJECTPTR Object, LONG Steps) {
   struct acRedo args = { Steps };
   return Action(AC_Redo, Object, &args);
}

INLINE ERROR acRedimension(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Z, DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
   struct acRedimension args = { X, Y, Z, Width, Height, Depth };
   return Action(AC_Redimension, Object, &args);
}

INLINE ERROR acRename(OBJECTPTR Object, CSTRING Name) {
   struct acRename args = { Name };
   return Action(AC_Rename, Object, &args);
}

INLINE ERROR acResize(OBJECTPTR Object, DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
   struct acResize args = { Width, Height, Depth };
   return Action(AC_Resize, Object, &args);
}

INLINE ERROR acScroll(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Z) {
   struct acScroll args = { X, Y, Z };
   return Action(AC_Scroll, Object, &args);
}

INLINE ERROR acScrollToPoint(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Z, LONG Flags) {
   struct acScrollToPoint args = { X, Y, Z, Flags };
   return Action(AC_ScrollToPoint, Object, &args);
}

INLINE ERROR acUndo(OBJECTPTR Object, LONG Steps) {
   struct acUndo args = { Steps };
   return Action(AC_Undo, Object, &args);
}

INLINE ERROR acGetVar(OBJECTPTR Object, CSTRING FieldName, STRING Buffer, LONG Size) {
   struct acGetVar args = { FieldName, Buffer, Size };
   ERROR error = Action(AC_GetVar, Object, &args);
   if ((error) and (Buffer)) Buffer[0] = 0;
   return error;
}

INLINE ERROR acMoveToPoint(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Z, LONG Flags) {
   struct acMoveToPoint moveto = { X, Y, Z, Flags };
   return Action(AC_MoveToPoint, Object, &moveto);
}

INLINE ERROR acSaveImage(OBJECTPTR Object, OBJECTID DestID, CLASSID ClassID) {
   struct acSaveImage args = { { DestID }, { ClassID } };
   return Action(AC_SaveImage, Object, &args);
}

INLINE ERROR acSaveToObject(OBJECTPTR Object, OBJECTID DestID, CLASSID ClassID) {
   struct acSaveToObject args = { { DestID }, { ClassID } };
   return Action(AC_SaveToObject, Object, &args);
}

INLINE ERROR acSeek(OBJECTPTR Object, DOUBLE Offset, LONG Position) {
   struct acSeek args = { Offset, Position };
   return Action(AC_Seek, Object, &args);
}

INLINE ERROR acSetVars(OBJECTPTR Object, CSTRING tags, ...) {
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

INLINE ERROR acWrite(OBJECTPTR Object, CPTR Buffer, LONG Bytes, LONG *Result) {
   ERROR error;
   struct acWrite write = { (BYTE *)Buffer, Bytes };
   if (!(error = Action(AC_Write, Object, &write))) {
      if (Result) *Result = write.Result;
   }
   else if (Result) *Result = 0;
   return error;
}

INLINE LONG acWriteResult(OBJECTPTR Object, CPTR Buffer, LONG Bytes) {
   struct acWrite write = { (BYTE *)Buffer, Bytes };
   if (!Action(AC_Write, Object, &write)) return write.Result;
   else return 0;
}

#define acSeekStart(a,b)    acSeek((a),(b),SEEK_START)
#define acSeekEnd(a,b)      acSeek((a),(b),SEEK_END)
#define acSeekCurrent(a,b)  acSeek((a),(b),SEEK_CURRENT)

INLINE ERROR acSelectArea(OBJECTPTR Object, DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height) {
   struct acSelectArea area = { X, Y, Width, Height };
   return Action(AC_SelectArea, Object, &area);
}

INLINE ERROR acSetVar(OBJECTPTR Object, CSTRING FieldName, CSTRING Value) {
   struct acSetVar args = { FieldName, Value };
   return Action(AC_SetVar, Object, &args);
}

#define GetVar(a,b,c,d)  acGetVar(a,b,c,d)
#define SetVar(a,b,c)    acSetVar(a,b,c)
  
// StorageDevice class definition

#define VER_STORAGEDEVICE (1.000000)

class objStorageDevice : public BaseClass {
   public:
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
   inline ERROR read(APTR Buffer, LONG Bytes, LONG *Result) {
      ERROR error;
      struct acRead read = { (BYTE *)Buffer, Bytes };
      if (!(error = Action(AC_Read, this, &read))) {
         if (Result) *Result = read.Result;
         return ERR_Okay;
      }
      else {
         if (Result) *Result = 0;
         return error;
      }
   }
   inline ERROR rename(CSTRING Name) {
      struct acRename args = { Name };
      return Action(AC_Rename, this, &args);
   }
   inline ERROR reset() { return Action(AC_Reset, this, NULL); }
   inline ERROR seek(DOUBLE Offset, LONG Position) {
      struct acSeek args = { Offset, Position };
      return Action(AC_Seek, this, &args);
   }
   inline ERROR seekStart(DOUBLE Offset)   { return seek(Offset, SEEK_START); }
   inline ERROR seekEnd(DOUBLE Offset)     { return seek(Offset, SEEK_END); }
   inline ERROR seekCurrent(DOUBLE Offset) { return seek(Offset, SEEK_CURRENT); }
   inline ERROR write(CPTR Buffer, LONG Bytes, LONG *Result) {
      ERROR error;
      struct acWrite write = { (BYTE *)Buffer, Bytes };
      if (!(error = Action(AC_Write, this, &write))) {
         if (Result) *Result = write.Result;
      }
      else if (Result) *Result = 0;
      return error;
   }
   inline LONG writeResult(CPTR Buffer, LONG Bytes) {
      struct acWrite write = { (BYTE *)Buffer, Bytes };
      if (!Action(AC_Write, this, &write)) return write.Result;
      else return 0;
   }
};

// Config class definition

#define VER_CONFIG (1.000000)

// Config methods

#define MT_CfgReadValue -1
#define MT_CfgReadIValue -2
#define MT_CfgWriteValue -3
#define MT_CfgDeleteKey -4
#define MT_CfgDeleteGroup -5
#define MT_CfgGetGroupFromIndex -6
#define MT_CfgSortByKey -7
#define MT_CfgMergeFile -9
#define MT_CfgMerge -10
#define MT_CfgSet -11

struct cfgReadValue { CSTRING Group; CSTRING Key; CSTRING Data;  };
struct cfgReadIValue { CSTRING Group; CSTRING Key; CSTRING Data;  };
struct cfgWriteValue { CSTRING Group; CSTRING Key; CSTRING Data;  };
struct cfgDeleteKey { CSTRING Group; CSTRING Key;  };
struct cfgDeleteGroup { CSTRING Group;  };
struct cfgGetGroupFromIndex { LONG Index; CSTRING Group;  };
struct cfgSortByKey { CSTRING Key; LONG Descending;  };
struct cfgMergeFile { CSTRING Path;  };
struct cfgMerge { OBJECTPTR Source;  };
struct cfgSet { CSTRING Group; CSTRING Key; CSTRING Data;  };

INLINE ERROR cfgReadValue(APTR Ob, CSTRING Group, CSTRING Key, CSTRING * Data) {
   struct cfgReadValue args = { Group, Key, 0 };
   ERROR error = Action(MT_CfgReadValue, (OBJECTPTR)Ob, &args);
   if (Data) *Data = args.Data;
   return(error);
}

INLINE ERROR cfgReadIValue(APTR Ob, CSTRING Group, CSTRING Key, CSTRING * Data) {
   struct cfgReadIValue args = { Group, Key, 0 };
   ERROR error = Action(MT_CfgReadIValue, (OBJECTPTR)Ob, &args);
   if (Data) *Data = args.Data;
   return(error);
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

INLINE ERROR cfgSet(APTR Ob, CSTRING Group, CSTRING Key, CSTRING Data) {
   struct cfgSet args = { Group, Key, Data };
   return(Action(MT_CfgSet, (OBJECTPTR)Ob, &args));
}


class objConfig : public BaseClass {
   public:
   STRING Path;         // Set this field to the location of the source configuration file.
   STRING KeyFilter;    // Set this field to enable key filtering.
   STRING GroupFilter;  // Set this field to enable group filtering.
   LONG   Flags;        // Optional flags may be set here.
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

INLINE ERROR cfgWriteInt(OBJECTPTR Self, CSTRING Group, CSTRING Key, LONG Integer)
{
   if (!Self) return ERR_NullArgs;
   char buffer[32];
   StrFormat(buffer, sizeof(buffer), "%d", Integer);
   struct cfgWriteValue write = { Group, Key, buffer };
   return Action(MT_CfgWriteValue, Self, &write);
}

INLINE ERROR cfgReadFloat(OBJECTPTR Self, CSTRING Group, CSTRING Key, DOUBLE *Value)
{
   ERROR error;
   struct cfgReadValue read = { .Group = Group, .Key = Key };
   if (!(error = Action(MT_CfgReadValue, Self, &read))) {
      *Value = StrToFloat(read.Data);
      return ERR_Okay;
   }
   else { *Value = 0; return error; }
}

INLINE ERROR cfgReadInt(OBJECTPTR Self, CSTRING Group, CSTRING Key, LONG *Value)
{
   ERROR error;
   struct cfgReadValue read = { .Group = Group, .Key = Key };
   if (!(error = Action(MT_CfgReadValue, Self, &read))) {
      *Value = StrToInt(read.Data);
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
   OBJECTID TargetID;  // Reference to the default container that new script objects will be initialised to.
   LONG     Flags;     // Optional flags.
   ERROR    Error;     // If a script fails during execution, an error code may be readable here.
   LONG     CurrentLine; // Indicates the current line being executed when in debug mode.
   LONG     LineOffset; // For debugging purposes, this value is added to any message referencing a line number.

#ifdef PRV_SCRIPT
   LARGE    ProcedureID;          // For callbacks
   struct   KeyStore *Vars;       // Global parameters
   STRING   *Results;
   char     Language[4];          // 3-character language code, null-terminated
   const struct ScriptArg *ProcArgs;  // Procedure args - applies during Exec
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
      if ((error) AND (Buffer)) Buffer[0] = 0;
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
#define MT_TaskCloseInstance -6

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

#define taskCloseInstance(obj) Action(MT_TaskCloseInstance,(obj),0)


class objTask : public BaseClass {
   public:
   DOUBLE TimeOut;    // Limits the amount of time to wait for a launched process to return.
   LONG   Flags;      // Optional flags.
   LONG   ReturnCode; // The task's return code can be retrieved following execution.
   LONG   ProcessID;  // Reflects the process ID when an executable is launched.
   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR getVar(CSTRING FieldName, STRING Buffer, LONG Size) {
      struct acGetVar args = { FieldName, Buffer, Size };
      ERROR error = Action(AC_GetVar, this, &args);
      if ((error) AND (Buffer)) Buffer[0] = 0;
      return error;
   }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
   inline ERROR acSetVar(CSTRING FieldName, CSTRING Value) {
      struct acSetVar args = { FieldName, Value };
      return Action(AC_SetVar, this, &args);
   }
   inline ERROR write(CPTR Buffer, LONG Bytes, LONG *Result) {
      ERROR error;
      struct acWrite write = { (BYTE *)Buffer, Bytes };
      if (!(error = Action(AC_Write, this, &write))) {
         if (Result) *Result = write.Result;
      }
      else if (Result) *Result = 0;
      return error;
   }
   inline LONG writeResult(CPTR Buffer, LONG Bytes) {
      struct acWrite write = { (BYTE *)Buffer, Bytes };
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

// Private task list control structure.

#define MAX_MEMLOCKS 64  // Maximum number of non-blocking memory locks allowed per task

struct TaskList {
   LARGE    CreationTime;  // Time at which the task slot was created
   LONG     ProcessID;     // Core process ID
   OBJECTID TaskID;        // Task ID for this array entry.  Also see the ParentID field
   MEMORYID MessageID;     // Message queue ID
   OBJECTID OutputID;      // The object that the task should output information to
   LONG     InstanceID;    // Instance that the task belongs to
   LONG     ReturnCode;    // Return code
   OBJECTID ParentID;      // The task responsible for creating this task slot
   OBJECTID ModalID;       // Set if a modal surface is to have the user's attention
   LONG     EventMask;     // The events that this task is listening to
   WORD     Returned:1;    // Process has finished (the ReturnCode is set)
   WORD     Index;         // Index in the shTasks array
   #ifdef _WIN32
      WINHANDLE Lock;      // The semaphore to signal when a message is sent to the task
   #endif
   struct   {
      MEMORYID MemoryID;
      WORD     AccessCount;
   } NoBlockLocks[MAX_MEMLOCKS+1]; // Allow for a NULL entry at the end of the array
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
   DOUBLE Version;                          // Minimum required version number.
   const struct Function * FunctionList;    // Refers to a list of public functions exported by the module.
   APTR   ModBase;                          // The Module's function base (jump table) must be read from this field.
   struct ModuleMaster * Master;            // For internal use only.
   struct ModHeader * Header;               // For internal usage only.
   LONG   Flags;                            // Optional flags.
   // Action stubs

   inline ERROR getVar(CSTRING FieldName, STRING Buffer, LONG Size) {
      struct acGetVar args = { FieldName, Buffer, Size };
      ERROR error = Action(AC_GetVar, this, &args);
      if ((error) AND (Buffer)) Buffer[0] = 0;
      return error;
   }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
   inline ERROR acSetVar(CSTRING FieldName, CSTRING Value) {
      struct acSetVar args = { FieldName, Value };
      return Action(AC_SetVar, this, &args);
   }
};

// Time class definition

#define VER_TIME (1.000000)

// Time methods

#define MT_TmSetTime -1

#define tmSetTime(obj) Action(MT_TmSetTime,(obj),0)


class objTime : public BaseClass {
   public:
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
   LARGE     TotalOutput; // A live counter of total bytes that have been output by the stream.
   OBJECTPTR Input;      // An input object that will supply data for decompression.
   OBJECTPTR Output;     // A target object that will receive data compressed by the stream.
   LONG      Format;     // The format of the compressed stream.  The default is GZIP.
};


INLINE ERROR GetLarge(OBJECTPTR Object, ULONG FieldID, LARGE *Value) {
   return GetField(Object, (FIELD)FieldID|TLARGE, Value);
}

INLINE ERROR GetLong(OBJECTPTR Object, ULONG FieldID, LONG *Value) {
   return GetField(Object, (FIELD)FieldID|TLONG, Value);
}

INLINE ERROR GetDouble(OBJECTPTR Object, ULONG FieldID, DOUBLE *Value) {
   return GetField(Object, (FIELD)FieldID|TDOUBLE, Value);
}

INLINE ERROR GetString(OBJECTPTR Object, ULONG FieldID, STRING *Value) {
   return GetField(Object, (FIELD)FieldID|TSTRING, Value);
}

INLINE ERROR GetPercentage(OBJECTPTR Object, ULONG FieldID, DOUBLE *Value) {
   return GetField(Object, (FIELD)FieldID|TDOUBLE|TPERCENT, Value);
}

INLINE ERROR GetPointer(OBJECTPTR Object, ULONG FieldID, APTR Value) {
   return GetField(Object, (FIELD)FieldID|TPTR, Value);
}

INLINE ERROR GetVariable(OBJECTPTR Object, ULONG FieldID, struct Variable *Value) {
   return GetField(Object, (FIELD)FieldID|TVAR, Value);
}

//****************************************************************************

INLINE ERROR SetLarge(OBJECTPTR Object, ULONG FieldID, LARGE Value) {
   return SetField(Object, (FIELD)FieldID|TLARGE, Value);
}

INLINE ERROR SetLong(OBJECTPTR Object, ULONG FieldID, LONG Value) {
   return SetField(Object, (FIELD)FieldID|TLONG, Value);
}

INLINE ERROR SetFunction(OBJECTPTR Object, ULONG FieldID, FUNCTION *Value) {
   return SetField(Object, (FIELD)FieldID|TFUNCTION, Value);
}

INLINE ERROR SetFunctionPtr(OBJECTPTR Object, ULONG FieldID, APTR Value) { // Yes, the pointer value will be converted to a StdC FUNCTION type internally.
   return SetField(Object, (FIELD)FieldID|TPTR, Value);
}

INLINE ERROR SetDouble(OBJECTPTR Object, ULONG FieldID, DOUBLE Value) {
   return SetField(Object, (FIELD)FieldID|TDOUBLE, Value);
}

INLINE ERROR SetString(OBJECTPTR Object, ULONG FieldID, CSTRING Value) {
   return SetField(Object, (FIELD)FieldID|TSTRING, Value);
}

INLINE ERROR SetPercentage(OBJECTPTR Object, ULONG FieldID, DOUBLE Value) {
   return SetField(Object, (FIELD)FieldID|TDOUBLE|TPERCENT, Value);
}

INLINE ERROR SetPointer(OBJECTPTR Object, ULONG FieldID, const void *Value) {
   return SetField(Object, (FIELD)FieldID|TPTR, Value);
}

INLINE ERROR SetVariable(OBJECTPTR Object, ULONG FieldID, struct Variable *Value) {
   return SetField(Object, (FIELD)FieldID|TVAR, Value);
}

#ifndef PRV_CORE

// Note that the length of the data is only needed when messaging between processes, so we can skip it for these
// direct-access data channel macros.

#define acDataContent(a,b)  acDataFeed((a),0,DATA_CONTENT,(b),0)
#define acDataXML(a,b)      acDataFeed((a),0,DATA_XML,(b),0)
#define acDataText(a,b)     acDataFeed((a),0,DATA_TEXT,(b),0)

INLINE ERROR acCustomID(OBJECTID ObjectID, LONG Number, CSTRING String) {
   struct acCustom args = { Number, String };
   return ActionMsg(AC_Custom, ObjectID, &args);
}

INLINE ERROR acDataFeedID(OBJECTID ObjectID, OBJECTID SenderID, LONG Datatype, const APTR Data, LONG Size) {
   struct acDataFeed channel = { { SenderID }, { Datatype }, Data, Size };
   return ActionMsg(AC_DataFeed, ObjectID, &channel);
}

INLINE ERROR acDragDropID(OBJECTID ObjectID, OBJECTID Source, LONG Item, CSTRING Datatype) {
   struct acDragDrop args = { { Source }, Item, Datatype };
   return ActionMsg(AC_DragDrop, ObjectID, &args);
}

INLINE ERROR acDrawAreaID(OBJECTID ObjectID, LONG X, LONG Y, LONG Width, LONG Height) {
   struct acDraw draw = { X, Y, Width, Height };
   return ActionMsg(AC_Draw, ObjectID, &draw);
}

INLINE ERROR acMoveID(OBJECTID ObjectID, DOUBLE X, DOUBLE Y, DOUBLE Z) {
   struct acMove move = { X, Y, Z };
   return ActionMsg(AC_Move, ObjectID, &move);
}

INLINE ERROR acMoveToPointID(OBJECTID ObjectID, DOUBLE X, DOUBLE Y, DOUBLE Z, LONG Flags) {
   struct acMoveToPoint moveto = { X, Y, Z, Flags };
   return ActionMsg(AC_MoveToPoint, ObjectID, &moveto);
}

INLINE ERROR acRedimensionID(OBJECTID ObjectID, DOUBLE X, DOUBLE Y, DOUBLE Z, DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
   struct acRedimension resize = { X, Y, Z, Width, Height, Depth };
   return ActionMsg(AC_Redimension, ObjectID, &resize);
}

INLINE ERROR acResizeID(OBJECTID ObjectID, DOUBLE Width, DOUBLE Height, DOUBLE Depth) {
   struct acResize resize = { Width, Height, Depth };
   return ActionMsg(AC_Resize, ObjectID, &resize);
}

INLINE ERROR acScrollToPointID(OBJECTID ObjectID, DOUBLE X, DOUBLE Y, DOUBLE Z, LONG Flags) {
   struct acScrollToPoint scroll = { X, Y, Z, Flags };
   return ActionMsg(AC_ScrollToPoint, ObjectID, &scroll);
}

INLINE ERROR acScrollID(OBJECTID ObjectID, DOUBLE X, DOUBLE Y, DOUBLE Z) {
   struct acScroll scroll = { X, Y, Z };
   return ActionMsg(AC_Scroll, ObjectID, &scroll);
}

INLINE ERROR acSelectAreaID(OBJECTID ObjectID, DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height) {
   struct acSelectArea area = { X, Y, Width, Height };
   return ActionMsg(AC_SelectArea, ObjectID, &area);
}

// The number of bytes written is not returned (you would need to use DelayMsg() for that).

INLINE ERROR acWriteID(OBJECTID ObjectID, CPTR Buffer, LONG Bytes) {
   struct acWrite write = { (BYTE *)Buffer, Bytes };
   return ActionMsg(AC_Write, ObjectID, &write);
}

INLINE ERROR LoadModule(CSTRING Name, FLOAT Version, OBJECTPTR *Module, APTR Functions) {
   OBJECTPTR module;
   if (!CreateObject(ID_MODULE, 0, &module,
         FID_Name|TSTR,   Name,
         FID_Version|TFLOAT, Version,
         TAGEND)) {
      APTR functionbase;
      if (!GetField(module, FID_ModBase|TPTR, &functionbase)) {
         if (Module) *Module = module;
         if (Functions) ((APTR *)Functions)[0] = functionbase;
         return ERR_Okay;
      }
      else return ERR_GetField;
   }
   else return ERR_CreateObject;
}

INLINE FIELD ResolveField(CSTRING Field) {
   return StrHash(Field, FALSE);
}

#endif // PRV_CORE

#ifdef __unix__
#include <pthread.h>
#endif

#ifdef __system__

// Public memory management structures.

struct PublicAddress {
   LARGE    AccessTime;        // The time at which the block was accessed
   MEMORYID MemoryID;          // Unique memory ID
   LONG     Size;              // Size of the memory block
   LONG     Offset;            // Offset of the memory block within the page file
   OBJECTID ObjectID;          // Object that the address belongs to
   OBJECTID TaskID;            // The task that the block is tracked back to
   OBJECTID ContextID;         // Context that locked the memory block (for debugging purposes only)
   LONG     InstanceID;        // Reference to the instance that this memory block is restricted to
   WORD     ActionID;          // Action that locked the memory block (for debugging purposes only)
   WORD     Flags;             // Special MEM_ address flags
   volatile UBYTE AccessCount;  // Count of locks on this address
   volatile UBYTE ExternalLock; // Incremented when a third party requires access during a lock
   #ifdef __unix__
      volatile LONG ThreadLockID;      // Globally unique ID from get_thread_id()
      volatile LONG ProcessLockID;     // If locked, this field refers to the ID of the semaphore that locked the block
   #endif
   #ifdef _WIN32
      LONG      OwnerProcess;   // The process ID of the task that created the block as referred to by Offset
      WINHANDLE Handle;         // Memory handle, if block does not belong to the core memory pool
      volatile OBJECTID ProcessLockID;  // If locked, refers to the ID of the process that locked it
      volatile LONG     ThreadLockID;   // If locked, the global thread ID of the locker.
   #endif
};

struct SortedAddress {
   MEMORYID MemoryID;
   LONG Index;
};

// Semaphore management structure.

#define MAX_SEMAPHORES  40  // Maximum number of semaphores that can be allocated in the system

struct SemaphoreEntry {   // The index of each semaphore in the array indicates their IDs
   ULONG NameID;          // Hashed name of the semaphore
   LONG  InstanceID;      // Reference to the instance that this semaphore is restricted to
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

// Message structure and internal ID's for standard Task-to-Task messages.

struct ActionMessage {
   WORD ActionID;            // ID of the action or method to execute
   UWORD SendArgs:1;         //
   UWORD ReturnResult:1;     // Set to TRUE if a result is required
   UWORD Delayed:1;          // TRUE if the message was intentionally delayed
   MEMORYID ReturnMessage;   // If ReturnResult is TRUE, the message queue for the result message must be set here
   OBJECTID ObjectID;        // The object that is to receive the action
   ERROR Error;              // If a result is required, the action's error code is returned here by MSGID_ACTIONRESULT
   LONG  Time;
   // Action arguments follow this structure in a buffer
};

#endif

enum { // For SysLock()
   PL_WAITLOCKS=1,
   PL_PUBLICMEM,
   PL_FORBID,
   PL_PROCESSES,
   PL_SEMAPHORES,
   #ifdef _WIN32
      CN_PUBLICMEM,
      CN_SEMAPHORES,
   #endif
   PL_END
};

struct SharedControl {
   LONG PoolSize;                   // Amount of allocated page space (starts at zero and expands)
   volatile LONG BlocksUsed;        // Total amount of shared memory blocks currently allocated
   LONG MaxBlocks;                  // Maximum amount of available blocks
   volatile LONG NextBlock;         // Next empty position in the blocks table
   volatile LONG IDCounter;         // ID counter
   volatile LONG PrivateIDCounter;  // ID counter for private memory blocks
   volatile LONG MessageIDCount;    // Counter of message ID's
   volatile LONG ClassIDCount;      // Counter of class ID's
   volatile LONG GlobalIDCount;     // Counter for general ID's
   volatile LONG ThreadIDCount;
   volatile LONG InputTotal;        // Total number of subscribers in InputMID
   volatile LONG ValidateProcess;
   volatile LONG InputIDCounter;    // Counter for input event subscriptions
   WORD SystemState;
   volatile WORD WLIndex;           // Current insertion point for the wait-lock array.
   LONG MagicKey;                   // This magic key is set to the semaphore key (used only as an indicator for initialisation)
   LONG BlocksOffset;               // Array of available shared memory pages
   LONG SortedBlocksOffset;         // Array of shared memory blocks sorted by MemoryID
   LONG SemaphoreOffset;            // Offset to the semaphore control array
   LONG TaskOffset;                 // Offset to the task control array
   LONG MemoryOffset;               // Offset to the shared memory allocations
   LONG WLOffset;                   // Offset to the wait-lock array
   LONG GlobalInstance;             // If glSharedControl belongs to a global instance, this is the PID of the creator.
   LONG SurfaceSemaphore;
   LONG InputSize;                  // Maximum number of subscribers allowed in InputMID
   LONG InstanceMsgPort;            // The message port of the process that created the instance.
   MEMORYID ObjectsMID;
   MEMORYID TranslationMID;
   MEMORYID SurfacesMID;
   MEMORYID ClassesMID;             // Class database
   MEMORYID ModulesMID;             // Module database
   MEMORYID InputMID;
   LONG ClassSemaphore;             // Semaphore for controlling access to the class database
   #ifdef __unix__
      struct {
         pthread_mutex_t Mutex;
         pthread_cond_t Cond;
         LONG PID;               // Resource tracking: Process that has the current lock.
         WORD Count;             // Resource tracking: Count of all locks (nesting)
      } PublicLocks[PL_END];
   #elif _WIN32
      // In windows, the shared memory controls are controlled by mutexes that have local handles.
   #endif
};

// Class database.

#define CL_ITEMS(c)        (ClassItem *)( (BYTE *)(c) + sizeof(ClassHeader) + ((c)->Total<<2) )
#define CL_OFFSETS(c)      ((LONG *)((c) + 1))
#define CL_SIZE_OFFSETS(c) (sizeof(LONG) * (c)->Total)
#define CL_ITEM(c,i)       ((ClassItem *)((BYTE *)(c) + offsets[(i)]))

struct ClassHeader {
   LONG Total;          // Total number of registered classes
   LONG Size;           // Size of the entire memory block
   // Followed by lookup table with offsets to each ClassItem, sorted by hash
};

struct ClassItem {
   CLASSID ClassID;
   CLASSID ParentID;    // Parent class reference.
   LONG  Category;
   WORD  Size;          // Size of the item structure, all accompanying strings and byte alignment
   WORD  PathOffset;
   WORD  MatchOffset;
   WORD  HeaderOffset;
   char Name[24];
   // Followed by [path, match, header] strings
};

// X11 Variables

struct X11Globals {
   UBYTE DGAInitialised;
   UBYTE InitCount;
   UBYTE DGACount;
   UBYTE RRInitialised;
   UBYTE Manager;
   UBYTE FailMsg;
   LONG PixelsPerLine;
   LONG BankSize;
};

// Event support

typedef struct rkEvent {
   EVENTID EventID;
   // Data follows
} rkEvent;

#define EVID_DISPLAY_RESOLUTION_CHANGE  GetEventID(EVG_DISPLAY, "resolution", "change")

#define EVID_GUI_SURFACE_FOCUS          GetEventID(EVG_GUI, "surface", "focus")

#define EVID_FILESYSTEM_VOLUME_CREATED  GetEventID(EVG_FILESYSTEM, "volume", "created")
#define EVID_FILESYSTEM_VOLUME_DELETED  GetEventID(EVG_FILESYSTEM, "volume", "deleted")

#define EVID_USER_STATUS_LOGIN          GetEventID(EVG_USER, "status", "login")
#define EVID_USER_STATUS_LOGOUT         GetEventID(EVG_USER, "status", "logout")
#define EVID_USER_STATUS_LOGGEDOUT      GetEventID(EVG_USER, "status", "loggedout")

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

#define EVID_HARDWARE_DRIVERS_STARTING  GetEventID(EVG_HARDWARE, "drivers", "starting")
#define EVID_HARDWARE_DRIVERS_STARTED   GetEventID(EVG_HARDWARE, "drivers", "started")
#define EVID_HARDWARE_DRIVERS_CLOSING   GetEventID(EVG_HARDWARE, "drivers", "closing")

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

// Hotplug event structure.  The hotlpug event is sent whenever a new hardware device is inserted by the user.

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

INLINE void SET_DEVICE(struct dcDeviceInput *Input, WORD Type, WORD Flags, DOUBLE Value, LARGE Timestamp)
{
   Input->Type  = Type;
   Input->Flags = Flags;
   Input->Value = Value;
   Input->Timestamp = Timestamp;
}

//****************************************************************************
// File Methods.

INLINE CSTRING flReadLine(OBJECTPTR Object) {
   struct flReadLine args;
   if (!Action(MT_FlReadLine, Object, &args)) return args.Result;
   else return NULL;
}

//****************************************************************************
// Little endian read functions.

INLINE ERROR flReadLE2(OBJECTPTR Object, WORD *Result)
{
   struct acRead read;
   UBYTE data[2];

   read.Buffer = data;
   read.Length = 2;
   if (!Action(AC_Read, Object, &read)) {
      if (read.Result IS 2) {
         #ifdef LITTLE_ENDIAN
            *Result = ((WORD *)data)[0];
         #else
            *Result = (data[1]<<8) | data[0];
         #endif
         return ERR_Okay;
      }
      else return ERR_Read;
   }
   else return ERR_Read;
}

INLINE ERROR flReadLE4(OBJECTPTR Object, LONG *Result)
{
   UBYTE data[4];

   struct acRead read = { data, sizeof(data) };
   if (!Action(AC_Read, Object, &read)) {
      if (read.Result IS sizeof(data)) {
         #ifdef LITTLE_ENDIAN
            *Result = ((LONG *)data)[0];
         #else
            *Result = (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|(data[3])
         #endif
         return ERR_Okay;
      }
      else return ERR_Read;
   }
   else return ERR_Read;
}

INLINE ERROR flReadLE8(OBJECTPTR Object, LARGE *Result)
{
   UBYTE data[8];
   struct acRead read = { data, sizeof(data) };
   if (!Action(AC_Read, Object, &read)) {
      if (read.Result IS sizeof(data)) {
         #ifdef LITTLE_ENDIAN
            *Result = ((LARGE *)data)[0];
         #else
            *Result = ((LARGE)data[0]<<56)|((LARGE)data[1]<<48)|((LARGE)data[2]<<40)|((LARGE)data[3]<<32)|(data[4]<<24)|(data[5]<<16)|(data[6]<<8)|(data[7])
         #endif
         return ERR_Okay;
      }
      else return ERR_Read;
   }
   else return ERR_Read;
}

#ifdef __cplusplus
template <class R>
constexpr FUNCTION make_function_stdc(R Routine, OBJECTPTR Context = CurrentContext()) {
   FUNCTION func = { .Type = CALL_STDC, .StdC = { .Context = Context, .Routine = (APTR)Routine } };
   return func;
}

INLINE FUNCTION make_function_script(OBJECTPTR Script, LARGE Procedure) {
   FUNCTION func = { .Type = CALL_SCRIPT, .Script = { .Script = (OBJECTPTR)Script, .ProcedureID = Procedure } };
   return func;
}
#endif

inline CSTRING BaseClass::className() { return Class->ClassName; }

  
