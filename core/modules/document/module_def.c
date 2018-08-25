// Auto-generated by idl-c.fluid

static LONG docCharLength(struct rkDocument * Document, LONG Index);

#ifndef FDEF
#define FDEF static const struct FunctionField
#endif

FDEF argsCharLength[] = { { "Result", FD_LONG }, { "Document", FD_OBJECTPTR }, { "Index", FD_LONG }, { 0, 0 } };

const struct Function glFunctions[] = {
   { (APTR)docCharLength, "CharLength", argsCharLength },
   { NULL, NULL, NULL }
};

#undef MOD_IDL
#define MOD_IDL "s.DocStyle:lVersion,oDocument,oFont,eFontColour:RGB8,eFontUnderline:RGB8,lStyleFlags\ns.escFont:wIndex,wOptions,eColour:RGB8\ns.SurfaceClip:pNext:SurfaceClip,lLeft,lTop,lRight,lBottom\ns.style_status:eFontStyle:escFont,pTable:process_table,pList:escList,cFace[36],wPoint,ucBit,ucBit\ns.docdraw:pObject,lID\ns.DocTrigger:pNext:DocTrigger,pPrev:DocTrigger,rFunction\nc.DBE:BOTTOM=0x8,LEFT=0x2,RIGHT=0x4,TOP=0x1\nc.RIPPLE:VERSION=""20160601""\nc.DRT:REFRESH=0x5,LOST_FOCUS=0x7,BEFORE_LAYOUT=0x0,USER_CLICK_RELEASE=0x3,MAX=0xa,GOT_FOCUS=0x6,PAGE_PROCESSED=0x9,LEAVING_PAGE=0x8,USER_MOVEMENT=0x4,USER_CLICK=0x2,AFTER_LAYOUT=0x1\nc.DCF:UNRESTRICTED=0x40,EDIT=0x1,NO_SCROLLBARS=0x10,DISABLED=0x8,OVERWRITE=0x2,NO_LAYOUT_MSG=0x20,NO_SYS_KEYS=0x4\nc.FSO:STYLES=0x17,NO_WRAP=0x100,ANCHOR=0x80,ALIGN_CENTER=0x40,BOLD=0x1,ITALIC=0x2,PREFORMAT=0x8,CAPS=0x10,ALIGN_RIGHT=0x20,UNDERLINE=0x4\nc.TT:LINK=0x2,EDIT=0x3,OBJECT=0x1\nc.DEF:LINK_ACTIVATED=0x2,LOCATION=0x1\n"
