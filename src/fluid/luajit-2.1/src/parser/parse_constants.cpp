// Copyright (C) 2025 Paul Manias

// Add a number constant.

[[nodiscard]] static BCReg const_num(FuncState* fs, ExpDesc* e)
{
   lua_State* L = fs->L;
   TValue* o;
   lj_assertFS(expr_isnumk(e), "bad usage");
   o = lj_tab_set(L, fs->kt, &e->u.nval);
   if (tvhaskslot(o)) return tvkslot(o);
   o->u64 = fs->nkn;
   return fs->nkn++;
}

// Add a GC object constant.

[[nodiscard]] static BCReg const_gc(FuncState* fs, GCobj* gc, uint32_t itype)
{
   lua_State* L = fs->L;
   TValue key, * o;
   setgcV(L, &key, gc, itype);
   // NOBARRIER: the key is new or kept alive.
   o = lj_tab_set(L, fs->kt, &key);
   if (tvhaskslot(o)) return tvkslot(o);
   o->u64 = fs->nkgc;
   return fs->nkgc++;
}

// Add a string constant.

[[nodiscard]] static BCReg const_str(FuncState* fs, ExpDesc* e)
{
   lj_assertFS(expr_isstrk(e) or e->k IS ExpKind::Global, "bad usage");
   return const_gc(fs, obj2gco(e->u.sval), LJ_TSTR);
}

// Anchor string constant to avoid GC.

GCstr* LexState::keepstr(std::string_view str)
{
   // NOBARRIER: the key is new or kept alive.
   lua_State* L = this->L;
   GCstr* s = lj_str_new(L, str.data(), str.size());
   TValue* tv = lj_tab_setstr(L, this->fs->kt, s);
   if (tvisnil(tv)) setboolV(tv, 1);
   lj_gc_check(L);
   return s;
}

GCstr* LexState::intern_empty_string()
{
   if (!this->empty_string_constant)
      this->empty_string_constant = this->keepstr(std::string_view());
   return this->empty_string_constant;
}

#if LJ_HASFFI
// Anchor cdata to avoid GC.
void LexState::keepcdata(TValue* tv, GCcdata* cd)
{
   // NOBARRIER: the key is new or kept alive.
   lua_State* L = this->L;
   setcdataV(L, tv, cd);
   setboolV(lj_tab_set(L, this->fs->kt, tv), 1);
}
#endif

extern GCstr * lj_parse_keepstr(LexState* ls, const char* str, size_t len)
{
   return ls->keepstr(std::string_view(str, len));
}

#if LJ_HASFFI
LJ_USED LJ_FUNC void lj_parse_keepcdata(LexState* ls, TValue* tv, GCcdata* cd)
{
   ls->keepcdata(tv, cd);
}
#endif

// Jump list handling

JumpListView::Iterator::Iterator(FuncState* State, BCPos Position)
   : func_state(State), position(Position)
{
}

[[nodiscard]] BCPos JumpListView::Iterator::operator*() const
{
   return position;
}

JumpListView::Iterator& JumpListView::Iterator::operator++()
{
   position = JumpListView::next(func_state, position);
   return *this;
}

[[nodiscard]] bool JumpListView::Iterator::operator==(const Iterator& Other) const
{
   return position IS Other.position;
}

[[nodiscard]] bool JumpListView::Iterator::operator!=(const Iterator& Other) const
{
   return not(position IS Other.position);
}

JumpListView::JumpListView(FuncState* State, BCPos Head)
   : func_state(State), list_head(Head)
{
}

[[nodiscard]] JumpListView::Iterator JumpListView::begin() const
{
   return Iterator(func_state, list_head);
}

[[nodiscard]] JumpListView::Iterator JumpListView::end() const
{
   return Iterator(func_state, NO_JMP);
}

[[nodiscard]] bool JumpListView::empty() const
{
   return list_head IS NO_JMP;
}

[[nodiscard]] BCPos JumpListView::head() const
{
   return list_head;
}

[[nodiscard]] BCPos JumpListView::next(FuncState* State, BCPos Position)
{
   ptrdiff_t delta = bc_j(State->bcbase[Position].ins);
   if (BCPos(delta) IS NO_JMP) return NO_JMP;
   return BCPos((ptrdiff_t(Position) + 1) + delta);
}

[[nodiscard]] BCPos JumpListView::next(BCPos Position) const
{
   return next(func_state, Position);
}

[[nodiscard]] bool JumpListView::produces_values() const
{
   for (BCPos list = list_head; not(list IS NO_JMP); list = next(func_state, list)) {
      BCIns prior = func_state->bcbase[list >= 1 ? list - 1 : list].ins;
      if (!(bc_op(prior) IS BC_ISTC or bc_op(prior) IS BC_ISFC or bc_a(prior) IS NO_REG))
         return true;
   }
   return false;
}

[[nodiscard]] bool JumpListView::patch_test_register(BCPos Position, BCReg Register) const
{
   BCInsLine* line = &func_state->bcbase[Position >= 1 ? Position - 1 : Position];
   BCOp op = bc_op(line->ins);
   if (op IS BC_ISTC or op IS BC_ISFC) {
      if (Register != NO_REG and Register != bc_d(line->ins)) {
         setbc_a(&line->ins, Register);
      }
      else {
         setbc_op(&line->ins, op + (BC_IST - BC_ISTC));
         setbc_a(&line->ins, 0);
      }
   }
   else if (bc_a(line->ins) IS NO_REG) {
      if (Register IS NO_REG) {
         line->ins = BCINS_AJ(BC_JMP, bc_a(func_state->bcbase[Position].ins), 0);
      }
      else {
         setbc_a(&line->ins, Register);
         if (Register >= bc_a(line[1].ins))
            setbc_a(&line[1].ins, Register + 1);
      }
   }
   else return false;
   return true;
}

void JumpListView::drop_values() const
{
   for (BCPos list = list_head; not(list IS NO_JMP); list = next(func_state, list))
      (void)patch_test_register(list, NO_REG);
}

void JumpListView::patch_instruction(BCPos Position, BCPos Destination) const
{
   FuncState* fs = func_state;
   BCIns* instruction = &func_state->bcbase[Position].ins;
   BCPos offset = Destination - (Position + 1) + BCBIAS_J;
   lj_assertFS(not(Destination IS NO_JMP), "uninitialized jump target");
   if (offset > BCMAX_D)
      func_state->ls->err_syntax(ErrMsg::XJUMP);
   setbc_d(instruction, offset);
}

[[nodiscard]] BCPos JumpListView::append(BCPos Other) const
{
   if (Other IS NO_JMP) return list_head;
   if (list_head IS NO_JMP) return Other;
   BCPos list = list_head;
   BCPos next_pc;
   while (true) {
      next_pc = next(func_state, list);
      if (next_pc IS NO_JMP) break;
      list = next_pc;
   }
   patch_instruction(list, Other);
   return list_head;
}

void JumpListView::patch_with_value(BCPos ValueTarget, BCReg Register, BCPos DefaultTarget) const
{
   BCPos list = list_head;
   while (not(list IS NO_JMP)) {
      BCPos next_pc = next(func_state, list);
      if (patch_test_register(list, Register)) patch_instruction(list, ValueTarget);
      else patch_instruction(list, DefaultTarget);
      list = next_pc;
   }
}

void JumpListView::patch_to_here() const
{
   func_state->lasttarget = func_state->pc;
   JumpListView pending(func_state, func_state->jpc);
   func_state->jpc = pending.append(list_head);
}

void JumpListView::patch_to(BCPos Target) const
{
   if (Target IS func_state->pc) {
      patch_to_here();
   }
   else {
      FuncState* fs = func_state;
      lj_assertFS(Target < func_state->pc, "bad jump target");
      patch_with_value(Target, NO_REG, Target);
   }
}

void JumpListView::patch_head(BCPos Destination) const
{
   if (list_head IS NO_JMP) return;
   patch_instruction(list_head, Destination);
}
