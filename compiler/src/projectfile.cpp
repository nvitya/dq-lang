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
#include "strparse.h"

using namespace std;

string SDqProjectDiagnostic::Format() const
{
  return format("{}({},{}) ERROR({}): {}", filename.string(), line, col, id, message);
}

struct ODqProjectFile::SParseContext
{
  filesystem::path filename;
  string text;
  TStrParseObj sp;
};

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

pair<int, int> ODqProjectFile::LineCol(const SParseContext & ctx, const char * pos) const
{
  int line = 1;
  int col = 1;
  for (const char * p = ctx.sp.bufstart; (p < pos) && (p < ctx.sp.bufend); ++p)
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

bool ODqProjectFile::Fail(const SParseContext & ctx, const string & id, const string & message,
                        const char * pos)
{
  if (diagnostics.empty())
  {
    if (!pos) pos = ctx.sp.readptr;
    auto [line, col] = LineCol(ctx, pos);
    diagnostics.push_back({ctx.filename, line, col, id, message});
  }
  return false;
}

bool ODqProjectFile::SkipSpace(SParseContext & ctx, bool include_line_end, bool & rsaw_line_end)
{
  rsaw_line_end = false;
  while (ctx.sp.readptr < ctx.sp.bufend)
  {
    char c = *ctx.sp.readptr;
    if ((c == ' ') || (c == '\t') || (c == '\f') || (c == '\v'))
    {
      ++ctx.sp.readptr;
      continue;
    }
    if ((c == '\r') || (c == '\n'))
    {
      rsaw_line_end = true;
      if (!include_line_end) return true;
      if (c == '\r') ++ctx.sp.readptr;
      if ((ctx.sp.readptr < ctx.sp.bufend) && (*ctx.sp.readptr == '\n')) ++ctx.sp.readptr;
      else if (c == '\n') ++ctx.sp.readptr;
      continue;
    }
    if (ctx.sp.CheckSymbol("//"))
    {
      ctx.sp.ReadTo("\r\n");
      continue;
    }
    if (ctx.sp.CheckSymbol("/*"))
    {
      bool contained_line_end = false;
      while (ctx.sp.readptr < ctx.sp.bufend)
      {
        if (ctx.sp.CheckSymbol("*/")) break;
        if ((*ctx.sp.readptr == '\r') || (*ctx.sp.readptr == '\n')) contained_line_end = true;
        ++ctx.sp.readptr;
      }
      if ((ctx.sp.readptr >= ctx.sp.bufend)
          && ((ctx.sp.bufend - ctx.sp.bufstart < 2)
              || (string_view(ctx.sp.bufend - 2, 2) != "*/")))
      {
        return Fail(ctx, "ProjectComment", "Unterminated block comment");
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

bool ODqProjectFile::SkipInlineSpace(SParseContext & ctx)
{
  bool saw_line_end = false;
  if (!SkipSpace(ctx, false, saw_line_end)) return false;
  return !saw_line_end;
}

bool ODqProjectFile::RequireInlineSpaceResult(SParseContext & ctx, const string & expected)
{
  if (SkipInlineSpace(ctx)) return true;
  if (!diagnostics.empty()) return false;
  return Fail(ctx, "ProjectSyntax", expected + " before the end of the statement");
}

bool ODqProjectFile::FinishStatement(SParseContext & ctx)
{
  bool saw_line_end = false;
  if (!SkipSpace(ctx, false, saw_line_end)) return false;
  if (saw_line_end || (ctx.sp.readptr >= ctx.sp.bufend)) return true;
  if (ctx.sp.CheckSymbol(";")) return true;
  return Fail(ctx, "ProjectTrailingToken", "Unexpected text after project statement");
}

bool ODqProjectFile::ReadIdentifier(SParseContext & ctx, string & rvalue, const string & what)
{
  if (ctx.sp.ReadIdentifier(rvalue)) return true;
  return Fail(ctx, "ProjectSyntax", "Expected " + what);
}

bool ODqProjectFile::ReadString(SParseContext & ctx, string & rvalue)
{
  if ((ctx.sp.readptr >= ctx.sp.bufend)
      || ((*ctx.sp.readptr != '\'') && (*ctx.sp.readptr != '"')))
  {
    return Fail(ctx, "ProjectValue", "Expected a quoted string");
  }
  const char * start = ctx.sp.readptr;
  if (!ctx.sp.ReadQuotedString(rvalue))
  {
    return Fail(ctx, "ProjectString", "Unterminated quoted string", start);
  }
  return true;
}

bool ODqProjectFile::ExpandVariables(SParseContext & ctx, const string & value, string & rvalue)
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
      return Fail(ctx, "ProjectVariable", "Unterminated project variable reference");
    }
    string name = value.substr(marker + 2, close - marker - 2);
    if (name == "PROJECT_DIR")
    {
      rvalue += project_dir.string();
    }
    else if (name == "THIS_DIR")
    {
      rvalue += ctx.filename.parent_path().string();
    }
    else
    {
      auto it = variables.find(name);
      if (it == variables.end())
      {
        return Fail(ctx, "ProjectVariable", format("Unknown project variable \"{}\"", name));
      }
      rvalue += it->second;
    }
    start = close + 1;
  }
  return true;
}

bool ODqProjectFile::ReadExpandedString(SParseContext & ctx, string & rvalue)
{
  string raw;
  if (!ReadString(ctx, raw)) return false;
  return ExpandVariables(ctx, raw, rvalue);
}

bool ODqProjectFile::ReadPath(SParseContext & ctx, filesystem::path & rpath)
{
  string value;
  if (!ReadExpandedString(ctx, value)) return false;
  filesystem::path path(value);
  if (path.is_relative()) path = ctx.filename.parent_path() / path;
  rpath = AbsNorm(path);
  return true;
}

bool ODqProjectFile::ReadBool(SParseContext & ctx, bool & rvalue)
{
  string value;
  if (!ReadIdentifier(ctx, value, "boolean value")) return false;
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
  return Fail(ctx, "ProjectValue", format("Expected true or false, got \"{}\"", value), ctx.sp.prevptr);
}

bool ODqProjectFile::ReadInt(SParseContext & ctx, int64_t & rvalue)
{
  const char * start = ctx.sp.readptr;
  const char * p = start;
  if ((p < ctx.sp.bufend) && ((*p == '+') || (*p == '-'))) ++p;
  const char * digits = p;
  while ((p < ctx.sp.bufend) && (*p >= '0') && (*p <= '9')) ++p;
  if (p == digits)
  {
    return Fail(ctx, "ProjectValue", "Expected an integer value", start);
  }
  const char * parse_start = ((*start == '+') ? start + 1 : start);
  auto result = from_chars(parse_start, p, rvalue);
  if (result.ec != errc() || result.ptr != p)
  {
    return Fail(ctx, "ProjectValue", "Integer value is out of range", start);
  }
  ctx.sp.prevptr = const_cast<char *>(start);
  ctx.sp.prevlen = int(p - start);
  ctx.sp.readptr = const_cast<char *>(p);
  return true;
}

bool ODqProjectFile::CheckDuplicate(SParseContext & ctx, const string & name)
{
  if (single_properties.insert(name).second) return true;
  return Fail(ctx, "ProjectDuplicate", format("Project property \"{}\" may only be specified once", name));
}

vector<string> ODqProjectFile::EffectivePackagePaths() const
{
  vector<string> result = g_opt.package_paths;
  result.insert(result.end(), command_line_package_paths.begin(), command_line_package_paths.end());
  return result;
}

bool ODqProjectFile::ReadPackagePathCall(SParseContext & ctx, string & rvalue)
{
  string function_name;
  if (!ReadIdentifier(ctx, function_name, "PackagePath function")) return false;
  if (function_name != "PackagePath")
  {
    return Fail(ctx, "ProjectFunction", format("Unknown project function \"{}\"", function_name), ctx.sp.prevptr);
  }
  if (!RequireInlineSpaceResult(ctx, "Expected '('") || !ctx.sp.CheckSymbol("("))
  {
    return diagnostics.empty() ? Fail(ctx, "ProjectSyntax", "Expected '(' after PackagePath") : false;
  }
  if (!RequireInlineSpaceResult(ctx, "Expected package name")) return false;
  string package_name;
  if (!ReadExpandedString(ctx, package_name)) return false;
  if (!RequireInlineSpaceResult(ctx, "Expected ')'") || !ctx.sp.CheckSymbol(")"))
  {
    return diagnostics.empty() ? Fail(ctx, "ProjectSyntax", "Expected ')' after package name") : false;
  }

  string checked_name;
  TStrParseObj name_parser(package_name.data(), package_name.size());
  if (!name_parser.ReadIdentifier(checked_name) || (name_parser.readptr != name_parser.bufend))
  {
    return Fail(ctx, "ProjectPackage", format("Invalid package name \"{}\"", package_name));
  }

  filesystem::path package_root;
  vector<string> roots = EffectivePackagePaths();
  if (!OModulePath::ResolvePackageRoot(package_name, roots, package_root))
  {
    return Fail(ctx, "ProjectPackage", format("Package \"{}\" was not found", package_name));
  }
  rvalue = package_root.string();
  return true;
}

bool ODqProjectFile::ParseVariable(SParseContext & ctx)
{
  if (!RequireInlineSpaceResult(ctx, "Expected variable name")) return false;
  string name;
  if (!ReadIdentifier(ctx, name, "variable name")) return false;
  if ((name == "PROJECT_DIR") || (name == "THIS_DIR") || variables.contains(name))
  {
    return Fail(ctx, "ProjectDuplicate", format("Project variable \"{}\" is already defined", name), ctx.sp.prevptr);
  }
  if (!RequireInlineSpaceResult(ctx, "Expected '='") || !ctx.sp.CheckSymbol("="))
  {
    return diagnostics.empty() ? Fail(ctx, "ProjectSyntax", "Expected '=' after project variable") : false;
  }
  if (!RequireInlineSpaceResult(ctx, "Expected variable value")) return false;

  string value;
  if ((ctx.sp.readptr < ctx.sp.bufend) && ((*ctx.sp.readptr == '\'') || (*ctx.sp.readptr == '"')))
  {
    if (!ReadExpandedString(ctx, value)) return false;
    filesystem::path path(value);
    if (path.is_relative()) value = AbsNorm(ctx.filename.parent_path() / path).string();
    else value = path.lexically_normal().string();
  }
  else if (!ReadPackagePathCall(ctx, value))
  {
    return false;
  }
  variables[name] = value;
  return FinishStatement(ctx);
}

bool ODqProjectFile::ParseInclude(SParseContext & ctx)
{
  const char * include_pos = ctx.sp.prevptr;
  if (!RequireInlineSpaceResult(ctx, "Expected include path")) return false;
  filesystem::path include_file;
  if (!ReadPath(ctx, include_file)) return false;
  if (include_file.extension() != ".dqproj")
  {
    return Fail(ctx, "ProjectInclude", "Included project fragments must use the .dqproj extension");
  }
  if (!FinishStatement(ctx)) return false;

  filesystem::path canonical_include = Canonical(include_file);
  auto cycle_it = find(include_stack.begin(), include_stack.end(), canonical_include);
  if (cycle_it != include_stack.end())
  {
    string chain;
    for (auto it = cycle_it; it != include_stack.end(); ++it)
    {
      if (!chain.empty()) chain += " -> ";
      chain += it->string();
    }
    chain += " -> " + canonical_include.string();
    return Fail(ctx, "ProjectIncludeCycle", "Project include cycle: " + chain, include_pos);
  }

  error_code ec;
  if (!filesystem::is_regular_file(include_file, ec) || ec)
  {
    return Fail(ctx, "ProjectInclude", format("Included project file \"{}\" was not found", include_file.string()),
                include_pos);
  }
  return ParseFile(include_file);
}

bool ODqProjectFile::ParseDefine(SParseContext & ctx)
{
  if (!RequireInlineSpaceResult(ctx, "Expected define name")) return false;
  string name;
  if (!ReadIdentifier(ctx, name, "define name")) return false;
  if (!define_names.insert(name).second)
  {
    return Fail(ctx, "ProjectDuplicate", format("Project define \"{}\" is already defined", name), ctx.sp.prevptr);
  }

  OCmdLineDefine def;
  def.name = name;
  bool saw_line_end = false;
  if (!SkipSpace(ctx, false, saw_line_end)) return false;
  if (saw_line_end || (ctx.sp.readptr >= ctx.sp.bufend))
  {
    g_opt.cmdline_defines.push_back(def);
    return true;
  }
  if (ctx.sp.CheckSymbol(";"))
  {
    g_opt.cmdline_defines.push_back(def);
    return true;
  }
  if (ctx.sp.CheckSymbol("="))
  {
    if (!RequireInlineSpaceResult(ctx, "Expected define value")) return false;
    if ((ctx.sp.readptr < ctx.sp.bufend)
        && ((*ctx.sp.readptr == '+') || (*ctx.sp.readptr == '-')
            || ((*ctx.sp.readptr >= '0') && (*ctx.sp.readptr <= '9'))))
    {
      if (!ReadInt(ctx, def.int_value)) return false;
      def.has_int_value = true;
    }
    else
    {
      if (!ReadBool(ctx, def.bool_value)) return false;
      def.has_bool_value = true;
    }
  }
  else
  {
    return Fail(ctx, "ProjectTrailingToken", "Unexpected text after project define");
  }
  g_opt.cmdline_defines.push_back(def);
  return FinishStatement(ctx);
}

bool ODqProjectFile::ParseProperty(SParseContext & ctx, const string & name)
{
  static const set<string> unsupported = {"cpu", "abi", "floatabi"};
  if (unsupported.contains(name))
  {
    return Fail(ctx, "ProjectUnsupported", format("Project property \"{}\" is not supported yet", name));
  }

  static const set<string> known = {
    "main", "output", "target", "link", "optlevel", "debuginfo", "linkscript",
    "packagepath", "linkerpath", "linkobject", "linkoption"
  };
  if (!known.contains(name))
  {
    return Fail(ctx, "ProjectProperty", format("Unknown project property \"{}\"", name));
  }

  bool single = (name == "main") || (name == "output") || (name == "target")
                || (name == "link") || (name == "optlevel") || (name == "debuginfo")
                || (name == "linkscript");
  if (single && !CheckDuplicate(ctx, name)) return false;
  if (!RequireInlineSpaceResult(ctx, "Expected '='") || !ctx.sp.CheckSymbol("="))
  {
    return diagnostics.empty() ? Fail(ctx, "ProjectSyntax", format("Expected '=' after \"{}\"", name)) : false;
  }
  if (!RequireInlineSpaceResult(ctx, "Expected property value")) return false;

  if ((name == "main") || (name == "output") || (name == "linkscript")
      || (name == "packagepath") || (name == "linkerpath") || (name == "linkobject"))
  {
    filesystem::path path;
    if (!ReadPath(ctx, path)) return false;
    if      (name == "main")        g_opt.project_main_filename = path.string();
    else if (name == "output")
    {
      g_opt.project_output_filename = path.string();
      g_opt.project_has_output = true;
    }
    else if (name == "packagepath") g_opt.package_paths.push_back(path.string());
    else if (name == "linkobject")  g_opt.link_objects.push_back(path.string());
    else if (name == "linkerpath")  g_opt.linker_args.push_back("--library-path=" + path.string());
    else                             g_opt.linker_args.push_back("--script=" + path.string());
  }
  else if (name == "target")
  {
    string value;
    if (!ReadExpandedString(ctx, value)) return false;
    string target_error;
    if (!g_opt.target.Configure(value, target_error))
    {
      return Fail(ctx, "ProjectValue", target_error, ctx.sp.prevptr);
    }
  }
  else if (name == "linkoption")
  {
    string value;
    if (!ReadExpandedString(ctx, value)) return false;
    g_opt.linker_args.push_back(value);
  }
  else if ((name == "link") || (name == "debuginfo"))
  {
    bool value = false;
    if (!ReadBool(ctx, value)) return false;
    if (name == "link") g_opt.link_mode = value ? DQC_LINK_FORCE : DQC_LINK_COMPILE_ONLY;
    else g_opt.dbg_info = value;
  }
  else
  {
    int64_t value = 0;
    if (!ReadInt(ctx, value)) return false;
    if ((value < 0) || (value > 3))
    {
      return Fail(ctx, "ProjectValue", "optlevel must be between 0 and 3", ctx.sp.prevptr);
    }
    g_opt.optlevel = int(value);
  }
  return FinishStatement(ctx);
}

bool ODqProjectFile::ParseFile(const filesystem::path & input_file)
{
  filesystem::path normalized = Canonical(input_file);
  auto cycle_it = find(include_stack.begin(), include_stack.end(), normalized);
  if (cycle_it != include_stack.end())
  {
    string chain;
    for (auto it = cycle_it; it != include_stack.end(); ++it)
    {
      if (!chain.empty()) chain += " -> ";
      chain += it->string();
    }
    chain += " -> " + normalized.string();
    SParseContext dummy;
    dummy.filename = normalized;
    dummy.text = " ";
    dummy.sp.Init(dummy.text.data(), dummy.text.size());
    return Fail(dummy, "ProjectIncludeCycle", "Project include cycle: " + chain);
  }

  SParseContext ctx;
  ctx.filename = normalized;
  ifstream input(normalized, ios::binary | ios::ate);
  if (!input)
  {
    ctx.text = " ";
    ctx.sp.Init(ctx.text.data(), ctx.text.size());
    return Fail(ctx, "ProjectRead", format("Can not read project file \"{}\"", normalized.string()));
  }
  streamsize size = input.tellg();
  if (size < 0)
  {
    ctx.text = " ";
    ctx.sp.Init(ctx.text.data(), ctx.text.size());
    return Fail(ctx, "ProjectRead", format("Can not determine project file size for \"{}\"", normalized.string()));
  }
  ctx.text.resize(size_t(size));
  input.seekg(0);
  if (size > 0) input.read(ctx.text.data(), size);
  if (!input && size > 0)
  {
    ctx.sp.Init(ctx.text.data(), ctx.text.size());
    return Fail(ctx, "ProjectRead", format("Can not read project file \"{}\"", normalized.string()));
  }
  ctx.sp.Init(ctx.text.data(), ctx.text.size());
  include_stack.push_back(normalized);

  while (ctx.sp.readptr < ctx.sp.bufend)
  {
    bool saw_line_end = false;
    if (!SkipSpace(ctx, true, saw_line_end)) return false;
    if (ctx.sp.readptr >= ctx.sp.bufend) break;
    if (ctx.sp.CheckSymbol(";")) continue;

    string statement;
    if (!ReadIdentifier(ctx, statement, "project statement")) return false;
    bool ok = false;
    if      (statement == "var")     ok = ParseVariable(ctx);
    else if (statement == "include") ok = ParseInclude(ctx);
    else if (statement == "define")  ok = ParseDefine(ctx);
    else                              ok = ParseProperty(ctx, statement);
    if (!ok) return false;
  }

  include_stack.pop_back();
  return true;
}

bool ODqProjectFile::Load(const filesystem::path & top_file,
                          const vector<string> & acommand_line_package_paths)
{
  filename = Canonical(top_file);
  diagnostics.clear();
  command_line_package_paths = acommand_line_package_paths;
  project_dir = filename.parent_path();
  variables.clear();
  single_properties.clear();
  define_names.clear();
  include_stack.clear();
  g_opt.project_filename = filename.string();

  if (filename.extension() != ".dqproj")
  {
    SParseContext ctx;
    ctx.filename = filename;
    ctx.text = " ";
    ctx.sp.Init(ctx.text.data(), ctx.text.size());
    return Fail(ctx, "ProjectExtension", "DQ project files must use the .dqproj extension");
  }
  if (!ParseFile(filename)) return false;
  if (g_opt.project_main_filename.empty())
  {
    SParseContext ctx;
    ctx.filename = filename;
    ctx.text = " ";
    ctx.sp.Init(ctx.text.data(), ctx.text.size());
    return Fail(ctx, "ProjectMain", "The assembled project does not specify main");
  }
  return true;
}
