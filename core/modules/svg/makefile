
include ../../core.mk

NAME= svg
DEST= $(PARASOL_MODULE)
HEADER= ../../include/parasol/modules/$(NAME).h
DOCUMENTS=  class_rsvg.c class_svg.c class_svgimage.c
CFLAGS += -DMOD_NAME=$(NAME)

compile: $(HEADER) *.c *.h
	$(CC) $(CFLAGS) $(LIBLINK_STDC) $(NAME).c $(LINK)

$(HEADER) class_svg_def.c class_svgimage_def.c: $(NAME).fdl $(DOCUMENTS)
	$(IDL_C) --src=$< --files={ $(DOCUMENTS) }
	$(IDL_DEF) --src=$< --output=svg_def.c --format=c

clean:
	@rm -f "$(DEST)"
