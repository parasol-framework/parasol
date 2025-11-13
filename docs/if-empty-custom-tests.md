# Custom Stress Tests for the if-empty Operator

These manual scenarios were drafted to reproduce the argument-leak bug across a variety of call shapes before promoting the most effective examples into Flute coverage.

## 1. Capturing Extra Arguments
```fluid
local Function = { status = 'Hello' }
local function capture(...)
   local args = { ... }
   return #args, args[1], args[2]
end

local count, first, second = capture(Function.status ? 'then')
assert(count is 2, "if-empty leaks a second slot into the call")
assert(first is 'Hello', "Primary value should survive")
assert(second is 'Hello', "Leaked slot unexpectedly duplicates the truthy operand")
```

## 2. Observing the Built-in Failure Message
```fluid
local Function = { status = 'Hello' }
local ok, err = pcall(function()
   return string.find((Function.status ? 'then'), 'e')
end)

assert(ok is false, "Bug currently triggers a runtime failure")
assert(string.find(tostring(err), "bad argument #3"),
   "Error text confirms the extraneous third argument")
```

## 3. Prefix Arguments
```fluid
local Function = { status = 'Hello' }
local function capture(prefix, ...)
   local args = { ... }
   return prefix, #args, args[1], args[2]
end

local prefix, count, first, second = capture('prefix', Function.status ? 'then')
assert(prefix is 'prefix', "Leading argument should remain intact")
assert(count is 2, "Vararg tail exposes the leaked slot")
```

Each snippet can be executed with `parasol scripts --run` or embedded into a temporary Flute test. Scenarios (1) and (2) were converted into permanent cases inside `test_if_empty.fluid` to document the failure until the parser defect is repaired.
