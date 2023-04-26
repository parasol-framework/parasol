// Auto-generated by idl-c.fluid

#ifdef  __cplusplus
extern "C" {
#endif

ERROR AccessMemory(MEMORYID Memory, MEM Flags, LONG MilliSeconds, APTR * Result);
ERROR Action(LONG Action, OBJECTPTR Object, APTR Parameters);
void ActionList(struct ActionTable ** Actions, LONG * Size);
ERROR ActionMsg(LONG Action, OBJECTID Object, APTR Args);
CSTRING ResolveClassID(CLASSID ID);
LONG AllocateID(IDTYPE Type);
ERROR AllocMemory(LONG Size, MEM Flags, APTR * Address, MEMORYID * ID);
ERROR AccessObject(OBJECTID Object, LONG MilliSeconds, OBJECTPTR * Result);
ERROR CheckAction(OBJECTPTR Object, LONG Action);
ERROR CheckMemoryExists(MEMORYID ID);
ERROR CheckObjectExists(OBJECTID Object);
ERROR InitObject(OBJECTPTR Object);
ERROR VirtualVolume(CSTRING Name, ...);
OBJECTPTR CurrentContext();
ERROR GetFieldArray(OBJECTPTR Object, FIELD Field, APTR * Result, LONG * Elements);
LONG AdjustLogLevel(LONG Adjust);
void LogF(CSTRING Header, CSTRING Message, ...);
ERROR FindObject(CSTRING Name, CLASSID ClassID, FOF Flags, OBJECTID * ObjectID);
objMetaClass * FindClass(CLASSID ClassID);
ERROR AnalysePath(CSTRING Path, LOC * Type);
LONG UTF8Copy(CSTRING Src, STRING Dest, LONG Chars, LONG Size);
ERROR FreeResource(MEMORYID ID);
CLASSID GetClassID(OBJECTID Object);
OBJECTID GetOwnerID(OBJECTID Object);
ERROR GetField(OBJECTPTR Object, FIELD Field, APTR Result);
ERROR GetFieldVariable(OBJECTPTR Object, CSTRING Field, STRING Buffer, LONG Size);
ERROR CompareFilePaths(CSTRING PathA, CSTRING PathB);
const struct SystemState * GetSystemState();
ERROR ListChildren(OBJECTID Object, pf::vector<ChildEntry> * List);
ERROR Base64Decode(struct pfBase64Decode * State, CSTRING Input, LONG InputSize, APTR Output, LONG * Written);
ERROR RegisterFD(HOSTHANDLE FD, RFD Flags, void (*Routine)(HOSTHANDLE, APTR) , APTR Data);
ERROR ResolvePath(CSTRING Path, RSF Flags, STRING * Result);
ERROR MemoryIDInfo(MEMORYID ID, struct MemInfo * MemInfo, LONG Size);
ERROR MemoryPtrInfo(APTR Address, struct MemInfo * MemInfo, LONG Size);
ERROR NewObject(LARGE ClassID, NF Flags, OBJECTPTR * Object);
void NotifySubscribers(OBJECTPTR Object, LONG Action, APTR Args, ERROR Error);
ERROR StrReadLocale(CSTRING Key, CSTRING * Value);
CSTRING UTF8ValidEncoding(CSTRING String, CSTRING Encoding);
ERROR ProcessMessages(PMF Flags, LONG TimeOut);
ERROR IdentifyFile(CSTRING Path, CLASSID * Class, CLASSID * SubClass);
ERROR ReallocMemory(APTR Memory, LONG Size, APTR * Address, MEMORYID * ID);
ERROR GetMessage(LONG Type, MSF Flags, APTR Buffer, LONG Size);
ERROR ReleaseMemory(MEMORYID MemoryID);
CLASSID ResolveClassName(CSTRING Name);
ERROR SendMessage(LONG Type, MSF Flags, APTR Data, LONG Size);
ERROR SetOwner(OBJECTPTR Object, OBJECTPTR Owner);
OBJECTPTR SetContext(OBJECTPTR Object);
ERROR SetField(OBJECTPTR Object, FIELD Field, ...);
CSTRING FieldName(ULONG FieldID);
ERROR ScanDir(struct DirInfo * Info);
ERROR SetName(OBJECTPTR Object, CSTRING Name);
void LogReturn();
ERROR StrCompare(CSTRING String1, CSTRING String2, LONG Length, STR Flags);
ERROR SubscribeAction(OBJECTPTR Object, LONG Action, FUNCTION * Callback);
ERROR SubscribeEvent(LARGE Event, FUNCTION * Callback, APTR Custom, APTR * Handle);
ERROR SubscribeTimer(DOUBLE Interval, FUNCTION * Callback, APTR * Subscription);
ERROR UpdateTimer(APTR Subscription, DOUBLE Interval);
ERROR UnsubscribeAction(OBJECTPTR Object, LONG Action);
void UnsubscribeEvent(APTR Handle);
ERROR BroadcastEvent(APTR Event, LONG EventSize);
void WaitTime(LONG Seconds, LONG MicroSeconds);
LARGE GetEventID(EVG Group, CSTRING SubGroup, CSTRING Event);
ULONG GenCRC32(ULONG CRC, APTR Data, ULONG Length);
LARGE GetResource(RES Resource);
LARGE SetResource(RES Resource, LARGE Value);
ERROR ScanMessages(LONG * Handle, LONG Type, APTR Buffer, LONG Size);
STT StrDatatype(CSTRING String);
void UnloadFile(struct CacheFile * Cache);
ERROR CreateFolder(CSTRING Path, PERMIT Permissions);
ERROR LoadFile(CSTRING Path, LDF Flags, struct CacheFile ** Cache);
ERROR SetVolume(CSTRING Name, CSTRING Path, CSTRING Icon, CSTRING Label, CSTRING Device, VOLUME Flags);
ERROR DeleteVolume(CSTRING Name);
ERROR MoveFile(CSTRING Source, CSTRING Dest, FUNCTION * Callback);
ERROR UpdateMessage(LONG Message, LONG Type, APTR Data, LONG Size);
ERROR AddMsgHandler(APTR Custom, LONG MsgType, FUNCTION * Routine, struct MsgHandler ** Handle);
ERROR QueueAction(LONG Action, OBJECTID Object, APTR Args);
LARGE PreciseTime();
ERROR OpenDir(CSTRING Path, RDF Flags, struct DirInfo ** Info);
OBJECTPTR GetObjectPtr(OBJECTID Object);
struct Field * FindField(OBJECTPTR Object, ULONG FieldID, OBJECTPTR * Target);
CSTRING GetErrorMsg(ERROR Error);
struct Message * GetActionMsg();
ERROR FuncError(CSTRING Header, ERROR Error);
ERROR SetArray(OBJECTPTR Object, FIELD Field, APTR Array, LONG Elements);
ULONG StrHash(CSTRING String, LONG CaseSensitive);
ERROR LockObject(OBJECTPTR Object, LONG MilliSeconds);
void ReleaseObject(OBJECTPTR Object);
ERROR ActionThread(LONG Action, OBJECTPTR Object, APTR Args, FUNCTION * Callback, LONG Key);
ERROR AddInfoTag(struct FileInfo * Info, CSTRING Name, CSTRING Value);
void SetDefaultPermissions(LONG User, LONG Group, PERMIT Permissions);
void VLogF(VLF Flags, CSTRING Header, CSTRING Message, va_list Args);
LONG Base64Encode(struct pfBase64Encode * State, const void * Input, LONG InputSize, STRING Output, LONG OutputSize);
ERROR ReadInfoTag(struct FileInfo * Info, CSTRING Name, CSTRING * Value);
ERROR SetResourcePath(RP PathType, CSTRING Path);
objTask * CurrentTask();
CSTRING ResolveGroupID(LONG Group);
CSTRING ResolveUserID(LONG User);
ERROR CreateLink(CSTRING From, CSTRING To);
ERROR DeleteFile(CSTRING Path, FUNCTION * Callback);
LONG UTF8CharOffset(CSTRING String, LONG Offset);
LONG UTF8Length(CSTRING String);
LONG UTF8OffsetToChar(CSTRING String, LONG Offset);
LONG UTF8PrevLength(CSTRING String, LONG Offset);
LONG UTF8CharLength(CSTRING String);
ULONG UTF8ReadValue(CSTRING String, LONG * Length);
LONG UTF8WriteValue(LONG Value, STRING Buffer, LONG Size);
ERROR CopyFile(CSTRING Source, CSTRING Dest, FUNCTION * Callback);
ERROR WaitForObjects(PMF Flags, LONG TimeOut, struct ObjectSignal * ObjectSignals);
ERROR ReadFileToBuffer(CSTRING Path, APTR Buffer, LONG BufferSize, LONG * Result);

#ifdef  __cplusplus
}
#endif
