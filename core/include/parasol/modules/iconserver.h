#ifndef MODULES_ICONSERVER
#define MODULES_ICONSERVER 1

// Name:      iconserver.h
// Copyright: Paul Manias © 2014-2017
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_ICONSERVER (1)

struct IconServerBase {
   ERROR (*_CreateIcon)(CSTRING, CSTRING, CSTRING, CSTRING, LONG, struct rkBitmap **);
};

#ifndef PRV_ICONSERVER_MODULE
#define iconCreateIcon(...) (IconServerBase->_CreateIcon)(__VA_ARGS__)
#endif

// IconServer class definition

#define VER_ICONSERVER (1.000000)

typedef struct rkIconServer {
   OBJECT_HEADER
   DOUBLE IconRatio;
   ERROR (*ResolvePath)(struct rkIconServer *, STRING, STRING, LONG);
   LONG   FixedSize;
   LONG   VolatileIcons;

#ifdef PRV_ICONSERVER
   UBYTE prvTheme[60];
  
#endif
} objIconServer;

#endif
