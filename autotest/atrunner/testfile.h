/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
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
  string            errid;

  OErrCapture(int aline, const string aerrid)
  :
    line(aline),
    errid(aerrid)
  {}
};

class ORunCapture
{
public:
  string            strid;
  string            checkvalue;

  ORunCapture(const string astrid, const string acheckvalue)
  :
    strid(astrid),
    checkvalue(acheckvalue)
  {}
};


class OTestFile
{
public:
  string            filename;
  string            text;

  string            comp_stdout;
  string            comp_stderr;
  int               comp_result;

  string            run_output;

  bool              processed  = false;

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

  OProcessRunner    procrunner;

public:
  OTestFile(const string & afilename);
  virtual ~OTestFile();

  void Process();
  void ExecRunTest();
  void ShowRunResults();
  void ExecErrorTest();  // check the expected compiler errors

  void AddRunError(const string astr);
  void AddEtError(const string astr);

protected:
  bool LoadText();
  bool ParseText();
  void ParseMarkerError();
  void ParseMarkerCheck();

  void AddTfError(const string astr);

protected:
  bool ExecCompiler(bool errmode);
};
