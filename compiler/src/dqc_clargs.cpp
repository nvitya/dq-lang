/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_clargs.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#include <string>
#include "dqc_clargs.h"
#include "comp_options.h"

using namespace std;

ODqCompClargs::ODqCompClargs()
{
}

ODqCompClargs::~ODqCompClargs()
{
}

int ODqCompClargs::ParseCmdLineArgs(int argc, char **argv)
{
  for (int i = 1; i < argc; i++)
  {
    string v(argv[i]);

    if ('-' == v[0])  // some compiler switch
    {
      if ("-v" == v)
      {
        g_opt.verbose = true;
      }
      else if ("-g" == v)
      {
        g_opt.dbg_info = true;
      }
    }
    else if ("" == in_filename)
    {
      in_filename = v;
    }
    else if ("" == out_filename)
    {
      out_filename = v;
    }
    else
    {

    }
  }

  if ("" == in_filename)
  {
    printf("Input file name is missing.\n");
    PrintUsage();
    return 1;
  }

  if ("" == out_filename)
  {
    out_filename = in_filename + ".o";
  }

  printf("Compiling: \"%s\"...\n", in_filename.c_str());
  return 0;
}

void ODqCompClargs::PrintUsage()
{
  printf("Usage: dq-comp <file.dq> [output.o] [-v] [-g]\n");
}
