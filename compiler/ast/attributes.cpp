/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    attributes.cpp
 * authors: nvitya
 * created: 2026-04-22
 * brief:   Attributes
 */

#include "attributes.h"
#include "errorcodes.h"
#include "dqc.h"

void OAttr::Reset()
{
  flags = 0;
  align_value = 0;
  section_name = "";
}

void OAttr::CheckInvalidAttributes(EAttrTarget atarget)
{
  CheckAttrAllowed(ATTF_EXTERNAL, atarget, ATGT_FUNCTION);
  CheckAttrAllowed(ATTF_ALIGN,    atarget, ATGT_GLOBAL_VAR | ATGT_GLOBAL_CONST);
  CheckAttrAllowed(ATTF_SECTION,  atarget, ATGT_FUNCTION | ATGT_GLOBAL_VAR | ATGT_GLOBAL_CONST);
  CheckAttrAllowed(ATTF_OVERLOAD, atarget, ATGT_FUNCTION);
  CheckAttrAllowed(ATTF_OVERRIDE, atarget, ATGT_FUNCTION);
  CheckAttrAllowed(ATTF_VIRTUAL,  atarget, ATGT_FUNCTION);
  CheckAttrAllowed(ATTF_VOLATILE, atarget, ATGT_GLOBAL_VAR | ATGT_STRUCT_MEMBER);
}

void OAttr::CheckAttrAllowed(EAttrFlag aflag, EAttrTarget atarget, uint32_t allowed_target_mask)
{
  if ( (flags & aflag) && (0 == (atarget & allowed_target_mask)) )
  {
    g_compiler->Warning(DQWARN_ATTR_IGNORED_FOR, AttrName(aflag), AttrTargetName(atarget), &scpos);
  }
}

//--------------------------------------

string AttrName(EAttrFlag aflag)
{
  switch (aflag)
  {
    case ATTF_ALIGN:         return "align";
    case ATTF_VOLATILE:      return "volatile";
    case ATTF_OVERLOAD:      return "overload";
    case ATTF_EXTERNAL:      return "external";
    case ATTF_SECTION:       return "section";
    case ATTF_VIRTUAL:       return "virtual";
    case ATTF_OVERRIDE:      return "override";

    default:                 return "ATTR_"+to_string(aflag);
  }
}

string AttrTargetName(EAttrTarget atarget)
{
  switch (atarget)
  {
    case ATGT_FUNCTION:      return "function";
    case ATGT_GLOBAL_VAR:    return "global variable";
    case ATGT_GLOBAL_CONST:  return "global constant";
    case ATGT_STRUCT_MEMBER: return "struct member";

    default:                 return "ATGT_"+to_string(atarget);
  }
}
