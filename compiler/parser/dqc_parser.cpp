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

void ODqCompParser::ParseVarDecl()
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

  scf->SkipWhite();
  if (not scf->ReadIdentifier(stype))
  {
    StatementError("Type identifier is expected after \"var\". Syntax: \"var identifier : type [ = initial value];\"");
    return;
  }

  // check the type here for proper source code position (scf->prevpos)
  ptype = g_module->scope_priv->FindType(stype);
  if (not ptype)
  {
    StatementError(format("Unknown type \"{}\"", stype), &scf->prevpos);
    return;
  }

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

      scf->SkipWhite();
      if (not scf->ReadIdentifier(sptype))
      {
        Error("Function parameter type name expected", &scf->prevpos);
        scf->ReadTo(",)");  // try to skip to next parameter
        continue;
      }

      OType * ptype = cur_mod_scope->FindType(sptype);
      if (!ptype)
      {
        Error(format("Unknown function parameter type \"{}\"", sptype), &scf->prevpos);
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

      if (VSK_VARIABLE == pvalsym->kind)
      {
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

void ODqCompParser::ParseStmtReturn()
{
  // "return" is already consumed.
  scf->SkipWhite();
  if (scf->CheckSymbol(";"))  // return without value, use the result variable to return
  {
    curblock->AddStatement(new OStmtReturn(scpos_statement_start, nullptr, curvsfunc));
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

  bool else_already = false;

  while (not scf->Eof())
  {
    string endstr = "";
    ReadStatementBlock(branch->body, "endif|elif|else", &endstr);

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
      if (else_already)
      {
        StatementError("if: else branch was already presented.");
        break;
      }
      else_already = true;
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
}

OExpr * ODqCompParser::ParseExpression()
{
  return ParseExprOr();
}

OExpr * ODqCompParser::ParseExprOr()
{
  OExpr * left = ParseExprAnd();
  while (not scf->Eof())
  {
    scf->SkipWhite();
    if (scf->CheckSymbol("or"))
    {
      left = new OLogicalExpr(LOGIOP_OR, left, ParseExprAnd());
      continue;
    }

    break;
  }
  return left;
}

OExpr * ODqCompParser::ParseExprAnd()
{
  OExpr * left = ParseExprNot();
  while (not scf->Eof())
  {
    scf->SkipWhite();
    if (scf->CheckSymbol("and"))
    {
      left = new OLogicalExpr(LOGIOP_AND, left, ParseExprAnd());
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
    return new ONotExpr(ParseExprNot());
  }

  return ParseComparison();
}

OExpr * ODqCompParser::ParseComparison()
{
  OExpr * left = ParseExprAdd();
  OExpr * right = nullptr;

  scf->SkipWhite();

  if (scf->CheckSymbol("=="))
  {
    right = ParseExprAdd();
    return new OCompareExpr(COMPOP_EQ, left, right);
  }

  if (scf->CheckSymbol("!=") or scf->CheckSymbol("<>"))
  {
    right = ParseExprAdd();
    return new OCompareExpr(COMPOP_NE, left, right);
  }

  if (scf->CheckSymbol("<"))
  {
    right = ParseExprAdd();
    return new OCompareExpr(COMPOP_LT, left, right);
  }

  if (scf->CheckSymbol("<="))
  {
    right = ParseExprAdd();
    return new OCompareExpr(COMPOP_LE, left, right);
  }

  if (scf->CheckSymbol(">"))
  {
    right = ParseExprAdd();
    return new OCompareExpr(COMPOP_GT, left, right);
  }

  if (scf->CheckSymbol(">="))
  {
    right = ParseExprAdd();
    return new OCompareExpr(COMPOP_GE, left, right);
  }

  return left;
}

OExpr * ODqCompParser::ParseExprAdd()
{
  scf->SkipWhite();

  OExpr * left  = ParseExprMul();
  OExpr * right = nullptr;

  while (not scf->Eof())
  {
    scf->SkipWhite();
    if (scf->CheckSymbol("+"))
    {
      right = ParseExprMul();
      if (right)
      {
        return CreateBinExpr(BINOP_ADD, left, right);
      }
    }
    else if (scf->CheckSymbol("-"))
    {
      right = ParseExprMul();
      if (right)
      {
        return CreateBinExpr(BINOP_SUB, left, right);
      }
    }
    else
    {
      break;
    }
  }

  return left;
}

OExpr * ODqCompParser::ParseExprMul()
{
  scf->SkipWhite();

  OExpr * left  = ParseExprDiv();
  OExpr * right = nullptr;

  while (not scf->Eof())
  {
    scf->SkipWhite();
    if (scf->CheckSymbol("*"))
    {
      right = ParseExprDiv();
      if (right)
      {
        return CreateBinExpr(BINOP_MUL, left, right);
      }
    }
    else
    {
      break;
    }
  }

  return left;
}

OExpr * ODqCompParser::ParseExprDiv()
{
  scf->SkipWhite();

  OExpr * left  = ParseExprPrimary();
  OExpr * right = nullptr;

  while (not scf->Eof())
  {
    scf->SkipWhite();
    if (scf->CheckSymbol("/"))
    {
      right = ParseExprPrimary();
      if (right)
      {
        return CreateBinExpr(BINOP_DIV, left, right);
      }
    }
    else if (scf->CheckSymbol("IDIV"))
    {
      right = ParseExprPrimary();
      if (right)
      {
        return CreateBinExpr(BINOP_IDIV, left, right);
      }
    }
    else if (scf->CheckSymbol("IMOD"))
    {
      right = ParseExprPrimary();
      if (right)
      {
        return CreateBinExpr(BINOP_IMOD, left, right);
      }
    }
    else
    {
      break;
    }
  }

  return left;
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

  if (tkl != tkr)
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


OExpr * ODqCompParser::ParseExprNeg()
{
  if (scf->CheckSymbol("-"))
  {
    return new ONegExpr(ParseExprNeg());
  }

  return ParseExprPrimary();
}

OExpr * ODqCompParser::ParseExprPrimary()
{
  OExpr * result = nullptr;

  scf->SkipWhite();

  if (scf->CheckSymbol("-"))
  {
    result = ParseExprPrimary();
    return new ONegExpr(result);
  }

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

  // identifier

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

  result = new OVarRef(vs);
  return result;
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

  scf->SkipWhite();
  if (not scf->ReadIdentifier(stype))
  {
    StatementError("Type identifier is expected after \"var\". Syntax: \"var identifier : type [ = expression];\"");
    return;
  }

  // check the type here for proper source code position (scf->prevpos)
  ptype = g_module->scope_priv->FindType(stype);
  if (not ptype)
  {
    StatementError(format("Unknown type \"{}\"", stype), &scf->prevpos);
    return;
  }

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
  else if (scf->CheckSymbol("+="))
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
  else if (scf->CheckSymbol("IDIV="))
  {
    op = BINOP_IDIV;
  }
  else if (scf->CheckSymbol("IMOD="))
  {
    op = BINOP_IMOD;
  }
  else
  {
    return false;
  }

  OExpr * expr = ParseExpression();

  scf->SkipWhite();
  if (!scf->CheckSymbol(";"))
  {
    Error("\";\" is missing after the var declaration");
  }

  if (!expr)
  {
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
  }
  else
  {
    curblock->AddStatement(new OStmtModifyAssign(scpos_statement_start, pvalsym, op, expr));
  }

  return true;
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
    else
    {
      Error(format("{} type mismatch: \"{}\" = \"{}\"", astmt, dsttype->name, (*rexpr)->ptype->name));
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
