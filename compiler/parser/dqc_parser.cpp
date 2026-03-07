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
#include "otype_array.h"
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

    // module root starters
    if (not scf->ReadIdentifier(sid))
    {
      StatementError("Module statement keyword expected", &scpos_statement_start);
      continue;
    }

    // The module root statement must start with a keyword like
    //   use, module, var, type, function, implementation

    if ("var" == sid) // global variable definition
    {
      ParseVarDecl();
    }
    else if ("const" == sid) // global constant definition
    {
      ParseConstDecl();
    }
    else if ("function" == sid)
    {
      ParseFunction();
      curscope = cur_mod_scope;
      curblock = nullptr;
    }
    else  // unknown
    {
      StatementError("Unknown module root statement qualifier: \"" + sid + "\"", &scpos_statement_start);
    }
  }

  if (g_opt.verbose)
  {
    printf("ParseModule finished.");
  }
}

void ODqCompParser::ParseVarDecl()  // global var declaration (the local var is a statement)
{
  // syntax form: "var identifier : type [ = initial value];"
  // note: "var" is already consumed

  string     sid;
  string     stype;
  OValSym *  pvalsym;
  OType *    ptype;

  scf->SkipWhite();
  if (not scf->ReadIdentifier(sid))
  {
    StatementError("Identifier is expected after \"var\". Syntax: \"var identifier : type [ = initial value];\"");
    return;
  }

  if (g_module->ValSymDeclared(sid, &pvalsym))
  {
    StatementError(format("Variable \"{}\" is already declared with the type \"{}\"", sid, pvalsym->ptype->name), &scf->prevpos);
    return;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(":"))
  {
    StatementError("Type specifier \":\" is expected after \"var\". Syntax: \"var identifier : type [ = initial value];\"");
    return;
  }

  ptype = ParseTypeSpec();
  if (not ptype)  return;

  ODecl * vdecl = AddDeclVar(scpos_statement_start, sid, ptype);

  OExpr * initexpr = nullptr;
  scf->SkipWhite();
  if (scf->CheckSymbol("="))  // variable initializer specified
  {
    scf->SkipWhite();
    OExpr * initexpr = ParseExpression();
    if (initexpr)
    {
      if (not vdecl->initvalue->CalculateConstant(initexpr))
      {
        StatementError("Error in the initial value expression", &scf->prevpos);
      }
    }
    delete initexpr;
  }

  // global variables are always initialized with 0 / null
  vdecl->pvalsym->initialized = true;

  if (not CheckStatementClose())
  {
    // error message already generated.
    return;
  }
}

void ODqCompParser::ParseConstDecl()
{
  // syntax form: "const identifier : type [ = initial value];"
  // note: "const" is already consumed

  string       sid;
  string       stype;
  OValSym *    pvalsym;
  OType *      ptype;
  OScPosition  expos;

  scf->SkipWhite();
  if (not scf->ReadIdentifier(sid))
  {
    StatementError("Identifier is expected after \"var\". Syntax: \"const identifier : type = value;\"");
    return;
  }

  if (g_module->ValSymDeclared(sid, &pvalsym))
  {
    StatementError(format("Constant/Variable \"{}\" is already declared with the type \"{}\"", sid, pvalsym->ptype->name), &scf->prevpos);
    return;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(":"))
  {
    StatementError("Type specifier \":\" is expected after \"const\". Syntax: \"const identifier : type = value;\"");
    return;
  }

  scf->SkipWhite();
  if (not scf->ReadIdentifier(stype))
  {
    StatementError("Type identifier is expected after \"const\". Syntax: \"const identifier : type = value;\"");
    return;
  }

  // check the type here for proper source code position (scf->prevpos)
  ptype = g_module->scope_priv->FindType(stype);
  if (not ptype)
  {
    StatementError(format("Unknown type \"{}\"", stype), &scf->prevpos);
    return;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol("="))  // variable initializer specified
  {
    StatementError("Assignment \"=\" is expected after \"const\". Syntax: \"const identifier : type = value;\"");
    return;
  }

  scf->SkipWhite();
  scf->SaveCurPos(expos);
  OExpr * valueexpr = ParseExpression();
  if (not valueexpr)
  {
    delete valueexpr;
    StatementError("Wrong value expression", &expos);
    return;
  }

  OValue * pvalue = ptype->CreateValue();
  if (not pvalue->CalculateConstant(valueexpr))
  {
    StatementError("Error evaluating value expression", &expos);

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

void ODqCompParser::ParseFunction()
{
  // note: "function" is already consumed
  // syntax form: "function identifier[(arglist)] [-> return_type] <statement_block | ;>"
  // statement block must follow, when ';' then it is a forward declaration

  string   sid;
  string   stype;

  scf->SkipWhite();
  if (not scf->ReadIdentifier(sid))
  {
    Error("Identifier is expected after \"function\". Syntax: \"function identifier(arglist) -> return_type\"");
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
          Error("\",\" expected for parameter lists", &scf->prevpos);
        }
        else { scf-> SkipWhite(); }
      }

      if (not scf->ReadIdentifier(spname))
      {
        Error("Parameter name expected", &scf->prevpos);
        if (not scf->ReadTo(",)"))  // try to skip to next parameter
        {
          break;  // serious problem, would lead to endless-loop
        }
        continue;
      }

      if (not tfunc->ParNameValid(spname))
      {
        Error("Invalid function parameter name \""+spname+"\"", &scf->prevpos);
        scf->ReadTo(",)");  // try to skip to next parameter
        continue;
      }

      scf->SkipWhite();
      if (not scf->CheckSymbol(":"))
      {
        Error("Parameter type specification expected: \": type\"", &scf->prevpos);
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

  scf->SkipWhite();
  if (scf->CheckSymbol("->"))  // return type
  {
    scf->SkipWhite();
    string frtname;
    if (not scf->ReadIdentifier(frtname))
    {
      Error("Function return type identifier expected after \"->\"");
    }
    else
    {
      tfunc->rettype = cur_mod_scope->FindType(frtname);
      if (not tfunc->rettype)
      {
        Error(format("Unknown function return type \"{}\"", frtname));
      }
    }
  }

  AddDeclFunc(scpos_statement_start, vsfunc);

  // go on with the function body

  ReadStatementBlock(vsfunc->body, "endfunc");

  vsfunc->scpos_endfunc = scf->prevpos;

  // check if the result is set
  if (vsfunc->vsresult and not vsfunc->vsresult->initialized)
  {
    Error(format("Function \"{}\" result is not set", vsfunc->name), &vsfunc->scpos_endfunc);
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
  OValSym * pvalsym;

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
    StatementError("\":\" is missing for statement block start");
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
      StatementError(format("Statement block closer \"{}\" is missing", block_closer));
      break;
    }

    scf->SaveCurPos(scpos_statement_start);

    if (scf->CheckSymbol(";"))  // empty ";", just ignore it
    {
      Hint("Meaningless \";\" was found.");
      continue;
    }

    // there should be a normal statement
    if (!scf->ReadIdentifier(sid))
    {
      StatementError("keyword or identifier is missing");
      continue;
    }

    if (ReservedWord(sid))
    {
      if ("var" == sid)  // local variable declaration
      {
        //StatementError("var statement parsing is not implemented");
        ParseStmtVar();
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
      else
      {
        StatementError(format("Statement \"{}\" not implemented yet", sid));
        continue;
      }
    }
    else // starts with a non-reserved word (in sid)
    {
      pvalsym = curscope->FindValSym(sid, nullptr, true);
      if (not pvalsym)
      {
        StatementError(format("Unknown identifier \"{}\"", sid));
        continue;
      }

      if (VSK_VARIABLE == pvalsym->kind or VSK_PARAMETER == pvalsym->kind)
      {
        scf->SkipWhite();
        if (TK_POINTER == pvalsym->ptype->kind and scf->CheckSymbol("^"))
        {
          ParseStmtDerefAssign(pvalsym);
          continue;
        }
        if ((TK_ARRAY == pvalsym->ptype->kind or TK_ARRAY_SLICE == pvalsym->ptype->kind)
            and scf->CheckSymbol("["))
        {
          ParseStmtArrayAssign(pvalsym);
          continue;
        }
        if (ParseStmtAssign(pvalsym))
        {
          continue;
        }
      }

      // function call
      OValSymFunc * vsfunc = dynamic_cast<OValSymFunc *>(pvalsym);
      if (vsfunc)
      {
        ParseStmtVoidCall(vsfunc);
        continue;
      }

      // unknown

      StatementError(format("Unknown statement/function \"{}\"", sid));
      continue;
    }
  }

  curscope = prev_scope;
  curblock = prev_block;
}

OType * ODqCompParser::ParseTypeSpec()
{
  // Parses type specification after ":"
  // Handles: ^type (pointer), type[N] (fixed array), type[] (slice)

  bool is_pointer = false;
  scf->SkipWhite();
  if (scf->CheckSymbol("^"))
  {
    is_pointer = true;
  }

  string stype;
  scf->SkipWhite();
  if (not scf->ReadIdentifier(stype))
  {
    Error("Type identifier expected");
    return nullptr;
  }

  OType * ptype = cur_mod_scope->FindType(stype);
  if (not ptype)
  {
    Error(format("Unknown type \"{}\"", stype), &scf->prevpos);
    return nullptr;
  }

  if (is_pointer)
  {
    ptype = ptype->GetPointerType();
  }

  // Check for array suffix: [N] or []
  scf->SkipWhite();
  if (scf->CheckSymbol("["))
  {
    if (is_pointer)
    {
      Error("Pointer-to-array is not supported");
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
      Error("Dynamic arrays (int[...]) are not yet implemented");
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
        Error("Array size (integer) or \"]\" expected");
        return nullptr;
      }
      if (arrlen <= 0)
      {
        Error("Array size must be a positive integer");
        return nullptr;
      }
      scf->SkipWhite();
      if (not scf->CheckSymbol("]"))
      {
        Error("\"]\" expected after array size");
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
    Error(format("The function \"{}\" has no return value, \";\" expected after the return", curvsfunc->name));
    return;
  }

  OExpr * expr = ParseExpression();
  scf->SkipWhite();
  if (!scf->CheckSymbol(";"))
  {
    Error("\";\" is missing after the return expression");
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
    StatementError("While condition is missing");
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
    StatementError("if condition is missing");
    return;
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
        StatementError("elif condition is missing");
        break;
      }
      branch = st->AddBranch(cond);
      continue;
    }

    if ("else" == endstr)
    {
      if (st->else_present)
      {
        StatementError("if: else branch was already presented.");
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
  return ParseExprOr();
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
  return new OCompareExpr(op, left, right);
}

OExpr * ODqCompParser::ParseExprAdd()
{
  scf->SkipWhite();

  OExpr *  left = ParseExprMul();
  if (!left)
  {
    return nullptr;
  }

  while (not scf->Eof())
  {
    scf->SkipWhite();
    EBinOp op = BINOP_NONE;
    if      (scf->CheckSymbol("+"))  op = BINOP_ADD;
    else if (scf->CheckSymbol("-"))  op = BINOP_SUB;
    else
    {
      break;
    }

    OExpr *  right = ParseExprMul();
    OExpr *  res = CreateBinExpr(op, left, right);
    if (!res) return FreeLeftRight(left, right);

    left = res;
  }

  return left;
}

OExpr * ODqCompParser::ParseExprMul()
{
  scf->SkipWhite();

  OExpr * left  = ParseExprDiv();
  if (!left)
  {
    return nullptr;
  }

  while (not scf->Eof())
  {
    scf->SkipWhite();
    EBinOp op = BINOP_NONE;
    if (scf->CheckSymbol("*"))   op = BINOP_MUL;
    else
    {
      break;
    }

    OExpr *  right = ParseExprDiv();
    OExpr *  res   = CreateBinExpr(op, left, right);
    if (!res)
    {
      return FreeLeftRight(left, right);
    }

    left = res;
  }

  return left;
}

OExpr * ODqCompParser::ParseExprDiv()
{
  scf->SkipWhite();

  OExpr *  left = ParseExprBinOr();
  if (!left)
  {
    return nullptr;
  }

  while (not scf->Eof())
  {
    scf->SkipWhite();
    EBinOp op = BINOP_NONE;
    if      (scf->CheckSymbol("/"))     op = BINOP_DIV;
    else if (scf->CheckSymbol("IDIV"))  op = BINOP_IDIV;
    else if (scf->CheckSymbol("IMOD"))  op = BINOP_IMOD;
    else
    {
      break;
    }

    OExpr *  right = ParseExprBinOr();
    OExpr *  res   = CreateBinExpr(op, left, right);
    if (!res)
    {
      return FreeLeftRight(left, right);
    }

    left = res;
  }

  return left;
}

OExpr * ODqCompParser::ParseExprBinOr()
{
  scf->SkipWhite();

  OExpr *  left = ParseExprBinAnd();
  if (!left)
  {
    return nullptr;
  }

  while (not scf->Eof())
  {
    scf->SkipWhite();

    EBinOp op = BINOP_NONE;
    if      (scf->CheckSymbol("OR"))   op = BINOP_IOR;
    else if (scf->CheckSymbol("XOR"))  op = BINOP_IXOR;
    else
    {
      break;
    }

    OExpr *  right = ParseExprBinAnd();
    OExpr *  res   = CreateBinExpr(op, left, right);
    if (!res)
    {
      return FreeLeftRight(left, right);
    }

    left = res;
  }

  return left;
}

OExpr * ODqCompParser::ParseExprBinAnd()
{
  scf->SkipWhite();

  OExpr *  left = ParseExprShift();
  if (!left)
  {
    return nullptr;
  }

  while (not scf->Eof())
  {
    scf->SkipWhite();

    EBinOp op = BINOP_NONE;
    if (scf->CheckSymbol("AND"))  op = BINOP_IAND;
    else
    {
      break;
    }

    OExpr *  right = ParseExprShift();
    OExpr *  res = CreateBinExpr(op, left, right);
    if (!res)
    {
      return FreeLeftRight(left, right);
    }

    left = res;
  }

  return left;
}


OExpr * ODqCompParser::ParseExprShift()
{
  scf->SkipWhite();

  OExpr * left  = ParseExprNeg();
  if (!left)
  {
    return nullptr;
  }

  while (not scf->Eof())
  {
    scf->SkipWhite();

    EBinOp op = BINOP_NONE;
    if      (scf->CheckSymbol("<<") or scf->CheckSymbol("SHL"))  op = BINOP_ISHL;
    else if (scf->CheckSymbol(">>") or scf->CheckSymbol("SHR"))  op = BINOP_ISHR;
    else
    {
      break;
    }

    OExpr *  right = ParseExprNeg();
    OExpr *  res = CreateBinExpr(op, left, right);
    if (!res)
    {
      return FreeLeftRight(left, right);
    }

    left = res;
  }

  return left;
}

OExpr * ODqCompParser::ParseExprNeg()
{
  if (scf->CheckSymbol("-"))
  {
    OExpr * val = ParseExprPrimary();
    if (!val) return nullptr;
    return new ONegExpr(val);
  }

  if (scf->CheckSymbol("NOT"))
  {
    OExpr * val = ParseExprPrimary();
    if (!val) return nullptr;
    return new OBinNotExpr(val);
  }

  return ParseExprPrimary();
}

OExpr * ODqCompParser::FreeLeftRight(OExpr * left, OExpr * right)
{
  if (left) delete left;
  if (right) delete right;
  return nullptr;
}

OExpr * ODqCompParser::CreateBinExpr(EBinOp op, OExpr * left, OExpr * right)
{
  OExpr * result = nullptr;
  OExpr * newleft  = left;
  OExpr * newright = right;

  if (not left  or  not right)
  {
    return nullptr;
  }

  // check type compatibilities
  ETypeKind tkl = left->ptype->kind;
  ETypeKind tkr = right->ptype->kind;

  if ((op >= BINOP_IAND) and (op <= BINOP_ISHR))
  {
    if ((tkl != TK_INT) or (tkr != TK_INT))
    {
      Error(format("Types mismatch for BinOp({}): \"{}\", \"{}\"", int(op), left->ptype->name, right->ptype->name));
      return nullptr;
    }
  }
  else if (tkl != tkr)
  {
    if ((TK_INT == tkl) and (TK_FLOAT == tkr))
    {
      newleft  = new OExprTypeConv(g_builtins->type_float, left);
    }
    else if ((TK_INT == tkr) and (TK_FLOAT == tkl))
    {
      newright = new OExprTypeConv(g_builtins->type_float, right);
    }
    else
    {
      Error(format("Types mismatch for BinOp({}): \"{}\", \"{}\"", int(op), left->ptype->name, right->ptype->name));
      return nullptr;
    }
  }
  else if ((TK_INT == tkl) and (BINOP_DIV == op))  // division results to floating point
  {
    newleft  = new OExprTypeConv(g_builtins->type_float, left);
    newright = new OExprTypeConv(g_builtins->type_float, right);
  }

  return new OBinExpr(op, newleft, newright);
}

OExpr * ODqCompParser::ParseExprPrimary()
{
  OExpr * result = nullptr;

  scf->SkipWhite();

  if (scf->CheckSymbol("("))
  {
    result = ParseExpression();
    scf->SkipWhite();
    if (!scf->CheckSymbol(")"))
    {
      Error("\")\" expected");
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
      Error("hexadecimal numbers expected after \"0x\"");
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
          Error("Floating point literal parsing error.");
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
      Error("Integer literal parsing error.");
    }
    return result;
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

  // address-of operator: &variable
  if (scf->CheckSymbol("&"))
  {
    scf->SkipWhite();
    string addrname;
    if (!scf->ReadIdentifier(addrname))
    {
      Error("Variable name expected after \"&\"");
      return nullptr;
    }
    OValSym * addrvs = curscope->FindValSym(addrname);
    if (!addrvs)
    {
      Error(format("Unknown variable \"{}\"", addrname));
      return nullptr;
    }
    if (VSK_VARIABLE != addrvs->kind and VSK_PARAMETER != addrvs->kind)
    {
      Error(format("\"{}\" is not a variable, cannot take its address", addrname));
      return nullptr;
    }
    // address of array element: &arr[index]
    if (TK_ARRAY == addrvs->ptype->kind)
    {
      scf->SkipWhite();
      if (scf->CheckSymbol("["))
      {
        if (not addrvs->initialized)
        {
          Error(format("Accessing uninitialized array \"{}\"", addrvs->name));
        }
        OExpr * indexexpr = ParseExpression();
        scf->SkipWhite();
        if (not scf->CheckSymbol("]"))
        {
          Error("\"]\" expected after array index in address-of");
          delete indexexpr;
          return nullptr;
        }
        return new OAddrOfArrayElemExpr(addrvs, indexexpr);
      }
    }
    result = new OAddrOfExpr(addrvs);
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
      Error(format("Unexpected expression char \"{}\"", *scf->curp));
    }
    else
    {
      Error("Expression expected.");
    }
    return result;
  }

  // builtin specials

  if ("len" == sid)
  {
    return ParseBuiltinLen();
  }

  OValSym * vs = curscope->FindValSym(sid);
  if (!vs)
  {
    Error(format("Unknown identifier \"{}\"", sid));
    return result;
  }

  // types
  //  - variable reference
  //  - constant
  //  - compbound variable
  //  - function

  //OType * ptype = vs->ptype;
  ETypeKind tk = vs->ptype->kind;

  scf->SkipWhite();

  OValSymFunc * vsfunc = dynamic_cast<OValSymFunc *>(vs);
  if (vsfunc)
  {
    if (scf->CheckSymbol("("))
    {
      result = ParseExprFuncCall(vsfunc);
      return result;
    }
    else
    {
      Error(format("\"(\" is required for function call"));
    }
  }

  if (TK_COMPOUND == tk)
  {
    Error(format("Object/Struct reference \"{}\" not implemented", sid));
    return result;
  }

  // array element access: arr[index]
  if ((TK_ARRAY == tk or TK_ARRAY_SLICE == tk) and scf->CheckSymbol("["))
  {
    if (not vs->initialized)
    {
      Error(format("Accessing uninitialized array \"{}\"", vs->name), &scpos_sid);
    }
    OExpr * indexexpr = ParseExpression();
    scf->SkipWhite();
    if (not scf->CheckSymbol("]"))
    {
      Error("\"]\" expected after array index");
    }
    return new OArrayIndexExpr(vs, indexexpr);
  }

  result = new OVarRef(vs);
  if (not vs->initialized)
  {
    Error(format("Accessing uninitialized variable \"{}\"", vs->name), &scpos_sid);
  }

  // postfix dereference: p^
  scf->SkipWhite();
  if (TK_POINTER == vs->ptype->kind and scf->CheckSymbol("^"))
  {
    result = new ODerefExpr(result);
  }

  return result;
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
        Error("\",\" expected in array literal");
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
      Error("\",\" or \")\" is missing at function call arguments");
      bok = false;
      break;
    }

    if (pcnt >= tfunc->params.size())
    {
      Error(format("Too many arguments provided, expected {}", tfunc->params.size()));
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

    OType * argtype = tfunc->params[pcnt]->ptype;
    if (not CheckAssignType(argtype, &argexpr, "Argument"))
    {
      bok = false;
      break;
    }
    // CheckAssignType may have replaced argexpr (e.g. array->slice conversion)
    result->args[pcnt] = argexpr;

    ++pcnt;
  }

  if (result->args.size() != tfunc->params.size())
  {
    Error(format("Too few arguments provided: {}, expected: {}", result->args.size(), tfunc->params.size()));
    bok = false;
  }

  if (!bok)
  {
    delete result;
    return nullptr;
  }

  return result;
}

OExpr * ODqCompParser::ParseBuiltinLen()
{
  scf->SkipWhite();
  if (not scf->CheckSymbol("("))
  {
    Error("\"(\" expected after \"len\"");
    return nullptr;
  }
  scf->SkipWhite();
  string lenarg;
  if (not scf->ReadIdentifier(lenarg))
  {
    Error("Variable name expected in len()");
    return nullptr;
  }
  OValSym * lenvs = curscope->FindValSym(lenarg);
  if (!lenvs)
  {
    Error(format("Unknown variable \"{}\"", lenarg));
    return nullptr;
  }
  scf->SkipWhite();
  if (not scf->CheckSymbol(")"))
  {
    Error("\")\" expected after len() argument");
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
  else
  {
    Error(format("len() requires an array or slice, got \"{}\"", lenvs->ptype->name));
    return nullptr;
  }
}

void ODqCompParser::ParseStmtVar()
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
    StatementError("Identifier is expected after \"var\". Syntax: \"var identifier : type [ = expression];\"");
    return;
  }

  pvalsym = curscope->FindValSym(sid, nullptr, false);  // do not search in the parent scopes this time !
  if (pvalsym)
  {
    StatementError(format("Variable \"{}\" is already declared with the type \"{}\"", sid, pvalsym->ptype->name), &scf->prevpos);
    return;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(":"))
  {
    StatementError("Type specifier \":\" is expected after \"var\". Syntax: \"var identifier : type [ = expression];\"");
    return;
  }

  ptype = ParseTypeSpec();
  if (not ptype)  return;

  OExpr * initexpr = nullptr;
  scf->SkipWhite();
  if (scf->CheckSymbol("="))  // variable initializer specified
  {
    scf->SkipWhite();
    initexpr = ParseExpression();
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol(";"))
  {
    Error("\";\" is missing after the var declaration");
  }

  if (initexpr and (not CheckAssignType(ptype, &initexpr, "Assignment")))  // might add implicit conversion
  {
    // error message is already provided.
    delete initexpr;
    return;
  }

  pvalsym = ptype->CreateValSym(scpos_statement_start, sid);
  curscope->DefineValSym(pvalsym);
  curblock->AddStatement(new OStmtVarDecl(scpos_statement_start, pvalsym, initexpr));
}

bool ODqCompParser::ParseStmtAssign(OValSym * pvalsym)
{
  // syntax form: "identifier = expression;"
  // note: identifier(=sid) is already consumed, assign operation expected

  string     stype;
  OType *    ptype = pvalsym->ptype;
  EBinOp     op = BINOP_NONE;

  scf->SkipWhite();
  if (scf->CheckSymbol("="))
  {
    op = BINOP_NONE;
  }
  else if (scf->CheckSymbol("+="))   // i += 1;  i =+= 1;
  {
    op = BINOP_ADD;
  }
  else if (scf->CheckSymbol("-="))
  {
    op = BINOP_SUB;
  }
  else if (scf->CheckSymbol("*="))
  {
    op = BINOP_MUL;
  }
  else if (scf->CheckSymbol("/="))
  {
    op = BINOP_DIV;
  }
  else if (scf->CheckSymbol("=IDIV="))
  {
    op = BINOP_IDIV;
  }
  else if (scf->CheckSymbol("=IMOD="))
  {
    op = BINOP_IMOD;
  }
  else if (scf->CheckSymbol("<<="))
  {
    op = BINOP_ISHL;
  }
  else if (scf->CheckSymbol(">>="))
  {
    op = BINOP_ISHR;
  }
  else if (scf->CheckSymbol("=AND="))   // i =AND= 1;
  {
    op = BINOP_IAND;
  }
  else if (scf->CheckSymbol("=OR="))    // i =OR= 1;
  {
    op = BINOP_IOR;
  }
  else if (scf->CheckSymbol("=XOR="))   // i =XOR= 1;
  {
    op = BINOP_IXOR;
  }

  else
  {
    return false;
  }

  OExpr * expr = ParseExpression();

  scf->SkipWhite();
  if (!scf->CheckSymbol(";"))
  {
    OScPosition scpos;
    scf->SaveCurPos(scpos);
    StatementError("\";\" is missing after the assignment statement", &scpos);
  }

  if (!expr)
  {
    return true;  // signalizes processed, not error-free here !
  }

  // Pointer arithmetic: p += int  or  p -= int
  if (TK_POINTER == ptype->kind and (BINOP_ADD == op or BINOP_SUB == op))
  {
    if (TK_INT != expr->ptype->kind)
    {
      Error(format("Pointer arithmetic requires an integer offset, got \"{}\"", expr->ptype->name));
      delete expr;
      return true;
    }
    if (not pvalsym->initialized)
      Error(format("Variable \"{}\" is not initialized.", pvalsym->name));
    else
      curblock->AddStatement(new OStmtModifyAssign(scpos_statement_start, pvalsym, op, expr));
    return true;
  }

  if (not CheckAssignType(ptype, &expr, "Modify assignment"))  // might add implicit conversion
  {
    // error message is already provided.
    delete expr;
    return true;  // signalizes processed, not error-free here !
  }

  if (BINOP_NONE == op)
  {
    curblock->AddStatement(new OStmtAssign(scpos_statement_start, pvalsym, expr));
    curblock->scope->SetVarInitialized(pvalsym);
  }
  else
  {
    if (not pvalsym->initialized)
    {
      Error(format("Variable \"{}\" is not initialized.", pvalsym->name));
    }
    else
    {
      curblock->AddStatement(new OStmtModifyAssign(scpos_statement_start, pvalsym, op, expr));
    }
  }

  return true; // signalizes processed, not error-free here !
}

void ODqCompParser::ParseStmtDerefAssign(OValSym * ptrvalsym)
{
  // syntax form: "ptrvar^ = expression;"
  // note: identifier and "^" are already consumed

  scf->SkipWhite();
  if (not scf->CheckSymbol("="))
  {
    StatementError("\"=\" is expected after pointer dereference \"^\"");
    return;
  }

  OExpr * expr = ParseExpression();
  if (!expr)
  {
    return;
  }

  OTypePointer * ptrtype = static_cast<OTypePointer *>(ptrvalsym->ptype);
  if (not CheckAssignType(ptrtype->basetype, &expr, "Pointer dereference assignment"))
  {
    delete expr;
    return;
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol(";"))
  {
    Error("\";\" is missing after the assignment");
  }

  curblock->AddStatement(new OStmtDerefAssign(scpos_statement_start, ptrvalsym, expr));
}

void ODqCompParser::ParseStmtArrayAssign(OValSym * arrayvalsym)
{
  // syntax form: "arr[index] = expression;"
  // note: identifier and "[" are already consumed

  OExpr * indexexpr = ParseExpression();
  if (!indexexpr)
  {
    return;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol("]"))
  {
    Error("\"]\" expected after array index");
    delete indexexpr;
    return;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol("="))
  {
    StatementError("\"=\" is expected after array index");
    delete indexexpr;
    return;
  }

  OExpr * expr = ParseExpression();
  if (!expr)
  {
    delete indexexpr;
    return;
  }

  // Determine the element type for type checking
  OType * elemtype = nullptr;
  if (TK_ARRAY == arrayvalsym->ptype->kind)
  {
    elemtype = static_cast<OTypeArray *>(arrayvalsym->ptype)->elemtype;
  }
  else // TK_ARRAY_SLICE
  {
    elemtype = static_cast<OTypeArraySlice *>(arrayvalsym->ptype)->elemtype;
  }

  if (not CheckAssignType(elemtype, &expr, "Array element assignment"))
  {
    delete indexexpr;
    delete expr;
    return;
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol(";"))
  {
    Error("\";\" is missing after the assignment");
  }

  // First element assignment marks the whole array as initialized
  curblock->scope->SetVarInitialized(arrayvalsym);

  curblock->AddStatement(new OStmtArrayAssign(scpos_statement_start, arrayvalsym, indexexpr, expr));
}

void ODqCompParser::ParseStmtVoidCall(OValSymFunc * vsfunc)
{
  scf->SkipWhite();
  if (not scf->CheckSymbol("("))
  {
    StatementError("\"(\" is missing for the function call");
    return;
  }

  OExpr * callexpr = ParseExprFuncCall(vsfunc);  // re-use the existing version in the expressions
  if (not callexpr)
  {
    return;
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol(";"))
  {
    Error("\";\" is missing after the var declaration");
  }

  curblock->AddStatement(new OStmtVoidCall(scpos_statement_start, callexpr));
}

bool ODqCompParser::CheckAssignType(OType * dsttype, OExpr ** rexpr, const string astmt)
{
  ETypeKind tkd = dsttype->kind;
  ETypeKind tke = (*rexpr)->ptype->kind;

  if (tkd != tke)
  {
    if ((TK_FLOAT == tkd) and (TK_INT == tke))
    {
      *rexpr = new OExprTypeConv(dsttype, *rexpr);
    }
    else if ((TK_ARRAY_SLICE == tkd) and (TK_ARRAY == tke))
    {
      // Implicit conversion: fixed array -> slice
      OTypeArraySlice * slicedst = static_cast<OTypeArraySlice *>(dsttype);
      OTypeArray * arrsrc = static_cast<OTypeArray *>((*rexpr)->ptype);
      if (slicedst->elemtype->kind != arrsrc->elemtype->kind)
      {
        Error(format("{} array element type mismatch: \"{}\" = \"{}\"", astmt, dsttype->name, (*rexpr)->ptype->name));
        return false;
      }
      // The source expression must be a variable reference so we can get its alloca
      OVarRef * varref = dynamic_cast<OVarRef *>(*rexpr);
      if (!varref)
      {
        Error(format("{}: cannot convert non-variable array to slice", astmt));
        return false;
      }
      *rexpr = new OArrayToSliceExpr(varref->pvalsym, dsttype);
      delete varref;
    }
    else
    {
      Error(format("{} type mismatch: \"{}\" = \"{}\"", astmt, dsttype->name, (*rexpr)->ptype->name));
      return false;
    }
  }
  else if (TK_POINTER == tkd)
  {
    // both are pointers: allow null (basetype == nullptr) or matching base types
    OTypePointer * ptrdst = static_cast<OTypePointer *>(dsttype);
    OTypePointer * ptrsrc = static_cast<OTypePointer *>((*rexpr)->ptype);
    if (ptrsrc->basetype and ptrdst->basetype and (ptrsrc->basetype->kind != ptrdst->basetype->kind))
    {
      Error(format("{} pointer type mismatch: \"{}\" = \"{}\"", astmt, dsttype->name, (*rexpr)->ptype->name));
      return false;
    }
  }
  else if (TK_ARRAY_SLICE == tkd)
  {
    // both are slices: check element types match
    OTypeArraySlice * slicedst = static_cast<OTypeArraySlice *>(dsttype);
    OTypeArraySlice * slicesrc = static_cast<OTypeArraySlice *>((*rexpr)->ptype);
    if (slicedst->elemtype->kind != slicesrc->elemtype->kind)
    {
      Error(format("{} slice element type mismatch: \"{}\" = \"{}\"", astmt, dsttype->name, (*rexpr)->ptype->name));
      return false;
    }
  }
  else if (TK_ARRAY == tkd)
  {
    OTypeArray * arrdst = static_cast<OTypeArray *>(dsttype);
    OTypeArray * arrsrc = static_cast<OTypeArray *>((*rexpr)->ptype);
    if (arrdst->elemtype != arrsrc->elemtype)
    {
      Error(format("{} array element type mismatch: \"{}\" = \"{}\"", astmt, dsttype->name, (*rexpr)->ptype->name));
      return false;
    }
    if (arrdst->arraylength != arrsrc->arraylength)
    {
      Error(format("{} array size mismatch: \"{}\" = \"{}\"", astmt, dsttype->name, (*rexpr)->ptype->name));
      return false;
    }
  }

  return true;
}

void ODqCompParser::StatementError(const string amsg, OScPosition * scpos, bool atryrecover)
{
  OScPosition log_scpos(scf->curfile, scf->curp);

  if (scpos and scpos->scfile) // use the position provided
  {
    log_scpos.Assign(*scpos);
  }

  Error(amsg, &log_scpos);
  //print("{}: {}\n", log_scpos.Format(), amsg);

  // try to recover
  if (atryrecover)
  {
    if (!scf->SearchPattern(";", true))  // TODO: improve to handle #{} and strings
    {

    }
  }

  scf->SkipWhite();
}

void ODqCompParser::ExpressionError(const string amsg, OScPosition *scpos)
{
  OScPosition log_scpos(scpos_statement_start);

  if (scpos and scpos->scfile) // use the position provided
  {
    log_scpos.Assign(*scpos);
  }

  Error(amsg, &log_scpos);
}

bool ODqCompParser::CheckStatementClose()
{
  scf->SkipWhite();
  if (not scf->CheckSymbol(";"))
  {
    StatementError("\";\" is expected to close the previous statement");
    return false;
  }
  return true;
}

void ODqCompParser::Error(const string amsg, OScPosition * ascpos)
{
  OScPosition * epos = ascpos;
  if (!epos) epos = errorpos;
  if (!epos) epos = &scpos_statement_start;

  print("{} ERROR: {}\n", epos->Format(), amsg);

  ++errorcnt;
}

void ODqCompParser::Warning(const string amsg, OScPosition * ascpos)
{
  OScPosition * epos = ascpos;
  if (!epos) epos = errorpos;
  if (!epos) epos = &scpos_statement_start;

  print("{} WARNING: {}\n", epos->Format(), amsg);
}

void ODqCompParser::Hint(const string amsg, OScPosition * ascpos)
{
  OScPosition * epos = ascpos;
  if (!epos) epos = errorpos;
  if (!epos) epos = &scpos_statement_start;

  print("{} HINT: {}\n", epos->Format(), amsg);
}
