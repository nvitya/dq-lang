/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    atrun_objects.h
 * authors: Codex
 * created: 2026-03-17
 * brief:   dqatrun object hierarchy draft
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "atr_options.h"
#include "processrunner.h"

using namespace std;

enum ETestVariantKind
{
  TVARK_ERROR = 0,
  TVARK_RUN   = 1
};

enum EDiagKind
{
  DIAG_ERROR   = 0,
  DIAG_WARNING = 1,
  DIAG_HINT    = 2
};

enum EDirectiveKind
{
  DIR_ERROR     = 0,
  DIR_WARNING   = 1,
  DIR_HINT      = 2,
  DIR_CHECK     = 3,
  DIR_CHECKERR  = 4,
  DIR_IGNORE    = 5,
  DIR_IGNOREERR = 6,
  DIR_EXIT      = 7
};

enum EOutputStream
{
  OSTREAM_STDOUT = 0,
  OSTREAM_STDERR = 1
};

enum EReportMark
{
  RPMARK_NONE       = 0,
  RPMARK_MISSING    = 1,
  RPMARK_UNEXPECTED = 2,
  RPMARK_NOTEQUAL   = 3,
  RPMARK_UNCHECKED  = 4
};

struct STextPos
{
  string            filename;
  int               line = 0;
  int               col = 0;
};

struct SDirectiveArg
{
  string            text;
  bool              quoted = false;
};

struct SDirective
{
  EDirectiveKind    kind = DIR_ERROR;
  STextPos          pos;
  vector<SDirectiveArg>  args;
  string            raw_text;
};

struct SDiagExpect
{
  EDiagKind         diag_kind = DIAG_ERROR;
  STextPos          pos;
  string            diag_id;
};

struct SRunExpect
{
  EDirectiveKind    kind = DIR_CHECK;
  EOutputStream     stream = OSTREAM_STDOUT;
  STextPos          pos;

  string            key;
  string            value;
  bool              keyed = false;
};

struct SCompilerDiag
{
  EDiagKind         diag_kind = DIAG_ERROR;
  STextPos          pos;
  string            diag_id;
  string            text;
  string            raw_line;
};

struct SOutputLine
{
  EOutputStream     stream = OSTREAM_STDOUT;
  string            text;

  bool              matched = false;
  EReportMark       report_mark = RPMARK_NONE;
  string            report_text;
};

struct SVariantStats
{
  int               failure_count = 0;
  int               matched_count = 0;
  int               unchecked_count = 0;
};

class OTestVariant
{
public:
  class OTestFile * testfile = nullptr;

  ETestVariantKind   variant_kind = TVARK_ERROR;
  string             variant_name;

  bool               enabled = false;
  bool               executed = false;
  bool               passed = false;

  string             compile_define;
  string             obj_filename;
  string             exe_filename;

  SProcessResult     compile_result;
  SProcessResult     run_result;

  vector<string>     report_lines;
  SVariantStats      stats;

public:
  OTestVariant();
  virtual ~OTestVariant();

  virtual void PrepareNames();
  virtual void Execute(const OAtrOptions & aoptions);
  virtual void Evaluate() = 0;
  virtual void FormatReport();
  virtual bool HasFailures();
  virtual bool NeedsRunStep();
};

class OErrorTestVariant : public OTestVariant
{
public:
  vector<SDiagExpect *>    error_expects;
  vector<SDiagExpect *>    warning_expects;
  vector<SDiagExpect *>    hint_expects;

  vector<SCompilerDiag>    compiler_diags;

public:
  OErrorTestVariant();
  virtual ~OErrorTestVariant();

  void PrepareNames() override;
  void Evaluate() override;
  void FormatReport() override;
};

class ORunTestVariant : public OTestVariant
{
public:
  vector<SRunExpect *>     stdout_checks;
  vector<SRunExpect *>     stderr_checks;
  vector<SRunExpect *>     stdout_ignores;
  vector<SRunExpect *>     stderr_ignores;

  vector<SOutputLine>      stdout_lines;
  vector<SOutputLine>      stderr_lines;

  int                      expected_exit_code = 0;
  bool                     has_exit_expect = false;

public:
  ORunTestVariant();
  virtual ~ORunTestVariant();

  void PrepareNames() override;
  void Evaluate() override;
  void FormatReport() override;
  bool NeedsRunStep() override;
};

class OTestFile
{
public:
  string            filename;
  string            filetext;

  vector<SDirective>    directives;
  vector<SDiagExpect>   diag_expects;
  vector<SRunExpect>    run_expects;

  OErrorTestVariant     error_test;
  ORunTestVariant       run_test;

  bool              loaded = false;
  bool              scanned = false;

public:
  OTestFile();
  virtual ~OTestFile();

  bool LoadFromFile(const string & afilename);
  bool Scan();
  void BuildVariants();
  void Execute(const OAtrOptions & aoptions);

  bool HasFailures();
  void RemoveTempFiles();
  void SaveAtrReport();

  void FormatSingleReport(vector<string> * adstlines, bool failures_only = false);
  void FormatBatchReport(vector<string> * adstlines);

protected:
  void AddDirective(EDirectiveKind akind, const STextPos & apos, const string & arawtext, const vector<SDirectiveArg> & aargs);
  bool ParseDirectiveLine(const string & aline, int alineidx);
};

class OProcessRunner;

class OBatchRun
{
public:
  OAtrOptions *           options = nullptr;

  vector<string>          test_filenames;
  vector<OTestFile *>     testfiles;

  vector<string>          header_lines;
  vector<string>          summary_lines;

  int                     error_tests_executed = 0;
  int                     run_tests_executed = 0;
  int                     error_failures = 0;
  int                     run_failures = 0;
  int                     error_failed_files = 0;
  int                     run_failed_files = 0;

public:
  OBatchRun();
  virtual ~OBatchRun();

  void DiscoverTests();
  void Execute();
  void FormatHeader();
  void FormatSummary();
};

class ODqAtrun
{
public:
  OBatchRun           batch_run;

public:
  ODqAtrun();
  virtual ~ODqAtrun();

  int Run(int argc, char ** argv);

protected:
  int RunSingle();
  int RunBatch();
};
