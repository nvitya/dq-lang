/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    module_intf.h
 * authors: nvitya
 * created: 2026-05-04
 * brief:   DQ Module Interface Class
 */

#include "module_intf.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <print>

#include "comp_options.h"
#include "dqm_if.h"
#include "otype_func.h"
#include "otype_int.h"
#include "otype_float.h"
#include "otype_bool.h"
#include "otype_cstring.h"

static string TypeName(OType * atype)
{
  return (atype ? atype->name : "?");
}

static string TypeKindName(ETypeKind akind)
{
  switch (akind)
  {
    case TK_VOID:         return "void";
    case TK_INT:          return "int";
    case TK_FLOAT:        return "float";
    case TK_BOOL:         return "bool";
    case TK_POINTER:      return "pointer";
    case TK_ARRAY:        return "array";
    case TK_ARRAY_SLICE:  return "array_slice";
    case TK_STRING:       return "string";
    case TK_ALIAS:        return "alias";
    case TK_ENUM:         return "enum";
    case TK_COMPOUND:     return "compound";
    case TK_FUNCTION:     return "function";
    case TK_FUNCREF:      return "funcref";
  }

  return "unknown";
}

static string ParamModeText(EParamMode amode)
{
  switch (amode)
  {
    case FPM_VALUE:    return "";
    case FPM_REF:      return "ref ";
    case FPM_REFIN:    return "refin ";
    case FPM_REFOUT:   return "refout ";
    case FPM_REFNULL:  return "refnull ";
  }

  return "";
}

static string EscapeStringLiteral(const string & avalue)
{
  string result = "\"";
  for (char c : avalue)
  {
    if ('\\' == c)
    {
      result += "\\\\";
    }
    else if ('"' == c)
    {
      result += "\\\"";
    }
    else if ('\n' == c)
    {
      result += "\\n";
    }
    else if ('\r' == c)
    {
      result += "\\r";
    }
    else if ('\t' == c)
    {
      result += "\\t";
    }
    else
    {
      result += c;
    }
  }
  result += "\"";
  return result;
}

static string ConstValueText(OValue * avalue)
{
  if (!avalue)
  {
    return "<null>";
  }
  if (auto * v = dynamic_cast<OValueInt *>(avalue))
  {
    return to_string(v->value);
  }
  if (auto * v = dynamic_cast<OValueFloat *>(avalue))
  {
    return to_string(v->value);
  }
  if (auto * v = dynamic_cast<OValueBool *>(avalue))
  {
    return (v->value ? "true" : "false");
  }
  if (auto * v = dynamic_cast<OValueCString *>(avalue))
  {
    return EscapeStringLiteral(v->value);
  }
  if (auto * v = dynamic_cast<OValuePointer *>(avalue))
  {
    return (v->is_null ? "null" : "<pointer>");
  }

  return "<unsupported>";
}

static OTypeFunc * FuncTypeOf(OValSymFunc * afunc)
{
  return (afunc ? dynamic_cast<OTypeFunc *>(afunc->ptype) : nullptr);
}

static bool IsImplicitReceiverParam(OValSymFunc * afunc, OFuncParam * aparam, bool afirst_param)
{
  return (afirst_param && afunc && afunc->owner_compound_type
          && aparam && ("__this" == aparam->name));
}

static string FunctionSignature(OValSymFunc * afunc)
{
  OTypeFunc * sigtype = FuncTypeOf(afunc);
  string result = "function ";
  result += (afunc ? afunc->name : "?");
  result += "(";

  bool first = true;
  if (sigtype)
  {
    for (OFuncParam * param : sigtype->params)
    {
      if (IsImplicitReceiverParam(afunc, param, first))
      {
        continue;
      }

      if (!first)
      {
        result += ", ";
      }

      result += ParamModeText(param->mode);
      result += param->name;
      result += " : ";
      result += TypeName(param->ptype);
      first = false;
    }

    if (sigtype->has_varargs)
    {
      if (!first)
      {
        result += ", ";
      }
      result += "...";
    }

    result += ")";
    if (sigtype->rettype)
    {
      result += " -> ";
      result += TypeName(sigtype->rettype);
    }
  }
  else
  {
    result += ")";
  }

  return result;
}

static string FunctionState(OValSymFunc * afunc)
{
  if (!afunc)
  {
    return "unknown";
  }
  if (afunc->is_external)
  {
    return "external";
  }
  if (afunc->IsForwardDecl())
  {
    return "forward";
  }
  if (afunc->has_body)
  {
    return "body";
  }
  return "decl";
}

static void WriteValSym(ostream & out, OValSym * avsym, const string & indent);

static void WriteFunction(ostream & out, OValSymFunc * afunc, const string & indent)
{
  out << indent << FunctionSignature(afunc) << " [" << FunctionState(afunc) << "]\n";
}

static void WriteOverloadSet(ostream & out, OValSymOverloadSet * aovset, const string & indent)
{
  out << indent << "overload " << (aovset ? aovset->name : "?") << "\n";
  if (aovset)
  {
    for (OValSymFunc * fn : aovset->funcs)
    {
      WriteFunction(out, fn, indent + "  ");
    }
  }
  out << indent << "endoverload\n";
}

static void WriteCompoundType(ostream & out, OCompoundType * atype, const string & indent)
{
  out << indent << (atype->is_object ? "object " : "struct ") << atype->name << "\n";

  for (OValSym * member : atype->member_order)
  {
    out << indent << "  field " << member->name << " : " << TypeName(member->ptype) << "\n";
  }

  if (atype->is_object)
  {
    for (auto & [name, vs] : atype->Members()->valsyms)
    {
      (void)name;
      if (VSK_FUNCTION == vs->kind)
      {
        WriteValSym(out, vs, indent + "  ");
      }
    }
  }

  out << indent << (atype->is_object ? "endobj" : "endstruct") << "\n";
}

static void WriteType(ostream & out, OType * atype, const string & indent)
{
  if (!atype)
  {
    out << indent << "type ?\n";
    return;
  }

  if (auto * alias = dynamic_cast<OTypeAlias *>(atype))
  {
    out << indent << "type " << alias->name << " = " << TypeName(alias->ptype) << "\n";
  }
  else if (auto * ctype = dynamic_cast<OCompoundType *>(atype))
  {
    WriteCompoundType(out, ctype, indent);
  }
  else if (auto * fref = dynamic_cast<OTypeFuncRef *>(atype))
  {
    out << indent << "type " << fref->name << " = " << FuncTypeName(fref->functype) << "\n";
  }
  else if (auto * ftype = dynamic_cast<OTypeFunc *>(atype))
  {
    out << indent << "type " << ftype->name << " = " << FuncTypeName(ftype) << "\n";
  }
  else
  {
    out << indent << "type " << atype->name << " [kind=" << TypeKindName(atype->kind)
        << ", size=" << atype->bytesize << "]\n";
  }
}

static void WriteValSym(ostream & out, OValSym * avsym, const string & indent)
{
  if (!avsym)
  {
    out << indent << "val ?\n";
    return;
  }

  if (auto * fn = dynamic_cast<OValSymFunc *>(avsym))
  {
    WriteFunction(out, fn, indent);
  }
  else if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(avsym))
  {
    WriteOverloadSet(out, ovset, indent);
  }
  else
  {
    if (auto * vconst = dynamic_cast<OValSymConst *>(avsym))
    {
      out << indent << "const " << avsym->name << " : " << TypeName(avsym->ptype)
          << " = " << ConstValueText(vconst->pvalue) << "\n";
    }
    else
    {
      string kind;
      if (VSK_PARAMETER == avsym->kind) kind = "param";
      else                              kind = "var";

      out << indent << kind << " " << avsym->name << " : " << TypeName(avsym->ptype) << "\n";
    }
  }
}

OIntfDecl * OModuleIntf::AddPublicType(OType * atype)
{
  OType * deftype = scope_pub->DefineType(atype);
  if (deftype != atype)
  {
    return nullptr;
  }

  OIntfDecl * result = new OIntfDecl(atype);
  declarations.push_back(result);
  return result;
}

OIntfDecl * OModuleIntf::AddPublicValSym(OValSym * avalsym)
{
  OValSym * defvs = scope_pub->DefineValSym(avalsym);
  if (defvs != avalsym)
  {
    return nullptr;
  }

  OIntfDecl * result = new OIntfDecl(avalsym);
  declarations.push_back(result);
  return result;
}

static string DqmIfTargetArch()
{
#if defined(HOST_X86)
  #if defined(TARGET_64BIT)
    return "x86_64";
  #else
    return "x86";
  #endif
#elif defined(HOST_ARM)
  #if defined(TARGET_64BIT)
    return "aarch64";
  #else
    return "arm";
  #endif
#elif defined(HOST_RISCV)
  #if defined(TARGET_64BIT)
    return "riscv64";
  #else
    return "riscv32";
  #endif
#else
  return "unknown";
#endif
}

static string DqmIfTargetRtl()
{
#if defined(TARGET_WIN)
  return "win";
#elif defined(TARGET_LINUX)
  return "linux";
#else
  return "unknown";
#endif
}

static string DqmIfBuildOptions()
{
  string result = "O" + to_string(g_opt.optlevel);
  if (g_opt.dbg_info)      result += ";g";
  if (g_opt.compile_only)  result += ";c";

  for (const OCmdLineDefine & def : g_opt.cmdline_defines)
  {
    result += ";D";
    result += def.name;
    if (def.has_bool_value)
    {
      result += "=";
      result += (def.bool_value ? "true" : "false");
    }
    else if (def.has_int_value)
    {
      result += "=";
      result += to_string(def.int_value);
    }
  }

  return result;
}

static bool WriteDqmIfSourceMetadata(ODqmIfWriter & writer, const string & source_filename)
{
  if (!writer.AddRecEmpty(DQMIF_H_BEGIN)) return false;
  if (!source_filename.empty()
      && !writer.AddRecStr(DQMIF_H_SRC_FILENAME, source_filename)) return false;

  error_code ec;
  uintmax_t fsize = filesystem::file_size(source_filename, ec);
  if (!ec && !writer.AddRecI64(DQMIF_H_SRC_FILESIZE, int64_t(fsize))) return false;

  ec.clear();
  auto ftime = filesystem::last_write_time(source_filename, ec);
  if (!ec)
  {
    auto ticks = chrono::duration_cast<chrono::nanoseconds>(ftime.time_since_epoch()).count();
    if (!writer.AddRecI64(DQMIF_H_SRC_FILETIME, int64_t(ticks))) return false;
  }

  if (!writer.AddRecStr(DQMIF_H_TARGET_ARCH, DqmIfTargetArch())) return false;
  if (!writer.AddRecStr(DQMIF_H_TARGET_RTL, DqmIfTargetRtl())) return false;
  if (!writer.AddRecStr(DQMIF_H_BUILD_OPTIONS, DqmIfBuildOptions())) return false;
  return writer.AddRecEmpty(DQMIF_H_END);
}

bool OModuleIntf::WriteInterface(const string & filename, const string & source_filename)
{
  ODqmIfWriter writer;

  if (!WriteDqmIfSourceMetadata(writer, source_filename))
  {
    print("Can not write module interface file: {}\n{}\n", filename, writer.error);
    return false;
  }

  for (const string & libname : g_opt.link_libraries)
  {
    if (!writer.AddRecStr(DQMIF_LINKLIB, libname))
    {
      print("Can not write module interface file: {}\n{}\n", filename, writer.error);
      return false;
    }
  }

  for (OIntfDecl * decl : declarations)
  {
    if (!decl)
    {
      continue;
    }

    if (IDK_TYPE == decl->kind)
    {
      if (!decl->ptype || !decl->ptype->WriteDqmIfDecl(writer))
      {
        print("Can not write module interface file: {}\n{}\n", filename, writer.error);
        return false;
      }
    }
    else if (IDK_VALSYM == decl->kind)
    {
      if (!decl->pvalsym || !decl->pvalsym->WriteDqmIfDecl(writer))
      {
        print("Can not write module interface file: {}\n{}\n", filename, writer.error);
        return false;
      }
    }
  }

  if (!writer.WriteToFile(filename))
  {
    print("Can not write module interface file: {}\n{}\n", filename, writer.error);
    return false;
  }

  print("Module interface written: {}\n", filename);
  return true;
}

bool OModuleIntf::ReadInterface(const string & filename)
{
  ifstream inf(filename, ios::binary);
  if (!inf)
  {
    print("Can not read module interface file: {}\n", filename);
    return false;
  }

  string header;
  getline(inf, header);
  if ("DQMIF-STUB" != header)
  {
    print("Invalid or unsupported module interface file: {}\n", filename);
    return false;
  }

  string line;
  while (getline(inf, line))
  {
    const string name_prefix = "name=";
    if (line.starts_with(name_prefix))
    {
      name = line.substr(name_prefix.size());
    }
  }

  return true;
}

bool DumpModuleInterface(const string & filename)
{
  OModuleIntf intf(nullptr, "dqm_if_dump");
  if (!intf.ReadInterface(filename))
  {
    return false;
  }

  print("DQ module interface dump: {}\n", filename);
  print("  format: stub\n");
  print("  name: {}\n", intf.name);
  return true;
}
