/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_clargs.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#include <print>
#include <string>
#include <limits>
#include <filesystem>
#include <cstdlib>
#include "dqc_clargs.h"
#include "comp_options.h"
#include "comp_config.h"
#include "executable_path.h"
#include "module_path.h"
#include "artifact_lock.h"

using namespace std;

bool ODqCompClargs::IsValidDefineName(const string & name)
{
  if (name.empty())
  {
    return false;
  }

  char c = name[0];
  if (!(((c >= 'A') and (c <= 'Z')) or ((c >= 'a') and (c <= 'z')) or (c == '_')))
  {
    return false;
  }

  for (size_t i = 1; i < name.size(); ++i)
  {
    c = name[i];
    if (!(((c >= 'A') and (c <= 'Z')) or ((c >= 'a') and (c <= 'z')) or (c == '_')
          or ((c >= '0') and (c <= '9'))))
    {
      return false;
    }
  }

  return true;
}

string ODqCompClargs::ResolveCompilerExecutable(const string & argv0)
{
  return CurrentExecutablePath(argv0, "dq-comp");
}

string ODqCompClargs::CompilerExecutableDir(const string & compiler_executable)
{
  filesystem::path p(compiler_executable);
  if (!p.has_parent_path())
  {
    return "";
  }
  return p.parent_path().lexically_normal().string();
}

string ODqCompClargs::DefaultTargetArch()
{
  return g_opt.target.arch;
}

string ODqCompClargs::DefaultTargetRtl()
{
  return g_opt.target.platform_name;
}

string ODqCompClargs::DefaultBuildTag()
{
  return g_opt.target.name;
}

void ODqCompClargs::AddDefaultPackagePaths()
{
  g_opt.package_paths.clear();

  g_opt.package_paths.push_back("/usr/lib/dq/stdpkg");

  if (!g_opt.compiler_executable_dir.empty())
  {
    filesystem::path prefix_stdpkg_path = filesystem::path(g_opt.compiler_executable_dir) / ".." / "lib" / "dq" / "stdpkg";
    g_opt.package_paths.push_back(prefix_stdpkg_path.lexically_normal().string());

    filesystem::path stdpkg_path = filesystem::path(g_opt.compiler_executable_dir) / ".." / "stdpkg";
    g_opt.package_paths.push_back(stdpkg_path.lexically_normal().string());
  }

  g_opt.package_paths.push_back("/usr/lib/dq/packages");

  if (!g_opt.compiler_executable_dir.empty())
  {
    filesystem::path prefix_packages_path = filesystem::path(g_opt.compiler_executable_dir) / ".." / "lib" / "dq" / "packages";
    g_opt.package_paths.push_back(prefix_packages_path.lexically_normal().string());
  }

  const char * home = getenv("HOME");
  if (home && home[0])
  {
    filesystem::path user_packages = filesystem::path(home) / ".dq" / "packages";
    g_opt.package_paths.push_back(user_packages.lexically_normal().string());
  }
}

string ODqCompClargs::NormalizeCompilerExecutable(const string & argv0)
{
  return ResolveCompilerExecutable(argv0);
}

void ODqCompClargs::ParseModuleUseStack(const string & text, vector<string> & rstack)
{
  rstack.clear();

  size_t start = 0;
  while (start <= text.size())
  {
    size_t comma = text.find(',', start);
    string item = (comma == string::npos ? text.substr(start) : text.substr(start, comma - start));
    if (!item.empty())
    {
      rstack.push_back(item);
    }
    if (comma == string::npos)
    {
      break;
    }
    start = comma + 1;
  }
}

bool ODqCompClargs::ParseDefineIntValue(const string & text, int64_t & rvalue)
{
  if (text.empty())
  {
    return false;
  }

  size_t pos = 0;
  bool negative = false;
  if ((text[pos] == '+') or (text[pos] == '-'))
  {
    negative = (text[pos] == '-');
    ++pos;
  }

  if (pos >= text.size())
  {
    return false;
  }

  uint64_t accum = 0;
  for (; pos < text.size(); ++pos)
  {
    char c = text[pos];
    if ((c < '0') or (c > '9'))
    {
      return false;
    }

    uint64_t digit = (c - '0');
    if (accum > ((numeric_limits<uint64_t>::max() - digit) / 10))
    {
      return false;
    }
    accum = accum * 10 + digit;
  }

  if (negative)
  {
    if (accum > uint64_t(numeric_limits<int64_t>::max()) + 1)
    {
      return false;
    }

    if (accum == uint64_t(numeric_limits<int64_t>::max()) + 1)
    {
      rvalue = numeric_limits<int64_t>::min();
    }
    else
    {
      rvalue = -int64_t(accum);
    }
  }
  else
  {
    if (accum > uint64_t(numeric_limits<int64_t>::max()))
    {
      return false;
    }

    rvalue = int64_t(accum);
  }

  return true;
}

bool ODqCompClargs::ParseDefineBoolValue(const string & text, bool & rvalue)
{
  if ("true" == text)
  {
    rvalue = true;
    return true;
  }

  if ("false" == text)
  {
    rvalue = false;
    return true;
  }

  return false;
}

bool ODqCompClargs::ParseLtoMode(const string & text)
{
  if (text.empty() || ("full" == text))
  {
    g_opt.lto_mode = LTOMODE_FULL;
    return true;
  }

  if ("off" == text)
  {
    g_opt.lto_mode = LTOMODE_OFF;
    return true;
  }

  ++errorcnt;
  if ("thin" == text)
  {
    print("ThinLTO is not supported yet; use --lto=full or --lto=off\n");
  }
  else
  {
    print("Unknown LTO mode \"{}\"; use --lto=full or --lto=off\n", text);
  }
  PrintUsage();
  return false;
}

bool ODqCompClargs::SelectLinkMode(ECompLinkMode mode, const string & option)
{
  if ((DQC_LINK_AUTO == g_opt.link_mode) || (mode == g_opt.link_mode))
  {
    g_opt.link_mode = mode;
    return true;
  }

  string previous = (DQC_LINK_COMPILE_ONLY == g_opt.link_mode ? "-c" : "--link");
  ++errorcnt;
  print("Conflicting link mode options: {} and {}\n", previous, option);
  PrintUsage();
  return false;
}

ODqCompClargs::ODqCompClargs()
{
}

ODqCompClargs::~ODqCompClargs()
{
}

bool ODqCompClargs::VerblevelSwitch(const string & aswitch)
{
  if      ("-v"   == aswitch)   g_opt.verblevel = VERBLEVEL_STATUS;
  else if ("-vv"  == aswitch)   g_opt.verblevel = VERBLEVEL_INFO;
  else if ("-vvv" == aswitch)   g_opt.verblevel = VERBLEVEL_DEBUG;
  else if ("-v0"  == aswitch)   g_opt.verblevel = VERBLEVEL_NONE;
  else if ("-v1"  == aswitch)   g_opt.verblevel = VERBLEVEL_STATUS;
  else if ("-v2"  == aswitch)   g_opt.verblevel = VERBLEVEL_INFO;
  else if ("-v3"  == aswitch)   g_opt.verblevel = VERBLEVEL_DEBUG;
  else                          return false;

  return true;
}

void ODqCompClargs::ParseCmdLineArgsVerblevel(int argc, char ** argv)
{
  for (int i = 1; i < argc; i++)
  {
    string v(argv[i]);

    if ('-' == v[0])  // some compiler switch
    {
      if (VerblevelSwitch(v))
      {
        // already handled in the function
      }
    }
  }
}

void ODqCompClargs::ParseCmdLineArgs(int argc, char ** argv)
{
  string explicit_output;
  string build_tag_suffix;
  g_opt.compiler_executable = NormalizeCompilerExecutable(argc > 0 ? argv[0] : "");
  g_opt.compiler_executable_dir = CompilerExecutableDir(g_opt.compiler_executable);
  g_opt.build_tag = DefaultBuildTag();
  if (g_opt.target.IsBare())
  {
    g_opt.no_use_sys = true;
  }
  AddDefaultPackagePaths();

  for (int i = 1; i < argc; i++)
  {
    string v(argv[i]);

    if ('-' == v[0])  // some compiler switch
    {
      if      ("--version" == v)  g_opt.print_version = true;
      else if (v.starts_with("--target="))
      {
        if (v.substr(9).empty())
        {
          ++errorcnt;
          print("Empty target name\n");
          PrintUsage();
          return;
        }
      }
      else if ("--target" == v)
      {
        if (i + 1 < argc)
        {
          ++i; // already validated before target-dependent compiler initialization
        }
        else
        {
          ++errorcnt;
          print("Missing target name after --target\n");
          PrintUsage();
          return;
        }
      }
      else if ("--ifgen"   == v)  g_opt.ifgen = true;
      else if ("--ifdump"  == v)  g_opt.ifdump = true;
      else if ("--no-use-sys" == v)  g_opt.no_use_sys = true;
      else if ("--regen-if-stale" == v)  g_opt.regen_if_stale = true;
      else if ("--link" == v)
      {
        if (!SelectLinkMode(DQC_LINK_FORCE, v)) return;
      }
      else if (v.starts_with("--linker-arg="))
      {
        string linker_arg = v.substr(13);
        if (linker_arg.empty())
        {
          ++errorcnt;
          print("Empty linker argument\n");
          PrintUsage();
          return;
        }
        g_opt.linker_args.push_back(linker_arg);
      }
      else if ("--lto" == v)
      {
        if (!ParseLtoMode(""))
        {
          return;
        }
      }
      else if (v.starts_with("--lto="))
      {
        if (!ParseLtoMode(v.substr(6)))
        {
          return;
        }
      }
      else if ("--ifstack" == v)
      {
        if (i + 1 < argc)
        {
          ++i;
          ParseModuleUseStack(argv[i], g_opt.module_use_stack);
        }
        else
        {
          ++errorcnt;
          print("Missing module stack after --ifstack\n");
          PrintUsage();
          return;
        }
      }
      else if ("--pkg-path" == v)
      {
        if (i + 1 < argc)
        {
          ++i;
          g_opt.package_paths.push_back(argv[i]);
        }
        else
        {
          ++errorcnt;
          print("Missing path after --pkg-path\n");
          PrintUsage();
          return;
        }
      }
      else if ("--build" == v)
      {
        if (i + 1 < argc)
        {
          ++i;
          g_opt.build_tag = argv[i];
          if (g_opt.build_tag.empty())
          {
            ++errorcnt;
            print("Empty build tag after --build\n");
            PrintUsage();
            return;
          }
        }
        else
        {
          ++errorcnt;
          print("Missing build tag after --build\n");
          PrintUsage();
          return;
        }
      }
      else if ("--build-suffix" == v)
      {
        if (i + 1 < argc)
        {
          ++i;
          build_tag_suffix = argv[i];
          if (build_tag_suffix.empty())
          {
            ++errorcnt;
            print("Empty build tag suffix after --build-suffix\n");
            PrintUsage();
            return;
          }
        }
        else
        {
          ++errorcnt;
          print("Missing build tag suffix after --build-suffix\n");
          PrintUsage();
          return;
        }
      }
      else if ("--build-root" == v)
      {
        if (i + 1 < argc)
        {
          ++i;
          g_opt.build_root_dir = argv[i];
        }
        else
        {
          ++errorcnt;
          print("Missing path after --build-root\n");
          PrintUsage();
          return;
        }
      }
      else if ("--mod-root" == v)
      {
        if (i + 1 < argc)
        {
          ++i;
          g_opt.module_root_dir = argv[i];
        }
        else
        {
          ++errorcnt;
          print("Missing path after --mod-root\n");
          PrintUsage();
          return;
        }
      }
      else if ("--mod-name" == v)
      {
        if (i + 1 < argc)
        {
          ++i;
          g_opt.module_name = argv[i];
        }
        else
        {
          ++errorcnt;
          print("Missing module name after --mod-name\n");
          PrintUsage();
          return;
        }
      }
      else if (VerblevelSwitch(v))  { /* already handled in the function */ }
      else if ("-g"  == v)    g_opt.dbg_info = true;
      else if ("-ir" == v)    g_opt.ir_print = true;
      else if ("-c"  == v)
      {
        if (!SelectLinkMode(DQC_LINK_COMPILE_ONLY, v)) return;
      }
      else if ((v.size() > 2) and ('D' == v[1]))
      {
        string defspec = v.substr(2);
        string defname = defspec;
        string defvalue;
        size_t eqpos = defspec.find('=');
        if (eqpos != string::npos)
        {
          defname = defspec.substr(0, eqpos);
          defvalue = defspec.substr(eqpos + 1);
        }

        if (!IsValidDefineName(defname))
        {
          ++errorcnt;
          print("Invalid command line define name: {}\n", defname);
          PrintUsage();
          return;
        }

        OCmdLineDefine def;
        def.name = defname;

        if (eqpos != string::npos)
        {
          if (ParseDefineBoolValue(defvalue, def.bool_value))
          {
            def.has_bool_value = true;
          }
          else if (ParseDefineIntValue(defvalue, def.int_value))
          {
            def.has_int_value = true;
          }
          else
          {
            ++errorcnt;
            print("Invalid command line define value: {}\n", v);
            PrintUsage();
            return;
          }
        }

        g_opt.cmdline_defines.push_back(def);
      }
      else if ("-O0" == v)    g_opt.optlevel = 0;
      else if ("-O1" == v)    g_opt.optlevel = 1;
      else if ("-O2" == v)    g_opt.optlevel = 2;
      else if ("-O3" == v)    g_opt.optlevel = 3;
      else if ("-o"  == v)
      {
        if (i + 1 < argc)
        {
          ++i;
          explicit_output = argv[i];
          has_dash_o = true;
        }
        else
        {
          ++errorcnt;
          print("Missing filename after -o\n");
          PrintUsage();
          return;
        }
      }
      else
      {
        ++errorcnt;
        print("Unknown command line switch: {}\n", v);
        PrintUsage();
        return;
      }
    }
    else if (in_filename.empty())
    {
      in_filename = v;
    }
    else if (!has_dash_o)
    {
      // backward compatibility: second positional arg = output name
      explicit_output = v;
      has_dash_o = true;
    }
    else
    {
      ++errorcnt;
      print("Unexpected argument: {}\n", v);
      PrintUsage();
      return;
    }
  }

  if (!build_tag_suffix.empty())
  {
    g_opt.build_tag += "-" + build_tag_suffix;
  }

  if (g_opt.print_version)
  {
    return;
  }

  if (g_opt.ifgen and g_opt.ifdump)
  {
    ++errorcnt;
    print("--ifgen and --ifdump can not be used together.\n");
    PrintUsage();
    return;
  }

  if (in_filename.empty())
  {
    ++errorcnt;
    print("Input file name is missing.\n");
    PrintUsage();
    return;
  }

  if (g_opt.ifdump)
  {
    if (has_dash_o)
    {
      ++errorcnt;
      print("--ifdump expects only one input .dqm_if file.\n");
      PrintUsage();
    }
    return;
  }

  if (g_opt.build_root_dir.empty())
  {
    error_code ec;
    filesystem::path input_path = filesystem::absolute(in_filename, ec);
    if (ec)
    {
      input_path = in_filename;
    }
    g_opt.build_root_dir = input_path.parent_path().lexically_normal().string();
  }
  else
  {
    error_code ec;
    filesystem::path build_root = filesystem::absolute(g_opt.build_root_dir, ec);
    g_opt.build_root_dir = (ec ? filesystem::path(g_opt.build_root_dir) : build_root).lexically_normal().string();
  }

  // derive base_name by stripping .dq extension
  if (in_filename.size() > 3 && in_filename.substr(in_filename.size() - 3) == ".dq")
  {
    base_name = in_filename.substr(0, in_filename.size() - 3);
  }
  else
  {
    base_name = in_filename;
  }

  OModulePath current_module;
  string module_error;
  filesystem::path default_artifact_path;
  filesystem::path default_interface_path;
  if (current_module.InitCurrent(in_filename, module_error))
  {
    default_artifact_path = current_module.artifact_path;
    default_interface_path = current_module.interface_artifact_path;
  }
  else
  {
    default_artifact_path = OModulePath::BuildArtifactPath(in_filename);
    default_interface_path = OModulePath::BuildInterfaceArtifactPath(in_filename);
  }

  if (g_opt.ifgen)
  {
    out_filename = has_dash_o ? explicit_output : default_interface_path.string();
    interface_out_filename = out_filename;
  }
  else if ((DQC_LINK_COMPILE_ONLY == g_opt.link_mode)
           || ((DQC_LINK_AUTO == g_opt.link_mode) && g_opt.target.IsBare()))
  {
    // Explicit compile-only and automatic bare builds produce an object directly.
    out_filename = has_dash_o ? explicit_output : default_artifact_path.string();
    interface_out_filename = ArtifactInterfacePathForObject(out_filename).string();
  }
  else
  {
    // full compilation produces a DQ module object and its interface
    out_filename = default_artifact_path.string();
    interface_out_filename = default_interface_path.string();
    // link_output is where the final result should go
    link_output = has_dash_o ? explicit_output : base_name;
    if (!has_dash_o && g_opt.target.IsBare())
    {
      link_output += ".elf";
    }
  }

  return;
}


void ODqCompClargs::PrintUsage()
{
  print("Usage:\n");
  print("  dq-comp [options] <file.dq>\n");
  print("Options:\n");
  print("  -o <file> : set output filename\n");
  print("  -c        : compile only (do not link)\n");
  print("  --link    : force linking even without a hosted Main function\n");
  print("  --linker-arg=<arg> : pass one argument directly to the linker (repeatable)\n");
  print("  --ifgen   : generate module interface file (.dqm_if)\n");
  print("  --ifdump  : dump module interface artifact (.dqm_if)\n");
  print("  --no-use-sys : do not add the implicit merged sys module\n");
  print("  --target=<name> : select the compiler target\n");
  print("  --pkg-path <path> : add a package search root (repeatable, last wins)\n");
  print("  --build <tag> : select .dqbuild build tag\n");
  print("  --build-suffix <suffix> : append to the selected .dqbuild build tag\n");
  print("  --lto[=full|off] : emit and link LLVM bitcode sidecars for full LTO\n");
  print("  --version : print compiler version\n");
  print("  -D<name>  : defines the <name> symbol with boolean true\n");
  print("  -D<name>=<value> : defines the <name> symbol with the <value> (int/bool)\n");
  print("  -On       : optimization level, n=0-3\n");
  print("  -g        : generate debug info\n");
  print("  -v,-v1    : print compile status messages\n");
  print("  -vv,-v2   : print detailed compiler information\n");
  print("  -vvv,-v3  : print compiler internal trace messages\n");
  print("  -v0       : no extra output (default)\n");
  print("  -ir       : print LLVM IR code\n");
}
