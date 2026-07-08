/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    at_runner.h
 * authors: Codex
 * created: 2026-03-17
 * brief:   dq autotest runner main coordinator
 */

#pragma once

#include <string>
#include <vector>

#include "testfile.h"
#include "testfileworker.h"

using namespace std;

class OAtRunner
{
public:
  vector<OTestFile *>        testfiles;
  vector<OTestFileWorker *>  workers;

public: // stats
  int                        testcnt_run = 0;
  int                        testcnt_err = 0;
  int                        invalid_tf_cnt = 0;

  int                        errorcnt_run = 0;
  int                        errorcnt_run_files = 0;
  int                        errorcnt_err = 0;
  int                        errorcnt_err_files = 0;

  int                        used_worker_count = 1;

  OAtRunner();
  virtual ~OAtRunner();

  int Run();

protected:

  void SleepMs(unsigned ms);
  void CollectTestFiles();
  void DebugPrintCollectedFiles();
  void StartWorkers();
  void StopWorkers();
  void ProcessBatchFilesParallel();
  void ProcessBatchFilesSequential();
  void ProcessResults();

  int RunBatch();
  int RunSingle();
};

extern OAtRunner *  g_atr;
