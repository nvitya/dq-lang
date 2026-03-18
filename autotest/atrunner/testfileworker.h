/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    testfileworker.h
 * authors: Codex
 * created: 2026-03-17
 * brief:   autotest file worker
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "testfile.h"

using namespace std;

class OTestFileWorker
{
public:
  int               worker_id = 0;
  OTestFile *       testfile = nullptr;

  thread            thr;
  mutex             wait_mtx;
  condition_variable cond;

  atomic<bool>      busy = false;
  atomic<bool>      run_requested = false;
  atomic<bool>      terminate_requested = false;

public:
  OTestFileWorker();
  virtual ~OTestFileWorker();

  virtual void Start(int aworker_id);
  virtual void Stop();
  virtual bool ProcessFile(OTestFile * atestfile);
  virtual bool IsIdle();

protected:
  virtual void ThreadFunc();
};
