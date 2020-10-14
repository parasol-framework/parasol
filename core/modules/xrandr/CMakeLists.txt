
#LIBLINK+= -L/usr/X11R6/lib -lX11 -D__xwindows__
#
#$(CC) $(CFLAGS) $(LIBLINK) $(NAME).c -lXrender -lXrandr -lXext
#
#$(HEADER): $(NAME).fdl
#	$(IDL_C) src=$< files={ $(NAME).c } output-defs=module_def.c prototypes=static
