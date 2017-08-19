
include ../../core.mk

NAME=     xml
CATEGORY= data
DEST=     $(PARASOL_MODULE)
HEADER=   ../../include/parasol/modules/$(NAME).h
CFLAGS += -DMOD_NAME=$(NAME)

compile: $(HEADER)
	$(CC) $(CFLAGS) $(LIBLINK) $(NAME).c

$(HEADER): $(NAME).fdl $(NAME).c
	$(IDL_C) src=$<
	$(IDL_DEF) src=$< append output=xml_def.c format=c

.FORCE:
test: .FORCE
	$(FLUTE) file=test.fluid

clean:
	@touch $(NAME).fdl
	@rm -f "$(DEST)"
