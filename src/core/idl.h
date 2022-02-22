#undef MOD_IDL
#define MOD_IDL "s.InputEvent:pNext:InputEvent,dValue,xTimestamp,lRecipientID,lOverID,lAbsX,lAbsY,lX,lY,lDeviceID,uwType,uwFlags,uwMask\ns.dcRequest:lItem,cPreference[4]\ns.dcAudio:lSize,lFormat\ns.dcKeyEntry:lFlags,lValue,xTimestamp,lUnicode\ns.dcDeviceInput:dValue,xTimestamp,lDeviceID,lFlags,uwType,uwUnused\ns.DateTime:lYear,lMonth,lDay,lHour,lMinute,lSecond,lTimeZone\ns.HSV:dHue,dSaturation,dValue\ns.FRGB:fRed,fGreen,fBlue,fAlpha\ns.DRGB:dRed,dGreen,dBlue,dAlpha\ns.RGB8:ucRed,ucGreen,ucBlue,ucAlpha\ns.RGB16:uwRed,uwGreen,uwBlue,uwAlpha\ns.RGB32:ulRed,ulGreen,ulBlue,ulAlpha\ns.RGBPalette:lAmtColours,eCol:RGB8[256]\ns.ColourFormat:ucRedShift,ucGreenShift,ucBlueShift,ucAlphaShift,ucRedMask,ucGreenMask,ucBlueMask,ucAlphaMask,ucRedPos,ucGreenPos,ucBluePos,ucAlphaPos,ucBitsPerPixel\ns.ClipRectangle:lLeft,lRight,lBottom,lTop\ns.Edges:lLeft,lRight,lBottom,lTop\ns.ObjectSignal:oObject\ns.rkBase64Decode:ucStep,ucPlainChar,ucBit\ns.rkBase64Encode:ucStep,ucResult,lStepCount\ns.FeedSubscription:lSubscriberID,lMessagePortMID,ulClassID\ns.FunctionField:sName,ulType\ns.Function:pAddress,sName,pArgs:FunctionField\ns.FieldArray:sName,ulFlags,mArg,pGetField,pSetField\ns.FieldDef:sName,lValue\ns.SystemState:sErrorMessages,sRootPath,sSystemPath,sModulePath,sPlatform,lConsoleFD,lCoreVersion,lCoreRevision,lInstanceID,lTotalErrorMessages,lStage\ns.Variable:ulType,lUnused,xLarge,dDouble,pPointer\ns.ActionArray:lActionCode,pRoutine\ns.MemoryLocks:lMemoryID,wLocks\ns.ActionTable:ulHash,lSize,sName,pArgs:FunctionField\ns.ChildEntry:lObjectID,ulClassID\ns.ListTasks:lProcessID,lTaskID,lWaitingProcessID,lWaitingMemoryID,lWaitingTime,lMessageID,lOutputID,lSemaphore,lInstanceID,lTotalMemoryLocks,lModalID,pMemoryLocks:MemoryLocks\ns.FDTable:lFD,pRoutine,pData,lFlags\ns.Message:xTime,lUniqueID,lType,lSize\ns.MemInfo:pStart,lObjectID,lSize,wAccessCount,wFlags,lMemoryID,lLockID,lTaskID,lHandle\ns.KeyStore:pMutex,pData,lTableSize,lTotal,lFlags\ns.CompressionFeedback:lFeedbackID,lIndex,sPath,sDest,xProgress,xOriginalSize,xCompressedSize,wYear,wMonth,wDay,wHour,wMinute,wSecond\ns.CompressedItem:xOriginalSize,xCompressedSize,pNext:CompressedItem,sPath,pTags:KeyStore,lPermissions,lUserID,lGroupID,lOthersID,lFlags,eCreated:DateTime,eModified:DateTime\ns.FileInfo:xSize,xTimeStamp,pNext:FileInfo,sName,pTags:KeyStore,lFlags,lPermissions,lUserID,lGroupID,eCreated:DateTime,eModified:DateTime\ns.DirInfo:pInfo:FileInfo\ns.FileFeedback:xSize,xPosition,sPath,sDest,lFeedbackID,cReserved[32]\ns.Field:mArg,pGetValue,pSetValue,pWriteValue,sName,ulFieldID,uwOffset,uwIndex,ulFlags\nc.FFR:OKAY=0x0,CONTINUE=0x0,SKIP=0x1,ABORT=0x2\nc.ALF:RECURSIVE=0x2,SHARED=0x1\nc.NSF:DELAY=0x8,FORCE_DELAY=0x8,LOCAL=0x2,LOCAL_TASK=0x2,EXCLUSIVE=0x1,OTHER_TASKS=0x4\nc.CCF:IO=0x40,NETWORK=0x1000,TOOL=0x100,MULTIMEDIA=0x2000,AUDIO=0x200,DRAWABLE=0x2,SYSTEM=0x80,COMMAND=0x1,MISC=0x800,FILESYSTEM=0x8,DATA=0x400,GRAPHICS=0x10,EFFECT=0x4,GUI=0x20\nc.MOF:SYSTEM_PROBE=0x4,LINK_LIBRARY=0x1,STATIC=0x2\nc.SMF:NON_BLOCKING=0x1,EXISTS=0x2,NO_BLOCKING=0x1\nc.STYLE:ENABLED=0x1,DISABLED=0x2,LOST_FOCUS=0x4,CONTENT=0x6,RESIZE=0x5,FOCUS=0x3\nc.RFD:REMOVE=0x8,READ=0x4,STOP_RECURSE=0x10,ALLOW_RECURSION=0x20,SOCKET=0x40,RECALL=0x80,WRITE=0x1,ALWAYS_CALL=0x100,EXCEPT=0x2\nc.FDT:ACCESSED=0x2,ARCHIVED=0x3,MODIFIED=0x0,CREATED=0x1\nc.DMF:RELATIVE_RADIUS=0x1010000,FIXED_RADIUS=0x2020000,STATUS_CHANGE=0xc00000,HORIZONTAL_FLAGS=0xa55,VERTICAL_FLAGS=0x5aa,WIDTH_FLAGS=0xa50,HEIGHT_FLAGS=0x5a0,RELATIVE_WIDTH=0x800,FIXED_Y=0x8,RELATIVE_HEIGHT=0x400,FIXED_Y_OFFSET=0x80,RELATIVE_Y_OFFSET=0x20,RELATIVE_RADIUS_X=0x10000,FIXED_DEPTH=0x1000,FIXED_RADIUS_X=0x20000,FIXED_X_OFFSET=0x40,RELATIVE_CENTER_X=0x40000,FIXED_Z=0x4000,RELATIVE_CENTER_Y=0x80000,RELATIVE_X=0x1,FIXED_CENTER_X=0x100000,RELATIVE_Y=0x2,FIXED_CENTER_Y=0x200000,FIXED_X=0x4,STATUS_CHANGE_H=0x400000,STATUS_CHANGE_V=0x800000,RELATIVE_RADIUS_Y=0x1000000,RELATIVE_X_OFFSET=0x10,FIXED_RADIUS_Y=0x2000000,RELATIVE_DEPTH=0x2000,X=0x5,Y=0xa,WIDTH=0xa00,RELATIVE_Z=0x8000,HEIGHT=0x500,FIXED_HEIGHT=0x100,X_OFFSET=0x50,FIXED_WIDTH=0x200,Y_OFFSET=0xa0\nc.DATA:CONTENT=0xb,TEXT=0x1,AUDIO=0x5,DEVICE_INPUT=0x3,XML=0x4,RECORD=0x6,IMAGE=0x7,REQUEST=0x8,RECEIPT=0x9,FILE=0xa,INPUT_READY=0xc,RAW=0x2\nc.AST:FLAGS=0x3,NAME=0x2,Path=0x1,Name=0x2,Flags=0x3,Icon=0x4,ICON=0x4,Comment=0x5,COMMENT=0x5,DEVICE=0x7,Label=0x6,LABEL=0x6,Device=0x7,DevicePath=0x8,DEVICE_PATH=0x8,ID=0x9,PATH=0x1\nc.VOLUME:PRIORITY=0x2,SAVE=0x8,SYSTEM=0x10,REPLACE=0x1,HIDDEN=0x4\nc.PERMIT:EVERYONE_ACCESS=0xfff,EVERYONE_READWRITE=0x333,READ=0x1,USER=0xf,GROUP=0xf0,OTHERS=0xf00,DELETE=0x8,GROUP_READ=0x10,GROUP_WRITE=0x20,GROUP_EXEC=0x40,GROUP_DELETE=0x80,OTHERS_READ=0x100,OTHERS_WRITE=0x200,NETWORK=0x80000,OTHERS_EXEC=0x400,OTHERS_DELETE=0x800,ARCHIVE=0x2000,PASSWORD=0x4000,USERID=0x8000,GROUPID=0x10000,USER_READ=0x1,USER_WRITE=0x2,OFFLINE=0x40000,EXEC=0x4,EVERYONE_READ=0x111,HIDDEN=0x1000,EVERYONE_WRITE=0x222,INHERIT=0x20000,EVERYONE_EXEC=0x444,WRITE=0x2,EVERYONE_DELETE=0x888,USER_EXEC=0x4,ALL_READ=0x111,ALL_WRITE=0x222,ALL_EXEC=0x444,ALL_DELETE=0x888\nc.FDL:FEEDBACK=0x1\nc.SEF:IGNORE_QUOTES=0x2,NO_SCRIPT=0x8,STRICT=0x1,KEEP_ESCAPE=0x4\nc.CMF:NEW=0x2,PASSWORD=0x1,READ_ONLY=0x8,APPLY_SECURITY=0x20,NO_LINKS=0x10,CREATE_FILE=0x4\nc.FDB:DECOMPRESS_OBJECT=0x4,DECOMPRESS_FILE=0x1,COMPRESS_FILE=0x2,REMOVE_FILE=0x3\nc.MSGID:EVENT=0x5e,VALIDATE_PROCESS=0x5d,THREAD_CALLBACK=0x5c,THREAD_ACTION=0x5b,WAIT_FOR_OBJECTS=0x5a,DEBUG=0x5f,QUIT=0x3e8,BREAK=0x66,COMMAND=0x65,EXPOSE=0x64,CORE_END=0x64,ACTION=0x63,SET_FIELD=0x62,GET_FIELD=0x61,ACTION_RESULT=0x60\nc.EVG:DISPLAY=0x5,IO=0x6,NETWORK=0x2,APP=0xc,USER=0x9,AUDIO=0x8,END=0xe,SYSTEM=0x3,ANDROID=0xd,FILESYSTEM=0x1,CLASS=0xb,HARDWARE=0x7,POWER=0xa,GUI=0x4\nc.CF:GZIP=0x1,ZLIB=0x2,DEFLATE=0x3\nc.RSF:NO_DEEP_SCAN=0x8,NO_FILE_CHECK=0x1,CHECK_VIRTUAL=0x2,CASE_SENSITIVE=0x20,APPROXIMATE=0x4,PATH=0x10\nc.FOF:SMART_NAMES=0x1,INCLUDE_SHARED=0x2\nc.NF:FREE=0x40,UNIQUE=0x40000000,NO_TRACK=0x1,NAME=0x80000000,FOREIGN_OWNER=0x4,INITIALISED=0x8,INTEGRAL=0x10,UNLOCK_FREE=0x20,TIMER_SUB=0x80,PUBLIC=0x2,SHARED=0x2,FREE_MARK=0x200,NEW_OBJECT=0x400,RECLASSED=0x800,PRIVATE=0x0,MESSAGE=0x1000,HAS_SHARED_RESOURCES=0x4000,SIGNALLED=0x2000,CREATE_OBJECT=0x100,UNTRACKED=0x1\nc.LTF:CURRENT_PROCESS=0x1\nc.SUB:WARN_EXISTS=0x7fffffff,FAIL_EXISTS=0x7ffffffe\nc.IDTYPE:FUNCTION=0x3,GLOBAL=0x2,MESSAGE=0x1\nc.PTR:SIZE_BOTTOM=0x9,CROSSHAIR=0xa,SIZING=0xc,SPLIT_VERTICAL=0xd,SPLIT_HORIZONTAL=0xe,MAGNIFIER=0xf,HAND=0x10,HAND_LEFT=0x11,HAND_RIGHT=0x12,PAINTBRUSH=0x14,INVISIBLE=0x16,DRAGGABLE=0x18,SLEEP=0xb,END=0x19,CUSTOM=0x17,STOP=0x15,TEXT=0x13,NO_CHANGE=0x0,DEFAULT=0x1,SIZE_BOTTOM_LEFT=0x2,SIZE_BOTTOM_RIGHT=0x3,SIZE_TOP_LEFT=0x4,SIZE_TOP_RIGHT=0x5,SIZE_LEFT=0x6,SIZE_RIGHT=0x7,SIZE_TOP=0x8\nc.STT:HEX=0x3,STRING=0x4,NUMBER=0x1,FLOAT=0x2\nc.SEM:GET_DATA_DOUBLE=0x6,SET_DATA_PTR=0x7,SET_DATA_LONG=0x8,SET_DATA_LARGE=0x9,SET_DATA_DOUBLE=0xa,GET_VAL=0x1,GET_COUNTER=0x2,GET_DATA_PTR=0x3,GET_DATA_LONG=0x4,GET_DATA_LARGE=0x5\nc.JTYPE:ANCHORED=0x2,DRAGGED=0x4,FEEDBACK=0x8,DIGITAL=0x10,DRAG_ITEM=0x800,ANALOG=0x20,REPEATED=0x400,EXT_MOVEMENT=0x40,DBL_CLICK=0x200,BUTTON=0x80,MOVEMENT=0x100,SECONDARY=0x1\nc.OPF:ERROR=0x1000,NAME=0x1,COPYRIGHT=0x2,DATE=0x4,AUTHOR=0x8,CORE_VERSION=0x10,JUMPTABLE=0x20,MAX_DEPTH=0x40,DETAIL=0x80,SHOW_MEMORY=0x100,SHOW_IO=0x200,SHOW_ERRORS=0x400,ARGS=0x800,COMPILED_AGAINST=0x2000,PRIVILEGED=0x4000,SYSTEM_PATH=0x8000,MODULE_PATH=0x10000,ROOT_PATH=0x20000,SCAN_MODULES=0x40000,GLOBAL_INSTANCE=0x80000,OPTIONS=0x100000,SHOW_PUBLIC_MEM=0x200000\nc.KQ:NUM_PAD=0x200,REPEAT=0x400,INFO=0x3c04,RELEASED=0x800,ALTGR=0x40,PRESSED=0x1000,R_CONTROL=0x10,NOT_PRINTABLE=0x2000,L_SHIFT=0x1,R_SHIFT=0x2,CAPS_LOCK=0x4,DEAD_KEY=0x10000,L_ALT=0x20,R_ALT=0x40,L_COMMAND=0x80,R_COMMAND=0x100,INSTRUCTION_KEYS=0x78,QUALIFIERS=0x1fb,CTRL=0x18,CONTROL=0x18,ALT=0x60,COMMAND=0x180,SCR_LOCK=0x4000,L_CTRL=0x8,WIN_CONTROL=0x20000,R_CTRL=0x10,NUM_LOCK=0x8000,SHIFT=0x3,L_CONTROL=0x8\nc.TSTATE:TERMINATED=0x3,RUNNING=0x0,PAUSED=0x1,STOPPING=0x2\nc.DRL:NORTH=0x0,SOUTH=0x1,EAST=0x2,WEST=0x3,RIGHT=0x2,LEFT=0x3,SOUTH_WEST=0x7,SOUTH_EAST=0x6,NORTH_WEST=0x5,NORTH_EAST=0x4,DOWN=0x1,UP=0x0\nc.JET:ANALOG2_Y=0x19,ANALOG2_Z=0x1a,WHEEL=0x1b,WHEEL_TILT=0x1c,PEN_TILT_VERTICAL=0x1d,PEN_TILT_HORIZONTAL=0x1e,ABS_X=0x1f,ABS_Y=0x20,ENTERED_SURFACE=0x21,ENTERED=0x21,LEFT_SURFACE=0x22,PRESSURE=0x23,DEVICE_TILT_X=0x24,DEVICE_TILT_Y=0x25,DEVICE_TILT_Z=0x26,DISPLAY_EDGE=0x27,END=0x28,LEFT_BUMPER_1=0x11,TRIGGER_LEFT=0xd,TRIGGER_RIGHT=0xe,DIGITAL_X=0x1,DIGITAL_Y=0x2,LEFT=0x22,BUTTON_1=0x3,LMB=0x3,BUTTON_SELECT=0x10,BUTTON_2=0x4,RMB=0x4,BUTTON_START=0xf,BUTTON_3=0x5,MMB=0x5,BUTTON_4=0x6,BUTTON_5=0x7,BUTTON_6=0x8,BUTTON_7=0x9,BUTTON_8=0xa,BUTTON_9=0xb,BUTTON_10=0xc,LEFT_BUMPER_2=0x12,RIGHT_BUMPER_1=0x13,RIGHT_BUMPER_2=0x14,ANALOG_X=0x15,ANALOG_Y=0x16,ANALOG_Z=0x17,ANALOG2_X=0x18\nc.RES:CORE_IDL=0x1f,CURRENT_MSG=0x13,OPEN_INFO=0x14,EXCEPTION_HANDLER=0x15,NET_PROCESSING=0x16,PROCESS_STATE=0x17,TOTAL_MEMORY=0x18,TOTAL_SWAP=0x19,MESSAGE_QUEUE=0x1,CONSOLE_FD=0x2,SHARED_CONTROL=0x4,USER_ID=0x5,DISPLAY_DRIVER=0x6,PRIVILEGED_USER=0x7,RANDOM_SEED=0x9,PARENT_CONTEXT=0xa,LOG_LEVEL=0xb,TOTAL_SHARED_MEMORY=0xc,TASK_CONTROL=0xd,TASK_LIST=0xe,PRIVILEGED=0x8,LOG_DEPTH=0x10,JNI_ENV=0x11,THREAD_ID=0x12,KEY_STATE=0x1e,GLOBAL_INSTANCE=0x3,FREE_SWAP=0x1d,FREE_MEMORY=0x1c,SHARED_BLOCKS=0x1b,CPU_SPEED=0x1a,MAX_PROCESSES=0xf\nc.ERR:DoNotExpunge=0x30,Read=0x16,FieldSearch=0x32,InvalidPath=0x33,MarkedForDeletion=0x35,IllegalMethodID=0x36,IllegalActionID=0x37,ModuleOpenFailed=0x38,IllegalActionAttempt=0x39,EntryMissingHeader=0x3a,ModuleMissingInit=0x3b,ModuleInitFailed=0x3c,MemoryDoesNotExist=0x3d,DeadLock=0x3e,SystemLocked=0x3f,ModuleMissingName=0x40,AddClass=0x41,DoubleInit=0x43,FieldNotSet=0x44,MissingClass=0x45,FileReadFlag=0x46,FileWriteFlag=0x47,NoMethods=0x49,NoMatchingObject=0x4a,MissingPath=0x4c,NotLocked=0x4d,NoSearchResult=0x4e,StatementUnsatisfied=0x4f,ObjectCorrupt=0x50,OwnerPassThrough=0x51,UseSubClass=0xa7,WrongType=0xa8,NoFieldAccess=0x57,GetField=0x56,FieldTypeMismatch=0x5a,UnrecognisedFieldType=0x5b,BufferOverflow=0x5c,UnsupportedField=0x5d,Mismatch=0x5e,OutOfBounds=0x5f,Loop=0x62,FileExists=0x63,MemoryInfo=0x66,NewObject=0x55,ResourceExists=0x68,SystemCall=0x6b,Okay=0x0,True=0x0,Terminate=0x9,ExceptionThreshold=0x9,Search=0x10,NotFound=0x10,FileNotFound=0x12,OpenFile=0x76,LockFailed=0x18,NoPermission=0x22,PermissionDenied=0x22,SetField=0x34,BadOwner=0x52,ExclusiveDenied=0x53,GetSurfaceInfo=0x7d,EOF=0x7e,EndOfFile=0x7e,OutOfData=0x7e,CoreVersion=0x26,StringFormat=0x7f,ConnectionRefused=0x83,NetworkUnreachable=0x84,Permissions=0x22,Disconnected=0x86,TaskStillExists=0x87,AlreadyLocked=0x9c,SchemaViolation=0x89,DataSize=0x8a,Busy=0x8b,ConnectionAborted=0x8c,NullArgs=0x8d,ExecViolation=0x8f,Recursion=0x90,VirtualVolume=0x58,DeleteFile=0x75,ReadFileToBuffer=0xb0,AllocMutex=0xa9,LockMutex=0xaa,File=0xe,END=0xb7,AlreadyDefined=0xb6,Args=0x14,Function=0xb5,AccessSemaphore=0x73,ResolvePath=0x64,NotPossible=0xb3,CreateResource=0xb2,Obsolete=0xb1,SetVolume=0xab,Immutable=0xaf,ExpectedFolder=0xae,NotInitialised=0x67,Decompression=0xac,OutsideMainThread=0xa6,OpenGL=0xa5,GlobalInstanceLocked=0xa4,Exception=0xa3,InvalidReference=0xa2,InvalidHTTPResponse=0xa1,ProxySSLTunnel=0xa0,CardReaderUnavailable=0x9f,NoMediaInserted=0x9e,CardReaderUnknown=0x9d,Locked=0x9c,LockRequired=0x9b,Deactivated=0x9a,ServiceUnavailable=0x99,InvalidValue=0x98,Security=0x97,InvalidHandle=0x96,LoadModule=0x95,InputOutput=0x94,WouldBlock=0x93,UnbalancedXML=0x92,IllegalAddress=0x91,WrongClass=0x8e,WrongObjectType=0x8e,InvalidObject=0x8e,ConstraintViolation=0x88,IntegrityViolation=0x88,HostUnreachable=0x85,InvalidURI=0x82,HostNotFound=0x81,BadState=0x80,Syntax=0x7f,Finished=0x7e,OutOfSpace=0x7c,SanityFailure=0x7b,Exists=0x7a,IdenticalPaths=0x79,DoesNotExist=0x78,ReadOnly=0x77,FileDoesNotExist=0x12,CreateFile=0x74,ResolveSymbol=0xb4,AllocSemaphore=0x72,Redimension=0x71,Resize=0x70,NegativeSubClassID=0x28,NeedWidthHeight=0x27,EmptyString=0x6d,SmallMask=0x6c,ListChildren=0x6a,Compression=0xad,CreateObject=0x65,TimeOut=0x1e,Seek=0x60,InvalidDimension=0x59,ReallocMemory=0x61,AllocMemory=0x54,AccessObject=0x53,ExpectedFile=0x6f,NoData=0x15,AccessMemory=0x4b,ObjectExists=0x6e,False=0x1,LimitedSuccess=0x2,Cancelled=0x3,NothingDone=0x4,Continue=0x5,Skip=0x6,Retry=0x7,DirEmpty=0x8,Query=0x2e,NoMemory=0xa,NoPointer=0xb,InUse=0xc,Failed=0xd,InvalidData=0xf,Write=0x17,InitModule=0x11,MemoryCorrupt=0x31,WrongVersion=0x13,Activate=0x42,UnsupportedOwner=0x52,ExamineFailed=0x19,LostClass=0x1a,NoAction=0x1b,NoSupport=0x1c,Memory=0x1d,Draw=0x48,NoStats=0x1f,LowCapacity=0x20,Refresh=0x69,SystemCorrupt=0x23,NeedOwner=0x24,OwnerNeedsBitmap=0x25,Init=0x21,Lock=0x18,NegativeClassID=0x29,MissingClassName=0x2a,OutOfRange=0x2b,ObtainMethod=0x2c,ArrayFull=0x2d,LostOwner=0x2f\nc.ERF:Delay=0x20000000,Notified=0x40000000\nc.SEEK:END=0x2,START=0x0,CURRENT=0x1\nc.K:F18=0x80,F19=0x81,F20=0x82,WIN_CONTROL=0x83,VOLUME_UP=0x84,VOLUME_DOWN=0x85,BACK=0x86,CALL=0x87,END_CALL=0x88,CAMERA=0x89,DELETE=0x6d,PLUS=0x8b,LENS_FOCUS=0x8c,UNDO=0x76,PREVIOUS=0x8f,FORWARD=0x90,REWIND=0x91,MUTE=0x92,STAR=0x93,POUND=0x94,PLAY=0x95,LIST_END=0x96,END=0x72,F12=0x57,REDO=0x77,NP_4=0x35,BREAK=0x7b,AT=0x8a,STOP=0x8d,MENU=0x78,INSERT=0x75,K=0xb,EXECUTE=0x74,A=0x1,B=0x2,C=0x3,D=0x4,E=0x5,F=0x6,G=0x7,H=0x8,I=0x9,J=0xa,L=0xc,M=0xd,N=0xe,O=0xf,P=0x10,Q=0x11,R=0x12,S=0x13,T=0x14,U=0x15,V=0x16,W=0x17,ONE=0x1b,TWO=0x1c,THREE=0x1d,FOUR=0x1e,FIVE=0x1f,SIX=0x20,SEVEN=0x21,EIGHT=0x22,NINE=0x23,ZERO=0x24,REVERSE_QUOTE=0x25,MINUS=0x26,EQUALS=0x27,L_SQUARE=0x28,R_SQUARE=0x29,SEMI_COLON=0x2a,APOSTROPHE=0x2b,COMMA=0x2c,PERIOD=0x2d,DOT=0x2d,SLASH=0x2e,BACK_SLASH=0x2f,SPACE=0x30,NP_0=0x31,NP_1=0x32,NP_2=0x33,NP_3=0x34,CLEAR=0x6e,NP_5=0x36,NP_6=0x37,NP_7=0x38,NP_8=0x39,NP_9=0x3a,NP_MULTIPLY=0x3b,NP_PLUS=0x3c,NP_SEPARATOR=0x3d,NP_BAR=0x3d,NP_MINUS=0x3e,NP_DECIMAL=0x3f,NP_DOT=0x3f,NP_DIVIDE=0x40,L_CONTROL=0x41,R_CONTROL=0x42,HELP=0x43,L_SHIFT=0x44,R_SHIFT=0x45,CAPS_LOCK=0x46,PRINT=0x47,L_ALT=0x48,R_ALT=0x49,L_COMMAND=0x4a,R_COMMAND=0x4b,F1=0x4c,F2=0x4d,F3=0x4e,F4=0x4f,F5=0x50,F6=0x51,F7=0x52,F8=0x53,F9=0x54,F10=0x55,F11=0x56,NEXT=0x8e,F13=0x58,F14=0x59,F15=0x5a,F16=0x5b,F17=0x5c,MACRO=0x5d,NP_PLUS_MINUS=0x5e,LESS_GREATER=0x5f,UP=0x60,DOWN=0x61,RIGHT=0x62,LEFT=0x63,SCR_LOCK=0x64,PAUSE=0x65,WAKE=0x66,SLEEP=0x67,POWER=0x68,BACKSPACE=0x69,TAB=0x6a,ENTER=0x6b,ESCAPE=0x6c,HOME=0x6f,PAGE_UP=0x70,PAGE_DOWN=0x71,SELECT=0x73,X=0x18,Y=0x19,Z=0x1a,FIND=0x79,CANCEL=0x7a,NUM_LOCK=0x7c,PRT_SCR=0x7d,NP_ENTER=0x7e,SYSRQ=0x7f\nc.KSF:AUTO_REMOVE=0x8,CASE=0x1,THREAD_SAFE=0x2,INTERNAL=0x10,UNTRACKED=0x4\nc.MAX:NAME_LEN=0x20\nc.CNF:NEW=0x8,STRIP_QUOTES=0x1,OPTIONAL_FILES=0x4,AUTO_SAVE=0x2\nc.TOI:ANDROID_ENV=0x2,ANDROID_CLASS=0x3,ANDROID_ASSETMGR=0x4,LOCAL_CACHE=0x0,LOCAL_STORAGE=0x1\nc.THF:AUTO_FREE=0x1,MSG_HANDLER=0x2\nc.ALIGN:RIGHT=0x2,LEFT=0x1,HORIZONTAL=0x4,CENTER=0xc,VERTICAL=0x8,TOP=0x10,BOTTOM=0x20,MIDDLE=0xc\nc.COPY:ALL=0x7fffffff\nc.VAS:DEREGISTER=0x1,SCAN_DIR=0x2,DELETE=0x3,OPEN_DIR=0x5,CLOSE_DIR=0x6,TEST_PATH=0x7,WATCH_PATH=0x8,IGNORE_FILE=0x9,GET_INFO=0xa,GET_DEVICE_INFO=0xb,RENAME=0x4,MAKE_DIR=0xd,SAME_FILE=0xe,READ_LINK=0x10,CREATE_LINK=0x11,CASE_SENSITIVE=0xf,IDENTIFY_FILE=0xc\nc.VLF:BRANCH=0x1,ERROR=0x2,WARNING=0x4,CRITICAL=0x8,INFO=0x10,API=0x20,EXTAPI=0x40,DEBUG=0x80,TRACE=0x100,FUNCTION=0x200\nc.STF:EXPRESSION=0x10,CASE=0x1,SCAN_SELECTION=0x4,WRAP=0x20,BACKWARDS=0x8,MOVE_CURSOR=0x2\nc.SCF:EXIT_ON_ERROR=0x1,DEBUG=0x2\nc.NETMSG:START=0x0,END=0x1\nc.ACF:ALL_USERS=0x1\nc.AC:Move=0x15,Read=0x1d,Rename=0x1e,Reset=0x1f,Resize=0x20,SaveImage=0x21,SaveToObject=0x22,Scroll=0x23,Seek=0x24,SetVar=0x25,Show=0x26,Undo=0x27,Unlock=0x28,Prev=0x2a,Write=0x2b,Refresh=0x2e,AccessObject=0x3,Enable=0x30,Redimension=0x31,MoveToPoint=0x32,ScrollToPoint=0x33,Custom=0x34,Sort=0x35,SaveSettings=0x36,SelectArea=0x37,Signal=0x38,ReleaseObject=0xe,Query=0x1c,Disable=0x2f,Clipboard=0x2d,SetField=0x2c,Next=0x29,END=0x39,Clear=0x4,ActionNotify=0x1,Activate=0x2,NewObject=0x1a,FreeWarning=0x5,OwnerDestroyed=0x6,CopyData=0x7,DataFeed=0x8,Deactivate=0x9,Draw=0xa,Flush=0xb,Focus=0xc,Free=0xd,GetVar=0xf,DragDrop=0x10,Hide=0x11,Init=0x12,Lock=0x13,LostFocus=0x14,MoveToBack=0x16,MoveToFront=0x17,NewChild=0x18,NewOwner=0x19,Redo=0x1b\nc.FD:PTR=0x8000000,VARIABLE=0x20000000,VOID=0x0,FLOAT=0x10000000,POINTER=0x8000000,PRIVATE=0x10000,LARGE=0x4000000,FUNCTION=0x2000000,BYTE=0x1000000,STRRESULT=0x800100,STRING=0x800000,PTRRESULT=0x8000100,WORD=0x400000,LONGRESULT=0x40000100,PERCENTAGE=0x200000,LARGERESULT=0x4000100,RGB=0x80000,DOUBLERESULT=0x80000100,UNSIGNED=0x40000,RW=0x300,SYNONYM=0x20000,SYSTEM=0x10000,ARRAY=0x1000,RESOURCE=0x2000,W=0x200,PTR_DOUBLERESULT=0x88000100,PTR_LARGERESULT=0xc000100,PTR_LONGRESULT=0x48000100,LOOKUP=0x80,I=0x400,FLAGS=0x40,RI=0x500,ALLOC=0x20,R=0x100,STRUCT=0x10,PTRBUFFER=0x8000200,VIRTUAL=0x8,OBJECTID=0x40000001,REQUIRED=0x4,OBJECTPTR=0x8000001,INTEGRAL=0x2,FUNCTIONPTR=0xa000000,OBJECT=0x1,PTRSIZE=0x80,STR=0x800000,TAGS=0x400,READ=0x100,BUFFER=0x200,VARTAGS=0x40,RESULT=0x100,CUSTOM=0x8000,BUFSIZE=0x80,ERROR=0x800,ARRAYSIZE=0x80,INIT=0x400,WRITE=0x200,DOUBLE=0x80000000,VOLATILE=0x0,LONG=0x40000000\nc.MOVE:RIGHT=0x8,LEFT=0x4,ALL=0xf,UP=0x2,DOWN=0x1\nc.SBF:SORT=0x2,DESC=0x8,CSV=0x10,NO_DUPLICATES=0x1,CASE=0x4\nc.RDF:SIZE=0x1,TIME=0x2,VIRTUAL=0x400,DATE=0x2,READ_ONLY=0x1000,FILES=0x8,FOLDER=0x10,TAGS=0x80,VOLUME=0x20,OPENDIR=0x4000,ARCHIVE=0x2000,LINK=0x40,QUALIFY=0x200,QUALIFIED=0x200,FILE=0x8,STREAM=0x800,READ_ALL=0x1f,PERMISSIONS=0x4,FOLDERS=0x10,HIDDEN=0x100\nc.MEM:TASK=0x200000,NO_LOCK=0x400,EXCLUSIVE=0x800,AUDIO=0x8,READ_WRITE=0x30000,NO_BLOCK=0x2000,NO_BLOCKING=0x2000,STRING=0x100,CALLER=0x800000,MANAGED=0x8000,OBJECT=0x200,HIDDEN=0x100000,RESERVED=0x80000,PUBLIC=0x1,SHARED=0x1,VIDEO=0x2,NO_CLEAR=0x40000,TEXTURE=0x4,DATA=0x0,FIXED=0x4000,CODE=0x10,READ=0x10000,NO_POOL=0x20,WRITE=0x20000,TMP_LOCK=0x40,DELETE=0x1000,UNTRACKED=0x80\nc.TSF:DUMMY=0x2,PIPE=0x400,RESET_PATH=0x8,PRIVILEGED=0x10,DEBUG=0x40,SHELL=0x20,ATTACHED=0x200,QUIET=0x80,DETACHED=0x100,FOREIGN=0x1,WAIT=0x4\nc.LAYOUT:TIGHT=0x1,BACKGROUND=0x8,FOREGROUND=0x10,LOCK=0x40,LEFT=0x2,SQUARE=0x0,IGNORE_CURSOR=0x80,WIDE=0x6,TILE=0x100,RIGHT=0x4,EMBEDDED=0x20\nc.MFF:FOLDER=0x200,MODIFY=0x2,CREATE=0x4,READ=0x1,DEEP=0x1000,SELF=0x800,MOVED=0x10,ATTRIB=0x20,CLOSED=0x80,DELETE=0x8,OPENED=0x40,WRITE=0x2,FILE=0x400,UNMOUNT=0x100,RENAME=0x10\nc.MHF:DEFAULT=0x2,STRUCTURE=0x2,NULL=0x1,STATIC=0x4\nc.RP:ROOT_PATH=0x3,SYSTEM_PATH=0x2,MODULE_PATH=0x1\nc.STR:WILDCARD=0x4,MATCH_CASE=0x1,CASE=0x1,MATCH_LEN=0x2\nc.DEVICE:SOFTWARE=0x40,NETWORK=0x80,READ=0x8,TAPE=0x100,PRINTER=0x200,SCANNER=0x400,TEMPORARY=0x800,MEMORY=0x1000,MODEM=0x2000,USB=0x4000,PRINTER_3D=0x8000,COMPACT_DISC=0x1,HARD_DISK=0x2,FLOPPY_DISK=0x4,WRITE=0x10,SCANNER_3D=0x10000,REMOVABLE=0x20,REMOVEABLE=0x20\nc.IDF:SECTION=0x1,HOST=0x2,IGNORE_HOST=0x4\nc.STP:Y=0x2,Z=0x4,ANIM=0x8,X=0x1\nc.CLF:SHARED_ONLY=0x1,SHARED_OBJECTS=0x1,PRIVATE_ONLY=0x2,NO_OWNERSHIP=0x20,PROMOTE_INTEGRAL=0x4,XML_CONTENT=0x10,PUBLIC_OBJECTS=0x8\nc.MSF:UPDATE=0x2,NO_DUPLICATE=0x4,MESSAGE_ID=0x20,ADD=0x8,ADDRESS=0x10,WAIT=0x1\nc.CLIPMODE:COPY=0x2,CUT=0x1,PASTE=0x4\nc.LDF:CHECK_EXISTS=0x1\nc.LOC:FOLDER=0x1,VOLUME=0x2,DIRECTORY=0x1,FILE=0x3\nc.MTF:Y=0x2,Z=0x4,ANIM=0x8,RELATIVE=0x10,X=0x1\nc.FBK:MOVE_FILE=0x1,COPY_FILE=0x2,DELETE_FILE=0x3\nc.EDGE:TOP_RIGHT=0x20,BOTTOM_LEFT=0x40,BOTTOM_RIGHT=0x80,RIGHT=0x4,LEFT=0x2,ALL=0xff,TOP=0x1,BOTTOM=0x8,TOP_LEFT=0x10\nc.FL:FOLDER=0x8,EXCLUDE_FILES=0x1000,LINK=0x20,READ=0x4,BUFFER=0x40,APPROXIMATE=0x10,EXCLUDE_FOLDERS=0x2000,NEW=0x2,DEVICE=0x400,LOOP=0x80,STREAM=0x800,WRITE=0x1,FILE=0x100,RESET_DATE=0x200,DIRECTORY=0x8\nc.PMF:SYSTEM_NO_BREAK=0x1\n"
