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
#include <limits>
#include "dqc_clargs.h"
#include "comp_options.h"

using namespace std;

static bool IsValidDefineName(const string & name)
{
  if (name.empty())
  {
    return false;
  }

  char c = name[0];
  if (!(((c >= 'A') and (c <= 'Z')) or ((c >= 'a') and (c <= 'z')) or (c == '_')))
  {
    return false;
  }

  for (size_t i = 1; i < name.size(); ++i)
  {
    c = name[i];
    if (!(((c >= 'A') and (c <= 'Z')) or ((c >= 'a') and (c <= 'z')) or (c == '_')
          or ((c >= '0') and (c <= '9'))))
    {
      return false;
    }
  }

  return true;
}

static bool ParseDefineIntValue(const string & text, int64_t & rvalue)
{
  if (text.empty())
  {
    return false;
  }

  size_t pos = 0;
  bool negative = false;
  if ((text[pos] == '+') or (text[pos] == '-'))
  {
    negative = (text[pos] == '-');
    ++pos;
  }

  if (pos >= text.size())
  {
    return false;
  }

  uint64_t accum = 0;
  for (; pos < text.size(); ++pos)
  {
    char c = text[pos];
    if ((c < '0') or (c > '9'))
    {
      return false;
    }

    uint64_t digit = (c - '0');
    if (accum > ((numeric_limits<uint64_t>::max() - digit) / 10))
    {
      return false;
    }
    accum = accum * 10 + digit;
  }

  if (negative)
  {
    if (accum > uint64_t(numeric_limits<int64_t>::max()) + 1)
    {
      return false;
    }

    if (accum == uint64_t(numeric_limits<int64_t>::max()) + 1)
    {
      rvalue = numeric_limits<int64_t>::min();
    }
    else
    {
      rvalue = -int64_t(accum);
    }
  }
  else
  {
    if (accum > uint64_t(numeric_limits<int64_t>::max()))
    {
      return false;
    }

    rvalue = int64_t(accum);
  }

  return true;
}

static bool ParseDefineBoolValue(const string & text, bool & rvalue)
{
  if ("true" == text)
  {
    rvalue = true;
    return true;
  }

  if ("false" == text)
  {
    rvalue = false;
    return true;
  }

  return false;
}

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
      if      ("--version" == v)  g_opt.print_version = true;
      else if ("-v"  == v)    g_opt.verbose = true;
      else if ("-g"  == v)    g_opt.dbg_info = true;
      else if ("-ir" == v)    g_opt.ir_print = true;
      else if ("-c"  == v)    g_opt.compile_only = true;
      else if ((v.size() > 2) and ('D' == v[1]))
      {
        string defspec = v.substr(2);
        string defname = defspec;
        string defvalue;
        size_t eqpos = defspec.find('=');
        if (eqpos != string::npos)
        {
          defname = defspec.substr(0, eqpos);
          defvalue = defspec.substr(eqpos + 1);
        }

        if (!IsValidDefineName(defname))
        {
          ++errorcnt;
          print("Invalid command line define name: {}\n", defname);
          PrintUsage();
          return;
        }

        OCmdLineDefine def;
        def.name = defname;

        if (eqpos != string::npos)
        {
          if (ParseDefineBoolValue(defvalue, def.bool_value))
          {
            def.has_bool_value = true;
          }
          else if (ParseDefineIntValue(defvalue, def.int_value))
          {
            def.has_int_value = true;
          }
          else
          {
            ++errorcnt;
            print("Invalid command line define value: {}\n", v);
            PrintUsage();
            return;
          }
        }

        g_opt.cmdline_defines.push_back(def);
      }
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

  if (g_opt.print_version)
  {
    return;
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
  print("  --version : print compiler version\n");
  print("  -D<name>  : defines the <name> symbol with boolean true\n");
  print("  -D<name>=<value> : defines the <name> symbol with the <value> (int/bool)\n");
  print("  -On       : optimization level, n=0-3\n");
  print("  -g        : generate debug info\n");
  print("  -v        : print compiler internal trace messages\n");
  print("  -ir       : print LLVM IR code\n");
}
