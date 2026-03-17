/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    atr_options.cpp
 * authors: Codex
 * created: 2026-03-17
 * brief:
 */

#include <print>
#include <string>
#include <vector>

#include "atr_options.h"

using namespace std;

OAtrOptions *  g_atropt = nullptr;

static bool IsNumber(const string & text)
{
  if (text.empty())
  {
    return false;
  }

  for (char c : text)
  {
    if ((c < '0') or (c > '9'))
    {
      return false;
    }
  }

  return true;
}

OAtrOptions::OAtrOptions()
{
}

OAtrOptions::~OAtrOptions()
{
}

void OAtrOptions::ParseCmdLineArgs(int argc, char ** argv)
{
  vector<string> posargs;

  for (int i = 1; i < argc; ++i)
  {
    string v(argv[i]);

    if ('-' == v[0])
    {
      if ("-v" == v)
      {
        verbose = true;
      }
      else if ("-c" == v)
      {
        if (i + 1 < argc)
        {
          ++i;
          compiler_filename = argv[i];
        }
        else
        {
          ++error_count;
          print("Missing compiler filename after -c\n");
          PrintUsage();
          return;
        }
      }
      else if ("-r" == v)
      {
        if (i + 1 < argc)
        {
          ++i;
          test_root = argv[i];
        }
        else
        {
          ++error_count;
          print("Missing test root after -r\n");
          PrintUsage();
          return;
        }
      }
      else if ("-j" == v)
      {
        if (i + 1 < argc)
        {
          ++i;
          string wn(argv[i]);
          if (!IsNumber(wn))
          {
            ++error_count;
            print("Invalid worker count: {}\n", wn);
            PrintUsage();
            return;
          }

          worker_count = stoi(wn);
          if (worker_count < 1)
          {
            ++error_count;
            print("Worker count must be at least 1.\n");
            PrintUsage();
            return;
          }
        }
        else
        {
          ++error_count;
          print("Missing worker count after -j\n");
          PrintUsage();
          return;
        }
      }
      else
      {
        ++error_count;
        print("Unknown command line switch: {}\n", v);
        PrintUsage();
        return;
      }
    }
    else
    {
      posargs.push_back(v);
    }
  }

  if (posargs.size() > 1)
  {
    ++error_count;
    print("Too many positional arguments.\n");
    PrintUsage();
    return;
  }

  if (posargs.size() == 1)
  {
    single_test_filename = posargs[0];
    run_mode = ATRMODE_SINGLE;
  }
  else
  {
    run_mode = ATRMODE_BATCH;
  }
}

void OAtrOptions::PrintUsage()
{
  print("Usage:\n");
  print("  dqatrun [options] <file.dq>\n");
  print("  dqatrun [options]\n");
  print("Options:\n");
  print("  -c <file> : set compiler executable filename\n");
  print("  -r <dir>  : set batch test root directory\n");
  print("  -j <n>    : set batch worker count\n");
  print("  -v        : verbose output\n");
}

void init_atr_options(int argc, char ** argv)
{
  g_atropt = new OAtrOptions();
  g_atropt->ParseCmdLineArgs(argc, argv);
}
