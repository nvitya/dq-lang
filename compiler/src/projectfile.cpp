/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    projectfile.cpp
 * authors: Codex
 * created: 2026-08-10
 * brief:   DQ compiler project file parser
 */

#include "projectfile.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <format>
#include <map>
#include <set>

#include "module_path.h"

using namespace std;

string SDqProjectDiagnostic::Format() const
{
  return format("{}({},{}) ERROR({}): {}", filename.string(), line, col, id, message);
}

ODqProjectFile::ODqProjectFile(ODqProject * aproject, const filesystem::path & afilename)
  : filename(afilename), pproject(aproject)
{
  filename = Canonical(filename);
  text = " ";
  sp.Init(text.data(), text.size());
}

filesystem::path ODqProjectFile::AbsNorm(const filesystem::path & path) const
{
  error_code ec;
  filesystem::path result = filesystem::absolute(path, ec);
  if (ec) result = path;
  return result.lexically_normal();
}

filesystem::path ODqProjectFile::Canonical(const filesystem::path & path) const
{
  error_code ec;
  filesystem::path result = filesystem::weakly_canonical(path, ec);
  return ec ? AbsNorm(path) : result;
}

pair<int, int> ODqProjectFile::LineCol(const char * pos) const
{
  int line = 1;
  int col = 1;
  for (const char * p = sp.bufstart; (p < pos) && (p < sp.bufend); ++p)
  {
    if (*p == '\n')
    {
      ++line;
      col = 1;
    }
    else
    {
      ++col;
    }
  }
  return {line, col};
}

bool ODqProjectFile::Fail(const string & id, const string & message,
                        const char * pos)
{
  if (pproject->diagnostics.empty())
  {
    if (!pos) pos = sp.readptr;
    auto [line, col] = LineCol(pos);
    pproject->diagnostics.push_back({filename, line, col, id, message});
  }
  return false;
}

bool ODqProjectFile::SkipSpace(bool include_line_end, bool & rsaw_line_end)
{
  rsaw_line_end = false;
  while (sp.readptr < sp.bufend)
  {
    char c = *sp.readptr;
    if ((c == ' ') || (c == '\t') || (c == '\f') || (c == '\v'))
    {
      ++sp.readptr;
      continue;
    }
    if ((c == '\r') || (c == '\n'))
    {
      rsaw_line_end = true;
      if (!include_line_end) return true;
      if (c == '\r') ++sp.readptr;
      if ((sp.readptr < sp.bufend) && (*sp.readptr == '\n')) ++sp.readptr;
      else if (c == '\n') ++sp.readptr;
      continue;
    }
    if (sp.CheckSymbol("//"))
    {
      sp.ReadTo("\r\n");
      continue;
    }
    if (sp.CheckSymbol("/*"))
    {
      bool contained_line_end = false;
      while (sp.readptr < sp.bufend)
      {
        if (sp.CheckSymbol("*/")) break;
        if ((*sp.readptr == '\r') || (*sp.readptr == '\n')) contained_line_end = true;
        ++sp.readptr;
      }
      if ((sp.readptr >= sp.bufend)
          && ((sp.bufend - sp.bufstart < 2)
              || (string_view(sp.bufend - 2, 2) != "*/")))
      {
        return Fail("ProjectComment", "Unterminated block comment");
      }
      if (contained_line_end)
      {
        rsaw_line_end = true;
        if (!include_line_end) return true;
      }
      continue;
    }
    break;
  }
  return true;
}

bool ODqProjectFile::SkipInlineSpace()
{
  bool saw_line_end = false;
  if (!SkipSpace(false, saw_line_end)) return false;
  return !saw_line_end;
}

bool ODqProjectFile::RequireInlineSpaceResult(const string & expected)
{
  if (SkipInlineSpace()) return true;
  if (!pproject->diagnostics.empty()) return false;
  return Fail("ProjectSyntax", expected + " before the end of the statement");
}

bool ODqProjectFile::FinishStatement()
{
  bool saw_line_end = false;
  if (!SkipSpace(false, saw_line_end)) return false;
  if (saw_line_end || (sp.readptr >= sp.bufend)) return true;
  if (sp.CheckSymbol(";")) return true;
  return Fail("ProjectTrailingToken", "Unexpected text after project statement");
}

bool ODqProjectFile::ReadIdentifier(string & rvalue, const string & what)
{
  if (sp.ReadIdentifier(rvalue)) return true;
  return Fail("ProjectSyntax", "Expected " + what);
}

bool ODqProjectFile::ReadString(string & rvalue)
{
  if ((sp.readptr >= sp.bufend)
      || ((*sp.readptr != '\'') && (*sp.readptr != '"')))
  {
    return Fail("ProjectValue", "Expected a quoted string");
  }
  const char * start = sp.readptr;
  if (!sp.ReadQuotedString(rvalue))
  {
    return Fail("ProjectString", "Unterminated quoted string", start);
  }
  return true;
}

bool ODqProjectFile::ExpandVariables(const string & value, string & rvalue)
{
  rvalue.clear();
  size_t start = 0;
  while (start < value.size())
  {
    size_t marker = value.find("${", start);
    if (marker == string::npos)
    {
      rvalue += value.substr(start);
      return true;
    }
    rvalue += value.substr(start, marker - start);
    size_t close = value.find('}', marker + 2);
    if (close == string::npos)
    {
      return Fail("ProjectVariable", "Unterminated project variable reference");
    }
    string name = value.substr(marker + 2, close - marker - 2);
    if (name == "PROJECT_DIR")
    {
      rvalue += pproject->project_dir.string();
    }
    else if (name == "THIS_DIR")
    {
      rvalue += filename.parent_path().string();
    }
    else
    {
      auto it = pproject->variables.find(name);
      if (it == pproject->variables.end())
      {
        return Fail("ProjectVariable", format("Unknown project variable \"{}\"", name));
      }
      rvalue += it->second;
    }
    start = close + 1;
  }
  return true;
}

bool ODqProjectFile::ReadExpandedString(string & rvalue)
{
  string raw;
  if (!ReadString(raw)) return false;
  return ExpandVariables(raw, rvalue);
}

bool ODqProjectFile::ReadPath(filesystem::path & rpath)
{
  string value;
  if (!ReadExpandedString(value)) return false;
  filesystem::path path(value);
  if (path.is_relative()) path = filename.parent_path() / path;
  rpath = AbsNorm(path);
  return true;
}

bool ODqProjectFile::ReadBool(bool & rvalue)
{
  string value;
  if (!ReadIdentifier(value, "boolean value")) return false;
  if (value == "true")
  {
    rvalue = true;
    return true;
  }
  if (value == "false")
  {
    rvalue = false;
    return true;
  }
  return Fail("ProjectValue", format("Expected true or false, got \"{}\"", value), sp.prevptr);
}

bool ODqProjectFile::ReadInt(int64_t & rvalue)
{
  const char * start = sp.readptr;
  const char * p = start;
  if ((p < sp.bufend) && ((*p == '+') || (*p == '-'))) ++p;
  const char * digits = p;
  while ((p < sp.bufend) && (*p >= '0') && (*p <= '9')) ++p;
  if (p == digits)
  {
    return Fail("ProjectValue", "Expected an integer value", start);
  }
  const char * parse_start = ((*start == '+') ? start + 1 : start);
  auto result = from_chars(parse_start, p, rvalue);
  if (result.ec != errc() || result.ptr != p)
  {
    return Fail("ProjectValue", "Integer value is out of range", start);
  }
  sp.prevptr = const_cast<char *>(start);
  sp.prevlen = int(p - start);
  sp.readptr = const_cast<char *>(p);
  return true;
}

bool ODqProjectFile::CheckDuplicate(const string & name)
{
  if (pproject->single_properties.insert(name).second) return true;
  return Fail("ProjectDuplicate", format("Project property \"{}\" may only be specified once", name));
}

vector<string> ODqProjectFile::EffectivePackagePaths() const
{
  vector<string> result = g_opt.package_paths;
  result.insert(result.end(), pproject->command_line_package_paths.begin(),
                pproject->command_line_package_paths.end());
  return result;
}

bool ODqProjectFile::ReadPackagePathCall(string & rvalue)
{
  string function_name;
  if (!ReadIdentifier(function_name, "PackagePath function")) return false;
  if (function_name != "PackagePath")
  {
    return Fail("ProjectFunction", format("Unknown project function \"{}\"", function_name), sp.prevptr);
  }
  if (!RequireInlineSpaceResult("Expected '('") || !sp.CheckSymbol("("))
  {
    return pproject->diagnostics.empty() ? Fail("ProjectSyntax", "Expected '(' after PackagePath") : false;
  }
  if (!RequireInlineSpaceResult("Expected package name")) return false;
  string package_name;
  if (!ReadExpandedString(package_name)) return false;
  if (!RequireInlineSpaceResult("Expected ')'") || !sp.CheckSymbol(")"))
  {
    return pproject->diagnostics.empty() ? Fail("ProjectSyntax", "Expected ')' after package name") : false;
  }

  string checked_name;
  TStrParseObj name_parser(package_name.data(), package_name.size());
  if (!name_parser.ReadIdentifier(checked_name) || (name_parser.readptr != name_parser.bufend))
  {
    return Fail("ProjectPackage", format("Invalid package name \"{}\"", package_name));
  }

  filesystem::path package_root;
  vector<string> roots = EffectivePackagePaths();
  if (!OModulePath::ResolvePackageRoot(package_name, roots, package_root))
  {
    return Fail("ProjectPackage", format("Package \"{}\" was not found", package_name));
  }
  rvalue = package_root.string();
  return true;
}

bool ODqProjectFile::ParseVariable()
{
  if (!RequireInlineSpaceResult("Expected variable name")) return false;
  string name;
  if (!ReadIdentifier(name, "variable name")) return false;
  if ((name == "PROJECT_DIR") || (name == "THIS_DIR") || pproject->variables.contains(name))
  {
    return Fail("ProjectDuplicate", format("Project variable \"{}\" is already defined", name), sp.prevptr);
  }
  if (!RequireInlineSpaceResult("Expected '='") || !sp.CheckSymbol("="))
  {
    return pproject->diagnostics.empty() ? Fail("ProjectSyntax", "Expected '=' after project variable") : false;
  }
  if (!RequireInlineSpaceResult("Expected variable value")) return false;

  string value;
  if ((sp.readptr < sp.bufend) && ((*sp.readptr == '\'') || (*sp.readptr == '"')))
  {
    if (!ReadExpandedString(value)) return false;
    filesystem::path path(value);
    if (path.is_relative()) value = AbsNorm(filename.parent_path() / path).string();
    else value = path.lexically_normal().string();
  }
  else if (!ReadPackagePathCall(value))
  {
    return false;
  }
  pproject->variables[name] = value;
  return FinishStatement();
}

bool ODqProjectFile::ParseInclude()
{
  const char * include_pos = sp.prevptr;
  if (!RequireInlineSpaceResult("Expected include path")) return false;
  filesystem::path include_file;
  if (!ReadPath(include_file)) return false;
  if (include_file.extension() != ".dqproj")
  {
    return Fail("ProjectInclude", "Included project fragments must use the .dqproj extension");
  }
  if (!FinishStatement()) return false;

  filesystem::path canonical_include = Canonical(include_file);
  auto cycle_it = find(pproject->include_stack.begin(), pproject->include_stack.end(), canonical_include);
  if (cycle_it != pproject->include_stack.end())
  {
    string chain;
    for (auto it = cycle_it; it != pproject->include_stack.end(); ++it)
    {
      if (!chain.empty()) chain += " -> ";
      chain += it->string();
    }
    chain += " -> " + canonical_include.string();
    return Fail("ProjectIncludeCycle", "Project include cycle: " + chain, include_pos);
  }

  error_code ec;
  if (!filesystem::is_regular_file(include_file, ec) || ec)
  {
    return Fail("ProjectInclude", format("Included project file \"{}\" was not found", include_file.string()),
                include_pos);
  }
  ODqProjectFile included_file(pproject, canonical_include);
  return included_file.ParseFile();
}

bool ODqProjectFile::ParseDefine()
{
  if (!RequireInlineSpaceResult("Expected define name")) return false;
  string name;
  if (!ReadIdentifier(name, "define name")) return false;
  if (!pproject->define_names.insert(name).second)
  {
    return Fail("ProjectDuplicate", format("Project define \"{}\" is already defined", name), sp.prevptr);
  }

  OCmdLineDefine def;
  def.name = name;
  bool saw_line_end = false;
  if (!SkipSpace(false, saw_line_end)) return false;
  if (saw_line_end || (sp.readptr >= sp.bufend))
  {
    g_opt.cmdline_defines.push_back(def);
    return true;
  }
  if (sp.CheckSymbol(";"))
  {
    g_opt.cmdline_defines.push_back(def);
    return true;
  }
  if (sp.CheckSymbol("="))
  {
    if (!RequireInlineSpaceResult("Expected define value")) return false;
    if ((sp.readptr < sp.bufend)
        && ((*sp.readptr == '+') || (*sp.readptr == '-')
            || ((*sp.readptr >= '0') && (*sp.readptr <= '9'))))
    {
      if (!ReadInt(def.int_value)) return false;
      def.has_int_value = true;
    }
    else
    {
      if (!ReadBool(def.bool_value)) return false;
      def.has_bool_value = true;
    }
  }
  else
  {
    return Fail("ProjectTrailingToken", "Unexpected text after project define");
  }
  g_opt.cmdline_defines.push_back(def);
  return FinishStatement();
}

bool ODqProjectFile::ParseProperty(const string & name)
{
  bool b;
  SkipSpace(false, b);

  if (!sp.CheckSymbol("="))
  {
    return Fail("ProjectSyntax", format("Expected '=' after \"{}\"", name));
  }

  SkipSpace(false, b);

  filesystem::path  path;
  bool              bvalue = false;

  if ("main" == name)
  {
    if (!CheckDuplicate(name))  return false;
    if (!ReadPath(path))        return false;
    g_opt.project_main_filename = path.string();
  }
  else if ("output" == name)
  {
    if (!CheckDuplicate(name))  return false;
    if (!ReadPath(path)) return false;
    g_opt.project_output_filename = path.string();
    g_opt.project_has_output = true;
  }
  else if ("target" == name)
  {
    if (!CheckDuplicate(name))  return false;

    string value;
    if (!ReadExpandedString(value)) return false;
    string target_error;
    if (!g_opt.target.Configure(value, target_error))
    {
      return Fail("ProjectValue", target_error, sp.prevptr);
    }
  }
  else if (("cpu" == name) || ("abi" == name) || ("floatabi" == name))
  {
    return Fail("ProjectUnsupported", format("Project property \"{}\" is not supported yet", name));
  }
  else if ("packagepath" == name)
  {
    if (!ReadPath(path)) return false;
    g_opt.package_paths.push_back(path.string());
  }
  else if ("link" == name)
  {
    if (!CheckDuplicate(name))  return false;
    if (!ReadBool(bvalue)) return false;
    g_opt.link_mode = (bvalue ? DQC_LINK_FORCE : DQC_LINK_COMPILE_ONLY);
  }
  else if ("linkobject"  == name)
  {
    if (!ReadPath(path)) return false;
    g_opt.link_objects.push_back(path.string());
  }
  else if ("linkerpath"  == name)
  {
    if (!ReadPath(path)) return false;
    g_opt.linker_args.push_back("--library-path=" + path.string());
  }
  else if ("linkoption" == name)
  {
    string value;
    if (!ReadExpandedString(value)) return false;
    g_opt.linker_args.push_back(value);
  }
  else if ("linkscript" == name)
  {
    if (!CheckDuplicate(name))  return false;
    if (!ReadPath(path)) return false;
    g_opt.linker_args.push_back("--script=" + path.string());
  }
  else if ("debuginfo" == name)
  {
    if (!CheckDuplicate(name))  return false;
    if (!ReadBool(bvalue)) return false;
    g_opt.dbg_info = bvalue;
  }
  else if ("optlevel" == name)
  {
    if (!CheckDuplicate(name))  return false;
    int64_t value = 0;
    if (!ReadInt(value)) return false;
    if ((value < 0) || (value > 3))
    {
      return Fail("ProjectValue", "optlevel must be between 0 and 3", sp.prevptr);
    }
    g_opt.optlevel = value;
  }
  else if ("lto" == name)
  {
    if (!CheckDuplicate(name))  return false;
    if (!ReadBool(bvalue)) return false;
    g_opt.lto_mode = (bvalue ? LTOMODE_FULL : LTOMODE_OFF);
  }

  else
  {
    return Fail("ProjectProperty", format("Unknown project property \"{}\"", name));
  }

  return FinishStatement();
}

bool ODqProjectFile::ParseFile()
{
  ifstream input(filename, ios::binary | ios::ate);
  if (!input)
  {
    return Fail("ProjectRead", format("Can not read project file \"{}\"", filename.string()));
  }
  streamsize size = input.tellg();
  if (size < 0)
  {
    return Fail("ProjectRead", format("Can not determine project file size for \"{}\"", filename.string()));
  }

  text.resize(size_t(size));
  sp.Init(text.data(), text.size());

  input.seekg(0);
  if (size > 0) input.read(text.data(), size);
  if (!input && size > 0)
  {
    return Fail("ProjectRead", format("Can not read project file \"{}\"", filename.string()));
  }

  pproject->include_stack.push_back(filename);

  bool ok = true;
  while (sp.readptr < sp.bufend)
  {
    bool saw_line_end = false;
    if (!SkipSpace(true, saw_line_end))
    {
      ok = false;
      break;
    }
    if (sp.readptr >= sp.bufend) break;
    if (sp.CheckSymbol(";")) continue;

    string statement;
    if (!ReadIdentifier(statement, "project statement"))
    {
      ok = false;
      break;
    }
    bool statement_ok = false;
    if      (statement == "var")      statement_ok = ParseVariable();
    else if (statement == "include")  statement_ok = ParseInclude();
    else if (statement == "define")   statement_ok = ParseDefine();
    else                              statement_ok = ParseProperty(statement);

    if (!statement_ok)
    {
      ok = false;
      break;
    }
  }

  pproject->include_stack.pop_back();
  return ok;
}

bool ODqProject::Load(const filesystem::path & top_file,
                      const vector<string> & acommand_line_package_paths)
{
  diagnostics.clear();
  command_line_package_paths = acommand_line_package_paths;
  variables.clear();
  single_properties.clear();
  define_names.clear();
  include_stack.clear();

  ODqProjectFile project_file(this, top_file);
  project_dir = project_file.filename.parent_path();
  g_opt.project_filename = project_file.filename.string();

  if (project_file.filename.extension() != ".dqproj")
  {
    return project_file.Fail("ProjectExtension", "DQ project files must use the .dqproj extension");
  }

  if (!project_file.ParseFile())  // parsing the project file, handling includes
  {
    return false;
  }

  if (g_opt.project_main_filename.empty())
  {
    return project_file.Fail("ProjectMain", "The assembled project does not specify main");
  }
  return true;
}
