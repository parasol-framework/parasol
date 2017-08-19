
include ../../core.mk

NAME=     parc
CATEGORY= system
DEST=     $(PARASOL_MODULE)
CFLAGS += -DMOD_NAME=$(NAME)

compile:
	$(CC) $(CFLAGS) $(LIBLINK) $(NAME).c

clean:
	@rm -f "$(DEST)"

.FORCE:
test: .FORCE
	$(FLUTE) file=test/test.fluid
