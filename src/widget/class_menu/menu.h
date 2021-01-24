
#define PRV_MENU_FIELDS \
   LARGE TimeHide; \
   LARGE TimeShow; \
   char  LanguageDir[32];      /* Directory to use for language files */ \
   char  prvNode[32]; \
   char  IconFilter[28]; \
   char  Language[4];                 /* 3-character language code, null-terminated */ \
   struct rkMenuItem *Selection;      /* Current/most recently selected item */ \
   struct rkMenuItem *prvLastItem;    /* The last item in the item list */ \
   struct rkMenuItem *HighlightItem;  /* Index of the item that is currently highlighted */ \
   struct rkMenuItem *ParentItem; \
   struct rkMenu *CurrentMenu;        /* Currently active sub-menu */ \
   objScrollbar *Scrollbar; \
   objXML *prvXML; \
   objPicture *Checkmark; \
   objConfig *Translation;      /* Translation data (config object) */ \
   APTR   prvKeyEvent;          /* Keyboard event subscription */ \
   STRING Config;               /* Configuration file for the menu content. */ \
   STRING Path;                 /* Location of the XML or config menu definition file */ \
   struct KeyStore *LocalArgs;  /* Argument table (created via calls to SetUnlistedField()) */ \
   struct rkMenu *RootMenu;     /* Reference to the top-most menu */ \
   FUNCTION ItemFeedback; \
   LARGE FadeTime; \
   TIMER MotionTimer; \
   TIMER ItemMotionTimer; \
   TIMER TimerID; \
   LONG  X, Y;                  /* (X,Y) within container. */ \
   LONG  VWhiteSpace;\
   LONG  VOffset;               /* Vertical offset from the parent menu */ \
   LONG  YPosition;             /* Menu page position */ \
   LONG  PageWidth, PageHeight; /* Width/Height of menu page */ \
   LONG  Width, Height;         /* Calculated menu width/height */ \
   LONG  InputHandle; \
   LONG  MonitorHandle; \
   UBYTE prvFade;\
   UBYTE prvReverseX:1;         /* If TRUE, menus go from right to left instead of left to right */ \
   UBYTE ShowCheckmarks:1;\
   UBYTE Visible:1;

#define PRV_MENUITEM_FIELDS \
   objMenu *Menu;           /* Parent menu (must be the owner) */ \
   char   KeyString[32]; \
   STRING ObjectName;     /* Object name to assign to the menu */ \
   STRING ChildXML; \
   LONG   Y;              /* Vertical coordinate of this item inside the menu */ \
   UBYTE  CheckmarkFailed:1;
