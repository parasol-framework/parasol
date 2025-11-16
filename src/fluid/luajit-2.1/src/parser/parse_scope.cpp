
static void bcreg_reserve(FuncState* fs, BCReg n);
static void fscope_uvmark(FuncState* fs, BCReg level);
static void execute_defers(FuncState* fs, BCReg limit);

// Jump types for break and continue statements.

enum { JUMP_BREAK, JUMP_CONTINUE };

//********************************************************************************************************************
// Check if a string is the blank identifier '_'.

static int is_blank_identifier(GCstr *name)
{
   return (name != nullptr and name->len == 1 and *(strdata(name)) == '_');
}

//********************************************************************************************************************
// Define a new local variable.

static void var_new(LexState* ls, BCReg n, GCstr *name)
{
   FuncState* fs = ls->fs;
   MSize vtop = ls->vtop;
   checklimit(fs, fs->nactvar + n, LJ_MAX_LOCVAR, "local variables");
   if (vtop >= ls->sizevstack) [[unlikely]] {
      if (ls->sizevstack >= LJ_MAX_VSTACK)
         lj_lex_error(ls, 0, LJ_ERR_XLIMC, LJ_MAX_VSTACK);
      lj_mem_growvec(ls->L, ls->vstack, ls->sizevstack, LJ_MAX_VSTACK, VarInfo);
   }
   lj_assertFS(name == NAME_BLANK or uintptr_t(name) < VARNAME__MAX or lj_tab_getstr(fs->kt, name) != nullptr, "unanchored variable name");
   // NOBARRIER: name is anchored in fs->kt and ls->vstack is not a GCobj.
   setgcref(ls->vstack[vtop].name, obj2gco(name));
   fs->varmap[fs->nactvar + n] = uint16_t(vtop);
   ls->vtop = vtop + 1;
}

//********************************************************************************************************************

static inline void var_new_lit(LexState* ls, BCReg n, const char* v, size_t len) {
   var_new(ls, n, lj_parse_keepstr(ls, v, len));
}

static inline void var_new_fixed(LexState* ls, BCReg n, uintptr_t vn) {
   var_new(ls, n, (GCstr*)vn);
}

//********************************************************************************************************************
// Add local variables.

static void var_add(LexState* ls, BCReg nvars)
{
   FuncState* fs = ls->fs;
   BCReg nactvar = fs->nactvar;
   while (nvars--) {
      VarInfo* v = &var_get(ls, fs, nactvar);
      v->startpc = fs->pc;
      v->slot = nactvar++;
      v->info = 0;
   }
   fs->nactvar = nactvar;
}

//********************************************************************************************************************
// Remove local variables.

static void var_remove(LexState* ls, BCReg tolevel)
{
   FuncState* fs = ls->fs;
   while (fs->nactvar > tolevel)
      var_get(ls, fs, --fs->nactvar).endpc = fs->pc;
}

//********************************************************************************************************************
// Lookup local variable name.

static std::optional<BCReg> var_lookup_local(FuncState* fs, GCstr* n)
{
   int i;
   for (i = fs->nactvar - 1; i >= 0; i--) {
      GCstr* varname = strref(var_get(fs->ls, fs, i).name);
      if (varname == NAME_BLANK) [[unlikely]]
         continue;  // Skip blank identifiers.
      if (n == varname) [[likely]]
         return BCReg(i);
   }
   return std::nullopt;  // Not found.
}

//********************************************************************************************************************
// Lookup or add upvalue index.

static MSize var_lookup_uv(FuncState* fs, MSize vidx, ExpDesc* e)
{
   MSize n = fs->nuv;
   // Check if upvalue already exists using range-based iteration.
   auto uvmap_view = std::span(fs->uvmap.data(), n);
   for (MSize i = 0; auto uv_idx : uvmap_view) {
      if (uv_idx == vidx)
         return i;  // Already exists.
      i++;
   }
   // Otherwise create a new one.
   checklimit(fs, fs->nuv, LJ_MAX_UPVAL, "upvalues");
   lj_assertFS(e->k == ExpKind::Local or e->k == ExpKind::Upval, "bad expr type %d", e->k);
   fs->uvmap[n] = uint16_t(vidx);
   fs->uvtmp[n] = uint16_t(e->k == ExpKind::Local ? vidx : LJ_MAX_VSTACK + e->u.s.info);
   fs->nuv = n + 1;
   return n;
}

//********************************************************************************************************************
// Recursively lookup variables in enclosing functions.

static MSize var_lookup_(FuncState* fs, GCstr* name, ExpDesc* e, int first)
{
   if (fs) {
      auto reg = var_lookup_local(fs, name);
      if (reg.has_value()) {  // Local in this function?
         expr_init(e, ExpKind::Local, reg.value());
         if (!first)
            fscope_uvmark(fs, reg.value());  // Scope now has an upvalue.
         return MSize(e->u.s.aux = uint32_t(fs->varmap[reg.value()]));
      }
      else {
         MSize vidx = var_lookup_(fs->prev, name, e, 0);  // Var in outer func?
         if (int32_t(vidx) >= 0) {  // Yes, make it an upvalue here.
            e->u.s.info = uint8_t(var_lookup_uv(fs, vidx, e));
            e->k = ExpKind::Upval;
            return vidx;
         }
      }
   }
   else {  // Not found in any function, must be a global.
      expr_init(e, ExpKind::Global, 0);
      e->u.sval = name;
   }
   return MSize(-1);  // Global.
}

// Lookup variable name.
static inline MSize var_lookup(LexState* ls, ExpDesc* e) {
   return var_lookup_(ls->fs, lex_str(ls), e, 1);
}

//********************************************************************************************************************
// Jump and target handling

// Add a new jump or target

static MSize gola_new(LexState* ls, int jump_type, uint8_t info, BCPos pc)
{
   FuncState* fs = ls->fs;
   MSize vtop = ls->vtop;
   if (vtop >= ls->sizevstack) [[unlikely]] {
      if (ls->sizevstack >= LJ_MAX_VSTACK) lj_lex_error(ls, 0, LJ_ERR_XLIMC, LJ_MAX_VSTACK);
      lj_mem_growvec(ls->L, ls->vstack, ls->sizevstack, LJ_MAX_VSTACK, VarInfo);
   }
   GCstr* name = (jump_type == JUMP_BREAK) ? NAME_BREAK : NAME_CONTINUE;
   // NOBARRIER: name is anchored in fs->kt and ls->vstack is not a GCobj.
   setgcref(ls->vstack[vtop].name, obj2gco(name));
   ls->vstack[vtop].startpc = pc;
   ls->vstack[vtop].slot = uint8_t(fs->nactvar);
   ls->vstack[vtop].info = info;
   ls->vtop = vtop + 1;
   return vtop;
}

//********************************************************************************************************************
// Constexpr helper functions for goto/label flag checking.

[[nodiscard]] static constexpr bool gola_is_jump(const VarInfo* v) {
   return v->info & VSTACK_JUMP;
}

[[nodiscard]] static constexpr bool gola_is_jump_target(const VarInfo* v) {
   return v->info & VSTACK_JUMP_TARGET;
}

[[nodiscard]] static constexpr bool gola_is_jump_or_target(const VarInfo* v) {
   return v->info & (VSTACK_JUMP | VSTACK_JUMP_TARGET);
}

//********************************************************************************************************************
// Patch goto to jump to target.

static void gola_patch(LexState* ls, VarInfo* vg, VarInfo* vl)
{
   FuncState* fs = ls->fs;
   BCPos pc = vg->startpc;
   setgcrefnull(vg->name);  // Invalidate pending goto.
   setbc_a(&fs->bcbase[pc].ins, vl->slot);
   jmp_patch(fs, pc, vl->startpc);
}

//********************************************************************************************************************
// Patch goto to close upvalues.

static void gola_close(LexState* ls, VarInfo* vg)
{
   FuncState* fs = ls->fs;
   BCPos pc = vg->startpc;
   BCIns* ip = &fs->bcbase[pc].ins;
   lj_assertFS(gola_is_jump(vg), "expected goto");
   lj_assertFS(bc_op(*ip) == BC_JMP or bc_op(*ip) == BC_UCLO, "bad bytecode op %d", bc_op(*ip));
   setbc_a(ip, vg->slot);
   if (bc_op(*ip) == BC_JMP) {
      BCPos next = jmp_next(fs, pc);
      if (next != NO_JMP) jmp_patch(fs, next, pc);  // Jump to UCLO.
      setbc_op(ip, BC_UCLO);  // Turn into UCLO.
      setbc_j(ip, NO_JMP);
   }
}

//********************************************************************************************************************
// Resolve pending forward jumps (break/continue) for target.

static void gola_resolve(LexState *ls, FuncScope *bl, MSize idx)
{
   VarInfo *vg = ls->vstack + bl->vstart;
   VarInfo *vl = ls->vstack + idx;
   for (; vg < vl; vg++) {
      if (gcrefeq(vg->name, vl->name) and gola_is_jump(vg)) {
         gola_patch(ls, vg, vl);
      }
   }
}

//********************************************************************************************************************
// Fixup remaining gotos and targets for scope.

static void gola_fixup(LexState* ls, FuncScope* bl)
{
   VarInfo* v = ls->vstack + bl->vstart;
   VarInfo* ve = ls->vstack + ls->vtop;
   for (; v < ve; v++) {
      GCstr* name = strref(v->name);
      if (name != nullptr) {  // Only consider remaining valid gotos/targets.
         if (gola_is_jump_target(v)) {
            VarInfo* vg;
            setgcrefnull(v->name);  // Invalidate target that goes out of scope.
            for (vg = v + 1; vg < ve; vg++)  // Resolve pending backward gotos.
               if (strref(vg->name) == name and gola_is_jump(vg)) {
                  if ((bl->flags & FSCOPE_UPVAL) and vg->slot > v->slot)
                     gola_close(ls, vg);
                  gola_patch(ls, vg, v);
               }
         }
         else if (gola_is_jump(v)) {
            if (bl->prev) {  // Propagate break/continue to outer scope.
               if (name == NAME_BREAK) bl->prev->flags |= FSCOPE_BREAK;
               else if (name == NAME_CONTINUE) bl->prev->flags |= FSCOPE_CONTINUE;
               v->slot = bl->nactvar;
               if ((bl->flags & FSCOPE_UPVAL)) gola_close(ls, v);
            }
            else {  // No outer scope: no loop for break/continue.
               ls->linenumber = ls->fs->bcbase[v->startpc].line;
               if (name == NAME_BREAK) lj_lex_error(ls, 0, LJ_ERR_XBREAK);
               else if (name == NAME_CONTINUE) lj_lex_error(ls, 0, LJ_ERR_XCONTINUE);
            }
         }
      }
   }
}

//********************************************************************************************************************
// Scope handling

// Begin a scope.

static void fscope_begin(FuncState* fs, FuncScope* bl, int flags)
{
   bl->nactvar = uint8_t(fs->nactvar);
   bl->flags = flags;
   bl->vstart = fs->ls->vtop;
   bl->prev = fs->bl;
   fs->bl = bl;
   lj_assertFS(fs->freereg == fs->nactvar, "bad regalloc");
}

//********************************************************************************************************************

static void fscope_loop_continue(FuncState* fs, BCPos pos)
{
   FuncScope* bl = fs->bl;
   LexState* ls = fs->ls;

   lj_assertFS((bl->flags & FSCOPE_LOOP), "continue outside loop scope");

   if (!(bl->flags & FSCOPE_CONTINUE)) return;

   bl->flags &= uint8_t(~FSCOPE_CONTINUE);

   MSize idx = gola_new(ls, JUMP_CONTINUE, VSTACK_JUMP_TARGET, pos);
   ls->vtop = idx;
   gola_resolve(ls, bl, idx);
}

static void execute_defers(FuncState* fs, BCReg limit)
{
   LexState* ls = fs->ls;
   BCReg i = fs->nactvar;
   BCReg oldfreereg;
   BCReg argc = 0;
   BCReg argslots[LJ_MAX_SLOTS];

   if (fs->freereg < fs->nactvar) fs->freereg = fs->nactvar;
   oldfreereg = fs->freereg;

   while (i > limit) {
      VarInfo* v = &var_get(ls, fs, --i);
      if (v->info & VSTACK_DEFERARG) {
         lj_assertFS(argc < LJ_MAX_SLOTS, "too many defer args");
         argslots[argc++] = v->slot;
         continue;
      }

      if (v->info & VSTACK_DEFER) {
         BCReg callbase = fs->freereg;
         BCReg j;
         bcreg_reserve(fs, argc + 1 + LJ_FR2);
         bcemit_AD(fs, BC_MOV, callbase, v->slot);
         for (j = 0; j < argc; j++) {
            BCReg src = argslots[argc - 1 - j];
            bcemit_AD(fs, BC_MOV, callbase + LJ_FR2 + j + 1, src);
         }
         bcemit_ABC(fs, BC_CALL, callbase, 1, argc + 1);
         argc = 0;
         continue;
      }
      lj_assertFS(argc == 0, "dangling defer arguments");
   }

   lj_assertFS(argc == 0, "dangling defer arguments");
   fs->freereg = oldfreereg;
}

//********************************************************************************************************************
// End a scope.

static void fscope_end(FuncState* fs)
{
   FuncScope* bl = fs->bl;
   LexState* ls = fs->ls;
   fs->bl = bl->prev;
   execute_defers(fs, bl->nactvar);
   var_remove(ls, bl->nactvar);
   fs->freereg = fs->nactvar;
   lj_assertFS(bl->nactvar == fs->nactvar, "bad regalloc");
   if ((bl->flags & (FSCOPE_UPVAL | FSCOPE_NOCLOSE)) == FSCOPE_UPVAL) bcemit_AJ(fs, BC_UCLO, bl->nactvar, 0);
   if ((bl->flags & FSCOPE_BREAK)) {
      if ((bl->flags & FSCOPE_LOOP)) {
         MSize idx = gola_new(ls, JUMP_BREAK, VSTACK_JUMP_TARGET, fs->pc);
         ls->vtop = idx;  // Drop break target immediately.
         gola_resolve(ls, bl, idx);
      }
      else {  // Need the fixup step to propagate the breaks.
         gola_fixup(ls, bl);
         return;
      }
   }
   if ((bl->flags & FSCOPE_CONTINUE)) {
      gola_fixup(ls, bl);
   }
}

//********************************************************************************************************************
// Mark scope as having an upvalue.

static void fscope_uvmark(FuncState* fs, BCReg level)
{
   FuncScope* bl;
   for (bl = fs->bl; bl and bl->nactvar > level; bl = bl->prev);
   if (bl) bl->flags |= FSCOPE_UPVAL;
}

//********************************************************************************************************************
// Fixup bytecode for prototype.

static void fs_fixup_bc(FuncState* fs, GCproto* pt, BCIns* bc, MSize n)
{
   BCInsLine* base = fs->bcbase;
   MSize i;
   pt->sizebc = n;
   bc[0] = BCINS_AD((fs->flags & PROTO_VARARG) ? BC_FUNCV : BC_FUNCF, fs->framesize, 0);
   for (i = 1; i < n; i++) bc[i] = base[i].ins;
}

//********************************************************************************************************************
// Fixup upvalues for child prototype, step #2.

static void fs_fixup_uv2(FuncState* fs, GCproto* pt)
{
   VarInfo* vstack = fs->ls->vstack;
   uint16_t* uv = proto_uv(pt);
   MSize i, n = pt->sizeuv;
   for (i = 0; i < n; i++) {
      VarIndex vidx = uv[i];
      if (vidx >= LJ_MAX_VSTACK) uv[i] = vidx - LJ_MAX_VSTACK;
      else if ((vstack[vidx].info & VSTACK_VAR_RW)) uv[i] = vstack[vidx].slot | PROTO_UV_LOCAL;
      else uv[i] = vstack[vidx].slot | PROTO_UV_LOCAL | PROTO_UV_IMMUTABLE;
   }
}

//********************************************************************************************************************
// Fixup constants for prototype.

static void fs_fixup_k(FuncState* fs, GCproto* pt, void* kptr)
{
   GCtab* kt;
   TValue* array;
   Node* node;
   MSize i, hmask;
   checklimitgt(fs, fs->nkn, BCMAX_D + 1, "constants");
   checklimitgt(fs, fs->nkgc, BCMAX_D + 1, "constants");
   setmref(pt->k, kptr);
   pt->sizekn = fs->nkn;
   pt->sizekgc = fs->nkgc;
   kt = fs->kt;
   array = tvref(kt->array);
   for (i = 0; i < kt->asize; i++) {
      if (tvhaskslot(&array[i])) {
         TValue* tv = &((TValue*)kptr)[tvkslot(&array[i])];
         if (LJ_DUALNUM) setintV(tv, int32_t(i));
         else setnumV(tv, lua_Number(i));
      }
   }

   node = noderef(kt->node);
   hmask = kt->hmask;

   for (i = 0; i <= hmask; i++) {
      Node* n = &node[i];
      if (tvhaskslot(&n->val)) {
         ptrdiff_t kidx = ptrdiff_t(tvkslot(&n->val));
         lj_assertFS(!tvisint(&n->key), "unexpected integer key");
         if (tvisnum(&n->key)) {
            TValue* tv = &((TValue*)kptr)[kidx];
            if (LJ_DUALNUM) {
               lua_Number nn = numV(&n->key);
               int32_t k = lj_num2int(nn);
               lj_assertFS(!tvismzero(&n->key), "unexpected -0 key");
               if (lua_Number(k) == nn) setintV(tv, k);
               else *tv = n->key;
            }
            else *tv = n->key;
         }
         else {
            GCobj* o = gcV(&n->key);
            setgcref(((GCRef*)kptr)[~kidx], o);
            lj_gc_objbarrier(fs->L, pt, o);
            if (tvisproto(&n->key)) fs_fixup_uv2(fs, gco2pt(o));
         }
      }
   }
}

//********************************************************************************************************************
// Fixup upvalues for prototype, step #1.

static void fs_fixup_uv1(FuncState* fs, GCproto* pt, uint16_t* uv)
{
   setmref(pt->uv, uv);
   pt->sizeuv = fs->nuv;
   memcpy(uv, fs->uvtmp.data(), fs->nuv * sizeof(VarIndex));
}

//********************************************************************************************************************

#ifndef LUAJIT_DISABLE_DEBUGINFO
// Prepare lineinfo for prototype.
static size_t fs_prep_line(FuncState* fs, BCLine numline)
{
   return (fs->pc - 1) << (numline < 256 ? 0 : numline < 65536 ? 1 : 2);
}

// Fixup lineinfo for prototype.
static void fs_fixup_line(FuncState* fs, GCproto* pt,
   void* lineinfo, BCLine numline)
{
   BCInsLine* base = fs->bcbase + 1;
   BCLine first = fs->linedefined;
   MSize i = 0, n = fs->pc - 1;
   pt->firstline = fs->linedefined;
   pt->numline = numline;
   setmref(pt->lineinfo, lineinfo);

   if (LJ_LIKELY(numline < 256)) {
      uint8_t* li = (uint8_t*)lineinfo;
      do {
         BCLine delta = base[i].line - first;
         lj_assertFS(delta >= 0 and delta < 256, "bad line delta");
         li[i] = uint8_t(delta);
      } while (++i < n);
   }
   else if (LJ_LIKELY(numline < 65536)) {
      uint16_t* li = (uint16_t*)lineinfo;
      do {
         BCLine delta = base[i].line - first;
         lj_assertFS(delta >= 0 and delta < 65536, "bad line delta");
         li[i] = uint16_t(delta);
      } while (++i < n);
   }
   else {
      uint32_t* li = (uint32_t*)lineinfo;
      do {
         BCLine delta = base[i].line - first;
         lj_assertFS(delta >= 0, "bad line delta");
         li[i] = uint32_t(delta);
      } while (++i < n);
   }
}

//********************************************************************************************************************
// Prepare variable info for prototype.

static size_t fs_prep_var(LexState* ls, FuncState* fs, size_t* ofsvar)
{
   VarInfo* vs = ls->vstack, * ve;
   BCPos lastpc;
   lj_buf_reset(&ls->sb);  // Copy to temp. string buffer.
   // Store upvalue names using range-based iteration.
   auto uvmap_range = std::span(fs->uvmap.data(), fs->nuv);
   for (auto uv_idx : uvmap_range) {
      GCstr* s = strref(vs[uv_idx].name);
      MSize len = s->len + 1;
      char* p = lj_buf_more(&ls->sb, len);
      p = lj_buf_wmem(p, strdata(s), len);
      ls->sb.w = p;
   }
   *ofsvar = sbuflen(&ls->sb);
   lastpc = 0;

   // Store local variable names and compressed ranges.

   for (ve = vs + ls->vtop, vs += fs->vbase; vs < ve; vs++) {
      if (!gola_is_jump_or_target(vs)) {
         GCstr* s = strref(vs->name);
         BCPos startpc;
         char* p;
         if (uintptr_t(s) < VARNAME__MAX) {
            p = lj_buf_more(&ls->sb, 1 + 2 * 5);
            *p++ = char(uintptr_t(s));
         }
         else {
            MSize len = s->len + 1;
            p = lj_buf_more(&ls->sb, len + 2 * 5);
            p = lj_buf_wmem(p, strdata(s), len);
         }
         startpc = vs->startpc;
         p = lj_strfmt_wuleb128(p, startpc - lastpc);
         p = lj_strfmt_wuleb128(p, vs->endpc - startpc);
         ls->sb.w = p;
         lastpc = startpc;
      }
   }
   lj_buf_putb(&ls->sb, '\0');  // Terminator for varinfo.
   return sbuflen(&ls->sb);
}

//********************************************************************************************************************
// Fixup variable info for prototype.

static void fs_fixup_var(LexState* ls, GCproto* pt, uint8_t* p, size_t ofsvar)
{
   setmref(pt->uvinfo, p);
   setmref(pt->varinfo, (char*)p + ofsvar);
   memcpy(p, ls->sb.b, sbuflen(&ls->sb));  // Copy from temp. buffer.
}
#else

// Initialize with empty debug info, if disabled.
#define fs_prep_line(fs, numline)		(UNUSED(numline), 0)
#define fs_fixup_line(fs, pt, li, numline) \
  pt->firstline = pt->numline = 0, setmref((pt)->lineinfo, nullptr)
#define fs_prep_var(ls, fs, ofsvar)		(UNUSED(ofsvar), 0)
#define fs_fixup_var(ls, pt, p, ofsvar) \
  setmref((pt)->uvinfo, nullptr), setmref((pt)->varinfo, nullptr)

#endif

//********************************************************************************************************************
// Check if bytecode op returns.

static int bcopisret(BCOp op)
{
   switch (op) {
   case BC_CALLMT: case BC_CALLT:
   case BC_RETM: case BC_RET: case BC_RET0: case BC_RET1:
      return 1;
   default:
      return 0;
   }
}

//********************************************************************************************************************
// Fixup return instruction for prototype.

static void fs_fixup_ret(FuncState* fs)
{
   BCPos lastpc = fs->pc;
   if (lastpc <= fs->lasttarget or !bcopisret(bc_op(fs->bcbase[lastpc - 1].ins))) {
      execute_defers(fs, 0);
      if ((fs->bl->flags & FSCOPE_UPVAL)) bcemit_AJ(fs, BC_UCLO, 0, 0);
      bcemit_AD(fs, BC_RET0, 0, 1);  // Need final return.
   }

   fs->bl->flags |= FSCOPE_NOCLOSE;  // Handled above.
   fscope_end(fs);
   lj_assertFS(fs->bl == nullptr, "bad scope nesting");

   // May need to fixup returns encoded before first function was created.

   if (fs->flags & PROTO_FIXUP_RETURN) {
      BCPos pc;
      for (pc = 1; pc < lastpc; pc++) {
         BCIns ins = fs->bcbase[pc].ins;
         BCPos offset;
         switch (bc_op(ins)) {
         case BC_CALLMT: case BC_CALLT:
         case BC_RETM: case BC_RET: case BC_RET0: case BC_RET1:
            offset = bcemit_INS(fs, ins);  // Copy original instruction.
            fs->bcbase[offset].line = fs->bcbase[pc].line;
            offset = offset - (pc + 1) + BCBIAS_J;
            if (offset > BCMAX_D) err_syntax(fs->ls, LJ_ERR_XFIXUP);
            // Replace with UCLO plus branch.
            fs->bcbase[pc].ins = BCINS_AD(BC_UCLO, 0, offset);
            break;
         case BC_UCLO:
            return;  // We're done.
         default:
            break;
         }
      }
   }
}

//********************************************************************************************************************
// Finish a FuncState and return the new prototype.

static GCproto* fs_finish(LexState* ls, BCLine line)
{
   lua_State* L = ls->L;
   FuncState* fs = ls->fs;
   BCLine numline = line - fs->linedefined;
   size_t sizept, ofsk, ofsuv, ofsli, ofsdbg, ofsvar;
   GCproto* pt;

   // Apply final fixups.
   fs_fixup_ret(fs);

   // Calculate total size of prototype including all colocated arrays.
   sizept = sizeof(GCproto) + fs->pc * sizeof(BCIns) + fs->nkgc * sizeof(GCRef);
   sizept = (sizept + sizeof(TValue) - 1) & ~(sizeof(TValue) - 1);
   ofsk = sizept; sizept += fs->nkn * sizeof(TValue);
   ofsuv = sizept; sizept += ((fs->nuv + 1) & ~1) * 2;
   ofsli = sizept; sizept += fs_prep_line(fs, numline);
   ofsdbg = sizept; sizept += fs_prep_var(ls, fs, &ofsvar);

   // Allocate prototype and initialize its fields.
   pt = (GCproto*)lj_mem_newgco(L, MSize(sizept));
   pt->gct = ~LJ_TPROTO;
   pt->sizept = MSize(sizept);
   pt->trace = 0;
   pt->flags = uint8_t(fs->flags & ~(PROTO_HAS_RETURN | PROTO_FIXUP_RETURN));
   pt->numparams = fs->numparams;
   pt->framesize = fs->framesize;
   setgcref(pt->chunkname, obj2gco(ls->chunkname));

   // Close potentially uninitialized gap between bc and kgc.
   *(uint32_t*)((char*)pt + ofsk - sizeof(GCRef) * (fs->nkgc + 1)) = 0;
   fs_fixup_bc(fs, pt, (BCIns*)((char*)pt + sizeof(GCproto)), fs->pc);
   fs_fixup_k(fs, pt, (void*)((char*)pt + ofsk));
   fs_fixup_uv1(fs, pt, (uint16_t*)((char*)pt + ofsuv));
   fs_fixup_line(fs, pt, (void*)((char*)pt + ofsli), numline);
   fs_fixup_var(ls, pt, (uint8_t*)((char*)pt + ofsdbg), ofsvar);

   lj_vmevent_send(L, BC,
      setprotoV(L, L->top++, pt);
   );

   L->top--;  // Pop table of constants.
   ls->vtop = fs->vbase;  // Reset variable stack.
   ls->fs = fs->prev;
   lj_assertL(ls->fs != nullptr or ls->tok == TK_eof, "bad parser state");
   return pt;
}

//********************************************************************************************************************
// Initialize a new FuncState.

static void fs_init(LexState* ls, FuncState* fs)
{
   lua_State *L = ls->L;
   fs->prev = ls->fs; ls->fs = fs;  // Append to list.
   fs->ls = ls;
   fs->vbase = ls->vtop;
   fs->L = L;
   fs->pc = 0;
   fs->lasttarget = 0;
   fs->jpc = NO_JMP;
   fs->freereg = 0;
   fs->nkgc = 0;
   fs->nkn = 0;
   fs->nactvar = 0;
   fs->nuv = 0;
   fs->bl = nullptr;
   fs->flags = 0;
   fs->framesize = 1;  // Minimum frame size.
   fs->kt = lj_tab_new(L, 0, 0);
   // Anchor table of constants in stack to avoid being collected.
   settabV(L, L->top, fs->kt);
   incr_top(L);
}
