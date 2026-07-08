/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_parser.h
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#pragma once

#include "stdint.h"
#include "otype_compound.h"
#include <filesystem>
#include <string>
#include "comp_options.h"
#include "statements.h"
#include "errorcodes.h"
#include "dqc_parser_stmt.h"
#include "module_path.h"

using namespace std;

class OTypeFunc;
class OTypeFuncRef;
class OTypeObject;

class ODqCompParser : public ODqCompParserStmt
{
private:
  using             super = ODqCompParserStmt;

public:
  ODqCompParser();
  virtual ~ODqCompParser();

  void ParseModule();

public: // root level items
  bool ParseUseModulePath(OModulePath & rpath);
  void ParseUseStatement();
  bool ParseUseSymbolList(const string & amodifier, vector<string> & rsymbol_names);
  void ParseRootTypeDecl();
  void ParseEnumDecl();
  void ParseFunction();
  void ParseStructDecl();
  void ParseObjectDecl();

public: // utility
  filesystem::path CurrentSourcePath() const;
  OValSymConst * ParseDefineConst(const OScPosition & scpos, const string & sid);
  bool ParseDefineCondition(const OScPosition & scpos, bool * rok = nullptr);

protected:
  void    ParseCompoundBlockStart(const string & end_keyword, string & rblock_closer);
  bool    CheckCompoundBlockEnd(const string & block_closer);
  void    RecoverFailedFunctionDecl();
  bool    FinishFunctionDecl(OValSymFunc * vsfunc, OScope * decl_scope, OScope * body_parent_scope,
                             bool ahidden_decl, bool aallow_external, const string & aowner_desc);
  void    ParseQualifiedObjectFunction(const string & object_name);
  bool    ReadCompoundMethod(OCompoundType * compound_type, EMemberVisibility avisibility);
  bool    ReadObjectProperty(OTypeObject * object_type, EMemberVisibility avisibility);
};
