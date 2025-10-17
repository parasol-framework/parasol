# XPath Reserved Words Handling Improvement Plan

This document captures a detailed, actionable plan for future work to address the reserved-word issues uncovered by the `xpath_xpath_reserved_words` test suite.

## 1. Establish a Shared Keyword-Information Source
1. Audit the existing keyword mapping tables in `src/xpath/parse/xpath_tokeniser.cpp` to confirm every keyword NCName is listed.
2. Introduce a helper (e.g., `bool keyword_allows_identifier(TokenType type)`) in `src/xpath/parse/xpath_tokeniser.h` or a new shared header that describes when a keyword token can behave as an identifier.
3. Populate the helper with data generated from the existing keyword table rather than hard-coding a new whitelist.
4. Add unit coverage to ensure the helper returns `true` for every reserved word that is syntactically valid as an NCName (e.g., `AND`, `OR`, `IF`, `FOR`, etc.).

## 2. Update Parser Identifier Checks to Use the Shared Source
1. Locate all identifier/step-start helper functions in `src/xpath/parse/xpath_parser.cpp` (e.g., `is_identifier_token`, `is_step_start_token`, `is_literal_token`).
2. Refactor these helpers to consult the shared keyword-information helper from Step 1 instead of maintaining independent whitelists.
3. Ensure any token-type comparisons are consolidated so the parser does not diverge from the tokeniser definition.
4. Add parser-focused unit tests in `src/xpath/tests/test_reserved_words.fluid` or nearby Fluid tests to cover cases where reserved words appear as element names, attribute names, and literals.

## 3. Preserve Expression-Context Keyword Behaviour
1. Identify all parser branches that intentionally expect reserved words as operators or FLWOR clauses (e.g., handling of `AND`, `OR`, `FOR`, `LET`, `RETURN`).
2. Confirm these branches continue to receive keyword tokens unchanged so logical expressions and FLWOR statements behave as before.
3. Where ambiguity exists (such as `FOR` inside path expressions versus FLWOR clauses), add explicit context checks to prioritise expression semantics over identifier usage.
4. Extend the unit tests to include mixed cases (e.g., `//for[@return='value']`) ensuring the parser distinguishes clause contexts from node tests.

## 4. Expand Regression Test Coverage
1. Augment `src/xpath/tests/test_reserved_words.fluid` with additional permutations covering namespaces, predicates, and attribute comparisons involving reserved words.
2. Add regression tests that mirror real-world patterns (e.g., reserved words inside nested paths or combined with arithmetic/logical operators) to guard against future regressions.
3. Run `ctest --build-config FastBuild --test-dir build/agents -L xpath` after rebuilding/installing to verify all reserved-word scenarios pass.

## 5. Document the Keyword Handling Rules
1. Update inline comments near the keyword table and parser helper definitions to explain how the shared helper governs identifier acceptance.
2. Add a short note to `docs/wiki/Fluid-XPath.md` (or the most relevant documentation file) summarising how reserved words are treated when used as node or attribute names.
3. Include references to the new regression tests so future contributors understand the expected behaviours.

Following this plan will centralise keyword-handling logic, eliminate divergent whitelists, and provide comprehensive regression coverage for reserved words in XPath expressions.
