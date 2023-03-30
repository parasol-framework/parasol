// Auto-generated by idl-c.fluid

#ifdef  __cplusplus
extern "C" {
#endif

ERROR AccessMemoryID(MEMORYID Memory, LONG Flags, LONG MilliSeconds, APTR * Result);
ERROR Action(LONG Action, OBJECTPTR Object, APTR Parameters);
void ActionList(struct ActionTable ** Actions, LONG * Size);
ERROR ActionMsg(LONG Action, OBJECTID Object, APTR Args);
CSTRING ResolveClassID(CLASSID ID);
LONG AllocateID(LONG Type);
ERROR AllocMemory(LONG Size, LONG Flags, APTR * Address, MEMORYID * ID);
ERROR AccessObjectID(OBJECTID Object, LONG MilliSeconds, OBJECTPTR * Result);
ERROR CheckAction(OBJECTPTR Object, LONG Action);
ERROR CheckMemoryExists(MEMORYID ID);
ERROR CheckObjectExists(OBJECTID Object);
ERROR DeleteFile(CSTRING Path, FUNCTION * Callback);
ERROR VirtualVolume(CSTRING Name, ...);
OBJECTPTR CurrentContext();
ERROR GetFieldArray(OBJECTPTR Object, FIELD Field, APTR * Result, LONG * Elements);
LONG AdjustLogLevel(LONG Adjust);
void LogF(CSTRING Header, CSTRING Message, ...);
ERROR FindObject(CSTRING Name, CLASSID ClassID, LONG Flags, OBJECTID * ObjectID);
objMetaClass * FindClass(CLASSID ClassID);
ERROR AnalysePath(CSTRING Path, LONG * Type);
LONG UTF8Copy(CSTRING Src, STRING Dest, LONG Chars, LONG Size);
ERROR FreeResource(MEMORYID ID);
CLASSID GetClassID(OBJECTID Object);
OBJECTID GetOwnerID(OBJECTID Object);
ERROR GetField(OBJECTPTR Object, FIELD Field, APTR Result);
ERROR GetFieldVariable(OBJECTPTR Object, CSTRING Field, STRING Buffer, LONG Size);
LONG TotalChildren(OBJECTID Object);
CSTRING GetName(OBJECTPTR Object);
ERROR ListChildren(OBJECTID Object, pf::vector<ChildEntry> * List);
ERROR Base64Decode(struct pfBase64Decode * State, CSTRING Input, LONG InputSize, APTR Output, LONG * Written);
ERROR RegisterFD(HOSTHANDLE FD, LONG Flags, void (*Routine)(HOSTHANDLE, APTR) , APTR Data);
ERROR ResolvePath(CSTRING Path, LONG Flags, STRING * Result);
ERROR MemoryIDInfo(MEMORYID ID, struct MemInfo * MemInfo, LONG Size);
ERROR MemoryPtrInfo(APTR Address, struct MemInfo * MemInfo, LONG Size);
ERROR NewObject(LARGE ClassID, NF Flags, OBJECTPTR * Object);
void NotifySubscribers(OBJECTPTR Object, LONG Action, APTR Args, ERROR Error);
ERROR StrReadLocale(CSTRING Key, CSTRING * Value);
CSTRING UTF8ValidEncoding(CSTRING String, CSTRING Encoding);
ERROR ProcessMessages(LONG Flags, LONG TimeOut);
ERROR IdentifyFile(CSTRING Path, CLASSID * Class, CLASSID * SubClass);
ERROR ReallocMemory(APTR Memory, LONG Size, APTR * Address, MEMORYID * ID);
ERROR GetMessage(MEMORYID Queue, LONG Type, LONG Flags, APTR Buffer, LONG Size);
ERROR ReleaseMemory(MEMORYID MemoryID);
CLASSID ResolveClassName(CSTRING Name);
ERROR SendMessage(OBJECTID Task, LONG Type, LONG Flags, APTR Data, LONG Size);
ERROR SetOwner(OBJECTPTR Object, OBJECTPTR Owner);
OBJECTPTR SetContext(OBJECTPTR Object);
ERROR SetField(OBJECTPTR Object, FIELD Field, ...);
CSTRING FieldName(ULONG FieldID);
ERROR ScanDir(struct DirInfo * Info);
ERROR SetName(OBJECTPTR Object, CSTRING Name);
void LogReturn();
ERROR StrCompare(CSTRING String1, CSTRING String2, LONG Length, LONG Flags);
ERROR SubscribeAction(OBJECTPTR Object, LONG Action, FUNCTION * Callback);
ERROR SubscribeEvent(LARGE Event, FUNCTION * Callback, APTR Custom, APTR * Handle);
ERROR SubscribeTimer(DOUBLE Interval, FUNCTION * Callback, APTR * Subscription);
ERROR UpdateTimer(APTR Subscription, DOUBLE Interval);
ERROR UnsubscribeAction(OBJECTPTR Object, LONG Action);
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
ERROR CreateFolder(CSTRING Path, LONG Permissions);
ERROR LoadFile(CSTRING Path, LONG Flags, struct CacheFile ** Cache);
ERROR SetVolume(CSTRING Name, CSTRING Path, CSTRING Icon, CSTRING Label, CSTRING Device, LONG Flags);
ERROR DeleteVolume(CSTRING Name);
ERROR MoveFile(CSTRING Source, CSTRING Dest, FUNCTION * Callback);
ERROR UpdateMessage(APTR Queue, LONG Message, LONG Type, APTR Data, LONG Size);
ERROR AddMsgHandler(APTR Custom, LONG MsgType, FUNCTION * Routine, struct MsgHandler ** Handle);
ERROR QueueAction(LONG Action, OBJECTID Object, APTR Args);
LARGE PreciseTime();
ERROR OpenDir(CSTRING Path, LONG Flags, struct DirInfo ** Info);
OBJECTPTR GetObjectPtr(OBJECTID Object);
struct Field * FindField(OBJECTPTR Object, ULONG FieldID, OBJECTPTR * Target);
CSTRING GetErrorMsg(ERROR Error);
struct Message * GetActionMsg();
ERROR FuncError(CSTRING Header, ERROR Error);
ERROR SetArray(OBJECTPTR Object, FIELD Field, APTR Array, LONG Elements);
ULONG StrHash(CSTRING String, LONG CaseSensitive);
ERROR LockObject(OBJECTPTR Object, LONG MilliSeconds);
void ReleaseObject(OBJECTPTR Object);
ERROR AllocMutex(LONG Flags, APTR * Result);
void FreeMutex(APTR Mutex);
ERROR LockMutex(APTR Mutex, LONG MilliSeconds);
void UnlockMutex(APTR Mutex);
ERROR ActionThread(LONG Action, OBJECTPTR Object, APTR Args, FUNCTION * Callback, LONG Key);
ERROR AllocSharedMutex(CSTRING Name, APTR * Mutex);
void FreeSharedMutex(APTR Mutex);
ERROR LockSharedMutex(APTR Mutex, LONG MilliSeconds);
void UnlockSharedMutex(APTR Mutex);
void VLogF(LONG Flags, CSTRING Header, CSTRING Message, va_list Args);
LONG Base64Encode(struct pfBase64Encode * State, const void * Input, LONG InputSize, STRING Output, LONG OutputSize);
ERROR ReadInfoTag(struct FileInfo * Info, CSTRING Name, CSTRING * Value);
ERROR SetResourcePath(LONG PathType, CSTRING Path);
OBJECTPTR CurrentTask();
CSTRING ResolveGroupID(LONG Group);
CSTRING ResolveUserID(LONG User);
ERROR CreateLink(CSTRING From, CSTRING To);
STRING * StrBuildArray(STRING List, LONG Size, LONG Total, LONG Flags);
LONG UTF8CharOffset(CSTRING String, LONG Offset);
LONG UTF8Length(CSTRING String);
LONG UTF8OffsetToChar(CSTRING String, LONG Offset);
LONG UTF8PrevLength(CSTRING String, LONG Offset);
LONG UTF8CharLength(CSTRING String);
ULONG UTF8ReadValue(CSTRING String, LONG * Length);
LONG UTF8WriteValue(LONG Value, STRING Buffer, LONG Size);
ERROR CopyFile(CSTRING Source, CSTRING Dest, FUNCTION * Callback);
ERROR WaitForObjects(LONG Flags, LONG TimeOut, struct ObjectSignal * ObjectSignals);
ERROR ReadFileToBuffer(CSTRING Path, APTR Buffer, LONG BufferSize, LONG * Result);
LONG StrDatatype(CSTRING String);
void UnloadFile(struct CacheFile * Cache);
void SetDefaultPermissions(LONG User, LONG Group, LONG Permissions);
ERROR CompareFilePaths(CSTRING PathA, CSTRING PathB);
const struct SystemState * GetSystemState();
ERROR AddInfoTag(struct FileInfo * Info, CSTRING Name, CSTRING Value);

#ifdef  __cplusplus
}
#endif
