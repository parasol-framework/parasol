// Auto-generated by idl-c.fluid

static const struct FieldDef clHTTPMethod[] = {
   { "Get", 0x00000000 },
   { "Post", 0x00000001 },
   { "Put", 0x00000002 },
   { "Head", 0x00000003 },
   { "Delete", 0x00000004 },
   { "Trace", 0x00000005 },
   { "MkCol", 0x00000006 },
   { "BCopy", 0x00000007 },
   { "BDelete", 0x00000008 },
   { "BMove", 0x00000009 },
   { "BPropFind", 0x0000000a },
   { "BPropPatch", 0x0000000b },
   { "Copy", 0x0000000c },
   { "Lock", 0x0000000d },
   { "Move", 0x0000000e },
   { "Notify", 0x0000000f },
   { "Options", 0x00000010 },
   { "Poll", 0x00000011 },
   { "PropFind", 0x00000012 },
   { "PropPatch", 0x00000013 },
   { "Search", 0x00000014 },
   { "Subscribe", 0x00000015 },
   { "Unlock", 0x00000016 },
   { "Unsubscribe", 0x00000017 },
   { NULL, 0 }
};

static const struct FieldDef clHTTPObjectMode[] = {
   { "DataFeed", 0x00000000 },
   { "ReadWrite", 0x00000001 },
   { "Read", 0x00000001 },
   { "Write", 0x00000001 },
   { NULL, 0 }
};

static const struct FieldDef clHTTPFlags[] = {
   { "Redirected", 0x00000008 },
   { "Debug", 0x00000200 },
   { "NoHead", 0x00000010 },
   { "Resume", 0x00000001 },
   { "Raw", 0x00000040 },
   { "Message", 0x00000002 },
   { "DebugSocket", 0x00000080 },
   { "SSL", 0x00000400 },
   { "Moved", 0x00000004 },
   { "NoDialog", 0x00000020 },
   { "RecvBuffer", 0x00000100 },
   { NULL, 0 }
};

static const struct FieldDef clHTTPStatus[] = {
   { "Continue", 0x00000064 },
   { "SwitchProtocols", 0x00000065 },
   { "Okay", 0x000000c8 },
   { "Created", 0x000000c9 },
   { "Accepted", 0x000000ca },
   { "UnverifiedContent", 0x000000cb },
   { "NoContent", 0x000000cc },
   { "ResetContent", 0x000000cd },
   { "PartialContent", 0x000000ce },
   { "MultipleChoices", 0x0000012c },
   { "MovedPermanently", 0x0000012d },
   { "Found", 0x0000012e },
   { "SeeOther", 0x0000012f },
   { "NotModified", 0x00000130 },
   { "UseProxy", 0x00000131 },
   { "TempRedirect", 0x00000133 },
   { "BadRequest", 0x00000190 },
   { "Unauthorised", 0x00000191 },
   { "PaymentRequired", 0x00000192 },
   { "Forbidden", 0x00000193 },
   { "NotFound", 0x00000194 },
   { "MethodNotAllowed", 0x00000195 },
   { "NotAcceptable", 0x00000196 },
   { "ProxyAuthentication", 0x00000197 },
   { "RequestTimeout", 0x00000198 },
   { "Conflict", 0x00000199 },
   { "Gone", 0x0000019a },
   { "LengthRequired", 0x0000019b },
   { "PreconditionFailed", 0x0000019c },
   { "EntityTooLarge", 0x0000019d },
   { "UriTooLong", 0x0000019e },
   { "UnsupportedMedia", 0x0000019f },
   { "OutOfRange", 0x000001a0 },
   { "ExpectationFailed", 0x000001a1 },
   { "ServerError", 0x000001f4 },
   { "NotImplemented", 0x000001f5 },
   { "BadGateway", 0x000001f6 },
   { "ServiceUnavailable", 0x000001f7 },
   { "GatewayTimeout", 0x000001f8 },
   { "VersionUnsupported", 0x000001f9 },
   { NULL, 0 }
};

static const struct FieldDef clHTTPDatatype[] = {
   { "Text", 0x00000001 },
   { "Raw", 0x00000002 },
   { "DeviceInput", 0x00000003 },
   { "Xml", 0x00000004 },
   { "Audio", 0x00000005 },
   { "Record", 0x00000006 },
   { "Image", 0x00000007 },
   { "Request", 0x00000008 },
   { "Receipt", 0x00000009 },
   { "File", 0x0000000a },
   { "Content", 0x0000000b },
   { "InputReady", 0x0000000c },
   { NULL, 0 }
};

static const struct FieldDef clHTTPCurrentState[] = {
   { "ReadingHeader", 0x00000000 },
   { "Authenticating", 0x00000001 },
   { "Authenticated", 0x00000002 },
   { "SendingContent", 0x00000003 },
   { "SendComplete", 0x00000004 },
   { "ReadingContent", 0x00000005 },
   { "Completed", 0x00000006 },
   { "Terminated", 0x00000007 },
   { NULL, 0 }
};

static const struct ActionArray clHTTPActions[] = {
   { AC_ActionNotify, (APTR)HTTP_ActionNotify },
   { AC_Activate, (APTR)HTTP_Activate },
   { AC_Deactivate, (APTR)HTTP_Deactivate },
   { AC_Free, (APTR)HTTP_Free },
   { AC_GetVar, (APTR)HTTP_GetVar },
   { AC_Init, (APTR)HTTP_Init },
   { AC_NewObject, (APTR)HTTP_NewObject },
   { AC_SetVar, (APTR)HTTP_SetVar },
   { AC_Write, (APTR)HTTP_Write },
   { 0, 0 }
};

#undef MOD_IDL
#define MOD_IDL "c.HOM:READ_WRITE=0x1,READ=0x1,DATA_FEED=0x0,WRITE=0x1\nc.HTS:METHOD_NOT_ALLOWED=0x195,NOT_ACCEPTABLE=0x196,PROXY_AUTHENTICATION=0x197,REQUEST_TIMEOUT=0x198,CONFLICT=0x199,GONE=0x19a,LENGTH_REQUIRED=0x19b,PRECONDITION_FAILED=0x19c,ENTITY_TOO_LARGE=0x19d,URI_TOO_LONG=0x19e,UNSUPPORTED_MEDIA=0x19f,OUT_OF_RANGE=0x1a0,EXPECTATION_FAILED=0x1a1,SERVER_ERROR=0x1f4,NOT_IMPLEMENTED=0x1f5,BAD_GATEWAY=0x1f6,SERVICE_UNAVAILABLE=0x1f7,GATEWAY_TIMEOUT=0x1f8,VERSION_UNSUPPORTED=0x1f9,CONTINUE=0x64,SWITCH_PROTOCOLS=0x65,OKAY=0xc8,CREATED=0xc9,ACCEPTED=0xca,UNVERIFIED_CONTENT=0xcb,NO_CONTENT=0xcc,RESET_CONTENT=0xcd,PARTIAL_CONTENT=0xce,MULTIPLE_CHOICES=0x12c,MOVED_PERMANENTLY=0x12d,FOUND=0x12e,SEE_OTHER=0x12f,NOT_MODIFIED=0x130,USE_PROXY=0x131,TEMP_REDIRECT=0x133,BAD_REQUEST=0x190,UNAUTHORISED=0x191,PAYMENT_REQUIRED=0x192,FORBIDDEN=0x193,NOT_FOUND=0x194\nc.HTF:RESUME=0x1,MESSAGE=0x2,MOVED=0x4,REDIRECTED=0x8,NO_HEAD=0x10,SSL=0x400,NO_DIALOG=0x20,DEBUG=0x200,RAW=0x40,RECV_BUFFER=0x100,DEBUG_SOCKET=0x80\nc.HGS:SEND_COMPLETE=0x4,READING_CONTENT=0x5,COMPLETED=0x6,TERMINATED=0x7,END=0x8,READING_HEADER=0x0,AUTHENTICATING=0x1,AUTHENTICATED=0x2,SENDING_CONTENT=0x3\nc.HTM:NOTIFY=0xf,OPTIONS=0x10,POLL=0x11,PROP_FIND=0x12,PROP_PATCH=0x13,SEARCH=0x14,SUBSCRIBE=0x15,UNLOCK=0x16,UNSUBSCRIBE=0x17,TRACE=0x5,MK_COL=0x6,DELETE=0x4,GET=0x0,POST=0x1,PUT=0x2,HEAD=0x3,B_COPY=0x7,B_DELETE=0x8,B_MOVE=0x9,B_PROP_FIND=0xa,B_PROP_PATCH=0xb,COPY=0xc,LOCK=0xd,MOVE=0xe\n"
