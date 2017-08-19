
include ../../../../makefile.inc

PROGRAMS= $(PARASOL_RELEASE)/test_memory$(EXE) $(PARASOL_RELEASE)/test_objlocking$(EXE) $(PARASOL_RELEASE)/test_memlocking$(EXE) $(PARASOL_RELEASE)/test_actionthreads$(EXE)

ifeq ($(OSTYPE),win)
   CFLAGS += -mconsole
endif

compile: $(PROGRAMS)

$(PARASOL_RELEASE)/test_memory$(EXE): memory.c
	$(CC) $(CFLAGS) $(EXELINK) $(CLIB) $(WINLINK) -o "$@" $<

$(PARASOL_RELEASE)/test_objlocking$(EXE): private_object_locking.c
	$(CC) $(CFLAGS) $(EXELINK) $(CLIB) $(WINLINK) -o "$@" $< -lpthread

$(PARASOL_RELEASE)/test_memlocking$(EXE): memory_locking.c
	$(CC) $(CFLAGS) $(EXELINK) $(CLIB) $(WINLINK) -o "$@" $< -lpthread

$(PARASOL_RELEASE)/test_actionthreads$(EXE): action_threads.c
	$(CC) $(CFLAGS) $(EXELINK) $(CLIB) $(WINLINK) -o "$@" $<

clean:
	@rm -f $(PROGRAMS)
