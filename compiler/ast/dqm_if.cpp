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

bool ODqmIfWriter::AddTypeSpecRec(TDqmIfRecId arecid, const string & atypename)
{
  if (atypename.empty())
  {
    return Fail("Can not write empty DQM interface type spec");
  }
  return AddRecStr(arecid, atypename);
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
