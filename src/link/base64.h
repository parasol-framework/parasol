#pragma once

#include <parasol/main.h>

typedef struct pfBase64Decode {
   UBYTE Step;             // Internal
   UBYTE PlainChar;        // Internal
   UBYTE Initialised:1;    // Internal
  pfBase64Decode() : Step(0), PlainChar(0), Initialised(0) { };
} BASE64DECODE;

typedef struct pfBase64Encode {
   UBYTE Step;        // Internal
   UBYTE Result;      // Internal
   LONG  StepCount;   // Internal
  pfBase64Encode() : Step(0), Result(0), StepCount(0) { };
} BASE64ENCODE;

const LONG CHARS_PER_LINE = 72;

LONG Base64Encode(pfBase64Encode *State, const void *Input, LONG InputSize, STRING Output, LONG OutputSize);
ERR Base64Decode(pfBase64Decode *State, CSTRING Input, LONG InputSize, APTR Output, LONG *Written);
