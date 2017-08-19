# Never use -ffast-math, it can generate very inaccurate icon resizing results.

include ../../core.mk

NAME=     iconserver
DEST=     $(PARASOL_MODULE)
DOCUMENTS= iconserver_class.c
HEADER=   ../../include/parasol/modules/$(NAME).h
CFLAGS += -DMOD_NAME=$(NAME)

compile: $(HEADER) module_def.c iconserver.c iconserver_class.c
	$(CC) $(CFLAGS) $(LIBLINK) $(NAME).c

$(HEADER) module_def.c: $(NAME).fdl iconserver.c iconserver_class.c
	$(IDL_C) src=$< files={ iconserver.c } output-defs=module_def.c prototypes=static

clean:
	@rm -f "$(DEST)"
