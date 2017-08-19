# Debian system preparation: "apt-get install libasound2-dev"

include ../../core.mk

.NOTPARALLEL:
NAME=      audio
CATEGORY=  audio
DEST=      $(PARASOL_MODULE)
DOCUMENTS= audio.c class_sound.c class_audio.c
HEADER=    ../../include/parasol/modules/$(NAME).h
CFLAGS += -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -DMOD_NAME=$(NAME)

ifeq ($(OSTYPE),win)
   C_INCLUDE_PATH=
   OBJECTS=  dsound.o
endif

compile: $(HEADER) audio.c functions.c commands.c $(OBJECTS)
ifeq ($(OSTYPE),win)
	$(CC) $(CFLAGS) -I$(PARASOL_HEADERS) $(LIBLINK) $(NAME).c $(OBJECTS)
else
	$(CC) $(CFLAGS) $(LIBLINK) $(NAME).c $(OBJECTS) -lasound
endif

dsound.o: dsound.c
	$(CC) $(CFLAGS) -I$(PARASOL_HEADERS) -c -o $@ $<

$(HEADER) module_def.c: $(NAME).fdl audio.c class_audio.c
	$(IDL_C) src=$< files={ audio.c } output-defs=module_def.c output-proto=module_def.c prototypes=static
	$(IDL_DEF) src=$< append output=module_def.c format=c

clean:
	@rm -f "$(DEST)" *.o
	@touch $(NAME).fdl
