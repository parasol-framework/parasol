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
   { "Moved", 0x00000004 },
   { "Raw", 0x00000040 },
   { "NoHead", 0x00000010 },
   { "DebugSocket", 0x00000080 },
   { "Resume", 0x00000001 },
   { "SSL", 0x00000400 },
   { "Message", 0x00000002 },
   { "RecvBuffer", 0x00000100 },
   { "Redirected", 0x00000008 },
   { "Debug", 0x00000200 },
   { "NoDialog", 0x00000020 },
   { NULL, 0 }
};

static const struct FieldDef clHTTPStatus[] = {
   { "EntityTooLarge", 0x0000019d },
   { "NoContent", 0x000000cc },
   { "TempRedirect", 0x00000133 },
   { "SeeOther", 0x0000012f },
   { "OutOfRange", 0x000001a0 },
   { "ExpectationFailed", 0x000001a1 },
   { "RequestTimeout", 0x00000198 },
   { "Unauthorised", 0x00000191 },
   { "Continue", 0x00000064 },
   { "NotFound", 0x00000194 },
   { "VersionUnsupported", 0x000001f9 },
   { "UnsupportedMedia", 0x0000019f },
   { "MultipleChoices", 0x0000012c },
   { "Gone", 0x0000019a },
   { "ProxyAuthentication", 0x00000197 },
   { "LengthRequired", 0x0000019b },
   { "BadRequest", 0x00000190 },
   { "Accepted", 0x000000ca },
   { "MovedPermanently", 0x0000012d },
   { "PreconditionFailed", 0x0000019c },
   { "UseProxy", 0x00000131 },
   { "NotImplemented", 0x000001f5 },
   { "ServiceUnavailable", 0x000001f7 },
   { "Okay", 0x000000c8 },
   { "ServerError", 0x000001f4 },
   { "BadGateway", 0x000001f6 },
   { "GatewayTimeout", 0x000001f8 },
   { "Found", 0x0000012e },
   { "ResetContent", 0x000000cd },
   { "NotAcceptable", 0x00000196 },
   { "Created", 0x000000c9 },
   { "NotModified", 0x00000130 },
   { "SwitchProtocols", 0x00000065 },
   { "PaymentRequired", 0x00000192 },
   { "UnverifiedContent", 0x000000cb },
   { "Forbidden", 0x00000193 },
   { "MethodNotAllowed", 0x00000195 },
   { "PartialContent", 0x000000ce },
   { "UriTooLong", 0x0000019e },
   { "Conflict", 0x00000199 },
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

static const struct FieldDef clHTTPState[] = {
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
#define MOD_IDL "c.HTS:ENTITY_TOO_LARGE=0x19d,NO_CONTENT=0xcc,TEMP_REDIRECT=0x133,SEE_OTHER=0x12f,OUT_OF_RANGE=0x1a0,EXPECTATION_FAILED=0x1a1,REQUEST_TIMEOUT=0x198,UNAUTHORISED=0x191,CONTINUE=0x64,NOT_FOUND=0x194,VERSION_UNSUPPORTED=0x1f9,UNSUPPORTED_MEDIA=0x19f,NOT_MODIFIED=0x130,GONE=0x19a,PROXY_AUTHENTICATION=0x197,LENGTH_REQUIRED=0x19b,BAD_REQUEST=0x190,RESET_CONTENT=0xcd,MOVED_PERMANENTLY=0x12d,PRECONDITION_FAILED=0x19c,USE_PROXY=0x131,CONFLICT=0x199,URI_TOO_LONG=0x19e,CREATED=0xc9,SERVER_ERROR=0x1f4,BAD_GATEWAY=0x1f6,GATEWAY_TIMEOUT=0x1f8,METHOD_NOT_ALLOWED=0x195,FORBIDDEN=0x193,NOT_ACCEPTABLE=0x196,NOT_IMPLEMENTED=0x1f5,PAYMENT_REQUIRED=0x192,SWITCH_PROTOCOLS=0x65,ACCEPTED=0xca,UNVERIFIED_CONTENT=0xcb,SERVICE_UNAVAILABLE=0x1f7,MULTIPLE_CHOICES=0x12c,PARTIAL_CONTENT=0xce,OKAY=0xc8,FOUND=0x12e\nc.HTM:MOVE=0xe,SUBSCRIBE=0x15,B_COPY=0x7,B_DELETE=0x8,B_MOVE=0x9,PROP_FIND=0x12,COPY=0xc,UNSUBSCRIBE=0x17,DELETE=0x4,UNLOCK=0x16,NOTIFY=0xf,POLL=0x11,B_PROP_PATCH=0xb,PROP_PATCH=0x13,B_PROP_FIND=0xa,GET=0x0,SEARCH=0x14,LOCK=0xd,MK_COL=0x6,POST=0x1,OPTIONS=0x10,TRACE=0x5,PUT=0x2,HEAD=0x3\nc.HOM:READ=0x1,DATA_FEED=0x0,READ_WRITE=0x1,WRITE=0x1\nc.HTF:MOVED=0x4,RAW=0x40,NO_HEAD=0x10,DEBUG_SOCKET=0x80,RESUME=0x1,SSL=0x400,MESSAGE=0x2,RECV_BUFFER=0x100,REDIRECTED=0x8,DEBUG=0x200,NO_DIALOG=0x20\nc.HGS:READING_HEADER=0x0,SENDING_CONTENT=0x3,TERMINATED=0x7,READING_CONTENT=0x5,AUTHENTICATED=0x2,END=0x8,SEND_COMPLETE=0x4,COMPLETED=0x6,AUTHENTICATING=0x1\n"
