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

  if (g_atropt->verblevel >= VERBLEVEL_DEBUG)
  {
    print("Processing \"{}\"...\n", filename);
  }

  if (not LoadText())
  {
    AddTfErrorNoLine("File load error.");
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
    AddTfErrorNoLine("Neither run test markers nor error test markers were found.");
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
    if (!g_atropt->batchmode)
    {
      ShowErrResults();
    }
  }

  processed = true;
}

void OTestFile::ExecRunTest()
{
  exec_run = true;

  if (!g_atropt->batchmode)
  {
    print("Run test \"{}\"\n", filename);
  }

  // 1. Compile the test file: dq-comp <filename>, capture the compiler output to comp_output
  if (not ExecCompiler(false))
  {
    // error executing the compiler
    AddRunError(format("Error executing the compiler \"{}\"", g_atropt->compiler_filename));
    if (!g_atropt->batchmode)
    {
      print("COMPERR: {}\n", msg_run.back());
    }
    return;
  }

  if (comp_result != 0)
  {
    if (!g_atropt->batchmode and g_atropt->verblevel >= VERBLEVEL_INFO)
    {
      print("Compile error:\n");
      if (not comp_out.empty())
      {
        print("{}\n", comp_out);
      }
    }

    AddRunTestCompileErrors(comp_out);
    if (0 == errorcnt_run)
    {
      AddRunError(format("COMPERR: Compile error {}", comp_result));
    }
    return;
  }

  if (!g_atropt->batchmode and g_atropt->verblevel >= VERBLEVEL_INFO)
  {
    print("Compiler output:\n{}\n", comp_out);
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
      if (!g_atropt->batchmode)
      {
        print("{}\n", msg_run.back());
      }
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
  string sid;

  while (sp.readptr < sp.bufend)
  {
    if (not sp.ReadLine())
    {
      break;
    }

    curline = sp.PrevStr();
    spl.Init(curline.data(), curline.size());
    spl.SkipSpaces();

    bool waschecked = false;
    errstr = "";

    if (not curline.empty())
    {

      // 1. identifier = value ?

      bool id_and_value = false;
      char * idstart = spl.readptr;
      if (spl.ReadToChar('='))
      {
        sid = spl.PrevStr();

        spl.CheckSymbol("="); // consume
        spl.SkipSpaces();

        // remove the trailing spaces
        while (not sid.empty() and ((sid.back() == ' ') or (sid.back() == '\t')))
        {
          sid.pop_back();
        }

        if (not sid.empty())
        {
          id_and_value = true;
        }
      }

      if (not id_and_value) // rewind the line parser
      {
        spl.readptr = spl.bufstart;
        spl.SkipSpaces();
      }

      // run checks
      for (ORunCapture * cap : run_captures)
      {
        if (id_and_value)
        {
          if (cap->strid == sid)
          {
            if (cap->captured)
            {
              errstr = "already captured";
            }
            else if (not spl.CheckSymbol(cap->checkvalue.c_str()))
            {
              errstr = format("!= {}", cap->checkvalue);
            }

            cap->captured = true;
            waschecked = true;
            break;
          }
        }
        else if (cap->checkvalue.empty() and spl.CheckSymbol(cap->strid.c_str()))
        {
          waschecked = true;
          break;
        }
      }

      if (not waschecked)
      {
        errstr = "unchecked";
      }
    }

    outline = format("{:<40} ` {}", curline, errstr);
    if (!g_atropt->batchmode)
    {
      print("{}\n", outline);
    }
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
      if (!g_atropt->batchmode)
      {
        print("{}\n", outline);
      }
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
  exec_err = true;

  if (!g_atropt->batchmode)
  {
    print("Error test \"{}\"\n", filename);
  }

  // 1. Compile the test file: dq-comp <filename> -DERRTEST, capture the compiler output to comp_output
  if (not ExecCompiler(true))
  {
    // error executing the compiler
    AddEtError(format("Error executing the compiler {}", g_atropt->compiler_filename));
    if (!g_atropt->batchmode)
    {
      print("COMPERR: {}\n", msg_err.back());
    }
    return;
  }

  if (!g_atropt->batchmode and g_atropt->verblevel >= VERBLEVEL_DEBUG)
  {
    if (not comp_out.empty())
    {
      print("Compiler output:\n{}\n", comp_out);
    }
  }

  AnalyzeErrOutput();
}

void OTestFile::AnalyzeErrOutput()
{
  PrintSeparator();

  // examples:
  //   dq_array.dq(64,38) ERROR(ArrSize): Array size mismatch: expected 3, got 4
  //   dq_array.dq(64,38) ERROR(Semicolon): ";" is missing to close the assignment statement
  //   dq_array.dq(65,1) ERROR(FuncResultNotSet): Function "dq_arr_fixed_test_error" result is not set

  sp.Init(comp_out.data(), comp_out.size());

  string errstr;
  string outline;

  string sid;
  string errid;

  string fname;
  int    linenum;
  int    colnum;

  while (sp.readptr < sp.bufend)
  {
    sp.SkipSpaces();
    if (not sp.ReadLine())
    {
      break;
    }

    curline = sp.PrevStr();
    spl.Init(curline.data(), curline.size());
    spl.SkipSpaces();

    bool waschecked = false;
    errstr = "";

    if (curline.empty())
    {
      continue;
    }

    // capture the file name, linenum, colnum first

    bool bok = false;
    if (spl.ReadToChar('('))
    {
      fname = spl.PrevStr();
      spl.CheckSymbol("("); // consume
      if (spl.ReadDecimalNumbers())
      {
        linenum = spl.PrevToInt();
        if (spl.CheckSymbol(","))
        {
          if (spl.ReadDecimalNumbers())
          {
            colnum = spl.PrevToInt();
            if (spl.CheckSymbol(")"))
            {
              bok = true;
            }
          }
        }
      }
    }

    if (bok) // filename, linenum, colnum captured properly ?
    {
      bok = false;

      spl.SkipSpaces();

      // ERROR, WARNING, HINT
      if (spl.ReadIdentifier(sid))
      {
        if (spl.CheckSymbol("("))
        {
          if (spl.ReadIdentifier(errid))
          {
            if (spl.CheckSymbol("):"))
            {
              bok = true;
            }
          }
        }
      }
    }

    if (not bok)
    {
      // invalid compiler output line
      AddEtError(format("INVALID_COMP_OUT: {}", curline));
      if (!g_atropt->batchmode)
      {
        print("{}\n", msg_err.back());
      }
    }
    else
    {
      PrintMissingErrors(linenum);

      // check the error message for matches

      for (OErrCapture * cap : err_captures)
      {
        if ((sid == cap->msgtype) and (cap->errid == errid) and (cap->line == linenum))
        {
          cap->captured = true;
          waschecked = true;
          break;
        }
      }

      if (not waschecked)
      {
        AddEtError(format("UNCHECKED: {}", curline));
        if (!g_atropt->batchmode)
        {
          print("{}\n", msg_err.back());
        }
      }
    }
  } // while all lines

  PrintMissingErrors(1000000);

  PrintSeparator();
}

void OTestFile::PrintMissingErrors(int alinenum)
{
  // write missing captures
  for (OErrCapture * cap : err_captures)
  {
    if (not cap->captured and not cap->missing_printed and cap->line < alinenum)
    {
      AddEtError(format("MISSING: {}({}): {}({})", filename, cap->line, cap->msgtype, cap->errid));
      if (!g_atropt->batchmode)
      {
        print("{}\n", msg_err.back());
      }
      cap->missing_printed = true;
    }
  }
}

void OTestFile::ShowErrResults()
{
#if 0
  for (string s : msg_run)
  {
    print("RUNERR: {}\n", s);
  }
#endif


  if (0 == errorcnt_err)
  {
    print("Error test PASSED.\n");
  }
  else
  {
    print("Error test FAILED: {} failures detected.\n", msg_err.size());
  }
  print("\n");
}

void OTestFile::PrintSeparator()
{
  if (!g_atropt->batchmode)
  {
    print("-------------------------------------------------------------------------------\n");
  }
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
    sp.SkipSpaces(false);

    while (sp.ReadIdentifier(sid))
    {
      // error test markers
      if ("error" == sid)
      {
        ParseMarkerError("ERROR");
      }
      else if ("warning" == sid)
      {
        ParseMarkerError("WARNING");
      }
      else if ("hint" == sid)
      {
        ParseMarkerError("HINT");
      }

      // run test markers
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
        break;
      }

      sp.SkipSpaces(false);
      if (sp.CheckSymbol(","))
      {
        sp.SkipSpaces(false);
        continue;
      }

      break;
    }
  }

  return (0 == errorcnt_tf);
}

void OTestFile::ParseMarkerError(const string amsgid)
{
  // sample: //?error(TypeSpecExpected)
  // note the "//?error" is already consumed

  int errline = sp.GetLineNum();

  sp.SkipSpaces(false);
  if (not sp.CheckSymbol("("))
  {
    AddTfError(format("\"(\" is missing after \"//?error\""));
    return;
  }
  sp.SkipSpaces(false);
  string errid;
  if (not sp.ReadIdentifier(errid))
  {
    AddTfError(format("Error id is missing after \"//?error\""));
    return;
  }

  sp.SkipSpaces(false);
  if (not sp.CheckSymbol(")"))
  {
    AddTfError(format("\")\" is missing after \"//?error\""));
    return;
  }

  err_captures.push_back(new OErrCapture(errline, amsgid, errid));

}

void OTestFile::ParseMarkerCheck(bool aignore)
{
  // sample: printf("Hello2=5\n");   //?check(Hello2, 5)
  // note "//?check" is already consumed

  string cmd = (aignore ? "ignore" : "check");


  sp.SkipSpaces(false);
  if (not sp.CheckSymbol("("))
  {
    AddTfError(format("\"(\" is missing after \"//?{}\"", cmd));
    return;
  }
  sp.SkipSpaces(false);
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
  sp.SkipSpaces(false);
  if (sp.CheckSymbol(","))
  {
    sp.SkipSpaces(false);
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

void OTestFile::AddTfErrorNoLine(const string astr)
{
  msg_tf.push_back(astr);
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
  comp_out = procrunner.stdout_text + "\n" + procrunner.stderr_text;
  return result;
}
