
include ../../core.mk

NAME=      document
CATEGORY=  gui
DEST=      $(PARASOL_MODULE)
DOCUMENTS= document.c class/
HEADER=    ../../include/parasol/modules/$(NAME).h
CFLAGS += -DMOD_NAME=$(NAME)

hashes.h: hashes.fdl
	$(IDL_C) src=hashes.fdl output=hashes.h

compile: $(HEADER) module_def.c hashes.h
	$(CC) $(CFLAGS) $(LIBLINK) $(NAME).c

$(HEADER) module_def.c: $(NAME).fdl document.c class/document_class.c
	$(IDL_C) --src=$< --files={ document.c } --output-defs=module_def.c --output-proto=module_def.c --prototypes=static
	$(IDL_DEF) --src=$< --append --output=module_def.c --format=c

clean:
	@rm -f "$(DEST)"
