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

#include "testfileworker.h"

#include <print>

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
  terminate_requested = true;
  cond.notify_all();

  if (thr.joinable())
  {
    thr.join();
  }
}

bool OTestFileWorker::ProcessFile(OTestFile * atestfile) // called by the distributor !
{
  if (busy or run_requested)
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
  return (!busy and !run_requested and (nullptr == testfile));
}

void OTestFileWorker::ThreadFunc()
{
  while (true)
  {
    {
      unique_lock<mutex> lock(wait_mtx);
      cond.wait(lock, [this]{ return run_requested or terminate_requested; });

      if (terminate_requested)
      {
        return;
      }
    }

    busy = true;
    run_requested = false;

    if (testfile and !testfile->processed)
    {
      print("W{}: {} starting\n", worker_id, testfile->filename);
      testfile->Process();
      testfile->processed = true;  // ensure 
      print("W{}: {} finished\n", worker_id, testfile->filename);
    }

    busy = false;
  }
}
