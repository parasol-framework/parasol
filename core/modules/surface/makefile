.NOTPARALLEL:
include ../../core.mk

NAME=      surface
CATEGORY=  gui
DEST=      $(PARASOL_MODULE)
OBJECTS=   surface.o
DOCUMENTS= ./ class_surface/ class_layout/
HEADER=   ../../include/parasol/modules/$(NAME).h
CFLAGS += -DMOD_NAME=$(NAME)

ifeq ($(OSTYPE),win)
OBJECTS+= class_surface/win32/windows.o
endif

-include deps-$(OSTYPE).mak

compile: $(DEST)

$(DEST): $(OBJECTS)
	$(CC) $(CFLAGS) $(LIBLINK_STDC) $(OBJECTS) $(LINK)

%.o: %.c makefile
	$(CC) -c $(CFLAGS) $*.c -o $@

$(HEADER) module_def.c: $(NAME).fdl surface.c class_surface/surface_class.c makefile
	$(IDL_C) src=$< files={ surface.c } output-defs=module_def.c output-proto=module_def.c prototypes=static
	$(IDL_DEF) src=$< append output=module_def.c format=c

clean:
	@rm -f "$(DEST)" $(OBJECTS) *.mak

deps-$(OSTYPE).mak: $(SOURCES)
	$(CC) -MM $(CFLAGS) surface.c class_surface/win32/windows.c >deps-$(OSTYPE).mak
