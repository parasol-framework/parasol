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

//********************************************************************************************************************

static std::string format_string_constant(std::string_view Data)
{
   static constexpr size_t max_length = 40;
   static constexpr std::string_view hex_digits = "0123456789ABCDEF";

   std::string text;
   const auto limit = std::min(Data.size(), max_length);
   const bool truncated = Data.size() > max_length;

   text.reserve(limit * 2 + (truncated ? 6 : 2)); // Pre-allocate for quotes and possible escapes

   for (size_t i = 0; i < limit; i++) {
      const unsigned char ch = Data[i];

      switch (ch) {
         case '\n': text += "\\n"; break;
         case '\r': text += "\\r"; break;
         case '\t': text += "\\t"; break;
         case '\\': text += "\\\\"; break;
         case '"':  text += "\\\""; break;
         default:
            if (ch < 32) {
               text += "\\x";
               text += hex_digits[(ch >> 4) & 15];
               text += hex_digits[ch & 15];
            }
            else text += char(ch);
            break;
      }
   }

   if (truncated) text += "...";

   return std::format("\"{}\"", text);
}

//********************************************************************************************************************

template<typename T>
static BCLine get_line_from_info(const void *LineInfo, BCPos Offset, BCLine FirstLine) {
   return FirstLine + (BCLine)((const T *)LineInfo)[Offset];
}

static BCLine get_proto_line(GCproto *Proto, BCPos Pc)
{
   const void *lineinfo = proto_lineinfo(Proto);

   if ((Pc <= Proto->sizebc) and lineinfo) {
      const BCLine first_line = Proto->firstline;
      if (Pc IS Proto->sizebc) return first_line + Proto->numline;
      if (Pc IS 0) return first_line;

      const BCPos offset = Pc - 1;

      if (Proto->numline < 256) return get_line_from_info<uint8_t>(lineinfo, offset, first_line);
      if (Proto->numline < 65536) return get_line_from_info<uint16_t>(lineinfo, offset, first_line);
      return get_line_from_info<uint32_t>(lineinfo, offset, first_line);
   }

   return 0;
}

//********************************************************************************************************************

static std::string_view get_proto_uvname(GCproto *Proto, uint32_t Index)
{
   const uint8_t *info = proto_uvinfo(Proto);
   if (not info or Index >= Proto->sizeuv) return {};

   const uint8_t *ptr = info;

   for (uint32_t i = 0; i < Index; ++i) {
      while (*ptr) ++ptr;
      ++ptr;
   }

   return (const char *)ptr;
}

//********************************************************************************************************************

static std::string describe_num_constant(const TValue *Value)
{
   if (tvisint(Value)) return std::format("{}", intV(Value));
   if (tvisnum(Value)) return std::format("{}", numV(Value));
   return "<number>";
}

//********************************************************************************************************************

static std::string describe_gc_constant(GCproto *Proto, ptrdiff_t Index, [[maybe_unused]] bool Compact)
{
   GCobj *gc_obj = proto_kgc(Proto, Index);

   if (gc_obj->gch.gct IS (uint8_t)~LJ_TSTR) {
      GCstr *str_obj = gco2str(gc_obj);
      return std::format("K{}", format_string_constant({strdata(str_obj), str_obj->len}));
   }

   if (gc_obj->gch.gct IS (uint8_t)~LJ_TPROTO) {
      GCproto *child = gco2pt(gc_obj);
      return std::format("K<func {}-{}>", child->firstline, child->firstline + child->numline);
   }

   if (gc_obj->gch.gct IS (uint8_t)~LJ_TTAB) return "K<table>";

#if LJ_HASFFI
   if (gc_obj->gch.gct IS (uint8_t)~LJ_TCDATA) return "K<cdata>";
#endif

   return "K<gc>";
}

//********************************************************************************************************************

static std::string describe_primitive(int Value)
{
   switch (Value) {
      case 0: return "nil";
      case 1: return "false";
      case 2: return "true";
      default: return std::format("pri({})", Value);
   }
}

//********************************************************************************************************************

static void append_operand(std::string &Operands, std::string_view Label, std::string_view Value)
{
   if (not Operands.empty()) Operands += ' ';
   Operands += std::format("{}={}", Label, Value);
}

//********************************************************************************************************************

static std::string describe_operand_value(GCproto *Proto, BCMode Mode, int Value, BCPos Pc, [[maybe_unused]] bool Compact)
{
   switch (Mode) {
      case BCMdst:
      case BCMbase:
      case BCMvar:
      case BCMrbase:
         return std::format("R{}", Value);

      case BCMuv: {
         auto name = get_proto_uvname(Proto, (uint32_t)Value);
         return name.empty() ? std::format("U{}", Value) : std::format("U{}({})", Value, name);
      }

      case BCMlit:
         return std::format("#{}", Value);

      case BCMlits:
         return std::format("#{}", (int16_t)Value);

      case BCMpri:
         return describe_primitive(Value);

      case BCMnum:
         return std::format("#{}", describe_num_constant(proto_knumtv(Proto, Value)));

      case BCMstr:
      case BCMfunc:
      case BCMtab:
      case BCMcdata:
         return describe_gc_constant(Proto, -(ptrdiff_t)Value - 1, Compact);

      case BCMjump: {
         if ((BCPos)Value IS NO_JMP) return "->(no)";

         const ptrdiff_t offset = (ptrdiff_t)Value - BCBIAS_J;
         const ptrdiff_t dest = (ptrdiff_t)Pc + 1 + offset;

         if (dest < 0) return "->(neg)";
         if (dest >= (ptrdiff_t)Proto->sizebc) return "->(out)";

         return std::format("->{}", (BCPos)dest);
      }

      case BCMnone:
      default:
         return std::format("{}", Value);
   }
}

//********************************************************************************************************************

static void emit_disassembly(GCproto *Proto, std::ostringstream &Buf, bool Compact, int Indent = 0);

static void emit_disassembly(GCproto *Proto, std::ostringstream &Buf, bool Compact, int Indent)
{
   const BCIns *bc_stream = proto_bc(Proto);
   std::vector<uint8_t> targets(Proto->sizebc ? Proto->sizebc : 1, 0);
   std::string indent_str(Indent * 2, ' ');

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
         Buf << std::format("{}[{}]{}{} {}",
            indent_str, pc,
            targets[pc] ? "*" : "",
            line >= 0 ? std::format("({})", line) : "",
            glBytecodeNames[opcode]);
         if (not operands.empty()) Buf << " " << operands;
         Buf << "\n";
      }
      else {
         Buf << std::format("{}{:04d} {} {} {:<9}",
            indent_str, pc,
            targets[pc] ? "=>" : "  ",
            line >= 0 ? std::format("{:4}", line) : "   -",
            glBytecodeNames[opcode]);
         if (not operands.empty()) Buf << " " << operands;
         Buf << "\n";
      }

      // If this is a FNEW instruction, recursively disassemble the child prototype
      if (std::string_view opname = glBytecodeNames[opcode]; opname IS "FNEW") {
         // FNEW uses D operand (BCMfunc mode) which encodes GC constant index as negative
         const ptrdiff_t index = -(ptrdiff_t)value_d - 1;
         // Check if index is valid for proto_kgc (expects negative indices from -1 to -sizekgc)
         const bool valid = ((uintptr_t)(intptr_t)index >= (uintptr_t)-(intptr_t)Proto->sizekgc);
         if (valid) {
            GCobj *gc_obj = proto_kgc(Proto, index);
            if (gc_obj->gch.gct IS (uint8_t)~LJ_TPROTO) {
               GCproto *child = gco2pt(gc_obj);

               if (not Compact) {
                  Buf << std::format("{}  --- lines {}-{}, {} bytecodes ---\n",
                     indent_str, child->firstline, child->firstline + child->numline, child->sizebc);
               }

               emit_disassembly(child, Buf, Compact, Indent + 1);
            }
         }
      }
   }
}

//********************************************************************************************************************

static int append_dump_chunk([[maybe_unused]] lua_State *, const void *Chunk, size_t Size, void *UserData)
{
   auto bytes = (std::vector<uint8_t> *)UserData;
   if ((not bytes) or (not Chunk)) return 1;
   if (not Size) return 0; // End of dump signaled.

   const auto data = (const uint8_t *)Chunk;
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

   static constexpr std::string_view hex_digits = "0123456789abcdef";

   if (Compact) {
      for (const auto byte : Data) {
         Buf << hex_digits[(byte >> 4) & 0x0f] << hex_digits[byte & 0x0f];
      }
      Buf << "\n";
      return;
   }

   static constexpr size_t bytes_per_line = 16;

   for (size_t offset = 0; offset < Data.size(); offset += bytes_per_line) {
      const size_t line_end = std::min(offset + bytes_per_line, Data.size());

      // Write offset and hex bytes
      Buf << std::format("{:04x}: ", offset);

      for (size_t index = offset; index < offset + bytes_per_line; ++index) {
         if (index < line_end) {
            const uint8_t byte = Data[index];
            Buf << hex_digits[(byte >> 4) & 0x0f] << hex_digits[byte & 0x0f];
         }
         else Buf << "  ";

         if (index + 1 < offset + bytes_per_line) Buf << ' ';
      }

      // Write ASCII representation
      Buf << "  ";
      for (size_t index = offset; index < line_end; ++index) {
         Buf << (std::isprint(Data[index]) ? char(Data[index]) : '.');
      }

      Buf << "\n";
   }
}

/*********************************************************************************************************************

-METHOD-
DebugLog: Acquire a debug log from a compiled Script.

Use the DebugLog() method to acquire debug information from a Fluid script.  This method can be called from within
the script itself, or post-compilation to analyse the generated byte code.

The amount of debug information returned is defined by the Options parameter, which is a CSV list supporting the
following options:

<list type="unordered>
<li>stack: Returns the current stack trace. [L]</li>
<li>locals: Returns a list of all local variables and their values. [L]</li>
<li>upvalues: Returns a list of all upvalues. [L]</li>
<li>globals: Returns a list of all global variables and their values.</li>
<li>memory: Returns information about memory allocation and usage.</li>
<li>state: Returns the current state of the Fluid engine.</li>
<li>disasm: Returns disassembled bytecode.</li>
<li>dump: Returns a binary dump of the script.</li>
<li>funcinfo: Returns information about functions in the script.</li>
<li>compact: Returns a compact representation of the log.</li>
</list>

Options marked with [L] are only available when calling DebugLog() from inside the script.

The resulting log information is returned as a string, which needs to be deallocated once no longer required.

*********************************************************************************************************************/

static ERR FLUID_DebugLog(objScript *Self, struct sc::DebugLog *Args)
{
   pf::Log log;

   if (not Args) return log.warning(ERR::NullArgs);

   auto prv = (prvFluid *)Self->ChildPrivate;
   if (not prv->Lua) return log.warning(ERR::NotInitialised);

   log.branch("Options: %s", Args->Options ? Args->Options : "(none)");

   // Parse options (CSV list)

   struct DebugOptions {
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
   } opts;

   auto has_option = [](std::string_view haystack, std::string_view needle) {
      return haystack.find(needle) != std::string_view::npos;
   };

   if (Args->Options) {
      const std::string_view options = Args->Options;

      if (has_option(options, "all")) {
         opts.show_stack = opts.show_locals = opts.show_upvalues = opts.show_globals =
         opts.show_memory = opts.show_state = opts.show_disasm = opts.show_funcinfo = true;
      }
      else {
         opts.show_stack    = has_option(options, "stack");
         opts.show_locals   = has_option(options, "locals");
         opts.show_upvalues = has_option(options, "upvalues");
         opts.show_globals  = has_option(options, "globals");
         opts.show_memory   = has_option(options, "memory");
         opts.show_state    = has_option(options, "state");
         opts.show_disasm   = has_option(options, "disasm") or has_option(options, "bytecode");
         opts.show_dump     = has_option(options, "dump");
         opts.show_funcinfo = has_option(options, "funcinfo");
      }
      opts.compact = has_option(options, "compact");
   }
   else opts.show_stack = true; // Default: just the stack trace

   // Build the log message

   std::ostringstream buf;

   if (prv->Recurse) {
      // If Recurse is defined then we know what we're being called from within the script itself.
      // Here we can process options that are exclusive to internal calls.

      if (opts.show_stack) { // Stack trace
         if (not opts.compact) buf << "=== CALL STACK ===\n";

#if LJ_HASPROFILE
         size_t dump_len = 0;
         // Format codes: F=function name, l=source:line, p=preserve full path
         auto dump = luaJIT_profile_dumpstack(prv->Lua, opts.compact ? "pF (l)\n" : "l f\n", 50, &dump_len);
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

            if (opts.compact) {
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

         if (not opts.compact) buf <<"\n";
      }

      if (opts.show_locals) { // Local variables
         lua_Debug ar;
         if (lua_getstack(prv->Lua, 1, &ar)) { // Level 1 = caller's frame
            if (not opts.compact) buf <<"=== LOCALS ===\n";

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

               if (not opts.compact) buf <<" (" << lua_typename(prv->Lua, type) << ")";
               buf << "\n";

               lua_pop(prv->Lua, 1);
               idx++;
            }

            if (not opts.compact) buf <<"\n";
         }
      }

      if (opts.show_upvalues) { // Upvalues
         lua_Debug ar;
         if (lua_getstack(prv->Lua, 1, &ar)) { // Level 1 = caller's frame
            lua_getinfo(prv->Lua, "f", &ar);

            if (not opts.compact) buf <<"=== UPVALUES ===\n";

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

               if (not opts.compact) buf <<" (" << lua_typename(prv->Lua, type) << ")";
               buf << "\n";
               lua_pop(prv->Lua, 1);
               idx++;
            }

            lua_pop(prv->Lua, 1); // Pop the function
            if (not opts.compact) buf <<"\n";
         }
      }
   }

   // Here we can process options that are meaningful post-execution.

   if (opts.show_funcinfo) {
      if (not opts.compact) buf <<"=== FUNCTION INFORMATION ===\n";

      if (prv->Recurse) {
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

            if (opts.compact) {
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
         if (not opts.compact) buf <<"\n";
      }
   }

   if (opts.show_disasm) {
      lua_Debug ar;
      if (lua_getstack(prv->Lua, 1, &ar)) {
         lua_getinfo(prv->Lua, "Sln", &ar);

         if (not opts.compact) buf <<"=== BYTECODE DISASSEMBLY ===\n";

         if (lua_getinfo(prv->Lua, "f", &ar)) {
            GCfunc *fn = funcV(prv->Lua->top - 1);
            if (isluafunc(fn)) {
               GCproto *proto = funcproto(fn);
               const char *func_name = ar.name ? ar.name : "<anonymous>";

               if (not opts.compact) {
                  buf << "Function: " << func_name << " (" << ar.short_src << ":" << ar.linedefined
                      << "-" << ar.lastlinedefined << ")\n";
                  buf << "Bytecodes: " << proto->sizebc << ", Constants: " << proto->sizekn
                      << " numeric, " << proto->sizekgc << " objects\n\n";
               }

               emit_disassembly(proto, buf, opts.compact);
            }
            else buf << "(current frame is a C function; bytecode unavailable)\n";

            lua_pop(prv->Lua, 1);
         }
         else buf << "(unable to inspect current frame)\n";

         if (not opts.compact) buf <<"\n";
      }
      else {
         // No active frame - try to disassemble the main chunk if available
         if (not opts.compact) buf <<"=== BYTECODE DISASSEMBLY ===\n";

         if (prv->MainChunkRef) {
            lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, prv->MainChunkRef);
            if (lua_isfunction(prv->Lua, -1)) {
               GCfunc *fn = funcV(prv->Lua->top - 1);
               if (isluafunc(fn)) {
                  GCproto *proto = funcproto(fn);

                  if (not opts.compact) {
                     buf << "Main chunk (lines " << proto->firstline << "-" << (proto->firstline + proto->numline) << ")\n";
                     buf << "Bytecodes: " << proto->sizebc << ", Constants: " << proto->sizekn
                         << " numeric, " << proto->sizekgc << " objects\n\n";
                  }

                  emit_disassembly(proto, buf, opts.compact);
               }
               else buf << "(main chunk is a C function; bytecode unavailable)\n";
            }
            else buf << "(main chunk reference is not a function)\n";
            lua_pop(prv->Lua, 1);
         }
         else buf << "(no main chunk reference stored; bytecode disassembly requires calling DebugLog from within a function)\n";

         if (not opts.compact) buf <<"\n";
      }
   }

   if (opts.show_dump) {
      lua_Debug ar;
      if (lua_getstack(prv->Lua, 1, &ar)) {
         lua_getinfo(prv->Lua, "Sln", &ar);

         if (not opts.compact) buf <<"=== BYTECODE DUMP ===\n";

         if (lua_getinfo(prv->Lua, "f", &ar)) {
            GCfunc *fn = funcV(prv->Lua->top - 1);
            if (isluafunc(fn)) {
               std::vector<uint8_t> binary;

               if (lua_dump(prv->Lua, append_dump_chunk, &binary) IS 0) {
                  if (not opts.compact) {
                     const char *func_name = ar.name ? ar.name : "<anonymous>";
                     buf << "Function: " << func_name << " (" << ar.short_src << ":" << ar.linedefined
                         << "-" << ar.lastlinedefined << ")\n";
                     buf << "Bytes: " << binary.size() << "\n";
                  }

                  append_hex_dump(binary, buf, opts.compact);
               }
               else buf << "(failed to serialise bytecode)\n";
            }
            else buf << "(current frame is a C function; bytecode unavailable)\n";

            lua_pop(prv->Lua, 1);
         }
         else buf << "(unable to inspect current frame)\n";

         if (not opts.compact) buf <<"\n";
      }
      else {
         // No active frame - try to dump the main chunk if available
         if (not opts.compact) buf <<"=== BYTECODE DUMP ===\n";

         if (prv->MainChunkRef) {
            lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, prv->MainChunkRef);
            if (lua_isfunction(prv->Lua, -1)) {
               GCfunc *fn = funcV(prv->Lua->top - 1);
               if (isluafunc(fn)) {
                  std::vector<uint8_t> binary;

                  if (lua_dump(prv->Lua, append_dump_chunk, &binary) IS 0) {
                     if (not opts.compact) {
                        buf << "Main chunk\n";
                        buf << "Bytes: " << binary.size() << "\n";
                     }

                     append_hex_dump(binary, buf, opts.compact);
                  }
                  else buf << "(failed to serialise bytecode)\n";
               }
               else buf << "(main chunk is a C function; bytecode unavailable)\n";
            }
            else buf << "(main chunk reference is not a function)\n";
            lua_pop(prv->Lua, 1);
         }
         else buf << "(no main chunk reference stored; bytecode dump requires calling DebugLog from within a function)\n";

         if (not opts.compact) buf <<"\n";
      }
   }

   if (opts.show_globals) { // Global variables
      if (not opts.compact) buf << "=== GLOBALS ===\n";

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

                  if (not opts.compact) buf <<" (" << lua_typename(prv->Lua, type) << ")";
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

      if (not opts.compact) buf << "\n";
   }

   if (opts.show_memory) { // Memory statistics
      if (not opts.compact) buf << "=== MEMORY STATISTICS ===\n";

      const int kb = lua_gc(prv->Lua, LUA_GCCOUNT, 0);
      const int bytes = lua_gc(prv->Lua, LUA_GCCOUNTB, 0);
      const double mb = kb / 1024.0 + bytes / (1024.0 * 1024.0);

      buf << (opts.compact
         ? std::format("Lua heap: {} MB\n", mb)
         : std::format("Lua heap usage: {} MB ({} KB + {} bytes)\n", mb, kb, bytes));

      if (not opts.compact) buf << "\n";
   }

   if (opts.show_state) { // State information
      if (not opts.compact) buf << "=== STATE ===\n";

      buf << std::format("Stack top: {}\n", lua_gettop(prv->Lua));
      buf << std::format("Protected globals: {}\n", prv->Lua->ProtectedGlobals ? "true" : "false");

      if (auto hook_mask = lua_gethookmask(prv->Lua)) {
         std::vector<std::string_view> flags;
         if (hook_mask & LUA_MASKCALL) flags.emplace_back("CALL");
         if (hook_mask & LUA_MASKRET) flags.emplace_back("RET");
         if (hook_mask & LUA_MASKLINE) flags.emplace_back("LINE");
         if (hook_mask & LUA_MASKCOUNT) flags.emplace_back("COUNT");

         buf << "Hook mask: ";
         for (size_t i = 0; i < flags.size(); ++i) {
            if (i > 0) buf << "|";
            buf << flags[i];
         }
         buf << "\n";
      }
      else buf << "Hook mask: none\n";

      if (not opts.compact) buf << "\n";
   }

   const std::string result = buf.str();
   if ((Args->Result = pf::strclone(result.c_str())) IS nullptr) {
      return ERR::AllocMemory;
   }
   return ERR::Okay;
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
