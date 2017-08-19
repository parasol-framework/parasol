
include ../../core.mk

NAME=     window
CATEGORY= gui
DEST=     $(PARASOL_MODULE)
HEADER= ../../include/parasol/modules/$(NAME).h
CFLAGS += -DMOD_NAME=$(NAME)

compile: $(HEADER)
	$(CC) $(CFLAGS) $(LIBLINK) $(NAME).c

$(HEADER): $(NAME).fdl
	$(IDL_C) src=$<
	$(IDL_DEF) src=$< output=window_def.c format=c

clean:
	@rm -f "$(DEST)"
	@touch $(NAME).fdl
