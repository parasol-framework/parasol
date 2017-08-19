
include ../../core.mk

NAME=     xrandr
CATEGORY= graphics
LIBLINK+= -L/usr/X11R6/lib -lX11 -D__xwindows__
DEST=   $(PARASOL_MODULE)
HEADER=   ../../include/parasol/modules/$(NAME).h
CFLAGS += -DMOD_NAME=$(NAME)

compile: $(HEADER)
ifneq ($(OSTYPE),win)
	$(CC) $(CFLAGS) $(LIBLINK) $(NAME).c -lXrender -lXrandr -lXext
endif

$(HEADER): $(NAME).fdl
	$(IDL_C) src=$< files={ $(NAME).c } output-defs=module_def.c prototypes=static

clean:
	@rm -f "$(DEST)"
	@touch $(NAME).fdl
