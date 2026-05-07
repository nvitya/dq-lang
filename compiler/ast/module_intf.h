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
#include <format>
#include <vector>
#include <ostream>
#include <filesystem>
#include "dqm_if.h"
#include "symbols.h"

using namespace std;

enum EIntfDeclKind
{
  IDK_TYPE,
  IDK_VALSYM
};

class OIntfDecl
{
public:
  EIntfDeclKind  kind;

  union
  {
    OType *      ptype;
    OValSym *    pvalsym;
  };

  OIntfDecl(OType * atype)
  :
    kind(IDK_TYPE),
    ptype(atype)
  {
  }

  OIntfDecl(OValSym * avalsym)
  :
    kind(IDK_VALSYM),
    pvalsym(avalsym)
  {
  }
};

class OValSymFunc;
class OValSymOverloadSet;
class OTypeFunc;

class OModuleIntf
{
private:
  struct SDqmIfAttributes
  {
    uint64_t  flags = 0;
    uint32_t  align = 0;
    string    external_linkage_name;
    string    section_name;
  };

  bool ReadDqmIfRecords(ODqmIfReader & reader);
  bool ReadTypeSpec(ODqmIfReader & reader, OType *& rtype);
  bool ReadTypeSpecInner(ODqmIfReader & reader, OType *& rtype, TDqmIfRecId aend_recid);
  bool ReadTypeDecl(ODqmIfReader & reader);
  bool ReadConstDecl(ODqmIfReader & reader);
  bool ReadVarDecl(ODqmIfReader & reader);
  bool ReadFunctionDecl(ODqmIfReader & reader, OCompoundType * aowner_type, bool amethod);
  bool ReadFunctionParam(ODqmIfReader & reader, OTypeFunc * asigtype);
  bool ReadCompoundDecl(ODqmIfReader & reader, bool ais_object);
  bool ReadFieldDecl(ODqmIfReader & reader, OCompoundType * aowner_type);
  bool ReadAttributes(ODqmIfReader & reader, SDqmIfAttributes & rattrs);
  bool ReadInlineValue(ODqmIfReader & reader, OType * atype, OValue *& rvalue);
  bool ApplyDqmIfAttributes(OValSym * avalsym, const SDqmIfAttributes & attrs);
  bool AddLoadedFunction(OValSymFunc * afunc, bool aoverload, OCompoundType * aowner_type);
  OType * ResolveDqmIfTypeName(const string & atype_name);
  void ClearDqmIfMetadata();
  bool ReadDqmIfHeaderMetadata(ODqmIfReader & reader);
  string DqmIfTargetArch() const;
  string DqmIfTargetRtl() const;
  string DqmIfBuildOptions() const;
  bool WriteDqmIfSourceMetadata(ODqmIfWriter & writer, const string & source_filename);
  bool WriteInterfaceRecords(ODqmIfWriter & writer, const string & source_filename);

  void WriteTypeDump(ostream & out, OType * atype, const string & indent);
  void WriteValSymDump(ostream & out, OValSym * avsym, const string & indent);
  void WriteFunctionDump(ostream & out, OValSymFunc * afunc, const string & indent);
  void WriteOverloadSetDump(ostream & out, OValSymOverloadSet * aovset, const string & indent);
  void WriteCompoundDump(ostream & out, OCompoundType * atype, const string & indent);

public:
  string           name;
  OScope *         scope_pub;
  vector<OIntfDecl *>  declarations;

  string   source_filename;
  int64_t  source_filesize = 0;
  int64_t  source_filetime = 0;
  string   target_arch;
  string   target_rtl;
  string   build_options;

  bool     has_source_filename = false;
  bool     has_source_filesize = false;
  bool     has_source_filetime = false;
  bool     has_target_arch = false;
  bool     has_target_rtl = false;
  bool     has_build_options = false;

  OModuleIntf(OScope * aparent, const string aname)
  :
    name(aname)
  {
    scope_pub = new OScope(aparent, aname);
  }

  virtual ~OModuleIntf()
  {
    for (OIntfDecl * decl : declarations)
    {
      delete decl;
    }
    delete scope_pub;
  }

  OIntfDecl * AddPublicType(OType * atype);
  OIntfDecl * AddPublicValSym(OValSym * avalsym);

  bool BuildInterfaceBytes(vector<uint8_t> & rdata, const string & source_filename);
  bool WriteInterface(const string & filename, const string & source_filename);
  bool ReadInterface(const string & filename);
  bool ReadMetadata(const string & filename, string & rerror);
  bool MetadataMatchesCurrentBuild(string & rreason) const;
  bool MetadataMatchesSource(const filesystem::path & source_path, string & rreason) const;
  bool CompiledArtifactIsFresh(const filesystem::path & artifact_path, const filesystem::path & source_path,
                               string & rreason);
  bool IsInModuleUseStack(const string & module_path) const;
  string FormatModuleCycle(const string & module_path) const;
  vector<string> ChildCompileArgs(const filesystem::path & source_path, const filesystem::path & artifact_path,
                                  const string & module_path) const;
  void WriteDump(ostream & out);
};

bool DumpModuleInterface(const string & filename);
