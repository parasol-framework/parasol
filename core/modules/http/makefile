
include ../../core.mk

.NOTPARALLEL:
NAME=     http
CATEGORY= network
DEST=     $(PARASOL_MODULE)
OBJECTS=  auth.o
HEADER= ../../include/parasol/modules/$(NAME).h
CFLAGS += -DMOD_NAME=$(NAME)

compile: $(HEADER) $(OBJECTS)
	$(CC) $(CFLAGS) $(LIBLINK) $(NAME).c $(OBJECTS)

auth.o: auth.fluid
	$(OBJCOPY) $(OBJFLAGS) --redefine-sym _binary_auth_fluid_start=glAuthScript --redefine-sym _binary_auth_fluid_end=glAuthScriptEnd -I binary $< $@

$(HEADER): $(NAME).fdl $(NAME).c
	$(IDL_C) src=$<
	$(IDL_DEF) src=$< append output=http_def.c format=c

clean:
	@rm -f "$(DEST)" $(OBJECTS)
	@touch $(NAME).fdl
