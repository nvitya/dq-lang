/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_parser_expr.h
 * authors: nvitya
 * created: 2026-06-13
 * brief:   Expression parsing layer
 */

#pragma once

#include "dqc_ast.h"
#include "otype_enum.h"

class ODqCompParserExpr : public ODqCompAst
{
public: // expressions
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
  OExpr * ParseDynArrayMethod(OExpr * receiver_expr, OLValueExpr * receiver, const string & membername);
  OExpr * ParseCStringMethod(OExpr * receiver_expr, OLValueExpr * receiver, const string & membername);
  OExpr * ParseStringMethod(OExpr * receiver_expr, OLValueExpr * receiver, const string & membername);
  OExpr * ParseAnyValueMethod(OExpr * receiver_expr, OLValueExpr * receiver, const string & membername);
  OExpr * ParseExprPostfix();
  OExpr * ParseExprPrimary();
  OExpr * ParseExplicitCastExpr(bool * rattempted = nullptr);

  bool ParseCallArguments(const string & callname, OTypeFunc * tfunc, vector<OExpr *> & rargs);
  bool ParseRawCallArguments(const string & callname, vector<TRawCallArg> & rargs);
  OExpr * ParseExprFuncCall(OValSymFunc * vsfunc);
  OExpr * ParseExprMethodCall(OValSymFunc * vsfunc, OLValueExpr * receiver);
  OExpr * ParseExprOverloadCall(OValSymOverloadSet * ovset);
  OExpr * ParseExprMethodOverloadCall(OValSymOverloadSet * ovset, OLValueExpr * receiver);
  OExpr * ParseExprIndirectCall(OExpr * callee, OTypeFuncRef * calltype);
  OExpr * ParseNewExpr();
  OExpr * ParseInheritedExpr();
  OExpr * ParseBuiltinIif();
  OExpr * ParseBuiltinLen();
  OExpr * ParseBuiltinSizeof();
  OExpr * ParseBuiltinOffsetof();
  OExpr * ParseBuiltinFloatRound(ERoundMode amode);
  OExpr * ParseBuiltinTryCast();
  OExpr * ParseBuiltinTypeName();
  OExpr * ParseBuiltinOrd();
  OExpr * ParseArrayLit();

public: // type parsing
  OType * ParseTypeSpec(bool aemit_errors = true);  // parses type after ":" — handles ^T, [N]T, []T
  OTypeFunc * ParseFunctionType(bool aemit_errors = true, const string & aowner_name = "function");

public: // utility from parser that are needed early
  bool ParseParamModeKeyword(const string & sid, EParamMode & rmode);
  bool ParseAttributes(bool areset);

protected:
  struct BinOpEntry { const char * sym; EBinOp op; };
  OExpr * ParseBinOpLevel(OExpr * (ODqCompParserExpr::*parse_next)(),
                          const BinOpEntry ops[], int nops);
  bool    ParseFunctionSignature(OTypeFunc * tfunc, bool atypespec, const string & aowner_name, bool aemit_errors = true);

  bool    ParseAttributeBlock();
  bool    ParseSingleAttribute(const string & attrname);
  bool    ParseAttrIntArg(const string & attrname, int64_t & rvalue, bool positive_only = false);
  bool    ParseAttrStringArg(const string & attrname, string & rvalue);

  OValSym * ResolveNamespaceValSym();
  OExpr *   ParseEnumTypeExpr(OTypeEnum * enum_type);
  bool      IsKnownEnumItem(const string & item_name);
  OExpr *   ParseExprOverloadCallWithRawArgs(OValSymOverloadSet * ovset, vector<TRawCallArg> & rawargs);
  bool      CheckSpecialReservedRootName(const string & aname);
  bool      CheckStatementClose(bool emit_error = true);

public:
  OAttr * attr = nullptr;
};
