/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    testfile.cpp
 * authors: Codex
 * created: 2026-03-17
 * brief:
 */

#include "testfile.h"

#include <cstdlib>
#include <print>
#include <format>
#include <fstream>
#include <filesystem>

#include "atr_options.h"

namespace fs = std::filesystem;

static string HostArchitecture()
{
#if defined(__x86_64__) || defined(_M_X64)
  return "x64";
#elif defined(__aarch64__) || defined(_M_ARM64)
  return "arm64";
#else
  return "unknown";
#endif
}

static string AbsolutePath(const fs::path & path)
{
  error_code ec;
  fs::path result = fs::absolute(path, ec);
  return (ec ? path : result.lexically_normal()).generic_string();
}

static string ResolveExecutablePath(const string & executable)
{
  fs::path candidate(executable);
  if (candidate.has_parent_path() or fs::exists(candidate))
  {
    return AbsolutePath(candidate);
  }

  const char * path_env = getenv("PATH");
  if (path_env)
  {
    string path_list(path_env);
    size_t start = 0;
    while (start <= path_list.size())
    {
      size_t end = path_list.find(':', start);
      fs::path directory = path_list.substr(start, end - start);
      fs::path path = (directory.empty() ? fs::path(".") : directory) / executable;
      if (fs::exists(path))
      {
        return AbsolutePath(path);
      }
      if (string::npos == end)
      {
        break;
      }
      start = end + 1;
    }
  }

  return executable;
}

OTestFile::OTestFile(const string & afilename)
{
  filename = afilename;
}

OTestFile::~OTestFile()
{
  for (OErrCapture * capture : err_captures)
  {
    delete capture;
  }
  for (ORunCapture * capture : run_captures)
  {
    delete capture;
  }
}

void OTestFile::Process()
{
  procrunner.exec_timeout_ms = 30000;

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

  if ((not ParseText()) or (not ValidateMarkers()))
  {
    ShowTestFileErrors();
    processed = true;
    return;
  }

  if (skip_test)
  {
    if (!g_atropt->batchmode and g_atropt->verblevel >= VERBLEVEL_INFO)
    {
      print("Skipping test \"{}\"\n", filename);
    }
    processed = true;
    return;
  }

  if ((not build_only) and (not HasRunTest()) and err_captures.empty())
  {
    // no atr marker was found
    AddTfErrorNoLine("Neither build, run, nor error test markers were found.");
    if (!g_atropt->batchmode)
    {
      print("TF_ERROR: {}\n", msg_tf.back());
    }
    processed = true;
    return;
  }

  if (build_only)
  {
    ExecBuildTest();
    if (!g_atropt->batchmode)
    {
      ShowBuildResults();
    }
  }

  if (HasRunTest())
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

void OTestFile::ExecBuildTest()
{
  exec_build = true;

  if (!g_atropt->batchmode)
  {
    print("Build-only test \"{}\"\n", filename);
  }

  if (not ExecCompiler(false))
  {
    AddBuildError(format("Error executing the compiler \"{}\"", g_atropt->compiler_filename));
    AddProcessFailureDetails(msg_build, true);
    return;
  }

  if (comp_result != 0)
  {
    AddCompilerOutputErrors(comp_out, msg_build, errorcnt_build);
    if (0 == errorcnt_build)
    {
      AddBuildError(format("COMPERR: Compile error {}", comp_result));
    }
  }
}

void OTestFile::ShowBuildResults()
{
  if (0 == errorcnt_build)
  {
    print("Build-only test PASSED.\n");
  }
  else
  {
    for (const string & message : msg_build)
    {
      print("{}\n", message);
    }
    print("Build-only test FAILED: {} failures detected.\n", errorcnt_build);
  }
  print("\n");
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
    size_t previous_message_count = msg_run.size();
    AddRunError(format("Error executing the compiler \"{}\"", g_atropt->compiler_filename));
    AddProcessFailureDetails(msg_run, true);
    if (!g_atropt->batchmode)
    {
      for (size_t i = previous_message_count; i < msg_run.size(); ++i)
      {
        print("COMPERR: {}\n", msg_run[i]);
      }
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

    size_t previous_message_count = msg_run.size();
    AddCompilerOutputErrors(comp_out, msg_run, errorcnt_run);
    if (0 == errorcnt_run)
    {
      AddRunError(format("COMPERR: Compile error {}", comp_result));
    }
    if (!g_atropt->batchmode)
    {
      for (size_t i = previous_message_count; i < msg_run.size(); ++i)
      {
        print("{}\n", msg_run[i]);
      }
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
  ConfigureRunEnvironment();
  // Driver tests can launch several compiler and analysis processes themselves.
  procrunner.exec_timeout_ms = 120000;
  if (not procrunner.Run())
  {
    size_t previous_message_count = msg_run.size();
    AddRunError(format("Error executing \"{}\"", exename));
    AddProcessFailureDetails(msg_run, true);
    if (!g_atropt->batchmode)
    {
      for (size_t i = previous_message_count; i < msg_run.size(); ++i)
      {
        print("{}\n", msg_run[i]);
      }
    }
    return;
  }

  run_output = procrunner.stdout_text;

  if (!g_atropt->batchmode and g_atropt->verblevel >= VERBLEVEL_DEBUG)
  {
    print("Run output:\n{}\n", run_output);
  }

  if (not run_captures.empty())
  {
    AnalyzeRunOutput();
  }

  if (procrunner.exit_code != expected_exit_code)
  {
    size_t previous_message_count = msg_run.size();
    AddRunError(format("Exit code {} != expected {}", procrunner.exit_code, expected_exit_code));
    AddProcessFailureDetails(msg_run, run_captures.empty());
    if (!g_atropt->batchmode)
    {
      for (size_t i = previous_message_count; i < msg_run.size(); ++i)
      {
        print("{}\n", msg_run[i]);
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
      ORunCapture * idcap = nullptr;
      if (id_and_value)
      {
        for (ORunCapture * cap : run_captures)
        {
          if (cap->strid == sid)
          {
            idcap = cap;
            break;
          }
        }

        if (idcap)
        {
          char * valueptr = spl.readptr;
          bool value_matches = idcap->checkvalue.empty() or spl.CheckSymbol(idcap->checkvalue.c_str());

          if (idcap->ignore)
          {
            if (value_matches)
            {
              idcap->captured = true;
              errstr = "";
              waschecked = true;
            }
          }
          else if (idcap->captured)
          {
            errstr = "already captured";
          }
          else
          {
            idcap->captured = true;
            if (value_matches)
            {
              errstr = "";
              waschecked = true;
            }
            else
            {
              errstr = format("!= {}", idcap->checkvalue);
            }
          }

          spl.readptr = valueptr;
        }
      }
      else
      {
        for (ORunCapture * cap : run_captures)
        {
          if (cap->ignore and cap->checkvalue.empty() and spl.CheckSymbol(cap->strid.c_str()))
          {
            cap->captured = true;
            waschecked = true;
            break;
          }
        }
      }

      if (not waschecked and errstr.empty())
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
    if (not cap->ignore and not cap->captured)
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
    print("Run test FAILED: {} failures detected.\n", errorcnt_run);
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
          if (not cap->msg_contains.empty() and (curline.find(cap->msg_contains) == string::npos))
          {
            AddEtError(format("MESSAGE_MISMATCH: {} does not contain \"{}\"", curline, cap->msg_contains));
            if (!g_atropt->batchmode)
            {
              print("{}\n", msg_err.back());
            }
          }
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

void OTestFile::AddBuildError(const string astr)
{
  msg_build.push_back(astr);
  ++errorcnt_build;
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

void OTestFile::AddCompilerOutputErrors(string & atext, vector<string> & rmessages,
                                        int & rerror_count)
{
  sp.Init(atext.data(), atext.size());
  sp.SkipSpaces();

  while (sp.readptr < sp.bufend)
  {
    if (not sp.ReadLine())
    {
      break;
    }

    string line = sp.PrevStr();
    if (not line.empty())
    {
      rmessages.push_back("COMPERR: " + line);
      ++rerror_count;
    }
  }
}

void OTestFile::AddProcessFailureDetails(vector<string> & rmessages, bool include_stdout)
{
  auto append_lines = [&](const string & prefix, string & text)
  {
    TStrParseObj parser;
    parser.Init(text.data(), text.size());
    while (parser.readptr < parser.bufend)
    {
      if (not parser.ReadLine())
      {
        break;
      }
      string line = parser.PrevStr();
      if (not line.empty())
      {
        rmessages.push_back(prefix + line);
      }
    }
  };

  if (include_stdout)
  {
    append_lines("STDOUT: ", procrunner.stdout_text);
  }
  append_lines("STDERR: ", procrunner.stderr_text);
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

      // file control markers
      else if (("notest" == sid) or ("skip_test" == sid))
      {
        skip_test = true;
      }
      else if ("build_only" == sid)
      {
        if (build_only)
        {
          AddTfError("Duplicate marker \"build_only\"");
        }
        build_only = true;
      }
      else if ("build_suffix" == sid)
      {
        ParseMarkerBuildSuffix();
      }
      else if ("compargs" == sid)
      {
        ParseMarkerCompilerArgs();
      }
      else if ("exitcode" == sid)
      {
        ParseMarkerExitCode();
      }
      else if ("hostarch" == sid)
      {
        ParseMarkerHostArch();
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

bool OTestFile::ValidateMarkers()
{
  if (build_only and HasRunTest())
  {
    AddTfErrorNoLine("//?build_only cannot be combined with runtime test markers.");
  }

  if (has_host_arch and (required_host_arch != HostArchitecture()))
  {
    skip_test = true;
  }

  return (0 == errorcnt_tf);
}

bool OTestFile::HasRunTest() const
{
  return has_exit_code or (not run_captures.empty());
}

void OTestFile::ParseMarkerError(const string amsgid)
{
  // sample: //?error(TypeSpecExpected)
  // sample: //?error(TypeSpecExpected, 'message fragment')
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
  string msg_contains;
  if (sp.CheckSymbol(","))
  {
    sp.SkipSpaces(false);
    if (!sp.ReadQuotedString())
    {
      AddTfError(format("Message fragment string is missing after \"//?error({}\"", errid));
      return;
    }
    msg_contains = sp.PrevStr();
    sp.SkipSpaces(false);
  }

  if (not sp.CheckSymbol(")"))
  {
    AddTfError(format("\")\" is missing after \"//?error\""));
    return;
  }

  err_captures.push_back(new OErrCapture(errline, amsgid, errid, msg_contains));

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

    sp.SkipSpaces(false);
    if (not sp.CheckSymbol(")"))
    {
      AddTfError(format("\")\" is missing after \"//?{}\"", cmd));
      return;
    }
  }
  else
  {
    if (not aignore)
    {
      AddTfError(format("\",\" is missing after \"//?check\""));
      return;
    }

    if (not sp.CheckSymbol(")"))
    {
      AddTfError(format("\")\" is missing after \"//?{}\"", cmd));
      return;
    }
  }

  run_captures.push_back(new ORunCapture(strid, sv, aignore));
}

void OTestFile::ParseMarkerCompilerArgs()
{
  sp.SkipSpaces(false);
  if (not sp.CheckSymbol("("))
  {
    AddTfError("\"(\" is missing after \"//?compargs\"");
    return;
  }

  sp.SkipSpaces(false);
  if (sp.CheckSymbol(")"))
  {
    return;
  }

  while (true)
  {
    string argument;
    if (not sp.ReadQuotedString(argument))
    {
      AddTfError("Quoted compiler argument is expected in \"//?compargs\"");
      return;
    }
    compiler_args.push_back(argument);

    sp.SkipSpaces(false);
    if (sp.CheckSymbol(")"))
    {
      return;
    }
    if (not sp.CheckSymbol(","))
    {
      AddTfError("\",\" or \")\" is expected in \"//?compargs\"");
      return;
    }
    sp.SkipSpaces(false);
  }
}

void OTestFile::ParseMarkerBuildSuffix()
{
  sp.SkipSpaces(false);
  if (!sp.CheckSymbol("("))
  {
    AddTfError("\"(\" is missing after \"//?build_suffix\"");
    return;
  }

  sp.SkipSpaces(false);
  string suffix;
  if (!sp.ReadQuotedString(suffix) or suffix.empty())
  {
    AddTfError("Non-empty quoted build suffix is expected in \"//?build_suffix\"");
    return;
  }

  sp.SkipSpaces(false);
  if (!sp.CheckSymbol(")"))
  {
    AddTfError("\")\" is missing after \"//?build_suffix\"");
    return;
  }
  if (!build_suffix.empty())
  {
    AddTfError("Duplicate marker \"build_suffix\"");
    return;
  }

  build_suffix = suffix;
}

void OTestFile::ParseMarkerExitCode()
{
  sp.SkipSpaces(false);
  if (not sp.CheckSymbol("("))
  {
    AddTfError("\"(\" is missing after \"//?exitcode\"");
    return;
  }

  sp.SkipSpaces(false);
  if (not sp.ReadDecimalNumbers())
  {
    AddTfError("Exit code is missing after \"//?exitcode(\"");
    return;
  }
  int exit_code = sp.PrevToInt();

  sp.SkipSpaces(false);
  if (not sp.CheckSymbol(")"))
  {
    AddTfError("\")\" is missing after \"//?exitcode\"");
    return;
  }
  if ((exit_code < 0) or (exit_code > 255))
  {
    AddTfError("Exit code must be between 0 and 255");
    return;
  }
  if (has_exit_code)
  {
    AddTfError("Duplicate marker \"exitcode\"");
    return;
  }

  has_exit_code = true;
  expected_exit_code = exit_code;
}

void OTestFile::ParseMarkerHostArch()
{
  sp.SkipSpaces(false);
  if (not sp.CheckSymbol("("))
  {
    AddTfError("\"(\" is missing after \"//?hostarch\"");
    return;
  }

  sp.SkipSpaces(false);
  string architecture;
  if (not sp.ReadQuotedString(architecture))
  {
    AddTfError("Quoted architecture is missing after \"//?hostarch(\"");
    return;
  }

  sp.SkipSpaces(false);
  if (not sp.CheckSymbol(")"))
  {
    AddTfError("\")\" is missing after \"//?hostarch\"");
    return;
  }
  if (("x64" != architecture) and ("arm64" != architecture))
  {
    AddTfError(format("Unknown host architecture \"{}\"", architecture));
    return;
  }
  if (has_host_arch)
  {
    AddTfError("Duplicate marker \"hostarch\"");
    return;
  }

  has_host_arch = true;
  required_host_arch = architecture;
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

string OTestFile::FindAutotestPackagePath() const
{
  fs::path cursor = fs::absolute(fs::path(filename)).parent_path();
  while (not cursor.empty())
  {
    fs::path package_root = cursor / "packages";
    if (fs::exists(package_root / "dqautotest" / "dqautotest.dq"))
    {
      return package_root.lexically_normal().generic_string();
    }

    fs::path parent = cursor.parent_path();
    if (parent == cursor)
    {
      break;
    }
    cursor = parent;
  }
  return {};
}

void OTestFile::ConfigureRunEnvironment()
{
  string compiler = ResolveExecutablePath(g_atropt->compiler_filename);
  fs::path compiler_dir = fs::path(compiler).parent_path();
  fs::path test_path = fs::absolute(fs::path(filename)).lexically_normal();

  procrunner.env_overrides = {
    {"DQ_AUTOTEST_COMPILER", compiler},
    {"DQ_AUTOTEST_BINDIR", compiler_dir.generic_string()},
    {"DQ_AUTOTEST_TEST_ROOT", AbsolutePath(g_atropt->test_root)},
    {"DQ_AUTOTEST_TEST_FILE", test_path.generic_string()},
    {"DQ_AUTOTEST_TEST_DIR", test_path.parent_path().generic_string()},
    {"DQ_AUTOTEST_HOSTARCH", HostArchitecture()}
  };
}

bool OTestFile::ExecCompiler(bool errmode)
{
  bool result = true;

  string exename = fs::path(filename).replace_extension("exe").generic_string();

  procrunner.env_overrides.clear();
  procrunner.args = { g_atropt->compiler_filename, filename };
  procrunner.args.insert(procrunner.args.end(), compiler_args.begin(), compiler_args.end());

  string package_path = FindAutotestPackagePath();
  if (not package_path.empty())
  {
    procrunner.args.push_back("--pkg-path");
    procrunner.args.push_back(package_path);
  }
  if (g_atropt->batchmode)
  {
    procrunner.args.push_back("--package-build-root");
    procrunner.args.push_back(AbsolutePath(g_atropt->test_root));
  }

  procrunner.args.push_back("-o");
  procrunner.args.push_back(exename);
  if (!build_suffix.empty() or errmode)
  {
    string suffix = build_suffix;
    if (errmode)
    {
      if (!suffix.empty()) suffix += "-";
      suffix += "err";
    }
    procrunner.args.push_back("--build-suffix");
    procrunner.args.push_back(suffix);
  }
  if (g_atropt->optlevel >= 0)
  {
    procrunner.args.push_back(format("-O{}", g_atropt->optlevel));
  }
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
