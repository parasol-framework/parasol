/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
TabFocus: Manages the 'user focus' via the tab key.

The TabFocus class manages the use of the tab key and its relationship to the user-focus in the GUI of each
application.  To create a tab-list, you only need to pass a list of focus-able objects that are in the application
window.  As the user presses the tab-key, the focus will switch to each surface in the list, following the order that
you have specified.

Objects can be added to the tabfocus using the #AddObject() method for individual objects, or you can set the
Objects field for a mass addition.  Some GUI classes such as the @Button support a TabFocus field that you can
set and this will cause it to automatically add itself to the referenced tabfocus object.
-END-

*****************************************************************************/

#define PRV_TABFOCUS
#include <parasol/main.h>
#include <parasol/modules/widget.h>
#include <parasol/modules/surface.h>
#include "defs.h"

static OBJECTPTR clTabFocus = NULL;

static void focus_object(objTabFocus *, UBYTE);
static OBJECTID object_surface(OBJECTID);
static LONG have_focus(objTabFocus *);
static void key_event(objTabFocus *, evKey *, LONG);

/*****************************************************************************
-ACTION-
Activate: Moves the focus to the next object in the focus list.
-END-
*****************************************************************************/

static ERROR TABFOCUS_Activate(objTabFocus *Self, APTR Args)
{
   if (Self->Total < 1) return ERR_Okay;
   if (!(Self->CurrentFocus = drwGetUserFocus())) return ERR_Okay;

   parasol::Log log;
   if (Self->CurrentFocus IS Self->SurfaceID) {
      // Focus on the first object in the tab list

      log.branch("Current: #%d == Monitored Surface", Self->CurrentFocus);

      for (LONG i=0; i < Self->Total; i++) {
         if (Self->TabList[i].ObjectID) {
            focus_object(Self, i);
            break;
         }
      }
   }
   else {
      log.branch("Current: #%d", Self->CurrentFocus);

      LONG focusindex = -1;
      for (LONG i=0; i < Self->Total; i++) {
         if (Self->TabList[i].SurfaceID IS Self->CurrentFocus) {
            if (Self->Reverse) {
               // Go to the previous object in the tag list
               i--;
               if (i < 0) i = Self->Total - 1;
               while (!Self->TabList[i].ObjectID) {
                  i--;
                  if (i < 0) i = Self->Total - 1;
               }
            }
            else  {
               // Go to the next object in the tag list
               i++;
               while (!Self->TabList[i].ObjectID) {
                  i++;
                  if (i >= Self->Total) i = 0;
               }
            }
            focusindex = i;
            break;
         }
      }

      // If the current user-selected object is not in our tab-list, reset the focus to the first object in the list.

      if (focusindex IS -1) {
         focusindex = Self->Index + 1;
         if ((focusindex < 0) OR (focusindex >= Self->Total)) focusindex = 0;
      }

      // This loop ensures that the object receiving the focus is enabled.  If there are disabled objects, we skip past
      // them to find the first active surface.

      LONG i = Self->Total;
      while (i > 0) {
         if (Self->TabList[focusindex].SurfaceID) {
            SURFACEINFO *info;
            if (!drwGetSurfaceInfo(Self->TabList[focusindex].SurfaceID, &info)) {
               if (!(info->Flags & RNF_DISABLED)) {
                  focus_object(Self, focusindex);
                  break;
               }
            }
         }
         else {
            focus_object(Self, focusindex);
            break;
         }

         if (++focusindex >= Self->Total) focusindex = 0;
         i--;
      }

      Self->Index = focusindex;
   }

   return ERR_Okay;
}

/*****************************************************************************
-METHOD-
AddObject: Adds a new object to the tab list.

New objects can be added to the tab list by calling this method.  The object can be of any class type, but it must
support the Focus action or it will not be able to respond when the tab list attempts to use it.

Once the object is added, the user will be able to focus on it by using the tab-key.

-INPUT-
oid Object: Reference to the object to be inserted.

-ERRORS-
Okay
NullArgs
Failed

*****************************************************************************/

static ERROR TABFOCUS_AddObject(objTabFocus *Self, struct tabAddObject *Args)
{
   if ((!Args) OR (!Args->ObjectID)) return PostError(ERR_NullArgs);

   OBJECTID objectid = Args->ObjectID;
   OBJECTID regionid = object_surface(objectid);

   // Do not allow references to our monitored surface

   if (objectid IS Self->SurfaceID) {
      LogMsg("Cannot add object #%d because it is the surface I monitor for keystrokes.", Args->ObjectID);
      return ERR_Failed;
   }

   Self->TabList[Self->Total].ObjectID = objectid;
   Self->TabList[Self->Total].SurfaceID = regionid;

   Self->Total++;
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Clear: Flushes the tab list.
-END-
*****************************************************************************/

static ERROR TABFOCUS_Clear(objTabFocus *Self, APTR Args)
{
   Self->Total = 0;
   return ERR_Okay;
}

//****************************************************************************

static ERROR TABFOCUS_Free(objTabFocus *Self, APTR Void)
{
   if (Self->prvKeyEvent) { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }
   return ERR_Okay;
}

//****************************************************************************

static ERROR TABFOCUS_Init(objTabFocus *Self, APTR Void)
{
   if (!Self->SurfaceID) { // Find our parent surface
      OBJECTID owner_id = GetOwner(Self);
      while ((owner_id) AND (GetClassID(owner_id) != ID_SURFACE)) {
         owner_id = GetOwnerID(owner_id);
      }
      if (owner_id) Self->SurfaceID = owner_id;
      else return PostError(ERR_UnsupportedOwner);
   }

   FUNCTION callback;
   SET_FUNCTION_STDC(callback, (APTR)&key_event);
   SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, Self, &Self->prvKeyEvent);

   return ERR_Okay;
}

/*****************************************************************************
-METHOD-
InsertObject: Inserts a new object in the tab list.

New objects can be inserted into the tab list by calling this method.  You need to provide the unique ID for a surface
object, or the routine may fail. Some intelligence is used when non-surface objects are passed to this method, whereby
the routine will check for Region and Surface fields to discover valid surface objects.

Once the object is inserted, the user will be able to focus on it by using the tab-key.

-INPUT-
int Index: The index at which the object should be inserted.
oid Object: The ID of the object that you want to insert.

-ERRORS-
Okay:
Args:
OutOfRange: The index is out of range.

*****************************************************************************/

static ERROR TABFOCUS_InsertObject(objTabFocus *Self, struct tabInsertObject *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   WORD index = Args->Index;
   if (index < 0) index = 0;
   if (index >= ARRAYSIZE(Self->TabList)) return ERR_OutOfRange;

   if (!Args->ObjectID) return PostError(ERR_NullArgs);

   OBJECTID objectid = Args->ObjectID;
   OBJECTID regionid = object_surface(objectid);

   // Do not allow references to our monitored surface

   if (regionid IS Self->SurfaceID) return ERR_Failed;

   if (index < Self->Total) {
      CopyMemory(Self->TabList + Self->Total, Self->TabList + Self->Total + 1, sizeof(Self->TabList[0]) * (Self->Total - index));
   }

   Self->TabList[index].ObjectID = objectid;
   Self->TabList[index].SurfaceID = regionid;

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
RemoveObject: Removes an object from the tab list.

Use the RemoveObject() method in instances where you need to remove an existing object from the tab list.  You only
need to provide this method with the ID of the object that you want to remove.

-INPUT-
oid Object: A reference to the object that you want to remove.

-ERRORS-
Okay: The object was removed, or did not already exist in the tab list.
Args:

*****************************************************************************/

static ERROR TABFOCUS_RemoveObject(objTabFocus *Self, struct tabRemoveObject *Args)
{
   if ((!Args) OR (!Args->ObjectID)) return PostError(ERR_NullArgs);

   WORD i;
   for (i=Self->Total-1; i > 0; i--) {
      if (Self->TabList[i].ObjectID IS Args->ObjectID) {
         Self->TabList[i].ObjectID = 0;
         Self->TabList[i].SurfaceID = 0;
         CopyMemory(Self->TabList + i + 1, Self->TabList + i, (Self->Total - i - 1) * sizeof(Self->TabList[0]));
         Self->Total--;
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SetObject: Changes the object for a specific index.

The tab list may be manipulated on a more direct basis by using the SetObject() method.  It allows you to change the
object ID for a specific index point.

-INPUT-
int Index: The index in the tab list that you want to set.
oid Object: The ID of the object that you want to set at the specified Index.

-ERRORS-
Okay
NullArgs
OutOfRange: The specified Index was out of range.

*****************************************************************************/

static ERROR TABFOCUS_SetObject(objTabFocus *Self, struct tabSetObject *Args)
{
   if ((!Args) OR (!Args->ObjectID)) return PostError(ERR_NullArgs);

   WORD index = Args->Index;
   if (index < 0) index = 0;
   if (index >= ARRAYSIZE(Self->TabList)) return ERR_OutOfRange;

   Self->TabList[index].ObjectID = Args->ObjectID;
   Self->TabList[index].SurfaceID = object_surface(Args->ObjectID);

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Flags: Optional flags.

-FIELD-
Object: New objects may be set at specific indexes via this field.

The Object field provides a field-based way of setting objects at specific indexes.  An object can be set by specifying
the index number, followed by the ID of the object that you want to set.  The following is a valid example, "3:-9495".
The index and object ID can be separated with any type of white-space or non-numeric character(s).

*****************************************************************************/

static ERROR SET_Object(objTabFocus *Self, CSTRING Value)
{
   while ((*Value < '0') OR (*Value > '9')) Value++; // Find the index value
   LONG index = StrToInt(Value); // Translate it
   while ((*Value >= '0') AND (*Value <= '9')) Value++; // Skip the index value

   OBJECTID objectid = StrToInt(Value); // Get the object ID

   if (objectid) {
      tabSetObject(Self, index, objectid);
      return ERR_Okay;
   }
   else return ERR_Failed;
}

/*****************************************************************************

-FIELD-
Objects: A string sequence of objects to be added to the tab list may be set here.

A string sequence of objects may be added to the tab list via this field. Objects must be specified as ID's and be
separated with white-space or non-numeric characters.  The following example illustrates a valid string
`7984, #9493, -4001`.

*****************************************************************************/

static ERROR SET_Objects(objTabFocus *Self, CSTRING Value)
{
   while (*Value) {
      while ((*Value) AND (*Value != '-') AND ((*Value < '0') OR (*Value > '9'))) Value++;
      OBJECTID objectid = StrToInt(Value);

      if (*Value IS '-') Value++;
      while ((*Value >= '0') AND (*Value <= '9')) Value++;

      if (objectid) {
         struct tabAddObject add;
         add.ObjectID = objectid;
         TABFOCUS_AddObject(Self, &add);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Surface: Indicates the surface that will be monitored for the tab-key when it has the focus.

On initialisation the tabfocus object will require a reference to a surface that should be monitored for tab-key
presses when it has the focus.  This will usually be set to the surface of the window that is hosting the tabfocus
object and the GUI elements in the tab list.

If this field is not set on initialisation, the tabfocus object will scan its parent hierarchy for the nearest
available surface and reference it in this field.

-FIELD-
Total: Indicates the total number of objects in the focus list.
-END-

*****************************************************************************/

static void focus_object(objTabFocus *Self, UBYTE Index)
{
   LogF("~focus_object()","Index: %d, Object: %d", Index, Self->TabList[Index].ObjectID);

   CLASSID class_id = GetClassID(Self->TabList[Index].ObjectID);

   if (class_id IS ID_INPUT) {
      OBJECTPTR input, text;
      if (!AccessObject(Self->TabList[Index].ObjectID, 1000, &input)) {
         acFocus(input);

         // If the object has a textinput field, select the text

         if ((!GetPointer(input, FID_UserInput, &text)) AND (text)) {
            txtSelectArea(text, 0,0, 200000, 200000);
         }

         ReleaseObject(input);
      }
   }
   else if (acFocusID(Self->TabList[Index].ObjectID) != ERR_Okay) {
      acFocusID(Self->TabList[Index].SurfaceID);
   }

/*
   else if (class_id IS ID_COMBOBOX) ActionMsg(AC_Focus, Self->TabList[Index].ObjectID, NULL);
   else if (Self->TabList[Index].SurfaceID) ActionMsg(AC_Focus, Self->TabList[Index].SurfaceID, NULL);
   else ActionMsg(AC_Focus, Self->TabList[Index].ObjectID, NULL);
*/

   LogReturn();
}

//****************************************************************************

static OBJECTID object_surface(OBJECTID ObjectID)
{
   if (GetClassID(ObjectID) != ID_SURFACE) {
      // If the referenced object is not a surface, check for a region field and try to use that instead.

      OBJECTPTR object;
      if (!AccessObject(ObjectID, 3000, &object)) {
         OBJECTID regionid = 0;
         if (FindField(object, FID_Region, 0)) {
            if (!GetLong(object, FID_Region, &regionid)) {
               if (GetClassID(regionid) != ID_SURFACE) regionid = 0;
            }
         }

         if (!regionid) {
            if (FindField(object, FID_Surface, 0)) {
               if (!GetLong(object, FID_Surface, &regionid)) {
                  if (GetClassID(regionid) != ID_SURFACE) regionid = 0;
               }
            }
         }

         ReleaseObject(object);
         return regionid;
      }
      else return 0;
   }
   else return ObjectID;
}

//****************************************************************************
// Check if the focus is valid, according to the user's primary focus (just because our monitored surface has the
// focus, doesn't necessarily mean that we want to be intercepting the tab key).

static LONG have_focus(objTabFocus *Self)
{
   Self->CurrentFocus = drwGetUserFocus();

   if (Self->Flags & TF_LIMIT_TO_LIST) {
      // In limit-to-list mode, the tab-focus only works if the primary focus is on the monitored surface, or if one of
      // our monitored objects has the focus.

      if (Self->CurrentFocus IS Self->SurfaceID) return TRUE;
      else {
         WORD i;
         for (i=0; i < Self->Total; i++) {
            SURFACEINFO *info;
            if (!drwGetSurfaceInfo(Self->TabList[i].SurfaceID, &info)) {
               if (info->Flags & RNF_HAS_FOCUS) return TRUE;
            }
         }
      }
   }
   else if (Self->Flags & (TF_LOCAL_FOCUS|TF_CHILD_FOCUS)) {
      // In LOCAL_FOCUS mode, in order for the tab-focus to activate, the monitored surface must match the user's
      // primary focus.
      //
      // In CHILD_FOCUS mode, the surface can either be the monitored surface, or it can be a child of our monitored
      // surface (i.e. only 1 layer deep).

      if (Self->CurrentFocus IS Self->SurfaceID) return TRUE;
      else if (Self->Flags & TF_CHILD_FOCUS) {
         SURFACEINFO *info;
         if (!drwGetSurfaceInfo(Self->CurrentFocus, &info)) {
            if (info->ParentID IS Self->SurfaceID) return TRUE;
         }
      }
   }
   else { // In normal mode, the current focus can lie anywhere in the vicinity of the focus-path.
      return TRUE;
   }

   return FALSE;
}

//****************************************************************************

static void key_event(objTabFocus *Self, evKey *Event, LONG Size)
{
   if (!(Event->Qualifiers & KQ_PRESSED)) return;

   if (Event->Code IS K_TAB) {
      // Check if our focus is valid (according to the user's primary focus)

      if (have_focus(Self) IS FALSE) return;

      // Focus on the next tablist object

      if (Event->Qualifiers & KQ_SHIFT) {
         Self->Reverse = TRUE;
         acActivate(Self);
         Self->Reverse = FALSE;
      }
      else acActivate(Self);
   }
}

//****************************************************************************

#include "class_tabfocus_def.c"

static const struct FieldArray clFields[] = {
   { "Surface", FDF_OBJECTID|FDF_RW,   ID_SURFACE, NULL, NULL },
   { "Total",   FDF_LONG|FDF_R,       0, NULL, NULL },
   { "Flags",   FDF_LONGFLAGS|FDF_RW, (MAXINT)&clTabFocusFlags, NULL, NULL },
   // Virtual fields
   { "Objects", FDF_STRING|FDF_W,     0, NULL, (APTR)SET_Objects },
   { "Object",  FDF_STRING|FDF_W,     0, NULL, (APTR)SET_Object },
   END_FIELD
};


//****************************************************************************

ERROR init_tabfocus(void)
{
   return(CreateObject(ID_METACLASS, 0, &clTabFocus,
      FID_ClassVersion|TFLOAT, VER_TABFOCUS,
      FID_Name|TSTRING,   "TabFocus",
      FID_Category|TLONG, CCF_GUI,
      FID_Actions|TPTR,   clTabFocusActions,
      FID_Methods|TARRAY, clTabFocusMethods,
      FID_Fields|TARRAY,  clFields,
      FID_Size|TLONG,     sizeof(objTabFocus),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND));
}

void free_tabfocus(void)
{
   if (clTabFocus) { acFree(clTabFocus); clTabFocus = NULL; }
}
