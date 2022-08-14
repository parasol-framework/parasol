#ifndef MODULES_WIDGET
#define MODULES_WIDGET 1

// Name:      widget.h
// Copyright: Paul Manias 2003-2020
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_WIDGET (1)

#ifndef MODULES_FONT_H
#include <parasol/modules/font.h>
#endif

#ifndef MODULES_PICTURE_H
#include <parasol/modules/picture.h>
#endif

#ifndef MODULES_VECTOR_H
#include <parasol/modules/vector.h>
#endif

#ifndef MODULES_XML_H
#include <parasol/modules/xml.h>
#endif

#ifndef MODULES_DISPLAY_H
#include <parasol/modules/display.h>
#endif

#ifndef MODULES_DOCUMENT_H
#include <parasol/modules/document.h>
#endif

// Axis options.  This determines the axis that is signalled when the slider is moved (note that it is feasible for a horizontal scrollbar to signal the Y axis, if this is desired by the client)

#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2

// Scroll directions.

#define SD_NEGATIVE 1
#define SD_UP 1
#define SD_LEFT 1
#define SD_POSITIVE 2
#define SD_DOWN 2
#define SD_RIGHT 2

// Direction options

#define SO_HORIZONTAL 1
#define SO_VERTICAL 2

// Resize class definition

#define VER_RESIZE (1.000000)

typedef struct rkResize {
   OBJECT_HEADER
   struct rkLayout * Layout;    // Layout manager
   OBJECTID ObjectID;           // Object that is to be resized
   LONG     Button;             // Determines what button is used for resizing
   LONG     Direction;          // Direction flags (horizontal/vertical)
   LONG     Border;             // Border flags can be used to monitor up to 8 separate areas at once
   LONG     BorderSize;         // Determines the size of the border edge

#ifdef PRV_RESIZE
   LONG  OriginalWidth, OriginalHeight;
   LONG  OriginalX, OriginalY;
   LONG  OriginalAbsX, OriginalAbsY;
   LONG  prvAnchorX, prvAnchorY;
   LONG  InputHandle;
   WORD  CursorSet;
   WORD  State;
   BYTE  prvAnchored;
  
#endif
} objResize;

#define CT_DATA 0
#define CT_AUDIO 1
#define CT_IMAGE 2
#define CT_FILE 3
#define CT_OBJECT 4
#define CT_TEXT 5
#define CT_END 6

// Clipboard types

#define CLIPTYPE_DATA 0x00000001
#define CLIPTYPE_AUDIO 0x00000002
#define CLIPTYPE_IMAGE 0x00000004
#define CLIPTYPE_FILE 0x00000008
#define CLIPTYPE_OBJECT 0x00000010
#define CLIPTYPE_TEXT 0x00000020

// Clipboard flags

#define CLF_DRAG_DROP 0x00000001
#define CLF_HOST 0x00000002

#define CEF_DELETE 0x00000001
#define CEF_EXTEND 0x00000002

// Clipboard class definition

#define VER_CLIPBOARD (1.000000)

typedef struct rkClipboard {
   OBJECT_HEADER
   LONG     Flags;      // Optional flags
   MEMORYID ClusterID;  // Identifies the data cluster (item grouping) that the clipboard will work with

#ifdef PRV_CLIPBOARD
   FUNCTION RequestHandler;
   BYTE     ClusterAllocated:1;
  
#endif
} objClipboard;

// Clipboard methods

#define MT_ClipAddFile -1
#define MT_ClipAddObject -2
#define MT_ClipAddObjects -3
#define MT_ClipGetFiles -4
#define MT_ClipAddText -5
#define MT_ClipRemove -6

struct clipAddFile { LONG Datatype; CSTRING Path; LONG Flags;  };
struct clipAddObject { LONG Datatype; OBJECTID ObjectID; LONG Flags;  };
struct clipAddObjects { LONG Datatype; OBJECTID * Objects; LONG Flags;  };
struct clipGetFiles { LONG Datatype; LONG Index; CSTRING * Files; LONG Flags;  };
struct clipAddText { CSTRING String;  };
struct clipRemove { LONG Datatype;  };

INLINE ERROR clipAddFile(APTR Ob, LONG Datatype, CSTRING Path, LONG Flags) {
   struct clipAddFile args = { Datatype, Path, Flags };
   return(Action(MT_ClipAddFile, (OBJECTPTR)Ob, &args));
}

INLINE ERROR clipAddObject(APTR Ob, LONG Datatype, OBJECTID ObjectID, LONG Flags) {
   struct clipAddObject args = { Datatype, ObjectID, Flags };
   return(Action(MT_ClipAddObject, (OBJECTPTR)Ob, &args));
}

INLINE ERROR clipAddObjects(APTR Ob, LONG Datatype, OBJECTID * Objects, LONG Flags) {
   struct clipAddObjects args = { Datatype, Objects, Flags };
   return(Action(MT_ClipAddObjects, (OBJECTPTR)Ob, &args));
}

INLINE ERROR clipGetFiles(APTR Ob, LONG * Datatype, LONG Index, CSTRING ** Files, LONG * Flags) {
   struct clipGetFiles args = { 0, Index, 0, 0 };
   ERROR error = Action(MT_ClipGetFiles, (OBJECTPTR)Ob, &args);
   if (Datatype) *Datatype = args.Datatype;
   if (Files) *Files = args.Files;
   if (Flags) *Flags = args.Flags;
   return(error);
}

INLINE ERROR clipAddText(APTR Ob, CSTRING String) {
   struct clipAddText args = { String };
   return(Action(MT_ClipAddText, (OBJECTPTR)Ob, &args));
}

INLINE ERROR clipRemove(APTR Ob, LONG Datatype) {
   struct clipRemove args = { Datatype };
   return(Action(MT_ClipRemove, (OBJECTPTR)Ob, &args));
}


#endif
