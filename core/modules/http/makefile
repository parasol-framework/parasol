
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

auth.c: auth.fluid
ifeq ($(XXD),)
	@echo The xxd command is unavailable on this system
else
	$(XXD) -c20 -i auth.fluid | sed 's/auth_fluid/glAuthScript/g;s/glAuthScript_len/glAuthScriptLength/g' > $@
endif

$(HEADER): $(NAME).fdl $(NAME).c
	$(IDL_C) --src=$<
	$(IDL_DEF) --src=$< --append --output=http_def.c --format=c

clean:
	@rm -f "$(DEST)" $(OBJECTS)
	@touch $(NAME).fdl
