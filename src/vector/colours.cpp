
#include <parasol/modules/display.h>

static const std::map<ULONG, RGB8> glNamedColours = { // For vecReadPainter()
  { SVF_NONE,                 { 0, 0, 0, 0 } },
  { SVF_ALICEBLUE,            { 240,248,255, 255 } },
  { SVF_ANTIQUEWHITE,         { 250,235,215, 255 } },
  { SVF_AQUA,                 { 0,255,255, 255 } },
  { SVF_AQUAMARINE,           { 127,255,212, 255 } },
  { SVF_AZURE,                { 240,255,255, 255 } },
  { SVF_BEIGE,                { 245,245,220, 255 } },
  { SVF_BISQUE,               { 255,228,196, 255 } },
  { SVF_BLACK,                { 0,0,0, 255 } },
  { SVF_BLANCHEDALMOND,       { 255,235,205, 255 } },
  { SVF_BLUE,                 { 0,0,255, 255 } },
  { SVF_BLUEVIOLET,           { 138,43,226, 255 } },
  { SVF_BROWN,                { 165,42,42, 255 } },
  { SVF_BURLYWOOD,            { 222,184,135, 255 } },
  { SVF_CADETBLUE,            { 95,158,160, 255 } },
  { SVF_CHARTREUSE,           { 127,255,0, 255 } },
  { SVF_CHOCOLATE,            { 210,105,30, 255 } },
  { SVF_CORAL,                { 255,127,80, 255 } },
  { SVF_CORNFLOWERBLUE,       { 100,149,237, 255 } },
  { SVF_CORNSILK,             { 255,248,220, 255 } },
  { SVF_CRIMSON,              { 220,20,60, 255 } },
  { SVF_CYAN,                 { 0,255,255, 255 } },
  { SVF_DARKBLUE,             { 0,0,139, 255 } },
  { SVF_DARKCYAN,             { 0,139,139, 255 } },
  { SVF_DARKGOLDENROD,        { 184,134,11, 255 } },
  { SVF_DARKGRAY,             { 169,169,169, 255 } },
  { SVF_DARKGREEN,            { 0,100,0, 255 } },
  { SVF_DARKGREY,             { 169,169,169, 255 } },
  { SVF_DARKKHAKI,            { 189,183,107, 255 } },
  { SVF_DARKMAGENTA,          { 139,0,139, 255 } },
  { SVF_DARKOLIVEGREEN,       { 85,107,47, 255 } },
  { SVF_DARKORANGE,           { 255,140,0, 255 } },
  { SVF_DARKORCHID,           { 153,50,204, 255 } },
  { SVF_DARKRED,              { 139,0,0, 255 } },
  { SVF_DARKSALMON,           { 233,150,122, 255 } },
  { SVF_DARKSEAGREEN,         { 143,188,143, 255 } },
  { SVF_DARKSLATEBLUE,        { 72,61,139, 255 } },
  { SVF_DARKSLATEGRAY,        { 47,79,79, 255 } },
  { SVF_DARKSLATEGREY,        { 47,79,79, 255 } },
  { SVF_DARKTURQUOISE,        { 0,206,209, 255 } },
  { SVF_DARKVIOLET,           { 148,0,211, 255 } },
  { SVF_DEEPPINK,             { 255,20,147, 255 } },
  { SVF_DEEPSKYBLUE,          { 0,191,255, 255 } },
  { SVF_DIMGRAY,              { 105,105,105, 255 } },
  { SVF_DIMGREY,              { 105,105,105, 255 } },
  { SVF_DODGERBLUE,           { 30,144,255, 255 } },
  { SVF_FIREBRICK,            { 178,34,34, 255 } },
  { SVF_FLORALWHITE,          { 255,250,240, 255 } },
  { SVF_FORESTGREEN,          { 34,139,34, 255 } },
  { SVF_FUCHSIA,              { 255,0,255, 255 } },
  { SVF_GAINSBORO,            { 220,220,220, 255 } },
  { SVF_GHOSTWHITE,           { 248,248,255, 255 } },
  { SVF_GOLD,                 { 255,215,0, 255 } },
  { SVF_GOLDENROD,            { 218,165,32, 255 } },
  { SVF_GRAY,                 { 128,128,128, 255 } },
  { SVF_GREEN,                { 0,128,0, 255 } },
  { SVF_GREENYELLOW,          { 173,255,47, 255 } },
  { SVF_GREY,                 { 128,128,128, 255 } },
  { SVF_HONEYDEW,             { 240,255,240, 255 } },
  { SVF_HOTPINK,              { 255,105,180, 255 } },
  { SVF_INDIANRED,            { 205,92,92, 255 } },
  { SVF_INDIGO,               { 75,0,130, 255 } },
  { SVF_IVORY,                { 255,255,240, 255 } },
  { SVF_KHAKI,                { 240,230,140, 255 } },
  { SVF_LAVENDER,             { 230,230,250, 255 } },
  { SVF_LAVENDERBLUSH,        { 255,240,245, 255 } },
  { SVF_LAWNGREEN,            { 124,252,0, 255 } },
  { SVF_LEMONCHIFFON,         { 255,250,205, 255 } },
  { SVF_LIGHTBLUE,            { 173,216,230, 255 } },
  { SVF_LIGHTCORAL,           { 240,128,128, 255 } },
  { SVF_LIGHTCYAN,            { 224,255,255, 255 } },
  { SVF_LIGHTGOLDENRODYELLOW, { 250,250,210, 255 } },
  { SVF_LIGHTGRAY,            { 211,211,211, 255 } },
  { SVF_LIGHTGREEN,           { 144,238,144, 255 } },
  { SVF_LIGHTGREY,            { 211,211,211, 255 } },
  { SVF_LIGHTPINK,            { 255,182,193, 255 } },
  { SVF_LIGHTSALMON,          { 255,160,122, 255 } },
  { SVF_LIGHTSEAGREEN,        { 32,178,170, 255 } },
  { SVF_LIGHTSKYBLUE,         { 135,206,250, 255 } },
  { SVF_LIGHTSLATEGRAY,       { 119,136,153, 255 } },
  { SVF_LIGHTSLATEGREY,       { 119,136,153, 255 } },
  { SVF_LIGHTSTEELBLUE,       { 176,196,222, 255 } },
  { SVF_LIGHTYELLOW,          { 255,255,224, 255 } },
  { SVF_LIME,                 { 0,255,0, 255 } },
  { SVF_LIMEGREEN,            { 50,205,50, 255 } },
  { SVF_LINEN,                { 250,240,230, 255 } },
  { SVF_MAGENTA,              { 255,0,255, 255 } },
  { SVF_MAROON,               { 128,0,0, 255 } },
  { SVF_MEDIUMAQUAMARINE,     { 102,205,170, 255 } },
  { SVF_MEDIUMBLUE,           { 0,0,205, 255 } },
  { SVF_MEDIUMORCHID,         { 186,85,211, 255 } },
  { SVF_MEDIUMPURPLE,         { 147,112,219, 255 } },
  { SVF_MEDIUMSEAGREEN,       { 60,179,113, 255 } },
  { SVF_MEDIUMSLATEBLUE,      { 123,104,238, 255 } },
  { SVF_MEDIUMSPRINGGREEN,    { 0,250,154, 255 } },
  { SVF_MEDIUMTURQUOISE,      { 72,209,204, 255 } },
  { SVF_MEDIUMVIOLETRED,      { 199,21,133, 255 } },
  { SVF_MIDNIGHTBLUE,         { 25,25,112, 255 } },
  { SVF_MINTCREAM,            { 245,255,250, 255 } },
  { SVF_MISTYROSE,            { 255,228,225, 255 } },
  { SVF_MOCCASIN,             { 255,228,181, 255 } },
  { SVF_NAVAJOWHITE,          { 255,222,173, 255 } },
  { SVF_NAVY,                 { 0,0,128, 255 } },
  { SVF_OLDLACE,              { 253,245,230, 255 } },
  { SVF_OLIVE,                { 128,128,0, 255 } },
  { SVF_OLIVEDRAB,            { 107,142,35, 255 } },
  { SVF_ORANGE,               { 255,165,0, 255 } },
  { SVF_ORANGERED,            { 255,69,0, 255 } },
  { SVF_ORCHID,               { 218,112,214, 255 } },
  { SVF_PALEGOLDENROD,        { 238,232,170, 255 } },
  { SVF_PALEGREEN,            { 152,251,152, 255 } },
  { SVF_PALETURQUOISE,        { 175,238,238, 255 } },
  { SVF_PALEVIOLETRED,        { 219,112,147, 255 } },
  { SVF_PAPAYAWHIP,           { 255,239,213, 255 } },
  { SVF_PEACHPUFF,            { 255,218,185, 255 } },
  { SVF_PERU,                 { 205,133,63, 255 } },
  { SVF_PINK,                 { 255,192,203, 255 } },
  { SVF_PLUM,                 { 221,160,221, 255 } },
  { SVF_POWDERBLUE,           { 176,224,230, 255 } },
  { SVF_PURPLE,               { 128,0,128, 255 } },
  { SVF_RED,                  { 255,0,0, 255 } },
  { SVF_ROSYBROWN,            { 188,143,143, 255 } },
  { SVF_ROYALBLUE,            { 65,105,225, 255 } },
  { SVF_SADDLEBROWN,          { 139,69,19, 255 } },
  { SVF_SALMON,               { 250,128,114, 255 } },
  { SVF_SANDYBROWN,           { 244,164,96, 255 } },
  { SVF_SEAGREEN,             { 46,139,87, 255 } },
  { SVF_SEASHELL,             { 255,245,238, 255 } },
  { SVF_SIENNA,               { 160,82,45, 255 } },
  { SVF_SILVER,               { 192,192,192, 255 } },
  { SVF_SKYBLUE,              { 135,206,235, 255 } },
  { SVF_SLATEBLUE,            { 106,90,205, 255 } },
  { SVF_SLATEGRAY,            { 112,128,144, 255 } },
  { SVF_SLATEGREY,            { 112,128,144, 255 } },
  { SVF_SNOW,                 { 255,250,250, 255 } },
  { SVF_SPRINGGREEN,          { 0,255,127, 255 } },
  { SVF_STEELBLUE,            { 70,130,180, 255 } },
  { SVF_TAN,                  { 210,180,140, 255 } },
  { SVF_TEAL,                 { 0,128,128, 255 } },
  { SVF_THISTLE,              { 216,191,216, 255 } },
  { SVF_TOMATO,               { 255,99,71, 255 } },
  { SVF_TURQUOISE,            { 64,224,208, 255 } },
  { SVF_VIOLET,               { 238,130,238, 255 } },
  { SVF_WHEAT,                { 245,222,179, 255 } },
  { SVF_WHITE,                { 255,255,255, 255 } },
  { SVF_WHITESMOKE,           { 245,245,245, 255 } },
  { SVF_YELLOW,               { 255,255,0, 255 } },
  { SVF_YELLOWGREEN,          { 154,205,50, 255 } }
};

static const std::map<ULONG, RGB8> glAppColours = { // For vecReadPainter()
  { SVF_ActiveBorder,         { 0x20, 0x60, 0xf9, 255 } },
  { SVF_ActiveCaption,        { 0x29, 0x80, 0xb9, 255 } },
  { SVF_AppWorkspace,         { 120, 150, 150, 255 } },
  { SVF_Background,           { 40, 40, 50, 255 } },
  { SVF_ButtonFace,           { 230, 230, 230, 255 } },
  { SVF_ButtonHighlight,      { 255, 255, 255, 128 } },
  { SVF_ButtonShadow,         { 0, 0, 0, 128 } },
  { SVF_ButtonText,           { 0, 0, 0, 255 } },
  { SVF_CaptionText,          { 255, 255, 255, 255 } },
  { SVF_GrayText,             { 90, 90, 90, 255 } },
  { SVF_Highlight,            { 0x34, 0x98, 0xdb, 255 } },
  { SVF_HighlightText,        { 255, 255, 255, 255 } },
  { SVF_InactiveBorder,       { 0, 0, 0, 255 } },
  { SVF_InactiveCaption,      { 0, 0, 0, 255 } },
  { SVF_InactiveCaptionText,  { 0, 0, 0, 255 } },
  { SVF_InfoBackground,       { 220, 220, 220, 255 } },
  { SVF_InfoText,             { 0, 0, 0, 255 } },
  { SVF_Menu,                 { 220, 220, 220, 255 } },
  { SVF_MenuText,             { 0, 0, 0, 255 } },
  { SVF_Scrollbar,            { 0, 0, 0, 255 } },
  { SVF_ThreeDDarkShadow,     { 64, 64, 64, 255 } },
  { SVF_ThreeDFace,           { 230, 230, 230, 255 } },
  { SVF_ThreeDHighlight,      { 255, 255, 255, 255 } },
  { SVF_ThreeDLightShadow,    { 64, 64, 64, 255 } },
  { SVF_ThreeDShadow,         { 64, 64, 64, 255 } },
  { SVF_Window,               { 220, 220, 220, 255 } },
  { SVF_WindowFrame,          { 0x10, 0x40, 0xa0, 255 } },
  { SVF_WindowText,           { 0, 0, 0, 255 } }
};
