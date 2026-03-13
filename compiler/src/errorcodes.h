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

DEF_DQ_ERR(DQERR_ID_EXP_AFTER,     "EIdExpectedAfter",
           "Identifier is expected after \"$1\"");

DEF_DQ_ERR(DQERR_TYPE_SPECIFIER_EXP_AFTER_SYM,     "ETypeSpecExpected",
           "Type specifier \":\" is expected after symbol \"$1\"");

DEF_DQ_ERR(DQERR_VAR_ALREADY_DECLARED_WITH_TYPE,   "EVarAlreadyDeclared",
           "Variable \"$1\" is already declared with type \"$2\"");


//-----------------------------------------------------------------------------
// WARNINGS
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// HINTS
//-----------------------------------------------------------------------------
