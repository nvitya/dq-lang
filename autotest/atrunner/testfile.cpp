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
#include <print>
#include <format>
#include <fstream>
#include <filesystem>

#include "atr_options.h"

namespace fs = std::filesystem;

OTestFile::OTestFile(const string & afilename)
{
  filename = afilename;
}

OTestFile::~OTestFile()
{
}

void OTestFile::Process()
{
  if (!g_atropt->batchmode and g_atropt->verbose)
  {
    print("Processing \"{}\"...\n", filename);
  }

  if (not LoadText())
  {
    msg_tf.push_back("File load error.");
    processed = true;
    return;
  }

  if (not ParseText())
  {
    processed = true;
    return;
  }

  if (not run_captures.empty())
  {
    ExecRunTest();
    if (!g_atropt->batchmode)
    {
      ShowRunResults();
    }
  }

  if (not err_captures.empty())
  {
    ExecErrorTest();
  }

  processed = true;
}

void OTestFile::ExecRunTest()
{
  if (!g_atropt->batchmode and g_atropt->verbose)
  {
    print("Executing run test...\n");
  }

  // 1. Compile the test file: dq-comp <filename>, capture the compiler output to comp_output
  if (not ExecCompiler(false))
  {
    // error executing the compiler
    AddRunError(format("Error executing the compiler {}", g_atropt->compiler_filename));
    return;
  }

  if (!g_atropt->batchmode and g_atropt->verbose)
  {
    print("Compiler output:\n{}\n", comp_stdout);
  }

  // 2. Executing the compiled test file
  string exename = fs::path(filename).replace_extension("").generic_string();
  #ifdef _WIN32
    exename += ".exe";
  #endif

  if (!g_atropt->batchmode and g_atropt->verbose)
  {
    print("Running {}...\n", exename);
  }
  procrunner.args = { exename };
  if (not procrunner.Run())
  {
    AddRunError(format("Error executing {}", exename));
    return;
  }

  if (!g_atropt->batchmode and g_atropt->verbose)
  {
    print("Run output:\n{}\n", procrunner.stdout_text);
  }

}

void OTestFile::ShowRunResults()
{
  for (string s : msg_run)
  {
    print("RUNERR: {}\n", s);
  }

  if (0 == errorcnt_run)
  {
    print("Run test result: OK\n");
  }
  else
  {
    print("Run test result: FAILED\n");
  }
  print("\n");
}

void OTestFile::ExecErrorTest()
{
  if (!g_atropt->batchmode and g_atropt->verbose)
  {
    print("Executing error test...\n");
  }

  // 1. Compile the test file: dq-comp <filename> -DERRTEST, capture the compiler output to comp_output
  if (not ExecCompiler(true))
  {
    // error executing the compiler
    AddEtError(format("Error executing the compiler {}", g_atropt->compiler_filename));
    return;
  }

  if (!g_atropt->batchmode and g_atropt->verbose)
  {
    print("Compiler output:\n{}\n", comp_stdout);
  }

}

void OTestFile::AddRunError(const string astr)
{
  msg_run.push_back(astr);
  ++errorcnt_run;
}

void OTestFile::AddEtError(const string astr)
{
  msg_err.push_back(astr);
  ++errorcnt_err;
}

bool OTestFile::LoadText()
{
  ifstream f(filename, ios::binary | ios::ate);
  if (!f)
  {
    return false;
  }

  int length = f.tellg();
  text.resize(length);
  if (length > 0)
  {
    f.seekg(0);
    f.read(text.data(), length);
  }

  return true;
}

bool OTestFile::ParseText()
{
  sp.Init(text.data(), text.size());

  string sid;

  // find all markers
  while (sp.readptr < sp.bufend)
  {
    if (not sp.SearchPattern("//?"))
    {
      break; // no more markers
    }

    sp.CheckSymbol("//?"); // the searchpatten does not consume the pattern itself, so do it now
    sp.SkipSpaces();

    if (!sp.ReadIdentifier(sid))
    {
      AddTfError("Identifier expected after \"//?\"");
    }

    if ("error" == sid)
    {
      ParseMarkerError();
    }
    else if ("check" == sid)
    {
      ParseMarkerCheck();
    }
    else
    {
      AddTfError(format("Unknown marker \"{}\"", sid));
    }
  }

  return (0 == errorcnt_tf);
}

void OTestFile::ParseMarkerError()
{
  // sample: //?error(TypeSpecExpected)
  // note the "//?error" is already consumed

  int errline = sp.GetLineNum();

  sp.SkipSpaces();
  if (not sp.CheckSymbol("("))
  {
    AddTfError(format("\"(\" is missing after \"//?error\""));
    return;
  }
  sp.SkipSpaces();
  string errid;
  if (not sp.ReadIdentifier(errid))
  {
    AddTfError(format("Error id is missing after \"//?error\""));
    return;
  }

  sp.SkipSpaces();
  if (not sp.CheckSymbol(")"))
  {
    AddTfError(format("\")\" is missing after \"//?error\""));
    return;
  }

  err_captures.push_back(new OErrCapture(errline, errid));

}

void OTestFile::ParseMarkerCheck()
{
  // sample: printf("Hello2=5\n");   //?check(Hello2, 5)
  // note "//?check" is already consumed

  sp.SkipSpaces();
  if (not sp.CheckSymbol("("))
  {
    AddTfError(format("\"(\" is missing after \"//?check\""));
    return;
  }
  sp.SkipSpaces();
  string strid;
  if (not sp.ReadIdentifier(strid))
  {
    AddTfError(format("Id is missing after \"//?check\""));
    return;
  }

  string sv = "";
  sp.SkipSpaces();
  if (sp.CheckSymbol(","))
  {
    sp.SkipSpaces();
    if (sp.ReadQuotedString())
    {
      sv = sp.PrevStr();
    }
    else
    {
      if (not sp.ReadToChar(')'))
      {
        AddTfError(format("\")\" is missing after \"//?check\""));
        return;
      }

      sv = sp.PrevStr();
      // remove the trailing spaces
      auto pos = sv.find_last_not_of(" \t\n\r\f\v");
      sv.erase(pos == std::string::npos ? 0 : pos + 1);
    }
  }
  else
  {
    if (not sp.CheckSymbol(")"))
    {
      AddTfError(format("\")\" is missing after \"//?check\""));
      return;
    }
  }

  run_captures.push_back(new ORunCapture(strid, sv));
}

void OTestFile::AddTfError(const string astr)
{
  int linenum = sp.GetLineNum(sp.prevptr);
  msg_tf.push_back(format("line {}: {}", linenum, astr));
  ++errorcnt_tf;
}

bool OTestFile::ExecCompiler(bool errmode)
{
  bool result = true;

  procrunner.args = { g_atropt->compiler_filename, filename };
  if (errmode)
  {
    procrunner.args.push_back("-DERRORTEST");
  }

  if (!procrunner.Run())
  {
    result = false;
  }

  comp_result = procrunner.exit_code;
  comp_stdout = procrunner.stdout_text;
  comp_stderr = procrunner.stderr_text;
  return result;
}
