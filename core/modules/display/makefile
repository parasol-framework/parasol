
include ../../core.mk

NAME=      display
CATEGORY=  graphics
DEST=      $(PARASOL_MODULE)
DOCUMENTS= class_pointer.c display-driver.c class_display.c class_bitmap.c
HEADER=   ../../include/parasol/modules/$(NAME).h

compile: $(HEADER)
ifeq ($(OSTYPE),win)
	make -f makefile.win32
else
   ifeq ($(OSTYPE),android)
	make -f makefile.gles11
   else
	make -f makefile.x11
   endif
endif

clean:
	@touch $(NAME).fdl
ifeq ($(OSTYPE),win)
	make -f makefile.win32 clean
else
   ifeq ($(OSTYPE),android)
	make -f makefile.gles11 clean
   else
	make -f makefile.x11 clean
   endif
endif

$(HEADER): $(NAME).fdl
	$(IDL_C) src=$< files={ $(DOCUMENTS) } output-defs=module_def.c output-proto=module_def.c prototypes=static
	$(IDL_DEF) src=$< output=idl.h format=c
