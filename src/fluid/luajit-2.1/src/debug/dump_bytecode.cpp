
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_buf.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_func.h"
#include "lj_state.h"
#include "lj_bc.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#endif
#include "lj_strfmt.h"
#include "lj_lex.h"
#include "lj_parse.h"
#include "lj_vm.h"
#include "lj_vmevent.h"

#include <parasol/main.h>

#include "token_types.h"
#include "parse_types.h"
#include "dump_bytecode.h"
#include "../../../defs.h"

//********************************************************************************************************************

static std::string format_string_constant(std::string_view Data)
{
   static constexpr size_t max_length = 40;
   static constexpr std::string_view hex_digits = "0123456789ABCDEF";

   std::string text;
   const auto limit = std::min(Data.size(), max_length);
   const bool truncated = Data.size() > max_length;

   text.reserve(limit * 2 + (truncated ? 6 : 2));

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

static std::string describe_num_constant(const TValue *Value)
{
   if (tvisint(Value)) return std::format("{}", intV(Value));
   if (tvisnum(Value)) return std::format("{}", numV(Value));
   return "<number>";
}

//********************************************************************************************************************

static std::string describe_gc_constant(GCproto *Proto, ptrdiff_t Index)
{
   GCobj *gc_obj = proto_kgc(Proto, Index);

   if (not gc_obj) {
      // Most likely an invalid index - could indicate an invalid bytecode stream has been generated.
      pf::Log("ByteCode").warning("describe_gc_constant: null GC object at index %" PRId64, uint64_t(Index));
      return "K<null>";
   }

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

static std::string_view get_proto_uvname(GCproto *Proto, uint32_t Index)
{
   const uint8_t *info = proto_uvinfo(Proto);
   if (not info or Index >= Proto->sizeuv) return {};

   const uint8_t *ptr = info;

   for (uint32_t i = 0; i < Index; ++i) {
      while (*ptr) ++ptr;
      ++ptr;
   }

   if (not *ptr) return {};
   return std::string_view(reinterpret_cast<const char *>(ptr));
}

//********************************************************************************************************************

static std::string describe_operand_value(GCproto *Proto, BCMode Mode, int Value, BCPos Pc)
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
         return describe_gc_constant(Proto, -(ptrdiff_t)Value - 1);

      case BCMjump: {
         if ((BCPos)Value IS NO_JMP) return "->(no)";

         const ptrdiff_t offset = (ptrdiff_t)Value - BCBIAS_J;
         const ptrdiff_t dest = (ptrdiff_t)Pc + 1 + offset;

         if (dest < 0) return "->(neg)";
         if (dest >= (ptrdiff_t)Proto->sizebc) return "->(out)";

         return std::format("->{}{}", dest, offset >= 0 ? std::format("(+{})", offset) : std::format("({})", offset));
      }

      default:
         return std::format("?{}", Value);
   }
}

//********************************************************************************************************************

static void append_operand(std::string &Operands, std::string_view Label, std::string_view Value)
{
   if (not Operands.empty()) Operands += ' ';
   Operands += std::format("{}={}", Label, Value);
}

//********************************************************************************************************************
// Describe operand value during parsing (from FuncState context)

static std::string describe_operand_from_fs(FuncState *fs, BCMode Mode, int Value, BCPos Pc)
{
   switch (Mode) {
      case BCMdst:
      case BCMbase:
      case BCMvar:
      case BCMrbase:
         return std::format("R{}", Value);

      case BCMuv:
         return std::format("U{}", Value);

      case BCMlit:
         return std::format("#{}", Value);

      case BCMlits:
         return std::format("#{}", (int16_t)Value);

      case BCMpri:
         return describe_primitive(Value);

      case BCMnum: {
         // Look up number constant in the constant table
         GCtab *kt = fs->kt;
         Node *node = noderef(kt->node);

         for (uint32_t i = 0; i <= kt->hmask; ++i) {
            TValue *val = &node[i].val;
            if (tvhaskslot(val) and tvkslot(val) IS (uint32_t)Value) {
               TValue *key_tv = &node[i].key;
               if (tvisnum(key_tv) or tvisint(key_tv)) {
                  return std::format("#{}", describe_num_constant(key_tv));
               }
               break;
            }
         }
         return std::format("#<num{}>", Value);
      }

      case BCMstr:
      case BCMfunc:
      case BCMtab:
      case BCMcdata: {
         // Look up GC constant in the constant table
         GCtab *kt = fs->kt;
         Node *node = noderef(kt->node);

         for (uint32_t i = 0; i <= kt->hmask; ++i) {
            TValue *val = &node[i].val;
            if (tvhaskslot(val) and tvkslot(val) IS (uint32_t)Value) {
               TValue *key_tv = &node[i].key;

               if (tvisstr(key_tv)) {
                  GCstr *str_obj = strV(key_tv);
                  return std::format("K{}", format_string_constant({strdata(str_obj), str_obj->len}));
               }

               if (tvisproto(key_tv)) {
                  GCproto *child = protoV(key_tv);
                  return std::format("K<func {}-{}>", child->firstline, child->firstline + child->numline);
               }

               if (tvistab(key_tv)) return "K<table>";

#if LJ_HASFFI
               if (tviscdata(key_tv)) return "K<cdata>";
#endif
               break;
            }
         }
         return std::format("K<gc{}>", Value);
      }

      case BCMjump: {
         if ((BCPos)Value IS NO_JMP) return "->(no)";

         const ptrdiff_t offset = (ptrdiff_t)Value - BCBIAS_J;
         const ptrdiff_t dest = (ptrdiff_t)Pc + 1 + offset;

         if (dest < 0) return "->(neg)";
         if (dest >= (ptrdiff_t)fs->pc) return "->(out)";

         return std::format("->{}{}", dest, offset >= 0 ? std::format("(+{})", offset) : std::format("({})", offset));
      }

      default:
         return std::format("?{}", Value);
   }
}

//********************************************************************************************************************
// Extract bytecode info and build operands string - common helper

struct BytecodeInfo {
   BCOp op;
   CSTRING op_name;
   BCMode mode_a, mode_b, mode_c, mode_d;
   int value_a, value_b, value_c, value_d;
};

static BytecodeInfo extract_instruction_info(BCIns Ins)
{
   BytecodeInfo info;
   info.op = bc_op(Ins);
   info.op_name = (info.op < BC__MAX) ? glBytecodeNames[info.op] : "???";
   info.mode_a  = bcmode_a(info.op);
   info.mode_b  = bcmode_b(info.op);
   info.mode_c  = bcmode_c(info.op);
   info.mode_d  = bcmode_d(info.op);
   info.value_a = bc_a(Ins);
   info.value_b = bc_b(Ins);
   info.value_c = bc_c(Ins);
   info.value_d = bc_d(Ins);
   return info;
}

//********************************************************************************************************************
// Recursively print bytecode for a finalized prototype.

static void trace_proto_bytecode(GCproto *Proto, int Indent = 0)
{
   pf::Log log("ByteCode");
   if (not Proto) return;

   const BCIns *bc_stream = proto_bc(Proto);
   std::string indent_str(Indent * 2, ' ');

   if (Indent > 0) {
      log.branch("%s--- Nested function: lines %d-%d, %d bytecodes ---", indent_str.c_str(), int(Proto->firstline),
         int(Proto->firstline + Proto->numline), int(Proto->sizebc));
   }

   for (BCPos pc = 0; pc < Proto->sizebc; ++pc) {
      BCIns instruction = bc_stream[pc];
      auto info = extract_instruction_info(instruction);

      std::string operands;

      if (info.mode_a != BCMnone) append_operand(operands, "A", describe_operand_value(Proto, info.mode_a, info.value_a, pc));

      if (bcmode_hasd(info.op)) {
         if (info.mode_d != BCMnone) append_operand(operands, "D", describe_operand_value(Proto, info.mode_d, info.value_d, pc));
      }
      else {
         if (info.mode_b != BCMnone) append_operand(operands, "B", describe_operand_value(Proto, info.mode_b, info.value_b, pc));
         if (info.mode_c != BCMnone) append_operand(operands, "C", describe_operand_value(Proto, info.mode_c, info.value_c, pc));
      }

      log.msg("%s[%04d] %-10s %s", indent_str.c_str(), (int)pc, info.op_name, operands.c_str());

      // If this is a FNEW instruction, recursively disassemble the child prototype
      if (info.op IS BC_FNEW) {
         const ptrdiff_t index = -(ptrdiff_t)info.value_d - 1;
         const bool valid = ((uintptr_t)(intptr_t)index >= (uintptr_t)-(intptr_t)Proto->sizekgc);

         if (valid) {
            GCobj *gc_obj = proto_kgc(Proto, index);
            if (gc_obj->gch.gct IS (uint8_t)~LJ_TPROTO) {
               GCproto *child = gco2pt(gc_obj);
               trace_proto_bytecode(child, Indent + 1);
            }
         }
      }
   }
}

//********************************************************************************************************************
// Print a complete disassembly of bytecode instructions.

extern void dump_bytecode(ParserContext &Context)
{
   pf::Log log("ByteCode");

   FuncState &fs = Context.func();
   log.branch("Instruction Count: %u", (unsigned)fs.pc);
   for (BCPos pc = 0; pc < fs.pc; ++pc) {
      const BCInsLine& line = fs.bcbase[pc];
      auto info = extract_instruction_info(line.ins);

      std::string operands;

      if (info.mode_a != BCMnone) append_operand(operands, "A", describe_operand_from_fs(&fs, info.mode_a, info.value_a, pc));

      if (bcmode_hasd(info.op)) {
         if (info.mode_d != BCMnone) append_operand(operands, "D", describe_operand_from_fs(&fs, info.mode_d, info.value_d, pc));
      }
      else {
         if (info.mode_b != BCMnone) append_operand(operands, "B", describe_operand_from_fs(&fs, info.mode_b, info.value_b, pc));
         if (info.mode_c != BCMnone) append_operand(operands, "C", describe_operand_from_fs(&fs, info.mode_c, info.value_c, pc));
      }

      log.msg("[%04d] %-10s %s", (int)pc, info.op_name, operands.c_str());

      // If this is a FNEW instruction, look up and print the child prototype
      if (info.op IS BC_FNEW) {
         // FNEW uses D operand which stores the constant slot index
         // Search the hash table to find the proto with this slot number
         GCtab *kt = fs.kt;
         Node *node = noderef(kt->node);

         for (uint32_t i = 0; i <= kt->hmask; ++i) {
            TValue *val = &node[i].val;
            if (tvhaskslot(val) and tvkslot(val) IS (uint32_t)info.value_d) {
               TValue *key_tv = &node[i].key;
               if (tvisproto(key_tv)) {
                  GCproto *child = protoV(key_tv);
                  trace_proto_bytecode(child, 1);
               }
               break;
            }
         }
      }
   }
}
