#pragma once

// Name:      parc.h
// Copyright: Paul Manias © 2023
// Generator: idl-c

#include <parasol/main.h>

#define MODVERSION_PARC (1)

class objParc;

// Parc class definition

#define VER_PARC (1.000000)

class objParc : public BaseClass {
   public:
   static constexpr CLASSID CLASS_ID = ID_PARC;
   static constexpr CSTRING CLASS_NAME = "Parc";

   using create = pf::Create<objParc>;

   STRING   Message;  // Stores user readable messages on error.
   OBJECTID OutputID; // Nominate an object for receiving program output.

   // Action stubs

   inline ERROR activate() { return Action(AC_Activate, this, NULL); }
   inline ERROR dataFeed(OBJECTID ObjectID, LONG Datatype, const void *Buffer, LONG Size) {
      struct acDataFeed args = { { ObjectID }, { Datatype }, Buffer, Size };
      return Action(AC_DataFeed, this, &args);
   }
   inline ERROR init() { return Action(AC_Init, this, NULL); }
};

