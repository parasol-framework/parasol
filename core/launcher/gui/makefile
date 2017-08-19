# Note: In Windows, compiling with C++ rather than C is needed because it prevents crashes when loading C++ compiled modules.

BUILD_ROOT := ../../
include ../core.mk

NAME=     parasol
DEST=    $(PARASOL_RELEASE)/$(NAME)$(EXE)

ifneq ($(OSTYPE),win)
   EXELINK += -lpthread
endif

compile: $(NAME).c $(OBJECTS) $(EXEDEP)
	$(CXX) $(CXXFLAGS) $(CLIB) -o "$(DEST)" $(EXELINK) $(NAME).c $(OBJECTS)
#ifeq ($(OSTYPE),win)
#	$(CXX) -g -Wall -mconsole -I$(PARASOL_HEADERS) -I$(SDK_ROOT)/core/link $(PARASOL_LIB) $(NAME).c -o "$(CONDEST)" $(OBJECTS) $(PARASOL_RES)
#endif

clean:
	@rm -f $(DEST) *.o
