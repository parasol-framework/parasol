
include ../../makefile.inc
CFLAGS += -I../include

ifeq ($(OSTYPE),win)
   DEST_DLL = init-dll
   OBJECTS += resource$(ARCHEXT).o $(DEST_DLL)$(ARCHEXT).o
endif
DEST_INIT = init-$(OSTYPE)
OBJECTS += $(DEST_INIT)$(ARCHEXT).o

compile: $(OBJECTS)

$(DEST_INIT)$(ARCHEXT).o: $(DEST_INIT).c
	$(CC) -c $(CFLAGS) -o $@ $(DEST_INIT).c

$(DEST_DLL)$(ARCHEXT).o: init-dll.c
	$(CC) -c $(CFLAGS) -o $@ init-dll.c

resource$(ARCHEXT).o: resource.rc
	$(WINDRES) -o $@ resource.rc

clean:
	@rm -f *.o
