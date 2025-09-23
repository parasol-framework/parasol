//********************************************************************************************************************
// XPath AST Core Structures
//
// This file contains the fundamental AST data structures for XPath evaluation:
// - Token types and structures
// - AST node types and tree structure
// - Core enumerations
//********************************************************************************************************************

#pragma once

#include <string>
#include <vector>
#include <memory>

//********************************************************************************************************************
// XPath Tokenization Infrastructure

enum class XPathTokenType {
   // Path operators
   SLASH,              // /
   DOUBLE_SLASH,       // //
   DOT,               // .
   DOUBLE_DOT,        // ..

   // Identifiers and literals
   IDENTIFIER,         // element names, function names
   STRING,            // quoted strings
   NUMBER,            // numeric literals
   WILDCARD,          // *

   // Brackets and delimiters
   LBRACKET,          // [
   RBRACKET,          // ]
   LPAREN,            // (
   RPAREN,            // )
   AT,                // @
   COMMA,             // ,
   PIPE,              // |

   // Operators
   EQUALS,            // =
   NOT_EQUALS,        // !=
   LESS_THAN,         // <
   LESS_EQUAL,        // <=
   GREATER_THAN,      // >
   GREATER_EQUAL,     // >=

   // Boolean operators
   AND,               // and
   OR,                // or
   NOT,               // not

   // Arithmetic operators
   PLUS,              // +
   MINUS,             // -
   MULTIPLY,          // * (when not wildcard)
   DIVIDE,            // div
   MODULO,            // mod

   // Axis specifiers
   AXIS_SEPARATOR,    // ::
   COLON,             // :

   // Variables and functions
   DOLLAR,            // $

   // Special tokens
   END_OF_INPUT,
   UNKNOWN
};

struct XPathToken {
   XPathTokenType type;
   std::string value;
   size_t position;
   size_t length;

   XPathToken(XPathTokenType t, std::string v, size_t pos = 0, size_t len = 0)
      : type(t), value(std::move(v)), position(pos), length(len) {}
};

//********************************************************************************************************************
// XPath AST Node Structure

enum class XPathNodeType {
   // Location path components
   LocationPath,
   Step,
   NodeTest,
   Predicate,
   Root,

   // Expressions
   Expression,
   BinaryOp,
   UnaryOp,
   FunctionCall,
   Literal,
   VariableReference,

   // Node tests
   NameTest,
   NodeTypeTest,
   ProcessingInstructionTest,
   Wildcard,

   // Axes
   AxisSpecifier,

   // Union
   Union,

   // Primary expressions
   Number,
   String,
   Path
};

struct XPathNode {
   XPathNodeType type;
   std::string value;
   std::vector<std::unique_ptr<XPathNode>> children;

   // Constructor
   XPathNode(XPathNodeType t, std::string v = "")
      : type(t), value(std::move(v)) {}

   // Helper methods
   void add_child(std::unique_ptr<XPathNode> child) {
      children.push_back(std::move(child));
   }

   XPathNode* get_child(size_t index) const {
      return index < children.size() ? children[index].get() : nullptr;
   }

   size_t child_count() const {
      return children.size();
   }
};