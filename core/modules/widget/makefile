
include ../../core.mk

.NOTPARALLEL:
NAME=     widget
CATEGORY= gui
DEST=     $(PARASOL_MODULE)
HEADER=    ../../include/parasol/modules/$(NAME).h
SOURCES = widget.c class_button.c class_checkbox.c class_resize.c class_scrollbar.c \
          class_combobox.c class_tabfocus.c class_input.c class_scroll.c class_image.c \
          class_text/text.c class_menu/menu.c class_view/view.c class_clipboard.c
DOCUMENTS = $(SOURCES)
OBJECTS := $(addprefix obj/$(PLATFORM)/,$(subst .c,.o,$(SOURCES)))
CFLAGS += -DMOD_NAME=$(NAME)
DIRS := obj/$(PLATFORM) obj/$(PLATFORM)/class_menu obj/$(PLATFORM)/class_text \
        obj/$(PLATFORM)/class_view obj/$(PLATFORM)/platform

-include deps-$(OSTYPE).mak

ifeq ($(OSTYPE),win)
   LINK += -lole32
   OBJECTS += obj/$(PLATFORM)/platform/windows.o
endif

compile: $(DEST) hashes.h

$(DEST): $(OBJECTS)
	$(CC) $(CFLAGS) $(LIBLINK_STDC) $^ $(LINK)

$(OBJECTS): obj/$(PLATFORM)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SOURCES): $(HEADER)

obj/$(PLATFORM)/platform/windows.o: platform/windows.c

obj/$(PLATFORM)/class_text/text.o: class_text/fields.c class_text/functions.c

obj/$(PLATFORM)/class_menu/menu.o: class_menu/functions.c class_menu/menuitem.c

obj/$(PLATFORM)/class_view/view.o: class_view/view_fields.c class_view/view_functions.c

hashes.h: hashes.fdl
	$(IDL_C) src=hashes.fdl output=hashes.h

clean:
	@rm -f "$(DEST)" $(OBJECTS)

$(HEADER) widget_def.c: $(NAME).fdl makefile
	$(IDL_C) --src=$< --files={ $(DOCUMENTS) }
	$(IDL_DEF) --src=$< --output=widget_def.c --format=c

deps-$(OSTYPE).mak: $(SOURCES)
	@rm -f deps-$(OSTYPE).mak
	$(foreach SRC,$(SOURCES),$(CC) $(CFLAGS) -MM -MT $(SRC:.c=.o) $(SRC) >> deps-$(OSTYPE).mak;)
	@touch deps-$(OSTYPE).mak

$(shell mkdir -p $(DIRS))
