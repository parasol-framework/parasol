/*********************************************************************************************************************

Test for XML count operations.

*********************************************************************************************************************/

#include <parasol/startup.h>
#include <parasol/modules/xml.h>

using namespace pf;

CSTRING ProgName = "XMLCountTest";

//********************************************************************************************************************

static int callback_count = 0;

static ERR xml_callback(objXML *XML, int TagID, CSTRING Attrib, APTR Meta)
{
   pf::Log log;
   callback_count++;

   // Get tag details
   XMLTag *tag = nullptr;
   if (XML->getTag(TagID, &tag) == ERR::Okay) {
      log.msg("Callback %d: Tag ID %d, Name: '%s'", callback_count, TagID, tag->Attribs[0].Name.c_str());
   } else {
      log.msg("Callback %d: Tag ID %d (failed to get tag)", callback_count, TagID);
   }

   return ERR::Okay;
}

//********************************************************************************************************************

int main(int argc, CSTRING *argv)
{
   pf::Log log;
   pf::vector<std::string> *args;

   if (auto msg = init_parasol(argc, argv)) {
      log.error("%s", msg);
      return -1;
   }

   log.msg("=== XML Count Test ===");

   // Create XML object with test data matching the failing test
   auto xml = objXML::create::global({
      fl::Statement("<root><section><item/><item/></section><section><item/><item/><item/></section></root>")
   });

   if (!xml) {
      log.error("Failed to create XML object");
      return -1;
   }

   log.msg("XML object created successfully");
   log.msg("Root tags count: %d", (int)xml->Tags.size());

   if (!xml->Tags.empty()) {
      log.msg("Root tag name: '%s'", xml->Tags[0].Attribs[0].Name.c_str());
      log.msg("Root children: %d", (int)xml->Tags[0].Children.size());

      // Print XML structure
      for (size_t i = 0; i < xml->Tags[0].Children.size(); i++) {
         auto& child = xml->Tags[0].Children[i];
         log.msg("  Child %d: '%s' has %d children", (int)i, child.Attribs[0].Name.c_str(), (int)child.Children.size());

         for (size_t j = 0; j < child.Children.size(); j++) {
            auto& grandchild = child.Children[j];
            log.msg("    Grandchild %d: '%s'", (int)j, grandchild.Attribs[0].Name.c_str());
         }
      }
   }

   log.msg("\n--- Testing Count Method with '//item' ---");

   // Test the Count method directly
   int count_result = 0;
   ERR err = xml->count("//item", &count_result);
   log.msg("Count method result: %s, count: %d", GetErrorMsg(err), count_result);

   log.msg("\n--- Testing Count Method with '/root/section/item' ---");

   // Test simpler path
   int simple_count = 0;
   ERR err2 = xml->count("/root/section/item", &simple_count);
   log.msg("Simple path count result: %s, count: %d", GetErrorMsg(err2), simple_count);

   log.msg("\n--- Testing FindTag with '//item' callback ---");

   // Test FindTag with callback to see what's happening
   callback_count = 0;
   FUNCTION callback = C_FUNCTION(xml_callback);

   int result_id;
   err = xml->findTag("//item", callback, &result_id);
   log.msg("FindTag result: %s, callback called %d times", GetErrorMsg(err), callback_count);

   log.msg("\n--- Testing FindTag with '/root/section/item' callback ---");

   // Test simpler FindTag
   callback_count = 0;
   err = xml->findTag("/root/section/item", callback, &result_id);
   log.msg("Simple FindTag result: %s, callback called %d times", GetErrorMsg(err), callback_count);

   log.msg("\n--- Testing FindTag with '/root/*/item' callback ---");

   // Test wildcard
   callback_count = 0;
   err = xml->findTag("/root/*/item", callback, &result_id);
   log.msg("Wildcard FindTag result: %s, callback called %d times", GetErrorMsg(err), callback_count);

   log.msg("\n=== Test Complete ===");

   return 0;
}