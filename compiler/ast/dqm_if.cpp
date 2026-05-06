/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqm_if.cpp
 * authors: nvitya
 * created: 2026-05-06
 * brief:   DQ Module Interface Binary Writer
 */

#include "dqm_if.h"

#include <cstring>
#include <fstream>
#include <format>
#include <print>

#include "symbols.h"
#include "otype_array.h"
#include "otype_bool.h"
#include "otype_cstring.h"
#include "otype_float.h"
#include "otype_func.h"
#include "otype_int.h"

using namespace std;

static_assert(sizeof(TDqmIfHeader) == 32);
static_assert(sizeof(TDqmIfRec) == 4);

bool ODqmIfWriter::Fail(const string & amsg)
{
  if (error.empty())
  {
    error = amsg;
  }
  return false;
}

void ODqmIfWriter::AddU8(vector<uint8_t> & rdst, uint8_t avalue) const
{
  rdst.push_back(avalue);
}

void ODqmIfWriter::AddU16(vector<uint8_t> & rdst, uint16_t avalue) const
{
  rdst.push_back(uint8_t(avalue));
  rdst.push_back(uint8_t(avalue >> 8));
}

void ODqmIfWriter::AddU32(vector<uint8_t> & rdst, uint32_t avalue) const
{
  rdst.push_back(uint8_t(avalue));
  rdst.push_back(uint8_t(avalue >> 8));
  rdst.push_back(uint8_t(avalue >> 16));
  rdst.push_back(uint8_t(avalue >> 24));
}

void ODqmIfWriter::AddU64(vector<uint8_t> & rdst, uint64_t avalue) const
{
  AddU32(rdst, uint32_t(avalue));
  AddU32(rdst, uint32_t(avalue >> 32));
}

bool ODqmIfWriter::AddRec(TDqmIfRecId arecid, const void * adata, size_t alen)
{
  if (!Ok())
  {
    return false;
  }
  if (alen > UINT16_MAX)
  {
    return Fail(format("DQM interface record 0x{:04X} is too large: {} bytes", arecid, alen));
  }
  if (alen && !adata)
  {
    return Fail(format("DQM interface record 0x{:04X} has null payload", arecid));
  }

  AddU16(payload, arecid);
  AddU16(payload, uint16_t(alen));

  const uint8_t * src = static_cast<const uint8_t *>(adata);
  for (size_t i = 0; i < alen; ++i)
  {
    payload.push_back(src[i]);
  }

  while (payload.size() & 3)
  {
    payload.push_back(0);
  }

  return true;
}

bool ODqmIfWriter::AddRec(TDqmIfRecId arecid, const vector<uint8_t> & adata)
{
  return AddRec(arecid, adata.data(), adata.size());
}

bool ODqmIfWriter::AddRecStr(TDqmIfRecId arecid, const string & avalue)
{
  return AddRec(arecid, avalue.data(), avalue.size());
}

bool ODqmIfWriter::AddRecEmpty(TDqmIfRecId arecid)
{
  return AddRec(arecid, nullptr, 0);
}

bool ODqmIfWriter::AddRecU8(TDqmIfRecId arecid, uint8_t avalue)
{
  return AddRec(arecid, &avalue, 1);
}

bool ODqmIfWriter::AddRecU32(TDqmIfRecId arecid, uint32_t avalue)
{
  vector<uint8_t> data;
  AddU32(data, avalue);
  return AddRec(arecid, data);
}

bool ODqmIfWriter::AddRecI32(TDqmIfRecId arecid, int32_t avalue)
{
  return AddRecU32(arecid, uint32_t(avalue));
}

bool ODqmIfWriter::AddRecU64(TDqmIfRecId arecid, uint64_t avalue)
{
  vector<uint8_t> data;
  AddU64(data, avalue);
  return AddRec(arecid, data);
}

bool ODqmIfWriter::AddRecI64(TDqmIfRecId arecid, int64_t avalue)
{
  return AddRecU64(arecid, uint64_t(avalue));
}

uint64_t ODqmIfWriter::Checksum(const vector<uint8_t> & adata) const
{
  uint64_t result = 0;
  for (uint8_t b : adata)
  {
    result += b;
  }
  return result;
}

bool ODqmIfWriter::WriteToFile(const string & filename)
{
  if (!Ok())
  {
    return false;
  }
  if (payload.size() > UINT32_MAX)
  {
    return Fail(format("DQM interface payload is too large: {} bytes", payload.size()));
  }

  TDqmIfHeader header = {};
  memcpy(header.magic, "DQMIF\0", sizeof(header.magic));
  header.header_size = sizeof(TDqmIfHeader);
  header.version = DQMIF_VERSION;
  header.payload_size = uint32_t(payload.size());
  header.payload_hash = Checksum(payload);
  header.header_csum = 0;

  vector<uint8_t> hdata;
  for (char c : header.magic)
  {
    AddU8(hdata, uint8_t(c));
  }
  AddU16(hdata, header.header_size);
  AddU32(hdata, header.version);
  AddU32(hdata, header.payload_size);
  AddU64(hdata, header.payload_hash);
  AddU64(hdata, 0);

  header.header_csum = Checksum(hdata);
  hdata.clear();
  for (char c : header.magic)
  {
    AddU8(hdata, uint8_t(c));
  }
  AddU16(hdata, header.header_size);
  AddU32(hdata, header.version);
  AddU32(hdata, header.payload_size);
  AddU64(hdata, header.payload_hash);
  AddU64(hdata, header.header_csum);

  ofstream outf(filename, ios::binary);
  if (!outf)
  {
    return Fail(format("Can not create module interface file: {}", filename));
  }

  outf.write(reinterpret_cast<const char *>(hdata.data()), hdata.size());
  outf.write(reinterpret_cast<const char *>(payload.data()), payload.size());

  if (!outf)
  {
    return Fail(format("Can not write module interface file: {}", filename));
  }

  return true;
}

static bool WriteTypeSpecRecord(ODqmIfWriter & writer, TDqmIfRecId arecid, OType * atype)
{
  if (!atype)
  {
    return writer.Fail("Can not write null type spec");
  }
  return writer.AddRecStr(arecid, atype->name);
}

bool OType::WriteDqmIfTypeSpec(ODqmIfWriter & writer)
{
  OType * base = this;
  int ptrdepth = 0;

  while (auto * ptrtype = dynamic_cast<OTypePointer *>(base))
  {
    ++ptrdepth;
    if (ptrdepth > 3)
    {
      return writer.Fail(format("DQM interface supports only up to 3 pointer levels: {}", name));
    }
    base = ptrtype->basetype;
  }

  switch (ptrdepth)
  {
    case 0:  return WriteTypeSpecRecord(writer, DQMIF_TYPE_SPEC, base);
    case 1:  return base ? WriteTypeSpecRecord(writer, DQMIF_TYPE_SPEC_PTR1, base)
                          : writer.AddRecStr(DQMIF_TYPE_SPEC_PTR1, "void");
    case 2:  return base ? WriteTypeSpecRecord(writer, DQMIF_TYPE_SPEC_PTR2, base)
                          : writer.AddRecStr(DQMIF_TYPE_SPEC_PTR2, "void");
    case 3:  return base ? WriteTypeSpecRecord(writer, DQMIF_TYPE_SPEC_PTR3, base)
                          : writer.AddRecStr(DQMIF_TYPE_SPEC_PTR3, "void");
  }

  return writer.Fail(format("Invalid pointer depth while writing type spec: {}", name));
}

bool OType::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  if (!writer.AddRecStr(DQMIF_TYPE_BEGIN, name)) return false;
  if (!WriteDqmIfTypeSpec(writer)) return false;
  return writer.AddRecEmpty(DQMIF_TYPE_END);
}

bool OTypeAlias::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  if (!ptype)
  {
    return writer.Fail(format("Type alias {} has no target type", name));
  }
  if (!writer.AddRecStr(DQMIF_TYPE_BEGIN, name)) return false;
  if (!ptype->WriteDqmIfTypeSpec(writer)) return false;
  return writer.AddRecEmpty(DQMIF_TYPE_END);
}

static bool WriteValSymAttributes(ODqmIfWriter & writer, OValSym * avsym, uint64_t aextra_flags = 0)
{
  if (!avsym)
  {
    return writer.Fail("Can not write attributes for null symbol");
  }

  uint64_t flags = aextra_flags;
  if (avsym->attr_is_overload) flags |= 1u << 0;
  if (avsym->attr_is_override) flags |= 1u << 1;
  if (avsym->attr_is_virtual)  flags |= 1u << 2;
  if (avsym->attr_is_volatile) flags |= 1u << 3;
  if (avsym->is_ref_alias)     flags |= 1u << 4;
  if (avsym->ref_nullable)     flags |= 1u << 5;

  if (flags && !writer.AddRecU64(DQMIF_ATTR_FLAGS, flags)) return false;
  if (avsym->attr_align && !writer.AddRecI32(DQMIF_ATTR_ALIGN_VALUE, int32_t(avsym->attr_align))) return false;
  if (!avsym->attr_section_name.empty()
      && !writer.AddRecStr(DQMIF_ATTR_SECTION_NAME, avsym->attr_section_name)) return false;

  return true;
}

bool OCompoundType::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  TDqmIfRecId begin_rec = (is_object ? DQMIF_OBJ_BEGIN : DQMIF_STRUCT_BEGIN);
  TDqmIfRecId end_rec   = (is_object ? DQMIF_OBJ_END   : DQMIF_STRUCT_END);

  if (!writer.AddRecStr(begin_rec, name)) return false;

  for (OValSym * member : member_order)
  {
    if (!member)
    {
      return writer.Fail(format("Compound type {} has a null member", name));
    }
    if (!writer.AddRecStr(DQMIF_FIELD_BEGIN, member->name)) return false;
    if (!WriteValSymAttributes(writer, member)) return false;
    if (!member->ptype)
    {
      return writer.Fail(format("Field {}.{} has no type", name, member->name));
    }
    if (!member->ptype->WriteDqmIfTypeSpec(writer)) return false;
    if (!writer.AddRecEmpty(DQMIF_FIELD_END)) return false;
  }

  if (is_object)
  {
    for (auto & [mname, vs] : Members()->valsyms)
    {
      (void)mname;
      if (!vs || VSK_FUNCTION != vs->kind)
      {
        continue;
      }
      if (auto * fn = dynamic_cast<OValSymFunc *>(vs))
      {
        if (!fn->WriteDqmIfFunction(writer, true)) return false;
      }
      else if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(vs))
      {
        if (!ovset->WriteDqmIfMethods(writer)) return false;
      }
      else
      {
        return writer.Fail(format("Unsupported object method symbol: {}", vs->name));
      }
    }
  }

  return writer.AddRecEmpty(end_rec);
}

bool OValue::WriteDqmIfValue(ODqmIfWriter & writer)
{
  return writer.Fail(format("Unsupported constant value type in DQM interface: {}", ptype ? ptype->name : "?"));
}

bool OValueInt::WriteDqmIfValue(ODqmIfWriter & writer)
{
  return writer.AddRecI64(DQMIF_VALUE_INLINE, value);
}

bool OValueFloat::WriteDqmIfValue(ODqmIfWriter & writer)
{
  uint64_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value));
  memcpy(&bits, &value, sizeof(bits));
  return writer.AddRecU64(DQMIF_VALUE_INLINE, bits);
}

bool OValueBool::WriteDqmIfValue(ODqmIfWriter & writer)
{
  return writer.AddRecU8(DQMIF_VALUE_INLINE, value ? 1 : 0);
}

bool OValueCString::WriteDqmIfValue(ODqmIfWriter & writer)
{
  return writer.AddRecStr(DQMIF_VALUE_INLINE, value);
}

bool OValuePointer::WriteDqmIfValue(ODqmIfWriter & writer)
{
  if (!is_null)
  {
    return writer.Fail("Only null pointer constants are supported in DQM interface generation");
  }
  return writer.AddRecU64(DQMIF_VALUE_INLINE, 0);
}

bool OValueArray::WriteDqmIfValue(ODqmIfWriter & writer)
{
  return writer.AddRecEmpty(DQMIF_VALUE_LINKED);
}

bool OValueFuncRef::WriteDqmIfValue(ODqmIfWriter & writer)
{
  if (!is_null)
  {
    return writer.Fail("Only null function reference constants are supported in DQM interface generation");
  }
  return writer.AddRecU64(DQMIF_VALUE_INLINE, 0);
}

bool OValSym::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  if (VSK_VARIABLE != kind)
  {
    return writer.Fail(format("Unsupported value symbol in DQM interface: {}", name));
  }
  if (!ptype)
  {
    return writer.Fail(format("Variable {} has no type", name));
  }

  if (!writer.AddRecStr(DQMIF_VAR_BEGIN, name)) return false;
  if (!WriteValSymAttributes(writer, this)) return false;
  if (!ptype->WriteDqmIfTypeSpec(writer)) return false;
  return writer.AddRecEmpty(DQMIF_VAR_END);
}

bool OValSymConst::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  if (!ptype)
  {
    return writer.Fail(format("Constant {} has no type", name));
  }
  if (!pvalue)
  {
    return writer.Fail(format("Constant {} has no value", name));
  }

  if (!writer.AddRecStr(DQMIF_CONST_BEGIN, name)) return false;
  if (!ptype->WriteDqmIfTypeSpec(writer)) return false;
  if (!pvalue->WriteDqmIfValue(writer)) return false;
  return writer.AddRecEmpty(DQMIF_CONST_END);
}

bool OFuncParam::WriteDqmIf(ODqmIfWriter & writer) const
{
  if (!ptype)
  {
    return writer.Fail(format("Function parameter {} has no type", name));
  }

  if (!writer.AddRecStr(DQMIF_FUNC_PARAM_BEGIN, name)) return false;

  switch (mode)
  {
    case FPM_VALUE:    break;
    case FPM_REF:      if (!writer.AddRecEmpty(DQMIF_FUNC_PARAM_MODE_REF)) return false; break;
    case FPM_REFIN:    if (!writer.AddRecEmpty(DQMIF_FUNC_PARAM_MODE_REFIN)) return false; break;
    case FPM_REFOUT:   if (!writer.AddRecEmpty(DQMIF_FUNC_PARAM_MODE_REFOUT)) return false; break;
    case FPM_REFNULL:  return writer.Fail(format("refnull parameter {} is not supported in DQM interface generation", name));
  }

  if (!ptype->WriteDqmIfTypeSpec(writer)) return false;
  if (defvalue && defvalue->pvalue && !defvalue->pvalue->WriteDqmIfValue(writer)) return false;
  return writer.AddRecEmpty(DQMIF_FUNC_PARAM_END);
}

bool OTypeFunc::WriteDqmIfTypeSpec(ODqmIfWriter & writer)
{
  return writer.AddRecStr(DQMIF_TYPE_SPEC_FUNCREF, FuncTypeName(this));
}

bool OValSymFunc::WriteDqmIfFunction(ODqmIfWriter & writer, bool amethod)
{
  OTypeFunc * sigtype = dynamic_cast<OTypeFunc *>(ptype);
  if (!sigtype)
  {
    return writer.Fail(format("Function {} has no function signature type", name));
  }

  if (!writer.AddRecStr(amethod ? DQMIF_METHOD_BEGIN : DQMIF_FUNC_BEGIN, name)) return false;

  uint64_t flags = 0;
  if (is_external) flags |= 1u << 6;
  if (!WriteValSymAttributes(writer, this, flags)) return false;
  if (!external_linkage_name.empty()
      && !writer.AddRecStr(DQMIF_ATTR_EXT_LINK_NAME, external_linkage_name)) return false;

  if (sigtype->rettype && TK_VOID != sigtype->rettype->kind)
  {
    if (!writer.AddRecEmpty(DQMIF_FUNC_RETVAL)) return false;
    if (!sigtype->rettype->WriteDqmIfTypeSpec(writer)) return false;
  }

  for (size_t i = 0; i < sigtype->params.size(); ++i)
  {
    OFuncParam * param = sigtype->params[i];
    if (!param)
    {
      return writer.Fail(format("Function {} has a null parameter", name));
    }
    if (amethod && (0 == i) && owner_compound_type && ("__this" == param->name))
    {
      continue;
    }
    if (!param->WriteDqmIf(writer)) return false;
  }

  if (sigtype->has_varargs && !writer.AddRecEmpty(DQMIF_FUNC_PARAM_VARARGS)) return false;

  return writer.AddRecEmpty(amethod ? DQMIF_METHOD_END : DQMIF_FUNC_END);
}

bool OValSymFunc::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  return WriteDqmIfFunction(writer, false);
}

bool OValSymOverloadSet::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  for (OValSymFunc * fn : funcs)
  {
    if (!fn)
    {
      return writer.Fail(format("Overload set {} has a null function", name));
    }
    if (!fn->WriteDqmIfFunction(writer, false)) return false;
  }
  return true;
}

bool OValSymOverloadSet::WriteDqmIfMethods(ODqmIfWriter & writer)
{
  for (OValSymFunc * fn : funcs)
  {
    if (!fn)
    {
      return writer.Fail(format("Overload set {} has a null method", name));
    }
    if (!fn->WriteDqmIfFunction(writer, true)) return false;
  }
  return true;
}

bool OTypeFuncRef::WriteDqmIfTypeSpec(ODqmIfWriter & writer)
{
  return writer.AddRecStr(DQMIF_TYPE_SPEC_FUNCREF, name);
}
