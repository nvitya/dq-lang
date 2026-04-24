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
  inline constexpr TDiagDefHint asym { astrid, atxt }

//-----------------------------------------------------------------------------
// ERRORS
//-----------------------------------------------------------------------------

DEF_DQ_ERR(DQERR_ID_EXP_AFTER,                     "IdExpected",             "Identifier is expected after \"$1\"");
DEF_DQ_ERR(DQERR_VARNAME_EXP_AFTER,                "VarNameExpected",        "Variable name is expected after \"$1\"");
DEF_DQ_ERR(DQERR_KW_OR_ID_MISSING,                 "KeywordOrIdMissing",     "Keyword or identifier is missing");

DEF_DQ_ERR(DQERR_MISSING_SEMICOLON,                "Semicolon",              "\";\" is missing");
DEF_DQ_ERR(DQERR_MISSING_SEMICOLON_TO_CLOSE,       "Semicolon",              "\";\" is missing to close the $1");
DEF_DQ_ERR(DQERR_MISSING_SEMICOLON_AFTER,          "Semicolon",              "\";\" is missing after $1");
DEF_DQ_ERR(DQERR_MISSING_COMMA,                    "MissingComma",           "\",\" is missing");
DEF_DQ_ERR(DQERR_MISSING_COMMA_IN,                 "MissingComma",           "\",\" is missing in $1");
DEF_DQ_ERR(DQERR_MISSING_ASSIGN_FOR,               "MissingAssign",          "Missing assignment \"=\" for \"$1\"");

DEF_DQ_ERR(DQERR_MISSING_OPEN_PAREN,               "OpenParen",              "Open parenthesis \"(\" is missing");
DEF_DQ_ERR(DQERR_MISSING_OPEN_PAREN_FOR,           "OpenParen",              "Open parenthesis \"(\" is missing for $1");
DEF_DQ_ERR(DQERR_MISSING_OPEN_PAREN_AFTER,         "OpenParen",              "Open parenthesis \"(\" is missing after \"$1\"");
DEF_DQ_ERR(DQERR_MISSING_CLOSE_PAREN,              "CloseParen",             "Closing parenthesis \")\" is missing");
DEF_DQ_ERR(DQERR_MISSING_CLOSE_PAREN_FOR,          "CloseParen",             "Closing parenthesis \")\" is missing for $1");
DEF_DQ_ERR(DQERR_MISSING_CLOSE_PAREN_AFTER,        "CloseParen",             "Closing parenthesis \")\" is missing after \"$1\"");

DEF_DQ_ERR(DQERR_MISSING_CLOSE_BRACKET_FOR,        "CloseBracket",           "Closing square bracket \"]\" is missing for $1");
DEF_DQ_ERR(DQERR_MISSING_CLOSE_BRACKET_AFTER,      "CloseBracket",           "Closing square bracket \"]\" is missing after $1");
DEF_DQ_ERR(DQERR_MISSING_ATTR_CLOSE_AFTER,         "AttrClose",              "Attribute closer \"]]\" is missing after \"$1\"");

DEF_DQ_ERR(DQERR_LIT_HEXNUM,                       "HexLiteral",             "Hexadecimal literal error");
DEF_DQ_ERR(DQERR_LIT_STRING,                       "StrLiteral",             "String literal error");
DEF_DQ_ERR(DQERR_LIT_FLOAT,                        "FloatLiteral",           "Floating point literal error");
DEF_DQ_ERR(DQERR_LIT_INT,                          "IntLiteral",             "Integer literal error");

DEF_DQ_ERR(DQERR_SIZE_SPEC,                        "SizeSpec",               "$1 size must be a positive integer");
DEF_DQ_ERR(DQERR_ARRAY_SIZESPEC,                   "ArraySizeSpec",          "Array size (or \"]\" is expected");
DEF_DQ_ERR(DQERR_ARRAY_CONSTEXPR,                  "ArrayConstExpr",         "Array constant expression error");

DEF_DQ_ERR(DQERR_NOT_IMPLEMENTED_YET,              "NotImplementedYet",      "$1 is not implemented yet");
DEF_DQ_ERR(DQERR_NOT_SUPPORTED,                    "NotSupported",           "$1 is not supported");

DEF_DQ_ERR(DQERR_OP_INVALID_FOR,                   "OpInvalid",              "Operation \"$1\" is invalid for $2");
DEF_DQ_ERR(DQERR_OP_UNHANDLED_FOR,                 "OpUnhandled",            "Unhandled operation \"$1\" for $2");

DEF_DQ_ERR(DQERR_STMTBLK_START_MISSING,            "StmtBlockStartMissing",  "\":\" is missing for statement block start");
DEF_DQ_ERR(DQERR_STMTBLK_CLOSE_MISSING,            "StmtBlockCloseMissing",  "Statement block closer \"$1\" is missing");
DEF_DQ_ERR(DQERR_STMT_UNKNOWN,                     "StmtUnknown",            "Unknown statement or function \"$1\"");
DEF_DQ_ERR(DQERR_STMT_INVALID,                     "StmtInvalid",            "Invalid statement \"$1\"");
DEF_DQ_ERR(DQERR_STMT_ASSIGN_OR_FCALL_EXP,         "StmtAssignOrFCallExp",   "Assignment statement or function call expected");

DEF_DQ_ERR(DQERR_EXPR_EXPECTED,                    "Expression",             "Expression expected");
DEF_DQ_ERR(DQERR_EXPR_UNEXPECTED_CHAR,             "ExprUnexpectedChar",     "Unexpected character \"$1\" in expression");
DEF_DQ_ERR(DQERR_EXPR_WRONG_VALUE_FOR,             "ExprWrongValue",         "Wrong value expression for \"$1\"");
DEF_DQ_ERR(DQERR_EXPR_INITVALUE,                   "ExprInitValue",          "Initial value expression error");
DEF_DQ_ERR(DQERR_EXPR_INITVALUE_FOR,               "ExprInitValue",          "Initial value expression error for \"$1\"");

DEF_DQ_ERR(DQERR_INT_CONSTEXPR_ERROR,              "IntConstExpr",           "Integer constant expressions error");
DEF_DQ_ERR(DQERR_BOOL_CONSTEXPR_ERROR,             "BoolConstExpr",          "Boolean constant expressions error");
DEF_DQ_ERR(DQERR_FLOAT_CONSTEXPR_ERROR,            "FloatConstExpr",         "Floating point constant expressions error");
DEF_DQ_ERR(DQERR_CONSTEXPR_ERROR,                  "ConstExpr",              "$1 constant expressions error");
DEF_DQ_ERR(DQERR_CONSTEXPR_INVALID_FOR,            "ConstExprInvalid",       "Invalid constant expression for \"$1\"");
DEF_DQ_ERR(DQERR_CONSTEXPR_NONCONST_SYM,           "ConstExprNonConstVs",    "Non-constant symbol \"$1\" in $2 constant expression");

DEF_DQ_ERR(DQERR_MODULE_STATEMENT_EXPECTED,        "ModStatementExpected",   "Module statement keyword expected");
DEF_DQ_ERR(DQERR_MODULE_STATEMENT_UNKNOWN,         "ModStatementUnknown",    "Unknown module statement \"$1\"");

DEF_DQ_ERR(DQERR_ATTR_NAME_EXPECTED,               "AttrNameMissing",        "Attribute name expected after \"[[\"");
DEF_DQ_ERR(DQERR_ATTR_UNKNOWN,                     "AttrUnknown",            "Unknown attribute \"$1\"");
DEF_DQ_ERR(DQERR_ATTR_SEPARATOR,                   "AttrSeparator",          "\",\" or \"]]\" expected in the attribute list");
DEF_DQ_ERR(DQERR_ATTR_PAREN_NOT_ALLOWED,           "AttrNoArgs",             "Attribute \"$1\" does not take arguments");
DEF_DQ_ERR(DQERR_ATTR_ARG_INT,                     "AttrArgInt",             "Attribute \"$1\" expects an integer literal argument");
DEF_DQ_ERR(DQERR_ATTR_ARG_POSITIVE_INT,            "AttrArgPosInt",          "Attribute \"$1\" expects a positive integer argument");
DEF_DQ_ERR(DQERR_ATTR_ARG_STRING,                  "AttrArgString",          "Attribute \"$1\" expects a string literal argument");

DEF_DQ_ERR(DQERR_TYPE_SPECIFIER_EXPECTED,          "TypeSpecExpected",       "Type specifier \":\" is expected");
DEF_DQ_ERR(DQERR_TYPE_SPECIFIER_EXP_AFTER,         "TypeSpecExpected",       "Type specifier \":\" is expected after \"$1\"");
DEF_DQ_ERR(DQERR_TYPE_UNKNOWN,                     "TypeUnknown",            "Type \"$1\" is unknown");
DEF_DQ_ERR(DQERR_TYPE_ID_EXP,                      "TypeIdExpected",         "Type identifier is expected");
DEF_DQ_ERR(DQERR_TYPE_ALREADY_DEFINED,             "TypeAlreadyDef",         "Type \"$1\" is already defined");
DEF_DQ_ERR(DQERR_TYPE_ALREADY_DEFINED_IN,          "TypeAlreadyDef",         "Type \"$1\" is already defined in scope \"$2\"");
DEF_DQ_ERR(DQERR_TYPE_ASSIGN_TO_CONST,             "TypeAssignToConst",      "Assignment target \"$1\" is constant");

DEF_DQ_ERR(DQERR_TYPEMISM,                         "TypeMismatch",           "Type mismatch: \"$1\" = \"$2\"");
DEF_DQ_ERR(DQERR_TYPEMISM_FOR_OP,                  "TypeMismatchOp",         "Type mismatch for operation: \"$1\" $2 \"$3\"");
DEF_DQ_ERR(DQERR_PTR_TYPEMISM,                     "TypeMismatchPtr",        "Pointer type mismatch: \"$1\" = \"$2\"");
DEF_DQ_ERR(DQERR_TYPEMISM_STMT_ASSIGN,             "TypeMismatchAssign",     "$1 type mismatch: \"$2\" = \"$3\"");
DEF_DQ_ERR(DQERR_LVALUE_NOT_WRITEABLE,             "NotWriteable",           "The left side of the assignment \"=\" must be writable");
DEF_DQ_ERR(DQERR_TYPE_EXPECTED,                    "TypeExpected",           "\"$1\" type expected, got \"$2\"");
DEF_DQ_ERR(DQERR_TYPE_FLOAT_EXPECTED_FOR,          "TypeFloatExpected",      "float type expected for \"$1\", got \"$2\"");
DEF_DQ_ERR(DQERR_CAST_INVALID,                     "CastInvalid",            "Invalid explicit cast: \"$2($1)\"");
DEF_DQ_ERR(DQERR_CAST_FLOAT_TO_INT,                "CastFloatToInt",         "Invalid float-to-int cast: use round(), ceil(), or floor() instead of \"$2($1)\"");
DEF_DQ_ERR(DQERR_CAST_PTR_WIDTH_MISM,              "CastPtrWidth",           "Pointer cast requires an exact pointer-width integer type, got \"$1\"");
DEF_DQ_ERR(DQERR_CAST_PTR_CONST_RANGE,             "CastPtrConstRange",      "Integer constant \"$1\" does not fit the target pointer width");

DEF_DQ_ERR(DQERR_LEN_INVALID_TYPE,                 "InvalidLenType",         "len() requires an array, slice, or cstring, got \"$1\"");

DEF_DQ_ERR(DQERR_TYPE_NO_MEMBERS,                  "TypeNoMembers",          "Member access \".\" requires a compound value or a ^compound pointer");
DEF_DQ_ERR(DQERR_PTR_OPAQUE_USAGE,                 "PointerOpaque",          "Opaque \"pointer\" must be cast to a typed pointer before $1");
DEF_DQ_ERR(DQERR_MEMBER_NAME_EXPECTED,             "MemberNameExpected",     "Member name expected after \".\"");
DEF_DQ_ERR(DQERR_MEMBER_UNKNOWN,                   "MemberUnknown",          "Unknown member \"$1\" in type \"$1\"");
DEF_DQ_ERR(DQERR_STRUCT_MBID_EXPECTED,             "StructMemberId",         "Member id or \"enstruct\" expected");

DEF_DQ_ERR(DQERR_NS_NAME_EXPECTED,                 "NsName",                 "Namespace name expected after \"@\"");
DEF_DQ_ERR(DQERR_NS_UNKNOWN,                       "NsUnknown",              "Unknown namespace \"$1\"");
DEF_DQ_ERR(DQERR_DOT_MISSING_AFTER_NS_NAME,        "NsDot",                  "Dot \".\" is missing after the namespace name");

DEF_DQ_ERR(DQERR_VS_UNKNOWN,                       "VsUnknown",              "Unknown symbol \"$1\"");
DEF_DQ_ERR(DQERR_VS_UNKNOWN_IN_NAMESPACE,          "NsVsUnknown",            "Unknown symbol \"$1\" in namespace \"$2\"");
DEF_DQ_ERR(DQERR_VAR_UNKNOWN,                      "VarUnknown",             "Unknown variable \"$1\"");
DEF_DQ_ERR(DQERR_VS_ALREADY_DECL_SCOPE,            "VsAlreadyDecl",          "Symbol \"$1\" is already defined in scope \"$2\"");
DEF_DQ_ERR(DQERR_VS_ALREADY_DECL_TYPE,             "VsAlreadyDecl",          "Symbol \"$1\" is already declared with type \"$2\"");
DEF_DQ_ERR(DQERR_OVERLOAD_MIXED_DECL,              "OverloadMixedDecl",      "Function \"$1\" cannot mix overloaded and non-overloaded declarations");
DEF_DQ_ERR(DQERR_OVERLOAD_RETURN_TYPE,             "OverloadReturnType",     "Function \"$1\" overloads must have the same return type");
DEF_DQ_ERR(DQERR_OVERLOAD_DUP_SIGNATURE,           "OverloadDupSignature",   "Function \"$1\" already has an overloaded variant with the same signature");
DEF_DQ_ERR(DQERR_GLOBALVAR_INITVALUE,              "GlobVarInitvalue",       "Invalid initialization value for the global variable \"$1\"");
DEF_DQ_ERR(DQERR_VAR_NOT_INITIALIZED,              "VarNotInit",             "Accessing unitialized variable \"$1\"");
DEF_DQ_ERR(DQERR_REF_LOCAL_INIT_REQUIRED,          "RefLocalInit",           "Local ref \"$1\" requires an initializer");
DEF_DQ_ERR(DQERR_REF_LOCAL_BIND_TARGET,            "RefLocalBind",           "Local ref \"$1\" must bind to an addressable writable variable");
DEF_DQ_ERR(DQERR_REF_LOCAL_TYPE_INFER,             "RefLocalTypeInfer",      "Local ref \"$1\" type cannot be inferred");
DEF_DQ_ERR(DQERR_REF_LOCAL_TYPE_MISM,              "RefLocalType",           "Local ref \"$1\" type mismatch: \"$2\" = \"$3\"");
DEF_DQ_ERR(DQERR_REF_LOCAL_MODE_UNSUPPORTED,       "RefLocalMode",           "Local \"$1\" declarations are not supported");
DEF_DQ_ERR(DQERR_REF_ASSIGN_READONLY,              "RefReadonly",            "Assignment target \"$1\" is read-only");
DEF_DQ_ERR(DQERR_REFOUT_READ_BEFORE_WRITE,         "RefOutRead",             "Output-only reference \"$1\" is read before assignment");

DEF_DQ_ERR(DQERR_ARR_ELEMCOUNT_MISM,               "ArrElemCount",           "Array element count mismatch: expected $1, got $2");  // for array literal definitions
DEF_DQ_ERR(DQERR_ARR_ELEM_TYPE_MISM,               "ArrElemType",            "Array element type mismatch: expected \"$1\", got \"$2\"");
DEF_DQ_ERR(DQERR_ARR_SIZE_MISM,                    "ArrSize",                "Array size mismatch: expected $1, got $2");
DEF_DQ_ERR(DQERR_ARR_SLICE_CONVERSION,             "ArrSlice",               "Cannot convert non-variable array to slice");
DEF_DQ_ERR(DQERR_CSTR_SIZE_EXPECTED,               "CStrSizeExpected",       "cstring size expected, example: cstring[n]");
DEF_DQ_ERR(DQERR_CSTR_SIZE_INVALID,                "CStrSizeInvalid",        "Invalid cstring size, it must be a positive integer");
DEF_DQ_ERR(DQERR_CSTR_CONSTEXPR,                   "CStrConstExpr",          "CString constant expression error: string literal expected");
DEF_DQ_ERR(DQERR_CSTR_CONVERSION,                  "CStrConversion",         "Invalid CString conversion");  // used with custom text

DEF_DQ_ERR(DQERR_PTRARITH_TYPE,                    "PtrArithType",           "Pointer arithmetic requires an integer offset, got \"$1\"");

DEF_DQ_ERR(DQERR_VARARGS_NOT_ALLOWED,              "VarargsNotAllowed",      "Variadic \"...\" is only allowed on [[external]] functions");
DEF_DQ_ERR(DQERR_VARARGS_ALONE,                    "VarargsAlone",           "Variadic functions must have at least one named parameter before '...'");

DEF_DQ_ERR(DQERR_FUNCPAR_NAME_EXP,                 "FuncParNameExpected",    "Function parameter name expected");
DEF_DQ_ERR(DQERR_FUNCPAR_NAME_INVALID,             "FuncParNameInvalid",     "Invalid function parameter name \"$1\"");
DEF_DQ_ERR(DQERR_FUNCPAR_DEFAULT_ORDER,            "FuncParDefaultOrder",    "Function parameter \"$1\" without default value cannot follow defaulted parameters");
DEF_DQ_ERR(DQERR_FUNCPAR_DEFAULT_TYPE,             "FuncParDefaultType",     "Function parameter \"$1\" default value is not supported for type \"$2\"");
DEF_DQ_ERR(DQERR_FUNCPAR_DEFAULT_REF,              "FuncParDefaultRef",      "Reference parameter \"$1\" cannot have a default value");
DEF_DQ_ERR(DQERR_FUNC_RETTYPE_EXPECTED,            "FuncRettypeExpected",    "Function return type identifier expected after \"->\"");
DEF_DQ_ERR(DQERR_FUNC_NO_BODY_ALLOWED_AFTER,       "FuncNoBodyAllowed",      "\";\" is expected after $1");
DEF_DQ_ERR(DQERR_FUNC_RESULT_NOT_SET,              "FuncResultNotSet",       "Function \"$1\" result is not set");
DEF_DQ_ERR(DQERR_FUNC_RESULT_SPECIFIED,            "FuncResultSet",          "Function \"$1\" result is set for function returning no value");
DEF_DQ_ERR(DQERR_FUNC_CALL_PARENTH,                "FuncCall",               "Function call \"$1\" missing parentheses: \"(\"");
DEF_DQ_ERR(DQERR_FUNC_ARGS_LIST,                   "FuncArgList",            "Function \"$1\" argument list error");  // used with custom text
DEF_DQ_ERR(DQERR_FUNC_ARGS_TOO_MANY,               "FuncArgsTooMany",        "Too many arguments are provided for the function \"$1\" call. Expected $2");
DEF_DQ_ERR(DQERR_FUNC_ARGS_TOO_FEW,                "FuncArgsTooFew",         "Too few arguments ($1) are provided for the function \"$2\" call. Expected $3");
DEF_DQ_ERR(DQERR_FUNC_ARG_REF_BIND,                "FuncArgRefBind",         "Argument $1 for function \"$2\" must be an addressable writable variable");
DEF_DQ_ERR(DQERR_FUNC_ARG_REF_NULL,                "FuncArgRefNull",         "Argument $1 for function \"$2\" cannot be null");
DEF_DQ_ERR(DQERR_FUNC_ARG_REF_TYPE,                "FuncArgRefType",         "Reference argument $1 type mismatch for function \"$2\": expected \"$3\"");
DEF_DQ_ERR(DQERR_FUNC_ARG_REF_UNINIT,              "FuncArgRefInit",         "Reference argument \"$1\" is not initialized");
DEF_DQ_ERR(DQERR_FUNCSIG_TYPEMISM,                 "FuncSigType",            "Function signature mismatch: \"$1\" = \"$2\"");
DEF_DQ_ERR(DQERR_EXPR_NOT_CALLABLE,                "ExprNotCallable",        "Expression of type \"$1\" is not callable");

DEF_DQ_ERR(DQERR_CONDEXPR_MISSING_FOR,             "CondExprMissing",        "Condition expression is mission for \"$1\"");
DEF_DQ_ERR(DQERR_BOOL_EXPR_EXPECTED,               "BoolExprExpected",       "bool expression expected, got \"$1\"");
DEF_DQ_ERR(DQERR_MULTIPLE_ELSE,                    "MultipleElse",           "Multiple else branches detected");

DEF_DQ_ERR(DQERR_EXPR_INVALID_ADDROF,              "ExprAddrofInvalid",      "Invalid expression for the address of \"&\" operator");
DEF_DQ_ERR(DQERR_EXPR_VS_NOT_ADDRESSABLE,          "ExprVsNotAddressable",   "\"$1\" is not a variable, cannot take its address");

DEF_DQ_ERR(DQERR_CDIR_KW_MISSING,                  "CDirKwMissing",          "Compiler directive keyword is missing");
DEF_DQ_ERR(DQERR_CDIR_CLOSER_MISSING,              "CDirClose",              "Compiler directive closer \"}\" is missing");
DEF_DQ_ERR(DQERR_CDIR_UNKNOWN,                     "CDirUnknown",            "Unknown compiler directive \"$1\"");
DEF_DQ_ERR(DQERR_CDIR_DEF_ID_MISSING,              "CDirDefIdMissing",       "#define error: identifier is missing");
DEF_DQ_ERR(DQERR_CDIR_COND_ID_MISSING,             "CDirCondIdMissing",      "$1 error: identifier is missing");
DEF_DQ_ERR(DQERR_CDIR_ELCOND_WITHOUT_IF,           "CDirElCondWithoutIf",    "$1 without previous #if...!");
DEF_DQ_ERR(DQERR_CDIR_ELCOND_AFTER_ELSE_PREVPOS,   "CDirElCondAfterElse",    "$1 after previous #else at position $2");
DEF_DQ_ERR(DQERR_CDIR_ENDIF_WITHOUT_IF,            "CDirEndifWithoutIf",     "#endif without previous #if...!");
DEF_DQ_ERR(DQERR_CDIR_ELSE_WITHOUT_IF,             "CDirElseWithoutIf",      "#else without previous #if...!");
DEF_DQ_ERR(DQERR_CDIR_MULTIPLE_ELSE_PREVPOS,       "CDirMultipleElse",       "Multiple #else branches detected, previous position: $1");
DEF_DQ_ERR(DQERR_CDIR_COND_WRONG_INC,              "CDirCondWrongInc",       "$1 for $2 in a different include file!");
DEF_DQ_ERR(DQERR_CDIR_INC_FN_MISSING,              "CDirIncFileNameMissing", "#include error: file name is missing");
DEF_DQ_ERR(DQERR_CDIR_INC_LOADING,                 "CDirIncFileLoad",        "Error loading #include file \"$1\"");
DEF_DQ_ERR(DQERR_CDIR_EXPR,                        "CDirExpr",               "Compiler directive expression error");
DEF_DQ_ERR(DQERR_CDIR_EXPR_TYPE,                   "CDirExprType",           "Compiler directive expression type error");


//-----------------------------------------------------------------------------
// WARNINGS
//-----------------------------------------------------------------------------

DEF_DQ_WARN(DQWARN_ATTR_IGNORED_FOR,               "AttrIgnored",            "Attribute \"$1\" is not applicable to $2 and will be ignored");

//-----------------------------------------------------------------------------
// HINTS
//-----------------------------------------------------------------------------

DEF_DQ_HINT(DQHINT_MEANINGLESS_SEMICOLON,          "Semicolon",              "Meaningless \";\" was found.");
