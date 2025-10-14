# Plan: Implement Document Construction Expressions in XPath Module

## 1. Analyse existing infrastructure
1. Review the generated XPath AST definitions in `src/xpath/xpath.fdl` and the corresponding header `include/parasol/modules/xpath.h` to understand how new node kinds must be introduced and regenerated. Confirm downstream consumers of `XPathNodeType` inside `src/xpath/xpath_parser.cpp` and `src/xpath/xpath_evaluator_*.cpp` rely on the enum ordering before modifying it.
2. Inspect the current primary expression parser (`XPathParser::parse_primary_expr` in `src/xpath/xpath_parser.cpp`) and tokeniser (`XPathTokenizer` in `src/xpath/xpath_tokenizer.cpp`) to map all points that will need to detect `<`, `</`, `{`, `}`, and the constructor keywords (`element`, `attribute`, `text`, `comment`, `processing-instruction`, `document`).
3. Examine the evaluator entry points (`XPathEvaluator::evaluate_expression`, `evaluate_union_value`, `evaluate_path_expression_value`) to plan where new constructor nodes should be executed. Catalogue how node sets and scalar values are currently materialised via `XPathVal` (`src/xml/xpath_value.h`) and how lifetime of temporary nodes is managed today (notably `XPathArena`).
4. Investigate namespace utilities already available through `extXML::resolvePrefix` in `src/xml/xml.h` and any existing namespace-aware matching in `xpath_evaluator_navigation.cpp` to determine how constructor-time namespace lookups should be implemented.

## 2. Extend the AST to represent constructors
1. Update `src/xpath/xpath.fdl` to add explicit `XPathNodeType` entries for each constructor concept, for example: `DIRECT_ELEMENT`, `DIRECT_ATTRIBUTE`, `DIRECT_TEXT`, `COMPUTED_ELEMENT`, `COMPUTED_ATTRIBUTE`, `TEXT_CONSTRUCTOR`, `COMMENT_CONSTRUCTOR`, `PI_CONSTRUCTOR`, `DOCUMENT_CONSTRUCTOR`, plus helper nodes such as `ATTRIBUTE_VALUE_TEMPLATE`, `CONTENT_SEQUENCE`, or similar as required for structured representation.
2. Define struct layouts (within the custom block in `xpath.fdl`) to store constructor metadata on AST nodes—e.g. flags for empty-element syntax, namespace prefix strings, literal tag names, and whether whitespace/text originated outside of `{}` expressions.
3. Regenerate headers (`cmake --build build/agents --config FastBuild --target xpath --parallel`) so `include/parasol/modules/xpath.h` picks up the new enum members before continuing with parser work.

## 3. Enhance tokenisation for constructor syntax
1. Introduce token kinds for braces and markup-specific delimiters: `{`, `}`, `<`, `</`, `/>`, `>`, and `:=` already exists but ensure conflicts with attribute default syntax are handled. Consider separate token types like `TAG_OPEN`, `TAG_CLOSE`, `EMPTY_ELEMENT_CLOSE`, `LBRACE`, `RBRACE`, `CLOSE_TAG_START` to simplify parsing.
2. Extend `XPathTokenizer::tokenize` to recognise sequences like `</` and `?>` before falling back to single-character operators. Maintain existing `<` comparison semantics by only emitting the new tokens when the lexer is in "constructor" mode. To accomplish this, track context (e.g. maintain a stack that toggles when `<` begins a direct constructor vs when inside computed expressions).
3. Implement scanning for attribute value templates: inside attribute strings, detect `{` and `}` pairs that denote embedded expressions (`<tag attr="{$expr}">`). Decide whether to split attribute literals into alternating TEXT/EXPR tokens during tokenisation or to leave raw text for the parser to process.
4. Ensure string unescaping aligns with XML rules (e.g., handle `&lt;` etc.), potentially reusing helpers from `src/xml/unescape.cpp`. Add unit coverage for the new lexing behaviour.

## 4. Parse direct element and attribute constructors
1. Extend `parse_primary_expr` to branch into a new `parse_direct_constructor` when encountering `<`. Implement a recursive-descent routine that handles start tags, attributes, optional namespace declarations, content, and matching end tags, while supporting `{}` expression escapes for both attribute values and element content.
2. Represent direct content as a sequence of AST children (e.g. literal text nodes and embedded expression nodes). Decide where to normalise whitespace or entity references—parsing phase vs evaluation.
3. Support empty-element syntax (`<tag/>`) and maintain proper error reporting (unmatched tags, unexpected EOF). Update `report_error` usage so the AST is only emitted when the constructor is well-formed.
4. Capture namespace declarations (`xmlns` and `xmlns:prefix`) as dedicated attribute nodes so the evaluator can add them to constructed nodes and update namespace maps.

## 5. Parse computed constructors
1. Extend `parse_primary_expr` (or a dedicated helper) to recognise the keywords `element`, `attribute`, `text`, `comment`, `processing-instruction`, and `document` followed by the constructor grammar specified by XQuery/XPath.
2. For each keyword, parse the optional name expression (for `element`/`attribute`/`processing-instruction`) followed by the `{ Expr }` content block. Reuse existing expression parsing to handle nested sequences inside braces.
3. Introduce AST node shapes mirroring the computed constructor structure (e.g. a parent node with child expression nodes for the name and content). Record lexical information such as explicit QName strings, NCName constraints for `processing-instruction`, etc., to allow semantic validation during evaluation.
4. Ensure computed constructors coexist with function calls and other IDENTIFIER-starting expressions—peek ahead for `{` after the keyword to disambiguate from ordinary element nodes or variables.

## 6. Manage runtime data structures for constructed nodes
1. Extend `XPathVal` (and/or create a helper structure) to own dynamically created `XMLTag` trees so their lifetime exceeds the evaluation scope. Consider adding a `std::shared_ptr<std::vector<std::unique_ptr<XMLTag>>>` within `XPathVal` or storing ownership inside `XPathEvaluator` with pointers handed to `XPathVal::node_set`.
2. Update `XPathEvaluator` to maintain a pool/vector of constructed node owners per evaluation to avoid leaks and double frees. Reset this storage between top-level evaluations (e.g. at start of `evaluate_xpath_expression`).
3. Provide helper functions (e.g. `build_constructed_element`, `append_constructed_content`) that transform constructor AST nodes into actual `XMLTag` instances. Ensure attributes/text nodes follow the same layout as parsed documents (first attribute entry holds element name, text nodes stored as child `XMLTag` with empty name attribute).
4. When constructors yield sequences of nodes and atomic values mixed, ensure the resulting `XPathVal` observes XQuery sequence normalisation rules (e.g. flatten sequences, convert atomic values to text nodes for direct constructors). Document any unsupported cases that must flag `expression_unsupported`.

## 7. Implement namespace handling during construction
1. During evaluation of direct constructors, parse namespace declarations captured in the AST and apply them to the constructed `XMLTag` (populate `Attribs` with `xmlns` attributes, update `NamespaceID`, and cascade prefixes to child constructors where appropriate).
2. For computed constructors, resolve namespace URIs via the context document when QName constructors use prefixes. Use `extXML::resolvePrefix` for prefix lookups and store the resulting namespace hash in the constructed node. Handle the case where a computed constructor provides a `QName` or `xs:QName` typed value by extracting prefix/local name pairs.
3. Ensure the evaluator respects in-scope namespaces when embedding computed nodes inside direct constructors (e.g., attribute value templates referencing names with prefixes defined by the constructor).
4. Add validation to reject conflicting namespace declarations or illegal usage (e.g., redeclaring `xml` prefix), returning appropriate errors through `record_error`.

## 8. Support non-element constructors
1. Implement evaluation paths for `text{}`, `comment{}`, and `processing-instruction{}` to create appropriate `XMLTag`/`XMLAttrib` structures: `text{}` should yield node-set entries representing text nodes; comments and processing instructions should set the relevant `XTF` flags and store target/text correctly.
2. For `processing-instruction`, enforce NCName rules on the target at evaluation time and reject content containing `?>`.
3. Implement the `document{}` constructor to wrap a sequence of nodes into a synthetic document root. Decide whether to produce a detached `XMLTag` tree with ID management or to reuse existing `objXML::Tags` clones. Update XPathVal to expose the resulting document node as a singleton node set.

## 9. Integrate with evaluation dispatch
1. Extend `XPathEvaluator::evaluate_expression` to handle the new constructor node types, invoking helper routines that build the constructed node trees and return `XPathVal` instances.
2. Ensure constructed nodes participate correctly in downstream operations (e.g. when passed to functions or combined in sequences). Update any helper methods (`apply_predicates_to_candidates`, axis traversals) if they assume nodes belong to the source document and adjust to accommodate detached nodes when necessary.
3. Update `XPathFunctionLibrary` if any core functions need awareness of constructed nodes (e.g., functions that inspect document order or base URIs) and define behaviour for nodes without backing `extXML` documents.

## 10. Testing strategy
1. Add new Fluid tests under `src/xml/tests/` (e.g. `test_xpath_constructors.fluid`) covering:
   - Direct element constructors with nested attributes, text nodes, and attribute value templates.
   - Computed constructors for every required keyword, including namespace-qualified element names and dynamically computed content.
   - Document constructors producing standalone documents and embedding them in sequences.
   - Namespace declaration and prefix resolution scenarios, including default namespaces and prefix collisions.
   - Error cases (invalid NCName in `processing-instruction`, mismatched end tags, `document{}` with multiple top-level elements if unsupported).
2. Reuse or extend existing helper Fluid functions to assert node structures. Include tests that round-trip constructed nodes through serialisation (if available) or string-value extraction to confirm content.
3. Run the xml module build and relevant tests: `cmake --build build/agents --config FastBuild --target xml --parallel` followed by `ctest --build-config FastBuild --test-dir build/agents -R xpath_constructors` (or the suite covering the new test file).

## 11. Documentation and maintenance
1. Update developer documentation (e.g. `docs/wiki` or module README if applicable) to describe the new constructor support and any limitations.
2. Ensure code comments explain lifetime management for constructed nodes and how namespace scope is propagated.
3. Add regression notes to the test documentation or changelog summarising coverage for document construction expressions.
