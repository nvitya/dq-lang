/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
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

  procrunner.args = { g_atropt->compiler_filename, "--version" };
  if (!procrunner.Run())
  {
    return "?";
  }

  if (!procrunner.stdout_text.empty())
  {
    return TrimLineEnd(procrunner.stdout_text);
  }

  if (!procrunner.stderr_text.empty())
  {
    return TrimLineEnd(procrunner.stderr_text);
  }

  return "?";
}

static void PrintBatchHeader()
{
  print("DQ Autotest v{}\n", ATR_VERSION);
  print("Compiler:  {}\n", g_atropt->compiler_filename);
  print("C. ver.:   v{}\n", QueryCompilerVersion());
  if (g_atropt->optlevel >= 0)
  {
    print("C. opt.:   -O{}\n", g_atropt->optlevel);
  }
  print("Test root: {}\n", g_atropt->test_root);
  print("\n");
}

static int DetectWorkerCount()
{
  unsigned thread_count = thread::hardware_concurrency();
  if (thread_count < 1)
  {
    return 1;
  }

  return static_cast<int>(thread_count);
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
    if ((".dq" != p.extension().string()) && (".dqproj" != p.extension().string()))
    {
      continue;
    }

    foundfiles.push_back(fs::relative(p, rootpath));
  }

  sort(foundfiles.begin(), foundfiles.end());

  for (const fs::path & rp : foundfiles)
  {
    testfiles.push_back(new OTestFile((rootpath / rp).generic_string()));
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

  for (int i = 0; i < used_worker_count; ++i)
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

void OAtRunner::ProcessBatchFilesParallel()
{
  StartWorkers();

  // assign all the work to workers, keep the workers always busy

  size_t fileidx = 0;
  while (fileidx < testfiles.size())
  {
    bool allbusy = true;

    for (OTestFileWorker * worker : workers)
    {
      if (worker->IsIdle())
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
      if (not worker->IsIdle())
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

void OAtRunner::ProcessBatchFilesSequential()
{
  for (OTestFile * tf : testfiles)
  {
    tf->Process();
    tf->processed = true;
  }
}

void OAtRunner::ProcessResults()
{
  // collect the results

  for (OTestFile * tf : testfiles)
  {
    if (tf->errorcnt_tf)
    {
      for (string s : tf->msg_tf)
      {
        print("INV({}): {}\n", tf->filename, s);
      }
      ++invalid_tf_cnt;
    }
  }
  if (invalid_tf_cnt)
  {
    print("\n");
  }

  for (OTestFile * tf : testfiles)
  {
    if (tf->exec_build)
    {
      ++testcnt_build;
      if (tf->errorcnt_build > 0)
      {
        ++errorcnt_build_files;
        errorcnt_build += tf->errorcnt_build;

        for (const string & message : tf->msg_build)
        {
          print("BLD({}): {}\n", tf->filename, message);
        }
      }
    }
  }
  if (errorcnt_build)
  {
    print("\n");
  }

  for (OTestFile * tf : testfiles)
  {
    if (tf->exec_err)
    {
      ++testcnt_err;
      if (tf->errorcnt_err > 0)
      {
        ++errorcnt_err_files;
        errorcnt_err += tf->errorcnt_err;

        for (string s : tf->msg_err)
        {
          print("ERR({}): {}\n", tf->filename, s);
        }
      }
    }
  }
  if (errorcnt_err)
  {
    print("\n");
  }

  for (OTestFile * tf : testfiles)
  {
    if (tf->exec_run)
    {
      ++testcnt_run;
      if (tf->errorcnt_run > 0)
      {
        ++errorcnt_run_files;
        errorcnt_run += tf->errorcnt_run;

        for (string s : tf->msg_run)
        {
          print("RUN({}): {}\n", tf->filename, s);
        }
      }
    }
  }
  if (errorcnt_run)
  {
    print("\n");
  }


  // display the results
  print("Build tests executed: {:3}\n", testcnt_build);
  print("Error tests executed: {:3}\n", testcnt_err);
  print("Run tests executed:   {:3}\n", testcnt_run);
  print("Build tests fails:    {:3}",   errorcnt_build);
  if (errorcnt_build) print(" ({} files)",  errorcnt_build_files);
  print("\n");
  print("Error tests fails:    {:3}",   errorcnt_err);
  if (errorcnt_err) print(" ({} files)",  errorcnt_err_files);
  print("\n");
  print("Run tests fails:      {:3}",   errorcnt_run);
  if (errorcnt_run) print(" ({} files)",  errorcnt_run_files);
  print("\n");

  if (invalid_tf_cnt > 0)
  {
    print("Invalid test files:   {:3}\n",   invalid_tf_cnt);
  }
}

int OAtRunner::Run()
{
  if (!g_atropt)
  {
    return 1;
  }

  if (g_atropt->clean)
  {
    int result = Clean();
    if (result)
    {
      return result;
    }
  }

  if (g_atropt->clean_only)
  {
    return 0;
  }

  if (g_atropt->batchmode)
  {
    return RunBatch();
  }
  else
  {
    return RunSingle();
  }
}

int OAtRunner::Clean()
{
  fs::path rootpath(g_atropt->test_root);
  error_code ec;
  if (!fs::exists(rootpath, ec) or ec)
  {
    print("The test root \"{}\" does not exist\n", rootpath.generic_string());
    return 1;
  }

  int error_count = 0;
  fs::recursive_directory_iterator iter(rootpath, fs::directory_options::skip_permission_denied, ec);
  fs::recursive_directory_iterator end;
  while (iter != end)
  {
    if (ec)
    {
      print("Cannot inspect test artifact \"{}\": {}\n", iter->path().generic_string(), ec.message());
      ++error_count;
      ec.clear();
      iter.increment(ec);
      continue;
    }

    const fs::directory_entry & entry = *iter;
    const fs::path path = entry.path();

    if (entry.is_directory(ec) and (".dqbuild" == path.filename().string()))
    {
      iter.disable_recursion_pending();
      fs::remove_all(path, ec);
    }
    else if (entry.is_regular_file(ec))
    {
      const string extension = path.extension().string();
      if ((".exe" == extension) or (".o" == extension) or (".dqm_if" == extension) or (".atr" == extension))
      {
        fs::remove(path, ec);
      }
    }

    if (ec)
    {
      print("Cannot remove test artifact \"{}\": {}\n", path.generic_string(), ec.message());
      ++error_count;
      ec.clear();
    }

    iter.increment(ec);
  }

  return error_count;
}

int OAtRunner::RunBatch()
{
  PrintBatchHeader();
  CollectTestFiles();

  used_worker_count = g_atropt->worker_count;
  if (used_worker_count < 1)
  {
    used_worker_count = DetectWorkerCount();
  }

  if (used_worker_count > 1)
  {
    ProcessBatchFilesParallel();
  }
  else
  {
    ProcessBatchFilesSequential();
  }

  ProcessResults();

  return errorcnt_build + errorcnt_run + errorcnt_err + invalid_tf_cnt;
}

int OAtRunner::RunSingle()
{
  for (OTestFile * tf : testfiles)
  {
    delete tf;
  }
  testfiles.clear();

  if (!fs::exists(g_atropt->single_test_filename))
  {
    print("The test file \"{}\" does not exist\n", g_atropt->single_test_filename);
    return 1;
  }

  OTestFile * tf = new OTestFile(g_atropt->single_test_filename);
  tf->Process();
  tf->processed = true;
  int result = tf->errorcnt_build + tf->errorcnt_err + tf->errorcnt_run + tf->errorcnt_tf;



  delete tf;
  return result;
}
