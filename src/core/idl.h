#undef MOD_IDL
#define MOD_IDL "s.InputEvent:pNext:InputEvent,dValue,xTimestamp,lRecipientID,lOverID,dAbsX,dAbsY,dX,dY,lDeviceID,lType,lFlags,lMask\ns.dcRequest:lItem,cPreference[4]\ns.dcAudio:lSize,lFormat\ns.dcKeyEntry:lFlags,lValue,xTimestamp,lUnicode\ns.dcDeviceInput:dValue,xTimestamp,lDeviceID,lFlags,lType\ns.DateTime:lYear,lMonth,lDay,lHour,lMinute,lSecond,lTimeZone\ns.HSV:dHue,dSaturation,dValue\ns.FRGB:fRed,fGreen,fBlue,fAlpha\ns.RGB8:ucRed,ucGreen,ucBlue,ucAlpha\ns.RGB16:uwRed,uwGreen,uwBlue,uwAlpha\ns.RGB32:ulRed,ulGreen,ulBlue,ulAlpha\ns.RGBPalette:lAmtColours,eCol:RGB8[256]\ns.ColourFormat:ucRedShift,ucGreenShift,ucBlueShift,ucAlphaShift,ucRedMask,ucGreenMask,ucBlueMask,ucAlphaMask,ucRedPos,ucGreenPos,ucBluePos,ucAlphaPos,ucBitsPerPixel\ns.ClipRectangle:lLeft,lTop,lRight,lBottom\ns.Edges:lLeft,lTop,lRight,lBottom\ns.ObjectSignal:oObject\ns.pfBase64Decode:ucStep,ucPlainChar,ucBit\ns.pfBase64Encode:ucStep,ucResult,lStepCount\ns.FunctionField:sName,ulType\ns.Function:pAddress,sName,pArgs:FunctionField\ns.FieldArray:sName,pGetField,pSetField,mArg,ulFlags\ns.FieldDef:sName,lValue\ns.SystemState:sPlatform,lConsoleFD,lCoreVersion,lCoreRevision,lStage\ns.Variable:ulType,lUnused,xLarge,dDouble,pPointer\ns.ActionArray:pRoutine,lActionCode\ns.ActionTable:ulHash,lSize,sName,pArgs:FunctionField\ns.ChildEntry:lObjectID,ulClassID\ns.Message:xTime,lUID,lType,lSize\ns.MemInfo:pStart,lObjectID,ulSize,lFlags,lMemoryID,wAccessCount\ns.CompressionFeedback:lFeedbackID,lIndex,sPath,sDest,xProgress,xOriginalSize,xCompressedSize,wYear,wMonth,wDay,wHour,wMinute,wSecond\ns.CompressedItem:xOriginalSize,xCompressedSize,pNext:CompressedItem,sPath,lPermissions,lUserID,lGroupID,lOthersID,lFlags,eCreated:DateTime,eModified:DateTime\ns.FileInfo:xSize,xTimeStamp,pNext:FileInfo,sName,lFlags,lPermissions,lUserID,lGroupID,eCreated:DateTime,eModified:DateTime\ns.DirInfo:pInfo:FileInfo\ns.FileFeedback:xSize,xPosition,sPath,sDest,lFeedbackID,cReserved[32]\ns.Field:mArg,pGetValue,pSetValue,pWriteValue,sName,ulFieldID,uwOffset,uwIndex,ulFlags\nc.AC:Activate=0x2,Clear=0x4,Clipboard=0x2d,CopyData=0x7,Custom=0x34,DataFeed=0x8,Deactivate=0x9,Disable=0x2f,DragDrop=0x10,Draw=0xa,END=0x35,Enable=0x30,Flush=0xb,Focus=0xc,Free=0xd,FreeWarning=0x5,GetVar=0xf,Hide=0x11,Init=0x12,Lock=0x13,LostFocus=0x14,Move=0x15,MoveToBack=0x16,MoveToFront=0x17,MoveToPoint=0x32,NewChild=0x18,NewObject=0x1a,NewOwner=0x19,Next=0x29,Prev=0x2a,Query=0x1c,Read=0x1d,Redimension=0x31,Redo=0x1b,Refresh=0x2e,Rename=0x1e,Reset=0x1f,Resize=0x20,SaveImage=0x21,SaveSettings=0xe,SaveToObject=0x22,Scroll=0x23,ScrollToPoint=0x33,Seek=0x24,SelectArea=0x3,SetField=0x2c,SetVar=0x25,Show=0x26,Signal=0x1,Sort=0x6,Undo=0x27,Unlock=0x28,Write=0x2b\nc.ALIGN:BOTTOM=0x20,CENTER=0xc,HORIZONTAL=0x4,LEFT=0x1,MIDDLE=0xc,RIGHT=0x2,TOP=0x10,VERTICAL=0x8\nc.CCF:AUDIO=0x200,COMMAND=0x1,DATA=0x400,DRAWABLE=0x2,EFFECT=0x4,FILESYSTEM=0x8,GRAPHICS=0x10,GUI=0x20,IO=0x40,MISC=0x800,MULTIMEDIA=0x2000,NETWORK=0x1000,SYSTEM=0x80,TOOL=0x100\nc.CF:DEFLATE=0x3,GZIP=0x1,ZLIB=0x2\nc.CLF:NO_OWNERSHIP=0x2,PROMOTE_INTEGRAL=0x1\nc.CLIPMODE:COPY=0x2,CUT=0x1,PASTE=0x4\nc.CMF:APPLY_SECURITY=0x20,CREATE_FILE=0x4,NEW=0x2,NO_LINKS=0x10,PASSWORD=0x1,READ_ONLY=0x8\nc.CNF:AUTO_SAVE=0x2,NEW=0x8,OPTIONAL_FILES=0x4,STRIP_QUOTES=0x1\nc.DATA:AUDIO=0x5,CONTENT=0xb,DEVICE_INPUT=0x3,FILE=0xa,IMAGE=0x7,INPUT_READY=0xc,RAW=0x2,RECEIPT=0x9,RECORD=0x6,REQUEST=0x8,TEXT=0x1,XML=0x4\nc.DEVICE:COMPACT_DISC=0x1,FLOPPY_DISK=0x4,HARD_DISK=0x2,MEMORY=0x1000,MODEM=0x2000,NETWORK=0x80,PRINTER=0x200,PRINTER_3D=0x8000,READ=0x8,REMOVABLE=0x20,REMOVEABLE=0x20,SCANNER=0x400,SCANNER_3D=0x10000,SOFTWARE=0x40,TAPE=0x100,TEMPORARY=0x800,USB=0x4000,WRITE=0x10\nc.DMF:FIXED_CENTER_X=0x100000,FIXED_CENTER_Y=0x200000,FIXED_DEPTH=0x1000,FIXED_HEIGHT=0x100,FIXED_RADIUS=0x2020000,FIXED_RADIUS_X=0x20000,FIXED_RADIUS_Y=0x2000000,FIXED_WIDTH=0x200,FIXED_X=0x4,FIXED_X_OFFSET=0x40,FIXED_Y=0x8,FIXED_Y_OFFSET=0x80,FIXED_Z=0x4000,HEIGHT=0x500,HEIGHT_FLAGS=0x5a0,HORIZONTAL_FLAGS=0xa55,RELATIVE_CENTER_X=0x40000,RELATIVE_CENTER_Y=0x80000,RELATIVE_DEPTH=0x2000,RELATIVE_HEIGHT=0x400,RELATIVE_RADIUS=0x1010000,RELATIVE_RADIUS_X=0x10000,RELATIVE_RADIUS_Y=0x1000000,RELATIVE_WIDTH=0x800,RELATIVE_X=0x1,RELATIVE_X_OFFSET=0x10,RELATIVE_Y=0x2,RELATIVE_Y_OFFSET=0x20,RELATIVE_Z=0x8000,STATUS_CHANGE=0xc00000,STATUS_CHANGE_H=0x400000,STATUS_CHANGE_V=0x800000,VERTICAL_FLAGS=0x5aa,WIDTH=0xa00,WIDTH_FLAGS=0xa50,X=0x5,X_OFFSET=0x50,Y=0xa,Y_OFFSET=0xa0\nc.DRL:DOWN=0x1,EAST=0x2,LEFT=0x3,NORTH=0x0,NORTH_EAST=0x4,NORTH_WEST=0x5,RIGHT=0x2,SOUTH=0x1,SOUTH_EAST=0x6,SOUTH_WEST=0x7,UP=0x0,WEST=0x3\nc.EDGE:ALL=0xff,BOTTOM=0x8,BOTTOM_LEFT=0x40,BOTTOM_RIGHT=0x80,LEFT=0x2,RIGHT=0x4,TOP=0x1,TOP_LEFT=0x10,TOP_RIGHT=0x20\nc.ERF:Delay=0x20000000,Notified=0x40000000\nc.ERR:AccessMemory=0x4b,AccessObject=0x53,AccessSemaphore=0x73,Activate=0x42,AddClass=0x41,AllocMemory=0x54,AllocSemaphore=0x72,AlreadyDefined=0xb6,AlreadyLocked=0x9c,Args=0x14,ArrayFull=0x2d,BadOwner=0x52,BufferOverflow=0x5c,Busy=0x8b,Cancelled=0x3,CardReaderUnavailable=0x9f,CardReaderUnknown=0x9d,Compression=0xad,ConnectionAborted=0x8c,ConnectionRefused=0x83,ConstraintViolation=0x88,Continue=0x5,CoreVersion=0x26,CreateFile=0x74,CreateObject=0x65,CreateResource=0xb2,DataSize=0x8a,Deactivated=0x9a,DeadLock=0x3e,Decompression=0xac,DeleteFile=0x75,DirEmpty=0x8,Disconnected=0x86,DoNotExpunge=0x30,DoesNotExist=0x78,DoubleInit=0x43,Draw=0x48,END=0xbe,EOF=0x7e,EmptyString=0x6d,EndOfFile=0x7e,EntryMissingHeader=0x3a,ExamineFailed=0x19,Exception=0xa3,ExceptionThreshold=0x9,ExclusiveDenied=0x53,ExecViolation=0x8f,Exists=0x7a,ExpectedFile=0x6f,ExpectedFolder=0xae,Failed=0xd,False=0x1,FieldNotSet=0x44,FieldSearch=0x32,FieldTypeMismatch=0x5a,File=0xe,FileDoesNotExist=0x12,FileExists=0x63,FileNotFound=0x12,FileReadFlag=0x46,FileWriteFlag=0x47,Finished=0x7e,Function=0xb5,GetField=0x56,GetSurfaceInfo=0x7d,HostNotFound=0x81,HostUnreachable=0x85,IdenticalPaths=0x79,IllegalActionAttempt=0x39,IllegalActionID=0x37,IllegalAddress=0x91,IllegalMethodID=0x36,Immutable=0xaf,InUse=0xc,Init=0x21,InitModule=0x11,InputOutput=0x94,IntegrityViolation=0x88,InvalidData=0xf,InvalidDimension=0x59,InvalidHTTPResponse=0xa1,InvalidHandle=0x96,InvalidObject=0x8e,InvalidPath=0x33,InvalidReference=0xa2,InvalidState=0x80,InvalidURI=0x82,InvalidValue=0x98,LimitedSuccess=0x2,ListChildren=0x6a,LoadModule=0x95,Lock=0x18,LockFailed=0x18,LockMutex=0xaa,LockRequired=0x9b,Locked=0x9c,Loop=0x62,LostClass=0x1a,LostOwner=0x2f,LowCapacity=0x20,MarkedForDeletion=0x35,Memory=0x1d,MemoryCorrupt=0x31,MemoryDoesNotExist=0x3d,MemoryInfo=0x66,Mismatch=0x5e,MissingClass=0x45,MissingClassName=0x2a,MissingPath=0x4c,ModuleInitFailed=0x3c,ModuleMissingInit=0x3b,ModuleMissingName=0x40,ModuleOpenFailed=0x38,NeedOwner=0x24,NeedWidthHeight=0x27,NegativeClassID=0x29,NegativeSubClassID=0x28,NetworkUnreachable=0x84,NewObject=0x55,NoAction=0x1b,NoData=0x15,NoFieldAccess=0x57,NoMatchingObject=0x4a,NoMediaInserted=0x9e,NoMemory=0xa,NoMethods=0x49,NoPermission=0x22,NoPointer=0xb,NoSearchResult=0x4e,NoStats=0x1f,NoSupport=0x1c,NotFound=0x10,NotInitialised=0x67,NotLocked=0x4d,NotPossible=0xb3,NothingDone=0x4,NullArgs=0x8d,ObjectCorrupt=0x50,ObjectExists=0x6e,Obsolete=0xb1,ObtainMethod=0x2c,Okay=0x0,OpenFile=0x76,OpenGL=0xa5,OutOfBounds=0x5f,OutOfData=0x7e,OutOfRange=0x2b,OutOfSpace=0x7c,OutsideMainThread=0xa6,OwnerNeedsBitmap=0x25,OwnerPassThrough=0x51,PermissionDenied=0x22,Permissions=0x22,ProxySSLTunnel=0xa0,Query=0x2e,Read=0x16,ReadFileToBuffer=0xb0,ReadOnly=0x77,ReallocMemory=0x61,Recursion=0x90,Redimension=0x71,Refresh=0x69,Resize=0x70,ResolvePath=0x64,ResolveSymbol=0xb4,ResourceExists=0x68,Retry=0x7,SanityFailure=0x7b,SchemaViolation=0x89,Search=0x10,Security=0x97,Seek=0x60,ServiceUnavailable=0x99,SetField=0x34,SetValueNotArray=0xbc,SetValueNotFunction=0xba,SetValueNotLookup=0xbd,SetValueNotNumeric=0xb7,SetValueNotObject=0xb9,SetValueNotPointer=0xbb,SetValueNotString=0xb8,SetVolume=0xab,Skip=0x6,SmallMask=0x6c,StatementUnsatisfied=0x4f,StringFormat=0x7f,Syntax=0x7f,SystemCall=0x6b,SystemCorrupt=0x23,SystemLocked=0x3f,TaskStillExists=0x87,Terminate=0x9,ThreadAlreadyActive=0xa4,ThreadNotLocked=0xa9,TimeOut=0x1e,True=0x0,UnbalancedXML=0x92,UndefinedField=0x44,UnrecognisedFieldType=0x5b,UnsupportedField=0x5d,UnsupportedOwner=0x52,UseSubClass=0xa7,VirtualVolume=0x58,WouldBlock=0x93,Write=0x17,WrongClass=0x8e,WrongObjectType=0x8e,WrongType=0xa8,WrongVersion=0x13\nc.EVG:ANDROID=0xd,APP=0xc,AUDIO=0x8,CLASS=0xb,DISPLAY=0x5,END=0xe,FILESYSTEM=0x1,GUI=0x4,HARDWARE=0x7,IO=0x6,NETWORK=0x2,POWER=0xa,SYSTEM=0x3,USER=0x9\nc.FBK:COPY_FILE=0x2,DELETE_FILE=0x3,MOVE_FILE=0x1\nc.FD:ALLOC=0x20,ARRAY=0x1000,ARRAYSIZE=0x80,BUFFER=0x200,BUFSIZE=0x80,BYTE=0x1000000,CPP=0x4000,CUSTOM=0x8000,DOUBLE=0x80000000,DOUBLERESULT=0x80000100,ERROR=0x800,FLAGS=0x40,FLOAT=0x10000000,FUNCTION=0x2000000,FUNCTIONPTR=0xa000000,I=0x400,INIT=0x400,INTEGRAL=0x2,LARGE=0x4000000,LARGERESULT=0x4000100,LONG=0x40000000,LONGRESULT=0x40000100,LOOKUP=0x80,OBJECT=0x1,OBJECTID=0x40000001,OBJECTPTR=0x8000001,PERCENTAGE=0x200000,POINTER=0x8000000,PRIVATE=0x10000,PTR=0x8000000,PTRBUFFER=0x8000200,PTRRESULT=0x8000100,PTRSIZE=0x80,PTR_DOUBLERESULT=0x88000100,PTR_LARGERESULT=0xc000100,PTR_LONGRESULT=0x48000100,R=0x100,READ=0x100,REQUIRED=0x4,RESOURCE=0x2000,RESULT=0x100,RGB=0x80000,RI=0x500,RW=0x300,STR=0x800000,STRING=0x800000,STRRESULT=0x800100,STRUCT=0x10,SYNONYM=0x20000,SYSTEM=0x10000,TAGS=0x400,UNSIGNED=0x40000,VARIABLE=0x20000000,VARTAGS=0x40,VIRTUAL=0x8,VOID=0x0,VOLATILE=0x0,W=0x200,WORD=0x400000,WRITE=0x200\nc.FDB:COMPRESS_FILE=0x2,DECOMPRESS_FILE=0x1,DECOMPRESS_OBJECT=0x4,REMOVE_FILE=0x3\nc.FDL:FEEDBACK=0x1\nc.FDT:ACCESSED=0x2,ARCHIVED=0x3,CREATED=0x1,MODIFIED=0x0\nc.FFR:ABORT=0x2,CONTINUE=0x0,OKAY=0x0,SKIP=0x1\nc.FL:APPROXIMATE=0x10,BUFFER=0x40,DEVICE=0x400,DIRECTORY=0x8,EXCLUDE_FILES=0x1000,EXCLUDE_FOLDERS=0x2000,FILE=0x100,FOLDER=0x8,LINK=0x20,LOOP=0x80,NEW=0x2,READ=0x4,RESET_DATE=0x200,STREAM=0x800,WRITE=0x1\nc.FOF:SMART_NAMES=0x1\nc.IDTYPE:FUNCTION=0x3,GLOBAL=0x2,MESSAGE=0x1\nc.JET:ABS_X=0x1f,ABS_Y=0x20,ANALOG2_X=0x18,ANALOG2_Y=0x19,ANALOG2_Z=0x1a,ANALOG_X=0x15,ANALOG_Y=0x16,ANALOG_Z=0x17,BUTTON_1=0x3,BUTTON_10=0xc,BUTTON_2=0x4,BUTTON_3=0x5,BUTTON_4=0x6,BUTTON_5=0x7,BUTTON_6=0x8,BUTTON_7=0x9,BUTTON_8=0xa,BUTTON_9=0xb,BUTTON_SELECT=0x10,BUTTON_START=0xf,DEVICE_TILT_X=0x24,DEVICE_TILT_Y=0x25,DEVICE_TILT_Z=0x26,DIGITAL_X=0x1,DIGITAL_Y=0x2,DISPLAY_EDGE=0x27,END=0x28,ENTERED=0x21,ENTERED_SURFACE=0x21,LEFT=0x22,LEFT_BUMPER_1=0x11,LEFT_BUMPER_2=0x12,LEFT_SURFACE=0x22,LMB=0x3,MMB=0x5,PEN_TILT_HORIZONTAL=0x1e,PEN_TILT_VERTICAL=0x1d,PRESSURE=0x23,RIGHT_BUMPER_1=0x13,RIGHT_BUMPER_2=0x14,RMB=0x4,TRIGGER_LEFT=0xd,TRIGGER_RIGHT=0xe,WHEEL=0x1b,WHEEL_TILT=0x1c\nc.JTYPE:ANALOG=0x20,ANCHORED=0x2,BUTTON=0x80,DBL_CLICK=0x200,DIGITAL=0x10,DRAGGED=0x4,DRAG_ITEM=0x800,EXT_MOVEMENT=0x40,FEEDBACK=0x8,MOVEMENT=0x100,REPEATED=0x400,SECONDARY=0x1\nc.KEY:A=0x1,APOSTROPHE=0x2b,AT=0x8a,B=0x2,BACK=0x86,BACKSPACE=0x69,BACK_SLASH=0x2f,BREAK=0x7b,C=0x3,CALL=0x87,CAMERA=0x89,CANCEL=0x7a,CAPS_LOCK=0x46,CLEAR=0x6e,COMMA=0x2c,D=0x4,DELETE=0x6d,DOT=0x2d,DOWN=0x61,E=0x5,EIGHT=0x22,END=0x72,END_CALL=0x88,ENTER=0x6b,EQUALS=0x27,ESCAPE=0x6c,EXECUTE=0x74,F=0x6,F1=0x4c,F10=0x55,F11=0x56,F12=0x57,F13=0x58,F14=0x59,F15=0x5a,F16=0x5b,F17=0x5c,F18=0x80,F19=0x81,F2=0x4d,F20=0x82,F3=0x4e,F4=0x4f,F5=0x50,F6=0x51,F7=0x52,F8=0x53,F9=0x54,FIND=0x79,FIVE=0x1f,FORWARD=0x90,FOUR=0x1e,G=0x7,H=0x8,HELP=0x43,HOME=0x6f,I=0x9,INSERT=0x75,J=0xa,K=0xb,L=0xc,LEFT=0x63,LENS_FOCUS=0x8c,LESS_GREATER=0x5f,LIST_END=0x96,L_ALT=0x48,L_COMMAND=0x4a,L_CONTROL=0x41,L_SHIFT=0x44,L_SQUARE=0x28,M=0xd,MACRO=0x5d,MENU=0x78,MINUS=0x26,MUTE=0x92,N=0xe,NEXT=0x8e,NINE=0x23,NP_0=0x31,NP_1=0x32,NP_2=0x33,NP_3=0x34,NP_4=0x35,NP_5=0x36,NP_6=0x37,NP_7=0x38,NP_8=0x39,NP_9=0x3a,NP_BAR=0x3d,NP_DECIMAL=0x3f,NP_DIVIDE=0x40,NP_DOT=0x3f,NP_ENTER=0x7e,NP_MINUS=0x3e,NP_MULTIPLY=0x3b,NP_PLUS=0x3c,NP_PLUS_MINUS=0x5e,NP_SEPARATOR=0x3d,NUM_LOCK=0x7c,O=0xf,ONE=0x1b,P=0x10,PAGE_DOWN=0x71,PAGE_UP=0x70,PAUSE=0x65,PERIOD=0x2d,PLAY=0x95,PLUS=0x8b,POUND=0x94,POWER=0x68,PREVIOUS=0x8f,PRINT=0x47,PRT_SCR=0x7d,Q=0x11,R=0x12,REDO=0x77,REVERSE_QUOTE=0x25,REWIND=0x91,RIGHT=0x62,R_ALT=0x49,R_COMMAND=0x4b,R_CONTROL=0x42,R_SHIFT=0x45,R_SQUARE=0x29,S=0x13,SCR_LOCK=0x64,SELECT=0x73,SEMI_COLON=0x2a,SEVEN=0x21,SIX=0x20,SLASH=0x2e,SLEEP=0x67,SPACE=0x30,STAR=0x93,STOP=0x8d,SYSRQ=0x7f,T=0x14,TAB=0x6a,THREE=0x1d,TWO=0x1c,U=0x15,UNDO=0x76,UP=0x60,V=0x16,VOLUME_DOWN=0x85,VOLUME_UP=0x84,W=0x17,WAKE=0x66,WIN_CONTROL=0x83,X=0x18,Y=0x19,Z=0x1a,ZERO=0x24\nc.KQ:ALT=0x60,ALTGR=0x40,CAPS_LOCK=0x4,COMMAND=0x180,CONTROL=0x18,CTRL=0x18,DEAD_KEY=0x10000,INFO=0x3c04,INSTRUCTION_KEYS=0x78,L_ALT=0x20,L_COMMAND=0x80,L_CONTROL=0x8,L_CTRL=0x8,L_SHIFT=0x1,NOT_PRINTABLE=0x2000,NUM_LOCK=0x8000,NUM_PAD=0x200,PRESSED=0x1000,QUALIFIERS=0x1fb,RELEASED=0x800,REPEAT=0x400,R_ALT=0x40,R_COMMAND=0x100,R_CONTROL=0x10,R_CTRL=0x10,R_SHIFT=0x2,SCR_LOCK=0x4000,SHIFT=0x3,WIN_CONTROL=0x20000\nc.LAYOUT:BACKGROUND=0x8,EMBEDDED=0x20,FOREGROUND=0x10,IGNORE_CURSOR=0x80,LEFT=0x2,LOCK=0x40,RIGHT=0x4,SQUARE=0x0,TIGHT=0x1,TILE=0x100,WIDE=0x6\nc.LDF:CHECK_EXISTS=0x1\nc.LOC:DIRECTORY=0x1,FILE=0x3,FOLDER=0x1,VOLUME=0x2\nc.MAX:NAME_LEN=0x20\nc.MEM:AUDIO=0x8,CALLER=0x800000,CODE=0x10,DATA=0x0,DELETE=0x1000,EXCLUSIVE=0x800,HIDDEN=0x100000,MANAGED=0x1,NO_BLOCK=0x2000,NO_BLOCKING=0x2000,NO_CLEAR=0x40000,NO_LOCK=0x400,NO_POOL=0x20,OBJECT=0x200,READ=0x10000,READ_WRITE=0x30000,STRING=0x100,TEXTURE=0x4,TMP_LOCK=0x40,UNTRACKED=0x80,VIDEO=0x2,WRITE=0x20000\nc.MFF:ATTRIB=0x20,CLOSED=0x80,CREATE=0x4,DEEP=0x1000,DELETE=0x8,FILE=0x400,FOLDER=0x200,MODIFY=0x2,MOVED=0x10,OPENED=0x40,READ=0x1,RENAME=0x10,SELF=0x800,UNMOUNT=0x100,WRITE=0x2\nc.MHF:DEFAULT=0x2,STATIC=0x1,STRUCTURE=0x2\nc.MOF:LINK_LIBRARY=0x1,STATIC=0x2,SYSTEM_PROBE=0x4\nc.MOVE:ALL=0xf,DOWN=0x1,LEFT=0x4,RIGHT=0x8,UP=0x2\nc.MSF:ADD=0x8,ADDRESS=0x10,MESSAGE_ID=0x20,NO_DUPLICATE=0x4,UPDATE=0x2,WAIT=0x1\nc.MSGID:ACTION=0x63,BREAK=0x64,COMMAND=0x65,CORE_END=0x64,DEBUG=0x5f,EVENT=0x5e,FREE=0x62,QUIT=0x3e8,THREAD_ACTION=0x5b,THREAD_CALLBACK=0x5c,VALIDATE_PROCESS=0x5d,WAIT_FOR_OBJECTS=0x5a\nc.MTF:ANIM=0x8,RELATIVE=0x10,X=0x1,Y=0x2,Z=0x4\nc.NETMSG:END=0x1,START=0x0\nc.NF:COLLECT=0x80,FREE=0x10,FREE_ON_UNLOCK=0x8,INITIALISED=0x2,INTEGRAL=0x4,MESSAGE=0x200,NAME=0x80000000,PRIVATE=0x0,RECLASSED=0x100,SIGNALLED=0x400,SUPPRESS_LOG=0x40,TIMER_SUB=0x20,UNIQUE=0x40000000,UNTRACKED=0x1\nc.OPF:ARGS=0x100,COMPILED_AGAINST=0x400,CORE_VERSION=0x2,DEPRECATED=0x1,DETAIL=0x10,ERROR=0x200,MAX_DEPTH=0x8,MODULE_PATH=0x2000,OPTIONS=0x4,PRIVILEGED=0x800,ROOT_PATH=0x4000,SCAN_MODULES=0x8000,SHOW_ERRORS=0x80,SHOW_IO=0x40,SHOW_MEMORY=0x20,SYSTEM_PATH=0x1000\nc.PERMIT:ALL_DELETE=0x888,ALL_EXEC=0x444,ALL_READ=0x111,ALL_WRITE=0x222,ARCHIVE=0x2000,DELETE=0x8,EVERYONE_ACCESS=0xfff,EVERYONE_DELETE=0x888,EVERYONE_EXEC=0x444,EVERYONE_READ=0x111,EVERYONE_READWRITE=0x333,EVERYONE_WRITE=0x222,EXEC=0x4,GROUP=0xf0,GROUPID=0x10000,GROUP_DELETE=0x80,GROUP_EXEC=0x40,GROUP_READ=0x10,GROUP_WRITE=0x20,HIDDEN=0x1000,INHERIT=0x20000,NETWORK=0x80000,OFFLINE=0x40000,OTHERS=0xf00,OTHERS_DELETE=0x800,OTHERS_EXEC=0x400,OTHERS_READ=0x100,OTHERS_WRITE=0x200,PASSWORD=0x4000,READ=0x1,USER=0xf,USERID=0x8000,USER_EXEC=0x4,USER_READ=0x1,USER_WRITE=0x2,WRITE=0x2\nc.PMF:SYSTEM_NO_BREAK=0x1\nc.PTC:CROSSHAIR=0xa,CUSTOM=0x17,DEFAULT=0x1,DRAGGABLE=0x18,END=0x19,HAND=0x10,HAND_LEFT=0x11,HAND_RIGHT=0x12,INVISIBLE=0x16,MAGNIFIER=0xf,NO_CHANGE=0x0,PAINTBRUSH=0x14,SIZE_BOTTOM=0x9,SIZE_BOTTOM_LEFT=0x2,SIZE_BOTTOM_RIGHT=0x3,SIZE_LEFT=0x6,SIZE_RIGHT=0x7,SIZE_TOP=0x8,SIZE_TOP_LEFT=0x4,SIZE_TOP_RIGHT=0x5,SIZING=0xc,SLEEP=0xb,SPLIT_HORIZONTAL=0xe,SPLIT_VERTICAL=0xd,STOP=0x15,TEXT=0x13\nc.RDF:ARCHIVE=0x2000,DATE=0x2,FILE=0x8,FILES=0x8,FOLDER=0x10,FOLDERS=0x10,HIDDEN=0x100,LINK=0x40,OPENDIR=0x4000,PERMISSIONS=0x4,QUALIFIED=0x200,QUALIFY=0x200,READ_ALL=0x1f,READ_ONLY=0x1000,SIZE=0x1,STREAM=0x800,TAGS=0x80,TIME=0x2,VIRTUAL=0x400,VOLUME=0x20\nc.RES:CONSOLE_FD=0x2,CORE_IDL=0x8,CPU_SPEED=0x16,DISPLAY_DRIVER=0x5,EXCEPTION_HANDLER=0x11,FREE_MEMORY=0x17,FREE_SWAP=0x1,JNI_ENV=0xe,KEY_STATE=0x3,LOG_DEPTH=0xd,LOG_LEVEL=0xa,MAX_PROCESSES=0xc,NET_PROCESSING=0x12,OPEN_INFO=0x10,PARENT_CONTEXT=0x9,PRIVILEGED=0x7,PRIVILEGED_USER=0x6,PROCESS_STATE=0x13,STATIC_BUILD=0x18,THREAD_ID=0xf,TOTAL_MEMORY=0x14,TOTAL_SHARED_MEMORY=0xb,TOTAL_SWAP=0x15,USER_ID=0x4\nc.RFD:ALLOW_RECURSION=0x20,ALWAYS_CALL=0x100,EXCEPT=0x2,READ=0x4,RECALL=0x80,REMOVE=0x8,SOCKET=0x40,STOP_RECURSE=0x10,WRITE=0x1\nc.RP:MODULE_PATH=0x1,ROOT_PATH=0x3,SYSTEM_PATH=0x2\nc.RSF:APPROXIMATE=0x4,CASE_SENSITIVE=0x20,CHECK_VIRTUAL=0x2,NO_DEEP_SCAN=0x8,NO_FILE_CHECK=0x1,PATH=0x10\nc.SCF:EXIT_ON_ERROR=0x1,LOG_ALL=0x2\nc.SEEK:CURRENT=0x1,END=0x2,RELATIVE=0x3,START=0x0\nc.STP:ANIM=0x8,X=0x1,Y=0x2,Z=0x4\nc.STR:CASE=0x1,MATCH_CASE=0x1,MATCH_LEN=0x2,WILDCARD=0x4\nc.STT:FLOAT=0x2,HEX=0x3,NUMBER=0x1,STRING=0x4\nc.THF:AUTO_FREE=0x1\nc.TOI:ANDROID_ASSETMGR=0x4,ANDROID_CLASS=0x3,ANDROID_ENV=0x2,LOCAL_CACHE=0x0,LOCAL_STORAGE=0x1\nc.TSF:ATTACHED=0x100,DETACHED=0x80,FOREIGN=0x1,LOG_ALL=0x20,PIPE=0x200,PRIVILEGED=0x8,QUIET=0x40,RESET_PATH=0x4,SHELL=0x10,WAIT=0x2\nc.TSTATE:PAUSED=0x1,RUNNING=0x0,STOPPING=0x2,TERMINATED=0x3\nc.VAS:CASE_SENSITIVE=0xf,CLOSE_DIR=0x6,CREATE_LINK=0x11,DELETE=0x3,DEREGISTER=0x1,DRIVER_SIZE=0x12,GET_DEVICE_INFO=0xb,GET_INFO=0xa,IDENTIFY_FILE=0xc,IGNORE_FILE=0x9,MAKE_DIR=0xd,OPEN_DIR=0x5,READ_LINK=0x10,RENAME=0x4,SAME_FILE=0xe,SCAN_DIR=0x2,TEST_PATH=0x7,WATCH_PATH=0x8\nc.VLF:API=0x20,BRANCH=0x1,CRITICAL=0x8,DEBUG=0x80,ERROR=0x2,EXTAPI=0x40,FUNCTION=0x200,INFO=0x10,TRACE=0x100,WARNING=0x4\nc.VOLUME:HIDDEN=0x4,PRIORITY=0x2,REPLACE=0x1,SYSTEM=0x8\n"
