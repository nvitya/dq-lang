/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    testfile.cpp
 * authors: Codex
 * created: 2026-03-17
 * brief:
 */

#include "testfile.h"

#include <chrono>
#include <random>
#include <thread>

OTestFile::OTestFile(const string & afilename)
{
  filename = afilename;
}

OTestFile::~OTestFile()
{
}

void OTestFile::Process()
{
  static thread_local mt19937 rng(random_device{}());
  uniform_int_distribution<int> dist(10, 500);

  int sleeptime = dist(rng);
  if (sleeptime < 220)
  {
    exec_err = true;
    errorcnt_err = (sleeptime % 3);
  }
  else
  {
    exec_run = true;
    errorcnt_run = (sleeptime % 3);
  }

  this_thread::sleep_for(chrono::milliseconds(sleeptime));

  processed = true;
}
