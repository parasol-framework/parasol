# Short Variable Declaration (`:=`) Implementation Plan

## Overview

This document provides a detailed step-by-step implementation plan for the short variable declaration operator `:=` in Fluid/LuaJIT. The `:=` operator allows declaring and assigning local variables in one statement, similar to Go's short variable declaration.

**Status:** 📋 **Implementation Plan** - Not yet started

**Priority:** ⭐⭐⭐⭐⭐ **Highest**

**Estimated Effort:** 8-12 hours

## Feature Specification

### Syntax

```lua
x := 10                    -- local x = 10
name := getUserName()      -- local name = getUserName()
x, y := getCoords()        -- local x, y = getCoords()
```

### Semantics

- **Local declaration**: Variables declared with `:=` are automatically local to the current scope
- **No redeclaration**: Cannot redeclare variables that already exist as locals in the same scope
- **Simple names only**: Only works with simple variable names, not table fields or indices
- **Multiple assignment**: Supports multiple variables: `x, y := f()`
- **Scope-aware**: Variables are local to the block/function where declared
- **Type inference**: Variable type inferred from assigned value (Lua's dynamic typing)

### Examples

```lua
-- Basic usage
x := 10
name := "hello"
result := calculateValue()

-- Multiple assignment
x, y := getCoords()
name, err := openFile("data.txt")

-- In conditionals
if x := getUserInput(); x > 0 then
   print("Positive:", x)
end

-- In loops
for i := 1, 10 do
   print(i)
end

-- Error: cannot redeclare existing local
local x = 5
x := 10  -- Error: x already declared as local

-- Error: cannot use with table fields
t.x := 10  -- Error: := only works with simple names
```

## Implementation Steps

### Step 1: Add Token Definition for `:=`

**File:** `src/fluid/luajit-2.1/src/lj_lex.h`

**Action:**
1. Add `colon_eq` token to `TKDEF` macro (around line 20-25):
   ```c
   #define TKDEF(_, __) \
     _(and) _(break) _(continue) _(do) _(else) _(elseif) _(end) _(false) \
     _(for) _(function) _(goto) _(if) _(in) _(is) _(local) _(nil) _(not) _(or) \
     _(repeat) _(return) _(then) _(true) _(until) _(while) \
     __(or_question, or?) \
     __(concat, ..) __(dots, ...) __(eq, ==) __(ge, >=) __(le, <=) __(ne, ~=) \
     __(shl, <<) __(shr, >>) \
     __(label, ::) __(number, <number>) __(name, <name>) __(string, <string>) \
     __(cadd, +=) __(csub, -=) __(cmul, *=) __(cdiv, /=) __(cconcat, ..=) __(cmod, %=) \
     __(colon_eq, :=) \
     __(plusplus, ++) \
     __(eof, <eof>)
   ```

**Expected Result:**
- `TK_colon_eq` token enum value created
- Token name string available for error messages

---

### Step 2: Update Lexer for `:=` Recognition

**File:** `src/fluid/luajit-2.1/src/lj_lex.c`

**Action:**
1. Modify the `:` case in `lex_scan()` to check for `:=`:
   ```c
   case ':':
     lex_next(ls);
     if (ls->c == ':') {  /* Check for :: (label) */
       lex_next(ls);
       return TK_label;
     }
     if (ls->c == '=') {  /* Check for := (short declaration) */
       lex_next(ls);
       return TK_colon_eq;
     }
     return ':';
   ```

**Location:** Find the `:` case in `lex_scan()` function (likely around line 400-450).

**Expected Result:**
- Lexer recognizes `:=` as a single token
- `TK_colon_eq` returned when `:=` is encountered

---

### Step 3: Parse Short Variable Declaration

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Create `parse_short_decl()` function:
   ```c
   /* Parse short variable declaration: name := value */
   static void parse_short_decl(LexState *ls)
   {
     FuncState *fs = ls->fs;
     LHSVarList vl;
     ExpDesc e;
     BCReg nexps, nvars = 0;
     BCLine line = ls->linenumber;

     /* Parse first variable name */
     if (ls->tok != TK_name && (LJ_52 || ls->tok != TK_goto)) {
        err_token(ls, TK_name);
     }

     /* Collect LHS variable names */
     do {
        GCstr *name = lex_str(ls);
        BCReg reg = var_lookup_local(fs, name);

        /* Check if variable already exists as local */
        if ((int32_t)reg >= 0) {
           /* Variable already declared as local - error */
           lj_lex_error(ls, ls->tok, LJ_ERR_XREDECL, strdata(name));
        }

        /* Create new local variable */
        var_new(ls, nvars++, name);

        /* Check for comma (multiple variables) */
     } while (lex_opt(ls, ','));

     /* Require := token */
     lex_check(ls, TK_colon_eq);

     /* Parse RHS */
     nexps = expr_list(ls, &e);

     /* Adjust LHS/RHS counts */
     assign_adjust(ls, nvars, nexps, &e);

     /* Add variables to scope */
     var_add(ls, nvars);

     /* Assign values to variables */
     if (nvars == 1) {
        /* Single assignment */
        ExpDesc v;
        expr_init(&v, VLOCAL, fs->nactvar - 1);
        v.u.s.aux = fs->varmap[fs->nactvar - 1];
        bcemit_store(fs, &v, &e);
     } else {
        /* Multiple assignment - assign in reverse order */
        BCReg i;
        for (i = nvars; i > 0; i--) {
           ExpDesc v;
           expr_init(&v, VLOCAL, fs->nactvar - i);
           v.u.s.aux = fs->varmap[fs->nactvar - i];
           if (i == nvars) {
              /* Last variable gets the expression */
              bcemit_store(fs, &v, &e);
           } else {
              /* Previous variables get previous register */
              expr_init(&e, VNONRELOC, fs->freereg - (nvars - i));
              bcemit_store(fs, &v, &e);
           }
        }
     }
   }
   ```

**Key Considerations:**
- Check that variable names don't already exist as locals
- Only works with simple names (handled by requiring `TK_name` token)
- Handle multiple assignment correctly
- Use existing `assign_adjust()` for LHS/RHS count matching

**Expected Result:**
- Short declarations parse correctly
- Error on redeclaration
- Multiple assignment works

---

### Step 4: Integrate into Statement Parser

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify `parse_call_assign()` to detect `:=`:
   ```c
   /* Parse call statement or assignment. */
   static void parse_call_assign(LexState *ls)
   {
     FuncState *fs = ls->fs;
     LHSVarList vl;
     expr_primary(ls, &vl.v);
     if (vl.v.k == VCALL) {  /* Function call statement. */
        setbc_b(bcptr(fs, &vl.v), 1);  /* No results. */
     }
     else if (ls->tok == TK_colon_eq) {  /* Short variable declaration. */
        /* Check that LHS is a simple name (not table field, etc.) */
        if (vl.v.k != VGLOBAL) {
           /* Variable already exists (local or upvalue) - not allowed */
           /* But we need to check if it's a global first */
           /* Actually, if it's VGLOBAL, it's fine - we'll create a local */
           /* If it's VLOCAL or VUPVAL, we have a problem */
           if (vl.v.k == VLOCAL || vl.v.k == VUPVAL) {
              err_syntax(ls, LJ_ERR_XREDECL);
           }
        }
        /* Parse short declaration */
        parse_short_decl(ls);
     }
     else if (ls->tok == TK_cadd || ls->tok == TK_csub || ls->tok == TK_cmul ||
                ls->tok == TK_cdiv || ls->tok == TK_cmod || ls->tok == TK_cconcat) {
        vl.prev = NULL;
        assign_compound(ls, &vl, ls->tok);
     }
     else if (ls->tok == ';') {
        /* Postfix increment (++) handled in expr_primary. */
     }
     else {  /* Start of an assignment. */
        vl.prev = NULL;
        parse_assignment(ls, &vl, 1);
     }
   }
   ```

**Challenge:** We've already parsed the first name with `expr_primary()`, but we need to handle it specially for `:=`. We may need to refactor this.

**Better Approach:** Detect `:=` before parsing the primary expression, or parse it differently.

**Revised Approach:**
```c
/* Parse call statement or assignment. */
static void parse_call_assign(LexState *ls)
{
   FuncState *fs = ls->fs;
   LHSVarList vl;

   /* Check if this looks like a short declaration (name followed by :=) */
   if (ls->tok == TK_name && (LJ_52 || ls->tok != TK_goto)) {
      LexToken lookahead = lj_lex_lookahead(ls);
      if (lookahead == TK_colon_eq || lookahead == ',') {
         /* Could be short declaration - parse it specially */
         parse_short_decl(ls);
         return;
      }
   }

   /* Otherwise, parse as normal call/assignment */
   expr_primary(ls, &vl.v);
   if (vl.v.k == VCALL) {  /* Function call statement. */
      setbc_b(bcptr(fs, &vl.v), 1);  /* No results. */
   }
   /* ... rest of existing code ... */
}
```

**Expected Result:**
- `:=` statements are recognized and parsed correctly
- Doesn't interfere with existing assignment/call parsing

---

### Step 5: Refactor Short Declaration Parsing

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Refine `parse_short_decl()` to handle the fact that we haven't parsed the name yet:
   ```c
   /* Parse short variable declaration: name := value */
   static void parse_short_decl(LexState *ls)
   {
     FuncState *fs = ls->fs;
     ExpDesc e;
     BCReg nexps, nvars = 0;
     BCLine line = ls->linenumber;

     /* Collect LHS variable names */
     do {
        GCstr *name;

        /* Parse variable name */
        if (ls->tok != TK_name && (LJ_52 || ls->tok != TK_goto)) {
           err_token(ls, TK_name);
        }
        name = lex_str(ls);

        /* Check if variable already exists as local */
        BCReg reg = var_lookup_local(fs, name);
        if ((int32_t)reg >= 0) {
           /* Variable already declared as local - error */
           lj_lex_error(ls, ls->tok, LJ_ERR_XREDECL, strdata(name));
        }

        /* Create new local variable */
        var_new(ls, nvars++, name);

        /* Check for comma (multiple variables) */
     } while (lex_opt(ls, ','));

     /* Require := token */
     lex_check(ls, TK_colon_eq);

     /* Parse RHS */
     nexps = expr_list(ls, &e);

     /* Adjust LHS/RHS counts */
     assign_adjust(ls, nvars, nexps, &e);

     /* Add variables to scope */
     var_add(ls, nvars);

     /* Now assign values to the newly created locals */
     /* Variables are at indices: nactvar - nvars, nactvar - nvars + 1, ... */
     BCReg base_reg = fs->nactvar - nvars;
     BCReg i;

     if (nvars == nexps) {
        /* Perfect match - direct assignment */
        if (e.k == VCALL) {
           if (bc_op(*bcptr(fs, &e)) == BC_VARG) {
              fs->freereg--;
              e.k = VRELOCABLE;
           } else {
              e.u.s.info = e.u.s.aux;
              e.k = VNONRELOC;
           }
        }

        /* Assign each variable */
        for (i = 0; i < nvars; i++) {
           ExpDesc v;
           expr_init(&v, VLOCAL, base_reg + i);
           v.u.s.aux = fs->varmap[base_reg + i];
           if (i == nvars - 1) {
              /* Last variable gets the expression */
              bcemit_store(fs, &v, &e);
           } else {
              /* Intermediate variables get from stack */
              expr_init(&e, VNONRELOC, fs->freereg - (nvars - i - 1));
              bcemit_store(fs, &v, &e);
           }
        }
     } else {
        /* Mismatch - use assign_adjust logic */
        /* This handles too many/few values */
        for (i = 0; i < nvars; i++) {
           ExpDesc v, val;
           expr_init(&v, VLOCAL, base_reg + i);
           v.u.s.aux = fs->varmap[base_reg + i];

           if (i < nexps) {
              if (i == nexps - 1 && e.k == VCALL) {
                 /* Last expression is a call - handle specially */
                 val = e;
              } else {
                 expr_init(&val, VNONRELOC, fs->freereg - (nexps - i - 1));
              }
           } else {
              /* Not enough values - set to nil */
              expr_init(&val, VKNIL, 0);
           }
           bcemit_store(fs, &v, &val);
        }
     }
   }
   ```

**Expected Result:**
- Short declarations handle single and multiple assignment
- Proper error handling for redeclaration
- Correct variable scope management

---

### Step 6: Handle Lookahead for Detection

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Use lookahead to detect `:=` before parsing primary expression:
   ```c
   /* Parse call statement or assignment. */
   static void parse_call_assign(LexState *ls)
   {
     FuncState *fs = ls->fs;
     LHSVarList vl;

     /* Check if this is a short declaration using lookahead */
     if ((ls->tok == TK_name || (!LJ_52 && ls->tok == TK_goto)) &&
         lj_lex_lookahead(ls) == TK_colon_eq) {
        /* This is a short declaration: name := value */
        parse_short_decl(ls);
        return;
     }

     /* Check for multiple names: name1, name2 := value */
     if ((ls->tok == TK_name || (!LJ_52 && ls->tok == TK_goto))) {
        LexToken lookahead = lj_lex_lookahead(ls);
        if (lookahead == ',') {
           /* Could be multiple names - peek ahead further */
           /* Store current position */
           const char *save_p = ls->p;
           LexChar save_c = ls->c;
           BCLine save_line = ls->linenumber;

           /* Skip name and comma */
           lj_lex_next(ls);  /* Skip first name */
           lj_lex_next(ls);  /* Skip comma */

           /* Check if next is name followed by := */
           if ((ls->tok == TK_name || (!LJ_52 && ls->tok == TK_goto)) &&
               lj_lex_lookahead(ls) == TK_colon_eq) {
              /* Restore position and parse as short declaration */
              ls->p = save_p;
              ls->c = save_c;
              ls->linenumber = save_line;
              parse_short_decl(ls);
              return;
           }

           /* Restore position */
           ls->p = save_p;
           ls->c = save_c;
           ls->linenumber = save_line;
        }
     }

     /* Otherwise, parse as normal call/assignment */
     expr_primary(ls, &vl.v);
     /* ... rest of existing code ... */
   }
   ```

**Note:** This approach uses lookahead, which is available via `lj_lex_lookahead()`. However, peeking ahead multiple tokens is more complex. A simpler approach might be to parse the first name, then check if next token is `:=`.

**Simpler Approach:**
```c
/* Parse call statement or assignment. */
static void parse_call_assign(LexState *ls)
{
   FuncState *fs = ls->fs;
   LHSVarList vl;

   /* Try parsing as short declaration first */
   if ((ls->tok == TK_name || (!LJ_52 && ls->tok == TK_goto))) {
      /* Store position in case we need to backtrack */
      const char *save_p = ls->p;
      LexChar save_c = ls->c;
      BCLine save_line = ls->linenumber;
      LexToken save_tok = ls->tok;
      TValue save_tokval = ls->tokval;

      /* Parse first name */
      GCstr *first_name = lex_str(ls);

      /* Check if next token is := or comma */
      if (ls->tok == TK_colon_eq) {
         /* Single name := value - parse as short declaration */
         /* But we've already consumed the name! */
         /* Need to pass it to parse_short_decl */
         parse_short_decl_with_first(ls, first_name);
         return;
      } else if (ls->tok == ',') {
         /* Multiple names - check if it's := after second name */
         lj_lex_next(ls);  /* Skip comma */
         if ((ls->tok == TK_name || (!LJ_52 && ls->tok == TK_goto))) {
            lex_str(ls);  /* Skip second name */
            if (ls->tok == TK_colon_eq) {
               /* Multiple names := value */
               /* Restore and parse properly */
               ls->p = save_p;
               ls->c = save_c;
               ls->linenumber = save_line;
               ls->tok = save_tok;
               ls->tokval = save_tokval;
               parse_short_decl(ls);
               return;
            }
         }
      }

      /* Not a short declaration - restore and parse normally */
      ls->p = save_p;
      ls->c = save_c;
      ls->linenumber = save_line;
      ls->tok = save_tok;
      ls->tokval = save_tokval;
   }

   /* Parse as normal call/assignment */
   expr_primary(ls, &vl.v);
   /* ... rest of existing code ... */
}
```

**Expected Result:**
- Short declarations are detected correctly
- Doesn't interfere with normal parsing

---

### Step 7: Simplify Detection Logic

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Use a simpler approach: parse the name, then check for `:=`:
   ```c
   /* Parse call statement or assignment. */
   static void parse_call_assign(LexState *ls)
   {
     FuncState *fs = ls->fs;
     LHSVarList vl;

     /* Parse primary expression (name, table field, etc.) */
     expr_primary(ls, &vl.v);

     if (vl.v.k == VCALL) {  /* Function call statement. */
        setbc_b(bcptr(fs, &vl.v), 1);  /* No results. */
     }
     else if (ls->tok == TK_colon_eq) {  /* Short variable declaration. */
        /* Can only use := with simple names (VGLOBAL means not yet declared) */
        if (vl.v.k != VGLOBAL) {
           /* Variable already exists - cannot redeclare */
           err_syntax(ls, LJ_ERR_XREDECL);
        }
        /* Parse short declaration starting from the name we just parsed */
        parse_short_decl_from_name(ls, &vl.v);
     }
     else if (ls->tok == TK_cadd || ls->tok == TK_csub || ls->tok == TK_cmul ||
                ls->tok == TK_cdiv || ls->tok == TK_cmod || ls->tok == TK_cconcat) {
        vl.prev = NULL;
        assign_compound(ls, &vl, ls->tok);
     }
     else if (ls->tok == ';') {
        /* Postfix increment (++) handled in expr_primary. */
     }
     else {  /* Start of an assignment. */
        vl.prev = NULL;
        parse_assignment(ls, &vl, 1);
     }
   }
   ```

**Better Approach:** Parse the name list first, then check for `:=`. Refactor `parse_short_decl()` to not consume the first name:

```c
/* Parse short variable declaration: name := value */
/* First name already parsed and stored in 'first_name' */
static void parse_short_decl(LexState *ls, GCstr *first_name)
{
   FuncState *fs = ls->fs;
   ExpDesc e;
   BCReg nexps, nvars = 0;

   /* Add first name */
   BCReg reg = var_lookup_local(fs, first_name);
   if ((int32_t)reg >= 0) {
      lj_lex_error(ls, ls->tok, LJ_ERR_XREDECL, strdata(first_name));
   }
   var_new(ls, nvars++, first_name);

   /* Collect remaining names */
   while (lex_opt(ls, ',')) {
      GCstr *name = lex_str(ls);
      reg = var_lookup_local(fs, name);
      if ((int32_t)reg >= 0) {
         lj_lex_error(ls, ls->tok, LJ_ERR_XREDECL, strdata(name));
      }
      var_new(ls, nvars++, name);
   }

   /* Require := token */
   lex_check(ls, TK_colon_eq);

   /* Parse RHS and assign (similar to parse_local) */
   nexps = expr_list(ls, &e);
   assign_adjust(ls, nvars, nexps, &e);
   var_add(ls, nvars);

   /* Assign values - reuse logic from parse_local */
   /* ... assignment code ... */
}
```

**Expected Result:**
- Cleaner integration with existing parser
- Proper handling of first name

---

### Step 8: Reuse Local Declaration Logic

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Refactor `parse_local()` to extract common logic, or reuse it directly:
   ```c
   /* Parse short variable declaration: name := value */
   static void parse_short_decl(LexState *ls, GCstr *first_name)
   {
     FuncState *fs = ls->fs;
     ExpDesc e;
     BCReg nexps, nvars = 0;

     /* Check first name doesn't exist */
     BCReg reg = var_lookup_local(fs, first_name);
     if ((int32_t)reg >= 0) {
        lj_lex_error(ls, ls->tok, LJ_ERR_XREDECL, strdata(first_name));
     }

     /* Create first variable */
     var_new(ls, nvars++, first_name);

     /* Collect remaining names */
     while (lex_opt(ls, ',')) {
        GCstr *name = lex_str(ls);
        reg = var_lookup_local(fs, name);
        if ((int32_t)reg >= 0) {
           lj_lex_error(ls, ls->tok, LJ_ERR_XREDECL, strdata(name));
        }
        var_new(ls, nvars++, name);
     }

     /* Require := token (already consumed in parse_call_assign) */
     /* Actually, we need to check it here */
     lex_check(ls, TK_colon_eq);

     /* Parse RHS - reuse logic from parse_local */
     nexps = expr_list(ls, &e);
     assign_adjust(ls, nvars, nexps, &e);
     var_add(ls, nvars);

     /* The assignment happens automatically via assign_adjust */
     /* But we need to store into the newly created locals */
     /* Similar to how parse_local does it */
   }
   ```

**Note:** `assign_adjust()` handles the assignment, but we need to ensure it targets the correct locals. Looking at `parse_local()`, it doesn't explicitly assign - `assign_adjust()` handles it. But we need to verify this works correctly.

**Expected Result:**
- Reuses existing assignment logic
- Consistent behavior with `local` declarations

---

### Step 9: Fix Assignment to New Locals

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Ensure values are assigned to the newly created locals. Review how `parse_local()` handles this and replicate:
   ```c
   /* Parse short variable declaration: name := value */
   static void parse_short_decl(LexState *ls, GCstr *first_name)
   {
     FuncState *fs = ls->fs;
     ExpDesc e;
     BCReg nexps, nvars = 0;

     /* Create variables (same as parse_local) */
     /* ... variable creation code ... */

     /* Parse RHS */
     nexps = expr_list(ls, &e);

     /* Adjust and assign - assign_adjust handles this */
     assign_adjust(ls, nvars, nexps, &e);

     /* Add variables to scope */
     var_add(ls, nvars);

     /* assign_adjust should have handled the assignment */
     /* But we may need to explicitly assign to the locals */
     /* Check how parse_local does it - it relies on assign_adjust */
     /* The variables are created but need values assigned */

     /* Actually, looking at parse_local more carefully: */
     /* After assign_adjust, the values are on the stack */
     /* The locals are created but not yet assigned */
     /* We need to explicitly store into them */

     /* Get base register for new locals */
     BCReg base = fs->nactvar - nvars;

     /* Assign values from stack to locals */
     BCReg i;
     for (i = 0; i < nvars; i++) {
        ExpDesc v;
        expr_init(&v, VLOCAL, base + i);
        v.u.s.aux = fs->varmap[base + i];

        if (i == nvars - 1 && nexps > 0) {
           /* Last variable gets the expression */
           bcemit_store(fs, &v, &e);
        } else if (i < nexps) {
           /* Get value from stack */
           ExpDesc val;
           expr_init(&val, VNONRELOC, fs->freereg - (nexps - i - 1));
           bcemit_store(fs, &v, &val);
        } else {
           /* Set to nil */
           ExpDesc nil;
           expr_init(&nil, VKNIL, 0);
           bcemit_store(fs, &v, &nil);
        }
     }
   }
   ```

**Expected Result:**
- Values correctly assigned to new locals
- Handles single and multiple assignment

---

### Step 10: Handle Multiple Names with Lookahead

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Update `parse_call_assign()` to handle multiple names:
   ```c
   /* Parse call statement or assignment. */
   static void parse_call_assign(LexState *ls)
   {
     FuncState *fs = ls->fs;
     LHSVarList vl;

     /* Parse primary expression */
     expr_primary(ls, &vl.v);

     if (vl.v.k == VCALL) {  /* Function call statement. */
        setbc_b(bcptr(fs, &vl.v), 1);  /* No results. */
     }
     else if (ls->tok == TK_colon_eq) {  /* Short variable declaration. */
        /* Check that LHS is a simple name (VGLOBAL) */
        if (vl.v.k != VGLOBAL) {
           err_syntax(ls, LJ_ERR_XREDECL);
        }
        /* Get the name */
        GCstr *first_name = vl.v.u.sval;
        parse_short_decl(ls, first_name);
     }
     else if (ls->tok == ',') {  /* Could be multiple names := value */
        /* Check if this is multiple names followed by := */
        LexToken lookahead = lj_lex_lookahead(ls);
        if (lookahead == TK_colon_eq) {
           /* Multiple names := value */
           /* But we've already parsed the first name as VGLOBAL */
           /* We need to handle this differently */
           /* Actually, if first is VGLOBAL and next is comma, then := */
           /* we can parse as short declaration */
           GCstr *first_name = vl.v.u.sval;
           parse_short_decl_multiple(ls, first_name);
        } else {
           /* Normal assignment with comma */
           vl.prev = NULL;
           parse_assignment(ls, &vl, 1);
        }
     }
     /* ... rest of existing code ... */
   }
   ```

**Expected Result:**
- Multiple names handled correctly
- `x, y := f()` works as expected

---

### Step 11: Create Test Suite

**File:** `src/fluid/tests/test_short_decl.fluid`

**Action:**
1. Create comprehensive test suite:
   ```lua
   -- Basic short declaration
   function testBasic()
      x := 10
      assert(x is 10, "Failed basic: expected 10")

      name := "hello"
      assert(name is "hello", "Failed string")
   end

   -- Multiple assignment
   function testMultiple()
      x, y := 10, 20
      assert(x is 10, "Failed x")
      assert(y is 20, "Failed y")

      a, b := getValues()
      -- Assuming getValues returns two values
   end

   -- Function call assignment
   function testFunctionCall()
      result := calculate()
      assert(type(result) is "number", "Failed function call")
   end

   -- Error: redeclaration
   function testRedeclaration()
      local x = 5
      -- This should error: x := 10
      -- Cannot redeclare existing local
   end

   -- Error: table field
   function testTableField()
      local t = {}
      -- This should error: t.x := 10
      -- := only works with simple names
   end

   -- Scope test
   function testScope()
      if true then
         x := 10
         assert(x is 10, "Failed scope")
      end
      -- x should not be accessible here (if scoped correctly)
   end

   -- Multiple assignment with function
   function testMultipleFunction()
      function getPair()
         return 1, 2
      end
      x, y := getPair()
      assert(x is 1, "Failed x")
      assert(y is 2, "Failed y")
   end

   return {
      tests = { 'testBasic', 'testMultiple', 'testFunctionCall',
                'testRedeclaration', 'testTableField', 'testScope',
                'testMultipleFunction' }
   }
   ```

---

### Step 12: Update Documentation

**File:** `docs/wiki/Fluid-Reference-Manual.md`

**Action:**
1. Add short variable declaration section:
   - Syntax and semantics
   - Examples
   - Comparison with `local` declarations
   - Restrictions (simple names only, no redeclaration)
   - Use cases

---

## Implementation Challenges

### Challenge 1: Detection Before Parsing

**Issue:** Need to detect `:=` before fully parsing the LHS expression.

**Solution:**
- Use lookahead: `lj_lex_lookahead()` to peek at next token
- Parse first name, then check for `:=`
- If `:=`, parse as short declaration; otherwise continue normal parsing

---

### Challenge 2: Multiple Names

**Issue:** `x, y := f()` requires parsing multiple names before `:=`.

**Solution:**
- Parse first name
- If next token is `,`, parse remaining names
- If next token is `:=`, parse as short declaration
- Otherwise, parse as normal assignment

---

### Challenge 3: Redeclaration Check

**Issue:** Must prevent redeclaring existing locals.

**Solution:**
- Use `var_lookup_local()` to check if name exists
- Error if variable already declared as local
- Allow shadowing globals (creating local with same name)

---

### Challenge 4: Assignment to New Locals

**Issue:** Need to assign values to newly created locals correctly.

**Solution:**
- Create locals first with `var_new()` and `var_add()`
- Then assign values using `bcemit_store()`
- Reuse logic from `parse_local()` where possible

---

## Testing Checklist

- [ ] Basic declaration: `x := 10`
- [ ] String assignment: `name := "hello"`
- [ ] Function call: `result := calculate()`
- [ ] Multiple assignment: `x, y := 10, 20`
- [ ] Multiple from function: `x, y := getPair()`
- [ ] Scope behavior (local to block)
- [ ] Error: redeclaration of existing local
- [ ] Error: using with table field `t.x := 10`
- [ ] Error: using with table index `t[1] := 10`
- [ ] Interaction with existing code
- [ ] Multiple names: `x, y, z := f()`
- [ ] Mismatched counts: `x, y := 10` (y gets nil)

---

## Key Files to Modify

1. **`src/fluid/luajit-2.1/src/lj_lex.h`**
   - Add `TK_colon_eq` token

2. **`src/fluid/luajit-2.1/src/lj_lex.c`**
   - Detect `:=` in lexer

3. **`src/fluid/luajit-2.1/src/lj_parse.c`**
   - Add `parse_short_decl()` function
   - Modify `parse_call_assign()` to detect `:=`
   - Handle multiple names

4. **`src/fluid/tests/test_short_decl.fluid`**
   - Create test suite

5. **`docs/wiki/Fluid-Reference-Manual.md`**
   - Add documentation

---

## Implementation Order

1. **Phase 1: Lexer** (1-2 hours)
   - Step 1-2: Add token, update lexer
   - Basic token recognition

2. **Phase 2: Parser** (4-6 hours)
   - Step 3-9: Parse short declarations
   - Handle single and multiple assignment
   - Error handling

3. **Phase 3: Testing** (2-3 hours)
   - Step 11: Test suite
   - Edge case testing

4. **Phase 4: Documentation** (1 hour)
   - Step 12: User documentation

---

## References

### Similar Implementations
- Go's short variable declaration: `x := 10`
- Rust's `let` binding: `let x = 10;`
- TypeScript's `const`/`let`: `const x = 10;`

### Related Code
- Local declarations: `parse_local()` (line 2831)
- Assignment parsing: `parse_assignment()` (line 2771)
- Variable lookup: `var_lookup_local()` (line 1336)
- Variable creation: `var_new()`, `var_add()` (lines 1288, 1314)

---

**Last Updated:** 2025-01-XX
**Status:** Implementation Plan - Ready for implementation
**Related:** Local variable declarations, assignment parsing
