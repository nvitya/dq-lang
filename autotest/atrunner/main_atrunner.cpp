/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
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
#include <string>
#include <print>

#include "at_runner.h"
#include "atr_options.h"
#include "atr_version.h"

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
  auto trace = stacktrace::current();
  bool user_code_reached = false;

  cerr << "Backtrace:\n";

  for (const auto & entry : trace)
  {
    // If we already passed the noise, print the frame
    if (user_code_reached)
    {
      cerr << "  " << entry << "\n";
      continue;
    }

    // DETECT THE BOUNDARY:
    // "__cxa_throw" is the specific GCC/Clang function that handles throws.
    // Once we see this, we know the NEXT frame is your code.
    if (entry.description().find("__cxa_throw") != string::npos)
    {
      user_code_reached = true;
    }
  }

  // Safety Fallback: If we never found "__cxa_throw" (e.g., a pure abort()
  // without an exception), print the raw trace so you don't lose info.
  if (!user_code_reached)
  {
      cerr << "  (Full Raw Trace)\n" << trace << "\n";
  }
#else
  cerr << "Backtrace: <not available>\n";
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

  if (g_atropt->arg_error_count)
  {
    return g_atropt->arg_error_count;
  }

  if (g_atropt->print_version) // print only the version
  {
    print("{}\n", ATR_VERSION);
    return 0;
  }

  g_atr = new OAtRunner();

  return g_atr->Run();
}
