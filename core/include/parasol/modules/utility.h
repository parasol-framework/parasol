#ifndef MODULES_UTILITY
#define MODULES_UTILITY 1

// Name:      utility.h
// Copyright: Paul Manias © 2000-2017
// Generator: idl-c

#ifndef MAIN_H
#include <parasol/main.h>
#endif

#define MODVERSION_UTILITY (1)

// Run modes.

#define RNL_OPEN 1
#define RNL_EDIT 2
#define RNL_PRINT 3
#define RNL_VIEW 4

// Run flags.

#define RNF_FOREIGN 0x00000001
#define RNF_WAIT 0x00000002
#define RNF_RESET_PATH 0x00000004
#define RNF_PRIVILEGED 0x00000008
#define RNF_RESTART 0x00000010
#define RNF_TRANSLATE 0x00000020
#define RNF_SHELL 0x00000040
#define RNF_VIEW_OUTPUT 0x00000080
#define RNF_VIEW_QUIT 0x00000100
#define RNF_QUIET 0x00000200
#define RNF_NO_DIALOG 0x00000400
#define RNF_ADMIN 0x00000800
#define RNF_ATTACH 0x00001000
#define RNF_ATTACHED 0x00001000
#define RNF_DETACH 0x00002000
#define RNF_DETACHED 0x00002000

// Run class definition

#define VER_RUN (1.000000)

typedef struct rkRun {
   OBJECT_HEADER
   DOUBLE    TimeOut;    // The maximum amount of time to wait for a process to return.
   OBJECTPTR Task;       // Reference to the task object used for process execution.
   OBJECTID  OutputID;   // An object that will receive program output
   LONG      Mode;       // The type of action to perform in launching the file
   LONG      Flags;      // Optional flags
   LONG      RestartLimit;
   LONG      ProcessID;  // ID of the launched process.

#ifdef PRV_RUN
   STRING    Args;            // The arguments to pass to the program
   STRING    *Files;
   STRING    FilePaths;
   OBJECTID  WindowID;
   UBYTE     Type[32];
   UBYTE     RestartCount;    // Total number of restarts in a 30 sec period
   LARGE     LastRestart;     // Time of the last restart
   LONG      FileCount;
   struct run_type *Types;
   APTR      TaskRemovedHandle;
  
#endif
} objRun;

#endif
