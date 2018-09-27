# Note: In Windows, compiling with C++ and not C is required because it prevents crashes when loading C++ compiled modules.

# Doesn't seem possible to get these static build options working in MinGW
# -static-libstdc++ -static-libgcc

BUILD_ROOT := ../../
include ../core.mk

NAME= parasol-cmd
DEST= $(PARASOL_RELEASE)/$(NAME)$(EXE)

ifeq ($(OSTYPE),win)
LIBS = $(PARASOL_RELEASE)/libgcc_s_dw2-1.dll $(PARASOL_RELEASE)/system/modules/lib/libstdc++-6.dll
endif

ifeq ($(HOST),mac)
   CXXFLAGS+=-pagezero_size 10000 -image_base 100000000
endif

compile: parasol-cmd.cpp $(PARASOL_RELEASE) $(EXEDEP_INIT) $(LIBS)
ifeq ($(OSTYPE),win)
	$(CXX) $(CXXFLAGS) -Wall -mconsole -I$(PARASOL_HEADERS) -I$(SDK_ROOT)/core/link $(PARASOL_LIB_INIT) \
   $(NAME).cpp sandbox-$(OSTYPE).cpp -o "$(DEST)" $(PARASOL_RES)
else
	$(CXX) $(CXXFLAGS) $(CLIB) -o "$(DEST)" -s $(NAME).cpp $(PARASOL_LIB_INIT) -lpthread -ldl
endif

$(PARASOL_RELEASE):
	mkdir -p "$(PARASOL_RELEASE)"

$(PARASOL_RELEASE)/libgcc_s_dw2-1.dll: $(MSYSTEM_PREFIX)/bin/libgcc_s_dw2-1.dll
	mkdir -p "$(PARASOL_RELEASE)" && cp $< $@

$(PARASOL_RELEASE)/system/modules/lib/libstdc++-6.dll: $(MSYSTEM_PREFIX)/bin/libstdc++-6.dll
	mkdir -p "$(PARASOL_RELEASE)/system/modules/lib" && cp $< $@

clean:
	@rm -f $(DEST) *.o
