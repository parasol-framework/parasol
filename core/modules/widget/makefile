
include ../../core.mk

.NOTPARALLEL:
NAME=     widget
CATEGORY= gui
DEST=     $(PARASOL_MODULE)
HEADER=    ../../include/parasol/modules/$(NAME).h
SOURCES = widget.c class_button.c class_checkbox.c class_resize.c class_scrollbar.c class_dialog.c \
          class_combobox.c class_tabfocus.c class_input.c class_scroll.c class_menubar.c class_image.c \
          class_text/text.c class_menu/menu.c class_view/view.c class_clipboard.c class_fileview/fileview.c
OBJECTS := $(addprefix obj/,$(subst .c,.o,$(SOURCES)))
CFLAGS += -DMOD_NAME=$(NAME)
BLOBS := obj/class_fileview/fileview_shortcut.o obj/class_dialog_script.o

-include deps-$(OSTYPE).mak

ifeq ($(OSTYPE),win)
   LINK += -lole32
   OBJECTS += obj/platform/windows.o
endif

compile: $(DEST) hashes.h

$(DEST): $(OBJECTS) $(BLOBS)
	$(CC) $(CFLAGS) $(LIBLINK_STDC) $^ $(LINK)

$(OBJECTS): obj/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SOURCES): $(HEADER)

obj/platform/windows.o: platform/windows.c

obj/class_text/text.o: class_text/fields.c class_text/functions.c

obj/class_menu/menu.o: class_menu/functions.c class_menu/menuitem.c

obj/class_view/view.o: class_view/view_fields.c class_view/view_functions.c

obj/class_fileview/fileview.o: class_fileview/fileview_fields.c class_fileview/fileview_functions.c

obj/class_fileview/fileview_shortcut.o: class_fileview/fileview_shortcut.fluid makefile
	$(OBJCOPY) $(OBJFLAGS) --redefine-sym _binary_class_fileview_fileview_shortcut_fluid_start=glNewShortcutScript --redefine-sym _binary_class_fileview_fileview_shortcut_fluid_end=glNewShortcutScriptEnd -I binary $< $@

obj/class_dialog_script.o: class_dialog_script.rpl makefile
	$(OBJCOPY) $(OBJFLAGS) --redefine-sym _binary_class_dialog_script_rpl_start=glDocumentXML --redefine-sym _binary_class_dialog_script_rpl_end=glDocumentXMLEnd -I binary $< $@

hashes.h: hashes.fdl
	$(IDL_C) src=hashes.fdl output=hashes.h

clean:
	@rm -f "$(DEST)" $(OBJECTS) $(BLOBS)

$(HEADER): $(NAME).fdl makefile
	$(IDL_C) src=$<
	$(IDL_DEF) src=$< output=widget_def.c format=c

deps-$(OSTYPE).mak: $(SOURCES)
	@rm -f deps-$(OSTYPE).mak
	$(foreach SRC,$(SOURCES),$(CC) $(CFLAGS) -MM -MT $(SRC:.c=.o) $(SRC) >> deps-$(OSTYPE).mak;)
	@touch deps-$(OSTYPE).mak
