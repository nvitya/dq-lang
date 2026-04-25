/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_parser.h
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#pragma once

#include "stdint.h"
#include <string>
#include "comp_options.h"
#include "statements.h"
#include "errorcodes.h"
#include "dqc_ast.h"

using namespace std;

class OTypeFunc;
class OTypeFuncRef;

class ODqCompParser : public ODqCompAst
{
private:
  using             super = ODqCompAst;


public:
  ODqCompParser();
  virtual ~ODqCompParser();

  void ParseModule();

public: // root level items
  void ParseStmtConst(bool arootstmt);  // used for statement blocks too
  void ParseRootTypeDecl();
  void ParseFunction();
  void ParseStructDecl();
  void ParseObjectDecl();

  void ParseStmtVar(bool arootstmt);  // used for statement blocks too
  void ParseStmtRef();

public: // statement blocks
  void ReadStatementBlock(OStmtBlock * stblock, const string blockend, string * rendstr = nullptr);

  bool FinalizeStmtAssign(OLValueExpr * leftexpr, EBinOp op, OExpr * rightexpr);
  void ParseStmtReturn();
  void ParseStmtWhile();
  void ParseStmtIf();
  void FinalizeStmtVoidCall(OExpr * callexpr);

  EBinOp ParseAssignOp();

public: // type parsing
  OType * ParseTypeSpec(bool aemit_errors = true);  // parses type after ":" — handles ^, [N], []
  OTypeFunc * ParseFunctionType(bool aemit_errors = true, const string & aowner_name = "function");

public: // utility
  bool ParseAttributes(bool areset);
  bool CheckStatementClose();
  OValSymConst * ParseDefineConst(const OScPosition & scpos, const string & sid);
  bool ParseDefineCondition(const OScPosition & scpos, bool * rok = nullptr);
  OValSym * ResolveNamespaceValSym();

public: // expressions
  OStmtBlock *  curblock = nullptr;
  OScope *      curscope = nullptr;

  bool          supress_varinit_check = false;  // do not emit unititalized variable errors (for left value expression parsing)

  struct TSuppressedVarInitDiag
  {
    OLValueVar *  varexpr = nullptr;
    OValSym *     valsym = nullptr;
    OScPosition   scpos;
  };

  struct TRawCallArg
  {
    OExpr *                        expr = nullptr;
    OScPosition                    scpos_start;
    vector<TSuppressedVarInitDiag> init_diags;
  };

  vector<TSuppressedVarInitDiag>  suppressed_varinit_diags;

  OExpr * ParseExpression(); // calls ParseExprOr()

  // Expression parsing in increasing operator priority:
  OExpr * ParseExprOr();
  OExpr * ParseExprAnd();
  OExpr * ParseExprNot();
  OExpr * ParseComparison();
  OExpr * ParseExprAdd();
  OExpr * ParseExprMul();
  OExpr * ParseExprDiv();
  OExpr * ParseExprBinOr();
  OExpr * ParseExprBinAnd();
  OExpr * ParseExprShift();
  OExpr * ParseExprUnary();
  OLValueExpr * ParseAddressableExpr();
  OExpr * ParsePostfix(OExpr * base);
  OExpr * ParseExprPostfix();
  OExpr * ParseExprPrimary();
  OExpr * ParseExplicitCastExpr(bool * rattempted = nullptr);

  bool ParseCallArguments(const string & callname, OTypeFunc * tfunc, vector<OExpr *> & rargs);
  bool ParseRawCallArguments(const string & callname, vector<TRawCallArg> & rargs);
  void FreeRawCallArguments(vector<TRawCallArg> & rawargs);
  void EmitStoredVarInitDiags(const vector<TSuppressedVarInitDiag> & diags);
  bool BindCallArguments(const string & callname, OTypeFunc * tfunc, vector<TRawCallArg> & rawargs, vector<OExpr *> & rargs);
  OExpr * ParseExprFuncCall(OValSymFunc * vsfunc);
  OExpr * ParseExprMethodCall(OValSymFunc * vsfunc, OLValueExpr * receiver);
  OExpr * ParseExprOverloadCall(OValSymOverloadSet * ovset);
  OExpr * ParseExprMethodOverloadCall(OValSymOverloadSet * ovset, OLValueExpr * receiver);
  OExpr * ParseExprIndirectCall(OExpr * callee, OTypeFuncRef * calltype);
  OExpr * ParseBuiltinIif();
  OExpr * ParseBuiltinLen();
  OExpr * ParseBuiltinSizeof();
  OExpr * ParseBuiltinFloatRound(ERoundMode amode);
  OExpr * ParseArrayLit();

protected:
  struct BinOpEntry { const char * sym; EBinOp op; };
  OExpr * ParseBinOpLevel(OExpr * (ODqCompParser::*parse_next)(),
                          const BinOpEntry ops[], int nops);
  bool    ParseFunctionSignature(OTypeFunc * tfunc, bool atypespec, const string & aowner_name, bool aemit_errors = true);

  bool    ParseAttributeBlock();
  bool    ParseSingleAttribute(const string & attrname);
  bool    ParseAttrIntArg(const string & attrname, int64_t & rvalue, bool positive_only = false);
  bool    ParseAttrStringArg(const string & attrname, string & rvalue);
  void    RecoverFailedFunctionDecl();
  bool    FinishFunctionDecl(OValSymFunc * vsfunc, OScope * decl_scope, OScope * body_parent_scope,
                             bool ahidden_decl, bool aallow_external, const string & aowner_desc);
  void    InjectObjectReceiver(OValSymFunc * vsfunc, OCompoundType * ctype);
  void    ParseQualifiedObjectFunction(const string & object_name);
  OExpr * ParseExprOverloadCallWithRawArgs(OValSymOverloadSet * ovset, vector<TRawCallArg> & rawargs);
  OExpr * CreateImplicitObjectMemberExpr(const string & sid, OValSym * vs, OScope * found_scope);
  bool    ReadObjectMethod(OCompoundType * ctype);

  void    VarInitError(OLValueVar * varexpr, OValSym * valsym, OScPosition & scpos);
  void    AddSuppressedVarInitDiag(OLValueVar * varexpr, OValSym * valsym, OScPosition & scpos);
  void    EmitSuppressedVarInitDiags();
  void    EmitFilteredAssignVarInitDiags(OLValueExpr * leftexpr, EBinOp op);

public:
  OAttr * attr = nullptr;
};
