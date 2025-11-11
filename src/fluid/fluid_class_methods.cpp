// Support methods for the Script class

//********************************************************************************************************************

static ERR FLUID_GetProcedureID(objScript *, struct sc::GetProcedureID *);
static ERR FLUID_DerefProcedure(objScript *, struct sc::DerefProcedure *);
static ERR FLUID_DebugLog(objScript *, struct sc::DebugLog *);

static const MethodEntry clMethods[] = {
   { sc::GetProcedureID::id, (APTR)FLUID_GetProcedureID, "GetProcedureID", nullptr, 0 },
   { sc::DerefProcedure::id, (APTR)FLUID_DerefProcedure, "DerefProcedure", nullptr, 0 },
   { sc::DebugLog::id,       (APTR)FLUID_DebugLog,       "DebugLog", nullptr, 0 },
   { AC::NIL, nullptr, nullptr, nullptr, 0 }
};

//********************************************************************************************************************

static const char *const glBytecodeNames[] = {
#define BCNAME(name, ma, mb, mc, mt) #name,
   BCDEF(BCNAME)
#undef BCNAME
};

static std::string format_string_constant(const char *Data, size_t Length)
{
   std::string text;
   size_t limit = Length;
   bool truncated = false;

   if (Length > 40) {
      limit = 40;
      truncated = true;
   }

   for (size_t i = 0; i < limit; i++) {
      unsigned char ch = (unsigned char)Data[i];

      if (ch IS '\n') text += "\\n";
      else if (ch IS '\r') text += "\\r";
      else if (ch IS '\t') text += "\\t";
      else if (ch IS '\\') text += "\\\\";
      else if (ch IS '"') text += "\\\"";
      else if (ch < 32) {
         const char *digits = "0123456789ABCDEF";
         text += "\\x";
         text += digits[(ch >> 4) & 15];
         text += digits[ch & 15];
      }
      else text += char(ch);
   }

   if (truncated) text += "...";

   return std::string("\"") + text + "\"";
}

static BCLine get_proto_line(GCproto *Proto, BCPos Pc)
{
   const void *lineinfo = proto_lineinfo(Proto);

   if ((Pc <= Proto->sizebc) and lineinfo) {
      BCLine first_line = Proto->firstline;
      if (Pc IS Proto->sizebc) return first_line + Proto->numline;
      if (Pc IS 0) return first_line;

      BCPos offset = Pc - 1;

      if (Proto->numline < 256) return first_line + (BCLine)((const uint8_t *)lineinfo)[offset];
      if (Proto->numline < 65536) return first_line + (BCLine)((const uint16_t *)lineinfo)[offset];
      return first_line + (BCLine)((const uint32_t *)lineinfo)[offset];
   }

   return 0;
}

static const char *get_proto_uvname(GCproto *Proto, uint32_t Index)
{
   const uint8_t *info = proto_uvinfo(Proto);
   if (not info) return "";
   if (Index >= Proto->sizeuv) return "";

   const uint8_t *ptr = info;
   uint32_t remaining = Index;

   while (remaining) {
      while (*ptr) ptr++;
      ptr++;
      remaining--;
   }

   return (const char *)ptr;
}

static std::string describe_num_constant(const TValue *Value)
{
   if (tvisint(Value)) return std::to_string(intV(Value));

   if (tvisnum(Value)) {
      std::ostringstream number;
      number << numV(Value);
      return number.str();
   }

   return std::string("<number>");
}

static std::string describe_gc_constant(GCproto *Proto, ptrdiff_t Index, bool Compact)
{
   GCobj *gc_obj = proto_kgc(Proto, Index);

   if (gc_obj->gch.gct IS (uint8_t)~LJ_TSTR) {
      GCstr *str_obj = gco2str(gc_obj);
      return std::string("K") + format_string_constant(strdata(str_obj), str_obj->len);
   }

   if (gc_obj->gch.gct IS (uint8_t)~LJ_TPROTO) {
      GCproto *child = gco2pt(gc_obj);
      std::ostringstream info;
      info << "K<func ";
      info << child->firstline;
      info << "-";
      info << (child->firstline + child->numline);
      info << ">";
      return info.str();
   }

   if (gc_obj->gch.gct IS (uint8_t)~LJ_TTAB) return std::string("K<table>");

#if LJ_HASFFI
   if (gc_obj->gch.gct IS (uint8_t)~LJ_TCDATA) return std::string("K<cdata>");
#endif

   (void)Compact;
   return std::string("K<gc>");
}

static std::string describe_primitive(int Value)
{
   if (Value IS 0) return std::string("nil");
   if (Value IS 1) return std::string("false");
   if (Value IS 2) return std::string("true");
   return std::string("pri(") + std::to_string(Value) + ")";
}

static void append_operand(std::string &Operands, const char *Label, const std::string &Value)
{
   if (not Operands.empty()) Operands += " ";
   Operands += Label;
   Operands += "=";
   Operands += Value;
}

static std::string describe_operand_value(GCproto *Proto, BCMode Mode, int Value, BCPos Pc, bool Compact)
{
   switch (Mode) {
      case BCMdst:
      case BCMbase:
      case BCMvar:
      case BCMrbase:
         return std::string("R") + std::to_string(Value);

      case BCMuv: {
         const char *name = get_proto_uvname(Proto, (uint32_t)Value);
         if (name && name[0]) {
            std::string text = "U";
            text += std::to_string(Value);
            text += "(";
            text += name;
            text += ")";
            return text;
         }
         return std::string("U") + std::to_string(Value);
      }

      case BCMlit:
         return std::string("#") + std::to_string(Value);

      case BCMlits:
         return std::string("#") + std::to_string((int16_t)Value);

      case BCMpri:
         return describe_primitive(Value);

      case BCMnum: {
         const TValue *number = proto_knumtv(Proto, Value);
         return std::string("#") + describe_num_constant(number);
      }

      case BCMstr: {
         ptrdiff_t index = -(ptrdiff_t)Value - 1;
         return describe_gc_constant(Proto, index, Compact);
      }

      case BCMfunc: {
         ptrdiff_t index = -(ptrdiff_t)Value - 1;
         return describe_gc_constant(Proto, index, Compact);
      }

      case BCMtab: {
         ptrdiff_t index = -(ptrdiff_t)Value - 1;
         return describe_gc_constant(Proto, index, Compact);
      }

      case BCMcdata: {
         ptrdiff_t index = -(ptrdiff_t)Value - 1;
         return describe_gc_constant(Proto, index, Compact);
      }

      case BCMjump: {
         if ((BCPos)Value IS NO_JMP) return std::string("->(no)");

         ptrdiff_t offset = (ptrdiff_t)Value - BCBIAS_J;
         ptrdiff_t dest = (ptrdiff_t)Pc + 1 + offset;

         if (dest < 0) return std::string("->(neg)");
         if (dest >= (ptrdiff_t)Proto->sizebc) return std::string("->(out)");

         return std::string("->") + std::to_string((BCPos)dest);
      }

      case BCMnone:
      default:
         break;
   }

   (void)Proto;
   (void)Pc;
   (void)Compact;
   return std::to_string(Value);
}

static void emit_disassembly(GCproto *Proto, std::ostringstream &Buf, bool Compact)
{
   const BCIns *bc_stream = proto_bc(Proto);
   std::vector<uint8_t> targets(Proto->sizebc ? Proto->sizebc : 1, 0);

   for (BCPos pc = 0; pc < Proto->sizebc; pc++) {
      BCIns instruction = bc_stream[pc];
      BCOp opcode = bc_op(instruction);

      if (bcmode_hasd(opcode)) {
         BCMode mode_d = bcmode_d(opcode);
         if (mode_d IS BCMjump) {
            int value = bc_d(instruction);
            if ((BCPos)value != NO_JMP) {
               ptrdiff_t offset = (ptrdiff_t)value - BCBIAS_J;
               ptrdiff_t dest = (ptrdiff_t)pc + 1 + offset;
               if (dest >= 0 and dest < (ptrdiff_t)Proto->sizebc) targets[(size_t)dest] = 1;
            }
         }
      }
   }

   for (BCPos pc = 0; pc < Proto->sizebc; pc++) {
      BCIns instruction = bc_stream[pc];
      BCOp opcode = bc_op(instruction);
      BCMode mode_a = bcmode_a(opcode);
      BCMode mode_b = bcmode_b(opcode);
      BCMode mode_c = bcmode_c(opcode);
      BCMode mode_d = bcmode_d(opcode);

      int value_a = bc_a(instruction);
      int value_b = bc_b(instruction);
      int value_c = bc_c(instruction);
      int value_d = bc_d(instruction);

      std::string operands;

      if (mode_a != BCMnone) append_operand(operands, "A", describe_operand_value(Proto, mode_a, value_a, pc, Compact));

      if (bcmode_hasd(opcode)) {
         if (mode_d != BCMnone) append_operand(operands, "D", describe_operand_value(Proto, mode_d, value_d, pc, Compact));
      }
      else {
         if (mode_b != BCMnone) append_operand(operands, "B", describe_operand_value(Proto, mode_b, value_b, pc, Compact));
         if (mode_c != BCMnone) append_operand(operands, "C", describe_operand_value(Proto, mode_c, value_c, pc, Compact));
      }

      BCLine line = get_proto_line(Proto, pc);

      if (Compact) {
         Buf << "[" << pc << "]";
         if (targets[pc]) Buf << "*";
         if (line >= 0) Buf << "(" << line << ")";
         Buf << " " << glBytecodeNames[opcode];
         if (not operands.empty()) Buf << " " << operands;
         Buf << "\n";
      }
      else {
         std::ostringstream line_buf;
         line_buf << std::setfill('0') << std::setw(4) << pc;
         line_buf << " ";
         line_buf << (targets[pc] ? "=>" : "  ");
         line_buf << " ";
         line_buf << std::setfill(' ');
         if (line >= 0) line_buf << std::setw(4) << line;
         else line_buf << "   -";
         line_buf << " ";
         line_buf << std::left << std::setw(9) << glBytecodeNames[opcode];
         line_buf << std::right;
         if (not operands.empty()) line_buf << " " << operands;
         Buf << line_buf.str() << "\n";
      }
   }
}

//********************************************************************************************************************

static int append_dump_chunk(lua_State *, const void *Chunk, size_t Size, void *UserData)
{
   auto bytes = reinterpret_cast<std::vector<uint8_t> *>(UserData);
   if (not bytes or not Chunk or (Size IS 0)) return 1;

   auto data = static_cast<const uint8_t *>(Chunk);
   bytes->insert(bytes->end(), data, data + Size);
   return 0;
}

//********************************************************************************************************************

static void append_hex_dump(const std::vector<uint8_t> &Data, std::ostringstream &Buf, bool Compact)
{
   if (Data.empty()) {
      Buf << "(empty)\n";
      return;
   }

   constexpr char digits[] = "0123456789abcdef";

   if (Compact) {
      for (auto byte : Data) {
         Buf << digits[(byte >> 4) & 0x0f] << digits[byte & 0x0f];
      }
      Buf << "\n";
      return;
   }

   constexpr size_t bytes_per_line = 16;
   size_t offset = 0;

   while (offset < Data.size()) {
      Buf << std::setfill('0') << std::setw(4) << offset << ": ";
      Buf << std::setfill(' ');

      size_t line_end = std::min(offset + bytes_per_line, Data.size());

      for (size_t index = offset; index < offset + bytes_per_line; index++) {
         if (index < line_end) {
            uint8_t byte = Data[index];
            Buf << digits[(byte >> 4) & 0x0f] << digits[byte & 0x0f];
         }
         else Buf << "  ";

         if (index + 1 < offset + bytes_per_line) Buf << ' ';
      }

      Buf << "  ";

      for (size_t index = offset; index < line_end; index++) {
         unsigned char ch = Data[index];
         Buf << (std::isprint(ch) ? char(ch) : '.');
      }

      Buf << "\n";
      offset += bytes_per_line;
   }
}

//********************************************************************************************************************

static ERR FLUID_DebugLog(objScript *Self, struct sc::DebugLog *Args)
{
   pf::Log log;

   if (not Args) return log.warning(ERR::NullArgs);
   
   auto prv = (prvFluid *)Self->ChildPrivate;
   if (not prv->Lua) return log.warning(ERR::NotInitialised);

   log.branch("Options: %s", Args->Options ? Args->Options : "(none)");

   // Parse options (CSV list)

   bool show_stack = false;
   bool show_locals = false;
   bool show_upvalues = false;
   bool show_globals = false;
   bool show_memory = false;
   bool show_state = false;
   bool show_disasm = false;
   bool show_dump = false;
   bool show_funcinfo = false;
   bool compact = false;
   bool log_output = false;

   if (Args->Options) {
      std::string_view opts = Args->Options;

      if (opts.find("all") != std::string::npos) {
         show_stack = show_locals = show_upvalues = show_globals =
         show_memory = show_state = show_disasm = true;
         show_funcinfo = true;
      }
      else {
         show_stack    = (opts.find("stack") != std::string::npos);
         show_locals   = (opts.find("locals") != std::string::npos);
         show_upvalues = (opts.find("upvalues") != std::string::npos);
         show_globals  = (opts.find("globals") != std::string::npos);
         show_memory   = (opts.find("memory") != std::string::npos);
         show_state    = (opts.find("state") != std::string::npos);
         show_disasm   = (opts.find("disasm") != std::string::npos);
         if (not show_disasm) show_disasm = (opts.find("bytecode") != std::string::npos);
         show_dump     = (opts.find("dump") != std::string::npos);
         show_funcinfo = (opts.find("funcinfo") != std::string::npos);
         log_output    = (opts.find("log") != std::string::npos);
      }
      compact = (opts.find("compact") != std::string::npos);
   }
   else show_stack = true; // Default: just the stack trace

   // Build the log message

   std::ostringstream buf;

   if (prv->Recurse) {
      // If Recurse is defined then we know what we're being called from within the script itself.
      // Here we can process options that are exclusive to internal calls.

      if (show_stack) { // Stack trace
         if (not compact) buf << "=== CALL STACK ===\n";

#if LJ_HASPROFILE
         size_t dump_len = 0;
         // Format codes: F=function name, l=source:line, p=preserve full path
         auto dump = luaJIT_profile_dumpstack(prv->Lua, compact ? "pF (l)\n" : "l f\n", 50, &dump_len);
         if (dump and dump_len) {
            // Skip the first line (level 0) which is the C function mtDebugLog itself
            auto first_newline = (const char *)memchr(dump, '\n', dump_len);
            if (first_newline and size_t(first_newline - dump + 1) < dump_len) {
               const char *start = first_newline + 1;
               size_t len = dump_len - (start - dump);
               buf << std::string_view(start, len);
            }
         }
#else
         lua_Debug ar;
         int level = 1; // Start at 1 to skip the C function (mtDebugLog) itself

         while (lua_getstack(prv->Lua, level, &ar)) {
            lua_getinfo(prv->Lua, "nSl", &ar);

            if (compact) {
               buf << "[" << level << "] ";
               if (ar.name) buf << ar.name;
               else buf << "?";
               if (ar.source and ar.source[0]) buf << " (" << ar.short_src << ":" << ar.currentline << ")";
               buf << "\n";
            }
            else {
               buf << "[" << level << "] ";
               if (ar.name) buf << ar.name;
               else buf << "<anonymous>";

               if (ar.source and ar.source[0]) {
                  buf << " (" << ar.short_src << ":" << ar.currentline << ")";
               }

               buf << " - ";
               if (ar.what) {
                  if (strcmp(ar.what, "Lua") IS 0) buf << "Lua function";
                  else if (strcmp(ar.what, "C") IS 0) buf << "C function";
                  else if (strcmp(ar.what, "main") IS 0) buf << "main chunk";
                  else buf << ar.what;
               }
               buf << "\n";
            }
            level++;
         }
#endif

         if (not compact) buf << "\n";
      }

      if (show_funcinfo) {
         if (not compact) buf << "=== FUNCTION INFORMATION ===\n";

         int level = 1;
         bool wrote = false;
         lua_Debug ar;

         while (lua_getstack(prv->Lua, level, &ar)) {
            if (not lua_getinfo(prv->Lua, "nSl", &ar)) {
               level++;
               continue;
            }

            const char *func_name = ar.name ? ar.name : "<anonymous>";
            bool is_lua_func = false;
            int param_count = 0;
            bool is_vararg = false;
            uint32_t frame_slots = 0;
            uint32_t bytecodes = 0;
            uint32_t numeric_consts = 0;
            uint32_t object_consts = 0;
            int upvalues = 0;

            if (lua_getinfo(prv->Lua, "f", &ar)) {
               GCfunc *fn = funcV(prv->Lua->top - 1);
               upvalues = fn->c.nupvalues;
               if (isluafunc(fn)) {
                  is_lua_func = true;
                  GCproto *pt = funcproto(fn);
                  param_count = pt->numparams;
                  is_vararg = (pt->flags & PROTO_VARARG) != 0;
                  frame_slots = pt->framesize;
                  bytecodes = pt->sizebc;
                  numeric_consts = pt->sizekn;
                  object_consts = pt->sizekgc;
               }
               else is_vararg = true;
               lua_pop(prv->Lua, 1);
            }

            if (compact) {
               buf << "[" << level << "] " << func_name;
               if (is_lua_func) {
                  buf << " (" << ar.short_src << ":" << ar.linedefined << "-" << ar.lastlinedefined << ")";
                  buf << " params=" << param_count;
                  buf << " vararg=" << (is_vararg ? "true" : "false");
                  buf << " slots=" << frame_slots;
                  buf << " bytecode=" << bytecodes;
                  buf << " consts=" << numeric_consts << "+" << object_consts;
               }
               else buf << " <C function>";
               buf << "\n";
            }
            else {
               buf << "Function [" << level << "]: " << func_name;
               if (is_lua_func) {
                  buf << " (" << ar.short_src << ":" << ar.linedefined << "-" << ar.lastlinedefined << ")\n";
                  buf << "   Parameters: " << param_count << "\n";
                  buf << "   Vararg: " << (is_vararg ? "true" : "false") << "\n";
                  buf << "   Stack slots: " << frame_slots << "\n";
                  buf << "   Bytecodes: " << bytecodes << "\n";
                  buf << "   Constants: " << numeric_consts << " numeric, " << object_consts << " objects\n";
                  buf << "   Upvalues: " << upvalues << "\n";
               }
               else {
                  buf << " (<C function>)\n";
                  buf << "   Upvalues: " << upvalues << "\n";
               }
            }

            wrote = true;
            level++;
         }

         if (not wrote) buf << "(no frames)";
         if (not compact) buf << "\n";
      }

      if (show_locals) { // Local variables
         lua_Debug ar;
         if (lua_getstack(prv->Lua, 1, &ar)) { // Level 1 = caller's frame
            if (not compact) buf << "=== LOCALS ===\n";

            int idx = 1;
            const char *name;
            while ((name = lua_getlocal(prv->Lua, &ar, idx))) {
               int type = lua_type(prv->Lua, -1);
               buf << name << " = ";

               switch (type) {
                  case LUA_TNIL: buf << "nil"; break;
                  case LUA_TBOOLEAN: buf << (lua_toboolean(prv->Lua, -1) ? "true" : "false"); break;
                  case LUA_TNUMBER: buf << lua_tonumber(prv->Lua, -1); break;
                  case LUA_TSTRING: {
                     size_t len;
                     const char *str = lua_tolstring(prv->Lua, -1, &len);
                     std::string_view sv(str, len);
                     if (len > 40) buf << "\"" << sv.substr(0, 40) << "...\"";
                     else buf << "\"" << sv << "\"";
                     break;
                  }
                  case LUA_TTABLE: buf << "{ ... }"; break;
                  case LUA_TFUNCTION: buf << "<function>"; break;
                  case LUA_TUSERDATA: buf << "<userdata>"; break;
                  case LUA_TTHREAD: buf << "<thread>"; break;
                  default: buf << "<" << lua_typename(prv->Lua, type) << ">"; break;
               }

               if (not compact) buf << " (" << lua_typename(prv->Lua, type) << ")";
               buf << "\n";

               lua_pop(prv->Lua, 1);
               idx++;
            }

            if (not compact) buf << "\n";
         }
      }

      if (show_upvalues) { // Upvalues
         lua_Debug ar;
         if (lua_getstack(prv->Lua, 1, &ar)) { // Level 1 = caller's frame
            lua_getinfo(prv->Lua, "f", &ar);

            if (not compact) buf << "=== UPVALUES ===\n";

            int idx = 1;
            const char *name;
            while ((name = lua_getupvalue(prv->Lua, -1, idx))) {
               int type = lua_type(prv->Lua, -1);

               buf << name << " = ";

               switch (type) {
                  case LUA_TNIL: buf << "nil"; break;
                  case LUA_TBOOLEAN: buf << (lua_toboolean(prv->Lua, -1) ? "true" : "false"); break;
                  case LUA_TNUMBER: buf << lua_tonumber(prv->Lua, -1); break;
                  case LUA_TSTRING: {
                     size_t len;
                     const char *str = lua_tolstring(prv->Lua, -1, &len);
                     std::string_view sv(str, len);
                     if (len > 40) buf << "\"" << sv.substr(0, 40) << "...\"";
                     else buf << "\"" << sv << "\"";
                     break;
                  }
                  case LUA_TTABLE: buf << "{ ... }"; break;
                  case LUA_TFUNCTION: buf << "<function>"; break;
                  default: buf << "<" << lua_typename(prv->Lua, type) << ">"; break;
               }

               if (not compact) buf << " (" << lua_typename(prv->Lua, type) << ")";
               buf << "\n";
               lua_pop(prv->Lua, 1);
               idx++;
            }

            lua_pop(prv->Lua, 1); // Pop the function
            if (not compact) buf << "\n";
         }
      }
   }

   // Here we can process options that are meaningful post-execution.

   if (show_disasm) {
      lua_Debug ar;
      if (lua_getstack(prv->Lua, 1, &ar)) {
         lua_getinfo(prv->Lua, "Sln", &ar);

         if (not compact) buf << "=== BYTECODE DISASSEMBLY ===\n";

         if (lua_getinfo(prv->Lua, "f", &ar)) {
            GCfunc *fn = funcV(prv->Lua->top - 1);
            if (isluafunc(fn)) {
               GCproto *proto = funcproto(fn);
               const char *func_name = ar.name ? ar.name : "<anonymous>";

               if (not compact) {
                  buf << "Function: " << func_name << " (" << ar.short_src << ":" << ar.linedefined
                      << "-" << ar.lastlinedefined << ")\n";
                  buf << "Bytecodes: " << proto->sizebc << ", Constants: " << proto->sizekn
                      << " numeric, " << proto->sizekgc << " objects\n\n";
               }

               emit_disassembly(proto, buf, compact);
            }
            else buf << "(current frame is a C function; bytecode unavailable)\n";

            lua_pop(prv->Lua, 1);
         }
         else buf << "(unable to inspect current frame)\n";

         if (not compact) buf << "\n";
      }
      else {
         if (not compact) buf << "=== BYTECODE DISASSEMBLY ===\n";
         buf << "(no active Lua frame)\n";
         if (not compact) buf << "\n";
      }
   }

   if (show_dump) {
      lua_Debug ar;
      if (lua_getstack(prv->Lua, 1, &ar)) {
         lua_getinfo(prv->Lua, "Sln", &ar);

         if (not compact) buf << "=== BYTECODE DUMP ===\n";

         if (lua_getinfo(prv->Lua, "f", &ar)) {
            GCfunc *fn = funcV(prv->Lua->top - 1);
            if (isluafunc(fn)) {
               std::vector<uint8_t> binary;

               if (lua_dump(prv->Lua, append_dump_chunk, &binary) IS 0) {
                  if (not compact) {
                     const char *func_name = ar.name ? ar.name : "<anonymous>";
                     buf << "Function: " << func_name << " (" << ar.short_src << ":" << ar.linedefined
                         << "-" << ar.lastlinedefined << ")\n";
                     buf << "Bytes: " << binary.size() << "\n";
                  }

                  append_hex_dump(binary, buf, compact);
               }
               else buf << "(failed to serialise bytecode)\n";
            }
            else buf << "(current frame is a C function; bytecode unavailable)\n";

            lua_pop(prv->Lua, 1);
         }
         else buf << "(unable to inspect current frame)\n";

         if (not compact) buf << "\n";
      }
      else {
         if (not compact) buf << "=== BYTECODE DUMP ===\n";
         buf << "(no active Lua frame)\n";
         if (not compact) buf << "\n";
      }
   }

   if (show_globals) { // Global variables
      if (not compact) buf << "=== GLOBALS ===\n";

      // Access the storage table where user-defined globals are stored.
      // The storage table is stored as an upvalue in the __index closure of the global metatable.

      lua_pushvalue(prv->Lua, LUA_GLOBALSINDEX); // Push global environment
      if (lua_getmetatable(prv->Lua, -1)) {
         lua_pushstring(prv->Lua, "__index");
         lua_rawget(prv->Lua, -2); // Get the __index closure

         if (lua_isfunction(prv->Lua, -1)) {
            // The storage table is the first upvalue of the __index closure
            auto upvalue_name = lua_getupvalue(prv->Lua, -1, 1);
            if (upvalue_name and lua_istable(prv->Lua, -1)) {
               int count = 0;
               lua_pushnil(prv->Lua);
               while (lua_next(prv->Lua, -2)) {
                  const char *key = lua_tostring(prv->Lua, -2);
                  buf << key << " = ";

                  int type = lua_type(prv->Lua, -1);
                  switch (type) {
                     case LUA_TNIL: buf << "nil"; break;
                     case LUA_TBOOLEAN: buf << (lua_toboolean(prv->Lua, -1) ? "true" : "false"); break;
                     case LUA_TNUMBER: buf << lua_tonumber(prv->Lua, -1); break;
                     case LUA_TSTRING: {
                        size_t len;
                        const char *str = lua_tolstring(prv->Lua, -1, &len);
                        std::string_view sv(str, len);
                        if (len > 40) buf << "\"" << sv.substr(0, 40) << "...\"";
                        else buf << "\"" << sv << "\"";
                        break;
                     }
                     case LUA_TTABLE: buf << "{ ... }"; break;
                     case LUA_TFUNCTION: buf << "<function>"; break;
                     default: buf << "<" << lua_typename(prv->Lua, type) << ">"; break;
                  }

                  if (not compact) buf << " (" << lua_typename(prv->Lua, type) << ")";
                  buf << "\n";
                  count++;

                  lua_pop(prv->Lua, 1);
               }

               if (count IS 0) buf << "(none)\n";
               lua_pop(prv->Lua, 1); // Pop storage table
            }
         }
         lua_pop(prv->Lua, 1); // Pop __index closure
         lua_pop(prv->Lua, 1); // Pop metatable
      }
      lua_pop(prv->Lua, 1); // Pop global environment

      if (not compact) buf << "\n";
   }

   if (show_memory) { // Memory statistics
      if (not compact) buf << "=== MEMORY STATISTICS ===\n";

      int kb = lua_gc(prv->Lua, LUA_GCCOUNT, 0);
      int bytes = lua_gc(prv->Lua, LUA_GCCOUNTB, 0);
      double mb = kb / 1024.0 + bytes / (1024.0 * 1024.0);

      if (compact) buf << "Lua heap: " << mb << " MB\n";
      else buf << "Lua heap usage: " << mb << " MB (" << kb << " KB + " << bytes << " bytes)\n";

      if (not compact) buf << "\n";
   }
  
   if (show_state) { // State information
      if (not compact) buf << "=== STATE ===\n";

      buf << "Stack top: " << lua_gettop(prv->Lua) << "\n";
      buf << "Protected globals: " << (prv->Lua->ProtectedGlobals ? "true" : "false") << "\n";

      if (auto hook_mask = lua_gethookmask(prv->Lua)) {
         buf << "Hook mask: ";
         bool first = true;
         if (hook_mask & LUA_MASKCALL) { buf << (first ? "" : "|") << "CALL"; first = false; }
         if (hook_mask & LUA_MASKRET) { buf << (first ? "" : "|") << "RET"; first = false; }
         if (hook_mask & LUA_MASKLINE) { buf << (first ? "" : "|") << "LINE"; first = false; }
         if (hook_mask & LUA_MASKCOUNT) { buf << (first ? "" : "|") << "COUNT"; first = false; }
         buf << "\n";
      }
      else buf << "Hook mask: none\n";

      if (not compact) buf << "\n";
   }

   std::string result = buf.str();
   if ((Args->Result = pf::strclone(result.c_str())) == nullptr) {
      return ERR::AllocMemory;
   }
   else {
      if (log_output) log.msg("%.400s", Args->Result);
      return ERR::Okay;
   }
}

//********************************************************************************************************************

static ERR FLUID_DerefProcedure(objScript *Self, struct sc::DerefProcedure *Args)
{
   pf::Log log;

   if (not Args) return ERR::NullArgs;

   if ((Args->Procedure) and (Args->Procedure->isScript())) {
      if (Args->Procedure->Context IS Self) { // Verification of ownership
         auto prv = (prvFluid *)Self->ChildPrivate;
         if (not prv) return log.warning(ERR::ObjectCorrupt);

         log.trace("Dereferencing procedure #%" PF64, (long long)Args->Procedure->ProcedureID);

         if (Args->Procedure->ProcedureID) {
            luaL_unref(prv->Lua, LUA_REGISTRYINDEX, Args->Procedure->ProcedureID);
            Args->Procedure->ProcedureID = 0;
         }
         return ERR::Okay;
      }
      else return log.warning(ERR::Args);
   }
   else return log.warning(ERR::Args);
}

//********************************************************************************************************************

static ERR FLUID_GetProcedureID(objScript *Self, struct sc::GetProcedureID *Args)
{
   pf::Log log;

   if ((not Args) or (not Args->Procedure) or (not Args->Procedure[0])) return log.warning(ERR::NullArgs);

   auto prv = (prvFluid *)Self->ChildPrivate;
   if (not prv) return log.warning(ERR::ObjectCorrupt);

   if ((not prv->Lua) or (not Self->ActivationCount)) {
      log.warning("Cannot resolve function '%s'.  Script requires activation.", Args->Procedure);
      return ERR::NotFound;
   }

   lua_getglobal(prv->Lua, Args->Procedure);
   auto id = luaL_ref(prv->Lua, LUA_REGISTRYINDEX);
   if ((id != LUA_REFNIL) and (id != LUA_NOREF)) {
      Args->ProcedureID = id;
      return ERR::Okay;
   }
   else {
      log.warning("Failed to resolve function name '%s' to an ID.", Args->Procedure);
      return ERR::NotFound;
   }
}
