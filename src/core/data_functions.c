// Auto-generated by idl-c.fluid

#ifndef FDEF
#define FDEF static const struct FunctionField
#endif

FDEF argsAccessMemory[] = { { "Error", FD_LONG|FD_ERROR }, { "Memory", FD_LONG }, { "Flags", FD_LONG }, { "MilliSeconds", FD_LONG }, { "Result", FD_PTR|FD_RESULT }, { 0, 0 } };
FDEF argsAccessObject[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTID }, { "MilliSeconds", FD_LONG }, { "Result", FD_OBJECTPTR|FD_RESULT }, { 0, 0 } };
FDEF argsAccessPrivateObject[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "MilliSeconds", FD_LONG }, { 0, 0 } };
FDEF argsAction[] = { { "Error", FD_LONG|FD_ERROR }, { "Action", FD_LONG }, { "Object", FD_OBJECTPTR }, { "Parameters", FD_PTR }, { 0, 0 } };
FDEF argsActionList[] = { { "Void", FD_VOID }, { "ActionTable:Actions", FD_ARRAY|FD_STRUCT|FD_RESULT }, { "Size", FD_LONG|FD_ARRAYSIZE|FD_RESULT }, { 0, 0 } };
FDEF argsActionMsg[] = { { "Error", FD_LONG|FD_ERROR }, { "Action", FD_LONG }, { "Object", FD_OBJECTID }, { "Args", FD_PTR }, { "MessageID", FD_LONG }, { "ClassID", FD_LONG|FD_UNSIGNED }, { 0, 0 } };
FDEF argsActionTags[] = { { "Error", FD_LONG|FD_ERROR }, { "Action", FD_LONG }, { "Object", FD_OBJECTPTR }, { "Tags", FD_TAGS }, { 0, 0 } };
FDEF argsActionThread[] = { { "Error", FD_LONG|FD_ERROR }, { "Action", FD_LONG }, { "Object", FD_OBJECTPTR }, { "Args", FD_PTR }, { "Callback", FD_FUNCTIONPTR }, { "Key", FD_LONG }, { 0, 0 } };
FDEF argsAddInfoTag[] = { { "Error", FD_LONG|FD_ERROR }, { "FileInfo:Info", FD_PTR|FD_STRUCT }, { "Name", FD_STR }, { "Value", FD_STR }, { 0, 0 } };
FDEF argsAddMsgHandler[] = { { "Error", FD_LONG|FD_ERROR }, { "Custom", FD_PTR }, { "MsgType", FD_LONG }, { "Routine", FD_FUNCTIONPTR }, { "MsgHandler:Handle", FD_PTR|FD_STRUCT|FD_RESOURCE|FD_ALLOC|FD_RESULT }, { 0, 0 } };
FDEF argsAdjustLogLevel[] = { { "Result", FD_LONG }, { "Adjust", FD_LONG }, { 0, 0 } };
FDEF argsAllocMemory[] = { { "Error", FD_LONG|FD_ERROR }, { "Size", FD_LONG }, { "Flags", FD_LONG }, { "Address", FD_PTR|FD_RESULT }, { "ID", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsAllocMutex[] = { { "Error", FD_LONG|FD_ERROR }, { "Flags", FD_LONG }, { "Result", FD_PTR|FD_RESULT }, { 0, 0 } };
FDEF argsAllocSharedMutex[] = { { "Error", FD_LONG|FD_ERROR }, { "Name", FD_STR }, { "Mutex", FD_PTR|FD_RESULT }, { 0, 0 } };
FDEF argsAllocateID[] = { { "Result", FD_LONG }, { "Type", FD_LONG }, { 0, 0 } };
FDEF argsAnalysePath[] = { { "Error", FD_LONG|FD_ERROR }, { "Path", FD_STR }, { "Type", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsBase64Decode[] = { { "Error", FD_LONG|FD_ERROR }, { "rkBase64Decode:State", FD_PTR|FD_STRUCT|FD_RESOURCE }, { "Input", FD_STR }, { "InputSize", FD_LONG|FD_BUFSIZE }, { "Output", FD_BUFFER|FD_PTR }, { "Written", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsBase64Encode[] = { { "Result", FD_LONG }, { "Input", FD_BUFFER|FD_PTR }, { "InputSize", FD_LONG|FD_BUFSIZE }, { "Output", FD_BUFFER|FD_STR }, { "OutputSize", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF argsBroadcastEvent[] = { { "Error", FD_LONG|FD_ERROR }, { "Event", FD_PTR }, { "EventSize", FD_LONG }, { 0, 0 } };
FDEF argsCheckAction[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Action", FD_LONG }, { 0, 0 } };
FDEF argsCheckMemoryExists[] = { { "Error", FD_LONG|FD_ERROR }, { "ID", FD_LONG }, { 0, 0 } };
FDEF argsCheckObjectExists[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTID }, { 0, 0 } };
FDEF argsCloneMemory[] = { { "Error", FD_LONG|FD_ERROR }, { "Address", FD_PTR }, { "Flags", FD_LONG }, { "NewAddress", FD_PTR|FD_RESULT }, { "NewID", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsCompareFilePaths[] = { { "Error", FD_LONG|FD_ERROR }, { "PathA", FD_STR }, { "PathB", FD_STR }, { 0, 0 } };
FDEF argsCopyFile[] = { { "Error", FD_LONG|FD_ERROR }, { "Source", FD_STR }, { "Dest", FD_STR }, { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF argsCopyMemory[] = { { "Error", FD_LONG|FD_ERROR }, { "Src", FD_PTR }, { "Dest", FD_PTR }, { "Size", FD_LONG }, { 0, 0 } };
FDEF argsCreateFolder[] = { { "Error", FD_LONG|FD_ERROR }, { "Path", FD_STR }, { "Permissions", FD_LONG }, { 0, 0 } };
FDEF argsCreateLink[] = { { "Error", FD_LONG|FD_ERROR }, { "From", FD_STR }, { "To", FD_STR }, { 0, 0 } };
FDEF argsCreateObject[] = { { "Error", FD_LONG|FD_ERROR }, { "ClassID", FD_LARGE }, { "Flags", FD_LONG }, { "Object", FD_OBJECTPTR|FD_RESULT }, { "Tags", FD_VARTAGS }, { 0, 0 } };
FDEF argsCurrentContext[] = { { "Object", FD_OBJECTPTR }, { 0, 0 } };
FDEF argsCurrentTask[] = { { "Object", FD_OBJECTPTR }, { 0, 0 } };
FDEF argsDeleteFile[] = { { "Error", FD_LONG|FD_ERROR }, { "Path", FD_STR }, { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF argsDeleteVolume[] = { { "Error", FD_LONG|FD_ERROR }, { "Name", FD_STR }, { 0, 0 } };
FDEF argsFindClass[] = { { "Object", FD_OBJECTPTR }, { "ClassID", FD_LONG|FD_UNSIGNED }, { 0, 0 } };
FDEF argsFindField[] = { { "Field", FD_PTR|FD_STRUCT }, { "Object", FD_OBJECTPTR }, { "FieldID", FD_LONG|FD_UNSIGNED }, { "Source", FD_OBJECTPTR|FD_RESULT }, { 0, 0 } };
FDEF argsFindObject[] = { { "Error", FD_LONG|FD_ERROR }, { "Name", FD_STR }, { "ClassID", FD_LONG|FD_UNSIGNED }, { "Flags", FD_LONG }, { "Array", FD_BUFFER|FD_ARRAY|FD_OBJECTID }, { "Count", FD_LONG|FD_ARRAYSIZE|FD_RESULT }, { 0, 0 } };
FDEF argsFindPrivateObject[] = { { "Error", FD_LONG|FD_ERROR }, { "Name", FD_STR }, { "Object", FD_OBJECTPTR|FD_RESULT }, { 0, 0 } };
FDEF argsFreeMutex[] = { { "Void", FD_VOID }, { "Mutex", FD_PTR }, { 0, 0 } };
FDEF argsFreeResource[] = { { "Error", FD_LONG|FD_ERROR }, { "Address", FD_PTR }, { 0, 0 } };
FDEF argsFreeResourceID[] = { { "Error", FD_LONG|FD_ERROR }, { "ID", FD_LONG }, { 0, 0 } };
FDEF argsFreeSharedMutex[] = { { "Void", FD_VOID }, { "Mutex", FD_PTR }, { 0, 0 } };
FDEF argsFuncError[] = { { "Error", FD_LONG|FD_ERROR }, { "Header", FD_STR }, { "Error", FD_LONG|FD_ERROR }, { 0, 0 } };
FDEF argsGenCRC32[] = { { "Result", FD_LONG|FD_UNSIGNED }, { "CRC", FD_LONG|FD_UNSIGNED }, { "Data", FD_PTR }, { "Length", FD_LONG|FD_UNSIGNED }, { 0, 0 } };
FDEF argsGetActionMsg[] = { { "Message", FD_PTR|FD_STRUCT|FD_RESOURCE }, { 0, 0 } };
FDEF argsGetClassID[] = { { "Result", FD_LONG|FD_UNSIGNED }, { "Object", FD_OBJECTID }, { 0, 0 } };
FDEF argsGetErrorMsg[] = { { "Result", FD_STR }, { "Error", FD_LONG|FD_ERROR }, { 0, 0 } };
FDEF argsGetEventID[] = { { "Result", FD_LARGE }, { "Group", FD_LONG }, { "SubGroup", FD_STR }, { "Event", FD_STR }, { 0, 0 } };
FDEF argsGetField[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Field", FD_LARGE }, { "Result", FD_PTR }, { 0, 0 } };
FDEF argsGetFieldArray[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Field", FD_LARGE }, { "Result", FD_PTR|FD_RESULT }, { "Elements", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsGetFieldVariable[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Field", FD_STR }, { "Buffer", FD_BUFFER|FD_STR }, { "Size", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF argsGetFields[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Tags", FD_VARTAGS }, { 0, 0 } };
FDEF argsGetMemAddress[] = { { "Result", FD_PTR }, { "ID", FD_LONG }, { 0, 0 } };
FDEF argsGetMessage[] = { { "Error", FD_LONG|FD_ERROR }, { "Queue", FD_LONG }, { "Type", FD_LONG }, { "Flags", FD_LONG }, { "Buffer", FD_BUFFER|FD_PTR }, { "Size", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF argsGetMsgPort[] = { { "Result", FD_LONG }, { "Object", FD_OBJECTID }, { 0, 0 } };
FDEF argsGetName[] = { { "Result", FD_STR }, { "Object", FD_OBJECTPTR }, { 0, 0 } };
FDEF argsGetObjectPtr[] = { { "Object", FD_OBJECTPTR }, { "Object", FD_OBJECTID }, { 0, 0 } };
FDEF argsGetOwnerID[] = { { "Result", FD_OBJECTID }, { "Object", FD_OBJECTID }, { 0, 0 } };
FDEF argsGetResource[] = { { "Result", FD_LARGE }, { "Resource", FD_LONG }, { 0, 0 } };
FDEF argsGetSystemState[] = { { "SystemState", FD_PTR|FD_STRUCT }, { 0, 0 } };
FDEF argsIdentifyFile[] = { { "Error", FD_LONG|FD_ERROR }, { "Path", FD_STR }, { "Mode", FD_STR }, { "Flags", FD_LONG }, { "Class", FD_LONG|FD_UNSIGNED|FD_RESULT }, { "SubClass", FD_LONG|FD_UNSIGNED|FD_RESULT }, { "Command", FD_STR|FD_ALLOC|FD_RESULT }, { 0, 0 } };
FDEF argsKeyGet[] = { { "Error", FD_LONG|FD_ERROR }, { "KeyStore:Store", FD_PTR|FD_STRUCT|FD_RESOURCE }, { "Key", FD_LONG|FD_UNSIGNED }, { "Data", FD_PTR|FD_RESULT }, { "Size", FD_LONG|FD_BUFSIZE|FD_RESULT }, { 0, 0 } };
FDEF argsKeyIterate[] = { { "Error", FD_LONG|FD_ERROR }, { "KeyStore:Store", FD_PTR|FD_STRUCT|FD_RESOURCE }, { "Index", FD_LONG|FD_UNSIGNED }, { "Key", FD_LONG|FD_UNSIGNED|FD_RESULT }, { "Data", FD_PTR|FD_RESULT }, { "Size", FD_LONG|FD_BUFSIZE|FD_RESULT }, { 0, 0 } };
FDEF argsKeySet[] = { { "Error", FD_LONG|FD_ERROR }, { "KeyStore:Store", FD_PTR|FD_STRUCT|FD_RESOURCE }, { "Key", FD_LONG|FD_UNSIGNED }, { "Data", FD_BUFFER|FD_PTR }, { "Size", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF argsListChildren[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTID }, { "IncludeShared", FD_LONG }, { "ChildEntry:List", FD_BUFFER|FD_ARRAY|FD_STRUCT|FD_RESOURCE }, { "Count", FD_LONG|FD_ARRAYSIZE|FD_RESULT }, { 0, 0 } };
FDEF argsListTasks[] = { { "Error", FD_LONG|FD_ERROR }, { "Flags", FD_LONG }, { "ListTasks:List", FD_PTR|FD_STRUCT|FD_RESULT }, { 0, 0 } };
FDEF argsLoadFile[] = { { "Error", FD_LONG|FD_ERROR }, { "Path", FD_STR }, { "Flags", FD_LONG }, { "CacheFile:Cache", FD_PTR|FD_STRUCT|FD_RESOURCE|FD_RESULT }, { 0, 0 } };
FDEF argsLockMutex[] = { { "Error", FD_LONG|FD_ERROR }, { "Mutex", FD_PTR }, { "MilliSeconds", FD_LONG }, { 0, 0 } };
FDEF argsLockSharedMutex[] = { { "Error", FD_LONG|FD_ERROR }, { "Mutex", FD_PTR }, { "MilliSeconds", FD_LONG }, { 0, 0 } };
FDEF argsLogF[] = { { "Void", FD_VOID }, { "Header", FD_STR }, { "Message", FD_STR }, { "Tags", FD_TAGS }, { 0, 0 } };
FDEF argsLogReturn[] = { { "Void", FD_VOID }, { 0, 0 } };
FDEF argsManageAction[] = { { "Error", FD_LONG|FD_ERROR }, { "Action", FD_LONG }, { "Routine", FD_PTR }, { 0, 0 } };
FDEF argsMemoryIDInfo[] = { { "Error", FD_LONG|FD_ERROR }, { "ID", FD_LONG }, { "MemInfo:MemInfo", FD_BUFFER|FD_PTR|FD_STRUCT }, { "Size", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF argsMemoryPtrInfo[] = { { "Error", FD_LONG|FD_ERROR }, { "Address", FD_PTR }, { "MemInfo:MemInfo", FD_BUFFER|FD_PTR|FD_STRUCT }, { "Size", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF argsMoveFile[] = { { "Error", FD_LONG|FD_ERROR }, { "Source", FD_STR }, { "Dest", FD_STR }, { "Callback", FD_FUNCTIONPTR }, { 0, 0 } };
FDEF argsNewLockedObject[] = { { "Error", FD_LONG|FD_ERROR }, { "ClassID", FD_LARGE }, { "Flags", FD_LONG }, { "Object", FD_OBJECTPTR|FD_RESULT }, { "ID", FD_OBJECTID|FD_RESULT }, { "Name", FD_STR }, { 0, 0 } };
FDEF argsNewObject[] = { { "Error", FD_LONG|FD_ERROR }, { "ClassID", FD_LARGE }, { "Flags", FD_LONG }, { "Object", FD_OBJECTPTR|FD_RESULT }, { 0, 0 } };
FDEF argsNotifySubscribers[] = { { "Result", FD_LONG }, { "Object", FD_OBJECTPTR }, { "Action", FD_LONG }, { "Args", FD_PTR }, { "Flags", FD_LONG }, { "Error", FD_LONG|FD_ERROR }, { 0, 0 } };
FDEF argsOpenDir[] = { { "Error", FD_LONG|FD_ERROR }, { "Path", FD_STR }, { "Flags", FD_LONG }, { "DirInfo:Info", FD_PTR|FD_STRUCT|FD_RESOURCE|FD_ALLOC|FD_RESULT }, { 0, 0 } };
FDEF argsPreciseTime[] = { { "Result", FD_LARGE }, { 0, 0 } };
FDEF argsPrintDiagnosis[] = { { "Void", FD_VOID }, { "Process", FD_LONG }, { "Signal", FD_LONG }, { 0, 0 } };
FDEF argsProcessMessages[] = { { "Error", FD_LONG|FD_ERROR }, { "Flags", FD_LONG }, { "TimeOut", FD_LONG }, { 0, 0 } };
FDEF argsRandomNumber[] = { { "Result", FD_LONG }, { "Range", FD_LONG }, { 0, 0 } };
FDEF argsReadFileToBuffer[] = { { "Error", FD_LONG|FD_ERROR }, { "Path", FD_STR }, { "Buffer", FD_BUFFER|FD_PTR }, { "BufferSize", FD_LONG|FD_BUFSIZE }, { "Result", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsReallocMemory[] = { { "Error", FD_LONG|FD_ERROR }, { "Memory", FD_PTR }, { "Size", FD_LONG }, { "Address", FD_PTR|FD_ALLOC|FD_RESULT }, { "ID", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsRegisterFD[] = { { "Error", FD_LONG|FD_ERROR }, { "FD", FD_PTR }, { "Flags", FD_LONG }, { "Routine", FD_PTR }, { "Data", FD_PTR }, { 0, 0 } };
FDEF argsReleaseMemory[] = { { "Result", FD_LONG }, { "Address", FD_PTR }, { 0, 0 } };
FDEF argsReleaseMemoryID[] = { { "Error", FD_LONG|FD_ERROR }, { "MemoryID", FD_LONG }, { 0, 0 } };
FDEF argsReleaseObject[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { 0, 0 } };
FDEF argsReleasePrivateObject[] = { { "Void", FD_VOID }, { "Object", FD_OBJECTPTR }, { 0, 0 } };
FDEF argsResolveClassID[] = { { "Result", FD_STR }, { "ID", FD_LONG|FD_UNSIGNED }, { 0, 0 } };
FDEF argsResolveClassName[] = { { "Result", FD_LONG|FD_UNSIGNED }, { "Name", FD_STR }, { 0, 0 } };
FDEF argsResolveGroupID[] = { { "Result", FD_STR }, { "Group", FD_LONG }, { 0, 0 } };
FDEF argsResolvePath[] = { { "Error", FD_LONG|FD_ERROR }, { "Path", FD_STR }, { "Flags", FD_LONG }, { "Result", FD_STR|FD_ALLOC|FD_RESULT }, { 0, 0 } };
FDEF argsResolveUserID[] = { { "Result", FD_STR }, { "User", FD_LONG }, { 0, 0 } };
FDEF argsSaveImageToFile[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Path", FD_STR }, { "Class", FD_LONG|FD_UNSIGNED }, { "Permissions", FD_LONG }, { 0, 0 } };
FDEF argsSaveObjectToFile[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Path", FD_STR }, { "Permissions", FD_LONG }, { 0, 0 } };
FDEF argsScanDir[] = { { "Error", FD_LONG|FD_ERROR }, { "DirInfo:Info", FD_PTR|FD_STRUCT|FD_RESOURCE }, { 0, 0 } };
FDEF argsScanMessages[] = { { "Error", FD_LONG|FD_ERROR }, { "Queue", FD_PTR }, { "Index", FD_LONG|FD_RESULT }, { "Type", FD_LONG }, { "Buffer", FD_BUFFER|FD_PTR }, { "Size", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF argsSendMessage[] = { { "Error", FD_LONG|FD_ERROR }, { "Queue", FD_LONG }, { "Type", FD_LONG }, { "Flags", FD_LONG }, { "Data", FD_BUFFER|FD_PTR }, { "Size", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF argsSetArray[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Field", FD_LARGE }, { "Array", FD_PTR }, { "Elements", FD_LONG }, { 0, 0 } };
FDEF argsSetContext[] = { { "Object", FD_OBJECTPTR }, { "Object", FD_OBJECTPTR }, { 0, 0 } };
FDEF argsSetDefaultPermissions[] = { { "Void", FD_VOID }, { "User", FD_LONG }, { "Group", FD_LONG }, { "Permissions", FD_LONG }, { 0, 0 } };
FDEF argsSetField[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Field", FD_LARGE }, { "Value", FD_VARTAGS }, { 0, 0 } };
FDEF argsSetFieldEval[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Field", FD_STR }, { "Value", FD_STR }, { 0, 0 } };
FDEF argsSetFields[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Tags", FD_VARTAGS }, { 0, 0 } };
FDEF argsSetFieldsID[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTID }, { "Tags", FD_VARTAGS }, { 0, 0 } };
FDEF argsSetName[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Name", FD_STR }, { 0, 0 } };
FDEF argsSetOwner[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Owner", FD_OBJECTPTR }, { 0, 0 } };
FDEF argsSetResource[] = { { "Result", FD_LARGE }, { "Resource", FD_LONG }, { "Value", FD_LARGE }, { 0, 0 } };
FDEF argsSetResourcePath[] = { { "Error", FD_LONG|FD_ERROR }, { "PathType", FD_LONG }, { "Path", FD_STR }, { 0, 0 } };
FDEF argsSetVolume[] = { { "Error", FD_LONG|FD_ERROR }, { "Tags", FD_VARTAGS }, { 0, 0 } };
FDEF argsStrBuildArray[] = { { "Result", FD_ARRAY|FD_STR|FD_ALLOC }, { "List", FD_STR }, { "Size", FD_LONG }, { "Total", FD_LONG }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF argsStrClone[] = { { "Result", FD_STR }, { "String", FD_STR }, { 0, 0 } };
FDEF argsStrCompare[] = { { "Error", FD_LONG|FD_ERROR }, { "String1", FD_STR }, { "String2", FD_STR }, { "Length", FD_LONG }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF argsStrCopy[] = { { "Result", FD_LONG }, { "Src", FD_STR }, { "Dest", FD_STR }, { "Length", FD_LONG }, { 0, 0 } };
FDEF argsStrDatatype[] = { { "Result", FD_LONG }, { "String", FD_STR }, { 0, 0 } };
FDEF argsStrFormat[] = { { "Result", FD_LONG }, { "Buffer", FD_BUFFER|FD_STR }, { "Size", FD_LONG|FD_BUFSIZE }, { "Format", FD_STR }, { "Parameters", FD_TAGS }, { 0, 0 } };
FDEF argsStrHash[] = { { "Result", FD_LONG|FD_UNSIGNED }, { "String", FD_STR }, { "CaseSensitive", FD_LONG }, { 0, 0 } };
FDEF argsStrReadLocale[] = { { "Error", FD_LONG|FD_ERROR }, { "Key", FD_STR }, { "Value", FD_STR|FD_RESULT }, { 0, 0 } };
FDEF argsStrSearch[] = { { "Result", FD_LONG }, { "Keyword", FD_STR }, { "String", FD_STR }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF argsStrSortCompare[] = { { "Result", FD_LONG }, { "String1", FD_STR }, { "String2", FD_STR }, { 0, 0 } };
FDEF argsSubscribeAction[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Action", FD_LONG }, { 0, 0 } };
FDEF argsSubscribeActionTags[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Actions", FD_TAGS }, { 0, 0 } };
FDEF argsSubscribeEvent[] = { { "Error", FD_LONG|FD_ERROR }, { "Event", FD_LARGE }, { "Callback", FD_FUNCTIONPTR }, { "Custom", FD_PTR }, { "Handle", FD_PTR|FD_RESULT }, { 0, 0 } };
FDEF argsSubscribeTimer[] = { { "Error", FD_LONG|FD_ERROR }, { "Interval", FD_DOUBLE }, { "Callback", FD_FUNCTIONPTR }, { "Subscription", FD_PTR|FD_RESULT }, { 0, 0 } };
FDEF argsSysLock[] = { { "Error", FD_LONG|FD_ERROR }, { "Index", FD_LONG }, { "MilliSeconds", FD_LONG }, { 0, 0 } };
FDEF argsSysUnlock[] = { { "Error", FD_LONG|FD_ERROR }, { "Index", FD_LONG }, { 0, 0 } };
FDEF argsTranslateCmdRef[] = { { "Error", FD_LONG|FD_ERROR }, { "String", FD_STR }, { "Command", FD_STR|FD_ALLOC|FD_RESULT }, { 0, 0 } };
FDEF argsUTF8CharLength[] = { { "Result", FD_LONG }, { "String", FD_STR }, { 0, 0 } };
FDEF argsUTF8CharOffset[] = { { "Result", FD_LONG }, { "String", FD_STR }, { "Offset", FD_LONG }, { 0, 0 } };
FDEF argsUTF8Copy[] = { { "Result", FD_LONG }, { "Src", FD_STR }, { "Dest", FD_STR }, { "Chars", FD_LONG }, { "Size", FD_LONG }, { 0, 0 } };
FDEF argsUTF8Length[] = { { "Result", FD_LONG }, { "String", FD_STR }, { 0, 0 } };
FDEF argsUTF8OffsetToChar[] = { { "Result", FD_LONG }, { "String", FD_STR }, { "Offset", FD_LONG }, { 0, 0 } };
FDEF argsUTF8PrevLength[] = { { "Result", FD_LONG }, { "String", FD_STR }, { "Offset", FD_LONG }, { 0, 0 } };
FDEF argsUTF8ReadValue[] = { { "Result", FD_LONG|FD_UNSIGNED }, { "String", FD_STR }, { "Length", FD_LONG|FD_RESULT }, { 0, 0 } };
FDEF argsUTF8ValidEncoding[] = { { "Result", FD_STR }, { "String", FD_STR }, { "Encoding", FD_STR }, { 0, 0 } };
FDEF argsUTF8WriteValue[] = { { "Result", FD_LONG }, { "Value", FD_LONG }, { "Buffer", FD_BUFFER|FD_STR }, { "Size", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF argsUnloadFile[] = { { "Void", FD_VOID }, { "CacheFile:Cache", FD_PTR|FD_STRUCT|FD_RESOURCE }, { 0, 0 } };
FDEF argsUnlockMutex[] = { { "Void", FD_VOID }, { "Mutex", FD_PTR }, { 0, 0 } };
FDEF argsUnlockSharedMutex[] = { { "Void", FD_VOID }, { "Mutex", FD_PTR }, { 0, 0 } };
FDEF argsUnsubscribeAction[] = { { "Error", FD_LONG|FD_ERROR }, { "Object", FD_OBJECTPTR }, { "Action", FD_LONG }, { 0, 0 } };
FDEF argsUnsubscribeEvent[] = { { "Void", FD_VOID }, { "Event", FD_PTR }, { 0, 0 } };
FDEF argsUpdateMessage[] = { { "Error", FD_LONG|FD_ERROR }, { "Queue", FD_PTR }, { "Message", FD_LONG }, { "Type", FD_LONG }, { "Data", FD_BUFFER|FD_PTR }, { "Size", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF argsUpdateTimer[] = { { "Error", FD_LONG|FD_ERROR }, { "Subscription", FD_PTR }, { "Interval", FD_DOUBLE }, { 0, 0 } };
FDEF argsVLogF[] = { { "Void", FD_VOID }, { "Flags", FD_LONG }, { "Header", FD_STR }, { "Message", FD_STR }, { "Args", FD_PTR }, { 0, 0 } };
FDEF argsVarCopy[] = { { "Error", FD_LONG|FD_ERROR }, { "KeyStore:Source", FD_PTR|FD_STRUCT|FD_RESOURCE }, { "KeyStore:Dest", FD_PTR|FD_STRUCT|FD_RESOURCE }, { 0, 0 } };
FDEF argsVarGet[] = { { "Error", FD_LONG|FD_ERROR }, { "KeyStore:Store", FD_PTR|FD_STRUCT|FD_RESOURCE }, { "Name", FD_STR }, { "Data", FD_PTR|FD_RESULT }, { "Size", FD_LONG|FD_BUFSIZE|FD_RESULT }, { 0, 0 } };
FDEF argsVarGetString[] = { { "Result", FD_STR }, { "KeyStore:Store", FD_PTR|FD_STRUCT|FD_RESOURCE }, { "Key", FD_STR }, { 0, 0 } };
FDEF argsVarIterate[] = { { "Error", FD_LONG|FD_ERROR }, { "KeyStore:Store", FD_PTR|FD_STRUCT|FD_RESOURCE }, { "Index", FD_STR }, { "Key", FD_STR|FD_RESULT }, { "Data", FD_PTR|FD_RESULT }, { "Size", FD_LONG|FD_BUFSIZE|FD_RESULT }, { 0, 0 } };
FDEF argsVarLock[] = { { "Error", FD_LONG|FD_ERROR }, { "KeyStore:Store", FD_PTR|FD_STRUCT|FD_RESOURCE }, { "Timeout", FD_LONG }, { 0, 0 } };
FDEF argsVarNew[] = { { "KeyStore", FD_PTR|FD_STRUCT|FD_RESOURCE }, { "InitialSize", FD_LONG }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF argsVarSet[] = { { "Result", FD_PTR }, { "KeyStore:Store", FD_PTR|FD_STRUCT|FD_RESOURCE }, { "Key", FD_STR }, { "Data", FD_BUFFER|FD_PTR }, { "Size", FD_LONG|FD_BUFSIZE }, { 0, 0 } };
FDEF argsVarSetSized[] = { { "Error", FD_LONG|FD_ERROR }, { "KeyStore:Store", FD_PTR|FD_STRUCT|FD_RESOURCE }, { "Key", FD_STR }, { "Size", FD_LONG }, { "Data", FD_PTR|FD_RESULT }, { "DataSize", FD_LONG|FD_BUFSIZE|FD_RESULT }, { 0, 0 } };
FDEF argsVarSetString[] = { { "Error", FD_LONG|FD_ERROR }, { "KeyStore:Store", FD_PTR|FD_STRUCT|FD_RESOURCE }, { "Key", FD_STR }, { "Value", FD_STR }, { 0, 0 } };
FDEF argsVarUnlock[] = { { "Void", FD_VOID }, { "KeyStore:Store", FD_PTR|FD_STRUCT|FD_RESOURCE }, { 0, 0 } };
FDEF argsVirtualVolume[] = { { "Error", FD_LONG|FD_ERROR }, { "Name", FD_STR }, { "Tags", FD_TAGS }, { 0, 0 } };
FDEF argsWaitForObjects[] = { { "Error", FD_LONG|FD_ERROR }, { "Flags", FD_LONG }, { "TimeOut", FD_LONG }, { "ObjectSignal:ObjectSignals", FD_PTR|FD_STRUCT }, { 0, 0 } };
FDEF argsWaitTime[] = { { "Void", FD_VOID }, { "Seconds", FD_LONG }, { "MicroSeconds", FD_LONG }, { 0, 0 } };
FDEF argsWakeProcess[] = { { "Error", FD_LONG|FD_ERROR }, { "ProcessID", FD_LONG }, { 0, 0 } };

const struct Function glFunctions[] = {
   { (APTR)AccessMemory, "AccessMemory", argsAccessMemory },
   { (APTR)Action, "Action", argsAction },
   { (APTR)ActionList, "ActionList", argsActionList },
   { (APTR)ActionMsg, "ActionMsg", argsActionMsg },
   { (APTR)ActionTags, "ActionTags", argsActionTags },
   { (APTR)ResolveClassID, "ResolveClassID", argsResolveClassID },
   { (APTR)AllocateID, "AllocateID", argsAllocateID },
   { (APTR)AllocMemory, "AllocMemory", argsAllocMemory },
   { (APTR)AccessObject, "AccessObject", argsAccessObject },
   { (APTR)ListTasks, "ListTasks", argsListTasks },
   { (APTR)CheckAction, "CheckAction", argsCheckAction },
   { (APTR)CheckMemoryExists, "CheckMemoryExists", argsCheckMemoryExists },
   { (APTR)CheckObjectExists, "CheckObjectExists", argsCheckObjectExists },
   { (APTR)CloneMemory, "CloneMemory", argsCloneMemory },
   { (APTR)CreateObject, "CreateObject", argsCreateObject },
   { (APTR)CurrentContext, "CurrentContext", argsCurrentContext },
   { (APTR)GetFieldArray, "GetFieldArray", argsGetFieldArray },
   { (APTR)AdjustLogLevel, "AdjustLogLevel", argsAdjustLogLevel },
   { (APTR)LogF, "LogF", argsLogF },
   { (APTR)FindObject, "FindObject", argsFindObject },
   { (APTR)FindClass, "FindClass", argsFindClass },
   { (APTR)ReleaseObject, "ReleaseObject", argsReleaseObject },
   { (APTR)FreeResource, "FreeResource", argsFreeResource },
   { (APTR)FreeResourceID, "FreeResourceID", argsFreeResourceID },
   { (APTR)GetClassID, "GetClassID", argsGetClassID },
   { (APTR)GetOwnerID, "GetOwnerID", argsGetOwnerID },
   { (APTR)GetField, "GetField", argsGetField },
   { (APTR)GetFieldVariable, "GetFieldVariable", argsGetFieldVariable },
   { (APTR)GetFields, "GetFields", argsGetFields },
   { (APTR)GetName, "GetName", argsGetName },
   { (APTR)ListChildren, "ListChildren", argsListChildren },
   { (APTR)Base64Decode, "Base64Decode", argsBase64Decode },
   { (APTR)RegisterFD, "RegisterFD", argsRegisterFD },
   { (APTR)ManageAction, "ManageAction", argsManageAction },
   { (APTR)MemoryIDInfo, "MemoryIDInfo", argsMemoryIDInfo },
   { (APTR)MemoryPtrInfo, "MemoryPtrInfo", argsMemoryPtrInfo },
   { (APTR)NewObject, "NewObject", argsNewObject },
   { (APTR)NotifySubscribers, "NotifySubscribers", argsNotifySubscribers },
   { (APTR)StrReadLocale, "StrReadLocale", argsStrReadLocale },
   { (APTR)GetMemAddress, "GetMemAddress", argsGetMemAddress },
   { (APTR)ProcessMessages, "ProcessMessages", argsProcessMessages },
   { (APTR)RandomNumber, "RandomNumber", argsRandomNumber },
   { (APTR)ReallocMemory, "ReallocMemory", argsReallocMemory },
   { (APTR)GetMessage, "GetMessage", argsGetMessage },
   { (APTR)ReleaseMemory, "ReleaseMemory", argsReleaseMemory },
   { (APTR)ResolveClassName, "ResolveClassName", argsResolveClassName },
   { (APTR)KeySet, "KeySet", argsKeySet },
   { (APTR)SendMessage, "SendMessage", argsSendMessage },
   { (APTR)SetOwner, "SetOwner", argsSetOwner },
   { (APTR)SetContext, "SetContext", argsSetContext },
   { (APTR)SetField, "SetField", argsSetField },
   { (APTR)SetFields, "SetFields", argsSetFields },
   { (APTR)SetFieldEval, "SetFieldEval", argsSetFieldEval },
   { (APTR)SetName, "SetName", argsSetName },
   { (APTR)LogReturn, "LogReturn", argsLogReturn },
   { (APTR)StrCompare, "StrCompare", argsStrCompare },
   { (APTR)SubscribeAction, "SubscribeAction", argsSubscribeAction },
   { (APTR)VarGet, "VarGet", argsVarGet },
   { (APTR)SubscribeEvent, "SubscribeEvent", argsSubscribeEvent },
   { (APTR)SubscribeTimer, "SubscribeTimer", argsSubscribeTimer },
   { (APTR)UpdateTimer, "UpdateTimer", argsUpdateTimer },
   { (APTR)UnsubscribeAction, "UnsubscribeAction", argsUnsubscribeAction },
   { (APTR)VarSet, "VarSet", argsVarSet },
   { (APTR)UnsubscribeEvent, "UnsubscribeEvent", argsUnsubscribeEvent },
   { (APTR)BroadcastEvent, "BroadcastEvent", argsBroadcastEvent },
   { (APTR)WaitTime, "WaitTime", argsWaitTime },
   { (APTR)GetEventID, "GetEventID", argsGetEventID },
   { (APTR)GenCRC32, "GenCRC32", argsGenCRC32 },
   { (APTR)GetResource, "GetResource", argsGetResource },
   { (APTR)SetResource, "SetResource", argsSetResource },
   { (APTR)ScanMessages, "ScanMessages", argsScanMessages },
   { (APTR)SysLock, "SysLock", argsSysLock },
   { (APTR)SysUnlock, "SysUnlock", argsSysUnlock },
   { (APTR)CopyMemory, "CopyMemory", argsCopyMemory },
   { (APTR)LoadFile, "LoadFile", argsLoadFile },
   { (APTR)SubscribeActionTags, "SubscribeActionTags", argsSubscribeActionTags },
   { (APTR)PrintDiagnosis, "PrintDiagnosis", argsPrintDiagnosis },
   { (APTR)NewLockedObject, "NewLockedObject", argsNewLockedObject },
   { (APTR)UpdateMessage, "UpdateMessage", argsUpdateMessage },
   { (APTR)AddMsgHandler, "AddMsgHandler", argsAddMsgHandler },
   { (APTR)FindPrivateObject, "FindPrivateObject", argsFindPrivateObject },
   { (APTR)PreciseTime, "PreciseTime", argsPreciseTime },
   { (APTR)SetFieldsID, "SetFieldsID", argsSetFieldsID },
   { (APTR)GetObjectPtr, "GetObjectPtr", argsGetObjectPtr },
   { (APTR)FindField, "FindField", argsFindField },
   { (APTR)GetMsgPort, "GetMsgPort", argsGetMsgPort },
   { (APTR)GetErrorMsg, "GetErrorMsg", argsGetErrorMsg },
   { (APTR)GetActionMsg, "GetActionMsg", argsGetActionMsg },
   { (APTR)FuncError, "FuncError", argsFuncError },
   { (APTR)SetArray, "SetArray", argsSetArray },
   { (APTR)ReleaseMemoryID, "ReleaseMemoryID", argsReleaseMemoryID },
   { (APTR)AccessPrivateObject, "AccessPrivateObject", argsAccessPrivateObject },
   { (APTR)ReleasePrivateObject, "ReleasePrivateObject", argsReleasePrivateObject },
   { (APTR)AllocMutex, "AllocMutex", argsAllocMutex },
   { (APTR)FreeMutex, "FreeMutex", argsFreeMutex },
   { (APTR)LockMutex, "LockMutex", argsLockMutex },
   { (APTR)UnlockMutex, "UnlockMutex", argsUnlockMutex },
   { (APTR)ActionThread, "ActionThread", argsActionThread },
   { (APTR)VarNew, "VarNew", argsVarNew },
   { (APTR)AllocSharedMutex, "AllocSharedMutex", argsAllocSharedMutex },
   { (APTR)FreeSharedMutex, "FreeSharedMutex", argsFreeSharedMutex },
   { (APTR)LockSharedMutex, "LockSharedMutex", argsLockSharedMutex },
   { (APTR)UnlockSharedMutex, "UnlockSharedMutex", argsUnlockSharedMutex },
   { (APTR)VLogF, "VLogF", argsVLogF },
   { (APTR)StrSearch, "StrSearch", argsStrSearch },
   { (APTR)VarSetSized, "VarSetSized", argsVarSetSized },
   { (APTR)VarLock, "VarLock", argsVarLock },
   { (APTR)WakeProcess, "WakeProcess", argsWakeProcess },
   { (APTR)SetResourcePath, "SetResourcePath", argsSetResourcePath },
   { (APTR)CurrentTask, "CurrentTask", argsCurrentTask },
   { (APTR)KeyIterate, "KeyIterate", argsKeyIterate },
   { (APTR)ResolveGroupID, "ResolveGroupID", argsResolveGroupID },
   { (APTR)StrCopy, "StrCopy", argsStrCopy },
   { (APTR)StrClone, "StrClone", argsStrClone },
   { (APTR)VarUnlock, "VarUnlock", argsVarUnlock },
   { (APTR)ResolveUserID, "ResolveUserID", argsResolveUserID },
   { (APTR)CreateLink, "CreateLink", argsCreateLink },
   { (APTR)StrBuildArray, "StrBuildArray", argsStrBuildArray },
   { (APTR)UTF8CharOffset, "UTF8CharOffset", argsUTF8CharOffset },
   { (APTR)UTF8Length, "UTF8Length", argsUTF8Length },
   { (APTR)UTF8OffsetToChar, "UTF8OffsetToChar", argsUTF8OffsetToChar },
   { (APTR)UTF8PrevLength, "UTF8PrevLength", argsUTF8PrevLength },
   { (APTR)UTF8CharLength, "UTF8CharLength", argsUTF8CharLength },
   { (APTR)UTF8ReadValue, "UTF8ReadValue", argsUTF8ReadValue },
   { (APTR)UTF8WriteValue, "UTF8WriteValue", argsUTF8WriteValue },
   { (APTR)StrFormat, "StrFormat", argsStrFormat },
   { (APTR)SaveImageToFile, "SaveImageToFile", argsSaveImageToFile },
   { (APTR)ReadFileToBuffer, "ReadFileToBuffer", argsReadFileToBuffer },
   { (APTR)StrDatatype, "StrDatatype", argsStrDatatype },
   { (APTR)UnloadFile, "UnloadFile", argsUnloadFile },
   { (APTR)SetDefaultPermissions, "SetDefaultPermissions", argsSetDefaultPermissions },
   { (APTR)CompareFilePaths, "CompareFilePaths", argsCompareFilePaths },
   { (APTR)GetSystemState, "GetSystemState", argsGetSystemState },
   { (APTR)StrSortCompare, "StrSortCompare", argsStrSortCompare },
   { (APTR)AddInfoTag, "AddInfoTag", argsAddInfoTag },
   { (APTR)UTF8Copy, "UTF8Copy", argsUTF8Copy },
   { (APTR)Base64Encode, "Base64Encode", argsBase64Encode },
   { (APTR)VarSetString, "VarSetString", argsVarSetString },
   { (APTR)VarGetString, "VarGetString", argsVarGetString },
   { (APTR)VarCopy, "VarCopy", argsVarCopy },
   { (APTR)StrHash, "StrHash", argsStrHash },
   { (APTR)UTF8ValidEncoding, "UTF8ValidEncoding", argsUTF8ValidEncoding },
   { (APTR)AnalysePath, "AnalysePath", argsAnalysePath },
   { (APTR)CreateFolder, "CreateFolder", argsCreateFolder },
   { (APTR)MoveFile, "MoveFile", argsMoveFile },
   { (APTR)ResolvePath, "ResolvePath", argsResolvePath },
   { (APTR)SetVolume, "SetVolume", argsSetVolume },
   { (APTR)DeleteVolume, "DeleteVolume", argsDeleteVolume },
   { (APTR)VirtualVolume, "VirtualVolume", argsVirtualVolume },
   { (APTR)CopyFile, "CopyFile", argsCopyFile },
   { (APTR)KeyGet, "KeyGet", argsKeyGet },
   { (APTR)VarIterate, "VarIterate", argsVarIterate },
   { (APTR)DeleteFile, "DeleteFile", argsDeleteFile },
   { (APTR)WaitForObjects, "WaitForObjects", argsWaitForObjects },
   { (APTR)SaveObjectToFile, "SaveObjectToFile", argsSaveObjectToFile },
   { (APTR)OpenDir, "OpenDir", argsOpenDir },
   { (APTR)ScanDir, "ScanDir", argsScanDir },
   { (APTR)IdentifyFile, "IdentifyFile", argsIdentifyFile },
   { (APTR)TranslateCmdRef, "TranslateCmdRef", argsTranslateCmdRef },
   { NULL, NULL, NULL }
};

