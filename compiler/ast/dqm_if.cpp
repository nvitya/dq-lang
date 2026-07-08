/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
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

#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Error.h>

#include "artifact_lock.h"

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

bool ODqmIfWriter::BuildFileData(vector<uint8_t> & rdata)
{
  rdata.clear();
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

  rdata = hdata;
  rdata.insert(rdata.end(), payload.begin(), payload.end());

  return true;
}

bool ODqmIfWriter::WriteToFile(const string & filename)
{
  vector<uint8_t> data;
  if (!BuildFileData(data))
  {
    return false;
  }

  string write_error;
  return ArtifactAtomicWrite(filename, data, write_error)
      || Fail(format("Can not write module interface file: {}\n{}", filename, write_error));
}

bool ODqmIfReader::Fail(const string & amsg)
{
  if (error.empty())
  {
    error = amsg;
  }
  return false;
}

uint64_t ODqmIfReader::Checksum(const vector<uint8_t> & adata) const
{
  uint64_t result = 0;
  for (uint8_t b : adata)
  {
    result += b;
  }
  return result;
}

static uint16_t DqmIfReadU16At(const vector<uint8_t> & data, size_t pos)
{
  return uint16_t(data[pos]) | (uint16_t(data[pos + 1]) << 8);
}

static uint32_t DqmIfReadU32At(const vector<uint8_t> & data, size_t pos)
{
  return uint32_t(data[pos])
       | (uint32_t(data[pos + 1]) << 8)
       | (uint32_t(data[pos + 2]) << 16)
       | (uint32_t(data[pos + 3]) << 24);
}

static uint64_t DqmIfReadU64At(const vector<uint8_t> & data, size_t pos)
{
  return uint64_t(DqmIfReadU32At(data, pos))
       | (uint64_t(DqmIfReadU32At(data, pos + 4)) << 32);
}

static string DqmIfLlvmErrorToString(llvm::Error aerr)
{
  return llvm::toString(std::move(aerr));
}

bool ODqmIfReader::ReadFromFile(const string & filename)
{
  ifstream inf(filename, ios::binary | ios::ate);
  if (!inf)
  {
    return Fail(format("Can not read module interface file: {}", filename));
  }

  streamsize fsize = inf.tellg();
  if (fsize < streamsize(sizeof(TDqmIfHeader)))
  {
    return Fail(format("Module interface file is too small: {}", filename));
  }

  vector<uint8_t> data(static_cast<size_t>(fsize));
  inf.seekg(0, ios::beg);
  inf.read(reinterpret_cast<char *>(data.data()), fsize);
  if (!inf)
  {
    return Fail(format("Can not read module interface file: {}", filename));
  }

  return ReadFromData(data, filename);
}

bool ODqmIfReader::ReadFromData(const vector<uint8_t> & data, const string & filename)
{
  if (data.size() < sizeof(TDqmIfHeader))
  {
    return Fail(format("Module interface file is too small: {}", filename));
  }

  if (memcmp(data.data(), "DQMIF\0", 6) != 0)
  {
    return Fail(format("Invalid module interface magic: {}", filename));
  }

  uint16_t header_size = DqmIfReadU16At(data, 6);
  if (header_size != sizeof(TDqmIfHeader))
  {
    return Fail(format("Unsupported module interface header size: {}", header_size));
  }

  uint32_t version = DqmIfReadU32At(data, 8);
  if ((version < DQMIF_MIN_VERSION) || (version > DQMIF_VERSION))
  {
    return Fail(format("Unsupported module interface version: {}.{}",
        version >> 16, version & 0xFFFF));
  }

  uint32_t payload_size = DqmIfReadU32At(data, 12);
  if (uint64_t(header_size) + payload_size != uint64_t(data.size()))
  {
    return Fail(format("Module interface payload size mismatch: header says {}, file has {}",
        payload_size, data.size() - header_size));
  }

  uint64_t payload_hash = DqmIfReadU64At(data, 16);
  uint64_t header_csum = DqmIfReadU64At(data, 24);

  vector<uint8_t> header(data.begin(), data.begin() + header_size);
  for (size_t i = 24; i < 32; ++i)
  {
    header[i] = 0;
  }
  if (Checksum(header) != header_csum)
  {
    return Fail(format("Module interface header checksum mismatch: {}", filename));
  }

  payload.assign(data.begin() + header_size, data.end());
  if (Checksum(payload) != payload_hash)
  {
    return Fail(format("Module interface payload checksum mismatch: {}", filename));
  }

  pos = 0;
  recid = 0;
  reclen = 0;
  recpos = 0;
  return true;
}

bool ODqmIfReader::ReadFromArtifact(const string & filename)
{
  ifstream inf(filename, ios::binary | ios::ate);
  if (!inf)
  {
    return Fail(format("Can not read module interface artifact: {}", filename));
  }

  streamsize fsize = inf.tellg();
  if (fsize < 0)
  {
    return Fail(format("Can not read module interface artifact size: {}", filename));
  }

  vector<uint8_t> data(static_cast<size_t>(fsize));
  inf.seekg(0, ios::beg);
  if (!data.empty())
  {
    inf.read(reinterpret_cast<char *>(data.data()), fsize);
  }
  if (!inf)
  {
    return Fail(format("Can not read module interface artifact: {}", filename));
  }

  if ((data.size() >= sizeof(TDqmIfHeader)) && (memcmp(data.data(), "DQMIF\0", 6) == 0))
  {
    return ReadFromData(data, filename);
  }

  llvm::Expected<llvm::object::OwningBinary<llvm::object::ObjectFile>> obj =
      llvm::object::ObjectFile::createObjectFile(filename);
  if (!obj)
  {
    return Fail(format("Can not read compiled module artifact: {}\n{}",
        filename, DqmIfLlvmErrorToString(obj.takeError())));
  }

  for (const llvm::object::SectionRef & sec : obj->getBinary()->sections())
  {
    llvm::Expected<llvm::StringRef> name = sec.getName();
    if (!name)
    {
      return Fail(format("Can not read section name from compiled module artifact: {}\n{}",
          filename, DqmIfLlvmErrorToString(name.takeError())));
    }

    if (*name != ".dqm_if")
    {
      continue;
    }

    llvm::Expected<llvm::StringRef> contents = sec.getContents();
    if (!contents)
    {
      return Fail(format("Can not read .dqm_if section from compiled module artifact: {}\n{}",
          filename, DqmIfLlvmErrorToString(contents.takeError())));
    }

    vector<uint8_t> dqm_if_data(contents->bytes_begin(), contents->bytes_end());
    return ReadFromData(dqm_if_data, filename);
  }

  return Fail(format("Compiled module artifact has no .dqm_if section: {}", filename));
}

bool ODqmIfReader::NextRec()
{
  if (!Ok())
  {
    return false;
  }
  if (Eof())
  {
    return Fail("Unexpected end of DQM interface record stream");
  }
  if (pos + sizeof(TDqmIfRec) > payload.size())
  {
    return Fail("Truncated DQM interface record header");
  }

  recid = DqmIfReadU16At(payload, pos);
  reclen = DqmIfReadU16At(payload, pos + 2);
  recpos = pos + sizeof(TDqmIfRec);

  size_t nextpos = recpos + reclen;
  if (nextpos > payload.size())
  {
    return Fail(format("DQM interface record 0x{:04X} is truncated", recid));
  }
  while (nextpos & 3)
  {
    ++nextpos;
  }
  if (nextpos > payload.size())
  {
    return Fail(format("DQM interface record 0x{:04X} padding is truncated", recid));
  }

  pos = nextpos;
  return true;
}

bool ODqmIfReader::ExpectEmpty(TDqmIfRecId arecid)
{
  if (recid != arecid)
  {
    return Fail(format("Expected DQM interface record 0x{:04X}, got 0x{:04X}", arecid, recid));
  }
  if (reclen != 0)
  {
    return Fail(format("DQM interface record 0x{:04X} must be empty", recid));
  }
  return true;
}

bool ODqmIfReader::ReadString(string & rvalue)
{
  rvalue.assign(reinterpret_cast<const char *>(payload.data() + recpos), reclen);
  return true;
}

bool ODqmIfReader::ReadU8(uint8_t & rvalue)
{
  if (reclen != 1)
  {
    return Fail(format("DQM interface record 0x{:04X} must contain an uint8", recid));
  }
  rvalue = payload[recpos];
  return true;
}

bool ODqmIfReader::ReadU32(uint32_t & rvalue)
{
  if (reclen != 4)
  {
    return Fail(format("DQM interface record 0x{:04X} must contain an uint32", recid));
  }
  rvalue = DqmIfReadU32At(payload, recpos);
  return true;
}

bool ODqmIfReader::ReadI32(int32_t & rvalue)
{
  uint32_t value = 0;
  if (!ReadU32(value))
  {
    return false;
  }
  rvalue = int32_t(value);
  return true;
}

bool ODqmIfReader::ReadU64(uint64_t & rvalue)
{
  if (reclen != 8)
  {
    return Fail(format("DQM interface record 0x{:04X} must contain an uint64", recid));
  }
  rvalue = DqmIfReadU64At(payload, recpos);
  return true;
}

bool ODqmIfReader::ReadI64(int64_t & rvalue)
{
  uint64_t value = 0;
  if (!ReadU64(value))
  {
    return false;
  }
  rvalue = int64_t(value);
  return true;
}

bool ODqmIfReader::ReadBlob(vector<uint8_t> & rvalue)
{
  rvalue.assign(payload.begin() + recpos, payload.begin() + recpos + reclen);
  return true;
}

bool ODqmIfReader::SkipGroup(TDqmIfRecId abegin_recid, TDqmIfRecId aend_recid)
{
  if (recid != abegin_recid)
  {
    return Fail(format("Expected DQM interface group 0x{:04X}, got 0x{:04X}",
        abegin_recid, recid));
  }

  int depth = 1;
  while (depth > 0)
  {
    if (!NextRec())
    {
      return false;
    }
    if (recid == abegin_recid)
    {
      ++depth;
    }
    else if (recid == aend_recid)
    {
      --depth;
    }
  }
  return true;
}
