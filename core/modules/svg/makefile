
include ../../core.mk

NAME= svg
DEST= $(PARASOL_MODULE)
HEADER= ../../include/parasol/modules/$(NAME).h
DOCUMENTS=  class_rsvg.c class_svg.c class_svgimage.c
CFLAGS += -DMOD_NAME=$(NAME) -I$(KATANA_PATH)
LIBLINK += -Lkatana-parser/.libs
LIBKATANA= katana-parser/.libs/libkatana.a

compile: $(HEADER) *.c *.h $(LIBKATANA)
ifeq ($(OSTYPE),win)
	$(CC) $(CFLAGS) -lm $(LIBLINK) -Ikatana-parser/src $(NAME).c $(LINK) -lkatana
else
	$(CC) $(CFLAGS) -lm $(LIBLINK_STDC) -Ikatana-parser/src $(NAME).c $(LINK) katana-parser/src/.libs/*.o
endif

$(HEADER) class_svg_def.c class_svgimage_def.c: $(NAME).fdl $(DOCUMENTS)
	$(IDL_C) --src=$< --files={ $(DOCUMENTS) }
	$(IDL_DEF) --src=$< --output=svg_def.c --format=c

$(LIBKATANA):
	cd katana-parser && sh ./autogen.sh && sh ./configure CFLAGS="-std=c99" && make

clean:
	@rm -f "$(DEST)"
