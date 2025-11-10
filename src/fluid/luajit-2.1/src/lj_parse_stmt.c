// -- Assignments ---------------------------------------------------------

// List of LHS variables.
typedef struct LHSVarList {
   ExpDesc v;			// LHS variable.
   struct LHSVarList* prev;	// Link to previous LHS variable.
} LHSVarList;

// Eliminate write-after-read hazards for local variable assignment.
static void assign_hazard(LexState* ls, LHSVarList* lh, const ExpDesc* v)
{
   FuncState* fs = ls->fs;
   BCReg reg = v->u.s.info;  // Check against this variable.
   BCReg tmp = fs->freereg;  // Rename to this temp. register (if needed).
   int hazard = 0;
   for (; lh; lh = lh->prev) {
      if (lh->v.k == VINDEXED) {
         if (lh->v.u.s.info == reg) {  // t[i], t = 1, 2
            hazard = 1;
            lh->v.u.s.info = tmp;
         }
         if (lh->v.u.s.aux == reg) {  // t[i], i = 1, 2
            hazard = 1;
            lh->v.u.s.aux = tmp;
         }
      }
   }
   if (hazard) {
      bcemit_AD(fs, BC_MOV, tmp, reg);  // Rename conflicting variable.
      bcreg_reserve(fs, 1);
   }
}

// Adjust LHS/RHS of an assignment.

static void assign_adjust(LexState* ls, BCReg nvars, BCReg nexps, ExpDesc* e)
{
   FuncState* fs = ls->fs;
   int32_t extra = (int32_t)nvars - (int32_t)nexps;
   if (e->k == VCALL) {
      extra++;  // Compensate for the VCALL itself.
      if (extra < 0) extra = 0;
      setbc_b(bcptr(fs, e), extra + 1);  // Fixup call results.
      if (extra > 1) bcreg_reserve(fs, (BCReg)extra - 1);
   }
   else {
      if (e->k != VVOID)
         expr_tonextreg(fs, e);  // Close last expression.
      if (extra > 0) {  // Leftover LHS are set to nil.
         BCReg reg = fs->freereg;
         bcreg_reserve(fs, (BCReg)extra);
         bcemit_nil(fs, reg, (BCReg)extra);
      }
   }
   if (nexps > nvars)
      ls->fs->freereg -= nexps - nvars;  // Drop leftover regs.
}

static int assign_compound(LexState* ls, LHSVarList* lh, LexToken opType)
{
   FuncState* fs = ls->fs;
   ExpDesc lhv, infix, rh;
   int32_t nexps;
   BinOpr op;
   BCReg freg_base;

   lhv = lh->v;

   checkcond(ls, vkisvar(lh->v.k), LJ_ERR_XLEFTCOMPOUND);

   switch (opType) {
   case TK_cadd: op = OPR_ADD; break;
   case TK_csub: op = OPR_SUB; break;
   case TK_cmul: op = OPR_MUL; break;
   case TK_cdiv: op = OPR_DIV; break;
   case TK_cmod: op = OPR_MOD; break;
   case TK_cconcat: op = OPR_CONCAT; break;
   default:
      lj_assertLS(0, "unknown compound operator");
      return 0;
   }
   lj_lex_next(ls);

   /* Preserve table base/index across RHS evaluation by duplicating them
   ** to the top of the stack && discharging using the duplicates. This retains
   ** the original registers for the final store && maintains LIFO free order. */
   freg_base = fs->freereg;
   if (lh->v.k == VINDEXED) {
      BCReg new_base, new_idx;
      uint32_t orig_aux = lhv.u.s.aux;  // Keep originals for the store.

      // Duplicate base to a fresh register.
      new_base = fs->freereg;
      bcemit_AD(fs, BC_MOV, new_base, lhv.u.s.info);
      bcreg_reserve(fs, 1);

      // If index is a register (0..BCMAX_C), duplicate it, too.
      if ((int32_t)orig_aux >= 0 && orig_aux <= BCMAX_C) {
         new_idx = fs->freereg;
         bcemit_AD(fs, BC_MOV, new_idx, (BCReg)orig_aux);
         bcreg_reserve(fs, 1);
         // Discharge using the duplicates; keep lhv pointing to originals.
         lh->v.u.s.info = new_base;
         lh->v.u.s.aux = new_idx;
      }
      else {
         // For string/byte keys, only the base needs duplicating.
         lh->v.u.s.info = new_base;
         // aux remains an encoded constant.
      }
   }

   // For concatenation, fix left operand placement before parsing RHS to
   // maintain BC_CAT stack adjacency && LIFO freeing semantics.

   if (op == OPR_CONCAT) {
      infix = lh->v;
      bcemit_binop_left(fs, op, &infix);
      nexps = expr_list(ls, &rh);
      checkcond(ls, nexps == 1, LJ_ERR_XRIGHTCOMPOUND);
   }
   else {
      // For bitwise ops, avoid pre-pushing LHS to keep call frame contiguous.
      if (!(op == OPR_BAND || op == OPR_BOR || op == OPR_BXOR || op == OPR_SHL || op == OPR_SHR))
         expr_tonextreg(fs, &lh->v);
      nexps = expr_list(ls, &rh);
      checkcond(ls, nexps == 1, LJ_ERR_XRIGHTCOMPOUND);
      infix = lh->v;
      bcemit_binop_left(fs, op, &infix);
   }
   bcemit_binop(fs, op, &infix, &rh);
   bcemit_store(fs, &lhv, &infix);
   // Drop any RHS temporaries && release original base/index in LIFO order.
   fs->freereg = freg_base;
   if (lhv.k == VINDEXED) {
      uint32_t orig_aux = lhv.u.s.aux;
      if ((int32_t)orig_aux >= 0 && orig_aux <= BCMAX_C)
         bcreg_free(fs, (BCReg)orig_aux);
      bcreg_free(fs, (BCReg)lhv.u.s.info);
   }
   return 1;
}

// Recursively parse assignment statement.
static void parse_assignment(LexState* ls, LHSVarList* lh, BCReg nvars)
{
   ExpDesc e;
   checkcond(ls, VLOCAL <= lh->v.k && lh->v.k <= VINDEXED, LJ_ERR_XSYNTAX);
   if (lex_opt(ls, ',')) {  // Collect LHS list && recurse upwards.
      LHSVarList vl;
      vl.prev = lh;
      expr_primary(ls, &vl.v);
      if (vl.v.k == VLOCAL)
         assign_hazard(ls, lh, &vl.v);
      checklimit(ls->fs, ls->level + nvars, LJ_MAX_XLEVEL, "variable names");
      parse_assignment(ls, &vl, nvars + 1);
   }
   else {  // Parse RHS.
      BCReg nexps;
      lex_check(ls, '=');
      nexps = expr_list(ls, &e);
      if (nexps == nvars) {
         if (e.k == VCALL) {
            if (bc_op(*bcptr(ls->fs, &e)) == BC_VARG) {  // Vararg assignment.
               ls->fs->freereg--;
               e.k = VRELOCABLE;
            }
            else {  // Multiple call results.
               e.u.s.info = e.u.s.aux;  // Base of call is not relocatable.
               e.k = VNONRELOC;
            }
         }
         bcemit_store(ls->fs, &lh->v, &e);
         return;
      }
      assign_adjust(ls, nvars, nexps, &e);
   }
   // Assign RHS to LHS && recurse downwards.
   expr_init(&e, VNONRELOC, ls->fs->freereg - 1);
   bcemit_store(ls->fs, &lh->v, &e);
}

// Parse call statement || assignment.
static void parse_call_assign(LexState* ls)
{
   FuncState* fs = ls->fs;
   LHSVarList vl;
   expr_primary(ls, &vl.v);
   if (vl.v.k == VNONRELOC && (vl.v.u.s.aux & POSTFIX_INC_STMT_FLAG))
      return;
   if (vl.v.k == VCALL) {  // Function call statement.
      setbc_b(bcptr(fs, &vl.v), 1);  // No results.
   }
   else if (ls->tok == TK_cadd || ls->tok == TK_csub || ls->tok == TK_cmul ||
      ls->tok == TK_cdiv || ls->tok == TK_cmod || ls->tok == TK_cconcat) {
      vl.prev = NULL;
      assign_compound(ls, &vl, ls->tok);
   }
   else if (ls->tok == ';') {
      // Postfix increment (++) handled in expr_primary.
   }
   else {  // Start of an assignment.
      vl.prev = NULL;
      parse_assignment(ls, &vl, 1);
   }
}

// Parse 'local' statement.
static void parse_local(LexState* ls)
{
   if (lex_opt(ls, TK_function)) {  // Local function declaration.
      ExpDesc v, b;
      FuncState* fs = ls->fs;
      var_new(ls, 0, lex_str(ls));
      expr_init(&v, VLOCAL, fs->freereg);
      v.u.s.aux = fs->varmap[fs->freereg];
      bcreg_reserve(fs, 1);
      var_add(ls, 1);
      parse_body(ls, &b, 0, ls->linenumber);
      // bcemit_store(fs, &v, &b) without setting VSTACK_VAR_RW.
      expr_free(fs, &b);
      expr_toreg(fs, &b, v.u.s.info);
      // The upvalue is in scope, but the local is only valid after the store.
      var_get(ls, fs, fs->nactvar - 1).startpc = fs->pc;
   }
   else {  // Local variable declaration.
      ExpDesc e;
      BCReg nexps, nvars = 0;
      do {  // Collect LHS.
         GCstr* name = lex_str(ls);
         // Use NAME_BLANK marker for blank identifiers.
         var_new(ls, nvars++, is_blank_identifier(name) ? NAME_BLANK : name);
      } while (lex_opt(ls, ','));
      if (lex_opt(ls, '=')) {  // Optional RHS.
         nexps = expr_list(ls, &e);
      }
      else {  // Or implicitly set to nil.
         e.k = VVOID;
         nexps = 0;
      }
      assign_adjust(ls, nvars, nexps, &e);
      var_add(ls, nvars);
   }
}

static void snapshot_return_regs(FuncState* fs, BCIns* ins)
{
   BCOp op = bc_op(*ins);

   if (op == BC_RET1) {
      BCReg src = bc_a(*ins);
      if (src < fs->nactvar) {
         BCReg dst = fs->freereg;
         bcreg_reserve(fs, 1);
         bcemit_AD(fs, BC_MOV, dst, src);
         setbc_a(ins, dst);
      }
   }
   else if (op == BC_RET) {
      BCReg base = bc_a(*ins);
      BCReg nres = bc_d(*ins);

      if (nres > 1) {
         BCReg count = nres - 1;
         BCReg dst = fs->freereg;
         BCReg i;

         bcreg_reserve(fs, count);
         for (i = 0; i < count; i++)
            bcemit_AD(fs, BC_MOV, dst + i, base + i);
         setbc_a(ins, dst);
      }
   }
}

static void parse_defer(LexState* ls)
{
   FuncState* fs = ls->fs;
   ExpDesc func, arg;
   BCLine line = ls->linenumber;
   BCReg reg = fs->freereg;
   BCReg nargs = 0;
   VarInfo* vi;

   lj_lex_next(ls);  // Skip 'defer'.
   var_new(ls, 0, NAME_BLANK);
   bcreg_reserve(fs, 1);
   var_add(ls, 1);
   vi = &var_get(ls, fs, fs->nactvar - 1);
   vi->info |= VSTACK_DEFER;

   parse_body_defer(ls, &func, line);
   expr_toreg(fs, &func, reg);

   if (ls->tok == '(') {
      BCLine argline = ls->linenumber;
      lj_lex_next(ls);
      if (ls->tok != ')') {
         do {
            expr(ls, &arg);
            expr_tonextreg(fs, &arg);
            nargs++;
         } while (lex_opt(ls, ','));
      }

      lex_match(ls, ')', '(', argline);

      if (nargs) {
         BCReg i;
         for (i = 0; i < nargs; i++) var_new(ls, i, NAME_BLANK);
         var_add(ls, nargs);
         for (i = 0; i < nargs; i++) {
            VarInfo* argi = &var_get(ls, fs, fs->nactvar - nargs + i);
            argi->info |= VSTACK_DEFERARG;
         }
      }
   }

   fs->freereg = fs->nactvar;
}

// Parse 'function' statement.
static void parse_func(LexState* ls, BCLine line)
{
   FuncState* fs;
   ExpDesc v, b;
   int needself = 0;
   lj_lex_next(ls);  // Skip 'function'.
   // Parse function name.
   var_lookup(ls, &v);
   while (ls->tok == '.')  // Multiple dot-separated fields.
      expr_field(ls, &v);
   if (ls->tok == ':') {  // Optional colon to signify method call.
      needself = 1;
      expr_field(ls, &v);
   }
   parse_body(ls, &b, needself, line);
   fs = ls->fs;
   bcemit_store(fs, &v, &b);
   fs->bcbase[fs->pc - 1].line = line;  // Set line for the store.
}

// -- Control transfer statements -----------------------------------------

// Check for end of block.
static int parse_isend(LexToken tok)
{
   switch (tok) {
   case TK_else: case TK_elseif: case TK_end: case TK_until: case TK_eof:
      return 1;
   default:
      return 0;
   }
}

// Parse 'return' statement.
static void parse_return(LexState* ls)
{
   BCIns ins;
   FuncState* fs = ls->fs;
   lj_lex_next(ls);  // Skip 'return'.
   fs->flags |= PROTO_HAS_RETURN;
   if (parse_isend(ls->tok) || ls->tok == ';') {  // Bare return.
      ins = BCINS_AD(BC_RET0, 0, 1);
   }
   else {  // Return with one || more values.
      ExpDesc e;  // Receives the _last_ expression in the list.
      BCReg nret = expr_list(ls, &e);
      if (nret == 1) {  // Return one result.
         if (e.k == VCALL) {  // Check for tail call.
            BCIns* ip = bcptr(fs, &e);
            // It doesn't pay off to add BC_VARGT just for 'return ...'.
            if (bc_op(*ip) == BC_VARG) goto notailcall;
            fs->pc--;
            ins = BCINS_AD(bc_op(*ip) - BC_CALL + BC_CALLT, bc_a(*ip), bc_c(*ip));
         }
         else {  // Can return the result from any register.
            ins = BCINS_AD(BC_RET1, expr_toanyreg(fs, &e), 2);
         }
      }
      else {
         if (e.k == VCALL) {  // Append all results from a call.
         notailcall:
            setbc_b(bcptr(fs, &e), 0);
            ins = BCINS_AD(BC_RETM, fs->nactvar, e.u.s.aux - fs->nactvar);
         }
         else {
            expr_tonextreg(fs, &e);  // Force contiguous registers.
            ins = BCINS_AD(BC_RET, fs->nactvar, nret + 1);
         }
      }
   }
   snapshot_return_regs(fs, &ins);
   execute_defers(fs, 0);
   if (fs->flags & PROTO_CHILD)
      bcemit_AJ(fs, BC_UCLO, 0, 0);  // May need to close upvalues first.
   bcemit_INS(fs, ins);
}

// Parse 'continue' statement.
static void parse_continue(LexState* ls)
{
   FuncState* fs = ls->fs;
   FuncScope* loop = fs->bl;

   while (loop && !(loop->flags & FSCOPE_LOOP))
      loop = loop->prev;
   lj_assertLS(loop != NULL, "continue outside loop");

   execute_defers(fs, loop->nactvar);
   fs->bl->flags |= FSCOPE_CONTINUE;
   gola_new(ls, JUMP_CONTINUE, VSTACK_JUMP, bcemit_jmp(fs));
}

// Parse 'break' statement.
static void parse_break(LexState* ls)
{
   FuncState* fs = ls->fs;
   FuncScope* loop = fs->bl;

   while (loop && !(loop->flags & FSCOPE_LOOP))
      loop = loop->prev;
   lj_assertLS(loop != NULL, "break outside loop");

   execute_defers(fs, loop->nactvar);
   fs->bl->flags |= FSCOPE_BREAK;
   gola_new(ls, JUMP_BREAK, VSTACK_JUMP, bcemit_jmp(fs));
}

// -- Blocks, loops && conditional statements ----------------------------

// Parse a block.
static void parse_block(LexState* ls)
{
   FuncState* fs = ls->fs;
   FuncScope bl;
   fscope_begin(fs, &bl, 0);
   parse_chunk(ls);
   fscope_end(fs);
}

// Parse 'while' statement.
static void parse_while(LexState* ls, BCLine line)
{
   FuncState* fs = ls->fs;
   BCPos start, loop, condexit;
   FuncScope bl;
   lj_lex_next(ls);  // Skip 'while'.
   start = fs->lasttarget = fs->pc;
   condexit = expr_cond(ls);
   fscope_begin(fs, &bl, FSCOPE_LOOP);
   lex_check(ls, TK_do);
   loop = bcemit_AD(fs, BC_LOOP, fs->nactvar, 0);
   parse_block(ls);
   jmp_patch(fs, bcemit_jmp(fs), start);
   lex_match(ls, TK_end, TK_while, line);
   fscope_loop_continue(fs, start);
   fscope_end(fs);
   jmp_tohere(fs, condexit);
   jmp_patchins(fs, loop, fs->pc);
}

// Parse 'repeat' statement.
static void parse_repeat(LexState* ls, BCLine line)
{
   FuncState* fs = ls->fs;
   BCPos loop = fs->lasttarget = fs->pc;
   BCPos condexit, iter;
   FuncScope bl1, bl2;
   fscope_begin(fs, &bl1, FSCOPE_LOOP);  // Breakable loop scope.
   fscope_begin(fs, &bl2, 0);  // Inner scope.
   lj_lex_next(ls);  // Skip 'repeat'.
   bcemit_AD(fs, BC_LOOP, fs->nactvar, 0);
   parse_chunk(ls);
   lex_match(ls, TK_until, TK_repeat, line);
   iter = fs->pc;
   condexit = expr_cond(ls);  // Parse condition (still inside inner scope).
   if (!(bl2.flags & FSCOPE_UPVAL)) {  // No upvalues? Just end inner scope.
      fscope_end(fs);
   }
   else {  // Otherwise generate: cond: UCLO+JMP out, !cond: UCLO+JMP loop.
      parse_break(ls);  // Break from loop && close upvalues.
      jmp_tohere(fs, condexit);
      fscope_end(fs);  // End inner scope && close upvalues.
      condexit = bcemit_jmp(fs);
   }
   jmp_patch(fs, condexit, loop);  // Jump backwards if !cond.
   jmp_patchins(fs, loop, fs->pc);
   fscope_loop_continue(fs, iter); // continue statements jump to condexit.
   fscope_end(fs);  // End loop scope.
}

// Parse numeric 'for'.
static void parse_for_num(LexState* ls, GCstr* varname, BCLine line)
{
   FuncState* fs = ls->fs;
   BCReg base = fs->freereg;
   FuncScope bl;
   BCPos loop, loopend;
   // Hidden control variables.
   var_new_fixed(ls, FORL_IDX, VARNAME_FOR_IDX);
   var_new_fixed(ls, FORL_STOP, VARNAME_FOR_STOP);
   var_new_fixed(ls, FORL_STEP, VARNAME_FOR_STEP);
   // Visible copy of index variable.
   var_new(ls, FORL_EXT, varname);
   lex_check(ls, '=');
   expr_next(ls);
   lex_check(ls, ',');
   expr_next(ls);
   if (lex_opt(ls, ',')) {
      expr_next(ls);
   }
   else {
      bcemit_AD(fs, BC_KSHORT, fs->freereg, 1);  // Default step is 1.
      bcreg_reserve(fs, 1);
   }
   var_add(ls, 3);  // Hidden control variables.
   lex_check(ls, TK_do);
   loop = bcemit_AJ(fs, BC_FORI, base, NO_JMP);
   fscope_begin(fs, &bl, 0);  // Scope for visible variables.
   var_add(ls, 1);
   bcreg_reserve(fs, 1);
   parse_block(ls);
   fscope_end(fs);
   // Perform loop inversion. Loop control instructions are at the end.
   loopend = bcemit_AJ(fs, BC_FORL, base, NO_JMP);
   fs->bcbase[loopend].line = line;  // Fix line for control ins.
   jmp_patchins(fs, loopend, loop + 1);
   jmp_patchins(fs, loop, fs->pc);
   fscope_loop_continue(fs, loopend); // continue statements jump to loopend.
}

/* Try to predict whether the iterator is next() && specialize the bytecode.
** Detecting next() && pairs() by name is simplistic, but quite effective.
** The interpreter backs off if the check for the closure fails at runtime.
*/
static int predict_next(LexState* ls, FuncState* fs, BCPos pc)
{
   BCIns ins = fs->bcbase[pc].ins;
   GCstr* name;
   cTValue* o;
   switch (bc_op(ins)) {
   case BC_MOV:
      name = gco2str(gcref(var_get(ls, fs, bc_d(ins)).name));
      break;
   case BC_UGET:
      name = gco2str(gcref(ls->vstack[fs->uvmap[bc_d(ins)]].name));
      break;
   case BC_GGET:
      // There's no inverse index (yet), so lookup the strings.
      o = lj_tab_getstr(fs->kt, lj_str_newlit(ls->L, "pairs"));
      if (o && tvhaskslot(o) && tvkslot(o) == bc_d(ins))
         return 1;
      o = lj_tab_getstr(fs->kt, lj_str_newlit(ls->L, "next"));
      if (o && tvhaskslot(o) && tvkslot(o) == bc_d(ins))
         return 1;
      return 0;
   default:
      return 0;
   }
   return (name->len == 5 && !strcmp(strdata(name), "pairs")) ||
      (name->len == 4 && !strcmp(strdata(name), "next"));
}

// Parse 'for' iterator.
static void parse_for_iter(LexState* ls, GCstr* indexname)
{
   FuncState* fs = ls->fs;
   ExpDesc e;
   BCReg nvars = 0;
   BCLine line;
   BCReg base = fs->freereg + 3;
   BCPos loop, loopend, iter, exprpc = fs->pc;
   FuncScope bl;
   int isnext;
   // Hidden control variables.
   var_new_fixed(ls, nvars++, VARNAME_FOR_GEN);
   var_new_fixed(ls, nvars++, VARNAME_FOR_STATE);
   var_new_fixed(ls, nvars++, VARNAME_FOR_CTL);
   // Visible variables returned from iterator.
   var_new(ls, nvars++, is_blank_identifier(indexname) ? NAME_BLANK : indexname);
   while (lex_opt(ls, ',')) {
      GCstr* name = lex_str(ls);
      var_new(ls, nvars++, is_blank_identifier(name) ? NAME_BLANK : name);
   }
   lex_check(ls, TK_in);
   line = ls->linenumber;
   assign_adjust(ls, 3, expr_list(ls, &e), &e);
   // The iterator needs another 3 [4] slots (func [pc] | state ctl).
   bcreg_bump(fs, 3 + LJ_FR2);
   isnext = (nvars <= 5 && predict_next(ls, fs, exprpc));
   var_add(ls, 3);  // Hidden control variables.
   lex_check(ls, TK_do);
   loop = bcemit_AJ(fs, isnext ? BC_ISNEXT : BC_JMP, base, NO_JMP);
   fscope_begin(fs, &bl, 0);  // Scope for visible variables.
   var_add(ls, nvars - 3);
   bcreg_reserve(fs, nvars - 3);
   parse_block(ls);
   fscope_end(fs);
   // Perform loop inversion. Loop control instructions are at the end.
   jmp_patchins(fs, loop, fs->pc);
   iter = bcemit_ABC(fs, isnext ? BC_ITERN : BC_ITERC, base, nvars - 3 + 1, 2 + 1);
   loopend = bcemit_AJ(fs, BC_ITERL, base, NO_JMP);
   fs->bcbase[loopend - 1].line = line;  // Fix line for control ins.
   fs->bcbase[loopend].line = line;
   jmp_patchins(fs, loopend, loop + 1);
   fscope_loop_continue(fs, iter); // continue statements jump to iter.
}

// Parse 'for' statement.
static void parse_for(LexState* ls, BCLine line)
{
   FuncState* fs = ls->fs;
   GCstr* varname;
   FuncScope bl;
   fscope_begin(fs, &bl, FSCOPE_LOOP);
   lj_lex_next(ls);  // Skip 'for'.
   varname = lex_str(ls);  // Get first variable name.
   if (ls->tok == '=')
      parse_for_num(ls, varname, line);
   else if (ls->tok == ',' || ls->tok == TK_in)
      parse_for_iter(ls, varname);
   else
      err_syntax(ls, LJ_ERR_XFOR);
   lex_match(ls, TK_end, TK_for, line);
   fscope_end(fs);  // Resolve break list.
}

// Parse condition && 'then' block.
static BCPos parse_then(LexState* ls)
{
   BCPos condexit;
   lj_lex_next(ls);  // Skip 'if' || 'elseif'.
   condexit = expr_cond(ls);
   lex_check(ls, TK_then);
   parse_block(ls);
   return condexit;
}

// Parse 'if' statement.
static void parse_if(LexState* ls, BCLine line)
{
   FuncState* fs = ls->fs;
   BCPos flist;
   BCPos escapelist = NO_JMP;
   flist = parse_then(ls);
   while (ls->tok == TK_elseif) {  // Parse multiple 'elseif' blocks.
      jmp_append(fs, &escapelist, bcemit_jmp(fs));
      jmp_tohere(fs, flist);
      flist = parse_then(ls);
   }
   if (ls->tok == TK_else) {  // Parse optional 'else' block.
      jmp_append(fs, &escapelist, bcemit_jmp(fs));
      jmp_tohere(fs, flist);
      lj_lex_next(ls);  // Skip 'else'.
      parse_block(ls);
   }
   else {
      jmp_append(fs, &escapelist, flist);
   }
   jmp_tohere(fs, escapelist);
   lex_match(ls, TK_end, TK_if, line);
}
