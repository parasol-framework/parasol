// Copyright (C) 2025 Paul Manias

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

void LexState::var_new(BCReg n, GCstr* name)
{
   FuncState* fs = this->fs;
   MSize vtop = this->vtop;
   checklimit(fs, fs->nactvar + n, LJ_MAX_LOCVAR, "local variables");
   if (vtop >= this->sizevstack) [[unlikely]] {
      if (this->sizevstack >= LJ_MAX_VSTACK)
         lj_lex_error(this, 0, ErrMsg::XLIMC, LJ_MAX_VSTACK);
      lj_mem_growvec(this->L, this->vstack, this->sizevstack, LJ_MAX_VSTACK, VarInfo);
   }
   lj_assertFS(name == NAME_BLANK or uintptr_t(name) < VARNAME__MAX or lj_tab_getstr(fs->kt, name) != nullptr, "unanchored variable name");
   // NOBARRIER: name is anchored in fs->kt and ls->vstack is not a GCobj.
   setgcref(this->vstack[vtop].name, obj2gco(name));
   fs->varmap[fs->nactvar + n] = uint16_t(vtop);
   this->vtop = vtop + 1;
}

//********************************************************************************************************************

void LexState::var_new_lit(BCReg n, std::string_view value)
{
   this->var_new(n, this->keepstr(value));
}

void LexState::var_new_fixed(BCReg n, uintptr_t name)
{
   this->var_new(n, (GCstr*)name);
}

//********************************************************************************************************************
// Add local variables.

void LexState::var_add(BCReg nvars)
{
   FuncState* fs = this->fs;
   BCReg nactvar = fs->nactvar;
   while (nvars--) {
      VarInfo* v = &var_get(this, fs, nactvar);
      v->startpc = fs->pc;
      v->slot = nactvar++;
      v->info = VarInfoFlag::None;
   }
   fs->nactvar = nactvar;
}

//********************************************************************************************************************
// Remove local variables.

void LexState::var_remove(BCReg tolevel)
{
   FuncState* fs = this->fs;
   while (fs->nactvar > tolevel)
      var_get(this, fs, --fs->nactvar).endpc = fs->pc;
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
MSize LexState::var_lookup(ExpDesc* e)
{
   return var_lookup_(this->fs, this->lex_str(), e, 1);
}

MSize LexState::var_lookup_symbol(GCstr* name, ExpDesc* e)
{
   if (name == nullptr or name == NAME_BLANK) {
      expr_init(e, ExpKind::Global, 0);
      e->u.sval = name ? name : NAME_BLANK;
      return MSize(-1);
   }
   return var_lookup_(this->fs, name, e, 1);
}

//********************************************************************************************************************
// Jump and target handling

// Add a new jump or target

MSize LexState::gola_new(int jump_type, VarInfoFlag info, BCPos pc)
{
   FuncState* fs = this->fs;
   MSize vtop = this->vtop;
   if (vtop >= this->sizevstack) [[unlikely]] {
      if (this->sizevstack >= LJ_MAX_VSTACK) lj_lex_error(this, 0, ErrMsg::XLIMC, LJ_MAX_VSTACK);
      lj_mem_growvec(this->L, this->vstack, this->sizevstack, LJ_MAX_VSTACK, VarInfo);
   }
   GCstr* name = (jump_type == JUMP_BREAK) ? NAME_BREAK : NAME_CONTINUE;
   // NOBARRIER: name is anchored in fs->kt and ls->vstack is not a GCobj.
   setgcrefp(this->vstack[vtop].name, obj2gco(name));
   this->vstack[vtop].startpc = pc;
   this->vstack[vtop].slot = uint8_t(fs->nactvar);
   this->vstack[vtop].info = info;
   this->vtop = vtop + 1;
   return vtop;
}

//********************************************************************************************************************
// Constexpr helper functions for goto/label flag checking.

[[nodiscard]] static inline bool gola_is_jump(const VarInfo* v) {
   return has_flag(v->info, VarInfoFlag::Jump);
}

[[nodiscard]] static inline bool gola_is_jump_target(const VarInfo* v) {
   return has_flag(v->info, VarInfoFlag::JumpTarget);
}

[[nodiscard]] static inline bool gola_is_jump_or_target(const VarInfo* v) {
   return has_flag(v->info, VarInfoFlag::Jump) or has_flag(v->info, VarInfoFlag::JumpTarget);
}

//********************************************************************************************************************
// Patch goto to jump to target.

void LexState::gola_patch(VarInfo* vg, VarInfo* vl)
{
   FuncState* fs = this->fs;
   BCPos pc = vg->startpc;
   setgcrefnull(vg->name);  // Invalidate pending goto.
   setbc_a(&fs->bcbase[pc].ins, vl->slot);
   JumpListView(fs, pc).patch_to(vl->startpc);
}

//********************************************************************************************************************
// Patch goto to close upvalues.

void LexState::gola_close(VarInfo* vg)
{
   FuncState* fs = this->fs;
   BCPos pc = vg->startpc;
   BCIns* ip = &fs->bcbase[pc].ins;
   lj_assertFS(gola_is_jump(vg), "expected goto");
   lj_assertFS(bc_op(*ip) IS BC_JMP or bc_op(*ip) IS BC_UCLO, "bad bytecode op %d", bc_op(*ip));
   setbc_a(ip, vg->slot);
   if (bc_op(*ip) IS BC_JMP) {
      BCPos next = JumpListView::next(fs, pc);
      if (next != NO_JMP) JumpListView(fs, next).patch_to(pc);  // Jump to UCLO.
      setbc_op(ip, BC_UCLO);  // Turn into UCLO.
      setbc_j(ip, NO_JMP);
   }
}

//********************************************************************************************************************
// Resolve pending forward jumps (break/continue) for target.

void LexState::gola_resolve(FuncScope* bl, MSize idx)
{
   VarInfo* vg = this->vstack + bl->vstart;
   VarInfo* vl = this->vstack + idx;
   for (; vg < vl; vg++) {
      if (gcrefeq(vg->name, vl->name) and gola_is_jump(vg)) {
         this->gola_patch(vg, vl);
      }
   }
}

//********************************************************************************************************************
// Fixup remaining gotos and targets for scope.

void LexState::gola_fixup(FuncScope* bl)
{
   VarInfo* v = this->vstack + bl->vstart;
   VarInfo* ve = this->vstack + this->vtop;
   for (; v < ve; v++) {
      GCstr* name = strref(v->name);
      if (name != nullptr) {  // Only consider remaining valid gotos/targets.
         if (gola_is_jump_target(v)) {
            VarInfo* vg;
            setgcrefnull(v->name);  // Invalidate target that goes out of scope.
            for (vg = v + 1; vg < ve; vg++)  // Resolve pending backward gotos.
               if (strref(vg->name) == name and gola_is_jump(vg)) {
                  if (has_flag(bl->flags, FuncScopeFlag::Upvalue) and vg->slot > v->slot)
                     this->gola_close(vg);
                  this->gola_patch(vg, v);
               }
         }
         else if (gola_is_jump(v)) {
            if (bl->prev) {  // Propagate break/continue to outer scope.
               if (name == NAME_BREAK) bl->prev->flags |= FuncScopeFlag::Break;
               else if (name == NAME_CONTINUE) bl->prev->flags |= FuncScopeFlag::Continue;
               v->slot = bl->nactvar;
               if (has_flag(bl->flags, FuncScopeFlag::Upvalue)) this->gola_close(v);
            }
            else {  // No outer scope: no loop for break/continue.
               this->linenumber = this->fs->bcbase[v->startpc].line;
               if (name == NAME_BREAK) lj_lex_error(this, 0, ErrMsg::XBREAK);
               else if (name == NAME_CONTINUE) lj_lex_error(this, 0, ErrMsg::XCONTINUE);
            }
         }
      }
   }
}

//********************************************************************************************************************
// Scope handling

// Begin a scope.

static void fscope_begin(FuncState* fs, FuncScope* bl, FuncScopeFlag flags)
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

   lj_assertFS(has_flag(bl->flags, FuncScopeFlag::Loop), "continue outside loop scope");

   if (!has_flag(bl->flags, FuncScopeFlag::Continue)) return;

   bl->flags &= ~FuncScopeFlag::Continue;

   VStackGuard vstack_guard(ls);
   MSize idx = ls->gola_new(JUMP_CONTINUE, VarInfoFlag::JumpTarget, pos);
   ls->gola_resolve(bl, idx);
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
      if (has_flag(v->info, VarInfoFlag::DeferArg)) {
         lj_assertFS(argc < LJ_MAX_SLOTS, "too many defer args");
         argslots[argc++] = v->slot;
         continue;
      }

      if (has_flag(v->info, VarInfoFlag::Defer)) {
         BCReg callbase = fs->freereg;
         BCReg j;
         RegisterAllocator allocator(fs);
         allocator.reserve(argc + 1 + LJ_FR2);
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
   if (not fs) return;

   FuncScope* bl = fs->bl;
   LexState* ls = fs->ls;
   fs->bl = bl->prev;
   execute_defers(fs, bl->nactvar);
   ls->var_remove(bl->nactvar);
   fs->freereg = fs->nactvar;
   lj_assertFS(bl->nactvar == fs->nactvar, "bad regalloc");
   if ((bl->flags & (FuncScopeFlag::Upvalue | FuncScopeFlag::NoClose)) IS FuncScopeFlag::Upvalue) bcemit_AJ(fs, BC_UCLO, bl->nactvar, 0);
   if (has_flag(bl->flags, FuncScopeFlag::Break)) {
      if (has_flag(bl->flags, FuncScopeFlag::Loop)) {
         VStackGuard vstack_guard(ls);
         MSize idx = ls->gola_new(JUMP_BREAK, VarInfoFlag::JumpTarget, fs->pc);
         ls->gola_resolve(bl, idx);
      }
      else {  // Need the fixup step to propagate the breaks.
         ls->gola_fixup(bl);
         return;
      }
   }
   if (has_flag(bl->flags, FuncScopeFlag::Continue)) {
      ls->gola_fixup(bl);
   }
}

//********************************************************************************************************************
// Mark scope as having an upvalue.

static void fscope_uvmark(FuncState* fs, BCReg level)
{
   FuncScope* bl;
   for (bl = fs->bl; bl and bl->nactvar > level; bl = bl->prev);
   if (bl) bl->flags |= FuncScopeFlag::Upvalue;
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
      else if (has_flag(vstack[vidx].info, VarInfoFlag::VarReadWrite)) uv[i] = vstack[vidx].slot | PROTO_UV_LOCAL;
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

//********************************************************************************************************************
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

size_t LexState::fs_prep_var(FuncState* FunctionState, size_t* OffsetVar)
{
   FuncState* fs = FunctionState;
   VarInfo* vs = this->vstack, * ve;
   BCPos lastpc;
   lj_buf_reset(&this->sb);  // Copy to temp. string buffer.
   // Store upvalue names using range-based iteration.
   auto uvmap_range = std::span(fs->uvmap.data(), fs->nuv);
   for (auto uv_idx : uvmap_range) {
      GCstr* s = strref(vs[uv_idx].name);
      MSize len = s->len + 1;
      char* p = lj_buf_more(&this->sb, len);
      p = lj_buf_wmem(p, strdata(s), len);
      this->sb.w = p;
   }
   *OffsetVar = sbuflen(&this->sb);
   lastpc = 0;

   // Store local variable names and compressed ranges.

   for (ve = vs + this->vtop, vs += fs->vbase; vs < ve; vs++) {
      if (!gola_is_jump_or_target(vs)) {
         GCstr* s = strref(vs->name);
         BCPos startpc;
         char* p;
         if (uintptr_t(s) < VARNAME__MAX) {
            p = lj_buf_more(&this->sb, 1 + 2 * 5);
            *p++ = char(uintptr_t(s));
         }
         else {
            MSize len = s->len + 1;
            p = lj_buf_more(&this->sb, len + 2 * 5);
            p = lj_buf_wmem(p, strdata(s), len);
         }
         startpc = vs->startpc;
         p = lj_strfmt_wuleb128(p, startpc - lastpc);
         p = lj_strfmt_wuleb128(p, vs->endpc - startpc);
         this->sb.w = p;
         lastpc = startpc;
      }
   }
   lj_buf_putb(&this->sb, '\0');  // Terminator for varinfo.
   return sbuflen(&this->sb);
}

//********************************************************************************************************************
// Fixup variable info for prototype.

void LexState::fs_fixup_var(GCproto* Prototype, uint8_t* Buffer, size_t OffsetVar)
{
   setmref(Prototype->uvinfo, Buffer);
   setmref(Prototype->varinfo, (char*)Buffer + OffsetVar);
   memcpy(Buffer, this->sb.b, sbuflen(&this->sb));  // Copy from temp. buffer.
}
#else

// Initialize with empty debug info, if disabled.
#define fs_prep_line(fs, numline)		(UNUSED(numline), 0)
#define fs_fixup_line(fs, pt, li, numline) \
  pt->firstline = pt->numline = 0, setmref((pt)->lineinfo, nullptr)

size_t LexState::fs_prep_var(FuncState* FunctionState, size_t* OffsetVar)
{
   UNUSED(FunctionState);
   UNUSED(OffsetVar);
   return 0;
}

void LexState::fs_fixup_var(GCproto* Prototype, uint8_t* Buffer, size_t OffsetVar)
{
   UNUSED(Buffer);
   UNUSED(OffsetVar);
   setmref((Prototype)->uvinfo, nullptr);
   setmref((Prototype)->varinfo, nullptr);
}

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
      if (has_flag(fs->bl->flags, FuncScopeFlag::Upvalue)) bcemit_AJ(fs, BC_UCLO, 0, 0);
      bcemit_AD(fs, BC_RET0, 0, 1);  // Need final return.
   }

   fs->bl->flags |= FuncScopeFlag::NoClose;  // Handled above.
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
            if (offset > BCMAX_D) fs->ls->err_syntax(ErrMsg::XFIXUP);
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

GCproto* LexState::fs_finish(BCLine Line)
{
   lua_State* L = this->L;
   FuncState* fs = this->fs;
   BCLine numline = Line - fs->linedefined;
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
   ofsdbg = sizept; sizept += this->fs_prep_var(fs, &ofsvar);

   // Allocate prototype and initialize its fields.
   pt = (GCproto*)lj_mem_newgco(L, MSize(sizept));
   pt->gct = ~LJ_TPROTO;
   pt->sizept = MSize(sizept);
   pt->trace = 0;
   pt->flags = uint8_t(fs->flags & ~(PROTO_HAS_RETURN | PROTO_FIXUP_RETURN));
   pt->numparams = fs->numparams;
   pt->framesize = fs->framesize;
   setgcref(pt->chunkname, obj2gco(this->chunkname));

   // Close potentially uninitialized gap between bc and kgc.
   *(uint32_t*)((char*)pt + ofsk - sizeof(GCRef) * (fs->nkgc + 1)) = 0;
   fs_fixup_bc(fs, pt, (BCIns*)((char*)pt + sizeof(GCproto)), fs->pc);
   fs_fixup_k(fs, pt, (void*)((char*)pt + ofsk));
   fs_fixup_uv1(fs, pt, (uint16_t*)((char*)pt + ofsuv));
   fs_fixup_line(fs, pt, (void*)((char*)pt + ofsli), numline);
   this->fs_fixup_var(pt, (uint8_t*)((char*)pt + ofsdbg), ofsvar);

   lj_vmevent_send(L, BC,
      setprotoV(L, L->top++, pt);
   );

   L->top--;  // Pop table of constants.
   this->vtop = fs->vbase;  // Reset variable stack.
   this->fs = fs->prev;
   lj_assertL(this->fs != nullptr or this->tok == TK_eof, "bad parser state");
   return pt;
}

//********************************************************************************************************************
// Initialize a new FuncState.

void LexState::fs_init(FuncState* FunctionState)
{
   FuncState* fs = FunctionState;
   lua_State* L = this->L;
   fs->prev = this->fs; this->fs = fs;  // Append to list.
   fs->ls = this;
   fs->vbase = this->vtop;
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
