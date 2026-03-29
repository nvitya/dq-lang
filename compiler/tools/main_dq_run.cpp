/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    main_dq_run.cpp
 * authors: Codex
 * created: 2026-03-29
 * brief:   dq-run utility
 */

#include <print>
#include <filesystem>
#include <string>
#include <vector>
#include <unistd.h>

#include "processrunner.h"

using namespace std;
namespace fs = std::filesystem;

struct SDqRunOptions
{
  vector<string>  compiler_args;
  vector<string>  run_args;
  string          input_filename;
  string          output_filename;
  bool            has_dash_o = false;
};

static void PrintUsage()
{
  print("Usage:\n");
  print("  dq-run [compiler-options] <file.dq> [program-args...]\n");
  print("  dq-run [compiler-options] <file.dq> -- [program-args...]\n");
  print("Notes:\n");
  print("  - compiler options must come before <file.dq>\n");
  print("  - program output is shown live\n");
  print("  - -c is not supported by dq-run\n");
}

static bool NeedsCompilerValue(const string & arg)
{
  return ("-o" == arg);
}

static bool ParseArgs(int argc, char ** argv, SDqRunOptions & opt)
{
  bool program_mode = false;

  for (int i = 1; i < argc; ++i)
  {
    string arg(argv[i]);

    if (program_mode)
    {
      opt.run_args.push_back(arg);
      continue;
    }

    if ("--" == arg)
    {
      program_mode = true;
      continue;
    }

    if (opt.input_filename.empty())
    {
      if (!arg.empty() && ('-' == arg[0]))
      {
        if ("-c" == arg)
        {
          print("dq-run does not support -c (compile only).\n");
          return false;
        }

        opt.compiler_args.push_back(arg);

        if (NeedsCompilerValue(arg))
        {
          if (i + 1 >= argc)
          {
            print("Missing value after {}\n", arg);
            return false;
          }

          ++i;
          opt.output_filename = argv[i];
          opt.has_dash_o = true;
          opt.compiler_args.push_back(opt.output_filename);
        }
      }
      else
      {
        opt.input_filename = arg;
        opt.compiler_args.push_back(arg);
      }
    }
    else
    {
      opt.run_args.push_back(arg);
    }
  }

  if (opt.input_filename.empty())
  {
    print("Input file name is missing.\n");
    return false;
  }

  if (!opt.has_dash_o)
  {
    if (opt.input_filename.size() > 3 && opt.input_filename.substr(opt.input_filename.size() - 3) == ".dq")
    {
      opt.output_filename = opt.input_filename.substr(0, opt.input_filename.size() - 3);
    }
    else
    {
      opt.output_filename = opt.input_filename;
    }
  }

  return true;
}

static string GetExecutableDir()
{
  vector<char> buf(4096);
  ssize_t len = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
  if (len <= 0)
  {
    return "";
  }

  buf[len] = 0;
  return fs::path(buf.data()).parent_path().string();
}

static string ResolveCompilerExecutable()
{
  string exedir = GetExecutableDir();
  if (!exedir.empty())
  {
    fs::path compiler_path = fs::path(exedir) / "dq-comp";
    if (fs::exists(compiler_path))
    {
      return compiler_path.string();
    }
  }

  return "dq-comp";
}

static string ResolveRunExecutable(const string & output_filename)
{
  fs::path outpath(output_filename);
  if (outpath.is_absolute())
  {
    return output_filename;
  }

  if (!outpath.has_parent_path())
  {
    return "./" + output_filename;
  }

  return output_filename;
}

int main(int argc, char ** argv)
{
  SDqRunOptions opt;
  if (!ParseArgs(argc, argv, opt))
  {
    PrintUsage();
    return 1;
  }

  OProcessRunner compiler_runner;
  compiler_runner.args.reserve(opt.compiler_args.size() + 1);
  compiler_runner.args.push_back(ResolveCompilerExecutable());
  for (const string & arg : opt.compiler_args)
  {
    compiler_runner.args.push_back(arg);
  }

  if (!compiler_runner.Run())
  {
    if (!compiler_runner.stdout_text.empty())
    {
      print("{}", compiler_runner.stdout_text);
    }
    if (!compiler_runner.stderr_text.empty())
    {
      print(stderr, "{}", compiler_runner.stderr_text);
    }
    return (compiler_runner.exit_code != 0 ? compiler_runner.exit_code : 1);
  }

  if (!compiler_runner.stdout_text.empty())
  {
    print("{}", compiler_runner.stdout_text);
  }
  if (!compiler_runner.stderr_text.empty())
  {
    print(stderr, "{}", compiler_runner.stderr_text);
  }

  if (compiler_runner.exit_code != 0)
  {
    return compiler_runner.exit_code;
  }

  vector<string> exec_args;
  exec_args.reserve(opt.run_args.size() + 1);
  exec_args.push_back(ResolveRunExecutable(opt.output_filename));
  for (const string & arg : opt.run_args)
  {
    exec_args.push_back(arg);
  }

  int run_exit_code = 0;
  string run_errtxt;
  if (!RunInteractiveProcess(exec_args, run_exit_code, &run_errtxt))
  {
    if (!run_errtxt.empty())
    {
      print(stderr, "{}", run_errtxt);
      if (run_errtxt.back() != '\n')
      {
        print(stderr, "\n");
      }
    }
    return (run_exit_code != 0 ? run_exit_code : 1);
  }

  return run_exit_code;
}
