
static ankerl::unordered_dense::map<std::string, std::string> glOfficial = {
   { "amp",  "&" },
   { "lt",   "<" },
   { "gt",   ">" },
   { "apos", "\'" },
   { "quot", "\"" }
};

static ankerl::unordered_dense::map<std::string, uint16_t> glHTML = {
   { "AElig",   0xc6 },
   { "Aacute",  0xC1 },
   { "Acirc",   0xC2 },
   { "Agrave",  0xC0 },
   { "Alpha",   0x391 },
   { "Aring",   0xC5 },
   { "Atilde",  0xC3 },
   { "Auml",    0xC4 },
   { "Beta",    0x392 },
   { "Ccedil",  0xC7 },
   { "Chi",     0x3A7 },
   { "Dagger",  0x2021 },
   { "Delta",   0x394 },
   { "ETH",     0xD0 },
   { "Eacute",  0xC9 },
   { "Ecirc",   0xCA },
   { "Egrave",  0xC8 },
   { "Epsilon", 0x395 },
   { "Eta",     0x397 },
   { "Euml",    0xCB },
   { "Gamma",   0x393 },
   { "Iacute",  0xCD },
   { "Icirc",   0xCE },
   { "Igrave",  0xCC },
   { "Iota",    0x399 },
   { "Iuml",    0xCF },
   { "Kappa",   0x39A },
   { "Lambda",  0x39B },
   { "Mu",      0x39C },
   { "Ntilde",  0xD1 },
   { "Nu",      0x39D },
   { "OElig",   0x152 },
   { "Oacute",  0xD3 },
   { "Ocirc",   0xD4 },
   { "Ograve",  0xD2 },
   { "Omega",   0x3A9 },
   { "Omicron", 0x39F },
   { "Oslash",  0xD8 },
   { "Otilde",  0xD5 },
   { "Ouml",    0xD6 },
   { "Phi",     0x3A6 },
   { "Pi",      0x3A0 },
   { "Prime",   0x2033 },
   { "Psi",     0x3A8 },
   { "Rho",     0x3A1 },
   { "Scaron",  0x160 },
   { "Sigma",   0x3A3 },
   { "THORN",   0xDE },
   { "Tau",     0x3A4 },
   { "Theta",   0x398 },
   { "Uacute",  0xDA },
   { "Ucirc",   0xDB },
   { "Ugrave",  0xD9 },
   { "Upsilon", 0x3A5 },
   { "Uuml",    0xDC },
   { "Xi",      0x39E },
   { "Yacute",  0xDD },
   { "Yuml",    0x178 },
   { "Zeta",    0x396 },
   { "aacute",  0xE1 },
   { "acirc",   0xE2 },
   { "acute",   0xB4 },
   { "aelig",   0xE6 },
   { "agrave",  0xE0 },
   { "alefsym", 0x2135 },
   { "alpha",   0x3B1 },
   { "and",     0x2227 },
   { "ang",     0x2220 },
   { "aring",   0xE5 },
   { "asymp",   0x2248 },
   { "atilde",  0xE3 },
   { "auml",    0xE4 },
   { "bdquo",   0x201E },
   { "beta",    0x3B2 },
   { "brvbar",  0xA6 },
   { "bull",    0x2022 },
   { "cap",     0x2229 },
   { "ccedil",  0xE7 },
   { "cedil",   0xB8 },
   { "cent",    0xA2 },
   { "chi",     0x3C7 },
   { "circ",    0x2C6 },
   { "clubs",   0x2663 },
   { "cong",    0x2245 },
   { "copy",    0xA9 },
   { "crarr",   0x21B5 },
   { "cup",     0x222A },
   { "curren",  0xA4 },
   { "dArr",    0x21D3 },
   { "dagger",  0x2020 },
   { "darr",    0x2193 },
   { "deg",     0xB0 },
   { "delta",   0x3B4 },
   { "diams",   0x2666 },
   { "divide",  0xF7 },
   { "eacute",  0xE9 },
   { "ecirc",   0xEA },
   { "egrave",  0xE8 },
   { "empty",   0x2205 },
   { "emsp",    0x2003 },
   { "ensp",    0x2002 },
   { "epsilon", 0x3B5 },
   { "equiv",   0x2261 },
   { "eta",     0x3B7 },
   { "eth",     0xF0 },
   { "euml",    0xEB },
   { "euro",    0x20AC },
   { "exist",   0x2203 },
   { "fnof",    0x192 },
   { "forall",  0x2200 },
   { "frac12",  0xBD },
   { "frac14",  0xBC },
   { "frac34",  0xBE },
   { "frasl",   0x2044 },
   { "gamma",   0x3B3 },
   { "ge",      0x2265 },
   { "gt",      0x3E },
   { "hArr",    0x21D4 },
   { "harr",    0x2194 },
   { "hearts",  0x2665 },
   { "hellip",  0x2026 },
   { "iacute",  0xED },
   { "icirc",   0xEE },
   { "iexcl",   0xA1 },
   { "igrave",  0xEC },
   { "image",   0x2111 },
   { "infin",   0x221E },
   { "int",     0x222B },
   { "iota",    0x3B9 },
   { "iquest",  0xBF },
   { "isin",    0x2208 },
   { "iuml",    0xEF },
   { "kappa",   0x3BA },
   { "lArr",    0x21D0 },
   { "lambda",  0x3BB },
   { "lang",    0x2329 },
   { "laquo",   0xAB },
   { "larr",    0x2190 },
   { "lceil",   0x2308 },
   { "ldquo",   0x201C },
   { "le",      0x2264 },
   { "lfloor",  0x230A },
   { "lowast",  0x2217 },
   { "loz",     0x25CA },
   { "lrm",     0x200E },
   { "lsaquo",  0x2039 },
   { "lsquo",   0x2018 },
   { "lt",     '<' },
   { "macr",    0xAF },
   { "mdash",   0x2014 },
   { "micro",   0xB5 },
   { "middot",  0xB7 },
   { "minus",   0x2212 },
   { "mu",      0x3BC },
   { "nabla",   0x2207 },
   { "nbsp",    0xA0 },
   { "ndash",   0x2013 },
   { "ne",      0x2260 },
   { "ni",      0x220B },
   { "not",     0xAC },
   { "notin",   0x2209 },
   { "nsub",    0x2284 },
   { "ntilde",  0xF1 },
   { "nu",      0x3BD },
   { "oacute",  0xF3 },
   { "ocirc",   0xF4 },
   { "oelig",   0x153 },
   { "ograve",  0xF2 },
   { "oline",   0x203E },
   { "omega",   0x3C9 },
   { "omicron", 0x3BF },
   { "oplus",   0x2295 },
   { "or",      0x2228 },
   { "ordf",    0xAA },
   { "ordm",    0xBA },
   { "oslash",  0xF8 },
   { "otilde",  0xF5 },
   { "otimes",  0x2297 },
   { "ouml",    0xF6 },
   { "para",    0xB6 },
   { "part",    0x2202 },
   { "permil",  0x2030 },
   { "perp",    0x22A5 },
   { "phi",     0x3D5 },
   { "pi",      0x3C0 },
   { "piv",     0x3D6 },
   { "plusmn",  0xB1 },
   { "pound",   0xA3 },
   { "prime",   0x2032 },
   { "prod",    0x220F },
   { "prop",    0x221D },
   { "psi",     0x3C8 },
   { "quot",    0x22 },
   { "rArr",    0x21D2 },
   { "radic",   0x221A },
   { "rang",    0x232A },
   { "raquo",   0xBB },
   { "rarr",    0x2192 },
   { "rceil",   0x2309 },
   { "rdquo",   0x201D },
   { "real",    0x211C },
   { "reg",     0xAE },
   { "rfloor",  0x230B },
   { "rho",     0x3C1 },
   { "rlm",     0x200F },
   { "rsaquo",  0x203A },
   { "rsquo",   0x2019 },
   { "sbquo",   0x201A },
   { "scaron",  0x161 },
   { "sdot",    0x22C5 },
   { "sect",    0xA7 },
   { "shy",     0xAD },
   { "sigma",   0x3C3 },
   { "sigmaf",  0x3C2 },
   { "sim",     0x223C },
   { "spades",  0x2660 },
   { "sub",     0x2282 },
   { "sube",    0x2286 },
   { "sum",     0x2211 },
   { "sup",     0x2283 },
   { "sup1",    0xB9 },
   { "sup2",    0xB2 },
   { "sup3",    0xB3 },
   { "supe",    0x2287 },
   { "szlig",   0xDF },
   { "tau",     0x3C4 },
   { "there4",  0x2234 },
   { "theta",   0x3B8 },
   { "thetasym",0x3D1 },
   { "thinsp",  0x2009 },
   { "thorn",   0xFE },
   { "tilde",   0x2DC },
   { "times",   0xD7 },
   { "trade",   0x2122 },
   { "uArr",    0x21D1 },
   { "uacute",  0xFA },
   { "uarr",    0x2191 },
   { "ucirc",   0xFB },
   { "ugrave",  0xF9 },
   { "uml",     0xA8 },
   { "upsih",   0x3D2 },
   { "upsilon", 0x3C5 },
   { "uuml",    0xFC },
   { "weierp",  0x2118 },
   { "xi",      0x3BE },
   { "yacute",  0xFD },
   { "yen",     0xA5 },
   { "yuml",    0xFF },
   { "zeta",    0x3B6 },
   { "zwj",     0x200D },
   { "zwnj",    0x200C }
};

static void xml_unescape(extXML *Self, std::string &String)
{
   pf::Log log(__FUNCTION__);

   // Single-pass algorithm: scan once and build output directly
   std::string result;
   result.reserve(String.size()); // Reserve to avoid reallocations

   size_t pos = 0;
   const size_t length = String.size();

   while (pos < length) {
      const size_t amp_pos = String.find('&', pos);

      // No more entities - append rest and exit
      if (amp_pos IS std::string::npos) {
         result.append(String, pos, length - pos);
         break;
      }

      // Append everything before '&'
      if (amp_pos > pos) {
         result.append(String, pos, amp_pos - pos);
      }

      // Check for numeric escape
      if ((amp_pos + 1 < length) and (String[amp_pos + 1] IS '#')) {
         size_t len = amp_pos + 2;
         uint32_t unicode = 0;

         if ((len < length) and (String[len] IS 'x')) { // Hexadecimal
            len++;
            while (len < length) {
               char ch = String[len];
               if (ch IS ';') break;

               if ((ch >= '0') and (ch <= '9')) unicode = (unicode << 4) + (ch - '0');
               else if ((ch >= 'a') and (ch <= 'f')) unicode = (unicode << 4) + (ch - 'a' + 10);
               else if ((ch >= 'A') and (ch <= 'F')) unicode = (unicode << 4) + (ch - 'A' + 10);
               else break;
               len++;
            }
         }
         else { // Decimal
            while (len < length) {
               char ch = String[len];
               if (ch IS ';') break;
               if ((ch >= '0') and (ch <= '9')) unicode = unicode * 10 + (ch - '0');
               else break;
               len++;
            }
         }

         if ((len < length) and (String[len] IS ';')) {
            char unichar[6];
            auto ulen = UTF8WriteValue(unicode, unichar, sizeof(unichar));
            result.append(unichar, ulen);
            pos = len + 1;
         }
         else {
            result.push_back('&'); // Invalid escape - keep literal '&'
            pos = amp_pos + 1;
         }
      }
      else {
         // Named entity reference
         const size_t semi_pos = String.find(';', amp_pos + 1);

         if (semi_pos IS std::string::npos) {
            result.push_back('&');
            pos = amp_pos + 1;
            continue;
         }

         const size_t name_len = semi_pos - amp_pos - 1;
         if (name_len IS 0) {
            result.push_back('&');
            pos = amp_pos + 1;
            continue;
         }

         // Use string_view to avoid allocation for lookup
         std::string_view lookup(&String[amp_pos + 1], name_len);

         // Check official XML entities first (most common)
         if (lookup IS "amp") { result.push_back('&'); pos = semi_pos + 1; }
         else if (lookup IS "lt") { result.push_back('<'); pos = semi_pos + 1; }
         else if (lookup IS "gt") { result.push_back('>'); pos = semi_pos + 1; }
         else if (lookup IS "quot") { result.push_back('"'); pos = semi_pos + 1; }
         else if (lookup IS "apos") { result.push_back('\''); pos = semi_pos + 1; }
         else if ((Self->Flags & XMF::PARSE_ENTITY) != XMF::NIL) {
            std::string resolved;
            std::string lookup_str(lookup); // Only create string if needed
            if (Self->resolveEntity(lookup_str, resolved) IS ERR::Okay) {
               result.append(resolved);
               pos = semi_pos + 1;
            }
            else {
               result.push_back('&');
               pos = amp_pos + 1;
            }
         }
         else if ((Self->Flags & XMF::PARSE_HTML) != XMF::NIL) {
            std::string lookup_str(lookup); // Only create string if needed
            if (glHTML.contains(lookup_str)) {
               auto unicode = glHTML[lookup_str];
               char unichar[6];
               auto unilen = UTF8WriteValue(unicode, unichar, sizeof(unichar));
               result.append(unichar, unilen);
               pos = semi_pos + 1;
            }
            else {
               result.push_back('&');
               pos = amp_pos + 1;
            }
         }
         else {
            result.push_back('&');
            pos = amp_pos + 1;
         }
      }
   }

   String = std::move(result);
}

//********************************************************************************************************************

static void unescape_all(extXML *Self, TAGS &Tags)
{
   for (auto &tag : Tags) {
      if (!tag.Children.empty()) {
         unescape_all(Self, tag.Children);
      }

      if ((tag.Flags & XTF::CDATA) != XTF::NIL) continue;
      for (auto &attrib : tag.Attribs) {
         if (attrib.Value.empty()) continue;
         xml_unescape(Self, attrib.Value);
      }
   }
};
