// Scintilla source code edit control
/** @file LexerNoExceptions.cxx
 ** A simple lexer with no state which does not throw exceptions so can be used in an external lexer.
 **/
// Copyright 1998-2010 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "PropSetSimple.h"
#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "LexerModule.h"
#include "LexerBase.h"
#include "LexerNoExceptions.h"

#ifdef SCI_NAMESPACE
using namespace Scintilla;
#endif

int SCI_METHOD LexerNoExceptions::PropertySet(const char *key, const char *val) {
   return LexerBase::PropertySet(key, val);
}

int SCI_METHOD LexerNoExceptions::WordListSet(int n, const char *wl) {
	return LexerBase::WordListSet(n, wl);
}

void SCI_METHOD LexerNoExceptions::Lex(unsigned int startPos, int length, int initStyle, IDocument *pAccess) {
	Accessor astyler(pAccess, &props);
	Lexer(startPos, length, initStyle, pAccess, astyler);
	astyler.Flush();
}
void SCI_METHOD LexerNoExceptions::Fold(unsigned int startPos, int length, int initStyle, IDocument *pAccess) {
	Accessor astyler(pAccess, &props);
   Folder(startPos, length, initStyle, pAccess, astyler);
	astyler.Flush();
}
