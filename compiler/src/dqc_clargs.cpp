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

#include <print>
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

void ODqCompClargs::ParseCmdLineArgs(int argc, char **argv)
{
  string explicit_output;

  for (int i = 1; i < argc; i++)
  {
    string v(argv[i]);

    if ('-' == v[0])  // some compiler switch
    {
      if      ("-v"  == v)    g_opt.verbose = true;
      else if ("-g"  == v)    g_opt.dbg_info = true;
      else if ("-ir" == v)    g_opt.ir_print = true;
      else if ("-c"  == v)    g_opt.compile_only = true;
      else if ("-O0" == v)    g_opt.optlevel = 0;
      else if ("-O1" == v)    g_opt.optlevel = 1;
      else if ("-O2" == v)    g_opt.optlevel = 2;
      else if ("-O3" == v)    g_opt.optlevel = 3;
      else if ("-o"  == v)
      {
        if (i + 1 < argc)
        {
          ++i;
          explicit_output = argv[i];
          has_dash_o = true;
        }
        else
        {
          ++errorcnt;
          print("Missing filename after -o\n");
          PrintUsage();
          return;
        }
      }
      else
      {
        ++errorcnt;
        print("Unknown command line switch: {}\n", v);
        PrintUsage();
        return;
      }
    }
    else if (in_filename.empty())
    {
      in_filename = v;
    }
    else if (!has_dash_o)
    {
      // backward compatibility: second positional arg = output name
      explicit_output = v;
      has_dash_o = true;
    }
    else
    {
      ++errorcnt;
      print("Unexpected argument: {}\n", v);
      PrintUsage();
      return;
    }
  }

  if (in_filename.empty())
  {
    ++errorcnt;
    print("Input file name is missing.\n");
    PrintUsage();
    return;
  }

  // derive base_name by stripping .dq extension
  if (in_filename.size() > 3 && in_filename.substr(in_filename.size() - 3) == ".dq")
  {
    base_name = in_filename.substr(0, in_filename.size() - 3);
  }
  else
  {
    base_name = in_filename;
  }

  if (g_opt.compile_only)
  {
    // -c: compile only, no linking
    out_filename = has_dash_o ? explicit_output : base_name + ".o";
  }
  else
  {
    // object file always goes to base_name.o
    out_filename = base_name + ".o";
    // link_output is where the final result should go
    link_output = has_dash_o ? explicit_output : base_name;
  }

  print("Compiling: \"{}\"...\n", in_filename);
  return;
}

void ODqCompClargs::PrintUsage()
{
  print("Usage:\n");
  print("  dq-comp [options] <file.dq>\n");
  print("Options:\n");
  print("  -o <file> : set output filename\n");
  print("  -c        : compile only (do not link)\n");
  print("  -On       : optimization level, n=0-3\n");
  print("  -g        : generate debug info\n");
  print("  -v        : print compiler internal trace messages\n");
  print("  -ir       : print LLVM IR code\n");
}
