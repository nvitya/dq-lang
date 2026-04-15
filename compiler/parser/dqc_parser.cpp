/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_parser.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#include <print>
#include <format>
#include <ranges>

#include "dq_module.h"
#include "dqc_parser.h"
#include "otype_func.h"
#include "otype_int.h"
#include "otype_array.h"
#include "otype_cstring.h"
#include "named_scopes.h"
#include "scope_defines.h"
#include "expressions.h"
#include "statements.h"

using namespace std;

ODqCompParser::ODqCompParser()
{
}

ODqCompParser::~ODqCompParser()
{

}

void ODqCompParser::ParseModule()
{
  string sid;

  section_public = true;
  cur_mod_scope = g_module->scope_pub;
  curscope = cur_mod_scope;

  while (not scf->Eof())
  {
    scf->SkipWhite(); // jumps to the first normal token
    if (scf->Eof())
    {
      return; // end of module
    }

    scf->SaveCurPos(scpos_statement_start);  // to display the statement start

    // check for [[attribute]] prefix
    bool has_external = false;
    if (scf->CheckSymbol("[["))
    {
      string attrname;
      scf->SkipWhite();
      if (not scf->ReadIdentifier(attrname))
      {
        Error(DQERR_ATTR_NAME_EXPECTED);
        SkipToSymbol("]]");
        continue;
      }
      scf->SkipWhite();
      if (not scf->CheckSymbol("]]"))
      {
        Error(DQERR_MISSING_ATTR_CLOSE_AFTER, attrname);
        continue;
      }
      if ("external" == attrname)
      {
        has_external = true;
      }
      else
      {
        Error(DQERR_ATTR_UNKNOWN, attrname);
        continue;
      }
      scf->SkipWhite();
    }

    // module root starters
    if (not scf->ReadIdentifier(sid))
    {
      RootStatementError(DQERR_MODULE_STATEMENT_EXPECTED, &scpos_statement_start);
      continue;
    }

    // The module root statement must start with a keyword like
    //   use, module, var, type, function, implementation

    if ("var" == sid) // global variable definition
    {
      ParseStmtVar(true);
    }
    else if ("const" == sid) // global constant definition
    {
      ParseRootConstDecl();
    }
    else if ("type" == sid)
    {
      ParseRootTypeDecl();
    }
    else if ("function" == sid)
    {
      ParseFunction(has_external);
      curscope = cur_mod_scope;
      curblock = nullptr;
    }
    else if ("struct" == sid)
    {
      ParseStructDecl();
    }
    else  // unknown
    {
      RootStatementError(DQERR_MODULE_STATEMENT_UNKNOWN, sid, &scpos_statement_start);
    }
  }

  if (g_opt.verblevel >= VERBLEVEL_DEBUG)
  {
    printf("ParseModule finished.");
  }
}

void ODqCompParser::ParseStmtVar(bool arootstmt)
{
  // syntax form: "var identifier : type [ = expression];"
  // note: "var" is already consumed

  string     sid;
  string     stype;
  OValSym *  pvalsym;
  OType *    ptype;

  scf->SkipWhite();
  if (not scf->ReadIdentifier(sid))
  {
    StatementError(DQERR_ID_EXP_AFTER, "var");
    return;
  }

  pvalsym = curscope->FindValSym(sid, nullptr, false);  // do not search in the parent scopes this time !
  if (pvalsym)
  {
    StatementError(DQERR_VS_ALREADY_DECL_TYPE, sid, pvalsym->ptype->name, &scf->prevpos);
    return;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(":"))
  {
    StatementError(DQERR_TYPE_SPECIFIER_EXP_AFTER, sid);
    return;
  }

  ptype = ParseTypeSpec();
  if (not ptype)  return;

  OExpr * initexpr = nullptr;
  bool zero_init = false;
  scf->SkipWhite();
  if (scf->CheckSymbol("="))  // variable initializer specified
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

  scf->SkipWhite();
  if (!scf->CheckSymbol(";"))
  {
    StatementError(DQERR_MISSING_SEMICOLON_TO_CLOSE, "variable declaration");
  }

  if (initexpr and (not CheckAssignType(ptype, &initexpr, "Assignment")))  // might add implicit conversion
  {
    // error message is already provided.
    delete initexpr;
    return;
  }

  if (arootstmt)
  {
    ODecl * vdecl = AddDeclVar(scpos_statement_start, sid, ptype);
    if (initexpr)
    {
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
    if (zero_init)  pvalsym->initialized = true;
    curscope->DefineValSym(pvalsym);
    curblock->AddStatement(new OStmtVarDecl(scpos_statement_start, pvalsym, initexpr));
  }
}


void ODqCompParser::ParseRootConstDecl()
{
  // syntax form: "const identifier : type [ = initial value];"
  // note: "const" is already consumed

  string       sid;
  OValSym *    pvalsym;
  OType *      ptype;
  OScPosition  expos;

  scf->SkipWhite();
  if (not scf->ReadIdentifier(sid))
  {
    RootStatementError(DQERR_ID_EXP_AFTER, "var");
    return;
  }

  if (g_module->ValSymDeclared(sid, &pvalsym))
  {
    RootStatementError(DQERR_VS_ALREADY_DECL_TYPE, sid, pvalsym->ptype->name, &scf->prevpos);
    return;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(":"))
  {
    RootStatementError(DQERR_TYPE_SPECIFIER_EXP_AFTER, sid);
    return;
  }

  ptype = ParseTypeSpec();
  if (not ptype)
  {
    return;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol("="))  // variable initializer specified
  {
    RootStatementError(DQERR_MISSING_ASSIGN_FOR, sid);
    return;
  }

  scf->SkipWhite();
  scf->SaveCurPos(expos);
  OExpr * valueexpr = ParseExpression();
  if (not valueexpr)
  {
    delete valueexpr;
    RootStatementError(DQERR_EXPR_WRONG_VALUE_FOR, sid, &expos);
    return;
  }

  OValue * pvalue = ptype->CreateValue();
  if (not pvalue->CalculateConstant(valueexpr))
  {
    RootStatementError(DQERR_CONSTEXPR_INVALID_FOR, sid, &expos);

    delete valueexpr;
    delete pvalue;
    return;
  }

  delete valueexpr;

  ODecl * vdecl = AddDeclConst(scpos_statement_start, sid, ptype, pvalue);

  if (not CheckStatementClose())
  {
    // error message already generated.
    return;
  }
}

void ODqCompParser::ParseRootTypeDecl()
{
  // syntax form: "type identifier = type;"
  // note: "type" is already consumed

  string sid;
  OType * ptype;
  OType * foundtype = nullptr;

  scf->SkipWhite();
  if (not scf->ReadIdentifier(sid))
  {
    RootStatementError(DQERR_ID_EXP_AFTER, "type");
    return;
  }

  if (g_module->TypeDeclared(sid, &foundtype))
  {
    RootStatementError(DQERR_TYPE_ALREADY_DEFINED, sid, &scf->prevpos);
    return;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol("="))
  {
    RootStatementError(DQERR_MISSING_ASSIGN_FOR, sid);
    return;
  }

  ptype = ParseTypeSpec();
  if (not ptype)  return;

  cur_mod_scope->DefineType(new OTypeAlias(sid, ptype));

  if (not CheckStatementClose())
  {
    return;
  }
}

void ODqCompParser::ParseStructDecl()
{
  // note: "struct" is already consumed
  // syntax form: "struct Name\n  field : type;\n  ...\nendstruct"

  string sname;
  scf->SkipWhite();
  if (not scf->ReadIdentifier(sname))
  {
    RootStatementError(DQERR_ID_EXP_AFTER, "struct");
    return;
  }

  OCompoundType * ctype = new OCompoundType(sname, cur_mod_scope);

  OScPosition mempos;
  string membername;

  while (not scf->Eof())
  {
    scf->SkipWhite();

    if (scf->CheckSymbol("endstruct"))
    {
      break;
    }

    scf->SaveCurPos(mempos);

    if (not scf->ReadIdentifier(membername))
    {
      StatementError(DQERR_STRUCT_MBID_EXPECTED);
      break;
    }

    scf->SkipWhite();
    if (not scf->CheckSymbol(":"))
    {
      StatementError(DQERR_TYPE_SPECIFIER_EXP_AFTER, membername);
      break;
    }

    OType * mtype = ParseTypeSpec();
    if (not mtype)  break;

    scf->SkipWhite();
    if (not scf->CheckSymbol(";"))
    {
      StatementError(DQERR_MISSING_SEMICOLON_AFTER, "member definition");
      break;
    }

    OValSym * mvsym = new OValSym(mempos, membername, mtype);
    mvsym->initialized = true;  // struct members are always accessible
    ctype->AddMember(mvsym);
  }

  // Compute byte size from LLVM type
  ctype->GetLlType();  // force creation

  cur_mod_scope->DefineType(ctype);
}

void ODqCompParser::ParseFunction(bool aexternal)
{
  // note: "function" is already consumed
  // syntax form: "function identifier[(arglist)] [-> return_type] <statement_block | ;>"
  // statement block must follow, when ';' then it is a forward declaration

  string   sid;
  string   stype;

  scf->SkipWhite();
  if (not scf->ReadIdentifier(sid))
  {
    Error(DQERR_ID_EXP_AFTER, "function");
    return;
  }

  OTypeFunc    * tfunc  = new OTypeFunc(sid);
  OValSymFunc  * vsfunc = new OValSymFunc(scpos_statement_start, sid, tfunc, cur_mod_scope);
  curvsfunc = vsfunc;

  scf->SkipWhite();
  if (scf->CheckSymbol("("))  // parameter list start
  {
    string spname;
    string sptype;

    while (not scf->Eof())
    {
      scf->SkipWhite();
      if (scf->CheckSymbol(")"))
      {
        break;
      }

      if (tfunc->params.size() > 0)
      {
        if (not scf->CheckSymbol(","))
        {
          Error(DQERR_MISSING_COMMA, &scf->prevpos);
        }
        else { scf-> SkipWhite(); }
      }

      // check for variadic "..."
      if (scf->CheckSymbol("..."))
      {
        tfunc->has_varargs = true;
        scf->SkipWhite();
        if (!scf->CheckSymbol(")"))
        {
          Error(DQERR_MISSING_CLOSE_PAREN_AFTER, "...");
        }
        break;
      }

      if (not scf->ReadIdentifier(spname))
      {
        Error(DQERR_FUNCPAR_NAME_EXP, &scf->prevpos);
        if (not scf->ReadTo(",)"))  // try to skip to next parameter
        {
          break;  // serious problem, would lead to endless-loop
        }
        continue;
      }

      if (not tfunc->ParNameValid(spname))
      {
        Error(DQERR_FUNCPAR_NAME_INVALID, spname, &scf->prevpos);
        scf->ReadTo(",)");  // try to skip to next parameter
        continue;
      }

      scf->SkipWhite();
      if (not scf->CheckSymbol(":"))
      {
        Error(DQERR_TYPE_SPECIFIER_EXP_AFTER, spname, &scf->prevpos);
        scf->ReadTo(",)");  // try to skip to next parameter
        continue;
      }

      OType * ptype = ParseTypeSpec();
      if (!ptype)
      {
        scf->ReadTo(",)");  // try to skip to next parameter
        continue;
      }

      // OK
      tfunc->AddParam(spname, ptype);

    }  // while function parameters
  }

  if (tfunc->has_varargs && !aexternal)
  {
    Error(DQERR_VARARGS_NOT_ALLOWED);
  }
  else if (tfunc->has_varargs && tfunc->params.empty())
  {
    Error(DQERR_VARARGS_ALONE);
  }

  scf->SkipWhite();
  if (scf->CheckSymbol("->"))  // return type
  {
    scf->SkipWhite();
    string frtname;
    if (not scf->ReadIdentifier(frtname))
    {
      Error(DQERR_FUNC_RETTYPE_EXPECTED);
    }
    else
    {
      tfunc->rettype = cur_mod_scope->FindType(frtname);
      if (not tfunc->rettype)
      {
        Error(DQERR_TYPE_UNKNOWN, frtname);
      }
    }
  }

  if (aexternal)
  {
    vsfunc->is_external = true;
  }

  AddDeclFunc(scpos_statement_start, vsfunc);

  if (aexternal)
  {
    // external functions have no body, expect ";"
    scf->SkipWhite();
    if (not scf->CheckSymbol(";"))
    {
      Error(DQERR_FUNC_NO_BODY_ALLOWED_AFTER, "external function declaration");
    }
    curvsfunc = nullptr;
    return;
  }

  // go on with the function body

  ReadStatementBlock(vsfunc->body, "endfunc");

  vsfunc->scpos_endfunc = scf->prevpos;

  // check if the result is set
  if (vsfunc->vsresult and not vsfunc->vsresult->initialized)
  {
    Error(DQERR_FUNC_RESULT_NOT_SET, vsfunc->name, &vsfunc->scpos_endfunc);
  }

  curvsfunc = nullptr;
}

void ODqCompParser::ReadStatementBlock(OStmtBlock * stblock, const string blockend, string * rendstr)
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

    // Try keywords first, use ReadIdentifier for whole word checking

    scf->SaveCurPos(scpos_statement_start);  // we jump back here if the identifier is unknown
    if (scf->ReadIdentifier(sid))
    {
      if ("var" == sid)  // local variable declaration
      {
        ParseStmtVar(false);
        continue;
      }
      else if ("return" == sid)
      {
        ParseStmtReturn();
        continue;
      }
      else if ("while" == sid)
      {
        ParseStmtWhile();
        continue;
      }
      else if ("if" == sid)
      {
        ParseStmtIf();
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

    EBinOp binop = ODqCompParser::ParseAssignOp();
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

      scf->SkipWhite();
      if (!scf->CheckSymbol(";"))
      {
        OScPosition scpos;
        scf->SaveCurPos(scpos);
        StatementError(DQERR_MISSING_SEMICOLON_TO_CLOSE, "assignment statement", &scpos);
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

    // the leftexpr should be a function call
    OCallExpr * callexpr = dynamic_cast<OCallExpr *>(leftexpr);
    if (!callexpr)
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
    scf->SkipWhite();
    if (!scf->CheckSymbol(";"))
    {
      OScPosition scpos;
      scf->SaveCurPos(scpos);
      StatementError(DQERR_MISSING_SEMICOLON_TO_CLOSE, "function call statement", &scpos);
    }

    FinalizeStmtVoidCall(callexpr);

  }

  curscope = prev_scope;
  curblock = prev_block;
}

OType * ODqCompParser::ParseTypeSpec(bool aemit_errors)
{
  // Parses type specification after ":"
  // Handles: ^type (pointer), type[N] (fixed array), type[] (slice)

  int pointer_level = 0;
  scf->SkipWhite();
  while (scf->CheckSymbol("^"))
  {
    ++pointer_level;
    scf->SkipWhite();
  }
  bool is_pointer = (pointer_level > 0);

  string stype;
  scf->SkipWhite();
  if (not scf->ReadIdentifier(stype))
  {
    if (aemit_errors)
    {
      Error(DQERR_TYPE_ID_EXP);
    }
    return nullptr;
  }

  OType * ptype = cur_mod_scope->FindType(stype);
  if (not ptype)
  {
    if (aemit_errors)
    {
      Error(DQERR_TYPE_UNKNOWN, stype, &scf->prevpos);
    }
    return nullptr;
  }
  ptype = ptype->ResolveAlias();

  while (pointer_level > 0)
  {
    ptype = ptype->GetPointerType();
    --pointer_level;
  }

  // cstring[N] handling: [N] means sized cstring, not array
  if (TK_STRING == ptype->kind and not is_pointer)
  {
    scf->SkipWhite();
    if (scf->CheckSymbol("["))
    {
      int64_t maxlen;
      if (not scf->ReadInt64Value(maxlen))
      {
        if (aemit_errors)
        {
          Error(DQERR_CSTR_SIZE_EXPECTED);
        }
        return nullptr;
      }
      if (maxlen <= 0)
      {
        if (aemit_errors)
        {
          Error(DQERR_CSTR_SIZE_EXPECTED);
        }
        return nullptr;
      }
      scf->SkipWhite();
      if (not scf->CheckSymbol("]"))
      {
        if (aemit_errors)
        {
          Error(DQERR_MISSING_CLOSE_BRACKET_FOR, "cstring size");
        }
        return nullptr;
      }
      return g_builtins->type_cstring->GetSizedType(uint32_t(maxlen));
    }
    return ptype;  // unsized cstring (for parameters)
  }

  // Check for array suffix: [N] or []
  scf->SkipWhite();
  if (scf->CheckSymbol("["))
  {
    if (is_pointer)
    {
      if (aemit_errors)
      {
        Error(DQERR_NOT_SUPPORTED, "Pointer-to-array");
      }
      return nullptr;
    }

    scf->SkipWhite();
    if (scf->CheckSymbol("]"))
    {
      // Empty brackets: type[] — array slice
      ptype = ptype->GetSliceType();
    }
    else if (scf->CheckSymbol("..."))
    {
      if (aemit_errors)
      {
        Error(DQERR_NOT_IMPLEMENTED_YET, "Dynamic array (int[...])");
      }
      scf->ReadTo("]");
      scf->CheckSymbol("]");
      return nullptr;
    }
    else
    {
      // Read array size: type[N]
      int64_t arrlen;
      if (not scf->ReadInt64Value(arrlen))
      {
        if (aemit_errors)
        {
          Error(DQERR_ARRAY_SIZESPEC);
        }
        return nullptr;
      }
      if (arrlen <= 0)
      {
        if (aemit_errors)
        {
          Error(DQERR_SIZE_SPEC, "Array");
        }
        return nullptr;
      }
      scf->SkipWhite();
      if (not scf->CheckSymbol("]"))
      {
        if (aemit_errors)
        {
          Error(DQERR_MISSING_CLOSE_BRACKET_FOR, "array size");
        }
        return nullptr;
      }
      ptype = ptype->GetArrayType(uint32_t(arrlen));
    }
  }

  return ptype;
}

void ODqCompParser::ParseStmtReturn()
{
  // "return" is already consumed.
  scf->SkipWhite();
  if (scf->CheckSymbol(";"))  // return without value, use the result variable to return
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
  scf->SkipWhite();
  if (!scf->CheckSymbol(";"))
  {
    Error(DQERR_MISSING_SEMICOLON_AFTER, "the return expression");
  }
  if (expr)
  {
    curblock->scope->SetVarInitialized(curvsfunc->vsresult);
    curblock->AddStatement(new OStmtReturn(scpos_statement_start, expr, curvsfunc));
  }
}

void ODqCompParser::ParseStmtWhile()
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

  ReadStatementBlock(st->body, "endwhile");

  st->body->scope->RevertFirstAssignments();
}

void ODqCompParser::ParseStmtIf()
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

OExpr * ODqCompParser::ParseExpression()
{
  return FoldExprTree(ParseExprOr());
}

OExpr * ODqCompParser::ParseExprOr()
{
  OExpr * left = ParseExprAnd();
  if (!left) return nullptr;

  while (not scf->Eof())
  {
    scf->SkipWhite();
    if (scf->CheckSymbol("or"))
    {
      OExpr * right = ParseExprAnd();
      if (!right)
      {
        return FreeLeftRight(left, nullptr);
      }
      left = new OLogicalExpr(LOGIOP_OR, left, right);
      continue;
    }

    break;
  }
  return left;
}

OExpr * ODqCompParser::ParseExprAnd()
{
  OExpr * left = ParseExprNot();
  if (!left) return nullptr;

  while (not scf->Eof())
  {
    scf->SkipWhite();
    if (scf->CheckSymbol("and"))
    {
      OExpr * right = ParseExprNot(); // Fixed recursion to ParseExprNot (was ParseExprAnd in original logic, but usually it chains to next priority or self)
      if (!right)
      {
        return FreeLeftRight(left, nullptr);
      }
      left = new OLogicalExpr(LOGIOP_AND, left, right);
      continue;
    }

    break;
  }
  return left;
}

OExpr * ODqCompParser::ParseExprNot()
{
  scf->SkipWhite();
  if (scf->CheckSymbol("not"))
  {
    OExpr *  val = ParseExprNot();
    if (!val)
    {
      return nullptr;
    }
    return new ONotExpr(val);
  }

  return ParseComparison();
}

OExpr * ODqCompParser::ParseComparison()
{
  OExpr *  left = ParseExprAdd();
  if (!left)
  {
    return nullptr;
  }

  scf->SkipWhite();

  ECompareOp op = COMPOP_NONE;

  // check first the ambigous expression terminators
  if (scf->CheckSymbol("<<=", false) or scf->CheckSymbol(">>=", false))
  {
    return left;
  }

  if      (scf->CheckSymbol("=="))    op = COMPOP_EQ;
  else if (scf->CheckSymbol("!=") or
           scf->CheckSymbol("<>"))    op = COMPOP_NE;
  else if (scf->CheckSymbol("<="))    op = COMPOP_LE;  // <= before <
  else if (scf->CheckSymbol("<"))     op = COMPOP_LT;
  else if (scf->CheckSymbol(">="))    op = COMPOP_GE;  // >= before >
  else if (scf->CheckSymbol(">"))     op = COMPOP_GT;
  else
  {
    return left;
  }

  OExpr *  right = ParseExprAdd();
  if (!right)
  {
    return FreeLeftRight(left, nullptr);
  }

  // Widen int operands to matching width before comparison
  ETypeKind tkl = left->ptype->kind;
  ETypeKind tkr = right->ptype->kind;
  if (TK_INT == tkl and TK_INT == tkr)
  {
    OTypeInt * intl = static_cast<OTypeInt *>(left->ResolvedType());
    OTypeInt * intr = static_cast<OTypeInt *>(right->ResolvedType());
    if (intl->bitlength != intr->bitlength)
    {
      if (intl->bitlength > intr->bitlength)
        right = new OExprTypeConv(left->ptype, right);
      else
        left = new OExprTypeConv(right->ptype, left);
    }
  }
  else if ((TK_INT == tkl) and (TK_FLOAT == tkr))
  {
    left = new OExprTypeConv(right->ptype, left);
  }
  else if ((TK_INT == tkr) and (TK_FLOAT == tkl))
  {
    right = new OExprTypeConv(left->ptype, right);
  }
  else if (TK_FLOAT == tkl and TK_FLOAT == tkr)
  {
    OTypeFloat * floatl = static_cast<OTypeFloat *>(left->ResolvedType());
    OTypeFloat * floatr = static_cast<OTypeFloat *>(right->ResolvedType());
    if (floatl->bitlength != floatr->bitlength)
    {
      if (floatl->bitlength > floatr->bitlength)
        right = new OExprTypeConv(left->ptype, right);
      else
        left = new OExprTypeConv(right->ptype, left);
    }
  }

  return new OCompareExpr(op, left, right);
}

// Table-driven binary operator parser: shared logic for all precedence levels
OExpr * ODqCompParser::ParseBinOpLevel(OExpr * (ODqCompParser::*parse_next)(), const BinOpEntry ops[], int nops)
{
  scf->SkipWhite();
  OExpr * left = (this->*parse_next)();
  if (!left) return nullptr;

  while (not scf->Eof())
  {
    scf->SkipWhite();

    // check first the ambigous expression terminators
    if (    scf->CheckSymbol("+=", false)
         or scf->CheckSymbol("-=", false)
         or scf->CheckSymbol("*=", false)
         or scf->CheckSymbol("/=", false)
         or scf->CheckSymbol("<<=", false)
         or scf->CheckSymbol(">>=", false)  )
    {
      break;
    }

    EBinOp op = BINOP_NONE;
    bool blocked_assignop = false;
    for (int i = 0; i < nops; ++i)
    {
      if (scf->CheckSymbol(ops[i].sym))
      {
        op = ops[i].op;
        break;
      }
    }
    if (op == BINOP_NONE)  break;

    OExpr * right = (this->*parse_next)();
    OExpr * res = CreateBinExpr(op, left, right);
    if (!res) return FreeLeftRight(left, right);
    left = res;
  }
  return left;
}

OExpr * ODqCompParser::ParseExprAdd()
{
  static const BinOpEntry ops[] = { {"+", BINOP_ADD}, {"-", BINOP_SUB} };
  return ParseBinOpLevel(&ODqCompParser::ParseExprMul, ops, 2);
}

OExpr * ODqCompParser::ParseExprMul()
{
  static const BinOpEntry ops[] = { {"*", BINOP_MUL} };
  return ParseBinOpLevel(&ODqCompParser::ParseExprDiv, ops, 1);
}

OExpr * ODqCompParser::ParseExprDiv()
{
  static const BinOpEntry ops[] = { {"/", BINOP_DIV}, {"IDIV", BINOP_IDIV}, {"IMOD", BINOP_IMOD} };
  return ParseBinOpLevel(&ODqCompParser::ParseExprBinOr, ops, 3);
}

OExpr * ODqCompParser::ParseExprBinOr()
{
  static const BinOpEntry ops[] = { {"OR", BINOP_IOR}, {"XOR", BINOP_IXOR} };
  return ParseBinOpLevel(&ODqCompParser::ParseExprBinAnd, ops, 2);
}

OExpr * ODqCompParser::ParseExprBinAnd()
{
  static const BinOpEntry ops[] = { {"AND", BINOP_IAND} };
  return ParseBinOpLevel(&ODqCompParser::ParseExprShift, ops, 1);
}

OExpr * ODqCompParser::ParseExprShift()
{
  static const BinOpEntry ops[] = { {"<<", BINOP_ISHL}, {"SHL", BINOP_ISHL}, {">>", BINOP_ISHR}, {"SHR", BINOP_ISHR} };
  return ParseBinOpLevel(&ODqCompParser::ParseExprUnary, ops, 4);
}

OExpr * ODqCompParser::ParseExprUnary()
{
  scf->SkipWhite();

  // address-of operator: consume a full postfix-capable lvalue operand
  if (scf->CheckSymbol("&"))
  {
    OLValueExpr * lval = ParseAddressableExpr();
    if (!lval) return nullptr;
    return new OAddrOfExpr(lval);
  }

  if (scf->CheckSymbol("-"))
  {
    OExpr * val = ParseExprUnary();
    if (!val) return nullptr;
    return new ONegExpr(val);
  }

  if (scf->CheckSymbol("NOT"))
  {
    OExpr * val = ParseExprUnary();
    if (!val) return nullptr;
    return new OBinNotExpr(val);
  }

  return ParseExprPostfix();
}

OLValueExpr * ODqCompParser::ParseAddressableExpr()
{
  OExpr * expr = ParseExprPostfix();
  if (!expr) return nullptr;

  OLValueExpr * lval = dynamic_cast<OLValueExpr *>(expr);
  if (!lval)
  {
    Error(DQERR_EXPR_INVALID_ADDROF);  // Address-of requires an lvalue expression;
    delete expr;
    return nullptr;
  }

  OValSym * varref = dynamic_cast<OLValueVar *>(lval) ? static_cast<OLValueVar *>(lval)->pvalsym : nullptr;
  if (varref and VSK_VARIABLE != varref->kind and VSK_PARAMETER != varref->kind)
  {
    Error(DQERR_EXPR_VS_NOT_ADDRESSABLE, varref->name);
    delete expr;
    return nullptr;
  }

  return lval;
}

OExpr * ODqCompParser::ParseExprPostfix()
{
  OExpr * result = ParseExprPrimary();
  if (!result) return nullptr;
  return ParsePostfix(result);
}

OExpr * ODqCompParser::ParseExplicitCastExpr(bool * rattempted)
{
  if (rattempted)
  {
    *rattempted = false;
  }

  OScPosition saved_pos;
  scf->SaveCurPos(saved_pos);

  OType * dsttype = ParseTypeSpec(false);
  if (!dsttype)
  {
    scf->SetCurPos(saved_pos);
    return nullptr;
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol("("))
  {
    scf->SetCurPos(saved_pos);
    return nullptr;
  }

  if (rattempted)
  {
    *rattempted = true;
  }

  OExpr * srcexpr = ParseExpression();
  if (!srcexpr)
  {
    return nullptr;
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol(")"))
  {
    delete srcexpr;
    Error(DQERR_MISSING_CLOSE_PAREN_FOR, "cast");
    return nullptr;
  }

  OExpr * result = nullptr;
  if (!ConvertExprToType(dsttype, srcexpr, &result, EXPCF_GENERATE_ERRORS | EXPCF_EXPLICIT_CAST))
  {
    delete srcexpr;
    return nullptr;
  }

  return result;
}

OExpr * ODqCompParser::ParsePostfix(OExpr * base)
{
  OExpr * result = base;
  if (!result) return nullptr;

  while (true)
  {
    scf->SkipWhite();

    if (!result->ptype)
    {
      break;  // void function call: no postfix operations are possible
    }

    ETypeKind      tk   = result->ptype->kind;
    OLValueExpr *  lval = dynamic_cast<OLValueExpr *>(result);

    if (lval)
    {
      // Struct member access on a compound lvalue or a ^compound pointer: x.field / p.field
      if (scf->CheckSymbol("."))
      {
        OLValueExpr * memberbase = nullptr;
        OCompoundType * ctype = nullptr;
        if (!ResolveCompoundMemberBase(lval, lval->ptype, memberbase, ctype))
        {
          OTypePointer * ptrtype = dynamic_cast<OTypePointer *>(lval->ResolvedType());
          if (ptrtype && ptrtype->IsOpaquePointer())
          {
            Error(DQERR_PTR_OPAQUE_USAGE, "member access");
            delete result;
            return nullptr;
          }
          else
          {
            Error(DQERR_TYPE_NO_MEMBERS);
          }
          return result;
        }

        string membername;
        scf->SkipWhite();
        if (not scf->ReadIdentifier(membername))
        {
          Error(DQERR_MEMBER_NAME_EXPECTED);
          return result;
        }
        int midx = ctype->FindMemberIndex(membername);
        if (midx < 0)
        {
          Error(DQERR_MEMBER_UNKNOWN, membername, ctype->name);
          return result;
        }
        OType * mtype = ctype->member_order[midx]->ptype;
        result = new OLValueMember(memberbase, ctype, midx, mtype);
        continue;
      }

      // Array/slice/cstring index on any lvalue: x[i]
      if ((TK_ARRAY == tk or TK_ARRAY_SLICE == tk or TK_STRING == tk)
          and scf->CheckSymbol("["))
      {
        OExpr * indexexpr = ParseExpression();
        scf->SkipWhite();
        if (not scf->CheckSymbol("]"))
        {
          Error(DQERR_MISSING_CLOSE_BRACKET_AFTER, "index");
        }
        result = new OLValueIndex(lval, lval->ptype, indexexpr);
        continue;
      }

      // Function call: f(args)
      OLValueVar * varref = dynamic_cast<OLValueVar *>(lval);
      if (varref)
      {
        OValSymFunc * vsfunc = dynamic_cast<OValSymFunc *>(varref->pvalsym);
        if (vsfunc)
        {
          if (not scf->CheckSymbol("("))
          {
            Error(DQERR_FUNC_CALL_PARENTH, vsfunc->name);
          }
          OExpr * callexpr = ParseExprFuncCall(vsfunc);
          delete result;
          result = callexpr;
          if (!result) return nullptr;
          continue;
        }
      }
    }

    // pointer operations — apply to any expression (not just lvalue)
    if (TK_POINTER == tk)
    {
      OTypePointer * ptrtype = static_cast<OTypePointer *>(result->ResolvedType());
      if (scf->CheckSymbol("[")) // p[i]: pointer indexing, no dereference
      {
        if (!ptrtype->IsTypedPointer())
        {
          Error(DQERR_PTR_OPAQUE_USAGE, "pointer indexing");
          delete result;
          return nullptr;
        }

        OExpr * indexexpr = ParseExpression();
        scf->SkipWhite();
        if (not scf->CheckSymbol("]"))
        {
          Error(DQERR_MISSING_CLOSE_BRACKET_AFTER, "pointer index");
        }
        result = new OPointerIndexExpr(result, indexexpr);
        continue;
      }

      if (scf->CheckSymbol("^")) // p^: dereference -> lvalue
      {
        if (!ptrtype->IsTypedPointer())
        {
          Error(DQERR_PTR_OPAQUE_USAGE, "dereference");
          delete result;
          return nullptr;
        }
        result = new OLValueDeref(result);
        continue;
      }
    }

    break;
  }

  return result;
}

OExpr * ODqCompParser::ParseExprPrimary()
{
  OExpr * result = nullptr;

  scf->SkipWhite();

  if (*scf->curp == '^'
      || ((*scf->curp >= 'A' && *scf->curp <= 'Z')
          || (*scf->curp >= 'a' && *scf->curp <= 'z')
          || (*scf->curp == '_')))
  {
    bool attempted_cast = false;
    result = ParseExplicitCastExpr(&attempted_cast);
    if (result)
    {
      return result;
    }
    if (attempted_cast)
    {
      return nullptr;
    }
  }

  if (scf->CheckSymbol("("))
  {
    result = ParseExpression();
    scf->SkipWhite();
    if (!scf->CheckSymbol(")"))
    {
      Error(DQERR_MISSING_CLOSE_PAREN);
    }
    return result;
  }

  if (scf->CheckSymbol("["))
  {
    return ParseArrayLit();
  }

  if (scf->CheckSymbol("0x"))  // hex number ?
  {
    uint64_t  hexval;
    if (scf->ReadHex64Value(hexval))
    {
      result = new OIntLit(int64_t(hexval));
    }
    else
    {
      ErrorTxt(DQERR_LIT_HEXNUM, "hexadecimal numbers expected after \"0x\"");
    }
    return result;
  }

  if (scf->IsNumChar())  // '0' .. '9' ?
  {
    int64_t  intval;
    if (scf->ReadInt64Value(intval))
    {
      // check for floating point: 0.123, 2.1e-5, 1.234E6, 0.
      char c = *scf->curp;
      if (('.' == c) or ('e' == c) or ('E' == c)) // convert to floating point
      {
        double fpval = intval;
        if (not scf->ReadFloatFracExp(fpval))
        {
          Error(DQERR_LIT_FLOAT);
        }
        result = new OFloatLit(fpval);
      }
      else
      {
        result = new OIntLit(intval);
      }
    }
    else  // impossible case
    {
      Error(DQERR_LIT_INT);
    }
    return result;
  }

  // String literal: "..." or '...'
  if (*scf->curp == '"' or *scf->curp == '\'')
  {
    string strval;
    if (scf->ReadQuotedString(strval))
    {
      return new OCStringLit(strval);
    }
    else
    {
      ErrorTxt(DQERR_LIT_STRING, "Unterminated string literal");
      return nullptr;
    }
  }

  if (scf->CheckSymbol("true"))
  {
    result = new OBoolLit(true);
    return result;
  }

  if (scf->CheckSymbol("false"))
  {
    result = new OBoolLit(false);
    return result;
  }

  if (scf->CheckSymbol("null"))
  {
    result = new ONullLit();
    return result;
  }

  if (scf->CheckSymbol("@"))
  {
    OScPosition scpos_ns = scf->prevpos;
    OValSym * vs = ResolveNamespaceValSym();
    if (!vs)
    {
      return nullptr;
    }

    result = new OLValueVar(vs);
    if (vs->kind != VSK_FUNCTION and not vs->initialized)
    {
      VarInitError(static_cast<OLValueVar *>(result), vs, scpos_ns);
    }
    return result;
  }

  // identifier

  OScPosition scpos_sid;
  scf->SaveCurPos(scpos_sid);

  string  sid;
  if (!scf->ReadIdentifier(sid))
  {
    if (!scf->Eof())
    {
      Error(DQERR_EXPR_UNEXPECTED_CHAR, string(1, *scf->curp));
    }
    else
    {
      Error(DQERR_EXPR_EXPECTED);
    }
    return result;
  }

  // builtin specials

  if ("len" == sid)
  {
    return ParseBuiltinLen();
  }

  if ("iif" == sid)
  {
    return ParseBuiltinIif();
  }

  if ("sizeof" == sid)
  {
    return ParseBuiltinSizeof();
  }

  if ("round" == sid)  return ParseBuiltinFloatRound(RNDMODE_ROUND);
  if ("ceil"  == sid)  return ParseBuiltinFloatRound(RNDMODE_CEIL);
  if ("floor" == sid)  return ParseBuiltinFloatRound(RNDMODE_FLOOR);

  OValSym * vs = curscope->FindValSym(sid);
  if (!vs)
  {
    Error(DQERR_VS_UNKNOWN, sid);
    return result;
  }

  result = new OLValueVar(vs);
  if (vs->kind != VSK_FUNCTION and not vs->initialized)
  {
    VarInitError(static_cast<OLValueVar *>(result), vs, scpos_sid);
  }

  return result;
}

OValSym * ODqCompParser::ResolveNamespaceValSym()
{
  string nsname;
  string symname;

  if (scf->CheckSymbol("."))
  {
    nsname = ".";
  }
  else if (!scf->ReadIdentifier(nsname))
  {
    Error(DQERR_NS_NAME_EXPECTED);
    return nullptr;
  }

  if ("." != nsname && !scf->CheckSymbol("."))
  {
    Error(DQERR_DOT_MISSING_AFTER_NS_NAME);
    return nullptr;
  }

  if (!scf->ReadIdentifier(symname))
  {
    Error(DQERR_ID_EXP_AFTER, "@"+nsname);
    return nullptr;
  }

  auto it = g_namespaces.find(nsname);
  if (it == g_namespaces.end())
  {
    Error(DQERR_NS_UNKNOWN, "@"+nsname);
    return nullptr;
  }

  OValSym * vs = it->second->FindValSym(symname, nullptr, true);
  if (!vs)
  {
    Error(DQERR_VS_UNKNOWN_IN_NAMESPACE, symname, "@"+nsname);
    return nullptr;
  }

  return vs;
}

OExpr * ODqCompParser::ParseArrayLit()
{
  // "[" is already consumed
  vector<OExpr *> elems;

  while (not scf->Eof())
  {
    scf->SkipWhite();
    if (scf->CheckSymbol("]"))
    {
      break;
    }

    if (elems.size() > 0)
    {
      if (not scf->CheckSymbol(","))
      {
        Error(DQERR_MISSING_COMMA_IN, "array literal");
      }
    }

    OExpr * val = ParseExpression();
    if (val)
    {
      elems.push_back(val);
    }
  }

  return new OArrayLit(elems);
}

OExpr * ODqCompParser::ParseExprFuncCall(OValSymFunc * vsfunc)
{
  // function name and "(" was already consumed

  OCallExpr * result = new OCallExpr(vsfunc);
  OTypeFunc * tfunc = static_cast<OTypeFunc *>(vsfunc->ptype);
  bool        bok = true;

  // parse and check the arguments
  int pcnt = 0;
  while (true)
  {
    scf->SkipWhite();
    if (scf->CheckSymbol(")"))
    {
      break;
    }

    if ((pcnt > 0) and not scf->CheckSymbol(","))
    {
      Error(DQERR_FUNC_ARGS_LIST, "\",\" or \")\" is missing at function \"$1\"call arguments", vsfunc->name);
      bok = false;
      break;
    }

    if (pcnt >= (int)tfunc->params.size() && !tfunc->has_varargs)
    {
      Error(DQERR_FUNC_ARGS_TOO_MANY, vsfunc->name, to_string(tfunc->params.size()));
      bok = false;
      break;
    }

    OExpr * argexpr = ParseExpression();
    if (!argexpr)
    {
      // error message already produced ?
      bok = false;
      break;
    }

    result->AddArgument(argexpr);  // to avoid memory leak, this must come before the type check

    if (pcnt < (int)tfunc->params.size())
    {
      OType * argtype = tfunc->params[pcnt]->ptype;
      if (not CheckAssignType(argtype, &argexpr, "Argument"))
      {
        bok = false;
        break;
      }
      // CheckAssignType may have replaced argexpr (e.g. array->slice conversion)
      result->args[pcnt] = argexpr;
    }

    ++pcnt;
  }

  if (result->args.size() < tfunc->params.size())
  {
    Error(DQERR_FUNC_ARGS_TOO_FEW, vsfunc->name, to_string(result->args.size()), to_string(tfunc->params.size()));
    bok = false;
  }

  if (!bok)
  {
    delete result;
    return nullptr;
  }

  return result;
}

OExpr * ODqCompParser::ParseBuiltinIif()
{
  auto recover_iif_tail = [this]()
  {
    scf->ReadTo(");");
    scf->CheckSymbol(")");
  };

  scf->SkipWhite();
  if (not scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, "iif");
    return nullptr;
  }

  OExpr * condexpr = ParseExpression();
  if (!condexpr)
  {
    Error(DQERR_FUNC_ARGS_TOO_FEW, "0", "iif", "3");
    return nullptr;
  }

  OType * condtype = condexpr->ResolvedType();
  if (!condtype || (TK_BOOL != condtype->kind))
  {
    Error(DQERR_BOOL_EXPR_EXPECTED, condtype ? condtype->name : "void");
    recover_iif_tail();
    delete condexpr;
    return nullptr;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(","))
  {
    Error(DQERR_FUNC_ARGS_TOO_FEW, "1", "iif", "3");
    recover_iif_tail();
    delete condexpr;
    return nullptr;
  }

  OExpr * trueexpr = ParseExpression();
  if (!trueexpr)
  {
    Error(DQERR_FUNC_ARGS_TOO_FEW, "iif", "1", "3");
    delete condexpr;
    return nullptr;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(","))
  {
    Error(DQERR_FUNC_ARGS_TOO_FEW, "2", "iif", "3");
    recover_iif_tail();
    delete condexpr;
    delete trueexpr;
    return nullptr;
  }

  OExpr * falseexpr = ParseExpression();
  if (!falseexpr)
  {
    Error(DQERR_FUNC_ARGS_TOO_FEW, "2", "iif", "3");
    recover_iif_tail();
    delete condexpr;
    delete trueexpr;
    return nullptr;
  }

  scf->SkipWhite();
  if (scf->CheckSymbol(","))
  {
    Error(DQERR_FUNC_ARGS_TOO_MANY, "iif", "3");
    recover_iif_tail();
    delete condexpr;
    delete trueexpr;
    delete falseexpr;
    return nullptr;
  }

  if (not scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_FOR, "iif");
    recover_iif_tail();
    delete condexpr;
    delete trueexpr;
    delete falseexpr;
    return nullptr;
  }

  OType * resulttype = nullptr;
  if (not ResolveIifType(&trueexpr, &falseexpr, &resulttype))
  {
    delete condexpr;
    delete trueexpr;
    delete falseexpr;
    return nullptr;
  }

  return FoldExprTree(new OIifExpr(condexpr, trueexpr, falseexpr, resulttype));
}

OExpr * ODqCompParser::ParseBuiltinFloatRound(ERoundMode amode)
{
  scf->SkipWhite();
  if (not scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, GetRoundModeName(amode));
    return nullptr;
  }
  OExpr * argexpr = ParseExpression();
  if (!argexpr)  return nullptr;

  if (TK_FLOAT != argexpr->ptype->kind)
  {
    Error(DQERR_TYPE_FLOAT_EXPECTED_FOR, GetRoundModeName(amode), argexpr->ptype->name);
    delete argexpr;
    return nullptr;
  }
  scf->SkipWhite();
  if (not scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_FOR, GetRoundModeName(amode));
    delete argexpr;
    return nullptr;
  }
  return new OFloatRoundExpr(amode, argexpr);
}

OExpr * ODqCompParser::ParseBuiltinLen()
{
  scf->SkipWhite();
  if (not scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, "len");
    return nullptr;
  }
  scf->SkipWhite();
  string lenarg;
  if (not scf->ReadIdentifier(lenarg))
  {
    Error(DQERR_VARNAME_EXP_AFTER, "len");
    return nullptr;
  }
  OValSym * lenvs = curscope->FindValSym(lenarg);
  if (!lenvs)
  {
    Error(DQERR_VAR_UNKNOWN, lenarg);
    return nullptr;
  }
  scf->SkipWhite();
  if (not scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_FOR, "len");
    return nullptr;
  }
  if (TK_ARRAY == lenvs->ptype->kind)
  {
    OTypeArray * arrtype = static_cast<OTypeArray *>(lenvs->ptype);
    return new OIntLit(arrtype->arraylength);
  }
  else if (TK_ARRAY_SLICE == lenvs->ptype->kind)
  {
    return new OSliceLengthExpr(lenvs);
  }
  else if (TK_STRING == lenvs->ptype->kind)
  {
    return new OCStringLenExpr(lenvs);
  }
  else
  {
    Error(DQERR_LEN_INVALID_TYPE, lenvs->ptype->name);
    return nullptr;
  }
}

OExpr * ODqCompParser::ParseBuiltinSizeof()
{
  scf->SkipWhite();
  if (not scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, "sizeof");
    return nullptr;
  }
  scf->SkipWhite();

  OType * sizetype = nullptr;
  OScPosition argpos;
  scf->SaveCurPos(argpos);

  if (scf->CheckSymbol("^", false))
  {
    sizetype = ParseTypeSpec();
  }
  else
  {
    string sarg;
    if (not scf->ReadIdentifier(sarg))
    {
      ErrorTxt(DQERR_VARNAME_EXP_AFTER, "Variable or type name is expected after \"sizeof\"");
      return nullptr;
    }

    OValSym * vs = curscope->FindValSym(sarg);
    if (vs)
    {
      sizetype = vs->ptype;
    }
    else
    {
      OType * foundtype = cur_mod_scope->FindType(sarg);
      if (!foundtype)
      {
        ErrorTxt(DQERR_EXPR_EXPECTED, "sizeof() expects a variable or type name");
        return nullptr;
      }

      scf->SetCurPos(argpos);
      sizetype = ParseTypeSpec();
    }
  }

  if (!sizetype)
  {
    return nullptr;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_FOR, "sizeof");
    return nullptr;
  }

  if (0 == sizetype->bytesize)
  {
    ErrorTxt(DQERR_NOT_SUPPORTED, "sizeof() requires a statically sized type or value");
    return nullptr;
  }

  return new OIntLit(sizetype->bytesize);
}

EBinOp ODqCompParser::ParseAssignOp()
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

void ODqCompParser::VarInitError(OLValueVar * varexpr, OValSym * valsym, OScPosition & scpos)
{
  if (supress_varinit_check)
  {
    AddSuppressedVarInitDiag(varexpr, valsym, scpos);
  }
  else
  {
    Error(DQERR_VAR_NOT_INITIALIZED, valsym->name, &scpos);
  }
}

void ODqCompParser::AddSuppressedVarInitDiag(OLValueVar * varexpr, OValSym * valsym, OScPosition & scpos)
{
  TSuppressedVarInitDiag diag;
  diag.varexpr = varexpr;
  diag.valsym = valsym;
  diag.scpos = scpos;
  suppressed_varinit_diags.push_back(diag);
}

void ODqCompParser::EmitSuppressedVarInitDiags()
{
  if (suppressed_varinit_diags.empty())
  {
    return;
  }

  for (auto & diag : suppressed_varinit_diags)
  {
    Error(DQERR_VAR_NOT_INITIALIZED, diag.valsym->name, &diag.scpos);
  }

  suppressed_varinit_diags.clear();
}

void ODqCompParser::EmitFilteredAssignVarInitDiags(OLValueExpr * leftexpr, EBinOp op)
{
  if (suppressed_varinit_diags.empty())
  {
    return;
  }

  vector<OLValueVar *> ignored;
  if (BINOP_NONE == op)
  {
    CollectIgnoredPlainAssignVars(leftexpr, ignored);
  }

  for (auto & diag : suppressed_varinit_diags)
  {
    bool emit = true;

    if (BINOP_NONE == op)
    {
      for (OLValueVar * ignoredvar : ignored)
      {
        if (ignoredvar == diag.varexpr)
        {
          emit = false;
          break;
        }
      }
    }

    if (emit)
    {
      Error(DQERR_VAR_NOT_INITIALIZED, diag.valsym->name, &diag.scpos);
    }
  }

  suppressed_varinit_diags.clear();
}

bool ODqCompParser::FinalizeStmtAssign(OLValueExpr * leftexpr, EBinOp op, OExpr * rightexpr)
{
  if (!leftexpr || !rightexpr)
  {
    delete leftexpr;
    delete rightexpr;
    return false;
  }

  OValSym * rootvalsym = GetAssignRootValSym(leftexpr);
  if (rootvalsym && VSK_CONST == rootvalsym->kind)
  {
    Error(DQERR_TYPE_ASSIGN_TO_CONST, rootvalsym->name);
    delete leftexpr;
    delete rightexpr;
    return false;
  }

  OType * targettype = leftexpr->ptype;

  // Pointer arithmetic: p += int  or  p -= int
  if (TK_POINTER == targettype->kind and (BINOP_ADD == op or BINOP_SUB == op))
  {
    OTypePointer * ptrtype = static_cast<OTypePointer *>(targettype->ResolveAlias());
    if (!ptrtype->IsTypedPointer())
    {
      Error(DQERR_PTR_OPAQUE_USAGE, "pointer arithmetic");
      delete leftexpr;
      delete rightexpr;
      return false;
    }

    if (TK_INT != rightexpr->ptype->kind)
    {
      Error(DQERR_PTRARITH_TYPE, rightexpr->ptype->name);
      delete leftexpr;
      delete rightexpr;
      return false;
    }

    curblock->AddStatement(new OStmtModifyAssign(scpos_statement_start, leftexpr, op, rightexpr));
    return true;
  }

  if (not CheckAssignType(targettype, &rightexpr, "Assignment"))
  {
    delete leftexpr;
    delete rightexpr;
    return false;
  }

  if (BINOP_NONE == op)
  {
    curblock->AddStatement(new OStmtAssign(scpos_statement_start, leftexpr, rightexpr));
    if (rootvalsym && (VSK_VARIABLE == rootvalsym->kind || VSK_PARAMETER == rootvalsym->kind))
    {
      curblock->scope->SetVarInitialized(rootvalsym);
    }
  }
  else
  {
    curblock->AddStatement(new OStmtModifyAssign(scpos_statement_start, leftexpr, op, rightexpr));
  }

  return true;
}

void ODqCompParser::FinalizeStmtVoidCall(OCallExpr * callexpr)
{
  curblock->AddStatement(new OStmtVoidCall(scpos_statement_start, callexpr));
}

bool ODqCompParser::CheckStatementClose()
{
  scf->SkipWhite();
  if (not scf->CheckSymbol(";"))
  {
    StatementError(DQERR_MISSING_SEMICOLON_TO_CLOSE, "previous statement");
    return false;
  }
  return true;
}

OValSymConst * ODqCompParser::ParseDefineConst(const OScPosition & scpos, const string & sid)
{
  OScPosition    saved_stmtpos = scpos_statement_start;
  OScPosition *  saved_errorpos = errorpos;
  OScope *       saved_scope = curscope;
  OStmtBlock *   saved_block = curblock;

  scpos_statement_start = scpos;
  errorpos = &scpos_statement_start;
  curscope = g_defines;
  curblock = nullptr;
  scf->SetDirectiveExprMode(true);

  OValSymConst *  result = nullptr;
  OExpr *         valueexpr = nullptr;

  scf->SkipWhite();
  valueexpr = ParseExpression();
  if (!valueexpr)
  {
    goto cleanup;
  }

  if (!valueexpr->ptype)
  {
    ErrorTxt(DQERR_CDIR_EXPR_TYPE, "Unable to determine the type of #define \"$1\"", sid);
    goto cleanup;
  }

  {
    OType *   valuetype = valueexpr->ResolvedType();
    OValue *  value = valuetype->CreateValue();
    if (!value)
    {
      ErrorTxt(DQERR_CDIR_EXPR_TYPE, "Unsupported #define value type: \"$1\"", valuetype->name);
      goto cleanup;
    }

    if (!value->CalculateConstant(valueexpr))
    {
      delete value;
      goto cleanup;
    }

    result = new OValSymConst(scpos_statement_start, sid, valuetype, value);
  }

cleanup:
  delete valueexpr;

  scpos_statement_start = saved_stmtpos;
  errorpos = saved_errorpos;
  curscope = saved_scope;
  curblock = saved_block;
  scf->SetDirectiveExprMode(false);

  return result;
}

bool ODqCompParser::ParseDefineCondition(const OScPosition & scpos, bool * rok)
{
  OScPosition    saved_stmtpos = scpos_statement_start;
  OScPosition *  saved_errorpos = errorpos;
  OScope *       saved_scope = curscope;
  OStmtBlock *   saved_block = curblock;

  scpos_statement_start = scpos;
  errorpos = &scpos_statement_start;
  curscope = g_defines;
  curblock = nullptr;
  scf->SetDirectiveExprMode(true);

  bool     result = false;
  bool     ok = false;
  OExpr *  valueexpr = nullptr;

  scf->SkipWhite();
  valueexpr = ParseExpression();
  if (!valueexpr)
  {
    goto cleanup;
  }

  if (!valueexpr->ResolvedType() || (TK_BOOL != valueexpr->ResolvedType()->kind))
  {
    ErrorTxt(DQERR_CDIR_EXPR, "Preprocessor condition must be a boolean expression");
    goto cleanup;
  }

  {
    OValueBool cond(g_builtins->type_bool, false);
    if (!cond.CalculateConstant(valueexpr))
    {
      goto cleanup;
    }
    result = cond.value;
    ok = true;
  }

cleanup:
  delete valueexpr;

  scpos_statement_start = saved_stmtpos;
  errorpos = saved_errorpos;
  curscope = saved_scope;
  curblock = saved_block;
  scf->SetDirectiveExprMode(false);

  if (rok)
  {
    *rok = ok;
  }

  return result;
}
