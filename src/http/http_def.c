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
   { "Read", 0x00000001 },
   { "ReadWrite", 0x00000001 },
   { "Write", 0x00000001 },
   { NULL, 0 }
};

static const struct FieldDef clHTTPFlags[] = {
   { "Resume", 0x00000001 },
   { "Message", 0x00000002 },
   { "Moved", 0x00000004 },
   { "Redirected", 0x00000008 },
   { "NoHead", 0x00000010 },
   { "NoDialog", 0x00000020 },
   { "Raw", 0x00000040 },
   { "DebugSocket", 0x00000080 },
   { "RecvBuffer", 0x00000100 },
   { "LogAll", 0x00000200 },
   { "SSL", 0x00000400 },
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
   { AC_Activate, HTTP_Activate },
   { AC_Deactivate, HTTP_Deactivate },
   { AC_Free, HTTP_Free },
   { AC_GetKey, HTTP_GetKey },
   { AC_Init, HTTP_Init },
   { AC_NewObject, HTTP_NewObject },
   { AC_SetKey, HTTP_SetKey },
   { AC_Write, HTTP_Write },
   { 0, NULL }
};

#undef MOD_IDL
#define MOD_IDL "c.HGS:AUTHENTICATED=0x2,AUTHENTICATING=0x1,COMPLETED=0x6,END=0x8,READING_CONTENT=0x5,READING_HEADER=0x0,SENDING_CONTENT=0x3,SEND_COMPLETE=0x4,TERMINATED=0x7\nc.HOM:DATA_FEED=0x0,READ=0x1,READ_WRITE=0x1,WRITE=0x1\nc.HTF:DEBUG_SOCKET=0x80,LOG_ALL=0x200,MESSAGE=0x2,MOVED=0x4,NO_DIALOG=0x20,NO_HEAD=0x10,RAW=0x40,RECV_BUFFER=0x100,REDIRECTED=0x8,RESUME=0x1,SSL=0x400\nc.HTM:B_COPY=0x7,B_DELETE=0x8,B_MOVE=0x9,B_PROP_FIND=0xa,B_PROP_PATCH=0xb,COPY=0xc,DELETE=0x4,GET=0x0,HEAD=0x3,LOCK=0xd,MK_COL=0x6,MOVE=0xe,NOTIFY=0xf,OPTIONS=0x10,POLL=0x11,POST=0x1,PROP_FIND=0x12,PROP_PATCH=0x13,PUT=0x2,SEARCH=0x14,SUBSCRIBE=0x15,TRACE=0x5,UNLOCK=0x16,UNSUBSCRIBE=0x17\nc.HTS:ACCEPTED=0xca,BAD_GATEWAY=0x1f6,BAD_REQUEST=0x190,CONFLICT=0x199,CONTINUE=0x64,CREATED=0xc9,ENTITY_TOO_LARGE=0x19d,EXPECTATION_FAILED=0x1a1,FORBIDDEN=0x193,FOUND=0x12e,GATEWAY_TIMEOUT=0x1f8,GONE=0x19a,LENGTH_REQUIRED=0x19b,METHOD_NOT_ALLOWED=0x195,MOVED_PERMANENTLY=0x12d,MULTIPLE_CHOICES=0x12c,NOT_ACCEPTABLE=0x196,NOT_FOUND=0x194,NOT_IMPLEMENTED=0x1f5,NOT_MODIFIED=0x130,NO_CONTENT=0xcc,OKAY=0xc8,OUT_OF_RANGE=0x1a0,PARTIAL_CONTENT=0xce,PAYMENT_REQUIRED=0x192,PRECONDITION_FAILED=0x19c,PROXY_AUTHENTICATION=0x197,REQUEST_TIMEOUT=0x198,RESET_CONTENT=0xcd,SEE_OTHER=0x12f,SERVER_ERROR=0x1f4,SERVICE_UNAVAILABLE=0x1f7,SWITCH_PROTOCOLS=0x65,TEMP_REDIRECT=0x133,UNAUTHORISED=0x191,UNSUPPORTED_MEDIA=0x19f,UNVERIFIED_CONTENT=0xcb,URI_TOO_LONG=0x19e,USE_PROXY=0x131,VERSION_UNSUPPORTED=0x1f9\n"
