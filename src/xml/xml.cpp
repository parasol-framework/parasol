/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-MODULE-
XML: Provides an interface for the management of structured data.

-END-

*********************************************************************************************************************/

#include <parasol/modules/xml.h>
#include <parasol/modules/xpath.h>
#include <parasol/strings.hpp>
#include <array>
#include <format>
#include <functional>
#include <sstream>
#include <vector>

#include "../link/unicode.h"
#include "xml.h"
#include "schema/schema_parser.h"
#include "schema/type_checker.h"
#include "uri_utils.h"
#include "unescape.h"
#include "xpath_value.h"

JUMPTABLE_CORE

static OBJECTPTR clXML = nullptr;
static OBJECTPTR modContext = nullptr;
static uint32_t glTagID = 1;

#ifndef PARASOL_STATIC
JUMPTABLE_XPATH
static OBJECTPTR modXPath = nullptr;
#endif

#include "xml_def.c"

//*********************************************************************************************************************
// Dynamic loader for the XPath functionality.  We only load it as needed due to the size of the module.

static ERR load_xpath(void)
{
#ifndef PARASOL_STATIC
   if (not modXPath) {
      auto context = SetContext(modContext);
      if (objModule::load("xpath", &modXPath, &XPathBase) != ERR::Okay) return ERR::InitModule;
      SetContext(context);
   }
#endif
   return ERR::Okay;
}

//*********************************************************************************************************************

#include "xml_functions.cpp"

namespace
{
   [[nodiscard]] static bool attribute_is_xml_base(const XMLAttrib &Attribute)
   {
      if (Attribute.Name.empty()) return false;
      return pf::iequals(Attribute.Name, "xml:base");
   }

   [[nodiscard]] static std::string document_base(extXML *Document)
   {
      if ((!Document) or (!Document->Path) or (!*Document->Path)) return std::string();
      return xml::uri::normalise_uri_separators(std::string(Document->Path));
   }

   [[nodiscard]] static std::string resolve_inherited_base(extXML *Document, XMLTag *Parent)
   {
      if (!Document) return std::string();
      if (!Parent) return document_base(Document);

      if (auto cached = Document->findBaseURI(Parent->ID)) return *cached;

      std::vector<std::string> chain;
      for (XMLTag *current = Parent; current; ) {
         for (size_t index = 1; index < current->Attribs.size(); ++index) {
            const XMLAttrib &attrib = current->Attribs[index];
            if (attribute_is_xml_base(attrib)) chain.push_back(attrib.Value);
         }

         if (!current->ParentID) break;
         current = Document->getTag(current->ParentID);
      }

      std::string base = document_base(Document);
      for (auto iterator = chain.rbegin(); iterator != chain.rend(); ++iterator) {
         if (base.empty()) base = *iterator;
         else base = xml::uri::resolve_relative_uri(*iterator, base);
         base = xml::uri::normalise_uri_separators(std::move(base));
      }

      return base;
   }

   static void refresh_base_uris(extXML *Document, XMLTag &Node, const std::string &InheritedBase)
   {
      std::string node_base = InheritedBase;

      for (size_t index = 1; index < Node.Attribs.size(); ++index) {
         const XMLAttrib &attrib = Node.Attribs[index];
         if (!attribute_is_xml_base(attrib)) continue;

         std::string resolved;
         if (attrib.Value.empty()) resolved = InheritedBase;
         else if (InheritedBase.empty()) resolved = attrib.Value;
         else resolved = xml::uri::resolve_relative_uri(attrib.Value, InheritedBase);

         node_base = xml::uri::normalise_uri_separators(std::move(resolved));
      }

      Document->BaseURIMap[Node.ID] = node_base;

      for (auto &child : Node.Children) refresh_base_uris(Document, child, node_base);
   }

   static void refresh_base_uris_for_insert(extXML *Document, TAGS &Inserted, XMLTag *Parent)
   {
      if ((!Document) or Inserted.empty()) return;

      std::string inherited = resolve_inherited_base(Document, Parent);
      for (auto &node : Inserted) refresh_base_uris(Document, node, inherited);
   }
}

static ERR add_xml_class(void);
static ERR SET_Statement(extXML *, CSTRING);
static ERR SET_Source(extXML *, OBJECTPTR);

//********************************************************************************************************************

static ERR MODInit(OBJECTPTR pModule, struct CoreBase *pCore)
{
   CoreBase = pCore;
   modContext = CurrentContext();
   return add_xml_class();
}

static ERR MODExpunge(void)
{
   if (clXML) { FreeResource(clXML); clXML = nullptr; }
#ifndef PARASOL_STATIC
   if (modXPath) { FreeResource(modXPath); modXPath = nullptr; }
#endif
   return ERR::Okay;
}

namespace xml {

/*********************************************************************************************************************

-FUNCTION-
XValueToNumber: Converts an XPathValue to a 64-bit floating point value.

Call XValueToNumber() to convert an XPathValue object to a 64-bit floating point number.  This function
also includes cover support for boolean types, converting true to 1.0 and false to 0.

-INPUT-
ptr(struct(XPathValue)) Value: The XPathValue to convert.
&double Result: The numeric representation of the value is returned here.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

ERR XValueToNumber(XPathValue *Value, double *Result)
{
   pf::Log log(__FUNCTION__);

   if ((not Value) or (not Result)) return log.warning(ERR::NullArgs);
   if (Value->type IS XPVT::NIL) return log.warning(ERR::NoData);

   auto val = (XPathVal *)Value;
   *Result = val->to_number();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FUNCTION-
XValueNodes: For node-set XPathValue objects, returns the node-set as an array.

If an XPathValue represents a node-set (type `XPVT::NODE_SET`) then XValueToNodes() will return a direct pointer to 
the node-set array.

Note: The integrity of the array is not guaranteed if the original XML document is modified or freed.

-INPUT-
ptr(struct(XPathValue)) Value: The XPathValue to convert.
&cpp(array(ptr(struct(XMLTag)))) Result: The node-set is returned here as an array of !XMLTag structures.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

ERR XValueNodes(XPathValue *Value, pf::vector<XMLTag *> *Result)
{
   pf::Log log(__FUNCTION__);
   if ((not Value) or (not Result)) return log.warning(ERR::NullArgs);
   if (Value->type IS XPVT::NIL) return log.warning(ERR::NoData);
   if (Value->type != XPVT::NodeSet) return log.warning(ERR::Mismatch);
   auto val = (XPathVal *)Value;
   *Result = val->to_node_set();
   return ERR::Okay;
}

/*********************************************************************************************************************

-FUNCTION-
XValueToString: Converts an XPathValue to its string representation.

Call XValueToString() to convert an XPathValue object into its string representation.

-INPUT-
ptr(cstruct(XPathValue)) Value: The XPathValue to convert.
&cpp(str) Result: Receives the string representation of the value.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

ERR XValueToString(const XPathValue *Value, std::string *Result)
{
   pf::Log log(__FUNCTION__);

   if ((not Value) or (not Result)) return log.warning(ERR::NullArgs);
   if (Value->type IS XPVT::NIL) return log.warning(ERR::NoData);

   auto val = (XPathVal *)Value;
   *Result = val->to_string();
   return ERR::Okay;
}

//********************************************************************************************************************

} // namespace xml

//********************************************************************************************************************

#include "xml_class.cpp"

static STRUCTS glStructures = {
   { "XMLTag", sizeof(XMLTag) },
   { "XPathValue", sizeof(XPathValue) }
};

//********************************************************************************************************************

PARASOL_MOD(MODInit, nullptr, nullptr, MODExpunge, MOD_IDL, &glStructures)
extern "C" struct ModHeader * register_xml_module() { return &ModHeader; }
