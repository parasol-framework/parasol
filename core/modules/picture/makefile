# The make process is split into two files so as to allow a parallel build.  This file performs the final link stage only.

include ../../core.mk

NAME=     picture
CATEGORY= graphics
DEST=     $(PARASOL_MODULE)
CFLAGS += -DMOD_NAME=$(NAME)
FLAGS=    $(CFLAGS) -Ilib
OBJECTS=  picture.o lib/png.o lib/pngset.o lib/pngget.o lib/pngread.o lib/pngpread.o \
          lib/pngrtran.o lib/pngrutil.o lib/pngtrans.o lib/pngwrite.o lib/pngwtran.o \
          lib/pngwutil.o lib/pngmem.o lib/pngerror.o
HEADER= ../../include/parasol/modules/$(NAME).h

LINKTO= -lm
ifeq ($(OSTYPE),android)
   LINKTO += -lz
endif
ifeq ($(OSTYPE),win)
   LINKTO += -lz
endif

compile: $(HEADER)
	make -j -f makefile_png
ifeq ($(OSTYPE),win)
	$(CC) -shared -s -o "$(DEST)" $(OBJECTS) $(LINKTO)
else
	$(TOOLCHAIN)ld -r -o lib.o $(OBJECTS)
	$(OBJCOPY) -K ModHeader `sed 's/^/-L /' syms.exp` -R .comment -R .note --strip-unneeded lib.o
	$(CC) $(CFLAGS) -nostartfiles -shared -s -Wl,-soname,$(DEST),-version-script=version -o "$(DEST)" lib.o $(LINKTO)
	rm -f lib.o
endif

$(HEADER) picture_def.c: $(NAME).fdl picture.c
	$(IDL_C) src=$<
	$(IDL_DEF) src=$< append output=picture_def.c format=c

clean:
	@rm -f $(OBJECTS) "$(DEST)"

