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
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <print>
#include <typeinfo>

#include "comp_options.h"
#include "dqm_if.h"
#include "otype_func.h"
#include "otype_array.h"
#include "otype_int.h"
#include "otype_float.h"
#include "otype_bool.h"
#include "otype_cstring.h"
#include "scope_builtins.h"

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
  if (typeid(*avalue) == typeid(OValue))
  {
    return "[linked]";
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

void OModuleIntf::WriteFunctionDump(ostream & out, OValSymFunc * afunc, const string & indent)
{
  string state = FunctionState(afunc);
  out << indent << FunctionSignature(afunc);
  if ("forward" != state)
  {
    out << " [" << state << "]";
  }
  out << "\n";
}

void OModuleIntf::WriteOverloadSetDump(ostream & out, OValSymOverloadSet * aovset, const string & indent)
{
  out << indent << "overload " << (aovset ? aovset->name : "?") << "\n";
  if (aovset)
  {
    for (OValSymFunc * fn : aovset->funcs)
    {
      WriteFunctionDump(out, fn, indent + "  ");
    }
  }
  out << indent << "endoverload\n";
}

void OModuleIntf::WriteCompoundDump(ostream & out, OCompoundType * atype, const string & indent)
{
  out << indent << (atype->is_object ? "object " : "struct ")
      << atype->name << "(" << atype->bytesize << ")\n";

  for (OValSym * member : atype->member_order)
  {
    out << indent << "  field(" << member->field_offset << ") "
        << member->name << " : " << TypeName(member->ptype) << "\n";
  }

  if (atype->is_object)
  {
    for (auto & [name, vs] : atype->Members()->valsyms)
    {
      (void)name;
      if (VSK_FUNCTION == vs->kind)
      {
        WriteValSymDump(out, vs, indent + "  ");
      }
    }
  }

  out << indent << (atype->is_object ? "endobj" : "endstruct") << "\n";
}

void OModuleIntf::WriteTypeDump(ostream & out, OType * atype, const string & indent)
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
    WriteCompoundDump(out, ctype, indent);
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

void OModuleIntf::WriteValSymDump(ostream & out, OValSym * avsym, const string & indent)
{
  if (!avsym)
  {
    out << indent << "val ?\n";
    return;
  }

  if (auto * fn = dynamic_cast<OValSymFunc *>(avsym))
  {
    WriteFunctionDump(out, fn, indent);
  }
  else if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(avsym))
  {
    WriteOverloadSetDump(out, ovset, indent);
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

void OModuleIntf::WriteDump(ostream & out)
{
  for (OIntfDecl * decl : declarations)
  {
    if (!decl)
    {
      continue;
    }
    if (IDK_TYPE == decl->kind)
    {
      WriteTypeDump(out, decl->ptype, "  ");
    }
    else if (IDK_VALSYM == decl->kind)
    {
      WriteValSymDump(out, decl->pvalsym, "  ");
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

bool OModuleIntf::WriteInterfaceRecords(ODqmIfWriter & writer, const string & source_filename)
{
  if (!WriteDqmIfSourceMetadata(writer, source_filename))
  {
    return false;
  }

  for (const string & libname : g_opt.link_libraries)
  {
    if (!writer.AddRecStr(DQMIF_LINKLIB, libname))
    {
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
        return false;
      }
    }
    else if (IDK_VALSYM == decl->kind)
    {
      if (!decl->pvalsym || !decl->pvalsym->WriteDqmIfDecl(writer))
      {
        return false;
      }
    }
  }

  return true;
}

bool OModuleIntf::BuildInterfaceBytes(vector<uint8_t> & rdata, const string & source_filename)
{
  ODqmIfWriter writer;

  if (!WriteInterfaceRecords(writer, source_filename) || !writer.BuildFileData(rdata))
  {
    print("Can not build module interface data for: {}\n{}\n", source_filename, writer.error);
    return false;
  }

  return true;
}

bool OModuleIntf::WriteInterface(const string & filename, const string & source_filename)
{
  ODqmIfWriter writer;

  if (!WriteInterfaceRecords(writer, source_filename))
  {
    print("Can not write module interface file: {}\n{}\n", filename, writer.error);
    return false;
  }

  if (!writer.WriteToFile(filename))
  {
    print("Can not write module interface file: {}\n{}\n", filename, writer.error);
    return false;
  }

  print("Module interface written: {}\n", filename);
  return true;
}

OType * OModuleIntf::ResolveDqmIfTypeName(const string & atype_name)
{
  OType * result = scope_pub->FindType(atype_name);
  if (result)
  {
    return result;
  }

  const string cstring_prefix = "cstring[";
  if (atype_name.starts_with(cstring_prefix) && atype_name.ends_with("]"))
  {
    string lenstr = atype_name.substr(cstring_prefix.size(),
        atype_name.size() - cstring_prefix.size() - 1);
    try
    {
      size_t used = 0;
      unsigned long len = stoul(lenstr, &used, 10);
      if ((used == lenstr.size()) && (len > 0) && (len <= UINT32_MAX))
      {
        return g_builtins->type_cstring->GetSizedType(uint32_t(len));
      }
    }
    catch (...)
    {
      return nullptr;
    }
  }

  if (atype_name.starts_with("function("))
  {
    return new OTypeFuncRef(nullptr, atype_name);
  }

  return nullptr;
}

bool OModuleIntf::ReadTypeSpec(ODqmIfReader & reader, OType *& rtype)
{
  rtype = nullptr;

  if ((DQMIF_TYPE_SPEC_SIMPLE == reader.recid) || (DQMIF_TYPE_SPEC_NAME == reader.recid)
      || (DQMIF_TYPE_SPEC_FUNCREF == reader.recid) || (DQMIF_TYPE_SPEC_OBJFUNCREF == reader.recid))
  {
    string typename_str;
    if (!reader.ReadString(typename_str))
    {
      return false;
    }
    rtype = ResolveDqmIfTypeName(typename_str);
    if (!rtype)
    {
      return reader.Fail(format("Unknown DQM interface type: {}", typename_str));
    }
    return true;
  }

  if (DQMIF_TYPE_SPEC_BEGIN == reader.recid)
  {
    if (!reader.NextRec())
    {
      return false;
    }
    if (!ReadTypeSpec(reader, rtype))
    {
      return false;
    }
    if (!reader.NextRec())
    {
      return false;
    }
    return reader.ExpectEmpty(DQMIF_TYPE_SPEC_END);
  }

  return ReadTypeSpecInner(reader, rtype, DQMIF_TYPE_SPEC_END);
}

bool OModuleIntf::ReadTypeSpecInner(ODqmIfReader & reader, OType *& rtype, TDqmIfRecId aend_recid)
{
  (void)aend_recid;

  if (DQMIF_TYPE_SPEC_PTR == reader.recid)
  {
    if (!reader.ExpectEmpty(DQMIF_TYPE_SPEC_PTR))
    {
      return false;
    }
    OType * basetype = nullptr;
    if (!reader.NextRec() || !ReadTypeSpec(reader, basetype))
    {
      return false;
    }
    rtype = basetype->GetPointerType();
    return true;
  }

  if (DQMIF_TYPE_SPEC_ARRAY_BEGIN == reader.recid)
  {
    int32_t arraylen = 0;
    if (!reader.ReadI32(arraylen) || (arraylen < 0))
    {
      return reader.Fail("Invalid DQM interface array type length");
    }

    OType * elemtype = nullptr;
    if (!reader.NextRec() || !ReadTypeSpec(reader, elemtype))
    {
      return false;
    }
    if (!reader.NextRec() || !reader.ExpectEmpty(DQMIF_TYPE_SPEC_ARRAY_END))
    {
      return false;
    }
    rtype = elemtype->GetArrayType(uint32_t(arraylen));
    return true;
  }

  if (DQMIF_TYPE_SPEC_SLICE_BEGIN == reader.recid)
  {
    if (!reader.ExpectEmpty(DQMIF_TYPE_SPEC_SLICE_BEGIN))
    {
      return false;
    }
    OType * elemtype = nullptr;
    if (!reader.NextRec() || !ReadTypeSpec(reader, elemtype))
    {
      return false;
    }
    if (!reader.NextRec() || !reader.ExpectEmpty(DQMIF_TYPE_SPEC_SLICE_END))
    {
      return false;
    }
    rtype = elemtype->GetSliceType();
    return true;
  }

  if (reader.recid == aend_recid)
  {
    return reader.Fail("Empty DQM interface type spec");
  }

  return reader.Fail(format("Unexpected DQM interface type spec record 0x{:04X}", reader.recid));
}

bool OModuleIntf::ReadAttributes(ODqmIfReader & reader, SDqmIfAttributes & rattrs)
{
  while ((DQMIF_ATTR_FLAGS == reader.recid) || (DQMIF_ATTR_ALIGN_VALUE == reader.recid)
         || (DQMIF_ATTR_EXT_LINK_NAME == reader.recid) || (DQMIF_ATTR_SECTION_NAME == reader.recid))
  {
    if (DQMIF_ATTR_FLAGS == reader.recid)
    {
      if (!reader.ReadU64(rattrs.flags)) return false;
    }
    else if (DQMIF_ATTR_ALIGN_VALUE == reader.recid)
    {
      int32_t align = 0;
      if (!reader.ReadI32(align) || (align < 0)) return reader.Fail("Invalid DQM interface align attribute");
      rattrs.align = uint32_t(align);
    }
    else if (DQMIF_ATTR_EXT_LINK_NAME == reader.recid)
    {
      if (!reader.ReadString(rattrs.external_linkage_name)) return false;
    }
    else if (DQMIF_ATTR_SECTION_NAME == reader.recid)
    {
      if (!reader.ReadString(rattrs.section_name)) return false;
    }

    if (!reader.NextRec())
    {
      return false;
    }
  }
  return true;
}

bool OModuleIntf::ApplyDqmIfAttributes(OValSym * avalsym, const SDqmIfAttributes & attrs)
{
  if (!avalsym)
  {
    return false;
  }

  avalsym->attr_is_overload = (attrs.flags & (1u << 0));
  avalsym->attr_is_override = (attrs.flags & (1u << 1));
  avalsym->attr_is_virtual  = (attrs.flags & (1u << 2));
  avalsym->attr_is_volatile = (attrs.flags & (1u << 3));
  avalsym->is_ref_alias     = (attrs.flags & (1u << 4));
  avalsym->ref_nullable     = (attrs.flags & (1u << 5));
  avalsym->attr_align = attrs.align;
  avalsym->attr_section_name = attrs.section_name;
  return true;
}

bool OModuleIntf::ReadInlineValue(ODqmIfReader & reader, OType * atype, OValue *& rvalue)
{
  rvalue = nullptr;
  if (!atype)
  {
    return reader.Fail("DQM interface value has no type");
  }

  if (DQMIF_VALUE_LINKED == reader.recid)
  {
    if (!reader.ExpectEmpty(DQMIF_VALUE_LINKED))
    {
      return false;
    }
    rvalue = new OValue(atype);
    return true;
  }

  if (DQMIF_VALUE_INLINE != reader.recid)
  {
    return reader.Fail(format("Expected DQM interface value record, got 0x{:04X}", reader.recid));
  }

  OType * rtype = atype->ResolveAlias();
  if (!rtype)
  {
    return reader.Fail("DQM interface value type can not be resolved");
  }

  if (TK_INT == rtype->kind)
  {
    int64_t value = 0;
    if (!reader.ReadI64(value)) return false;
    rvalue = new OValueInt(atype, value);
  }
  else if (TK_BOOL == rtype->kind)
  {
    uint8_t value = 0;
    if (!reader.ReadU8(value)) return false;
    rvalue = new OValueBool(atype, value != 0);
  }
  else if (TK_FLOAT == rtype->kind)
  {
    uint64_t bits = 0;
    double value = 0.0;
    if (!reader.ReadU64(bits)) return false;
    static_assert(sizeof(bits) == sizeof(value));
    memcpy(&value, &bits, sizeof(value));
    rvalue = new OValueFloat(atype, value);
  }
  else if (TK_STRING == rtype->kind)
  {
    string value;
    if (!reader.ReadString(value)) return false;
    auto * cstrtype = dynamic_cast<OTypeCString *>(atype);
    uint32_t maxlen = (cstrtype ? cstrtype->maxlen : 0);
    auto * cvalue = new OValueCString(atype, maxlen);
    cvalue->value = value;
    rvalue = cvalue;
  }
  else if (TK_POINTER == rtype->kind)
  {
    uint64_t value = 0;
    if (!reader.ReadU64(value)) return false;
    if (value != 0)
    {
      return reader.Fail("Only null pointer values are supported in DQM interface loading");
    }
    rvalue = new OValuePointer(atype, true);
  }
  else if (TK_FUNCREF == rtype->kind)
  {
    uint64_t value = 0;
    if (!reader.ReadU64(value)) return false;
    if (value != 0)
    {
      return reader.Fail("Only null function reference values are supported in DQM interface loading");
    }
    rvalue = new OValueFuncRef(atype, true);
  }
  else
  {
    return reader.Fail(format("Unsupported DQM interface value type: {}", atype->name));
  }

  return true;
}

bool OModuleIntf::ReadTypeDecl(ODqmIfReader & reader)
{
  string declname;
  if (!reader.ReadString(declname) || !reader.NextRec())
  {
    return false;
  }

  OType * ptype = nullptr;
  if (!ReadTypeSpec(reader, ptype))
  {
    return false;
  }
  if (!reader.NextRec() || !reader.ExpectEmpty(DQMIF_TYPE_END))
  {
    return false;
  }

  return AddPublicType(new OTypeAlias(declname, ptype)) != nullptr;
}

bool OModuleIntf::ReadConstDecl(ODqmIfReader & reader)
{
  string declname;
  if (!reader.ReadString(declname) || !reader.NextRec())
  {
    return false;
  }

  OType * ptype = nullptr;
  if (!ReadTypeSpec(reader, ptype) || !reader.NextRec())
  {
    return false;
  }

  OValue * pvalue = nullptr;
  if (!ReadInlineValue(reader, ptype, pvalue))
  {
    return false;
  }
  if (!reader.NextRec() || !reader.ExpectEmpty(DQMIF_CONST_END))
  {
    delete pvalue;
    return false;
  }

  OScPosition scpos;
  return AddPublicValSym(new OValSymConst(scpos, declname, ptype, pvalue)) != nullptr;
}

bool OModuleIntf::ReadVarDecl(ODqmIfReader & reader)
{
  string declname;
  if (!reader.ReadString(declname) || !reader.NextRec())
  {
    return false;
  }

  SDqmIfAttributes attrs;
  if (!ReadAttributes(reader, attrs))
  {
    return false;
  }

  OType * ptype = nullptr;
  if (!ReadTypeSpec(reader, ptype))
  {
    return false;
  }
  if (!reader.NextRec() || !reader.ExpectEmpty(DQMIF_VAR_END))
  {
    return false;
  }

  OScPosition scpos;
  OValSym * vsym = new OValSym(scpos, declname, ptype);
  vsym->initialized = true;
  ApplyDqmIfAttributes(vsym, attrs);
  return AddPublicValSym(vsym) != nullptr;
}

bool OModuleIntf::ReadFunctionParam(ODqmIfReader & reader, OTypeFunc * asigtype)
{
  string paramname;
  if (!asigtype || !reader.ReadString(paramname))
  {
    return false;
  }

  EParamMode mode = FPM_VALUE;
  OType * ptype = nullptr;
  OValue * defvalue = nullptr;

  while (true)
  {
    if (!reader.NextRec())
    {
      delete defvalue;
      return false;
    }
    if (DQMIF_FUNC_PARAM_END == reader.recid)
    {
      if (!reader.ExpectEmpty(DQMIF_FUNC_PARAM_END))
      {
        delete defvalue;
        return false;
      }
      break;
    }
    else if (DQMIF_FUNC_PARAM_MODE_REF == reader.recid)
    {
      if (!reader.ExpectEmpty(DQMIF_FUNC_PARAM_MODE_REF)) { delete defvalue; return false; }
      mode = FPM_REF;
    }
    else if (DQMIF_FUNC_PARAM_MODE_REFIN == reader.recid)
    {
      if (!reader.ExpectEmpty(DQMIF_FUNC_PARAM_MODE_REFIN)) { delete defvalue; return false; }
      mode = FPM_REFIN;
    }
    else if (DQMIF_FUNC_PARAM_MODE_REFOUT == reader.recid)
    {
      if (!reader.ExpectEmpty(DQMIF_FUNC_PARAM_MODE_REFOUT)) { delete defvalue; return false; }
      mode = FPM_REFOUT;
    }
    else if (DQMIF_FUNC_PARAM_MODE_REFNULL == reader.recid)
    {
      if (!reader.ExpectEmpty(DQMIF_FUNC_PARAM_MODE_REFNULL)) { delete defvalue; return false; }
      mode = FPM_REFNULL;
    }
    else if ((DQMIF_TYPE_SPEC_SIMPLE == reader.recid) || (DQMIF_TYPE_SPEC_BEGIN == reader.recid)
             || (DQMIF_TYPE_SPEC_NAME == reader.recid) || (DQMIF_TYPE_SPEC_FUNCREF == reader.recid)
             || (DQMIF_TYPE_SPEC_OBJFUNCREF == reader.recid) || (DQMIF_TYPE_SPEC_PTR == reader.recid)
             || (DQMIF_TYPE_SPEC_ARRAY_BEGIN == reader.recid) || (DQMIF_TYPE_SPEC_SLICE_BEGIN == reader.recid))
    {
      if (!ReadTypeSpec(reader, ptype))
      {
        delete defvalue;
        return false;
      }
    }
    else if ((DQMIF_VALUE_INLINE == reader.recid) || (DQMIF_VALUE_LINKED == reader.recid))
    {
      if (!ptype)
      {
        delete defvalue;
        return reader.Fail(format("Parameter {} default value appears before its type", paramname));
      }
      if (!ReadInlineValue(reader, ptype, defvalue))
      {
        delete defvalue;
        return false;
      }
    }
    else
    {
      delete defvalue;
      return reader.Fail(format("Unexpected DQM interface function parameter record 0x{:04X}", reader.recid));
    }
  }

  if (!ptype)
  {
    delete defvalue;
    return reader.Fail(format("DQM interface parameter {} has no type", paramname));
  }

  OFuncParam * param = asigtype->AddParam(paramname, ptype, mode);
  if (defvalue)
  {
    OScPosition scpos;
    param->defvalue = new OValSymConst(scpos, paramname, ptype, defvalue);
  }
  return true;
}

bool OModuleIntf::AddLoadedFunction(OValSymFunc * afunc, bool aoverload, OCompoundType * aowner_type)
{
  if (!afunc)
  {
    return false;
  }

  if (aowner_type)
  {
    afunc->owner_compound_type = aowner_type;
    if (!aoverload)
    {
      aowner_type->Members()->DefineValSym(afunc);
      return true;
    }

    OValSym * existing = aowner_type->Members()->FindValSym(afunc->name, nullptr, false);
    OValSymOverloadSet * ovset = dynamic_cast<OValSymOverloadSet *>(existing);
    if (!ovset)
    {
      OScPosition scpos;
      ovset = new OValSymOverloadSet(scpos, afunc->name, g_builtins->type_func);
      ovset->owner_compound_type = aowner_type;
      aowner_type->Members()->DefineValSym(ovset);
    }
    ovset->AddFunc(afunc);
    return true;
  }

  if (!aoverload)
  {
    return AddPublicValSym(afunc) != nullptr;
  }

  OValSym * existing = scope_pub->FindValSym(afunc->name, nullptr, false);
  OValSymOverloadSet * ovset = dynamic_cast<OValSymOverloadSet *>(existing);
  if (!ovset)
  {
    OScPosition scpos;
    ovset = new OValSymOverloadSet(scpos, afunc->name, g_builtins->type_func);
    if (!AddPublicValSym(ovset))
    {
      return false;
    }
  }
  ovset->AddFunc(afunc);
  return true;
}

bool OModuleIntf::ReadFunctionDecl(ODqmIfReader & reader, OCompoundType * aowner_type, bool amethod)
{
  string declname;
  if (!reader.ReadString(declname) || !reader.NextRec())
  {
    return false;
  }

  SDqmIfAttributes attrs;
  if (!ReadAttributes(reader, attrs))
  {
    return false;
  }

  OTypeFunc * sigtype = new OTypeFunc("function_" + declname);
  while (true)
  {
    if ((amethod && (DQMIF_METHOD_END == reader.recid))
        || (!amethod && (DQMIF_FUNC_END == reader.recid)))
    {
      if (!reader.ExpectEmpty(amethod ? DQMIF_METHOD_END : DQMIF_FUNC_END))
      {
        delete sigtype;
        return false;
      }
      break;
    }
    else if (DQMIF_FUNC_RETVAL == reader.recid)
    {
      if (!reader.ExpectEmpty(DQMIF_FUNC_RETVAL) || !reader.NextRec()
          || !ReadTypeSpec(reader, sigtype->rettype) || !reader.NextRec())
      {
        delete sigtype;
        return false;
      }
    }
    else if (DQMIF_FUNC_PARAM_BEGIN == reader.recid)
    {
      if (!ReadFunctionParam(reader, sigtype) || !reader.NextRec())
      {
        delete sigtype;
        return false;
      }
    }
    else if (DQMIF_FUNC_PARAM_VARARGS == reader.recid)
    {
      if (!reader.ExpectEmpty(DQMIF_FUNC_PARAM_VARARGS) || !reader.NextRec())
      {
        delete sigtype;
        return false;
      }
      sigtype->has_varargs = true;
    }
    else
    {
      delete sigtype;
      return reader.Fail(format("Unexpected DQM interface function record 0x{:04X}", reader.recid));
    }
  }

  OScPosition scpos;
  OValSymFunc * fn = new OValSymFunc(scpos, declname, sigtype, nullptr);
  ApplyDqmIfAttributes(fn, attrs);
  fn->is_external = (attrs.flags & (1u << 6));
  fn->external_linkage_name = attrs.external_linkage_name;
  if (!fn->is_external)
  {
    fn->has_body = false;
  }

  return AddLoadedFunction(fn, fn->attr_is_overload, aowner_type);
}

bool OModuleIntf::ReadFieldDecl(ODqmIfReader & reader, OCompoundType * aowner_type)
{
  if (!aowner_type)
  {
    return false;
  }

  string fieldname;
  if (!reader.ReadString(fieldname) || !reader.NextRec())
  {
    return false;
  }

  SDqmIfAttributes attrs;
  if (!ReadAttributes(reader, attrs))
  {
    return false;
  }

  uint32_t offset = 0;
  if (DQMIF_FIELD_OFFSET == reader.recid)
  {
    int32_t signed_offset = 0;
    if (!reader.ReadI32(signed_offset) || (signed_offset < 0) || !reader.NextRec())
    {
      return reader.Fail(format("Invalid DQM interface field offset for {}", fieldname));
    }
    offset = uint32_t(signed_offset);
  }

  OType * ptype = nullptr;
  if (!ReadTypeSpec(reader, ptype) || !reader.NextRec())
  {
    return false;
  }

  if ((DQMIF_VALUE_INLINE == reader.recid) || (DQMIF_VALUE_LINKED == reader.recid))
  {
    OValue * ignored_value = nullptr;
    if (!ReadInlineValue(reader, ptype, ignored_value) || !reader.NextRec())
    {
      delete ignored_value;
      return false;
    }
    delete ignored_value;
  }

  if (!reader.ExpectEmpty(DQMIF_FIELD_END))
  {
    return false;
  }

  OScPosition scpos;
  OValSym * field = new OValSym(scpos, fieldname, ptype);
  field->initialized = true;
  field->field_offset = offset;
  ApplyDqmIfAttributes(field, attrs);
  aowner_type->AddMember(field);
  return true;
}

bool OModuleIntf::ReadCompoundDecl(ODqmIfReader & reader, bool ais_object)
{
  string declname;
  if (!reader.ReadString(declname))
  {
    return false;
  }

  OCompoundType * ctype = new OCompoundType(declname, scope_pub, ais_object);
  if (!reader.NextRec())
  {
    delete ctype;
    return false;
  }

  while (true)
  {
    if ((!ais_object && (DQMIF_STRUCT_END == reader.recid))
        || (ais_object && (DQMIF_OBJ_END == reader.recid)))
    {
      if (!reader.ExpectEmpty(ais_object ? DQMIF_OBJ_END : DQMIF_STRUCT_END))
      {
        delete ctype;
        return false;
      }
      break;
    }
    else if (DQMIF_SIZE_SPEC == reader.recid)
    {
      int32_t bytesize = 0;
      if (!reader.ReadI32(bytesize) || (bytesize < 0) || !reader.NextRec())
      {
        delete ctype;
        return reader.Fail(format("Invalid DQM interface size for {}", declname));
      }
      ctype->bytesize = uint32_t(bytesize);
    }
    else if (DQMIF_FIELD_BEGIN == reader.recid)
    {
      if (!ReadFieldDecl(reader, ctype) || !reader.NextRec())
      {
        delete ctype;
        return false;
      }
    }
    else if (ais_object && (DQMIF_METHOD_BEGIN == reader.recid))
    {
      if (!ReadFunctionDecl(reader, ctype, true) || !reader.NextRec())
      {
        delete ctype;
        return false;
      }
    }
    else
    {
      delete ctype;
      return reader.Fail(format("Unexpected DQM interface compound record 0x{:04X}", reader.recid));
    }
  }

  ctype->layout_ready = true;
  ctype->manual_ll_layout = true;
  return AddPublicType(ctype) != nullptr;
}

bool OModuleIntf::ReadDqmIfRecords(ODqmIfReader & reader)
{
  while (!reader.Eof())
  {
    if (!reader.NextRec())
    {
      return false;
    }

    if (DQMIF_H_BEGIN == reader.recid)
    {
      if (!reader.SkipGroup(DQMIF_H_BEGIN, DQMIF_H_END)) return false;
    }
    else if (DQMIF_LINKLIB == reader.recid)
    {
      string ignored;
      if (!reader.ReadString(ignored)) return false;
    }
    else if (DQMIF_USE_BEGIN == reader.recid)
    {
      if (!reader.SkipGroup(DQMIF_USE_BEGIN, DQMIF_USE_END)) return false;
    }
    else if (DQMIF_TYPE_BEGIN == reader.recid)
    {
      if (!ReadTypeDecl(reader)) return false;
    }
    else if (DQMIF_CONST_BEGIN == reader.recid)
    {
      if (!ReadConstDecl(reader)) return false;
    }
    else if (DQMIF_VAR_BEGIN == reader.recid)
    {
      if (!ReadVarDecl(reader)) return false;
    }
    else if (DQMIF_FUNC_BEGIN == reader.recid)
    {
      if (!ReadFunctionDecl(reader, nullptr, false)) return false;
    }
    else if (DQMIF_STRUCT_BEGIN == reader.recid)
    {
      if (!ReadCompoundDecl(reader, false)) return false;
    }
    else if (DQMIF_OBJ_BEGIN == reader.recid)
    {
      if (!ReadCompoundDecl(reader, true)) return false;
    }
    else
    {
      return reader.Fail(format("Unexpected top-level DQM interface record 0x{:04X}", reader.recid));
    }
  }

  return true;
}

bool OModuleIntf::ReadInterface(const string & filename)
{
  ODqmIfReader reader;
  if (!reader.ReadFromFile(filename) || !ReadDqmIfRecords(reader))
  {
    print("Can not read module interface file: {}\n{}\n", filename, reader.error);
    return false;
  }

  return true;
}

bool DumpModuleInterface(const string & filename)
{
  OModuleIntf intf(g_builtins, "dqm_if_dump");
  if (!intf.ReadInterface(filename))
  {
    return false;
  }

  print("DQ module interface dump: {}\n", filename);
  intf.WriteDump(cout);
  return true;
}
