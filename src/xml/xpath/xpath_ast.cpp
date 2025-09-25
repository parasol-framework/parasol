//********************************************************************************************************************
// XPath AST Core Structures Implementation
//********************************************************************************************************************
//
// This translation unit intentionally remains lightweight: the bulk of the Abstract Syntax Tree
// (AST) implementation for XPath lives in the accompanying header so that the evaluator can keep
// node construction inlined and allocation-free.  The file documents the architectural intent for
// the AST layer and serves as the staging point for any future behavioural extensions that require
// out-of-line definitions (for example, tree normalisation passes or diagnostics helpers).
//
// The AST types describe a minimal, dependency-free hierarchy that mirrors the XPath grammar used
// by the parser.  Each node stores its role in an expression (location paths, steps, operators,
// literal values, and so on) alongside a small vector of child nodes.  The header exposes inline
// conveniences for traversal and construction, while this source file is available for
// implementations that may need stateful logic, additional validation, or other functionality that
// would otherwise bloat the header.  Keeping the structure centralised also ensures that the
// evaluator, parser, and optimiser layers share a consistent view of the tree.

// Currently no out-of-line methods are required; this file exists as a placeholder so that the
// build system has a stable compilation unit to link against when optional AST behaviour is added.