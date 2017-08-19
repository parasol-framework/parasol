
include ../../core.mk

NAME=     json
CATEGORY= data
DEST=     $(PARASOL_MODULE)
CFLAGS += -DMOD_NAME=$(NAME)

compile:
	$(CC) $(CFLAGS) $(LIBLINK) $(NAME).c

clean:
	@rm -f "$(DEST)"
