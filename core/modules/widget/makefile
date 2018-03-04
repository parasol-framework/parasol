
include ../../core.mk

.NOTPARALLEL:
NAME=     widget
CATEGORY= gui
DEST=     $(PARASOL_MODULE)
HEADER=    ../../include/parasol/modules/$(NAME).h
SOURCES = widget.c class_button.c class_checkbox.c class_resize.c class_scrollbar.c class_dialog.c \
          class_combobox.c class_tabfocus.c class_input.c class_scroll.c class_menubar.c class_image.c \
          class_text/text.c class_menu/menu.c class_view/view.c class_clipboard.c class_fileview/fileview.c
OBJECTS := $(addprefix obj/$(PLATFORM)/,$(subst .c,.o,$(SOURCES)))
CFLAGS += -DMOD_NAME=$(NAME)
BLOBS := class_fileview/fileview_shortcut.c class_dialog_script.h
DIRS := obj/$(PLATFORM) obj/$(PLATFORM)/class_fileview obj/$(PLATFORM)/class_menu obj/$(PLATFORM)/class_text \
        obj/$(PLATFORM)/class_view obj/$(PLATFORM)/platform

-include deps-$(OSTYPE).mak

ifeq ($(OSTYPE),win)
   LINK += -lole32
   OBJECTS += obj/$(PLATFORM)/platform/windows.o
endif

compile: $(DEST) hashes.h

class_dialog_script.h: class_dialog_script.rpl
ifeq ($(XXD),)
	@echo The xxd command is unavailable on this system
else
	$(XXD) -c20 -i class_dialog_script.rpl | sed 's/class_dialog_script_rpl/glDocumentXML/g;s/glDocumentXML_len/glDocumentXMLLength/g' > $@
endif

$(DEST): $(OBJECTS)
	$(CC) $(CFLAGS) $(LIBLINK_STDC) $^ $(LINK)

$(OBJECTS): obj/$(PLATFORM)/%.o: %.c class_dialog_script.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(SOURCES): $(HEADER)

obj/$(PLATFORM)/platform/windows.o: platform/windows.c

obj/$(PLATFORM)/class_text/text.o: class_text/fields.c class_text/functions.c

obj/$(PLATFORM)/class_menu/menu.o: class_menu/functions.c class_menu/menuitem.c

obj/$(PLATFORM)/class_view/view.o: class_view/view_fields.c class_view/view_functions.c

obj/$(PLATFORM)/class_fileview/fileview.o: class_fileview/fileview_fields.c class_fileview/fileview_functions.c class_fileview/fileview_shortcut.c

class_fileview/fileview_shortcut.c: class_fileview/fileview_shortcut.fluid
ifeq ($(XXD),)
	@echo The xxd command is unavailable on this system
else
	$(XXD) -c20 -i class_fileview/fileview_shortcut.fluid | sed 's/class_fileview_fileview_shortcut_fluid/glNewShortcutScript/g;s/glNewShortcutScript_len/glNewShortcutScriptLength/g' > $@
endif

hashes.h: hashes.fdl
	$(IDL_C) src=hashes.fdl output=hashes.h

clean:
	@rm -f "$(DEST)" $(OBJECTS)

$(HEADER): $(NAME).fdl makefile
	$(IDL_C) src=$<
	$(IDL_DEF) src=$< output=widget_def.c format=c

deps-$(OSTYPE).mak: $(SOURCES)
	@rm -f deps-$(OSTYPE).mak
	$(foreach SRC,$(SOURCES),$(CC) $(CFLAGS) -MM -MT $(SRC:.c=.o) $(SRC) >> deps-$(OSTYPE).mak;)
	@touch deps-$(OSTYPE).mak

$(shell mkdir -p $(DIRS))
