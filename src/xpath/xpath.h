// A basic class for holding the compiled AST, a record of errors, and validity status.

#pragma once

#include <utility>
#include <parasol/modules/xpath.h>
#include "xpath_ast.h"

class CompiledXPath : public XPathExpression {
   public:
   std::shared_ptr<XPathNode> ast;
   CompiledXPath() {}

   // Compile an XPath expression
   static CompiledXPath * compile(std::string_view XPath, std::vector<std::string> &);

   // Get the compiled AST (for internal use by evaluator)
   [[nodiscard]] const XPathNode * getAST() const { return ast.get(); }
   [[nodiscard]] std::shared_ptr<XPathNode> getASTShared() const { return ast; }

   // Disable copy constructor and assignment to avoid shared AST issues
   CompiledXPath(const CompiledXPath &) = delete;
   CompiledXPath & operator=(const CompiledXPath &) = delete;
};

// Lightweight view-based trim used for cache key normalisation.

inline std::string_view trim_view(std::string_view Value)
{
   auto start = Value.find_first_not_of(" \t\r\n");
   if (start IS std::string_view::npos) return std::string_view();

   auto end = Value.find_last_not_of(" \t\r\n");
   return Value.substr(start, end - start + 1);
}
