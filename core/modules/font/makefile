
include ../../core.mk

NAME= font
DEST= $(PARASOL_MODULE)
DOCUMENT= font.c class_font.c
HEADER= ../../include/parasol/modules/$(NAME).h
FT:= freetype-2.7/

# A bug in (MinGW?) GCC requires the following flag is applied to ensure that __packed__ works correctly.
CFLAGS += -mno-ms-bitfields -DMOD_NAME=$(NAME)

OBJECTS:= $(NAME).o \
 $(FT)src/autofit/autofit.o    $(FT)src/psnames/psnames.o $(FT)src/raster/raster.o \
 $(FT)src/cff/cff.o            $(FT)src/base/ftglyph.o    $(FT)src/sfnt/sfnt.o \
 $(FT)src/base/ftbase.o        $(FT)src/base/ftsystem.o   $(FT)src/smooth/smooth.o \
 $(FT)src/truetype/truetype.o  $(FT)src/base/ftinit.o     $(FT)src/base/ftstroke.o \
 $(FT)src/base/ftbitmap.o      $(FT)src/base/ftdebug.o    $(FT)src/pshinter/pshinter.o

compile: $(HEADER) font_def.c
	make -j -f makefile_obj
	$(CC) $(LIBLINK_STDC) $(OBJECTS) -lm $(LINK)

$(HEADER) font_def.c: $(NAME).fdl $(DOCUMENT)
	$(IDL_C) --src=$< --files={ font.c } --output-defs=font_def.c --output-proto=font_def.c --prototypes=static
	$(IDL_DEF) --src=$< --append --output=font_def.c --format=c

clean:
	@rm -f *.mak $(OBJECTS) "$(DEST)"
	@touch $(NAME).fdl
