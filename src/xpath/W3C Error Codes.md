# XQuery Error Codes

This file defines standard XQuery/XPath error codes as specified by the W3C.
Reference: https://www.w3.org/2005/xqt-errors/

## Error Code Prefixes

XPST - XPath Static Errors (detected during parsing)
XPTY - XPath Type Errors
XPDY - XPath Dynamic Errors
XQST - XQuery Static Errors
XQTY - XQuery Type Errors
XQDY - XQuery Dynamic Errors
FOER - Function and Operator Errors
FOCA - Casting Errors
FORG - General Function Errors
FORX - Regular Expression Errors
FOTY - Type Errors in Functions and Operators

## Functions and Operators

FOAP0001: Wrong number of arguments.
FOAR0001: Division by zero.
FOAR0002: Numeric operation overflow/underflow.
FOAY0001: Array index out of bounds.
FOAY0002: Negative array length.
FOCA0001: Input value too large for decimal.
FOCA0002: Invalid lexical value.
FOCA0003: Input value too large for integer.
FOCA0005: NaN supplied as float/double value.
FOCA0006: String to be cast to decimal has too many digits of precision.
FOCH0001: Codepoint not valid.
FOCH0002: Unsupported collation.
FOCH0003: Unsupported normalization form.
FOCH0004: Collation does not support collation units.
FODC0001: No context document.
FODC0002: Error retrieving resource.
FODC0003: Function not defined as deterministic.
FODC0004: Invalid collection URI.
FODC0005: Invalid argument to fn:doc or fn:doc-available.
FODC0006: String passed to fn:parse-xml is not a well-formed XML document.
FODC0010: The processor does not support serialization.
FODF1280: Invalid decimal format name.
FODF1310: Invalid decimal format picture string.
FODT0001: Overflow/underflow in date/time operation.
FODT0002: Overflow/underflow in duration operation.
FODT0003: Invalid timezone value.
FOER0000: Unidentified error.
FOFD1340: Invalid date/time formatting parameters.
FOFD1350: Invalid date/time formatting component.
FOJS0001: JSON syntax error.
FOJS0003: JSON duplicate keys.
FOJS0004: JSON: not schema-aware.
FOJS0005: Invalid options.
FOJS0006: Invalid XML representation of JSON.
FOJS0007: Bad JSON escape sequence.
FONS0004: No namespace found for prefix.
FONS0005: Base-uri not defined in the static context.
FOQM0001: Module URI is a zero-length string.
FOQM0002: Module URI not found.
FOQM0003: Static error in dynamically-loaded XQuery module.
FOQM0005: Parameter for dynamically-loaded XQuery module has incorrect type.
FOQM0006: No suitable XQuery processor available.
FORG0001: Invalid value for cast/constructor.
FORG0002: Invalid argument to fn:resolve-uri().
FORG0003: fn:zero-or-one called with a sequence containing more than one item.
FORG0004: fn:one-or-more called with a sequence containing no items.
FORG0005: fn:exactly-one called with a sequence containing zero or more than one item.
FORG0006: Invalid argument type.
FORG0008: The two arguments to fn:dateTime have inconsistent timezones.
FORG0009: Error in resolving a relative URI against a base URI in fn:resolve-uri.
FORG0010: Invalid date/time.
FORX0001: Invalid regular expression flags.
FORX0002: Invalid regular expression.
FORX0003: Regular expression matches zero-length string.
FORX0004: Invalid replacement string.
FOTY0012: Argument to fn:data() contains a node that does not have a typed value.
FOTY0013: The argument to fn:data() contains a function item.
FOTY0014: The argument to fn:string() is a function item.
FOTY0015: An argument to fn:deep-equal() contains a function item.
FOUT1170: Invalid $href argument to fn:unparsed-text() (etc.)
FOUT1190: Cannot decode resource retrieved by fn:unparsed-text() (etc.)
FOUT1200: Cannot infer encoding of resource retrieved by fn:unparsed-text() (etc.)
FOXT0001: No suitable XSLT processor available
FOXT0002: Invalid parameters to XSLT transformation
FOXT0003: XSLT transformation failed
FOXT0004: XSLT transformation has been disabled
FOXT0006: XSLT output contains non-accepted characters

## XSLT

XTDE0030: It is a non-recoverable dynamic error if the effective value of an attribute written using curly brackets, in a position where an attribute value template is permitted, is a value that is not one of the permitted values for that attribute. If the processor is able to detect the error statically (for example, when any XPath expressions within the curly brackets can be evaluated statically), then the processor may optionally signal this as a static error.
XTDE0040: It is a non-recoverable dynamic error if the invocation of the stylesheet specifies a template name that does not match the expanded QName of a named template defined in the stylesheet, whose visibility is public or final.
XTDE0041: It is a if the invocation of the stylesheet specifies a function name and arity that does not match the expanded QName and arity of a named defined in the stylesheet, whose visibility is public or final.
XTDE0044: It is a dynamic error if the invocation of the stylesheet specifies an when no is supplied (either explicitly, or defaulted to the ).
XTDE0045: It is a dynamic error if the invocation of the stylesheet specifies an and the specified mode is not eligible as an initial mode (as defined above).
XTDE0050: It is a dynamic error if a stylesheet declares a visible stylesheet parameter that is explicitly or implicitly mandatory, and no value for this parameter is supplied when the stylesheet is primed. A stylesheet parameter is visible if it is not masked by another global variable or parameter with the same name and higher import precedence. If the parameter is a then the value must be supplied prior to the static analysis phase.
XTDE0160: It is a non-recoverable dynamic error if an element has an effective version of V (with V < 3.0) when the implementation does not support backwards compatible behavior for XSLT version V.
XTDE0290: Where the result of evaluating an XPath expression (or an attribute value template) is required to be a lexical QName, or if it is permitted to be a lexical QName and the actual value takes the form of a lexical QName, then unless otherwise specified it is a non-recoverable dynamic error if the value has a prefix and the defining element has no namespace node whose name matches that prefix. This error may be signaled as a static error if the value of the expression can be determined statically.
XTDE0410: It is a non-recoverable dynamic error if the sequence used to construct the content of an element node contains a namespace node or attribute node that is preceded in the sequence by a node that is neither a namespace node nor an attribute node.
XTDE0420: It is a non-recoverable dynamic error if the sequence used to construct the content of a document node contains a namespace node or attribute node.
XTDE0430: It is a non-recoverable dynamic error if the sequence contains two or more namespace nodes having the same name but different string values (that is, namespace nodes that map the same prefix to different namespace URIs).
XTDE0440: It is a non-recoverable dynamic error if the sequence contains a namespace node with no name and the element node being constructed has a null namespace URI (that is, it is an error to define a default namespace when the element is in no namespace).
XTDE0450: It is a non-recoverable dynamic error if the result sequence contains a function item.
XTDE0540: It is a non-recoverable dynamic error if the conflict resolution algorithm for template rules leaves more than one matching template rule when the declaration of the relevant mode has an on-multiple-match attribute with the value fail.
XTDE0555: It is a non-recoverable dynamic error if xsl:apply-templates, xsl:apply-imports or xsl:next-match is used to process a node using a mode whose declaration specifies on-no-match="fail" when there is no in the whose match pattern matches that node.
XTDE0560: It is a non-recoverable dynamic error if xsl:apply-imports or xsl:next-match is evaluated when the current template rule is .
XTDE0640: In general, a circularity in a stylesheet is a non-recoverable dynamic error.
XTDE0700: It is a non-recoverable dynamic error if a template that has an or parameter is invoked without supplying a value for that parameter.
XTDE0820: It is a non-recoverable dynamic error if the effective value of the name attribute of the xsl:element instruction is not a lexical QName.
XTDE0830: In the case of an xsl:element instruction with no namespace attribute, it is a non-recoverable dynamic error if the effective value of the name attribute is a lexical QName whose prefix is not declared in an in-scope namespace declaration for the xsl:element instruction.
XTDE0835: It is a non-recoverable dynamic error if the effective value of the namespace attribute of the xsl:element instruction is not in the lexical space of the xs:anyURI datatype or if it is the string http://www.w3.org/2000/xmlns/.
XTDE0850: It is a non-recoverable dynamic error if the effective value of the name attribute of an xsl:attribute instruction is not a lexical QName.
XTDE0855: In the case of an xsl:attribute instruction with no namespace attribute, it is a non-recoverable dynamic error if the effective value of the name attribute is the string xmlns.
XTDE0860: In the case of an xsl:attribute instruction with no namespace attribute, it is a non-recoverable dynamic error if the effective value of the name attribute is a lexical QName whose prefix is not declared in an in-scope namespace declaration for the xsl:attribute instruction.
XTDE0865: It is a non-recoverable dynamic error if the effective value of the namespace attribute of the xsl:attribute instruction is not in the lexical space of the xs:anyURI datatype or if it is the string http://www.w3.org/2000/xmlns/.
XTDE0890: It is a non-recoverable dynamic error if the effective value of the name attribute of the xsl:processing-instruction instruction is not both an NCName and a PITarget.
XTDE0905: It is a non-recoverable dynamic error if the string value of the new namespace node is not valid in the lexical space of the datatype xs:anyURI, or if it is the string http://www.w3.org/2000/xmlns/.
XTDE0920: It is a non-recoverable dynamic error if the effective value of the name attribute of the xsl:namespace instruction is neither a zero-length string nor an NCName, or if it is xmlns.
XTDE0925: It is a non-recoverable dynamic error if the xsl:namespace instruction generates a namespace node whose name is xml and whose string value is not http://www.w3.org/XML/1998/namespace, or a namespace node whose string value is http://www.w3.org/XML/1998/namespace and whose name is not xml.
XTDE0930: It is a non-recoverable dynamic error if evaluating the select attribute or the contained of an xsl:namespace instruction results in a zero-length string.
XTDE0980: It is a non-recoverable dynamic error if any undiscarded item in the atomized sequence supplied as the value of the value attribute of xsl:number cannot be converted to an integer, or if the resulting integer is less than 0 (zero).
XTDE1030: It is a non-recoverable dynamic error if, for any sort key component, the set of sort key values evaluated for all the items in the initial sequence, after any type conversion requested, contains a pair of ordinary values for which the result of the XPath lt operator is an error. If the processor is able to detect the error statically, it may optionally signal it as a static error.
XTDE1035: It is a non-recoverable dynamic error if the collation attribute of xsl:sort (after resolving against the base URI) is not a URI that is recognized by the implementation as referring to a collation.
XTDE1110: It is a non-recoverable dynamic error if the collation URI specified to xsl:for-each-group (after resolving against the base URI) is a collation that is not recognized by the implementation. (For notes, .)
XTDE1140: It is a non-recoverable dynamic error if the effective value of the regex attribute of the xsl:analyze-string instruction does not conform to the required syntax for regular expressions, as specified in . If the regular expression is known statically (for example, if the attribute does not contain any expressions enclosed in curly brackets) then the processor may signal the error as a static error.
XTDE1145: It is a non-recoverable dynamic error if the effective value of the flags attribute of the xsl:analyze-string instruction has a value other than the values defined in . If the value of the attribute is known statically (for example, if the attribute does not contain any expressions enclosed in curly brackets) then the processor may signal the error as a static error.
XTDE1420: It is a non-recoverable dynamic error if the arguments supplied to a call on an extension function do not satisfy the rules defined for that particular extension function, or if the extension function reports an error, or if the result of the extension function cannot be converted to an XPath value.
XTDE1425: When the containing element is processed with XSLT 1.0 behavior, it is a non-recoverable dynamic error to evaluate an extension function call if no implementation of the extension function is available.
XTDE1450: When a processor performs fallback for an extension instruction that is not recognized, if the instruction element has one or more xsl:fallback children, then the content of each of the xsl:fallback children must be evaluated; it is a non-recoverable dynamic error if it has no xsl:fallback children.
XTDE1460: It is a non-recoverable dynamic error if the effective value of the format attribute of an xsl:result-document element is not a valid EQName, or if it does not match the expanded QName of an output definition in the containing package. If the processor is able to detect the error statically (for example, when the format attribute contains no curly brackets), then the processor may optionally signal this as a static error.
XTDE1480: It is a non-recoverable dynamic error to evaluate the xsl:result-document instruction in temporary output state.
XTDE1490: It is a non-recoverable dynamic error for a transformation to generate two or more final result trees with the same URI.
XTDE1500: It is a recoverable dynamic error for a stylesheet to write to an external resource and read from the same resource during a single transformation, if the same absolute URI is used to access the resource in both cases.
XTDE1665: A may be raised if the input to the processor includes an item that requires availability of an optional feature that the processor does not provide.
XTDE2210: It is a dynamic error if there are two xsl:merge-key elements that occupy corresponding positions among the xsl:merge-key children of two different xsl:merge-source elements and that have differing effective values for any of the attributes lang, order, collation, case-order, or data-type. Values are considered to differ if the attribute is present on one element and not on the other, or if it is present on both elements with effective values that are not equal to each other. In the case of the collation attribute, the values are compared as absolute URIs after resolving against the base URI. The error may be reported statically if it is detected statically.
XTDE2220: It is a dynamic error if any input sequence to an xsl:merge instruction contains two items that are not correctly sorted according to the merge key values defined on the xsl:merge-key children of the corresponding xsl:merge-source element, when compared using the collation rules defined by the attributes of the corresponding xsl:merge-key children of the xsl:merge instruction, unless the attribute sort-before-merge is present with the value yes.
XTDE3052: It is a dynamic error if an invocation of an abstract component is evaluated.
XTDE3160: It is a non-recoverable dynamic error if the target expression of an xsl:evaluate instruction is not a valid (that is, if a static error occurs when analyzing the string according to the rules of the XPath specification).
XTDE3175: It is a if an xsl:evaluate instruction is evaluated when use of xsl:evaluate has been statically or dynamically disabled.
XTDE3365: A dynamic error occurs if the set of keys in the maps resulting from evaluating the sequence constructor within an xsl:map instruction contains duplicates.
XTDE3530: It is a if an xsl:try instruction is unable to recover the state of a final result tree because recovery has been disabled by use of the attribute rollback-output="no".
XTMM9000: When a transformation is terminated by use of <xsl:message terminate="yes"/>, the effect is the same as when a non-recoverable dynamic error occurs during the transformation. The default error code is XTMM9000; this may be overridden using the error-code attribute of the xsl:message instruction.
XTMM9001: When a transformation is terminated by use of xsl:assert, the effect is the same as when a non-recoverable dynamic error occurs during the transformation. The default error code is XTMM9001; this may be overridden using the error-code attribute of the xsl:assert instruction.
XTSE0010: It is a static error if an XSLT-defined element is used in a context where it is not permitted, if a required attribute is omitted, or if the content of the element does not correspond to the content that is allowed for the element.
XTSE0020: It is a static error if an attribute (other than an attribute written using curly brackets in a position where an attribute value template is permitted) contains a value that is not one of the permitted values for that attribute.
XTSE0080: It is a static error to use a reserved namespace in the name of a named template, a mode, an attribute set, a key, a decimal-format, a variable or parameter, a stylesheet function, a named output definition, an , or a character map; except that the name xsl:initial-template is permitted as a template name.
XTSE0085: It is a to use a in the name of any or , other than a function or instruction defined in this specification or in a normatively referenced specification. It is a to use a prefix bound to a reserved namespace in the [xsl:]extension-element-prefixes attribute.
XTSE0090: It is a static error for an element from the XSLT namespace to have an attribute whose namespace is either null (that is, an attribute with an unprefixed name) or the XSLT namespace, other than attributes defined for the element in this document.
XTSE0110: The value of the version attribute if present must be a number: specifically, it must be a valid instance of the type xs:decimal as defined in .
XTSE0120: An xsl:stylesheet, xsl:transform, or xsl:package element must not have any text node children.
XTSE0125: It is a static error if the value of an [xsl:]default-collation attribute, after resolving against the base URI, contains no URI that the implementation recognizes as a collation URI.
XTSE0130: It is a static error if an xsl:stylesheet, xsl:transform, or xsl:package element has a child element whose name has a null namespace URI.
XTSE0150: A literal result element that is used as the outermost element of a simplified stylesheet module must have an xsl:version attribute.
XTSE0165: It is a static error if the processor is not able to retrieve the resource identified by the URI reference in the href attribute of xsl:include or xsl:import , or if the resource that is retrieved does not contain a stylesheet module conforming to this specification.
XTSE0170: An xsl:include element must be a top-level element.
XTSE0180: It is a static error if a stylesheet module directly or indirectly includes itself.
XTSE0190: An xsl:import element must be a top-level element.
XTSE0210: It is a static error if a stylesheet module directly or indirectly imports itself.
XTSE0215: It is a static error if an xsl:import-schema element that contains an xs:schema element has a schema-location attribute, or if it has a namespace attribute that conflicts with the target namespace of the contained schema.
XTSE0220: It is a static error if the synthetic schema document does not satisfy the constraints described in (section 5.1, Errors in Schema Construction and Structure). This includes, without loss of generality, conflicts such as multiple definitions of the same name.
XTSE0260: Within an XSLT element that is required to be empty, any content other than comments or processing instructions, including any whitespace text node preserved using the xml:space="preserve" attribute, is a static error.
XTSE0265: It is a static error if there is a stylesheet module in a package that specifies input-type-annotations="strip" and another stylesheet module that specifies input-type-annotations="preserve", or if a stylesheet module specifies the value strip or preserve and the same value is not specified on the xsl:package element of the containing package.
XTSE0270: It is a static error if within any package the same NameTest appears in both an xsl:strip-space and an xsl:preserve-space declaration if both have the same import precedence. Two NameTests are considered the same if they match the same set of names (which can be determined by comparing them after expanding namespace prefixes to URIs).
XTSE0280: In the case of a prefixed lexical QName used as the value (or as part of the value) of an attribute in the stylesheet, or appearing within an XPath expression in the stylesheet, it is a static error if the defining element has no namespace node whose name matches the prefix of the lexical QName.
XTSE0340: Where an attribute is defined to contain a pattern, it is a static error if the pattern does not match the production Pattern30.
XTSE0350: It is a static error if an unescaped left curly bracket appears in a fixed part of a value template without a matching right curly bracket.
XTSE0370: It is a static error if an unescaped right curly bracket occurs in a fixed part of a value template.
XTSE0500: An xsl:template element must have either a match attribute or a name attribute, or both. An xsl:template element that has no match attribute must have no mode attribute and no priority attribute. An xsl:template element that has no name attribute must have no visibility attribute.
XTSE0530: The value of the priority attribute of the xsl:template element must conform to the rules for the xs:decimal type defined in . Negative values are permitted.
XTSE0545: It is a static error if for any named or unnamed mode, a package explicitly specifies two conflicting values for the same attribute in different xsl:mode declarations having the same import precedence, unless there is another definition of the same attribute with higher import precedence. The attributes in question are the attributes other than name on the xsl:mode element, and the as attribute on the contained xsl:context-item element if present.
XTSE0550: It is a static error if the list of modes in the mode attribute of xsl:template is empty, if the same token is included more than once in the list, if the list contains an invalid token, or if the token #all appears together with any other value.
XTSE0580: It is a static error if the values of the name attribute of two sibling xsl:param elements represent the same expanded QName.
XTSE0620: It is a static error if a variable-binding element has a select attribute and has non-empty content.
XTSE0630: It is a static error if a package contains more than one non-hidden binding of a global variable with the same name and same import precedence, unless it also contains another binding with the same name and higher import precedence.
XTSE0650: It is a static error if a package contains an xsl:call-template instruction whose name attribute does not match the name attribute of any named template visible in the containing package (this includes any template defined in this package, as well as templates accepted from used packages whose visibility in this package is not hidden). For more details of the process of binding the called template, see .
XTSE0660: It is a static error if a package contains more than one non-hidden template with the same name and the same import precedence, unless it also contains a template with the same name and higher import precedence.
XTSE0670: It is a static error if two or more sibling xsl:with-param elements have name attributes that represent the same expanded QName.
XTSE0680: In the case of xsl:call-template, it is a static error to pass a non-tunnel parameter named x to a template that does not have a non-tunnel template parameter named x, unless the xsl:call-template instruction is processed with XSLT 1.0 behavior.
XTSE0690: It is a static error if a package contains both (a) a named template named T that is not overridden by another named template of higher import precedence and that has an non-tunnel parameter named P, and (b) an xsl:call-template instruction whose name attribute equals T and that has no non-tunnel xsl:with-param child element whose name attribute equals P. (All names are compared as QNames.)
XTSE0710: It is a static error if the value of the use-attribute-sets attribute of an xsl:copy, xsl:element, or xsl:attribute-set element, or the xsl:use-attribute-sets attribute of a literal result element, is not a whitespace-separated sequence of EQNames, or if it contains an EQName that does not match the name attribute of any xsl:attribute-set declaration in the containing package.
XTSE0730: If an xsl:attribute set element specifies streamable="yes" then every attribute set referenced in its use-attribute-sets attribute (if present) must also specify streamable="yes".
XTSE0740: It is a static error if a stylesheet function has a name that is in no namespace.
XTSE0760: It is a static error if an xsl:param child of an xsl:function element has either a select attribute or non-empty content.
XTSE0770: It is a static error for a package to contain two or more xsl:function declarations with the same expanded QName, the same arity, and the same import precedence, unless there is another xsl:function declaration with the same expanded QName and arity, and a higher import precedence.
XTSE0805: It is a static error if an attribute on a literal result element is in the XSLT namespace, unless it is one of the attributes explicitly defined in this specification.
XTSE0808: It is a static error if a namespace prefix is used within the [xsl:]exclude-result-prefixes attribute and there is no namespace binding in scope for that prefix.
XTSE0809: It is a static error if the value #default is used within the [xsl:]exclude-result-prefixes attribute and the parent element of the [xsl:]exclude-result-prefixes attribute has no default namespace.
XTSE0810: It is a static error if within a package there is more than one such declaration more than one xsl:namespace-alias declaration with the same literal namespace URI and the same import precedence and different values for the target namespace URI, unless there is also an xsl:namespace-alias declaration with the same literal namespace URI and a higher import precedence.
XTSE0812: It is a static error if a value other than #default is specified for either the stylesheet-prefix or the result-prefix attributes of the xsl:namespace-alias element when there is no in-scope binding for that namespace prefix.
XTSE0840: It is a static error if the select attribute of the xsl:attribute element is present unless the element has empty content.
XTSE0870: It is a static error if the select attribute of the xsl:value-of element is present when the content of the element is non-empty., or if the select attribute is absent when the content is empty.
XTSE0880: It is a static error if the select attribute of the xsl:processing-instruction element is present unless the element has empty content.
XTSE0910: It is a static error if the select attribute of the xsl:namespace element is present when the element has content other than one or more xsl:fallback instructions, or if the select attribute is absent when the element has empty content.
XTSE0940: It is a static error if the select attribute of the xsl:comment element is present unless the element has empty content.
XTSE0975: It is a static error if the value attribute of xsl:number is present unless the select, level, count, and from attributes are all absent.
XTSE1015: It is a static error if an xsl:sort element with a select attribute has non-empty content.
XTSE1017: It is a static error if an xsl:sort element other than the first in a sequence of sibling xsl:sort elements has a stable attribute.
XTSE1040: It is a static error if an xsl:perform-sort instruction with a select attribute has any content other than xsl:sort and xsl:fallback instructions.
XTSE1080: These four attributes the group-by, group-adjacent, group-starting-with, and group-ending-with attributes of xsl:for-each-group are mutually exclusive: it is a static error if none of these four attributes is present or if more than one of them is present.
XTSE1090: It is a static error to specify the collation attribute or the composite attribute if neither the group-by attribute nor group-adjacent attribute is specified.
XTSE1130: It is a static error if the xsl:analyze-string instruction contains neither an xsl:matching-substring nor an xsl:non-matching-substring element.
XTSE1205: It is a static error if an xsl:key declaration has a use attribute and has non-empty content, or if it has empty content and no use attribute.
XTSE1210: It is a static error if the xsl:key declaration has a collation attribute whose value (after resolving against the base URI) is not a URI recognized by the implementation as referring to a collation.
XTSE1220: It is a static error if there are several xsl:key declarations in the same package with the same key name and different effective collations. Two collations are the same if their URIs are equal under the rules for comparing xs:anyURI values, or if the implementation can determine that they are different URIs referring to the same collation.
XTSE1222: It is a static error if there are several xsl:key declarations in a package with the same key name and different effective values for the composite attribute.
XTSE1290: It is a static error if a named or unnamed decimal format contains two conflicting values for the same attribute in different xsl:decimal-format declarations having the same import precedence, unless there is another definition of the same attribute with higher import precedence.
XTSE1295: It is a static error if the character specified in the zero-digit attribute is not a digit or is a digit that does not have the numeric value zero.
XTSE1300: It is a static error if, for any named or unnamed decimal format, the variables representing characters used in a picture string do not each have distinct values. These variables are decimal-separator-sign, grouping-sign, percent-sign, per-mille-sign, digit-zero-sign, digit-sign, and pattern-separator-sign.
XTSE1430: It is a static error if there is no namespace bound to the prefix on the element bearing the [xsl:]extension-element-prefixes attribute or, when #default is specified, if there is no default namespace.
XTSE1505: It is a static error if both the [xsl:]type and [xsl:]validation attributes are present on the xsl:element, xsl:attribute, xsl:copy, xsl:copy-of, xsl:document, xsl:result-document, xsl:source-document, or xsl:merge-source elements, or on a literal result element.
XTSE1520: It is a static error if the value of the type attribute of an xsl:element, xsl:attribute, xsl:copy, xsl:copy-of, xsl:document, or xsl:result-document instruction, or the xsl:type attribute of a literal result element, is not a valid QName, or if it uses a prefix that is not defined in an in-scope namespace declaration, or if the QName is not the name of a type definition included in the in-scope schema components for the package.
XTSE1530: It is a static error if the value of the type attribute of an xsl:attribute instruction refers to a complex type definition
XTSE1560: It is a static error if two xsl:output declarations within an output definition specify explicit values for the same attribute (other than cdata-section-elements, suppress-indentation, and use-character-maps), with the values of the attributes being not equal, unless there is another xsl:output declaration within the same output definition that has higher import precedence and that specifies an explicit value for the same attribute.
XTSE1570: The value of the method attribute on xsl:output must (if present) be a valid EQName. If it is a lexical QName with no a prefix, then it identifies a method specified in and must be one of xml, html, xhtml, or text.
XTSE1580: It is a static error if a package contains two or more character maps with the same name and the same import precedence, unless it also contains another character map with the same name and higher import precedence.
XTSE1590: It is a static error if a name in the use-character-maps attribute of the xsl:output or xsl:character-map elements does not match the name attribute of any xsl:character-map in the containing package.
XTSE1600: It is a static error if a character map references itself, directly or indirectly, via a name in the use-character-maps attribute.
XTSE1650: A non-schema-aware processor must signal a static error if a package includes an xsl:import-schema declaration.
XTSE1660: A non-schema-aware processor must signal a static error if a package includes an [xsl:]type attribute; or an [xsl:]validation or [xsl:]default-validation attribute with a value other than strip, preserve, or lax; or an xsl:mode element whose typed attribute is equal to yes or strict; or an as attribute whose value is a that can only match nodes with a type annotation other than xs:untyped or xs:untypedAtomic (for example, as="element(*, xs:integer)").
XTSE2200: It is a static error if the number of xsl:merge-key children of a xsl:merge-source element is not equal to the number of xsl:merge-key children of another xsl:merge-source child of the same xsl:merge instruction.
XTSE3000: It is a if no package matching the package name and version specified in an xsl:use-package declaration can be located.
XTSE3005: It is a if a package is dependent on itself, where package A is defined as being dependent on package B if A contains an xsl:use-package declaration that references B, or if A contains an xsl:use-package declaration that references a package C that is itself dependent on B.
XTSE3008: It is a if an xsl:use-package declaration appears in a that is not in the same as the of the .
XTSE3010: It is a static error if the explicit exposed visibility of a component is inconsistent with its declared visibility, as defined in the above table. (This error occurs only when the component declaration has an explicit visibility attribute, and the component is also listed explicitly by name in an xsl:expose declaration.)
XTSE3020: It is a static error if a token in the names attribute of xsl:expose, other than a wildcard, matches no component in the containing package.
XTSE3022: It is a static error if the component attribute of xsl:expose specifies * (meaning all component kinds) and the names attribute is not a wildcard.
XTSE3025: It is a static error if the effect of an xsl:expose declaration would be to make a component abstract, unless the component is already abstract in the absence of the xsl:expose declaration.
XTSE3030: It is a static error if a token in the names attribute of xsl:accept, other than a wildcard, matches no component in the used package.
XTSE3032: It is a static error if the component attribute of xsl:accept specifies * (meaning all component kinds) and the names attribute is not a wildcard.
XTSE3040: It is a static error if the visibility assigned to a component by an xsl:accept element is incompatible with the visibility of the corresponding component in the used package, as defined by the above table, unless the token that matches the component name is a wildcard, in which case the xsl:accept element is treated as not matching that component.
XTSE3050: It is a static error if the xsl:use-package elements in a package manifest cause two or more homonymous components to be accepted with a visibility other than hidden.
XTSE3051: It is a static error if a token in the names attribute of xsl:accept, other than a wildcard, matches the symbolic name of a component declared within an xsl:override child of the same xsl:use-package element.
XTSE3055: It is a static error if a component declaration appearing as a child of xsl:override is homonymous with any other declaration in the using package, regardless of import precedence, including any other overriding declaration in the package manifest of the using package.
XTSE3058: It is a static error if a component declaration appearing as a child of xsl:override does not match (is not homonymous with) some component in the used package.
XTSE3060: It is a static error if the component referenced by an xsl:override declaration has visibility other than public or abstract
XTSE3070: It is a static error if the signature of an overriding component is not compatible with the signature of the component that it is overriding.
XTSE3075: It is a static error to use the component reference xsl:original when the overridden component has visibility="abstract".
XTSE3080: It is a static error if a top-level package (as distinct from a library package) contains symbolic references referring to components whose visibility is abstract.
XTSE3085: It is a static error, when the effective value of the declared-modes attribute of an xsl:package element is yes, if the package contains an explicit reference to an undeclared mode, or if it implicitly uses the unnamed mode and the unnamed mode is undeclared.
XTSE3087: It is a static error if more than one xsl:global-context-item declaration appears within a , or if several modules within a single contain inconsistent xsl:global-context-item declarations
XTSE3088: It is a if the as attribute is present on the xsl:context-item element when use="absent" is specified.
XTSE3089: It is a if the as attribute is present on the xsl:global-context-item element when use="absent" is specified.
XTSE3105: It is a static error if a template rule applicable to a mode that is defined with typed="strict" uses a match pattern that contains a RelativePathExprP whose first StepExprP is an AxisStepP whose ForwardStepP uses an axis whose principal node kind is Element and whose NodeTest is an EQName that does not correspond to the name of any global element declaration in the in-scope schema components.
XTSE3120: It is a static error if an xsl:break or xsl:next-iteration element appears other than in a tail position within the sequence constructor forming the body of an xsl:iterate instruction.
XTSE3125: It is a static error if the select attribute of xsl:break or xsl:on-completion is present and the instruction has children.
XTSE3130: It is a static error if the name attribute of an xsl:with-param child of an xsl:next-iteration element does not match the name attribute of an xsl:param child of the innermost containing xsl:iterate instruction.
XTSE3140: It is a static error if the select attribute of the xsl:try element is present and the element has children other than xsl:catch and xsl:fallback elements.
XTSE3150: It is a static error if the select attribute of the xsl:catch element is present unless the element has empty content.
XTSE3155: It is a static error if an xsl:function element with no xsl:param children has a streamability attribute with any value other than unclassified.
XTSE3185: It is a static error if the select attribute of xsl:sequence is present and the instruction has children other than xsl:fallback.
XTSE3190: It is a static error if two sibling xsl:merge-source elements have the same name.
XTSE3195: If the for-each-item is present then the for-each-source, use-accumulators, and streamable attributes must both be absent. If the use-accumulators attribute is present then the for-each-source attribute must be present. If the for-each-source attribute is present then the for-each-item attribute must be absent.
XTSE3200: It is a static error if an xsl:merge-key element with a select attribute has non-empty content.
XTSE3280: It is a static error if the select attribute of the xsl:map-entry element is present unless the element has no children other than xsl:fallback elements.
XTSE3300: It is a static error if the list of accumulator names in the use-accumulators attribute contains an invalid token, contains the same token more than once, or contains the token #all along with any other value; or if any token (other than #all) is not the name of a accumulator visible in the containing package.
XTSE3350: It is a static error for a package to contain two or more non-hidden accumulators with the same expanded QName and the same import precedence, unless there is another accumulator with the same expanded QName, and a higher import precedence.
XTSE3430: It is a if a package contains a construct that is declared to be streamable but which is not , unless the user has indicated that the processor is to handle this situation by processing the stylesheet without streaming or by making use of processor extensions to the streamability rules where available.
XTSE3440: In the case of a (that is, an xsl:template element having a match attribute) appearing as a child of xsl:override, it is a static error if the list of modes in the mode attribute contains #all or #unnamed, or if it contains #default and the default mode is the , or if the mode attribute is omitted when the default mode is the .
XTSE3450: It is a static error if a variable declared with static="yes" is inconsistent with another static variable of the same name that is declared earlier in stylesheet tree order and that has lower .
XTSE3460: It is a if an xsl:apply-imports element appears in a declared within an xsl:override element. (To invoke the template rule that is being overridden, xsl:next-match should therefore be used.)
XTSE3520: It is a static error if a parameter to xsl:iterate is .
XTSE3540: A processor that does not provide the raises a if any of the following XPath constructs are found in an , , , or ItemType: a TypedFunctionTest, a NamedFunctionRef, an InlineFunctionExpr, or an ArgumentPlaceholder
XTTE0505: It is a type error if the result of evaluating the sequence constructor cannot be converted to the required type.
XTTE0510: It is a type error if an xsl:apply-templates instruction with no select attribute is evaluated when the context item is not a node.
XTTE0570: It is a type error if the supplied value of a variable cannot be converted to the required type.
XTTE0590: It is a type error if the conversion of the supplied value of a parameter to its fails.
XTTE0780: If the as attribute of xsl:function is specified, then the result evaluated by the sequence constructor (see ) is converted to the required type, using the function conversion rules. It is a type error if this conversion fails.
XTTE0945: It is a type error to use the xsl:copy instruction with no select attribute when the context item is absent.
XTTE0950: It is a type error to use the xsl:copy or xsl:copy-of instruction to copy a node that has namespace-sensitive content if the copy-namespaces attribute has the value no and its explicit or implicit validation attribute has the value preserve. It is also a type error if either of these instructions (with validation="preserve") is used to copy an attribute having namespace-sensitive content, unless the parent element is also copied. A node has namespace-sensitive content if its typed value contains an item of type xs:QName or xs:NOTATION or a type derived therefrom. The reason this is an error is because the validity of the content depends on the namespace context being preserved.
XTTE0990: It is a type error if the xsl:number instruction is evaluated, with no value or select attribute, when the context item is not a node.
XTTE1000: It is a type error if the result of evaluating the select attribute of the xsl:number instruction is anything other than a single node.
XTTE1020: If any sort key value, after atomization and any type conversion required by the data-type attribute, is a sequence containing more than one item, then the effect depends on whether the xsl:sort element is processed with XSLT 1.0 behavior. With XSLT 1.0 behavior, the effective sort key value is the first item in the sequence. In other cases, this is a type error.
XTTE1100: It is a type error if the result of evaluating the group-adjacent expression is an empty sequence or a sequence containing more than one item, unless composite="yes" is specified.
XTTE1510: If the validation attribute of an xsl:element, xsl:attribute, xsl:copy, xsl:copy-of, or xsl:result-document instruction, or the xsl:validation attribute of a literal result element, has the effective value strict, and schema validity assessment concludes that the validity of the element or attribute is invalid or unknown, a type error occurs. As with other type errors, the error may be signaled statically if it can be detected statically.
XTTE1512: If the validation attribute of an xsl:element, xsl:attribute, xsl:copy, xsl:copy-of, or xsl:result-document instruction, or the xsl:validation attribute of a literal result element, has the effective value strict, and there is no matching top-level declaration in the schema, then a type error occurs. As with other type errors, the error may be signaled statically if it can be detected statically.
XTTE1515: If the validation attribute of an xsl:element, xsl:attribute, xsl:copy, xsl:copy-of, or xsl:result-document instruction, or the xsl:validation attribute of a literal result element, has the effective value lax, and schema validity assessment concludes that the element or attribute is invalid, a type error occurs. As with other type errors, the error may be signaled statically if it can be detected statically.
XTTE1535: It is a type error if the value of the type attribute of an xsl:copy or xsl:copy-of instruction refers to a complex type definition and one or more of the items being copied is an attribute node.
XTTE1540: It is a type error if an [xsl:]type attribute is defined for a constructed element or attribute, and the outcome of schema validity assessment against that type is that the validity property of that element or attribute information item is other than valid.
XTTE1545: A type error occurs if a type or validation attribute is defined (explicitly or implicitly) for an instruction that constructs a new attribute node, if the effect of this is to cause the attribute value to be validated against a type that is derived from, or constructed by list or union from, the primitive types xs:QName or xs:NOTATION.
XTTE1550: A type error occurs when a document node is validated unless the children of the document node comprise exactly one element node, no text nodes, and zero or more comment and processing instruction nodes, in any order.
XTTE1555: It is a type error if, when validating a document node, document-level constraints (such as ID/IDREF constraints) are not satisfied. These constraints include identity constraints (xs:unique, xs:key, and xs:keyref) and ID/IDREF constraints.
XTTE2230: It is a type error if some item selected by a particular merge key in one input sequence is not comparable using the XPath le operator with some item selected by the corresponding sort key in another input sequence.
XTTE3090: It is a type error if the xsl:context-item child of xsl:template specifies that a context item is required and none is supplied by the caller, that is, if the context item is absent at the point where xsl:call-template is evaluated.
XTTE3100: It is a type error if an xsl:apply-templates instruction in a particular mode selects an element or attribute whose type is xs:untyped or xs:untypedAtomic when the typed attribute of that mode specifies the value yes, strict, or lax.
XTTE3110: It is a type error if an xsl:apply-templates instruction in a particular mode selects an element or attribute whose type is anything other than xs:untyped or xs:untypedAtomic when the typed attribute of that mode specifies the value no.
XTTE3165: It is a type error if the result of evaluating the expression in the with-params attribute of the xsl:evaluate instruction is anything other than a single map of type map(xs:QName, item()*).
XTTE3170: It is a type error if the result of evaluating the namespace-context attribute of the xsl:evaluate instruction is anything other than a single node.
XTTE3180: It is a type error if the result of evaluating the select expression of the xsl:copy element is a sequence of more than one item.
XTTE3210: If the result of evaluating the context-item expression of an xsl:evaluate instruction is a sequence containing more than one item, then a is signaled.
XTTE3375: A type error occurs if the result of evaluating the sequence constructor within an xsl:map instruction is not an instance of the required type map(*)*.

## XQuery and XPath

XPDY0002: It is a dynamic error if evaluation of an expression relies on some part of the dynamic context that has not been assigned a valueis .
XQDY0025: It is a dynamic error if any attribute of a constructed element does not have a name that is distinct from the names of all other attributes of the constructed element.
XQDY0026: It is a dynamic error if the result of the content expression of a computed processing instruction constructor contains the string "?>".
XQDY0027: In a validate expression, it is a dynamic error if the root element information item in the PSVI resulting from validation does not have the expected validity property: valid if validation mode is strict, or either valid or notKnown if validation mode is lax.
XQDY0041: It is a dynamic error if the value of the name expression in a computed processing instruction constructor cannot be cast to the type xs:NCName.
XQDY0044: It is a dynamic error the node-name of a node constructed by a computed attribute constructor has any of the following properties: Its namespace prefix is xmlns. It has no namespace prefix and its local name is xmlns. Its namespace URI is http://www.w3.org/2000/xmlns/. Its namespace prefix is xml and its namespace URI is not http://www.w3.org/XML/1998/namespace. Its namespace prefix is other than xml and its namespace URI is http://www.w3.org/XML/1998/namespace.
XPDY0050: It is a dynamic error if the dynamic type of the operand of a treat expression does not match the sequence type specified by the treat expression. This error might also be raised by a path expression beginning with "/" or "//" if the context node is not in a tree that is rooted at a document node. This is because a leading "/" or "//" in a path expression is an abbreviation for an initial step that includes the clause treat as document-node().
XQDY0054: It is a dynamic error if a cycle is encountered in the definition of a module's dynamic context components, for example because of a cycle in variable declarations.
XQDY0061: It is a dynamic error if the operand of a validate expression is a document node whose children do not consist of exactly one element node and zero or more comment and processing instruction nodes, in any order.
XQDY0064: It is a dynamic error if the value of the name expression in a computed processing instruction constructor is equal to "XML" (in any combination of upper and lower case).
XQDY0072: It is a dynamic error if the result of the content expression of a computed comment constructor contains two adjacent hyphens or ends with a hyphen.
XQDY0074: It is a dynamic error if the value of the name expression in a computed element or attribute constructor cannot be converted to an expanded QName (for example, because it contains a namespace prefix not found in statically known namespaces.)
XQDY0084: It is a dynamic error if the element validated by a validate statement does not have a top-level element declaration in the in-scope element declarations, if validation mode is strict.
XQDY0091: An implementation MAY raise a dynamic error if an xml:id error, as defined in , is encountered during construction of an attribute named xml:id.
XQDY0092: An implementation MAY raise a dynamic error if a constructed attribute named xml:space has a value other than preserve or default.
XQDY0096: It is a dynamic error if the node-name of a node constructed by a computed element constructor has any of the following properties: Its namespace prefix is xmlns. Its namespace URI is http://www.w3.org/2000/xmlns/. Its namespace prefix is xml and its namespace URI is not http://www.w3.org/XML/1998/namespace. Its namespace prefix is other than xml and its namespace URI is http://www.w3.org/XML/1998/namespace.
XQDY0101: An error is raised if a computed namespace constructor attempts to do any of the following: Bind the prefix xml to some namespace URI other than http://www.w3.org/XML/1998/namespace. Bind a prefix other than xml to the namespace URI http://www.w3.org/XML/1998/namespace. Bind the prefix xmlns to any namespace URI. Bind a prefix to the namespace URI http://www.w3.org/2000/xmlns/. Bind any prefix (including the empty prefix) to a zero-length namespace URI.
XQDY0102: In an element constructor, if two or more namespace bindings in the in-scope bindings would have the same prefix, then an error is raised if they have different URIs; if they would have the same prefix and URI, duplicate bindings are ignored. If the name of an element in an element constructor is in no namespace, creating a default namespace for that element using a computed namespace constructor is an error.
XPDY0130: An implementation-dependent limit has been exceeded.
XPDY0137: No two keys in a map may have the same key value.
XPST0001: It is a static error if analysis of an expression relies on some component of the static context that has not been assigned a valueis .
XPST0003: It is a static error if an expression is not a valid instance of the grammar defined in .
XPST0005: During the analysis phase, it is a static error if the static type assigned to an expression other than the expression () or data(()) is empty-sequence().
XPST0008: It is a static error if an expression refers to an element name, attribute name, schema type name, namespace prefix, or variable name that is not defined in the static context, except for an ElementName in an ElementTest or an AttributeName in an AttributeTest.
XQST0009: An implementation that does not support the Schema Aware Feature must raise a static error if a Prolog contains a schema import.
XPST0010: An implementation that does not support the namespace axis must raise a static error if it encounters a reference to the namespace axis and XPath 1.0 compatibility mode is false.
XQST0012: It is a static error if the set of definitions contained in all schemas imported by a Prolog do not satisfy the conditions for schema validity specified in Sections 3 and 5 of Part 1 of or --i.e., each definition must be valid, complete, and unique.
XQST0013: It is a static error if an implementation recognizes a pragma but determines that its content is invalid.
XQST0016: An implementation that does not support the Module Feature raises a static error if it encounters a module declaration or a module import.
XPST0017: It is a static error if the expanded QName and number of arguments in a static function call do not match the name and arity of a function signature in the static context.
XQST0022: It is a static error if the value of a namespace declaration attribute is not a URILiteral.contains an EnclosedExpr.
XQST0031: It is a static error if the version number specified in a version declaration is not supported by the implementation.
XQST0032: A static error is raised if a Prolog contains more than one base URI declaration.
XQST0033: It is a static error if a module contains multiple bindings for the same namespace prefix.
XQST0034: It is a static error if multiple functions declared or imported by a module have the same number of arguments and their expanded QNames are equal (as defined by the eq operator).
XQST0035: It is a static error to import two schema components that both define the same name in the same symbol space and in the same scope.
XQST0038: It is a static error if a Prolog contains more than one default collation declaration, or the value specified by a default collation declaration is not present in statically known collations.
XPST0039: It is a static error for a function declaration or an inline function expression to have more than one parameter with the same name.
XQST0040: It is a static error if the attributes specified by a direct element constructor do not have distinct expanded QNames.
XQST0045: It is a static error if the name of a variable annotation, a function annotation, or the function name in a function declaration is in a reserved namespace.
XPST0046: An implementation MAYMAY raise a static error if the value of a URILiteral or a BracedURILiteral is of nonzero length and is not in the lexical space of xs:anyURIneither an absolute URI nor a relative URI.
XQST0047: It is a static error if multiple module imports in the same Prolog specify the same target namespace.
XQST0048: It is a static error if a function or variable declared in a library module is not in the target namespace of the library module.
XQST0049: It is a static error if two or more variables declared or imported by a module have equal expanded QNames (as defined by the eq operator.)
XPST0051: It is a static error if the expanded QName for an AtomicOrUnionType in a SequenceType is not defined in the in-scope schema types as a generalized atomic type.
XPST0052: The type named in a cast or castable expression must be the name of a type defined in the in-scope schema types, and the{variety} of the type must be simple.
XQST0055: It is a static error if a Prolog contains more than one copy-namespaces declaration.
XQST0057: It is a static error if a schema import binds a namespace prefix but does not specify a target namespace other than a zero-length string.
XQST0058: It is a static error if multiple schema imports specify the same target namespace.
XQST0059: It is a static error if an implementation is unable to process a schema or module import by finding a schema or module with the specified target namespace.
XQST0060: It is a static error if the name of a function in a function declaration is not in a namespace (expanded QName has a null namespace URI).
XQST0065: A static error is raised if a Prolog contains more than one ordering mode declaration.
XQST0066: A static error is raised if a Prolog contains more than one default element/type namespace declaration, or more than one default function namespace declaration.
XQST0067: A static error is raised if a Prolog contains more than one construction declaration.
XQST0068: A static error is raised if a Prolog contains more than one boundary-space declaration.
XQST0069: A static error is raised if a Prolog contains more than one empty order declaration.
XPST0070: A static error is raised if one of the predefined prefixes xml or xmlns appears in a namespace declaration or a default namespace declaration, or if any of the following conditions is statically detected in any expression or declaration: A static error is raised if any of the following conditions is statically detected in any expression: The prefix xml is bound to some namespace URI other than http://www.w3.org/XML/1998/namespace. A prefix other than xml is bound to the namespace URI http://www.w3.org/XML/1998/namespace. The prefix xmlns is bound to any namespace URI. A prefix other than xmlns is bound to the namespace URI http://www.w3.org/2000/xmlns/.
XQST0071: A static error is raised if the namespace declaration attributes of a direct element constructor do not have distinct names.
XQST0075: An implementation that does not support the Schema Aware Feature Validation Feature must raise a static error if it encounters a validate expression.
XQST0076: It is a static error if a collation subclause in an order by or group by clause of a FLWOR expression does not identify a collation that is present in statically known collations.
XQST0079: It is a static error if an extension expression contains neither a pragma that is recognized by the implementation nor an expression enclosed in curly braces.
XPST0080: It is a static error if the target type of a cast or castable expression is xs:NOTATION, xs:anySimpleType, or xs:anyAtomicType.
XPST0081: It is a static error if a QName used in a queryan expression contains a namespace prefix that cannot be expanded into a namespace URI by using the statically known namespaces.
XQST0085: It is a static error if the namespace URI in a namespace declaration attribute is a zero-length string, and the implementation does not support .
XQST0087: It is a static error if the encoding specified in a Version Declaration does not conform to the definition of EncName specified in .
XQST0088: It is a static error if the literal that specifies the target namespace in a module import or a module declaration is of zero length.
XQST0089: It is a static error if a variable bound in a for or window clause of a FLWOR expression, and its associated positional variable, do not have distinct names (expanded QNames).
XQST0090: It is a static error if a character reference does not identify a valid character in the version of XML that is in use.
XQST0094: The name of each grouping variable must be equal (by the eq operator on expanded QNames) to the name of a variable in the input tuple stream.
XQST0097: It is a static error for a decimal-format to specify a value that is not valid for a given property, as described in statically known decimal formats
XQST0098: It is a static error if, for any named or unnamed decimal format, the properties representing characters used in a picture string do not each have distinct values. The following properties represent characters used in a picture string: decimal-separator, exponent-separator, grouping-separator, percent, per-mille, the family of ten decimal digits starting with zero-digit, digit, and pattern-separator.
XQST0099: A ContextItemDecl must not occur after an expression that relies on the initial context item, and no queryNo module may contain more than one ContextItemDecl.
XQST0103: All variables in a window clause must have distinct names.
XQST0104: A TypeName that is specified in a validate expression must be found in the in-scope schema definitions
XQST0106: It is a static error if a function declaration contains both a %private and a %public annotation. It is a static error if a function's annotations contain more than one annotation named %private or %public, more than one %private annotation, or more than one %public annotation. It is a static error if a function's annotations contain more than one annotation named %deterministic or %nondeterministic.
XQST0108: It is a static error if an output declaration occurs in a library module.
XQST0109: It is a static error if the local name of an output declaration in the http://www.w3.org/2010/xslt-xquery-serialization namespace is not one of the serialization parameter names listed in , or if the name of an output declaration is use-character-maps.
XQST0110: It is a static error if the same serialization parameter is used more than once in an output declaration.
XQST0111: It is a static error for a query prolog to contain two decimal formats with the same name, or to contain two default decimal formats.
XQST0113: Specifying a VarValue or VarDefaultValue for a context item declaration in a library module is a static error.
XQST0114: It is a static error for a decimal format declaration to define the same property more than once.
XQST0115: It is a static error if the document specified by the option "http://www.w3.org/2010/xslt-xquery-serialization":parameter-document raises a serialization error.
XQST0116: It is a static error if a variable declaration's annotations contain more than one annotation named %private or %public. if a variable declaration contains both a %private and a %public annotation, more than one %private annotation, or more than one %public annotation.
XQST0118: In a direct element constructor, the name used in the end tag must exactly match the name used in the corresponding start tag, including its prefix or absence of a prefix.
XQST0119: It is a static error if the implementation is not able to process the value of an output:parameter-document declaration to produce an XDM instance.
XQST0125: It is a static error if an inline function expression is annotated as %public or %private.
XQST0129: An implementation that does not provide the Higher-Order Function Feature MUST raise a static error if it encounters a FunctionTest, dynamic function call, named function reference, inline function expression, or partial function application.
XPST0134: The namespace axis is not supported.
XPTY0004: It is a type error if, during the static analysis phase, an expression is found to have a static type that is not appropriate for the context in which the expression occurs, or during the dynamic evaluation phase, the dynamic type of a value does not match a required type as specified by the matching rules in .
XPTY0018: It is a type error if the result of a path operator contains both nodes and non-nodes.
XPTY0019: It is a type error if E1 in a path expression E1/E2 does not evaluate to a sequence of nodes.
XPTY0020: It is a type error if, in an axis step, the context item is not a node.
XQTY0024: It is a type error if the content sequence in an element constructor contains an attribute node following a node that is not an attribute node.
XQTY0030: It is a type error if the argument of a validate expression does not evaluate to exactly one document or element node.
XQTY0086: It is a type error if the typed value of a copied element or attribute node is namespace-sensitive when construction mode is preserve and copy-namespaces mode is no-preserve.
XQTY0105: It is a type error if the content sequence in an element constructor contains a function item.
XPTY0117: When applying the function conversion rules, if an item is of type xs:untypedAtomic and the expected type is namespace-sensitive, a type error is raised.

## Serialization

SENR0001: It is an error if an item in S6 in sequence normalization is an attribute node or a namespace node.
SERE0003: It is an error if the serializer is unable to satisfy the rules for either a well-formed XML document entity or a well-formed XML external general parsed entity, or both, except for content modified by the character expansion phase of serialization.
SEPM0004: It is an error to specify the doctype-system parameter, or to specify the standalone parameter with a value other than omit, if the instance of the data model contains text nodes or multiple element nodes as children of the root node.
SERE0005: It is an error if the serialized result would contain an NCName that contains a character that is not permitted by the version of Namespaces in XML specified by the version parameter.
SERE0006: It is an error if the serialized result would contain a character that is not permitted by the version of XML specified by the version parameter.
SESU0007: It is an error if an output encoding other than UTF-8 or UTF-16 is requested and the serializer does not support that encoding.
SERE0008: It is an error if a character that cannot be represented in the encoding that the serializer is using for output appears in a context where character references are not allowed (for example if the character occurs in the name of an element).
SEPM0009: It is an error if the omit-xml-declaration parameter has the value yes, true or 1, and the standalone attribute has a value other than omit; or the version parameter has a value other than 1.0 and the doctype-system parameter is specified.
SEPM0010: It is an error if the output method is xml or xhtml, the value of the undeclare-prefixes parameter is one of, yes, true or 1, and the value of the version parameter is 1.0.
SESU0011: It is an error if the value of the normalization-form parameter specifies a normalization form that is not supported by the serializer.
SERE0012: It is an error if the value of the normalization-form parameter is fully-normalized and any relevant construct of the result begins with a combining character.
SESU0013: It is an error if the serializer does not support the version of XML or HTML specified by the version parameter.
SERE0014: It is an error to use the HTML output method if characters which are permitted in XML but not in HTML appear in the instance of the data model.
SERE0015: It is an error to use the HTML output method when > appears within a processing instruction in the data model instance being serialized.
SEPM0016: It is an error if a parameter value is invalid for the defined domain.
SEPM0017: It is an error if evaluating an expression in order to extract the setting of a serialization parameter from a data model instance would yield an error.
SEPM0018: It is an error if evaluating an expression in order to extract the setting of the use-character-maps serialization parameter from a data model instance would yield a sequence of length greater than one.
SEPM0019: It is an error if an instance of the data model used to specify the settings of serialization parameters specifies the value of the same parameter more than once.
SERE0020: It is an error if a numeric value being serialized using the JSON output method cannot be represented in the JSON grammar (e.g. +INF, -INF, NaN).
SERE0021: It is an error if a sequence being serialized using the JSON output method includes items for which no rules are provided in the appropriate section of the serialization rules.
SERE0022: It is an error if a map being serialized using the JSON output method has two keys with the same string value, unless the allow-duplicate-names has the value yes, true or 1.
SERE0023: It is an error if a sequence being serialized using the JSON output method is of length greater than one.
