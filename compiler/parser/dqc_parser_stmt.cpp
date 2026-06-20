/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_parser_stmt.cpp
 */

#include "dqc_parser_stmt.h"
#include "scf_dq.h"
#include "symbols.h"
#include "dq_module.h"
#include "dqc.h"
#include "named_scopes.h"
#include "otype_compound.h"
#include "otype_array.h"
#include <ranges>

static bool EnsureExceptionRtlUse()
{
  if (g_namespaces.end() != g_namespaces.find("__dq_exception"))
  {
    return true;
  }
  return g_compiler->AddImplicitUse("rtl/exception", "__dq_exception", g_module->scope_pub, false, MUM_ALL);
}

static OTypeObject * ExceptionBaseType(OScope * scope)
{
  OType * type = scope ? scope->FindType("Exception") : nullptr;
  return dynamic_cast<OTypeObject *>(type ? type->ResolveAlias() : nullptr);
}

static bool IsExceptionType(OTypeObject * type, OScope * scope)
{
  OTypeObject * base = ExceptionBaseType(scope);
  return type && base && type->IsSameOrDerivedFrom(base);
}

static OTypeObject * ExceptionObjectTypeFromExpr(OExpr * expr)
{
  OType * type = expr ? expr->ResolvedType() : nullptr;
  type = type ? type->ResolveAlias() : nullptr;
  if (auto * object_type = dynamic_cast<OTypeObject *>(type))
  {
    return object_type;
  }
  if (auto * ptrtype = dynamic_cast<OTypePointer *>(type))
  {
    return dynamic_cast<OTypeObject *>(ptrtype->basetype ? ptrtype->basetype->ResolveAlias() : nullptr);
  }
  return nullptr;
}


void ODqCompParserStmt::ParseStmtVar(bool arootstmt)
{
  // syntax form: "var identifier : type [ = expression];"
  // note: "var" is already consumed

  string     sid;
  string     stype;
  OValSym *  pvalsym;
  OType *    ptype = nullptr;

  scf->SkipWhite();
  if (not scf->ReadIdentifier(sid))
  {
    StatementError(DQERR_ID_EXP_AFTER, "var");
    return;
  }
  if (arootstmt && !CheckSpecialReservedRootName(sid))
  {
    return;
  }

  pvalsym = curscope->FindValSym(sid, nullptr, false);  // do not search in the parent scopes this time !
  if (pvalsym)
  {
    StatementError(DQERR_VS_ALREADY_DECL_TYPE, sid, pvalsym->ptype->name, &scf->prevpos);
    return;
  }

  OExpr * initexpr = nullptr;
  bool zero_init = false;
  bool fixed_object = false;
  bool fixed_ctor_call_at_decl = false;
  vector<OExpr *> fixed_ctor_args;
  scf->SkipWhite();
  if (scf->CheckSymbol("<-"))
  {
    fixed_object = true;
    ptype = ParseTypeSpec();
    if (not ptype)
    {
      SkipToModuleStatementStart();
      return;
    }

    OTypeObject * object_type = dynamic_cast<OTypeObject *>(ptype->ResolveAlias());
    if (!object_type)
    {
      StatementError(DQERR_TYPE_EXPECTED, "object", ptype->name);
      return;
    }

    scf->SkipWhite();
    if (scf->CheckSymbol("("))
    {
      fixed_ctor_call_at_decl = true;
      vector<TRawCallArg> rawargs;
      if (!ParseRawCallArguments(sid, rawargs))
      {
        return;
      }
      for (TRawCallArg & rawarg : rawargs)
      {
        fixed_ctor_args.push_back(rawarg.expr);
        rawarg.expr = nullptr;
      }
      FreeRawCallArguments(rawargs);
    }
  }
  else if (scf->CheckSymbol(":"))
  {
    ptype = ParseTypeSpec();
    if (not ptype)
    {
      SkipToModuleStatementStart();
      return;
    }
  }
  else if (!arootstmt && scf->CheckSymbol("="))
  {
    scf->SkipWhite();
    initexpr = ParseExpression();
    if (!initexpr)
    {
      return;
    }

    auto * newexpr = dynamic_cast<ONewExpr *>(initexpr);
    if (!newexpr)
    {
      delete initexpr;
      StatementError(DQERR_TYPE_SPECIFIER_EXP_AFTER, sid);
      return;
    }
    OTypeObject * new_object_type = dynamic_cast<OTypeObject *>(newexpr->alloc_type ? newexpr->alloc_type->ResolveAlias() : nullptr);
    ptype = (new_object_type ? newexpr->alloc_type : newexpr->ptype);
  }
  else
  {
    StatementError(DQERR_TYPE_SPECIFIER_EXP_AFTER, sid);
    return;
  }

  scf->SkipWhite();
  if (scf->CheckSymbol("tryfrom"))
  {
    scf->SkipWhite();
    OExpr * src_expr = ParseExpression();
    if (src_expr)
    {
      initexpr = new OTryCastExpr(ptype, src_expr);
    }
  }
  else if (scf->CheckSymbol("="))  // variable initializer specified
  {
    scf->SkipWhite();
    // Check for {} zero-initializer (for compound types)
    if (scf->CheckSymbol("{"))
    {
      scf->SkipWhite();
      if (not scf->CheckSymbol("}"))
      {
        ErrorTxt(DQERR_EXPR_INITVALUE, "\"}\" expected for zero-initializer");
        return;
      }
      zero_init = true;
    }
    else
    {
      initexpr = ParseExpression();
    }
  }

  if (arootstmt)
  {
    if (!ParseAttributes(false))
    {
      delete initexpr;
      SkipToModuleStatementStart();
      return;
    }
  }

  if (auto * inferred_arr = dynamic_cast<OTypeArray *>(ptype ? ptype->ResolveAlias() : nullptr);
      inferred_arr && inferred_arr->arraylength == 0)
  {
    auto * arrlit = dynamic_cast<OArrayLit *>(initexpr);
    if (!arrlit || arrlit->elements.empty())
    {
      StatementError(DQERR_ARRAY_SIZESPEC);
      delete initexpr;
      return;
    }
    ptype = inferred_arr->elemtype->GetArrayType(uint32_t(arrlit->elements.size()));
  }

  CheckStatementClose();

  OTypeObject * decl_object_type = dynamic_cast<OTypeObject *>(ptype ? ptype->ResolveAlias() : nullptr);
  if (fixed_object && decl_object_type && decl_object_type->is_abstract)
  {
    StatementError(DQERR_NOT_SUPPORTED, format("constructing abstract object \"{}\"", decl_object_type->name));
    return;
  }
  if (zero_init && decl_object_type && !fixed_object)
  {
    StatementError(DQERR_NOT_SUPPORTED, "value-style object zero-initialization");
    return;
  }
  if (fixed_object && fixed_ctor_call_at_decl)
  {
    OValSymFunc * ctor = nullptr;
    if (!CheckObjectCtorArgs(decl_object_type, fixed_ctor_args, ctor))
    {
      return;
    }
  }

  if (initexpr and (not CheckAssignType(ptype, &initexpr, "Assignment")))  // might add implicit conversion
  {
    // error message is already provided.
    delete initexpr;
    return;
  }

  if (auto * cstrtype = dynamic_cast<OTypeCString *>(ptype ? ptype->ResolveAlias() : nullptr);
      cstrtype && (0 == cstrtype->maxlen) && !initexpr)
  {
    StatementError(DQERR_NOT_SUPPORTED, "standalone unsized cstring declaration without target storage");
    return;
  }

  if (arootstmt)
  {
    ODecl * vdecl = AddDeclVar(scpos_statement_start, sid, ptype);
    if (fixed_object)
    {
      auto * objsym = dynamic_cast<OVsObject *>(vdecl->pvalsym);
      if (!objsym)
      {
        throw logic_error(format("Fixed object variable \"{}\" was not created as OVsObject", sid));
      }
      objsym->SetObjectStorage(OSK_OBJECT_FIXED);
      objsym->SetObjectCtorArgs(std::move(fixed_ctor_args));
      objsym->SetObjectCtorCallAtDecl(fixed_ctor_call_at_decl);
      g_module->EnsureModuleInitFunc(scpos_statement_start);
    }
    else if (vdecl->pvalsym->ptype && (TK_DYN_ARRAY == vdecl->pvalsym->ptype->ResolveAlias()->kind))
    {
      g_module->EnsureModuleInitFunc(scpos_statement_start);
    }
    else if (auto * objsym = dynamic_cast<OVsObject *>(vdecl->pvalsym))
    {
      objsym->SetObjectStorage(OSK_OBJECT_REF);
    }
    vdecl->pvalsym->ApplyAttributes(attr, ATGT_GLOBAL_VAR);
    if (initexpr)
    {
      if (!vdecl->initvalue)
      {
        if (auto * objsym = dynamic_cast<OVsObject *>(vdecl->pvalsym); objsym && objsym->IsObjectReference())
        {
          vdecl->initvalue = objsym->GetStorageType()->CreateValue();
        }
      }
      if (!vdecl->initvalue)
      {
        StatementError(DQERR_CONSTEXPR_INVALID_FOR, ptype->name);
        delete initexpr;
        return;
      }
      if (not vdecl->initvalue->CalculateConstant(initexpr))
      {
        // the error message is already generated
      }
      delete initexpr;
    }

    // global variables are always initialized with 0 / null
    vdecl->pvalsym->initialized = true;
  }
  else
  {
    pvalsym = ptype->CreateValSym(scpos_statement_start, sid);
    if (fixed_object)
    {
      auto * objsym = dynamic_cast<OVsObject *>(pvalsym);
      if (!objsym)
      {
        throw logic_error(format("Fixed object variable \"{}\" was not created as OVsObject", sid));
      }
      objsym->SetObjectStorage(OSK_OBJECT_FIXED);
      objsym->SetObjectCtorArgs(std::move(fixed_ctor_args));
      objsym->SetObjectCtorCallAtDecl(fixed_ctor_call_at_decl);
      pvalsym->initialized = true;
    }
    else if (auto * objsym = dynamic_cast<OVsObject *>(pvalsym))
    {
      objsym->SetObjectStorage(OSK_OBJECT_REF);
    }
    if (pvalsym->ptype && (TK_DYN_ARRAY == pvalsym->ptype->ResolveAlias()->kind))
    {
      pvalsym->initialized = true;
    }
    if (pvalsym->ptype && (TK_DYNSTR == pvalsym->ptype->ResolveAlias()->kind || TK_STRVIEW == pvalsym->ptype->ResolveAlias()->kind))
    {
      pvalsym->initialized = true;
    }
    if (pvalsym->ptype && (TK_ANYVALUE == pvalsym->ptype->ResolveAlias()->kind))
    {
      pvalsym->initialized = true;
    }
    if (zero_init)  pvalsym->initialized = true;
    if (pvalsym->ptype && (TK_ARRAY == pvalsym->ptype->ResolveAlias()->kind))
    {
      pvalsym->initialized = true;
    }
    curscope->DefineValSym(pvalsym);
    curblock->AddStatement(new OStmtVarDecl(scpos_statement_start, pvalsym, initexpr));
  }
}

void ODqCompParserStmt::ParseStmtRef()
{
  string     sid;
  OValSym *  pvalsym = nullptr;
  OType *    ptype = nullptr;

  scf->SkipWhite();
  if (not scf->ReadIdentifier(sid))
  {
    StatementError(DQERR_ID_EXP_AFTER, "ref");
    return;
  }

  pvalsym = curscope->FindValSym(sid, nullptr, false);
  if (pvalsym)
  {
    StatementError(DQERR_VS_ALREADY_DECL_TYPE, sid, pvalsym->ptype->name, &scf->prevpos);
    return;
  }

  scf->SkipWhite();
  if (scf->CheckSymbol(":"))
  {
    ptype = ParseTypeSpec();
    if (!ptype)
    {
      return;
    }
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol("="))
  {
    StatementError(DQERR_REF_LOCAL_INIT_REQUIRED, sid);
    return;
  }

  scf->SkipWhite();
  OExpr * bindexpr = ParseExpression();
  if (!bindexpr)
  {
    return;
  }

  OLValueExpr * bindlval = dynamic_cast<OLValueExpr *>(bindexpr);
  OValSym * rootvalsym = (bindlval ? GetAssignRootValSym(bindlval) : nullptr);
  if (!bindlval || (rootvalsym && (VSK_CONST == rootvalsym->kind || !rootvalsym->IsRefWriteable())))
  {
    delete bindexpr;
    StatementError(DQERR_REF_LOCAL_BIND_TARGET, sid);
    return;
  }

  if (!ptype)
  {
    ptype = bindexpr->ptype;
    if (!ptype)
    {
      delete bindexpr;
      StatementError(DQERR_REF_LOCAL_TYPE_INFER, sid);
      return;
    }
  }
  else if (!OTypeFunc::SameRefBindingType(ptype, bindexpr->ptype))
  {
    string srcname = (bindexpr->ptype ? bindexpr->ptype->name : "?");
    delete bindexpr;
    StatementError(DQERR_REF_LOCAL_TYPE_MISM, sid, ptype->name, srcname);
    return;
  }

  CheckStatementClose();

  pvalsym = ptype->CreateValSym(scpos_statement_start, sid);
  pvalsym->param_mode = FPM_REF;
  pvalsym->is_ref_alias = true;
  pvalsym->initialized = true;
  if (auto * objsym = dynamic_cast<OVsObject *>(pvalsym))
  {
    objsym->SetObjectStorage(OSK_PLAIN);
  }
  curscope->DefineValSym(pvalsym);
  curblock->AddStatement(new OStmtVarDecl(scpos_statement_start, pvalsym, new OAddrOfExpr(bindlval)));
}

void ODqCompParserStmt::ParseStmtConst(bool arootstmt)
{
  // syntax form: "const identifier : type [ = initial value];"
  // note: "const" is already consumed

  string       sid;
  OValSym *    pvalsym;
  OType *      ptype;
  OScPosition  expos;

  auto emit_error = [&](const TDiagDefErr & adiag, string_view par1 = "", string_view par2 = "",
                        OScPosition * scpos = nullptr, bool atryrecover = true)
  {
    if (arootstmt)
    {
      RootStatementError(adiag, par1, par2, scpos, atryrecover);
    }
    else
    {
      StatementError(adiag, par1, par2, scpos, atryrecover);
    }
  };

  scf->SkipWhite();
  if (not scf->ReadIdentifier(sid))
  {
    emit_error(DQERR_ID_EXP_AFTER, "const");
    return;
  }
  if (arootstmt && !CheckSpecialReservedRootName(sid))
  {
    return;
  }

  pvalsym = nullptr;
  if (arootstmt)
  {
    g_module->ValSymDeclared(sid, &pvalsym);
  }
  else
  {
    pvalsym = curscope->FindValSym(sid, nullptr, false);  // check only the current scope
  }
  if (pvalsym)
  {
    emit_error(DQERR_VS_ALREADY_DECL_TYPE, sid, pvalsym->ptype->name, &scf->prevpos);
    return;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(":"))
  {
    emit_error(DQERR_TYPE_SPECIFIER_EXP_AFTER, sid);
    return;
  }

  ptype = ParseTypeSpec();
  if (not ptype)
  {
    return;
  }
  if (TK_DYN_ARRAY == ptype->ResolveAlias()->kind)
  {
    emit_error(DQERR_NOT_SUPPORTED, "dynamic array constant");
    return;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol("="))  // variable initializer specified
  {
    emit_error(DQERR_MISSING_ASSIGN_FOR, sid);
    return;
  }

  scf->SkipWhite();
  scf->SaveCurPos(expos);
  OExpr * valueexpr = ParseExpression();
  if (not valueexpr)
  {
    delete valueexpr;
    emit_error(DQERR_EXPR_WRONG_VALUE_FOR, sid, "", &expos);
    return;
  }

  OValue * pvalue = ptype->CreateValue();
  if (!pvalue)
  {
    emit_error(DQERR_CONSTEXPR_INVALID_FOR, sid, "", &expos);
    delete valueexpr;
    return;
  }
  if (not pvalue->CalculateConstant(valueexpr))
  {
    emit_error(DQERR_CONSTEXPR_INVALID_FOR, sid, "", &expos);

    delete valueexpr;
    delete pvalue;
    return;
  }

  delete valueexpr;

  if (arootstmt)
  {
    if (!ParseAttributes(false))
    {
      SkipToModuleStatementStart();
      delete pvalue;
      return;
    }
  }

  if (arootstmt)
  {
    ODecl * decl = AddDeclConst(scpos_statement_start, sid, ptype, pvalue);
    decl->pvalsym->ApplyAttributes(attr, ATGT_GLOBAL_CONST);
  }
  else
  {
    curscope->DefineValSym(new OValSymConst(scpos_statement_start, sid, ptype, pvalue));
  }

  if (not CheckStatementClose())
  {
    // error message already generated.
    return;
  }
}

void ODqCompParserStmt::ReadStatementBlock(OStmtBlock * stblock, const string blockend, string * rendstr)
{

  OStmtBlock * prev_block = curblock;
  OScope *     prev_scope = curscope;

  curblock = stblock;
  curscope = stblock->scope;

  string block_closer;
  string sid;

  scf->SkipWhite();
  if (scf->CheckSymbol("{"))
  {
    block_closer = "}";
  }
  else if (scf->CheckSymbol(":"))
  {
    block_closer = blockend;
  }
  else
  {
    StatementError(DQERR_STMTBLK_START_MISSING);
    block_closer = blockend;
  }

  bool endfound = false;
  while (not scf->Eof())
  {
    scf->SkipWhite();

    auto split_view = block_closer | views::split('|');
    for (auto chunk : split_view)
    {
      string chunkstr = string(string_view(chunk));
      if (scf->CheckSymbol(chunkstr.c_str()))
      {
        endfound = true;
        if (rendstr)  *rendstr = chunkstr;
        break;
      }
    }

    if (endfound)
    {
      break;  // block closer was found
    }

    if (scf->Eof())
    {
      if (rendstr)  *rendstr = "";
      StatementError(DQERR_STMTBLK_CLOSE_MISSING, block_closer);
      break;
    }

    scf->SaveCurPos(scpos_statement_start);

    if (scf->CheckSymbol(";"))  // empty ";", just ignore it
    {
      Hint(DQHINT_MEANINGLESS_SEMICOLON);
      continue;
    }

    if (!ParseAttributes(true))
    {
      SkipCurStatement();
      continue;
    }

    ODqCompBaseSuppressWarningsScope sws(this, attr->IsSet(ATTF_NOWARN));
    if (attr->flags)
    {
      attr->CheckInvalidAttributes(ATGT_STATEMENT);
    }

    // Try keywords first, use ReadIdentifier for whole word checking

    scf->SaveCurPos(scpos_statement_start);  // we jump back here if the identifier is unknown
    if (scf->ReadIdentifier(sid))
    {
      if ("var" == sid)  // local variable declaration
      {
        ParseStmtVar(false);
        continue;
      }
      else if ("ref" == sid)
      {
        ParseStmtRef();
        continue;
      }
      else if ("refin" == sid || "refout" == sid || "refnull" == sid)
      {
        StatementError(DQERR_REF_LOCAL_MODE_UNSUPPORTED, sid);
        continue;
      }
      else if ("const" == sid)
      {
        ParseStmtConst(false);
        continue;
      }
      else if ("return" == sid)
      {
        ParseStmtReturn();
        continue;
      }
      else if ("break" == sid)
      {
        if (loop_depth < 1)
        {
          StatementError(DQERR_STMT_INVALID, sid);
          continue;
        }
        if (!CheckStatementClose())
        {
          continue;
        }
        curblock->AddStatement(new OBreakStmt(scpos_statement_start));
        continue;
      }
      else if ("continue" == sid)
      {
        if (loop_depth < 1)
        {
          StatementError(DQERR_STMT_INVALID, sid);
          continue;
        }
        if (!CheckStatementClose())
        {
          continue;
        }
        curblock->AddStatement(new OContinueStmt(scpos_statement_start));
        continue;
      }
      else if ("while" == sid)
      {
        ParseStmtWhile();
        continue;
      }
      else if ("for" == sid)
      {
        ParseStmtFor();
        continue;
      }
      else if ("if" == sid)
      {
        ParseStmtIf();
        continue;
      }
      else if ("try" == sid)
      {
        ParseStmtTry();
        continue;
      }
      else if ("raise" == sid)
      {
        ParseStmtRaise();
        continue;
      }
      else if ("delete" == sid)
      {
        ParseStmtDelete();
        continue;
      }
      else if ("inherited" == sid)
      {
        ParseStmtInherited();
        continue;
      }
      else if (ReservedWord(sid))
      {
        StatementError(DQERR_STMT_INVALID, sid);
        continue;
      }
      else  // not handled, restore position and go on with expression parsing
      {
        scf->SetCurPos(scpos_statement_start);
      }
    }

    // Now, there are two possibilities: function call or  - assignment
    // Both start with an expression

    int prev_errorcnt = errorcnt;
    suppressed_varinit_diags.clear();
    supress_varinit_check = true;  // do not generate variable not initialized error for the left value
    OExpr * leftexpr = ParseExpression();
    supress_varinit_check = false;
    if (!leftexpr)
    {
      EmitSuppressedVarInitDiags();
      if (prev_errorcnt == errorcnt)  // no error was generated yet ?
      {
        Error(DQERR_EXPR_EXPECTED, &scpos_statement_start);
      }
      SkipToStatementEnd();  // try to find the ";"
      continue;
    }

    scf->SkipWhite();

    // check for assignment operator

    EBinOp binop = ODqCompParserStmt::ParseAssignOp();
    if (int(binop) >= 0)
    {
      scf->SkipWhite();
      prev_errorcnt = errorcnt;
      OExpr * rightexpr = ParseExpression();
      if (!rightexpr)
      {
        if (prev_errorcnt == errorcnt)  // no error was generated yet ?
        {
          Error(DQERR_EXPR_EXPECTED);
        }
        OLValueExpr * lval = dynamic_cast<OLValueExpr *>(leftexpr);
        if (lval)
        {
          EmitFilteredAssignVarInitDiags(lval, binop);
        }
        else
        {
          EmitSuppressedVarInitDiags();
        }
        delete leftexpr;
        SkipToStatementEnd();  // try to find the ";"
        continue;
      }

      if (!CheckStatementClose())
      {
        OScPosition scpos;
        scf->SaveCurPos(scpos);
      }

      OLValueExpr * lval = dynamic_cast<OLValueExpr *>(leftexpr);
      if (!lval)
      {
        EmitSuppressedVarInitDiags();
        Error(DQERR_LVALUE_NOT_WRITEABLE);
        delete leftexpr;
        delete rightexpr;
        continue;
      }

      EmitFilteredAssignVarInitDiags(lval, binop);
      FinalizeStmtAssign(lval, binop, rightexpr);
      continue;
    }

    // the leftexpr should be a callable expression
    bool is_call_stmt = (dynamic_cast<OCallExpr *>(leftexpr) != nullptr)
                     || (dynamic_cast<OIndirectCallExpr *>(leftexpr) != nullptr)
                     || (dynamic_cast<ODynArrayMethodCallExpr *>(leftexpr) != nullptr)
                     || (dynamic_cast<OCStringMethodCallExpr *>(leftexpr) != nullptr)
                     || (dynamic_cast<OStringMethodCallExpr *>(leftexpr) != nullptr)
                     || (dynamic_cast<OAnyValueMethodCallExpr *>(leftexpr) != nullptr)
                     || (dynamic_cast<OInvalidCallExpr *>(leftexpr) != nullptr);
    if (!is_call_stmt)
    {
      EmitSuppressedVarInitDiags();
      StatementError(DQERR_STMT_ASSIGN_OR_FCALL_EXP);
      scf->SkipWhite();
      if (!scf->CheckSymbol(";"))
      {
        SkipToStatementEnd();
      }
      delete leftexpr;
      continue;
    }

    EmitSuppressedVarInitDiags();
    if (!CheckStatementClose())
    {
      OScPosition scpos;
      scf->SaveCurPos(scpos);
    }

    FinalizeStmtVoidCall(leftexpr);

  }

  curscope = prev_scope;
  curblock = prev_block;
}

void ODqCompParserStmt::ParseStmtReturn()
{
  // "return" is already consumed.
  if (CheckStatementClose(false)) // return without value
  {
    curblock->AddStatement(new OStmtReturn(scpos_statement_start, nullptr, curvsfunc));
    return;
  }

  if (not curvsfunc->vsresult)
  {
    Error(DQERR_FUNC_RESULT_SPECIFIED, curvsfunc->name);
    return;
  }

  OExpr * expr = ParseExpression();
  if (!CheckStatementClose())
  {
    // error is emitted by CheckStatementClose
  }
  
  if (expr)
  {
    if (!CheckAssignType(curvsfunc->vsresult->ptype, &expr, "Return"))
    {
      OExpr::DeleteTree(expr);
      return;
    }
    curblock->scope->SetVarInitialized(curvsfunc->vsresult);
    curblock->AddStatement(new OStmtReturn(scpos_statement_start, expr, curvsfunc));
  }
}

void ODqCompParserStmt::ParseStmtDelete()
{
  OValSymFunc * memfree_func = dynamic_cast<OValSymFunc *>(curscope->FindValSym("MemFree"));
  if (!memfree_func)
  {
    auto nsit = g_namespaces.find("sys");
    if (nsit != g_namespaces.end() && nsit->second)
    {
      memfree_func = dynamic_cast<OValSymFunc *>(nsit->second->FindValSym("MemFree", nullptr, false));
    }
  }
  if (!memfree_func)
  {
    Error(DQERR_VS_UNKNOWN, "MemFree");
    SkipToStatementEnd();
    return;
  }

  scf->SkipWhite();
  OExpr * ptrexpr = ParseExpression();
  if (!ptrexpr)
  {
    return;
  }

  OType * ptrtype = ptrexpr->ResolvedType();
  OTypeObject * delete_object_type = dynamic_cast<OTypeObject *>(ptrtype);
  bool deleting_object = delete_object_type;
  bool clear_after_free = deleting_object;
  if (!ptrtype || ((TK_POINTER != ptrtype->kind) && !deleting_object))
  {
    string got = (ptrtype ? ptrtype->name : "?");
    delete ptrexpr;
    Error(DQERR_TYPE_EXPECTED, "pointer", got);
    SkipToStatementEnd();
    return;
  }
  if (deleting_object)
  {
    OLValueExpr * lval = dynamic_cast<OLValueExpr *>(ptrexpr);
    OValSym * rootvalsym = (lval ? GetAssignRootValSym(lval) : nullptr);
    if (!lval)
    {
      delete ptrexpr;
      Error(DQERR_LVALUE_NOT_WRITEABLE);
      SkipToStatementEnd();
      return;
    }
    if (lval->IsFixedObjectStorageExpr())
    {
      delete ptrexpr;
      Error(DQERR_REF_ASSIGN_READONLY, "fixed object");
      SkipToStatementEnd();
      return;
    }
    if (rootvalsym && ((VSK_CONST == rootvalsym->kind) || !rootvalsym->IsRefWriteable()))
    {
      delete ptrexpr;
      Error(DQERR_REF_ASSIGN_READONLY, rootvalsym->name);
      SkipToStatementEnd();
      return;
    }
  }

  scf->SkipWhite();
  if (scf->CheckSymbol("="))
  {
    clear_after_free = true;
    scf->SkipWhite();
    if (!scf->CheckSymbol("null"))
    {
      delete ptrexpr;
      Error(DQERR_EXPR_WRONG_VALUE_FOR, "delete");
      SkipToStatementEnd();
      return;
    }

    OLValueExpr * lval = dynamic_cast<OLValueExpr *>(ptrexpr);
    OValSym * rootvalsym = (lval ? GetAssignRootValSym(lval) : nullptr);
    if (!lval)
    {
      delete ptrexpr;
      Error(DQERR_LVALUE_NOT_WRITEABLE);
      SkipToStatementEnd();
      return;
    }
    if (rootvalsym && VSK_CONST == rootvalsym->kind)
    {
      delete ptrexpr;
      Error(DQERR_TYPE_ASSIGN_TO_CONST, rootvalsym->name);
      SkipToStatementEnd();
      return;
    }
    if (rootvalsym && !rootvalsym->IsRefWriteable())
    {
      delete ptrexpr;
      Error(DQERR_REF_ASSIGN_READONLY, rootvalsym->name);
      SkipToStatementEnd();
      return;
    }
  }

  CheckStatementClose();

  auto * delstmt = new OStmtDelete(scpos_statement_start, ptrexpr, clear_after_free, memfree_func);
  if (deleting_object)
  {
    delstmt->object_dtor_func = delete_object_type->FindSpecialMethod(OSF_DESTROY);
  }
  curblock->AddStatement(delstmt);
}

void ODqCompParserStmt::ParseStmtInherited()
{
  auto * owner_object = dynamic_cast<OTypeObject *>(curvsfunc ? curvsfunc->owner_compound_type : nullptr);
  if (!owner_object || !owner_object->base_type)
  {
    StatementError(DQERR_INHERITED_CALL_INVALID, "inherited is only valid inside a derived object method");
    SkipToStatementEnd();
    return;
  }

  string method_name;
  vector<OExpr *> args;
  if (CheckStatementClose(false))
  {
    if ((OSF_CREATE == curvsfunc->object_specfunc_kind) or (OSF_DESTROY == curvsfunc->object_specfunc_kind))
    {
      ErrorTxt(DQERR_INHERITED_CALL_INVALID, "short form of inherited is not valid in lifecycle methods");
      return;
    }
    method_name = curvsfunc->name;
    for (size_t i = 1; i < curvsfunc->args.size(); ++i)
    {
      args.push_back(new OLValueVar(curvsfunc->args[i]));
    }
  }
  else
  {
    if (!scf->ReadIdentifier(method_name))
    {
      StatementError(DQERR_ID_EXP_AFTER, "inherited");
      SkipToStatementEnd();
      return;
    }
    scf->SkipWhite();
    if (!scf->CheckSymbol("("))
    {
      Error(DQERR_FUNC_CALL_PARENTH, method_name);
      SkipToStatementEnd();
      return;
    }
    vector<TRawCallArg> rawargs;
    if (!ParseRawCallArguments(method_name, rawargs))
    {
      return;
    }
    for (TRawCallArg & rawarg : rawargs)
    {
      args.push_back(rawarg.expr);
      rawarg.expr = nullptr;
    }
    FreeRawCallArguments(rawargs);

    CheckStatementClose();
  }

  OValSymFunc * method = FindInheritedMethod(method_name, args);
  OTypeFunc * sig = dynamic_cast<OTypeFunc *>(method ? method->ptype : nullptr);
  if (!method || !sig || sig->params.size() != args.size() + 1)
  {
    for (OExpr * arg : args) OExpr::DeleteTree(arg);
    Error(DQERR_OVERLOAD_NO_MATCH, method_name);
    return;
  }
  for (size_t i = 0; i < args.size(); ++i)
  {
    if (!CheckAssignType(sig->params[i + 1]->ptype, &args[i], "inherited argument"))
    {
      for (OExpr * arg : args) OExpr::DeleteTree(arg);
      return;
    }
  }

  auto * stmt = new OStmtInheritedCall(scpos_statement_start, curvsfunc, method, args);
  EObjectSpecFuncKind osf = ObjectSpecFuncKindFromName(method_name);
  stmt->emit_derived_field_init = (OSF_CREATE == curvsfunc->object_specfunc_kind && OSF_CREATE == osf);
  stmt->emit_derived_field_destroy = (OSF_DESTROY == curvsfunc->object_specfunc_kind && OSF_DESTROY == osf);
  curblock->AddStatement(stmt);
}

void ODqCompParserStmt::ParseStmtWhile()
{
  // note: "while" is already consumed
  // syntax form: "while <condition>: <statement_block> endwhile"

  scf->SkipWhite();

  OExpr * cond = ParseExpression();
  if (!cond)
  {
    StatementError(DQERR_CONDEXPR_MISSING_FOR, "while");
    return;
  }

  OStmtWhile * st = new OStmtWhile(scpos_statement_start, cond, curscope);
  curblock->AddStatement(st);

  ++loop_depth;
  ReadStatementBlock(st->body, "endwhile");
  --loop_depth;

  st->body->scope->RevertFirstAssignments();
}

void ODqCompParserStmt::ParseStmtFor()
{
  // note: "for" is already consumed
  // syntax forms:
  //   for i = start to|downto end [step step_expr]: ... endfor
  //   for i = start count|downcount count_expr [step step_expr]: ... endfor
  //   for i = start while condition [step step_expr]: ... endfor
  //   for i : T = start ...

  enum class EForKind
  {
    TO,
    DOWNTO,
    COUNT,
    DOWNCOUNT,
    WHILE
  };

  OScope * saved_scope = curscope;

  auto restore_scope = [&]()
  {
    curscope = saved_scope;
  };

  auto is_integer_type = [](OType * ptype)
  {
    OType * resolved = (ptype ? ptype->ResolveAlias() : nullptr);
    return resolved && (TK_INT == resolved->kind);
  };

  auto is_positive_const_step = [&](OExpr * expr, const string & form)
  {
    OType * exprtype = (expr ? expr->ResolvedType() : nullptr);
    if (!exprtype || (TK_INT != exprtype->kind))
    {
      return true;
    }

    OValueInt value(exprtype, 0);
    if (value.CalculateConstant(expr, false) && (value.value <= 0))
    {
      Error(DQERR_FOR_STEP_POSITIVE, form);
      return false;
    }

    return true;
  };

  auto parse_optional_step = [&]() -> OExpr *
  {
    string sid;
    scf->SkipWhite();
    if (scf->ReadIdentifier(sid, false) && ("step" == sid))
    {
      scf->ReadIdentifier(sid);
      scf->SkipWhite();
      OExpr * stepexpr = ParseExpression();
      if (!stepexpr)
      {
        Error(DQERR_EXPR_EXPECTED);
      }
      return stepexpr;
    }

    return new OIntLit(1);
  };

  auto make_compare = [](OValSym * var, ECompareOp op, OExpr * right) -> OExpr *
  {
    return new OCompareExpr(op, new OLValueVar(var), right);
  };

  string loopvar_name;
  scf->SkipWhite();
  if (!scf->ReadIdentifier(loopvar_name))
  {
    StatementError(DQERR_ID_EXP_AFTER, "for");
    return;
  }

  OStmtFor * st = new OStmtFor(scpos_statement_start, saved_scope);
  curblock->AddStatement(st);

  auto abort_for = [&]()
  {
    restore_scope();
    SkipToSymbol("endfor");
  };

  OType * specified_type = nullptr;
  scf->SkipWhite();
  if (scf->CheckSymbol(":"))
  {
    specified_type = ParseTypeSpec();
    if (!specified_type)
    {
      abort_for();
      return;
    }
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol("="))
  {
    StatementError(DQERR_MISSING_ASSIGN_FOR, loopvar_name);
    abort_for();
    return;
  }

  scf->SkipWhite();
  OExpr * start_expr = ParseExpression();
  if (!start_expr)
  {
    abort_for();
    return;
  }

  OValSym * loopvar = saved_scope->FindValSym(loopvar_name);
  bool declare_loopvar = false;

  if (specified_type)
  {
    if (loopvar)
    {
      if (loopvar->ptype != specified_type)
      {
        Error(DQERR_TYPEMISM_STMT_ASSIGN, "for loop variable", specified_type->name, loopvar->ptype->name);
        OExpr::DeleteTree(start_expr);
        abort_for();
        return;
      }
    }
    else
    {
      loopvar = specified_type->CreateValSym(scpos_statement_start, loopvar_name);
      declare_loopvar = true;
    }
  }
  else if (!loopvar)
  {
    Error(DQERR_VAR_UNKNOWN, loopvar_name);
    OExpr::DeleteTree(start_expr);
    abort_for();
    return;
  }

  if (!is_integer_type(loopvar->ptype))
  {
    Error(DQERR_TYPE_EXPECTED, "integer", loopvar->ptype->name);
    OExpr::DeleteTree(start_expr);
    abort_for();
    return;
  }

  if (loopvar && (VSK_CONST == loopvar->kind))
  {
    Error(DQERR_TYPE_ASSIGN_TO_CONST, loopvar->name);
    OExpr::DeleteTree(start_expr);
    abort_for();
    return;
  }

  if (loopvar && !loopvar->IsRefWriteable())
  {
    Error(DQERR_REF_ASSIGN_READONLY, loopvar->name);
    OExpr::DeleteTree(start_expr);
    abort_for();
    return;
  }

  if (!CheckAssignType(loopvar->ptype, &start_expr, "for initializer"))
  {
    OExpr::DeleteTree(start_expr);
    abort_for();
    return;
  }

  if (declare_loopvar)
  {
    st->init->scope->DefineValSym(loopvar);
    st->init->AddStatement(new OStmtVarDecl(scpos_statement_start, loopvar, start_expr));
  }
  else
  {
    st->init->AddStatement(new OStmtAssign(scpos_statement_start, new OLValueVar(loopvar), start_expr));
    st->init->scope->SetVarInitialized(loopvar);
  }

  curscope = st->init->scope;

  string kindstr;
  scf->SkipWhite();
  if (!scf->ReadIdentifier(kindstr))
  {
    Error(DQERR_KW_OR_ID_MISSING);
    abort_for();
    return;
  }

  EForKind kind;
  if      ("to"        == kindstr)  kind = EForKind::TO;
  else if ("downto"    == kindstr)  kind = EForKind::DOWNTO;
  else if ("count"     == kindstr)  kind = EForKind::COUNT;
  else if ("downcount" == kindstr)  kind = EForKind::DOWNCOUNT;
  else if ("while"     == kindstr)  kind = EForKind::WHILE;
  else if ("in"        == kindstr)
  {
    Error(DQERR_NOT_SUPPORTED, "for ... in");
    abort_for();
    return;
  }
  else
  {
    Error(DQERR_KW_OR_ID_MISSING);
    abort_for();
    return;
  }

  OExpr * limit_expr = nullptr;
  OExpr * step_expr = nullptr;

  if (EForKind::WHILE == kind)
  {
    limit_expr = ParseExpression();
    if (!limit_expr)
    {
      StatementError(DQERR_CONDEXPR_MISSING_FOR, "for");
      abort_for();
      return;
    }

    if (TK_BOOL != limit_expr->ResolvedType()->kind)
    {
      Error(DQERR_BOOL_EXPR_EXPECTED, limit_expr->ResolvedType()->name);
    }

    st->condition = limit_expr;
    step_expr = parse_optional_step();
    if (!step_expr)
    {
      abort_for();
      return;
    }
  }
  else
  {
    limit_expr = ParseExpression();
    if (!limit_expr)
    {
      Error(DQERR_EXPR_EXPECTED);
      abort_for();
      return;
    }

    if (!CheckAssignType(loopvar->ptype, &limit_expr, "for limit"))
    {
      OExpr::DeleteTree(limit_expr);
      abort_for();
      return;
    }

    step_expr = parse_optional_step();
    if (!step_expr)
    {
      abort_for();
      return;
    }
    if (!is_positive_const_step(step_expr, kindstr))
    {
      OExpr::DeleteTree(step_expr);
      abort_for();
      return;
    }

    if (EForKind::TO == kind)
    {
      st->condition = make_compare(loopvar, COMPOP_LE, limit_expr);
    }
    else if (EForKind::DOWNTO == kind)
    {
      st->condition = make_compare(loopvar, COMPOP_GE, limit_expr);
    }
    else
    {
      OValSym * countvar = loopvar->ptype->CreateValSym(scpos_statement_start,
          format("__for_count_{}_{}", scpos_statement_start.line, scpos_statement_start.col));
      st->init->scope->DefineValSym(countvar);
      st->init->AddStatement(new OStmtVarDecl(scpos_statement_start, countvar, limit_expr));
      st->condition = make_compare(countvar, COMPOP_GT, new OIntLit(0, loopvar->ptype));
      st->step->AddStatement(new OStmtModifyAssign(scpos_statement_start, new OLValueVar(countvar),
          BINOP_SUB, new OIntLit(1, loopvar->ptype)));
    }
  }

  if (!CheckAssignType(loopvar->ptype, &step_expr, "for step"))
  {
    OExpr::DeleteTree(step_expr);
    abort_for();
    return;
  }

  EBinOp step_op = (EForKind::DOWNTO == kind || EForKind::DOWNCOUNT == kind) ? BINOP_SUB : BINOP_ADD;
  st->step->AddStatement(new OStmtModifyAssign(scpos_statement_start, new OLValueVar(loopvar), step_op, step_expr));

  ++loop_depth;
  ReadStatementBlock(st->body, "endfor");
  --loop_depth;
  st->body->scope->RevertFirstAssignments();

  restore_scope();
}

void ODqCompParserStmt::ParseStmtIf()
{
  // note: "if" is already consumed
  // syntax form: "if <condition>: <stblock> elif <condition>: <stblock> else: <stblock> endif"
  scf->SkipWhite();

  OExpr * cond = ParseExpression();
  if (!cond)
  {
    StatementError(DQERR_CONDEXPR_MISSING_FOR, "if");
    return;
  }

  if (TK_BOOL != cond->ResolvedType()->kind)
  {
    Error(DQERR_BOOL_EXPR_EXPECTED, cond->ResolvedType()->name);
    // continue parsing the if branch for better error recovery
  }

  OStmtIf * st = new OStmtIf(scpos_statement_start, curscope);
  OIfBranch * branch = st->AddBranch(cond);
  curblock->AddStatement(st);

  while (not scf->Eof())
  {
    string endstr = "";
    ReadStatementBlock(branch->body, "endif|elif|else", &endstr);
    branch->body->scope->RevertFirstAssignments();

    if ("endif" == endstr)
    {
      break;  // if closed
    }

    if ("elif" == endstr)
    {
      cond = ParseExpression();
      if (!cond)
      {
        StatementError(DQERR_CONDEXPR_MISSING_FOR, "elif");
        break;
      }
      if (TK_BOOL != cond->ResolvedType()->kind)
      {
        Error(DQERR_BOOL_EXPR_EXPECTED, cond->ResolvedType()->name);
        // continue for better error recovery
      }

      branch = st->AddBranch(cond);
      continue;
    }

    if ("else" == endstr)
    {
      if (st->else_present)
      {
        StatementError(DQERR_MULTIPLE_ELSE);
        break;
      }
      st->else_present = true;
      branch = st->AddBranch(nullptr);
      continue;
    }

    if ("}" == endstr) // for braces mode
    {
      scf->SkipWhite();
      if (scf->CheckSymbol("endif"))
      {
        endstr = "endif";
        continue;
      }
      if (scf->CheckSymbol("elif"))
      {
        endstr = "elif";
        continue;
      }
      if (scf->CheckSymbol("else"))
      {
        endstr = "else";
        continue;
      }
    }

    break;
  }

  // re-set variable initializations when else was present and first-assigns present in all branches
  if (st->else_present)
  {
    OScope * sfirst = st->branches[0]->body->scope;
    for (OValSym * vs : sfirst->firstassign)
    {
      // check if this vs presents in all the other scopes
      bool initok = true;
      for (int bi = 1; bi < st->branches.size(); ++bi)
      {
        OScope * scp = st->branches[bi]->body->scope;
        if (not scp->FirstAssigned(vs))
        {
          initok = false;
          break;
        }
      }
      if (initok)
      {
        vs->initialized = true;
      }
    }
  }
}

void ODqCompParserStmt::ParseStmtTry()
{
  if (!EnsureExceptionRtlUse())
  {
    ++errorcnt;
    SkipToSymbol("endtry");
    return;
  }

  OStmtTry * st = new OStmtTry(scpos_statement_start, curscope);
  curblock->AddStatement(st);

  string endstr;
  ReadStatementBlock(st->body, "except|finally|endtry", &endstr);
  st->body->scope->RevertFirstAssignments();

  bool seen_catch_all = false;
  bool seen_finally = false;

  while (!scf->Eof())
  {
    if ("except" == endstr)
    {
      if (seen_finally)
      {
        StatementError(DQERR_STMT_INVALID, "except after finally");
        SkipToSymbol("endtry");
        return;
      }
      if (seen_catch_all)
      {
        StatementError(DQERR_STMT_INVALID, "except after catch-all");
        SkipToSymbol("endtry");
        return;
      }

      OTypeObject * exception_type = nullptr;
      OValSym * bound_variable = nullptr;

      scf->SkipWhite();
      if (!scf->CheckSymbol(":", false) && !scf->CheckSymbol("{", false))
      {
        OType * type = ParseTypeSpec();
        exception_type = dynamic_cast<OTypeObject *>(type ? type->ResolveAlias() : nullptr);
        if (!IsExceptionType(exception_type, curscope))
        {
          Error(DQERR_TYPE_EXPECTED, "Exception", type ? type->name : string("?"));
          SkipToSymbol("endtry");
          return;
        }

        scf->SkipWhite();
        string as_keyword;
        if (scf->ReadIdentifier(as_keyword, false) && "as" == as_keyword)
        {
          scf->ReadIdentifier(as_keyword);
          scf->SkipWhite();
          string varname;
          if (!scf->ReadIdentifier(varname))
          {
            StatementError(DQERR_ID_EXP_AFTER, "as");
            SkipToSymbol("endtry");
            return;
          }
          bound_variable = exception_type->CreateValSym(scpos_statement_start, varname);
        }
      }
      else
      {
        seen_catch_all = true;
      }

      OExceptBranch * branch = st->AddExceptBranch(exception_type, bound_variable, curscope);
      ++except_depth;
      ReadStatementBlock(branch->body, "except|finally|endtry", &endstr);
      --except_depth;
      branch->body->scope->RevertFirstAssignments();
      continue;
    }

    if ("finally" == endstr)
    {
      if (seen_finally)
      {
        StatementError(DQERR_STMT_INVALID, "multiple finally branches");
        SkipToSymbol("endtry");
        return;
      }
      seen_finally = true;
      OStmtBlock * finally_body = st->EnsureFinally(curscope);
      ReadStatementBlock(finally_body, "endtry", &endstr);
      finally_body->scope->RevertFirstAssignments();
      break;
    }

    break;
  }

  if (st->except_branches.empty() && !st->finally_body)
  {
    StatementError(DQERR_STMT_INVALID, "try without except or finally");
  }
}

void ODqCompParserStmt::ParseStmtRaise()
{
  if (!EnsureExceptionRtlUse())
  {
    ++errorcnt;
    SkipToStatementEnd();
    return;
  }

  if (CheckStatementClose(false))
  {
    if (except_depth < 1)
    {
      StatementError(DQERR_STMT_INVALID, "standalone raise outside except");
      return;
    }
    curblock->AddStatement(new OStmtRaise(scpos_statement_start, nullptr));
    return;
  }

  auto find_memalloc = [&]() -> OValSymFunc *
  {
    OValSymFunc * memalloc_func = dynamic_cast<OValSymFunc *>(curscope->FindValSym("MemAlloc"));
    if (!memalloc_func)
    {
      auto nsit = g_namespaces.find("sys");
      if (nsit != g_namespaces.end() && nsit->second)
      {
        memalloc_func = dynamic_cast<OValSymFunc *>(nsit->second->FindValSym("MemAlloc", nullptr, false));
      }
    }
    return memalloc_func;
  };

  auto parse_raise_shorthand = [&]() -> OExpr *
  {
    OScPosition saved;
    scf->SaveCurPos(saved);

    OType * type = ParseTypeSpec(false);
    OTypeObject * object_type = dynamic_cast<OTypeObject *>(type ? type->ResolveAlias() : nullptr);
    scf->SkipWhite();
    if (!object_type || !scf->CheckSymbol("(", false))
    {
      scf->SetCurPos(saved);
      return nullptr;
    }
    if (!IsExceptionType(object_type, curscope))
    {
      Error(DQERR_TYPE_EXPECTED, "Exception", object_type->name);
      return nullptr;
    }
    if (object_type->is_abstract)
    {
      ErrorTxt(DQERR_NOT_SUPPORTED, format("constructing abstract object \"{}\"", object_type->name));
      return nullptr;
    }

    vector<TRawCallArg> rawargs;
    scf->CheckSymbol("(");
    if (!ParseRawCallArguments(object_type->name, rawargs))
    {
      return nullptr;
    }

    vector<OExpr *> ctor_args;
    for (TRawCallArg & rawarg : rawargs)
    {
      ctor_args.push_back(rawarg.expr);
      rawarg.expr = nullptr;
    }
    FreeRawCallArguments(rawargs);

    OValSymFunc * ctor = nullptr;
    if (!CheckObjectCtorArgs(object_type, ctor_args, ctor))
    {
      for (OExpr * arg : ctor_args) OExpr::DeleteTree(arg);
      return nullptr;
    }

    OValSymFunc * memalloc_func = find_memalloc();
    if (!memalloc_func)
    {
      Error(DQERR_VS_UNKNOWN, "MemAlloc");
      for (OExpr * arg : ctor_args) OExpr::DeleteTree(arg);
      return nullptr;
    }

    ONewExpr * result = new ONewExpr(object_type, nullptr, memalloc_func);
    result->ctor_func = ctor;
    result->ctor_args = ctor_args;
    return result;
  };

  OExpr * expr = nullptr;
  scf->SkipWhite();
  OScPosition newpos;
  scf->SaveCurPos(newpos);
  string sid;
  if (scf->ReadIdentifier(sid, false) && "new" == sid)
  {
    scf->ReadIdentifier(sid);
    expr = ParseNewExpr();
  }
  else
  {
    expr = parse_raise_shorthand();
    if (!expr)
    {
      expr = ParseExpression();
    }
  }

  if (!expr)
  {
    SkipToStatementEnd();
    return;
  }

  OTypeObject * expr_object_type = ExceptionObjectTypeFromExpr(expr);
  if (!IsExceptionType(expr_object_type, curscope))
  {
    Error(DQERR_TYPE_EXPECTED, "Exception", expr->ResolvedType() ? expr->ResolvedType()->name : string("?"));
    OExpr::DeleteTree(expr);
    SkipToStatementEnd();
    return;
  }

  if (!CheckStatementClose())
  {
    OExpr::DeleteTree(expr);
    return;
  }

  curblock->AddStatement(new OStmtRaise(scpos_statement_start, expr));
}

EBinOp ODqCompParserStmt::ParseAssignOp()
{
  scf->SkipWhite();

  if      (scf->CheckSymbol("+="))      return BINOP_ADD;
  else if (scf->CheckSymbol("-="))      return BINOP_SUB;
  else if (scf->CheckSymbol("*="))      return BINOP_MUL;
  else if (scf->CheckSymbol("/="))      return BINOP_DIV;
  else if (scf->CheckSymbol("<<="))     return BINOP_ISHL;
  else if (scf->CheckSymbol(">>="))     return BINOP_ISHR;
  else if (scf->CheckSymbol("=IDIV="))  return BINOP_IDIV;
  else if (scf->CheckSymbol("=IMOD="))  return BINOP_IMOD;
  else if (scf->CheckSymbol("=AND="))   return BINOP_IAND;
  else if (scf->CheckSymbol("=OR="))    return BINOP_IOR;
  else if (scf->CheckSymbol("=XOR="))   return BINOP_IXOR;
  else if (scf->CheckSymbol("="))       return BINOP_NONE;  // must come after =XXX=!, simple assign (ab)uses BINOP_NONE
  return EBinOp(-1);  // not an assignment operator
}

void ODqCompParserStmt::FinalizeStmtVoidCall(OExpr * callexpr)
{
  curblock->AddStatement(new OStmtVoidCall(scpos_statement_start, callexpr));
}
