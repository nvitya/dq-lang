/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    at_runner.cpp
 * authors: Codex
 * created: 2026-03-17
 * brief:
 */

#include <print>
#include <chrono>
#include <filesystem>
#include <string>
#include <algorithm>
#include <thread>
#include <vector>

#include "at_runner.h"
#include "atr_options.h"
#include "atr_version.h"
#include "processrunner.h"

using namespace std;
namespace fs = std::filesystem;

OAtRunner *  g_atr = nullptr;

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

  vector<string> args { g_atropt->compiler_filename, "--version" };

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

OAtRunner::OAtRunner()
{
}

OAtRunner::~OAtRunner()
{
  StopWorkers();

  for (OTestFile * tf : testfiles)
  {
    delete tf;
  }
}

void OAtRunner::SleepMs(unsigned ms)
{
  this_thread::sleep_for(chrono::milliseconds(ms));
}

void OAtRunner::CollectTestFiles()
{
  for (OTestFile * tf : testfiles)
  {
    delete tf;
  }
  testfiles.clear();

  fs::path rootpath(g_atropt->test_root);
  if (!fs::exists(rootpath))
  {
    return;
  }

  vector<fs::path> foundfiles;

  for (const fs::directory_entry & de : fs::recursive_directory_iterator(rootpath))
  {
    if (!de.is_regular_file())
    {
      continue;
    }

    fs::path p = de.path();
    if (".dq" != p.extension().string())
    {
      continue;
    }

    foundfiles.push_back(fs::relative(p, rootpath));
  }

  sort(foundfiles.begin(), foundfiles.end());

  for (const fs::path & rp : foundfiles)
  {
    testfiles.push_back(new OTestFile(rp.generic_string()));
  }
}

void OAtRunner::DebugPrintCollectedFiles()
{
  for (OTestFile * tf : testfiles)
  {
    print("{}\n", tf->filename);
  }
}

void OAtRunner::StartWorkers()
{
  StopWorkers();

  int worker_count = g_atropt->worker_count;
  if (worker_count < 1)
  {
    worker_count = 1;
  }

  for (int i = 0; i < worker_count; ++i)
  {
    OTestFileWorker * worker = new OTestFileWorker();
    workers.push_back(worker);
    worker->Start(i + 1);
  }
}

void OAtRunner::StopWorkers()
{
  for (OTestFileWorker * worker : workers)
  {
    worker->Stop();
    delete worker;
  }

  workers.clear();
}

void OAtRunner::ProcessBatchFiles()
{
  StartWorkers();

  // assign all the work to workers, keep the workers always busy

  size_t fileidx = 0;
  while (fileidx < testfiles.size())
  {
    bool allbusy = true;

    for (OTestFileWorker * worker : workers)
    {
      if (not worker->busy)
      {
        allbusy = false;
        if (worker->ProcessFile(testfiles[fileidx]))
        {
          ++fileidx;
          if (fileidx >= testfiles.size())
          {
            break;
          }
        }
      }
    }

    if (allbusy)
    {
      SleepMs(2);
    }
  }

  // wait for the last completions

  while (true)
  {
    bool allidle = true;

    for (OTestFileWorker * worker : workers)
    {
      if (worker->busy)
      {
        allidle = false;
      }
    }

    if (allidle)
    {
      break;
    }

    SleepMs(2);
  }

  StopWorkers();
}

void OAtRunner::ProcessResults()
{
  // collect the results

  for (OTestFile * tf : testfiles)
  {
    if (tf->exec_err)
    {
      ++testcnt_err;
      if (tf->errorcnt_err > 0)
      {
        ++errorcnt_err_files;
        errorcnt_err += tf->errorcnt_err;
      }
    }

    if (tf->exec_run)
    {
      ++testcnt_run;
      if (tf->errorcnt_run > 0)
      {
        ++errorcnt_run_files;
        errorcnt_run += tf->errorcnt_run;
      }
    }
  }

  // display the results
  print("Error tests executed:   {}\n", testcnt_err);
  print("Run tests executed:     {}\n", testcnt_run);
  print("Error tests failed:     {}", errorcnt_err);
  if (errorcnt_err) print(" ({} files)",  errorcnt_err_files);
  print("\n");
  print("Run tests failed:       {}", errorcnt_run);
  if (errorcnt_err) print(" ({} files)",  errorcnt_run_files);
  print("\n");

}

int OAtRunner::Run()
{
  if (!g_atropt)
  {
    return 1;
  }

  if (ATRMODE_BATCH == g_atropt->run_mode)
  {
    return RunBatch();
  }

  return RunSingle();
}

int OAtRunner::RunBatch()
{
  PrintBatchHeader();
  CollectTestFiles();
  ProcessBatchFiles();
  ProcessResults();

  return errorcnt_run + errorcnt_err;
}

int OAtRunner::RunSingle()
{
  return 0;
}
