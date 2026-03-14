/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    errorcodes.h
 * authors: nvitya
 * created: 2026-03-13
 * brief:   DQ Compiler Error/Warning/Hint codes
 */

#pragma once

// Same structure with different names so the call Error() with warning symbol produces an error

struct TDiagDefErr
{
  const char *  strid;
  const char *  text;
};

#define DEF_DQ_ERR(asym, astrid, atxt) \
  inline constexpr TDiagDefErr asym { astrid, atxt }

struct TDiagDefWarn
{
  const char *  strid;
  const char *  text;
};

#define DEF_DQ_WARN(asym, astrid, atxt) \
  inline constexpr TDiagDefWarn asym { astrid, atxt }

struct TDiagDefHint
{
  const char *  strid;
  const char *  text;
};

#define DEF_DQ_HINT(asym, astrid, atxt) \
  inline constexpr TDiagDefWarn asym { astrid, atxt }

//-----------------------------------------------------------------------------
// ERRORS
//-----------------------------------------------------------------------------

DEF_DQ_ERR(DQERR_ID_EXP_AFTER,                     "EIdExpected",             "Identifier is expected after \"$1\"");
DEF_DQ_ERR(DQERR_VARNAME_EXP_AFTER,                "EVarNameExpected",        "Variable name is expected after \"$1\"");
DEF_DQ_ERR(DQERR_KW_OR_ID_MISSING,                 "EKeywordOrIdMissing",     "Keyword or identifier is missing");
DEF_DQ_ERR(DQERR_SYM_EXPECTED,                     "ESymExpected",            "\"$1\" is expected");
DEF_DQ_ERR(DQERR_SYM_EXPECTED_AFTER,               "ESymExpected",            "\"$1\" is expected after $2");
DEF_DQ_ERR(DQERR_MISSING_SEMICOLON,                "EMissingSemicolon",       "\";\" is missing");
DEF_DQ_ERR(DQERR_MISSING_SEMICOLON_TO_CLOSE,       "EMissingSemicolon",       "\";\" is missing to close the $1");
DEF_DQ_ERR(DQERR_MISSING_SEMICOLON_AFTER,          "EMissingSemicolon",       "\";\" is missing after $1");
DEF_DQ_ERR(DQERR_MISSING_COMMA,                    "EMissingComma",           "\",\" is missing");
DEF_DQ_ERR(DQERR_MISSING_ASSIGN_FOR,               "EMissingAssign",          "Missing assignment \"=\" for \"$1\"");
DEF_DQ_ERR(DQERR_MISSING_PARENTH,                  "EMissingParenth",         "Missing parenthesis \"$1\"");
DEF_DQ_ERR(DQERR_MISSING_OPEN_PARENTH,             "EMissingOpenParenth",     "Missing parenthesis \"(\"");
DEF_DQ_ERR(DQERR_MISSING_OPEN_PARENTH_FOR,         "EMissingOpenParenth",     "Missing parenthesis \"(\" for $1");
DEF_DQ_ERR(DQERR_MISSING_OPEN_PARENTH_AFTER,       "EMissingOpenParenth",     "Missing parenthesis \"(\" after \"$1\"");
DEF_DQ_ERR(DQERR_MISSING_CLOSE_PARENTH,            "EMissingCloseParenth",    "Missing parenthesis \")\"");
DEF_DQ_ERR(DQERR_MISSING_CLOSE_PARENTH_FOR,        "EMissingCloseParenth",    "Missing parenthesis \")\" for $1");
DEF_DQ_ERR(DQERR_MISSING_PARENTH_AFTER,            "EMissingParenth",         "Missing parenthesis \"$1\" after \"$1\"");

DEF_DQ_ERR(DQERR_LIT_HEXNUM,                       "ELitHex",                 "Hexadecimal literal error");
DEF_DQ_ERR(DQERR_LIT_STRING,                       "ELitStr",                 "String literal error");
DEF_DQ_ERR(DQERR_LIT_FLOAT,                        "ELitFloat",               "Floating point literal error");
DEF_DQ_ERR(DQERR_LIT_INT,                          "ELitInt",                 "Integer literal error");

DEF_DQ_ERR(DQERR_SIZE_SPEC,                        "ESizeSpec",               "$1 size must be a positive integer");
DEF_DQ_ERR(DQERR_ARRAY_SIZESPEC,                   "EArraySizeSpec",          "Array size (or \"]\" is expected");
DEF_DQ_ERR(DQERR_ARRAY_CONSTEXPR,                  "EArrayConstExpr",         "Array constant expression error");

DEF_DQ_ERR(DQERR_NOT_IMPLEMENTED_YET,              "ENotImplementedYet",      "$1 is not implemented yet");
DEF_DQ_ERR(DQERR_NOT_SUPPORTED,                    "ENotSupported",           "$1 is not supported");

DEF_DQ_ERR(DQERR_OP_INVALID_FOR,                   "EOpInvalid",              "Operation \"$1\" is invalid for $2");
DEF_DQ_ERR(DQERR_OP_UNHANDLED_FOR,                 "EOpUnhandled",            "Unhandled operation \"$1\" for $2");

DEF_DQ_ERR(DQERR_STMTBLK_START_MISSING,            "EStmtBlockStartMissing",  "\":\" is missing for statement block start");
DEF_DQ_ERR(DQERR_STMTBLK_CLOSE_MISSING,            "EStmtBlockCloseMissing",  "Statement block closer \"$1\" is missing");
DEF_DQ_ERR(DQERR_STMT_UNKNOWN,                     "EStmtUnknown",            "Unknown statement or function \"$1\"");

DEF_DQ_ERR(DQERR_EXPR_WRONG_VALUE_FOR,             "EExprWrongValue",         "Wrong value expression for \"$1\"");
DEF_DQ_ERR(DQERR_EXPR_INITVALUE,                   "EExprInitValue",          "Initial value expression error");
DEF_DQ_ERR(DQERR_EXPR_INITVALUE_FOR,               "EExprInitValue",          "Initial value expression error for \"$1\"");

DEF_DQ_ERR(DQERR_CONSTEXPR_ERROR,                  "EConstExprError",         "$1 constant expressions error");
DEF_DQ_ERR(DQERR_CONSTEXPR_INVALID_FOR,            "EConstExprInvalid",       "Invalid constant expression for \"$1\"");
DEF_DQ_ERR(DQERR_CONSTEXPR_NONCONST_SYM,           "EConstExprNonConstVs",    "Non-constant symbol \"$1\" in $2 constant expression");

DEF_DQ_ERR(DQERR_MODULE_STATEMENT_EXPECTED,        "EModStatementExpected",   "Module statement keyword expected");
DEF_DQ_ERR(DQERR_MODULE_STATEMENT_UNKNOWN,         "EModStatementUnknown",    "Unknown module statement \"$1\"");

DEF_DQ_ERR(DQERR_ATTR_NAME_EXPECTED,               "EAttrNameMissing",        "Attribute name expected after \"[[\"");
DEF_DQ_ERR(DQERR_ATTR_UNKNOWN,                     "EAttrUnknown",            "Unknown attribute \"$1\"");

DEF_DQ_ERR(DQERR_TYPE_SPECIFIER_EXPECTED,          "ETypeSpecExpected",       "Type specifier \":\" is expected");
DEF_DQ_ERR(DQERR_TYPE_SPECIFIER_EXP_AFTER,         "ETypeSpecExpAfter",       "Type specifier \":\" is expected after \"$1\"");
DEF_DQ_ERR(DQERR_TYPE_UNKNOWN,                     "ETypeUnknown",            "Type \"$1\" is unknown");
DEF_DQ_ERR(DQERR_TYPE_ID_EXP,                      "ETypeIdExpected",         "Type identifier is expected");
DEF_DQ_ERR(DQERR_TYPE_ALREADY_DEFINED,             "ETypeAlreadyDef",         "Type \"$1\" is already defined");
DEF_DQ_ERR(DQERR_TYPE_ALREADY_DEFINED_IN,          "ETypeAlreadyDef",         "Type \"$1\" is already defined in scope \"$2\"");
DEF_DQ_ERR(DQERR_TYPEDEF_ASSIGN_FOR,               "ETypeDefAssign",          "Missing type definition assignment \"=\" for \"$1\"");

DEF_DQ_ERR(DQERR_TYPEMISM_FOR_OP,                  "ETypeMismatchOp",         "Type mismatch for operation: \"$1\" $2 \"$3\"");
DEF_DQ_ERR(DQERR_TYPEMISM_STMT_ASSIGN,             "ETypeMismatchAssign",     "$1 type mismatch: \"$1\" = \"$2\"");
DEF_DQ_ERR(DQERR_TYPE_EXPECTED,                    "ETypeExpected",           "\"$1\" type expected, got \"$2\"");
DEF_DQ_ERR(DQERR_TYPE_FLOAT_EXPECTED_FOR,          "ETypeFloatExpected",      "float type expected for \"$1\", got \"$2\"");

DEF_DQ_ERR(DQERR_VS_UNKNOWN,                       "EVsUnknown",              "Unknown symbol \"$1\"");
DEF_DQ_ERR(DQERR_VAR_UNKNOWN,                      "EVarUnknown",             "Unknown variable \"$1\"");
DEF_DQ_ERR(DQERR_VS_ALREADY_DECL_SCOPE,            "EVsAlreadyDeclScope",     "Symbol \"$1\" is already defined in scope \"$2\"");
DEF_DQ_ERR(DQERR_VS_ALREADY_DECL_TYPE,             "EVsAlreadyDeclType",      "Symbol \"$1\" is already declared with type \"$2\"");
DEF_DQ_ERR(DQERR_GLOBALVAR_INITVALUE,              "EGlobVarInitvalue",       "Invalid initialization value for the global variable \"$1\"");
DEF_DQ_ERR(DQERR_VAR_NOT_INITIALIZED,              "EVarNotInit",             "Accessing unitialized variable \"$1\"");

DEF_DQ_ERR(DQERR_STRUCT_MBID_EXPECTED,             "EStructMemberId",         "Member id or \"enstruct\" expected");

DEF_DQ_ERR(DQERR_ARR_ELEMCOUNT_MISM,               "EArrElemCount",           "Array element count mismatch: expected $1, got $2");
DEF_DQ_ERR(DQERR_CSTR_SIZE_EXPECTED,               "ECstrSizeExpected",       "cstring size expected, example: cstring[n]");
DEF_DQ_ERR(DQERR_CSTR_SIZE_INVALID,                "ECstrSizeInvalid",        "Invalid cstring size, it must be a positive integer");
DEF_DQ_ERR(DQERR_CSTR_CONSTEXPR,                   "ECstrConstExpr",          "CString constant expression error: string literal expected");

DEF_DQ_ERR(DQERR_VARARGS_NOT_ALLOWED,              "EVarargsNotAllowed",      "Variadic \"...\" is only allowed on [[external]] functions");
DEF_DQ_ERR(DQERR_VARARGS_ALONE,                    "EVarargsAlone",           "Variadic functions must have at least one named parameter before '...'");

DEF_DQ_ERR(DQERR_FUNCPAR_NAME_EXP,                 "EFuncParNameExpected",    "Function parameter name expected");
DEF_DQ_ERR(DQERR_FUNCPAR_NAME_INVALID,             "EFuncParNameInvalid",     "Invalid function parameter name \"$1\"");
DEF_DQ_ERR(DQERR_FUNC_RETTYPE_EXPECTED,            "EFuncRettypeExpected",    "Function return type identifier expected after \"->\"");
DEF_DQ_ERR(DQERR_FUNC_NO_BODY_ALLOWED_AFTER,       "EFuncNoBodyAllowed",      "\";\" is expected after $1");
DEF_DQ_ERR(DQERR_FUNC_RESULT_NOT_SET,              "EFuncResultNotSet",       "Function \"$1\" result is not set");
DEF_DQ_ERR(DQERR_FUNC_RESULT_SPECIFIED,            "EFuncResultSet",          "Function \"$1\" result is set for function returning no value");
DEF_DQ_ERR(DQERR_FUNC_CALL_PARENTH,                "EFuncCall",               "Function call \"$1\" missing parentheses: \"(\"");
DEF_DQ_ERR(DQERR_FUNC_ARGS_LIST,                   "EFuncArgList",            "Function \"$1\" argument list error");  // used with custom text
DEF_DQ_ERR(DQERR_FUNC_ARGS_TOO_MANY,               "EFuncArgsTooMany",        "Too many arguments are provided for the function \"$1\" call. Expected $2");
DEF_DQ_ERR(DQERR_FUNC_ARGS_TOO_FEW,                "EFuncArgsTooFew",         "Too few arguments ($1) are provided for the function \"$2\" call. Expected $3");

DEF_DQ_ERR(DQERR_CONDEXPR_MISSING_FOR,             "ECondExprMissing",        "Condition expression is mission for \"$1\"");
DEF_DQ_ERR(DQERR_MULTIPLE_ELSE,                    "EMultipleElse",           "Multiple else branches detected");

DEF_DQ_ERR(DQERR_EXPR_INVALID_ADDROF,              "EExprAddrofInvalid",      "Invalid expression for the address of \"&\" operator");
DEF_DQ_ERR(DQERR_EXPR_VS_NOT_ADDRESSABLE,          "EExprVsNotAddressable",   "\"$1\" is not a variable, cannot take its address");


DEF_DQ_ERR(DQERR_CDIR_KW_MISSING,                  "ECDirKwMissing",          "Compiler directive keyword is missing");
DEF_DQ_ERR(DQERR_CDIR_CLOSER_MISSING,              "ECDirClose",              "Compiler directive closer \"}\" is missing");
DEF_DQ_ERR(DQERR_CDIR_UNKNOWN,                     "ECDirUnknown",            "Unknown compiler directive \"$1\"");
DEF_DQ_ERR(DQERR_CDIR_DEF_ID_MISSING,              "ECDirDefIdMissing",       "#define error: identifier is missing");
DEF_DQ_ERR(DQERR_CDIR_COND_ID_MISSING,             "ECDirCondIdMissing",      "$1 error: identifier is missing");
DEF_DQ_ERR(DQERR_CDIR_ELCOND_WITHOUT_IF,           "ECDirElCondWithoutIf",    "$1 without previous #if...!");
DEF_DQ_ERR(DQERR_CDIR_ELCOND_AFTER_ELSE_PREVPOS,   "ECDirElCondAfterElse",    "$1 after previous #else at position $2");
DEF_DQ_ERR(DQERR_CDIR_ENDIF_WITHOUT_IF,            "ECDirEndifWithoutIf",     "#endif without previous #if...!");
DEF_DQ_ERR(DQERR_CDIR_ELSE_WITHOUT_IF,             "ECDirElseWithoutIf",      "#else without previous #if...!");
DEF_DQ_ERR(DQERR_CDIR_MULTIPLE_ELSE_PREVPOS,       "ECDirMultipleElse",       "Multiple #else branches detected, previous position: $1");
DEF_DQ_ERR(DQERR_CDIR_COND_WRONG_INC,              "ECDirCondWrongInc",       "$1 for $2 in a different include file!");
DEF_DQ_ERR(DQERR_CDIR_INC_FN_MISSING,              "ECDirIncFileNameMissing", "#include error: file name is missing");
DEF_DQ_ERR(DQERR_CDIR_INC_LOADING,                 "ECDirIncFileLoad",        "Error loading #include file \"$1\"");
DEF_DQ_ERR(DQERR_CDIR_EXPR,                        "ECDirExpr",               "Compiler directive expression error");
DEF_DQ_ERR(DQERR_CDIR_EXPR_TYPE,                   "ECDirExprType",           "Compiler directive expression type error");


//-----------------------------------------------------------------------------
// WARNINGS
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// HINTS
//-----------------------------------------------------------------------------
