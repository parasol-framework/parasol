// Auto-generated by idl-c.fluid

extern "C" ERR AccessMemory(MEMORYID Memory, MEM Flags, LONG MilliSeconds, APTR * Result);
extern "C" ERR Action(AC Action, OBJECTPTR Object, APTR Parameters);
extern "C" void ActionList(struct ActionTable ** Actions, LONG * Size);
extern "C" ERR DeleteFile(CSTRING Path, FUNCTION * Callback);
extern "C" CSTRING ResolveClassID(CLASSID ID);
extern "C" LONG AllocateID(IDTYPE Type);
extern "C" ERR AllocMemory(LONG Size, MEM Flags, APTR * Address, MEMORYID * ID);
extern "C" ERR AccessObject(OBJECTID Object, LONG MilliSeconds, OBJECTPTR * Result);
extern "C" ERR CheckAction(OBJECTPTR Object, AC Action);
extern "C" ERR CheckMemoryExists(MEMORYID ID);
extern "C" ERR CheckObjectExists(OBJECTID Object);
extern "C" ERR InitObject(OBJECTPTR Object);
extern "C" ERR VirtualVolume(CSTRING Name, ...);
extern "C" OBJECTPTR CurrentContext();
extern "C" ERR GetFieldArray(OBJECTPTR Object, FIELD Field, APTR * Result, LONG * Elements);
extern "C" LONG AdjustLogLevel(LONG Delta);
extern "C" ERR ReadFileToBuffer(CSTRING Path, APTR Buffer, LONG BufferSize, LONG * Result);
extern "C" ERR FindObject(CSTRING Name, CLASSID ClassID, FOF Flags, OBJECTID * ObjectID);
extern "C" objMetaClass * FindClass(CLASSID ClassID);
extern "C" ERR AnalysePath(CSTRING Path, LOC * Type);
extern "C" ERR FreeResource(MEMORYID ID);
extern "C" CLASSID GetClassID(OBJECTID Object);
extern "C" OBJECTID GetOwnerID(OBJECTID Object);
extern "C" ERR GetField(OBJECTPTR Object, FIELD Field, APTR Result);
extern "C" ERR GetFieldVariable(OBJECTPTR Object, CSTRING Field, STRING Buffer, LONG Size);
extern "C" ERR CompareFilePaths(CSTRING PathA, CSTRING PathB);
extern "C" const struct SystemState * GetSystemState();
extern "C" ERR ListChildren(OBJECTID Object, pf::vector<ChildEntry> * List);
extern "C" ERR RegisterFD(HOSTHANDLE FD, RFD Flags, void (*Routine)(HOSTHANDLE, APTR) , APTR Data);
extern "C" ERR ResolvePath(const std::string_view & Path, RSF Flags, std::string * Result);
extern "C" ERR MemoryIDInfo(MEMORYID ID, struct MemInfo * MemInfo, LONG Size);
extern "C" ERR MemoryPtrInfo(APTR Address, struct MemInfo * MemInfo, LONG Size);
extern "C" ERR NewObject(CLASSID ClassID, NF Flags, OBJECTPTR * Object);
extern "C" void NotifySubscribers(OBJECTPTR Object, AC Action, APTR Args, ERR Error);
extern "C" ERR CopyFile(CSTRING Source, CSTRING Dest, FUNCTION * Callback);
extern "C" ERR ProcessMessages(PMF Flags, LONG TimeOut);
extern "C" ERR IdentifyFile(CSTRING Path, CLASSID * Class, CLASSID * SubClass);
extern "C" ERR ReallocMemory(APTR Memory, ULONG Size, APTR * Address, MEMORYID * ID);
extern "C" ERR GetMessage(LONG Type, MSF Flags, APTR Buffer, LONG Size);
extern "C" ERR ReleaseMemory(MEMORYID MemoryID);
extern "C" CLASSID ResolveClassName(CSTRING Name);
extern "C" ERR SendMessage(LONG Type, MSF Flags, APTR Data, LONG Size);
extern "C" ERR SetOwner(OBJECTPTR Object, OBJECTPTR Owner);
extern "C" OBJECTPTR SetContext(OBJECTPTR Object);
extern "C" ERR SetField(OBJECTPTR Object, FIELD Field, ...);
extern "C" CSTRING FieldName(ULONG FieldID);
extern "C" ERR ScanDir(struct DirInfo * Info);
extern "C" ERR SetName(OBJECTPTR Object, CSTRING Name);
extern "C" void LogReturn();
extern "C" ERR SubscribeAction(OBJECTPTR Object, AC Action, FUNCTION * Callback);
extern "C" ERR SubscribeEvent(LARGE Event, FUNCTION * Callback, APTR Custom, APTR * Handle);
extern "C" ERR SubscribeTimer(DOUBLE Interval, FUNCTION * Callback, APTR * Subscription);
extern "C" ERR UpdateTimer(APTR Subscription, DOUBLE Interval);
extern "C" ERR UnsubscribeAction(OBJECTPTR Object, AC Action);
extern "C" void UnsubscribeEvent(APTR Handle);
extern "C" ERR BroadcastEvent(APTR Event, LONG EventSize);
extern "C" void WaitTime(LONG Seconds, LONG MicroSeconds);
extern "C" LARGE GetEventID(EVG Group, CSTRING SubGroup, CSTRING Event);
extern "C" ULONG GenCRC32(ULONG CRC, APTR Data, ULONG Length);
extern "C" LARGE GetResource(RES Resource);
extern "C" LARGE SetResource(RES Resource, LARGE Value);
extern "C" ERR ScanMessages(LONG * Handle, LONG Type, APTR Buffer, LONG Size);
extern "C" ERR WaitForObjects(PMF Flags, LONG TimeOut, struct ObjectSignal * ObjectSignals);
extern "C" void UnloadFile(struct CacheFile * Cache);
extern "C" ERR CreateFolder(CSTRING Path, PERMIT Permissions);
extern "C" ERR LoadFile(CSTRING Path, LDF Flags, struct CacheFile ** Cache);
extern "C" ERR SetVolume(CSTRING Name, CSTRING Path, CSTRING Icon, CSTRING Label, CSTRING Device, VOLUME Flags);
extern "C" ERR DeleteVolume(CSTRING Name);
extern "C" ERR MoveFile(CSTRING Source, CSTRING Dest, FUNCTION * Callback);
extern "C" ERR UpdateMessage(LONG Message, LONG Type, APTR Data, LONG Size);
extern "C" ERR AddMsgHandler(APTR Custom, LONG MsgType, FUNCTION * Routine, struct MsgHandler ** Handle);
extern "C" ERR QueueAction(AC Action, OBJECTID Object, APTR Args);
extern "C" LARGE PreciseTime();
extern "C" ERR OpenDir(CSTRING Path, RDF Flags, struct DirInfo ** Info);
extern "C" OBJECTPTR GetObjectPtr(OBJECTID Object);
extern "C" struct Field * FindField(OBJECTPTR Object, ULONG FieldID, OBJECTPTR * Target);
extern "C" CSTRING GetErrorMsg(ERR Error);
extern "C" struct Message * GetActionMsg();
extern "C" ERR FuncError(CSTRING Header, ERR Error);
extern "C" ERR SetArray(OBJECTPTR Object, FIELD Field, APTR Array, LONG Elements);
extern "C" ERR LockObject(OBJECTPTR Object, LONG MilliSeconds);
extern "C" void ReleaseObject(OBJECTPTR Object);
extern "C" ERR AsyncAction(AC Action, OBJECTPTR Object, APTR Args, FUNCTION * Callback);
extern "C" ERR AddInfoTag(struct FileInfo * Info, CSTRING Name, CSTRING Value);
extern "C" void SetDefaultPermissions(LONG User, LONG Group, PERMIT Permissions);
extern "C" void VLogF(VLF Flags, CSTRING Header, CSTRING Message, va_list Args);
extern "C" ERR ReadInfoTag(struct FileInfo * Info, CSTRING Name, CSTRING * Value);
extern "C" ERR SetResourcePath(RP PathType, CSTRING Path);
extern "C" objTask * CurrentTask();
extern "C" CSTRING ResolveGroupID(LONG Group);
extern "C" CSTRING ResolveUserID(LONG User);
extern "C" ERR CreateLink(CSTRING From, CSTRING To);
extern "C" OBJECTPTR ParentContext();
