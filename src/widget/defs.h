
extern ERROR init_button(void);
extern ERROR init_checkbox(void);
extern ERROR init_clipboard(void);
extern ERROR init_resize(void);
extern ERROR init_combobox(void);
extern ERROR init_input(void);
extern ERROR init_image(void);
extern ERROR init_menu(void);
extern ERROR init_menuitem(void);
extern ERROR init_scroll(void);
extern ERROR init_scrollbar(void);
extern ERROR init_tabfocus(void);
extern ERROR init_text(void);
extern ERROR init_view(void);

extern void free_button(void);
extern void free_checkbox(void);
extern void free_clipboard(void);
extern void free_combobox(void);
extern void free_image(void);
extern void free_input(void);
extern void free_menu(void);
extern void free_menuitem(void);
extern void free_resize(void);
extern void free_scroll(void);
extern void free_scrollbar(void);
extern void free_tabfocus(void);
extern void free_text(void);
extern void free_view(void);

extern struct DisplayBase *DisplayBase;
extern struct SurfaceBase *SurfaceBase;
extern struct FontBase *FontBase;
extern struct IconServerBase *IconServerBase;
extern struct KeyboardBase *KeyboardBase;
extern struct VectorBase *VectorBase;
extern OBJECTPTR modFont, modWidget, modDisplay, modSurface, modIconServer, modVector;
extern char glDefaultFace[64];
extern char glWindowFace[64];
extern char glWidgetFace[64];
extern char glLabelFace[64];
extern LONG glMargin;

extern "C" ERROR widgetCreateIcon(CSTRING Path, CSTRING Class, CSTRING Filter, LONG Size, struct rkBitmap ** Bitmap);
