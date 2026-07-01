/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_parser_expr.cpp
 */

#include "dqc_parser_expr.h"
#include "scf_dq.h"
#include "symbols.h"
#include "dq_module.h"
#include "dqc.h"
#include "otype_array.h"
#include "otype_compound.h"
#include "otype_enum.h"
#include "named_scopes.h"

static bool EnsureDynArrayRtlUse()
{
  if (g_namespaces.end() != g_namespaces.find("__dq_dynarray"))
  {
    return true;
  }
  return g_compiler->AddImplicitUse("rtl/dynarrmgr", "__dq_dynarray", nullptr, true, MUM_NONE);
}

static bool EnsureCStringRtlUse()
{
  if (g_namespaces.end() != g_namespaces.find("__dq_cstring"))
  {
    return true;
  }
  return g_compiler->AddImplicitUse("rtl/cstrings", "__dq_cstring", nullptr, true, MUM_NONE);
}

static bool EnsureDynStringRtlUse()
{
  if (g_namespaces.end() != g_namespaces.find("__dq_dynstr"))
  {
    return true;
  }
  return g_compiler->AddImplicitUse("rtl/dynstrmgr", "__dq_dynstr", nullptr, true, MUM_NONE);
}

static bool EnsureTextFormatRtlUse()
{
  if (g_namespaces.end() != g_namespaces.find("__dq_textformat"))
  {
    return true;
  }
  return g_compiler->AddImplicitUse("rtl/textformat", "__dq_textformat", nullptr, true, MUM_NONE);
}

static bool IsCStringMethodSourceType(OType * type)
{
  return IsTextSourceType(type);
}

static bool IsStringMethodSourceType(OType * type)
{
  return IsTextSourceType(type);
}

static OLValueExpr * CloneContextLValue(OLValueExpr * src)
{
  if (auto * var = dynamic_cast<OLValueVar *>(src))
  {
    return new OLValueVar(var->pvalsym);
  }
  if (auto * member = dynamic_cast<OLValueMember *>(src))
  {
    OLValueExpr * base = CloneContextLValue(member->base);
    if (!base)
    {
      return nullptr;
    }
    return new OLValueMember(base, member->structtype, member->memberindex, member->ptype);
  }
  return nullptr;
}

bool ODqCompParserExpr::ParseParamModeKeyword(const string & sid, EParamMode & rmode)
{
  if ("ref" == sid)
  {
    rmode = FPM_REF;
    return true;
  }

  if ("refin" == sid)
  {
    rmode = FPM_REFIN;
    return true;
  }

  if ("refout" == sid)
  {
    rmode = FPM_REFOUT;
    return true;
  }

  if ("refnull" == sid)
  {
    rmode = FPM_REFNULL;
    return true;
  }

  return false;
}

bool ODqCompParserExpr::ParseAttrIntArg(const string & attrname, int64_t & rvalue, bool positive_only)
{
  scf->SkipWhite();
  if (!scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, attrname);
    return false;
  }

  scf->SkipWhite();
  if (!scf->ReadInt64Value(rvalue))
  {
    Error(DQERR_ATTR_ARG_INT, attrname);
    return false;
  }

  if (positive_only && (rvalue <= 0))
  {
    Error(DQERR_ATTR_ARG_POSITIVE_INT, attrname);
    return false;
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_AFTER, attrname);
    return false;
  }

  return true;
}

bool ODqCompParserExpr::ParseAttrStringArg(const string & attrname, string & rvalue)
{
  scf->SkipWhite();
  if (!scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, attrname);
    return false;
  }

  scf->SkipWhite();
  if (!scf->ReadQuotedString(rvalue))
  {
    Error(DQERR_ATTR_ARG_STRING, attrname);
    return false;
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_AFTER, attrname);
    return false;
  }

  return true;
}

bool ODqCompParserExpr::ParseSingleAttribute(const string & attrname)
{
  scf->SkipWhite();

  if ("external" == attrname)
  {
    attr->SetFlag(ATTF_EXTERNAL);
    attr->external_linkage_name = "";
    if (scf->CheckSymbol("(", false))
    {
      if (!ParseAttrStringArg(attrname, attr->external_linkage_name))
      {
        return false;
      }
    }
    return true;
  }

  if ("export" == attrname)
  {
    attr->SetFlag(ATTF_EXPORT);
    return ParseAttrStringArg(attrname, attr->export_linkage_name);
  }

  if ("cexport" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_CEXPORT);
    attr->export_linkage_name = "";
    return true;
  }

  if ("align" == attrname)
  {
    attr->SetFlag(ATTF_ALIGN);
    return ParseAttrIntArg(attrname, attr->align_value, true);
  }

  if ("packed" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_PACKED);
    return true;
  }

  if ("section" == attrname)
  {
    attr->SetFlag(ATTF_SECTION);
    return ParseAttrStringArg(attrname, attr->section_name);
  }

  if ("overload" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_OVERLOAD);
    return true;
  }

  if ("override" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_OVERRIDE);
    return true;
  }

  if ("virtual" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_VIRTUAL);
    return true;
  }

  if ("abstract" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_ABSTRACT);
    return true;
  }

  if ("final" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_FINAL);
    return true;
  }

  if ("forward" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_FORWARD);
    return true;
  }

  if ("nowarn" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_NOWARN);
    return true;
  }

  if ("volatile" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_VOLATILE);
    return true;
  }

  if ("inline" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_INLINE);
    return true;
  }

  if ("always_inline" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_ALWAYS_INLINE);
    return true;
  }

  if ("noinline" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_NOINLINE);
    return true;
  }

  Error(DQERR_ATTR_UNKNOWN, attrname);
  return false;
}

bool ODqCompParserExpr::ParseAttributeBlock()
{
  OScPosition attrpos;

  scf->SaveCurPos(attrpos);
  if (!scf->CheckSymbol("[["))
  {
    return true;
  }

  attr->scpos = attrpos;

  while (!scf->Eof())
  {
    string attrname;

    scf->SkipWhite();
    if (!scf->ReadIdentifier(attrname))
    {
      Error(DQERR_ATTR_NAME_EXPECTED);
      SkipToSymbol("]]");
      return false;
    }

    if (!ParseSingleAttribute(attrname))
    {
      SkipToSymbol("]]");
      return false;
    }

    scf->SkipWhite();
    if (scf->CheckSymbol("]]"))
    {
      return true;
    }

    if (!scf->CheckSymbol(","))
    {
      Error(DQERR_ATTR_SEPARATOR);
      SkipToSymbol("]]");
      return false;
    }
  }

  Error(DQERR_MISSING_ATTR_CLOSE_AFTER, "attribute list");
  return false;
}

bool ODqCompParserExpr::ParseAttributes(bool areset)
{
  if (areset && attr)
  {
    attr->Reset();
  }

  while (!scf->Eof())
  {
    scf->SkipWhite();
    if (!scf->CheckSymbol("[[", false))
    {
      break;
    }

    if (!ParseAttributeBlock())
    {
      return false;
    }
  }

  return true;
}

bool ODqCompParserExpr::CheckSpecialReservedRootName(const string & aname)
{
  if (SFK_NONE == SpecialFuncKindFromName(aname))
  {
    return true;
  }

  OScPosition namepos(scf->curfile, scf->prevp);
  RootStatementError(DQERR_SPECIAL_FUNC_RESERVED, aname, &namepos);
  return false;
}

OType * ODqCompParserExpr::ParseTypeSpec(bool aemit_errors)
{
  // Parses type specification after ":"
  // Handles prefix type constructors: ^type, [N]type, []type

  scf->SkipWhite();

  if (scf->CheckSymbol("[[", false))
  {
    if (!ParseAttributes(false))
    {
      return nullptr;
    }
    if (attr && attr->IsSet(ATTF_NOWARN))
    {
      suppress_warnings = true;
    }
  }

  if (scf->CheckSymbol("^"))
  {
    OType * basetype = ParseTypeSpec(aemit_errors);
    if (!basetype)
    {
      return nullptr;
    }
    return basetype->GetPointerType();
  }

  if (scf->CheckSymbol("["))
  {
    scf->SkipWhite();
    if (scf->CheckSymbol("]"))
    {
      OType * elemtype = ParseTypeSpec(aemit_errors);
      if (!elemtype)
      {
        return nullptr;
      }
      return elemtype->GetSliceType();
    }

    if (scf->CheckSymbol("*"))
    {
      if (!scf->CheckSymbol("]"))
      {
        if (aemit_errors)
        {
          Error(DQERR_MISSING_CLOSE_BRACKET_AFTER, "dynamic array type");
        }
        return nullptr;
      }
      OType * elemtype = ParseTypeSpec(aemit_errors);
      if (!elemtype)
      {
        return nullptr;
      }
      elemtype->EnsureLayout();
      if (0 == elemtype->bytesize)
      {
        if (aemit_errors)
        {
          Error(DQERR_NOT_SUPPORTED, "dynamic array with unsized element type");
        }
        return nullptr;
      }
      if (!EnsureDynArrayRtlUse())
      {
        return nullptr;
      }
      return elemtype->GetDynArrayType();
    }

    if (scf->CheckSymbol("?"))
    {
      if (!scf->CheckSymbol("]"))
      {
        if (aemit_errors)
        {
          Error(DQERR_MISSING_CLOSE_BRACKET_FOR, "inferred array size");
        }
        return nullptr;
      }
      OType * elemtype = ParseTypeSpec(aemit_errors);
      if (!elemtype)
      {
        return nullptr;
      }
      return elemtype->GetArrayType(0);
    }

    bool prev_suppress = suppress_errors;
    if (!aemit_errors) suppress_errors = true;
    OExpr * arrlen_expr = ParseExpression();
    suppress_errors = prev_suppress;
    int64_t arrlen = 0;
    if (!arrlen_expr)
    {
      if (aemit_errors)
      {
        Error(DQERR_ARRAY_SIZESPEC);
      }
      return nullptr;
    }
    if (!TryCalculateIntConstant(arrlen_expr, arrlen))
    {
      OExpr::DeleteTree(arrlen_expr);
      if (aemit_errors)
      {
        Error(DQERR_ARRAY_SIZESPEC);
      }
      return nullptr;
    }
    OExpr::DeleteTree(arrlen_expr);
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

    OType * elemtype = ParseTypeSpec(aemit_errors);
    if (!elemtype)
    {
      return nullptr;
    }
    return elemtype->GetArrayType(uint32_t(arrlen));
  }

  scf->SkipWhite();
  OType * ptype = nullptr;
  string stype;

  if (scf->CheckSymbol("function"))
  {
    OTypeFunc * sigtype = ParseFunctionType(aemit_errors, "function");
    if (!sigtype)
    {
      return nullptr;
    }

    scf->SkipWhite();
    if (scf->CheckSymbol("of"))
    {
      scf->SkipWhite();
      if (scf->CheckSymbol("object"))
      {
        ptype = new OTypeFuncRef(sigtype, "", true);
        return ptype;
      }

      if (aemit_errors)
      {
        Error(DQERR_NOT_IMPLEMENTED_YET, "\"function(...) of ...\"");
      }
      delete sigtype;
      return nullptr;
    }

    ptype = new OTypeFuncRef(sigtype);
  }
  else if (not scf->ReadIdentifier(stype))
  {
    if (aemit_errors)
    {
      Error(DQERR_TYPE_ID_EXP);
    }
    return nullptr;
  }
  else
  {
    ptype = cur_mod_scope->FindType(stype);
    if (not ptype)
    {
      if (aemit_errors)
      {
        Error(DQERR_TYPE_UNKNOWN, stype, &scf->prevpos);
      }
      return nullptr;
    }
    ptype = ptype->ResolveAlias();
  }

  // cstring(N) handling: N is the usable logical length; storage has N + 1 bytes.
  if (TK_CSTRING == ptype->kind)
  {
    if (!EnsureCStringRtlUse())
    {
      return nullptr;
    }
    scf->SkipWhite();
    if (scf->CheckSymbol("[[", false))
    {
      return ptype;
    }
    if (scf->CheckSymbol("["))
    {
      if (aemit_errors)
      {
        ErrorTxt(DQERR_CSTR_SIZE_EXPECTED, "cstring size expected; use cstring(n), not cstring[n]");
      }
      return nullptr;
    }
    if (scf->CheckSymbol("("))
    {
      bool prev_suppress = suppress_errors;
      if (!aemit_errors) suppress_errors = true;
      OExpr * maxlen_expr = ParseExpression();
      suppress_errors = prev_suppress;
      int64_t maxlen = 0;
      if (!maxlen_expr)
      {
        if (aemit_errors)
        {
          Error(DQERR_CSTR_SIZE_EXPECTED);
        }
        return nullptr;
      }
      if (!TryCalculateIntConstant(maxlen_expr, maxlen))
      {
        OExpr::DeleteTree(maxlen_expr);
        if (aemit_errors)
        {
          Error(DQERR_CSTR_SIZE_EXPECTED);
        }
        return nullptr;
      }
      OExpr::DeleteTree(maxlen_expr);
      if (maxlen <= 0)
      {
        if (aemit_errors)
        {
          Error(DQERR_CSTR_SIZE_EXPECTED);
        }
        return nullptr;
      }
      scf->SkipWhite();
      if (not scf->CheckSymbol(")"))
      {
        if (aemit_errors)
        {
          Error(DQERR_MISSING_CLOSE_PAREN_FOR, "cstring size");
        }
        return nullptr;
      }
      if (aemit_errors && (((maxlen + 1) % 4) != 0))
      {
        Warning(DQWARN_CSTR_STORAGE_SIZE, to_string(maxlen), to_string(maxlen + 1), &scf->prevpos);
      }
      return g_builtins->type_cstring->GetSizedType(uint32_t(maxlen));
    }
    return ptype;  // unsized cstring (for parameters)
  }

  if (TK_DYNSTR == ptype->kind || TK_STRVIEW == ptype->kind)
  {
    if (!EnsureDynStringRtlUse())
    {
      return nullptr;
    }
  }

  if (TK_ANYVALUE == ptype->kind)
  {
    if (!EnsureAnyValueRtlUse())
    {
      return nullptr;
    }
  }

  // Array suffixes are no longer part of the type grammar. Keep [[...]]
  // available for attributes after the type specifier.
  scf->SkipWhite();
  if (scf->CheckSymbol("[[", false))
  {
    return ptype;
  }
  if (scf->CheckSymbol("[", false))
  {
    if (aemit_errors)
    {
      Error(DQERR_NOT_SUPPORTED, "Postfix array type syntax; use [N]T or []T");
    }
    return nullptr;
  }

  return ptype;
}

bool ODqCompParserExpr::ParseFunctionSignature(OTypeFunc * tfunc, bool atypespec, const string & aowner_name, bool aemit_errors)
{
  if (!tfunc)
  {
    return false;
  }

  bool berror = false;

  auto fail_or_recover = [&, this]() -> bool
  {
    berror = true;

    if (!scf->ReadTo(",)"))
    {
      return false;
    }

    scf->CheckSymbol(",");
    return true;
  };

  scf->SkipWhite();
  bool has_param_list = scf->CheckSymbol("(");
  if (!has_param_list)
  {
    if (atypespec)
    {
      if (aemit_errors)
      {
        Error(DQERR_MISSING_OPEN_PAREN_AFTER, "function");
      }
      return false;
    }
  }
  else
  {
    bool default_seen = false;

    while (not scf->Eof())
    {
      scf->SkipWhite();
      if (scf->CheckSymbol(")"))
      {
        break;
      }

      if (!tfunc->params.empty())
      {
        if (!scf->CheckSymbol(","))
        {
          if (aemit_errors)
          {
            Error(DQERR_MISSING_COMMA, &scf->prevpos);
          }
          if (!fail_or_recover())
          {
            break;
          }
          continue;
        }
        scf->SkipWhite();
      }

      if (scf->CheckSymbol("..."))
      {
        tfunc->has_varargs = true;
        scf->SkipWhite();
        if (!scf->CheckSymbol(")"))
        {
          if (aemit_errors)
          {
            Error(DQERR_MISSING_CLOSE_PAREN_AFTER, "...");
          }
          if (atypespec)
          {
            return false;
          }
        }
        break;
      }

      string spname;
      if (!scf->ReadIdentifier(spname))
      {
        if (aemit_errors)
        {
          Error(DQERR_FUNCPAR_NAME_EXP, &scf->prevpos);
        }
        if (!fail_or_recover())
        {
          break;
        }
        continue;
      }

      if (!tfunc->ParNameValid(spname))
      {
        if (aemit_errors)
        {
          Error(DQERR_FUNCPAR_NAME_INVALID, spname, &scf->prevpos);
        }
        if (!fail_or_recover())
        {
          break;
        }
        continue;
      }

      scf->SkipWhite();
      if (!scf->CheckSymbol(":"))
      {
        if (aemit_errors)
        {
          Error(DQERR_TYPE_SPECIFIER_EXP_AFTER, spname, &scf->prevpos);
        }
        if (!fail_or_recover())
        {
          break;
        }
        continue;
      }

      EParamMode pmode = FPM_VALUE;
      scf->SkipWhite();
      string mode_keyword;
      if (scf->ReadIdentifier(mode_keyword, false) && ParseParamModeKeyword(mode_keyword, pmode))
      {
        scf->ReadIdentifier(mode_keyword);
      }

      OType * ptype = ParseTypeSpec(aemit_errors);
      if (!ptype)
      {
        if (atypespec)
        {
          return false;
        }

        if (!fail_or_recover())
        {
          break;
        }
        continue;
      }

      if ((TK_DYN_ARRAY == ptype->ResolveAlias()->kind) && !ParamModeIsRefLike(pmode))
      {
        if (aemit_errors)
        {
          Error(DQERR_NOT_SUPPORTED, "dynamic array value parameter");
        }
        if (atypespec)
        {
          return false;
        }
        if (!fail_or_recover())
        {
          break;
        }
        continue;
      }

      OFuncParam * fparam = tfunc->AddParam(spname, ptype, pmode);

      scf->SkipWhite();
      if (scf->CheckSymbol("="))
      {
        if (ParamModeIsRefLike(pmode))
        {
          if (aemit_errors)
          {
            Error(DQERR_FUNCPAR_DEFAULT_REF, spname);
          }
          if (!fail_or_recover())
          {
            break;
          }
          continue;
        }

        if (!SupportsFuncParamDefaultType(ptype))
        {
          if (aemit_errors)
          {
            Error(DQERR_FUNCPAR_DEFAULT_TYPE, spname, ptype->ResolveAlias()->name);
          }
          if (!fail_or_recover())
          {
            break;
          }
          continue;
        }

        default_seen = true;

        scf->SkipWhite();
        OScPosition defexpr_pos;
        scf->SaveCurPos(defexpr_pos);

        OExpr * defexpr = ParseExpression();
        if (!defexpr)
        {
          if (atypespec)
          {
            return false;
          }

          if (!fail_or_recover())
          {
            break;
          }
          continue;
        }

        if (!CheckAssignType(ptype, &defexpr, "Argument"))
        {
          OExpr::DeleteTree(defexpr);
          if (atypespec)
          {
            return false;
          }

          if (!fail_or_recover())
          {
            break;
          }
          continue;
        }

        OValue * defvalue = ptype->CreateValue();
        if (!defvalue)
        {
          if (aemit_errors)
          {
            Error(DQERR_FUNCPAR_DEFAULT_TYPE, spname, ptype->ResolveAlias()->name, &defexpr_pos);
          }
          OExpr::DeleteTree(defexpr);
          if (atypespec)
          {
            return false;
          }

          if (!fail_or_recover())
          {
            break;
          }
          continue;
        }

        if (!defvalue->CalculateConstant(defexpr, true))
        {
          delete defvalue;
          OExpr::DeleteTree(defexpr);
          if (atypespec)
          {
            return false;
          }

          if (!fail_or_recover())
          {
            break;
          }
          continue;
        }

        fparam->defvalue = new OValSymConst(defexpr_pos, format("__defarg_{}_{}", aowner_name, spname), ptype, defvalue);
        OExpr::DeleteTree(defexpr);
      }
      else if (default_seen)
      {
        if (aemit_errors)
        {
          Error(DQERR_FUNCPAR_DEFAULT_ORDER, spname);
        }

        if (atypespec)
        {
          return false;
        }
      }
    } // while

    if (berror and atypespec)
    {
      return false;
    }

  }

  if (tfunc->has_varargs && tfunc->params.empty())
  {
    if (aemit_errors)
    {
      Error(DQERR_VARARGS_ALONE);
    }
    if (atypespec)
    {
      return false;
    }
  }

  scf->SkipWhite();
  if (scf->CheckSymbol("->"))
  {
    tfunc->rettype = ParseTypeSpec(aemit_errors);
    if (!tfunc->rettype)
    {
      return false;
    }
    if (TK_DYN_ARRAY == tfunc->rettype->ResolveAlias()->kind)
    {
      if (aemit_errors)
      {
        Error(DQERR_NOT_SUPPORTED, "dynamic array return value");
      }
      return false;
    }
  }

  return true;
}

OTypeFunc * ODqCompParserExpr::ParseFunctionType(bool aemit_errors, const string & aowner_name)
{
  OTypeFunc * tfunc = new OTypeFunc(aowner_name);

  if (!ParseFunctionSignature(tfunc, true, aowner_name, aemit_errors))
  {
    delete tfunc;
    return nullptr;
  }

  return tfunc;
}

OExpr * ODqCompParserExpr::ParseExpression()
{
  OExpr * expr = ParseExprOr();
  // This is the parser-side fold for the full expression tree; later AST helpers only
  // need to fold again when they inject new conversion nodes after parsing.
  OExpr::FoldTree(&expr);
  return expr;
}

OExpr * ODqCompParserExpr::ParseExprOr()
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

OExpr * ODqCompParserExpr::ParseExprAnd()
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

OExpr * ODqCompParserExpr::ParseExprNot()
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

OExpr * ODqCompParserExpr::ParseComparison()
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
  else if (scf->CheckSymbol("is"))
  {
    scf->SkipWhite();
    OType * target_type = ParseTypeSpec();
    if (!target_type)
    {
      return FreeLeftRight(left, nullptr);
    }
    return new OIsExpr(left, target_type);
  }
  else
  {
    return left;
  }

  OExpr *  right = ParseExprAdd();
  if (!right)
  {
    return FreeLeftRight(left, nullptr);
  }

  auto * unresolved_left = dynamic_cast<OUnresolvedEnumItemExpr *>(left);
  auto * unresolved_right = dynamic_cast<OUnresolvedEnumItemExpr *>(right);
  auto * left_enum = dynamic_cast<OTypeEnum *>(left->ResolvedType());
  auto * right_enum = dynamic_cast<OTypeEnum *>(right->ResolvedType());
  if (unresolved_left && right_enum)
  {
    ConvertExprToType(right_enum, &left, EXPCF_GENERATE_ERRORS);
    left_enum = right_enum;
  }
  else if (unresolved_right && left_enum)
  {
    ConvertExprToType(left_enum, &right, EXPCF_GENERATE_ERRORS);
    right_enum = left_enum;
  }
  else if (unresolved_left || unresolved_right)
  {
    string item_name = unresolved_left ? unresolved_left->item_name : unresolved_right->item_name;
    Error(DQERR_ENUM_TYPE_INFER, item_name);
    OExpr::DeleteTree(left);
    OExpr::DeleteTree(right);
    return new OBoolLit(false);
  }
  if ((left_enum || right_enum) && left_enum != right_enum)
  {
    Error(DQERR_TYPEMISM_FOR_OP, left->ptype->name, GetCompareSymbol(op), right->ptype->name);
    OExpr::DeleteTree(left);
    OExpr::DeleteTree(right);
    return new OBoolLit(false);
  }

  auto empty_array_literal = [](OExpr * expr) -> bool
  {
    auto * arrlit = dynamic_cast<OArrayLit *>(expr);
    return arrlit && arrlit->elements.empty();
  };
  auto array_empty_compare = [&](OExpr * a, OExpr * b) -> bool
  {
    OType * atype = a && a->ptype ? a->ptype->ResolveAlias() : nullptr;
    return (COMPOP_EQ == op || COMPOP_NE == op)
           && atype
           && (TK_ARRAY_SLICE == atype->kind || TK_DYN_ARRAY == atype->kind)
           && empty_array_literal(b);
  };
  auto compare_symbol = [](ECompareOp cmp) -> string
  {
    switch (cmp)
    {
      case COMPOP_EQ: return "==";
      case COMPOP_NE: return "<>";
      case COMPOP_LT: return "<";
      case COMPOP_GT: return ">";
      case COMPOP_LE: return "<=";
      case COMPOP_GE: return ">=";
      default:        return "?";
    }
  };
  OType * ltype = left && left->ptype ? left->ptype->ResolveAlias() : nullptr;
  OType * rtype = right && right->ptype ? right->ptype->ResolveAlias() : nullptr;
  bool ltext = IsStringComparableTextType(ltype);
  bool rtext = IsStringComparableTextType(rtype);
  bool has_string_family = IsStringFamilyTextType(ltype) || IsStringFamilyTextType(rtype);
  if ((ltext || rtext) && has_string_family)
  {
    if ((COMPOP_EQ != op && COMPOP_NE != op) || !ltext || !rtext)
    {
      Error(DQERR_TYPEMISM_FOR_OP, left->ptype->name, compare_symbol(op), right->ptype->name);
      OExpr::DeleteTree(left);
      OExpr::DeleteTree(right);
      return new OBoolLit(false);
    }
    return new OCompareExpr(op, left, right);
  }
  if ((ltype && (TK_DYN_ARRAY == ltype->kind || TK_ARRAY_SLICE == ltype->kind))
      || (rtype && (TK_DYN_ARRAY == rtype->kind || TK_ARRAY_SLICE == rtype->kind)))
  {
    if (!array_empty_compare(left, right) && !array_empty_compare(right, left))
    {
      Error(DQERR_TYPEMISM_FOR_OP, left->ptype->name, compare_symbol(op), right->ptype->name);
      OExpr::DeleteTree(left);
      OExpr::DeleteTree(right);
      return new OBoolLit(false);
    }
  }

  HarmonizeNumericOperands(&left, &right);

  return new OCompareExpr(op, left, right);
}

OExpr * ODqCompParserExpr::ParseExprAdd()
{
  static const BinOpEntry ops[] = { {"+", BINOP_ADD}, {"-", BINOP_SUB} };
  return ParseBinOpLevel(&ODqCompParserExpr::ParseExprMul, ops, 2);
}

OExpr * ODqCompParserExpr::ParseExprMul()
{
  static const BinOpEntry ops[] = { {"*", BINOP_MUL} };
  return ParseBinOpLevel(&ODqCompParserExpr::ParseExprDiv, ops, 1);
}

OExpr * ODqCompParserExpr::ParseExprDiv()
{
  static const BinOpEntry ops[] = { {"/", BINOP_DIV}, {"IDIV", BINOP_IDIV}, {"IMOD", BINOP_IMOD} };
  return ParseBinOpLevel(&ODqCompParserExpr::ParseExprBinOr, ops, 3);
}

OExpr * ODqCompParserExpr::ParseExprBinOr()
{
  static const BinOpEntry ops[] = { {"OR", BINOP_IOR}, {"XOR", BINOP_IXOR} };
  return ParseBinOpLevel(&ODqCompParserExpr::ParseExprBinAnd, ops, 2);
}

OExpr * ODqCompParserExpr::ParseExprBinAnd()
{
  static const BinOpEntry ops[] = { {"AND", BINOP_IAND} };
  return ParseBinOpLevel(&ODqCompParserExpr::ParseExprShift, ops, 1);
}

OExpr * ODqCompParserExpr::ParseExprShift()
{
  static const BinOpEntry ops[] = { {"<<", BINOP_ISHL}, {"SHL", BINOP_ISHL}, {">>", BINOP_ISHR}, {"SHR", BINOP_ISHR} };
  return ParseBinOpLevel(&ODqCompParserExpr::ParseExprUnary, ops, 4);
}

OExpr * ODqCompParserExpr::ParseExprUnary()
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

OLValueExpr * ODqCompParserExpr::ParseAddressableExpr()
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
  if (auto * property = dynamic_cast<OPropertyExpr *>(lval))
  {
    Error(DQERR_PROPERTY_NOT_ADDRESSABLE, property->property->name);
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

OExpr * ODqCompParserExpr::ParseExprPostfix()
{
  OExpr * result = ParseExprPrimary();
  if (!result) return nullptr;
  return ParsePostfix(result);
}

OExpr * ODqCompParserExpr::ParseExplicitCastExpr(bool * rattempted)
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

  if (!ConvertExprToType(dsttype, &srcexpr, EXPCF_GENERATE_ERRORS | EXPCF_EXPLICIT_CAST))
  {
    delete srcexpr;
    return nullptr;
  }

  return srcexpr;
}

OExpr * ODqCompParserExpr::ParseDynArrayMethod(OExpr * receiver_expr, OLValueExpr * receiver, const string & membername)
{
  if (!scf->CheckSymbol("("))
  {
    Error(DQERR_MEMBER_UNKNOWN, membername, receiver->ptype->name);
    return receiver_expr;
  }

  vector<TRawCallArg> rawargs;
  int64_t prev_context_len = array_index_context_len;
  OLValueExpr * prev_context_lval = array_index_context_lval;
  array_index_context_len = -1;
  array_index_context_lval = receiver;
  if (!ParseRawCallArguments(membername, rawargs))
  {
    array_index_context_len = prev_context_len;
    array_index_context_lval = prev_context_lval;
    return nullptr;
  }
  array_index_context_len = prev_context_len;
  array_index_context_lval = prev_context_lval;

  auto free_and_fail = [&]() -> OExpr *
  {
    FreeRawCallArguments(rawargs);
    delete receiver_expr;
    return nullptr;
  };

  auto check_count = [&](size_t mincnt, size_t maxcnt) -> bool
  {
    if (rawargs.size() < mincnt)
    {
      Error(DQERR_FUNC_ARGS_TOO_FEW, to_string(rawargs.size()), membername, to_string(mincnt));
      return false;
    }
    if (rawargs.size() > maxcnt)
    {
      Error(DQERR_FUNC_ARGS_TOO_MANY, membername, to_string(maxcnt));
      return false;
    }
    return true;
  };

  OTypeDynArray * dyntype = static_cast<OTypeDynArray *>(receiver->ptype->ResolveAlias());
  EDynArrayMethod dynmethod = DYNM_CLEAR;
  vector<OType *> argtypes;
  auto is_range_arg = [](OExpr * expr) -> bool
  {
    OType * rtype = expr && expr->ptype ? expr->ptype->ResolveAlias() : nullptr;
    return rtype && (TK_ARRAY == rtype->kind || TK_ARRAY_SLICE == rtype->kind || TK_DYN_ARRAY == rtype->kind);
  };

  auto prefer_slice_append = [&](OExpr * expr) -> bool
  {
    if (!is_range_arg(expr)) return false;
    int cost_elem = dyntype->elemtype->GetConversionCostFromExpr(expr, EXPCF_ALLOW_ARRAY_LITERAL_SLICE | EXPCF_ALLOW_LAZY_CSTRING);
    if (cost_elem >= 0)
    {
      int cost_slice = dyntype->elemtype->GetSliceType()->GetConversionCostFromExpr(expr, EXPCF_ALLOW_ARRAY_LITERAL_SLICE | EXPCF_ALLOW_LAZY_CSTRING);
      if (cost_slice < 0 || cost_elem >= cost_slice) return false;
    }
    return true;
  };

  if ("Clear" == membername)
  {
    if (!check_count(0, 1)) return free_and_fail();
    dynmethod = DYNM_CLEAR;
    if (!rawargs.empty())
    {
      argtypes = {g_builtins->type_bool};
    }
  }
  else if ("Reserve" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    dynmethod = DYNM_RESERVE;
    argtypes = {g_builtins->type_uint};
  }
  else if ("Compact" == membername)
  {
    if (!check_count(0, 0)) return free_and_fail();
    dynmethod = DYNM_COMPACT;
  }
  else if ("SetLength" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    dynmethod = DYNM_SET_LENGTH;
    argtypes = {g_builtins->type_uint};
  }
  else if ("SetCapacity" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    dynmethod = DYNM_SET_CAPACITY;
    argtypes = {g_builtins->type_uint};
  }
  else if ("Append" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    if (prefer_slice_append(rawargs[0].expr))
    {
      dynmethod = DYNM_APPEND_SLICE;
      argtypes = {dyntype->elemtype->GetSliceType()};
    }
    else
    {
      dynmethod = DYNM_APPEND;
      argtypes = {dyntype->elemtype};
    }
  }
  else if ("AppendSlice" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    dynmethod = DYNM_APPEND_SLICE;
    argtypes = {dyntype->elemtype->GetSliceType()};
  }
  else if ("Prepend" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    if (prefer_slice_append(rawargs[0].expr))
    {
      dynmethod = DYNM_PREPEND_SLICE;
      argtypes = {dyntype->elemtype->GetSliceType()};
    }
    else
    {
      dynmethod = DYNM_PREPEND;
      argtypes = {dyntype->elemtype};
    }
  }
  else if ("Insert" == membername)
  {
    if (!check_count(2, 2)) return free_and_fail();
    if (prefer_slice_append(rawargs[1].expr))
    {
      dynmethod = DYNM_INSERT_SLICE;
      argtypes = {g_builtins->type_int, dyntype->elemtype->GetSliceType()};
    }
    else
    {
      dynmethod = DYNM_INSERT;
      argtypes = {g_builtins->type_int, dyntype->elemtype};
    }
  }
  else if ("InsertSlice" == membername)
  {
    if (!check_count(2, 2)) return free_and_fail();
    dynmethod = DYNM_INSERT_SLICE;
    argtypes = {g_builtins->type_int, dyntype->elemtype->GetSliceType()};
  }
  else if ("Delete" == membername)
  {
    if (!check_count(1, 2)) return free_and_fail();
    dynmethod = DYNM_DELETE;
    argtypes = {g_builtins->type_int};
    if (rawargs.size() > 1)
    {
      argtypes.push_back(g_builtins->type_int);
    }
  }
  else if ("Clone" == membername)
  {
    if (!check_count(0, 0)) return free_and_fail();
    dynmethod = DYNM_CLONE;
  }
  else if ("Pop" == membername)
  {
    if (!check_count(0, 0)) return free_and_fail();
    dynmethod = DYNM_POP;
  }
  else if ("PopFirst" == membername)
  {
    if (!check_count(0, 0)) return free_and_fail();
    dynmethod = DYNM_POP_FIRST;
  }
  else
  {
    Error(DQERR_MEMBER_UNKNOWN, membername, receiver->ptype->name);
    return free_and_fail();
  }

  OType * rettype = nullptr;
  if (DYNM_CLONE == dynmethod)
  {
    rettype = dyntype;
  }
  else if (DYNM_POP == dynmethod || DYNM_POP_FIRST == dynmethod)
  {
    rettype = dyntype->elemtype;
  }
  auto * callexpr = new ODynArrayMethodCallExpr(dynmethod, receiver, rettype);
  for (size_t i = 0; i < rawargs.size(); ++i)
  {
    OExpr * argexpr = rawargs[i].expr;
    rawargs[i].expr = nullptr;
    uint32_t conv_flags = EXPCF_GENERATE_ERRORS | EXPCF_ALLOW_LAZY_CSTRING;
    if (argtypes[i]->ResolveAlias()->kind == TK_ARRAY_SLICE)
    {
      conv_flags |= EXPCF_ALLOW_ARRAY_LITERAL_SLICE;
    }
    if (!ConvertExprToType(argtypes[i], &argexpr, conv_flags))
    {
      OExpr::DeleteTree(argexpr);
      delete callexpr;
      return free_and_fail();
    }
    callexpr->args.push_back(argexpr);
  }
  FreeRawCallArguments(rawargs);
  return callexpr;
}

OExpr * ODqCompParserExpr::ParseCStringMethod(OExpr * receiver_expr, OLValueExpr * receiver, const string & membername)
{
  vector<TRawCallArg> rawargs;
  if (!scf->CheckSymbol("("))
  {
    Error(DQERR_FUNC_CALL_PARENTH, membername);
    return receiver_expr;
  }
  int64_t prev_context_len = array_index_context_len;
  OLValueExpr * prev_context_lval = array_index_context_lval;
  array_index_context_len = -1;
  array_index_context_lval = receiver;
  if (!ParseRawCallArguments(membername, rawargs))
  {
    array_index_context_len = prev_context_len;
    array_index_context_lval = prev_context_lval;
    return nullptr;
  }
  array_index_context_len = prev_context_len;
  array_index_context_lval = prev_context_lval;

  auto free_and_fail = [&]() -> OExpr *
  {
    FreeRawCallArguments(rawargs);
    delete receiver_expr;
    return nullptr;
  };

  auto check_count = [&](size_t mincnt, size_t maxcnt) -> bool
  {
    if (rawargs.size() < mincnt)
    {
      Error(DQERR_FUNC_ARGS_TOO_FEW, to_string(rawargs.size()), membername, to_string(mincnt));
      return false;
    }
    if (rawargs.size() > maxcnt)
    {
      Error(DQERR_FUNC_ARGS_TOO_MANY, membername, to_string(maxcnt));
      return false;
    }
    return true;
  };

  ECStringMethod method = CSM_CLEAR;
  vector<OType *> argtypes;
  size_t source_arg_index = rawargs.size();
  if ("Clear" == membername)
  {
    if (!check_count(0, 0)) return free_and_fail();
    method = CSM_CLEAR;
  }
  else if ("AppendChar" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    method = CSM_APPEND;
    argtypes.push_back(g_builtins->type_char);
  }
  else if ("Set" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    method = CSM_SET;
    source_arg_index = 0;
  }
  else if ("Append" == membername || "Add" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    method = CSM_APPEND;
    source_arg_index = 0;
  }
  else if ("AddFmt" == membername)
  {
    if (!check_count(2, 2)) return free_and_fail();
    method = CSM_ADDFMT;
    argtypes.push_back(g_builtins->type_strview);
    argtypes.push_back(g_builtins->type_anyvalue->GetSliceType());
    if (!EnsureTextFormatRtlUse()) return free_and_fail();
  }
  else if ("Prepend" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    method = CSM_PREPEND;
    source_arg_index = 0;
  }
  else if ("Insert" == membername)
  {
    if (!check_count(2, 2)) return free_and_fail();
    method = CSM_INSERT;
    argtypes.push_back(g_builtins->type_int);
    source_arg_index = 1;
  }
  else if ("Delete" == membername)
  {
    if (!check_count(1, 2)) return free_and_fail();
    method = CSM_DELETE;
    argtypes.push_back(g_builtins->type_int);
    if (rawargs.size() > 1)
    {
      argtypes.push_back(g_builtins->type_int);
    }
  }
  else
  {
    Error(DQERR_MEMBER_UNKNOWN, membername, receiver->ptype->name);
    return free_and_fail();
  }

  if (!EnsureCStringRtlUse())
  {
    return free_and_fail();
  }

  auto * callexpr = new OCStringMethodCallExpr(receiver, method);
  for (size_t i = 0; i < rawargs.size(); ++i)
  {
    OExpr * argexpr = rawargs[i].expr;
    rawargs[i].expr = nullptr;
    if (i < argtypes.size())
    {
      if (!ConvertExprToType(argtypes[i], &argexpr, EXPCF_GENERATE_ERRORS))
      {
        OExpr::DeleteTree(argexpr);
        delete callexpr;
        return free_and_fail();
      }
    }
    if ((i == source_arg_index) && !IsCStringMethodSourceType(argexpr->ResolvedType()))
    {
      ErrorTxt(DQERR_CSTR_CONVERSION, "cstring method source must be char, cchar, str, strview, cstring, or ^cchar");
      OExpr::DeleteTree(argexpr);
      delete callexpr;
      return free_and_fail();
    }
    callexpr->args.push_back(argexpr);
  }
  FreeRawCallArguments(rawargs);
  return callexpr;
}

OExpr * ODqCompParserExpr::ParseStringMethod(OExpr * receiver_expr, OLValueExpr * receiver, const string & membername)
{
  vector<TRawCallArg> rawargs;
  if (!scf->CheckSymbol("("))
  {
    Error(DQERR_FUNC_CALL_PARENTH, membername);
    return receiver_expr;
  }
  int64_t prev_context_len = array_index_context_len;
  OLValueExpr * prev_context_lval = array_index_context_lval;
  array_index_context_len = -1;
  array_index_context_lval = receiver;
  if (!ParseRawCallArguments(membername, rawargs))
  {
    array_index_context_len = prev_context_len;
    array_index_context_lval = prev_context_lval;
    return nullptr;
  }
  array_index_context_len = prev_context_len;
  array_index_context_lval = prev_context_lval;

  auto free_and_fail = [&]() -> OExpr *
  {
    FreeRawCallArguments(rawargs);
    delete receiver_expr;
    return nullptr;
  };

  auto check_count = [&](size_t mincnt, size_t maxcnt) -> bool
  {
    if (rawargs.size() < mincnt)
    {
      Error(DQERR_FUNC_ARGS_TOO_FEW, to_string(rawargs.size()), membername, to_string(mincnt));
      return false;
    }
    if (rawargs.size() > maxcnt)
    {
      Error(DQERR_FUNC_ARGS_TOO_MANY, membername, to_string(maxcnt));
      return false;
    }
    return true;
  };

  EStringMethod method = STRM_CLEAR;
  vector<OType *> argtypes;
  size_t source_arg_index = rawargs.size();
  OType * rettype = nullptr;

  if ("Clear" == membername)
  {
    if (!check_count(0, 1)) return free_and_fail();
    method = STRM_CLEAR;
    if (!rawargs.empty()) argtypes.push_back(g_builtins->type_bool);
  }
  else if ("Set" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    method = STRM_SET;
    source_arg_index = 0;
  }
  else if ("AppendChar" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    method = STRM_APPEND;
    argtypes.push_back(g_builtins->type_char);
  }
  else if ("Reserve" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    method = STRM_RESERVE;
    argtypes.push_back(g_builtins->type_uint);
  }
  else if ("Compact" == membername)
  {
    if (!check_count(0, 0)) return free_and_fail();
    method = STRM_COMPACT;
  }
  else if ("SetLength" == membername)
  {
    if (!check_count(2, 2)) return free_and_fail();
    method = STRM_SET_LENGTH;
    argtypes.push_back(g_builtins->type_uint);
    argtypes.push_back(g_builtins->type_char);
  }
  else if ("SetCapacity" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    method = STRM_SET_CAPACITY;
    argtypes.push_back(g_builtins->type_uint);
  }
  else if ("Truncate" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    method = STRM_TRUNCATE;
    argtypes.push_back(g_builtins->type_uint);
  }
  else if ("Append" == membername || "Add" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    method = STRM_APPEND;
    source_arg_index = 0;
  }
  else if ("AddFmt" == membername)
  {
    if (!check_count(2, 2)) return free_and_fail();
    method = STRM_ADDFMT;
    argtypes.push_back(g_builtins->type_strview);
    argtypes.push_back(g_builtins->type_anyvalue->GetSliceType());
    if (!EnsureTextFormatRtlUse()) return free_and_fail();
  }
  else if ("Prepend" == membername)
  {
    if (!check_count(1, 1)) return free_and_fail();
    method = STRM_PREPEND;
    source_arg_index = 0;
  }
  else if ("Insert" == membername)
  {
    if (!check_count(2, 2)) return free_and_fail();
    method = STRM_INSERT;
    argtypes.push_back(g_builtins->type_int);
    source_arg_index = 1;
  }
  else if ("Delete" == membername)
  {
    if (!check_count(1, 2)) return free_and_fail();
    method = STRM_DELETE;
    argtypes.push_back(g_builtins->type_int);
    if (rawargs.size() > 1) argtypes.push_back(g_builtins->type_int);
  }
  else if ("Clone" == membername)
  {
    if (!check_count(0, 0)) return free_and_fail();
    method = STRM_CLONE;
    rettype = g_builtins->type_str;
  }
  else if ("Pop" == membername)
  {
    if (!check_count(0, 1)) return free_and_fail();
    if (rawargs.empty())
    {
      method = STRM_POP_CHAR;
      rettype = g_builtins->type_char;
    }
    else
    {
      method = STRM_POP;
      argtypes.push_back(g_builtins->type_int);
      rettype = g_builtins->type_str;
    }
  }
  else if ("PopFirst" == membername)
  {
    if (!check_count(0, 1)) return free_and_fail();
    if (rawargs.empty())
    {
      method = STRM_POP_FIRST_CHAR;
      rettype = g_builtins->type_char;
    }
    else
    {
      method = STRM_POP_FIRST;
      argtypes.push_back(g_builtins->type_int);
      rettype = g_builtins->type_str;
    }
  }
  else
  {
    Error(DQERR_MEMBER_UNKNOWN, membername, receiver->ptype->name);
    return free_and_fail();
  }

  if (!EnsureDynStringRtlUse())
  {
    return free_and_fail();
  }

  auto * callexpr = new OStringMethodCallExpr(receiver, method, rettype);
  for (size_t i = 0; i < rawargs.size(); ++i)
  {
    OExpr * argexpr = rawargs[i].expr;
    rawargs[i].expr = nullptr;
    if (i < argtypes.size())
    {
      if (!ConvertExprToType(argtypes[i], &argexpr, EXPCF_GENERATE_ERRORS | EXPCF_ALLOW_LAZY_CSTRING))
      {
        OExpr::DeleteTree(argexpr);
        delete callexpr;
        return free_and_fail();
      }
    }
    if ((i == source_arg_index) && !IsStringMethodSourceType(argexpr->ResolvedType()))
    {
      ErrorTxt(DQERR_TYPEMISM, "string method source must be char, cchar, str, strview, cstring, or ^cchar");
      OExpr::DeleteTree(argexpr);
      delete callexpr;
      return free_and_fail();
    }
    callexpr->args.push_back(argexpr);
  }
  FreeRawCallArguments(rawargs);
  return callexpr;
}

OExpr * ODqCompParserExpr::ParseAnyValueMethod(OExpr * receiver_expr, OLValueExpr * receiver, const string & membername)
{
  vector<TRawCallArg> rawargs;
  if (!scf->CheckSymbol("("))
  {
    Error(DQERR_FUNC_CALL_PARENTH, membername);
    return receiver_expr;
  }
  if (!ParseRawCallArguments(membername, rawargs))
  {
    return nullptr;
  }

  auto free_and_fail = [&]() -> OExpr *
  {
    FreeRawCallArguments(rawargs);
    delete receiver_expr;
    return nullptr;
  };

  auto check_count = [&](size_t mincnt, size_t maxcnt) -> bool
  {
    if (rawargs.size() < mincnt)
    {
      Error(DQERR_FUNC_ARGS_TOO_FEW, to_string(rawargs.size()), membername, to_string(mincnt));
      return false;
    }
    if (rawargs.size() > maxcnt)
    {
      Error(DQERR_FUNC_ARGS_TOO_MANY, membername, to_string(maxcnt));
      return false;
    }
    return true;
  };

  EAnyValueMethod method = AVM_IS_NULL;
  vector<OType *> argtypes;
  size_t text_arg_index = rawargs.size();
  OType * rettype = nullptr;

  if ("IsNull" == membername)          { if (!check_count(0, 0)) return free_and_fail(); method = AVM_IS_NULL; rettype = g_builtins->type_bool; }
  else if ("SetNull" == membername)    { if (!check_count(0, 0)) return free_and_fail(); method = AVM_SET_NULL; }
  else if ("IsNumber" == membername)   { if (!check_count(0, 0)) return free_and_fail(); method = AVM_IS_NUMBER; rettype = g_builtins->type_bool; }
  else if ("IsInt" == membername)      { if (!check_count(0, 0)) return free_and_fail(); method = AVM_IS_INT; rettype = g_builtins->type_bool; }
  else if ("IsSInt" == membername)     { if (!check_count(0, 0)) return free_and_fail(); method = AVM_IS_SINT; rettype = g_builtins->type_bool; }
  else if ("IsUint" == membername)     { if (!check_count(0, 0)) return free_and_fail(); method = AVM_IS_UINT; rettype = g_builtins->type_bool; }
  else if ("AsInt" == membername)      { if (!check_count(1, 1)) return free_and_fail(); method = AVM_AS_INT; argtypes = {g_builtins->type_int}; rettype = g_builtins->type_int; }
  else if ("AsUint" == membername)     { if (!check_count(1, 1)) return free_and_fail(); method = AVM_AS_UINT; argtypes = {g_builtins->type_uint}; rettype = g_builtins->type_uint; }
  else if ("SetInt" == membername)     { if (!check_count(1, 1)) return free_and_fail(); method = AVM_SET_INT; argtypes = {g_builtins->type_int}; }
  else if ("SetUInt" == membername)    { if (!check_count(1, 1)) return free_and_fail(); method = AVM_SET_UINT; argtypes = {g_builtins->type_uint}; }
  else if ("IsBool" == membername)     { if (!check_count(0, 0)) return free_and_fail(); method = AVM_IS_BOOL; rettype = g_builtins->type_bool; }
  else if ("AsBool" == membername)     { if (!check_count(1, 1)) return free_and_fail(); method = AVM_AS_BOOL; argtypes = {g_builtins->type_bool}; rettype = g_builtins->type_bool; }
  else if ("SetBool" == membername)    { if (!check_count(1, 1)) return free_and_fail(); method = AVM_SET_BOOL; argtypes = {g_builtins->type_bool}; }
  else if ("IsPointer" == membername)  { if (!check_count(0, 0)) return free_and_fail(); method = AVM_IS_POINTER; rettype = g_builtins->type_bool; }
  else if ("AsPointer" == membername)  { if (!check_count(1, 1)) return free_and_fail(); method = AVM_AS_POINTER; argtypes = {g_builtins->type_pointer}; rettype = g_builtins->type_pointer; }
  else if ("SetPointer" == membername) { if (!check_count(1, 1)) return free_and_fail(); method = AVM_SET_POINTER; argtypes = {g_builtins->type_pointer}; }
  else if ("IsFloat" == membername)    { if (!check_count(0, 0)) return free_and_fail(); method = AVM_IS_FLOAT; rettype = g_builtins->type_bool; }
  else if ("IsFloat32" == membername)  { if (!check_count(0, 0)) return free_and_fail(); method = AVM_IS_FLOAT32; rettype = g_builtins->type_bool; }
  else if ("IsFloat64" == membername)  { if (!check_count(0, 0)) return free_and_fail(); method = AVM_IS_FLOAT64; rettype = g_builtins->type_bool; }
  else if ("AsFloat" == membername)    { if (!check_count(1, 1)) return free_and_fail(); method = AVM_AS_FLOAT; argtypes = {g_builtins->type_float}; rettype = g_builtins->type_float; }
  else if ("AsFloat32" == membername)  { if (!check_count(1, 1)) return free_and_fail(); method = AVM_AS_FLOAT32; argtypes = {g_builtins->type_float32}; rettype = g_builtins->type_float32; }
  else if ("AsFloat64" == membername)  { if (!check_count(1, 1)) return free_and_fail(); method = AVM_AS_FLOAT64; argtypes = {g_builtins->type_float64}; rettype = g_builtins->type_float64; }
  else if ("SetFloat" == membername)   { if (!check_count(1, 1)) return free_and_fail(); method = AVM_SET_FLOAT; argtypes = {g_builtins->type_float}; }
  else if ("SetFloat32" == membername) { if (!check_count(1, 1)) return free_and_fail(); method = AVM_SET_FLOAT32; argtypes = {g_builtins->type_float32}; }
  else if ("SetFloat64" == membername) { if (!check_count(1, 1)) return free_and_fail(); method = AVM_SET_FLOAT64; argtypes = {g_builtins->type_float64}; }
  else if ("IsText" == membername)     { if (!check_count(0, 0)) return free_and_fail(); method = AVM_IS_TEXT; rettype = g_builtins->type_bool; }
  else if ("AsText" == membername)     { if (!check_count(1, 1)) return free_and_fail(); method = AVM_AS_TEXT; text_arg_index = 0; rettype = g_builtins->type_strview; }
  else if ("AsStrView" == membername)  { if (!check_count(1, 1)) return free_and_fail(); method = AVM_AS_TEXT; text_arg_index = 0; rettype = g_builtins->type_strview; }
  else if ("SetText" == membername)    { if (!check_count(1, 1)) return free_and_fail(); method = AVM_SET_TEXT; text_arg_index = 0; }
  else if ("SetCString" == membername) { if (!check_count(1, 1)) return free_and_fail(); method = AVM_SET_CSTRING; text_arg_index = 0; }
  else if ("IsStr" == membername)      { if (!check_count(0, 0)) return free_and_fail(); method = AVM_IS_STR; rettype = g_builtins->type_bool; }
  else if ("AsStr" == membername)      { if (!check_count(1, 1)) return free_and_fail(); method = AVM_AS_STR; text_arg_index = 0; rettype = g_builtins->type_str; }
  else if ("SetStr" == membername)     { if (!check_count(1, 1)) return free_and_fail(); method = AVM_SET_STR; text_arg_index = 0; }
  else
  {
    Error(DQERR_MEMBER_UNKNOWN, membername, receiver->ptype->name);
    return free_and_fail();
  }

  if (!EnsureAnyValueRtlUse())
  {
    return free_and_fail();
  }

  auto * callexpr = new OAnyValueMethodCallExpr(receiver, method, rettype);
  for (size_t i = 0; i < rawargs.size(); ++i)
  {
    OExpr * argexpr = rawargs[i].expr;
    rawargs[i].expr = nullptr;
    if (i < argtypes.size())
    {
      if (!ConvertExprToType(argtypes[i], &argexpr, EXPCF_GENERATE_ERRORS | EXPCF_ALLOW_LAZY_CSTRING))
      {
        OExpr::DeleteTree(argexpr);
        delete callexpr;
        return free_and_fail();
      }
    }
    if ((i == text_arg_index) && !IsTextSourceType(argexpr->ResolvedType()))
    {
      ErrorTxt(DQERR_TYPEMISM, "anyvalue text method source must be char, cchar, str, strview, cstring, or ^cchar");
      OExpr::DeleteTree(argexpr);
      delete callexpr;
      return free_and_fail();
    }
    callexpr->args.push_back(argexpr);
  }
  FreeRawCallArguments(rawargs);
  return callexpr;
}

OExpr * ODqCompParserExpr::ParsePostfix(OExpr * base)
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

    if (auto * property_expr = dynamic_cast<OPropertyExpr *>(result);
        property_expr && property_expr->property->IsIndexed() && property_expr->indices.empty()
        && scf->CheckSymbol("["))
    {
      if (!ParsePropertyIndices(property_expr))
      {
        return result;
      }
      CheckPropertyReadable(property_expr);
      continue;
    }

    OType * resolved_result_type = result->ResolvedType();
    auto * result_object_type = dynamic_cast<OTypeObject *>(resolved_result_type);
    if (result_object_type && scf->CheckSymbol("["))
    {
      OCompoundType * decl_type = nullptr;
      OValSymProperty * default_property = result_object_type->FindDefaultProperty(&decl_type);
      if (!default_property || !ObjectMemberAccessAllowed(decl_type, default_property))
      {
        Error(DQERR_MEMBER_UNKNOWN, "default property", result_object_type->name);
        return result;
      }
      auto * property_expr = new OPropertyExpr(result, default_property);
      result = property_expr;
      if (!ParsePropertyIndices(property_expr))
      {
        return result;
      }
      CheckPropertyReadable(property_expr);
      continue;
    }

    if (!lval && result_object_type && scf->CheckSymbol("."))
    {
      string membername;
      scf->SkipWhite();
      if (!scf->ReadIdentifier(membername))
      {
        Error(DQERR_MEMBER_NAME_EXPECTED);
        return result;
      }
      OCompoundType * decl_type = result_object_type;
      OValSym * member = result_object_type->FindMemberSymbol(membername, &decl_type);
      auto * property = dynamic_cast<OValSymProperty *>(member);
      if (!property || !ObjectMemberAccessAllowed(decl_type, property))
      {
        Error(DQERR_MEMBER_UNKNOWN, membername, result_object_type->name);
        return result;
      }
      auto * property_expr = new OPropertyExpr(result, property);
      result = property_expr;
      if (!property->IsIndexed())
      {
        CheckPropertyReadable(property_expr);
      }
      continue;
    }

    if (TK_ENUM == tk && scf->CheckSymbol("."))
    {
      string membername;
      scf->SkipWhite();
      if (!scf->ReadIdentifier(membername))
      {
        Error(DQERR_MEMBER_NAME_EXPECTED);
        return result;
      }
      if ("ord" == membername)
      {
        result = new OEnumOrdExpr(result);
        continue;
      }
      Error(DQERR_MEMBER_UNKNOWN, membername, result->ptype->name);
      return result;
    }

    if (lval)
    {
      // Struct member access on a compound lvalue or a ^compound pointer: x.field / p.field
      if (scf->CheckSymbol("."))
      {
        string membername;
        scf->SkipWhite();
        if (not scf->ReadIdentifier(membername))
        {
          Error(DQERR_MEMBER_NAME_EXPECTED);
          return result;
        }

        if (TK_ARRAY == tk || TK_ARRAY_SLICE == tk || TK_DYN_ARRAY == tk)
        {
          if ("length" == membername)
          {
            result = new OArrayMetaFieldExpr(lval, lval->ptype, AMF_LENGTH);
            continue;
          }
          if ((TK_DYN_ARRAY == tk) && ("capacity" == membername))
          {
            result = new OArrayMetaFieldExpr(lval, lval->ptype, AMF_CAPACITY);
            continue;
          }
          if ((TK_DYN_ARRAY == tk) && ("refcount" == membername))
          {
            result = new OArrayMetaFieldExpr(lval, lval->ptype, AMF_REFCOUNT);
            continue;
          }
          if (TK_DYN_ARRAY == tk)
          {
            result = ParseDynArrayMethod(result, lval, membername);
            if (!result) return nullptr;
            continue;
          }

          Error(DQERR_MEMBER_UNKNOWN, membername, lval->ptype->name);
          if (scf->CheckSymbol("("))
          {
            vector<TRawCallArg> rawargs;
            ParseRawCallArguments(membername, rawargs);
            FreeRawCallArguments(rawargs);
            delete result;
            result = new OInvalidCallExpr();
          }
          return result;
        }

        if (TK_CSTRING == tk)
        {
          if ("length" == membername)
          {
            result = new OCStringMetaFieldExpr(lval, CSMF_LENGTH);
            continue;
          }
          if ("maxlength" == membername)
          {
            result = new OCStringMetaFieldExpr(lval, CSMF_MAXLENGTH);
            continue;
          }
          if ("storage_size" == membername)
          {
            result = new OCStringMetaFieldExpr(lval, CSMF_STORAGE_SIZE);
            continue;
          }
          result = ParseCStringMethod(result, lval, membername);
          if (!result) return nullptr;
          continue;
        }

        if (TK_DYNSTR == tk || TK_STRVIEW == tk)
        {
          if ("length" == membername)
          {
            result = new OStringMetaFieldExpr(lval, SMF_LENGTH);
            continue;
          }
          if (TK_DYNSTR == tk && "capacity" == membername)
          {
            result = new OStringMetaFieldExpr(lval, SMF_CAPACITY);
            continue;
          }
          if (TK_DYNSTR == tk && "refcount" == membername)
          {
            result = new OStringMetaFieldExpr(lval, SMF_REFCOUNT);
            continue;
          }
          if (TK_DYNSTR == tk)
          {
            result = ParseStringMethod(result, lval, membername);
            if (!result) return nullptr;
            continue;
          }

          Error(DQERR_MEMBER_UNKNOWN, membername, lval->ptype->name);
          if (scf->CheckSymbol("("))
          {
            vector<TRawCallArg> rawargs;
            ParseRawCallArguments(membername, rawargs);
            FreeRawCallArguments(rawargs);
            delete result;
            result = new OInvalidCallExpr();
          }
          return result;
        }

        if (TK_ANYVALUE == tk)
        {
          result = ParseAnyValueMethod(result, lval, membername);
          if (!result) return nullptr;
          continue;
        }

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

        OCompoundType * decl_type = ctype;
        OValSym * objsym = ctype->FindMemberSymbol(membername, &decl_type);
        if (auto * property = dynamic_cast<OValSymProperty *>(objsym))
        {
          if (!ObjectMemberAccessAllowed(decl_type, property))
          {
            Error(DQERR_MEMBER_UNKNOWN, membername, ctype->name);
            delete result;
            return nullptr;
          }
          auto * property_expr = new OPropertyExpr(memberbase, property);
          result = property_expr;
          if (!property->IsIndexed())
          {
            CheckPropertyReadable(property_expr);
          }
          continue;
        }
        if (auto * method = dynamic_cast<OValSymFunc *>(objsym))
        {
          if (!ObjectMemberAccessAllowed(decl_type, method))
          {
            Error(DQERR_MEMBER_UNKNOWN, membername, ctype->name);
            delete result;
            return nullptr;
          }
          if (!scf->CheckSymbol("("))
          {
            result = new OBoundMethodExpr(method, memberbase);
            continue;
          }

          OExpr * callexpr = ParseExprMethodCall(method, memberbase);
          result = callexpr;
          if (!result) return nullptr;
          continue;
        }
        if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(objsym))
        {
          if (!ObjectMemberAccessAllowed(decl_type, ovset))
          {
            Error(DQERR_MEMBER_UNKNOWN, membername, ctype->name);
            delete result;
            return nullptr;
          }
          if (!scf->CheckSymbol("("))
          {
            result = new OBoundMethodOverloadExpr(ovset, memberbase);
            continue;
          }

          OExpr * callexpr = ParseExprMethodOverloadCall(ovset, memberbase);
          result = callexpr;
          if (!result) return nullptr;
          continue;
        }

        decl_type = ctype;
        int midx = ctype->FindFieldIndex(membername, &decl_type);
        if (midx < 0)
        {
          Error(DQERR_MEMBER_UNKNOWN, membername, ctype->name);
          return result;
        }
        OType * mtype = decl_type->member_order[midx]->ptype;
        if (!ObjectMemberAccessAllowed(decl_type, decl_type->member_order[midx]))
        {
          Error(DQERR_MEMBER_UNKNOWN, membername, ctype->name);
          delete result;
          return nullptr;
        }
        result = new OLValueMember(memberbase, decl_type, midx, mtype);
        continue;
      }

      // Array/slice/dynamic-array/cstring/string index on any lvalue: x[i], or slice x[a:b]
      if ((TK_ARRAY == tk or TK_ARRAY_SLICE == tk or TK_DYN_ARRAY == tk or TK_CSTRING == tk
           or TK_DYNSTR == tk or TK_STRVIEW == tk)
          and scf->CheckSymbol("["))
      {
        if (auto * property = dynamic_cast<OPropertyExpr *>(lval))
        {
          Error(DQERR_PROPERTY_NOT_ADDRESSABLE, property->property->name);
          return result;
        }
        OExpr * indexexpr = nullptr;
        OExpr * endexpr = nullptr;
        bool has_first_expr = false;
        bool inclusive_slice = false;
        int64_t prev_context_len = array_index_context_len;
        OLValueExpr * prev_context_lval = array_index_context_lval;
        if (TK_ARRAY == tk)
        {
          array_index_context_len = static_cast<OTypeArray *>(lval->ptype->ResolveAlias())->arraylength;
          array_index_context_lval = nullptr;
        }
        else
        {
          array_index_context_len = -1;
          array_index_context_lval = lval;
        }
        scf->SkipWhite();
        if (!scf->CheckSymbol(":", false))
        {
          indexexpr = ParseExpression();
          has_first_expr = true;
        }
        scf->SkipWhite();
        if (scf->CheckSymbol(":"))
        {
          inclusive_slice = scf->CheckSymbol(":");
          if (TK_CSTRING == tk)
          {
            Error(DQERR_NOT_SUPPORTED, "cstring slicing");
            OExpr::DeleteTree(indexexpr);
            array_index_context_len = prev_context_len;
            array_index_context_lval = prev_context_lval;
            return result;
          }
          scf->SkipWhite();
          if (!scf->CheckSymbol("]", false))
          {
            endexpr = ParseExpression();
          }
          scf->SkipWhite();
          if (not scf->CheckSymbol("]"))
          {
            Error(DQERR_MISSING_CLOSE_BRACKET_AFTER, "slice");
          }
          array_index_context_len = prev_context_len;
          array_index_context_lval = prev_context_lval;
          if (TK_DYNSTR == tk || TK_STRVIEW == tk)
          {
            result = new OStringSliceExpr(lval, indexexpr, endexpr, inclusive_slice);
          }
          else
          {
            result = new OArraySliceExpr(lval, lval->ptype, indexexpr, endexpr, inclusive_slice);
          }
          continue;
        }
        if (!has_first_expr)
        {
          Error(DQERR_EXPR_EXPECTED);
          array_index_context_len = prev_context_len;
          array_index_context_lval = prev_context_lval;
          return result;
        }
        if (not scf->CheckSymbol("]"))
        {
          Error(DQERR_MISSING_CLOSE_BRACKET_AFTER, "index");
        }
        array_index_context_len = prev_context_len;
        array_index_context_lval = prev_context_lval;
        result = new OLValueIndex(lval, lval->ptype, indexexpr);
        continue;
      }

      // Function call: f(args)
      OLValueVar * varref = dynamic_cast<OLValueVar *>(lval);
      if (varref)
      {
        if (dynamic_cast<OValSymOverloadSet *>(varref->pvalsym) && scf->CheckSymbol("("))
        {
          OExpr * callexpr = ParseExprOverloadCall(static_cast<OValSymOverloadSet *>(varref->pvalsym));
          delete result;
          result = callexpr;
          if (!result) return nullptr;
          continue;
        }

        OValSymFunc * vsfunc = dynamic_cast<OValSymFunc *>(varref->pvalsym);
        if (vsfunc && scf->CheckSymbol("("))
        {
          OExpr * callexpr = ParseExprFuncCall(vsfunc);
          delete result;
          result = callexpr;
          if (!result) return nullptr;
          continue;
        }
      }
    }

    if (scf->CheckSymbol("(", false))
    {
      if (TK_FUNCREF == tk)
      {
        scf->CheckSymbol("(");
        OTypeFuncRef * calltype = static_cast<OTypeFuncRef *>(result->ResolvedType());
        OExpr * callexpr = ParseExprIndirectCall(result, calltype);
        result = callexpr;
        if (!result) return nullptr;
        continue;
      }

      Error(DQERR_EXPR_NOT_CALLABLE, result->ptype->name);
      delete result;
      return nullptr;
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

bool ODqCompParserExpr::ParsePropertyIndices(OPropertyExpr * property_expr)
{
  OValSymProperty * property = property_expr ? property_expr->property : nullptr;
  if (!property)
  {
    return false;
  }

  size_t index_no = 0;
  while (!scf->Eof())
  {
    OExpr * index_expr = ParseExpression();
    if (!index_expr)
    {
      return false;
    }
    if (index_no >= property->indices.size())
    {
      Error(DQERR_FUNC_ARGS_TOO_MANY, property->name, to_string(property->indices.size()));
      OExpr::DeleteTree(index_expr);
      return false;
    }
    if (!CheckAssignType(property->indices[index_no].ptype, &index_expr, "Property index"))
    {
      OExpr::DeleteTree(index_expr);
      return false;
    }
    property_expr->indices.push_back(index_expr);
    ++index_no;

    scf->SkipWhite();
    if (scf->CheckSymbol("]"))
    {
      break;
    }
    if (!scf->CheckSymbol(","))
    {
      Error(DQERR_MISSING_CLOSE_BRACKET_AFTER, "property index");
      return false;
    }
  }

  if (index_no < property->indices.size())
  {
    Error(DQERR_FUNC_ARGS_TOO_FEW, to_string(index_no), property->name,
          to_string(property->indices.size()));
    return false;
  }
  return true;
}

void ODqCompParserExpr::CheckPropertyReadable(OPropertyExpr * property_expr)
{
  if (property_expr && property_expr->property && !property_expr->property->read_accessor
      && !supress_varinit_check)
  {
    Error(DQERR_PROPERTY_WRITE_ONLY, property_expr->property->name);
  }
}

OExpr * ODqCompParserExpr::ParseExprPrimary()
{
  OExpr * result = nullptr;

  scf->SkipWhite();

  if (*scf->curp == '^'
      || *scf->curp == '['
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

  if (scf->CheckSymbol("$"))
  {
    string ctxname;
    if (!scf->ReadIdentifier(ctxname))
    {
      Error(DQERR_ID_EXP_AFTER, "$");
      return nullptr;
    }
    if (array_index_context_len < 0 && !array_index_context_lval)
    {
      Error(DQERR_VS_UNKNOWN, "$" + ctxname);
      return nullptr;
    }
    if (array_index_context_lval)
    {
      OLValueExpr * ctx_lval = CloneContextLValue(array_index_context_lval);
      if (!ctx_lval)
      {
        Error(DQERR_NOT_SUPPORTED, "$" + ctxname + " for this array expression");
        return nullptr;
      }
      OType * ctx_type = ctx_lval->ptype ? ctx_lval->ptype->ResolveAlias() : nullptr;
      OExpr * lenexpr = (ctx_type && (TK_DYNSTR == ctx_type->kind || TK_STRVIEW == ctx_type->kind))
          ? static_cast<OExpr *>(new OStringMetaFieldExpr(ctx_lval, SMF_LENGTH))
          : static_cast<OExpr *>(new OArrayMetaFieldExpr(ctx_lval, ctx_lval->ptype, AMF_LENGTH));
      if ("end" == ctxname)
      {
        return lenexpr;
      }
      if ("last" == ctxname)
      {
        return CreateBinExpr(BINOP_SUB, lenexpr, new OIntLit(1));
      }
      delete lenexpr;
      Error(DQERR_VS_UNKNOWN, "$" + ctxname);
      return nullptr;
    }
    if ("end" == ctxname)
    {
      return new OIntLit(array_index_context_len);
    }
    if ("last" == ctxname)
    {
      return new OIntLit(array_index_context_len - 1);
    }
    Error(DQERR_VS_UNKNOWN, "$" + ctxname);
    return nullptr;
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
    char quotech = *scf->curp;
    string strval;
    if (scf->ReadQuotedString(strval))
    {
      if (('\'' == quotech) && (strval.size() == 1))
      {
        return new OIntLit(uint8_t(strval[0]), g_builtins->type_char);
      }
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
      if (vs->IsRefLike() && (FPM_REFOUT == vs->param_mode))
      {
        if (supress_varinit_check)
        {
          AddSuppressedVarInitDiag(static_cast<OLValueVar *>(result), vs, scpos_ns);
        }
        else
        {
          Error(DQERR_REFOUT_READ_BEFORE_WRITE, vs->name, &scpos_ns);
        }
      }
      else
      {
        VarInitError(static_cast<OLValueVar *>(result), vs, scpos_ns);
      }
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
      ++scf->curp;
    }
    else
    {
      Error(DQERR_EXPR_EXPECTED);
    }
    return result;
  }

  // builtin specials

  if ("Len" == sid)
  {
    return ParseBuiltinLen();
  }

  if ("iif" == sid || "Iif" == sid)
  {
    return ParseBuiltinIif();
  }

  if ("SizeOf" == sid)
  {
    return ParseBuiltinSizeof();
  }

  if ("OffsetOf" == sid)
  {
    return ParseBuiltinOffsetof();
  }

  if ("Round" == sid)  return ParseBuiltinFloatRound(RNDMODE_ROUND);
  if ("Ceil" == sid)  return ParseBuiltinFloatRound(RNDMODE_CEIL);
  if ("Floor" == sid)  return ParseBuiltinFloatRound(RNDMODE_FLOOR);

  if ("TryCast" == sid)  return ParseBuiltinTryCast();
  if ("TypeName" == sid)  return ParseBuiltinTypeName();
  if ("Ord" == sid)  return ParseBuiltinOrd();

  if ("new" == sid)
  {
    return ParseNewExpr();
  }

  if ("inherited" == sid)
  {
    return ParseInheritedExpr();
  }

  OScope * found_scope = nullptr;
  OValSym * vs = curscope->FindValSym(sid, &found_scope);
  if (!vs)
  {
    OType * named_type = cur_mod_scope->FindType(sid);
    auto * enum_type = dynamic_cast<OTypeEnum *>(named_type ? named_type->ResolveAlias() : nullptr);
    scf->SkipWhite();
    if (enum_type && scf->CheckSymbol(".", false))
    {
      return ParseEnumTypeExpr(enum_type);
    }
    if (IsKnownEnumItem(sid))
    {
      return new OUnresolvedEnumItemExpr(sid);
    }
    bool object_method_scope = (curvsfunc && curvsfunc->owner_compound_type && curvsfunc->receiver_arg);
    if (object_method_scope)
    {
      OCompoundType * decl_type = nullptr;
      OCompoundType * owner_type = curvsfunc->owner_compound_type;
      OValSym * member = owner_type->FindMemberSymbol(sid, &decl_type);
      if (member && (VSK_FUNCTION != member->kind))
      {
        if (!ObjectMemberAccessAllowed(decl_type, member))
        {
          Error(DQERR_MEMBER_UNKNOWN, sid, owner_type->name);
          return result;
        }
        int midx = decl_type->FindMemberIndex(sid);
        if (midx >= 0)
        {
          return new OLValueMember(new OLValueVar(curvsfunc->receiver_arg), decl_type, midx, member->ptype);
        }
      }

      auto nsit = g_namespaces.find(".");
      OScope * root_scope = (g_namespaces.end() != nsit ? nsit->second : nullptr);
      if (root_scope && root_scope->FindValSym(sid, nullptr, true))
      {
        Error(DQERR_OBJ_VS_IN_MODULE_NS, sid);

        //ErrorTxt(DQERR_VS_UNKNOWN,
        //         format("Unknown symbol \"{}\" in object method scope; module-root symbol exists, use \"@.{}\" to access it",
        //                sid, sid));
        return result;
      }
    }
    Error(DQERR_VS_UNKNOWN, sid);
    return result;
  }

  result = CreateImplicitObjectMemberExpr(sid, vs, found_scope);
  if (!result)
  {
    result = new OLValueVar(vs);
  }
  if (auto * property_expr = dynamic_cast<OPropertyExpr *>(result);
      property_expr && !property_expr->property->IsIndexed())
  {
    CheckPropertyReadable(property_expr);
  }
  if (vs->kind != VSK_FUNCTION and not vs->initialized)
  {
    if (vs->IsRefLike() && (FPM_REFOUT == vs->param_mode))
    {
      if (supress_varinit_check)
      {
        AddSuppressedVarInitDiag(static_cast<OLValueVar *>(result), vs, scpos_sid);
      }
      else
      {
        Error(DQERR_REFOUT_READ_BEFORE_WRITE, vs->name, &scpos_sid);
      }
    }
    else
    {
      VarInitError(static_cast<OLValueVar *>(result), vs, scpos_sid);
    }
  }

  return result;
}

bool ODqCompParserExpr::IsKnownEnumItem(const string & item_name)
{
  for (OScope * scope = curscope; scope; scope = scope->parent_scope)
  {
    for (const auto & [name, type] : scope->typesyms)
    {
      (void)name;
      auto * enum_type = dynamic_cast<OTypeEnum *>(type ? type->ResolveAlias() : nullptr);
      if (enum_type && enum_type->FindItem(item_name))
      {
        return true;
      }
    }
  }
  return false;
}

OExpr * ODqCompParserExpr::ParseEnumTypeExpr(OTypeEnum * enum_type)
{
  scf->CheckSymbol(".");
  string member_name;
  if (!scf->ReadIdentifier(member_name))
  {
    Error(DQERR_MEMBER_NAME_EXPECTED);
    return nullptr;
  }
  if (const SEnumItem * item = enum_type->FindItem(member_name))
  {
    return new OEnumValueExpr(enum_type, item->value);
  }

  EEnumFromOrdKind kind;
  if ("FromOrd" == member_name) kind = EFOK_THROW;
  else if ("TryFromOrd" == member_name) kind = EFOK_TRY;
  else
  {
    Error(DQERR_MEMBER_UNKNOWN, member_name, enum_type->name);
    return nullptr;
  }

  if (!scf->CheckSymbol("("))
  {
    Error(DQERR_FUNC_CALL_PARENTH, member_name);
    return nullptr;
  }
  vector<TRawCallArg> rawargs;
  if (!ParseRawCallArguments(member_name, rawargs))
  {
    return nullptr;
  }

  size_t expected_min = (EFOK_TRY == kind ? 2 : 1);
  size_t expected_max = (EFOK_TRY == kind ? 2 : 2);
  if (rawargs.size() < expected_min)
  {
    Error(DQERR_FUNC_ARGS_TOO_FEW, to_string(rawargs.size()), member_name, to_string(expected_min));
    FreeRawCallArguments(rawargs);
    return nullptr;
  }
  if (rawargs.size() > expected_max)
  {
    Error(DQERR_FUNC_ARGS_TOO_MANY, member_name, to_string(expected_max));
    FreeRawCallArguments(rawargs);
    return nullptr;
  }

  OExpr * value_expr = rawargs[0].expr;
  rawargs[0].expr = nullptr;
  if (!value_expr->ResolvedType() || TK_INT != value_expr->ResolvedType()->kind)
  {
    Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", "int", value_expr->ptype ? value_expr->ptype->name : "?");
    OExpr::DeleteTree(value_expr);
    FreeRawCallArguments(rawargs);
    return nullptr;
  }

  if (EFOK_THROW == kind && rawargs.size() == 2)
  {
    kind = EFOK_DEFAULT;
  }
  auto * result = new OEnumFromOrdExpr(kind, enum_type, value_expr);

  if (EFOK_DEFAULT == kind)
  {
    OExpr * default_expr = rawargs[1].expr;
    rawargs[1].expr = nullptr;
    if (!ConvertExprToType(enum_type, &default_expr, EXPCF_GENERATE_ERRORS))
    {
      OExpr::DeleteTree(default_expr);
      delete result;
      FreeRawCallArguments(rawargs);
      return nullptr;
    }
    result->default_expr = default_expr;
  }
  else if (EFOK_TRY == kind)
  {
    if (!rawargs[1].init_diags.empty())
    {
      EmitStoredVarInitDiags(rawargs[1].init_diags);
      delete result;
      FreeRawCallArguments(rawargs);
      return nullptr;
    }
    OExpr * output = rawargs[1].expr;
    rawargs[1].expr = nullptr;
    auto * output_lval = dynamic_cast<OLValueExpr *>(output);
    if (!output_lval || output->ResolvedType() != enum_type)
    {
      ErrorTxt(DQERR_FUNC_ARG_REF_TYPE,
          format("Reference argument 2 type mismatch for function \"TryFromOrd\": expected \"{}\"", enum_type->name));
      OExpr::DeleteTree(output);
      delete result;
      FreeRawCallArguments(rawargs);
      return nullptr;
    }
    result->output_expr = output_lval;
  }
  else if (!EnsureEnumRtlUse())
  {
    delete result;
    FreeRawCallArguments(rawargs);
    return nullptr;
  }

  FreeRawCallArguments(rawargs);
  return result;
}

OValSym * ODqCompParserExpr::ResolveNamespaceValSym()
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

OExpr * ODqCompParserExpr::ParseArrayLit()
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
    else
    {
      for (OExpr * e : elems) delete e;
      return nullptr;
    }
  }

  return new OArrayLit(elems);
}

bool ODqCompParserExpr::ParseCallArguments(const string & callname, OTypeFunc * tfunc, vector<OExpr *> & rargs)
{
  vector<TRawCallArg> rawargs;
  if (!ParseRawCallArguments(callname, rawargs))
  {
    return false;
  }

  bool result = BindCallArguments(callname, tfunc, rawargs, rargs);
  FreeRawCallArguments(rawargs);
  return result;
}

bool ODqCompParserExpr::ParseRawCallArguments(const string & callname, vector<TRawCallArg> & rargs)
{
  // "(" was already consumed

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
      Error(DQERR_FUNC_ARGS_LIST, "\",\" or \")\" is missing at function \"$1\"call arguments", callname);
      FreeRawCallArguments(rargs);
      return false;
    }

    TRawCallArg rawarg;
    scf->SaveCurPos(rawarg.scpos_start);

    size_t suppressed_start = suppressed_varinit_diags.size();
    bool saved_suppress = supress_varinit_check;
    supress_varinit_check = true;
    rawarg.expr = ParseExpression();
    supress_varinit_check = saved_suppress;

    rawarg.init_diags.assign(suppressed_varinit_diags.begin() + suppressed_start, suppressed_varinit_diags.end());
    suppressed_varinit_diags.resize(suppressed_start);

    if (!rawarg.expr)
    {
      FreeRawCallArguments(rargs);
      return false;
    }

    rargs.push_back(rawarg);
    ++pcnt;
  }

  return true;
}

OExpr * ODqCompParserExpr::ParseExprFuncCall(OValSymFunc * vsfunc)
{
  if (vsfunc && vsfunc->owner_compound_type)
  {
    return ParseExprMethodCall(vsfunc, nullptr);
  }

  OCallExpr * result = new OCallExpr(vsfunc);
  if (!ParseCallArguments(vsfunc->name, static_cast<OTypeFunc *>(vsfunc->ptype), result->args))
  {
    delete result;
    return nullptr;
  }

  return result;
}

OExpr * ODqCompParserExpr::ParseExprMethodCall(OValSymFunc * vsfunc, OLValueExpr * receiver)
{
  if (!vsfunc || !vsfunc->owner_compound_type)
  {
    Error(DQERR_EXPR_NOT_CALLABLE, "method");
    OExpr::DeleteTree(receiver);
    return nullptr;
  }
  if ( vsfunc->attr_is_virtual && curvsfunc &&
       ((OSF_CREATE == curvsfunc->object_specfunc_kind) or (OSF_DESTROY == curvsfunc->object_specfunc_kind)) )
  {
    ErrorTxt(DQERR_VIRT_FUNC_CALL_INVALID, vsfunc->name, "constructors or destructors");
    OExpr::DeleteTree(receiver);
    return nullptr;
  }

  vector<TRawCallArg> rawargs;

  TRawCallArg thisarg;
  scf->SaveCurPos(thisarg.scpos_start);
  if (receiver)
  {
    thisarg.expr = receiver;
  }
  else if (curvsfunc && curvsfunc->owner_compound_type == vsfunc->owner_compound_type && curvsfunc->receiver_arg)
  {
    thisarg.expr = new OLValueVar(curvsfunc->receiver_arg);
  }
  else
  {
    Error(DQERR_EXPR_NOT_CALLABLE, vsfunc->name);
    return nullptr;
  }
  rawargs.push_back(thisarg);

  if (!ParseRawCallArguments(vsfunc->name, rawargs))
  {
    return nullptr;
  }

  OCallExpr * result = new OCallExpr(vsfunc);
  if (!BindCallArguments(vsfunc->name, static_cast<OTypeFunc *>(vsfunc->ptype), rawargs, result->args))
  {
    delete result;
    FreeRawCallArguments(rawargs);
    return nullptr;
  }

  FreeRawCallArguments(rawargs);
  return result;
}

OExpr * ODqCompParserExpr::ParseExprOverloadCall(OValSymOverloadSet * ovset)
{
  if (ovset && ovset->owner_compound_type)
  {
    return ParseExprMethodOverloadCall(ovset, nullptr);
  }

  vector<TRawCallArg> rawargs;
  return ParseExprOverloadCallWithRawArgs(ovset, rawargs);
}

OExpr * ODqCompParserExpr::ParseExprMethodOverloadCall(OValSymOverloadSet * ovset, OLValueExpr * receiver)
{
  if (!ovset || !ovset->owner_compound_type)
  {
    Error(DQERR_EXPR_NOT_CALLABLE, "method");
    OExpr::DeleteTree(receiver);
    return nullptr;
  }

  vector<TRawCallArg> rawargs;

  TRawCallArg thisarg;
  scf->SaveCurPos(thisarg.scpos_start);
  if (receiver)
  {
    thisarg.expr = receiver;
  }
  else if (curvsfunc && curvsfunc->owner_compound_type == ovset->owner_compound_type && curvsfunc->receiver_arg)
  {
    thisarg.expr = new OLValueVar(curvsfunc->receiver_arg);
  }
  else
  {
    Error(DQERR_EXPR_NOT_CALLABLE, ovset->name);
    return nullptr;
  }
  rawargs.push_back(thisarg);

  return ParseExprOverloadCallWithRawArgs(ovset, rawargs);
}

OExpr * ODqCompParserExpr::ParseExprOverloadCallWithRawArgs(OValSymOverloadSet * ovset, vector<TRawCallArg> & rawargs)
{
  if (!ovset)
  {
    Error(DQERR_EXPR_NOT_CALLABLE, "function");
    return nullptr;
  }

  if (!ParseRawCallArguments(ovset->name, rawargs))
  {
    return nullptr;
  }

  vector<TFuncCallArgMatch> callargs;
  callargs.reserve(rawargs.size());
  for (const TRawCallArg & rawarg : rawargs)
  {
    callargs.push_back({rawarg.expr, !rawarg.init_diags.empty()});
  }

  OValSymFunc * best_func = nullptr;
  TFuncCallMatchScore best_score;
  bool ambiguous = false;

  for (OValSymFunc * fn : ovset->funcs)
  {
    OTypeFunc * tfunc = dynamic_cast<OTypeFunc *>(fn ? fn->ptype : nullptr);
    TFuncCallMatchScore score;
    if (!tfunc || !tfunc->AnalyzeCallCandidate(callargs, score))
    {
      continue;
    }

    if (!best_func)
    {
      best_func = fn;
      best_score = score;
      ambiguous = false;
      continue;
    }

    int cmp = OTypeFunc::CompareCallCandidateScore(score, best_score);
    if (cmp < 0)
    {
      best_func = fn;
      best_score = score;
      ambiguous = false;
    }
    else if (0 == cmp)
    {
      ambiguous = true;
    }
  }

  if (!best_func || ambiguous)
  {
    FreeRawCallArguments(rawargs);
    if (ambiguous)
    {
      Error(DQERR_OVERLOAD_AMBIGUOUS, ovset->name);
    }
    else
    {
      Error(DQERR_OVERLOAD_NO_MATCH, ovset->name);
    }
    return nullptr;
  }

  OCallExpr * result = new OCallExpr(best_func);
  if (best_func->attr_is_virtual && curvsfunc &&
      ((OSF_CREATE == curvsfunc->object_specfunc_kind) or (OSF_DESTROY == curvsfunc->object_specfunc_kind)) )
  {
    delete result;
    FreeRawCallArguments(rawargs);
    ErrorTxt(DQERR_VIRT_FUNC_CALL_INVALID, best_func->name, "constructors or destructors");
    return nullptr;
  }
  if (!BindCallArguments(ovset->name, static_cast<OTypeFunc *>(best_func->ptype), rawargs, result->args))
  {
    delete result;
    FreeRawCallArguments(rawargs);
    return nullptr;
  }

  FreeRawCallArguments(rawargs);
  return result;
}

OExpr * ODqCompParserExpr::ParseExprIndirectCall(OExpr * callee, OTypeFuncRef * calltype)
{
  OIndirectCallExpr * result = new OIndirectCallExpr(callee, calltype);
  string callname = (calltype ? calltype->name : string("funcref"));
  if (!ParseCallArguments(callname, (calltype ? calltype->functype : nullptr), result->args))
  {
    delete result;
    return nullptr;
  }

  return result;
}

OExpr * ODqCompParserExpr::ParseNewExpr()
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
  if (!memalloc_func)
  {
    Error(DQERR_VS_UNKNOWN, "MemAlloc");
    return nullptr;
  }

  OType * alloc_type = ParseTypeSpec();
  if (!alloc_type)
  {
    return nullptr;
  }
  alloc_type = alloc_type->ResolveAlias();
  alloc_type->EnsureLayout();

  if (TK_VOID == alloc_type->kind)
  {
    Error(DQERR_TYPE_EXPECTED, "non-void", alloc_type->name);
    return nullptr;
  }
  if (auto * object_type = dynamic_cast<OTypeObject *>(alloc_type))
  {
    if (object_type->is_abstract)
    {
      ErrorTxt(DQERR_NOT_SUPPORTED, format("constructing abstract object \"{}\"", object_type->name));
      return nullptr;
    }
    vector<OExpr *> ctor_args;
    scf->SkipWhite();
    bool has_parens = scf->CheckSymbol("(");
    if (has_parens)
    {
      vector<TRawCallArg> rawargs;
      if (!ParseRawCallArguments(alloc_type->name, rawargs))
      {
        return nullptr;
      }
      for (TRawCallArg & rawarg : rawargs)
      {
        ctor_args.push_back(rawarg.expr);
        rawarg.expr = nullptr;
      }
      FreeRawCallArguments(rawargs);
    }

    OValSymFunc * ctor = nullptr;
    if (has_parens && !CheckObjectCtorArgs(object_type, ctor_args, ctor))
    {
      for (OExpr * arg : ctor_args) OExpr::DeleteTree(arg);
      return nullptr;
    }

    ONewExpr * result = new ONewExpr(alloc_type, nullptr, memalloc_func);
    result->ctor_func = ctor;
    result->ctor_args = ctor_args;
    return result;
  }
  if (0 == alloc_type->bytesize)
  {
    Error(DQERR_NOT_SUPPORTED, "new for dynamically sized type");
    return nullptr;
  }

  OExpr * initexpr = nullptr;
  scf->SkipWhite();
  if (scf->CheckSymbol("("))
  {
    scf->SkipWhite();
    if (!scf->CheckSymbol(")"))
    {
      initexpr = ParseExpression();
      if (!initexpr)
      {
        return nullptr;
      }

      scf->SkipWhite();
      if (scf->CheckSymbol(","))
      {
        delete initexpr;
        Error(DQERR_FUNC_ARGS_TOO_MANY, "new", "1");
        scf->ReadTo(")");
        scf->CheckSymbol(")");
        return nullptr;
      }

      if (!scf->CheckSymbol(")"))
      {
        delete initexpr;
        Error(DQERR_MISSING_CLOSE_PAREN_FOR, "new");
        return nullptr;
      }

      if (!CheckAssignType(alloc_type, &initexpr, "new initializer"))
      {
        delete initexpr;
        return nullptr;
      }
    }
  }

  return new ONewExpr(alloc_type, initexpr, memalloc_func);
}

OExpr * ODqCompParserExpr::ParseInheritedExpr()
{
  auto * owner_object = dynamic_cast<OTypeObject *>(curvsfunc ? curvsfunc->owner_compound_type : nullptr);
  if (!owner_object || !owner_object->base_type)
  {
    ErrorTxt(DQERR_INHERITED_CALL_INVALID, "inherited is only valid inside a derived object method");
    return nullptr;
  }
  if ((OSF_CREATE == curvsfunc->object_specfunc_kind) or (OSF_DESTROY == curvsfunc->object_specfunc_kind))
  {
    ErrorTxt(DQERR_INHERITED_CALL_INVALID, "inherited expressions are not valid in lifecycle methods");
    return nullptr;
  }

  string method_name;
  scf->SkipWhite();
  if (!scf->ReadIdentifier(method_name))
  {
    method_name = curvsfunc->name;
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol("("))
  {
    Error(DQERR_FUNC_CALL_PARENTH, method_name);
    return nullptr;
  }

  vector<TRawCallArg> rawargs;
  TRawCallArg thisarg;
  scf->SaveCurPos(thisarg.scpos_start);
  thisarg.expr = new OLValueVar(curvsfunc->receiver_arg);
  rawargs.push_back(thisarg);
  if (!ParseRawCallArguments(method_name, rawargs))
  {
    return nullptr;
  }

  vector<OExpr *> user_args;
  for (size_t i = 1; i < rawargs.size(); ++i)
  {
    user_args.push_back(rawargs[i].expr);
  }
  OValSymFunc * method = FindInheritedMethod(method_name, user_args);
  user_args.clear();
  if (!method)
  {
    FreeRawCallArguments(rawargs);
    Error(DQERR_OVERLOAD_NO_MATCH, method_name);
    return nullptr;
  }

  OCallExpr * result = new OCallExpr(method);
  result->force_direct = true;
  if (!BindCallArguments(method_name, static_cast<OTypeFunc *>(method->ptype), rawargs, result->args))
  {
    delete result;
    FreeRawCallArguments(rawargs);
    return nullptr;
  }

  FreeRawCallArguments(rawargs);
  return result;
}

OExpr * ODqCompParserExpr::ParseBuiltinIif()
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

  return new OIifExpr(condexpr, trueexpr, falseexpr, resulttype);
}

OExpr * ODqCompParserExpr::ParseBuiltinFloatRound(ERoundMode amode)
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

OExpr * ODqCompParserExpr::ParseBuiltinLen()
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
  else if (TK_DYN_ARRAY == lenvs->ptype->kind)
  {
    return new ODynArrayLengthExpr(lenvs);
  }
  else if (TK_CSTRING == lenvs->ptype->kind)
  {
    return new OCStringLenExpr(lenvs);
  }
  else if (TK_DYNSTR == lenvs->ptype->ResolveAlias()->kind || TK_STRVIEW == lenvs->ptype->ResolveAlias()->kind)
  {
    return new OStringMetaFieldExpr(new OLValueVar(lenvs), SMF_LENGTH);
  }
  else
  {
    Error(DQERR_LEN_INVALID_TYPE, lenvs->ptype->name);
    return nullptr;
  }
}

OExpr * ODqCompParserExpr::ParseBuiltinTryCast()
{
  scf->SkipWhite();
  if (not scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, "TryCast");
    return nullptr;
  }
  scf->SkipWhite();

  OType * casttype = ParseTypeSpec();
  if (!casttype)
  {
    return nullptr;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(","))
  {
    Error(DQERR_MISSING_COMMA, &scf->prevpos);
    return nullptr;
  }

  OExpr * source_expr = ParseExpression();
  if (!source_expr)
  {
    return nullptr;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_FOR, "TryCast");
    OExpr::DeleteTree(source_expr);
    return nullptr;
  }

  return new OTryCastExpr(casttype, source_expr);
}

OExpr * ODqCompParserExpr::ParseBuiltinTypeName()
{
  scf->SkipWhite();
  if (not scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, "TypeName");
    return nullptr;
  }
  scf->SkipWhite();

  OType * argtype = nullptr;
  OScPosition argpos;
  scf->SaveCurPos(argpos);

  if (scf->CheckSymbol("^", false) || scf->CheckSymbol("[", false))
  {
    argtype = ParseTypeSpec();
  }
  else
  {
    string sarg;
    if (scf->ReadIdentifier(sarg))
    {
      OValSym * vs = curscope->FindValSym(sarg);
      if (vs)
      {
        scf->SetCurPos(argpos); // Variable found, parse as expression
      }
      else
      {
        OType * foundtype = cur_mod_scope->FindType(sarg);
        if (foundtype)
        {
          scf->SetCurPos(argpos);
          argtype = ParseTypeSpec(); // It's a type name, parse it!
        }
        else
        {
          scf->SetCurPos(argpos); // Rewind and parse as expression
        }
      }
    }
    else
    {
      scf->SetCurPos(argpos); // Rewind and parse as expression
    }
  }

  OExpr * result_expr = nullptr;
  if (argtype)
  {
    result_expr = new OCStringLit(argtype->name);
  }
  else
  {
    OExpr * argexpr = ParseExpression();
    if (!argexpr) return nullptr;
    result_expr = new OTypeNameExpr(argexpr);
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_FOR, "TypeName");
    OExpr::DeleteTree(result_expr);
    return nullptr;
  }
  return result_expr;
}

OExpr * ODqCompParserExpr::ParseBuiltinOrd()
{
  scf->SkipWhite();
  if (!scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, "Ord");
    return nullptr;
  }
  OExpr * arg = ParseExpression();
  if (!arg)
  {
    return nullptr;
  }
  scf->SkipWhite();
  if (!scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_FOR, "Ord");
    OExpr::DeleteTree(arg);
    return nullptr;
  }
  if (!arg->ResolvedType() || TK_ENUM != arg->ResolvedType()->kind)
  {
    Error(DQERR_TYPE_EXPECTED, "enum", arg->ptype ? arg->ptype->name : "?");
    OExpr::DeleteTree(arg);
    return nullptr;
  }
  return new OEnumOrdExpr(arg);
}


OExpr * ODqCompParserExpr::ParseBuiltinSizeof()
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

  if (scf->CheckSymbol("^", false) || scf->CheckSymbol("[", false))
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
  sizetype->EnsureLayout();

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

OExpr * ODqCompParserExpr::ParseBuiltinOffsetof()
{
  scf->SkipWhite();
  if (not scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, "offsetof");
    return nullptr;
  }

  OType * ptype = ParseTypeSpec();
  if (!ptype)
  {
    return nullptr;
  }
  ptype = ptype->ResolveAlias();

  OCompoundType * ctype = dynamic_cast<OCompoundType *>(ptype);
  if (!ctype)
  {
    Error(DQERR_TYPE_EXPECTED, "compound", ptype->name);
    return nullptr;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(","))
  {
    Error(DQERR_MISSING_COMMA);
    return nullptr;
  }

  scf->SkipWhite();
  string membername;
  if (not scf->ReadIdentifier(membername))
  {
    Error(DQERR_MEMBER_NAME_EXPECTED);
    return nullptr;
  }

  ctype->EnsureLayout();
  int midx = ctype->FindMemberIndex(membername);
  if (midx < 0)
  {
    Error(DQERR_MEMBER_UNKNOWN, membername, ctype->name);
    return nullptr;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_FOR, "offsetof");
    return nullptr;
  }

  return new OIntLit(ctype->member_order[midx]->field_offset);
}

bool ODqCompParserExpr::CheckStatementClose(bool emit_error)
{
  int line_before = scf->last_token_end_line;
  scf->SkipWhite();

  if (scf->CheckSymbol(";"))
  {
    return true;
  }

  if (scf->curline > line_before)
  {
    return true;
  }

  if (scf->Eof() || scf->CheckSymbol("}", false))
  {
    return true;
  }

  if (emit_error)
  {
    StatementError(DQERR_MISSING_SEMICOLON_TO_CLOSE, "previous statement");
  }
  return false;
}

OExpr * ODqCompParserExpr::ParseBinOpLevel(OExpr * (ODqCompParserExpr::*parse_next)(), const BinOpEntry ops[], int nops)
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
