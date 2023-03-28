
/*****************************************************************************
** Internal: xml_unescape()
**
** Converts XML escape codes in the source string to their relevant character values.  Because XML escape codes are
** always larger than their UTF-8 equivalents, there is no need for buffer management.  This is true even when
** referencing very large unicode code values (e.g. &#7ffffffff produces fewer characters when converted to UTF-8).
**
** If an unknown escape sequence is encountered, this routine will treat all the characters that are involved as normal
** text.
*/

#define ESCAPE(a,b) { (a), (b), sizeof((a))-1 }

static const struct {
   CSTRING Escape; // Escape code
   UWORD Value;   // Unicode value
   UWORD Length;
} glHTML[] = {
  // This list must be maintained in alphabetical order
  ESCAPE("AElig",   0xC6),
  ESCAPE("Aacute",  0xC1),
  ESCAPE("Acirc",   0xC2),
  ESCAPE("Agrave",  0xC0),
  ESCAPE("Alpha",   0x391),
  ESCAPE("Aring",   0xC5),
  ESCAPE("Atilde",  0xC3),
  ESCAPE("Auml",    0xC4),
  ESCAPE("Beta",    0x392),
  ESCAPE("Ccedil",  0xC7),
  ESCAPE("Chi",     0x3A7),
  ESCAPE("Dagger",  0x2021),
  ESCAPE("Delta",   0x394),
  ESCAPE("ETH",     0xD0),
  ESCAPE("Eacute",  0xC9),
  ESCAPE("Ecirc",   0xCA),
  ESCAPE("Egrave",  0xC8),
  ESCAPE("Epsilon", 0x395),
  ESCAPE("Eta",     0x397),
  ESCAPE("Euml",    0xCB),
  ESCAPE("Gamma",   0x393),
  ESCAPE("Iacute",  0xCD),
  ESCAPE("Icirc",   0xCE),
  ESCAPE("Igrave",  0xCC),
  ESCAPE("Iota",    0x399),
  ESCAPE("Iuml",    0xCF),
  ESCAPE("Kappa",   0x39A),
  ESCAPE("Lambda",  0x39B),
  ESCAPE("Mu",      0x39C),
  ESCAPE("Ntilde",  0xD1),
  ESCAPE("Nu",      0x39D),
  ESCAPE("OElig",   0x152),
  ESCAPE("Oacute",  0xD3),
  ESCAPE("Ocirc",   0xD4),
  ESCAPE("Ograve",  0xD2),
  ESCAPE("Omega",   0x3A9),
  ESCAPE("Omicron", 0x39F),
  ESCAPE("Oslash",  0xD8),
  ESCAPE("Otilde",  0xD5),
  ESCAPE("Ouml",    0xD6),
  ESCAPE("Phi",     0x3A6),
  ESCAPE("Pi",      0x3A0),
  ESCAPE("Prime",   0x2033),
  ESCAPE("Psi",     0x3A8),
  ESCAPE("Rho",     0x3A1),
  ESCAPE("Scaron",  0x160),
  ESCAPE("Sigma",   0x3A3),
  ESCAPE("THORN",   0xDE),
  ESCAPE("Tau",     0x3A4),
  ESCAPE("Theta",   0x398),
  ESCAPE("Uacute",  0xDA),
  ESCAPE("Ucirc",   0xDB),
  ESCAPE("Ugrave",  0xD9),
  ESCAPE("Upsilon", 0x3A5),
  ESCAPE("Uuml",    0xDC),
  ESCAPE("Xi",      0x39E),
  ESCAPE("Yacute",  0xDD),
  ESCAPE("Yuml",    0x178),
  ESCAPE("Zeta",    0x396),
  ESCAPE("aacute",  0xE1),
  ESCAPE("acirc",   0xE2),
  ESCAPE("acute",   0xB4),
  ESCAPE("aelig",   0xE6),
  ESCAPE("agrave",  0xE0),
  ESCAPE("alefsym", 0x2135),
  ESCAPE("alpha",   0x3B1),
  ESCAPE("and",     0x2227),
  ESCAPE("ang",     0x2220),
  ESCAPE("aring",   0xE5),
  ESCAPE("asymp",   0x2248),
  ESCAPE("atilde",  0xE3),
  ESCAPE("auml",    0xE4),
  ESCAPE("bdquo",   0x201E),
  ESCAPE("beta",    0x3B2),
  ESCAPE("brvbar",  0xA6),
  ESCAPE("bull",    0x2022),
  ESCAPE("cap",     0x2229),
  ESCAPE("ccedil",  0xE7),
  ESCAPE("cedil",   0xB8),
  ESCAPE("cent",    0xA2),
  ESCAPE("chi",     0x3C7),
  ESCAPE("circ",    0x2C6),
  ESCAPE("clubs",   0x2663),
  ESCAPE("cong",    0x2245),
  ESCAPE("copy",    0xA9),
  ESCAPE("crarr",   0x21B5),
  ESCAPE("cup",     0x222A),
  ESCAPE("curren",  0xA4),
  ESCAPE("dArr",    0x21D3),
  ESCAPE("dagger",  0x2020),
  ESCAPE("darr",    0x2193),
  ESCAPE("deg",     0xB0),
  ESCAPE("delta",   0x3B4),
  ESCAPE("diams",   0x2666),
  ESCAPE("divide",  0xF7),
  ESCAPE("eacute",  0xE9),
  ESCAPE("ecirc",   0xEA),
  ESCAPE("egrave",  0xE8),
  ESCAPE("empty",   0x2205),
  ESCAPE("emsp",    0x2003),
  ESCAPE("ensp",    0x2002),
  ESCAPE("epsilon", 0x3B5),
  ESCAPE("equiv",   0x2261),
  ESCAPE("eta",     0x3B7),
  ESCAPE("eth",     0xF0),
  ESCAPE("euml",    0xEB),
  ESCAPE("euro",    0x20AC),
  ESCAPE("exist",   0x2203),
  ESCAPE("fnof",    0x192),
  ESCAPE("forall",  0x2200),
  ESCAPE("frac12",  0xBD),
  ESCAPE("frac14",  0xBC),
  ESCAPE("frac34",  0xBE),
  ESCAPE("frasl",   0x2044),
  ESCAPE("gamma",   0x3B3),
  ESCAPE("ge",      0x2265),
  ESCAPE("gt",      0x3E),
  ESCAPE("hArr",    0x21D4),
  ESCAPE("harr",    0x2194),
  ESCAPE("hearts",  0x2665),
  ESCAPE("hellip",  0x2026),
  ESCAPE("iacute",  0xED),
  ESCAPE("icirc",   0xEE),
  ESCAPE("iexcl",   0xA1),
  ESCAPE("igrave",  0xEC),
  ESCAPE("image",   0x2111),
  ESCAPE("infin",   0x221E),
  ESCAPE("int",     0x222B),
  ESCAPE("iota",    0x3B9),
  ESCAPE("iquest",  0xBF),
  ESCAPE("isin",    0x2208),
  ESCAPE("iuml",    0xEF),
  ESCAPE("kappa",   0x3BA),
  ESCAPE("lArr",    0x21D0),
  ESCAPE("lambda",  0x3BB),
  ESCAPE("lang",    0x2329),
  ESCAPE("laquo",   0xAB),
  ESCAPE("larr",    0x2190),
  ESCAPE("lceil",   0x2308),
  ESCAPE("ldquo",   0x201C),
  ESCAPE("le",      0x2264),
  ESCAPE("lfloor",  0x230A),
  ESCAPE("lowast",  0x2217),
  ESCAPE("loz",     0x25CA),
  ESCAPE("lrm",     0x200E),
  ESCAPE("lsaquo",  0x2039),
  ESCAPE("lsquo",   0x2018),
  ESCAPE("lt",     '<'),
  ESCAPE("macr",    0xAF),
  ESCAPE("mdash",   0x2014),
  ESCAPE("micro",   0xB5),
  ESCAPE("middot",  0xB7),
  ESCAPE("minus",   0x2212),
  ESCAPE("mu",      0x3BC),
  ESCAPE("nabla",   0x2207),
  ESCAPE("nbsp",    0xA0),
  ESCAPE("ndash",   0x2013),
  ESCAPE("ne",      0x2260),
  ESCAPE("ni",      0x220B),
  ESCAPE("not",     0xAC),
  ESCAPE("notin",   0x2209),
  ESCAPE("nsub",    0x2284),
  ESCAPE("ntilde",  0xF1),
  ESCAPE("nu",      0x3BD),
  ESCAPE("oacute",  0xF3),
  ESCAPE("ocirc",   0xF4),
  ESCAPE("oelig",   0x153),
  ESCAPE("ograve",  0xF2),
  ESCAPE("oline",   0x203E),
  ESCAPE("omega",   0x3C9),
  ESCAPE("omicron", 0x3BF),
  ESCAPE("oplus",   0x2295),
  ESCAPE("or",      0x2228),
  ESCAPE("ordf",    0xAA),
  ESCAPE("ordm",    0xBA),
  ESCAPE("oslash",  0xF8),
  ESCAPE("otilde",  0xF5),
  ESCAPE("otimes",  0x2297),
  ESCAPE("ouml",    0xF6),
  ESCAPE("para",    0xB6),
  ESCAPE("part",    0x2202),
  ESCAPE("permil",  0x2030),
  ESCAPE("perp",    0x22A5),
  ESCAPE("phi",     0x3D5),
  ESCAPE("pi",      0x3C0),
  ESCAPE("piv",     0x3D6),
  ESCAPE("plusmn",  0xB1),
  ESCAPE("pound",   0xA3),
  ESCAPE("prime",   0x2032),
  ESCAPE("prod",    0x220F),
  ESCAPE("prop",    0x221D),
  ESCAPE("psi",     0x3C8),
  ESCAPE("quot",    0x22),
  ESCAPE("rArr",    0x21D2),
  ESCAPE("radic",   0x221A),
  ESCAPE("rang",    0x232A),
  ESCAPE("raquo",   0xBB),
  ESCAPE("rarr",    0x2192),
  ESCAPE("rceil",   0x2309),
  ESCAPE("rdquo",   0x201D),
  ESCAPE("real",    0x211C),
  ESCAPE("reg",     0xAE),
  ESCAPE("rfloor",  0x230B),
  ESCAPE("rho",     0x3C1),
  ESCAPE("rlm",     0x200F),
  ESCAPE("rsaquo",  0x203A),
  ESCAPE("rsquo",   0x2019),
  ESCAPE("sbquo",   0x201A),
  ESCAPE("scaron",  0x161),
  ESCAPE("sdot",    0x22C5),
  ESCAPE("sect",    0xA7),
  ESCAPE("shy",     0xAD),
  ESCAPE("sigma",   0x3C3),
  ESCAPE("sigmaf",  0x3C2),
  ESCAPE("sim",     0x223C),
  ESCAPE("spades",  0x2660),
  ESCAPE("sub",     0x2282),
  ESCAPE("sube",    0x2286),
  ESCAPE("sum",     0x2211),
  ESCAPE("sup",     0x2283),
  ESCAPE("sup1",    0xB9),
  ESCAPE("sup2",    0xB2),
  ESCAPE("sup3",    0xB3),
  ESCAPE("supe",    0x2287),
  ESCAPE("szlig",   0xDF),
  ESCAPE("tau",     0x3C4),
  ESCAPE("there4",  0x2234),
  ESCAPE("theta",   0x3B8),
  ESCAPE("thetasym",0x3D1),
  ESCAPE("thinsp",  0x2009),
  ESCAPE("thorn",   0xFE),
  ESCAPE("tilde",   0x2DC),
  ESCAPE("times",   0xD7),
  ESCAPE("trade",   0x2122),
  ESCAPE("uArr",    0x21D1),
  ESCAPE("uacute",  0xFA),
  ESCAPE("uarr",    0x2191),
  ESCAPE("ucirc",   0xFB),
  ESCAPE("ugrave",  0xF9),
  ESCAPE("uml",     0xA8),
  ESCAPE("upsih",   0x3D2),
  ESCAPE("upsilon", 0x3C5),
  ESCAPE("uuml",    0xFC),
  ESCAPE("weierp",  0x2118),
  ESCAPE("xi",      0x3BE),
  ESCAPE("yacute",  0xFD),
  ESCAPE("yen",     0xA5),
  ESCAPE("yuml",    0xFF),
  ESCAPE("zeta",    0x3B6),
  ESCAPE("zwj",     0x200D),
  ESCAPE("zwnj",    0x200C)
};

INLINE BYTE strmatch(CSTRING Name1, CSTRING Name2)
{
   while ((*Name1) and (*Name2)) {
      if (*Name1 < *Name2) return -1;
      else if (*Name1 > *Name2) return 1;
      Name1++;
      Name2++;
   }

   if (!*Name1) {
      if (!*Name2) return 0;
      else return -1;
   }
   else return 1;
}

static void xml_unescape(objXML *Self, STRING String)
{
   parasol::Log log(__FUNCTION__);
   LONG len, i;
   ULONG val;

   STRING src = String;
   STRING dest = String;
   while (*src) {
      if (*src != '&') {
         *dest++ = *src++;
      }
      else {
         src++;
         if (*src IS '#') {
            len = 1;
            val = 0;

            if (src[len] IS 'x') {
               len++;

               // Hexadecimal literal

               while ((src[len]) and (src[len] != ';')) {
                  val <<= 4;
                  if ((src[len] >= '0') and (src[len] <= '9')) val += src[len] - '0';
                  else if ((src[len] >= 'a') and (src[len] <= 'f')) val += src[len] - 'a' + 10;
                  else if ((src[len] >= 'A') and (src[len] <= 'F')) val += src[len] - 'A' + 10;
                  else break;
                  len++;
               }
            }
            else {
               // Decimal literal

               while ((src[len]) and (src[len] != ';')) {
                  val *= 10;
                  if ((src[len] >= '0') and (src[len] <= '9')) val += src[len] - '0';
                  else break;
                  len++;
               }
            }

            if (src[len] IS ';') { // The correct terminator must be present
               src += len + 1;
               dest += UTF8WriteValue(val, dest, len+1);
            }
            else *dest++ = '&';
         }
         else {
            STRING restore;
            BYTE valid = FALSE;
            for (i=0; src[i] and (i < 6); i++) {
               if (src[i] IS ';') {
                  src[i] = 0;
                  valid = TRUE;
                  restore = src + i;
                  break;
               }
            }

            if (!valid) {
               //LogErrorMsg("Invalid use of & in '%.40s'.", String);
               *dest++ = '&';
            }
            else {
               // These are the 5 official XML escape codes
               if (!StrMatch(src, "amp"))       { *dest++ = '&'; src += 4; }
               else if (!StrMatch(src, "lt"))   { *dest++ = '<'; src += 3; }
               else if (!StrMatch(src, "gt"))   { *dest++ = '>'; src += 3; }
               else if (!StrMatch(src, "apos")) { *dest++ = '\''; src += 5; }
               else if (!StrMatch(src, "quot")) { *dest++ = '\"'; src += 5; }
               else if (Self->Flags & XMF_PARSE_ENTITY) {

               }
               else if (Self->Flags & XMF_PARSE_HTML) { // Process HTML escape codes
                  log.trace("Checking escape code '%s'", src);

                  LONG floor   = 0;
                  LONG ceiling = ARRAYSIZE(glHTML);
                  while (TRUE) {
                     LONG mid = ((ceiling + floor)>>1);

                     log.trace("Index: %d (from %d - %d) Compare: %s - %s", mid, floor, ceiling, src, glHTML[mid].Escape);

                     BYTE result = strmatch(src, glHTML[mid].Escape);
                     if (!result) {
                        log.trace("Escape code %s recognised.", src);
                        if (glHTML[mid].Value < 128) *dest++ = glHTML[mid].Value;
                        else dest += UTF8WriteValue(glHTML[mid].Value, dest, glHTML[mid].Length);
                        src += glHTML[mid].Length + 1;
                        break;
                     }
                     else {
                        if (ceiling - floor <= 1) {
                           log.trace("Escape code %s unrecognised.", src);
                           *dest++ = '&';
                           break;
                        }

                        if (result < 0) ceiling = mid;
                        else if (result > 0) floor = mid;
                     }
                  }
               }
               else *dest++ = '&';

               *restore = ';';
            }
         }
      }
   }

   *dest = 0;
}
