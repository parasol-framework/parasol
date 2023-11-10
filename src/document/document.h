
class extDocument;

typedef int INDEX;
using SEGINDEX = int;

enum class RTD : UBYTE {
   NIL=0,
   OBJECT_TEMP,         // The object can be removed after parsing has finished
   OBJECT_UNLOAD,       // Default choice for object termination, terminates immediately
   OBJECT_UNLOAD_DELAY, // Use SendMessage() to terminate the object
   PERSISTENT_SCRIPT,   // The script can survive refreshes
   PERSISTENT_OBJECT    // The object can survive refreshes
};

DEFINE_ENUM_FLAG_OPERATORS(RTD)

enum class IXF : UBYTE {
   NIL        = 0x00,
   SIBLINGS   = 0x01,
   HOLDSTYLE  = 0x02,
   RESETSTYLE = 0x04,
   CLOSESTYLE = 0x08
};

DEFINE_ENUM_FLAG_OPERATORS(IXF)

enum class PXF : WORD {
   NIL       = 0,
   ARGS      = 0x0001,
   TRANSLATE = 0x0002
};

DEFINE_ENUM_FLAG_OPERATORS(PXF)

enum class LINK : UBYTE {
   NIL = 0,
   HREF,
   FUNCTION
};

DEFINE_ENUM_FLAG_OPERATORS(LINK)

enum {
   COND_NOT_EQUAL=1,
   COND_EQUAL,
   COND_LESS_THAN,
   COND_LESS_EQUAL,
   COND_GREATER_THAN,
   COND_GREATER_EQUAL
};

enum class ESC : char {
   NIL = 0,
   TEXT,
   FONT,
   FONTCOLOUR,
   UNDERLINE,
   BACKGROUND,
   INVERSE,
   VECTOR,
   LINK,
   TABDEF,
   PARAGRAPH_END,   // 10
   PARAGRAPH_START,
   LINK_END,
   ADVANCE,
   LIST_START,
   LIST_END,        // 15
   TABLE_START,
   TABLE_END,
   ROW,
   CELL,
   CELL_END,        // 20
   ROW_END,
   SET_MARGINS,
   INDEX_START,
   INDEX_END,
   XML,
   IMAGE,
   // End of list - NB: PLEASE UPDATE byteCode() IF YOU ADD NEW CODES
   END
};

DEFINE_ENUM_FLAG_OPERATORS(ESC)

enum class ULD : UBYTE {
   NIL             = 0,
   TERMINATE       = 0x01,
   KEEP_PARAMETERS = 0x02,
   REFRESH         = 0x04,
   REDRAW          = 0x08
};

DEFINE_ENUM_FLAG_OPERATORS(ULD)

enum class NL : char {
   NONE = 0,
   PARAGRAPH
};

DEFINE_ENUM_FLAG_OPERATORS(NL)

enum class WRAP : char {
   DO_NOTHING = 0,
   EXTEND_PAGE,
   WRAPPED
};

DEFINE_ENUM_FLAG_OPERATORS(WRAP)

enum {
   STATE_OUTSIDE=0,
   STATE_ENTERED,
   STATE_INSIDE
};

enum class IPF : ULONG {
   NIL = 0,
   NO_CONTENT  = 0x0001,
   STRIP_FEEDS = 0x0002,
   CHECK_ELSE  = 0x0004,
   // These flag values are in the upper word so that we can or them with IPF and TAG constants.
   FILTER_TABLE = 0x80000000, // The tag is restricted to use within <table> sections.
   FILTER_ROW   = 0X40000000, // The tag is restricted to use within <row> sections.
   FILTER_ALL   = 0xffff0000
};

DEFINE_ENUM_FLAG_OPERATORS(IPF)

enum class TAG : ULONG {
   NIL = 0,
   CHILDREN     = 0x00000001, // Children are compulsory for the tag to have an effect
   CONTENT      = 0x00000002, // Tag has a direct impact on text content or the page layout
   CONDITIONAL  = 0x00000004, // Tag is a conditional statement
   INSTRUCTION  = 0x00000008, // Tag is an executable instruction
   ROOT         = 0x00000010, // Tag is limited to use at the root of the document
   PARAGRAPH    = 0x00000020, // Tag results in paragraph formatting (will force some type of line break)
   OBJECTOK     = 0x00000040, // It is OK for this tag to be used within any object
   // These flag values are in the upper word so that we can or them with IPF and TAG constants.
   FILTER_TABLE = 0x80000000, // The tag is restricted to use within <table> sections.
   FILTER_ROW   = 0X40000000, // The tag is restricted to use within <row> sections.
   FILTER_ALL   = 0xffff0000
};

DEFINE_ENUM_FLAG_OPERATORS(TAG)

enum class TRF : ULONG {
   NIL      = 0,
   BREAK    = 0x00000001,
   CONTINUE = 0x00000002
};

DEFINE_ENUM_FLAG_OPERATORS(TRF)

//********************************************************************************************************************
// Tab is used to represent interactive entities within the document that can be tabbed to.

struct Tab {
   // The Ref is a UID for the Type, so you can use it to find the tab in the document stream
   LONG  Ref;        // For TT_OBJECT: ObjectID; TT_LINK: LinkID
   LONG  XRef;       // For TT_OBJECT: SurfaceID (if found)
   UBYTE Type;       // TT_LINK, TT_OBJECT
   bool  Active;     // true if the tabbable entity is active/visible
};

//********************************************************************************************************************

struct deLinkActivated {
   std::map<std::string, std::string> Values;  // All key-values associated with the link.
};

//********************************************************************************************************************
// Every instruction in the document stream is represented by a StreamCode entity.  The Code refers to what the thing
// is, while the UID hash refers to further information in the Codes table.

struct StreamCode {
   ESC Code;  // Type
   ULONG UID; // Lookup for the Codes table

   StreamCode() : Code(ESC::NIL), UID(0) { }
   StreamCode(ESC pCode, ULONG pID) : Code(pCode), UID(pID) { }
};

using RSTREAM = std::vector<StreamCode>;

//********************************************************************************************************************

class BaseCode {
public:
   ULONG UID;   // Unique identifier for lookup
   ESC Code = ESC::NIL; // Byte code

   BaseCode() { UID = glByteCodeID++; }
   BaseCode(ESC pCode) : Code(pCode) { UID = glByteCodeID++; }
};

//********************************************************************************************************************
// StreamChar provides indexing to specific characters in the stream.  It is designed to handle positional changes so
// that text string boundaries can be crossed without incident.
//
// The Index and Offset are set to -1 if the StreamChar is invalidated.

struct StreamChar {
   INDEX Index;     // A TEXT code position within the stream
   size_t Offset;   // Specific character offset within the escText.Text string

   StreamChar() : Index(-1), Offset(-1) { }
   StreamChar(INDEX pIndex, ULONG pOffset) : Index(pIndex), Offset(pOffset) { }
   StreamChar(INDEX pIndex) : Index(pIndex), Offset(0) { }

   bool operator==(const StreamChar &Other) const {
      return (this->Index == Other.Index) and (this->Offset == Other.Offset);
   }

   bool operator<(const StreamChar &Other) const {
      if (this->Index < Other.Index) return true;
      else if ((this->Index IS Other.Index) and (this->Offset < Other.Offset)) return true;
      else return false;
   }

   bool operator>(const StreamChar &Other) const {
      if (this->Index > Other.Index) return true;
      else if ((this->Index IS Other.Index) and (this->Offset > Other.Offset)) return true;
      else return false;
   }

   bool operator<=(const StreamChar &Other) const {
      if (this->Index < Other.Index) return true;
      else if ((this->Index IS Other.Index) and (this->Offset <= Other.Offset)) return true;
      else return false;
   }

   bool operator>=(const StreamChar &Other) const {
      if (this->Index > Other.Index) return true;
      else if ((this->Index IS Other.Index) and (this->Offset >= Other.Offset)) return true;
      else return false;
   }

   void operator+=(const LONG Value) {
      Offset += Value;
   }

   inline void reset() { Index = -1; Offset = -1; }
   inline bool valid() { return Index != -1; }
   inline bool valid(const RSTREAM &Stream) { return (Index != -1) and (Index < INDEX(Stream.size())); }

   inline void set(INDEX pIndex, ULONG pOffset) {
      Index  = pIndex;
      Offset = pOffset;
   }

   inline INDEX prevCode() {
      Index--;
      if (Index < 0) { Index = -1; Offset = -1; }
      else Offset = 0;
      return Index;
   }

   inline INDEX nextCode() {
      Offset = 0;
      Index++;
      return Index;
   }

   char getChar(extDocument *, const RSTREAM &);
   char getChar(extDocument *, const RSTREAM &, LONG Seek);
   char getPrevChar(extDocument *, const RSTREAM &);
   void eraseChar(extDocument *, RSTREAM &); // Erase a character OR an escape code.
   void nextChar(extDocument *, const RSTREAM &);
   void prevChar(extDocument *, const RSTREAM &);
};

//********************************************************************************************************************

typedef void TAGROUTINE (extDocument *, objXML *, XMLTag &, objXML::TAGS &, StreamChar &, IPF);
