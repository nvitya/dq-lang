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

#pragma once

#include <string>
#include "otype_compound.h"
#include <format>
#include <vector>
#include <ostream>
#include <filesystem>
#include "dqm_if.h"
#include "symbols.h"

using namespace std;

class OValSymFunc;
class OValSymOverloadSet;
class OTypeFunc;
class OTypeEnum;
class OModulePath;

enum class EModuleArtifactEnsureError
{
  NONE,
  SOURCE_MISSING,
  REGEN_FAILED,
  ARTIFACT_MISSING
};

struct SModuleArtifactEnsureResult
{
  EModuleArtifactEnsureError  error = EModuleArtifactEnsureError::NONE;
  string                      reason;
  filesystem::path            interface_load_path;

  bool Ok() const { return EModuleArtifactEnsureError::NONE == error; }
};

class OModuleIntf : public OModuleBase
{
private:
  typedef OModuleBase  super;

  struct SDqmIfAttributes
  {
    uint64_t  flags = 0;
    uint32_t  align = 0;
    string    external_linkage_name;
    string    linkage_name;
    string    section_name;
  };

  bool ReadDqmIfRecords(ODqmIfReader & reader);
  bool ReadUseDecl(ODqmIfReader & reader);
  bool ReadTypeSpec(ODqmIfReader & reader, OType *& rtype);
  bool ReadTypeSpecInner(ODqmIfReader & reader, OType *& rtype, TDqmIfRecId aend_recid);
  bool ReadFunctionRefTypeSpec(ODqmIfReader & reader, bool aobject_ref, OType *& rtype);
  bool ReadTypeDecl(ODqmIfReader & reader);
  bool ReadEnumDecl(ODqmIfReader & reader);
  bool ReadConstDecl(ODqmIfReader & reader);
  bool ReadVarDecl(ODqmIfReader & reader);
  bool ReadFunctionDecl(ODqmIfReader & reader, OCompoundType * aowner_type, bool amethod);
  bool ReadFunctionParam(ODqmIfReader & reader, OTypeFunc * asigtype);
  bool ReadCompoundDecl(ODqmIfReader & reader, bool ais_object);
  bool ReadFieldDecl(ODqmIfReader & reader, OCompoundType * aowner_type);
  bool ReadPropertyDecl(ODqmIfReader & reader, OTypeObject * aowner_type);
  bool ReadAttributes(ODqmIfReader & reader, SDqmIfAttributes & rattrs);
  bool ReadInlineValue(ODqmIfReader & reader, OType * atype, OValue *& rvalue);
  bool ApplyDqmIfAttributes(OValSym * avalsym, const SDqmIfAttributes & attrs);
  bool AddLoadedFunction(OValSymFunc * afunc, bool aoverload, OCompoundType * aowner_type);
  OType * ResolveDqmIfTypeName(const string & atype_name);
  string FormatUnresolvedDqmIfTypeError(const string & atype_name) const;
  void ClearDqmIfMetadata();
  bool ReadDqmIfHeaderMetadata(ODqmIfReader & reader);
  string DqmIfTargetArch() const;
  string DqmIfTargetRtl() const;
  string DqmIfBuildOptions() const;
  bool WriteDqmIfSourceMetadata(ODqmIfWriter & writer, const string & source_filename);
  bool WriteDqmIfUse(ODqmIfWriter & writer, OModuleUse * ause);
  bool WriteInterfaceRecords(ODqmIfWriter & writer, const string & source_filename);
  bool ReadModuleInitDecl(ODqmIfReader & reader);
  vector<OModuleIntf *> reexport_modules;

  void WriteTypeDump(ostream & out, OType * atype, const string & indent);
  void WriteValSymDump(ostream & out, OValSym * avsym, const string & indent);
  void WriteFunctionDump(ostream & out, OValSymFunc * afunc, const string & indent);
  void WriteOverloadSetDump(ostream & out, OValSymOverloadSet * aovset, const string & indent);
  void WriteCompoundDump(ostream & out, OCompoundType * atype, const string & indent);

public:
  string   source_filename;
  int64_t  source_filesize = 0;
  int64_t  source_filetime = 0;
  string   target_arch;
  string   target_rtl;
  string   build_options;
  string   interface_filename;
  vector<string> reexport_artifacts;
  vector<string> link_dependencies;
  string module_init_linkage_name;
  string last_interface_error;
  OValSymFunc * module_init_func = nullptr;

  bool     has_source_filename = false;
  bool     has_source_filesize = false;
  bool     has_source_filetime = false;
  bool     has_target_arch = false;
  bool     has_target_rtl = false;
  bool     has_build_options = false;

  OModuleIntf(OScope * aparent, const string aname)
  :
    super(aparent, aname)
  {
  }

  virtual ~OModuleIntf();

  OIntfDecl * AddPublicType(OType * atype);
  OIntfDecl * AddPublicValSym(OValSym * avalsym);
  string LinkerSymbolName(char atype_prefix, const string & symbol_name) const;
  static string LinkerSymbolNameForModule(char atype_prefix, const string & module_name,
                                          const string & symbol_name);

  bool BuildInterfaceBytes(vector<uint8_t> & rdata, const string & source_filename);
  bool WriteInterface(const string & filename, const string & source_filename);
  bool ReadInterface(const string & filename);
  bool ReadInterface(const string & filename, bool alock, bool aquiet = false);
  bool ReadMetadata(const string & filename, string & rerror, bool alock = true);
  bool MetadataMatchesCurrentBuild(string & rreason) const;
  bool MetadataMatchesSource(const filesystem::path & source_path, string & rreason) const;
  bool CompiledArtifactIsFresh(const filesystem::path & artifact_path, const filesystem::path & source_path,
                               string & rreason, bool alock = true);
  bool FindFreshInterfaceArtifact(const filesystem::path & interface_artifact_path,
                                  const filesystem::path & object_artifact_path,
                                  const filesystem::path & source_path,
                                  filesystem::path & rinterface_path,
                                  string & rreason);
  bool IsInModuleUseStack(const string & module_path) const;
  string FormatModuleCycle(const string & module_path) const;
  SModuleArtifactEnsureResult EnsureFreshInterfaceArtifact(const OModulePath & module_path,
                                                           bool in_module_stack);
  SModuleArtifactEnsureResult EnsureFreshCompiledArtifact(const OModulePath & module_path);
  vector<string> ChildCompileArgs(const filesystem::path & source_path, const filesystem::path & artifact_path,
                                  const string & module_path, const filesystem::path & module_root_dir) const;
  vector<string> ChildInterfaceArgs(const filesystem::path & source_path,
                                    const filesystem::path & interface_artifact_path,
                                    const string & module_path,
                                    const filesystem::path & module_root_dir) const;
  void WriteDump(ostream & out);
};

bool DumpModuleInterface(const string & filename);
