# Fluid has a dependency on the installation of libffi, found in the 3rdparty folder.

include ../../core.mk

.NOTPARALLEL:
NAME=     fluid
CATEGORY= data
DEST=     $(PARASOL_MODULE)
LUA_PATH:= luajit-2.0.5/src/
FFI_PATH:= $(subst /fficonfig.h,,$(wildcard ../../3rdparty/libffi-*/*/fficonfig.h))
HEADER=   ../../include/parasol/modules/$(NAME).h
CFLAGS += -DLUAJIT_ENABLE_LUA52COMPAT
LIBLINK += -L$(FFI_PATH)/.libs
CFLAGS += -I$(FFI_PATH)/include -DMOD_NAME=$(NAME)
LINK += -lm

FLUID= fluid.o
LUA = $(LUA_PATH)ljamalg.o

compile: $(FLUID) $(LUA) $(LUA_PATH)lj_vm.o hashes.h
ifeq ($(OSTYPE),win)
	$(CC) -lm $(LIBLINK) $(FLUID) $(LUA) $(LUA_PATH)lj_vm.o -Wl,-Bstatic -lffi
else
	$(CC) $(CFLAGS) $(LIBLINK_STDC) $(FLUID) $(LUA) $(LUA_PATH)lj_vm.o $(LINK) $(FFI_PATH)/src/.libs/*.o $(FFI_PATH)/src/*/.libs/*.o
endif

hashes.h: hashes.fdl
	$(IDL_C) src=$< output=$@

../../include/parasol/modules/$(NAME).h: $(NAME).fdl fluid.c
	$(IDL_C) src=$< files={ fluid.c }

$(FLUID): $(HEADER) *.c hashes.h
	$(CC) $(CFLAGS) -I$(LUA_PATH) -c -o fluid.o fluid.c

$(LUA):
ifeq ($(OSTYPE),win)
	make -C $(LUA_PATH) amalg
else
	CFLAGS="-fPIC" make -C $(LUA_PATH) amalg
endif

.FORCE:
test: .FORCE
	$(FLUTE) file=tests/catch.fluid
	$(FLUTE) file=tests/action_threads.fluid
	$(FLUTE) file=tests/nz.fluid
	$(FLUTE) file=tests/object.fluid
	$(FLUTE) file=tests/struct.fluid
#	$(FLUTE) file=tests/module_calls.fluid

clean:
	@rm -f $(FLUID) $(LUA) $(LUA_PATH)lj_vm.o
	make -C $(LUA_PATH) clean
