# Scintilla module

set (MOD scintilla)

idl_all ("${MOD}.fdl" NAME ${MOD}_defs OUTPUT "${INCLUDE_OUTPUT}/modules/${MOD}.h" APPEND_IDL "module_def.c")

set (SOURCE
   "scintilla/lexlib/Accessor.cxx"
   "scintilla/lexlib/CharacterSet.cxx"
   "scintilla/lexlib/LexerBase.cxx"
   "scintilla/lexlib/LexerModule.cxx"
   "scintilla/lexlib/LexerNoExceptions.cxx"
   "scintilla/lexlib/LexerSimple.cxx"
   "scintilla/lexlib/PropSetSimple.cxx"
   "scintilla/lexlib/StyleContext.cxx"
   "scintilla/lexlib/WordList.cxx"
   "scintilla/src/AutoComplete.cxx"
   "scintilla/src/CallTip.cxx"
   "scintilla/src/Catalogue.cxx"
   "scintilla/src/CellBuffer.cxx"
   "scintilla/src/CharClassify.cxx"
   "scintilla/src/ContractionState.cxx"
   "scintilla/src/Decoration.cxx"
   "scintilla/src/Document.cxx"
   "scintilla/src/Editor.cxx"
   "scintilla/src/ExternalLexer.cxx"
   "scintilla/src/Indicator.cxx"
   "scintilla/src/KeyMap.cxx"
   "scintilla/src/LineMarker.cxx"
   "scintilla/src/PerLine.cxx"
   "scintilla/src/PositionCache.cxx"
   "scintilla/src/RESearch.cxx"
   "scintilla/src/RunStyles.cxx"
   "scintilla/src/ScintillaBase.cxx"
   "scintilla/src/Selection.cxx"
   "scintilla/src/Style.cxx"
   "scintilla/src/UniConversion.cxx"
   "scintilla/src/ViewStyle.cxx"
   "scintilla/src/XPM.cxx"
   "class_scintilla.cxx"
   "class_scintilla_search.cxx")

set (LEXERS
   "scintilla/lexers/LexAsm.cxx"
   "scintilla/lexers/LexBash.cxx"
   "scintilla/lexers/LexBasic.cxx"
   "scintilla/lexers/LexCPP.cxx"
   "scintilla/lexers/LexCSS.cxx"
   "scintilla/lexers/LexD.cxx"
   "scintilla/lexers/LexHaskell.cxx"
   "scintilla/lexers/LexHTML.cxx"
   "scintilla/lexers/LexLua.cxx"
   "scintilla/lexers/LexMSSQL.cxx"
   "scintilla/lexers/LexMySQL.cxx"
   "scintilla/lexers/LexOthers.cxx"
   "scintilla/lexers/LexPascal.cxx"
   "scintilla/lexers/LexPerl.cxx"
   "scintilla/lexers/LexRebol.cxx"
   "scintilla/lexers/LexPython.cxx"
   "scintilla/lexers/LexRuby.cxx"
   "scintilla/lexers/LexSQL.cxx"
   "scintilla/lexers/LexVB.cxx")

# Build the library

add_library (scintilla SHARED)

target_compile_options (scintilla PRIVATE "$<${GCC_COMPAT}:$<BUILD_INTERFACE:-fpermissive>>")

set_module_defaults (scintilla)

target_sources (scintilla PRIVATE ${SOURCE} ${LEXERS})

target_include_directories (scintilla PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}/scintilla/src"
   "${CMAKE_CURRENT_SOURCE_DIR}/scintilla/lexlib"
   "${CMAKE_CURRENT_SOURCE_DIR}/scintilla/include")

add_definitions ("-DSCI_NAMESPACE" "-D__PARASOL__" "-DSCI_LEXER")
