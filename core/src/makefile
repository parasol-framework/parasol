BUILD_ROOT=../../
CFLAGS += -I../include
CXXFLAGS += -I../include
include ../core.mk

NAME=    core
DEST=   $(PARASOL_ROOTMOD)
ASM:=    $(ASM) -Iasm

.NOTPARALLEL:

# A bug in (MinGW?) GCC requires the following flag is applied to ensure that __packed__ works correctly.
CFLAGS += -mno-ms-bitfields

LINKLIB += -lz

ifeq ($(OSTYPE),android)
   LINKLIB += -landroid
   OBJECTS += obj/classes/class_assets.o
endif

ifeq ($(OSTYPE),win)
   FLAGS= $(CFLAGS) -c -D__system__
   OBJECTS= obj/microsoft/windows.o
   LINKLIB += -lwsock32 -lpsapi
   ifdef DEBUG
      LINKLIB += -ldbghelp
   endif
else
   FLAGS= $(CFLAGS) -c -D__unix__ -D__system__
endif

# DBG_DIAGNOSTICS: Print enhanced diagnostic messages on crash - e.g. semaphore and memory locks.
#
# DBG_SLEEP: Debug any area of the system that puts the process to sleep.
#
# RANDOMISE_MEM=50000: Randomise memory when it is freed.  The value is
# the maximum memsize allowed for randomisation
#
# Header generation for libdbghelp: dlltool -k -d dbghelp.def -l libdbghelp.a
# Copy libdbghelp.a to your MinGW libs folder.

#FLAGS += -DRANDOMISE_MEM=50000
FLAGS += -DDBG_DIAGNOSTICS

OBJECTS+= obj/core.o obj/data.o obj/data_actionlist.o obj/lib_log.o obj/lib_functions.o obj/lib_memory.o \
          obj/lib_newobject.o obj/lib_actions.o obj/lib_fields_read.o obj/lib_fields_write.o obj/lib_messages.o \
          obj/lib_locking.o obj/internal.o obj/lib_semaphores.o obj/lib_events.o obj/lib_keypairs.o \
          obj/classes/class_module.o obj/classes/class_task.o obj/classes/class_thread.o \
          obj/classes/class_time.o obj/classes/class_config.o obj/classes/class_metaclass.o obj/classes/class_script.o \
          obj/lib_strings.o obj/classes/class_file.o obj/classes/class_file_feedback.o obj/classes/class_storagedevice.o \
          obj/lib_filesystem.o obj/classes/class_archive.o obj/classes/class_compressed_stream.o obj/compression/class_compression.o

SOURCES := $(subst obj/,src/,$(subst .o,.c,$(OBJECTS)))
-include deps-$(OSTYPE).mak

HEADERS := ../include/parasol/modules/core.h ../include/parasol/system/errors.h \
           ../include/parasol/system/fields.h ../include/parasol/system/registry.h

compile: $(HEADERS) $(OBJECTS)
ifeq ($(OSTYPE),win)
	$(CC) -shared $(LINK) -o "$(DEST)" $(OBJECTS) $(LINKLIB)
else
   ifeq ($(OSTYPE),android)
      ifeq ($(DEBUG),1)
	$(CC) -shared $(LINK) -Wl,-soname,lib$(NAME)$(LIBEXT)$(LIBEXT),-version-script=version -o $(DEST) $(OBJECTS) -llog -lpthread -landroid
      else
	$(TOOLCHAIN)ld -r -o obj/lib.o $(OBJECTS)
	$(OBJCOPY) -K CloseCore -K OpenCore -R .comment -R .note --strip-unneeded obj/lib.o
	$(CC) --sysroot "$(SYSROOT)" -llog -shared -fPIC $(LINK) -Wl,-soname,lib$(NAME)$(LIBEXT),-version-script=version -o $(PARASOL_ROOTMOD) obj/lib.o
#	$(CC) --sysroot "$(SYSROOT)" -shared $(LINK) -Wl,-soname,$(LIBNAME)$(LIBEXT) -o $(PARASOL_ROOTMOD) $(OBJECTS)
	rm obj/lib.o
      endif
   else
      # Note: -lrt is required for clock_gettime()

      ifeq ($(DEBUG),1)
	$(CC) -shared $(LINK) -Wl,-soname,$(LIBNAME)$(LIBEXT),-version-script=version -o $(PARASOL_ROOTMOD) $(OBJECTS) -ldl -lrt -lpthread $(LINKLIB)
      else
	$(TOOLCHAIN)ld -r -o obj/lib.o $(OBJECTS)
	$(OBJCOPY) -K CloseCore -K OpenCore -R .comment -R .note --strip-unneeded obj/lib.o
	mkdir -p $(PARASOL_MODULES)
	$(CC) -shared -fPIC $(LINK) -Wl,-soname,$(LIBNAME)$(LIBEXT),-version-script=version -o $(PARASOL_ROOTMOD) obj/lib.o -ldl -lrt -lpthread $(LINKLIB)
	rm obj/lib.o
      endif
   endif
endif

clean:
	@rm -f *.mak $(OBJECTS) obj/lib.o

.FORCE:
test: .FORCE
	$(FLUTE) file=tests/filesystem.fluid
	$(FLUTE) file=tests/keypairs.fluid
	$(FLUTE) file=tests/misc.fluid

######### SOURCE CODE #########

$(OBJECTS): obj/%.o: src/%.c src/defs.h src/data.c src/data_functions.c makefile
	$(CC) $(FLAGS) -c -o $@ $<

../include/parasol/modules/core.h: core.fdl src/lib_fields_read.c src/lib_fields_write.c src/lib_functions.c src/lib_messages.c \
   src/lib_locking.c src/lib_newobject.c src/lib_events.c src/lib_actions.c src/lib_memory.c src/lib_log.c src/core.c \
   src/lib_keypairs.c src/lib_base64.c src/lib_conversion.c src/lib_unicode.c src/lib_strings.c src/lib_filesystem.c
	$(IDL_C) src=$< output=$@ output-proto=src/prototypes.h output-defs=src/data_functions.c files={ src/lib_fields_read.c src/lib_fields_write.c src/lib_functions.c \
   src/lib_messages.c src/lib_locking.c src/lib_newobject.c src/lib_events.c src/lib_actions.c src/lib_memory.c \
   src/lib_log.c src/core.c src/lib_keypairs.c src/lib_base64.c src/lib_conversion.c src/lib_unicode.c src/lib_strings.c \
   src/lib_filesystem.c src/fs_folders.c src/fs_identify.c src/fs_resolution.c src/fs_volumes.c }
	$(IDL_DEF) src=$< output=src/idl.h format=c

../include/parasol/system/errors.h: errors.fdl
	$(IDL_C) src=$< output=$@

../include/parasol/system/fields.h: fields.fdl
	$(IDL_C) src=$< output=$@

../include/parasol/system/registry.h: registry.fdl
	$(IDL_C) src=$< output=$@

# The use of 'sed' is to ensure that 'obj/' is prepended to all .o references in the output .mak file

deps-$(OSTYPE).mak:
	$(CC) -MM $(CFLAGS) $(SOURCES) | sed 's|[a-zA-Z0-9_-]*\.o|obj/&|' >deps-$(OSTYPE).mak
#	$(foreach SRC,$(SOURCES),$(CC) $(CFLAGS) -MM -MT $(SRC:.c=.o) $(SRC) >> deps-$(OSTYPE).mak;)
