/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    main_dq_comp.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:   DQ Compiler Main (entry point) and crash handler
 */

#include "stdio.h"

#include <csignal>
#include <cstdlib>
#include <exception>

#if !defined(_WIN32) && __has_include(<execinfo.h>)
  #include <execinfo.h>
  #include <unistd.h>
  #define HAS_EXECINFO 1
#endif

#if __has_include(<stacktrace>)
#include <stacktrace>
#define HAS_STACKTRACE 1
#endif

#include <iostream>
#include <vector>
#include <string>
#include <print>

#include <fstream>
#include <sstream>
#include <filesystem>

#include "dqc.h"
#include "projectfile.h"
#include "version.h"

#include "ll_defs.h"
#include "named_scopes.h"
#include "scope_builtins.h"
#include "scope_defines.h"

#define CALL_TESTCODE 0

#if CALL_TESTCODE
  #include "testcode.h"
#endif

using namespace std;

void print_backtrace()
{
#if HAS_EXECINFO
  const int max_frames = 50;
  void * array[max_frames];

  // get void*'s for all entries on the stack
  int size = backtrace(array, max_frames);

  // print out all the frames to stderr
  backtrace_symbols_fd(array, size, STDERR_FILENO);
#endif
}

// This function will run when ANY exception goes uncaught
void my_crash_handler()
{
  // 1. Try to get the exception message
  try
  {
    rethrow_exception(current_exception());
  }
  catch (const exception & e)
  {
    cerr << "UNCAUGHT EXCEPTION: " << e.what() << "\n";
  }
  catch (...)
  {
    cerr << "UNCAUGHT EXCEPTION: [Unknown Type]\n";
  }

  // 2. Filter the backtrace
#if HAS_STACKTRACE
  auto trace = std::stacktrace::current();
  bool user_code_reached = false;

  std::cerr << "Backtrace:\n";

  for (const auto & entry : trace)
  {
    // If we already passed the noise, print the frame
    if (user_code_reached)
    {
      std::cerr << "  " << entry << "\n";
      continue;
    }

    // DETECT THE BOUNDARY:
    // "__cxa_throw" is the specific GCC/Clang function that handles throws.
    // Once we see this, we know the NEXT frame is your code.
    if (entry.description().find("__cxa_throw") != std::string::npos)
    {
      user_code_reached = true;
    }
  }

  // Safety Fallback: If we never found "__cxa_throw" (e.g., a pure std::abort()
  // without an exception), print the raw trace so you don't lose info.
  if (!user_code_reached)
  {
      std::cerr << "  (Full Raw Trace)\n" << trace << "\n";
  }
#else
  std::cerr << "Backtrace: <not available>\n";
#endif

  // 3. Must abort manually (standard requirement)
  abort();
}

void signal_handler(int signal)
{
  cout << "Cought Signal SIGSEGV" << endl;

#if HAS_STACKTRACE
  // Capture the current stacktrace
  auto trace = stacktrace::current();
  // Print it (default formatting includes line numbers if -g is used)
  cout << trace << endl;
#endif

  exit(signal);
}

//--------------------------------------------------------------------

static bool HasVersionArg(int argc, char ** argv)
{
  for (int i = 1; i < argc; ++i)
  {
    if (string(argv[i]) == "--version")
    {
      return true;
    }
  }

  return false;
}

struct SProjectStartupArgs
{
  string project_filename;
  vector<string> command_line_package_paths;
};

static SProjectStartupArgs ScanProjectStartupArgs(int argc, char ** argv)
{
  SProjectStartupArgs result;
  bool found_first_positional = false;

  for (int i = 1; i < argc; ++i)
  {
    string arg(argv[i]);
    if (arg == "--pkg-path")
    {
      if (i + 1 < argc) result.command_line_package_paths.push_back(argv[++i]);
      continue;
    }
    if (OCompOptions::CommandLineOptionHasValue(arg))
    {
      if (i + 1 < argc) ++i;
      continue;
    }
    if (!arg.empty() && (arg[0] == '-')) continue;
    if (found_first_positional) continue;
    found_first_positional = true;
    filesystem::path input(arg);
    if (input.extension() == ".dqproj" || input.extension() == ".dqprj")
    {
      result.project_filename = arg;
    }
  }
  return result;
}

int main(int argc, char ** argv)
{
  int r;

  if (HasVersionArg(argc, argv))
  {
    print("{}\n", DQ_COMPILER_VERSION);
    return 0;
  }

  // Top level error handlers for stack tracing
  set_terminate(my_crash_handler);  // uncaught exception handler with stacktrace
  signal(SIGSEGV, signal_handler);  // separate method for segfaults for stacktrace

  #if CALL_TESTCODE
    print("Calling testcode...\n");
    testcode();
  #endif

  g_opt.InitializeCompilerExecutable(argc > 0 ? argv[0] : "");
  g_opt.package_paths = g_opt.DefaultPackagePaths();

  // Pre-collect some command line arguments:
  //   - project file name
  //   - package search paths making them availabe for project files
  SProjectStartupArgs startup_args = ScanProjectStartupArgs(argc, argv);

  // Process the project file
  ODqProject project;
  if (!startup_args.project_filename.empty())
  {
    if (filesystem::path(startup_args.project_filename).extension() != ".dqproj")
    {
      print("DQ project files must use the .dqproj extension\n");
      return 1;
    }
    if (!project.Load(startup_args.project_filename, startup_args.command_line_package_paths))
    {
      for (const SDqProjectDiagnostic & diagnostic : project.diagnostics)
      {
        print("{}\n", diagnostic.Format());
      }
      return 1;
    }
  }

  // Pre-configure target
  string target_error;
  string project_target = (startup_args.project_filename.empty() ? "" : g_opt.target.name);
  if (!g_opt.target.ConfigureFromCommandLine(argc, argv, target_error, project_target))
  {
    print("{}\n", target_error);
    return 1;
  }

  g_opt.ApplyTargetDefaults();

  string command_line_error;
  if (!g_opt.ProcessCommandLineOpts(argc, argv, command_line_error))
  {
    print("{}\n", command_line_error);
    OCompOptions::PrintUsage();
    return 1;
  }

  string runtime_error;
  if (!g_opt.ValidateRuntimeSettings(runtime_error))
  {
    print("{}\n", runtime_error);
    return 1;
  }

  // Compiler initialization

  ll_defs_init();

  g_compiler = new ODqCompiler();

  init_scope_builtins();   // this may depend on some command line / project options
  init_scope_defines();    // this may depend on some command line / project options

  init_dq_module();
  init_named_scopes();

  g_compiler->Run(argc, argv);
  r = g_compiler->errorcnt;

  delete g_compiler;

  //printf("\n");
  return r;
}
