//********************************************************************************************************************
// XPath Abstract Syntax Tree (AST) Core Structures
//
// This file contains the fundamental AST data structures for XPath evaluation:
// 
// - Token types and structures
// - AST node types and tree structure
// - Core enumerations

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <parasol/modules/xpath.h>

using XPathAttributeValuePart = XPathNode::XPathAttributeValuePart;
using XPathConstructorAttribute = XPathNode::XPathConstructorAttribute;
using XPathConstructorInfo = XPathNode::XPathConstructorInfo;

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
   UNION,             // union keyword
   INTERSECT,         // intersect keyword
   EXCEPT,            // except keyword

   // Operators
   EQUALS,            // =
   NOT_EQUALS,        // !=
   LESS_THAN,         // <
   LESS_EQUAL,        // <=
   GREATER_THAN,      // >
   GREATER_EQUAL,     // >=
   EQ,                // eq
   NE,                // ne
   LT,                // lt
   LE,                // le
   GT,                // gt
   GE,                // ge

   // Boolean operators
   AND,               // and
   OR,                // or
   NOT,               // not

   // Flow keywords
   IF,                // if
   THEN,              // then
   ELSE,              // else
   FOR,               // for
   LET,               // let
   IN,                // in
   RETURN,            // return
   SOME,              // some
   EVERY,             // every
   SATISFIES,         // satisfies

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
   ASSIGN,            // :=

   // Constructor delimiters
   LBRACE,            // {
   RBRACE,            // }
   TAG_OPEN,          // < (direct constructors)
   CLOSE_TAG_OPEN,    // </
   TAG_CLOSE,         // >
   EMPTY_TAG_CLOSE,   // />
   PI_START,          // <?
   PI_END,            // ?>

   // Special tokens
   END_OF_INPUT,
   UNKNOWN
};

struct XPathToken {
   XPathTokenType type;
   std::string_view value;
   size_t position;
   size_t length;

   // For tokens that need string storage (e.g., processed strings with escapes)
   std::string stored_value;
   bool is_attribute_value = false;
   std::vector<XPathAttributeValuePart> attribute_value_parts;

   // Constructor for string_view tokens (no copying)
   XPathToken(XPathTokenType t, std::string_view v, size_t pos = 0, size_t len = 0)
      : type(t), value(v), position(pos), length(len) {}

   // Constructor for tokens requiring string storage
   XPathToken(XPathTokenType t, std::string v, size_t pos = 0, size_t len = 0)
      : type(t), position(pos), length(len), stored_value(std::move(v)) {
      value = stored_value;
   }

   [[nodiscard]] bool has_attribute_template() const
   {
      return is_attribute_value and !attribute_value_parts.empty();
   }
};
