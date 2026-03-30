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
  procrunner.exec_timeout_ms = 5000; // compile or run should finish in 5s

  if (!g_atropt->batchmode and g_atropt->verblevel >= VERBLEVEL_DEBUG)
  {
    print("Processing \"{}\"...\n", filename);
  }

  if (not LoadText())
  {
    msg_tf.push_back("File load error.");
    if (!g_atropt->batchmode)
    {
      print("TF_ERROR: {}\n", msg_tf.back());
    }
    processed = true;
    return;
  }

  if (not ParseText())
  {
    ShowTestFileErrors();
    processed = true;
    return;
  }

  if (run_captures.empty() and err_captures.empty())
  {
    // no atr marker was found
    msg_tf.push_back("Neither run test markers nor error test markers were found.");
    if (!g_atropt->batchmode)
    {
      print("TF_ERROR: {}\n", msg_tf.back());
    }
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
  if (!g_atropt->batchmode)
  {
    print("Run test \"{}\"\n", filename);
  }

  // 1. Compile the test file: dq-comp <filename>, capture the compiler output to comp_output
  if (not ExecCompiler(false))
  {
    // error executing the compiler
    AddRunError(format("Error executing the compiler {}", g_atropt->compiler_filename));
    return;
  }

  if (comp_result != 0)
  {
    if (!g_atropt->batchmode and g_atropt->verblevel >= VERBLEVEL_STATUS)
    {
      print("Compile error:\n");
      if (not comp_stdout.empty())
      {
        print("{}\n", comp_stdout);
      }
      if (not comp_stderr.empty())
      {
        print("{}\n", comp_stderr);
      }
    }

    AddRunTestCompileErrors(comp_stdout);
    AddRunTestCompileErrors(comp_stderr);
    if (0 == errorcnt_run)
    {
      AddRunError(format("COMPERR: Compile error {}", comp_result));
    }
    return;
  }

  if (!g_atropt->batchmode and g_atropt->verblevel >= VERBLEVEL_INFO)
  {
    print("Compiler output:\n{}\n", comp_stdout);
  }

  // 2. Executing the compiled test file
  string exename = fs::path(filename).replace_extension("exe").generic_string();
  #ifndef _WIN32
    // adding "./" to the front for local files
    if (not exename.empty() and exename[0] != '/' and exename.find('/') == string::npos)
    {
      exename.insert(0, "./");
    }
  #endif

  if (!g_atropt->batchmode and g_atropt->verblevel >= VERBLEVEL_DEBUG)
  {
    print("Executing \"{}\"\n", exename);
  }
  procrunner.args = { exename };
  if (not procrunner.Run())
  {
    AddRunError(format("Error executing \"{}\"", exename));
    if (!g_atropt->batchmode)
    {
      print("{}\n", msg_run.back());
    }
    return;
  }

  run_output = procrunner.stdout_text;

  if (!g_atropt->batchmode and g_atropt->verblevel >= VERBLEVEL_DEBUG)
  {
    print("Run output:\n{}\n", run_output);
  }

  AnalyzeRunOutput();
}

void OTestFile::AddRunTestCompileErrors(string & astr)
{
  // add the compile errors to the msg_run

  sp.Init(astr.data(), astr.size());
  sp.SkipSpaces(); // go to the first non-space

  while (sp.readptr < sp.bufend)
  {
    if (not sp.ReadLine())
    {
      break;
    }

    curline = sp.PrevStr();
    if (not curline.empty())
    {
      AddRunError("COMPERR: "+curline);
    }
  }

}

void OTestFile::AnalyzeRunOutput()
{
  PrintSeparator();

  sp.Init(run_output.data(), run_output.size());
  sp.SkipSpaces(); // go to the first non-space

  string errstr;
  string outline;

  while (sp.readptr < sp.bufend)
  {
    if (not sp.ReadLine())
    {
      break;
    }

    curline = sp.PrevStr();
    spl.Init(curline.data(), curline.size());

    bool waschecked = false;
    errstr = "";

    if (not curline.empty())
    {
      // run checks
      for (ORunCapture * cap : run_captures)
      {
        if (spl.CheckSymbol(cap->strid.c_str()))
        {
          waschecked = true;

          if (cap->checkvalue.empty())  // empty value or ignore
          {
            break;
          }

          if (cap->captured)
          {
            errstr = "already captured";
            break;
          }

          spl.SkipSpaces(false);
          if (spl.CheckSymbol("="))
          {
            spl.SkipSpaces(false);
          }

          cap->captured = true;

          // check against the value
          if (not spl.CheckSymbol(cap->checkvalue.c_str()))
          {
            errstr = format("!= {}", cap->checkvalue);
          }

          break;
        }
      }

      if (not waschecked)
      {
        errstr = "unchecked";
      }
    }

    outline = format("{:<40} ` {}", curline, errstr);
    print("{}\n", outline);
    if (not errstr.empty())
    {
      AddRunError(outline);
    }

  } // while all lines

  // write missing captures
  for (ORunCapture * cap : run_captures)
  {
    if (not cap->captured and not cap->checkvalue.empty())
    {
      outline = format("{:<40} ` missing: {} = {}", "", cap->strid, cap->checkvalue);
      print("{}\n", outline);
      AddRunError(outline);
    }
  }

  PrintSeparator();
}

void OTestFile::ShowRunResults()
{
#if 0
  for (string s : msg_run)
  {
    print("RUNERR: {}\n", s);
  }
#endif


  if (0 == errorcnt_run)
  {
    print("Run test PASSED.\n");
  }
  else
  {
    print("Run test FAILED: {} failures detected.\n", msg_run.size());
  }
  print("\n");
}

void OTestFile::ShowTestFileErrors()
{
  for (string s : msg_tf)
  {
    print("TF_ERROR: {}\n", s);
  }
}

void OTestFile::ExecErrorTest()
{
  if (!g_atropt->batchmode and g_atropt->verblevel >= VERBLEVEL_STATUS)
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

  if (!g_atropt->batchmode and g_atropt->verblevel >= VERBLEVEL_INFO)
  {
    print("Compiler output:\n{}\n", comp_stdout);
  }

}

void OTestFile::PrintSeparator()
{
  print("-------------------------------------------------------------------------------\n");
}

void OTestFile::AddRunError(const string astr)
{
  msg_run.push_back(astr);
  ++errorcnt_run;
}

void OTestFile::AddRunLineError(const string astr)
{
  AddRunError(format("{} ` {}", curline, astr));
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
      ParseMarkerCheck(false);
    }
    else if ("ignore" == sid)
    {
      ParseMarkerCheck(true);
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

void OTestFile::ParseMarkerCheck(bool aignore)
{
  // sample: printf("Hello2=5\n");   //?check(Hello2, 5)
  // note "//?check" is already consumed

  string cmd = (aignore ? "ignore" : "check");


  sp.SkipSpaces();
  if (not sp.CheckSymbol("("))
  {
    AddTfError(format("\"(\" is missing after \"//?{}\"", cmd));
    return;
  }
  sp.SkipSpaces();
  string strid;
  if (sp.ReadQuotedString())
  {
    strid = sp.PrevStr();
  }
  else if (not sp.ReadIdentifier(strid))
  {
    AddTfError(format("Id is missing after \"//?{}\"", cmd));
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
        AddTfError(format("\")\" is missing after \"//?{}\"", cmd));
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
      AddTfError(format("\")\" is missing after \"//?{}\"", cmd));
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

  string exename = fs::path(filename).replace_extension("exe").generic_string();

  procrunner.args = { g_atropt->compiler_filename, filename, "-o", exename };
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
