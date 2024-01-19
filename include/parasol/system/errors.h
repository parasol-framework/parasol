#pragma once

// Name:      system/errors.h
// Copyright: Paul Manias © 1996-2024
// Generator: idl-c

// Universal error codes

#define ERR_Okay 0
#define ERR_True 0
#define ERR_False 1
#define ERR_LimitedSuccess 2
#define ERR_Cancelled 3
#define ERR_NothingDone 4
#define ERR_Continue 5
#define ERR_Skip 6
#define ERR_Retry 7
#define ERR_DirEmpty 8
#define ERR_Terminate 9
#define ERR_ExceptionThreshold 9
#define ERR_NoMemory 10
#define ERR_NoPointer 11
#define ERR_InUse 12
#define ERR_Failed 13
#define ERR_File 14
#define ERR_InvalidData 15
#define ERR_Search 16
#define ERR_NotFound 16
#define ERR_InitModule 17
#define ERR_FileNotFound 18
#define ERR_FileDoesNotExist 18
#define ERR_WrongVersion 19
#define ERR_Args 20
#define ERR_NoData 21
#define ERR_Read 22
#define ERR_Write 23
#define ERR_LockFailed 24
#define ERR_Lock 24
#define ERR_ExamineFailed 25
#define ERR_LostClass 26
#define ERR_NoAction 27
#define ERR_NoSupport 28
#define ERR_Memory 29
#define ERR_TimeOut 30
#define ERR_NoStats 31
#define ERR_LowCapacity 32
#define ERR_Init 33
#define ERR_NoPermission 34
#define ERR_Permissions 34
#define ERR_PermissionDenied 34
#define ERR_SystemCorrupt 35
#define ERR_NeedOwner 36
#define ERR_OwnerNeedsBitmap 37
#define ERR_CoreVersion 38
#define ERR_NeedWidthHeight 39
#define ERR_NegativeSubClassID 40
#define ERR_NegativeClassID 41
#define ERR_MissingClassName 42
#define ERR_OutOfRange 43
#define ERR_ObtainMethod 44
#define ERR_ArrayFull 45
#define ERR_Query 46
#define ERR_LostOwner 47
#define ERR_DoNotExpunge 48
#define ERR_MemoryCorrupt 49
#define ERR_FieldSearch 50
#define ERR_InvalidPath 51
#define ERR_SetField 52
#define ERR_MarkedForDeletion 53
#define ERR_IllegalMethodID 54
#define ERR_IllegalActionID 55
#define ERR_ModuleOpenFailed 56
#define ERR_IllegalActionAttempt 57
#define ERR_EntryMissingHeader 58
#define ERR_ModuleMissingInit 59
#define ERR_ModuleInitFailed 60
#define ERR_MemoryDoesNotExist 61
#define ERR_DeadLock 62
#define ERR_SystemLocked 63
#define ERR_ModuleMissingName 64
#define ERR_AddClass 65
#define ERR_Activate 66
#define ERR_DoubleInit 67
#define ERR_UndefinedField 68
#define ERR_FieldNotSet 68
#define ERR_MissingClass 69
#define ERR_FileReadFlag 70
#define ERR_FileWriteFlag 71
#define ERR_Draw 72
#define ERR_NoMethods 73
#define ERR_NoMatchingObject 74
#define ERR_AccessMemory 75
#define ERR_MissingPath 76
#define ERR_NotLocked 77
#define ERR_NoSearchResult 78
#define ERR_StatementUnsatisfied 79
#define ERR_ObjectCorrupt 80
#define ERR_OwnerPassThrough 81
#define ERR_UnsupportedOwner 82
#define ERR_BadOwner 82
#define ERR_ExclusiveDenied 83
#define ERR_AccessObject 83
#define ERR_AllocMemory 84
#define ERR_NewObject 85
#define ERR_GetField 86
#define ERR_NoFieldAccess 87
#define ERR_VirtualVolume 88
#define ERR_InvalidDimension 89
#define ERR_FieldTypeMismatch 90
#define ERR_UnrecognisedFieldType 91
#define ERR_BufferOverflow 92
#define ERR_UnsupportedField 93
#define ERR_Mismatch 94
#define ERR_OutOfBounds 95
#define ERR_Seek 96
#define ERR_ReallocMemory 97
#define ERR_Loop 98
#define ERR_FileExists 99
#define ERR_ResolvePath 100
#define ERR_CreateObject 101
#define ERR_MemoryInfo 102
#define ERR_NotInitialised 103
#define ERR_ResourceExists 104
#define ERR_Refresh 105
#define ERR_ListChildren 106
#define ERR_SystemCall 107
#define ERR_SmallMask 108
#define ERR_EmptyString 109
#define ERR_ObjectExists 110
#define ERR_ExpectedFile 111
#define ERR_Resize 112
#define ERR_Redimension 113
#define ERR_AllocSemaphore 114
#define ERR_AccessSemaphore 115
#define ERR_CreateFile 116
#define ERR_DeleteFile 117
#define ERR_OpenFile 118
#define ERR_ReadOnly 119
#define ERR_DoesNotExist 120
#define ERR_IdenticalPaths 121
#define ERR_Exists 122
#define ERR_SanityFailure 123
#define ERR_OutOfSpace 124
#define ERR_GetSurfaceInfo 125
#define ERR_Finished 126
#define ERR_EOF 126
#define ERR_EndOfFile 126
#define ERR_OutOfData 126
#define ERR_Syntax 127
#define ERR_StringFormat 127
#define ERR_InvalidState 128
#define ERR_HostNotFound 129
#define ERR_InvalidURI 130
#define ERR_ConnectionRefused 131
#define ERR_NetworkUnreachable 132
#define ERR_HostUnreachable 133
#define ERR_Disconnected 134
#define ERR_TaskStillExists 135
#define ERR_IntegrityViolation 136
#define ERR_ConstraintViolation 136
#define ERR_SchemaViolation 137
#define ERR_DataSize 138
#define ERR_Busy 139
#define ERR_ConnectionAborted 140
#define ERR_NullArgs 141
#define ERR_InvalidObject 142
#define ERR_WrongObjectType 142
#define ERR_WrongClass 142
#define ERR_ExecViolation 143
#define ERR_Recursion 144
#define ERR_IllegalAddress 145
#define ERR_UnbalancedXML 146
#define ERR_WouldBlock 147
#define ERR_InputOutput 148
#define ERR_LoadModule 149
#define ERR_InvalidHandle 150
#define ERR_Security 151
#define ERR_InvalidValue 152
#define ERR_ServiceUnavailable 153
#define ERR_Deactivated 154
#define ERR_LockRequired 155
#define ERR_AlreadyLocked 156
#define ERR_Locked 156
#define ERR_CardReaderUnknown 157
#define ERR_NoMediaInserted 158
#define ERR_CardReaderUnavailable 159
#define ERR_ProxySSLTunnel 160
#define ERR_InvalidHTTPResponse 161
#define ERR_InvalidReference 162
#define ERR_Exception 163
#define ERR_ThreadAlreadyActive 164
#define ERR_OpenGL 165
#define ERR_OutsideMainThread 166
#define ERR_UseSubClass 167
#define ERR_WrongType 168
#define ERR_ThreadNotLocked 169
#define ERR_LockMutex 170
#define ERR_SetVolume 171
#define ERR_Decompression 172
#define ERR_Compression 173
#define ERR_ExpectedFolder 174
#define ERR_Immutable 175
#define ERR_ReadFileToBuffer 176
#define ERR_Obsolete 177
#define ERR_CreateResource 178
#define ERR_NotPossible 179
#define ERR_ResolveSymbol 180
#define ERR_Function 181
#define ERR_AlreadyDefined 182
#define ERR_SetValueNotNumeric 183
#define ERR_SetValueNotString 184
#define ERR_SetValueNotObject 185
#define ERR_SetValueNotFunction 186
#define ERR_SetValueNotPointer 187
#define ERR_SetValueNotArray 188
#define ERR_SetValueNotLookup 189
#define ERR_END 190

// Special error flags

#define ERF_Delay 536870912
#define ERF_Notified 1073741824

