#pragma once

#include <parasol/main.h>

namespace pf {

struct BASE64DECODE {
   UBYTE Step;             // Internal
   UBYTE PlainChar;        // Internal
   UBYTE Initialised:1;    // Internal
  BASE64DECODE() : Step(0), PlainChar(0), Initialised(0) { };
};

struct BASE64ENCODE {
   UBYTE Step;        // Internal
   UBYTE Result;      // Internal
   LONG  StepCount;   // Internal
  BASE64ENCODE() : Step(0), Result(0), StepCount(0) { };
};

const LONG CHARS_PER_LINE = 72;

LONG Base64Encode(BASE64ENCODE *, const void *, LONG, STRING, LONG );
ERR Base64Decode(BASE64DECODE *, CSTRING, LONG, APTR, LONG *);

};
