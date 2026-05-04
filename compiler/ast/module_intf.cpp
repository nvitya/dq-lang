/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    module_intf.h
 * authors: nvitya
 * created: 2026-05-04
 * brief:   DQ Module Interface Class
 */

#include "module_intf.h"

#include <fstream>
#include <print>

bool OModuleIntf::WriteInterface(const string & filename)
{
  ofstream outf(filename, ios::binary);
  if (!outf)
  {
    print("Can not create module interface file: {}\n", filename);
    return false;
  }

  outf << "DQMIF-STUB\n";
  outf << "name=" << name << "\n";

  if (!outf)
  {
    print("Can not write module interface file: {}\n", filename);
    return false;
  }

  print("Module interface written: {}\n", filename);
  return true;
}

bool OModuleIntf::ReadInterface(const string & filename)
{
  ifstream inf(filename, ios::binary);
  if (!inf)
  {
    print("Can not read module interface file: {}\n", filename);
    return false;
  }

  string header;
  getline(inf, header);
  if ("DQMIF-STUB" != header)
  {
    print("Invalid or unsupported module interface file: {}\n", filename);
    return false;
  }

  string line;
  while (getline(inf, line))
  {
    const string name_prefix = "name=";
    if (line.starts_with(name_prefix))
    {
      name = line.substr(name_prefix.size());
    }
  }

  return true;
}

bool DumpModuleInterface(const string & filename)
{
  OModuleIntf intf(nullptr, "dqm_if_dump");
  if (!intf.ReadInterface(filename))
  {
    return false;
  }

  print("DQ module interface dump: {}\n", filename);
  print("  format: stub\n");
  print("  name: {}\n", intf.name);
  return true;
}
