/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    testfileworker.cpp
 * authors: Codex
 * created: 2026-03-17
 * brief:
 */

#include <print>
#include "testfileworker.h"
#include "atr_options.h"

using namespace std;

OTestFileWorker::OTestFileWorker()
{
}

OTestFileWorker::~OTestFileWorker()
{
  Stop();
}

void OTestFileWorker::Start(int aworker_id)
{
  worker_id = aworker_id;
  thr = thread(&OTestFileWorker::ThreadFunc, this);
}

void OTestFileWorker::Stop()
{
  {
    lock_guard<mutex> lock(wait_mtx);
    terminate_requested = true;
  }
  cond.notify_all();

  if (thr.joinable())
  {
    thr.join();
  }
}

bool OTestFileWorker::ProcessFile(OTestFile * atestfile) // called by the distributor !
{
  lock_guard<mutex> lock(wait_mtx);

  if (busy or run_requested or (nullptr != testfile))
  {
    return false;
  }

  testfile = atestfile;
  run_requested = true;
  cond.notify_all();

  return true;
}

bool OTestFileWorker::IsIdle()
{
  lock_guard<mutex> lock(wait_mtx);
  return (!busy and !run_requested and (nullptr == testfile));
}

void OTestFileWorker::ThreadFunc()
{
  while (true)
  {
    OTestFile * ltestfile = nullptr;

    {
      unique_lock<mutex> lock(wait_mtx);
      cond.wait(lock, [this]{ return run_requested or terminate_requested; });

      if (terminate_requested and !run_requested)
      {
        return;
      }

      busy = true;
      run_requested = false;
      ltestfile = testfile;
    }

    if (ltestfile and !ltestfile->processed)
    {
      if (g_atropt->verblevel >= VERBLEVEL_DEBUG)
      {
        print("W{}: {} starting\n", worker_id, ltestfile->filename);
      }
      ltestfile->Process();
      ltestfile->processed = true;  // ensure
      if (g_atropt->verblevel >= VERBLEVEL_DEBUG)
      {
        print("W{}: {} finished\n", worker_id, ltestfile->filename);
      }
    }

    {
      lock_guard<mutex> lock(wait_mtx);
      testfile = nullptr;
      busy = false;
    }

    cond.notify_all();
  }
}
