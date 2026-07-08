/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    testfile.h
 * authors: Codex
 * created: 2026-03-17
 * brief:   autotest file object
 */

#pragma once

#include <string>
#include <vector>
#include "strparse.h"
#include "processrunner.h"

using namespace std;

class OErrCapture
{
public:
  int               line;
  string            msgtype;  // ERROR/WARNING/HINT
  string            errid;
  string            msg_contains;
  bool              captured = false;
  bool              missing_printed = false;

  OErrCapture(int aline, const string amsgtype, const string aerrid, const string amsg_contains)
  :
    line(aline),
    msgtype(amsgtype),
    errid(aerrid),
    msg_contains(amsg_contains)
  {}
};

class ORunCapture
{
public:
  string            strid;
  string            checkvalue;
  bool              ignore = false;
  bool              captured = false;

  ORunCapture(const string astrid, const string acheckvalue, bool aignore)
  :
    strid(astrid),
    checkvalue(acheckvalue),
    ignore(aignore)
  {}
};


class OTestFile
{
public:
  string            filename;
  string            text;

  string            comp_out;
  int               comp_result;

  string            run_output;
  string            curline;

  bool              processed  = false;

  bool              skip_test  = false;
  bool              exec_run   = false;
  bool              exec_err   = false;

  int               errorcnt_err = 0;
  int               errorcnt_run = 0;
  int               errorcnt_tf  = 0;

  vector<OErrCapture *>  err_captures;
  vector<ORunCapture *>  run_captures;

  vector<string>    msg_err;
  vector<string>    msg_run;
  vector<string>    msg_tf;

  TStrParseObj      sp;
  TStrParseObj      spl;  // line parser

  OProcessRunner    procrunner;

public:
  OTestFile(const string & afilename);
  virtual ~OTestFile();

  void Process();
  void ExecRunTest();
  void AddRunTestCompileErrors(string & astr);
  void AnalyzeRunOutput();
  void ShowRunResults();

  void ShowTestFileErrors();
  void ExecErrorTest();  // check the expected compiler errors
  void AnalyzeErrOutput();
  void PrintMissingErrors(int alinenum);
  void ShowErrResults();

  void PrintSeparator();
  void AddRunError(const string astr);
  void AddRunLineError(const string astr);

  void AddEtError(const string astr);

protected:
  bool LoadText();
  bool ParseText();
  void ParseMarkerError(const string amsgid);
  void ParseMarkerCheck(bool aignore);

  void AddTfError(const string astr);
  void AddTfErrorNoLine(const string astr);

protected:
  bool ExecCompiler(bool errmode);
};
