// Auto-generated by idl-c.fluid

#ifdef  __cplusplus
extern "C" {
#endif

ERROR AccessMemory(MEMORYID Memory, LONG Flags, LONG MilliSeconds, APTR * Result);
ERROR Action(LONG Action, OBJECTPTR Object, APTR Parameters);
void ActionList(struct ActionTable ** Actions, LONG * Size);
ERROR ActionMsg(LONG Action, OBJECTID Object, APTR Args, MEMORYID MessageID, CLASSID ClassID);
ERROR ActionTags(LONG Action, OBJECTPTR Object, ...);
CSTRING ResolveClassID(CLASSID ID);
LONG AllocateID(LONG Type);
ERROR AllocMemory(LONG Size, LONG Flags, APTR * Address, MEMORYID * ID);
ERROR AccessObject(OBJECTID Object, LONG MilliSeconds, OBJECTPTR * Result);
ERROR ListTasks(LONG Flags, struct ListTasks ** List);
ERROR CheckAction(OBJECTPTR Object, LONG Action);
ERROR CheckMemoryExists(MEMORYID ID);
ERROR CheckObjectExists(OBJECTID Object);
ERROR CloneMemory(APTR Address, LONG Flags, APTR * NewAddress, MEMORYID * NewID);
ERROR CreateObject(LARGE ClassID, LONG Flags, OBJECTPTR * Object, ...);
OBJECTPTR CurrentContext();
ERROR GetFieldArray(OBJECTPTR Object, FIELD Field, APTR * Result, LONG * Elements);
LONG AdjustLogLevel(LONG Adjust);
void LogF(CSTRING Header, CSTRING Message, ...);
ERROR FindObject(CSTRING Name, CLASSID ClassID, LONG Flags, OBJECTID * Array, LONG * Count);
objMetaClass * FindClass(CLASSID ClassID);
ERROR ReleaseObject(OBJECTPTR Object);
ERROR FreeResource(const void * Address);
ERROR FreeResourceID(MEMORYID ID);
CLASSID GetClassID(OBJECTID Object);
OBJECTID GetOwnerID(OBJECTID Object);
ERROR GetField(OBJECTPTR Object, FIELD Field, APTR Result);
ERROR GetFieldVariable(OBJECTPTR Object, CSTRING Field, STRING Buffer, LONG Size);
ERROR GetFields(OBJECTPTR Object, ...);
CSTRING GetName(OBJECTPTR Object);
ERROR ListChildren(OBJECTID Object, LONG IncludeShared, struct ChildEntry * List, LONG * Count);
ERROR Base64Decode(struct rkBase64Decode * State, CSTRING Input, LONG InputSize, APTR Output, LONG * Written);
ERROR RegisterFD(HOSTHANDLE FD, LONG Flags, void (*Routine)(HOSTHANDLE, APTR) , APTR Data);
ERROR ManageAction(LONG Action, APTR Routine);
ERROR MemoryIDInfo(MEMORYID ID, struct MemInfo * MemInfo, LONG Size);
ERROR MemoryPtrInfo(APTR Address, struct MemInfo * MemInfo, LONG Size);
ERROR NewObject(LARGE ClassID, LONG Flags, OBJECTPTR * Object);
LONG NotifySubscribers(OBJECTPTR Object, LONG Action, APTR Args, LONG Flags, ERROR Error);
ERROR StrReadLocale(CSTRING Key, CSTRING * Value);
APTR GetMemAddress(MEMORYID ID);
ERROR ProcessMessages(LONG Flags, LONG TimeOut);
LONG RandomNumber(LONG Range);
ERROR ReallocMemory(APTR Memory, LONG Size, APTR * Address, MEMORYID * ID);
ERROR GetMessage(MEMORYID Queue, LONG Type, LONG Flags, APTR Buffer, LONG Size);
MEMORYID ReleaseMemory(APTR Address);
CLASSID ResolveClassName(CSTRING Name);
ERROR KeySet(struct KeyStore * Store, ULONG Key, const void * Data, LONG Size);
ERROR SendMessage(MEMORYID Queue, LONG Type, LONG Flags, APTR Data, LONG Size);
ERROR SetOwner(OBJECTPTR Object, OBJECTPTR Owner);
OBJECTPTR SetContext(OBJECTPTR Object);
ERROR SetField(OBJECTPTR Object, FIELD Field, ...);
ERROR SetFields(OBJECTPTR Object, ...);
ERROR SetFieldEval(OBJECTPTR Object, CSTRING Field, CSTRING Value);
ERROR SetName(OBJECTPTR Object, CSTRING Name);
void LogReturn();
ERROR StrCompare(CSTRING String1, CSTRING String2, LONG Length, LONG Flags);
ERROR SubscribeAction(OBJECTPTR Object, LONG Action);
ERROR VarGet(struct KeyStore * Store, CSTRING Name, APTR * Data, LONG * Size);
ERROR SubscribeEvent(LARGE Event, FUNCTION * Callback, APTR Custom, APTR * Handle);
ERROR SubscribeTimer(DOUBLE Interval, FUNCTION * Callback, APTR * Subscription);
ERROR UpdateTimer(APTR Subscription, DOUBLE Interval);
ERROR UnsubscribeAction(OBJECTPTR Object, LONG Action);
APTR VarSet(struct KeyStore * Store, CSTRING Key, APTR Data, LONG Size);
void UnsubscribeEvent(APTR Event);
ERROR BroadcastEvent(APTR Event, LONG EventSize);
void WaitTime(LONG Seconds, LONG MicroSeconds);
LARGE GetEventID(LONG Group, CSTRING SubGroup, CSTRING Event);
ULONG GenCRC32(ULONG CRC, APTR Data, ULONG Length);
LARGE GetResource(LONG Resource);
LARGE SetResource(LONG Resource, LARGE Value);
ERROR ScanMessages(APTR Queue, LONG * Index, LONG Type, APTR Buffer, LONG Size);
ERROR SysLock(LONG Index, LONG MilliSeconds);
ERROR SysUnlock(LONG Index);
ERROR CopyMemory(const void * Src, APTR Dest, LONG Size);
ERROR LoadFile(CSTRING Path, LONG Flags, struct CacheFile ** Cache);
ERROR SubscribeActionTags(OBJECTPTR Object, ...);
void PrintDiagnosis(LONG Process, LONG Signal);
ERROR NewLockedObject(LARGE ClassID, LONG Flags, OBJECTPTR * Object, OBJECTID * ID, CSTRING Name);
ERROR UpdateMessage(APTR Queue, LONG Message, LONG Type, APTR Data, LONG Size);
ERROR AddMsgHandler(APTR Custom, LONG MsgType, FUNCTION * Routine, struct MsgHandler ** Handle);
ERROR FindPrivateObject(CSTRING Name, OBJECTPTR * Object);
LARGE PreciseTime();
ERROR SetFieldsID(OBJECTID Object, ...);
OBJECTPTR GetObjectPtr(OBJECTID Object);
struct Field * FindField(OBJECTPTR Object, ULONG FieldID, OBJECTPTR * Source);
LONG GetMsgPort(OBJECTID Object);
CSTRING GetErrorMsg(ERROR Error);
struct Message * GetActionMsg();
ERROR FuncError(CSTRING Header, ERROR Error);
ERROR SetArray(OBJECTPTR Object, FIELD Field, APTR Array, LONG Elements);
ERROR ReleaseMemoryID(MEMORYID MemoryID);
ERROR AccessPrivateObject(OBJECTPTR Object, LONG MilliSeconds);
void ReleasePrivateObject(OBJECTPTR Object);
ERROR AllocMutex(LONG Flags, APTR * Result);
void FreeMutex(APTR Mutex);
ERROR LockMutex(APTR Mutex, LONG MilliSeconds);
void UnlockMutex(APTR Mutex);
ERROR ActionThread(LONG Action, OBJECTPTR Object, APTR Args, FUNCTION * Callback, LONG Key);
struct KeyStore * VarNew(LONG InitialSize, LONG Flags);
ERROR AllocSharedMutex(CSTRING Name, APTR * Mutex);
void FreeSharedMutex(APTR Mutex);
ERROR LockSharedMutex(APTR Mutex, LONG MilliSeconds);
void UnlockSharedMutex(APTR Mutex);
void VLogF(LONG Flags, CSTRING Header, CSTRING Message, va_list Args);
LONG StrSearch(CSTRING Keyword, CSTRING String, LONG Flags);
ERROR VarSetSized(struct KeyStore * Store, CSTRING Key, LONG Size, APTR * Data, LONG * DataSize);
ERROR VarLock(struct KeyStore * Store, LONG Timeout);
ERROR WakeProcess(LONG ProcessID);
ERROR SetResourcePath(LONG PathType, CSTRING Path);
OBJECTPTR CurrentTask();
ERROR KeyIterate(struct KeyStore * Store, ULONG Index, ULONG * Key, APTR * Data, LONG * Size);
DOUBLE StrToFloat(CSTRING String);
LONG StrCopy(CSTRING Src, STRING Dest, LONG Length);
STRING StrClone(CSTRING String);
LONG StrLength(CSTRING String);
LARGE StrToInt(CSTRING String);
ERROR StrSort(CSTRING * List, LONG Flags);
STRING * StrBuildArray(STRING List, LONG Size, LONG Total, LONG Flags);
LONG UTF8CharOffset(CSTRING String, LONG Offset);
LONG UTF8Length(CSTRING String);
LONG UTF8OffsetToChar(CSTRING String, LONG Offset);
LONG UTF8PrevLength(CSTRING String, LONG Offset);
LONG UTF8CharLength(CSTRING String);
ULONG UTF8ReadValue(CSTRING String, LONG * Length);
LONG UTF8WriteValue(LONG Value, STRING Buffer, LONG Size);
LONG StrFormat(STRING Buffer, LONG Size, CSTRING Format, ...);
ERROR SaveImageToFile(OBJECTPTR Object, CSTRING Path, CLASSID Class, LONG Permissions);
ERROR ReadFileToBuffer(CSTRING Path, APTR Buffer, LONG BufferSize, LONG * Result);
LONG StrDatatype(CSTRING String);
void UnloadFile(struct CacheFile * Cache);
LARGE StrToHex(CSTRING String);
ERROR CompareFilePaths(CSTRING PathA, CSTRING PathB);
const struct SystemState * GetSystemState();
LONG StrSortCompare(CSTRING String1, CSTRING String2);
ERROR AddInfoTag(struct FileInfo * Info, CSTRING Name, CSTRING Value);
LONG UTF8Copy(CSTRING Src, STRING Dest, LONG Chars, LONG Size);
LONG Base64Encode(const void * Input, LONG InputSize, STRING Output, LONG OutputSize);
ERROR VarSetString(struct KeyStore * Store, CSTRING Key, CSTRING Value);
CSTRING VarGetString(struct KeyStore * Store, CSTRING Key);
ERROR VarCopy(struct KeyStore * Source, struct KeyStore * Dest);
ULONG StrHash(CSTRING String, LONG CaseSensitive);
CSTRING UTF8ValidEncoding(CSTRING String, CSTRING Encoding);
ERROR AnalysePath(CSTRING Path, LONG * Type);
ERROR CreateFolder(CSTRING Path, LONG Permissions);
ERROR MoveFile(CSTRING Source, CSTRING Dest, FUNCTION * Callback);
ERROR ResolvePath(CSTRING Path, LONG Flags, STRING * Result);
ERROR SetVolume(LARGE,...);
ERROR DeleteVolume(CSTRING Name);
ERROR VirtualVolume(CSTRING Name, ...);
ERROR CopyFile(CSTRING Source, CSTRING Dest, FUNCTION * Callback);
ERROR KeyGet(struct KeyStore * Store, ULONG Key, APTR * Data, LONG * Size);
ERROR VarIterate(struct KeyStore * Store, CSTRING Index, CSTRING * Key, APTR * Data, LONG * Size);
ERROR DeleteFile(CSTRING Path, FUNCTION * Callback);
ERROR WaitForObjects(LONG Flags, LONG TimeOut, struct ObjectSignal * ObjectSignals);
ERROR SaveObjectToFile(OBJECTPTR Object, CSTRING Path, LONG Permissions);
ERROR OpenDir(CSTRING Path, LONG Flags, struct DirInfo ** Info);
ERROR ScanDir(struct DirInfo * Info);
ERROR IdentifyFile(CSTRING Path, CSTRING Mode, LONG Flags, CLASSID * Class, CLASSID * SubClass, STRING * Command);
ERROR TranslateCmdRef(CSTRING String, STRING * Command);
ERROR CreateLink(CSTRING From, CSTRING To);
void VarUnlock(struct KeyStore * Store);
void SetDefaultPermissions(LONG User, LONG Group, LONG Permissions);
CSTRING ResolveUserID(LONG User);
CSTRING ResolveGroupID(LONG Group);

#ifdef  __cplusplus
}
#endif
