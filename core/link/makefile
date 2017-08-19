
include ../../makefile.inc
CFLAGS += -I../include

ifeq ($(OSTYPE),win)
   DEST_DLL = init-dll.o
   OBJECTS += resource.o $(DEST_DLL)
endif
DEST_INIT = init-$(OSTYPE)
OBJECTS += $(DEST_INIT)$(ARCHEXT).o

compile: $(OBJECTS)

$(DEST_INIT)$(ARCHEXT).o: $(DEST_INIT).c
	$(CC) -c $(CFLAGS) -o $@ $(DEST_INIT).c

$(DEST_DLL): init-dll.c
	$(CC) -c $(CFLAGS) -o $@ init-dll.c

resource.o: resource.rc
	$(WINDRES) -o resource.o resource.rc

clean:
	@rm -f *.o
