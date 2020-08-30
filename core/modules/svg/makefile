
include ../../core.mk

NAME= svg
DEST= $(PARASOL_MODULE)
HEADER= ../../include/parasol/modules/$(NAME).h
DOCUMENTS=  class_rsvg.c class_svg.c class_svgimage.c
CFLAGS += -DMOD_NAME=$(NAME)
KATPATH := katana-parser/src/

OBJS += $(NAME).o $(KATPATH)foundation.o $(KATPATH)katana.lex.o $(KATPATH)katana.tab.o $(KATPATH)parser.o \
   $(KATPATH)selector.o  $(KATPATH)tokenizer.o

compile: $(HEADER) $(OBJS) *.c *.h
ifeq ($(OSTYPE),win)
	$(CC) $(LIBLINK) -lm $(LINK) $(OBJS)
else
	$(CC) $(CFLAGS) -lm $(LIBLINK_STDC) -Ikatana-parser/src $(NAME).c $(LINK) $(OBJS)
endif

$(HEADER) class_svg_def.c class_svgimage_def.c: $(NAME).fdl $(DOCUMENTS)
	$(IDL_C) --src=$< --files={ $(DOCUMENTS) }
	$(IDL_DEF) --src=$< --output=svg_def.c --format=c

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -Ikatana-parser/src -Wno-main -c -o $@ $<

clean:
	@rm -f "$(DEST)" katana-parser/.libs/*
