# Fluid Parameter Type Annotations

Fluid supports optional type annotations on function parameters to surface static analysis diagnostics during parsing.
Annotations follow the parameter name after a colon and constrain the expected argument type.

## Syntax

```lua
function process(Path:str, Count:num, Options:table)
   -- Path must be a string
   -- Count must be a number
   -- Options must be a table
end
```

Untyped parameters omit the annotation:

```lua
function mixed(Untyped, Typed:bool)
   return Untyped, Typed
end
```

## Supported Type Names

| Shorthand | Full Name   | Notes                         |
|-----------|-------------|-------------------------------|
| `num`     | `number`    | Numeric values                |
| `str`     | `string`    | Text strings                  |
| `bool`    | `boolean`   | Boolean values                |
| `table`   | `table`     | Tables and dictionaries       |
| `func`    | `function`  | Callable values               |
| `nil`     | `nil`       | Explicit nil                  |
| `any`     | `any`       | Accepts any type              |
| `thread`  | `thread`    | Coroutines                    |
| `cdata`   | `cdata`     | FFI data (if FFI is enabled)  |
| `obj`     | `object`    | Parasol objects (userdata)    |

Unknown type names raise diagnostics during parsing with the `UnknownTypeName` error code.

## Static Analysis Behaviour

* The parser records annotated parameters and passes the information to the type analyser.
* The analyser checks call sites and emits warnings by default; setting `type_errors_are_fatal` in the parser configuration
  escalates them to errors.
* Diagnostics preserve the parser error codes so call-site mismatches can be categorised alongside parse-time failures.

## Testing Notes

The `src/fluid/tests/test_type_annotations.fluid` suite compiles sample functions using every supported type name,
exercises annotated calls, and asserts that unknown type names are rejected during parsing.
