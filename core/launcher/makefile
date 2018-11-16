# Note: In Windows, compiling with C++ and not C is required because it prevents crashes when loading C++ compiled modules.

# Doesn't seem possible to get these static build options working in MinGW64
# -static-libstdc++ -static-libgcc

BUILD_ROOT := ../../
include ../core.mk

PARASOL_CMD = parasol
PARASOL_DEST = "$(PARASOL_RELEASE)/$(PARASOL_CMD)$(EXE)"

FLUID_CMD = fluid
FLUID_DEST = "$(PARASOL_RELEASE)/$(FLUID_CMD)$(EXE)"

ifeq ($(OSTYPE),win)
LIBS = $(PARASOL_RELEASE)/libgcc_s_dw2-1.dll $(PARASOL_RELEASE)/system/modules/lib/libstdc++-6.dll
endif

ifeq ($(HOST),mac)
   CXXFLAGS+=-pagezero_size 10000 -image_base 100000000
endif

compile: $(PARASOL_DEST) $(FLUID_DEST)

$(PARASOL_DEST): parasol.cpp $(PARASOL_RELEASE) $(EXEDEP_INIT) $(LIBS)
ifeq ($(OSTYPE),win)
	$(CXX) $(CXXFLAGS) -Wall -mwindows -I$(PARASOL_HEADERS) -I$(SDK_ROOT)/core/link $(PARASOL_LIB_INIT) \
      parasol.cpp sandbox-$(OSTYPE).cpp -o "$(PARASOL_DEST)" $(PARASOL_RES)
else
	$(CXX) $(CXXFLAGS) $(CLIB) -o "$(PARASOL_DEST)" -s parasol.cpp $(PARASOL_LIB_INIT) -lpthread -ldl
endif

$(FLUID_DEST): fluid.cpp $(PARASOL_RELEASE) $(EXEDEP_INIT) $(LIBS)
ifeq ($(OSTYPE),win)
	$(CXX) $(CXXFLAGS) -Wall -mwindows -I$(PARASOL_HEADERS) -I$(SDK_ROOT)/core/link $(PARASOL_LIB_INIT) \
      $(FLUID_CMD).cpp -o "$(FLUID_DEST)" $(PARASOL_RES)
else
	$(CXX) $(CXXFLAGS) $(CLIB) -o "$(FLUID_DEST)" -s $(FLUID_CMD).cpp $(PARASOL_LIB_INIT) -lpthread -ldl
endif

$(PARASOL_RELEASE):
	mkdir -p "$(PARASOL_RELEASE)"

$(PARASOL_RELEASE)/libgcc_s_dw2-1.dll: $(MSYSTEM_PREFIX)/bin/libgcc_s_dw2-1.dll
	mkdir -p "$(PARASOL_RELEASE)" && cp $< $@

$(PARASOL_RELEASE)/system/modules/lib/libstdc++-6.dll: $(MSYSTEM_PREFIX)/bin/libstdc++-6.dll
	mkdir -p "$(PARASOL_RELEASE)/system/modules/lib" && cp $< $@

clean:
	@rm -f $(PARASOL_DEST) *.o
