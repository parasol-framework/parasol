
include ../../core.mk

NAME=     scintilla
CATEGORY= tools
DEST=     $(PARASOL_MODULE)
HEADER=    ../../include/parasol/modules/$(NAME).h
DOCUMENTS= class_scintilla.cxx class_scintilla_search.cxx
CFLAGS += -DMOD_NAME=$(NAME)
CXXFLAGS += -DMOD_NAME=$(NAME)

OBJECTS= scintilla/lexlib/Accessor.o   scintilla/lexlib/CharacterSet.o   scintilla/lexlib/LexerBase.o   scintilla/lexlib/LexerModule.o \
  scintilla/lexlib/LexerNoExceptions.o   scintilla/lexlib/LexerSimple.o   scintilla/lexlib/PropSetSimple.o   scintilla/lexlib/StyleContext.o \
  scintilla/lexlib/WordList.o   scintilla/src/AutoComplete.o   scintilla/src/CallTip.o   scintilla/src/Catalogue.o \
  scintilla/src/CellBuffer.o   scintilla/src/CharClassify.o   scintilla/src/ContractionState.o   scintilla/src/Decoration.o \
  scintilla/src/Document.o   scintilla/src/Editor.o   scintilla/src/ExternalLexer.o   scintilla/src/Indicator.o \
  scintilla/src/KeyMap.o   scintilla/src/LineMarker.o   scintilla/src/PerLine.o   scintilla/src/PositionCache.o \
  scintilla/src/RESearch.o   scintilla/src/RunStyles.o   scintilla/src/ScintillaBase.o   scintilla/src/Selection.o \
  scintilla/src/Style.o   scintilla/src/UniConversion.o   scintilla/src/ViewStyle.o   scintilla/src/XPM.o \
  class_scintilla.o class_scintilla_search.o

LEXOBJS = scintilla/lexers/LexAsm.o scintilla/lexers/LexBash.o scintilla/lexers/LexBasic.o scintilla/lexers/LexCPP.o \
          scintilla/lexers/LexCSS.o scintilla/lexers/LexD.o scintilla/lexers/LexHaskell.o scintilla/lexers/LexHTML.o scintilla/lexers/LexLua.o \
          scintilla/lexers/LexMSSQL.o scintilla/lexers/LexMySQL.o scintilla/lexers/LexOthers.o scintilla/lexers/LexPascal.o scintilla/lexers/LexPerl.o scintilla/lexers/LexRebol.o \
          scintilla/lexers/LexPython.o scintilla/lexers/LexRuby.o scintilla/lexers/LexSQL.o scintilla/lexers/LexVB.o

compile: $(HEADER) $(NAME).fdl
	make -j -f makefile_obj
ifeq ($(OSTYPE),WIN)
# For some reason, *static* linking with C++ is now required because we've seen crashes on exit otherwise (fault lies
# with MinGW???)
	$(CXX) $(LIBLINK_STDCPP) $(OBJECTS) $(LEXOBJS) -static -lsupc++ -lm
else
	$(TOOLCHAIN)ld -r -o lib.o $(OBJECTS) $(LEXOBJS)
	$(CXX) $(CXXFLAGS) -shared -Wl,-soname,$(DEST),-version-script=version,-Bsymbolic -o "$(DEST)" lib.o
   #This version works but produces a bigger binary
	#$(CXX) $(LIBLINK_STDCPP) $(OBJECTS) $(LEXOBJS)
endif

clean:
	@rm -f $(OBJECTS) $(LEXOBJS) $(DEST) *.mak
	@touch $(NAME).fdl

$(HEADER): $(NAME).fdl $(DOCUMENTS)
	$(IDL_C) --src=$<
	$(IDL_DEF) --src=$< --output=module_def.c --format=c
