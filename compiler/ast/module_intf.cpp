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
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <print>
#include <typeinfo>

#include "comp_options.h"
#include "dqm_if.h"
#include "module_path.h"
#include "otype_func.h"
#include "otype_array.h"
#include "otype_int.h"
#include "otype_float.h"
#include "otype_bool.h"
#include "otype_cstring.h"
#include "otype_object.h"
#include "scope_builtins.h"
#include "artifact_lock.h"
#include "processrunner.h"

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

OModuleIntf::~OModuleIntf()
{
  for (OModuleIntf * intf : reexport_modules)
  {
    delete intf;
  }
  delete module_init_func;
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
  if (afunc && afunc->IsSpecial())
  {
    result += "*";
  }
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

      result += param->name;
      result += " : ";
      result += ParamModeText(param->mode);
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
  if (!atype->module)
  {
    atype->module = this;
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
  if (!avalsym->module)
  {
    avalsym->module = this;
  }

  OIntfDecl * result = new OIntfDecl(avalsym);
  declarations.push_back(result);
  return result;
}

string OModuleIntf::LinkerSymbolName(char atype_prefix, const string & symbol_name) const
{
  return LinkerSymbolNameForModule(atype_prefix, name, symbol_name);
}

string OModuleIntf::LinkerSymbolNameForModule(char atype_prefix, const string & module_name,
                                              const string & symbol_name)
{
  string result;
  result += atype_prefix;
  result += "dq__";

  auto append_part = [&](const string & text)
  {
    bool last_was_sep = false;
    for (char c : text)
    {
      bool keep = ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'))
                  || ((c >= '0') && (c <= '9')) || ('_' == c);
      if (keep)
      {
        result += c;
        last_was_sep = false;
      }
      else if (!last_was_sep)
      {
        result += "_";
        last_was_sep = true;
      }
    }
  };

  append_part(module_name);
  if (!result.ends_with("_"))
  {
    result += "_";
  }
  append_part(symbol_name);
  return result;
}

string OModuleIntf::DqmIfTargetArch() const
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

string OModuleIntf::DqmIfTargetRtl() const
{
#if defined(TARGET_WIN)
  return "win";
#elif defined(TARGET_LINUX)
  return "linux";
#else
  return "unknown";
#endif
}

string OModuleIntf::DqmIfBuildOptions() const
{
  string result = "O" + to_string(g_opt.optlevel) + ";linkmangle=1;module=" + name;
  if (g_opt.dbg_info)      result += ";g";

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

void OModuleIntf::ClearDqmIfMetadata()
{
  source_filename.clear();
  source_filesize = 0;
  source_filetime = 0;
  target_arch.clear();
  target_rtl.clear();
  build_options.clear();
  reexport_artifacts.clear();
  link_dependencies.clear();

  has_source_filename = false;
  has_source_filesize = false;
  has_source_filetime = false;
  has_target_arch = false;
  has_target_rtl = false;
  has_build_options = false;
}

bool OModuleIntf::ReadDqmIfHeaderMetadata(ODqmIfReader & reader)
{
  while (!reader.Eof())
  {
    if (!reader.NextRec())
    {
      return false;
    }

    if (DQMIF_H_BEGIN != reader.recid)
    {
      continue;
    }

    if (!reader.ExpectEmpty(DQMIF_H_BEGIN))
    {
      return false;
    }

    while (true)
    {
      if (!reader.NextRec())
      {
        return false;
      }

      if (DQMIF_H_END == reader.recid)
      {
        return reader.ExpectEmpty(DQMIF_H_END);
      }

      if (DQMIF_H_SRC_FILENAME == reader.recid)
      {
        if (!reader.ReadString(source_filename)) return false;
        has_source_filename = true;
      }
      else if (DQMIF_H_SRC_FILESIZE == reader.recid)
      {
        if (!reader.ReadI64(source_filesize)) return false;
        has_source_filesize = true;
      }
      else if (DQMIF_H_SRC_FILETIME == reader.recid)
      {
        if (!reader.ReadI64(source_filetime)) return false;
        has_source_filetime = true;
      }
      else if (DQMIF_H_TARGET_ARCH == reader.recid)
      {
        if (!reader.ReadString(target_arch)) return false;
        has_target_arch = true;
      }
      else if (DQMIF_H_TARGET_RTL == reader.recid)
      {
        if (!reader.ReadString(target_rtl)) return false;
        has_target_rtl = true;
      }
      else if (DQMIF_H_BUILD_OPTIONS == reader.recid)
      {
        if (!reader.ReadString(build_options)) return false;
        has_build_options = true;
      }
    }
  }

  return reader.Fail("DQM interface header metadata is missing");
}

bool OModuleIntf::ReadMetadata(const string & filename, string & rerror, bool alock)
{
  ClearDqmIfMetadata();
  rerror.clear();

  OArtifactLock lock;
  if (alock && !lock.Lock(filename, EArtifactLockMode::SHARED))
  {
    rerror = lock.error;
    return false;
  }

  ODqmIfReader reader;
  if (!reader.ReadFromArtifact(filename) || !ReadDqmIfHeaderMetadata(reader))
  {
    rerror = reader.error;
    return false;
  }

  return true;
}

bool OModuleIntf::MetadataMatchesCurrentBuild(string & rreason) const
{
  if (!has_target_arch || !has_target_rtl || !has_build_options)
  {
    rreason = "missing target or build option metadata";
    return false;
  }

  if (target_arch != DqmIfTargetArch())
  {
    rreason = format("target architecture changed: {} != {}", target_arch, DqmIfTargetArch());
    return false;
  }

  if (target_rtl != DqmIfTargetRtl())
  {
    rreason = format("target runtime changed: {} != {}", target_rtl, DqmIfTargetRtl());
    return false;
  }

  if (build_options != DqmIfBuildOptions())
  {
    rreason = format("build options changed: {} != {}", build_options, DqmIfBuildOptions());
    return false;
  }

  return true;
}

bool OModuleIntf::MetadataMatchesSource(const filesystem::path & source_path, string & rreason) const
{
  if (!has_source_filesize || !has_source_filetime)
  {
    rreason = "missing source freshness metadata";
    return false;
  }

  error_code ec;
  uintmax_t cur_source_filesize = filesystem::file_size(source_path, ec);
  if (ec)
  {
    rreason = format("can not read source file size: {}", source_path.string());
    return false;
  }

  if (source_filesize != int64_t(cur_source_filesize))
  {
    rreason = format("source file size changed: {} != {}", source_filesize, cur_source_filesize);
    return false;
  }

  ec.clear();
  auto cur_source_filetime = filesystem::last_write_time(source_path, ec);
  if (ec)
  {
    rreason = format("can not read source file time: {}", source_path.string());
    return false;
  }

  int64_t cur_source_filetime_ticks =
      int64_t(chrono::duration_cast<chrono::nanoseconds>(cur_source_filetime.time_since_epoch()).count());
  if (source_filetime != cur_source_filetime_ticks)
  {
    rreason = "source file modification time changed";
    return false;
  }

  return true;
}

bool OModuleIntf::CompiledArtifactIsFresh(const filesystem::path & artifact_path,
                                          const filesystem::path & source_path,
                                          string & rreason, bool alock)
{
  error_code ec;
  if (!filesystem::exists(artifact_path, ec) || ec)
  {
    rreason = "compiled module artifact is missing";
    return false;
  }

  string metadata_error;
  if (!ReadMetadata(artifact_path.string(), metadata_error, alock))
  {
    rreason = metadata_error;
    return false;
  }

  return MetadataMatchesSource(source_path, rreason) && MetadataMatchesCurrentBuild(rreason);
}

bool OModuleIntf::FindFreshInterfaceArtifact(const filesystem::path & interface_artifact_path,
                                             const filesystem::path & object_artifact_path,
                                             const filesystem::path & source_path,
                                             filesystem::path & rinterface_path,
                                             string & rreason)
{
  string object_reason;
  if (CompiledArtifactIsFresh(object_artifact_path, source_path, object_reason))
  {
    ArtifactCleanupInterfaceSidecarForObject(object_artifact_path);
    rinterface_path = object_artifact_path;
    rreason.clear();
    return true;
  }

  string interface_reason;
  if (CompiledArtifactIsFresh(interface_artifact_path, source_path, interface_reason))
  {
    rinterface_path = interface_artifact_path;
    rreason.clear();
    return true;
  }

  rinterface_path.clear();
  rreason = format("interface artifact: {}; compiled artifact: {}", interface_reason, object_reason);
  return false;
}

bool OModuleIntf::IsInModuleUseStack(const string & module_path) const
{
  return find(g_opt.module_use_stack.begin(), g_opt.module_use_stack.end(), module_path)
      != g_opt.module_use_stack.end();
}

string OModuleIntf::FormatModuleCycle(const string & module_path) const
{
  auto it = find(g_opt.module_use_stack.begin(), g_opt.module_use_stack.end(), module_path);
  string result;

  if (it == g_opt.module_use_stack.end())
  {
    for (const string & item : g_opt.module_use_stack)
    {
      if (!result.empty()) result += " -> ";
      result += item;
    }
  }
  else
  {
    for (; it != g_opt.module_use_stack.end(); ++it)
    {
      if (!result.empty()) result += " -> ";
      result += *it;
    }
  }

  if (!result.empty()) result += " -> ";
  result += module_path;
  return result;
}

static SModuleArtifactEnsureResult ModuleArtifactEnsureError(EModuleArtifactEnsureError error,
                                                             const string & reason = "")
{
  SModuleArtifactEnsureResult result;
  result.error = error;
  result.reason = reason;
  return result;
}

static bool ModuleSourceExists(const filesystem::path & source_path)
{
  error_code ec;
  return filesystem::exists(source_path, ec) && !ec;
}

static bool RunModuleChildCompile(const vector<string> & args, const string & stale_reason, string & rreason)
{
  OProcessRunner procrunner;
  procrunner.args = args;
  bool exec_ok = procrunner.Run();
  if (exec_ok && (0 == procrunner.exit_code))
  {
    return true;
  }

  if (!procrunner.stdout_text.empty())
  {
    print("{}", procrunner.stdout_text);
  }
  if (!procrunner.stderr_text.empty())
  {
    print("{}", procrunner.stderr_text);
  }

  rreason = format("subprocess exited with code {}", procrunner.exit_code);
  if (!stale_reason.empty())
  {
    rreason += format(" after stale artifact ({})", stale_reason);
  }
  return false;
}

SModuleArtifactEnsureResult OModuleIntf::EnsureFreshInterfaceArtifact(const OModulePath & module_path,
                                                                      bool in_module_stack)
{
  SModuleArtifactEnsureResult result;
  string stale_reason;

  auto interface_is_fresh = [&]() -> bool
  {
    if (in_module_stack)
    {
      if (CompiledArtifactIsFresh(module_path.interface_artifact_path, module_path.source_path, stale_reason))
      {
        result.interface_load_path = module_path.interface_artifact_path;
        return true;
      }
      return false;
    }

    return FindFreshInterfaceArtifact(module_path.interface_artifact_path, module_path.artifact_path,
                                      module_path.source_path, result.interface_load_path, stale_reason);
  };

  if (interface_is_fresh())
  {
    return result;
  }

  if (!ModuleSourceExists(module_path.source_path))
  {
    return ModuleArtifactEnsureError(EModuleArtifactEnsureError::SOURCE_MISSING);
  }

  string regen_reason;
  if (!RunModuleChildCompile(ChildInterfaceArgs(module_path.source_path, module_path.interface_artifact_path,
                                                module_path.module_id, module_path.root_dir),
                             stale_reason, regen_reason))
  {
    return ModuleArtifactEnsureError(EModuleArtifactEnsureError::REGEN_FAILED, regen_reason);
  }

  if (!interface_is_fresh())
  {
    return ModuleArtifactEnsureError(EModuleArtifactEnsureError::REGEN_FAILED, stale_reason);
  }

  return result;
}

SModuleArtifactEnsureResult OModuleIntf::EnsureFreshCompiledArtifact(const OModulePath & module_path)
{
  SModuleArtifactEnsureResult result;
  string stale_reason;

  if (CompiledArtifactIsFresh(module_path.artifact_path, module_path.source_path, stale_reason))
  {
    if (!FindFreshInterfaceArtifact(module_path.interface_artifact_path, module_path.artifact_path,
                                    module_path.source_path, result.interface_load_path, stale_reason))
    {
      return ModuleArtifactEnsureError(EModuleArtifactEnsureError::REGEN_FAILED, stale_reason);
    }
    return result;
  }

  if (!ModuleSourceExists(module_path.source_path))
  {
    return ModuleArtifactEnsureError(EModuleArtifactEnsureError::SOURCE_MISSING);
  }

  string regen_reason;
  if (!RunModuleChildCompile(ChildCompileArgs(module_path.source_path, module_path.artifact_path,
                                             module_path.module_id, module_path.root_dir),
                             stale_reason, regen_reason))
  {
    return ModuleArtifactEnsureError(EModuleArtifactEnsureError::REGEN_FAILED, regen_reason);
  }

  if (!CompiledArtifactIsFresh(module_path.artifact_path, module_path.source_path, stale_reason))
  {
    return ModuleArtifactEnsureError(EModuleArtifactEnsureError::REGEN_FAILED, stale_reason);
  }

  if (!FindFreshInterfaceArtifact(module_path.interface_artifact_path, module_path.artifact_path,
                                  module_path.source_path, result.interface_load_path, stale_reason))
  {
    return ModuleArtifactEnsureError(EModuleArtifactEnsureError::REGEN_FAILED, stale_reason);
  }

  error_code ec;
  if (!filesystem::exists(module_path.artifact_path, ec) || ec)
  {
    return ModuleArtifactEnsureError(EModuleArtifactEnsureError::ARTIFACT_MISSING);
  }

  return result;
}

static vector<string> ModuleChildArgs(const filesystem::path & source_path,
                                      const filesystem::path & artifact_path,
                                      const string & module_path,
                                      const filesystem::path & module_root_dir,
                                      bool interface_only)
{
  vector<string> args;
  args.push_back(g_opt.compiler_executable.empty() ? "dq-comp" : g_opt.compiler_executable);
  args.push_back(interface_only ? "--ifgen" : "-c");
  args.push_back(source_path.string());
  args.push_back("-o");
  args.push_back(artifact_path.string());
  args.push_back("--regen-if-stale");
  if (g_opt.no_use_sys || ("sys" == module_path))
  {
    args.push_back("--no-use-sys");
  }
  if (!g_opt.build_root_dir.empty())
  {
    args.push_back("--build-root");
    args.push_back(g_opt.build_root_dir);
  }
  if (!g_opt.build_tag.empty())
  {
    args.push_back("--build");
    args.push_back(g_opt.build_tag);
  }
  args.push_back("--mod-root");
  args.push_back(module_root_dir.string());
  args.push_back("--mod-name");
  args.push_back(module_path);
  args.push_back(format("-O{}", g_opt.optlevel));

  if (g_opt.dbg_info)
  {
    args.push_back("-g");
  }

  if (g_opt.verblevel > VERBLEVEL_NONE)
  {
    args.push_back(format("-v{}", g_opt.verblevel));
  }

  for (const OCmdLineDefine & def : g_opt.cmdline_defines)
  {
    string defarg = "-D" + def.name;
    if (def.has_bool_value)
    {
      defarg += "=";
      defarg += (def.bool_value ? "true" : "false");
    }
    else if (def.has_int_value)
    {
      defarg += "=";
      defarg += to_string(def.int_value);
    }
    args.push_back(defarg);
  }

  for (const string & package_path : g_opt.package_paths)
  {
    args.push_back("--pkg-path");
    args.push_back(package_path);
  }

  string stack_text;
  for (const string & item : g_opt.module_use_stack)
  {
    if (!stack_text.empty()) stack_text += ",";
    stack_text += item;
  }
  if (!stack_text.empty()) stack_text += ",";
  stack_text += module_path;

  args.push_back("--ifstack");
  args.push_back(stack_text);

  return args;
}

vector<string> OModuleIntf::ChildCompileArgs(const filesystem::path & source_path,
                                             const filesystem::path & artifact_path,
                                             const string & module_path,
                                             const filesystem::path & module_root_dir) const
{
  return ModuleChildArgs(source_path, artifact_path, module_path, module_root_dir, false);
}

vector<string> OModuleIntf::ChildInterfaceArgs(const filesystem::path & source_path,
                                               const filesystem::path & interface_artifact_path,
                                               const string & module_path,
                                               const filesystem::path & module_root_dir) const
{
  return ModuleChildArgs(source_path, interface_artifact_path, module_path, module_root_dir, true);
}

bool OModuleIntf::WriteDqmIfSourceMetadata(ODqmIfWriter & writer, const string & source_filename)
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

bool OModuleIntf::WriteDqmIfUse(ODqmIfWriter & writer, OModuleUse * ause)
{
  if (!ause || !ause->module || !ause->reexport || ause->is_private)
  {
    return true;
  }

  if (!writer.AddRecStr(DQMIF_USE_BEGIN, ause->module->name)) return false;
  if (!writer.AddRecEmpty(DQMIF_USE_REEXPORT)) return false;
  for (const string & name : ause->EffectiveSymbolNames())
  {
    if (!writer.AddRecStr(DQMIF_USE_ONLY, name)) return false;
  }
  return writer.AddRecEmpty(DQMIF_USE_END);
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

  vector<string> link_deps;
  auto add_link_dep = [&](const string & module_name) -> bool
  {
    if (module_name.empty() || (module_name == name))
    {
      return true;
    }
    if (link_deps.end() != find(link_deps.begin(), link_deps.end(), module_name))
    {
      return true;
    }
    link_deps.push_back(module_name);
    return writer.AddRecStr(DQMIF_LINKDEP, module_name);
  };

  for (OModuleUse * use : used_modules)
  {
    if (!use || !use->module)
    {
      continue;
    }
    if (!add_link_dep(use->module->name))
    {
      return false;
    }
    if (OModuleIntf * intf = dynamic_cast<OModuleIntf *>(use->module))
    {
      for (const string & dep_name : intf->link_dependencies)
      {
        if (!add_link_dep(dep_name))
        {
          return false;
        }
      }
    }
  }

  for (OModuleUse * use : used_modules)
  {
    if (!WriteDqmIfUse(writer, use))
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
         || (DQMIF_ATTR_EXT_LINK_NAME == reader.recid) || (DQMIF_ATTR_SECTION_NAME == reader.recid)
         || (DQMIF_ATTR_LINK_NAME == reader.recid))
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
    else if (DQMIF_ATTR_LINK_NAME == reader.recid)
    {
      if (!reader.ReadString(rattrs.linkage_name)) return false;
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
  avalsym->attr_has_linkage_name = (attrs.flags & (1u << 7));
  if (attrs.flags & (1u << 8))
  {
    if (auto * objsym = dynamic_cast<OVsObject *>(avalsym))
    {
      objsym->SetObjectStorage(OSK_OBJECT_REF);
    }
  }
  else if (attrs.flags & (1u << 9))
  {
    if (auto * objsym = dynamic_cast<OVsObject *>(avalsym))
    {
      objsym->SetObjectStorage(OSK_OBJECT_FIXED);
    }
  }
  if (attrs.flags & (1u << 10))
  {
    avalsym->member_visibility = MV_PRIVATE;
  }
  else if (attrs.flags & (1u << 11))
  {
    avalsym->member_visibility = MV_PROTECTED;
  }
  else
  {
    avalsym->member_visibility = MV_PUBLIC;
  }
  avalsym->attr_is_abstract = (attrs.flags & (1u << 12));
  avalsym->attr_is_final    = (attrs.flags & (1u << 13));
  avalsym->attr_linkage_name = attrs.linkage_name;
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

  SDqmIfAttributes attrs;
  if (!ReadAttributes(reader, attrs))
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
  OValSymConst * vsym = new OValSymConst(scpos, declname, ptype, pvalue);
  vsym->owner_module_name = name;
  ApplyDqmIfAttributes(vsym, attrs);
  return AddPublicValSym(vsym) != nullptr;
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
  OValSym * vsym = ptype->CreateValSym(scpos, declname);
  vsym->initialized = true;
  vsym->owner_module_name = name;
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
    afunc->generated_linkage_name = aowner_type->name + "." + afunc->name;
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
      ovset->generated_linkage_prefix = afunc->generated_linkage_name;
      ovset->member_visibility = afunc->member_visibility;
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
  ESpecialFuncKind special_kind = SFK_NONE;
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
    else if (DQMIF_FUNC_SPECIAL_KIND == reader.recid)
    {
      uint8_t kind = 0;
      if (!reader.ReadU8(kind) || !reader.NextRec())
      {
        delete sigtype;
        return false;
      }
      special_kind = ESpecialFuncKind(kind);
      if ((special_kind < SFK_NONE) || (special_kind > SFK_MODULE_INIT))
      {
        delete sigtype;
        return reader.Fail(format("Invalid special function kind {}", kind));
      }
    }
    else
    {
      delete sigtype;
      return reader.Fail(format("Unexpected DQM interface function record 0x{:04X}", reader.recid));
    }
  }

  OScPosition scpos;
  if (aowner_type)
  {
    sigtype->params.insert(sigtype->params.begin(), new OFuncParam("__this", aowner_type, FPM_REF));
  }
  OValSymFunc * fn = new OValSymFunc(scpos, declname, sigtype, nullptr);
  fn->owner_module_name = name;
  fn->special_kind = special_kind;
  ApplyDqmIfAttributes(fn, attrs);
  fn->is_external = (attrs.flags & (1u << 6));
  fn->external_linkage_name = attrs.external_linkage_name;
  if (!fn->is_external)
  {
    fn->has_body = false;
  }

  bool added = AddLoadedFunction(fn, fn->attr_is_overload, aowner_type);
  if (added && aowner_type)
  {
    if ("Create" == fn->name)
    {
      fn->object_specfunc_kind = OSF_CREATE;
      aowner_type->constructors.push_back(fn);
    }
    else if ("Destroy" == fn->name)
    {
      fn->object_specfunc_kind = OSF_DESTROY;
      aowner_type->destructor = fn;
    }
  }
  if (added && (SFK_MODULE_INIT == fn->special_kind))
  {
    module_init_func = fn;
    module_init_linkage_name = fn->GetLinkageName(true, 'F');
  }
  return added;
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
  OValSym * field = ptype->CreateValSym(scpos, fieldname);
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
    else if (ais_object && (DQMIF_OBJ_BASE == reader.recid))
    {
      string basename;
      if (!reader.ReadString(basename) || !reader.NextRec())
      {
        delete ctype;
        return false;
      }
      OType * basetype = scope_pub->FindType(basename);
      OCompoundType * baseobj = dynamic_cast<OCompoundType *>(basetype ? basetype->ResolveAlias() : nullptr);
      if (!baseobj || !baseobj->is_object)
      {
        delete ctype;
        return reader.Fail(format("Invalid DQM interface base object {} for {}", basename, declname));
      }
      ctype->base_type = baseobj;
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

  ctype->UpdateObjectInheritanceFlags();
  ctype->layout_ready = true;
  ctype->manual_ll_layout = true;
  return AddPublicType(ctype) != nullptr;
}

bool OModuleIntf::ReadUseDecl(ODqmIfReader & reader)
{
  string module_path;
  if (!reader.ReadString(module_path))
  {
    return false;
  }

  bool reexport = false;
  bool has_selection = false;
  vector<string> symbol_names;

  while (true)
  {
    if (!reader.NextRec())
    {
      return false;
    }
    if (DQMIF_USE_END == reader.recid)
    {
      break;
    }
    if (DQMIF_USE_ALIAS == reader.recid)
    {
      string ignored_alias;
      if (!reader.ReadString(ignored_alias)) return false;
    }
    else if (DQMIF_USE_ONLY == reader.recid)
    {
      string name;
      if (!reader.ReadString(name)) return false;
      has_selection = true;
      symbol_names.push_back(name);
    }
    else if (DQMIF_USE_REEXPORT == reader.recid)
    {
      if (!reader.ExpectEmpty(DQMIF_USE_REEXPORT)) return false;
      reexport = true;
    }
    else
    {
      return reader.Fail(format("Unexpected DQM interface use record 0x{:04X}", reader.recid));
    }
  }

  if (!reexport)
  {
    return true;
  }

  filesystem::path artifact_path;
  if (!OModulePath::ResolveCanonicalArtifact(module_path, name, interface_filename, artifact_path))
  {
    return reader.Fail(format("Can not resolve reexported module artifact: {}", module_path));
  }

  OModuleIntf * intf = new OModuleIntf(scope_pub->parent_scope, module_path);
  if (!intf->ReadInterface(artifact_path.string()))
  {
    delete intf;
    return reader.Fail(format("Can not load reexported module interface: {}", artifact_path.string()));
  }

  EModuleUseMergeMode merge_mode = (has_selection ? MUM_ONLY : MUM_ALL);
  OModuleUse * use = new OModuleUse(intf, false, merge_mode, symbol_names, true);
  if (has_selection)
  {
    for (const string & name : symbol_names)
    {
      if (!intf->scope_pub->FindType(name, nullptr, false)
          && !intf->scope_pub->FindValSym(name, nullptr, false))
      {
        delete use;
        delete intf;
        return reader.Fail(format("Reexported module \"{}\" has no public symbol \"{}\"", module_path, name));
      }
    }
  }

  for (const string & name : use->EffectiveSymbolNames())
  {
    if (OType * type = intf->scope_pub->FindType(name, nullptr, false))
    {
      auto found = scope_pub->typesyms.find(type->name);
      if (found != scope_pub->typesyms.end() && found->second != type)
      {
        delete use;
        delete intf;
        return reader.Fail(format("Reexported type \"{}\" conflicts in module \"{}\"", type->name, this->name));
      }
      if (found == scope_pub->typesyms.end())
      {
        scope_pub->typesyms[type->name] = type;
        declarations.push_back(new OIntfDecl(type));
      }
    }

    if (OValSym * vs = intf->scope_pub->FindValSym(name, nullptr, false))
    {
      auto found = scope_pub->valsyms.find(vs->name);
      if (found != scope_pub->valsyms.end() && found->second != vs)
      {
        delete use;
        delete intf;
        return reader.Fail(format("Reexported symbol \"{}\" conflicts in module \"{}\"", vs->name, this->name));
      }
      if (found == scope_pub->valsyms.end())
      {
        scope_pub->valsyms[vs->name] = vs;
        declarations.push_back(new OIntfDecl(vs));
      }
    }
  }

  string artifact_name = artifact_path.string();
  if (reexport_artifacts.end() == find(reexport_artifacts.begin(), reexport_artifacts.end(), artifact_name))
  {
    reexport_artifacts.push_back(artifact_name);
  }
  for (const string & child_artifact : intf->reexport_artifacts)
  {
    if (reexport_artifacts.end() == find(reexport_artifacts.begin(), reexport_artifacts.end(), child_artifact))
    {
      reexport_artifacts.push_back(child_artifact);
    }
  }

  reexport_modules.push_back(intf);
  used_modules.push_back(use);
  return true;
}

bool OModuleIntf::ReadModuleInitDecl(ODqmIfReader & reader)
{
  if (!reader.ReadString(module_init_linkage_name))
  {
    return false;
  }

  OScPosition scpos;
  OTypeFunc * sigtype = new OTypeFunc("__dq_module_init");
  module_init_func = new OValSymFunc(scpos, "__dq_module_init", sigtype, scope_pub->parent_scope);
  module_init_func->attr_has_linkage_name = true;
  module_init_func->attr_linkage_name = module_init_linkage_name;
  module_init_func->owner_module_name = name;
  return true;
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
    else if (DQMIF_LINKDEP == reader.recid)
    {
      string dep_name;
      if (!reader.ReadString(dep_name)) return false;
      if (link_dependencies.end() == find(link_dependencies.begin(), link_dependencies.end(), dep_name))
      {
        link_dependencies.push_back(dep_name);
      }
    }
    else if (DQMIF_MODULE_INIT == reader.recid)
    {
      if (!ReadModuleInitDecl(reader)) return false;
    }
    else if (DQMIF_USE_BEGIN == reader.recid)
    {
      if (!ReadUseDecl(reader)) return false;
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
  return ReadInterface(filename, true);
}

bool OModuleIntf::ReadInterface(const string & filename, bool alock, bool aquiet)
{
  ClearDqmIfMetadata();
  interface_filename = filename;
  OArtifactLock lock;
  if (alock && !lock.Lock(filename, EArtifactLockMode::SHARED))
  {
    if (!aquiet)
    {
      print("Can not read module interface artifact: {}\n{}\n", filename, lock.error);
    }
    return false;
  }

  ODqmIfReader reader;
  if (!reader.ReadFromArtifact(filename) || !ReadDqmIfRecords(reader))
  {
    if (!aquiet)
    {
      print("Can not read module interface artifact: {}\n{}\n", filename, reader.error);
    }
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
