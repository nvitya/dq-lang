/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    main_atrunner.cpp
 * authors: Codex
 * created: 2026-03-17
 * brief:   dqatrun main entry point
 */

#include "stdio.h"
#include <print>

#include <csignal>
#include <cstdlib>
#include <execinfo.h>
#include <unistd.h>

#include <stacktrace>

#include <iostream>
#include <vector>
#include <string>
#include <print>

#include <fstream>
#include <sstream>
#include <string_view>

#include "atr_version.h"
#include "atr_options.h"
#include "processrunner.h"

using namespace std;

void print_backtrace()
{
  const int max_frames = 50;
  void * array[max_frames];

  // get void*'s for all entries on the stack
  int size = backtrace(array, max_frames);

  // print out all the frames to stderr
  backtrace_symbols_fd(array, size, STDERR_FILENO);
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

  // 3. Must abort manually (standard requirement)
  abort();
}

void signal_handler(int signal)
{
  cout << "Cought Signal SIGSEGV" << endl;

  // Capture the current stacktrace
  auto trace = stacktrace::current();
  // Print it (default formatting includes line numbers if -g is used)
  cout << trace << endl;

  exit(signal);
}

//--------------------------------------------------------------------

static string TrimLineEnd(string s)
{
  while (!s.empty() and ((s.back() == '\n') or (s.back() == '\r')))
  {
    s.pop_back();
  }

  return s;
}

static string QueryCompilerVersion()
{
  OProcessRunner procrunner;
  SProcessResult procresult;

  vector<string> args;
  args.push_back(g_atropt->compiler_filename);
  args.push_back("--version");

  if (!procrunner.Run(args, &procresult))
  {
    return "?";
  }

  if (!procresult.stdout_text.empty())
  {
    return TrimLineEnd(procresult.stdout_text);
  }

  if (!procresult.stderr_text.empty())
  {
    return TrimLineEnd(procresult.stderr_text);
  }

  return "?";
}

static void PrintBatchHeader()
{
  print("DQ Autotest v{}\n", ATR_VERSION);
  print("Compiler:  {}\n", g_atropt->compiler_filename);
  print("C. ver.:   v{}\n", QueryCompilerVersion());
  print("Test root: {}\n", g_atropt->test_root);
  print("\n");
}

int main(int argc, char ** argv)
{
  // Top level error handlers for stack tracing
  set_terminate(my_crash_handler);  // uncaught exception handler with stacktrace
  signal(SIGSEGV, signal_handler);  // separate method for segfaults for stacktrace

  init_atr_options(argc, argv);

  if (!g_atropt)
  {
    return 1;
  }

  if (g_atropt->error_count)
  {
    return g_atropt->error_count;
  }

  if (ATRMODE_BATCH == g_atropt->run_mode)
  {
    PrintBatchHeader();
  }

  return 0;
}
