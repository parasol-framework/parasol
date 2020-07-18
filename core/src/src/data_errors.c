
CSTRING glHeaders[] = {
  NULL,
  "AccessMemory",
  "Action",
  "ActionMsg",
  "AddMemEntry",
  "ActionNotify",
  "Activate",
  "AddClass",
  "AddPrivateObject",
  "AddSharedObject",
  "AllocMemory",
  "Volume",
  "AccessObject",
  "Bitmap",
  "Broadcast",
  "Broadcaster",
  "CheckAction",
  "CheckInit",
  "CheckObjectExists",
  "Class",
  "Clear",
  "CloneMemory",
  "CloseFeed",
  "CloseModule",
  "UpdateTimer",
  "IdentifyFile",
  "CopyArguments",
  "CopyData",
  "DataFeed",
  "Deactivate",
  "DeleteVolume",
  "Draw",
  "Error",
  "Event",
  "ExpungeModule",
  "File",
  "FileSystem",
  "FindChildren",
  "FindClass",
  "ResolveClassName",
  "FindField",
  "FindObject",
  "Flush",
  "Focus",
  "Free",
  "FreeClass",
  "ReleaseObject",
  "FreeResource",
  "FreeResourceID",
  "GetClassID",
  "GetField",
  "GetName",
  "Hide",
  "Init",
  "InitModule",
  "Kernel",
  "ListChildren",
  "LoadFromObject",
  "ManageAction",
  "MemoryIDInfo",
  "Method",
  "GetDeviceInfo",
  "Module",
  "Move",
  "MoveToFront",
  "MoveToBack",
  "NewClass",
  "NewObject",
  "NotifySubscribers",
  "ObtainMethod",
  "OpenFeed",
  "OpenModule",
  "Parasol",
  "Pointer",
  "ProcessMessages",
  "Query",
  "Read",
  "ReadConfig",
  "Realloc",
  "ReceiveMessage",
  "ReleaseMemory",
  "RemoveObject",
  "Rename",
  "Reset",
  "Resize",
  "ResolvePath",
  "SaveImage",
  "SaveToObject",
  "Screen",
  "Script",
  "Scroll",
  "Seek",
  "SetOwner",
  "SetField",
  "SetVar",
  "SendMessage",
  "Show",
  "SetName",
  "Strings",
  "StrReplace",
  "StrSearch",
  "SubscribeAction",
  "SubscribeTimer",
  "Subscriber",
  "TotalChildren", // OBSOLETE
  "UserClick",
  "UserMovement",
  "Unhook",
  "UnsubscribeAction",
  "Write",
  "NewChild",
  "CopyLocation",
  "SubscribeEvent",
  "UnsubscribeEvent",
  "Function",
  "FastFindObject",
  "Clipboard",
  "Refresh",
  "XML",
  "ConvertFontCoords",
  "CalcStringWidth",
  "CreateObject",
  "CheckMemoryExists",
  "SleepingTask",
  "SetContext",
  "UnsubscribeFeed",
  "UnsubscribeTimer",
  "AllocateID",
  "GetContainerID",
  "ActionTags",
  "GetFields",
  "SetFields",
  "GetFieldVariable",
  "SetFieldEval",
  "LogError",
  "ReallocMemory",
  "MemoryPtrInfo",
  "Redimension",
  "ScanMessages",
  "Timer",
  "AccessSemaphore",
  "ReleaseSemaphore",
  "AllocSemaphore",
  "FreeSemaphore",
  "SetDimension",
  "GetDimension",
  "GetVar",
  "MoveToPoint",
  "ScrollToPoint",
  "SetWidth",
  "SetHeight",
  "SetX",
  "SetY",
  "SetXOffset",
  "SetYOffset",
  "GetWidth",
  "GetHeight",
  "GetX",
  "GetY",
  "GetXOffset",
  "GetYOffset",
  "ListTasks",
  "LostFocus",
  "UpdateMessage",
  "ValidateProcess",
  "UserClickRelease",
  "ListObjects",
  "FindPrivateObject",
  "RegisterFD",
  "AccessSurface",
  "ReleaseSurface",
  "LockBitmap",
  "UnlockBitmap",
  "CopySurface",
  "GetSurfaceInfo",
  "SetRegion",
  "Sort",
  "TranslateText",
  "TranslateRefresh",
  "SaveSettings",
  "Custom",
  "MonitorFile",
  "GetFileInfo",
  "FindPublicObject",
  "BuildJumpTable",
  "CheckResident",
  "Android",
  "Display",
  "KernelSleep",
  "LoadModules",
  "LoadClasses",
  "CopyArea",
  "CopyStretch",
  "SetCursor",
  "UnlockCursor",
  "DrawRectangle",
  "OpenDir",
  "ScanDir"
};

CSTRING glMessages[ERR_END] = {
 "Operation successful.",
 "The result is false.",
 "Limited success.",
 "Operation cancelled.",
 "There is nothing to process.",
 "Please continue processing.",
 "Skip this operation.",
 "Retry the operation (could not succeed at this time).",
 "The folder is empty.",
 "Please terminate future calls.",
 "Not enough memory available.",
 "Required pointer not present.",
 "Previous allocations have not been freed.",
 "General failure.",
 "File error, e.g. file not found.",
 "There is an error in the given data.",
 "A search routine in this function failed.",
 "Trouble initialising/using a module.",
 "File not found.",
 "Wrong version, or version not supported.",
 "Invalid arguments passed to function.",
 "No data is available for use.",
 "Error reading data from file.",
 "Error writing data to file.",
 "Failed to lock a required resource.",
 "Could not examine folder or file.",
 "The object has lost its class reference.",
 "The object does not support this operation.",
 "This request is not supported.",
 "General memory error.",
 "Function timed-out before successful completion.",
 "This object has lost its stats structure.",
 "Array is at low capacity.",
 "Error in Init()ialising an object.",
 "General security violation.",
 "The operating system has been badly corrupted.",
 "I need a container to operate correctly.",
 "My container needs to have a Bitmap field.",
 "I need a newer version of the kernel.",
 "The Width and/or Height field values need to be set.",
 "The Object ID is negative.",
 "The Class ID is negative",
 "You have not given the Class a name.",
 "A specified number is outside of the valid range.",
 "A call to ObtainMethod() failed.",
 "Array has reached capacity and cannot be expanded.",
 "Attempt to Query() failed.",
 "The object has lost its container.",
 "Please do not expunge me.",
 "The memory block is corrupt.",
 "I could not find the field that you requested.",
 "Invalid file or folder path detected.",
 "An unspecific error occurred during a SetField() operation.",
 "A resource cannot be accessed as it is marked for deletion.",
 "Illegal method ID (number outside of valid range).",
 "Illegal action ID (number outside of valid range).",
 "Module failed in its Open routine.",
 "Illegal attempt to execute an action on an object that belongs to another Task.",
 "The ModEntry->Header is missing.",
 "Module has not defined support for Init().",
 "Module failed its Initialisation routine.",
 "Memory block does not exist.",
 "Dead-lock detected - procedure aborted.",
 "Part of the system is unreachable due to a persistent lock.",
 "Module has not declared an official name.",
 "An error occurred while creating a new class.",
 "Error while Activate()ing an object.",
 "Warning - Attempt to initialise an object twice.",
 "A vital field has not been set in this object.",
 "The class could not be found in the system.",
 "The File flag FL_READ was not set on initialisation.",
 "The File flag FL_WRITE was not set on initialisation.",
 "An error occurred while drawing the object.",
 "The class does not export any methods.",
 "No matching object was found for the given object ID.",
 "Access to a shared memory block was denied.",
 "The object is missing a setting in the Path or Location field.",
 "There is no exclusive lock on this object.",
 "The search yielded no results.",
 "The tested statement was not satisfied.",
 "The object structure is corrupt or has not been initialised.",
 "Container pass through notification.",
 "The given container is not supported by this object.",
 "An attempt to gain exclusive access to a shared object failed.",
 "A call to AllocMemory() failed to create a new memory block.",
 "A call to NewObject() failed to produce a new object.",
 "A call to GetField() failed to retrieve a field value.",
 "Access to a field was denied.",
 "ResolvePath() failed to resolve the path because it is a virtual reference.",
 "A dimension specification is invalid.",
 "Field type mismatch while getting or setting a field.",
 "A field type ID (e.g. LONG, FLOAT) is not a recognised type of the system.",
 "A buffer overflow has occurred.",
 "The specified field is not supported by the object's class.",
 "A mis-match has been detected that prevents further processing.",
 "An out-of-bounds error has occurred.",
 "An error occurred during a seek operation.",
 "The reallocation of a memory block failed.",
 "An infinite loop was detected.",
 "The destination file or folder already exists.",
 "A volume could not be resolved.",
 "A call to CreateObject() failed.",
 "Failed to obtain information on a memory block (MemoryInfo() failed).",
 "The object has not been initialised.",
 "A new resource could not be created because a matching resource exists.",
 "An attempt to Refresh() an object failed.",
 "An error occurred in a call to ListChildren().",
 "A call to the underlying system's native functions has failed.",
 "The size of the mask is smaller than the source Bitmap's dimensions.",
 "A required string value contains no characters.",
 "Object exists.",
 "The operation expected a path to a file.",
 "Failure during the resize of an object.",
 "Failure during the redimensioning of an object.",
 "Failure in attempting to allocate a semaphore.",
 "Failed to access a semaphore.",
 "A new file could not be created.",
 "Deletion of a folder or file failed (e.g. permissions, read-only media).",
 "The file could not be opened.",
 "A delete or write operation failed due to read-only status.",
 "Resource does not exist.",
 "The destination path is the same as the source path.",
 "Resource exists.",
 "A sanity check failed.",
 "Out of space.  There is no available room to complete the request.",
 "GetSurfaceInfo() failed.",
 "Operation finished.",
 "Invalid syntax detected.",
 "Object was in an incorrect state for the operation.",
 "The internet host name could not be resolved.",
 "Invalid Uniform Resource Identifier.",
 "The remote host refused the connection.",
 "The network was unreachable.",
 "No route to host.",
 "The connection between client and server was terminated.",
 "Task/Process still exists and is running.",
 "Referential integrity / constraint violation.",
 "Record changes would violate the DB schema.",
 "The size of a data chunk or buffer is incorrect.",
 "Operation cannot complete because system is busy with an earlier operation.",
 "Attempt to connect to server aborted.",
 "Function call missing argument value(s)",
 "An object is not of the required type.",
 "Execution violation - function must be called from the task that owns the object.",
 "Detected a recursive function call.",
 "Address pointer is outside of the task's memory map.",
 "The XML tags do not balance.",
 "The requested operation would block if it were performed at this time.",
 "A non-specific I/O error has occurred.",
 "Failed to load a module binary.",
 "A required handle is invalid.",
 "A non-specific security violation has occurred.",
 "A required value was not within expected constraints.",
 "The required service process is not running.",
 "The object must be activated with the Activate action.",
 "A lock is required before accessing this functionality.",
 "Object or resource has already been locked.",
 "The requested smart card reader was not found in the available list of card readers.",
 "No media is inserted in the device.",
 "The requested smart card reader is in use.",
 "Failed to establish an SSL connection with the proxy server",
 "A valid HTTP response was not received from the server",
 "The reference is invalid",
 "An exception was caught",
 "The global instance is permanently locked.",
 "Failure has been reported from OpenGL",
 "An operation has been attempted that is only possible from within the main thread.",
 "Requested to use a registered sub-class (not an error).",
 "The expected type was not provided.",
 "A call to AllocMutex() or AllocSharedMutex() failed.",
 "An attempt to lock a mutex failed.",
 "A call to SetVolume() failed.",
 "Unspecified failure during decompression of a data stream.",
 "Unspecified failure during compression of a data stream.",
 "The operation expected a folder path.",
 "This operation would violate the object's immutable status.",
 "A call to ReadFile() failed.",
 "This feature is no longer available (obsolete).",
 "Failed to create a new resource.",
 "It is not possible to perform the requested operation."
};

const LONG glTotalHeaders = ARRAYSIZE(glHeaders);
const LONG glTotalMessages = ARRAYSIZE(glMessages);
