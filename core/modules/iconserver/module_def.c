// Auto-generated by idl-c.fluid

#ifndef FDEF
#define FDEF static const struct FunctionField
#endif

FDEF argsCreateIcon[] = { { "Error", FD_LONG|FD_ERROR }, { "Path", FD_STR }, { "Class", FD_STR }, { "Theme", FD_STR }, { "Filter", FD_STR }, { "Size", FD_LONG }, { "Bitmap", FD_OBJECTPTR|FD_ALLOC|FD_RESULT }, { 0, 0 } };

const struct Function glFunctions[] = {
   { (APTR)iconCreateIcon, "CreateIcon", argsCreateIcon },
   { NULL, NULL, NULL }
};

